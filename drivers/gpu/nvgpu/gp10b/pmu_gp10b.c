/*
 * GP10B PMU
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/tegra-fuse.h>
#include "gk20a/gk20a.h"
#include "gk20a/pmu_gk20a.h"
#include "gm20b/acr_gm20b.h"
#include "gm20b/pmu_gm20b.h"

#include "pmu_gp10b.h"
#include "hw_pwr_gp10b.h"
#include "hw_fuse_gp10b.h"
#include "gp10b_sysfs.h"

#define gp10b_dbg_pmu(fmt, arg...) \
	gk20a_dbg(gpu_dbg_pmu, fmt, ##arg)
/*!
 * Structure/object which single register write need to be done during PG init
 * sequence to set PROD values.
 */
struct pg_init_sequence_list {
	u32 regaddr;
	u32 writeval;
};

/* PROD settings for ELPG sequencing registers*/
static struct pg_init_sequence_list _pginitseq_gp10b[] = {
		{0x0010ab10, 0x0000868B} ,
		{0x0010e118, 0x8590848F} ,
		{0x0010e000, 0} ,
		{0x0010e06c, 0x000000A3} ,
		{0x0010e06c, 0x000000A0} ,
		{0x0010e06c, 0x00000095} ,
		{0x0010e06c, 0x000000A6} ,
		{0x0010e06c, 0x0000008C} ,
		{0x0010e06c, 0x00000080} ,
		{0x0010e06c, 0x00000081} ,
		{0x0010e06c, 0x00000087} ,
		{0x0010e06c, 0x00000088} ,
		{0x0010e06c, 0x0000008D} ,
		{0x0010e06c, 0x00000082} ,
		{0x0010e06c, 0x00000083} ,
		{0x0010e06c, 0x00000089} ,
		{0x0010e06c, 0x0000008A} ,
		{0x0010e06c, 0x000000A2} ,
		{0x0010e06c, 0x00000097} ,
		{0x0010e06c, 0x00000092} ,
		{0x0010e06c, 0x00000099} ,
		{0x0010e06c, 0x0000009B} ,
		{0x0010e06c, 0x0000009D} ,
		{0x0010e06c, 0x0000009F} ,
		{0x0010e06c, 0x000000A1} ,
		{0x0010e06c, 0x00000096} ,
		{0x0010e06c, 0x00000091} ,
		{0x0010e06c, 0x00000098} ,
		{0x0010e06c, 0x0000009A} ,
		{0x0010e06c, 0x0000009C} ,
		{0x0010e06c, 0x0000009E} ,
		{0x0010ab14, 0x00000000} ,
		{0x0010e024, 0x00000000} ,
		{0x0010e028, 0x00000000} ,
		{0x0010e11c, 0x00000000} ,
		{0x0010ab1c, 0x140B0BFF} ,
		{0x0010e020, 0x0E2626FF} ,
		{0x0010e124, 0x251010FF} ,
		{0x0010ab20, 0x89abcdef} ,
		{0x0010ab24, 0x00000000} ,
		{0x0010e02c, 0x89abcdef} ,
		{0x0010e030, 0x00000000} ,
		{0x0010e128, 0x89abcdef} ,
		{0x0010e12c, 0x00000000} ,
		{0x0010ab28, 0x7FFFFFFF} ,
		{0x0010ab2c, 0x70000000} ,
		{0x0010e034, 0x7FFFFFFF} ,
		{0x0010e038, 0x70000000} ,
		{0x0010e130, 0x7FFFFFFF} ,
		{0x0010e134, 0x70000000} ,
		{0x0010ab30, 0x00000000} ,
		{0x0010ab34, 0x00000001} ,
		{0x00020004, 0x00000000} ,
		{0x0010e138, 0x00000000} ,
		{0x0010e040, 0x00000000} ,
		{0x0010e168, 0x00000000} ,
		{0x0010e114, 0x0000A5A4} ,
		{0x0010e110, 0x00000000} ,
		{0x0010e10c, 0x8590848F} ,
		{0x0010e05c, 0x00000000} ,
		{0x0010e044, 0x00000000} ,
		{0x0010a644, 0x0000868B} ,
		{0x0010a648, 0x00000000} ,
		{0x0010a64c, 0x00829493} ,
		{0x0010a650, 0x00000000} ,
		{0x0010e000, 0} ,
		{0x0010e068, 0x000000A3} ,
		{0x0010e068, 0x000000A0} ,
		{0x0010e068, 0x00000095} ,
		{0x0010e068, 0x000000A6} ,
		{0x0010e068, 0x0000008C} ,
		{0x0010e068, 0x00000080} ,
		{0x0010e068, 0x00000081} ,
		{0x0010e068, 0x00000087} ,
		{0x0010e068, 0x00000088} ,
		{0x0010e068, 0x0000008D} ,
		{0x0010e068, 0x00000082} ,
		{0x0010e068, 0x00000083} ,
		{0x0010e068, 0x00000089} ,
		{0x0010e068, 0x0000008A} ,
		{0x0010e068, 0x000000A2} ,
		{0x0010e068, 0x00000097} ,
		{0x0010e068, 0x00000092} ,
		{0x0010e068, 0x00000099} ,
		{0x0010e068, 0x0000009B} ,
		{0x0010e068, 0x0000009D} ,
		{0x0010e068, 0x0000009F} ,
		{0x0010e068, 0x000000A1} ,
		{0x0010e068, 0x00000096} ,
		{0x0010e068, 0x00000091} ,
		{0x0010e068, 0x00000098} ,
		{0x0010e068, 0x0000009A} ,
		{0x0010e068, 0x0000009C} ,
		{0x0010e068, 0x0000009E} ,
		{0x0010e000, 0} ,
		{0x0010e004, 0x0000008E},
};

