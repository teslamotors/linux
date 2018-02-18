/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/delay.h>	/* for udelay */
#include <linux/clk.h>
#include "gk20a/gk20a.h"
#include "gk20a/pmu_gk20a.h"

#include "gm206/pmu_gm206.h"
#include "gm20b/pmu_gm20b.h"
#include "gp10b/pmu_gp10b.h"
#include "gp106/pmu_gp106.h"
#include "gp106/acr_gp106.h"
#include "gp106/hw_mc_gp106.h"
#include "gp106/hw_pwr_gp106.h"
#include "gp106/hw_psec_gp106.h"
#include "sec2_gp106.h"
#include "acr.h"

/*Defines*/
#define gm20b_dbg_pmu(fmt, arg...) \
	gk20a_dbg(gpu_dbg_pmu, fmt, ##arg)

int sec2_clear_halt_interrupt_status(struct gk20a *g, unsigned int timeout)
{
	u32 data = 0;
	unsigned long end_jiffies = jiffies + msecs_to_jiffies(timeout);

	while (time_before(jiffies, end_jiffies) ||
			!tegra_platform_is_silicon()) {
		gk20a_writel(g, psec_falcon_irqsclr_r(),
			     gk20a_readl(g, psec_falcon_irqsclr_r()) | (0x10));
		data = gk20a_readl(g, psec_falcon_irqstat_r());
		if ((data & psec_falcon_irqstat_halt_true_f()) !=
			psec_falcon_irqstat_halt_true_f())
			/*halt irq is clear*/
			break;
		timeout--;
		udelay(1);
	}
	if (timeout == 0)
		return -EBUSY;
	return 0;
}

int sec2_wait_for_halt(struct gk20a *g, unsigned int timeout)
{
	u32 data = 0;
	int completion = -EBUSY;
	unsigned long end_jiffies = jiffies + msecs_to_jiffies(timeout);

	while (time_before(jiffies, end_jiffies) ||
			!tegra_platform_is_silicon()) {
		data = gk20a_readl(g, psec_falcon_cpuctl_r());
		if (data & psec_falcon_cpuctl_halt_intr_m()) {
			/*CPU is halted break*/
			completion = 0;
			break;
		}
		udelay(1);
	}
	if (completion){
		gk20a_err(dev_from_gk20a(g), "ACR boot timed out");
	}
	else {

		g->acr.capabilities = gk20a_readl(g, psec_falcon_mailbox1_r());
		gm20b_dbg_pmu("ACR capabilities %x\n", g->acr.capabilities);
		data = gk20a_readl(g, psec_falcon_mailbox0_r());
		if (data) {

			gk20a_err(dev_from_gk20a(g),
				"ACR boot failed, err %x", data);
			completion = -EAGAIN;
		}
	}

	init_pmu_setup_hw1(g);

	return completion;
}

void sec2_copy_to_dmem(struct pmu_gk20a *pmu,
		u32 dst, u8 *src, u32 size, u8 port)
{
	struct gk20a *g = gk20a_from_pmu(pmu);
	u32 i, words, bytes;
	u32 data, addr_mask;
	u32 *src_u32 = (u32*)src;

	if (size == 0) {
		gk20a_err(dev_from_gk20a(g),
			"size is zero");
		return;
	}

	if (dst & 0x3) {
		gk20a_err(dev_from_gk20a(g),
			"dst (0x%08x) not 4-byte aligned", dst);
		return;
	}

	mutex_lock(&pmu->pmu_copy_lock);

	words = size >> 2;
	bytes = size & 0x3;

	addr_mask = psec_falcon_dmemc_offs_m() |
		    psec_falcon_dmemc_blk_m();

	dst &= addr_mask;

	gk20a_writel(g, psec_falcon_dmemc_r(port),
		dst | psec_falcon_dmemc_aincw_f(1));

	for (i = 0; i < words; i++)
		gk20a_writel(g, psec_falcon_dmemd_r(port), src_u32[i]);

	if (bytes > 0) {
		data = 0;
		for (i = 0; i < bytes; i++)
			((u8 *)&data)[i] = src[(words << 2) + i];
		gk20a_writel(g, psec_falcon_dmemd_r(port), data);
	}

	data = gk20a_readl(g, psec_falcon_dmemc_r(port)) & addr_mask;
	size = ALIGN(size, 4);
	if (data != dst + size) {
		gk20a_err(dev_from_gk20a(g),
			"copy failed. bytes written %d, expected %d",
			data - dst, size);
	}
	mutex_unlock(&pmu->pmu_copy_lock);
	return;
}

int bl_bootstrap_sec2(struct pmu_gk20a *pmu,
	void *desc, u32 bl_sz)
{
	struct gk20a *g = gk20a_from_pmu(pmu);
	struct acr_desc *acr = &g->acr;
	struct mm_gk20a *mm = &g->mm;
	u32 imem_dst_blk = 0;
	u32 virt_addr = 0;
	u32 tag = 0;
	u32 index = 0;
	struct hsflcn_bl_desc *pmu_bl_gm10x_desc = g->acr.pmu_hsbl_desc;
	u32 *bl_ucode;
	u32 data = 0;

	gk20a_dbg_fn("");

	/* SEC2 Config */
	gk20a_writel(g, psec_falcon_itfen_r(),
			gk20a_readl(g, psec_falcon_itfen_r()) |
			psec_falcon_itfen_ctxen_enable_f());

	gk20a_writel(g, psec_falcon_nxtctx_r(),
			pwr_pmu_new_instblk_ptr_f(
			gk20a_mm_inst_block_addr(g, &mm->pmu.inst_block) >> 12) |
			pwr_pmu_new_instblk_valid_f(1) |
			gk20a_aperture_mask(g, &mm->pmu.inst_block,
				pwr_pmu_new_instblk_target_sys_coh_f(),
				pwr_pmu_new_instblk_target_fb_f()));

	data = gk20a_readl(g, psec_falcon_debug1_r());
	data |= psec_falcon_debug1_ctxsw_mode_m();
	gk20a_writel(g, psec_falcon_debug1_r(), data);

	data = gk20a_readl(g, psec_falcon_engctl_r());
	data |= (1 << 3);
	gk20a_writel(g, psec_falcon_engctl_r(), data);

	/* TBD: load all other surfaces */
	/*copy bootloader interface structure to dmem*/
	gk20a_writel(g, psec_falcon_dmemc_r(0),
			psec_falcon_dmemc_offs_f(0) |
			psec_falcon_dmemc_blk_f(0)  |
			psec_falcon_dmemc_aincw_f(1));
	sec2_copy_to_dmem(pmu, 0, (u8 *)desc,
		sizeof(struct flcn_bl_dmem_desc), 0);
	/*TODO This had to be copied to bl_desc_dmem_load_off, but since
	 * this is 0, so ok for now*/

	/* Now copy bootloader to TOP of IMEM */
	imem_dst_blk = (psec_falcon_hwcfg_imem_size_v(
			gk20a_readl(g, psec_falcon_hwcfg_r()))) - bl_sz/256;

	/* Set Auto-Increment on write */
	gk20a_writel(g, psec_falcon_imemc_r(0),
			psec_falcon_imemc_offs_f(0) |
			psec_falcon_imemc_blk_f(imem_dst_blk)  |
			psec_falcon_imemc_aincw_f(1));
	virt_addr = pmu_bl_gm10x_desc->bl_start_tag << 8;
	tag = virt_addr >> 8; /* tag is always 256B aligned */
	bl_ucode = (u32 *)(acr->hsbl_ucode.cpu_va);
	for (index = 0; index < bl_sz/4; index++) {
		if ((index % 64) == 0) {
			gk20a_writel(g, psec_falcon_imemt_r(0),
				(tag & 0xffff) << 0);
			tag++;
		}
		gk20a_writel(g, psec_falcon_imemd_r(0),
				bl_ucode[index] & 0xffffffff);
	}
	gk20a_writel(g, psec_falcon_imemt_r(0), (0 & 0xffff) << 0);

	gm20b_dbg_pmu("Before starting falcon with BL\n");

	gk20a_writel(g, psec_falcon_mailbox0_r(), 0xDEADA5A5);

	gk20a_writel(g, psec_falcon_bootvec_r(),
			psec_falcon_bootvec_vec_f(virt_addr));

	gk20a_writel(g, psec_falcon_cpuctl_r(),
			psec_falcon_cpuctl_startcpu_f(1));

	return 0;
}

void sec_enable_irq(struct pmu_gk20a *pmu, bool enable)
{
	struct gk20a *g = gk20a_from_pmu(pmu);

	gk20a_dbg_fn("");

	gk20a_writel(g, psec_falcon_irqmclr_r(),
		psec_falcon_irqmclr_gptmr_f(1)  |
		psec_falcon_irqmclr_wdtmr_f(1)  |
		psec_falcon_irqmclr_mthd_f(1)   |
		psec_falcon_irqmclr_ctxsw_f(1)  |
		psec_falcon_irqmclr_halt_f(1)   |
		psec_falcon_irqmclr_exterr_f(1) |
		psec_falcon_irqmclr_swgen0_f(1) |
		psec_falcon_irqmclr_swgen1_f(1) |
		psec_falcon_irqmclr_ext_f(0xff));

	if (enable) {
		/* dest 0=falcon, 1=host; level 0=irq0, 1=irq1 */
		gk20a_writel(g, psec_falcon_irqdest_r(),
			psec_falcon_irqdest_host_gptmr_f(0)    |
			psec_falcon_irqdest_host_wdtmr_f(1)    |
			psec_falcon_irqdest_host_mthd_f(0)     |
			psec_falcon_irqdest_host_ctxsw_f(0)    |
			psec_falcon_irqdest_host_halt_f(1)     |
			psec_falcon_irqdest_host_exterr_f(0)   |
			psec_falcon_irqdest_host_swgen0_f(1)   |
			psec_falcon_irqdest_host_swgen1_f(0)   |
			psec_falcon_irqdest_host_ext_f(0xff)   |
			psec_falcon_irqdest_target_gptmr_f(1)  |
			psec_falcon_irqdest_target_wdtmr_f(0)  |
			psec_falcon_irqdest_target_mthd_f(0)   |
			psec_falcon_irqdest_target_ctxsw_f(0)  |
			psec_falcon_irqdest_target_halt_f(0)   |
			psec_falcon_irqdest_target_exterr_f(0) |
			psec_falcon_irqdest_target_swgen0_f(0) |
			psec_falcon_irqdest_target_swgen1_f(1) |
			psec_falcon_irqdest_target_ext_f(0xff));

		/* 0=disable, 1=enable */
		gk20a_writel(g, psec_falcon_irqmset_r(),
			psec_falcon_irqmset_gptmr_f(1)  |
			psec_falcon_irqmset_wdtmr_f(1)  |
			psec_falcon_irqmset_mthd_f(0)   |
			psec_falcon_irqmset_ctxsw_f(0)  |
			psec_falcon_irqmset_halt_f(1)   |
			psec_falcon_irqmset_exterr_f(1) |
			psec_falcon_irqmset_swgen0_f(1) |
			psec_falcon_irqmset_swgen1_f(1));

	}

	gk20a_dbg_fn("done");
}

void init_pmu_setup_hw1(struct gk20a *g)
{
	struct mm_gk20a *mm = &g->mm;
	struct pmu_gk20a *pmu = &g->pmu;
	struct gk20a_platform *platform = dev_get_drvdata(g->dev);

	/* PMU TRANSCFG */
	/* setup apertures - virtual */
	gk20a_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_UCODE),
			pwr_fbif_transcfg_mem_type_physical_f() |
			pwr_fbif_transcfg_target_local_fb_f());
	gk20a_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_VIRT),
			pwr_fbif_transcfg_mem_type_virtual_f());
	/* setup apertures - physical */
	gk20a_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_PHYS_VID),
			pwr_fbif_transcfg_mem_type_physical_f() |
			pwr_fbif_transcfg_target_local_fb_f());
	gk20a_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_PHYS_SYS_COH),
			pwr_fbif_transcfg_mem_type_physical_f() |
			pwr_fbif_transcfg_target_coherent_sysmem_f());
	gk20a_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_PHYS_SYS_NCOH),
			pwr_fbif_transcfg_mem_type_physical_f() |
			pwr_fbif_transcfg_target_noncoherent_sysmem_f());

	/* PMU Config */
	gk20a_writel(g, pwr_falcon_itfen_r(),
				gk20a_readl(g, pwr_falcon_itfen_r()) |
				pwr_falcon_itfen_ctxen_enable_f());
	gk20a_writel(g, pwr_pmu_new_instblk_r(),
				pwr_pmu_new_instblk_ptr_f(
					gk20a_mm_inst_block_addr(g, &mm->pmu.inst_block) >> 12) |
				pwr_pmu_new_instblk_valid_f(1) |
				gk20a_aperture_mask(g, &mm->pmu.inst_block,
					pwr_pmu_new_instblk_target_sys_coh_f(),
					pwr_pmu_new_instblk_target_fb_f()));

	/*Copying pmu cmdline args*/
	g->ops.pmu_ver.set_pmu_cmdline_args_cpu_freq(pmu,
				clk_get_rate(platform->clk[1]));
	g->ops.pmu_ver.set_pmu_cmdline_args_secure_mode(pmu, 1);
	g->ops.pmu_ver.set_pmu_cmdline_args_trace_size(
		pmu, GK20A_PMU_TRACE_BUFSIZE);
	g->ops.pmu_ver.set_pmu_cmdline_args_trace_dma_base(pmu);
	g->ops.pmu_ver.set_pmu_cmdline_args_trace_dma_idx(
		pmu, GK20A_PMU_DMAIDX_VIRT);

	pmu_copy_to_dmem(pmu, g->acr.pmu_args,
			(u8 *)(g->ops.pmu_ver.get_pmu_cmdline_args_ptr(pmu)),
			g->ops.pmu_ver.get_pmu_cmdline_args_size(pmu), 0);

}

