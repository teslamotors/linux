/*
 * GK20A Graphics
 *
 * Copyright (c) 2011-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/delay.h>	/* for udelay */
#include <linux/mm.h>		/* for totalram_pages */
#include <linux/scatterlist.h>
#include <linux/tegra-soc.h>
#include <linux/debugfs.h>
#include <uapi/linux/nvgpu.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/nvhost.h>
#include <linux/sort.h>
#include <linux/bsearch.h>
#include <trace/events/gk20a.h>

#include "gk20a.h"
#include "kind_gk20a.h"
#include "gr_ctx_gk20a.h"
#include "nvgpu_common.h"

#include "hw_ccsr_gk20a.h"
#include "hw_ctxsw_prog_gk20a.h"
#include "hw_fifo_gk20a.h"
#include "hw_gr_gk20a.h"
#include "hw_gmmu_gk20a.h"
#include "hw_mc_gk20a.h"
#include "hw_ram_gk20a.h"
#include "hw_pri_ringmaster_gk20a.h"
#include "hw_pri_ringstation_sys_gk20a.h"
#include "hw_pri_ringstation_gpc_gk20a.h"
#include "hw_pri_ringstation_fbp_gk20a.h"
#include "hw_top_gk20a.h"
#include "hw_ltc_gk20a.h"
#include "hw_fb_gk20a.h"
#include "hw_therm_gk20a.h"
#include "hw_pbdma_gk20a.h"
#include "gr_pri_gk20a.h"
#include "regops_gk20a.h"
#include "dbg_gpu_gk20a.h"
#include "debug_gk20a.h"
#include "semaphore_gk20a.h"
#include "platform_gk20a.h"
#include "ctxsw_trace_gk20a.h"

#define BLK_SIZE (256)
#define NV_PMM_FBP_STRIDE	0x1000
#define NV_PERF_PMM_FBP_ROUTER_STRIDE 0x0200
#define NV_PERF_PMMGPC_CHIPLET_OFFSET	0x1000
#define NV_PERF_PMMGPCROUTER_STRIDE	0x0200
#define NV_PCFG_BASE		0x00088000
#define NV_XBAR_MXBAR_PRI_GPC_GNIC_STRIDE	0x0020
#define FE_PWR_MODE_TIMEOUT_MAX 2000
#define FE_PWR_MODE_TIMEOUT_DEFAULT 10
#define CTXSW_MEM_SCRUBBING_TIMEOUT_MAX 1000
#define CTXSW_MEM_SCRUBBING_TIMEOUT_DEFAULT 10
#define FECS_ARB_CMD_TIMEOUT_MAX 40
#define FECS_ARB_CMD_TIMEOUT_DEFAULT 2

static int gk20a_init_gr_bind_fecs_elpg(struct gk20a *g);
static int gr_gk20a_commit_inst(struct channel_gk20a *c, u64 gpu_va);

/* global ctx buffer */
static int  gr_gk20a_alloc_global_ctx_buffers(struct gk20a *g);
static void gr_gk20a_free_global_ctx_buffers(struct gk20a *g);
static int  gr_gk20a_map_global_ctx_buffers(struct gk20a *g,
					    struct channel_gk20a *c);
static void gr_gk20a_unmap_global_ctx_buffers(struct channel_gk20a *c);

/* channel gr ctx buffer */
static int  gr_gk20a_alloc_channel_gr_ctx(struct gk20a *g,
					struct channel_gk20a *c,
					u32 class, u32 padding);
static void gr_gk20a_free_channel_gr_ctx(struct channel_gk20a *c);

/* channel patch ctx buffer */
static int  gr_gk20a_alloc_channel_patch_ctx(struct gk20a *g,
					struct channel_gk20a *c);
static void gr_gk20a_free_channel_patch_ctx(struct channel_gk20a *c);

/* golden ctx image */
static int gr_gk20a_init_golden_ctx_image(struct gk20a *g,
					  struct channel_gk20a *c);
/*elcg init */
static void gr_gk20a_enable_elcg(struct gk20a *g);

int gr_gk20a_get_ctx_id(struct gk20a *g,
		struct channel_gk20a *c,
		u32 *ctx_id)
{
	struct channel_ctx_gk20a *ch_ctx = &c->ch_ctx;

	/* Channel gr_ctx buffer is gpu cacheable.
	   Flush and invalidate before cpu update. */
	g->ops.mm.l2_flush(g, true);

	if (gk20a_mem_begin(g, &ch_ctx->gr_ctx->mem))
		return -ENOMEM;

	*ctx_id = gk20a_mem_rd(g, &ch_ctx->gr_ctx->mem,
			ctxsw_prog_main_image_context_id_o());

	gk20a_mem_end(g, &ch_ctx->gr_ctx->mem);

	return 0;
}

void gk20a_fecs_dump_falcon_stats(struct gk20a *g)
{
	int i;

	gk20a_err(dev_from_gk20a(g), "gr_fecs_os_r : %d",
		gk20a_readl(g, gr_fecs_os_r()));
	gk20a_err(dev_from_gk20a(g), "gr_fecs_cpuctl_r : 0x%x",
		gk20a_readl(g, gr_fecs_cpuctl_r()));
	gk20a_err(dev_from_gk20a(g), "gr_fecs_idlestate_r : 0x%x",
		gk20a_readl(g, gr_fecs_idlestate_r()));
	gk20a_err(dev_from_gk20a(g), "gr_fecs_mailbox0_r : 0x%x",
		gk20a_readl(g, gr_fecs_mailbox0_r()));
	gk20a_err(dev_from_gk20a(g), "gr_fecs_mailbox1_r : 0x%x",
		gk20a_readl(g, gr_fecs_mailbox1_r()));
	gk20a_err(dev_from_gk20a(g), "gr_fecs_irqstat_r : 0x%x",
		gk20a_readl(g, gr_fecs_irqstat_r()));
	gk20a_err(dev_from_gk20a(g), "gr_fecs_irqmode_r : 0x%x",
		gk20a_readl(g, gr_fecs_irqmode_r()));
	gk20a_err(dev_from_gk20a(g), "gr_fecs_irqmask_r : 0x%x",
		gk20a_readl(g, gr_fecs_irqmask_r()));
	gk20a_err(dev_from_gk20a(g), "gr_fecs_irqdest_r : 0x%x",
		gk20a_readl(g, gr_fecs_irqdest_r()));
	gk20a_err(dev_from_gk20a(g), "gr_fecs_debug1_r : 0x%x",
		gk20a_readl(g, gr_fecs_debug1_r()));
	gk20a_err(dev_from_gk20a(g), "gr_fecs_debuginfo_r : 0x%x",
		gk20a_readl(g, gr_fecs_debuginfo_r()));

	for (i = 0; i < gr_fecs_ctxsw_mailbox__size_1_v(); i++)
		gk20a_err(dev_from_gk20a(g), "gr_fecs_ctxsw_mailbox_r(%d) : 0x%x",
			i, gk20a_readl(g, gr_fecs_ctxsw_mailbox_r(i)));

	gk20a_err(dev_from_gk20a(g), "gr_fecs_engctl_r : 0x%x",
		gk20a_readl(g, gr_fecs_engctl_r()));
	gk20a_err(dev_from_gk20a(g), "gr_fecs_curctx_r : 0x%x",
		gk20a_readl(g, gr_fecs_curctx_r()));
	gk20a_err(dev_from_gk20a(g), "gr_fecs_nxtctx_r : 0x%x",
		gk20a_readl(g, gr_fecs_nxtctx_r()));

	gk20a_writel(g, gr_fecs_icd_cmd_r(),
		gr_fecs_icd_cmd_opc_rreg_f() |
		gr_fecs_icd_cmd_idx_f(PMU_FALCON_REG_IMB));
	gk20a_err(dev_from_gk20a(g), "FECS_FALCON_REG_IMB : 0x%x",
		gk20a_readl(g, gr_fecs_icd_rdata_r()));

	gk20a_writel(g, gr_fecs_icd_cmd_r(),
		gr_fecs_icd_cmd_opc_rreg_f() |
		gr_fecs_icd_cmd_idx_f(PMU_FALCON_REG_DMB));
	gk20a_err(dev_from_gk20a(g), "FECS_FALCON_REG_DMB : 0x%x",
		gk20a_readl(g, gr_fecs_icd_rdata_r()));

	gk20a_writel(g, gr_fecs_icd_cmd_r(),
		gr_fecs_icd_cmd_opc_rreg_f() |
		gr_fecs_icd_cmd_idx_f(PMU_FALCON_REG_CSW));
	gk20a_err(dev_from_gk20a(g), "FECS_FALCON_REG_CSW : 0x%x",
		gk20a_readl(g, gr_fecs_icd_rdata_r()));

	gk20a_writel(g, gr_fecs_icd_cmd_r(),
		gr_fecs_icd_cmd_opc_rreg_f() |
		gr_fecs_icd_cmd_idx_f(PMU_FALCON_REG_CTX));
	gk20a_err(dev_from_gk20a(g), "FECS_FALCON_REG_CTX : 0x%x",
		gk20a_readl(g, gr_fecs_icd_rdata_r()));

	gk20a_writel(g, gr_fecs_icd_cmd_r(),
		gr_fecs_icd_cmd_opc_rreg_f() |
		gr_fecs_icd_cmd_idx_f(PMU_FALCON_REG_EXCI));
	gk20a_err(dev_from_gk20a(g), "FECS_FALCON_REG_EXCI : 0x%x",
		gk20a_readl(g, gr_fecs_icd_rdata_r()));

	for (i = 0; i < 4; i++) {
		gk20a_writel(g, gr_fecs_icd_cmd_r(),
			gr_fecs_icd_cmd_opc_rreg_f() |
			gr_fecs_icd_cmd_idx_f(PMU_FALCON_REG_PC));
		gk20a_err(dev_from_gk20a(g), "FECS_FALCON_REG_PC : 0x%x",
			gk20a_readl(g, gr_fecs_icd_rdata_r()));

		gk20a_writel(g, gr_fecs_icd_cmd_r(),
			gr_fecs_icd_cmd_opc_rreg_f() |
			gr_fecs_icd_cmd_idx_f(PMU_FALCON_REG_SP));
		gk20a_err(dev_from_gk20a(g), "FECS_FALCON_REG_SP : 0x%x",
			gk20a_readl(g, gr_fecs_icd_rdata_r()));
	}
}

static void gr_gk20a_load_falcon_dmem(struct gk20a *g)
{
	u32 i, ucode_u32_size;
	const u32 *ucode_u32_data;
	u32 checksum;

	gk20a_dbg_fn("");

	gk20a_writel(g, gr_gpccs_dmemc_r(0), (gr_gpccs_dmemc_offs_f(0) |
					      gr_gpccs_dmemc_blk_f(0)  |
					      gr_gpccs_dmemc_aincw_f(1)));

	ucode_u32_size = g->gr.ctx_vars.ucode.gpccs.data.count;
	ucode_u32_data = (const u32 *)g->gr.ctx_vars.ucode.gpccs.data.l;

	for (i = 0, checksum = 0; i < ucode_u32_size; i++) {
		gk20a_writel(g, gr_gpccs_dmemd_r(0), ucode_u32_data[i]);
		checksum += ucode_u32_data[i];
	}

	gk20a_writel(g, gr_fecs_dmemc_r(0), (gr_fecs_dmemc_offs_f(0) |
					     gr_fecs_dmemc_blk_f(0)  |
					     gr_fecs_dmemc_aincw_f(1)));

	ucode_u32_size = g->gr.ctx_vars.ucode.fecs.data.count;
	ucode_u32_data = (const u32 *)g->gr.ctx_vars.ucode.fecs.data.l;

	for (i = 0, checksum = 0; i < ucode_u32_size; i++) {
		gk20a_writel(g, gr_fecs_dmemd_r(0), ucode_u32_data[i]);
		checksum += ucode_u32_data[i];
	}
	gk20a_dbg_fn("done");
}

static void gr_gk20a_load_falcon_imem(struct gk20a *g)
{
	u32 cfg, fecs_imem_size, gpccs_imem_size, ucode_u32_size;
	const u32 *ucode_u32_data;
	u32 tag, i, pad_start, pad_end;
	u32 checksum;

	gk20a_dbg_fn("");

	cfg = gk20a_readl(g, gr_fecs_cfg_r());
	fecs_imem_size = gr_fecs_cfg_imem_sz_v(cfg);

	cfg = gk20a_readl(g, gr_gpc0_cfg_r());
	gpccs_imem_size = gr_gpc0_cfg_imem_sz_v(cfg);

	/* Use the broadcast address to access all of the GPCCS units. */
	gk20a_writel(g, gr_gpccs_imemc_r(0), (gr_gpccs_imemc_offs_f(0) |
					      gr_gpccs_imemc_blk_f(0) |
					      gr_gpccs_imemc_aincw_f(1)));

	/* Setup the tags for the instruction memory. */
	tag = 0;
	gk20a_writel(g, gr_gpccs_imemt_r(0), gr_gpccs_imemt_tag_f(tag));

	ucode_u32_size = g->gr.ctx_vars.ucode.gpccs.inst.count;
	ucode_u32_data = (const u32 *)g->gr.ctx_vars.ucode.gpccs.inst.l;

	for (i = 0, checksum = 0; i < ucode_u32_size; i++) {
		if (i && ((i % (256/sizeof(u32))) == 0)) {
			tag++;
			gk20a_writel(g, gr_gpccs_imemt_r(0),
				      gr_gpccs_imemt_tag_f(tag));
		}
		gk20a_writel(g, gr_gpccs_imemd_r(0), ucode_u32_data[i]);
		checksum += ucode_u32_data[i];
	}

	pad_start = i*4;
	pad_end = pad_start+(256-pad_start%256)+256;
	for (i = pad_start;
	     (i < gpccs_imem_size * 256) && (i < pad_end);
	     i += 4) {
		if (i && ((i % 256) == 0)) {
			tag++;
			gk20a_writel(g, gr_gpccs_imemt_r(0),
				      gr_gpccs_imemt_tag_f(tag));
		}
		gk20a_writel(g, gr_gpccs_imemd_r(0), 0);
	}

	gk20a_writel(g, gr_fecs_imemc_r(0), (gr_fecs_imemc_offs_f(0) |
					     gr_fecs_imemc_blk_f(0) |
					     gr_fecs_imemc_aincw_f(1)));

	/* Setup the tags for the instruction memory. */
	tag = 0;
	gk20a_writel(g, gr_fecs_imemt_r(0), gr_fecs_imemt_tag_f(tag));

	ucode_u32_size = g->gr.ctx_vars.ucode.fecs.inst.count;
	ucode_u32_data = (const u32 *)g->gr.ctx_vars.ucode.fecs.inst.l;

	for (i = 0, checksum = 0; i < ucode_u32_size; i++) {
		if (i && ((i % (256/sizeof(u32))) == 0)) {
			tag++;
			gk20a_writel(g, gr_fecs_imemt_r(0),
				      gr_fecs_imemt_tag_f(tag));
		}
		gk20a_writel(g, gr_fecs_imemd_r(0), ucode_u32_data[i]);
		checksum += ucode_u32_data[i];
	}

	pad_start = i*4;
	pad_end = pad_start+(256-pad_start%256)+256;
	for (i = pad_start; (i < fecs_imem_size * 256) && i < pad_end; i += 4) {
		if (i && ((i % 256) == 0)) {
			tag++;
			gk20a_writel(g, gr_fecs_imemt_r(0),
				      gr_fecs_imemt_tag_f(tag));
		}
		gk20a_writel(g, gr_fecs_imemd_r(0), 0);
	}
}

int gr_gk20a_wait_idle(struct gk20a *g, unsigned long end_jiffies,
		       u32 expect_delay)
{
	u32 delay = expect_delay;
	bool gr_enabled;
	bool ctxsw_active;
	bool gr_busy;
	u32 gr_engine_id;

	gk20a_dbg_fn("");

	gr_engine_id = gk20a_fifo_get_gr_engine_id(g);

	do {
		/* fmodel: host gets fifo_engine_status(gr) from gr
		   only when gr_status is read */
		gk20a_readl(g, gr_status_r());

		gr_enabled = gk20a_readl(g, mc_enable_r()) &
			mc_enable_pgraph_enabled_f();

		ctxsw_active = gk20a_readl(g,
			fifo_engine_status_r(gr_engine_id)) &
			fifo_engine_status_ctxsw_in_progress_f();

		gr_busy = gk20a_readl(g, gr_engine_status_r()) &
			gr_engine_status_value_busy_f();

		if (!gr_enabled || (!gr_busy && !ctxsw_active)) {
			gk20a_dbg_fn("done");
			return 0;
		}

		usleep_range(delay, delay * 2);
		delay = min_t(u32, delay << 1, GR_IDLE_CHECK_MAX);

	} while (time_before(jiffies, end_jiffies)
			|| !tegra_platform_is_silicon());

	gk20a_err(dev_from_gk20a(g),
		"timeout, ctxsw busy : %d, gr busy : %d",
		ctxsw_active, gr_busy);

	return -EAGAIN;
}

static int gr_gk20a_wait_fe_idle(struct gk20a *g, unsigned long end_jiffies,
		u32 expect_delay)
{
	u32 val;
	u32 delay = expect_delay;

	if (tegra_platform_is_linsim())
		return 0;

	gk20a_dbg_fn("");

	do {
		val = gk20a_readl(g, gr_status_r());

		if (!gr_status_fe_method_lower_v(val)) {
			gk20a_dbg_fn("done");
			return 0;
		}

		usleep_range(delay, delay * 2);
		delay = min_t(u32, delay << 1, GR_IDLE_CHECK_MAX);
	} while (time_before(jiffies, end_jiffies)
			|| !tegra_platform_is_silicon());

	gk20a_err(dev_from_gk20a(g),
		"timeout, fe busy : %x", val);

	return -EAGAIN;
}

int gr_gk20a_ctx_wait_ucode(struct gk20a *g, u32 mailbox_id,
			    u32 *mailbox_ret, u32 opc_success,
			    u32 mailbox_ok, u32 opc_fail,
			    u32 mailbox_fail, bool sleepduringwait)
{
	unsigned long end_jiffies = jiffies +
		msecs_to_jiffies(gk20a_get_gr_idle_timeout(g));
	u32 delay = GR_FECS_POLL_INTERVAL;
	u32 check = WAIT_UCODE_LOOP;
	u32 reg;

	gk20a_dbg_fn("");

	if (sleepduringwait)
		delay = GR_IDLE_CHECK_DEFAULT;

	while (check == WAIT_UCODE_LOOP) {
		if (!time_before(jiffies, end_jiffies) &&
				tegra_platform_is_silicon())
			check = WAIT_UCODE_TIMEOUT;

		reg = gk20a_readl(g, gr_fecs_ctxsw_mailbox_r(mailbox_id));

		if (mailbox_ret)
			*mailbox_ret = reg;

		switch (opc_success) {
		case GR_IS_UCODE_OP_EQUAL:
			if (reg == mailbox_ok)
				check = WAIT_UCODE_OK;
			break;
		case GR_IS_UCODE_OP_NOT_EQUAL:
			if (reg != mailbox_ok)
				check = WAIT_UCODE_OK;
			break;
		case GR_IS_UCODE_OP_AND:
			if (reg & mailbox_ok)
				check = WAIT_UCODE_OK;
			break;
		case GR_IS_UCODE_OP_LESSER:
			if (reg < mailbox_ok)
				check = WAIT_UCODE_OK;
			break;
		case GR_IS_UCODE_OP_LESSER_EQUAL:
			if (reg <= mailbox_ok)
				check = WAIT_UCODE_OK;
			break;
		case GR_IS_UCODE_OP_SKIP:
			/* do no success check */
			break;
		default:
			gk20a_err(dev_from_gk20a(g),
				   "invalid success opcode 0x%x", opc_success);

			check = WAIT_UCODE_ERROR;
			break;
		}

		switch (opc_fail) {
		case GR_IS_UCODE_OP_EQUAL:
			if (reg == mailbox_fail)
				check = WAIT_UCODE_ERROR;
			break;
		case GR_IS_UCODE_OP_NOT_EQUAL:
			if (reg != mailbox_fail)
				check = WAIT_UCODE_ERROR;
			break;
		case GR_IS_UCODE_OP_AND:
			if (reg & mailbox_fail)
				check = WAIT_UCODE_ERROR;
			break;
		case GR_IS_UCODE_OP_LESSER:
			if (reg < mailbox_fail)
				check = WAIT_UCODE_ERROR;
			break;
		case GR_IS_UCODE_OP_LESSER_EQUAL:
			if (reg <= mailbox_fail)
				check = WAIT_UCODE_ERROR;
			break;
		case GR_IS_UCODE_OP_SKIP:
			/* do no check on fail*/
			break;
		default:
			gk20a_err(dev_from_gk20a(g),
				   "invalid fail opcode 0x%x", opc_fail);
			check = WAIT_UCODE_ERROR;
			break;
		}

		if (sleepduringwait) {
			usleep_range(delay, delay * 2);
			delay = min_t(u32, delay << 1, GR_IDLE_CHECK_MAX);
		} else
			udelay(delay);
	}

	if (check == WAIT_UCODE_TIMEOUT) {
		gk20a_err(dev_from_gk20a(g),
			   "timeout waiting on ucode response");
		gk20a_fecs_dump_falcon_stats(g);
		gk20a_gr_debug_dump(g->dev);
		return -1;
	} else if (check == WAIT_UCODE_ERROR) {
		gk20a_err(dev_from_gk20a(g),
			   "ucode method failed on mailbox=%d value=0x%08x",
			   mailbox_id, reg);
		gk20a_fecs_dump_falcon_stats(g);
		return -1;
	}

	gk20a_dbg_fn("done");
	return 0;
}

/* The following is a less brittle way to call gr_gk20a_submit_fecs_method(...)
 * We should replace most, if not all, fecs method calls to this instead. */
int gr_gk20a_submit_fecs_method_op(struct gk20a *g,
				   struct fecs_method_op_gk20a op,
				   bool sleepduringwait)
{
	struct gr_gk20a *gr = &g->gr;
	int ret;

	mutex_lock(&gr->fecs_mutex);

	if (op.mailbox.id != 0)
		gk20a_writel(g, gr_fecs_ctxsw_mailbox_r(op.mailbox.id),
			     op.mailbox.data);

	gk20a_writel(g, gr_fecs_ctxsw_mailbox_clear_r(0),
		gr_fecs_ctxsw_mailbox_clear_value_f(op.mailbox.clr));

	gk20a_writel(g, gr_fecs_method_data_r(), op.method.data);
	gk20a_writel(g, gr_fecs_method_push_r(),
		gr_fecs_method_push_adr_f(op.method.addr));

	/* op.mb.id == 4 cases require waiting for completion on
	 * for op.mb.id == 0 */
	if (op.mailbox.id == 4)
		op.mailbox.id = 0;

	ret = gr_gk20a_ctx_wait_ucode(g, op.mailbox.id, op.mailbox.ret,
				      op.cond.ok, op.mailbox.ok,
				      op.cond.fail, op.mailbox.fail,
				      sleepduringwait);

	mutex_unlock(&gr->fecs_mutex);

	return ret;
}

/* Sideband mailbox writes are done a bit differently */
int gr_gk20a_submit_fecs_sideband_method_op(struct gk20a *g,
		struct fecs_method_op_gk20a op)
{
	struct gr_gk20a *gr = &g->gr;
	int ret;

	mutex_lock(&gr->fecs_mutex);

	gk20a_writel(g, gr_fecs_ctxsw_mailbox_clear_r(op.mailbox.id),
		gr_fecs_ctxsw_mailbox_clear_value_f(op.mailbox.clr));

	gk20a_writel(g, gr_fecs_method_data_r(), op.method.data);
	gk20a_writel(g, gr_fecs_method_push_r(),
		gr_fecs_method_push_adr_f(op.method.addr));

	ret = gr_gk20a_ctx_wait_ucode(g, op.mailbox.id, op.mailbox.ret,
				      op.cond.ok, op.mailbox.ok,
				      op.cond.fail, op.mailbox.fail,
				      false);

	mutex_unlock(&gr->fecs_mutex);

	return ret;
}

static int gr_gk20a_ctrl_ctxsw(struct gk20a *g, u32 fecs_method, u32 *ret)
{
	return gr_gk20a_submit_fecs_method_op(g,
	      (struct fecs_method_op_gk20a) {
		      .method.addr = fecs_method,
		      .method.data = ~0,
		      .mailbox = { .id   = 1, /*sideband?*/
				   .data = ~0, .clr = ~0, .ret = ret,
				   .ok   = gr_fecs_ctxsw_mailbox_value_pass_v(),
				   .fail = gr_fecs_ctxsw_mailbox_value_fail_v(), },
		      .cond.ok = GR_IS_UCODE_OP_EQUAL,
		      .cond.fail = GR_IS_UCODE_OP_EQUAL }, true);
}

/* Stop processing (stall) context switches at FECS.
 * The caller must hold the dbg_sessions_lock, else if mutliple stop methods
 * are sent to the ucode in sequence, it can get into an undefined state. */
int gr_gk20a_disable_ctxsw(struct gk20a *g)
{
	gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg, "");
	return gr_gk20a_ctrl_ctxsw(g,
			gr_fecs_method_push_adr_stop_ctxsw_v(), NULL);
}

/* Start processing (continue) context switches at FECS */
int gr_gk20a_enable_ctxsw(struct gk20a *g)
{
	gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg, "");
	return gr_gk20a_ctrl_ctxsw(g,
			gr_fecs_method_push_adr_start_ctxsw_v(), NULL);
}

int gr_gk20a_halt_pipe(struct gk20a *g)
{
	return gr_gk20a_submit_fecs_method_op(g,
	      (struct fecs_method_op_gk20a) {
		      .method.addr =
				gr_fecs_method_push_adr_halt_pipeline_v(),
		      .method.data = ~0,
		      .mailbox = { .id   = 1, /*sideband?*/
				.data = ~0, .clr = ~0, .ret = NULL,
				.ok   = gr_fecs_ctxsw_mailbox_value_pass_v(),
				.fail = gr_fecs_ctxsw_mailbox_value_fail_v(), },
		      .cond.ok = GR_IS_UCODE_OP_EQUAL,
		      .cond.fail = GR_IS_UCODE_OP_EQUAL }, false);
}


static int gr_gk20a_commit_inst(struct channel_gk20a *c, u64 gpu_va)
{
	u32 addr_lo;
	u32 addr_hi;

	gk20a_dbg_fn("");

	addr_lo = u64_lo32(gpu_va) >> 12;
	addr_hi = u64_hi32(gpu_va);

	gk20a_mem_wr32(c->g, &c->inst_block, ram_in_gr_wfi_target_w(),
		 ram_in_gr_cs_wfi_f() | ram_in_gr_wfi_mode_virtual_f() |
		 ram_in_gr_wfi_ptr_lo_f(addr_lo));

	gk20a_mem_wr32(c->g, &c->inst_block, ram_in_gr_wfi_ptr_hi_w(),
		 ram_in_gr_wfi_ptr_hi_f(addr_hi));

	return 0;
}

/*
 * Context state can be written directly, or "patched" at times. So that code
 * can be used in either situation it is written using a series of
 * _ctx_patch_write(..., patch) statements. However any necessary map overhead
 * should be minimized; thus, bundle the sequence of these writes together, and
 * set them up and close with _ctx_patch_write_begin/_ctx_patch_write_end.
 */

int gr_gk20a_ctx_patch_write_begin(struct gk20a *g,
					  struct channel_ctx_gk20a *ch_ctx)
{
	return gk20a_mem_begin(g, &ch_ctx->patch_ctx.mem);
}

void gr_gk20a_ctx_patch_write_end(struct gk20a *g,
					struct channel_ctx_gk20a *ch_ctx)
{
	gk20a_mem_end(g, &ch_ctx->patch_ctx.mem);
}

void gr_gk20a_ctx_patch_write(struct gk20a *g,
				    struct channel_ctx_gk20a *ch_ctx,
				    u32 addr, u32 data, bool patch)
{
	if (patch) {
		u32 patch_slot = ch_ctx->patch_ctx.data_count * 2;
		gk20a_mem_wr32(g, &ch_ctx->patch_ctx.mem, patch_slot, addr);
		gk20a_mem_wr32(g, &ch_ctx->patch_ctx.mem, patch_slot + 1, data);
		ch_ctx->patch_ctx.data_count++;
	} else {
		gk20a_writel(g, addr, data);
	}
}

static u32 fecs_current_ctx_data(struct gk20a *g, struct mem_desc *inst_block)
{
	u32 ptr = u64_lo32(gk20a_mm_inst_block_addr(g, inst_block)
			>> ram_in_base_shift_v());
	u32 aperture = gk20a_aperture_mask(g, inst_block,
			gr_fecs_current_ctx_target_sys_mem_ncoh_f(),
			gr_fecs_current_ctx_target_vid_mem_f());

	return gr_fecs_current_ctx_ptr_f(ptr) | aperture |
		gr_fecs_current_ctx_valid_f(1);
}

static int gr_gk20a_fecs_ctx_bind_channel(struct gk20a *g,
					struct channel_gk20a *c)
{
	u32 inst_base_ptr = u64_lo32(gk20a_mm_inst_block_addr(g, &c->inst_block)
				     >> ram_in_base_shift_v());
	u32 data = fecs_current_ctx_data(g, &c->inst_block);
	u32 ret;

	gk20a_dbg_info("bind channel %d inst ptr 0x%08x",
		   c->hw_chid, inst_base_ptr);

	ret = gr_gk20a_submit_fecs_method_op(g,
		     (struct fecs_method_op_gk20a) {
		     .method.addr = gr_fecs_method_push_adr_bind_pointer_v(),
		     .method.data = data,
		     .mailbox = { .id = 0, .data = 0,
				  .clr = 0x30,
				  .ret = NULL,
				  .ok = 0x10,
				  .fail = 0x20, },
		     .cond.ok = GR_IS_UCODE_OP_AND,
		     .cond.fail = GR_IS_UCODE_OP_AND}, true);
	if (ret)
		gk20a_err(dev_from_gk20a(g),
			"bind channel instance failed");

	return ret;
}

static int gr_gk20a_ctx_zcull_setup(struct gk20a *g, struct channel_gk20a *c)
{
	struct channel_ctx_gk20a *ch_ctx = &c->ch_ctx;
	struct mem_desc *mem = &ch_ctx->gr_ctx->mem;
	u32 va_lo, va_hi, va;
	int ret = 0;

	gk20a_dbg_fn("");

	if (gk20a_mem_begin(g, mem))
		return -ENOMEM;

	if (ch_ctx->zcull_ctx.gpu_va == 0 &&
	    ch_ctx->zcull_ctx.ctx_sw_mode ==
		ctxsw_prog_main_image_zcull_mode_separate_buffer_v()) {
		ret = -EINVAL;
		goto clean_up;
	}

	va_lo = u64_lo32(ch_ctx->zcull_ctx.gpu_va);
	va_hi = u64_hi32(ch_ctx->zcull_ctx.gpu_va);
	va = ((va_lo >> 8) & 0x00FFFFFF) | ((va_hi << 24) & 0xFF000000);

	ret = gk20a_disable_channel_tsg(g, c);
	if (ret) {
		gk20a_err(dev_from_gk20a(g), "failed to disable channel/TSG\n");
		goto clean_up;
	}
	ret = gk20a_fifo_preempt(g, c);
	if (ret) {
		gk20a_enable_channel_tsg(g, c);
		gk20a_err(dev_from_gk20a(g), "failed to preempt channel/TSG\n");
		goto clean_up;
	}

	gk20a_mem_wr(g, mem,
			ctxsw_prog_main_image_zcull_o(),
		 ch_ctx->zcull_ctx.ctx_sw_mode);

	gk20a_mem_wr(g, mem,
			ctxsw_prog_main_image_zcull_ptr_o(), va);

	gk20a_enable_channel_tsg(g, c);

clean_up:
	gk20a_mem_end(g, mem);

	return ret;
}

static int gr_gk20a_commit_global_cb_manager(struct gk20a *g,
			struct channel_gk20a *c, bool patch)
{
	struct gr_gk20a *gr = &g->gr;
	struct channel_ctx_gk20a *ch_ctx = &c->ch_ctx;
	u32 attrib_offset_in_chunk = 0;
	u32 alpha_offset_in_chunk = 0;
	u32 pd_ab_max_output;
	u32 gpc_index, ppc_index;
	u32 temp;
	u32 cbm_cfg_size1, cbm_cfg_size2;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 ppc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_PPC_IN_GPC_STRIDE);

	gk20a_dbg_fn("");

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_ds_tga_constraintlogic_r(),
		gr_ds_tga_constraintlogic_beta_cbsize_f(gr->attrib_cb_default_size) |
		gr_ds_tga_constraintlogic_alpha_cbsize_f(gr->alpha_cb_default_size),
		patch);

	pd_ab_max_output = (gr->alpha_cb_default_size *
		gr_gpc0_ppc0_cbm_cfg_size_granularity_v()) /
		gr_pd_ab_dist_cfg1_max_output_granularity_v();

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_pd_ab_dist_cfg1_r(),
		gr_pd_ab_dist_cfg1_max_output_f(pd_ab_max_output) |
		gr_pd_ab_dist_cfg1_max_batches_init_f(), patch);

	alpha_offset_in_chunk = attrib_offset_in_chunk +
		gr->tpc_count * gr->attrib_cb_size;

	for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++) {
		temp = gpc_stride * gpc_index;
		for (ppc_index = 0; ppc_index < gr->gpc_ppc_count[gpc_index];
		     ppc_index++) {
			cbm_cfg_size1 = gr->attrib_cb_default_size *
				gr->pes_tpc_count[ppc_index][gpc_index];
			cbm_cfg_size2 = gr->alpha_cb_default_size *
				gr->pes_tpc_count[ppc_index][gpc_index];

			gr_gk20a_ctx_patch_write(g, ch_ctx,
				gr_gpc0_ppc0_cbm_cfg_r() + temp +
				ppc_in_gpc_stride * ppc_index,
				gr_gpc0_ppc0_cbm_cfg_timeslice_mode_f(gr->timeslice_mode) |
				gr_gpc0_ppc0_cbm_cfg_start_offset_f(attrib_offset_in_chunk) |
				gr_gpc0_ppc0_cbm_cfg_size_f(cbm_cfg_size1), patch);

			attrib_offset_in_chunk += gr->attrib_cb_size *
				gr->pes_tpc_count[ppc_index][gpc_index];

			gr_gk20a_ctx_patch_write(g, ch_ctx,
				gr_gpc0_ppc0_cbm_cfg2_r() + temp +
				ppc_in_gpc_stride * ppc_index,
				gr_gpc0_ppc0_cbm_cfg2_start_offset_f(alpha_offset_in_chunk) |
				gr_gpc0_ppc0_cbm_cfg2_size_f(cbm_cfg_size2), patch);

			alpha_offset_in_chunk += gr->alpha_cb_size *
				gr->pes_tpc_count[ppc_index][gpc_index];
		}
	}

	return 0;
}

static int gr_gk20a_commit_global_ctx_buffers(struct gk20a *g,
			struct channel_gk20a *c, bool patch)
{
	struct gr_gk20a *gr = &g->gr;
	struct channel_ctx_gk20a *ch_ctx = &c->ch_ctx;
	u64 addr;
	u32 size;

	gk20a_dbg_fn("");
	if (patch) {
		int err;
		err = gr_gk20a_ctx_patch_write_begin(g, ch_ctx);
		if (err)
			return err;
	}

	/* global pagepool buffer */
	addr = (u64_lo32(ch_ctx->global_ctx_buffer_va[PAGEPOOL_VA]) >>
		gr_scc_pagepool_base_addr_39_8_align_bits_v()) |
		(u64_hi32(ch_ctx->global_ctx_buffer_va[PAGEPOOL_VA]) <<
		 (32 - gr_scc_pagepool_base_addr_39_8_align_bits_v()));

	size = gr->global_ctx_buffer[PAGEPOOL].mem.size /
		gr_scc_pagepool_total_pages_byte_granularity_v();

	if (size == g->ops.gr.pagepool_default_size(g))
		size = gr_scc_pagepool_total_pages_hwmax_v();

	gk20a_dbg_info("pagepool buffer addr : 0x%016llx, size : %d",
		addr, size);

	g->ops.gr.commit_global_pagepool(g, ch_ctx, addr, size, patch);

	/* global bundle cb */
	addr = (u64_lo32(ch_ctx->global_ctx_buffer_va[CIRCULAR_VA]) >>
		gr_scc_bundle_cb_base_addr_39_8_align_bits_v()) |
		(u64_hi32(ch_ctx->global_ctx_buffer_va[CIRCULAR_VA]) <<
		 (32 - gr_scc_bundle_cb_base_addr_39_8_align_bits_v()));

	size = gr->bundle_cb_default_size;

	gk20a_dbg_info("bundle cb addr : 0x%016llx, size : %d",
		addr, size);

	g->ops.gr.commit_global_bundle_cb(g, ch_ctx, addr, size, patch);

	/* global attrib cb */
	addr = (u64_lo32(ch_ctx->global_ctx_buffer_va[ATTRIBUTE_VA]) >>
		gr_gpcs_setup_attrib_cb_base_addr_39_12_align_bits_v()) |
		(u64_hi32(ch_ctx->global_ctx_buffer_va[ATTRIBUTE_VA]) <<
		 (32 - gr_gpcs_setup_attrib_cb_base_addr_39_12_align_bits_v()));

	gk20a_dbg_info("attrib cb addr : 0x%016llx", addr);
	g->ops.gr.commit_global_attrib_cb(g, ch_ctx, addr, patch);
	g->ops.gr.commit_global_cb_manager(g, c, patch);

	if (patch)
		gr_gk20a_ctx_patch_write_end(g, ch_ctx);

	return 0;
}

static void gr_gk20a_commit_global_attrib_cb(struct gk20a *g,
					    struct channel_ctx_gk20a *ch_ctx,
					    u64 addr, bool patch)
{
	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_setup_attrib_cb_base_r(),
		gr_gpcs_setup_attrib_cb_base_addr_39_12_f(addr) |
		gr_gpcs_setup_attrib_cb_base_valid_true_f(), patch);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_tpcs_pe_pin_cb_global_base_addr_r(),
		gr_gpcs_tpcs_pe_pin_cb_global_base_addr_v_f(addr) |
		gr_gpcs_tpcs_pe_pin_cb_global_base_addr_valid_true_f(), patch);
}

static void gr_gk20a_commit_global_bundle_cb(struct gk20a *g,
					    struct channel_ctx_gk20a *ch_ctx,
					    u64 addr, u64 size, bool patch)
{
	u32 data;

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_scc_bundle_cb_base_r(),
		gr_scc_bundle_cb_base_addr_39_8_f(addr), patch);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_scc_bundle_cb_size_r(),
		gr_scc_bundle_cb_size_div_256b_f(size) |
		gr_scc_bundle_cb_size_valid_true_f(), patch);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_setup_bundle_cb_base_r(),
		gr_gpcs_setup_bundle_cb_base_addr_39_8_f(addr), patch);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_setup_bundle_cb_size_r(),
		gr_gpcs_setup_bundle_cb_size_div_256b_f(size) |
		gr_gpcs_setup_bundle_cb_size_valid_true_f(), patch);

	/* data for state_limit */
	data = (g->gr.bundle_cb_default_size *
		gr_scc_bundle_cb_size_div_256b_byte_granularity_v()) /
		gr_pd_ab_dist_cfg2_state_limit_scc_bundle_granularity_v();

	data = min_t(u32, data, g->gr.min_gpm_fifo_depth);

	gk20a_dbg_info("bundle cb token limit : %d, state limit : %d",
		   g->gr.bundle_cb_token_limit, data);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_pd_ab_dist_cfg2_r(),
		gr_pd_ab_dist_cfg2_token_limit_f(g->gr.bundle_cb_token_limit) |
		gr_pd_ab_dist_cfg2_state_limit_f(data), patch);

}

static int gr_gk20a_commit_global_timeslice(struct gk20a *g, struct channel_gk20a *c, bool patch)
{
	struct gr_gk20a *gr = &g->gr;
	struct channel_ctx_gk20a *ch_ctx = NULL;
	u32 gpm_pd_cfg;
	u32 pd_ab_dist_cfg0;
	u32 ds_debug;
	u32 mpc_vtg_debug;
	u32 pe_vaf;
	u32 pe_vsc_vpc;

	gk20a_dbg_fn("");

	gpm_pd_cfg = gk20a_readl(g, gr_gpcs_gpm_pd_cfg_r());
	pd_ab_dist_cfg0 = gk20a_readl(g, gr_pd_ab_dist_cfg0_r());
	ds_debug = gk20a_readl(g, gr_ds_debug_r());
	mpc_vtg_debug = gk20a_readl(g, gr_gpcs_tpcs_mpc_vtg_debug_r());

	if (patch) {
		int err;
		ch_ctx = &c->ch_ctx;
		err = gr_gk20a_ctx_patch_write_begin(g, ch_ctx);
		if (err)
			return err;
	}

	if (gr->timeslice_mode == gr_gpcs_ppcs_cbm_cfg_timeslice_mode_enable_v()) {
		pe_vaf = gk20a_readl(g, gr_gpcs_tpcs_pe_vaf_r());
		pe_vsc_vpc = gk20a_readl(g, gr_gpcs_tpcs_pes_vsc_vpc_r());

		gpm_pd_cfg = gr_gpcs_gpm_pd_cfg_timeslice_mode_enable_f() | gpm_pd_cfg;
		pe_vaf = gr_gpcs_tpcs_pe_vaf_fast_mode_switch_true_f() | pe_vaf;
		pe_vsc_vpc = gr_gpcs_tpcs_pes_vsc_vpc_fast_mode_switch_true_f() | pe_vsc_vpc;
		pd_ab_dist_cfg0 = gr_pd_ab_dist_cfg0_timeslice_enable_en_f() | pd_ab_dist_cfg0;
		ds_debug = gr_ds_debug_timeslice_mode_enable_f() | ds_debug;
		mpc_vtg_debug = gr_gpcs_tpcs_mpc_vtg_debug_timeslice_mode_enabled_f() | mpc_vtg_debug;

		gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_gpm_pd_cfg_r(), gpm_pd_cfg, patch);
		gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_tpcs_pe_vaf_r(), pe_vaf, patch);
		gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_tpcs_pes_vsc_vpc_r(), pe_vsc_vpc, patch);
		gr_gk20a_ctx_patch_write(g, ch_ctx, gr_pd_ab_dist_cfg0_r(), pd_ab_dist_cfg0, patch);
		gr_gk20a_ctx_patch_write(g, ch_ctx, gr_ds_debug_r(), ds_debug, patch);
		gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_tpcs_mpc_vtg_debug_r(), mpc_vtg_debug, patch);
	} else {
		gpm_pd_cfg = gr_gpcs_gpm_pd_cfg_timeslice_mode_disable_f() | gpm_pd_cfg;
		pd_ab_dist_cfg0 = gr_pd_ab_dist_cfg0_timeslice_enable_dis_f() | pd_ab_dist_cfg0;
		ds_debug = gr_ds_debug_timeslice_mode_disable_f() | ds_debug;
		mpc_vtg_debug = gr_gpcs_tpcs_mpc_vtg_debug_timeslice_mode_disabled_f() | mpc_vtg_debug;

		gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_gpm_pd_cfg_r(), gpm_pd_cfg, patch);
		gr_gk20a_ctx_patch_write(g, ch_ctx, gr_pd_ab_dist_cfg0_r(), pd_ab_dist_cfg0, patch);
		gr_gk20a_ctx_patch_write(g, ch_ctx, gr_ds_debug_r(), ds_debug, patch);
		gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_tpcs_mpc_vtg_debug_r(), mpc_vtg_debug, patch);
	}

	if (patch)
		gr_gk20a_ctx_patch_write_end(g, ch_ctx);

	return 0;
}

int gr_gk20a_setup_rop_mapping(struct gk20a *g, struct gr_gk20a *gr)
{
	u32 norm_entries, norm_shift;
	u32 coeff5_mod, coeff6_mod, coeff7_mod, coeff8_mod, coeff9_mod, coeff10_mod, coeff11_mod;
	u32 map0, map1, map2, map3, map4, map5;

	if (!gr->map_tiles)
		return -1;

	gk20a_dbg_fn("");

	gk20a_writel(g, gr_crstr_map_table_cfg_r(),
		     gr_crstr_map_table_cfg_row_offset_f(gr->map_row_offset) |
		     gr_crstr_map_table_cfg_num_entries_f(gr->tpc_count));

	map0 =  gr_crstr_gpc_map0_tile0_f(gr->map_tiles[0]) |
		gr_crstr_gpc_map0_tile1_f(gr->map_tiles[1]) |
		gr_crstr_gpc_map0_tile2_f(gr->map_tiles[2]) |
		gr_crstr_gpc_map0_tile3_f(gr->map_tiles[3]) |
		gr_crstr_gpc_map0_tile4_f(gr->map_tiles[4]) |
		gr_crstr_gpc_map0_tile5_f(gr->map_tiles[5]);

	map1 =  gr_crstr_gpc_map1_tile6_f(gr->map_tiles[6]) |
		gr_crstr_gpc_map1_tile7_f(gr->map_tiles[7]) |
		gr_crstr_gpc_map1_tile8_f(gr->map_tiles[8]) |
		gr_crstr_gpc_map1_tile9_f(gr->map_tiles[9]) |
		gr_crstr_gpc_map1_tile10_f(gr->map_tiles[10]) |
		gr_crstr_gpc_map1_tile11_f(gr->map_tiles[11]);

	map2 =  gr_crstr_gpc_map2_tile12_f(gr->map_tiles[12]) |
		gr_crstr_gpc_map2_tile13_f(gr->map_tiles[13]) |
		gr_crstr_gpc_map2_tile14_f(gr->map_tiles[14]) |
		gr_crstr_gpc_map2_tile15_f(gr->map_tiles[15]) |
		gr_crstr_gpc_map2_tile16_f(gr->map_tiles[16]) |
		gr_crstr_gpc_map2_tile17_f(gr->map_tiles[17]);

	map3 =  gr_crstr_gpc_map3_tile18_f(gr->map_tiles[18]) |
		gr_crstr_gpc_map3_tile19_f(gr->map_tiles[19]) |
		gr_crstr_gpc_map3_tile20_f(gr->map_tiles[20]) |
		gr_crstr_gpc_map3_tile21_f(gr->map_tiles[21]) |
		gr_crstr_gpc_map3_tile22_f(gr->map_tiles[22]) |
		gr_crstr_gpc_map3_tile23_f(gr->map_tiles[23]);

	map4 =  gr_crstr_gpc_map4_tile24_f(gr->map_tiles[24]) |
		gr_crstr_gpc_map4_tile25_f(gr->map_tiles[25]) |
		gr_crstr_gpc_map4_tile26_f(gr->map_tiles[26]) |
		gr_crstr_gpc_map4_tile27_f(gr->map_tiles[27]) |
		gr_crstr_gpc_map4_tile28_f(gr->map_tiles[28]) |
		gr_crstr_gpc_map4_tile29_f(gr->map_tiles[29]);

	map5 =  gr_crstr_gpc_map5_tile30_f(gr->map_tiles[30]) |
		gr_crstr_gpc_map5_tile31_f(gr->map_tiles[31]) |
		gr_crstr_gpc_map5_tile32_f(0) |
		gr_crstr_gpc_map5_tile33_f(0) |
		gr_crstr_gpc_map5_tile34_f(0) |
		gr_crstr_gpc_map5_tile35_f(0);

	gk20a_writel(g, gr_crstr_gpc_map0_r(), map0);
	gk20a_writel(g, gr_crstr_gpc_map1_r(), map1);
	gk20a_writel(g, gr_crstr_gpc_map2_r(), map2);
	gk20a_writel(g, gr_crstr_gpc_map3_r(), map3);
	gk20a_writel(g, gr_crstr_gpc_map4_r(), map4);
	gk20a_writel(g, gr_crstr_gpc_map5_r(), map5);

	switch (gr->tpc_count) {
	case 1:
		norm_shift = 4;
		break;
	case 2:
	case 3:
		norm_shift = 3;
		break;
	case 4:
	case 5:
	case 6:
	case 7:
		norm_shift = 2;
		break;
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:
	case 15:
		norm_shift = 1;
		break;
	default:
		norm_shift = 0;
		break;
	}

	norm_entries = gr->tpc_count << norm_shift;
	coeff5_mod = (1 << 5) % norm_entries;
	coeff6_mod = (1 << 6) % norm_entries;
	coeff7_mod = (1 << 7) % norm_entries;
	coeff8_mod = (1 << 8) % norm_entries;
	coeff9_mod = (1 << 9) % norm_entries;
	coeff10_mod = (1 << 10) % norm_entries;
	coeff11_mod = (1 << 11) % norm_entries;

	gk20a_writel(g, gr_ppcs_wwdx_map_table_cfg_r(),
		     gr_ppcs_wwdx_map_table_cfg_row_offset_f(gr->map_row_offset) |
		     gr_ppcs_wwdx_map_table_cfg_normalized_num_entries_f(norm_entries) |
		     gr_ppcs_wwdx_map_table_cfg_normalized_shift_value_f(norm_shift) |
		     gr_ppcs_wwdx_map_table_cfg_coeff5_mod_value_f(coeff5_mod) |
		     gr_ppcs_wwdx_map_table_cfg_num_entries_f(gr->tpc_count));

	gk20a_writel(g, gr_ppcs_wwdx_map_table_cfg2_r(),
		     gr_ppcs_wwdx_map_table_cfg2_coeff6_mod_value_f(coeff6_mod) |
		     gr_ppcs_wwdx_map_table_cfg2_coeff7_mod_value_f(coeff7_mod) |
		     gr_ppcs_wwdx_map_table_cfg2_coeff8_mod_value_f(coeff8_mod) |
		     gr_ppcs_wwdx_map_table_cfg2_coeff9_mod_value_f(coeff9_mod) |
		     gr_ppcs_wwdx_map_table_cfg2_coeff10_mod_value_f(coeff10_mod) |
		     gr_ppcs_wwdx_map_table_cfg2_coeff11_mod_value_f(coeff11_mod));

	gk20a_writel(g, gr_ppcs_wwdx_map_gpc_map0_r(), map0);
	gk20a_writel(g, gr_ppcs_wwdx_map_gpc_map1_r(), map1);
	gk20a_writel(g, gr_ppcs_wwdx_map_gpc_map2_r(), map2);
	gk20a_writel(g, gr_ppcs_wwdx_map_gpc_map3_r(), map3);
	gk20a_writel(g, gr_ppcs_wwdx_map_gpc_map4_r(), map4);
	gk20a_writel(g, gr_ppcs_wwdx_map_gpc_map5_r(), map5);

	gk20a_writel(g, gr_rstr2d_map_table_cfg_r(),
		     gr_rstr2d_map_table_cfg_row_offset_f(gr->map_row_offset) |
		     gr_rstr2d_map_table_cfg_num_entries_f(gr->tpc_count));

	gk20a_writel(g, gr_rstr2d_gpc_map0_r(), map0);
	gk20a_writel(g, gr_rstr2d_gpc_map1_r(), map1);
	gk20a_writel(g, gr_rstr2d_gpc_map2_r(), map2);
	gk20a_writel(g, gr_rstr2d_gpc_map3_r(), map3);
	gk20a_writel(g, gr_rstr2d_gpc_map4_r(), map4);
	gk20a_writel(g, gr_rstr2d_gpc_map5_r(), map5);

	return 0;
}

static inline u32 count_bits(u32 mask)
{
	u32 temp = mask;
	u32 count;
	for (count = 0; temp != 0; count++)
		temp &= temp - 1;

	return count;
}

static inline u32 clear_count_bits(u32 num, u32 clear_count)
{
	u32 count = clear_count;
	for (; (num != 0) && (count != 0); count--)
		num &= num - 1;

	return num;
}

static int gr_gk20a_setup_alpha_beta_tables(struct gk20a *g,
					struct gr_gk20a *gr)
{
	u32 table_index_bits = 5;
	u32 rows = (1 << table_index_bits);
	u32 row_stride = gr_pd_alpha_ratio_table__size_1_v() / rows;

	u32 row;
	u32 index;
	u32 gpc_index;
	u32 gpcs_per_reg = 4;
	u32 pes_index;
	u32 tpc_count_pes;
	u32 num_pes_per_gpc = nvgpu_get_litter_value(g, GPU_LIT_NUM_PES_PER_GPC);

	u32 alpha_target, beta_target;
	u32 alpha_bits, beta_bits;
	u32 alpha_mask, beta_mask, partial_mask;
	u32 reg_offset;
	bool assign_alpha;

	u32 *map_alpha;
	u32 *map_beta;
	u32 *map_reg_used;

	gk20a_dbg_fn("");

	map_alpha = kzalloc(3 * gr_pd_alpha_ratio_table__size_1_v() *
				sizeof(u32), GFP_KERNEL);
	if (!map_alpha)
		return -ENOMEM;
	map_beta = map_alpha + gr_pd_alpha_ratio_table__size_1_v();
	map_reg_used = map_beta + gr_pd_alpha_ratio_table__size_1_v();

	for (row = 0; row < rows; ++row) {
		alpha_target = max_t(u32, gr->tpc_count * row / rows, 1);
		beta_target = gr->tpc_count - alpha_target;

		assign_alpha = (alpha_target < beta_target);

		for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++) {
			reg_offset = (row * row_stride) + (gpc_index / gpcs_per_reg);
			alpha_mask = beta_mask = 0;

			for (pes_index = 0; pes_index < num_pes_per_gpc; pes_index++) {
				tpc_count_pes = gr->pes_tpc_count[pes_index][gpc_index];

				if (assign_alpha) {
					alpha_bits = (alpha_target == 0) ? 0 : tpc_count_pes;
					beta_bits = tpc_count_pes - alpha_bits;
				} else {
					beta_bits = (beta_target == 0) ? 0 : tpc_count_pes;
					alpha_bits = tpc_count_pes - beta_bits;
				}

				partial_mask = gr->pes_tpc_mask[pes_index][gpc_index];
				partial_mask = clear_count_bits(partial_mask, tpc_count_pes - alpha_bits);
				alpha_mask |= partial_mask;

				partial_mask = gr->pes_tpc_mask[pes_index][gpc_index] ^ partial_mask;
				beta_mask |= partial_mask;

				alpha_target -= min(alpha_bits, alpha_target);
				beta_target -= min(beta_bits, beta_target);

				if ((alpha_bits > 0) || (beta_bits > 0))
					assign_alpha = !assign_alpha;
			}

			switch (gpc_index % gpcs_per_reg) {
			case 0:
				map_alpha[reg_offset] |= gr_pd_alpha_ratio_table_gpc_4n0_mask_f(alpha_mask);
				map_beta[reg_offset] |= gr_pd_beta_ratio_table_gpc_4n0_mask_f(beta_mask);
				break;
			case 1:
				map_alpha[reg_offset] |= gr_pd_alpha_ratio_table_gpc_4n1_mask_f(alpha_mask);
				map_beta[reg_offset] |= gr_pd_beta_ratio_table_gpc_4n1_mask_f(beta_mask);
				break;
			case 2:
				map_alpha[reg_offset] |= gr_pd_alpha_ratio_table_gpc_4n2_mask_f(alpha_mask);
				map_beta[reg_offset] |= gr_pd_beta_ratio_table_gpc_4n2_mask_f(beta_mask);
				break;
			case 3:
				map_alpha[reg_offset] |= gr_pd_alpha_ratio_table_gpc_4n3_mask_f(alpha_mask);
				map_beta[reg_offset] |= gr_pd_beta_ratio_table_gpc_4n3_mask_f(beta_mask);
				break;
			}
			map_reg_used[reg_offset] = true;
		}
	}

	for (index = 0; index < gr_pd_alpha_ratio_table__size_1_v(); index++) {
		if (map_reg_used[index]) {
			gk20a_writel(g, gr_pd_alpha_ratio_table_r(index), map_alpha[index]);
			gk20a_writel(g, gr_pd_beta_ratio_table_r(index), map_beta[index]);
		}
	}

	kfree(map_alpha);
	return 0;
}

static u32 gr_gk20a_get_gpc_tpc_mask(struct gk20a *g, u32 gpc_index)
{
	/* One TPC for gk20a */
	return 0x1;
}

static void gr_gk20a_program_active_tpc_counts(struct gk20a *g, u32 gpc_index)
{
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 gpc_offset = gpc_stride * gpc_index;
	struct gr_gk20a *gr = &g->gr;

	gk20a_writel(g, gr_gpc0_gpm_pd_active_tpcs_r() + gpc_offset,
		gr_gpc0_gpm_pd_active_tpcs_num_f(gr->gpc_tpc_count[gpc_index]));
	gk20a_writel(g, gr_gpc0_gpm_sd_active_tpcs_r() + gpc_offset,
		gr_gpc0_gpm_sd_active_tpcs_num_f(gr->gpc_tpc_count[gpc_index]));
}

static void gr_gk20a_init_sm_id_table(struct gk20a *g)
{
	u32 gpc, tpc;
	u32 sm_id = 0;

	for (tpc = 0; tpc < g->gr.max_tpc_per_gpc_count; tpc++) {
		for (gpc = 0; gpc < g->gr.gpc_count; gpc++) {

			if (tpc < g->gr.gpc_tpc_count[gpc]) {
				g->gr.sm_to_cluster[sm_id].tpc_index = tpc;
				g->gr.sm_to_cluster[sm_id].gpc_index = gpc;
				sm_id++;
			}
		}
	}
	g->gr.no_of_sm = sm_id;
}

static void gr_gk20a_program_sm_id_numbering(struct gk20a *g,
					     u32 gpc, u32 tpc, u32 sm_id)
{
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 gpc_offset = gpc_stride * gpc;
	u32 tpc_offset = tpc_in_gpc_stride * tpc;

	gk20a_writel(g, gr_gpc0_tpc0_sm_cfg_r() + gpc_offset + tpc_offset,
			gr_gpc0_tpc0_sm_cfg_sm_id_f(sm_id));
	gk20a_writel(g, gr_gpc0_tpc0_l1c_cfg_smid_r() + gpc_offset + tpc_offset,
			gr_gpc0_tpc0_l1c_cfg_smid_value_f(sm_id));
	gk20a_writel(g, gr_gpc0_gpm_pd_sm_id_r(tpc) + gpc_offset,
			gr_gpc0_gpm_pd_sm_id_id_f(sm_id));
	gk20a_writel(g, gr_gpc0_tpc0_pe_cfg_smid_r() + gpc_offset + tpc_offset,
			gr_gpc0_tpc0_pe_cfg_smid_value_f(sm_id));
}

int gr_gk20a_init_fs_state(struct gk20a *g)
{
	struct gr_gk20a *gr = &g->gr;
	u32 tpc_index, gpc_index;
	u32 sm_id = 0, gpc_id = 0;
	u32 tpc_per_gpc;
	u32 fuse_tpc_mask;

	gk20a_dbg_fn("");

	gr_gk20a_init_sm_id_table(g);

	for (sm_id = 0; sm_id < gr->tpc_count; sm_id++) {
		tpc_index = g->gr.sm_to_cluster[sm_id].tpc_index;
		gpc_index = g->gr.sm_to_cluster[sm_id].gpc_index;

		g->ops.gr.program_sm_id_numbering(g, gpc_index, tpc_index, sm_id);

		if (g->ops.gr.program_active_tpc_counts)
			g->ops.gr.program_active_tpc_counts(g, gpc_index);
	}

	for (tpc_index = 0, gpc_id = 0;
	     tpc_index < gr_pd_num_tpc_per_gpc__size_1_v();
	     tpc_index++, gpc_id += 8) {

		if (gpc_id >= gr->gpc_count)
			continue;

		tpc_per_gpc =
			gr_pd_num_tpc_per_gpc_count0_f(gr->gpc_tpc_count[gpc_id + 0]) |
			gr_pd_num_tpc_per_gpc_count1_f(gr->gpc_tpc_count[gpc_id + 1]) |
			gr_pd_num_tpc_per_gpc_count2_f(gr->gpc_tpc_count[gpc_id + 2]) |
			gr_pd_num_tpc_per_gpc_count3_f(gr->gpc_tpc_count[gpc_id + 3]) |
			gr_pd_num_tpc_per_gpc_count4_f(gr->gpc_tpc_count[gpc_id + 4]) |
			gr_pd_num_tpc_per_gpc_count5_f(gr->gpc_tpc_count[gpc_id + 5]) |
			gr_pd_num_tpc_per_gpc_count6_f(gr->gpc_tpc_count[gpc_id + 6]) |
			gr_pd_num_tpc_per_gpc_count7_f(gr->gpc_tpc_count[gpc_id + 7]);

		gk20a_writel(g, gr_pd_num_tpc_per_gpc_r(tpc_index), tpc_per_gpc);
		gk20a_writel(g, gr_ds_num_tpc_per_gpc_r(tpc_index), tpc_per_gpc);
	}

	/* gr__setup_pd_mapping stubbed for gk20a */
	gr_gk20a_setup_rop_mapping(g, gr);
	if (g->ops.gr.setup_alpha_beta_tables)
		g->ops.gr.setup_alpha_beta_tables(g, gr);

	for (gpc_index = 0;
	     gpc_index < gr_pd_dist_skip_table__size_1_v() * 4;
	     gpc_index += 4) {

		gk20a_writel(g, gr_pd_dist_skip_table_r(gpc_index/4),
			     gr_pd_dist_skip_table_gpc_4n0_mask_f(gr->gpc_skip_mask[gpc_index]) ||
			     gr_pd_dist_skip_table_gpc_4n1_mask_f(gr->gpc_skip_mask[gpc_index + 1]) ||
			     gr_pd_dist_skip_table_gpc_4n2_mask_f(gr->gpc_skip_mask[gpc_index + 2]) ||
			     gr_pd_dist_skip_table_gpc_4n3_mask_f(gr->gpc_skip_mask[gpc_index + 3]));
	}

	fuse_tpc_mask = g->ops.gr.get_gpc_tpc_mask(g, 0);
	if (g->tpc_fs_mask_user &&
		fuse_tpc_mask == (0x1 << gr->max_tpc_count) - 1) {
		u32 val = g->tpc_fs_mask_user;
		val &= (0x1 << gr->max_tpc_count) - 1;
		gk20a_writel(g, gr_cwd_fs_r(),
			gr_cwd_fs_num_gpcs_f(gr->gpc_count) |
			gr_cwd_fs_num_tpcs_f(hweight32(val)));
	} else {
		gk20a_writel(g, gr_cwd_fs_r(),
			gr_cwd_fs_num_gpcs_f(gr->gpc_count) |
			gr_cwd_fs_num_tpcs_f(gr->tpc_count));
	}

	gk20a_writel(g, gr_bes_zrop_settings_r(),
		     gr_bes_zrop_settings_num_active_fbps_f(gr->num_fbps));
	gk20a_writel(g, gr_bes_crop_settings_r(),
		     gr_bes_crop_settings_num_active_fbps_f(gr->num_fbps));

	return 0;
}

static int gr_gk20a_fecs_ctx_image_save(struct channel_gk20a *c, u32 save_type)
{
	struct gk20a *g = c->g;
	int ret;

	gk20a_dbg_fn("");

	ret = gr_gk20a_submit_fecs_method_op(g,
		(struct fecs_method_op_gk20a) {
		.method.addr = save_type,
		.method.data = fecs_current_ctx_data(g, &c->inst_block),
		.mailbox = {.id = 0, .data = 0, .clr = 3, .ret = NULL,
			.ok = 1, .fail = 2,
		},
		.cond.ok = GR_IS_UCODE_OP_AND,
		.cond.fail = GR_IS_UCODE_OP_AND,
		 }, true);

	if (ret)
		gk20a_err(dev_from_gk20a(g), "save context image failed");

	return ret;
}

static u32 gk20a_init_sw_bundle(struct gk20a *g)
{
	struct av_list_gk20a *sw_bundle_init = &g->gr.ctx_vars.sw_bundle_init;
	u32 last_bundle_data = 0;
	u32 err = 0;
	int i;
	unsigned long end_jiffies = jiffies +
		msecs_to_jiffies(gk20a_get_gr_idle_timeout(g));

	/* disable fe_go_idle */
	gk20a_writel(g, gr_fe_go_idle_timeout_r(),
		gr_fe_go_idle_timeout_count_disabled_f());
	/* enable pipe mode override */
	gk20a_writel(g, gr_pipe_bundle_config_r(),
		gr_pipe_bundle_config_override_pipe_mode_enabled_f());

	/* load bundle init */
	for (i = 0; i < sw_bundle_init->count; i++) {
		if (i == 0 || last_bundle_data != sw_bundle_init->l[i].value) {
			gk20a_writel(g, gr_pipe_bundle_data_r(),
				sw_bundle_init->l[i].value);
			last_bundle_data = sw_bundle_init->l[i].value;
		}

		gk20a_writel(g, gr_pipe_bundle_address_r(),
			     sw_bundle_init->l[i].addr);

		if (gr_pipe_bundle_address_value_v(sw_bundle_init->l[i].addr) ==
		    GR_GO_IDLE_BUNDLE)
			err |= gr_gk20a_wait_idle(g, end_jiffies,
					GR_IDLE_CHECK_DEFAULT);

		err = gr_gk20a_wait_fe_idle(g, end_jiffies,
					GR_IDLE_CHECK_DEFAULT);
		if (err)
			break;
	}

	/* disable pipe mode override */
	gk20a_writel(g, gr_pipe_bundle_config_r(),
		     gr_pipe_bundle_config_override_pipe_mode_disabled_f());

	/* restore fe_go_idle */
	gk20a_writel(g, gr_fe_go_idle_timeout_r(),
		     gr_fe_go_idle_timeout_count_prod_f());

	return err;
}

/* init global golden image from a fresh gr_ctx in channel ctx.
   save a copy in local_golden_image in ctx_vars */
static int gr_gk20a_init_golden_ctx_image(struct gk20a *g,
					  struct channel_gk20a *c)
{
	struct gr_gk20a *gr = &g->gr;
	struct channel_ctx_gk20a *ch_ctx = &c->ch_ctx;
	u32 ctx_header_bytes = ctxsw_prog_fecs_header_v();
	u32 ctx_header_words;
	u32 i;
	u32 data;
	struct mem_desc *gold_mem = &gr->global_ctx_buffer[GOLDEN_CTX].mem;
	struct mem_desc *gr_mem = &ch_ctx->gr_ctx->mem;
	u32 err = 0;
	struct aiv_list_gk20a *sw_ctx_load = &g->gr.ctx_vars.sw_ctx_load;
	struct av_list_gk20a *sw_method_init = &g->gr.ctx_vars.sw_method_init;
	unsigned long end_jiffies = jiffies +
		msecs_to_jiffies(gk20a_get_gr_idle_timeout(g));
	u32 last_method_data = 0;
	int retries = FE_PWR_MODE_TIMEOUT_MAX / FE_PWR_MODE_TIMEOUT_DEFAULT;

	gk20a_dbg_fn("");

	/* golden ctx is global to all channels. Although only the first
	   channel initializes golden image, driver needs to prevent multiple
	   channels from initializing golden ctx at the same time */
	mutex_lock(&gr->ctx_mutex);

	if (gr->ctx_vars.golden_image_initialized)
		goto clean_up;

	if (!tegra_platform_is_linsim()) {
		gk20a_writel(g, gr_fe_pwr_mode_r(),
			gr_fe_pwr_mode_req_send_f() | gr_fe_pwr_mode_mode_force_on_f());
		do {
			u32 req = gr_fe_pwr_mode_req_v(gk20a_readl(g, gr_fe_pwr_mode_r()));
			if (req == gr_fe_pwr_mode_req_done_v())
				break;
			udelay(FE_PWR_MODE_TIMEOUT_MAX);
		} while (--retries || !tegra_platform_is_silicon());
	}

	if (!retries)
		gk20a_err(g->dev, "timeout forcing FE on");

	gk20a_writel(g, gr_fecs_ctxsw_reset_ctl_r(),
			gr_fecs_ctxsw_reset_ctl_sys_halt_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_gpc_halt_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_be_halt_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_sys_engine_reset_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_gpc_engine_reset_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_be_engine_reset_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_sys_context_reset_enabled_f() |
			gr_fecs_ctxsw_reset_ctl_gpc_context_reset_enabled_f() |
			gr_fecs_ctxsw_reset_ctl_be_context_reset_enabled_f());
	gk20a_readl(g, gr_fecs_ctxsw_reset_ctl_r());
	udelay(10);

	gk20a_writel(g, gr_fecs_ctxsw_reset_ctl_r(),
			gr_fecs_ctxsw_reset_ctl_sys_halt_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_gpc_halt_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_be_halt_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_sys_engine_reset_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_gpc_engine_reset_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_be_engine_reset_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_sys_context_reset_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_gpc_context_reset_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_be_context_reset_disabled_f());
	gk20a_readl(g, gr_fecs_ctxsw_reset_ctl_r());
	udelay(10);

	if (!tegra_platform_is_linsim()) {
		gk20a_writel(g, gr_fe_pwr_mode_r(),
			gr_fe_pwr_mode_req_send_f() | gr_fe_pwr_mode_mode_auto_f());

		retries = FE_PWR_MODE_TIMEOUT_MAX / FE_PWR_MODE_TIMEOUT_DEFAULT;
		do {
			u32 req = gr_fe_pwr_mode_req_v(gk20a_readl(g, gr_fe_pwr_mode_r()));
			if (req == gr_fe_pwr_mode_req_done_v())
				break;
			udelay(FE_PWR_MODE_TIMEOUT_DEFAULT);
		} while (--retries || !tegra_platform_is_silicon());

		if (!retries)
			gk20a_err(g->dev, "timeout setting FE power to auto");
	}

	/* clear scc ram */
	gk20a_writel(g, gr_scc_init_r(),
		gr_scc_init_ram_trigger_f());

	err = gr_gk20a_fecs_ctx_bind_channel(g, c);
	if (err)
		goto clean_up;

	err = gr_gk20a_wait_idle(g, end_jiffies, GR_IDLE_CHECK_DEFAULT);

	/* load ctx init */
	for (i = 0; i < sw_ctx_load->count; i++)
		gk20a_writel(g, sw_ctx_load->l[i].addr,
			     sw_ctx_load->l[i].value);

	if (g->ops.gr.init_preemption_state)
		g->ops.gr.init_preemption_state(g);

	if (g->ops.clock_gating.blcg_gr_load_gating_prod)
		g->ops.clock_gating.blcg_gr_load_gating_prod(g, g->blcg_enabled);

	err = gr_gk20a_wait_idle(g, end_jiffies, GR_IDLE_CHECK_DEFAULT);
	if (err)
		goto clean_up;

	/* disable fe_go_idle */
	gk20a_writel(g, gr_fe_go_idle_timeout_r(),
		gr_fe_go_idle_timeout_count_disabled_f());

	err = gr_gk20a_commit_global_ctx_buffers(g, c, false);
	if (err)
		goto clean_up;

	/* override a few ctx state registers */
	gr_gk20a_commit_global_timeslice(g, c, false);

	/* floorsweep anything left */
	g->ops.gr.init_fs_state(g);

	err = gr_gk20a_wait_idle(g, end_jiffies, GR_IDLE_CHECK_DEFAULT);
	if (err)
		goto restore_fe_go_idle;

	err = gk20a_init_sw_bundle(g);
	if (err)
		goto clean_up;

restore_fe_go_idle:
	/* restore fe_go_idle */
	gk20a_writel(g, gr_fe_go_idle_timeout_r(),
		     gr_fe_go_idle_timeout_count_prod_f());

	if (err || gr_gk20a_wait_idle(g, end_jiffies, GR_IDLE_CHECK_DEFAULT))
		goto clean_up;

	/* load method init */
	if (sw_method_init->count) {
		gk20a_writel(g, gr_pri_mme_shadow_raw_data_r(),
			     sw_method_init->l[0].value);
		gk20a_writel(g, gr_pri_mme_shadow_raw_index_r(),
			     gr_pri_mme_shadow_raw_index_write_trigger_f() |
			     sw_method_init->l[0].addr);
		last_method_data = sw_method_init->l[0].value;
	}
	for (i = 1; i < sw_method_init->count; i++) {
		if (sw_method_init->l[i].value != last_method_data) {
			gk20a_writel(g, gr_pri_mme_shadow_raw_data_r(),
				sw_method_init->l[i].value);
			last_method_data = sw_method_init->l[i].value;
		}
		gk20a_writel(g, gr_pri_mme_shadow_raw_index_r(),
			gr_pri_mme_shadow_raw_index_write_trigger_f() |
			sw_method_init->l[i].addr);
	}

	err = gr_gk20a_wait_idle(g, end_jiffies, GR_IDLE_CHECK_DEFAULT);
	if (err)
		goto clean_up;

	kfree(gr->sm_error_states);

	/* we need to allocate this after g->ops.gr.init_fs_state() since
	 * we initialize gr->no_of_sm in this function
	 */
	gr->sm_error_states = kzalloc(
			sizeof(struct nvgpu_dbg_gpu_sm_error_state_record)
			* gr->no_of_sm, GFP_KERNEL);
	if (!gr->sm_error_states) {
		err = -ENOMEM;
		goto restore_fe_go_idle;
	}

	if (gk20a_mem_begin(g, gold_mem))
		goto clean_up;

	if (gk20a_mem_begin(g, gr_mem))
		goto clean_up;

	ctx_header_words =  roundup(ctx_header_bytes, sizeof(u32));
	ctx_header_words >>= 2;

	g->ops.mm.l2_flush(g, true);

	for (i = 0; i < ctx_header_words; i++) {
		data = gk20a_mem_rd32(g, gr_mem, i);
		gk20a_mem_wr32(g, gold_mem, i, data);
	}

	gk20a_mem_wr(g, gold_mem, ctxsw_prog_main_image_zcull_o(),
		 ctxsw_prog_main_image_zcull_mode_no_ctxsw_v());

	gk20a_mem_wr(g, gold_mem, ctxsw_prog_main_image_zcull_ptr_o(), 0);

	gr_gk20a_commit_inst(c, ch_ctx->global_ctx_buffer_va[GOLDEN_CTX_VA]);

	gr_gk20a_fecs_ctx_image_save(c, gr_fecs_method_push_adr_wfi_golden_save_v());

	if (gr->ctx_vars.local_golden_image == NULL) {

		gr->ctx_vars.local_golden_image =
			vzalloc(gr->ctx_vars.golden_image_size);

		if (gr->ctx_vars.local_golden_image == NULL) {
			err = -ENOMEM;
			goto clean_up;
		}

		gk20a_mem_rd_n(g, gold_mem, 0,
				gr->ctx_vars.local_golden_image,
				gr->ctx_vars.golden_image_size);
	}

	gr_gk20a_commit_inst(c, gr_mem->gpu_va);

	gr->ctx_vars.golden_image_initialized = true;

	gk20a_writel(g, gr_fecs_current_ctx_r(),
		gr_fecs_current_ctx_valid_false_f());

clean_up:
	if (err)
		gk20a_err(dev_from_gk20a(g), "fail");
	else
		gk20a_dbg_fn("done");

	gk20a_mem_end(g, gold_mem);
	gk20a_mem_end(g, gr_mem);

	mutex_unlock(&gr->ctx_mutex);
	return err;
}

int gr_gk20a_update_smpc_ctxsw_mode(struct gk20a *g,
				    struct channel_gk20a *c,
				    bool enable_smpc_ctxsw)
{
	struct channel_ctx_gk20a *ch_ctx = &c->ch_ctx;
	struct mem_desc *mem;
	u32 data;
	int ret;

	gk20a_dbg_fn("");

	if (!ch_ctx->gr_ctx) {
		gk20a_err(dev_from_gk20a(g), "no graphics context allocated");
		return -EFAULT;
	}

	mem = &ch_ctx->gr_ctx->mem;

	ret = gk20a_disable_channel_tsg(g, c);
	if (ret) {
		gk20a_err(dev_from_gk20a(g), "failed to disable channel/TSG\n");
		goto out;
	}
	ret = gk20a_fifo_preempt(g, c);
	if (ret) {
		gk20a_enable_channel_tsg(g, c);
		gk20a_err(dev_from_gk20a(g), "failed to preempt channel/TSG\n");
		goto out;
	}

	/* Channel gr_ctx buffer is gpu cacheable.
	   Flush and invalidate before cpu update. */
	g->ops.mm.l2_flush(g, true);

	if (gk20a_mem_begin(g, mem)) {
		ret = -ENOMEM;
		goto out;
	}

	data = gk20a_mem_rd(g, mem,
			ctxsw_prog_main_image_pm_o());
	data = data & ~ctxsw_prog_main_image_pm_smpc_mode_m();
	data |= enable_smpc_ctxsw ?
		ctxsw_prog_main_image_pm_smpc_mode_ctxsw_f() :
		ctxsw_prog_main_image_pm_smpc_mode_no_ctxsw_f();
	gk20a_mem_wr(g, mem,
			ctxsw_prog_main_image_pm_o(),
			data);

	gk20a_mem_end(g, mem);

out:
	gk20a_enable_channel_tsg(g, c);
	return ret;
}

int gr_gk20a_update_hwpm_ctxsw_mode(struct gk20a *g,
				  struct channel_gk20a *c,
				  bool enable_hwpm_ctxsw)
{
	struct channel_ctx_gk20a *ch_ctx = &c->ch_ctx;
	struct pm_ctx_desc *pm_ctx = &ch_ctx->pm_ctx;
	struct mem_desc *gr_mem;
	u32 data, virt_addr;
	int ret;

	gk20a_dbg_fn("");

	if (!ch_ctx->gr_ctx) {
		gk20a_err(dev_from_gk20a(g), "no graphics context allocated");
		return -EFAULT;
	}

	gr_mem = &ch_ctx->gr_ctx->mem;

	if (enable_hwpm_ctxsw) {
		if (pm_ctx->pm_mode == ctxsw_prog_main_image_pm_mode_ctxsw_f())
			return 0;
	} else {
		if (pm_ctx->pm_mode == ctxsw_prog_main_image_pm_mode_no_ctxsw_f())
			return 0;
	}

	ret = gk20a_disable_channel_tsg(g, c);
	if (ret) {
		gk20a_err(dev_from_gk20a(g), "failed to disable channel/TSG\n");
		return ret;
	}

	ret = gk20a_fifo_preempt(g, c);
	if (ret) {
		gk20a_enable_channel_tsg(g, c);
		gk20a_err(dev_from_gk20a(g), "failed to preempt channel/TSG\n");
		return ret;
	}

	/* Channel gr_ctx buffer is gpu cacheable.
	   Flush and invalidate before cpu update. */
	g->ops.mm.l2_flush(g, true);

	if (enable_hwpm_ctxsw) {
		/* Allocate buffer if necessary */
		if (pm_ctx->mem.gpu_va == 0) {
			ret = gk20a_gmmu_alloc_attr_sys(g,
					DMA_ATTR_NO_KERNEL_MAPPING,
					g->gr.ctx_vars.pm_ctxsw_image_size,
					&pm_ctx->mem);
			if (ret) {
				c->g->ops.fifo.enable_channel(c);
				gk20a_err(dev_from_gk20a(g),
					"failed to allocate pm ctxt buffer");
				return ret;
			}

			pm_ctx->mem.gpu_va = gk20a_gmmu_map(c->vm,
							&pm_ctx->mem.sgt,
							pm_ctx->mem.size,
							NVGPU_MAP_BUFFER_FLAGS_CACHEABLE_TRUE,
							gk20a_mem_flag_none, true,
							pm_ctx->mem.aperture);
			if (!pm_ctx->mem.gpu_va) {
				gk20a_err(dev_from_gk20a(g),
					"failed to map pm ctxt buffer");
				gk20a_gmmu_free_attr(g, DMA_ATTR_NO_KERNEL_MAPPING,
						&pm_ctx->mem);
				c->g->ops.fifo.enable_channel(c);
				return -ENOMEM;
			}
		}

		/* Now clear the buffer */
		if (gk20a_mem_begin(g, &pm_ctx->mem)) {
			ret = -ENOMEM;
			goto cleanup_pm_buf;
		}

		gk20a_memset(g, &pm_ctx->mem, 0, 0, pm_ctx->mem.size);

		gk20a_mem_end(g, &pm_ctx->mem);
	}

	if (gk20a_mem_begin(g, gr_mem)) {
		ret = -ENOMEM;
		goto cleanup_pm_buf;
	}

	data = gk20a_mem_rd(g, gr_mem, ctxsw_prog_main_image_pm_o());
	data = data & ~ctxsw_prog_main_image_pm_mode_m();

	if (enable_hwpm_ctxsw) {
		pm_ctx->pm_mode = ctxsw_prog_main_image_pm_mode_ctxsw_f();

		/* pack upper 32 bits of virtual address into a 32 bit number
		 * (256 byte boundary)
		 */
		virt_addr = (u32)(pm_ctx->mem.gpu_va >> 8);
	} else {
		pm_ctx->pm_mode = ctxsw_prog_main_image_pm_mode_no_ctxsw_f();
		virt_addr = 0;
	}

	data |= pm_ctx->pm_mode;

	gk20a_mem_wr(g, gr_mem, ctxsw_prog_main_image_pm_o(), data);
	gk20a_mem_wr(g, gr_mem, ctxsw_prog_main_image_pm_ptr_o(), virt_addr);

	gk20a_mem_end(g, gr_mem);

	/* enable channel */
	gk20a_enable_channel_tsg(g, c);

	return 0;
cleanup_pm_buf:
	gk20a_gmmu_unmap(c->vm, pm_ctx->mem.gpu_va, pm_ctx->mem.size,
			gk20a_mem_flag_none);
	gk20a_gmmu_free_attr(g, DMA_ATTR_NO_KERNEL_MAPPING, &pm_ctx->mem);
	memset(&pm_ctx->mem, 0, sizeof(struct mem_desc));

	gk20a_enable_channel_tsg(g, c);
	return ret;
}

/* load saved fresh copy of gloden image into channel gr_ctx */
int gr_gk20a_load_golden_ctx_image(struct gk20a *g,
					struct channel_gk20a *c)
{
	struct gr_gk20a *gr = &g->gr;
	struct channel_ctx_gk20a *ch_ctx = &c->ch_ctx;
	u32 virt_addr_lo;
	u32 virt_addr_hi;
	u32 virt_addr = 0;
	u32 v, data;
	int ret = 0;
	struct mem_desc *mem = &ch_ctx->gr_ctx->mem;

	gk20a_dbg_fn("");

	if (gr->ctx_vars.local_golden_image == NULL)
		return -1;

	/* Channel gr_ctx buffer is gpu cacheable.
	   Flush and invalidate before cpu update. */
	g->ops.mm.l2_flush(g, true);

	if (gk20a_mem_begin(g, mem))
		return -ENOMEM;

	gk20a_mem_wr_n(g, mem, 0,
			gr->ctx_vars.local_golden_image,
			gr->ctx_vars.golden_image_size);

	if (g->ops.gr.enable_cde_in_fecs && c->cde)
		g->ops.gr.enable_cde_in_fecs(g, mem);

	gk20a_mem_wr(g, mem, ctxsw_prog_main_image_num_save_ops_o(), 0);
	gk20a_mem_wr(g, mem, ctxsw_prog_main_image_num_restore_ops_o(), 0);

	/* set priv access map */
	virt_addr_lo =
		 u64_lo32(ch_ctx->global_ctx_buffer_va[PRIV_ACCESS_MAP_VA]);
	virt_addr_hi =
		 u64_hi32(ch_ctx->global_ctx_buffer_va[PRIV_ACCESS_MAP_VA]);

	if (g->allow_all)
		data = ctxsw_prog_main_image_priv_access_map_config_mode_allow_all_f();
	else
		data = ctxsw_prog_main_image_priv_access_map_config_mode_use_map_f();

	gk20a_mem_wr(g, mem, ctxsw_prog_main_image_priv_access_map_config_o(),
		 data);
	gk20a_mem_wr(g, mem, ctxsw_prog_main_image_priv_access_map_addr_lo_o(),
		 virt_addr_lo);
	gk20a_mem_wr(g, mem, ctxsw_prog_main_image_priv_access_map_addr_hi_o(),
		 virt_addr_hi);
	/* disable verif features */
	v = gk20a_mem_rd(g, mem, ctxsw_prog_main_image_misc_options_o());
	v = v & ~(ctxsw_prog_main_image_misc_options_verif_features_m());
	v = v | ctxsw_prog_main_image_misc_options_verif_features_disabled_f();
	gk20a_mem_wr(g, mem, ctxsw_prog_main_image_misc_options_o(), v);

	if (g->ops.gr.update_ctxsw_preemption_mode)
		g->ops.gr.update_ctxsw_preemption_mode(g, ch_ctx, mem);

	virt_addr_lo = u64_lo32(ch_ctx->patch_ctx.mem.gpu_va);
	virt_addr_hi = u64_hi32(ch_ctx->patch_ctx.mem.gpu_va);

	gk20a_mem_wr(g, mem, ctxsw_prog_main_image_patch_count_o(),
		 ch_ctx->patch_ctx.data_count);
	gk20a_mem_wr(g, mem, ctxsw_prog_main_image_patch_adr_lo_o(),
		 virt_addr_lo);
	gk20a_mem_wr(g, mem, ctxsw_prog_main_image_patch_adr_hi_o(),
		 virt_addr_hi);

	/* Update main header region of the context buffer with the info needed
	 * for PM context switching, including mode and possibly a pointer to
	 * the PM backing store.
	 */
	if (ch_ctx->pm_ctx.pm_mode == ctxsw_prog_main_image_pm_mode_ctxsw_f()) {
		if (ch_ctx->pm_ctx.mem.gpu_va == 0) {
			gk20a_err(dev_from_gk20a(g),
				"context switched pm with no pm buffer!");
			gk20a_mem_end(g, mem);
			return -EFAULT;
		}

		/* pack upper 32 bits of virtual address into a 32 bit number
		 * (256 byte boundary)
		 */
		virt_addr = (u32)(ch_ctx->pm_ctx.mem.gpu_va >> 8);
	} else
		virt_addr = 0;

	data = gk20a_mem_rd(g, mem, ctxsw_prog_main_image_pm_o());
	data = data & ~ctxsw_prog_main_image_pm_mode_m();
	data |= ch_ctx->pm_ctx.pm_mode;

	gk20a_mem_wr(g, mem, ctxsw_prog_main_image_pm_o(), data);
	gk20a_mem_wr(g, mem, ctxsw_prog_main_image_pm_ptr_o(), virt_addr);

	gk20a_mem_end(g, mem);

	if (tegra_platform_is_linsim()) {
		u32 mdata = fecs_current_ctx_data(g, &c->inst_block);

		ret = gr_gk20a_submit_fecs_method_op(g,
			  (struct fecs_method_op_gk20a) {
				  .method.data = mdata,
				  .method.addr =
					  gr_fecs_method_push_adr_restore_golden_v(),
				  .mailbox = {
					  .id = 0, .data = 0,
					  .clr = ~0, .ret = NULL,
					  .ok = gr_fecs_ctxsw_mailbox_value_pass_v(),
					  .fail = 0},
				  .cond.ok = GR_IS_UCODE_OP_EQUAL,
				  .cond.fail = GR_IS_UCODE_OP_SKIP}, false);

		if (ret)
			gk20a_err(dev_from_gk20a(g),
				   "restore context image failed");
	}

	return ret;
}

static void gr_gk20a_start_falcon_ucode(struct gk20a *g)
{
	gk20a_dbg_fn("");

	gk20a_writel(g, gr_fecs_ctxsw_mailbox_clear_r(0),
		     gr_fecs_ctxsw_mailbox_clear_value_f(~0));

	gk20a_writel(g, gr_gpccs_dmactl_r(), gr_gpccs_dmactl_require_ctx_f(0));
	gk20a_writel(g, gr_fecs_dmactl_r(), gr_fecs_dmactl_require_ctx_f(0));

	gk20a_writel(g, gr_gpccs_cpuctl_r(), gr_gpccs_cpuctl_startcpu_f(1));
	gk20a_writel(g, gr_fecs_cpuctl_r(), gr_fecs_cpuctl_startcpu_f(1));

	gk20a_dbg_fn("done");
}

static int gr_gk20a_init_ctxsw_ucode_vaspace(struct gk20a *g)
{
	struct mm_gk20a *mm = &g->mm;
	struct vm_gk20a *vm = &mm->pmu.vm;
	struct device *d = dev_from_gk20a(g);
	struct gk20a_ctxsw_ucode_info *ucode_info = &g->ctxsw_ucode_info;
	int err;

	err = gk20a_alloc_inst_block(g, &ucode_info->inst_blk_desc);
	if (err)
		return err;

	gk20a_init_inst_block(&ucode_info->inst_blk_desc, vm, 0);

	/* Map ucode surface to GMMU */
	ucode_info->surface_desc.gpu_va = gk20a_gmmu_map(vm,
					&ucode_info->surface_desc.sgt,
					ucode_info->surface_desc.size,
					0, /* flags */
					gk20a_mem_flag_read_only,
					false,
					ucode_info->surface_desc.aperture);
	if (!ucode_info->surface_desc.gpu_va) {
		gk20a_err(d, "failed to update gmmu ptes\n");
		return -ENOMEM;
	}

	return 0;
}

static void gr_gk20a_init_ctxsw_ucode_segment(
	struct gk20a_ctxsw_ucode_segment *p_seg, u32 *offset, u32 size)
{
	p_seg->offset = *offset;
	p_seg->size = size;
	*offset = ALIGN(*offset + size, BLK_SIZE);
}

static void gr_gk20a_init_ctxsw_ucode_segments(
	struct gk20a_ctxsw_ucode_segments *segments, u32 *offset,
	struct gk20a_ctxsw_bootloader_desc *bootdesc,
	u32 code_size, u32 data_size)
{
	u32 boot_size = ALIGN(bootdesc->size, sizeof(u32));
	segments->boot_entry = bootdesc->entry_point;
	segments->boot_imem_offset = bootdesc->imem_offset;
	gr_gk20a_init_ctxsw_ucode_segment(&segments->boot, offset, boot_size);
	gr_gk20a_init_ctxsw_ucode_segment(&segments->code, offset, code_size);
	gr_gk20a_init_ctxsw_ucode_segment(&segments->data, offset, data_size);
}

static int gr_gk20a_copy_ctxsw_ucode_segments(
	struct gk20a *g,
	struct mem_desc *dst,
	struct gk20a_ctxsw_ucode_segments *segments,
	u32 *bootimage,
	u32 *code, u32 *data)
{
	int i;

	gk20a_mem_wr_n(g, dst, segments->boot.offset, bootimage,
			segments->boot.size);
	gk20a_mem_wr_n(g, dst, segments->code.offset, code,
			segments->code.size);
	gk20a_mem_wr_n(g, dst, segments->data.offset, data,
			segments->data.size);

	/* compute a "checksum" for the boot binary to detect its version */
	segments->boot_signature = 0;
	for (i = 0; i < segments->boot.size / sizeof(u32); i++)
		segments->boot_signature += bootimage[i];

	return 0;
}

int gr_gk20a_init_ctxsw_ucode(struct gk20a *g)
{
	struct device *d = dev_from_gk20a(g);
	struct mm_gk20a *mm = &g->mm;
	struct vm_gk20a *vm = &mm->pmu.vm;
	struct gk20a_ctxsw_bootloader_desc *fecs_boot_desc;
	struct gk20a_ctxsw_bootloader_desc *gpccs_boot_desc;
	const struct firmware *fecs_fw;
	const struct firmware *gpccs_fw;
	u32 *fecs_boot_image;
	u32 *gpccs_boot_image;
	struct gk20a_ctxsw_ucode_info *ucode_info = &g->ctxsw_ucode_info;
	u32 ucode_size;
	int err = 0;

	fecs_fw = nvgpu_request_firmware(g, GK20A_FECS_UCODE_IMAGE, 0);
	if (!fecs_fw) {
		gk20a_err(d, "failed to load fecs ucode!!");
		return -ENOENT;
	}

	fecs_boot_desc = (void *)fecs_fw->data;
	fecs_boot_image = (void *)(fecs_fw->data +
				sizeof(struct gk20a_ctxsw_bootloader_desc));

	gpccs_fw = nvgpu_request_firmware(g, GK20A_GPCCS_UCODE_IMAGE, 0);
	if (!gpccs_fw) {
		release_firmware(fecs_fw);
		gk20a_err(d, "failed to load gpccs ucode!!");
		return -ENOENT;
	}

	gpccs_boot_desc = (void *)gpccs_fw->data;
	gpccs_boot_image = (void *)(gpccs_fw->data +
				sizeof(struct gk20a_ctxsw_bootloader_desc));

	ucode_size = 0;
	gr_gk20a_init_ctxsw_ucode_segments(&ucode_info->fecs, &ucode_size,
		fecs_boot_desc,
		g->gr.ctx_vars.ucode.fecs.inst.count * sizeof(u32),
		g->gr.ctx_vars.ucode.fecs.data.count * sizeof(u32));
	gr_gk20a_init_ctxsw_ucode_segments(&ucode_info->gpccs, &ucode_size,
		gpccs_boot_desc,
		g->gr.ctx_vars.ucode.gpccs.inst.count * sizeof(u32),
		g->gr.ctx_vars.ucode.gpccs.data.count * sizeof(u32));

	err = gk20a_gmmu_alloc_sys(g, ucode_size, &ucode_info->surface_desc);
	if (err)
		goto clean_up;

	gr_gk20a_copy_ctxsw_ucode_segments(g, &ucode_info->surface_desc,
		&ucode_info->fecs,
		fecs_boot_image,
		g->gr.ctx_vars.ucode.fecs.inst.l,
		g->gr.ctx_vars.ucode.fecs.data.l);

	release_firmware(fecs_fw);
	fecs_fw = NULL;

	gr_gk20a_copy_ctxsw_ucode_segments(g, &ucode_info->surface_desc,
		&ucode_info->gpccs,
		gpccs_boot_image,
		g->gr.ctx_vars.ucode.gpccs.inst.l,
		g->gr.ctx_vars.ucode.gpccs.data.l);

	release_firmware(gpccs_fw);
	gpccs_fw = NULL;

	err = gr_gk20a_init_ctxsw_ucode_vaspace(g);
	if (err)
		goto clean_up;

	return 0;

 clean_up:
	if (ucode_info->surface_desc.gpu_va)
		gk20a_gmmu_unmap(vm, ucode_info->surface_desc.gpu_va,
			ucode_info->surface_desc.size, gk20a_mem_flag_none);
	gk20a_gmmu_free(g, &ucode_info->surface_desc);

	release_firmware(gpccs_fw);
	gpccs_fw = NULL;
	release_firmware(fecs_fw);
	fecs_fw = NULL;

	return err;
}

void gr_gk20a_load_falcon_bind_instblk(struct gk20a *g)
{
	struct gk20a_ctxsw_ucode_info *ucode_info = &g->ctxsw_ucode_info;
	int retries = FECS_ARB_CMD_TIMEOUT_MAX / FECS_ARB_CMD_TIMEOUT_DEFAULT;
	phys_addr_t inst_ptr;
	u32 val;

	while ((gk20a_readl(g, gr_fecs_ctxsw_status_1_r()) &
			gr_fecs_ctxsw_status_1_arb_busy_m()) && retries) {
		udelay(FECS_ARB_CMD_TIMEOUT_DEFAULT);
		retries--;
	}
	if (!retries) {
		gk20a_err(dev_from_gk20a(g),
			  "arbiter idle timeout, status: %08x",
			  gk20a_readl(g, gr_fecs_ctxsw_status_1_r()));
	}

	gk20a_writel(g, gr_fecs_arb_ctx_adr_r(), 0x0);

	inst_ptr = gk20a_mm_inst_block_addr(g, &ucode_info->inst_blk_desc);
	gk20a_writel(g, gr_fecs_new_ctx_r(),
			gr_fecs_new_ctx_ptr_f(inst_ptr >> 12) |
			gk20a_aperture_mask(g, &ucode_info->inst_blk_desc,
				gr_fecs_new_ctx_target_sys_mem_ncoh_f(),
				gr_fecs_new_ctx_target_vid_mem_f()) |
			gr_fecs_new_ctx_valid_m());

	gk20a_writel(g, gr_fecs_arb_ctx_ptr_r(),
			gr_fecs_arb_ctx_ptr_ptr_f(inst_ptr >> 12) |
			gk20a_aperture_mask(g, &ucode_info->inst_blk_desc,
				gr_fecs_arb_ctx_ptr_target_sys_mem_ncoh_f(),
				gr_fecs_arb_ctx_ptr_target_vid_mem_f()));

	gk20a_writel(g, gr_fecs_arb_ctx_cmd_r(), 0x7);

	/* Wait for arbiter command to complete */
	retries = FECS_ARB_CMD_TIMEOUT_MAX / FECS_ARB_CMD_TIMEOUT_DEFAULT;
	val = gk20a_readl(g, gr_fecs_arb_ctx_cmd_r());
	while (gr_fecs_arb_ctx_cmd_cmd_v(val) && retries) {
		udelay(FECS_ARB_CMD_TIMEOUT_DEFAULT);
		retries--;
		val = gk20a_readl(g, gr_fecs_arb_ctx_cmd_r());
	}
	if (!retries)
		gk20a_err(dev_from_gk20a(g), "arbiter complete timeout");

	gk20a_writel(g, gr_fecs_current_ctx_r(),
			gr_fecs_current_ctx_ptr_f(inst_ptr >> 12) |
			gr_fecs_current_ctx_target_m() |
			gr_fecs_current_ctx_valid_m());
	/* Send command to arbiter to flush */
	gk20a_writel(g, gr_fecs_arb_ctx_cmd_r(), gr_fecs_arb_ctx_cmd_cmd_s());

	retries = FECS_ARB_CMD_TIMEOUT_MAX / FECS_ARB_CMD_TIMEOUT_DEFAULT;
	val = (gk20a_readl(g, gr_fecs_arb_ctx_cmd_r()));
	while (gr_fecs_arb_ctx_cmd_cmd_v(val) && retries) {
		udelay(FECS_ARB_CMD_TIMEOUT_DEFAULT);
		retries--;
		val = gk20a_readl(g, gr_fecs_arb_ctx_cmd_r());
	}
	if (!retries)
		gk20a_err(dev_from_gk20a(g), "arbiter complete timeout");
}

void gr_gk20a_load_ctxsw_ucode_header(struct gk20a *g, u64 addr_base,
	struct gk20a_ctxsw_ucode_segments *segments, u32 reg_offset)
{
	u32 addr_code32;
	u32 addr_data32;

	addr_code32 = u64_lo32((addr_base + segments->code.offset) >> 8);
	addr_data32 = u64_lo32((addr_base + segments->data.offset) >> 8);

	/*
	 * Copy falcon bootloader header into dmem at offset 0.
	 * Configure dmem port 0 for auto-incrementing writes starting at dmem
	 * offset 0.
	 */
	gk20a_writel(g, reg_offset + gr_fecs_dmemc_r(0),
			gr_fecs_dmemc_offs_f(0) |
			gr_fecs_dmemc_blk_f(0) |
			gr_fecs_dmemc_aincw_f(1));

	/* Write out the actual data */
	switch (segments->boot_signature) {
	case FALCON_UCODE_SIG_T18X_GPCCS_WITH_RESERVED:
	case FALCON_UCODE_SIG_T21X_FECS_WITH_DMEM_SIZE:
	case FALCON_UCODE_SIG_T21X_FECS_WITH_RESERVED:
	case FALCON_UCODE_SIG_T21X_GPCCS_WITH_RESERVED:
	case FALCON_UCODE_SIG_T12X_FECS_WITH_RESERVED:
	case FALCON_UCODE_SIG_T12X_GPCCS_WITH_RESERVED:
		gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0), 0);
		gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0), 0);
		gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0), 0);
		gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0), 0);
		/* fallthrough */
	case FALCON_UCODE_SIG_T12X_FECS_WITHOUT_RESERVED:
	case FALCON_UCODE_SIG_T12X_GPCCS_WITHOUT_RESERVED:
	case FALCON_UCODE_SIG_T21X_FECS_WITHOUT_RESERVED:
	case FALCON_UCODE_SIG_T21X_FECS_WITHOUT_RESERVED2:
	case FALCON_UCODE_SIG_T21X_GPCCS_WITHOUT_RESERVED:
		gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0), 0);
		gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0), 0);
		gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0), 0);
		gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0), 0);
		gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0), 4);
		gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0),
				addr_code32);
		gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0), 0);
		gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0),
				segments->code.size);
		gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0), 0);
		gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0), 0);
		gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0), 0);
		gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0),
				addr_data32);
		gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0),
				segments->data.size);
		break;
	case FALCON_UCODE_SIG_T12X_FECS_OLDER:
	case FALCON_UCODE_SIG_T12X_GPCCS_OLDER:
		gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0), 0);
		gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0),
				addr_code32);
		gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0), 0);
		gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0),
				segments->code.size);
		gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0), 0);
		gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0),
				addr_data32);
		gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0),
				segments->data.size);
		gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0),
				addr_code32);
		gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0), 0);
		gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0), 0);
		break;
	default:
		gk20a_err(dev_from_gk20a(g),
				"unknown falcon ucode boot signature 0x%08x"
				" with reg_offset 0x%08x",
				segments->boot_signature, reg_offset);
		BUG();
	}
}

void gr_gk20a_load_ctxsw_ucode_boot(struct gk20a *g, u64 addr_base,
	struct gk20a_ctxsw_ucode_segments *segments, u32 reg_offset)
{
	u32 addr_load32;
	u32 blocks;
	u32 b;
	u32 dst;

	addr_load32 = u64_lo32((addr_base + segments->boot.offset) >> 8);
	blocks = ((segments->boot.size + 0xFF) & ~0xFF) >> 8;

	/*
	 * Set the base FB address for the DMA transfer. Subtract off the 256
	 * byte IMEM block offset such that the relative FB and IMEM offsets
	 * match, allowing the IMEM tags to be properly created.
	 */

	dst = segments->boot_imem_offset;
	gk20a_writel(g, reg_offset + gr_fecs_dmatrfbase_r(),
			(addr_load32 - (dst >> 8)));

	for (b = 0; b < blocks; b++) {
		/* Setup destination IMEM offset */
		gk20a_writel(g, reg_offset + gr_fecs_dmatrfmoffs_r(),
				dst + (b << 8));

		/* Setup source offset (relative to BASE) */
		gk20a_writel(g, reg_offset + gr_fecs_dmatrffboffs_r(),
				dst + (b << 8));

		gk20a_writel(g, reg_offset + gr_fecs_dmatrfcmd_r(),
				gr_fecs_dmatrfcmd_imem_f(0x01) |
				gr_fecs_dmatrfcmd_write_f(0x00) |
				gr_fecs_dmatrfcmd_size_f(0x06) |
				gr_fecs_dmatrfcmd_ctxdma_f(0));
	}

	/* Specify the falcon boot vector */
	gk20a_writel(g, reg_offset + gr_fecs_bootvec_r(),
			gr_fecs_bootvec_vec_f(segments->boot_entry));
}

static int gr_gk20a_load_ctxsw_ucode_segments(struct gk20a *g, u64 addr_base,
	struct gk20a_ctxsw_ucode_segments *segments, u32 reg_offset)
{
	gk20a_writel(g, reg_offset + gr_fecs_dmactl_r(),
			gr_fecs_dmactl_require_ctx_f(0));

	/* Copy falcon bootloader into dmem */
	gr_gk20a_load_ctxsw_ucode_header(g, addr_base, segments, reg_offset);
	gr_gk20a_load_ctxsw_ucode_boot(g, addr_base, segments, reg_offset);

	/* Write to CPUCTL to start the falcon */
	gk20a_writel(g, reg_offset + gr_fecs_cpuctl_r(),
			gr_fecs_cpuctl_startcpu_f(0x01));

	return 0;
}

static void gr_gk20a_load_falcon_with_bootloader(struct gk20a *g)
{
	struct gk20a_ctxsw_ucode_info *ucode_info = &g->ctxsw_ucode_info;
	u64 addr_base = ucode_info->surface_desc.gpu_va;

	gk20a_writel(g, gr_fecs_ctxsw_mailbox_clear_r(0), 0x0);

	gr_gk20a_load_falcon_bind_instblk(g);

	g->ops.gr.falcon_load_ucode(g, addr_base,
		&g->ctxsw_ucode_info.fecs, 0);

	g->ops.gr.falcon_load_ucode(g, addr_base,
		&g->ctxsw_ucode_info.gpccs,
		gr_gpcs_gpccs_falcon_hwcfg_r() -
		gr_fecs_falcon_hwcfg_r());
}

int gr_gk20a_load_ctxsw_ucode(struct gk20a *g)
{
	int err;

	gk20a_dbg_fn("");

	if (tegra_platform_is_linsim()) {
		gk20a_writel(g, gr_fecs_ctxsw_mailbox_r(7),
			gr_fecs_ctxsw_mailbox_value_f(0xc0de7777));
		gk20a_writel(g, gr_gpccs_ctxsw_mailbox_r(7),
			gr_gpccs_ctxsw_mailbox_value_f(0xc0de7777));
	}

	/*
	 * In case bootloader is not supported, revert to the old way of
	 * loading gr ucode, without the faster bootstrap routine.
	 */
	if (!g->ops.gr_ctx.use_dma_for_fw_bootstrap) {
		gr_gk20a_load_falcon_dmem(g);
		gr_gk20a_load_falcon_imem(g);
		gr_gk20a_start_falcon_ucode(g);
	} else {
		if (!g->gr.skip_ucode_init) {
			err = gr_gk20a_init_ctxsw_ucode(g);

			if (err)
				return err;
		}
		gr_gk20a_load_falcon_with_bootloader(g);
		g->gr.skip_ucode_init = true;
	}
	gk20a_dbg_fn("done");
	return 0;
}

static int gr_gk20a_wait_ctxsw_ready(struct gk20a *g)
{
	u32 ret;

	gk20a_dbg_fn("");

	ret = gr_gk20a_ctx_wait_ucode(g, 0, NULL,
				      GR_IS_UCODE_OP_EQUAL,
				      eUcodeHandshakeInitComplete,
				      GR_IS_UCODE_OP_SKIP, 0, false);
	if (ret) {
		gk20a_err(dev_from_gk20a(g), "falcon ucode init timeout");
		return ret;
	}

	if (g->ops.gr_ctx.use_dma_for_fw_bootstrap || g->ops.securegpccs)
		gk20a_writel(g, gr_fecs_current_ctx_r(),
			gr_fecs_current_ctx_valid_false_f());

	gk20a_writel(g, gr_fecs_ctxsw_mailbox_clear_r(0), 0xffffffff);
	gk20a_writel(g, gr_fecs_method_data_r(), 0x7fffffff);
	gk20a_writel(g, gr_fecs_method_push_r(),
		     gr_fecs_method_push_adr_set_watchdog_timeout_f());

	gk20a_dbg_fn("done");
	return 0;
}

int gr_gk20a_init_ctx_state(struct gk20a *g)
{
	u32 ret;
	struct fecs_method_op_gk20a op = {
		.mailbox = { .id = 0, .data = 0,
			     .clr = ~0, .ok = 0, .fail = 0},
		.method.data = 0,
		.cond.ok = GR_IS_UCODE_OP_NOT_EQUAL,
		.cond.fail = GR_IS_UCODE_OP_SKIP,
		};

	gk20a_dbg_fn("");
	if (!g->gr.ctx_vars.golden_image_size) {
		op.method.addr =
			gr_fecs_method_push_adr_discover_image_size_v();
		op.mailbox.ret = &g->gr.ctx_vars.golden_image_size;
		ret = gr_gk20a_submit_fecs_method_op(g, op, false);
		if (ret) {
			gk20a_err(dev_from_gk20a(g),
				   "query golden image size failed");
			return ret;
		}
		op.method.addr =
			gr_fecs_method_push_adr_discover_zcull_image_size_v();
		op.mailbox.ret = &g->gr.ctx_vars.zcull_ctxsw_image_size;
		ret = gr_gk20a_submit_fecs_method_op(g, op, false);
		if (ret) {
			gk20a_err(dev_from_gk20a(g),
				   "query zcull ctx image size failed");
			return ret;
		}
		op.method.addr =
			gr_fecs_method_push_adr_discover_pm_image_size_v();
		op.mailbox.ret = &g->gr.ctx_vars.pm_ctxsw_image_size;
		ret = gr_gk20a_submit_fecs_method_op(g, op, false);
		if (ret) {
			gk20a_err(dev_from_gk20a(g),
				   "query pm ctx image size failed");
			return ret;
		}
		g->gr.ctx_vars.priv_access_map_size = 512 * 1024;
	}

	gk20a_dbg_fn("done");
	return 0;
}

static void gk20a_gr_destroy_ctx_buffer(struct gk20a *g,
					struct gr_ctx_buffer_desc *desc)
{
	if (!desc)
		return;
	gk20a_gmmu_free_attr(g, DMA_ATTR_NO_KERNEL_MAPPING, &desc->mem);
	desc->destroy = NULL;
}

static int gk20a_gr_alloc_ctx_buffer(struct gk20a *g,
				     struct gr_ctx_buffer_desc *desc,
				     size_t size)
{
	int err = 0;

	err = gk20a_gmmu_alloc_attr_sys(g, DMA_ATTR_NO_KERNEL_MAPPING,
				    size, &desc->mem);
	if (err)
		return err;

	desc->destroy = gk20a_gr_destroy_ctx_buffer;

	return err;
}

static void gr_gk20a_free_global_ctx_buffers(struct gk20a *g)
{
	struct gr_gk20a *gr = &g->gr;
	u32 i;

	for (i = 0; i < NR_GLOBAL_CTX_BUF; i++) {
		/* destroy exists iff buffer is allocated */
		if (gr->global_ctx_buffer[i].destroy) {
			gr->global_ctx_buffer[i].destroy(g,
					&gr->global_ctx_buffer[i]);
		}
	}

	gk20a_dbg_fn("done");
}

static int gr_gk20a_alloc_global_ctx_buffers(struct gk20a *g)
{
	struct gk20a_platform *platform = dev_get_drvdata(g->dev);
	struct gr_gk20a *gr = &g->gr;
	int attr_buffer_size, err;
	struct device *dev = g->dev;

	u32 cb_buffer_size = gr->bundle_cb_default_size *
		gr_scc_bundle_cb_size_div_256b_byte_granularity_v();

	u32 pagepool_buffer_size = g->ops.gr.pagepool_default_size(g) *
		gr_scc_pagepool_total_pages_byte_granularity_v();

	gk20a_dbg_fn("");

	attr_buffer_size = g->ops.gr.calc_global_ctx_buffer_size(g);

	gk20a_dbg_info("cb_buffer_size : %d", cb_buffer_size);

	err = gk20a_gr_alloc_ctx_buffer(g, &gr->global_ctx_buffer[CIRCULAR],
					cb_buffer_size);
	if (err)
		goto clean_up;

	if (platform->secure_alloc)
		platform->secure_alloc(dev,
				       &gr->global_ctx_buffer[CIRCULAR_VPR],
				       cb_buffer_size);

	gk20a_dbg_info("pagepool_buffer_size : %d", pagepool_buffer_size);

	err = gk20a_gr_alloc_ctx_buffer(g, &gr->global_ctx_buffer[PAGEPOOL],
					pagepool_buffer_size);
	if (err)
		goto clean_up;

	if (platform->secure_alloc)
		platform->secure_alloc(dev,
				       &gr->global_ctx_buffer[PAGEPOOL_VPR],
				       pagepool_buffer_size);

	gk20a_dbg_info("attr_buffer_size : %d", attr_buffer_size);

	err = gk20a_gr_alloc_ctx_buffer(g, &gr->global_ctx_buffer[ATTRIBUTE],
					attr_buffer_size);
	if (err)
		goto clean_up;

	if (platform->secure_alloc)
		platform->secure_alloc(dev,
				       &gr->global_ctx_buffer[ATTRIBUTE_VPR],
				       attr_buffer_size);

	if (platform->secure_buffer.destroy)
		platform->secure_buffer.destroy(dev, &platform->secure_buffer);

	gk20a_dbg_info("golden_image_size : %d",
		   gr->ctx_vars.golden_image_size);

	err = gk20a_gr_alloc_ctx_buffer(g,
					&gr->global_ctx_buffer[GOLDEN_CTX],
					gr->ctx_vars.golden_image_size);
	if (err)
		goto clean_up;

	gk20a_dbg_info("priv_access_map_size : %d",
		   gr->ctx_vars.priv_access_map_size);

	err = gk20a_gr_alloc_ctx_buffer(g,
					&gr->global_ctx_buffer[PRIV_ACCESS_MAP],
					gr->ctx_vars.priv_access_map_size);

	if (err)
		goto clean_up;

	gk20a_dbg_fn("done");
	return 0;

 clean_up:
	gk20a_err(dev_from_gk20a(g), "fail");
	gr_gk20a_free_global_ctx_buffers(g);
	return -ENOMEM;
}

static int gr_gk20a_map_global_ctx_buffers(struct gk20a *g,
					struct channel_gk20a *c)
{
	struct vm_gk20a *ch_vm = c->vm;
	u64 *g_bfr_va = c->ch_ctx.global_ctx_buffer_va;
	u64 *g_bfr_size = c->ch_ctx.global_ctx_buffer_size;
	struct gr_gk20a *gr = &g->gr;
	struct mem_desc *mem;
	u64 gpu_va;
	u32 i;
	gk20a_dbg_fn("");

	/* Circular Buffer */
	if (!c->vpr || (gr->global_ctx_buffer[CIRCULAR_VPR].mem.sgt == NULL)) {
		mem = &gr->global_ctx_buffer[CIRCULAR].mem;
	} else {
		mem = &gr->global_ctx_buffer[CIRCULAR_VPR].mem;
	}

	gpu_va = gk20a_gmmu_map(ch_vm, &mem->sgt, mem->size,
				NVGPU_MAP_BUFFER_FLAGS_CACHEABLE_TRUE,
				gk20a_mem_flag_none, true, mem->aperture);
	if (!gpu_va)
		goto clean_up;
	g_bfr_va[CIRCULAR_VA] = gpu_va;
	g_bfr_size[CIRCULAR_VA] = mem->size;

	/* Attribute Buffer */
	if (!c->vpr || (gr->global_ctx_buffer[ATTRIBUTE_VPR].mem.sgt == NULL)) {
		mem = &gr->global_ctx_buffer[ATTRIBUTE].mem;
	} else {
		mem = &gr->global_ctx_buffer[ATTRIBUTE_VPR].mem;
	}

	gpu_va = gk20a_gmmu_map(ch_vm, &mem->sgt, mem->size,
				NVGPU_MAP_BUFFER_FLAGS_CACHEABLE_TRUE,
				gk20a_mem_flag_none, false, mem->aperture);
	if (!gpu_va)
		goto clean_up;
	g_bfr_va[ATTRIBUTE_VA] = gpu_va;
	g_bfr_size[ATTRIBUTE_VA] = mem->size;

	/* Page Pool */
	if (!c->vpr || (gr->global_ctx_buffer[PAGEPOOL_VPR].mem.sgt == NULL)) {
		mem = &gr->global_ctx_buffer[PAGEPOOL].mem;
	} else {
		mem = &gr->global_ctx_buffer[PAGEPOOL_VPR].mem;
	}

	gpu_va = gk20a_gmmu_map(ch_vm, &mem->sgt, mem->size,
				NVGPU_MAP_BUFFER_FLAGS_CACHEABLE_TRUE,
				gk20a_mem_flag_none, true, mem->aperture);
	if (!gpu_va)
		goto clean_up;
	g_bfr_va[PAGEPOOL_VA] = gpu_va;
	g_bfr_size[PAGEPOOL_VA] = mem->size;

	/* Golden Image */
	mem = &gr->global_ctx_buffer[GOLDEN_CTX].mem;
	gpu_va = gk20a_gmmu_map(ch_vm, &mem->sgt, mem->size, 0,
				gk20a_mem_flag_none, true, mem->aperture);
	if (!gpu_va)
		goto clean_up;
	g_bfr_va[GOLDEN_CTX_VA] = gpu_va;
	g_bfr_size[GOLDEN_CTX_VA] = mem->size;

	/* Priv register Access Map */
	mem = &gr->global_ctx_buffer[PRIV_ACCESS_MAP].mem;
	gpu_va = gk20a_gmmu_map(ch_vm, &mem->sgt, mem->size, 0,
				gk20a_mem_flag_none, true, mem->aperture);
	if (!gpu_va)
		goto clean_up;
	g_bfr_va[PRIV_ACCESS_MAP_VA] = gpu_va;
	g_bfr_size[PRIV_ACCESS_MAP_VA] = mem->size;

	c->ch_ctx.global_ctx_buffer_mapped = true;
	return 0;

 clean_up:
	for (i = 0; i < NR_GLOBAL_CTX_BUF_VA; i++) {
		if (g_bfr_va[i]) {
			gk20a_gmmu_unmap(ch_vm, g_bfr_va[i],
					 gr->global_ctx_buffer[i].mem.size,
					 gk20a_mem_flag_none);
			g_bfr_va[i] = 0;
		}
	}
	return -ENOMEM;
}

static void gr_gk20a_unmap_global_ctx_buffers(struct channel_gk20a *c)
{
	struct vm_gk20a *ch_vm = c->vm;
	u64 *g_bfr_va = c->ch_ctx.global_ctx_buffer_va;
	u64 *g_bfr_size = c->ch_ctx.global_ctx_buffer_size;
	u32 i;

	gk20a_dbg_fn("");

	for (i = 0; i < NR_GLOBAL_CTX_BUF_VA; i++) {
		if (g_bfr_va[i]) {
			gk20a_gmmu_unmap(ch_vm, g_bfr_va[i],
					 g_bfr_size[i],
					 gk20a_mem_flag_none);
			g_bfr_va[i] = 0;
			g_bfr_size[i] = 0;
		}
	}
	c->ch_ctx.global_ctx_buffer_mapped = false;
}

int gr_gk20a_alloc_gr_ctx(struct gk20a *g,
			  struct gr_ctx_desc **__gr_ctx, struct vm_gk20a *vm,
			  u32 class,
			  u32 padding)
{
	struct gr_ctx_desc *gr_ctx = NULL;
	struct gr_gk20a *gr = &g->gr;
	int err = 0;

	gk20a_dbg_fn("");

	if (gr->ctx_vars.buffer_size == 0)
		return 0;

	/* alloc channel gr ctx buffer */
	gr->ctx_vars.buffer_size = gr->ctx_vars.golden_image_size;
	gr->ctx_vars.buffer_total_size = gr->ctx_vars.golden_image_size;

	gr_ctx = kzalloc(sizeof(*gr_ctx), GFP_KERNEL);
	if (!gr_ctx)
		return -ENOMEM;

	err = gk20a_gmmu_alloc_attr(g, DMA_ATTR_NO_KERNEL_MAPPING,
					gr->ctx_vars.buffer_total_size,
					&gr_ctx->mem);
	if (err)
		goto err_free_ctx;

	gr_ctx->mem.gpu_va = gk20a_gmmu_map(vm,
					&gr_ctx->mem.sgt,
					gr_ctx->mem.size,
					NVGPU_MAP_BUFFER_FLAGS_CACHEABLE_TRUE,
					gk20a_mem_flag_none, true,
					gr_ctx->mem.aperture);
	if (!gr_ctx->mem.gpu_va)
		goto err_free_mem;

	*__gr_ctx = gr_ctx;

	return 0;

 err_free_mem:
	gk20a_gmmu_free_attr(g, DMA_ATTR_NO_KERNEL_MAPPING, &gr_ctx->mem);
 err_free_ctx:
	kfree(gr_ctx);
	gr_ctx = NULL;

	return err;
}

static int gr_gk20a_alloc_tsg_gr_ctx(struct gk20a *g,
			struct tsg_gk20a *tsg, u32 class, u32 padding)
{
	struct gr_ctx_desc **gr_ctx = &tsg->tsg_gr_ctx;
	int err;

	if (!tsg->vm) {
		gk20a_err(dev_from_gk20a(tsg->g), "No address space bound\n");
		return -ENOMEM;
	}

	err = g->ops.gr.alloc_gr_ctx(g, gr_ctx, tsg->vm, class, padding);
	if (err)
		return err;

	return 0;
}

static int gr_gk20a_alloc_channel_gr_ctx(struct gk20a *g,
				struct channel_gk20a *c,
				u32 class,
				u32 padding)
{
	struct gr_ctx_desc **gr_ctx = &c->ch_ctx.gr_ctx;
	int err = g->ops.gr.alloc_gr_ctx(g, gr_ctx, c->vm, class, padding);
	if (err)
		return err;

	return 0;
}

void gr_gk20a_free_gr_ctx(struct gk20a *g,
			  struct vm_gk20a *vm, struct gr_ctx_desc *gr_ctx)
{
	gk20a_dbg_fn("");

	if (!gr_ctx || !gr_ctx->mem.gpu_va)
		return;

	gk20a_gmmu_unmap(vm, gr_ctx->mem.gpu_va,
		gr_ctx->mem.size, gk20a_mem_flag_none);
	gk20a_gmmu_free_attr(g, DMA_ATTR_NO_KERNEL_MAPPING, &gr_ctx->mem);
	kfree(gr_ctx);
}

void gr_gk20a_free_tsg_gr_ctx(struct tsg_gk20a *tsg)
{
	if (!tsg->vm) {
		gk20a_err(dev_from_gk20a(tsg->g), "No address space bound\n");
		return;
	}
	tsg->g->ops.gr.free_gr_ctx(tsg->g, tsg->vm, tsg->tsg_gr_ctx);
	tsg->tsg_gr_ctx = NULL;
}

static void gr_gk20a_free_channel_gr_ctx(struct channel_gk20a *c)
{
	c->g->ops.gr.free_gr_ctx(c->g, c->vm, c->ch_ctx.gr_ctx);
	c->ch_ctx.gr_ctx = NULL;
}

static int gr_gk20a_alloc_channel_patch_ctx(struct gk20a *g,
				struct channel_gk20a *c)
{
	struct patch_desc *patch_ctx = &c->ch_ctx.patch_ctx;
	struct vm_gk20a *ch_vm = c->vm;
	int err = 0;

	gk20a_dbg_fn("");

	err = gk20a_gmmu_alloc_map_attr_sys(ch_vm, DMA_ATTR_NO_KERNEL_MAPPING,
					128 * sizeof(u32), &patch_ctx->mem);
	if (err)
		return err;

	gk20a_dbg_fn("done");
	return 0;
}

static void gr_gk20a_free_channel_patch_ctx(struct channel_gk20a *c)
{
	struct patch_desc *patch_ctx = &c->ch_ctx.patch_ctx;
	struct gk20a *g = c->g;

	gk20a_dbg_fn("");

	if (patch_ctx->mem.gpu_va)
		gk20a_gmmu_unmap(c->vm, patch_ctx->mem.gpu_va,
				 patch_ctx->mem.size, gk20a_mem_flag_none);

	gk20a_gmmu_free_attr(g, DMA_ATTR_NO_KERNEL_MAPPING, &patch_ctx->mem);
	patch_ctx->data_count = 0;
}

static void gr_gk20a_free_channel_pm_ctx(struct channel_gk20a *c)
{
	struct pm_ctx_desc *pm_ctx = &c->ch_ctx.pm_ctx;
	struct gk20a *g = c->g;

	gk20a_dbg_fn("");

	if (pm_ctx->mem.gpu_va) {
		gk20a_gmmu_unmap(c->vm, pm_ctx->mem.gpu_va,
				 pm_ctx->mem.size, gk20a_mem_flag_none);

		gk20a_gmmu_free_attr(g, DMA_ATTR_NO_KERNEL_MAPPING, &pm_ctx->mem);
	}
}

void gk20a_free_channel_ctx(struct channel_gk20a *c)
{
	gr_gk20a_unmap_global_ctx_buffers(c);
	gr_gk20a_free_channel_patch_ctx(c);
	gr_gk20a_free_channel_pm_ctx(c);
	if (!gk20a_is_channel_marked_as_tsg(c))
		gr_gk20a_free_channel_gr_ctx(c);

	/* zcull_ctx */

	memset(&c->ch_ctx, 0, sizeof(struct channel_ctx_gk20a));

	c->first_init = false;
}

static bool gr_gk20a_is_valid_class(struct gk20a *g, u32 class_num)
{
	bool valid = false;

	switch (class_num) {
	case KEPLER_COMPUTE_A:
	case KEPLER_C:
	case FERMI_TWOD_A:
	case KEPLER_DMA_COPY_A:
		valid = true;
		break;

	default:
		break;
	}

	return valid;
}

int gk20a_alloc_obj_ctx(struct channel_gk20a  *c,
			struct nvgpu_alloc_obj_ctx_args *args)
{
	struct gk20a *g = c->g;
	struct fifo_gk20a *f = &g->fifo;
	struct channel_ctx_gk20a *ch_ctx = &c->ch_ctx;
	struct tsg_gk20a *tsg = NULL;
	int err = 0;

	gk20a_dbg_fn("");

	/* an address space needs to have been bound at this point.*/
	if (!gk20a_channel_as_bound(c) && !c->vm) {
		gk20a_err(dev_from_gk20a(g),
			   "not bound to address space at time"
			   " of grctx allocation");
		return -EINVAL;
	}

	if (!g->ops.gr.is_valid_class(g, args->class_num)) {
		gk20a_err(dev_from_gk20a(g),
			   "invalid obj class 0x%x", args->class_num);
		err = -EINVAL;
		goto out;
	}
	c->obj_class = args->class_num;

	if (gk20a_is_channel_marked_as_tsg(c))
		tsg = &f->tsg[c->tsgid];

	/* allocate gr ctx buffer */
	if (!tsg) {
		if (!ch_ctx->gr_ctx) {
			err = gr_gk20a_alloc_channel_gr_ctx(g, c,
							    args->class_num,
							    args->flags);
			if (err) {
				gk20a_err(dev_from_gk20a(g),
					"fail to allocate gr ctx buffer");
				goto out;
			}
		} else {
			/*TBD: needs to be more subtle about which is
			 * being allocated as some are allowed to be
			 * allocated along same channel */
			gk20a_err(dev_from_gk20a(g),
				"too many classes alloc'd on same channel");
			err = -EINVAL;
			goto out;
		}
	} else {
		if (!tsg->tsg_gr_ctx) {
			tsg->vm = c->vm;
			gk20a_vm_get(tsg->vm);
			err = gr_gk20a_alloc_tsg_gr_ctx(g, tsg,
							args->class_num,
							args->flags);
			if (err) {
				gk20a_err(dev_from_gk20a(g),
					"fail to allocate TSG gr ctx buffer");
				gk20a_vm_put(tsg->vm);
				tsg->vm = NULL;
				goto out;
			}
		}
		ch_ctx->gr_ctx = tsg->tsg_gr_ctx;
	}

	/* PM ctxt switch is off by default */
	ch_ctx->pm_ctx.pm_mode = ctxsw_prog_main_image_pm_mode_no_ctxsw_f();

	/* commit gr ctx buffer */
	err = gr_gk20a_commit_inst(c, ch_ctx->gr_ctx->mem.gpu_va);
	if (err) {
		gk20a_err(dev_from_gk20a(g),
			"fail to commit gr ctx buffer");
		goto out;
	}

	/* allocate patch buffer */
	if (ch_ctx->patch_ctx.mem.sgt == NULL) {
		err = gr_gk20a_alloc_channel_patch_ctx(g, c);
		if (err) {
			gk20a_err(dev_from_gk20a(g),
				"fail to allocate patch buffer");
			goto out;
		}
	}

	/* map global buffer to channel gpu_va and commit */
	if (!ch_ctx->global_ctx_buffer_mapped) {
		err = gr_gk20a_map_global_ctx_buffers(g, c);
		if (err) {
			gk20a_err(dev_from_gk20a(g),
				"fail to map global ctx buffer");
			goto out;
		}
		gr_gk20a_elpg_protected_call(g,
			gr_gk20a_commit_global_ctx_buffers(g, c, true));
	}

	/* tweak any perf parameters per-context here */
	if (args->class_num == KEPLER_COMPUTE_A) {
		u32 tex_lock_disable_mask;
		u32 texlock;
		u32 lockboost_mask;
		u32 lockboost;

		if (support_gk20a_pmu(g->dev)) {
			err = gk20a_pmu_disable_elpg(g);
			if (err) {
				gk20a_err(dev_from_gk20a(g),
						"failed to set disable elpg");
			}
		}

		tex_lock_disable_mask =
			gr_gpcs_tpcs_sm_sch_texlock_tex_hash_m()         |
			gr_gpcs_tpcs_sm_sch_texlock_tex_hash_tile_m()    |
			gr_gpcs_tpcs_sm_sch_texlock_tex_hash_phase_m()   |
			gr_gpcs_tpcs_sm_sch_texlock_tex_hash_tex_m()     |
			gr_gpcs_tpcs_sm_sch_texlock_tex_hash_timeout_m() |
			gr_gpcs_tpcs_sm_sch_texlock_dot_t_unlock_m();

		texlock = gk20a_readl(g, gr_gpcs_tpcs_sm_sch_texlock_r());

		texlock = (texlock & ~tex_lock_disable_mask) |
		(gr_gpcs_tpcs_sm_sch_texlock_tex_hash_disable_f()         |
		 gr_gpcs_tpcs_sm_sch_texlock_tex_hash_tile_disable_f()    |
		 gr_gpcs_tpcs_sm_sch_texlock_tex_hash_phase_disable_f()   |
		 gr_gpcs_tpcs_sm_sch_texlock_tex_hash_tex_disable_f()     |
		 gr_gpcs_tpcs_sm_sch_texlock_tex_hash_timeout_disable_f() |
		 gr_gpcs_tpcs_sm_sch_texlock_dot_t_unlock_disable_f());

		lockboost_mask =
			gr_gpcs_tpcs_sm_sch_macro_sched_lockboost_size_m();

		lockboost = gk20a_readl(g, gr_gpcs_tpcs_sm_sch_macro_sched_r());
		lockboost = (lockboost & ~lockboost_mask) |
			gr_gpcs_tpcs_sm_sch_macro_sched_lockboost_size_f(0);

		err = gr_gk20a_ctx_patch_write_begin(g, ch_ctx);

		if (!err) {
			gr_gk20a_ctx_patch_write(g, ch_ctx,
				gr_gpcs_tpcs_sm_sch_texlock_r(),
				texlock, true);
			gr_gk20a_ctx_patch_write(g, ch_ctx,
				gr_gpcs_tpcs_sm_sch_macro_sched_r(),
				lockboost, true);
			gr_gk20a_ctx_patch_write_end(g, ch_ctx);
		} else {
			gk20a_err(dev_from_gk20a(g),
				   "failed to set texlock for compute class");
		}

		args->flags |= NVGPU_ALLOC_OBJ_FLAGS_LOCKBOOST_ZERO;

		if (support_gk20a_pmu(g->dev))
			gk20a_pmu_enable_elpg(g);
	}

	/* init golden image, ELPG enabled after this is done */
	err = gr_gk20a_init_golden_ctx_image(g, c);
	if (err) {
		gk20a_err(dev_from_gk20a(g),
			"fail to init golden ctx image");
		goto out;
	}

	/* load golden image */
	if (!c->first_init) {
		err = gr_gk20a_elpg_protected_call(g,
			gr_gk20a_load_golden_ctx_image(g, c));
		if (err) {
			gk20a_err(dev_from_gk20a(g),
				"fail to load golden ctx image");
			goto out;
		}
		if (g->ops.fecs_trace.bind_channel) {
			err = g->ops.fecs_trace.bind_channel(g, c);
			if (err) {
				gk20a_warn(dev_from_gk20a(g),
					"fail to bind channel for ctxsw trace");
			}
		}
		c->first_init = true;
	}

	gk20a_dbg_fn("done");
	return 0;
out:
	/* 1. gr_ctx, patch_ctx and global ctx buffer mapping
	   can be reused so no need to release them.
	   2. golden image init and load is a one time thing so if
	   they pass, no need to undo. */
	gk20a_err(dev_from_gk20a(g), "fail");
	return err;
}

int gk20a_comptag_allocator_init(struct gk20a_comptag_allocator *allocator,
		unsigned long size)
{
	mutex_init(&allocator->lock);
	/*
	 * 0th comptag is special and is never used. The base for this bitmap
	 * is 1, and its size is one less than the size of comptag store.
	 */
	size--;
	allocator->bitmap = vzalloc(BITS_TO_LONGS(size) * sizeof(long));
	if (!allocator->bitmap)
		return -ENOMEM;
	allocator->size = size;
	return 0;
}

void gk20a_comptag_allocator_destroy(struct gk20a_comptag_allocator *allocator)
{
	/*
	 * called only when exiting the driver (gk20a_remove, or unwinding the
	 * init stage); no users should be active, so taking the mutex is
	 * unnecessary here.
	 */
	allocator->size = 0;
	vfree(allocator->bitmap);
}

static void gk20a_remove_gr_support(struct gr_gk20a *gr)
{
	struct gk20a *g = gr->g;

	gk20a_dbg_fn("");

	gr_gk20a_free_cyclestats_snapshot_data(g);

	gr_gk20a_free_global_ctx_buffers(g);

	gk20a_gmmu_free(g, &gr->mmu_wr_mem);
	gk20a_gmmu_free(g, &gr->mmu_rd_mem);

	gk20a_gmmu_free_attr(g, DMA_ATTR_NO_KERNEL_MAPPING,
			     &gr->compbit_store.mem);

	memset(&gr->compbit_store, 0, sizeof(struct compbit_store_desc));

	kfree(gr->sm_error_states);
	kfree(gr->gpc_tpc_count);
	kfree(gr->gpc_zcb_count);
	kfree(gr->gpc_ppc_count);
	kfree(gr->pes_tpc_count[0]);
	kfree(gr->pes_tpc_count[1]);
	kfree(gr->pes_tpc_mask[0]);
	kfree(gr->pes_tpc_mask[1]);
	kfree(gr->sm_to_cluster);
	kfree(gr->gpc_skip_mask);
	kfree(gr->map_tiles);
	kfree(gr->fbp_rop_l2_en_mask);
	gr->gpc_tpc_count = NULL;
	gr->gpc_zcb_count = NULL;
	gr->gpc_ppc_count = NULL;
	gr->pes_tpc_count[0] = NULL;
	gr->pes_tpc_count[1] = NULL;
	gr->pes_tpc_mask[0] = NULL;
	gr->pes_tpc_mask[1] = NULL;
	gr->gpc_skip_mask = NULL;
	gr->map_tiles = NULL;
	gr->fbp_rop_l2_en_mask = NULL;

	gr->ctx_vars.valid = false;
	kfree(gr->ctx_vars.ucode.fecs.inst.l);
	kfree(gr->ctx_vars.ucode.fecs.data.l);
	kfree(gr->ctx_vars.ucode.gpccs.inst.l);
	kfree(gr->ctx_vars.ucode.gpccs.data.l);
	kfree(gr->ctx_vars.sw_bundle_init.l);
	kfree(gr->ctx_vars.sw_method_init.l);
	kfree(gr->ctx_vars.sw_ctx_load.l);
	kfree(gr->ctx_vars.sw_non_ctx_load.l);
	kfree(gr->ctx_vars.ctxsw_regs.sys.l);
	kfree(gr->ctx_vars.ctxsw_regs.gpc.l);
	kfree(gr->ctx_vars.ctxsw_regs.tpc.l);
	kfree(gr->ctx_vars.ctxsw_regs.zcull_gpc.l);
	kfree(gr->ctx_vars.ctxsw_regs.ppc.l);
	kfree(gr->ctx_vars.ctxsw_regs.pm_sys.l);
	kfree(gr->ctx_vars.ctxsw_regs.pm_gpc.l);
	kfree(gr->ctx_vars.ctxsw_regs.pm_tpc.l);
	kfree(gr->ctx_vars.ctxsw_regs.pm_ppc.l);
	kfree(gr->ctx_vars.ctxsw_regs.perf_sys.l);
	kfree(gr->ctx_vars.ctxsw_regs.fbp.l);
	kfree(gr->ctx_vars.ctxsw_regs.perf_gpc.l);
	kfree(gr->ctx_vars.ctxsw_regs.fbp_router.l);
	kfree(gr->ctx_vars.ctxsw_regs.gpc_router.l);
	kfree(gr->ctx_vars.ctxsw_regs.pm_ltc.l);
	kfree(gr->ctx_vars.ctxsw_regs.pm_fbpa.l);

	vfree(gr->ctx_vars.local_golden_image);
	gr->ctx_vars.local_golden_image = NULL;

	if (gr->ctx_vars.hwpm_ctxsw_buffer_offset_map)
		nvgpu_free(gr->ctx_vars.hwpm_ctxsw_buffer_offset_map);
	gr->ctx_vars.hwpm_ctxsw_buffer_offset_map = NULL;

	gk20a_comptag_allocator_destroy(&gr->comp_tags);
}

static void gr_gk20a_bundle_cb_defaults(struct gk20a *g)
{
	struct gr_gk20a *gr = &g->gr;

	gr->bundle_cb_default_size =
		gr_scc_bundle_cb_size_div_256b__prod_v();
	gr->min_gpm_fifo_depth =
		gr_pd_ab_dist_cfg2_state_limit_min_gpm_fifo_depths_v();
	gr->bundle_cb_token_limit =
		gr_pd_ab_dist_cfg2_token_limit_init_v();
}

static int gr_gk20a_init_gr_config(struct gk20a *g, struct gr_gk20a *gr)
{
	u32 gpc_index, pes_index;
	u32 pes_tpc_mask;
	u32 pes_tpc_count;
	u32 pes_heavy_index;
	u32 gpc_new_skip_mask;
	u32 tmp;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);

	tmp = gk20a_readl(g, pri_ringmaster_enum_fbp_r());
	gr->num_fbps = pri_ringmaster_enum_fbp_count_v(tmp);

	tmp = gk20a_readl(g, top_num_gpcs_r());
	gr->max_gpc_count = top_num_gpcs_value_v(tmp);

	tmp = gk20a_readl(g, top_num_fbps_r());
	gr->max_fbps_count = top_num_fbps_value_v(tmp);

	gr->fbp_en_mask = g->ops.gr.get_fbp_en_mask(g);

	gr->fbp_rop_l2_en_mask =
		kzalloc(gr->max_fbps_count * sizeof(u32), GFP_KERNEL);
	if (!gr->fbp_rop_l2_en_mask)
		goto clean_up;

	tmp = gk20a_readl(g, top_tpc_per_gpc_r());
	gr->max_tpc_per_gpc_count = top_tpc_per_gpc_value_v(tmp);

	gr->max_tpc_count = gr->max_gpc_count * gr->max_tpc_per_gpc_count;

	tmp = gk20a_readl(g, top_num_fbps_r());
	gr->sys_count = top_num_fbps_value_v(tmp);

	tmp = gk20a_readl(g, pri_ringmaster_enum_gpc_r());
	gr->gpc_count = pri_ringmaster_enum_gpc_count_v(tmp);

	gr->pe_count_per_gpc = nvgpu_get_litter_value(g, GPU_LIT_NUM_PES_PER_GPC);
	if (WARN(gr->pe_count_per_gpc > GK20A_GR_MAX_PES_PER_GPC,
		 "too many pes per gpc\n"))
		goto clean_up;

	gr->max_zcull_per_gpc_count = nvgpu_get_litter_value(g, GPU_LIT_NUM_ZCULL_BANKS);

	if (!gr->gpc_count) {
		gk20a_err(dev_from_gk20a(g), "gpc_count==0!");
		goto clean_up;
	}

	gr->gpc_tpc_count = kzalloc(gr->gpc_count * sizeof(u32), GFP_KERNEL);
	gr->gpc_tpc_mask = kzalloc(gr->gpc_count * sizeof(u32), GFP_KERNEL);
	gr->gpc_zcb_count = kzalloc(gr->gpc_count * sizeof(u32), GFP_KERNEL);
	gr->gpc_ppc_count = kzalloc(gr->gpc_count * sizeof(u32), GFP_KERNEL);

	gr->gpc_skip_mask =
		kzalloc(gr_pd_dist_skip_table__size_1_v() * 4 * sizeof(u32),
			GFP_KERNEL);

	if (!gr->gpc_tpc_count || !gr->gpc_tpc_mask || !gr->gpc_zcb_count ||
	    !gr->gpc_ppc_count || !gr->gpc_skip_mask)
		goto clean_up;

	gr->ppc_count = 0;
	gr->tpc_count = 0;
	gr->zcb_count = 0;
	for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++) {
		tmp = gk20a_readl(g, gr_gpc0_fs_gpc_r() +
				     gpc_stride * gpc_index);

		gr->gpc_tpc_count[gpc_index] =
			gr_gpc0_fs_gpc_num_available_tpcs_v(tmp);
		gr->tpc_count += gr->gpc_tpc_count[gpc_index];

		gr->gpc_zcb_count[gpc_index] =
			gr_gpc0_fs_gpc_num_available_zculls_v(tmp);
		gr->zcb_count += gr->gpc_zcb_count[gpc_index];

		if (g->ops.gr.get_gpc_tpc_mask)
			gr->gpc_tpc_mask[gpc_index] =
				g->ops.gr.get_gpc_tpc_mask(g, gpc_index);

		for (pes_index = 0; pes_index < gr->pe_count_per_gpc; pes_index++) {
			if (!gr->pes_tpc_count[pes_index]) {
				gr->pes_tpc_count[pes_index] =
					kzalloc(gr->gpc_count * sizeof(u32),
						GFP_KERNEL);
				gr->pes_tpc_mask[pes_index] =
					kzalloc(gr->gpc_count * sizeof(u32),
						GFP_KERNEL);
				if (!gr->pes_tpc_count[pes_index] ||
				    !gr->pes_tpc_mask[pes_index])
					goto clean_up;
			}

			tmp = gk20a_readl(g,
				gr_gpc0_gpm_pd_pes_tpc_id_mask_r(pes_index) +
				gpc_index * gpc_stride);

			pes_tpc_mask = gr_gpc0_gpm_pd_pes_tpc_id_mask_mask_v(tmp);
			pes_tpc_count = count_bits(pes_tpc_mask);

			/* detect PES presence by seeing if there are
			 * TPCs connected to it.
			 */
			if (pes_tpc_count != 0)
				gr->gpc_ppc_count[gpc_index]++;

			gr->pes_tpc_count[pes_index][gpc_index] = pes_tpc_count;
			gr->pes_tpc_mask[pes_index][gpc_index] = pes_tpc_mask;
		}

		gr->ppc_count += gr->gpc_ppc_count[gpc_index];

		gpc_new_skip_mask = 0;
		if (gr->pe_count_per_gpc > 1 &&
		    gr->pes_tpc_count[0][gpc_index] +
		    gr->pes_tpc_count[1][gpc_index] == 5) {
			pes_heavy_index =
				gr->pes_tpc_count[0][gpc_index] >
				gr->pes_tpc_count[1][gpc_index] ? 0 : 1;

			gpc_new_skip_mask =
				gr->pes_tpc_mask[pes_heavy_index][gpc_index] ^
				   (gr->pes_tpc_mask[pes_heavy_index][gpc_index] &
				   (gr->pes_tpc_mask[pes_heavy_index][gpc_index] - 1));

		} else if (gr->pe_count_per_gpc > 1 &&
			   (gr->pes_tpc_count[0][gpc_index] +
			    gr->pes_tpc_count[1][gpc_index] == 4) &&
			   (gr->pes_tpc_count[0][gpc_index] !=
			    gr->pes_tpc_count[1][gpc_index])) {
				pes_heavy_index =
				    gr->pes_tpc_count[0][gpc_index] >
				    gr->pes_tpc_count[1][gpc_index] ? 0 : 1;

			gpc_new_skip_mask =
				gr->pes_tpc_mask[pes_heavy_index][gpc_index] ^
				   (gr->pes_tpc_mask[pes_heavy_index][gpc_index] &
				   (gr->pes_tpc_mask[pes_heavy_index][gpc_index] - 1));
		}
		gr->gpc_skip_mask[gpc_index] = gpc_new_skip_mask;
	}

	gr->sm_to_cluster = kzalloc(gr->gpc_count * gr->tpc_count *
							sizeof(struct sm_info), GFP_KERNEL);
	gr->no_of_sm = 0;

	gk20a_dbg_info("fbps: %d", gr->num_fbps);
	gk20a_dbg_info("max_gpc_count: %d", gr->max_gpc_count);
	gk20a_dbg_info("max_fbps_count: %d", gr->max_fbps_count);
	gk20a_dbg_info("max_tpc_per_gpc_count: %d", gr->max_tpc_per_gpc_count);
	gk20a_dbg_info("max_zcull_per_gpc_count: %d", gr->max_zcull_per_gpc_count);
	gk20a_dbg_info("max_tpc_count: %d", gr->max_tpc_count);
	gk20a_dbg_info("sys_count: %d", gr->sys_count);
	gk20a_dbg_info("gpc_count: %d", gr->gpc_count);
	gk20a_dbg_info("pe_count_per_gpc: %d", gr->pe_count_per_gpc);
	gk20a_dbg_info("tpc_count: %d", gr->tpc_count);
	gk20a_dbg_info("ppc_count: %d", gr->ppc_count);

	for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++)
		gk20a_dbg_info("gpc_tpc_count[%d] : %d",
			   gpc_index, gr->gpc_tpc_count[gpc_index]);
	for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++)
		gk20a_dbg_info("gpc_zcb_count[%d] : %d",
			   gpc_index, gr->gpc_zcb_count[gpc_index]);
	for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++)
		gk20a_dbg_info("gpc_ppc_count[%d] : %d",
			   gpc_index, gr->gpc_ppc_count[gpc_index]);
	for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++)
		gk20a_dbg_info("gpc_skip_mask[%d] : %d",
			   gpc_index, gr->gpc_skip_mask[gpc_index]);
	for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++)
		for (pes_index = 0;
		     pes_index < gr->pe_count_per_gpc;
		     pes_index++)
			gk20a_dbg_info("pes_tpc_count[%d][%d] : %d",
				   pes_index, gpc_index,
				   gr->pes_tpc_count[pes_index][gpc_index]);

	for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++)
		for (pes_index = 0;
		     pes_index < gr->pe_count_per_gpc;
		     pes_index++)
			gk20a_dbg_info("pes_tpc_mask[%d][%d] : %d",
				   pes_index, gpc_index,
				   gr->pes_tpc_mask[pes_index][gpc_index]);

	g->ops.gr.bundle_cb_defaults(g);
	g->ops.gr.cb_size_default(g);
	g->ops.gr.calc_global_ctx_buffer_size(g);
	gr->timeslice_mode = gr_gpcs_ppcs_cbm_cfg_timeslice_mode_enable_v();

	gk20a_dbg_info("bundle_cb_default_size: %d",
		   gr->bundle_cb_default_size);
	gk20a_dbg_info("min_gpm_fifo_depth: %d", gr->min_gpm_fifo_depth);
	gk20a_dbg_info("bundle_cb_token_limit: %d", gr->bundle_cb_token_limit);
	gk20a_dbg_info("attrib_cb_default_size: %d",
		   gr->attrib_cb_default_size);
	gk20a_dbg_info("attrib_cb_size: %d", gr->attrib_cb_size);
	gk20a_dbg_info("alpha_cb_default_size: %d", gr->alpha_cb_default_size);
	gk20a_dbg_info("alpha_cb_size: %d", gr->alpha_cb_size);
	gk20a_dbg_info("timeslice_mode: %d", gr->timeslice_mode);

	return 0;

clean_up:
	return -ENOMEM;
}

static int gr_gk20a_init_mmu_sw(struct gk20a *g, struct gr_gk20a *gr)
{
	int err;

	err = gk20a_gmmu_alloc_sys(g, 0x1000, &gr->mmu_wr_mem);
	if (err)
		goto err;

	err = gk20a_gmmu_alloc_sys(g, 0x1000, &gr->mmu_rd_mem);
	if (err)
		goto err_free_wr_mem;
	return 0;

 err_free_wr_mem:
	gk20a_gmmu_free(g, &gr->mmu_wr_mem);
 err:
	return -ENOMEM;
}

static u32 prime_set[18] = {
	2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61 };

static int gr_gk20a_init_map_tiles(struct gk20a *g, struct gr_gk20a *gr)
{
	s32 comm_denom;
	s32 mul_factor;
	s32 *init_frac = NULL;
	s32 *init_err = NULL;
	s32 *run_err = NULL;
	s32 *sorted_num_tpcs = NULL;
	s32 *sorted_to_unsorted_gpc_map = NULL;
	u32 gpc_index;
	u32 gpc_mark = 0;
	u32 num_tpc;
	u32 max_tpc_count = 0;
	u32 swap;
	u32 tile_count;
	u32 index;
	bool delete_map = false;
	bool gpc_sorted;
	int ret = 0;
	int num_gpcs = nvgpu_get_litter_value(g, GPU_LIT_NUM_GPCS);
	int num_tpc_per_gpc = nvgpu_get_litter_value(g, GPU_LIT_NUM_TPC_PER_GPC);

	init_frac = kzalloc(num_gpcs * sizeof(s32), GFP_KERNEL);
	init_err = kzalloc(num_gpcs * sizeof(s32), GFP_KERNEL);
	run_err = kzalloc(num_gpcs * sizeof(s32), GFP_KERNEL);
	sorted_num_tpcs =
		kzalloc(num_gpcs * num_tpc_per_gpc * sizeof(s32),
			GFP_KERNEL);
	sorted_to_unsorted_gpc_map =
		kzalloc(num_gpcs * sizeof(s32), GFP_KERNEL);

	if (!(init_frac && init_err && run_err && sorted_num_tpcs &&
	      sorted_to_unsorted_gpc_map)) {
		ret = -ENOMEM;
		goto clean_up;
	}

	gr->map_row_offset = INVALID_SCREEN_TILE_ROW_OFFSET;

	if (gr->tpc_count == 3)
		gr->map_row_offset = 2;
	else if (gr->tpc_count < 3)
		gr->map_row_offset = 1;
	else {
		gr->map_row_offset = 3;

		for (index = 1; index < 18; index++) {
			u32 prime = prime_set[index];
			if ((gr->tpc_count % prime) != 0) {
				gr->map_row_offset = prime;
				break;
			}
		}
	}

	switch (gr->tpc_count) {
	case 15:
		gr->map_row_offset = 6;
		break;
	case 14:
		gr->map_row_offset = 5;
		break;
	case 13:
		gr->map_row_offset = 2;
		break;
	case 11:
		gr->map_row_offset = 7;
		break;
	case 10:
		gr->map_row_offset = 6;
		break;
	case 7:
	case 5:
		gr->map_row_offset = 1;
		break;
	default:
		break;
	}

	if (gr->map_tiles) {
		if (gr->map_tile_count != gr->tpc_count)
			delete_map = true;

		for (tile_count = 0; tile_count < gr->map_tile_count; tile_count++) {
			if ((u32)gr->map_tiles[tile_count] >= gr->tpc_count)
				delete_map = true;
		}

		if (delete_map) {
			kfree(gr->map_tiles);
			gr->map_tiles = NULL;
			gr->map_tile_count = 0;
		}
	}

	if (gr->map_tiles == NULL) {
		gr->map_tile_count = num_gpcs;

		gr->map_tiles = kzalloc(num_gpcs * sizeof(u8), GFP_KERNEL);
		if (gr->map_tiles == NULL) {
			ret = -ENOMEM;
			goto clean_up;
		}

		for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++) {
			sorted_num_tpcs[gpc_index] = gr->gpc_tpc_count[gpc_index];
			sorted_to_unsorted_gpc_map[gpc_index] = gpc_index;
		}

		gpc_sorted = false;
		while (!gpc_sorted) {
			gpc_sorted = true;
			for (gpc_index = 0; gpc_index < gr->gpc_count - 1; gpc_index++) {
				if (sorted_num_tpcs[gpc_index + 1] > sorted_num_tpcs[gpc_index]) {
					gpc_sorted = false;
					swap = sorted_num_tpcs[gpc_index];
					sorted_num_tpcs[gpc_index] = sorted_num_tpcs[gpc_index + 1];
					sorted_num_tpcs[gpc_index + 1] = swap;
					swap = sorted_to_unsorted_gpc_map[gpc_index];
					sorted_to_unsorted_gpc_map[gpc_index] =
						sorted_to_unsorted_gpc_map[gpc_index + 1];
					sorted_to_unsorted_gpc_map[gpc_index + 1] = swap;
				}
			}
		}

		for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++)
			if (gr->gpc_tpc_count[gpc_index] > max_tpc_count)
				max_tpc_count = gr->gpc_tpc_count[gpc_index];

		mul_factor = gr->gpc_count * max_tpc_count;
		if (mul_factor & 0x1)
			mul_factor = 2;
		else
			mul_factor = 1;

		comm_denom = gr->gpc_count * max_tpc_count * mul_factor;

		for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++) {
			num_tpc = sorted_num_tpcs[gpc_index];

			init_frac[gpc_index] = num_tpc * gr->gpc_count * mul_factor;

			if (num_tpc != 0)
				init_err[gpc_index] = gpc_index * max_tpc_count * mul_factor - comm_denom/2;
			else
				init_err[gpc_index] = 0;

			run_err[gpc_index] = init_frac[gpc_index] + init_err[gpc_index];
		}

		while (gpc_mark < gr->tpc_count) {
			for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++) {
				if ((run_err[gpc_index] * 2) >= comm_denom) {
					gr->map_tiles[gpc_mark++] = (u8)sorted_to_unsorted_gpc_map[gpc_index];
					run_err[gpc_index] += init_frac[gpc_index] - comm_denom;
				} else
					run_err[gpc_index] += init_frac[gpc_index];
			}
		}
	}

clean_up:
	kfree(init_frac);
	kfree(init_err);
	kfree(run_err);
	kfree(sorted_num_tpcs);
	kfree(sorted_to_unsorted_gpc_map);

	if (ret)
		gk20a_err(dev_from_gk20a(g), "fail");
	else
		gk20a_dbg_fn("done");

	return ret;
}

static int gr_gk20a_init_zcull(struct gk20a *g, struct gr_gk20a *gr)
{
	struct gr_zcull_gk20a *zcull = &gr->zcull;

	zcull->aliquot_width = gr->tpc_count * 16;
	zcull->aliquot_height = 16;

	zcull->width_align_pixels = gr->tpc_count * 16;
	zcull->height_align_pixels = 32;

	zcull->aliquot_size =
		zcull->aliquot_width * zcull->aliquot_height;

	/* assume no floor sweeping since we only have 1 tpc in 1 gpc */
	zcull->pixel_squares_by_aliquots =
		gr->zcb_count * 16 * 16 * gr->tpc_count /
		(gr->gpc_count * gr->gpc_tpc_count[0]);

	zcull->total_aliquots =
		gr_gpc0_zcull_total_ram_size_num_aliquots_f(
			gk20a_readl(g, gr_gpc0_zcull_total_ram_size_r()));

	return 0;
}

u32 gr_gk20a_get_ctxsw_zcull_size(struct gk20a *g, struct gr_gk20a *gr)
{
	/* assuming gr has already been initialized */
	return gr->ctx_vars.zcull_ctxsw_image_size;
}

int gr_gk20a_bind_ctxsw_zcull(struct gk20a *g, struct gr_gk20a *gr,
			struct channel_gk20a *c, u64 zcull_va, u32 mode)
{
	struct zcull_ctx_desc *zcull_ctx = &c->ch_ctx.zcull_ctx;

	zcull_ctx->ctx_sw_mode = mode;
	zcull_ctx->gpu_va = zcull_va;

	/* TBD: don't disable channel in sw method processing */
	return gr_gk20a_ctx_zcull_setup(g, c);
}

int gr_gk20a_get_zcull_info(struct gk20a *g, struct gr_gk20a *gr,
			struct gr_zcull_info *zcull_params)
{
	struct gr_zcull_gk20a *zcull = &gr->zcull;

	zcull_params->width_align_pixels = zcull->width_align_pixels;
	zcull_params->height_align_pixels = zcull->height_align_pixels;
	zcull_params->pixel_squares_by_aliquots =
		zcull->pixel_squares_by_aliquots;
	zcull_params->aliquot_total = zcull->total_aliquots;

	zcull_params->region_byte_multiplier =
		gr->gpc_count * gr_zcull_bytes_per_aliquot_per_gpu_v();
	zcull_params->region_header_size =
		nvgpu_get_litter_value(g, GPU_LIT_NUM_GPCS) *
		gr_zcull_save_restore_header_bytes_per_gpc_v();

	zcull_params->subregion_header_size =
		nvgpu_get_litter_value(g, GPU_LIT_NUM_GPCS) *
		gr_zcull_save_restore_subregion_header_bytes_per_gpc_v();

	zcull_params->subregion_width_align_pixels =
		gr->tpc_count * gr_gpc0_zcull_zcsize_width_subregion__multiple_v();
	zcull_params->subregion_height_align_pixels =
		gr_gpc0_zcull_zcsize_height_subregion__multiple_v();
	zcull_params->subregion_count = gr_zcull_subregion_qty_v();

	return 0;
}

static void gr_gk20a_detect_sm_arch(struct gk20a *g)
{
	u32 v = gk20a_readl(g, gr_gpc0_tpc0_sm_arch_r());

	u32 raw_version = gr_gpc0_tpc0_sm_arch_spa_version_v(v);
	u32 version = 0;

	if (raw_version == gr_gpc0_tpc0_sm_arch_spa_version_smkepler_lp_v())
		version = 0x320; /* SM 3.2 */
	else
		gk20a_err(dev_from_gk20a(g), "Unknown SM version 0x%x\n",
			  raw_version);

	/* on Kepler, SM version == SPA version */
	g->gpu_characteristics.sm_arch_spa_version = version;
	g->gpu_characteristics.sm_arch_sm_version = version;

	g->gpu_characteristics.sm_arch_warp_count =
		gr_gpc0_tpc0_sm_arch_warp_count_v(v);
}

int gr_gk20a_add_zbc_color(struct gk20a *g, struct gr_gk20a *gr,
			   struct zbc_entry *color_val, u32 index)
{
	u32 i;

	/* update l2 table */
	g->ops.ltc.set_zbc_color_entry(g, color_val, index);

	/* update ds table */
	gk20a_writel(g, gr_ds_zbc_color_r_r(),
		gr_ds_zbc_color_r_val_f(color_val->color_ds[0]));
	gk20a_writel(g, gr_ds_zbc_color_g_r(),
		gr_ds_zbc_color_g_val_f(color_val->color_ds[1]));
	gk20a_writel(g, gr_ds_zbc_color_b_r(),
		gr_ds_zbc_color_b_val_f(color_val->color_ds[2]));
	gk20a_writel(g, gr_ds_zbc_color_a_r(),
		gr_ds_zbc_color_a_val_f(color_val->color_ds[3]));

	gk20a_writel(g, gr_ds_zbc_color_fmt_r(),
		gr_ds_zbc_color_fmt_val_f(color_val->format));

	gk20a_writel(g, gr_ds_zbc_tbl_index_r(),
		gr_ds_zbc_tbl_index_val_f(index + GK20A_STARTOF_ZBC_TABLE));

	/* trigger the write */
	gk20a_writel(g, gr_ds_zbc_tbl_ld_r(),
		gr_ds_zbc_tbl_ld_select_c_f() |
		gr_ds_zbc_tbl_ld_action_write_f() |
		gr_ds_zbc_tbl_ld_trigger_active_f());

	/* update local copy */
	for (i = 0; i < GK20A_ZBC_COLOR_VALUE_SIZE; i++) {
		gr->zbc_col_tbl[index].color_l2[i] = color_val->color_l2[i];
		gr->zbc_col_tbl[index].color_ds[i] = color_val->color_ds[i];
	}
	gr->zbc_col_tbl[index].format = color_val->format;
	gr->zbc_col_tbl[index].ref_cnt++;

	return 0;
}

int gr_gk20a_add_zbc_depth(struct gk20a *g, struct gr_gk20a *gr,
			   struct zbc_entry *depth_val, u32 index)
{
	/* update l2 table */
	g->ops.ltc.set_zbc_depth_entry(g, depth_val, index);

	/* update ds table */
	gk20a_writel(g, gr_ds_zbc_z_r(),
		gr_ds_zbc_z_val_f(depth_val->depth));

	gk20a_writel(g, gr_ds_zbc_z_fmt_r(),
		gr_ds_zbc_z_fmt_val_f(depth_val->format));

	gk20a_writel(g, gr_ds_zbc_tbl_index_r(),
		gr_ds_zbc_tbl_index_val_f(index + GK20A_STARTOF_ZBC_TABLE));

	/* trigger the write */
	gk20a_writel(g, gr_ds_zbc_tbl_ld_r(),
		gr_ds_zbc_tbl_ld_select_z_f() |
		gr_ds_zbc_tbl_ld_action_write_f() |
		gr_ds_zbc_tbl_ld_trigger_active_f());

	/* update local copy */
	gr->zbc_dep_tbl[index].depth = depth_val->depth;
	gr->zbc_dep_tbl[index].format = depth_val->format;
	gr->zbc_dep_tbl[index].ref_cnt++;

	return 0;
}

void gr_gk20a_pmu_save_zbc(struct gk20a *g, u32 entries)
{
	struct fifo_gk20a *f = &g->fifo;
	struct fifo_engine_info_gk20a *gr_info = NULL;
	unsigned long end_jiffies = jiffies +
		msecs_to_jiffies(gk20a_get_gr_idle_timeout(g));
	u32 ret;
	u32 engine_id;

	engine_id = gk20a_fifo_get_gr_engine_id(g);
	gr_info = (f->engine_info + engine_id);

	ret = gk20a_fifo_disable_engine_activity(g, gr_info, true);
	if (ret) {
		gk20a_err(dev_from_gk20a(g),
			"failed to disable gr engine activity");
		return;
	}

	ret = g->ops.gr.wait_empty(g, end_jiffies, GR_IDLE_CHECK_DEFAULT);
	if (ret) {
		gk20a_err(dev_from_gk20a(g),
			"failed to idle graphics");
		goto clean_up;
	}

	/* update zbc */
	gk20a_pmu_save_zbc(g, entries);

clean_up:
	ret = gk20a_fifo_enable_engine_activity(g, gr_info);
	if (ret) {
		gk20a_err(dev_from_gk20a(g),
			"failed to enable gr engine activity\n");
	}
}

int gr_gk20a_add_zbc(struct gk20a *g, struct gr_gk20a *gr,
		     struct zbc_entry *zbc_val)
{
	struct zbc_color_table *c_tbl;
	struct zbc_depth_table *d_tbl;
	u32 i, ret = -ENOMEM;
	bool added = false;
	u32 entries;

	/* no endian swap ? */

	mutex_lock(&gr->zbc_lock);
	switch (zbc_val->type) {
	case GK20A_ZBC_TYPE_COLOR:
		/* search existing tables */
		for (i = 0; i < gr->max_used_color_index; i++) {

			c_tbl = &gr->zbc_col_tbl[i];

			if (c_tbl->ref_cnt && c_tbl->format == zbc_val->format &&
			    memcmp(c_tbl->color_ds, zbc_val->color_ds,
				sizeof(zbc_val->color_ds)) == 0) {

				if (memcmp(c_tbl->color_l2, zbc_val->color_l2,
				    sizeof(zbc_val->color_l2))) {
					gk20a_err(dev_from_gk20a(g),
						"zbc l2 and ds color don't match with existing entries");
					ret = -EINVAL;
					goto err_mutex;
				}
				added = true;
				c_tbl->ref_cnt++;
				ret = 0;
				break;
			}
		}
		/* add new table */
		if (!added &&
		    gr->max_used_color_index < GK20A_ZBC_TABLE_SIZE) {

			c_tbl =
			    &gr->zbc_col_tbl[gr->max_used_color_index];
			WARN_ON(c_tbl->ref_cnt != 0);

			ret = g->ops.gr.add_zbc_color(g, gr,
				zbc_val, gr->max_used_color_index);

			if (!ret)
				gr->max_used_color_index++;
		}
		break;
	case GK20A_ZBC_TYPE_DEPTH:
		/* search existing tables */
		for (i = 0; i < gr->max_used_depth_index; i++) {

			d_tbl = &gr->zbc_dep_tbl[i];

			if (d_tbl->ref_cnt &&
			    d_tbl->depth == zbc_val->depth &&
			    d_tbl->format == zbc_val->format) {
				added = true;
				d_tbl->ref_cnt++;
				ret = 0;
				break;
			}
		}
		/* add new table */
		if (!added &&
		    gr->max_used_depth_index < GK20A_ZBC_TABLE_SIZE) {

			d_tbl =
			    &gr->zbc_dep_tbl[gr->max_used_depth_index];
			WARN_ON(d_tbl->ref_cnt != 0);

			ret = g->ops.gr.add_zbc_depth(g, gr,
				zbc_val, gr->max_used_depth_index);

			if (!ret)
				gr->max_used_depth_index++;
		}
		break;
	default:
		gk20a_err(dev_from_gk20a(g),
			"invalid zbc table type %d", zbc_val->type);
		ret = -EINVAL;
		goto err_mutex;
	}

	if (!added && ret == 0) {
		/* update zbc for elpg only when new entry is added */
		entries = max(gr->max_used_color_index,
					gr->max_used_depth_index);
		g->ops.gr.pmu_save_zbc(g, entries);
	}

err_mutex:
	mutex_unlock(&gr->zbc_lock);
	return ret;
}

/* get a zbc table entry specified by index
 * return table size when type is invalid */
int gr_gk20a_query_zbc(struct gk20a *g, struct gr_gk20a *gr,
			struct zbc_query_params *query_params)
{
	u32 index = query_params->index_size;
	u32 i;

	switch (query_params->type) {
	case GK20A_ZBC_TYPE_INVALID:
		query_params->index_size = GK20A_ZBC_TABLE_SIZE;
		break;
	case GK20A_ZBC_TYPE_COLOR:
		if (index >= GK20A_ZBC_TABLE_SIZE) {
			gk20a_err(dev_from_gk20a(g),
				"invalid zbc color table index\n");
			return -EINVAL;
		}
		for (i = 0; i < GK20A_ZBC_COLOR_VALUE_SIZE; i++) {
			query_params->color_l2[i] =
				gr->zbc_col_tbl[index].color_l2[i];
			query_params->color_ds[i] =
				gr->zbc_col_tbl[index].color_ds[i];
		}
		query_params->format = gr->zbc_col_tbl[index].format;
		query_params->ref_cnt = gr->zbc_col_tbl[index].ref_cnt;
		break;
	case GK20A_ZBC_TYPE_DEPTH:
		if (index >= GK20A_ZBC_TABLE_SIZE) {
			gk20a_err(dev_from_gk20a(g),
				"invalid zbc depth table index\n");
			return -EINVAL;
		}
		query_params->depth = gr->zbc_dep_tbl[index].depth;
		query_params->format = gr->zbc_dep_tbl[index].format;
		query_params->ref_cnt = gr->zbc_dep_tbl[index].ref_cnt;
		break;
	default:
		gk20a_err(dev_from_gk20a(g),
				"invalid zbc table type\n");
		return -EINVAL;
	}

	return 0;
}

static int gr_gk20a_load_zbc_table(struct gk20a *g, struct gr_gk20a *gr)
{
	int i, ret;

	for (i = 0; i < gr->max_used_color_index; i++) {
		struct zbc_color_table *c_tbl = &gr->zbc_col_tbl[i];
		struct zbc_entry zbc_val;

		zbc_val.type = GK20A_ZBC_TYPE_COLOR;
		memcpy(zbc_val.color_ds,
		       c_tbl->color_ds, sizeof(zbc_val.color_ds));
		memcpy(zbc_val.color_l2,
		       c_tbl->color_l2, sizeof(zbc_val.color_l2));
		zbc_val.format = c_tbl->format;

		ret = g->ops.gr.add_zbc_color(g, gr, &zbc_val, i);

		if (ret)
			return ret;
	}
	for (i = 0; i < gr->max_used_depth_index; i++) {
		struct zbc_depth_table *d_tbl = &gr->zbc_dep_tbl[i];
		struct zbc_entry zbc_val;

		zbc_val.type = GK20A_ZBC_TYPE_DEPTH;
		zbc_val.depth = d_tbl->depth;
		zbc_val.format = d_tbl->format;

		ret = g->ops.gr.add_zbc_depth(g, gr, &zbc_val, i);
		if (ret)
			return ret;
	}
	return 0;
}

int gr_gk20a_load_zbc_default_table(struct gk20a *g, struct gr_gk20a *gr)
{
	struct zbc_entry zbc_val;
	u32 i, err;

	mutex_init(&gr->zbc_lock);

	/* load default color table */
	zbc_val.type = GK20A_ZBC_TYPE_COLOR;

	/* Opaque black (i.e. solid black, fmt 0x28 = A8B8G8R8) */
	zbc_val.format = gr_ds_zbc_color_fmt_val_a8_b8_g8_r8_v();
	for (i = 0; i < GK20A_ZBC_COLOR_VALUE_SIZE; i++) {
		zbc_val.color_ds[i] = 0;
		zbc_val.color_l2[i] = 0;
	}
	zbc_val.color_l2[0] = 0xff000000;
	zbc_val.color_ds[3] = 0x3f800000;
	err = gr_gk20a_add_zbc(g, gr, &zbc_val);

	/* Transparent black = (fmt 1 = zero) */
	zbc_val.format = gr_ds_zbc_color_fmt_val_zero_v();
	for (i = 0; i < GK20A_ZBC_COLOR_VALUE_SIZE; i++) {
		zbc_val.color_ds[i] = 0;
		zbc_val.color_l2[i] = 0;
	}
	err = gr_gk20a_add_zbc(g, gr, &zbc_val);

	/* Opaque white (i.e. solid white) = (fmt 2 = uniform 1) */
	zbc_val.format = gr_ds_zbc_color_fmt_val_unorm_one_v();
	for (i = 0; i < GK20A_ZBC_COLOR_VALUE_SIZE; i++) {
		zbc_val.color_ds[i] = 0x3f800000;
		zbc_val.color_l2[i] = 0xffffffff;
	}
	err |= gr_gk20a_add_zbc(g, gr, &zbc_val);

	if (!err)
		gr->max_default_color_index = 3;
	else {
		gk20a_err(dev_from_gk20a(g),
			   "fail to load default zbc color table\n");
		return err;
	}

	/* load default depth table */
	zbc_val.type = GK20A_ZBC_TYPE_DEPTH;

	zbc_val.format = gr_ds_zbc_z_fmt_val_fp32_v();
	zbc_val.depth = 0x3f800000;
	err |= gr_gk20a_add_zbc(g, gr, &zbc_val);

	zbc_val.format = gr_ds_zbc_z_fmt_val_fp32_v();
	zbc_val.depth = 0;
	err = gr_gk20a_add_zbc(g, gr, &zbc_val);

	if (!err)
		gr->max_default_depth_index = 2;
	else {
		gk20a_err(dev_from_gk20a(g),
			   "fail to load default zbc depth table\n");
		return err;
	}

	return 0;
}

int _gk20a_gr_zbc_set_table(struct gk20a *g, struct gr_gk20a *gr,
			struct zbc_entry *zbc_val)
{
	struct fifo_gk20a *f = &g->fifo;
	struct fifo_engine_info_gk20a *gr_info = NULL;
	unsigned long end_jiffies;
	int ret;
	u32 engine_id;

	engine_id = gk20a_fifo_get_gr_engine_id(g);
	gr_info = (f->engine_info + engine_id);

	ret = gk20a_fifo_disable_engine_activity(g, gr_info, true);
	if (ret) {
		gk20a_err(dev_from_gk20a(g),
			"failed to disable gr engine activity");
		return ret;
	}

	end_jiffies = jiffies + msecs_to_jiffies(gk20a_get_gr_idle_timeout(g));
	ret = g->ops.gr.wait_empty(g, end_jiffies, GR_IDLE_CHECK_DEFAULT);
	if (ret) {
		gk20a_err(dev_from_gk20a(g),
			"failed to idle graphics");
		goto clean_up;
	}

	ret = gr_gk20a_add_zbc(g, gr, zbc_val);

clean_up:
	if (gk20a_fifo_enable_engine_activity(g, gr_info)) {
		gk20a_err(dev_from_gk20a(g),
			"failed to enable gr engine activity");
	}

	return ret;
}

int gk20a_gr_zbc_set_table(struct gk20a *g, struct gr_gk20a *gr,
			struct zbc_entry *zbc_val)
{
	gk20a_dbg_fn("");

	return gr_gk20a_elpg_protected_call(g,
		gr_gk20a_add_zbc(g, gr, zbc_val));
}

void gr_gk20a_init_blcg_mode(struct gk20a *g, u32 mode, u32 engine)
{
	u32 gate_ctrl;

	gate_ctrl = gk20a_readl(g, therm_gate_ctrl_r(engine));

	switch (mode) {
	case BLCG_RUN:
		gate_ctrl = set_field(gate_ctrl,
				therm_gate_ctrl_blk_clk_m(),
				therm_gate_ctrl_blk_clk_run_f());
		break;
	case BLCG_AUTO:
		gate_ctrl = set_field(gate_ctrl,
				therm_gate_ctrl_blk_clk_m(),
				therm_gate_ctrl_blk_clk_auto_f());
		break;
	default:
		gk20a_err(dev_from_gk20a(g),
			"invalid blcg mode %d", mode);
		return;
	}

	gk20a_writel(g, therm_gate_ctrl_r(engine), gate_ctrl);
}

void gr_gk20a_init_elcg_mode(struct gk20a *g, u32 mode, u32 engine)
{
	u32 gate_ctrl;

	gate_ctrl = gk20a_readl(g, therm_gate_ctrl_r(engine));

	switch (mode) {
	case ELCG_RUN:
		gate_ctrl = set_field(gate_ctrl,
				therm_gate_ctrl_eng_clk_m(),
				therm_gate_ctrl_eng_clk_run_f());
		gate_ctrl = set_field(gate_ctrl,
				therm_gate_ctrl_eng_pwr_m(),
				/* set elpg to auto to meet hw expectation */
				therm_gate_ctrl_eng_pwr_auto_f());
		break;
	case ELCG_STOP:
		gate_ctrl = set_field(gate_ctrl,
				therm_gate_ctrl_eng_clk_m(),
				therm_gate_ctrl_eng_clk_stop_f());
		break;
	case ELCG_AUTO:
		gate_ctrl = set_field(gate_ctrl,
				therm_gate_ctrl_eng_clk_m(),
				therm_gate_ctrl_eng_clk_auto_f());
		break;
	default:
		gk20a_err(dev_from_gk20a(g),
			"invalid elcg mode %d", mode);
	}

	gk20a_writel(g, therm_gate_ctrl_r(engine), gate_ctrl);
}

void gr_gk20a_init_cg_mode(struct gk20a *g, u32 cgmode, u32 mode_config)
{
	u32 engine_idx;
	u32 active_engine_id = 0;
	struct fifo_engine_info_gk20a *engine_info = NULL;
	struct fifo_gk20a *f = &g->fifo;

	for (engine_idx = 0; engine_idx < f->num_engines; ++engine_idx) {
		active_engine_id = f->active_engines_list[engine_idx];
		engine_info = &f->engine_info[active_engine_id];

		/* gr_engine supports both BLCG and ELCG */
		if ((cgmode == BLCG_MODE) &&
			(engine_info->engine_enum == ENGINE_GR_GK20A)) {
				gr_gk20a_init_blcg_mode(g, mode_config, active_engine_id);
				break;
		} else if (cgmode == ELCG_MODE)
			gr_gk20a_init_elcg_mode(g, mode_config, active_engine_id);
		else
			gk20a_err(dev_from_gk20a(g), "invalid cg mode %d %d", cgmode, mode_config);
	}
}

static int gr_gk20a_zcull_init_hw(struct gk20a *g, struct gr_gk20a *gr)
{
	u32 gpc_index, gpc_tpc_count, gpc_zcull_count;
	u32 *zcull_map_tiles, *zcull_bank_counters;
	u32 map_counter;
	u32 rcp_conserv;
	u32 offset;
	bool floorsweep = false;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	int num_gpcs = nvgpu_get_litter_value(g, GPU_LIT_NUM_GPCS);
	int num_tpc_per_gpc = nvgpu_get_litter_value(g, GPU_LIT_NUM_TPC_PER_GPC);

	if (!gr->map_tiles)
		return -1;

	zcull_map_tiles = kzalloc(num_gpcs *
			num_tpc_per_gpc * sizeof(u32), GFP_KERNEL);
	if (!zcull_map_tiles) {
		gk20a_err(dev_from_gk20a(g),
			"failed to allocate zcull temp buffers");
		return -ENOMEM;
	}
	zcull_bank_counters = kzalloc(num_gpcs *
			num_tpc_per_gpc * sizeof(u32), GFP_KERNEL);

	if (!zcull_bank_counters) {
		gk20a_err(dev_from_gk20a(g),
			"failed to allocate zcull temp buffers");
		kfree(zcull_map_tiles);
		return -ENOMEM;
	}

	for (map_counter = 0; map_counter < gr->tpc_count; map_counter++) {
		zcull_map_tiles[map_counter] =
			zcull_bank_counters[gr->map_tiles[map_counter]];
		zcull_bank_counters[gr->map_tiles[map_counter]]++;
	}

	gk20a_writel(g, gr_gpcs_zcull_sm_in_gpc_number_map0_r(),
		gr_gpcs_zcull_sm_in_gpc_number_map0_tile_0_f(zcull_map_tiles[0]) |
		gr_gpcs_zcull_sm_in_gpc_number_map0_tile_1_f(zcull_map_tiles[1]) |
		gr_gpcs_zcull_sm_in_gpc_number_map0_tile_2_f(zcull_map_tiles[2]) |
		gr_gpcs_zcull_sm_in_gpc_number_map0_tile_3_f(zcull_map_tiles[3]) |
		gr_gpcs_zcull_sm_in_gpc_number_map0_tile_4_f(zcull_map_tiles[4]) |
		gr_gpcs_zcull_sm_in_gpc_number_map0_tile_5_f(zcull_map_tiles[5]) |
		gr_gpcs_zcull_sm_in_gpc_number_map0_tile_6_f(zcull_map_tiles[6]) |
		gr_gpcs_zcull_sm_in_gpc_number_map0_tile_7_f(zcull_map_tiles[7]));

	gk20a_writel(g, gr_gpcs_zcull_sm_in_gpc_number_map1_r(),
		gr_gpcs_zcull_sm_in_gpc_number_map1_tile_8_f(zcull_map_tiles[8]) |
		gr_gpcs_zcull_sm_in_gpc_number_map1_tile_9_f(zcull_map_tiles[9]) |
		gr_gpcs_zcull_sm_in_gpc_number_map1_tile_10_f(zcull_map_tiles[10]) |
		gr_gpcs_zcull_sm_in_gpc_number_map1_tile_11_f(zcull_map_tiles[11]) |
		gr_gpcs_zcull_sm_in_gpc_number_map1_tile_12_f(zcull_map_tiles[12]) |
		gr_gpcs_zcull_sm_in_gpc_number_map1_tile_13_f(zcull_map_tiles[13]) |
		gr_gpcs_zcull_sm_in_gpc_number_map1_tile_14_f(zcull_map_tiles[14]) |
		gr_gpcs_zcull_sm_in_gpc_number_map1_tile_15_f(zcull_map_tiles[15]));

	gk20a_writel(g, gr_gpcs_zcull_sm_in_gpc_number_map2_r(),
		gr_gpcs_zcull_sm_in_gpc_number_map2_tile_16_f(zcull_map_tiles[16]) |
		gr_gpcs_zcull_sm_in_gpc_number_map2_tile_17_f(zcull_map_tiles[17]) |
		gr_gpcs_zcull_sm_in_gpc_number_map2_tile_18_f(zcull_map_tiles[18]) |
		gr_gpcs_zcull_sm_in_gpc_number_map2_tile_19_f(zcull_map_tiles[19]) |
		gr_gpcs_zcull_sm_in_gpc_number_map2_tile_20_f(zcull_map_tiles[20]) |
		gr_gpcs_zcull_sm_in_gpc_number_map2_tile_21_f(zcull_map_tiles[21]) |
		gr_gpcs_zcull_sm_in_gpc_number_map2_tile_22_f(zcull_map_tiles[22]) |
		gr_gpcs_zcull_sm_in_gpc_number_map2_tile_23_f(zcull_map_tiles[23]));

	gk20a_writel(g, gr_gpcs_zcull_sm_in_gpc_number_map3_r(),
		gr_gpcs_zcull_sm_in_gpc_number_map3_tile_24_f(zcull_map_tiles[24]) |
		gr_gpcs_zcull_sm_in_gpc_number_map3_tile_25_f(zcull_map_tiles[25]) |
		gr_gpcs_zcull_sm_in_gpc_number_map3_tile_26_f(zcull_map_tiles[26]) |
		gr_gpcs_zcull_sm_in_gpc_number_map3_tile_27_f(zcull_map_tiles[27]) |
		gr_gpcs_zcull_sm_in_gpc_number_map3_tile_28_f(zcull_map_tiles[28]) |
		gr_gpcs_zcull_sm_in_gpc_number_map3_tile_29_f(zcull_map_tiles[29]) |
		gr_gpcs_zcull_sm_in_gpc_number_map3_tile_30_f(zcull_map_tiles[30]) |
		gr_gpcs_zcull_sm_in_gpc_number_map3_tile_31_f(zcull_map_tiles[31]));

	kfree(zcull_map_tiles);
	kfree(zcull_bank_counters);

	for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++) {
		gpc_tpc_count = gr->gpc_tpc_count[gpc_index];
		gpc_zcull_count = gr->gpc_zcb_count[gpc_index];

		if (gpc_zcull_count != gr->max_zcull_per_gpc_count &&
		    gpc_zcull_count < gpc_tpc_count) {
			gk20a_err(dev_from_gk20a(g),
				"zcull_banks (%d) less than tpcs (%d) for gpc (%d)",
				gpc_zcull_count, gpc_tpc_count, gpc_index);
			return -EINVAL;
		}
		if (gpc_zcull_count != gr->max_zcull_per_gpc_count &&
		    gpc_zcull_count != 0)
			floorsweep = true;
	}

	/* ceil(1.0f / SM_NUM * gr_gpc0_zcull_sm_num_rcp_conservative__max_v()) */
	rcp_conserv = DIV_ROUND_UP(gr_gpc0_zcull_sm_num_rcp_conservative__max_v(),
		gr->gpc_tpc_count[0]);

	for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++) {
		offset = gpc_index * gpc_stride;

		if (floorsweep) {
			gk20a_writel(g, gr_gpc0_zcull_ram_addr_r() + offset,
				gr_gpc0_zcull_ram_addr_row_offset_f(gr->map_row_offset) |
				gr_gpc0_zcull_ram_addr_tiles_per_hypertile_row_per_gpc_f(
					gr->max_zcull_per_gpc_count));
		} else {
			gk20a_writel(g, gr_gpc0_zcull_ram_addr_r() + offset,
				gr_gpc0_zcull_ram_addr_row_offset_f(gr->map_row_offset) |
				gr_gpc0_zcull_ram_addr_tiles_per_hypertile_row_per_gpc_f(
					gr->gpc_tpc_count[gpc_index]));
		}

		gk20a_writel(g, gr_gpc0_zcull_fs_r() + offset,
			gr_gpc0_zcull_fs_num_active_banks_f(gr->gpc_zcb_count[gpc_index]) |
			gr_gpc0_zcull_fs_num_sms_f(gr->tpc_count));

		gk20a_writel(g, gr_gpc0_zcull_sm_num_rcp_r() + offset,
			gr_gpc0_zcull_sm_num_rcp_conservative_f(rcp_conserv));
	}

	gk20a_writel(g, gr_gpcs_ppcs_wwdx_sm_num_rcp_r(),
		gr_gpcs_ppcs_wwdx_sm_num_rcp_conservative_f(rcp_conserv));

	return 0;
}

static void gk20a_gr_enable_gpc_exceptions(struct gk20a *g)
{
	struct gr_gk20a *gr = &g->gr;
	u32 tpc_mask;

	gk20a_writel(g, gr_gpcs_tpcs_tpccs_tpc_exception_en_r(),
			gr_gpcs_tpcs_tpccs_tpc_exception_en_tex_enabled_f() |
			gr_gpcs_tpcs_tpccs_tpc_exception_en_sm_enabled_f());

	tpc_mask =
		gr_gpcs_gpccs_gpc_exception_en_tpc_f((1 << gr->tpc_count) - 1);

	gk20a_writel(g, gr_gpcs_gpccs_gpc_exception_en_r(), tpc_mask);
}


void gr_gk20a_enable_hww_exceptions(struct gk20a *g)
{
	/* enable exceptions */
	gk20a_writel(g, gr_fe_hww_esr_r(),
		     gr_fe_hww_esr_en_enable_f() |
		     gr_fe_hww_esr_reset_active_f());
	gk20a_writel(g, gr_memfmt_hww_esr_r(),
		     gr_memfmt_hww_esr_en_enable_f() |
		     gr_memfmt_hww_esr_reset_active_f());
}

static void gr_gk20a_set_hww_esr_report_mask(struct gk20a *g)
{
	/* setup sm warp esr report masks */
	gk20a_writel(g, gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_r(),
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_stack_error_report_f()	|
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_api_stack_error_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_ret_empty_stack_error_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_pc_wrap_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_misaligned_pc_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_pc_overflow_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_misaligned_immc_addr_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_misaligned_reg_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_illegal_instr_encoding_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_illegal_sph_instr_combo_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_illegal_instr_param_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_invalid_const_addr_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_oor_reg_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_oor_addr_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_misaligned_addr_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_invalid_addr_space_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_illegal_instr_param2_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_invalid_const_addr_ldc_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_geometry_sm_error_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_divergent_report_f());

	/* setup sm global esr report mask */
	gk20a_writel(g, gr_gpcs_tpcs_sm_hww_global_esr_report_mask_r(),
		gr_gpcs_tpcs_sm_hww_global_esr_report_mask_sm_to_sm_fault_report_f() |
		gr_gpcs_tpcs_sm_hww_global_esr_report_mask_l1_error_report_f() |
		gr_gpcs_tpcs_sm_hww_global_esr_report_mask_multiple_warp_errors_report_f() |
		gr_gpcs_tpcs_sm_hww_global_esr_report_mask_physical_stack_overflow_error_report_f() |
		gr_gpcs_tpcs_sm_hww_global_esr_report_mask_bpt_int_report_f() |
		gr_gpcs_tpcs_sm_hww_global_esr_report_mask_bpt_pause_report_f() |
		gr_gpcs_tpcs_sm_hww_global_esr_report_mask_single_step_complete_report_f());
}

static int gk20a_init_gr_setup_hw(struct gk20a *g)
{
	struct gr_gk20a *gr = &g->gr;
	struct aiv_list_gk20a *sw_ctx_load = &g->gr.ctx_vars.sw_ctx_load;
	struct av_list_gk20a *sw_method_init = &g->gr.ctx_vars.sw_method_init;
	u32 data;
	u64 addr;
	unsigned long end_jiffies = jiffies +
		msecs_to_jiffies(gk20a_get_gr_idle_timeout(g));
	u32 last_method_data = 0;
	u32 i, err;

	gk20a_dbg_fn("");

	/* init mmu debug buffer */
	addr = g->ops.mm.get_iova_addr(g, gr->mmu_wr_mem.sgt->sgl, 0);
	addr >>= fb_mmu_debug_wr_addr_alignment_v();

	gk20a_writel(g, fb_mmu_debug_wr_r(),
		     gk20a_aperture_mask(g, &gr->mmu_wr_mem,
		       fb_mmu_debug_wr_aperture_sys_mem_ncoh_f(),
		       fb_mmu_debug_wr_aperture_vid_mem_f()) |
		     fb_mmu_debug_wr_vol_false_f() |
		     fb_mmu_debug_wr_addr_f(addr));

	addr = g->ops.mm.get_iova_addr(g, gr->mmu_rd_mem.sgt->sgl, 0);
	addr >>= fb_mmu_debug_rd_addr_alignment_v();

	gk20a_writel(g, fb_mmu_debug_rd_r(),
		     gk20a_aperture_mask(g, &gr->mmu_rd_mem,
		       fb_mmu_debug_wr_aperture_sys_mem_ncoh_f(),
		       fb_mmu_debug_rd_aperture_vid_mem_f()) |
		     fb_mmu_debug_rd_vol_false_f() |
		     fb_mmu_debug_rd_addr_f(addr));

	if (g->ops.gr.init_gpc_mmu)
		g->ops.gr.init_gpc_mmu(g);

	/* load gr floorsweeping registers */
	data = gk20a_readl(g, gr_gpc0_ppc0_pes_vsc_strem_r());
	data = set_field(data, gr_gpc0_ppc0_pes_vsc_strem_master_pe_m(),
			gr_gpc0_ppc0_pes_vsc_strem_master_pe_true_f());
	gk20a_writel(g, gr_gpc0_ppc0_pes_vsc_strem_r(), data);

	gr_gk20a_zcull_init_hw(g, gr);

	/* Bug 1340570: increase the clock timeout to avoid potential
	 * operation failure at high gpcclk rate. Default values are 0x400.
	 */
	gk20a_writel(g, pri_ringstation_sys_master_config_r(0x15), 0x800);
	gk20a_writel(g, pri_ringstation_gpc_master_config_r(0xa), 0x800);
	gk20a_writel(g, pri_ringstation_fbp_master_config_r(0x8), 0x800);

	/* enable fifo access */
	gk20a_writel(g, gr_gpfifo_ctl_r(),
		     gr_gpfifo_ctl_access_enabled_f() |
		     gr_gpfifo_ctl_semaphore_access_enabled_f());

	/* TBD: reload gr ucode when needed */

	/* enable interrupts */
	gk20a_writel(g, gr_intr_r(), 0xFFFFFFFF);
	gk20a_writel(g, gr_intr_en_r(), 0xFFFFFFFF);

	/* enable fecs error interrupts */
	gk20a_writel(g, gr_fecs_host_int_enable_r(),
		     gr_fecs_host_int_enable_ctxsw_intr1_enable_f() |
		     gr_fecs_host_int_enable_fault_during_ctxsw_enable_f() |
		     gr_fecs_host_int_enable_umimp_firmware_method_enable_f() |
		     gr_fecs_host_int_enable_umimp_illegal_method_enable_f() |
		     gr_fecs_host_int_enable_watchdog_enable_f());

	g->ops.gr.enable_hww_exceptions(g);
	g->ops.gr.set_hww_esr_report_mask(g);

	/* enable TPC exceptions per GPC */
	gk20a_gr_enable_gpc_exceptions(g);

	/* TBD: ECC for L1/SM */
	/* TBD: enable per BE exceptions */

	/* reset and enable all exceptions */
	gk20a_writel(g, gr_exception_r(), 0xFFFFFFFF);
	gk20a_writel(g, gr_exception_en_r(), 0xFFFFFFFF);
	gk20a_writel(g, gr_exception1_r(), 0xFFFFFFFF);
	gk20a_writel(g, gr_exception1_en_r(), 0xFFFFFFFF);
	gk20a_writel(g, gr_exception2_r(), 0xFFFFFFFF);
	gk20a_writel(g, gr_exception2_en_r(), 0xFFFFFFFF);

	gr_gk20a_load_zbc_table(g, gr);

	if (g->ops.ltc.init_cbc)
		g->ops.ltc.init_cbc(g, gr);

	/* load ctx init */
	for (i = 0; i < sw_ctx_load->count; i++)
		gk20a_writel(g, sw_ctx_load->l[i].addr,
			     sw_ctx_load->l[i].value);

	err = gr_gk20a_wait_idle(g, end_jiffies, GR_IDLE_CHECK_DEFAULT);
	if (err)
		goto out;

	if (g->ops.gr.init_preemption_state) {
		err = g->ops.gr.init_preemption_state(g);
		if (err)
			goto out;
	}

	/* disable fe_go_idle */
	gk20a_writel(g, gr_fe_go_idle_timeout_r(),
		gr_fe_go_idle_timeout_count_disabled_f());

	/* override a few ctx state registers */
	gr_gk20a_commit_global_timeslice(g, NULL, false);

	/* floorsweep anything left */
	err = g->ops.gr.init_fs_state(g);
	if (err)
		goto out;

	err = gr_gk20a_wait_idle(g, end_jiffies, GR_IDLE_CHECK_DEFAULT);
	if (err)
		goto restore_fe_go_idle;

restore_fe_go_idle:
	/* restore fe_go_idle */
	gk20a_writel(g, gr_fe_go_idle_timeout_r(),
		     gr_fe_go_idle_timeout_count_prod_f());

	if (err || gr_gk20a_wait_idle(g, end_jiffies, GR_IDLE_CHECK_DEFAULT))
		goto out;

	/* load method init */
	if (sw_method_init->count) {
		gk20a_writel(g, gr_pri_mme_shadow_raw_data_r(),
			     sw_method_init->l[0].value);
		gk20a_writel(g, gr_pri_mme_shadow_raw_index_r(),
			     gr_pri_mme_shadow_raw_index_write_trigger_f() |
			     sw_method_init->l[0].addr);
		last_method_data = sw_method_init->l[0].value;
	}
	for (i = 1; i < sw_method_init->count; i++) {
		if (sw_method_init->l[i].value != last_method_data) {
			gk20a_writel(g, gr_pri_mme_shadow_raw_data_r(),
				sw_method_init->l[i].value);
			last_method_data = sw_method_init->l[i].value;
		}
		gk20a_writel(g, gr_pri_mme_shadow_raw_index_r(),
			gr_pri_mme_shadow_raw_index_write_trigger_f() |
			sw_method_init->l[i].addr);
	}

	err = gr_gk20a_wait_idle(g, end_jiffies, GR_IDLE_CHECK_DEFAULT);
	if (err)
		goto out;

	kfree(gr->sm_error_states);

	/* we need to allocate this after g->ops.gr.init_fs_state() since
	 * we initialize gr->no_of_sm in this function
	 */
	gr->sm_error_states = kzalloc(
			sizeof(struct nvgpu_dbg_gpu_sm_error_state_record)
			* gr->no_of_sm, GFP_KERNEL);
	if (!gr->sm_error_states) {
		err = -ENOMEM;
		goto restore_fe_go_idle;
	}

out:
	gk20a_dbg_fn("done");
	return err;
}

static void gr_gk20a_load_gating_prod(struct gk20a *g)
{
	/* slcg prod values */
	if (g->ops.clock_gating.slcg_bus_load_gating_prod)
		g->ops.clock_gating.slcg_bus_load_gating_prod(g,
				g->slcg_enabled);
	if (g->ops.clock_gating.slcg_chiplet_load_gating_prod)
		g->ops.clock_gating.slcg_chiplet_load_gating_prod(g,
				g->slcg_enabled);
	if (g->ops.clock_gating.slcg_gr_load_gating_prod)
		g->ops.clock_gating.slcg_gr_load_gating_prod(g,
				g->slcg_enabled);
	if (g->ops.clock_gating.slcg_ctxsw_firmware_load_gating_prod)
		g->ops.clock_gating.slcg_ctxsw_firmware_load_gating_prod(g,
				g->slcg_enabled);
	if (g->ops.clock_gating.slcg_perf_load_gating_prod)
		g->ops.clock_gating.slcg_perf_load_gating_prod(g, g->slcg_enabled);
	if (g->ops.clock_gating.slcg_xbar_load_gating_prod)
		g->ops.clock_gating.slcg_xbar_load_gating_prod(g,
				g->slcg_enabled);

	/* blcg prod values */
	if (g->ops.clock_gating.blcg_bus_load_gating_prod)
		g->ops.clock_gating.blcg_bus_load_gating_prod(g,
				g->blcg_enabled);
	if (g->ops.clock_gating.blcg_ce_load_gating_prod)
		g->ops.clock_gating.blcg_ce_load_gating_prod(g,
				g->blcg_enabled);
	if (g->ops.clock_gating.blcg_gr_load_gating_prod)
		g->ops.clock_gating.blcg_gr_load_gating_prod(g, g->blcg_enabled);
	if (g->ops.clock_gating.blcg_ctxsw_firmware_load_gating_prod)
		g->ops.clock_gating.blcg_ctxsw_firmware_load_gating_prod(g,
				g->blcg_enabled);
	if (g->ops.clock_gating.blcg_xbar_load_gating_prod)
		g->ops.clock_gating.blcg_xbar_load_gating_prod(g,
				g->blcg_enabled);
	if (g->ops.clock_gating.pg_gr_load_gating_prod)
		g->ops.clock_gating.pg_gr_load_gating_prod(g, true);
}

static int gk20a_init_gr_prepare(struct gk20a *g)
{
	u32 gpfifo_ctrl, pmc_en;
	u32 err = 0;
	u32 ce_reset_mask;

	ce_reset_mask = gk20a_fifo_get_all_ce_engine_reset_mask(g);

	/* disable fifo access */
	pmc_en = gk20a_readl(g, mc_enable_r());
	if (pmc_en & mc_enable_pgraph_enabled_f()) {
		gpfifo_ctrl = gk20a_readl(g, gr_gpfifo_ctl_r());
		gpfifo_ctrl &= ~gr_gpfifo_ctl_access_enabled_f();
		gk20a_writel(g, gr_gpfifo_ctl_r(), gpfifo_ctrl);
	}

	/* reset gr engine */
	gk20a_reset(g, mc_enable_pgraph_enabled_f()
			| mc_enable_blg_enabled_f()
			| mc_enable_perfmon_enabled_f()
			| ce_reset_mask);

	gr_gk20a_load_gating_prod(g);

	/* Disable elcg until it gets enabled later in the init*/
	gr_gk20a_init_cg_mode(g, ELCG_MODE, ELCG_RUN);

	/* enable fifo access */
	gk20a_writel(g, gr_gpfifo_ctl_r(),
		gr_gpfifo_ctl_access_enabled_f() |
		gr_gpfifo_ctl_semaphore_access_enabled_f());

	if (!g->gr.ctx_vars.valid) {
		err = gr_gk20a_init_ctx_vars(g, &g->gr);
		if (err)
			gk20a_err(dev_from_gk20a(g),
				"fail to load gr init ctx");
	}
	return err;
}

static int gr_gk20a_wait_mem_scrubbing(struct gk20a *g)
{
	int retries = CTXSW_MEM_SCRUBBING_TIMEOUT_MAX /
		      CTXSW_MEM_SCRUBBING_TIMEOUT_DEFAULT;
	bool fecs_scrubbing;
	bool gpccs_scrubbing;

	gk20a_dbg_fn("");

	do {
		fecs_scrubbing = gk20a_readl(g, gr_fecs_dmactl_r()) &
			(gr_fecs_dmactl_imem_scrubbing_m() |
			 gr_fecs_dmactl_dmem_scrubbing_m());

		gpccs_scrubbing = gk20a_readl(g, gr_gpccs_dmactl_r()) &
			(gr_gpccs_dmactl_imem_scrubbing_m() |
			 gr_gpccs_dmactl_imem_scrubbing_m());

		if (!fecs_scrubbing && !gpccs_scrubbing) {
			gk20a_dbg_fn("done");
			return 0;
		}

		udelay(CTXSW_MEM_SCRUBBING_TIMEOUT_DEFAULT);
	} while (--retries || !tegra_platform_is_silicon());

	gk20a_err(dev_from_gk20a(g), "Falcon mem scrubbing timeout");
	return -ETIMEDOUT;
}

static int gr_gk20a_init_ctxsw(struct gk20a *g)
{
	u32 err = 0;

	err = g->ops.gr.load_ctxsw_ucode(g);
	if (err)
		goto out;

	err = gr_gk20a_wait_ctxsw_ready(g);
	if (err)
		goto out;

out:
	if (err)
		gk20a_err(dev_from_gk20a(g), "fail");
	else
		gk20a_dbg_fn("done");

	return err;
}

static int gk20a_init_gr_reset_enable_hw(struct gk20a *g)
{
	struct av_list_gk20a *sw_non_ctx_load = &g->gr.ctx_vars.sw_non_ctx_load;
	unsigned long end_jiffies = jiffies +
		msecs_to_jiffies(gk20a_get_gr_idle_timeout(g));
	u32 i, err = 0;

	gk20a_dbg_fn("");

	/* enable interrupts */
	gk20a_writel(g, gr_intr_r(), ~0);
	gk20a_writel(g, gr_intr_en_r(), ~0);

	/* load non_ctx init */
	for (i = 0; i < sw_non_ctx_load->count; i++)
		gk20a_writel(g, sw_non_ctx_load->l[i].addr,
			sw_non_ctx_load->l[i].value);

	err = gr_gk20a_wait_mem_scrubbing(g);
	if (err)
		goto out;

	err = gr_gk20a_wait_idle(g, end_jiffies, GR_IDLE_CHECK_DEFAULT);
	if (err)
		goto out;

out:
	if (err)
		gk20a_err(dev_from_gk20a(g), "fail");
	else
		gk20a_dbg_fn("done");

	return 0;
}

static int gr_gk20a_init_access_map(struct gk20a *g)
{
	struct gr_gk20a *gr = &g->gr;
	struct mem_desc *mem = &gr->global_ctx_buffer[PRIV_ACCESS_MAP].mem;
	u32 w, nr_pages =
		DIV_ROUND_UP(gr->ctx_vars.priv_access_map_size,
			     PAGE_SIZE);
	u32 *whitelist = NULL;
	int num_entries = 0;

	if (gk20a_mem_begin(g, mem)) {
		gk20a_err(dev_from_gk20a(g),
			  "failed to map priv access map memory");
		return -ENOMEM;
	}

	gk20a_memset(g, mem, 0, 0, PAGE_SIZE * nr_pages);

	g->ops.gr.get_access_map(g, &whitelist, &num_entries);

	for (w = 0; w < num_entries; w++) {
		u32 map_bit, map_byte, map_shift, x;
		map_bit = whitelist[w] >> 2;
		map_byte = map_bit >> 3;
		map_shift = map_bit & 0x7; /* i.e. 0-7 */
		gk20a_dbg_info("access map addr:0x%x byte:0x%x bit:%d",
			       whitelist[w], map_byte, map_shift);
		x = gk20a_mem_rd32(g, mem, map_byte / sizeof(u32));
		x |= 1 << (
			   (map_byte % sizeof(u32) * BITS_PER_BYTE)
			  + map_shift);
		gk20a_mem_wr32(g, mem, map_byte / sizeof(u32), x);
	}

	gk20a_mem_end(g, mem);
	return 0;
}

static int gk20a_init_gr_setup_sw(struct gk20a *g)
{
	struct gr_gk20a *gr = &g->gr;
	int err;

	gk20a_dbg_fn("");

	if (gr->sw_ready) {
		gk20a_dbg_fn("skip init");
		return 0;
	}

	if (g->ops.gr.fuse_override)
		g->ops.gr.fuse_override(g);

	gr->g = g;

#if defined(CONFIG_GK20A_CYCLE_STATS)
	mutex_init(&g->gr.cs_lock);
#endif

	err = gr_gk20a_init_gr_config(g, gr);
	if (err)
		goto clean_up;

	err = gr_gk20a_init_mmu_sw(g, gr);
	if (err)
		goto clean_up;

	err = gr_gk20a_init_map_tiles(g, gr);
	if (err)
		goto clean_up;

	gk20a_dbg_info("total ram pages : %lu", totalram_pages);
	gr->max_comptag_mem = totalram_pages
				 >> (10 - (PAGE_SHIFT - 10));
	err = g->ops.ltc.init_comptags(g, gr);
	if (err)
		goto clean_up;

	err = gr_gk20a_init_zcull(g, gr);
	if (err)
		goto clean_up;

	err = gr_gk20a_alloc_global_ctx_buffers(g);
	if (err)
		goto clean_up;

	err = gr_gk20a_init_access_map(g);
	if (err)
		goto clean_up;

	gr_gk20a_load_zbc_default_table(g, gr);

	mutex_init(&gr->ctx_mutex);
	spin_lock_init(&gr->ch_tlb_lock);

	gr->remove_support = gk20a_remove_gr_support;
	gr->sw_ready = true;

	if (g->ops.gr.create_gr_sysfs)
		g->ops.gr.create_gr_sysfs(g->dev);

	gk20a_dbg_fn("done");
	return 0;

clean_up:
	gk20a_err(dev_from_gk20a(g), "fail");
	gk20a_remove_gr_support(gr);
	return err;
}

static int gk20a_init_gr_bind_fecs_elpg(struct gk20a *g)
{
	struct pmu_gk20a *pmu = &g->pmu;
	struct mm_gk20a *mm = &g->mm;
	struct vm_gk20a *vm = &mm->pmu.vm;
	struct device *d = dev_from_gk20a(g);
	int err = 0;

	u32 size;

	gk20a_dbg_fn("");

	size = 0;

	err = gr_gk20a_fecs_get_reglist_img_size(g, &size);
	if (err) {
		gk20a_err(dev_from_gk20a(g),
			"fail to query fecs pg buffer size");
		return err;
	}

	if (!pmu->pg_buf.cpu_va) {
		err = gk20a_gmmu_alloc_map_sys(vm, size, &pmu->pg_buf);
		if (err) {
			gk20a_err(d, "failed to allocate memory\n");
			return -ENOMEM;
		}
	}


	err = gr_gk20a_fecs_set_reglist_bind_inst(g, &mm->pmu.inst_block);
	if (err) {
		gk20a_err(dev_from_gk20a(g),
			"fail to bind pmu inst to gr");
		return err;
	}

	err = gr_gk20a_fecs_set_reglist_virtual_addr(g, pmu->pg_buf.gpu_va);
	if (err) {
		gk20a_err(dev_from_gk20a(g),
			"fail to set pg buffer pmu va");
		return err;
	}

	return err;
}

int gk20a_init_gr_support(struct gk20a *g)
{
	u32 err;

	gk20a_dbg_fn("");

	/* this is required before gr_gk20a_init_ctx_state */
	mutex_init(&g->gr.fecs_mutex);

	err = gr_gk20a_init_ctxsw(g);
	if (err)
		return err;

	/* this appears query for sw states but fecs actually init
	   ramchain, etc so this is hw init */
	err = g->ops.gr.init_ctx_state(g);
	if (err)
		return err;

	err = gk20a_init_gr_setup_sw(g);
	if (err)
		return err;

	err = gk20a_init_gr_setup_hw(g);
	if (err)
		return err;

	err = gk20a_init_gr_bind_fecs_elpg(g);
	if (err)
		return err;

	gr_gk20a_enable_elcg(g);
	/* GR is inialized, signal possible waiters */
	g->gr.initialized = true;
	wake_up(&g->gr.init_wq);

	return 0;
}

/* Wait until GR is initialized */
void gk20a_gr_wait_initialized(struct gk20a *g)
{
	wait_event(g->gr.init_wq, g->gr.initialized);
}

#define NVA297_SET_ALPHA_CIRCULAR_BUFFER_SIZE	0x02dc
#define NVA297_SET_CIRCULAR_BUFFER_SIZE		0x1280
#define NVA297_SET_SHADER_EXCEPTIONS		0x1528
#define NVA0C0_SET_SHADER_EXCEPTIONS		0x1528

#define NVA297_SET_SHADER_EXCEPTIONS_ENABLE_FALSE 0

void gk20a_gr_set_shader_exceptions(struct gk20a *g, u32 data)
{
	gk20a_dbg_fn("");

	if (data == NVA297_SET_SHADER_EXCEPTIONS_ENABLE_FALSE) {
		gk20a_writel(g,
			gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_r(), 0);
		gk20a_writel(g,
			gr_gpcs_tpcs_sm_hww_global_esr_report_mask_r(), 0);
	} else {
		/* setup sm warp esr report masks */
		gk20a_writel(g, gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_r(),
			gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_stack_error_report_f()	|
			gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_api_stack_error_report_f() |
			gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_ret_empty_stack_error_report_f() |
			gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_pc_wrap_report_f() |
			gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_misaligned_pc_report_f() |
			gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_pc_overflow_report_f() |
			gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_misaligned_immc_addr_report_f() |
			gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_misaligned_reg_report_f() |
			gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_illegal_instr_encoding_report_f() |
			gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_illegal_sph_instr_combo_report_f() |
			gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_illegal_instr_param_report_f() |
			gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_invalid_const_addr_report_f() |
			gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_oor_reg_report_f() |
			gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_oor_addr_report_f() |
			gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_misaligned_addr_report_f() |
			gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_invalid_addr_space_report_f() |
			gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_illegal_instr_param2_report_f() |
			gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_invalid_const_addr_ldc_report_f() |
			gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_geometry_sm_error_report_f() |
			gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_divergent_report_f());

		/* setup sm global esr report mask */
		gk20a_writel(g, gr_gpcs_tpcs_sm_hww_global_esr_report_mask_r(),
			gr_gpcs_tpcs_sm_hww_global_esr_report_mask_sm_to_sm_fault_report_f() |
			gr_gpcs_tpcs_sm_hww_global_esr_report_mask_l1_error_report_f() |
			gr_gpcs_tpcs_sm_hww_global_esr_report_mask_multiple_warp_errors_report_f() |
			gr_gpcs_tpcs_sm_hww_global_esr_report_mask_physical_stack_overflow_error_report_f() |
			gr_gpcs_tpcs_sm_hww_global_esr_report_mask_bpt_int_report_f() |
			gr_gpcs_tpcs_sm_hww_global_esr_report_mask_bpt_pause_report_f() |
			gr_gpcs_tpcs_sm_hww_global_esr_report_mask_single_step_complete_report_f());
	}
}

static void gk20a_gr_set_circular_buffer_size(struct gk20a *g, u32 data)
{
	struct gr_gk20a *gr = &g->gr;
	u32 gpc_index, ppc_index, stride, val, offset;
	u32 cb_size = data * 4;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 ppc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_PPC_IN_GPC_STRIDE);

	gk20a_dbg_fn("");

	if (cb_size > gr->attrib_cb_size)
		cb_size = gr->attrib_cb_size;

	gk20a_writel(g, gr_ds_tga_constraintlogic_r(),
		(gk20a_readl(g, gr_ds_tga_constraintlogic_r()) &
		 ~gr_ds_tga_constraintlogic_beta_cbsize_f(~0)) |
		 gr_ds_tga_constraintlogic_beta_cbsize_f(cb_size));

	for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++) {
		stride = gpc_stride * gpc_index;

		for (ppc_index = 0; ppc_index < gr->gpc_ppc_count[gpc_index];
			ppc_index++) {

			val = gk20a_readl(g, gr_gpc0_ppc0_cbm_cfg_r() +
				stride +
				ppc_in_gpc_stride * ppc_index);

			offset = gr_gpc0_ppc0_cbm_cfg_start_offset_v(val);

			val = set_field(val,
				gr_gpc0_ppc0_cbm_cfg_size_m(),
				gr_gpc0_ppc0_cbm_cfg_size_f(cb_size *
					gr->pes_tpc_count[ppc_index][gpc_index]));
			val = set_field(val,
				gr_gpc0_ppc0_cbm_cfg_start_offset_m(),
				(offset + 1));

			gk20a_writel(g, gr_gpc0_ppc0_cbm_cfg_r() +
				stride +
				ppc_in_gpc_stride * ppc_index, val);

			val = set_field(val,
				gr_gpc0_ppc0_cbm_cfg_start_offset_m(),
				offset);

			gk20a_writel(g, gr_gpc0_ppc0_cbm_cfg_r() +
				stride +
				ppc_in_gpc_stride * ppc_index, val);
		}
	}
}

static void gk20a_gr_set_alpha_circular_buffer_size(struct gk20a *g, u32 data)
{
	struct gr_gk20a *gr = &g->gr;
	u32 gpc_index, ppc_index, stride, val;
	u32 pd_ab_max_output;
	u32 alpha_cb_size = data * 4;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 ppc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_PPC_IN_GPC_STRIDE);

	gk20a_dbg_fn("");
	/* if (NO_ALPHA_BETA_TIMESLICE_SUPPORT_DEF)
		return; */

	if (alpha_cb_size > gr->alpha_cb_size)
		alpha_cb_size = gr->alpha_cb_size;

	gk20a_writel(g, gr_ds_tga_constraintlogic_r(),
		(gk20a_readl(g, gr_ds_tga_constraintlogic_r()) &
		 ~gr_ds_tga_constraintlogic_alpha_cbsize_f(~0)) |
		 gr_ds_tga_constraintlogic_alpha_cbsize_f(alpha_cb_size));

	pd_ab_max_output = alpha_cb_size *
		gr_gpc0_ppc0_cbm_cfg_size_granularity_v() /
		gr_pd_ab_dist_cfg1_max_output_granularity_v();

	gk20a_writel(g, gr_pd_ab_dist_cfg1_r(),
		gr_pd_ab_dist_cfg1_max_output_f(pd_ab_max_output) |
		gr_pd_ab_dist_cfg1_max_batches_init_f());

	for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++) {
		stride = gpc_stride * gpc_index;

		for (ppc_index = 0; ppc_index < gr->gpc_ppc_count[gpc_index];
			ppc_index++) {

			val = gk20a_readl(g, gr_gpc0_ppc0_cbm_cfg2_r() +
				stride + ppc_in_gpc_stride * ppc_index);

			val = set_field(val, gr_gpc0_ppc0_cbm_cfg2_size_m(),
					gr_gpc0_ppc0_cbm_cfg2_size_f(alpha_cb_size *
						gr->pes_tpc_count[ppc_index][gpc_index]));

			gk20a_writel(g, gr_gpc0_ppc0_cbm_cfg2_r() +
				stride + ppc_in_gpc_stride * ppc_index, val);
		}
	}
}

int gk20a_enable_gr_hw(struct gk20a *g)
{
	int err;

	gk20a_dbg_fn("");

	err = gk20a_init_gr_prepare(g);
	if (err)
		return err;

	err = gk20a_init_gr_reset_enable_hw(g);
	if (err)
		return err;

	gk20a_dbg_fn("done");

	return 0;
}

static void gr_gk20a_enable_elcg(struct gk20a *g)
{
	if (g->elcg_enabled) {
		gr_gk20a_init_cg_mode(g, ELCG_MODE, ELCG_AUTO);
	} else {
		gr_gk20a_init_cg_mode(g, ELCG_MODE, ELCG_RUN);
	}
}

int gk20a_gr_reset(struct gk20a *g)
{
	int err;
	u32 size;

	mutex_lock(&g->gr.fecs_mutex);

	err = gk20a_enable_gr_hw(g);
	if (err)
		goto fail_mutex;

	err = gk20a_init_gr_setup_hw(g);
	if (err)
		goto fail_mutex;

	err = gr_gk20a_init_ctxsw(g);
	if (err)
		goto fail_mutex;

	mutex_unlock(&g->gr.fecs_mutex);

	/* this appears query for sw states but fecs actually init
	   ramchain, etc so this is hw init */
	err = g->ops.gr.init_ctx_state(g);
	if (err)
		return err;

	size = 0;
	err = gr_gk20a_fecs_get_reglist_img_size(g, &size);
	if (err) {
		gk20a_err(dev_from_gk20a(g),
			"fail to query fecs pg buffer size");
		return err;
	}

	err = gr_gk20a_fecs_set_reglist_bind_inst(g, &g->mm.pmu.inst_block);
	if (err) {
		gk20a_err(dev_from_gk20a(g),
			"fail to bind pmu inst to gr");
		return err;
	}

	err = gr_gk20a_fecs_set_reglist_virtual_addr(g, g->pmu.pg_buf.gpu_va);
	if (err) {
		gk20a_err(dev_from_gk20a(g),
			"fail to set pg buffer pmu va");
		return err;
	}

	gr_gk20a_load_gating_prod(g);
	gr_gk20a_enable_elcg(g);

	return err;

fail_mutex:
	mutex_unlock(&g->gr.fecs_mutex);
	return err;
}

static int gr_gk20a_handle_sw_method(struct gk20a *g, u32 addr,
					  u32 class_num, u32 offset, u32 data)
{
	gk20a_dbg_fn("");

	trace_gr_gk20a_handle_sw_method(dev_name(g->dev));

	if (class_num == KEPLER_COMPUTE_A) {
		switch (offset << 2) {
		case NVA0C0_SET_SHADER_EXCEPTIONS:
			gk20a_gr_set_shader_exceptions(g, data);
			break;
		default:
			goto fail;
		}
	}

	if (class_num == KEPLER_C) {
		switch (offset << 2) {
		case NVA297_SET_SHADER_EXCEPTIONS:
			gk20a_gr_set_shader_exceptions(g, data);
			break;
		case NVA297_SET_CIRCULAR_BUFFER_SIZE:
			g->ops.gr.set_circular_buffer_size(g, data);
			break;
		case NVA297_SET_ALPHA_CIRCULAR_BUFFER_SIZE:
			g->ops.gr.set_alpha_circular_buffer_size(g, data);
			break;
		default:
			goto fail;
		}
	}
	return 0;

fail:
	return -EINVAL;
}

static int gk20a_gr_handle_semaphore_timeout_pending(struct gk20a *g,
		  struct gr_gk20a_isr_data *isr_data)
{
	struct fifo_gk20a *f = &g->fifo;
	struct channel_gk20a *ch = &f->channel[isr_data->chid];
	gk20a_dbg_fn("");
	gk20a_set_error_notifier(ch,
				NVGPU_CHANNEL_GR_SEMAPHORE_TIMEOUT);
	gk20a_err(dev_from_gk20a(g),
		   "gr semaphore timeout\n");
	return -EINVAL;
}

static int gk20a_gr_intr_illegal_notify_pending(struct gk20a *g,
		  struct gr_gk20a_isr_data *isr_data)
{
	struct fifo_gk20a *f = &g->fifo;
	struct channel_gk20a *ch = &f->channel[isr_data->chid];
	gk20a_dbg_fn("");
	gk20a_set_error_notifier(ch,
				NVGPU_CHANNEL_GR_ILLEGAL_NOTIFY);
	/* This is an unrecoverable error, reset is needed */
	gk20a_err(dev_from_gk20a(g),
		   "gr semaphore timeout\n");
	return -EINVAL;
}

static int gk20a_gr_handle_illegal_method(struct gk20a *g,
					  struct gr_gk20a_isr_data *isr_data)
{
	int ret = g->ops.gr.handle_sw_method(g, isr_data->addr,
			isr_data->class_num, isr_data->offset,
			isr_data->data_lo);
	if (ret)
		gk20a_err(dev_from_gk20a(g), "invalid method class 0x%08x"
			", offset 0x%08x address 0x%08x\n",
			isr_data->class_num, isr_data->offset, isr_data->addr);

	return ret;
}

static int gk20a_gr_handle_illegal_class(struct gk20a *g,
					  struct gr_gk20a_isr_data *isr_data)
{
	struct fifo_gk20a *f = &g->fifo;
	struct channel_gk20a *ch = &f->channel[isr_data->chid];
	gk20a_dbg_fn("");
	gk20a_set_error_notifier(ch,
				NVGPU_CHANNEL_GR_ERROR_SW_NOTIFY);
	gk20a_err(dev_from_gk20a(g),
		   "invalid class 0x%08x, offset 0x%08x",
		   isr_data->class_num, isr_data->offset);
	return -EINVAL;
}

int gk20a_gr_handle_fecs_error(struct gk20a *g, struct channel_gk20a *ch,
					  struct gr_gk20a_isr_data *isr_data)
{
	u32 gr_fecs_intr = gk20a_readl(g, gr_fecs_host_int_status_r());
	int ret = 0;

	gk20a_dbg_fn("");

	if (!gr_fecs_intr)
		return 0;

	gk20a_err(dev_from_gk20a(g),
		   "unhandled fecs error interrupt 0x%08x for channel %u",
		   gr_fecs_intr, isr_data->chid);

	if (gr_fecs_intr & gr_fecs_host_int_status_umimp_firmware_method_f(1)) {
		gk20a_err(dev_from_gk20a(g),
			  "firmware method error 0x%08x for offset 0x%04x",
			  gk20a_readl(g, gr_fecs_ctxsw_mailbox_r(6)),
			  isr_data->data_lo);
		ret = -1;
	}

	gk20a_writel(g, gr_fecs_host_int_clear_r(), gr_fecs_intr);
	return ret;
}

static int gk20a_gr_handle_class_error(struct gk20a *g,
				       struct gr_gk20a_isr_data *isr_data)
{
	struct fifo_gk20a *f = &g->fifo;
	struct channel_gk20a *ch = &f->channel[isr_data->chid];
	u32 gr_class_error =
		gr_class_error_code_v(gk20a_readl(g, gr_class_error_r()));
	gk20a_dbg_fn("");

	gk20a_set_error_notifier(ch,
			NVGPU_CHANNEL_GR_ERROR_SW_NOTIFY);
	gk20a_err(dev_from_gk20a(g),
		   "class error 0x%08x, offset 0x%08x, unhandled intr 0x%08x for channel %u\n",
		   isr_data->class_num, isr_data->offset,
		   gr_class_error, ch->hw_chid);
	return -EINVAL;
}

static int gk20a_gr_handle_firmware_method(struct gk20a *g,
					   struct gr_gk20a_isr_data *isr_data)
{
	struct fifo_gk20a *f = &g->fifo;
	struct channel_gk20a *ch = &f->channel[isr_data->chid];

	gk20a_dbg_fn("");

	gk20a_set_error_notifier(ch,
			NVGPU_CHANNEL_GR_ERROR_SW_NOTIFY);
	gk20a_err(dev_from_gk20a(g),
		   "firmware method 0x%08x, offset 0x%08x for channel %u\n",
		   isr_data->class_num, isr_data->offset,
		   ch->hw_chid);
	return -EINVAL;
}

static int gk20a_gr_handle_semaphore_pending(struct gk20a *g,
					     struct gr_gk20a_isr_data *isr_data)
{
	struct fifo_gk20a *f = &g->fifo;
	struct channel_gk20a *ch = &f->channel[isr_data->chid];

	wake_up_interruptible_all(&ch->semaphore_wq);

	return 0;
}

#if defined(CONFIG_GK20A_CYCLE_STATS)
static inline bool is_valid_cyclestats_bar0_offset_gk20a(struct gk20a *g,
							 u32 offset)
{
	/* support only 24-bit 4-byte aligned offsets */
	bool valid = !(offset & 0xFF000003);

	if (g->allow_all)
		return true;

	/* whitelist check */
	valid = valid &&
		is_bar0_global_offset_whitelisted_gk20a(g, offset);
	/* resource size check in case there was a problem
	 * with allocating the assumed size of bar0 */
	valid = valid &&
		offset < resource_size(g->regs);
	return valid;
}
#endif

static int gk20a_gr_handle_notify_pending(struct gk20a *g,
					  struct gr_gk20a_isr_data *isr_data)
{
	struct fifo_gk20a *f = &g->fifo;
	struct channel_gk20a *ch = &f->channel[isr_data->chid];

#if defined(CONFIG_GK20A_CYCLE_STATS)
	void *virtual_address;
	u32 buffer_size;
	u32 offset;
	bool exit;

	/* GL will never use payload 0 for cycle state */
	if ((ch->cyclestate.cyclestate_buffer == NULL) || (isr_data->data_lo == 0))
		return 0;

	mutex_lock(&ch->cyclestate.cyclestate_buffer_mutex);

	virtual_address = ch->cyclestate.cyclestate_buffer;
	buffer_size = ch->cyclestate.cyclestate_buffer_size;
	offset = isr_data->data_lo;
	exit = false;
	while (!exit) {
		struct share_buffer_head *sh_hdr;
		u32 min_element_size;

		/* validate offset */
		if (offset + sizeof(struct share_buffer_head) > buffer_size ||
		    offset + sizeof(struct share_buffer_head) < offset) {
			gk20a_err(dev_from_gk20a(g),
				  "cyclestats buffer overrun at offset 0x%x\n",
				  offset);
			break;
		}

		sh_hdr = (struct share_buffer_head *)
			((char *)virtual_address + offset);

		min_element_size =
			(sh_hdr->operation == OP_END ?
			 sizeof(struct share_buffer_head) :
			 sizeof(struct gk20a_cyclestate_buffer_elem));

		/* validate sh_hdr->size */
		if (sh_hdr->size < min_element_size ||
		    offset + sh_hdr->size > buffer_size ||
		    offset + sh_hdr->size < offset) {
			gk20a_err(dev_from_gk20a(g),
				  "bad cyclestate buffer header size at offset 0x%x\n",
				  offset);
			sh_hdr->failed = true;
			break;
		}

		switch (sh_hdr->operation) {
		case OP_END:
			exit = true;
			break;

		case BAR0_READ32:
		case BAR0_WRITE32:
		{
			struct gk20a_cyclestate_buffer_elem *op_elem =
				(struct gk20a_cyclestate_buffer_elem *)sh_hdr;
			bool valid = is_valid_cyclestats_bar0_offset_gk20a(
				g, op_elem->offset_bar0);
			u32 raw_reg;
			u64 mask_orig;
			u64 v;

			if (!valid) {
				gk20a_err(dev_from_gk20a(g),
					   "invalid cycletstats op offset: 0x%x\n",
					   op_elem->offset_bar0);

				sh_hdr->failed = exit = true;
				break;
			}


			mask_orig =
				((1ULL <<
				  (op_elem->last_bit + 1))
				 -1)&~((1ULL <<
					op_elem->first_bit)-1);

			raw_reg =
				gk20a_readl(g,
					    op_elem->offset_bar0);

			switch (sh_hdr->operation) {
			case BAR0_READ32:
				op_elem->data =
					(raw_reg & mask_orig)
					>> op_elem->first_bit;
				break;

			case BAR0_WRITE32:
				v = 0;
				if ((unsigned int)mask_orig !=
				    (unsigned int)~0) {
					v = (unsigned int)
						(raw_reg & ~mask_orig);
				}

				v |= ((op_elem->data
				       << op_elem->first_bit)
				      & mask_orig);

				gk20a_writel(g,
					     op_elem->offset_bar0,
					     (unsigned int)v);
				break;
			default:
				/* nop ok?*/
				break;
			}
		}
		break;

		default:
			/* no operation content case */
			exit = true;
			break;
		}
		sh_hdr->completed = true;
		offset += sh_hdr->size;
	}
	mutex_unlock(&ch->cyclestate.cyclestate_buffer_mutex);
#endif
	gk20a_dbg_fn("");
	wake_up(&ch->notifier_wq);
	return 0;
}

/* Used by sw interrupt thread to translate current ctx to chid.
 * Also used by regops to translate current ctx to chid and tsgid.
 * For performance, we don't want to go through 128 channels every time.
 * curr_ctx should be the value read from gr_fecs_current_ctx_r().
 * A small tlb is used here to cache translation.
 *
 * Returned channel must be freed with gk20a_channel_put() */
static struct channel_gk20a *gk20a_gr_get_channel_from_ctx(
	struct gk20a *g, u32 curr_ctx, int *curr_tsgid)
{
	struct fifo_gk20a *f = &g->fifo;
	struct gr_gk20a *gr = &g->gr;
	u32 chid = -1;
	int tsgid = NVGPU_INVALID_TSG_ID;
	u32 i;
	struct channel_gk20a *ret = NULL;

	/* when contexts are unloaded from GR, the valid bit is reset
	 * but the instance pointer information remains intact. So the
	 * valid bit must be checked to be absolutely certain that a
	 * valid context is currently resident. */
	if (!gr_fecs_current_ctx_valid_v(curr_ctx))
		return NULL;

	spin_lock(&gr->ch_tlb_lock);

	/* check cache first */
	for (i = 0; i < GR_CHANNEL_MAP_TLB_SIZE; i++) {
		if (gr->chid_tlb[i].curr_ctx == curr_ctx) {
			chid = gr->chid_tlb[i].hw_chid;
			tsgid = gr->chid_tlb[i].tsgid;
			ret = gk20a_channel_get(&f->channel[chid]);
			goto unlock;
		}
	}

	/* slow path */
	for (chid = 0; chid < f->num_channels; chid++) {
		struct channel_gk20a *ch = &f->channel[chid];
		if (!gk20a_channel_get(ch))
			continue;

		if ((u32)(gk20a_mm_inst_block_addr(g, &ch->inst_block) >>
					ram_in_base_shift_v()) ==
				gr_fecs_current_ctx_ptr_v(curr_ctx)) {
			tsgid = ch->tsgid;
			/* found it */
			ret = ch;
			break;
		}
		gk20a_channel_put(ch);
	}

	if (!ret)
		goto unlock;

	/* add to free tlb entry */
	for (i = 0; i < GR_CHANNEL_MAP_TLB_SIZE; i++) {
		if (gr->chid_tlb[i].curr_ctx == 0) {
			gr->chid_tlb[i].curr_ctx = curr_ctx;
			gr->chid_tlb[i].hw_chid = chid;
			gr->chid_tlb[i].tsgid = tsgid;
			goto unlock;
		}
	}

	/* no free entry, flush one */
	gr->chid_tlb[gr->channel_tlb_flush_index].curr_ctx = curr_ctx;
	gr->chid_tlb[gr->channel_tlb_flush_index].hw_chid = chid;
	gr->chid_tlb[gr->channel_tlb_flush_index].tsgid = tsgid;

	gr->channel_tlb_flush_index =
		(gr->channel_tlb_flush_index + 1) &
		(GR_CHANNEL_MAP_TLB_SIZE - 1);

unlock:
	spin_unlock(&gr->ch_tlb_lock);
	if (curr_tsgid)
		*curr_tsgid = tsgid;
	return ret;
}

int gk20a_gr_lock_down_sm(struct gk20a *g,
				 u32 gpc, u32 tpc, u32 global_esr_mask,
				 bool check_errors)
{
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 offset = gpc_stride * gpc + tpc_in_gpc_stride * tpc;
	u32 dbgr_control0;

	gk20a_dbg(gpu_dbg_intr | gpu_dbg_gpu_dbg,
			"GPC%d TPC%d: locking down SM", gpc, tpc);

	/* assert stop trigger */
	dbgr_control0 =
		gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_control0_r() + offset);
	dbgr_control0 |= gr_gpc0_tpc0_sm_dbgr_control0_stop_trigger_enable_f();
	gk20a_writel(g,
		gr_gpc0_tpc0_sm_dbgr_control0_r() + offset, dbgr_control0);

	return gk20a_gr_wait_for_sm_lock_down(g, gpc, tpc, global_esr_mask,
			check_errors);
}

bool gk20a_gr_sm_debugger_attached(struct gk20a *g)
{
	u32 dbgr_control0 = gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_control0_r());

	/* check if an sm debugger is attached.
	 * assumption: all SMs will have debug mode enabled/disabled
	 * uniformly. */
	if (gr_gpc0_tpc0_sm_dbgr_control0_debugger_mode_v(dbgr_control0) ==
			gr_gpc0_tpc0_sm_dbgr_control0_debugger_mode_on_v())
		return true;

	return false;
}

void gk20a_gr_clear_sm_hww(struct gk20a *g,
		u32 gpc, u32 tpc, u32 global_esr)
{
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 offset = gpc_stride * gpc + tpc_in_gpc_stride * tpc;

	gk20a_writel(g, gr_gpc0_tpc0_sm_hww_global_esr_r() + offset,
			global_esr);

	/* clear the warp hww */
	gk20a_writel(g, gr_gpc0_tpc0_sm_hww_warp_esr_r() + offset,
			gr_gpc0_tpc0_sm_hww_warp_esr_error_none_f());
}

u32 gk20a_mask_hww_warp_esr(u32 hww_warp_esr)
{
	return hww_warp_esr;
}

static int gk20a_gr_record_sm_error_state(struct gk20a *g, u32 gpc, u32 tpc)
{
	int sm_id;
	struct gr_gk20a *gr = &g->gr;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g,
					       GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 offset = gpc_stride * gpc + tpc_in_gpc_stride * tpc;

	mutex_lock(&g->dbg_sessions_lock);

	sm_id = gr_gpc0_tpc0_sm_cfg_sm_id_v(gk20a_readl(g,
			gr_gpc0_tpc0_sm_cfg_r() + offset));

	gr->sm_error_states[sm_id].hww_global_esr = gk20a_readl(g,
			gr_gpc0_tpc0_sm_hww_global_esr_r() + offset);
	gr->sm_error_states[sm_id].hww_warp_esr = gk20a_readl(g,
			gr_gpc0_tpc0_sm_hww_warp_esr_r() + offset);
	gr->sm_error_states[sm_id].hww_global_esr_report_mask = gk20a_readl(g,
		       gr_gpcs_tpcs_sm_hww_global_esr_report_mask_r() + offset);
	gr->sm_error_states[sm_id].hww_warp_esr_report_mask = gk20a_readl(g,
			gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_r() + offset);

	mutex_unlock(&g->dbg_sessions_lock);

	return 0;
}

static int gk20a_gr_update_sm_error_state(struct gk20a *g,
		struct channel_gk20a *ch, u32 sm_id,
		struct nvgpu_dbg_gpu_sm_error_state_record *sm_error_state)
{
	u32 gpc, tpc, offset;
	struct gr_gk20a *gr = &g->gr;
	struct channel_ctx_gk20a *ch_ctx = &ch->ch_ctx;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g,
					       GPU_LIT_TPC_IN_GPC_STRIDE);
	int err = 0;

	mutex_lock(&g->dbg_sessions_lock);

	gr->sm_error_states[sm_id].hww_global_esr =
			sm_error_state->hww_global_esr;
	gr->sm_error_states[sm_id].hww_warp_esr =
			sm_error_state->hww_warp_esr;
	gr->sm_error_states[sm_id].hww_global_esr_report_mask =
			sm_error_state->hww_global_esr_report_mask;
	gr->sm_error_states[sm_id].hww_warp_esr_report_mask =
			sm_error_state->hww_warp_esr_report_mask;

	err = gr_gk20a_disable_ctxsw(g);
	if (err) {
		gk20a_err(dev_from_gk20a(g), "unable to stop gr ctxsw\n");
		goto fail;
	}

	gpc = g->gr.sm_to_cluster[sm_id].gpc_index;
	tpc = g->gr.sm_to_cluster[sm_id].tpc_index;

	offset = gpc_stride * gpc + tpc_in_gpc_stride * tpc;

	if (gk20a_is_channel_ctx_resident(ch)) {
		gk20a_writel(g, gr_gpc0_tpc0_sm_hww_global_esr_r() + offset,
				gr->sm_error_states[sm_id].hww_global_esr);
		gk20a_writel(g, gr_gpc0_tpc0_sm_hww_warp_esr_r() + offset,
				gr->sm_error_states[sm_id].hww_warp_esr);
		gk20a_writel(g, gr_gpcs_tpcs_sm_hww_global_esr_report_mask_r() + offset,
				gr->sm_error_states[sm_id].hww_global_esr_report_mask);
		gk20a_writel(g, gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_r() + offset,
				gr->sm_error_states[sm_id].hww_warp_esr_report_mask);
	} else {
		err = gr_gk20a_ctx_patch_write_begin(g, ch_ctx);
		if (err)
			goto enable_ctxsw;

		gr_gk20a_ctx_patch_write(g, ch_ctx,
				gr_gpcs_tpcs_sm_hww_global_esr_report_mask_r() + offset,
				gr->sm_error_states[sm_id].hww_global_esr_report_mask,
				true);
		gr_gk20a_ctx_patch_write(g, ch_ctx,
				gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_r() + offset,
				gr->sm_error_states[sm_id].hww_warp_esr_report_mask,
				true);

		gr_gk20a_ctx_patch_write_end(g, ch_ctx);
	}

enable_ctxsw:
	err = gr_gk20a_enable_ctxsw(g);

fail:
	mutex_unlock(&g->dbg_sessions_lock);
	return err;
}

static int gk20a_gr_clear_sm_error_state(struct gk20a *g,
		struct channel_gk20a *ch, u32 sm_id)
{
	u32 gpc, tpc, offset;
	u32 val;
	struct gr_gk20a *gr = &g->gr;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g,
					       GPU_LIT_TPC_IN_GPC_STRIDE);
	int err = 0;

	mutex_lock(&g->dbg_sessions_lock);

	memset(&gr->sm_error_states[sm_id], 0, sizeof(*gr->sm_error_states));

	err = gr_gk20a_disable_ctxsw(g);
	if (err) {
		gk20a_err(dev_from_gk20a(g), "unable to stop gr ctxsw\n");
		goto fail;
	}

	if (gk20a_is_channel_ctx_resident(ch)) {
		gpc = g->gr.sm_to_cluster[sm_id].gpc_index;
		tpc = g->gr.sm_to_cluster[sm_id].tpc_index;

		offset = gpc_stride * gpc + tpc_in_gpc_stride * tpc;

		val = gk20a_readl(g, gr_gpc0_tpc0_sm_hww_global_esr_r() + offset);
		gk20a_writel(g, gr_gpc0_tpc0_sm_hww_global_esr_r() + offset,
				val);
		gk20a_writel(g, gr_gpc0_tpc0_sm_hww_warp_esr_r() + offset,
				0);
	}

	err = gr_gk20a_enable_ctxsw(g);

fail:
	mutex_unlock(&g->dbg_sessions_lock);
	return err;
}

int gr_gk20a_handle_sm_exception(struct gk20a *g, u32 gpc, u32 tpc,
		bool *post_event, struct channel_gk20a *fault_ch,
		u32 *hww_global_esr)
{
	int ret = 0;
	bool do_warp_sync = false, early_exit = false, ignore_debugger = false;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 offset = gpc_stride * gpc + tpc_in_gpc_stride * tpc;

	/* these three interrupts don't require locking down the SM. They can
	 * be handled by usermode clients as they aren't fatal. Additionally,
	 * usermode clients may wish to allow some warps to execute while others
	 * are at breakpoints, as opposed to fatal errors where all warps should
	 * halt. */
	u32 global_mask = gr_gpc0_tpc0_sm_hww_global_esr_bpt_int_pending_f()   |
			  gr_gpc0_tpc0_sm_hww_global_esr_bpt_pause_pending_f() |
			  gr_gpc0_tpc0_sm_hww_global_esr_single_step_complete_pending_f();
	u32 global_esr, warp_esr;
	bool sm_debugger_attached = gk20a_gr_sm_debugger_attached(g);

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg, "");

	global_esr = gk20a_readl(g,
				 gr_gpc0_tpc0_sm_hww_global_esr_r() + offset);
	warp_esr = gk20a_readl(g, gr_gpc0_tpc0_sm_hww_warp_esr_r() + offset);
	warp_esr = g->ops.gr.mask_hww_warp_esr(warp_esr);

	if (!sm_debugger_attached) {
		gk20a_err(dev_from_gk20a(g), "sm hww global %08x warp %08x\n",
			  global_esr, warp_esr);
		return -EFAULT;
	}

	gk20a_dbg(gpu_dbg_intr | gpu_dbg_gpu_dbg,
		  "sm hww global %08x warp %08x", global_esr, warp_esr);

	gr_gk20a_elpg_protected_call(g,
		g->ops.gr.record_sm_error_state(g, gpc, tpc));
	*hww_global_esr = global_esr;

	if (g->ops.gr.pre_process_sm_exception) {
		ret = g->ops.gr.pre_process_sm_exception(g, gpc, tpc,
				global_esr, warp_esr,
				sm_debugger_attached,
				fault_ch,
				&early_exit,
				&ignore_debugger);
		if (ret) {
			gk20a_err(dev_from_gk20a(g), "could not pre-process sm error!\n");
			return ret;
		}
	}

	if (early_exit) {
		gk20a_dbg(gpu_dbg_intr | gpu_dbg_gpu_dbg,
				"returning early");
		return ret;
	}

	/* if an sm debugger is attached, disable forwarding of tpc exceptions.
	 * the debugger will reenable exceptions after servicing them. */
	if (!ignore_debugger && sm_debugger_attached) {
		u32 tpc_exception_en = gk20a_readl(g,
				gr_gpc0_tpc0_tpccs_tpc_exception_en_r() +
				offset);
		tpc_exception_en &= ~gr_gpc0_tpc0_tpccs_tpc_exception_en_sm_enabled_f();
		gk20a_writel(g,
			     gr_gpc0_tpc0_tpccs_tpc_exception_en_r() + offset,
			     tpc_exception_en);
		gk20a_dbg(gpu_dbg_intr | gpu_dbg_gpu_dbg, "SM debugger attached");
	}

	/* if a debugger is present and an error has occurred, do a warp sync */
	if (!ignore_debugger && sm_debugger_attached &&
	    ((warp_esr != 0) || ((global_esr & ~global_mask) != 0))) {
		gk20a_dbg(gpu_dbg_intr, "warp sync needed");
		do_warp_sync = true;
	}

	if (do_warp_sync) {
		ret = gk20a_gr_lock_down_sm(g, gpc, tpc, global_mask, true);
		if (ret) {
			gk20a_err(dev_from_gk20a(g), "sm did not lock down!\n");
			return ret;
		}
	}

	if (ignore_debugger)
		gk20a_dbg(gpu_dbg_intr | gpu_dbg_gpu_dbg, "ignore_debugger set, skipping event posting");
	else
		*post_event |= true;

	return ret;
}

int gr_gk20a_handle_tex_exception(struct gk20a *g, u32 gpc, u32 tpc,
		bool *post_event)
{
	int ret = 0;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 offset = gpc_stride * gpc + tpc_in_gpc_stride * tpc;
	u32 esr;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg, "");

	esr = gk20a_readl(g,
			 gr_gpc0_tpc0_tex_m_hww_esr_r() + offset);
	gk20a_dbg(gpu_dbg_intr | gpu_dbg_gpu_dbg, "0x%08x", esr);

	gk20a_writel(g,
		     gr_gpc0_tpc0_tex_m_hww_esr_r() + offset,
		     esr);

	return ret;
}

static int gk20a_gr_handle_tpc_exception(struct gk20a *g, u32 gpc, u32 tpc,
		bool *post_event, struct channel_gk20a *fault_ch,
		u32 *hww_global_esr)
{
	int ret = 0;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 offset = gpc_stride * gpc + tpc_in_gpc_stride * tpc;
	u32 tpc_exception = gk20a_readl(g, gr_gpc0_tpc0_tpccs_tpc_exception_r()
			+ offset);

	gk20a_dbg(gpu_dbg_intr | gpu_dbg_gpu_dbg, "");

	/* check if an sm exeption is pending */
	if (gr_gpc0_tpc0_tpccs_tpc_exception_sm_v(tpc_exception) ==
			gr_gpc0_tpc0_tpccs_tpc_exception_sm_pending_v()) {
		gk20a_dbg(gpu_dbg_intr | gpu_dbg_gpu_dbg,
				"GPC%d TPC%d: SM exception pending", gpc, tpc);
		ret = g->ops.gr.handle_sm_exception(g, gpc, tpc,
							post_event, fault_ch,
							hww_global_esr);
	}

	/* check if a tex exeption is pending */
	if (gr_gpc0_tpc0_tpccs_tpc_exception_tex_v(tpc_exception) ==
			gr_gpc0_tpc0_tpccs_tpc_exception_tex_pending_v()) {
		gk20a_dbg(gpu_dbg_intr | gpu_dbg_gpu_dbg,
				"GPC%d TPC%d: TEX exception pending", gpc, tpc);
		ret = g->ops.gr.handle_tex_exception(g, gpc, tpc, post_event);
	}

	return ret;
}

static int gk20a_gr_handle_gpc_exception(struct gk20a *g, bool *post_event,
		struct channel_gk20a *fault_ch, u32 *hww_global_esr)
{
	int ret = 0;
	u32 gpc_offset, tpc_offset, gpc, tpc;
	struct gr_gk20a *gr = &g->gr;
	u32 exception1 = gk20a_readl(g, gr_exception1_r());
	u32 gpc_exception, global_esr;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);

	gk20a_dbg(gpu_dbg_intr | gpu_dbg_gpu_dbg, "");

	for (gpc = 0; gpc < gr->gpc_count; gpc++) {
		if ((exception1 & (1 << gpc)) == 0)
			continue;

		gk20a_dbg(gpu_dbg_intr | gpu_dbg_gpu_dbg,
				"GPC%d exception pending", gpc);

		gpc_offset = gpc_stride * gpc;

		gpc_exception = gk20a_readl(g, gr_gpc0_gpccs_gpc_exception_r()
				+ gpc_offset);

		/* check if any tpc has an exception */
		for (tpc = 0; tpc < gr->tpc_count; tpc++) {
			if ((gr_gpc0_gpccs_gpc_exception_tpc_v(gpc_exception) &
				(1 << tpc)) == 0)
				continue;

			gk20a_dbg(gpu_dbg_intr | gpu_dbg_gpu_dbg,
				  "GPC%d: TPC%d exception pending", gpc, tpc);

			tpc_offset = tpc_in_gpc_stride * tpc;

			global_esr = gk20a_readl(g,
					gr_gpc0_tpc0_sm_hww_global_esr_r() +
					gpc_offset + tpc_offset);

			ret = gk20a_gr_handle_tpc_exception(g, gpc, tpc,
					post_event, fault_ch, hww_global_esr);

			/* clear the hwws, also causes tpc and gpc
			 * exceptions to be cleared */
			gk20a_gr_clear_sm_hww(g, gpc, tpc, global_esr);
		}
	}

	return ret;
}

static int gk20a_gr_post_bpt_events(struct gk20a *g, struct channel_gk20a *ch,
				    u32 global_esr)
{
	if (global_esr & gr_gpc0_tpc0_sm_hww_global_esr_bpt_int_pending_f()) {
		if (gk20a_is_channel_marked_as_tsg(ch)) {
			struct tsg_gk20a *tsg = &g->fifo.tsg[ch->tsgid];

			gk20a_tsg_event_id_post_event(tsg,
				NVGPU_IOCTL_CHANNEL_EVENT_ID_BPT_INT);
		} else {
			gk20a_channel_event_id_post_event(ch,
				NVGPU_IOCTL_CHANNEL_EVENT_ID_BPT_INT);
		}
	}
	if (global_esr & gr_gpc0_tpc0_sm_hww_global_esr_bpt_pause_pending_f()) {
		if (gk20a_is_channel_marked_as_tsg(ch)) {
			struct tsg_gk20a *tsg = &g->fifo.tsg[ch->tsgid];

			gk20a_tsg_event_id_post_event(tsg,
				NVGPU_IOCTL_CHANNEL_EVENT_ID_BPT_PAUSE);
		} else {
			gk20a_channel_event_id_post_event(ch,
				NVGPU_IOCTL_CHANNEL_EVENT_ID_BPT_PAUSE);
		}
	}

	return 0;
}

int gk20a_gr_isr(struct gk20a *g)
{
	struct device *dev = dev_from_gk20a(g);
	struct gr_gk20a_isr_data isr_data;
	u32 grfifo_ctl;
	u32 obj_table;
	int need_reset = 0;
	u32 gr_intr = gk20a_readl(g, gr_intr_r());
	struct channel_gk20a *ch = NULL;
	struct channel_gk20a *fault_ch = NULL;
	int tsgid = NVGPU_INVALID_TSG_ID;
	u32 gr_engine_id;
	u32 global_esr = 0;

	gk20a_dbg_fn("");
	gk20a_dbg(gpu_dbg_intr, "pgraph intr %08x", gr_intr);

	if (!gr_intr)
		return 0;

	gr_engine_id = gk20a_fifo_get_gr_engine_id(g);

	grfifo_ctl = gk20a_readl(g, gr_gpfifo_ctl_r());
	grfifo_ctl &= ~gr_gpfifo_ctl_semaphore_access_f(1);
	grfifo_ctl &= ~gr_gpfifo_ctl_access_f(1);

	gk20a_writel(g, gr_gpfifo_ctl_r(),
		grfifo_ctl | gr_gpfifo_ctl_access_f(0) |
		gr_gpfifo_ctl_semaphore_access_f(0));

	isr_data.addr = gk20a_readl(g, gr_trapped_addr_r());
	isr_data.data_lo = gk20a_readl(g, gr_trapped_data_lo_r());
	isr_data.data_hi = gk20a_readl(g, gr_trapped_data_hi_r());
	isr_data.curr_ctx = gk20a_readl(g, gr_fecs_current_ctx_r());
	isr_data.offset = gr_trapped_addr_mthd_v(isr_data.addr);
	isr_data.sub_chan = gr_trapped_addr_subch_v(isr_data.addr);
	obj_table = (isr_data.sub_chan < 4) ? gk20a_readl(g,
		gr_fe_object_table_r(isr_data.sub_chan)) : 0;
	isr_data.class_num = gr_fe_object_table_nvclass_v(obj_table);

	ch = gk20a_gr_get_channel_from_ctx(g, isr_data.curr_ctx, &tsgid);
	if (ch)
		isr_data.chid = ch->hw_chid;
	else
		isr_data.chid = 0xffffffff;

	gk20a_dbg(gpu_dbg_intr | gpu_dbg_gpu_dbg,
		"channel %d: addr 0x%08x, "
		"data 0x%08x 0x%08x,"
		"ctx 0x%08x, offset 0x%08x, "
		"subchannel 0x%08x, class 0x%08x",
		isr_data.chid, isr_data.addr,
		isr_data.data_hi, isr_data.data_lo,
		isr_data.curr_ctx, isr_data.offset,
		isr_data.sub_chan, isr_data.class_num);

	if (gr_intr & gr_intr_notify_pending_f()) {
		gk20a_gr_handle_notify_pending(g, &isr_data);
		gk20a_writel(g, gr_intr_r(),
			gr_intr_notify_reset_f());
		gr_intr &= ~gr_intr_notify_pending_f();
	}

	if (gr_intr & gr_intr_semaphore_pending_f()) {
		gk20a_gr_handle_semaphore_pending(g, &isr_data);
		gk20a_writel(g, gr_intr_r(),
			gr_intr_semaphore_reset_f());
		gr_intr &= ~gr_intr_semaphore_pending_f();
	}

	if (gr_intr & gr_intr_semaphore_timeout_pending_f()) {
		need_reset |= gk20a_gr_handle_semaphore_timeout_pending(g,
			&isr_data);
		gk20a_writel(g, gr_intr_r(),
			gr_intr_semaphore_reset_f());
		gr_intr &= ~gr_intr_semaphore_pending_f();
	}

	if (gr_intr & gr_intr_illegal_notify_pending_f()) {
		need_reset |= gk20a_gr_intr_illegal_notify_pending(g,
			&isr_data);
		gk20a_writel(g, gr_intr_r(),
			gr_intr_illegal_notify_reset_f());
		gr_intr &= ~gr_intr_illegal_notify_pending_f();
	}

	if (gr_intr & gr_intr_illegal_method_pending_f()) {
		need_reset |= gk20a_gr_handle_illegal_method(g, &isr_data);
		gk20a_writel(g, gr_intr_r(),
			gr_intr_illegal_method_reset_f());
		gr_intr &= ~gr_intr_illegal_method_pending_f();
	}

	if (gr_intr & gr_intr_illegal_class_pending_f()) {
		need_reset |= gk20a_gr_handle_illegal_class(g, &isr_data);
		gk20a_writel(g, gr_intr_r(),
			gr_intr_illegal_class_reset_f());
		gr_intr &= ~gr_intr_illegal_class_pending_f();
	}

	if (gr_intr & gr_intr_fecs_error_pending_f()) {
		need_reset |= g->ops.gr.handle_fecs_error(g, ch, &isr_data);
		gk20a_writel(g, gr_intr_r(),
			gr_intr_fecs_error_reset_f());
		gr_intr &= ~gr_intr_fecs_error_pending_f();
	}

	if (gr_intr & gr_intr_class_error_pending_f()) {
		need_reset |= gk20a_gr_handle_class_error(g, &isr_data);
		gk20a_writel(g, gr_intr_r(),
			gr_intr_class_error_reset_f());
		gr_intr &= ~gr_intr_class_error_pending_f();
	}

	/* this one happens if someone tries to hit a non-whitelisted
	 * register using set_falcon[4] */
	if (gr_intr & gr_intr_firmware_method_pending_f()) {
		need_reset |= gk20a_gr_handle_firmware_method(g, &isr_data);
		gk20a_dbg(gpu_dbg_intr | gpu_dbg_gpu_dbg, "firmware method intr pending\n");
		gk20a_writel(g, gr_intr_r(),
			gr_intr_firmware_method_reset_f());
		gr_intr &= ~gr_intr_firmware_method_pending_f();
	}

	if (gr_intr & gr_intr_exception_pending_f()) {
		u32 exception = gk20a_readl(g, gr_exception_r());

		gk20a_dbg(gpu_dbg_intr | gpu_dbg_gpu_dbg, "exception %08x\n", exception);

		if (exception & gr_exception_fe_m()) {
			u32 fe = gk20a_readl(g, gr_fe_hww_esr_r());
			gk20a_err(dev, "fe warning %08x", fe);
			gk20a_writel(g, gr_fe_hww_esr_r(), fe);
			need_reset |= -EFAULT;
		}

		if (exception & gr_exception_memfmt_m()) {
			u32 memfmt = gk20a_readl(g, gr_memfmt_hww_esr_r());
			gk20a_err(dev, "memfmt exception %08x", memfmt);
			gk20a_writel(g, gr_memfmt_hww_esr_r(), memfmt);
			need_reset |= -EFAULT;
		}

		/* check if a gpc exception has occurred */
		if (exception & gr_exception_gpc_m() && need_reset == 0) {
			bool post_event = false;

			gk20a_dbg(gpu_dbg_intr | gpu_dbg_gpu_dbg, "GPC exception pending");


			fault_ch = gk20a_fifo_channel_from_hw_chid(g,
							isr_data.chid);

			/* check if any gpc has an exception */
			need_reset |= gk20a_gr_handle_gpc_exception(g,
					&post_event, fault_ch, &global_esr);

			/* signal clients waiting on an event */
			if (gk20a_gr_sm_debugger_attached(g) && post_event && fault_ch) {
				gk20a_dbg_gpu_post_events(fault_ch);
			}

			if (need_reset && ch)
				gk20a_set_error_notifier(ch,
					NVGPU_CHANNEL_GR_ERROR_SW_NOTIFY);
		}

		if (exception & gr_exception_ds_m()) {
			u32 ds = gk20a_readl(g, gr_ds_hww_esr_r());
			gk20a_err(dev, "ds exception %08x", ds);
			gk20a_writel(g, gr_ds_hww_esr_r(), ds);
			need_reset |= -EFAULT;
		}

		gk20a_writel(g, gr_intr_r(), gr_intr_exception_reset_f());
		gr_intr &= ~gr_intr_exception_pending_f();
	}

	if (need_reset) {
		if (tsgid != NVGPU_INVALID_TSG_ID)
			gk20a_fifo_recover(g, BIT(gr_engine_id),
					   tsgid, true, true, true);
		else if (ch)
			gk20a_fifo_recover(g, BIT(gr_engine_id),
					   ch->hw_chid, false, true, true);
		else
			gk20a_fifo_recover(g, BIT(gr_engine_id),
					   0, false, false, true);
	}

	if (gr_intr && !ch) {
		/* Clear interrupts for unused channel. This is
		   probably an interrupt during gk20a_free_channel() */
		gk20a_err(dev_from_gk20a(g),
			  "unhandled gr interrupt 0x%08x for unreferenceable channel, clearing",
			  gr_intr);
		gk20a_writel(g, gr_intr_r(), gr_intr);
		gr_intr = 0;
	}

	gk20a_writel(g, gr_gpfifo_ctl_r(),
		grfifo_ctl | gr_gpfifo_ctl_access_f(1) |
		gr_gpfifo_ctl_semaphore_access_f(1));

	if (gr_intr)
		gk20a_err(dev_from_gk20a(g),
			   "unhandled gr interrupt 0x%08x", gr_intr);

	/* Posting of BPT events should be the last thing in this function */
	if (global_esr && fault_ch)
		gk20a_gr_post_bpt_events(g, fault_ch, global_esr);

	if (ch)
		gk20a_channel_put(ch);

	return 0;
}

int gk20a_gr_nonstall_isr(struct gk20a *g)
{
	u32 gr_intr = gk20a_readl(g, gr_intr_nonstall_r());

	gk20a_dbg(gpu_dbg_intr, "pgraph nonstall intr %08x", gr_intr);

	if (gr_intr & gr_intr_nonstall_trap_pending_f()) {
		/* Clear the interrupt */
		gk20a_writel(g, gr_intr_nonstall_r(),
			gr_intr_nonstall_trap_pending_f());
		/* Wakeup all the waiting channels */
		gk20a_channel_semaphore_wakeup(g, true);
	}

	return 0;
}

int gr_gk20a_fecs_get_reglist_img_size(struct gk20a *g, u32 *size)
{
	BUG_ON(size == NULL);
	return gr_gk20a_submit_fecs_method_op(g,
		   (struct fecs_method_op_gk20a) {
			   .mailbox.id = 0,
			   .mailbox.data = 0,
			   .mailbox.clr = ~0,
			   .method.data = 1,
			   .method.addr = gr_fecs_method_push_adr_discover_reglist_image_size_v(),
			   .mailbox.ret = size,
			   .cond.ok = GR_IS_UCODE_OP_NOT_EQUAL,
			   .mailbox.ok = 0,
			   .cond.fail = GR_IS_UCODE_OP_SKIP,
			   .mailbox.fail = 0}, false);
}

int gr_gk20a_fecs_set_reglist_bind_inst(struct gk20a *g,
		struct mem_desc *inst_block)
{
	u32 data = fecs_current_ctx_data(g, inst_block);

	return gr_gk20a_submit_fecs_method_op(g,
		   (struct fecs_method_op_gk20a){
			   .mailbox.id = 4,
			   .mailbox.data = data,
			   .mailbox.clr = ~0,
			   .method.data = 1,
			   .method.addr = gr_fecs_method_push_adr_set_reglist_bind_instance_v(),
			   .mailbox.ret = NULL,
			   .cond.ok = GR_IS_UCODE_OP_EQUAL,
			   .mailbox.ok = 1,
			   .cond.fail = GR_IS_UCODE_OP_SKIP,
			   .mailbox.fail = 0}, false);
}

int gr_gk20a_fecs_set_reglist_virtual_addr(struct gk20a *g, u64 pmu_va)
{
	return gr_gk20a_submit_fecs_method_op(g,
		   (struct fecs_method_op_gk20a) {
			   .mailbox.id = 4,
			   .mailbox.data = u64_lo32(pmu_va >> 8),
			   .mailbox.clr = ~0,
			   .method.data = 1,
			   .method.addr = gr_fecs_method_push_adr_set_reglist_virtual_address_v(),
			   .mailbox.ret = NULL,
			   .cond.ok = GR_IS_UCODE_OP_EQUAL,
			   .mailbox.ok = 1,
			   .cond.fail = GR_IS_UCODE_OP_SKIP,
			   .mailbox.fail = 0}, false);
}

int gk20a_gr_suspend(struct gk20a *g)
{
	unsigned long end_jiffies = jiffies +
		msecs_to_jiffies(gk20a_get_gr_idle_timeout(g));
	u32 ret = 0;

	gk20a_dbg_fn("");

	ret = g->ops.gr.wait_empty(g, end_jiffies, GR_IDLE_CHECK_DEFAULT);
	if (ret)
		return ret;

	gk20a_writel(g, gr_gpfifo_ctl_r(),
		gr_gpfifo_ctl_access_disabled_f());

	/* disable gr intr */
	gk20a_writel(g, gr_intr_r(), 0);
	gk20a_writel(g, gr_intr_en_r(), 0);

	/* disable all exceptions */
	gk20a_writel(g, gr_exception_r(), 0);
	gk20a_writel(g, gr_exception_en_r(), 0);
	gk20a_writel(g, gr_exception1_r(), 0);
	gk20a_writel(g, gr_exception1_en_r(), 0);
	gk20a_writel(g, gr_exception2_r(), 0);
	gk20a_writel(g, gr_exception2_en_r(), 0);

	gk20a_gr_flush_channel_tlb(&g->gr);

	g->gr.initialized = false;

	gk20a_dbg_fn("done");
	return ret;
}

static int gr_gk20a_find_priv_offset_in_buffer(struct gk20a *g,
					       u32 addr,
					       bool is_quad, u32 quad,
					       u32 *context_buffer,
					       u32 context_buffer_size,
					       u32 *priv_offset);

static int gr_gk20a_find_priv_offset_in_pm_buffer(struct gk20a *g,
					          u32 addr,
					          u32 *priv_offset);

/* This function will decode a priv address and return the partition type and numbers. */
static int gr_gk20a_decode_priv_addr(struct gk20a *g, u32 addr,
			      int  *addr_type, /* enum ctxsw_addr_type */
			      u32 *gpc_num, u32 *tpc_num, u32 *ppc_num, u32 *be_num,
			      u32 *broadcast_flags)
{
	u32 gpc_addr;
	u32 ppc_address;
	u32 ppc_broadcast_addr;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg, "addr=0x%x", addr);

	/* setup defaults */
	ppc_address = 0;
	ppc_broadcast_addr = 0;
	*addr_type = CTXSW_ADDR_TYPE_SYS;
	*broadcast_flags = PRI_BROADCAST_FLAGS_NONE;
	*gpc_num = 0;
	*tpc_num = 0;
	*ppc_num = 0;
	*be_num  = 0;

	if (pri_is_gpc_addr(g, addr)) {
		*addr_type = CTXSW_ADDR_TYPE_GPC;
		gpc_addr = pri_gpccs_addr_mask(addr);
		if (pri_is_gpc_addr_shared(g, addr)) {
			*addr_type = CTXSW_ADDR_TYPE_GPC;
			*broadcast_flags |= PRI_BROADCAST_FLAGS_GPC;
		} else
			*gpc_num = pri_get_gpc_num(g, addr);

		if (pri_is_ppc_addr(g, gpc_addr)) {
			*addr_type = CTXSW_ADDR_TYPE_PPC;
			if (pri_is_ppc_addr_shared(g, gpc_addr)) {
				*broadcast_flags |= PRI_BROADCAST_FLAGS_PPC;
				return 0;
			}
		}
		if (g->ops.gr.is_tpc_addr(g, gpc_addr)) {
			*addr_type = CTXSW_ADDR_TYPE_TPC;
			if (pri_is_tpc_addr_shared(g, gpc_addr)) {
				*broadcast_flags |= PRI_BROADCAST_FLAGS_TPC;
				return 0;
			}
			*tpc_num = g->ops.gr.get_tpc_num(g, gpc_addr);
		}
		return 0;
	} else if (pri_is_be_addr(g, addr)) {
		*addr_type = CTXSW_ADDR_TYPE_BE;
		if (pri_is_be_addr_shared(g, addr)) {
			*broadcast_flags |= PRI_BROADCAST_FLAGS_BE;
			return 0;
		}
		*be_num = pri_get_be_num(g, addr);
		return 0;
	} else if (pri_is_ltc_addr(addr)) {
		*addr_type = CTXSW_ADDR_TYPE_LTCS;
		if (g->ops.gr.is_ltcs_ltss_addr(g, addr))
			*broadcast_flags |= PRI_BROADCAST_FLAGS_LTCS;
		else if (g->ops.gr.is_ltcn_ltss_addr(g, addr))
			*broadcast_flags |= PRI_BROADCAST_FLAGS_LTSS;
		return 0;
	} else if (pri_is_fbpa_addr(g, addr)) {
		*addr_type = CTXSW_ADDR_TYPE_FBPA;
		if (pri_is_fbpa_addr_shared(g, addr)) {
			*broadcast_flags |= PRI_BROADCAST_FLAGS_FBPA;
			return 0;
		}
		return 0;
	} else {
		*addr_type = CTXSW_ADDR_TYPE_SYS;
		return 0;
	}
	/* PPC!?!?!?! */

	/*NOTREACHED*/
	return -EINVAL;
}

static int gr_gk20a_split_ppc_broadcast_addr(struct gk20a *g, u32 addr,
				      u32 gpc_num,
				      u32 *priv_addr_table, u32 *t)
{
    u32 ppc_num;

    gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg, "addr=0x%x", addr);

    for (ppc_num = 0; ppc_num < g->gr.pe_count_per_gpc; ppc_num++)
	    priv_addr_table[(*t)++] = pri_ppc_addr(g, pri_ppccs_addr_mask(addr),
						   gpc_num, ppc_num);

    return 0;
}

/*
 * The context buffer is indexed using BE broadcast addresses and GPC/TPC
 * unicast addresses. This function will convert a BE unicast address to a BE
 * broadcast address and split a GPC/TPC broadcast address into a table of
 * GPC/TPC addresses.  The addresses generated by this function can be
 * successfully processed by gr_gk20a_find_priv_offset_in_buffer
 */
static int gr_gk20a_create_priv_addr_table(struct gk20a *g,
					   u32 addr,
					   u32 *priv_addr_table,
					   u32 *num_registers)
{
	int addr_type; /*enum ctxsw_addr_type */
	u32 gpc_num, tpc_num, ppc_num, be_num;
	u32 broadcast_flags;
	u32 t;
	int err;
	u32 fbpa_num;

	t = 0;
	*num_registers = 0;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg, "addr=0x%x", addr);

	err = gr_gk20a_decode_priv_addr(g, addr, &addr_type,
					&gpc_num, &tpc_num, &ppc_num, &be_num,
					&broadcast_flags);
	gk20a_dbg(gpu_dbg_gpu_dbg, "addr_type = %d", addr_type);
	if (err)
		return err;

	if ((addr_type == CTXSW_ADDR_TYPE_SYS) ||
	    (addr_type == CTXSW_ADDR_TYPE_BE)) {
		/* The BE broadcast registers are included in the compressed PRI
		 * table. Convert a BE unicast address to a broadcast address
		 * so that we can look up the offset. */
		if ((addr_type == CTXSW_ADDR_TYPE_BE) &&
		    !(broadcast_flags & PRI_BROADCAST_FLAGS_BE))
			priv_addr_table[t++] = pri_be_shared_addr(g, addr);
		else
			priv_addr_table[t++] = addr;

		*num_registers = t;
		return 0;
	}

	/* The GPC/TPC unicast registers are included in the compressed PRI
	 * tables. Convert a GPC/TPC broadcast address to unicast addresses so
	 * that we can look up the offsets. */
	if (broadcast_flags & PRI_BROADCAST_FLAGS_GPC) {
		for (gpc_num = 0; gpc_num < g->gr.gpc_count; gpc_num++) {

			if (broadcast_flags & PRI_BROADCAST_FLAGS_TPC)
				for (tpc_num = 0;
				     tpc_num < g->gr.gpc_tpc_count[gpc_num];
				     tpc_num++)
					priv_addr_table[t++] =
						pri_tpc_addr(g, pri_tpccs_addr_mask(addr),
							     gpc_num, tpc_num);

			else if (broadcast_flags & PRI_BROADCAST_FLAGS_PPC) {
				err = gr_gk20a_split_ppc_broadcast_addr(g, addr, gpc_num,
							       priv_addr_table, &t);
				if (err)
					return err;
			} else
				priv_addr_table[t++] =
					pri_gpc_addr(g, pri_gpccs_addr_mask(addr),
						     gpc_num);
		}
	}

	if (broadcast_flags & PRI_BROADCAST_FLAGS_LTSS) {
		g->ops.gr.split_lts_broadcast_addr(g, addr,
							priv_addr_table, &t);
	} else if (broadcast_flags & PRI_BROADCAST_FLAGS_LTCS) {
		g->ops.gr.split_ltc_broadcast_addr(g, addr,
							priv_addr_table, &t);
	} else if (broadcast_flags & PRI_BROADCAST_FLAGS_FBPA) {
		for (fbpa_num = 0;
		     fbpa_num < nvgpu_get_litter_value(g, GPU_LIT_NUM_FBPAS);
		     fbpa_num++)
			priv_addr_table[t++] = pri_fbpa_addr(g,
					pri_fbpa_addr_mask(g, addr), fbpa_num);
	} else if (!(broadcast_flags & PRI_BROADCAST_FLAGS_GPC)) {
		if (broadcast_flags & PRI_BROADCAST_FLAGS_TPC)
			for (tpc_num = 0;
			     tpc_num < g->gr.gpc_tpc_count[gpc_num];
			     tpc_num++)
				priv_addr_table[t++] =
					pri_tpc_addr(g, pri_tpccs_addr_mask(addr),
						     gpc_num, tpc_num);
		else if (broadcast_flags & PRI_BROADCAST_FLAGS_PPC)
			err = gr_gk20a_split_ppc_broadcast_addr(g, addr, gpc_num,
						       priv_addr_table, &t);
		else
			priv_addr_table[t++] = addr;
	}

	*num_registers = t;
	return 0;
}

int gr_gk20a_get_ctx_buffer_offsets(struct gk20a *g,
				    u32 addr,
				    u32 max_offsets,
				    u32 *offsets, u32 *offset_addrs,
				    u32 *num_offsets,
				    bool is_quad, u32 quad)
{
	u32 i;
	u32 priv_offset = 0;
	u32 *priv_registers;
	u32 num_registers = 0;
	int err = 0;
	struct gr_gk20a *gr = &g->gr;
	u32 potential_offsets = gr->max_gpc_count * gr->max_tpc_per_gpc_count;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg, "addr=0x%x", addr);

	/* implementation is crossed-up if either of these happen */
	if (max_offsets > potential_offsets)
		return -EINVAL;

	if (!g->gr.ctx_vars.golden_image_initialized)
		return -ENODEV;

	priv_registers = kzalloc(sizeof(u32) * potential_offsets, GFP_KERNEL);
	if (!priv_registers) {
		gk20a_dbg_fn("failed alloc for potential_offsets=%d", potential_offsets);
		err = PTR_ERR(priv_registers);
		goto cleanup;
	}
	memset(offsets,      0, sizeof(u32) * max_offsets);
	memset(offset_addrs, 0, sizeof(u32) * max_offsets);
	*num_offsets = 0;

	gr_gk20a_create_priv_addr_table(g, addr, &priv_registers[0], &num_registers);

	if ((max_offsets > 1) && (num_registers > max_offsets)) {
		err = -EINVAL;
		goto cleanup;
	}

	if ((max_offsets == 1) && (num_registers > 1))
		num_registers = 1;

	if (!g->gr.ctx_vars.local_golden_image) {
		gk20a_dbg_fn("no context switch header info to work with");
		err = -EINVAL;
		goto cleanup;
	}

	for (i = 0; i < num_registers; i++) {
		err = gr_gk20a_find_priv_offset_in_buffer(g,
						  priv_registers[i],
						  is_quad, quad,
						  g->gr.ctx_vars.local_golden_image,
						  g->gr.ctx_vars.golden_image_size,
						  &priv_offset);
		if (err) {
			gk20a_dbg_fn("Could not determine priv_offset for addr:0x%x",
				      addr); /*, grPriRegStr(addr)));*/
			goto cleanup;
		}

		offsets[i] = priv_offset;
		offset_addrs[i] = priv_registers[i];
	}

	*num_offsets = num_registers;
cleanup:
	if (!IS_ERR_OR_NULL(priv_registers))
		kfree(priv_registers);

	return err;
}

int gr_gk20a_get_pm_ctx_buffer_offsets(struct gk20a *g,
				       u32 addr,
				       u32 max_offsets,
				       u32 *offsets, u32 *offset_addrs,
				       u32 *num_offsets)
{
	u32 i;
	u32 priv_offset = 0;
	u32 *priv_registers;
	u32 num_registers = 0;
	int err = 0;
	struct gr_gk20a *gr = &g->gr;
	u32 potential_offsets = gr->max_gpc_count * gr->max_tpc_per_gpc_count;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg, "addr=0x%x", addr);

	/* implementation is crossed-up if either of these happen */
	if (max_offsets > potential_offsets)
		return -EINVAL;

	if (!g->gr.ctx_vars.golden_image_initialized)
		return -ENODEV;

	priv_registers = kzalloc(sizeof(u32) * potential_offsets, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(priv_registers)) {
		gk20a_dbg_fn("failed alloc for potential_offsets=%d", potential_offsets);
		return -ENOMEM;
	}
	memset(offsets,      0, sizeof(u32) * max_offsets);
	memset(offset_addrs, 0, sizeof(u32) * max_offsets);
	*num_offsets = 0;

	gr_gk20a_create_priv_addr_table(g, addr, priv_registers, &num_registers);

	if ((max_offsets > 1) && (num_registers > max_offsets)) {
		err = -EINVAL;
		goto cleanup;
	}

	if ((max_offsets == 1) && (num_registers > 1))
		num_registers = 1;

	if (!g->gr.ctx_vars.local_golden_image) {
		gk20a_dbg_fn("no context switch header info to work with");
		err = -EINVAL;
		goto cleanup;
	}

	for (i = 0; i < num_registers; i++) {
		err = gr_gk20a_find_priv_offset_in_pm_buffer(g,
						  priv_registers[i],
						  &priv_offset);
		if (err) {
			gk20a_dbg_fn("Could not determine priv_offset for addr:0x%x",
				      addr); /*, grPriRegStr(addr)));*/
			goto cleanup;
		}

		offsets[i] = priv_offset;
		offset_addrs[i] = priv_registers[i];
	}

	*num_offsets = num_registers;
cleanup:
	kfree(priv_registers);

	return err;
}

/* Setup some register tables.  This looks hacky; our
 * register/offset functions are just that, functions.
 * So they can't be used as initializers... TBD: fix to
 * generate consts at least on an as-needed basis.
 */
static const u32 _num_ovr_perf_regs = 17;
static u32 _ovr_perf_regs[17] = { 0, };
/* Following are the blocks of registers that the ucode
 stores in the extended region.*/
/* ==  ctxsw_extended_sm_dsm_perf_counter_register_stride_v() ? */
static const u32 _num_sm_dsm_perf_regs = 5;
/* ==  ctxsw_extended_sm_dsm_perf_counter_control_register_stride_v() ?*/
static const u32 _num_sm_dsm_perf_ctrl_regs = 4;
static u32 _sm_dsm_perf_regs[5];
static u32 _sm_dsm_perf_ctrl_regs[4];

static void init_ovr_perf_reg_info(void)
{
	if (_ovr_perf_regs[0] != 0)
		return;

	_ovr_perf_regs[0] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter_control_sel0_r();
	_ovr_perf_regs[1] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter_control_sel1_r();
	_ovr_perf_regs[2] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter_control0_r();
	_ovr_perf_regs[3] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter_control5_r();
	_ovr_perf_regs[4] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter_status1_r();
	_ovr_perf_regs[5] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter0_control_r();
	_ovr_perf_regs[6] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter1_control_r();
	_ovr_perf_regs[7] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter2_control_r();
	_ovr_perf_regs[8] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter3_control_r();
	_ovr_perf_regs[9] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter4_control_r();
	_ovr_perf_regs[10] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter5_control_r();
	_ovr_perf_regs[11] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter6_control_r();
	_ovr_perf_regs[12] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter7_control_r();
	_ovr_perf_regs[13] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter4_r();
	_ovr_perf_regs[14] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter5_r();
	_ovr_perf_regs[15] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter6_r();
	_ovr_perf_regs[16] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter7_r();
}

static void gr_gk20a_init_sm_dsm_reg_info(void)
{
	if (_sm_dsm_perf_regs[0] != 0)
		return;

	_sm_dsm_perf_regs[0] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter_status_r();
	_sm_dsm_perf_regs[1] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter0_r();
	_sm_dsm_perf_regs[2] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter1_r();
	_sm_dsm_perf_regs[3] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter2_r();
	_sm_dsm_perf_regs[4] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter3_r();

	_sm_dsm_perf_ctrl_regs[0] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter_control1_r();
	_sm_dsm_perf_ctrl_regs[1] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter_control2_r();
	_sm_dsm_perf_ctrl_regs[2] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter_control3_r();
	_sm_dsm_perf_ctrl_regs[3] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter_control4_r();

}

/* TBD: would like to handle this elsewhere, at a higher level.
 * these are currently constructed in a "test-then-write" style
 * which makes it impossible to know externally whether a ctx
 * write will actually occur. so later we should put a lazy,
 *  map-and-hold system in the patch write state */
static int gr_gk20a_ctx_patch_smpc(struct gk20a *g,
			    struct channel_ctx_gk20a *ch_ctx,
			    u32 addr, u32 data,
			    struct mem_desc *mem)
{
	u32 num_gpc = g->gr.gpc_count;
	u32 num_tpc;
	u32 tpc, gpc, reg;
	u32 chk_addr;
	u32 vaddr_lo;
	u32 vaddr_hi;
	u32 tmp;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);

	init_ovr_perf_reg_info();
	g->ops.gr.init_sm_dsm_reg_info();

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg, "addr=0x%x", addr);

	for (reg = 0; reg < _num_ovr_perf_regs; reg++) {
		for (gpc = 0; gpc < num_gpc; gpc++)  {
			num_tpc = g->gr.gpc_tpc_count[gpc];
			for (tpc = 0; tpc < num_tpc; tpc++) {
				chk_addr = ((gpc_stride * gpc) +
					    (tpc_in_gpc_stride * tpc) +
					    _ovr_perf_regs[reg]);
				if (chk_addr != addr)
					continue;
				/* reset the patch count from previous
				   runs,if ucode has already processed
				   it */
				tmp = gk20a_mem_rd(g, mem,
				       ctxsw_prog_main_image_patch_count_o());

				if (!tmp)
					ch_ctx->patch_ctx.data_count = 0;

				gr_gk20a_ctx_patch_write(g, ch_ctx,
							 addr, data, true);

				vaddr_lo = u64_lo32(ch_ctx->patch_ctx.mem.gpu_va);
				vaddr_hi = u64_hi32(ch_ctx->patch_ctx.mem.gpu_va);

				gk20a_mem_wr(g, mem,
					 ctxsw_prog_main_image_patch_count_o(),
					 ch_ctx->patch_ctx.data_count);
				gk20a_mem_wr(g, mem,
					 ctxsw_prog_main_image_patch_adr_lo_o(),
					 vaddr_lo);
				gk20a_mem_wr(g, mem,
					 ctxsw_prog_main_image_patch_adr_hi_o(),
					 vaddr_hi);

				/* we're not caching these on cpu side,
				   but later watch for it */
				return 0;
			}
		}
	}

	return 0;
}

static void gr_gk20a_access_smpc_reg(struct gk20a *g, u32 quad, u32 offset)
{
	u32 reg;
	u32 quad_ctrl;
	u32 half_ctrl;
	u32 tpc, gpc;
	u32 gpc_tpc_addr;
	u32 gpc_tpc_stride;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg, "offset=0x%x", offset);

	gpc = pri_get_gpc_num(g, offset);
	gpc_tpc_addr = pri_gpccs_addr_mask(offset);
	tpc = g->ops.gr.get_tpc_num(g, gpc_tpc_addr);

	quad_ctrl = quad & 0x1; /* first bit tells us quad */
	half_ctrl = (quad >> 1) & 0x1; /* second bit tells us half */

	gpc_tpc_stride = gpc * gpc_stride + tpc * tpc_in_gpc_stride;
	gpc_tpc_addr = gr_gpc0_tpc0_sm_halfctl_ctrl_r() + gpc_tpc_stride;

	reg = gk20a_readl(g, gpc_tpc_addr);
	reg = set_field(reg,
		gr_gpcs_tpcs_sm_halfctl_ctrl_sctl_read_quad_ctl_m(),
		gr_gpcs_tpcs_sm_halfctl_ctrl_sctl_read_quad_ctl_f(quad_ctrl));

	gk20a_writel(g, gpc_tpc_addr, reg);

	gpc_tpc_addr = gr_gpc0_tpc0_sm_debug_sfe_control_r() + gpc_tpc_stride;
	reg = gk20a_readl(g, gpc_tpc_addr);
	reg = set_field(reg,
		gr_gpcs_tpcs_sm_debug_sfe_control_read_half_ctl_m(),
		gr_gpcs_tpcs_sm_debug_sfe_control_read_half_ctl_f(half_ctrl));
	gk20a_writel(g, gpc_tpc_addr, reg);
}

#define ILLEGAL_ID (~0)

static inline bool check_main_image_header_magic(u8 *context)
{
	u32 magic = *(u32 *)(context + ctxsw_prog_main_image_magic_value_o());
	gk20a_dbg(gpu_dbg_gpu_dbg, "main image magic=0x%x", magic);
	return magic == ctxsw_prog_main_image_magic_value_v_value_v();
}
static inline bool check_local_header_magic(u8 *context)
{
	u32 magic = *(u32 *)(context + ctxsw_prog_local_magic_value_o());
	gk20a_dbg(gpu_dbg_gpu_dbg, "local magic=0x%x",  magic);
	return magic == ctxsw_prog_local_magic_value_v_value_v();

}

/* most likely dupe of ctxsw_gpccs_header__size_1_v() */
static inline int ctxsw_prog_ucode_header_size_in_bytes(void)
{
	return 256;
}

static void gr_gk20a_get_sm_dsm_perf_regs(struct gk20a *g,
					  u32 *num_sm_dsm_perf_regs,
					  u32 **sm_dsm_perf_regs,
					  u32 *perf_register_stride)
{
	*num_sm_dsm_perf_regs = _num_sm_dsm_perf_regs;
	*sm_dsm_perf_regs = _sm_dsm_perf_regs;
	*perf_register_stride = ctxsw_prog_extended_sm_dsm_perf_counter_register_stride_v();
}

static void gr_gk20a_get_sm_dsm_perf_ctrl_regs(struct gk20a *g,
					       u32 *num_sm_dsm_perf_ctrl_regs,
					       u32 **sm_dsm_perf_ctrl_regs,
					       u32 *ctrl_register_stride)
{
	*num_sm_dsm_perf_ctrl_regs = _num_sm_dsm_perf_ctrl_regs;
	*sm_dsm_perf_ctrl_regs = _sm_dsm_perf_ctrl_regs;
	*ctrl_register_stride = ctxsw_prog_extended_sm_dsm_perf_counter_control_register_stride_v();
}

static int gr_gk20a_find_priv_offset_in_ext_buffer(struct gk20a *g,
						   u32 addr,
						   bool is_quad, u32 quad,
						   u32 *context_buffer,
						   u32 context_buffer_size,
						   u32 *priv_offset)
{
	u32 i, data32;
	u32 gpc_num, tpc_num;
	u32 num_gpcs, num_tpcs;
	u32 chk_addr;
	u32 ext_priv_offset, ext_priv_size;
	u8 *context;
	u32 offset_to_segment, offset_to_segment_end;
	u32 sm_dsm_perf_reg_id = ILLEGAL_ID;
	u32 sm_dsm_perf_ctrl_reg_id = ILLEGAL_ID;
	u32 num_ext_gpccs_ext_buffer_segments;
	u32 inter_seg_offset;
	u32 max_tpc_count;
	u32 *sm_dsm_perf_ctrl_regs = NULL;
	u32 num_sm_dsm_perf_ctrl_regs = 0;
	u32 *sm_dsm_perf_regs = NULL;
	u32 num_sm_dsm_perf_regs = 0;
	u32 buffer_segments_size = 0;
	u32 marker_size = 0;
	u32 control_register_stride = 0;
	u32 perf_register_stride = 0;
	struct gr_gk20a *gr = &g->gr;
	u32 gpc_base = nvgpu_get_litter_value(g, GPU_LIT_GPC_BASE);
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_base = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_BASE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 tpc_gpc_mask = (tpc_in_gpc_stride - 1);

	/* Only have TPC registers in extended region, so if not a TPC reg,
	   then return error so caller can look elsewhere. */
	if (pri_is_gpc_addr(g, addr))   {
		u32 gpc_addr = 0;
		gpc_num = pri_get_gpc_num(g, addr);
		gpc_addr = pri_gpccs_addr_mask(addr);
		if (g->ops.gr.is_tpc_addr(g, gpc_addr))
			tpc_num = g->ops.gr.get_tpc_num(g, gpc_addr);
		else
			return -EINVAL;

		gk20a_dbg_info(" gpc = %d tpc = %d",
				gpc_num, tpc_num);
	} else
		return -EINVAL;

	buffer_segments_size = ctxsw_prog_extended_buffer_segments_size_in_bytes_v();
	/* note below is in words/num_registers */
	marker_size = ctxsw_prog_extended_marker_size_in_bytes_v() >> 2;

	context = (u8 *)context_buffer;
	/* sanity check main header */
	if (!check_main_image_header_magic(context)) {
		gk20a_err(dev_from_gk20a(g),
			   "Invalid main header: magic value");
		return -EINVAL;
	}
	num_gpcs = *(u32 *)(context + ctxsw_prog_main_image_num_gpcs_o());
	if (gpc_num >= num_gpcs) {
		gk20a_err(dev_from_gk20a(g),
		   "GPC 0x%08x is greater than total count 0x%08x!\n",
			   gpc_num, num_gpcs);
		return -EINVAL;
	}

	data32 = *(u32 *)(context + ctxsw_prog_main_extended_buffer_ctl_o());
	ext_priv_size   = ctxsw_prog_main_extended_buffer_ctl_size_v(data32);
	if (0 == ext_priv_size) {
		gk20a_dbg_info(" No extended memory in context buffer");
		return -EINVAL;
	}
	ext_priv_offset = ctxsw_prog_main_extended_buffer_ctl_offset_v(data32);

	offset_to_segment = ext_priv_offset * ctxsw_prog_ucode_header_size_in_bytes();
	offset_to_segment_end = offset_to_segment +
		(ext_priv_size * buffer_segments_size);

	/* check local header magic */
	context += ctxsw_prog_ucode_header_size_in_bytes();
	if (!check_local_header_magic(context)) {
		gk20a_err(dev_from_gk20a(g),
			   "Invalid local header: magic value\n");
		return -EINVAL;
	}

	/*
	 * See if the incoming register address is in the first table of
	 * registers. We check this by decoding only the TPC addr portion.
	 * If we get a hit on the TPC bit, we then double check the address
	 * by computing it from the base gpc/tpc strides.  Then make sure
	 * it is a real match.
	 */
	g->ops.gr.get_sm_dsm_perf_regs(g, &num_sm_dsm_perf_regs,
				       &sm_dsm_perf_regs,
				       &perf_register_stride);

	g->ops.gr.init_sm_dsm_reg_info();

	for (i = 0; i < num_sm_dsm_perf_regs; i++) {
		if ((addr & tpc_gpc_mask) == (sm_dsm_perf_regs[i] & tpc_gpc_mask)) {
			sm_dsm_perf_reg_id = i;

			gk20a_dbg_info("register match: 0x%08x",
					sm_dsm_perf_regs[i]);

			chk_addr = (gpc_base + gpc_stride * gpc_num) +
				   tpc_in_gpc_base +
				   (tpc_in_gpc_stride * tpc_num) +
				   (sm_dsm_perf_regs[sm_dsm_perf_reg_id] & tpc_gpc_mask);

			if (chk_addr != addr) {
				gk20a_err(dev_from_gk20a(g),
				   "Oops addr miss-match! : 0x%08x != 0x%08x\n",
					   addr, chk_addr);
				return -EINVAL;
			}
			break;
		}
	}

	/* Didn't find reg in supported group 1.
	 *  so try the second group now */
	g->ops.gr.get_sm_dsm_perf_ctrl_regs(g, &num_sm_dsm_perf_ctrl_regs,
				       &sm_dsm_perf_ctrl_regs,
				       &control_register_stride);

	if (ILLEGAL_ID == sm_dsm_perf_reg_id) {
		for (i = 0; i < num_sm_dsm_perf_ctrl_regs; i++) {
			if ((addr & tpc_gpc_mask) ==
			    (sm_dsm_perf_ctrl_regs[i] & tpc_gpc_mask)) {
				sm_dsm_perf_ctrl_reg_id = i;

				gk20a_dbg_info("register match: 0x%08x",
						sm_dsm_perf_ctrl_regs[i]);

				chk_addr = (gpc_base + gpc_stride * gpc_num) +
					   tpc_in_gpc_base +
					   tpc_in_gpc_stride * tpc_num +
					   (sm_dsm_perf_ctrl_regs[sm_dsm_perf_ctrl_reg_id] &
					    tpc_gpc_mask);

				if (chk_addr != addr) {
					gk20a_err(dev_from_gk20a(g),
						   "Oops addr miss-match! : 0x%08x != 0x%08x\n",
						   addr, chk_addr);
					return -EINVAL;

				}

				break;
			}
		}
	}

	if ((ILLEGAL_ID == sm_dsm_perf_ctrl_reg_id) &&
	    (ILLEGAL_ID == sm_dsm_perf_reg_id))
		return -EINVAL;

	/* Skip the FECS extended header, nothing there for us now. */
	offset_to_segment += buffer_segments_size;

	/* skip through the GPCCS extended headers until we get to the data for
	 * our GPC.  The size of each gpc extended segment is enough to hold the
	 * max tpc count for the gpcs,in 256b chunks.
	 */

	max_tpc_count = gr->max_tpc_per_gpc_count;

	num_ext_gpccs_ext_buffer_segments = (u32)((max_tpc_count + 1) / 2);

	offset_to_segment += (num_ext_gpccs_ext_buffer_segments *
			      buffer_segments_size * gpc_num);

	num_tpcs = g->gr.gpc_tpc_count[gpc_num];

	/* skip the head marker to start with */
	inter_seg_offset = marker_size;

	if (ILLEGAL_ID != sm_dsm_perf_ctrl_reg_id) {
		/* skip over control regs of TPC's before the one we want.
		 *  then skip to the register in this tpc */
		inter_seg_offset = inter_seg_offset +
			(tpc_num * control_register_stride) +
			sm_dsm_perf_ctrl_reg_id;
	} else {
		/* skip all the control registers */
		inter_seg_offset = inter_seg_offset +
			(num_tpcs * control_register_stride);

		/* skip the marker between control and counter segments */
		inter_seg_offset += marker_size;

		/* skip over counter regs of TPCs before the one we want */
		inter_seg_offset = inter_seg_offset +
			(tpc_num * perf_register_stride) *
			ctxsw_prog_extended_num_smpc_quadrants_v();

		/* skip over the register for the quadrants we do not want.
		 *  then skip to the register in this tpc */
		inter_seg_offset = inter_seg_offset +
			(perf_register_stride * quad) +
			sm_dsm_perf_reg_id;
	}

	/* set the offset to the segment offset plus the inter segment offset to
	 *  our register */
	offset_to_segment += (inter_seg_offset * 4);

	/* last sanity check: did we somehow compute an offset outside the
	 * extended buffer? */
	if (offset_to_segment > offset_to_segment_end) {
		gk20a_err(dev_from_gk20a(g),
			   "Overflow ctxsw buffer! 0x%08x > 0x%08x\n",
			   offset_to_segment, offset_to_segment_end);
		return -EINVAL;
	}

	*priv_offset = offset_to_segment;

	return 0;
}


static int
gr_gk20a_process_context_buffer_priv_segment(struct gk20a *g,
					     int addr_type,/* enum ctxsw_addr_type */
					     u32 pri_addr,
					     u32 gpc_num, u32 num_tpcs,
					     u32 num_ppcs, u32 ppc_mask,
					     u32 *priv_offset)
{
	u32 i;
	u32 address, base_address;
	u32 sys_offset, gpc_offset, tpc_offset, ppc_offset;
	u32 ppc_num, tpc_num, tpc_addr, gpc_addr, ppc_addr;
	struct aiv_gk20a *reg;
	u32 gpc_base = nvgpu_get_litter_value(g, GPU_LIT_GPC_BASE);
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 ppc_in_gpc_base = nvgpu_get_litter_value(g, GPU_LIT_PPC_IN_GPC_BASE);
	u32 ppc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_PPC_IN_GPC_STRIDE);
	u32 tpc_in_gpc_base = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_BASE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg, "pri_addr=0x%x", pri_addr);

	if (!g->gr.ctx_vars.valid)
		return -EINVAL;

	/* Process the SYS/BE segment. */
	if ((addr_type == CTXSW_ADDR_TYPE_SYS) ||
	    (addr_type == CTXSW_ADDR_TYPE_BE)) {
		for (i = 0; i < g->gr.ctx_vars.ctxsw_regs.sys.count; i++) {
			reg = &g->gr.ctx_vars.ctxsw_regs.sys.l[i];
			address    = reg->addr;
			sys_offset = reg->index;

			if (pri_addr == address) {
				*priv_offset = sys_offset;
				return 0;
			}
		}
	}

	/* Process the TPC segment. */
	if (addr_type == CTXSW_ADDR_TYPE_TPC) {
		for (tpc_num = 0; tpc_num < num_tpcs; tpc_num++) {
			for (i = 0; i < g->gr.ctx_vars.ctxsw_regs.tpc.count; i++) {
				reg = &g->gr.ctx_vars.ctxsw_regs.tpc.l[i];
				address = reg->addr;
				tpc_addr = pri_tpccs_addr_mask(address);
				base_address = gpc_base +
					(gpc_num * gpc_stride) +
					tpc_in_gpc_base +
					(tpc_num * tpc_in_gpc_stride);
				address = base_address + tpc_addr;
				/*
				 * The data for the TPCs is interleaved in the context buffer.
				 * Example with num_tpcs = 2
				 * 0    1    2    3    4    5    6    7    8    9    10   11 ...
				 * 0-0  1-0  0-1  1-1  0-2  1-2  0-3  1-3  0-4  1-4  0-5  1-5 ...
				 */
				tpc_offset = (reg->index * num_tpcs) + (tpc_num * 4);

				if (pri_addr == address) {
					*priv_offset = tpc_offset;
					return 0;
				}
			}
		}
	}

	/* Process the PPC segment. */
	if (addr_type == CTXSW_ADDR_TYPE_PPC) {
		for (ppc_num = 0; ppc_num < num_ppcs; ppc_num++) {
			for (i = 0; i < g->gr.ctx_vars.ctxsw_regs.ppc.count; i++) {
				reg = &g->gr.ctx_vars.ctxsw_regs.ppc.l[i];
				address = reg->addr;
				ppc_addr = pri_ppccs_addr_mask(address);
				base_address = gpc_base +
					(gpc_num * gpc_stride) +
					ppc_in_gpc_base +
					(ppc_num * ppc_in_gpc_stride);
				address = base_address + ppc_addr;
				/*
				 * The data for the PPCs is interleaved in the context buffer.
				 * Example with numPpcs = 2
				 * 0    1    2    3    4    5    6    7    8    9    10   11 ...
				 * 0-0  1-0  0-1  1-1  0-2  1-2  0-3  1-3  0-4  1-4  0-5  1-5 ...
				 */
				ppc_offset = (reg->index * num_ppcs) + (ppc_num * 4);

				if (pri_addr == address)  {
					*priv_offset = ppc_offset;
					return 0;
				}
			}
		}
	}


	/* Process the GPC segment. */
	if (addr_type == CTXSW_ADDR_TYPE_GPC) {
		for (i = 0; i < g->gr.ctx_vars.ctxsw_regs.gpc.count; i++) {
			reg = &g->gr.ctx_vars.ctxsw_regs.gpc.l[i];

			address = reg->addr;
			gpc_addr = pri_gpccs_addr_mask(address);
			gpc_offset = reg->index;

			base_address = gpc_base + (gpc_num * gpc_stride);
			address = base_address + gpc_addr;

			if (pri_addr == address) {
				*priv_offset = gpc_offset;
				return 0;
			}
		}
	}

	return -EINVAL;
}

static int gr_gk20a_determine_ppc_configuration(struct gk20a *g,
					       u8 *context,
					       u32 *num_ppcs, u32 *ppc_mask,
					       u32 *reg_ppc_count)
{
	u32 data32;
	u32 num_pes_per_gpc = nvgpu_get_litter_value(g, GPU_LIT_NUM_PES_PER_GPC);

	/*
	 * if there is only 1 PES_PER_GPC, then we put the PES registers
	 * in the GPC reglist, so we can't error out if ppc.count == 0
	 */
	if ((!g->gr.ctx_vars.valid) ||
	    ((g->gr.ctx_vars.ctxsw_regs.ppc.count == 0) &&
	     (num_pes_per_gpc > 1)))
		return -EINVAL;

	data32 = *(u32 *)(context + ctxsw_prog_local_image_ppc_info_o());

	*num_ppcs = ctxsw_prog_local_image_ppc_info_num_ppcs_v(data32);
	*ppc_mask = ctxsw_prog_local_image_ppc_info_ppc_mask_v(data32);

	*reg_ppc_count = g->gr.ctx_vars.ctxsw_regs.ppc.count;

	return 0;
}

/*
 *  This function will return the 32 bit offset for a priv register if it is
 *  present in the context buffer. The context buffer is in CPU memory.
 */
static int gr_gk20a_find_priv_offset_in_buffer(struct gk20a *g,
					       u32 addr,
					       bool is_quad, u32 quad,
					       u32 *context_buffer,
					       u32 context_buffer_size,
					       u32 *priv_offset)
{
	struct gr_gk20a *gr = &g->gr;
	u32 i, data32;
	int err;
	int addr_type; /*enum ctxsw_addr_type */
	u32 broadcast_flags;
	u32 gpc_num, tpc_num, ppc_num, be_num;
	u32 num_gpcs, num_tpcs, num_ppcs;
	u32 offset;
	u32 sys_priv_offset, gpc_priv_offset;
	u32 ppc_mask, reg_list_ppc_count;
	u8 *context;
	u32 offset_to_segment;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg, "addr=0x%x", addr);

	err = gr_gk20a_decode_priv_addr(g, addr, &addr_type,
					&gpc_num, &tpc_num, &ppc_num, &be_num,
					&broadcast_flags);
	if (err)
		return err;

	context = (u8 *)context_buffer;
	if (!check_main_image_header_magic(context)) {
		gk20a_err(dev_from_gk20a(g),
			   "Invalid main header: magic value");
		return -EINVAL;
	}
	num_gpcs = *(u32 *)(context + ctxsw_prog_main_image_num_gpcs_o());

	/* Parse the FECS local header. */
	context += ctxsw_prog_ucode_header_size_in_bytes();
	if (!check_local_header_magic(context)) {
		gk20a_err(dev_from_gk20a(g),
			   "Invalid FECS local header: magic value\n");
		return -EINVAL;
	}
	data32 = *(u32 *)(context + ctxsw_prog_local_priv_register_ctl_o());
	sys_priv_offset = ctxsw_prog_local_priv_register_ctl_offset_v(data32);

	/* If found in Ext buffer, ok.
	 * If it failed and we expected to find it there (quad offset)
	 * then return the error.  Otherwise continue on.
	 */
	err = gr_gk20a_find_priv_offset_in_ext_buffer(g,
				      addr, is_quad, quad, context_buffer,
				      context_buffer_size, priv_offset);
	if (!err || (err && is_quad))
		return err;

	if ((addr_type == CTXSW_ADDR_TYPE_SYS) ||
	    (addr_type == CTXSW_ADDR_TYPE_BE)) {
		/* Find the offset in the FECS segment. */
		offset_to_segment = sys_priv_offset *
			ctxsw_prog_ucode_header_size_in_bytes();

		err = gr_gk20a_process_context_buffer_priv_segment(g,
					   addr_type, addr,
					   0, 0, 0, 0,
					   &offset);
		if (err)
			return err;

		*priv_offset = (offset_to_segment + offset);
		return 0;
	}

	if ((gpc_num + 1) > num_gpcs)  {
		gk20a_err(dev_from_gk20a(g),
			   "GPC %d not in this context buffer.\n",
			   gpc_num);
		return -EINVAL;
	}

	/* Parse the GPCCS local header(s).*/
	for (i = 0; i < num_gpcs; i++) {
		context += ctxsw_prog_ucode_header_size_in_bytes();
		if (!check_local_header_magic(context)) {
			gk20a_err(dev_from_gk20a(g),
				   "Invalid GPCCS local header: magic value\n");
			return -EINVAL;

		}
		data32 = *(u32 *)(context + ctxsw_prog_local_priv_register_ctl_o());
		gpc_priv_offset = ctxsw_prog_local_priv_register_ctl_offset_v(data32);

		err = gr_gk20a_determine_ppc_configuration(g, context,
							   &num_ppcs, &ppc_mask,
							   &reg_list_ppc_count);
		if (err)
			return err;

		num_tpcs = *(u32 *)(context + ctxsw_prog_local_image_num_tpcs_o());

		if ((i == gpc_num) && ((tpc_num + 1) > num_tpcs)) {
			gk20a_err(dev_from_gk20a(g),
			   "GPC %d TPC %d not in this context buffer.\n",
				   gpc_num, tpc_num);
			return -EINVAL;
		}

		/* Find the offset in the GPCCS segment.*/
		if (i == gpc_num) {
			offset_to_segment = gpc_priv_offset *
				ctxsw_prog_ucode_header_size_in_bytes();

			if (addr_type == CTXSW_ADDR_TYPE_TPC) {
				/*reg = gr->ctx_vars.ctxsw_regs.tpc.l;*/
			} else if (addr_type == CTXSW_ADDR_TYPE_PPC) {
				/* The ucode stores TPC data before PPC data.
				 * Advance offset past TPC data to PPC data. */
				offset_to_segment +=
					((gr->ctx_vars.ctxsw_regs.tpc.count *
					  num_tpcs) << 2);
			} else if (addr_type == CTXSW_ADDR_TYPE_GPC) {
				/* The ucode stores TPC/PPC data before GPC data.
				 * Advance offset past TPC/PPC data to GPC data. */
				/* note 1 PES_PER_GPC case */
				u32 num_pes_per_gpc = nvgpu_get_litter_value(g,
						GPU_LIT_NUM_PES_PER_GPC);
				if (num_pes_per_gpc > 1) {
					offset_to_segment +=
						(((gr->ctx_vars.ctxsw_regs.tpc.count *
						   num_tpcs) << 2) +
						 ((reg_list_ppc_count * num_ppcs) << 2));
				} else {
					offset_to_segment +=
						((gr->ctx_vars.ctxsw_regs.tpc.count *
						  num_tpcs) << 2);
				}
			} else {
				gk20a_dbg_fn("Unknown address type.");
				return -EINVAL;
			}
			err = gr_gk20a_process_context_buffer_priv_segment(g,
							   addr_type, addr,
							   i, num_tpcs,
							   num_ppcs, ppc_mask,
							   &offset);
			if (err)
			    return -EINVAL;

			*priv_offset = offset_to_segment + offset;
			return 0;
		}
	}

	return -EINVAL;
}

static int map_cmp(const void *a, const void *b)
{
	struct ctxsw_buf_offset_map_entry *e1 =
					(struct ctxsw_buf_offset_map_entry *)a;
	struct ctxsw_buf_offset_map_entry *e2 =
					(struct ctxsw_buf_offset_map_entry *)b;

	if (e1->addr < e2->addr)
		return -1;

	if (e1->addr > e2->addr)
		return 1;
	return 0;
}

static int add_ctxsw_buffer_map_entries_pmsys(struct ctxsw_buf_offset_map_entry *map,
						struct aiv_list_gk20a *regs,
						u32 *count, u32 *offset,
						u32 max_cnt, u32 base, u32 mask)
{
	u32 idx;
	u32 cnt = *count;
	u32 off = *offset;

	if ((cnt + regs->count) > max_cnt)
		return -EINVAL;

	for (idx = 0; idx < regs->count; idx++) {
		if ((base + (regs->l[idx].addr & mask)) < 0xFFF)
			map[cnt].addr = base + (regs->l[idx].addr & mask)
					+ NV_PCFG_BASE;
		else
			map[cnt].addr = base + (regs->l[idx].addr & mask);
		map[cnt++].offset = off;
		off += 4;
	}
	*count = cnt;
	*offset = off;
	return 0;
}

static int add_ctxsw_buffer_map_entries_pmgpc(struct gk20a *g,
					struct ctxsw_buf_offset_map_entry *map,
					struct aiv_list_gk20a *regs,
					u32 *count, u32 *offset,
					u32 max_cnt, u32 base, u32 mask)
{
	u32 idx;
	u32 cnt = *count;
	u32 off = *offset;

	if ((cnt + regs->count) > max_cnt)
		return -EINVAL;

	/* NOTE: The PPC offsets get added to the pm_gpc list if numPpc <= 1
	 * To handle the case of PPC registers getting added into GPC, the below
	 * code specifically checks for any PPC offsets and adds them using
	 * proper mask
	 */
	for (idx = 0; idx < regs->count; idx++) {
		/* Check if the address is PPC address */
		if (pri_is_ppc_addr_shared(g, regs->l[idx].addr & mask)) {
			u32 ppc_in_gpc_base = nvgpu_get_litter_value(g,
						GPU_LIT_PPC_IN_GPC_BASE);
			u32 ppc_in_gpc_stride = nvgpu_get_litter_value(g,
						GPU_LIT_PPC_IN_GPC_STRIDE);
			/* Use PPC mask instead of the GPC mask provided */
			u32 ppcmask = ppc_in_gpc_stride - 1;

			map[cnt].addr = base + ppc_in_gpc_base
					+ (regs->l[idx].addr & ppcmask);
		} else
			map[cnt].addr = base + (regs->l[idx].addr & mask);
		map[cnt++].offset = off;
		off += 4;
	}
	*count = cnt;
	*offset = off;
	return 0;
}

static int add_ctxsw_buffer_map_entries(struct ctxsw_buf_offset_map_entry *map,
					struct aiv_list_gk20a *regs,
					u32 *count, u32 *offset,
					u32 max_cnt, u32 base, u32 mask)
{
	u32 idx;
	u32 cnt = *count;
	u32 off = *offset;

	if ((cnt + regs->count) > max_cnt)
		return -EINVAL;

	for (idx = 0; idx < regs->count; idx++) {
		map[cnt].addr = base + (regs->l[idx].addr & mask);
		map[cnt++].offset = off;
		off += 4;
	}
	*count = cnt;
	*offset = off;
	return 0;
}

/* Helper function to add register entries to the register map for all
 * subunits
 */
static int add_ctxsw_buffer_map_entries_subunits(
					struct ctxsw_buf_offset_map_entry *map,
					struct aiv_list_gk20a *regs,
					u32 *count, u32 *offset,
					u32 max_cnt, u32 base,
					u32 num_units, u32 stride, u32 mask)
{
	u32 unit;
	u32 idx;
	u32 cnt = *count;
	u32 off = *offset;

	if ((cnt + (regs->count * num_units)) > max_cnt)
		return -EINVAL;

	/* Data is interleaved for units in ctxsw buffer */
	for (idx = 0; idx < regs->count; idx++) {
		for (unit = 0; unit < num_units; unit++) {
			map[cnt].addr = base + (regs->l[idx].addr & mask) +
					(unit * stride);
			map[cnt++].offset = off;
			off += 4;
		}
	}
	*count = cnt;
	*offset = off;
	return 0;
}

static int add_ctxsw_buffer_map_entries_gpcs(struct gk20a *g,
					struct ctxsw_buf_offset_map_entry *map,
					u32 *count, u32 *offset, u32 max_cnt)
{
	u32 num_gpcs = g->gr.gpc_count;
	u32 num_ppcs, num_tpcs, gpc_num, base;
	u32 gpc_base = nvgpu_get_litter_value(g, GPU_LIT_GPC_BASE);
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 ppc_in_gpc_base = nvgpu_get_litter_value(g, GPU_LIT_PPC_IN_GPC_BASE);
	u32 ppc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_PPC_IN_GPC_STRIDE);
	u32 tpc_in_gpc_base = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_BASE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);

	for (gpc_num = 0; gpc_num < num_gpcs; gpc_num++) {
		num_tpcs = g->gr.gpc_tpc_count[gpc_num];
		base = gpc_base + (gpc_stride * gpc_num) + tpc_in_gpc_base;
		if (add_ctxsw_buffer_map_entries_subunits(map,
					&g->gr.ctx_vars.ctxsw_regs.pm_tpc,
					count, offset, max_cnt, base, num_tpcs,
					tpc_in_gpc_stride,
					(tpc_in_gpc_stride - 1)))
			return -EINVAL;

		num_ppcs = g->gr.gpc_ppc_count[gpc_num];
		base = gpc_base + (gpc_stride * gpc_num) + ppc_in_gpc_base;
		if (add_ctxsw_buffer_map_entries_subunits(map,
					&g->gr.ctx_vars.ctxsw_regs.pm_ppc,
					count, offset, max_cnt, base, num_ppcs,
					ppc_in_gpc_stride,
					(ppc_in_gpc_stride - 1)))
			return -EINVAL;

		base = gpc_base + (gpc_stride * gpc_num);
		if (add_ctxsw_buffer_map_entries_pmgpc(g, map,
					&g->gr.ctx_vars.ctxsw_regs.pm_gpc,
					count, offset, max_cnt, base,
					(gpc_stride - 1)))
			return -EINVAL;

		base = NV_XBAR_MXBAR_PRI_GPC_GNIC_STRIDE * gpc_num;
		if (add_ctxsw_buffer_map_entries(map,
					&g->gr.ctx_vars.ctxsw_regs.pm_ucgpc,
					count, offset, max_cnt, base, ~0))
			return -EINVAL;

		base = (NV_PERF_PMMGPC_CHIPLET_OFFSET * gpc_num);
		if (add_ctxsw_buffer_map_entries(map,
					&g->gr.ctx_vars.ctxsw_regs.perf_gpc,
					count, offset, max_cnt, base, ~0))
			return -EINVAL;

		base = (NV_PERF_PMMGPCROUTER_STRIDE * gpc_num);
		if (add_ctxsw_buffer_map_entries(map,
					&g->gr.ctx_vars.ctxsw_regs.gpc_router,
					count, offset, max_cnt, base, ~0))
			return -EINVAL;

		*offset = ALIGN(*offset, 256);
	}
	return 0;
}

/*
 *            PM CTXSW BUFFER LAYOUT :
 *|---------------------------------------------|0x00 <----PM CTXSW BUFFER BASE
 *|                                             |
 *|        LIST_compressed_pm_ctx_reg_SYS       |Space allocated: numRegs words
 *|---------------------------------------------|
 *|                                             |
 *|    LIST_compressed_nv_perf_ctx_reg_SYS      |Space allocated: numRegs words
 *|---------------------------------------------|
 *|                                             |
 *|    LIST_compressed_nv_perf_ctx_reg_sysrouter|Space allocated: numRegs words
 *|---------------------------------------------|
 *|                                             |
 *|    LIST_compressed_nv_perf_ctx_reg_PMA      |Space allocated: numRegs words
 *|---------------------------------------------|
 *|        PADDING for 256 byte alignment       |
 *|---------------------------------------------|<----256 byte aligned
 *|    LIST_compressed_nv_perf_fbp_ctx_regs     |
 *|                                             |Space allocated: numRegs * n words (for n FB units)
 *|---------------------------------------------|
 *| LIST_compressed_nv_perf_fbprouter_ctx_regs  |
 *|                                             |Space allocated: numRegs * n words (for n FB units)
 *|---------------------------------------------|
 *|    LIST_compressed_pm_fbpa_ctx_regs         |
 *|                                             |Space allocated: numRegs * n words (for n FB units)
 *|---------------------------------------------|
 *|    LIST_compressed_pm_rop_ctx_regs          |
 *|---------------------------------------------|
 *|    LIST_compressed_pm_ltc_ctx_regs          |
 *|                                  LTC0 LTS0  |
 *|                                  LTC1 LTS0  |Space allocated: numRegs * n words (for n LTC units)
 *|                                  LTCn LTS0  |
 *|                                  LTC0 LTS1  |
 *|                                  LTC1 LTS1  |
 *|                                  LTCn LTS1  |
 *|                                  LTC0 LTSn  |
 *|                                  LTC1 LTSn  |
 *|                                  LTCn LTSn  |
 *|---------------------------------------------|
 *|        PADDING for 256 byte alignment       |
 *|---------------------------------------------|<----256 byte aligned
 *|                            GPC0  REG0 TPC0  |Each GPC has space allocated to accommodate
 *|                                  REG0 TPC1  |    all the GPC/TPC register lists
 *| Lists in each GPC region:        REG0 TPCn  |Per GPC allocated space is always 256 byte aligned
 *|  LIST_pm_ctx_reg_TPC             REG1 TPC0  |
 *|             * numTpcs            REG1 TPC1  |
 *|  LIST_pm_ctx_reg_PPC             REG1 TPCn  |
 *|             * numPpcs            REGn TPC0  |
 *|  LIST_pm_ctx_reg_GPC             REGn TPC1  |
 *|  List_pm_ctx_reg_uc_GPC          REGn TPCn  |
 *|  LIST_nv_perf_ctx_reg_GPC                   |
 *|                                       ----  |--
 *|                            GPC1         .   |
 *|                                         .   |<----
 *|---------------------------------------------|
 *=                                             =
 *|                            GPCn             |
 *=                                             =
 *|---------------------------------------------|
 */

static int gr_gk20a_create_hwpm_ctxsw_buffer_offset_map(struct gk20a *g)
{
	u32 hwpm_ctxsw_buffer_size = g->gr.ctx_vars.pm_ctxsw_image_size;
	u32 hwpm_ctxsw_reg_count_max;
	u32 map_size;
	u32 i, count = 0;
	u32 offset = 0;
	struct ctxsw_buf_offset_map_entry *map;
	u32 ltc_stride = nvgpu_get_litter_value(g, GPU_LIT_LTC_STRIDE);
	u32 num_fbpas = nvgpu_get_litter_value(g, GPU_LIT_NUM_FBPAS);
	u32 fbpa_stride = nvgpu_get_litter_value(g, GPU_LIT_FBPA_STRIDE);
	u32 num_ltc = g->ops.gr.get_max_ltc_per_fbp(g) * g->gr.num_fbps;

	if (hwpm_ctxsw_buffer_size == 0) {
		gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg,
			"no PM Ctxsw buffer memory in context buffer");
		return -EINVAL;
	}

	hwpm_ctxsw_reg_count_max = hwpm_ctxsw_buffer_size >> 2;
	map_size = hwpm_ctxsw_reg_count_max * sizeof(*map);

	map = nvgpu_alloc(map_size, true);
	if (!map)
		return -ENOMEM;

	/* Add entries from _LIST_pm_ctx_reg_SYS */
	if (add_ctxsw_buffer_map_entries_pmsys(map, &g->gr.ctx_vars.ctxsw_regs.pm_sys,
				&count, &offset, hwpm_ctxsw_reg_count_max, 0, ~0))
		goto cleanup;

	/* Add entries from _LIST_nv_perf_ctx_reg_SYS */
	if (add_ctxsw_buffer_map_entries(map, &g->gr.ctx_vars.ctxsw_regs.perf_sys,
				&count, &offset, hwpm_ctxsw_reg_count_max, 0, ~0))
		goto cleanup;

	/* Add entries from _LIST_nv_perf_sysrouter_ctx_reg*/
	if (add_ctxsw_buffer_map_entries(map, &g->gr.ctx_vars.ctxsw_regs.perf_sys_router,
				&count, &offset, hwpm_ctxsw_reg_count_max, 0, ~0))
		goto cleanup;

	/* Add entries from _LIST_nv_perf_pma_ctx_reg*/
	if (add_ctxsw_buffer_map_entries(map, &g->gr.ctx_vars.ctxsw_regs.perf_pma,
				&count, &offset, hwpm_ctxsw_reg_count_max, 0, ~0))
		goto cleanup;

	offset = ALIGN(offset, 256);

	/* Add entries from _LIST_nv_perf_fbp_ctx_regs */
	if (add_ctxsw_buffer_map_entries_subunits(map,
					&g->gr.ctx_vars.ctxsw_regs.fbp,
					&count, &offset,
					hwpm_ctxsw_reg_count_max, 0,
					g->gr.num_fbps, NV_PMM_FBP_STRIDE, ~0))
		goto cleanup;

	/* Add entries from _LIST_nv_perf_fbprouter_ctx_regs */
	if (add_ctxsw_buffer_map_entries_subunits(map,
					&g->gr.ctx_vars.ctxsw_regs.fbp_router,
					&count, &offset,
					hwpm_ctxsw_reg_count_max, 0, g->gr.num_fbps,
					NV_PERF_PMM_FBP_ROUTER_STRIDE, ~0))
		goto cleanup;

	/* Add entries from _LIST_nv_pm_fbpa_ctx_regs */
	if (add_ctxsw_buffer_map_entries_subunits(map,
					&g->gr.ctx_vars.ctxsw_regs.pm_fbpa,
					&count, &offset,
					hwpm_ctxsw_reg_count_max, 0,
					num_fbpas, fbpa_stride, ~0))
		goto cleanup;

	/* Add entries from _LIST_nv_pm_rop_ctx_regs */
	if (add_ctxsw_buffer_map_entries(map,
					&g->gr.ctx_vars.ctxsw_regs.pm_rop,
					&count, &offset,
					hwpm_ctxsw_reg_count_max, 0, ~0))
		goto cleanup;

	/* Add entries from _LIST_compressed_nv_pm_ltc_ctx_regs */
	if (add_ctxsw_buffer_map_entries_subunits(map,
					&g->gr.ctx_vars.ctxsw_regs.pm_ltc,
					&count, &offset,
					hwpm_ctxsw_reg_count_max, 0,
					num_ltc, ltc_stride, ~0))
		goto cleanup;

	offset = ALIGN(offset, 256);

	/* Add GPC entries */
	if (add_ctxsw_buffer_map_entries_gpcs(g, map, &count, &offset,
					hwpm_ctxsw_reg_count_max))
		goto cleanup;

	if (offset > hwpm_ctxsw_buffer_size) {
		gk20a_err(dev_from_gk20a(g), "offset > buffer size");
		goto cleanup;
	}

	sort(map, count, sizeof(*map), map_cmp, NULL);

	g->gr.ctx_vars.hwpm_ctxsw_buffer_offset_map = map;
	g->gr.ctx_vars.hwpm_ctxsw_buffer_offset_map_count = count;

	gk20a_dbg_info("Reg Addr => HWPM Ctxt switch buffer offset");

	for (i = 0; i < count; i++)
		gk20a_dbg_info("%08x => %08x", map[i].addr, map[i].offset);

	return 0;
cleanup:
	gk20a_err(dev_from_gk20a(g), "Failed to create HWPM buffer offset map");
	nvgpu_free(map);
	return -EINVAL;
}

/*
 *  This function will return the 32 bit offset for a priv register if it is
 *  present in the PM context buffer.
 */
static int gr_gk20a_find_priv_offset_in_pm_buffer(struct gk20a *g,
					          u32 addr,
					          u32 *priv_offset)
{
	struct gr_gk20a *gr = &g->gr;
	int err = 0;
	u32 count;
	struct ctxsw_buf_offset_map_entry *map, *result, map_key;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg, "addr=0x%x", addr);

	/* Create map of pri address and pm offset if necessary */
	if (gr->ctx_vars.hwpm_ctxsw_buffer_offset_map == NULL) {
		err = gr_gk20a_create_hwpm_ctxsw_buffer_offset_map(g);
		if (err)
			return err;
	}

	*priv_offset = 0;

	map = gr->ctx_vars.hwpm_ctxsw_buffer_offset_map;
	count = gr->ctx_vars.hwpm_ctxsw_buffer_offset_map_count;

	map_key.addr = addr;
	result = bsearch(&map_key, map, count, sizeof(*map), map_cmp);

	if (result)
		*priv_offset = result->offset;
	else {
		gk20a_err(dev_from_gk20a(g), "Lookup failed for address 0x%x", addr);
		err = -EINVAL;
	}
	return err;
}

bool gk20a_is_channel_ctx_resident(struct channel_gk20a *ch)
{
	int curr_gr_ctx, curr_gr_tsgid;
	struct gk20a *g = ch->g;
	struct channel_gk20a *curr_ch;
	bool ret = false;

	curr_gr_ctx  = gk20a_readl(g, gr_fecs_current_ctx_r());
	curr_ch = gk20a_gr_get_channel_from_ctx(g, curr_gr_ctx,
					      &curr_gr_tsgid);

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg,
		  "curr_gr_chid=%d curr_tsgid=%d, ch->tsgid=%d"
		  " ch->hw_chid=%d",
		  curr_ch ? curr_ch->hw_chid : -1,
		  curr_gr_tsgid,
		  ch->tsgid,
		  ch->hw_chid);

	if (!curr_ch)
		return false;

	if (ch->hw_chid == curr_ch->hw_chid)
		ret = true;

	if (gk20a_is_channel_marked_as_tsg(ch) && (ch->tsgid == curr_gr_tsgid))
		ret = true;

	gk20a_channel_put(curr_ch);
	return ret;
}

int gr_gk20a_exec_ctx_ops(struct channel_gk20a *ch,
			  struct nvgpu_dbg_gpu_reg_op *ctx_ops, u32 num_ops,
			  u32 num_ctx_wr_ops, u32 num_ctx_rd_ops)
{
	struct gk20a *g = ch->g;
	struct channel_ctx_gk20a *ch_ctx = &ch->ch_ctx;
	bool gr_ctx_ready = false;
	bool pm_ctx_ready = false;
	struct mem_desc *current_mem = NULL;
	bool ch_is_curr_ctx, restart_gr_ctxsw = false;
	u32 i, j, offset, v;
	struct gr_gk20a *gr = &g->gr;
	u32 max_offsets = gr->max_gpc_count * gr->max_tpc_per_gpc_count;
	u32 *offsets = NULL;
	u32 *offset_addrs = NULL;
	u32 ctx_op_nr, num_ctx_ops[2] = {num_ctx_wr_ops, num_ctx_rd_ops};
	int err, pass;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg, "wr_ops=%d rd_ops=%d",
		   num_ctx_wr_ops, num_ctx_rd_ops);

	/* disable channel switching.
	 * at that point the hardware state can be inspected to
	 * determine if the context we're interested in is current.
	 */
	err = gr_gk20a_disable_ctxsw(g);
	if (err) {
		gk20a_err(dev_from_gk20a(g), "unable to stop gr ctxsw");
		/* this should probably be ctx-fatal... */
		goto cleanup;
	}

	restart_gr_ctxsw = true;

	ch_is_curr_ctx = gk20a_is_channel_ctx_resident(ch);

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg, "is curr ctx=%d", ch_is_curr_ctx);

	if (ch_is_curr_ctx) {
		for (pass = 0; pass < 2; pass++) {
			ctx_op_nr = 0;
			for (i = 0; (ctx_op_nr < num_ctx_ops[pass]) && (i < num_ops); ++i) {
				/* only do ctx ops and only on the right pass */
				if ((ctx_ops[i].type == REGOP(TYPE_GLOBAL)) ||
				    (((pass == 0) && reg_op_is_read(ctx_ops[i].op)) ||
				     ((pass == 1) && !reg_op_is_read(ctx_ops[i].op))))
					continue;

				/* if this is a quad access, setup for special access*/
				if (ctx_ops[i].type == REGOP(TYPE_GR_CTX_QUAD)
						&& g->ops.gr.access_smpc_reg)
					g->ops.gr.access_smpc_reg(g,
							ctx_ops[i].quad,
							ctx_ops[i].offset);
				offset = ctx_ops[i].offset;

				if (pass == 0) { /* write pass */
					v = gk20a_readl(g, offset);
					v &= ~ctx_ops[i].and_n_mask_lo;
					v |= ctx_ops[i].value_lo;
					gk20a_writel(g, offset, v);

					gk20a_dbg(gpu_dbg_gpu_dbg,
						   "direct wr: offset=0x%x v=0x%x",
						   offset, v);

					if (ctx_ops[i].op == REGOP(WRITE_64)) {
						v = gk20a_readl(g, offset + 4);
						v &= ~ctx_ops[i].and_n_mask_hi;
						v |= ctx_ops[i].value_hi;
						gk20a_writel(g, offset + 4, v);

						gk20a_dbg(gpu_dbg_gpu_dbg,
							   "direct wr: offset=0x%x v=0x%x",
							   offset + 4, v);
					}

				} else { /* read pass */
					ctx_ops[i].value_lo =
						gk20a_readl(g, offset);

					gk20a_dbg(gpu_dbg_gpu_dbg,
						   "direct rd: offset=0x%x v=0x%x",
						   offset, ctx_ops[i].value_lo);

					if (ctx_ops[i].op == REGOP(READ_64)) {
						ctx_ops[i].value_hi =
							gk20a_readl(g, offset + 4);

						gk20a_dbg(gpu_dbg_gpu_dbg,
							   "direct rd: offset=0x%x v=0x%x",
							   offset, ctx_ops[i].value_lo);
					} else
						ctx_ops[i].value_hi = 0;
				}
				ctx_op_nr++;
			}
		}
		goto cleanup;
	}

	/* they're the same size, so just use one alloc for both */
	offsets = kzalloc(2 * sizeof(u32) * max_offsets, GFP_KERNEL);
	if (!offsets) {
		err = -ENOMEM;
		goto cleanup;
	}
	offset_addrs = offsets + max_offsets;

	err = gr_gk20a_ctx_patch_write_begin(g, ch_ctx);
	if (err)
		goto cleanup;

	g->ops.mm.l2_flush(g, true);

	/* write to appropriate place in context image,
	 * first have to figure out where that really is */

	/* first pass is writes, second reads */
	for (pass = 0; pass < 2; pass++) {
		ctx_op_nr = 0;
		for (i = 0; (ctx_op_nr < num_ctx_ops[pass]) && (i < num_ops); ++i) {
			u32 num_offsets;

			/* only do ctx ops and only on the right pass */
			if ((ctx_ops[i].type == REGOP(TYPE_GLOBAL)) ||
			    (((pass == 0) && reg_op_is_read(ctx_ops[i].op)) ||
			     ((pass == 1) && !reg_op_is_read(ctx_ops[i].op))))
				continue;

			err = gr_gk20a_get_ctx_buffer_offsets(g,
						ctx_ops[i].offset,
						max_offsets,
						offsets, offset_addrs,
						&num_offsets,
						ctx_ops[i].type == REGOP(TYPE_GR_CTX_QUAD),
						ctx_ops[i].quad);
			if (!err) {
				if (!gr_ctx_ready) {
					/* would have been a variant of
					 * gr_gk20a_apply_instmem_overrides,
					 * recoded in-place instead.
					 */
					if (gk20a_mem_begin(g, &ch_ctx->gr_ctx->mem)) {
						err = -ENOMEM;
						goto cleanup;
					}
					gr_ctx_ready = true;
				}
				current_mem = &ch_ctx->gr_ctx->mem;
			} else {
				err = gr_gk20a_get_pm_ctx_buffer_offsets(g,
							ctx_ops[i].offset,
							max_offsets,
							offsets, offset_addrs,
							&num_offsets);
				if (err) {
					gk20a_dbg(gpu_dbg_gpu_dbg,
					   "ctx op invalid offset: offset=0x%x",
					   ctx_ops[i].offset);
					ctx_ops[i].status =
						NVGPU_DBG_GPU_REG_OP_STATUS_INVALID_OFFSET;
					continue;
				}
				if (!pm_ctx_ready) {
					/* Make sure ctx buffer was initialized */
					if (!ch_ctx->pm_ctx.mem.pages) {
						gk20a_err(dev_from_gk20a(g),
							"Invalid ctx buffer");
						err = -EINVAL;
						goto cleanup;
					}
					if (gk20a_mem_begin(g, &ch_ctx->pm_ctx.mem)) {
						err = -ENOMEM;
						goto cleanup;
					}
					pm_ctx_ready = true;
				}
				current_mem = &ch_ctx->pm_ctx.mem;
			}

			/* if this is a quad access, setup for special access*/
			if (ctx_ops[i].type == REGOP(TYPE_GR_CTX_QUAD) &&
					g->ops.gr.access_smpc_reg)
				g->ops.gr.access_smpc_reg(g, ctx_ops[i].quad,
							 ctx_ops[i].offset);

			for (j = 0; j < num_offsets; j++) {
				/* sanity check gr ctxt offsets,
				 * don't write outside, worst case
				 */
				if ((current_mem == &ch_ctx->gr_ctx->mem) &&
					(offsets[j] >= g->gr.ctx_vars.golden_image_size))
					continue;
				if (pass == 0) { /* write pass */
					v = gk20a_mem_rd(g, current_mem, offsets[j]);
					v &= ~ctx_ops[i].and_n_mask_lo;
					v |= ctx_ops[i].value_lo;
					gk20a_mem_wr(g, current_mem, offsets[j], v);

					gk20a_dbg(gpu_dbg_gpu_dbg,
						   "context wr: offset=0x%x v=0x%x",
						   offsets[j], v);

					if (ctx_ops[i].op == REGOP(WRITE_64)) {
						v = gk20a_mem_rd(g, current_mem, offsets[j] + 4);
						v &= ~ctx_ops[i].and_n_mask_hi;
						v |= ctx_ops[i].value_hi;
						gk20a_mem_wr(g, current_mem, offsets[j] + 4, v);

						gk20a_dbg(gpu_dbg_gpu_dbg,
							   "context wr: offset=0x%x v=0x%x",
							   offsets[j] + 4, v);
					}

					/* check to see if we need to add a special WAR
					   for some of the SMPC perf regs */
					gr_gk20a_ctx_patch_smpc(g, ch_ctx, offset_addrs[j],
							v, current_mem);

				} else { /* read pass */
					ctx_ops[i].value_lo =
						gk20a_mem_rd(g, current_mem, offsets[0]);

					gk20a_dbg(gpu_dbg_gpu_dbg, "context rd: offset=0x%x v=0x%x",
						   offsets[0], ctx_ops[i].value_lo);

					if (ctx_ops[i].op == REGOP(READ_64)) {
						ctx_ops[i].value_hi =
							gk20a_mem_rd(g, current_mem, offsets[0] + 4);

						gk20a_dbg(gpu_dbg_gpu_dbg,
							   "context rd: offset=0x%x v=0x%x",
							   offsets[0] + 4, ctx_ops[i].value_hi);
					} else
						ctx_ops[i].value_hi = 0;
				}
			}
			ctx_op_nr++;
		}
	}
#if 0
	/* flush cpu caches for the ctx buffer? only if cpu cached, of course.
	 * they aren't, yet */
	if (cached) {
		FLUSH_CPU_DCACHE(ctx_ptr,
			 sg_phys(ch_ctx->gr_ctx.mem.ref), size);
	}
#endif

 cleanup:
	if (offsets)
		kfree(offsets);

	if (ch_ctx->patch_ctx.mem.cpu_va)
		gr_gk20a_ctx_patch_write_end(g, ch_ctx);
	if (gr_ctx_ready)
		gk20a_mem_end(g, &ch_ctx->gr_ctx->mem);
	if (pm_ctx_ready)
		gk20a_mem_end(g, &ch_ctx->pm_ctx.mem);

	if (restart_gr_ctxsw) {
		int tmp_err = gr_gk20a_enable_ctxsw(g);
		if (tmp_err) {
			gk20a_err(dev_from_gk20a(g), "unable to restart ctxsw!\n");
			err = tmp_err;
		}
	}

	return err;
}

static void gr_gk20a_cb_size_default(struct gk20a *g)
{
	struct gr_gk20a *gr = &g->gr;

	if (!gr->attrib_cb_default_size)
		gr->attrib_cb_default_size =
			gr_gpc0_ppc0_cbm_cfg_size_default_v();
	gr->alpha_cb_default_size =
		gr_gpc0_ppc0_cbm_cfg2_size_default_v();
}

static int gr_gk20a_calc_global_ctx_buffer_size(struct gk20a *g)
{
	struct gr_gk20a *gr = &g->gr;
	int size;

	gr->attrib_cb_size = gr->attrib_cb_default_size;
	gr->alpha_cb_size = gr->alpha_cb_default_size
		+ (gr->alpha_cb_default_size >> 1);

	size = gr->attrib_cb_size *
		gr_gpc0_ppc0_cbm_cfg_size_granularity_v() *
		gr->max_tpc_count;

	size += gr->alpha_cb_size *
		gr_gpc0_ppc0_cbm_cfg2_size_granularity_v() *
		gr->max_tpc_count;

	return size;
}

void gr_gk20a_commit_global_pagepool(struct gk20a *g,
					    struct channel_ctx_gk20a *ch_ctx,
					    u64 addr, u32 size, bool patch)
{
	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_scc_pagepool_base_r(),
		gr_scc_pagepool_base_addr_39_8_f(addr), patch);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_scc_pagepool_r(),
		gr_scc_pagepool_total_pages_f(size) |
		gr_scc_pagepool_valid_true_f(), patch);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_gcc_pagepool_base_r(),
		gr_gpcs_gcc_pagepool_base_addr_39_8_f(addr), patch);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_gcc_pagepool_r(),
		gr_gpcs_gcc_pagepool_total_pages_f(size), patch);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_pd_pagepool_r(),
		gr_pd_pagepool_total_pages_f(size) |
		gr_pd_pagepool_valid_true_f(), patch);
}

void gk20a_init_gr(struct gk20a *g)
{
	init_waitqueue_head(&g->gr.init_wq);
}

static bool gr_gk20a_is_tpc_addr(struct gk20a *g, u32 addr)
{
	u32 tpc_in_gpc_base = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_BASE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 num_tpc_per_gpc = nvgpu_get_litter_value(g, GPU_LIT_NUM_TPC_PER_GPC);
	return ((addr >= tpc_in_gpc_base) &&
		(addr < tpc_in_gpc_base +
		 (num_tpc_per_gpc * tpc_in_gpc_stride)))
		|| pri_is_tpc_addr_shared(g, addr);
}

static u32 gr_gk20a_get_tpc_num(struct gk20a *g, u32 addr)
{
	u32 i, start;
	u32 num_tpcs = nvgpu_get_litter_value(g, GPU_LIT_NUM_TPC_PER_GPC);
	u32 tpc_in_gpc_base = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_BASE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);

	for (i = 0; i < num_tpcs; i++) {
		start = tpc_in_gpc_base + (i * tpc_in_gpc_stride);
		if ((addr >= start) &&
		    (addr < (start + tpc_in_gpc_stride)))
			return i;
	}
	return 0;
}

int gk20a_gr_wait_for_sm_lock_down(struct gk20a *g, u32 gpc, u32 tpc,
		u32 global_esr_mask, bool check_errors)
{
	bool locked_down;
	bool no_error_pending;
	u32 delay = GR_IDLE_CHECK_DEFAULT;
	bool mmu_debug_mode_enabled = g->ops.mm.is_debug_mode_enabled(g);
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 offset =
		gpc_stride * gpc + tpc_in_gpc_stride * tpc;

	gk20a_dbg(gpu_dbg_intr | gpu_dbg_gpu_dbg,
		"GPC%d TPC%d: locking down SM", gpc, tpc);

	/* wait for the sm to lock down */
	do {
		u32 global_esr = gk20a_readl(g,
				gr_gpc0_tpc0_sm_hww_global_esr_r() + offset);
		u32 warp_esr = gk20a_readl(g,
				gr_gpc0_tpc0_sm_hww_warp_esr_r() + offset);
		u32 dbgr_status0 = gk20a_readl(g,
				gr_gpc0_tpc0_sm_dbgr_status0_r() + offset);

		warp_esr = g->ops.gr.mask_hww_warp_esr(warp_esr);

		locked_down =
		    (gr_gpc0_tpc0_sm_dbgr_status0_locked_down_v(dbgr_status0) ==
		     gr_gpc0_tpc0_sm_dbgr_status0_locked_down_true_v());
		no_error_pending =
			check_errors &&
			(gr_gpc0_tpc0_sm_hww_warp_esr_error_v(warp_esr) ==
			 gr_gpc0_tpc0_sm_hww_warp_esr_error_none_v()) &&
			((global_esr & ~global_esr_mask) == 0);

		if (locked_down || no_error_pending) {
			gk20a_dbg(gpu_dbg_intr | gpu_dbg_gpu_dbg,
				  "GPC%d TPC%d: locked down SM", gpc, tpc);
			return 0;
		}

		/* if an mmu fault is pending and mmu debug mode is not
		 * enabled, the sm will never lock down. */
		if (!mmu_debug_mode_enabled &&
		     gk20a_fifo_mmu_fault_pending(g)) {
			gk20a_err(dev_from_gk20a(g),
				"GPC%d TPC%d: mmu fault pending,"
				" sm will never lock down!", gpc, tpc);
			return -EFAULT;
		}

		usleep_range(delay, delay * 2);
		delay = min_t(u32, delay << 1, GR_IDLE_CHECK_MAX);
	} while (!locked_down);

	gk20a_err(dev_from_gk20a(g),
		  "GPC%d TPC%d: timed out while trying to lock down SM",
		  gpc, tpc);

	return -EAGAIN;
}

void gk20a_suspend_single_sm(struct gk20a *g,
		u32 gpc, u32 tpc,
		u32 global_esr_mask, bool check_errors)
{
	u32 offset;
	int err;
	u32 dbgr_control0;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	offset = gpc_stride * gpc + tpc_in_gpc_stride * tpc;

	/* if an SM debugger isn't attached, skip suspend */
	if (!gk20a_gr_sm_debugger_attached(g)) {
		gk20a_err(dev_from_gk20a(g),
			"SM debugger not attached, skipping suspend!\n");
		return;
	}

	/* assert stop trigger. */
	dbgr_control0 = gk20a_readl(g,
				gr_gpc0_tpc0_sm_dbgr_control0_r() + offset);
	dbgr_control0 |= gr_gpcs_tpcs_sm_dbgr_control0_stop_trigger_enable_f();
	gk20a_writel(g, gr_gpc0_tpc0_sm_dbgr_control0_r() + offset,
			dbgr_control0);

	err = gk20a_gr_wait_for_sm_lock_down(g, gpc, tpc,
			global_esr_mask, check_errors);
	if (err) {
		gk20a_err(dev_from_gk20a(g),
			"SuspendSm failed\n");
		return;
	}
}

void gk20a_suspend_all_sms(struct gk20a *g,
		u32 global_esr_mask, bool check_errors)
{
	struct gr_gk20a *gr = &g->gr;
	u32 gpc, tpc;
	int err;
	u32 dbgr_control0;

	/* if an SM debugger isn't attached, skip suspend */
	if (!gk20a_gr_sm_debugger_attached(g)) {
		gk20a_err(dev_from_gk20a(g),
			"SM debugger not attached, skipping suspend!\n");
		return;
	}

	/* assert stop trigger. uniformity assumption: all SMs will have
	 * the same state in dbg_control0. */
	dbgr_control0 =
		gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_control0_r());
	dbgr_control0 |= gr_gpcs_tpcs_sm_dbgr_control0_stop_trigger_enable_f();

	/* broadcast write */
	gk20a_writel(g,
		gr_gpcs_tpcs_sm_dbgr_control0_r(), dbgr_control0);

	for (gpc = 0; gpc < gr->gpc_count; gpc++) {
		for (tpc = 0; tpc < gr->tpc_count; tpc++) {
			err =
			 gk20a_gr_wait_for_sm_lock_down(g, gpc, tpc,
					global_esr_mask, check_errors);
			if (err) {
				gk20a_err(dev_from_gk20a(g),
					"SuspendAllSms failed\n");
				return;
			}
		}
	}
}

void gk20a_resume_single_sm(struct gk20a *g,
		u32 gpc, u32 tpc)
{
	u32 dbgr_control0;
	u32 offset;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	/*
	 * The following requires some clarification. Despite the fact that both
	 * RUN_TRIGGER and STOP_TRIGGER have the word "TRIGGER" in their
	 *  names, only one is actually a trigger, and that is the STOP_TRIGGER.
	 * Merely writing a 1(_TASK) to the RUN_TRIGGER is not sufficient to
	 * resume the gpu - the _STOP_TRIGGER must explicitly be set to 0
	 * (_DISABLE) as well.

	* Advice from the arch group:  Disable the stop trigger first, as a
	* separate operation, in order to ensure that the trigger has taken
	* effect, before enabling the run trigger.
	*/

	offset = gpc_stride * gpc + tpc_in_gpc_stride * tpc;

	/*De-assert stop trigger */
	dbgr_control0 =
		gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_control0_r() + offset);
	dbgr_control0 = set_field(dbgr_control0,
			gr_gpcs_tpcs_sm_dbgr_control0_stop_trigger_m(),
			gr_gpcs_tpcs_sm_dbgr_control0_stop_trigger_disable_f());
	gk20a_writel(g,
		gr_gpc0_tpc0_sm_dbgr_control0_r() + offset, dbgr_control0);

	/* Run trigger */
	dbgr_control0 |= gr_gpcs_tpcs_sm_dbgr_control0_run_trigger_task_f();
	gk20a_writel(g,
		gr_gpc0_tpc0_sm_dbgr_control0_r() + offset, dbgr_control0);
}

void gk20a_resume_all_sms(struct gk20a *g)
{
	u32 dbgr_control0;
	/*
	 * The following requires some clarification. Despite the fact that both
	 * RUN_TRIGGER and STOP_TRIGGER have the word "TRIGGER" in their
	 *  names, only one is actually a trigger, and that is the STOP_TRIGGER.
	 * Merely writing a 1(_TASK) to the RUN_TRIGGER is not sufficient to
	 * resume the gpu - the _STOP_TRIGGER must explicitly be set to 0
	 * (_DISABLE) as well.

	* Advice from the arch group:  Disable the stop trigger first, as a
	* separate operation, in order to ensure that the trigger has taken
	* effect, before enabling the run trigger.
	*/

	/*De-assert stop trigger */
	dbgr_control0 =
		gk20a_readl(g, gr_gpcs_tpcs_sm_dbgr_control0_r());
	dbgr_control0 &= ~gr_gpcs_tpcs_sm_dbgr_control0_stop_trigger_enable_f();
	gk20a_writel(g,
		gr_gpcs_tpcs_sm_dbgr_control0_r(), dbgr_control0);

	/* Run trigger */
	dbgr_control0 |= gr_gpcs_tpcs_sm_dbgr_control0_run_trigger_task_f();
	gk20a_writel(g,
		gr_gpcs_tpcs_sm_dbgr_control0_r(), dbgr_control0);
}

static u32 gr_gk20a_pagepool_default_size(struct gk20a *g)
{
	return gr_scc_pagepool_total_pages_hwmax_value_v();
}

static u32 gr_gk20a_get_max_fbps_count(struct gk20a *g)
{
	u32 max_fbps_count, tmp;
	tmp = gk20a_readl(g, top_num_fbps_r());
	max_fbps_count = top_num_fbps_value_v(tmp);
	return max_fbps_count;
}


static u32 gr_gk20a_get_fbp_en_mask(struct gk20a *g)
{
	u32 fbp_en_mask, opt_fbio;
	opt_fbio = gk20a_readl(g,  top_fs_status_fbp_r());
	fbp_en_mask = top_fs_status_fbp_cluster_v(opt_fbio);
	return fbp_en_mask;
}

static u32 gr_gk20a_get_max_ltc_per_fbp(struct gk20a *g)
{
	return 1;
}

static u32 gr_gk20a_get_max_lts_per_ltc(struct gk20a *g)
{
	return 1;
}

static u32 *gr_gk20a_rop_l2_en_mask(struct gk20a *g)
{
	/* gk20a doesnt have rop_l2_en_mask */
	return NULL;
}



static int gr_gk20a_dump_gr_status_regs(struct gk20a *g,
			   struct gk20a_debug_output *o)
{
	u32 gr_engine_id;

	gr_engine_id = gk20a_fifo_get_gr_engine_id(g);

	gk20a_debug_output(o, "NV_PGRAPH_STATUS: 0x%x\n",
		gk20a_readl(g, gr_status_r()));
	gk20a_debug_output(o, "NV_PGRAPH_STATUS1: 0x%x\n",
		gk20a_readl(g, gr_status_1_r()));
	gk20a_debug_output(o, "NV_PGRAPH_STATUS2: 0x%x\n",
		gk20a_readl(g, gr_status_2_r()));
	gk20a_debug_output(o, "NV_PGRAPH_ENGINE_STATUS: 0x%x\n",
		gk20a_readl(g, gr_engine_status_r()));
	gk20a_debug_output(o, "NV_PGRAPH_GRFIFO_STATUS : 0x%x\n",
		gk20a_readl(g, gr_gpfifo_status_r()));
	gk20a_debug_output(o, "NV_PGRAPH_GRFIFO_CONTROL : 0x%x\n",
		gk20a_readl(g, gr_gpfifo_ctl_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FECS_HOST_INT_STATUS : 0x%x\n",
		gk20a_readl(g, gr_fecs_host_int_status_r()));
	gk20a_debug_output(o, "NV_PGRAPH_EXCEPTION  : 0x%x\n",
		gk20a_readl(g, gr_exception_r()));
	gk20a_debug_output(o, "NV_PGRAPH_FECS_INTR  : 0x%x\n",
		gk20a_readl(g, gr_fecs_intr_r()));
	gk20a_debug_output(o, "NV_PFIFO_ENGINE_STATUS(GR) : 0x%x\n",
		gk20a_readl(g, fifo_engine_status_r(gr_engine_id)));
	gk20a_debug_output(o, "NV_PGRAPH_ACTIVITY0: 0x%x\n",
		gk20a_readl(g, gr_activity_0_r()));
	gk20a_debug_output(o, "NV_PGRAPH_ACTIVITY1: 0x%x\n",
		gk20a_readl(g, gr_activity_1_r()));
	gk20a_debug_output(o, "NV_PGRAPH_ACTIVITY2: 0x%x\n",
		gk20a_readl(g, gr_activity_2_r()));
	gk20a_debug_output(o, "NV_PGRAPH_ACTIVITY4: 0x%x\n",
		gk20a_readl(g, gr_activity_4_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_SKED_ACTIVITY: 0x%x\n",
		gk20a_readl(g, gr_pri_sked_activity_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_GPCCS_GPC_ACTIVITY0: 0x%x\n",
		gk20a_readl(g, gr_pri_gpc0_gpccs_gpc_activity0_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_GPCCS_GPC_ACTIVITY1: 0x%x\n",
		gk20a_readl(g, gr_pri_gpc0_gpccs_gpc_activity1_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_GPCCS_GPC_ACTIVITY2: 0x%x\n",
		gk20a_readl(g, gr_pri_gpc0_gpccs_gpc_activity2_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_GPCCS_GPC_ACTIVITY3: 0x%x\n",
		gk20a_readl(g, gr_pri_gpc0_gpccs_gpc_activity3_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_TPC0_TPCCS_TPC_ACTIVITY0: 0x%x\n",
		gk20a_readl(g, gr_pri_gpc0_tpc0_tpccs_tpc_activity_0_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_TPCS_TPCCS_TPC_ACTIVITY0: 0x%x\n",
		gk20a_readl(g, gr_pri_gpc0_tpcs_tpccs_tpc_activity_0_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPCS_GPCCS_GPC_ACTIVITY0: 0x%x\n",
		gk20a_readl(g, gr_pri_gpcs_gpccs_gpc_activity_0_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPCS_GPCCS_GPC_ACTIVITY1: 0x%x\n",
		gk20a_readl(g, gr_pri_gpcs_gpccs_gpc_activity_1_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPCS_GPCCS_GPC_ACTIVITY2: 0x%x\n",
		gk20a_readl(g, gr_pri_gpcs_gpccs_gpc_activity_2_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPCS_GPCCS_GPC_ACTIVITY3: 0x%x\n",
		gk20a_readl(g, gr_pri_gpcs_gpccs_gpc_activity_3_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPCS_TPC0_TPCCS_TPC_ACTIVITY0: 0x%x\n",
		gk20a_readl(g, gr_pri_gpcs_tpc0_tpccs_tpc_activity_0_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPCS_TPCS_TPCCS_TPC_ACTIVITY0: 0x%x\n",
		gk20a_readl(g, gr_pri_gpcs_tpcs_tpccs_tpc_activity_0_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_BE0_BECS_BE_ACTIVITY0: 0x%x\n",
		gk20a_readl(g, gr_pri_be0_becs_be_activity0_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_BES_BECS_BE_ACTIVITY0: 0x%x\n",
		gk20a_readl(g, gr_pri_bes_becs_be_activity0_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_DS_MPIPE_STATUS: 0x%x\n",
		gk20a_readl(g, gr_pri_ds_mpipe_status_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FE_GO_IDLE_ON_STATUS: 0x%x\n",
		gk20a_readl(g, gr_pri_fe_go_idle_on_status_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FE_GO_IDLE_TIMEOUT : 0x%x\n",
		gk20a_readl(g, gr_fe_go_idle_timeout_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FE_GO_IDLE_CHECK : 0x%x\n",
		gk20a_readl(g, gr_pri_fe_go_idle_check_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FE_GO_IDLE_INFO : 0x%x\n",
		gk20a_readl(g, gr_pri_fe_go_idle_info_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_TPC0_TEX_M_TEX_SUBUNITS_STATUS: 0x%x\n",
		gk20a_readl(g, gr_pri_gpc0_tpc0_tex_m_tex_subunits_status_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FECS_CTXSW_STATUS_FE_0: 0x%x\n",
		gk20a_readl(g, gr_fecs_ctxsw_status_fe_0_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FECS_CTXSW_STATUS_1: 0x%x\n",
		gk20a_readl(g, gr_fecs_ctxsw_status_1_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_GPCCS_CTXSW_STATUS_GPC_0: 0x%x\n",
		gk20a_readl(g, gr_gpc0_gpccs_ctxsw_status_gpc_0_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_GPCCS_CTXSW_STATUS_1: 0x%x\n",
		gk20a_readl(g, gr_gpc0_gpccs_ctxsw_status_1_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FECS_CTXSW_IDLESTATE : 0x%x\n",
		gk20a_readl(g, gr_fecs_ctxsw_idlestate_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_GPCCS_CTXSW_IDLESTATE : 0x%x\n",
		gk20a_readl(g, gr_gpc0_gpccs_ctxsw_idlestate_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FECS_CURRENT_CTX : 0x%x\n",
		gk20a_readl(g, gr_fecs_current_ctx_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FECS_NEW_CTX : 0x%x\n",
		gk20a_readl(g, gr_fecs_new_ctx_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_BE0_CROP_STATUS1 : 0x%x\n",
		gk20a_readl(g, gr_pri_be0_crop_status1_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_BES_CROP_STATUS1 : 0x%x\n",
		gk20a_readl(g, gr_pri_bes_crop_status1_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_BE0_ZROP_STATUS : 0x%x\n",
		gk20a_readl(g, gr_pri_be0_zrop_status_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_BE0_ZROP_STATUS2 : 0x%x\n",
		gk20a_readl(g, gr_pri_be0_zrop_status2_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_BES_ZROP_STATUS : 0x%x\n",
		gk20a_readl(g, gr_pri_bes_zrop_status_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_BES_ZROP_STATUS2 : 0x%x\n",
		gk20a_readl(g, gr_pri_bes_zrop_status2_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_BE0_BECS_BE_EXCEPTION: 0x%x\n",
		gk20a_readl(g, gr_pri_be0_becs_be_exception_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_BE0_BECS_BE_EXCEPTION_EN: 0x%x\n",
		gk20a_readl(g, gr_pri_be0_becs_be_exception_en_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_GPCCS_GPC_EXCEPTION: 0x%x\n",
		gk20a_readl(g, gr_pri_gpc0_gpccs_gpc_exception_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_GPCCS_GPC_EXCEPTION_EN: 0x%x\n",
		gk20a_readl(g, gr_pri_gpc0_gpccs_gpc_exception_en_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_TPC0_TPCCS_TPC_EXCEPTION: 0x%x\n",
		gk20a_readl(g, gr_pri_gpc0_tpc0_tpccs_tpc_exception_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_TPC0_TPCCS_TPC_EXCEPTION_EN: 0x%x\n",
		gk20a_readl(g, gr_pri_gpc0_tpc0_tpccs_tpc_exception_en_r()));
	return 0;
}

#ifdef CONFIG_DEBUG_FS
int gr_gk20a_debugfs_init(struct gk20a *g)
{
	struct gk20a_platform *platform = dev_get_drvdata(g->dev);

	g->debugfs_gr_default_attrib_cb_size =
		debugfs_create_u32("gr_default_attrib_cb_size",
				   S_IRUGO|S_IWUSR, platform->debugfs,
				   &g->gr.attrib_cb_default_size);

	return 0;
}
#endif

static void gr_gk20a_init_cyclestats(struct gk20a *g)
{
#if defined(CONFIG_GK20A_CYCLE_STATS)
	g->gpu_characteristics.flags |=
		NVGPU_GPU_FLAGS_SUPPORT_CYCLE_STATS;
#else
	(void)g;
#endif
}

int gr_gk20a_set_sm_debug_mode(struct gk20a *g,
	struct channel_gk20a *ch, u64 sms, bool enable)
{
	struct nvgpu_dbg_gpu_reg_op *ops;
	int i = 0, sm_id, err;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);

	ops = kcalloc(g->gr.no_of_sm, sizeof(*ops), GFP_KERNEL);
	if (!ops)
		return -ENOMEM;
	for (sm_id = 0; sm_id < g->gr.no_of_sm; sm_id++) {
		int gpc, tpc;
		u32 tpc_offset, gpc_offset, reg_offset, reg_mask, reg_val;

		if (!(sms & (1 << sm_id)))
			continue;

		gpc = g->gr.sm_to_cluster[sm_id].gpc_index;
		tpc = g->gr.sm_to_cluster[sm_id].tpc_index;

		tpc_offset = tpc_in_gpc_stride * tpc;
		gpc_offset = gpc_stride * gpc;
		reg_offset = tpc_offset + gpc_offset;

		ops[i].op = REGOP(WRITE_32);
		ops[i].type = REGOP(TYPE_GR_CTX);
		ops[i].offset  = gr_gpc0_tpc0_sm_dbgr_control0_r() + reg_offset;

		reg_mask = 0;
		reg_val = 0;
		if (enable) {
			reg_mask |= gr_gpc0_tpc0_sm_dbgr_control0_debugger_mode_m();
			reg_val |= gr_gpc0_tpc0_sm_dbgr_control0_debugger_mode_on_f();
			reg_mask |= gr_gpc0_tpc0_sm_dbgr_control0_stop_on_any_warp_m();
			reg_val |= gr_gpc0_tpc0_sm_dbgr_control0_stop_on_any_warp_disable_f();
			reg_mask |= gr_gpc0_tpc0_sm_dbgr_control0_stop_on_any_sm_m();
			reg_val |= gr_gpc0_tpc0_sm_dbgr_control0_stop_on_any_sm_disable_f();
		} else {
			reg_mask |= gr_gpc0_tpc0_sm_dbgr_control0_debugger_mode_m();
			reg_val |= gr_gpc0_tpc0_sm_dbgr_control0_debugger_mode_off_f();
		}

		ops[i].and_n_mask_lo = reg_mask;
		ops[i].value_lo = reg_val;
		i++;
	}

	err = gr_gk20a_exec_ctx_ops(ch, ops, i, i, 0);
	if (err)
		gk20a_err(dev_from_gk20a(g), "Failed to access register\n");
	kfree(ops);
	return err;
}

static void gr_gk20a_bpt_reg_info(struct gk20a *g, struct warpstate *w_state)
{
	/* Check if we have at least one valid warp */
	struct gr_gk20a *gr = &g->gr;
	u32 gpc, tpc, sm_id;
	u32  tpc_offset, gpc_offset, reg_offset;
	u64 warps_valid = 0, warps_paused = 0, warps_trapped = 0;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);

	for (sm_id = 0; sm_id < gr->no_of_sm; sm_id++) {
		gpc = g->gr.sm_to_cluster[sm_id].gpc_index;
		tpc = g->gr.sm_to_cluster[sm_id].tpc_index;

		tpc_offset = tpc_in_gpc_stride * tpc;
		gpc_offset = gpc_stride * gpc;
		reg_offset = tpc_offset + gpc_offset;

		/* 64 bit read */
		warps_valid = (u64)gk20a_readl(g, gr_gpc0_tpc0_sm_warp_valid_mask_r() + reg_offset + 4) << 32;
		warps_valid |= gk20a_readl(g, gr_gpc0_tpc0_sm_warp_valid_mask_r() + reg_offset);


		/* 64 bit read */
		warps_paused = (u64)gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_bpt_pause_mask_r() + reg_offset + 4) << 32;
		warps_paused |= gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_bpt_pause_mask_r() + reg_offset);

		/* 64 bit read */
		warps_trapped = (u64)gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_bpt_trap_mask_r() + reg_offset + 4) << 32;
		warps_trapped |= gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_bpt_trap_mask_r() + reg_offset);

		w_state[sm_id].valid_warps[0] = warps_valid;
		w_state[sm_id].trapped_warps[0] = warps_trapped;
		w_state[sm_id].paused_warps[0] = warps_paused;
	}

	/* Only for debug purpose */
	for (sm_id = 0; sm_id < gr->no_of_sm; sm_id++) {
		gk20a_dbg_fn("w_state[%d].valid_warps[0]: %llx\n",
						sm_id, w_state[sm_id].valid_warps[0]);
		gk20a_dbg_fn("w_state[%d].trapped_warps[0]: %llx\n",
						sm_id, w_state[sm_id].trapped_warps[0]);
		gk20a_dbg_fn("w_state[%d].paused_warps[0]:  %llx\n",
						sm_id, w_state[sm_id].paused_warps[0]);
	}
}

static void gr_gk20a_get_access_map(struct gk20a *g,
				   u32 **whitelist, int *num_entries)
{
	static u32 wl_addr_gk20a[] = {
		/* this list must be sorted (low to high) */
		0x404468, /* gr_pri_mme_max_instructions       */
		0x418800, /* gr_pri_gpcs_setup_debug           */
		0x419a04, /* gr_pri_gpcs_tpcs_tex_lod_dbg      */
		0x419a08, /* gr_pri_gpcs_tpcs_tex_samp_dbg     */
		0x419e10, /* gr_pri_gpcs_tpcs_sm_dbgr_control0 */
		0x419f78, /* gr_pri_gpcs_tpcs_sm_disp_ctrl     */
	};

	*whitelist = wl_addr_gk20a;
	*num_entries = ARRAY_SIZE(wl_addr_gk20a);
}

/*
 * gr_gk20a_suspend_context()
 * This API should be called with dbg_session lock held
 * and ctxsw disabled
 * Returns bool value indicating if context was resident
 * or not
 */
bool gr_gk20a_suspend_context(struct channel_gk20a *ch)
{
	struct gk20a *g = ch->g;
	bool ctx_resident = false;

	if (gk20a_is_channel_ctx_resident(ch)) {
		gk20a_suspend_all_sms(g, 0, false);
		ctx_resident = true;
	} else {
		gk20a_disable_channel_tsg(g, ch);
	}

	return ctx_resident;
}

bool gr_gk20a_resume_context(struct channel_gk20a *ch)
{
	struct gk20a *g = ch->g;
	bool ctx_resident = false;

	if (gk20a_is_channel_ctx_resident(ch)) {
		gk20a_resume_all_sms(g);
		ctx_resident = true;
	} else {
		gk20a_enable_channel_tsg(g, ch);
	}

	return ctx_resident;
}

int gr_gk20a_suspend_contexts(struct gk20a *g,
			      struct dbg_session_gk20a *dbg_s,
			      int *ctx_resident_ch_fd)
{
	int local_ctx_resident_ch_fd = -1;
	bool ctx_resident;
	struct channel_gk20a *ch;
	struct dbg_session_channel_data *ch_data;
	int err = 0;

	mutex_lock(&g->dbg_sessions_lock);

	err = gr_gk20a_disable_ctxsw(g);
	if (err) {
		gk20a_err(dev_from_gk20a(g), "unable to stop gr ctxsw");
		goto clean_up;
	}

	mutex_lock(&dbg_s->ch_list_lock);

	list_for_each_entry(ch_data, &dbg_s->ch_list, ch_entry) {
		ch = g->fifo.channel + ch_data->chid;

		ctx_resident = gr_gk20a_suspend_context(ch);
		if (ctx_resident)
			local_ctx_resident_ch_fd = ch_data->channel_fd;
	}

	mutex_unlock(&dbg_s->ch_list_lock);

	err = gr_gk20a_enable_ctxsw(g);
	if (err)
		gk20a_err(dev_from_gk20a(g), "unable to restart ctxsw!\n");

	*ctx_resident_ch_fd = local_ctx_resident_ch_fd;

clean_up:
	mutex_unlock(&g->dbg_sessions_lock);

	return err;
}

int gr_gk20a_resume_contexts(struct gk20a *g,
			      struct dbg_session_gk20a *dbg_s,
			      int *ctx_resident_ch_fd)
{
	int local_ctx_resident_ch_fd = -1;
	bool ctx_resident;
	struct channel_gk20a *ch;
	int err = 0;
	struct dbg_session_channel_data *ch_data;

	mutex_lock(&g->dbg_sessions_lock);

	err = gr_gk20a_disable_ctxsw(g);
	if (err) {
		gk20a_err(dev_from_gk20a(g), "unable to stop gr ctxsw");
		goto clean_up;
	}

	list_for_each_entry(ch_data, &dbg_s->ch_list, ch_entry) {
		ch = g->fifo.channel + ch_data->chid;

		ctx_resident = gr_gk20a_resume_context(ch);
		if (ctx_resident)
			local_ctx_resident_ch_fd = ch_data->channel_fd;
	}

	err = gr_gk20a_enable_ctxsw(g);
	if (err)
		gk20a_err(dev_from_gk20a(g), "unable to restart ctxsw!\n");

	*ctx_resident_ch_fd = local_ctx_resident_ch_fd;

clean_up:
	mutex_unlock(&g->dbg_sessions_lock);

	return err;
}

static int gr_gk20a_get_preemption_mode_flags(struct gk20a *g,
		struct nvgpu_preemption_modes_rec *preemption_modes_rec)
{
	preemption_modes_rec->graphics_preemption_mode_flags =
			NVGPU_GRAPHICS_PREEMPTION_MODE_WFI;
	preemption_modes_rec->compute_preemption_mode_flags =
			NVGPU_COMPUTE_PREEMPTION_MODE_WFI;

	preemption_modes_rec->default_graphics_preempt_mode =
			NVGPU_GRAPHICS_PREEMPTION_MODE_WFI;
	preemption_modes_rec->default_compute_preempt_mode =
			NVGPU_COMPUTE_PREEMPTION_MODE_WFI;

	return 0;
}

static bool gr_gk20a_is_ltcs_ltss_addr_stub(struct gk20a *g, u32 addr)
{
	return false;
}

static bool gr_gk20a_is_ltcn_ltss_addr_stub(struct gk20a *g, u32 addr)
{
	return false;
}

static void gr_gk20a_split_lts_broadcast_addr_stub(struct gk20a *g, u32 addr,
					u32 *priv_addr_table,
					u32 *priv_addr_table_index)
{
}

static void gr_gk20a_split_ltc_broadcast_addr_stub(struct gk20a *g, u32 addr,
					u32 *priv_addr_table,
					u32 *priv_addr_table_index)
{
}

void gk20a_init_gr_ops(struct gpu_ops *gops)
{
	gops->gr.access_smpc_reg = gr_gk20a_access_smpc_reg;
	gops->gr.bundle_cb_defaults = gr_gk20a_bundle_cb_defaults;
	gops->gr.cb_size_default = gr_gk20a_cb_size_default;
	gops->gr.calc_global_ctx_buffer_size =
		gr_gk20a_calc_global_ctx_buffer_size;
	gops->gr.commit_global_attrib_cb = gr_gk20a_commit_global_attrib_cb;
	gops->gr.commit_global_bundle_cb = gr_gk20a_commit_global_bundle_cb;
	gops->gr.commit_global_cb_manager = gr_gk20a_commit_global_cb_manager;
	gops->gr.commit_global_pagepool = gr_gk20a_commit_global_pagepool;
	gops->gr.handle_sw_method = gr_gk20a_handle_sw_method;
	gops->gr.set_alpha_circular_buffer_size =
		gk20a_gr_set_circular_buffer_size;
	gops->gr.set_circular_buffer_size =
		gk20a_gr_set_alpha_circular_buffer_size;
	gops->gr.enable_hww_exceptions = gr_gk20a_enable_hww_exceptions;
	gops->gr.is_valid_class = gr_gk20a_is_valid_class;
	gops->gr.get_sm_dsm_perf_regs = gr_gk20a_get_sm_dsm_perf_regs;
	gops->gr.get_sm_dsm_perf_ctrl_regs = gr_gk20a_get_sm_dsm_perf_ctrl_regs;
	gops->gr.init_fs_state = gr_gk20a_init_fs_state;
	gops->gr.set_hww_esr_report_mask = gr_gk20a_set_hww_esr_report_mask;
	gops->gr.setup_alpha_beta_tables = gr_gk20a_setup_alpha_beta_tables;
	gops->gr.falcon_load_ucode = gr_gk20a_load_ctxsw_ucode_segments;
	gops->gr.load_ctxsw_ucode = gr_gk20a_load_ctxsw_ucode;
	gops->gr.get_gpc_tpc_mask = gr_gk20a_get_gpc_tpc_mask;
	gops->gr.free_channel_ctx = gk20a_free_channel_ctx;
	gops->gr.alloc_obj_ctx = gk20a_alloc_obj_ctx;
	gops->gr.bind_ctxsw_zcull = gr_gk20a_bind_ctxsw_zcull;
	gops->gr.get_zcull_info = gr_gk20a_get_zcull_info;
	gops->gr.is_tpc_addr = gr_gk20a_is_tpc_addr;
	gops->gr.get_tpc_num = gr_gk20a_get_tpc_num;
	gops->gr.detect_sm_arch = gr_gk20a_detect_sm_arch;
	gops->gr.add_zbc_color = gr_gk20a_add_zbc_color;
	gops->gr.add_zbc_depth = gr_gk20a_add_zbc_depth;
	gops->gr.zbc_set_table = gk20a_gr_zbc_set_table;
	gops->gr.zbc_query_table = gr_gk20a_query_zbc;
	gops->gr.pmu_save_zbc = gr_gk20a_pmu_save_zbc;
	gops->gr.add_zbc = _gk20a_gr_zbc_set_table;
	gops->gr.pagepool_default_size = gr_gk20a_pagepool_default_size;
	gops->gr.init_ctx_state = gr_gk20a_init_ctx_state;
	gops->gr.alloc_gr_ctx = gr_gk20a_alloc_gr_ctx;
	gops->gr.free_gr_ctx = gr_gk20a_free_gr_ctx;
	gops->gr.dump_gr_regs = gr_gk20a_dump_gr_status_regs;
	gops->gr.get_max_fbps_count = gr_gk20a_get_max_fbps_count;
	gops->gr.get_fbp_en_mask = gr_gk20a_get_fbp_en_mask;
	gops->gr.get_max_ltc_per_fbp = gr_gk20a_get_max_ltc_per_fbp;
	gops->gr.get_max_lts_per_ltc = gr_gk20a_get_max_lts_per_ltc;
	gops->gr.get_rop_l2_en_mask = gr_gk20a_rop_l2_en_mask;
	gops->gr.init_sm_dsm_reg_info = gr_gk20a_init_sm_dsm_reg_info;
	gops->gr.wait_empty = gr_gk20a_wait_idle;
	gops->gr.init_cyclestats = gr_gk20a_init_cyclestats;
	gops->gr.set_sm_debug_mode = gr_gk20a_set_sm_debug_mode;
	gops->gr.bpt_reg_info = gr_gk20a_bpt_reg_info;
	gops->gr.get_access_map = gr_gk20a_get_access_map;
	gops->gr.handle_fecs_error = gk20a_gr_handle_fecs_error;
	gops->gr.mask_hww_warp_esr = gk20a_mask_hww_warp_esr;
	gops->gr.handle_sm_exception = gr_gk20a_handle_sm_exception;
	gops->gr.handle_tex_exception = gr_gk20a_handle_tex_exception;
	gops->gr.get_lrf_tex_ltc_dram_override = NULL;
	gops->gr.update_smpc_ctxsw_mode = gr_gk20a_update_smpc_ctxsw_mode;
	gops->gr.update_hwpm_ctxsw_mode = gr_gk20a_update_hwpm_ctxsw_mode;
	gops->gr.record_sm_error_state = gk20a_gr_record_sm_error_state;
	gops->gr.update_sm_error_state = gk20a_gr_update_sm_error_state;
	gops->gr.clear_sm_error_state = gk20a_gr_clear_sm_error_state;
	gops->gr.suspend_contexts = gr_gk20a_suspend_contexts;
	gops->gr.get_preemption_mode_flags = gr_gk20a_get_preemption_mode_flags;
	gops->gr.program_active_tpc_counts = gr_gk20a_program_active_tpc_counts;
	gops->gr.program_sm_id_numbering = gr_gk20a_program_sm_id_numbering;
	gops->gr.is_ltcs_ltss_addr = gr_gk20a_is_ltcs_ltss_addr_stub;
	gops->gr.is_ltcn_ltss_addr = gr_gk20a_is_ltcn_ltss_addr_stub;
	gops->gr.split_lts_broadcast_addr =
					gr_gk20a_split_lts_broadcast_addr_stub;
	gops->gr.split_ltc_broadcast_addr =
					gr_gk20a_split_ltc_broadcast_addr_stub;
}