static void gp10b_pmu_load_multiple_falcons(struct gk20a *g, u32 falconidmask,
					 u32 flags)
{
	struct pmu_gk20a *pmu = &g->pmu;
	struct pmu_cmd cmd;
	u32 seq;

	gk20a_dbg_fn("");

	gp10b_dbg_pmu("wprinit status = %x\n", g->ops.pmu.lspmuwprinitdone);
	if (g->ops.pmu.lspmuwprinitdone) {
		/* send message to load FECS falcon */
		memset(&cmd, 0, sizeof(struct pmu_cmd));
		cmd.hdr.unit_id = PMU_UNIT_ACR;
		cmd.hdr.size = PMU_CMD_HDR_SIZE +
		  sizeof(struct pmu_acr_cmd_bootstrap_multiple_falcons);
		cmd.cmd.acr.boot_falcons.cmd_type =
		  PMU_ACR_CMD_ID_BOOTSTRAP_MULTIPLE_FALCONS;
		cmd.cmd.acr.boot_falcons.flags = flags;
		cmd.cmd.acr.boot_falcons.falconidmask =
				falconidmask;
		cmd.cmd.acr.boot_falcons.usevamask = 0;
		cmd.cmd.acr.boot_falcons.wprvirtualbase.lo =
				u64_lo32(g->pmu.wpr_buf.gpu_va);
		cmd.cmd.acr.boot_falcons.wprvirtualbase.hi =
				u64_hi32(g->pmu.wpr_buf.gpu_va);
		gp10b_dbg_pmu("PMU_ACR_CMD_ID_BOOTSTRAP_MULTIPLE_FALCONS:%x\n",
				falconidmask);
		gk20a_pmu_cmd_post(g, &cmd, NULL, NULL, PMU_COMMAND_QUEUE_HPQ,
				pmu_handle_fecs_boot_acr_msg, pmu, &seq, ~0);
	}

	gk20a_dbg_fn("done");
	return;
}