int init_sec2_setup_hw1(struct gk20a *g,
		void *desc, u32 bl_sz)
{
	struct pmu_gk20a *pmu = &g->pmu;
	int err;
	u32 data = 0;

	gk20a_dbg_fn("");

	mutex_lock(&pmu->isr_mutex);
	g->ops.pmu.reset(g);
	pmu->isr_enabled = true;
	mutex_unlock(&pmu->isr_mutex);

	data = gk20a_readl(g, psec_fbif_ctl_r());
	data |= psec_fbif_ctl_allow_phys_no_ctx_allow_f();
	gk20a_writel(g, psec_fbif_ctl_r(), data);

	data = gk20a_readl(g, psec_falcon_dmactl_r());
	data &= ~(psec_falcon_dmactl_require_ctx_f(1));
	gk20a_writel(g, psec_falcon_dmactl_r(), data);

	/* setup apertures - virtual */
	gk20a_writel(g, psec_fbif_transcfg_r(GK20A_PMU_DMAIDX_UCODE),
			psec_fbif_transcfg_mem_type_physical_f() |
			psec_fbif_transcfg_target_local_fb_f());
	gk20a_writel(g, psec_fbif_transcfg_r(GK20A_PMU_DMAIDX_VIRT),
			psec_fbif_transcfg_mem_type_virtual_f());
	/* setup apertures - physical */
	gk20a_writel(g, psec_fbif_transcfg_r(GK20A_PMU_DMAIDX_PHYS_VID),
			psec_fbif_transcfg_mem_type_physical_f() |
			psec_fbif_transcfg_target_local_fb_f());
	gk20a_writel(g, psec_fbif_transcfg_r(GK20A_PMU_DMAIDX_PHYS_SYS_COH),
			psec_fbif_transcfg_mem_type_physical_f() |
			psec_fbif_transcfg_target_coherent_sysmem_f());
	gk20a_writel(g, psec_fbif_transcfg_r(GK20A_PMU_DMAIDX_PHYS_SYS_NCOH),
			psec_fbif_transcfg_mem_type_physical_f() |
			psec_fbif_transcfg_target_noncoherent_sysmem_f());

	/*disable irqs for hs falcon booting as we will poll for halt*/
	mutex_lock(&pmu->isr_mutex);
	pmu_enable_irq(pmu, false);
	sec_enable_irq(pmu, false);
	pmu->isr_enabled = false;
	mutex_unlock(&pmu->isr_mutex);
	err = bl_bootstrap_sec2(pmu, desc, bl_sz);
	if (err)
		return err;

	return 0;
}