int gp10b_load_falcon_ucode(struct gk20a *g, u32 falconidmask)
{
	u32 flags = PMU_ACR_CMD_BOOTSTRAP_FALCON_FLAGS_RESET_YES;

	/* GM20B PMU supports loading FECS and GPCCS only */
	if (falconidmask == 0)
		return -EINVAL;
	if (falconidmask & ~((1 << LSF_FALCON_ID_FECS) |
				(1 << LSF_FALCON_ID_GPCCS)))
				return -EINVAL;
	g->ops.pmu.lsfloadedfalconid = 0;
	/* check whether pmu is ready to bootstrap lsf if not wait for it */
	if (!g->ops.pmu.lspmuwprinitdone) {
		pmu_wait_message_cond(&g->pmu,
				gk20a_get_gr_idle_timeout(g),
				&g->ops.pmu.lspmuwprinitdone, 1);
		/* check again if it still not ready indicate an error */
		if (!g->ops.pmu.lspmuwprinitdone) {
			gk20a_err(dev_from_gk20a(g),
				"PMU not ready to load LSF");
			return -ETIMEDOUT;
		}
	}
	/* load falcon(s) */
	gp10b_pmu_load_multiple_falcons(g, falconidmask, flags);
	pmu_wait_message_cond(&g->pmu,
			gk20a_get_gr_idle_timeout(g),
			&g->ops.pmu.lsfloadedfalconid, falconidmask);
	if (g->ops.pmu.lsfloadedfalconid != falconidmask)
		return -ETIMEDOUT;
	return 0;
}

static void pmu_handle_gr_param_msg(struct gk20a *g, struct pmu_msg *msg,
			void *param, u32 handle, u32 status)
{
	gk20a_dbg_fn("");

	if (status != 0) {
		gk20a_err(dev_from_gk20a(g), "GR PARAM cmd aborted");
		/* TBD: disable ELPG */
		return;
	}

	gp10b_dbg_pmu("GR PARAM is acknowledged from PMU %x \n",
			msg->msg.pg.msg_type);

	return;
}

int gp10b_pg_gr_init(struct gk20a *g, u32 pg_engine_id)
{
	struct pmu_gk20a *pmu = &g->pmu;
	struct pmu_cmd cmd;
	u32 seq;

	if (pg_engine_id == PMU_PG_ELPG_ENGINE_ID_GRAPHICS) {
		memset(&cmd, 0, sizeof(struct pmu_cmd));
		cmd.hdr.unit_id = PMU_UNIT_PG;
		cmd.hdr.size = PMU_CMD_HDR_SIZE +
				sizeof(struct pmu_pg_cmd_gr_init_param);
		cmd.cmd.pg.gr_init_param.cmd_type =
				PMU_PG_CMD_ID_PG_PARAM;
		cmd.cmd.pg.gr_init_param.sub_cmd_id =
				PMU_PG_PARAM_CMD_GR_INIT_PARAM;
		cmd.cmd.pg.gr_init_param.featuremask =
				PMU_PG_FEATURE_GR_POWER_GATING_ENABLED;

		gp10b_dbg_pmu("cmd post PMU_PG_CMD_ID_PG_PARAM ");
		gk20a_pmu_cmd_post(g, &cmd, NULL, NULL, PMU_COMMAND_QUEUE_HPQ,
				pmu_handle_gr_param_msg, pmu, &seq, ~0);

	} else
		return -EINVAL;

	return 0;
}

static void gp10b_pmu_elpg_statistics(struct gk20a *g, u32 pg_engine_id,
		struct pmu_pg_stats_data *pg_stat_data)
{
	struct pmu_gk20a *pmu = &g->pmu;
	struct pmu_pg_stats_v1 stats;

	pmu_copy_from_dmem(pmu,
		pmu->stat_dmem_offset[pg_engine_id],
		(u8 *)&stats, sizeof(struct pmu_pg_stats_v1), 0);

	pg_stat_data->ingating_time = stats.total_sleep_timeus;
	pg_stat_data->ungating_time = stats.total_nonsleep_timeus;
	pg_stat_data->gating_cnt = stats.entry_count;
	pg_stat_data->avg_entry_latency_us = stats.entrylatency_avgus;
	pg_stat_data->avg_exit_latency_us = stats.exitlatency_avgus;
}

static int gp10b_pmu_setup_elpg(struct gk20a *g)
{
	int ret = 0;
	u32 reg_writes;
	u32 index;

	gk20a_dbg_fn("");

	if (g->elpg_enabled) {
		reg_writes = ((sizeof(_pginitseq_gp10b) /
				sizeof((_pginitseq_gp10b)[0])));
		/* Initialize registers with production values*/
		for (index = 0; index < reg_writes; index++) {
			gk20a_writel(g, _pginitseq_gp10b[index].regaddr,
				_pginitseq_gp10b[index].writeval);
		}
	}

	gk20a_dbg_fn("done");
	return ret;
}

void gp10b_write_dmatrfbase(struct gk20a *g, u32 addr)
{
	gk20a_writel(g, pwr_falcon_dmatrfbase_r(),
				addr);
	gk20a_writel(g, pwr_falcon_dmatrfbase1_r(),
				0x0);
}

static int gp10b_init_pmu_setup_hw1(struct gk20a *g)
{
	struct pmu_gk20a *pmu = &g->pmu;
	int err;

	gk20a_dbg_fn("");

	mutex_lock(&pmu->isr_mutex);
	pmu_reset(pmu);
	pmu->isr_enabled = true;
	mutex_unlock(&pmu->isr_mutex);

	/* setup apertures - virtual */
	gk20a_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_UCODE),
		pwr_fbif_transcfg_mem_type_virtual_f());
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

	err = pmu_bootstrap(pmu);
	if (err)
		return err;

	gk20a_dbg_fn("done");
	return 0;

}

static void pmu_handle_ecc_en_dis_msg(struct gk20a *g, struct pmu_msg *msg,
			void *param, u32 handle, u32 status)
{
	struct pmu_gk20a *pmu = &g->pmu;
	struct pmu_msg_lrf_tex_ltc_dram_en_dis *ecc =
			&msg->msg.lrf_tex_ltc_dram.en_dis;
	gk20a_dbg_fn("");

	if (status != 0) {
		gk20a_err(dev_from_gk20a(g), "ECC en dis cmd aborted");
		return;
	}
	if (msg->msg.lrf_tex_ltc_dram.msg_type !=
			PMU_LRF_TEX_LTC_DRAM_MSG_ID_EN_DIS) {
		gk20a_err(dev_from_gk20a(g),
			"Invalid msg for LRF_TEX_LTC_DRAM_CMD_ID_EN_DIS cmd");
		return;
	} else if (ecc->pmu_status != 0) {
		gk20a_err(dev_from_gk20a(g),
			"LRF_TEX_LTC_DRAM_MSG_ID_EN_DIS msg status = %x",
			ecc->pmu_status);
		gk20a_err(dev_from_gk20a(g),
			"LRF_TEX_LTC_DRAM_MSG_ID_EN_DIS msg en fail = %x",
			ecc->en_fail_mask);
		gk20a_err(dev_from_gk20a(g),
			"LRF_TEX_LTC_DRAM_MSG_ID_EN_DIS msg dis fail = %x",
			ecc->dis_fail_mask);
	} else
		pmu->override_done = 1;
	gk20a_dbg_fn("done");
}

static int send_ecc_overide_en_dis_cmd(struct gk20a *g, u32 bitmask)
{
	struct pmu_gk20a *pmu = &g->pmu;
	struct pmu_cmd cmd;
	u32 seq;
	int status;
	gk20a_dbg_fn("");

	if (!tegra_fuse_readl(FUSE_OPT_ECC_EN)) {
		gk20a_err(dev_from_gk20a(g), "Board not ECC capable");
		return -1;
	}
	if (!(g->acr.capabilities &
			ACR_LRF_TEX_LTC_DRAM_PRIV_MASK_ENABLE_LS_OVERRIDE)) {
		gk20a_err(dev_from_gk20a(g), "check ACR capabilities");
		return -1;
	}
	memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_FECS_MEM_OVERRIDE;
	cmd.hdr.size = PMU_CMD_HDR_SIZE +
			sizeof(struct pmu_cmd_lrf_tex_ltc_dram_en_dis);
	cmd.cmd.lrf_tex_ltc_dram.en_dis.cmd_type =
			PMU_LRF_TEX_LTC_DRAM_CMD_ID_EN_DIS;
	cmd.cmd.lrf_tex_ltc_dram.en_dis.en_dis_mask = (u8)(bitmask & 0xff);

	gp10b_dbg_pmu("cmd post PMU_ECC_CMD_ID_EN_DIS_ECC");
	pmu->override_done = 0;
	status = gk20a_pmu_cmd_post(g, &cmd, NULL, NULL, PMU_COMMAND_QUEUE_LPQ,
			pmu_handle_ecc_en_dis_msg, NULL, &seq, ~0);
	if (status)
		gk20a_err(dev_from_gk20a(g), "ECC override failed");
	else
		pmu_wait_message_cond(pmu, gk20a_get_gr_idle_timeout(g),
				      &pmu->override_done, 1);
	gk20a_dbg_fn("done");
	return status;
}

static bool gp10b_is_lazy_bootstrap(u32 falcon_id)
{
	bool enable_status = false;

	switch (falcon_id) {
	case LSF_FALCON_ID_FECS:
		enable_status = false;
		break;
	case LSF_FALCON_ID_GPCCS:
		enable_status = true;
		break;
	default:
		break;
	}

	return enable_status;
}

static bool gp10b_is_priv_load(u32 falcon_id)
{
	bool enable_status = false;

	switch (falcon_id) {
	case LSF_FALCON_ID_FECS:
		enable_status = false;
		break;
	case LSF_FALCON_ID_GPCCS:
		enable_status = true;
		break;
	default:
		break;
	}

	return enable_status;
}

/*Dump Security related fuses*/
static void pmu_dump_security_fuses_gp10b(struct gk20a *g)
{
	gk20a_err(dev_from_gk20a(g), "FUSE_OPT_SEC_DEBUG_EN_0 : 0x%x",
			gk20a_readl(g, fuse_opt_sec_debug_en_r()));
	gk20a_err(dev_from_gk20a(g), "FUSE_OPT_PRIV_SEC_EN_0 : 0x%x",
			gk20a_readl(g, fuse_opt_priv_sec_en_r()));
	gk20a_err(dev_from_gk20a(g), "FUSE_GCPLEX_CONFIG_FUSE_0 : 0x%x",
			tegra_fuse_readl(FUSE_GCPLEX_CONFIG_FUSE_0));
}

void gp10b_init_pmu_ops(struct gpu_ops *gops)
{
	if (gops->privsecurity) {
		gm20b_init_secure_pmu(gops);
		gops->pmu.init_wpr_region = gm20b_pmu_init_acr;
		gops->pmu.load_lsfalcon_ucode = gp10b_load_falcon_ucode;
		gops->pmu.is_lazy_bootstrap = gp10b_is_lazy_bootstrap;
		gops->pmu.is_priv_load = gp10b_is_priv_load;
	} else {
		gk20a_init_pmu_ops(gops);
		gops->pmu.load_lsfalcon_ucode = NULL;
		gops->pmu.init_wpr_region = NULL;
		gops->pmu.pmu_setup_hw_and_bootstrap = gp10b_init_pmu_setup_hw1;
	}
	gops->pmu.pmu_setup_elpg = gp10b_pmu_setup_elpg;
	gops->pmu.lspmuwprinitdone = false;
	gops->pmu.fecsbootstrapdone = false;
	gops->pmu.write_dmatrfbase = gp10b_write_dmatrfbase;
	gops->pmu.pmu_elpg_statistics = gp10b_pmu_elpg_statistics;
	gops->pmu.pmu_pg_init_param = gp10b_pg_gr_init;
	gops->pmu.pmu_pg_supported_engines_list = gk20a_pmu_pg_engines_list;
	gops->pmu.pmu_pg_engines_feature_list = gk20a_pmu_pg_feature_list;
	gops->pmu.pmu_pg_param_post_init = NULL;
	gops->pmu.send_lrf_tex_ltc_dram_overide_en_dis_cmd =
			send_ecc_overide_en_dis_cmd;
	gops->pmu.reset = gk20a_pmu_reset;
	gops->pmu.dump_secure_fuses = pmu_dump_security_fuses_gp10b;
}
