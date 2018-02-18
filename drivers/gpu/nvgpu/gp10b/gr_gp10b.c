/*
 * GP10B GPU GR
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

#include "gk20a/gk20a.h" /* FERMI and MAXWELL classes defined here */
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/tegra-fuse.h>

#include <dt-bindings/soc/gp10b-fuse.h>

#include "gk20a/gr_gk20a.h"
#include "gk20a/semaphore_gk20a.h"
#include "gk20a/dbg_gpu_gk20a.h"

#include "gm20b/gr_gm20b.h" /* for MAXWELL classes */
#include "gp10b/gr_gp10b.h"
#include "hw_gr_gp10b.h"
#include "hw_fifo_gp10b.h"
#include "hw_ctxsw_prog_gp10b.h"
#include "hw_mc_gp10b.h"
#include "gp10b_sysfs.h"
#include <linux/vmalloc.h>

#define NVGPU_GFXP_WFI_TIMEOUT_US	100LL

static bool gr_gp10b_is_valid_class(struct gk20a *g, u32 class_num)
{
	bool valid = false;

	switch (class_num) {
	case PASCAL_COMPUTE_A:
	case PASCAL_A:
	case PASCAL_DMA_COPY_A:
		valid = true;
		break;

	case MAXWELL_COMPUTE_B:
	case MAXWELL_B:
	case FERMI_TWOD_A:
	case KEPLER_DMA_COPY_A:
	case MAXWELL_DMA_COPY_A:
		valid = true;
		break;

	default:
		break;
	}
	gk20a_dbg_info("class=0x%x valid=%d", class_num, valid);
	return valid;
}

static void gr_gp10b_sm_lrf_ecc_overcount_war(int single_err,
						u32 sed_status,
						u32 ded_status,
						u32 *count_to_adjust,
						u32 opposite_count)
{
	u32 over_count = 0;

	sed_status >>= gr_pri_gpc0_tpc0_sm_lrf_ecc_status_single_err_detected_qrfdp0_b();
	ded_status >>= gr_pri_gpc0_tpc0_sm_lrf_ecc_status_double_err_detected_qrfdp0_b();

	/* One overcount for each partition on which a SBE occurred but not a
	   DBE (or vice-versa) */
	if (single_err) {
		over_count =
			hweight32(sed_status & ~ded_status);
	} else {
		over_count =
			hweight32(ded_status & ~sed_status);
	}

	/* If both a SBE and a DBE occur on the same partition, then we have an
	   overcount for the subpartition if the opposite error counts are
	   zero. */
	if ((sed_status & ded_status) && (opposite_count == 0)) {
		over_count +=
			hweight32(sed_status & ded_status);
	}

	if (*count_to_adjust > over_count)
		*count_to_adjust -= over_count;
	else
		*count_to_adjust = 0;
}

static int gr_gp10b_handle_sm_exception(struct gk20a *g, u32 gpc, u32 tpc,
			bool *post_event, struct channel_gk20a *fault_ch,
			u32 *hww_global_esr)
{
	int ret = 0;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 offset = gpc_stride * gpc + tpc_in_gpc_stride * tpc;
	u32 lrf_ecc_status, lrf_ecc_sed_status, lrf_ecc_ded_status;
	u32 lrf_single_count_delta, lrf_double_count_delta;
	u32 shm_ecc_status;

	gr_gk20a_handle_sm_exception(g, gpc, tpc, post_event, fault_ch, hww_global_esr);

	/* Check for LRF ECC errors. */
        lrf_ecc_status = gk20a_readl(g,
			gr_pri_gpc0_tpc0_sm_lrf_ecc_status_r() + offset);
	lrf_ecc_sed_status = lrf_ecc_status &
				(gr_pri_gpc0_tpc0_sm_lrf_ecc_status_single_err_detected_qrfdp0_pending_f() |
				 gr_pri_gpc0_tpc0_sm_lrf_ecc_status_single_err_detected_qrfdp1_pending_f() |
				 gr_pri_gpc0_tpc0_sm_lrf_ecc_status_single_err_detected_qrfdp2_pending_f() |
				 gr_pri_gpc0_tpc0_sm_lrf_ecc_status_single_err_detected_qrfdp3_pending_f());
	lrf_ecc_ded_status = lrf_ecc_status &
				(gr_pri_gpc0_tpc0_sm_lrf_ecc_status_double_err_detected_qrfdp0_pending_f() |
				 gr_pri_gpc0_tpc0_sm_lrf_ecc_status_double_err_detected_qrfdp1_pending_f() |
				 gr_pri_gpc0_tpc0_sm_lrf_ecc_status_double_err_detected_qrfdp2_pending_f() |
				 gr_pri_gpc0_tpc0_sm_lrf_ecc_status_double_err_detected_qrfdp3_pending_f());
	lrf_single_count_delta =
		gk20a_readl(g,
			gr_pri_gpc0_tpc0_sm_lrf_ecc_single_err_count_r() +
			offset);
	lrf_double_count_delta =
		gk20a_readl(g,
			gr_pri_gpc0_tpc0_sm_lrf_ecc_double_err_count_r() +
			offset);
	gk20a_writel(g,
		gr_pri_gpc0_tpc0_sm_lrf_ecc_single_err_count_r() + offset,
		0);
	gk20a_writel(g,
		gr_pri_gpc0_tpc0_sm_lrf_ecc_double_err_count_r() + offset,
		0);
	if (lrf_ecc_sed_status) {
		gk20a_dbg(gpu_dbg_fn | gpu_dbg_intr,
			"Single bit error detected in SM LRF!");

		gr_gp10b_sm_lrf_ecc_overcount_war(1,
						lrf_ecc_sed_status,
						lrf_ecc_ded_status,
						&lrf_single_count_delta,
						lrf_double_count_delta);
		g->gr.t18x.ecc_stats.sm_lrf_single_err_count.counters[tpc] +=
							lrf_single_count_delta;
	}
	if (lrf_ecc_ded_status) {
		gk20a_dbg(gpu_dbg_fn | gpu_dbg_intr,
			"Double bit error detected in SM LRF!");

		gr_gp10b_sm_lrf_ecc_overcount_war(0,
						lrf_ecc_sed_status,
						lrf_ecc_ded_status,
						&lrf_double_count_delta,
						lrf_single_count_delta);
		g->gr.t18x.ecc_stats.sm_lrf_double_err_count.counters[tpc] +=
							lrf_double_count_delta;
	}
	gk20a_writel(g, gr_pri_gpc0_tpc0_sm_lrf_ecc_status_r() + offset,
			lrf_ecc_status);

	/* Check for SHM ECC errors. */
        shm_ecc_status = gk20a_readl(g,
			gr_pri_gpc0_tpc0_sm_shm_ecc_status_r() + offset);
	if ((shm_ecc_status &
		gr_pri_gpc0_tpc0_sm_shm_ecc_status_single_err_corrected_shm0_pending_f()) ||
		(shm_ecc_status &
		gr_pri_gpc0_tpc0_sm_shm_ecc_status_single_err_corrected_shm1_pending_f()) ||
		(shm_ecc_status &
		gr_pri_gpc0_tpc0_sm_shm_ecc_status_single_err_detected_shm0_pending_f()) ||
		(shm_ecc_status &
		gr_pri_gpc0_tpc0_sm_shm_ecc_status_single_err_detected_shm1_pending_f()) ) {
		u32 ecc_stats_reg_val;

		gk20a_dbg(gpu_dbg_fn | gpu_dbg_intr,
			"Single bit error detected in SM SHM!");

		ecc_stats_reg_val =
			gk20a_readl(g,
				gr_pri_gpc0_tpc0_sm_shm_ecc_err_count_r() + offset);
		g->gr.t18x.ecc_stats.sm_shm_sec_count.counters[tpc] +=
			gr_pri_gpc0_tpc0_sm_shm_ecc_err_count_single_corrected_v(ecc_stats_reg_val);
		g->gr.t18x.ecc_stats.sm_shm_sed_count.counters[tpc] +=
			gr_pri_gpc0_tpc0_sm_shm_ecc_err_count_single_detected_v(ecc_stats_reg_val);
		ecc_stats_reg_val &= ~(gr_pri_gpc0_tpc0_sm_shm_ecc_err_count_single_corrected_m() |
					gr_pri_gpc0_tpc0_sm_shm_ecc_err_count_single_detected_m());
		gk20a_writel(g,
			gr_pri_gpc0_tpc0_sm_shm_ecc_err_count_r() + offset,
			ecc_stats_reg_val);
	}
	if ( (shm_ecc_status &
		gr_pri_gpc0_tpc0_sm_shm_ecc_status_double_err_detected_shm0_pending_f()) ||
		(shm_ecc_status &
		gr_pri_gpc0_tpc0_sm_shm_ecc_status_double_err_detected_shm1_pending_f()) ) {
		u32 ecc_stats_reg_val;

		gk20a_dbg(gpu_dbg_fn | gpu_dbg_intr,
			"Double bit error detected in SM SHM!");

		ecc_stats_reg_val =
			gk20a_readl(g,
				gr_pri_gpc0_tpc0_sm_shm_ecc_err_count_r() + offset);
		g->gr.t18x.ecc_stats.sm_shm_ded_count.counters[tpc] +=
			gr_pri_gpc0_tpc0_sm_shm_ecc_err_count_double_detected_v(ecc_stats_reg_val);
		ecc_stats_reg_val &= ~(gr_pri_gpc0_tpc0_sm_shm_ecc_err_count_double_detected_m());
		gk20a_writel(g,
			gr_pri_gpc0_tpc0_sm_shm_ecc_err_count_r() + offset,
			ecc_stats_reg_val);
	}
	gk20a_writel(g, gr_pri_gpc0_tpc0_sm_shm_ecc_status_r() + offset,
			shm_ecc_status);


	return ret;
}

static int gr_gp10b_handle_tex_exception(struct gk20a *g, u32 gpc, u32 tpc,
		bool *post_event)
{
	int ret = 0;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 offset = gpc_stride * gpc + tpc_in_gpc_stride * tpc;
	u32 esr;
	u32 ecc_stats_reg_val;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg, "");

	esr = gk20a_readl(g,
			 gr_gpc0_tpc0_tex_m_hww_esr_r() + offset);
	gk20a_dbg(gpu_dbg_intr | gpu_dbg_gpu_dbg, "0x%08x", esr);

	if (esr & gr_gpc0_tpc0_tex_m_hww_esr_ecc_sec_pending_f()) {
		gk20a_dbg(gpu_dbg_fn | gpu_dbg_intr,
			"Single bit error detected in TEX!");

		/* Pipe 0 counters */
		gk20a_writel(g,
			gr_pri_gpc0_tpc0_tex_m_routing_r() + offset,
			gr_pri_gpc0_tpc0_tex_m_routing_sel_pipe0_f());

		ecc_stats_reg_val = gk20a_readl(g,
				gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_r() + offset);
		g->gr.t18x.ecc_stats.tex_total_sec_pipe0_count.counters[tpc] +=
				gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_sec_v(ecc_stats_reg_val);
		ecc_stats_reg_val &= ~gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_sec_m();
		gk20a_writel(g,
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_r() + offset,
			ecc_stats_reg_val);

		ecc_stats_reg_val = gk20a_readl(g,
				gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_r() + offset);
		g->gr.t18x.ecc_stats.tex_unique_sec_pipe0_count.counters[tpc] +=
				gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_sec_v(ecc_stats_reg_val);
		ecc_stats_reg_val &= ~gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_sec_m();
		gk20a_writel(g,
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_r() + offset,
			ecc_stats_reg_val);


		/* Pipe 1 counters */
		gk20a_writel(g,
			gr_pri_gpc0_tpc0_tex_m_routing_r() + offset,
			gr_pri_gpc0_tpc0_tex_m_routing_sel_pipe1_f());

		ecc_stats_reg_val = gk20a_readl(g,
				gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_r() + offset);
		g->gr.t18x.ecc_stats.tex_total_sec_pipe1_count.counters[tpc] +=
				gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_sec_v(ecc_stats_reg_val);
		ecc_stats_reg_val &= ~gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_sec_m();
		gk20a_writel(g,
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_r() + offset,
			ecc_stats_reg_val);

		ecc_stats_reg_val = gk20a_readl(g,
				gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_r() + offset);
		g->gr.t18x.ecc_stats.tex_unique_sec_pipe1_count.counters[tpc] +=
				gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_sec_v(ecc_stats_reg_val);
		ecc_stats_reg_val &= ~gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_sec_m();
		gk20a_writel(g,
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_r() + offset,
			ecc_stats_reg_val);


		gk20a_writel(g,
			gr_pri_gpc0_tpc0_tex_m_routing_r() + offset,
			gr_pri_gpc0_tpc0_tex_m_routing_sel_default_f());
	}
	if (esr & gr_gpc0_tpc0_tex_m_hww_esr_ecc_ded_pending_f()) {
		gk20a_dbg(gpu_dbg_fn | gpu_dbg_intr,
			"Double bit error detected in TEX!");

		/* Pipe 0 counters */
		gk20a_writel(g,
			gr_pri_gpc0_tpc0_tex_m_routing_r() + offset,
			gr_pri_gpc0_tpc0_tex_m_routing_sel_pipe0_f());

		ecc_stats_reg_val = gk20a_readl(g,
				gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_r() + offset);
		g->gr.t18x.ecc_stats.tex_total_ded_pipe0_count.counters[tpc] +=
				gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_ded_v(ecc_stats_reg_val);
		ecc_stats_reg_val &= ~gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_ded_m();
		gk20a_writel(g,
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_r() + offset,
			ecc_stats_reg_val);

		ecc_stats_reg_val = gk20a_readl(g,
				gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_r() + offset);
		g->gr.t18x.ecc_stats.tex_unique_ded_pipe0_count.counters[tpc] +=
				gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_ded_v(ecc_stats_reg_val);
		ecc_stats_reg_val &= ~gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_ded_m();
		gk20a_writel(g,
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_r() + offset,
			ecc_stats_reg_val);


		/* Pipe 1 counters */
		gk20a_writel(g,
			gr_pri_gpc0_tpc0_tex_m_routing_r() + offset,
			gr_pri_gpc0_tpc0_tex_m_routing_sel_pipe1_f());

		ecc_stats_reg_val = gk20a_readl(g,
				gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_r() + offset);
		g->gr.t18x.ecc_stats.tex_total_ded_pipe1_count.counters[tpc] +=
				gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_ded_v(ecc_stats_reg_val);
		ecc_stats_reg_val &= ~gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_ded_m();
		gk20a_writel(g,
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_r() + offset,
			ecc_stats_reg_val);

		ecc_stats_reg_val = gk20a_readl(g,
				gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_r() + offset);
		g->gr.t18x.ecc_stats.tex_unique_ded_pipe1_count.counters[tpc] +=
				gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_ded_v(ecc_stats_reg_val);
		ecc_stats_reg_val &= ~gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_ded_m();
		gk20a_writel(g,
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_r() + offset,
			ecc_stats_reg_val);


		gk20a_writel(g,
			gr_pri_gpc0_tpc0_tex_m_routing_r() + offset,
			gr_pri_gpc0_tpc0_tex_m_routing_sel_default_f());
	}

	gk20a_writel(g,
		     gr_gpc0_tpc0_tex_m_hww_esr_r() + offset,
		     esr | gr_gpc0_tpc0_tex_m_hww_esr_reset_active_f());

	return ret;
}

static int gr_gp10b_commit_global_cb_manager(struct gk20a *g,
			struct channel_gk20a *c, bool patch)
{
	struct gr_gk20a *gr = &g->gr;
	struct channel_ctx_gk20a *ch_ctx = &c->ch_ctx;
	struct gr_ctx_desc *gr_ctx = ch_ctx->gr_ctx;
	u32 attrib_offset_in_chunk = 0;
	u32 alpha_offset_in_chunk = 0;
	u32 pd_ab_max_output;
	u32 gpc_index, ppc_index;
	u32 temp, temp2;
	u32 cbm_cfg_size_beta, cbm_cfg_size_alpha, cbm_cfg_size_steadystate;
	u32 attrib_size_in_chunk, cb_attrib_cache_size_init;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 ppc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_PPC_IN_GPC_STRIDE);
	u32 num_pes_per_gpc = nvgpu_get_litter_value(g, GPU_LIT_NUM_PES_PER_GPC);

	gk20a_dbg_fn("");

	if (gr_ctx->graphics_preempt_mode == NVGPU_GRAPHICS_PREEMPTION_MODE_GFXP) {
		attrib_size_in_chunk = gr->attrib_cb_default_size +
				  (gr_gpc0_ppc0_cbm_beta_cb_size_v_gfxp_v() -
				   gr_gpc0_ppc0_cbm_beta_cb_size_v_default_v());
		cb_attrib_cache_size_init = gr->attrib_cb_default_size +
				  (gr_gpc0_ppc0_cbm_beta_cb_size_v_gfxp_v() -
				   gr_gpc0_ppc0_cbm_beta_cb_size_v_default_v());
	} else {
		attrib_size_in_chunk = gr->attrib_cb_size;
		cb_attrib_cache_size_init = gr->attrib_cb_default_size;
	}

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_ds_tga_constraintlogic_beta_r(),
		gr->attrib_cb_default_size, patch);
	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_ds_tga_constraintlogic_alpha_r(),
		gr->alpha_cb_default_size, patch);

	pd_ab_max_output = (gr->alpha_cb_default_size *
		gr_gpc0_ppc0_cbm_beta_cb_size_v_granularity_v()) /
		gr_pd_ab_dist_cfg1_max_output_granularity_v();

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_pd_ab_dist_cfg1_r(),
		gr_pd_ab_dist_cfg1_max_output_f(pd_ab_max_output) |
		gr_pd_ab_dist_cfg1_max_batches_init_f(), patch);

	attrib_offset_in_chunk = alpha_offset_in_chunk +
		gr->tpc_count * gr->alpha_cb_size;

	for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++) {
		temp = gpc_stride * gpc_index;
		temp2 = num_pes_per_gpc * gpc_index;
		for (ppc_index = 0; ppc_index < gr->gpc_ppc_count[gpc_index];
		     ppc_index++) {
			cbm_cfg_size_beta = cb_attrib_cache_size_init *
				gr->pes_tpc_count[ppc_index][gpc_index];
			cbm_cfg_size_alpha = gr->alpha_cb_default_size *
				gr->pes_tpc_count[ppc_index][gpc_index];
			cbm_cfg_size_steadystate = gr->attrib_cb_default_size *
				gr->pes_tpc_count[ppc_index][gpc_index];

			gr_gk20a_ctx_patch_write(g, ch_ctx,
				gr_gpc0_ppc0_cbm_beta_cb_size_r() + temp +
				ppc_in_gpc_stride * ppc_index,
				cbm_cfg_size_beta, patch);

			gr_gk20a_ctx_patch_write(g, ch_ctx,
				gr_gpc0_ppc0_cbm_beta_cb_offset_r() + temp +
				ppc_in_gpc_stride * ppc_index,
				attrib_offset_in_chunk, patch);

			gr_gk20a_ctx_patch_write(g, ch_ctx,
				gr_gpc0_ppc0_cbm_beta_steady_state_cb_size_r() + temp +
				ppc_in_gpc_stride * ppc_index,
				cbm_cfg_size_steadystate,
				patch);

			attrib_offset_in_chunk += attrib_size_in_chunk *
				gr->pes_tpc_count[ppc_index][gpc_index];

			gr_gk20a_ctx_patch_write(g, ch_ctx,
				gr_gpc0_ppc0_cbm_alpha_cb_size_r() + temp +
				ppc_in_gpc_stride * ppc_index,
				cbm_cfg_size_alpha, patch);

			gr_gk20a_ctx_patch_write(g, ch_ctx,
				gr_gpc0_ppc0_cbm_alpha_cb_offset_r() + temp +
				ppc_in_gpc_stride * ppc_index,
				alpha_offset_in_chunk, patch);

			alpha_offset_in_chunk += gr->alpha_cb_size *
				gr->pes_tpc_count[ppc_index][gpc_index];

			gr_gk20a_ctx_patch_write(g, ch_ctx,
				gr_gpcs_swdx_tc_beta_cb_size_r(ppc_index + temp2),
				gr_gpcs_swdx_tc_beta_cb_size_v_f(cbm_cfg_size_steadystate),
				patch);
		}
	}

	return 0;
}

static void gr_gp10b_commit_global_pagepool(struct gk20a *g,
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
}

static int gr_gp10b_add_zbc_color(struct gk20a *g, struct gr_gk20a *gr,
				  struct zbc_entry *color_val, u32 index)
{
	u32 i;
	u32 zbc_c;

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

	gk20a_writel_check(g, gr_gpcs_swdx_dss_zbc_color_r_r(index),
			   color_val->color_ds[0]);
	gk20a_writel_check(g, gr_gpcs_swdx_dss_zbc_color_g_r(index),
			   color_val->color_ds[1]);
	gk20a_writel_check(g, gr_gpcs_swdx_dss_zbc_color_b_r(index),
			   color_val->color_ds[2]);
	gk20a_writel_check(g, gr_gpcs_swdx_dss_zbc_color_a_r(index),
			   color_val->color_ds[3]);
	zbc_c = gk20a_readl(g, gr_gpcs_swdx_dss_zbc_c_01_to_04_format_r() + (index & ~3));
	zbc_c &= ~(0x7f << ((index % 4) * 7));
	zbc_c |= color_val->format << ((index % 4) * 7);
	gk20a_writel_check(g, gr_gpcs_swdx_dss_zbc_c_01_to_04_format_r() + (index & ~3), zbc_c);

	return 0;
}

static int gr_gp10b_add_zbc_depth(struct gk20a *g, struct gr_gk20a *gr,
				struct zbc_entry *depth_val, u32 index)
{
	u32 zbc_z;

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

	gk20a_writel(g, gr_gpcs_swdx_dss_zbc_z_r(index), depth_val->depth);
	zbc_z = gk20a_readl(g, gr_gpcs_swdx_dss_zbc_z_01_to_04_format_r() + (index & ~3));
	zbc_z &= ~(0x7f << (index % 4) * 7);
	zbc_z |= depth_val->format << (index % 4) * 7;
	gk20a_writel(g, gr_gpcs_swdx_dss_zbc_z_01_to_04_format_r() + (index & ~3), zbc_z);

	return 0;
}

static u32 gr_gp10b_pagepool_default_size(struct gk20a *g)
{
	return gr_scc_pagepool_total_pages_hwmax_value_v();
}

static int gr_gp10b_calc_global_ctx_buffer_size(struct gk20a *g)
{
	struct gr_gk20a *gr = &g->gr;
	int size;

	gr->attrib_cb_size = gr->attrib_cb_default_size;
	gr->alpha_cb_size = gr->alpha_cb_default_size;

	gr->attrib_cb_size = min(gr->attrib_cb_size,
		 gr_gpc0_ppc0_cbm_beta_cb_size_v_f(~0) / g->gr.tpc_count);
	gr->alpha_cb_size = min(gr->alpha_cb_size,
		 gr_gpc0_ppc0_cbm_alpha_cb_size_v_f(~0) / g->gr.tpc_count);

	size = gr->attrib_cb_size *
		gr_gpc0_ppc0_cbm_beta_cb_size_v_granularity_v() *
		gr->max_tpc_count;

	size += gr->alpha_cb_size *
		gr_gpc0_ppc0_cbm_alpha_cb_size_v_granularity_v() *
		gr->max_tpc_count;

	size = ALIGN(size, 128);

	return size;
}

static void gr_gp10b_set_go_idle_timeout(struct gk20a *g, u32 data)
{
	gk20a_writel(g, gr_fe_go_idle_timeout_r(), data);
}

static void gr_gp10b_set_coalesce_buffer_size(struct gk20a *g, u32 data)
{
	u32 val;

	gk20a_dbg_fn("");

	val = gk20a_readl(g, gr_gpcs_tc_debug0_r());
	val = set_field(val, gr_gpcs_tc_debug0_limit_coalesce_buffer_size_m(),
			     gr_gpcs_tc_debug0_limit_coalesce_buffer_size_f(data));
	gk20a_writel(g, gr_gpcs_tc_debug0_r(), val);

	gk20a_dbg_fn("done");
}

static int gr_gp10b_handle_sw_method(struct gk20a *g, u32 addr,
				     u32 class_num, u32 offset, u32 data)
{
	gk20a_dbg_fn("");

	if (class_num == PASCAL_COMPUTE_A) {
		switch (offset << 2) {
		case NVC0C0_SET_SHADER_EXCEPTIONS:
			gk20a_gr_set_shader_exceptions(g, data);
			break;
		default:
			goto fail;
		}
	}

	if (class_num == PASCAL_A) {
		switch (offset << 2) {
		case NVC097_SET_SHADER_EXCEPTIONS:
			gk20a_gr_set_shader_exceptions(g, data);
			break;
		case NVC097_SET_CIRCULAR_BUFFER_SIZE:
			g->ops.gr.set_circular_buffer_size(g, data);
			break;
		case NVC097_SET_ALPHA_CIRCULAR_BUFFER_SIZE:
			g->ops.gr.set_alpha_circular_buffer_size(g, data);
			break;
		case NVC097_SET_GO_IDLE_TIMEOUT:
			gr_gp10b_set_go_idle_timeout(g, data);
			break;
		case NVC097_SET_COALESCE_BUFFER_SIZE:
			gr_gp10b_set_coalesce_buffer_size(g, data);
			break;
		default:
			goto fail;
		}
	}
	return 0;

fail:
	return -EINVAL;
}

static void gr_gp10b_cb_size_default(struct gk20a *g)
{
	struct gr_gk20a *gr = &g->gr;

	if (!gr->attrib_cb_default_size)
		gr->attrib_cb_default_size = 0x800;
	gr->alpha_cb_default_size =
		gr_gpc0_ppc0_cbm_alpha_cb_size_v_default_v();
}

static void gr_gp10b_set_alpha_circular_buffer_size(struct gk20a *g, u32 data)
{
	struct gr_gk20a *gr = &g->gr;
	u32 gpc_index, ppc_index, stride, val;
	u32 pd_ab_max_output;
	u32 alpha_cb_size = data * 4;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 ppc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_PPC_IN_GPC_STRIDE);

	gk20a_dbg_fn("");

	if (alpha_cb_size > gr->alpha_cb_size)
		alpha_cb_size = gr->alpha_cb_size;

	gk20a_writel(g, gr_ds_tga_constraintlogic_alpha_r(),
		(gk20a_readl(g, gr_ds_tga_constraintlogic_alpha_r()) &
		 ~gr_ds_tga_constraintlogic_alpha_cbsize_f(~0)) |
		 gr_ds_tga_constraintlogic_alpha_cbsize_f(alpha_cb_size));

	pd_ab_max_output = alpha_cb_size *
		gr_gpc0_ppc0_cbm_alpha_cb_size_v_granularity_v() /
		gr_pd_ab_dist_cfg1_max_output_granularity_v();

	gk20a_writel(g, gr_pd_ab_dist_cfg1_r(),
		gr_pd_ab_dist_cfg1_max_output_f(pd_ab_max_output) |
		gr_pd_ab_dist_cfg1_max_batches_init_f());

	for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++) {
		stride = gpc_stride * gpc_index;

		for (ppc_index = 0; ppc_index < gr->gpc_ppc_count[gpc_index];
			ppc_index++) {

			val = gk20a_readl(g, gr_gpc0_ppc0_cbm_alpha_cb_size_r() +
				stride +
				ppc_in_gpc_stride * ppc_index);

			val = set_field(val, gr_gpc0_ppc0_cbm_alpha_cb_size_v_m(),
					gr_gpc0_ppc0_cbm_alpha_cb_size_v_f(alpha_cb_size *
						gr->pes_tpc_count[ppc_index][gpc_index]));

			gk20a_writel(g, gr_gpc0_ppc0_cbm_alpha_cb_size_r() +
				stride +
				ppc_in_gpc_stride * ppc_index, val);
		}
	}
}

static void gr_gp10b_set_circular_buffer_size(struct gk20a *g, u32 data)
{
	struct gr_gk20a *gr = &g->gr;
	u32 gpc_index, ppc_index, stride, val;
	u32 cb_size_steady = data * 4, cb_size;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 ppc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_PPC_IN_GPC_STRIDE);

	gk20a_dbg_fn("");

	if (cb_size_steady > gr->attrib_cb_size)
		cb_size_steady = gr->attrib_cb_size;
	if (gk20a_readl(g, gr_gpc0_ppc0_cbm_beta_cb_size_r()) !=
		gk20a_readl(g,
			gr_gpc0_ppc0_cbm_beta_steady_state_cb_size_r())) {
		cb_size = cb_size_steady +
			(gr_gpc0_ppc0_cbm_beta_cb_size_v_gfxp_v() -
			 gr_gpc0_ppc0_cbm_beta_cb_size_v_default_v());
	} else {
		cb_size = cb_size_steady;
	}

	gk20a_writel(g, gr_ds_tga_constraintlogic_beta_r(),
		(gk20a_readl(g, gr_ds_tga_constraintlogic_beta_r()) &
		 ~gr_ds_tga_constraintlogic_beta_cbsize_f(~0)) |
		 gr_ds_tga_constraintlogic_beta_cbsize_f(cb_size_steady));

	for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++) {
		stride = gpc_stride * gpc_index;

		for (ppc_index = 0; ppc_index < gr->gpc_ppc_count[gpc_index];
			ppc_index++) {

			val = gk20a_readl(g, gr_gpc0_ppc0_cbm_beta_cb_size_r() +
				stride +
				ppc_in_gpc_stride * ppc_index);

			val = set_field(val,
				gr_gpc0_ppc0_cbm_beta_cb_size_v_m(),
				gr_gpc0_ppc0_cbm_beta_cb_size_v_f(cb_size *
					gr->pes_tpc_count[ppc_index][gpc_index]));

			gk20a_writel(g, gr_gpc0_ppc0_cbm_beta_cb_size_r() +
				stride +
				ppc_in_gpc_stride * ppc_index, val);

			gk20a_writel(g, ppc_in_gpc_stride * ppc_index +
				gr_gpc0_ppc0_cbm_beta_steady_state_cb_size_r() +
				stride,
				gr_gpc0_ppc0_cbm_beta_steady_state_cb_size_v_f(
					cb_size_steady));

			val = gk20a_readl(g, gr_gpcs_swdx_tc_beta_cb_size_r(
						ppc_index + gpc_index));

			val = set_field(val,
				gr_gpcs_swdx_tc_beta_cb_size_v_m(),
				gr_gpcs_swdx_tc_beta_cb_size_v_f(
					cb_size_steady *
					gr->gpc_ppc_count[gpc_index]));

			gk20a_writel(g, gr_gpcs_swdx_tc_beta_cb_size_r(
						ppc_index + gpc_index), val);
		}
	}
}

static int gr_gp10b_init_ctx_state(struct gk20a *g)
{
	struct fecs_method_op_gk20a op = {
		.mailbox = { .id = 0, .data = 0,
			     .clr = ~0, .ok = 0, .fail = 0},
		.method.data = 0,
		.cond.ok = GR_IS_UCODE_OP_NOT_EQUAL,
		.cond.fail = GR_IS_UCODE_OP_SKIP,
		};
	int err;

	gk20a_dbg_fn("");

	err = gr_gk20a_init_ctx_state(g);
	if (err)
		return err;

	if (!g->gr.t18x.ctx_vars.preempt_image_size) {
		op.method.addr =
			gr_fecs_method_push_adr_discover_preemption_image_size_v();
		op.mailbox.ret = &g->gr.t18x.ctx_vars.preempt_image_size;
		err = gr_gk20a_submit_fecs_method_op(g, op, false);
		if (err) {
			gk20a_err(dev_from_gk20a(g),
					"query preempt image size failed");
			return err;
		}
	}

	gk20a_dbg_info("preempt image size: %u",
		g->gr.t18x.ctx_vars.preempt_image_size);

	gk20a_dbg_fn("done");

	return 0;
}

int gr_gp10b_alloc_buffer(struct vm_gk20a *vm, size_t size,
			struct mem_desc *mem)
{
	int err;

	gk20a_dbg_fn("");

	err = gk20a_gmmu_alloc_sys(vm->mm->g, size, mem);
	if (err)
		return err;

	mem->gpu_va = gk20a_gmmu_map(vm,
				&mem->sgt,
				size,
				NVGPU_MAP_BUFFER_FLAGS_CACHEABLE_TRUE,
				gk20a_mem_flag_none,
				false,
				mem->aperture);

	if (!mem->gpu_va) {
		err = -ENOMEM;
		goto fail_free;
	}

	return 0;

fail_free:
	gk20a_gmmu_free(vm->mm->g, mem);
	return err;
}

static int gr_gp10b_set_ctxsw_preemption_mode(struct gk20a *g,
				struct gr_ctx_desc *gr_ctx,
				struct vm_gk20a *vm, u32 class,
				u32 graphics_preempt_mode,
				u32 compute_preempt_mode)
{
	int err = 0;

	if (class == PASCAL_A && g->gr.t18x.ctx_vars.force_preemption_gfxp)
		graphics_preempt_mode = NVGPU_GRAPHICS_PREEMPTION_MODE_GFXP;

	if (class == PASCAL_COMPUTE_A &&
			g->gr.t18x.ctx_vars.force_preemption_cilp)
		compute_preempt_mode = NVGPU_COMPUTE_PREEMPTION_MODE_CILP;

	/* check for invalid combinations */
	if ((graphics_preempt_mode == 0) && (compute_preempt_mode == 0))
		return -EINVAL;

	if ((graphics_preempt_mode == NVGPU_GRAPHICS_PREEMPTION_MODE_GFXP) &&
		   (compute_preempt_mode == NVGPU_COMPUTE_PREEMPTION_MODE_CILP))
		return -EINVAL;

	/* set preemption modes */
	switch (graphics_preempt_mode) {
	case NVGPU_GRAPHICS_PREEMPTION_MODE_GFXP:
		{
		u32 spill_size =
			gr_gpc0_swdx_rm_spill_buffer_size_256b_default_v() *
			gr_gpc0_swdx_rm_spill_buffer_size_256b_byte_granularity_v();
		u32 pagepool_size = g->ops.gr.pagepool_default_size(g) *
			gr_scc_pagepool_total_pages_byte_granularity_v();
		u32 betacb_size = g->gr.attrib_cb_default_size +
				  (gr_gpc0_ppc0_cbm_beta_cb_size_v_gfxp_v() -
				   gr_gpc0_ppc0_cbm_beta_cb_size_v_default_v());
		u32 attrib_cb_size = (betacb_size + g->gr.alpha_cb_size) *
				  gr_gpc0_ppc0_cbm_beta_cb_size_v_granularity_v() *
				  g->gr.max_tpc_count;
		attrib_cb_size = ALIGN(attrib_cb_size, 128);

		gk20a_dbg_info("gfxp context spill_size=%d", spill_size);
		gk20a_dbg_info("gfxp context pagepool_size=%d", pagepool_size);
		gk20a_dbg_info("gfxp context attrib_cb_size=%d",
				attrib_cb_size);

		err = gr_gp10b_alloc_buffer(vm,
					g->gr.t18x.ctx_vars.preempt_image_size,
					&gr_ctx->t18x.preempt_ctxsw_buffer);
		if (err) {
			gk20a_err(dev_from_gk20a(g),
				  "cannot allocate preempt buffer");
			goto fail;
		}

		err = gr_gp10b_alloc_buffer(vm,
					spill_size,
					&gr_ctx->t18x.spill_ctxsw_buffer);
		if (err) {
			gk20a_err(dev_from_gk20a(g),
				  "cannot allocate spill buffer");
			goto fail_free_preempt;
		}

		err = gr_gp10b_alloc_buffer(vm,
					attrib_cb_size,
					&gr_ctx->t18x.betacb_ctxsw_buffer);
		if (err) {
			gk20a_err(dev_from_gk20a(g),
				  "cannot allocate beta buffer");
			goto fail_free_spill;
		}

		err = gr_gp10b_alloc_buffer(vm,
					pagepool_size,
					&gr_ctx->t18x.pagepool_ctxsw_buffer);
		if (err) {
			gk20a_err(dev_from_gk20a(g),
				  "cannot allocate page pool");
			goto fail_free_betacb;
		}

		gr_ctx->graphics_preempt_mode = graphics_preempt_mode;
		break;
		}

	case NVGPU_GRAPHICS_PREEMPTION_MODE_WFI:
		gr_ctx->graphics_preempt_mode = graphics_preempt_mode;
		break;

	default:
		break;
	}

	if (class == PASCAL_COMPUTE_A) {
		switch (compute_preempt_mode) {
		case NVGPU_COMPUTE_PREEMPTION_MODE_WFI:
		case NVGPU_COMPUTE_PREEMPTION_MODE_CTA:
		case NVGPU_COMPUTE_PREEMPTION_MODE_CILP:
			gr_ctx->compute_preempt_mode = compute_preempt_mode;
			break;
		default:
			break;
		}
	}

	return 0;

fail_free_betacb:
	gk20a_gmmu_unmap_free(vm, &gr_ctx->t18x.betacb_ctxsw_buffer);
fail_free_spill:
	gk20a_gmmu_unmap_free(vm, &gr_ctx->t18x.spill_ctxsw_buffer);
fail_free_preempt:
	gk20a_gmmu_unmap_free(vm, &gr_ctx->t18x.preempt_ctxsw_buffer);
fail:
	return err;
}

static int gr_gp10b_alloc_gr_ctx(struct gk20a *g,
			  struct gr_ctx_desc **gr_ctx, struct vm_gk20a *vm,
			  u32 class,
			  u32 flags)
{
	int err;
	u32 graphics_preempt_mode = 0;
	u32 compute_preempt_mode = 0;

	gk20a_dbg_fn("");

	err = gr_gk20a_alloc_gr_ctx(g, gr_ctx, vm, class, flags);
	if (err)
		return err;

	(*gr_ctx)->t18x.ctx_id_valid = false;

	if (flags & NVGPU_ALLOC_OBJ_FLAGS_GFXP)
		graphics_preempt_mode = NVGPU_GRAPHICS_PREEMPTION_MODE_GFXP;
	if (flags & NVGPU_ALLOC_OBJ_FLAGS_CILP)
		compute_preempt_mode = NVGPU_COMPUTE_PREEMPTION_MODE_CILP;

	if (graphics_preempt_mode || compute_preempt_mode) {
		if (g->ops.gr.set_ctxsw_preemption_mode) {
			err = g->ops.gr.set_ctxsw_preemption_mode(g, *gr_ctx, vm,
			    class, graphics_preempt_mode, compute_preempt_mode);
			if (err) {
				gk20a_err(dev_from_gk20a(g),
						"set_ctxsw_preemption_mode failed");
				goto fail_free_gk20a_ctx;
			}
		} else
			goto fail_free_gk20a_ctx;
	}

	gk20a_dbg_fn("done");

	return 0;

fail_free_gk20a_ctx:
	gr_gk20a_free_gr_ctx(g, vm, *gr_ctx);
	*gr_ctx = NULL;

	return err;
}

static void dump_ctx_switch_stats(struct gk20a *g, struct vm_gk20a *vm,
		  struct gr_ctx_desc *gr_ctx)
{
	struct mem_desc *mem = &gr_ctx->mem;

	if (gk20a_mem_begin(g, mem)) {
		WARN_ON("Cannot map context");
		return;
	}
	gk20a_err(dev_from_gk20a(g), "ctxsw_prog_main_image_magic_value_o : %x (expect %x)\n",
		gk20a_mem_rd(g, mem,
				ctxsw_prog_main_image_magic_value_o()),
		ctxsw_prog_main_image_magic_value_v_value_v());

	gk20a_err(dev_from_gk20a(g), "ctxsw_prog_main_image_context_timestamp_buffer_ptr_hi : %x\n",
		gk20a_mem_rd(g, mem,
				ctxsw_prog_main_image_context_timestamp_buffer_ptr_hi_o()));

	gk20a_err(dev_from_gk20a(g), "ctxsw_prog_main_image_context_timestamp_buffer_ptr : %x\n",
		gk20a_mem_rd(g, mem,
				ctxsw_prog_main_image_context_timestamp_buffer_ptr_o()));

	gk20a_err(dev_from_gk20a(g), "ctxsw_prog_main_image_context_timestamp_buffer_control : %x\n",
		gk20a_mem_rd(g, mem,
				ctxsw_prog_main_image_context_timestamp_buffer_control_o()));

	gk20a_err(dev_from_gk20a(g), "NUM_SAVE_OPERATIONS : %d\n",
		gk20a_mem_rd(g, mem,
			ctxsw_prog_main_image_num_save_ops_o()));
	gk20a_err(dev_from_gk20a(g), "WFI_SAVE_OPERATIONS : %d\n",
		gk20a_mem_rd(g, mem,
			ctxsw_prog_main_image_num_wfi_save_ops_o()));
	gk20a_err(dev_from_gk20a(g), "CTA_SAVE_OPERATIONS : %d\n",
		gk20a_mem_rd(g, mem,
			ctxsw_prog_main_image_num_cta_save_ops_o()));
	gk20a_err(dev_from_gk20a(g), "GFXP_SAVE_OPERATIONS : %d\n",
		gk20a_mem_rd(g, mem,
			ctxsw_prog_main_image_num_gfxp_save_ops_o()));
	gk20a_err(dev_from_gk20a(g), "CILP_SAVE_OPERATIONS : %d\n",
		gk20a_mem_rd(g, mem,
			ctxsw_prog_main_image_num_cilp_save_ops_o()));
	gk20a_err(dev_from_gk20a(g),
		"image gfx preemption option (GFXP is 1) %x\n",
		gk20a_mem_rd(g, mem,
			ctxsw_prog_main_image_graphics_preemption_options_o()));
	gk20a_mem_end(g, mem);
}

static void gr_gp10b_free_gr_ctx(struct gk20a *g, struct vm_gk20a *vm,
			  struct gr_ctx_desc *gr_ctx)
{
	gk20a_dbg_fn("");

	if (!gr_ctx)
		return;

	if (g->gr.t18x.ctx_vars.dump_ctxsw_stats_on_channel_close)
		dump_ctx_switch_stats(g, vm, gr_ctx);

	gk20a_gmmu_unmap_free(vm, &gr_ctx->t18x.pagepool_ctxsw_buffer);
	gk20a_gmmu_unmap_free(vm, &gr_ctx->t18x.betacb_ctxsw_buffer);
	gk20a_gmmu_unmap_free(vm, &gr_ctx->t18x.spill_ctxsw_buffer);
	gk20a_gmmu_unmap_free(vm, &gr_ctx->t18x.preempt_ctxsw_buffer);
	gr_gk20a_free_gr_ctx(g, vm, gr_ctx);
	gk20a_dbg_fn("done");
}


static void gr_gp10b_update_ctxsw_preemption_mode(struct gk20a *g,
		struct channel_ctx_gk20a *ch_ctx,
		struct mem_desc *mem)
{
	struct gr_ctx_desc *gr_ctx = ch_ctx->gr_ctx;
	u32 gfxp_preempt_option =
		ctxsw_prog_main_image_graphics_preemption_options_control_gfxp_f();
	u32 cilp_preempt_option =
		ctxsw_prog_main_image_compute_preemption_options_control_cilp_f();
	u32 cta_preempt_option =
		ctxsw_prog_main_image_compute_preemption_options_control_cta_f();
	int err;

	gk20a_dbg_fn("");

	if (gr_ctx->graphics_preempt_mode == NVGPU_GRAPHICS_PREEMPTION_MODE_GFXP) {
		gk20a_dbg_info("GfxP: %x", gfxp_preempt_option);
		gk20a_mem_wr(g, mem,
				ctxsw_prog_main_image_graphics_preemption_options_o(),
				gfxp_preempt_option);
	}

	if (gr_ctx->compute_preempt_mode == NVGPU_COMPUTE_PREEMPTION_MODE_CILP) {
		gk20a_dbg_info("CILP: %x", cilp_preempt_option);
		gk20a_mem_wr(g, mem,
				ctxsw_prog_main_image_compute_preemption_options_o(),
				cilp_preempt_option);
	}

	if (gr_ctx->compute_preempt_mode == NVGPU_COMPUTE_PREEMPTION_MODE_CTA) {
		gk20a_dbg_info("CTA: %x", cta_preempt_option);
		gk20a_mem_wr(g, mem,
				ctxsw_prog_main_image_compute_preemption_options_o(),
				cta_preempt_option);
	}

	if (gr_ctx->t18x.preempt_ctxsw_buffer.gpu_va) {
		u32 addr;
		u32 size;
		u32 cbes_reserve;

		gk20a_mem_wr(g, mem,
				ctxsw_prog_main_image_full_preemption_ptr_o(),
				gr_ctx->t18x.preempt_ctxsw_buffer.gpu_va >> 8);

		err = gr_gk20a_ctx_patch_write_begin(g, ch_ctx);
		if (err) {
			gk20a_err(dev_from_gk20a(g),
					"can't map patch context");
			goto out;
		}

		addr = (u64_lo32(gr_ctx->t18x.betacb_ctxsw_buffer.gpu_va) >>
			gr_gpcs_setup_attrib_cb_base_addr_39_12_align_bits_v()) |
			(u64_hi32(gr_ctx->t18x.betacb_ctxsw_buffer.gpu_va) <<
			 (32 - gr_gpcs_setup_attrib_cb_base_addr_39_12_align_bits_v()));

		gk20a_dbg_info("attrib cb addr : 0x%016x", addr);
		g->ops.gr.commit_global_attrib_cb(g, ch_ctx, addr, true);

		addr = (u64_lo32(gr_ctx->t18x.pagepool_ctxsw_buffer.gpu_va) >>
			gr_scc_pagepool_base_addr_39_8_align_bits_v()) |
			(u64_hi32(gr_ctx->t18x.pagepool_ctxsw_buffer.gpu_va) <<
			 (32 - gr_scc_pagepool_base_addr_39_8_align_bits_v()));
		size = gr_ctx->t18x.pagepool_ctxsw_buffer.size;

		if (size == g->ops.gr.pagepool_default_size(g))
			size = gr_scc_pagepool_total_pages_hwmax_v();

		g->ops.gr.commit_global_pagepool(g, ch_ctx, addr, size, true);

		addr = (u64_lo32(gr_ctx->t18x.spill_ctxsw_buffer.gpu_va) >>
			gr_gpc0_swdx_rm_spill_buffer_addr_39_8_align_bits_v()) |
			(u64_hi32(gr_ctx->t18x.spill_ctxsw_buffer.gpu_va) <<
			 (32 - gr_gpc0_swdx_rm_spill_buffer_addr_39_8_align_bits_v()));
		size = gr_ctx->t18x.spill_ctxsw_buffer.size /
			gr_gpc0_swdx_rm_spill_buffer_size_256b_byte_granularity_v();

		gr_gk20a_ctx_patch_write(g, ch_ctx,
				gr_gpc0_swdx_rm_spill_buffer_addr_r(),
				gr_gpc0_swdx_rm_spill_buffer_addr_39_8_f(addr),
				true);
		gr_gk20a_ctx_patch_write(g, ch_ctx,
				gr_gpc0_swdx_rm_spill_buffer_size_r(),
				gr_gpc0_swdx_rm_spill_buffer_size_256b_f(size),
				true);

		cbes_reserve = gr_gpcs_swdx_beta_cb_ctrl_cbes_reserve_gfxp_v();
		gr_gk20a_ctx_patch_write(g, ch_ctx,
				gr_gpcs_swdx_beta_cb_ctrl_r(),
				gr_gpcs_swdx_beta_cb_ctrl_cbes_reserve_f(
					cbes_reserve),
				true);
		gr_gk20a_ctx_patch_write(g, ch_ctx,
				gr_gpcs_ppcs_cbm_beta_cb_ctrl_r(),
				gr_gpcs_ppcs_cbm_beta_cb_ctrl_cbes_reserve_f(
					cbes_reserve),
				true);

		gr_gk20a_ctx_patch_write_end(g, ch_ctx);
	}

out:
	gk20a_dbg_fn("done");
}

static int gr_gp10b_dump_gr_status_regs(struct gk20a *g,
			   struct gk20a_debug_output *o)
{
	struct gr_gk20a *gr = &g->gr;
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
	if (gr->gpc_tpc_count[0] == 2)
		gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_TPC1_TPCCS_TPC_ACTIVITY0: 0x%x\n",
			gk20a_readl(g, gr_pri_gpc0_tpc1_tpccs_tpc_activity_0_r()));
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
	if (gr->gpc_tpc_count[0] == 2)
		gk20a_debug_output(o, "NV_PGRAPH_PRI_GPCS_TPC1_TPCCS_TPC_ACTIVITY0: 0x%x\n",
			gk20a_readl(g, gr_pri_gpcs_tpc1_tpccs_tpc_activity_0_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPCS_TPCS_TPCCS_TPC_ACTIVITY0: 0x%x\n",
		gk20a_readl(g, gr_pri_gpcs_tpcs_tpccs_tpc_activity_0_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_BE0_BECS_BE_ACTIVITY0: 0x%x\n",
		gk20a_readl(g, gr_pri_be0_becs_be_activity0_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_BE1_BECS_BE_ACTIVITY0: 0x%x\n",
		gk20a_readl(g, gr_pri_be1_becs_be_activity0_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_BES_BECS_BE_ACTIVITY0: 0x%x\n",
		gk20a_readl(g, gr_pri_bes_becs_be_activity0_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_DS_MPIPE_STATUS: 0x%x\n",
		gk20a_readl(g, gr_pri_ds_mpipe_status_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FE_GO_IDLE_TIMEOUT : 0x%x\n",
		gk20a_readl(g, gr_fe_go_idle_timeout_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FE_GO_IDLE_INFO : 0x%x\n",
		gk20a_readl(g, gr_pri_fe_go_idle_info_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_TPC0_TEX_M_TEX_SUBUNITS_STATUS: 0x%x\n",
		gk20a_readl(g, gr_pri_gpc0_tpc0_tex_m_tex_subunits_status_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_CWD_FS: 0x%x\n",
		gk20a_readl(g, gr_cwd_fs_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FE_TPC_FS: 0x%x\n",
		gk20a_readl(g, gr_fe_tpc_fs_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_CWD_GPC_TPC_ID(0): 0x%x\n",
		gk20a_readl(g, gr_cwd_gpc_tpc_id_r(0)));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_CWD_SM_ID(0): 0x%x\n",
		gk20a_readl(g, gr_cwd_sm_id_r(0)));
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

static bool gr_activity_empty_or_preempted(u32 val)
{
	while(val) {
		u32 v = val & 7;
		if (v != gr_activity_4_gpc0_empty_v() &&
		    v != gr_activity_4_gpc0_preempted_v())
			return false;
		val >>= 3;
	}

	return true;
}

static int gr_gp10b_wait_empty(struct gk20a *g, unsigned long end_jiffies,
		       u32 expect_delay)
{
	u32 delay = expect_delay;
	bool gr_enabled;
	bool ctxsw_active;
	bool gr_busy;
	u32 gr_status;
	u32 activity0, activity1, activity2, activity4;

	gk20a_dbg_fn("");

	do {
		/* fmodel: host gets fifo_engine_status(gr) from gr
		   only when gr_status is read */
		gr_status = gk20a_readl(g, gr_status_r());

		gr_enabled = gk20a_readl(g, mc_enable_r()) &
			mc_enable_pgraph_enabled_f();

		ctxsw_active = gr_status & 1<<7;

		activity0 = gk20a_readl(g, gr_activity_0_r());
		activity1 = gk20a_readl(g, gr_activity_1_r());
		activity2 = gk20a_readl(g, gr_activity_2_r());
		activity4 = gk20a_readl(g, gr_activity_4_r());

		gr_busy = !(gr_activity_empty_or_preempted(activity0) &&
			    gr_activity_empty_or_preempted(activity1) &&
			    activity2 == 0 &&
			    gr_activity_empty_or_preempted(activity4));

		if (!gr_enabled || (!gr_busy && !ctxsw_active)) {
			gk20a_dbg_fn("done");
			return 0;
		}

		usleep_range(delay, delay * 2);
		delay = min_t(u32, delay << 1, GR_IDLE_CHECK_MAX);

	} while (time_before(jiffies, end_jiffies)
			|| !tegra_platform_is_silicon());

	gk20a_err(dev_from_gk20a(g),
		"timeout, ctxsw busy : %d, gr busy : %d, %08x, %08x, %08x, %08x",
		ctxsw_active, gr_busy, activity0, activity1, activity2, activity4);

	return -EAGAIN;
}

static void gr_gp10b_commit_global_attrib_cb(struct gk20a *g,
					     struct channel_ctx_gk20a *ch_ctx,
					     u64 addr, bool patch)
{
	struct gr_ctx_desc *gr_ctx = ch_ctx->gr_ctx;
	int attrBufferSize;

	if (gr_ctx->t18x.preempt_ctxsw_buffer.gpu_va)
		attrBufferSize = gr_ctx->t18x.betacb_ctxsw_buffer.size;
	else
		attrBufferSize = g->ops.gr.calc_global_ctx_buffer_size(g);

	attrBufferSize /= gr_gpcs_tpcs_tex_rm_cb_1_size_div_128b_granularity_f();

	gr_gm20b_commit_global_attrib_cb(g, ch_ctx, addr, patch);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_tpcs_mpc_vtg_cb_global_base_addr_r(),
		gr_gpcs_tpcs_mpc_vtg_cb_global_base_addr_v_f(addr) |
		gr_gpcs_tpcs_mpc_vtg_cb_global_base_addr_valid_true_f(), patch);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_tpcs_tex_rm_cb_0_r(),
		gr_gpcs_tpcs_tex_rm_cb_0_base_addr_43_12_f(addr), patch);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_tpcs_tex_rm_cb_1_r(),
		gr_gpcs_tpcs_tex_rm_cb_1_size_div_128b_f(attrBufferSize) |
		gr_gpcs_tpcs_tex_rm_cb_1_valid_true_f(), patch);
}

static void gr_gp10b_commit_global_bundle_cb(struct gk20a *g,
					    struct channel_ctx_gk20a *ch_ctx,
					    u64 addr, u64 size, bool patch)
{
	u32 data;

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_scc_bundle_cb_base_r(),
		gr_scc_bundle_cb_base_addr_39_8_f(addr), patch);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_scc_bundle_cb_size_r(),
		gr_scc_bundle_cb_size_div_256b_f(size) |
		gr_scc_bundle_cb_size_valid_true_f(), patch);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_swdx_bundle_cb_base_r(),
		gr_gpcs_swdx_bundle_cb_base_addr_39_8_f(addr), patch);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_swdx_bundle_cb_size_r(),
		gr_gpcs_swdx_bundle_cb_size_div_256b_f(size) |
		gr_gpcs_swdx_bundle_cb_size_valid_true_f(), patch);

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

static int gr_gp10b_load_smid_config(struct gk20a *g)
{
	u32 *tpc_sm_id;
	u32 i, j;
	u32 tpc_index, gpc_index;
	u32 max_gpcs = nvgpu_get_litter_value(g, GPU_LIT_NUM_GPCS);

	tpc_sm_id = kcalloc(gr_cwd_sm_id__size_1_v(), sizeof(u32), GFP_KERNEL);
	if (!tpc_sm_id)
		return -ENOMEM;

	/* Each NV_PGRAPH_PRI_CWD_GPC_TPC_ID can store 4 TPCs.*/
	for (i = 0; i <= ((g->gr.tpc_count-1) / 4); i++) {
		u32 reg = 0;
		u32 bit_stride = gr_cwd_gpc_tpc_id_gpc0_s() +
				 gr_cwd_gpc_tpc_id_tpc0_s();

		for (j = 0; j < 4; j++) {
			u32 sm_id = (i * 4) + j;
			u32 bits;

			if (sm_id >= g->gr.tpc_count)
				break;

			gpc_index = g->gr.sm_to_cluster[sm_id].gpc_index;
			tpc_index = g->gr.sm_to_cluster[sm_id].tpc_index;

			bits = gr_cwd_gpc_tpc_id_gpc0_f(gpc_index) |
			       gr_cwd_gpc_tpc_id_tpc0_f(tpc_index);
			reg |= bits << (j * bit_stride);

			tpc_sm_id[gpc_index + max_gpcs * ((tpc_index & 4) >> 2)]
				|= sm_id << (bit_stride * (tpc_index & 3));
		}
		gk20a_writel(g, gr_cwd_gpc_tpc_id_r(i), reg);
	}

	for (i = 0; i < gr_cwd_sm_id__size_1_v(); i++)
		gk20a_writel(g, gr_cwd_sm_id_r(i), tpc_sm_id[i]);

	kfree(tpc_sm_id);

	return 0;
}

int gr_gp10b_init_fs_state(struct gk20a *g)
{
	u32 data;

	gk20a_dbg_fn("");

	data = gk20a_readl(g, gr_gpcs_tpcs_sm_texio_control_r());
	data = set_field(data, gr_gpcs_tpcs_sm_texio_control_oor_addr_check_mode_m(),
			gr_gpcs_tpcs_sm_texio_control_oor_addr_check_mode_arm_63_48_match_f());
	gk20a_writel(g, gr_gpcs_tpcs_sm_texio_control_r(), data);

	data = gk20a_readl(g, gr_gpcs_tpcs_sm_disp_ctrl_r());
	data = set_field(data, gr_gpcs_tpcs_sm_disp_ctrl_re_suppress_m(),
			 gr_gpcs_tpcs_sm_disp_ctrl_re_suppress_disable_f());
	gk20a_writel(g, gr_gpcs_tpcs_sm_disp_ctrl_r(), data);

	if (g->gr.t18x.fecs_feature_override_ecc_val != 0) {
		gk20a_writel(g,
			gr_fecs_feature_override_ecc_r(),
			g->gr.t18x.fecs_feature_override_ecc_val);
	}

	return gr_gm20b_init_fs_state(g);
}

static void gr_gp10b_init_cyclestats(struct gk20a *g)
{
#if defined(CONFIG_GK20A_CYCLE_STATS)
	g->gpu_characteristics.flags |=
		NVGPU_GPU_FLAGS_SUPPORT_CYCLE_STATS;
	g->gpu_characteristics.flags |=
		NVGPU_GPU_FLAGS_SUPPORT_CYCLE_STATS_SNAPSHOT;
#else
	(void)g;
#endif
}

static void gr_gp10b_set_gpc_tpc_mask(struct gk20a *g, u32 gpc_index)
{
	tegra_fuse_writel(0x1, FUSE_FUSEBYPASS_0);
	tegra_fuse_writel(0x0, FUSE_WRITE_ACCESS_SW_0);

	if (g->gr.gpc_tpc_mask[gpc_index] == 0x1)
		tegra_fuse_writel(0x2, FUSE_OPT_GPU_TPC0_DISABLE_0);
	else if (g->gr.gpc_tpc_mask[gpc_index] == 0x2)
		tegra_fuse_writel(0x1, FUSE_OPT_GPU_TPC0_DISABLE_0);
	else
		tegra_fuse_writel(0x0, FUSE_OPT_GPU_TPC0_DISABLE_0);
}

static void gr_gp10b_get_access_map(struct gk20a *g,
				   u32 **whitelist, int *num_entries)
{
	static u32 wl_addr_gp10b[] = {
		/* this list must be sorted (low to high) */
		0x404468, /* gr_pri_mme_max_instructions       */
		0x418300, /* gr_pri_gpcs_rasterarb_line_class  */
		0x418800, /* gr_pri_gpcs_setup_debug           */
		0x418e00, /* gr_pri_gpcs_swdx_config           */
		0x418e40, /* gr_pri_gpcs_swdx_tc_bundle_ctrl   */
		0x418e44, /* gr_pri_gpcs_swdx_tc_bundle_ctrl   */
		0x418e48, /* gr_pri_gpcs_swdx_tc_bundle_ctrl   */
		0x418e4c, /* gr_pri_gpcs_swdx_tc_bundle_ctrl   */
		0x418e50, /* gr_pri_gpcs_swdx_tc_bundle_ctrl   */
		0x418e58, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e5c, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e60, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e64, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e68, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e6c, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e70, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e74, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e78, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e7c, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e80, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e84, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e88, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e8c, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e90, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e94, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x419864, /* gr_pri_gpcs_tpcs_pe_l2_evict_policy */
		0x419a04, /* gr_pri_gpcs_tpcs_tex_lod_dbg      */
		0x419a08, /* gr_pri_gpcs_tpcs_tex_samp_dbg     */
		0x419e10, /* gr_pri_gpcs_tpcs_sm_dbgr_control0 */
		0x419f78, /* gr_pri_gpcs_tpcs_sm_disp_ctrl     */
	};

	*whitelist = wl_addr_gp10b;
	*num_entries = ARRAY_SIZE(wl_addr_gp10b);
}

static int gr_gp10b_disable_channel_or_tsg(struct gk20a *g, struct channel_gk20a *fault_ch)
{
	int ret = 0;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr, "");

	ret = gk20a_disable_channel_tsg(g, fault_ch);
	if (ret) {
		gk20a_err(dev_from_gk20a(g),
				"CILP: failed to disable channel/TSG!\n");
		return ret;
	}

	ret = g->ops.fifo.update_runlist(g, fault_ch->runlist_id, ~0, true, false);
	if (ret) {
		gk20a_err(dev_from_gk20a(g),
				"CILP: failed to restart runlist 0!");
		return ret;
	}

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr, "CILP: restarted runlist");

	if (gk20a_is_channel_marked_as_tsg(fault_ch))
		gk20a_fifo_issue_preempt(g, fault_ch->tsgid, true);
	else
		gk20a_fifo_issue_preempt(g, fault_ch->hw_chid, false);

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr, "CILP: preempted the channel/tsg");

	return ret;
}

static int gr_gp10b_set_cilp_preempt_pending(struct gk20a *g, struct channel_gk20a *fault_ch)
{
	int ret;
	struct gr_ctx_desc *gr_ctx = fault_ch->ch_ctx.gr_ctx;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr, "");

	if (!gr_ctx)
		return -EINVAL;

	if (gr_ctx->t18x.cilp_preempt_pending) {
		gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr,
				"CILP is already pending for chid %d",
				fault_ch->hw_chid);
		return 0;
	}

	/* get ctx_id from the ucode image */
	if (!gr_ctx->t18x.ctx_id_valid) {
		gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr,
				"CILP: looking up ctx id");
		ret = gr_gk20a_get_ctx_id(g, fault_ch, &gr_ctx->t18x.ctx_id);
		if (ret) {
			gk20a_err(dev_from_gk20a(g), "CILP: error looking up ctx id!\n");
			return ret;
		}
		gr_ctx->t18x.ctx_id_valid = true;
	}

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr,
			"CILP: ctx id is 0x%x", gr_ctx->t18x.ctx_id);

	/* send ucode method to set ctxsw interrupt */
	ret = gr_gk20a_submit_fecs_sideband_method_op(g,
			(struct fecs_method_op_gk20a) {
			.method.data = gr_ctx->t18x.ctx_id,
			.method.addr =
			gr_fecs_method_push_adr_configure_interrupt_completion_option_v(),
			.mailbox = {
			.id = 1 /* sideband */, .data = 0,
			.clr = ~0, .ret = NULL,
			.ok = gr_fecs_ctxsw_mailbox_value_pass_v(),
			.fail = 0},
			.cond.ok = GR_IS_UCODE_OP_EQUAL,
			.cond.fail = GR_IS_UCODE_OP_SKIP});

	if (ret) {
		gk20a_err(dev_from_gk20a(g),
				"CILP: failed to enable ctxsw interrupt!");
		return ret;
	}

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr,
				"CILP: enabled ctxsw completion interrupt");

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr,
			"CILP: disabling channel %d",
			fault_ch->hw_chid);

	ret = gr_gp10b_disable_channel_or_tsg(g, fault_ch);
	if (ret) {
		gk20a_err(dev_from_gk20a(g),
				"CILP: failed to disable channel!!");
		return ret;
	}

	/* set cilp_preempt_pending = true and record the channel */
	gr_ctx->t18x.cilp_preempt_pending = true;
	g->gr.t18x.cilp_preempt_pending_chid = fault_ch->hw_chid;

	if (gk20a_is_channel_marked_as_tsg(fault_ch)) {
		struct tsg_gk20a *tsg = &g->fifo.tsg[fault_ch->tsgid];

		gk20a_tsg_event_id_post_event(tsg,
				NVGPU_IOCTL_CHANNEL_EVENT_ID_CILP_PREEMPTION_STARTED);
	} else {
		gk20a_channel_event_id_post_event(fault_ch,
				NVGPU_IOCTL_CHANNEL_EVENT_ID_CILP_PREEMPTION_STARTED);
	}

	return 0;
}

static int gr_gp10b_clear_cilp_preempt_pending(struct gk20a *g,
					       struct channel_gk20a *fault_ch)
{
	struct gr_ctx_desc *gr_ctx = fault_ch->ch_ctx.gr_ctx;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr, "");

	if (!gr_ctx)
		return -EINVAL;

	/* The ucode is self-clearing, so all we need to do here is
	   to clear cilp_preempt_pending. */
	if (!gr_ctx->t18x.cilp_preempt_pending) {
		gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr,
				"CILP is already cleared for chid %d\n",
				fault_ch->hw_chid);
		return 0;
	}

	gr_ctx->t18x.cilp_preempt_pending = false;
	g->gr.t18x.cilp_preempt_pending_chid = -1;

	return 0;
}

/* @brief pre-process work on the SM exceptions to determine if we clear them or not.
 *
 * On Pascal, if we are in CILP preemtion mode, preempt the channel and handle errors with special processing
 */
static int gr_gp10b_pre_process_sm_exception(struct gk20a *g,
		u32 gpc, u32 tpc, u32 global_esr, u32 warp_esr,
		bool sm_debugger_attached, struct channel_gk20a *fault_ch,
		bool *early_exit, bool *ignore_debugger)
{
	int ret;
	bool cilp_enabled = (fault_ch->ch_ctx.gr_ctx->compute_preempt_mode ==
			NVGPU_COMPUTE_PREEMPTION_MODE_CILP) ;
	u32 global_mask = 0, dbgr_control0, global_esr_copy;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 offset = gpc_stride * gpc + tpc_in_gpc_stride * tpc;

	*early_exit = false;
	*ignore_debugger = false;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg, "SM Exception received on gpc %d tpc %d = %u\n",
			gpc, tpc, global_esr);

	if (cilp_enabled && sm_debugger_attached) {
		if (global_esr & gr_gpc0_tpc0_sm_hww_global_esr_bpt_int_pending_f())
			gk20a_writel(g, gr_gpc0_tpc0_sm_hww_global_esr_r() + offset,
					gr_gpc0_tpc0_sm_hww_global_esr_bpt_int_pending_f());

		if (global_esr & gr_gpc0_tpc0_sm_hww_global_esr_single_step_complete_pending_f())
			gk20a_writel(g, gr_gpc0_tpc0_sm_hww_global_esr_r() + offset,
					gr_gpc0_tpc0_sm_hww_global_esr_single_step_complete_pending_f());

		global_mask = gr_gpc0_tpc0_sm_hww_global_esr_sm_to_sm_fault_pending_f() |
			gr_gpcs_tpcs_sm_hww_global_esr_l1_error_pending_f() |
			gr_gpcs_tpcs_sm_hww_global_esr_multiple_warp_errors_pending_f() |
			gr_gpcs_tpcs_sm_hww_global_esr_physical_stack_overflow_error_pending_f() |
			gr_gpcs_tpcs_sm_hww_global_esr_timeout_error_pending_f() |
			gr_gpcs_tpcs_sm_hww_global_esr_bpt_pause_pending_f();

		if (warp_esr != 0 || (global_esr & global_mask) != 0) {
			*ignore_debugger = true;

			gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg,
					"CILP: starting wait for LOCKED_DOWN on gpc %d tpc %d\n",
					gpc, tpc);

			if (gk20a_dbg_gpu_broadcast_stop_trigger(fault_ch)) {
				gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg,
						"CILP: Broadcasting STOP_TRIGGER from gpc %d tpc %d\n",
						gpc, tpc);
				gk20a_suspend_all_sms(g, global_mask, false);

				gk20a_dbg_gpu_clear_broadcast_stop_trigger(fault_ch);
			} else {
				gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg,
						"CILP: STOP_TRIGGER from gpc %d tpc %d\n",
						gpc, tpc);
				gk20a_suspend_single_sm(g, gpc, tpc, global_mask, true);
			}

			/* reset the HWW errors after locking down */
			global_esr_copy = gk20a_readl(g, gr_gpc0_tpc0_sm_hww_global_esr_r() + offset);
			gk20a_gr_clear_sm_hww(g, gpc, tpc, global_esr_copy);
			gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg,
					"CILP: HWWs cleared for gpc %d tpc %d\n",
					gpc, tpc);

			gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg, "CILP: Setting CILP preempt pending\n");
			ret = gr_gp10b_set_cilp_preempt_pending(g, fault_ch);
			if (ret) {
				gk20a_err(dev_from_gk20a(g), "CILP: error while setting CILP preempt pending!\n");
				return ret;
			}

			dbgr_control0 = gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_control0_r() + offset);
			if (dbgr_control0 & gr_gpcs_tpcs_sm_dbgr_control0_single_step_mode_enable_f()) {
				gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg,
						"CILP: clearing SINGLE_STEP_MODE before resume for gpc %d tpc %d\n",
						gpc, tpc);
				dbgr_control0 = set_field(dbgr_control0,
						gr_gpcs_tpcs_sm_dbgr_control0_single_step_mode_m(),
						gr_gpcs_tpcs_sm_dbgr_control0_single_step_mode_disable_f());
				gk20a_writel(g, gr_gpc0_tpc0_sm_dbgr_control0_r() + offset, dbgr_control0);
			}

			gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg,
					"CILP: resume for gpc %d tpc %d\n",
					gpc, tpc);
			gk20a_resume_single_sm(g, gpc, tpc);

			*ignore_debugger = true;
			gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg, "CILP: All done on gpc %d, tpc %d\n", gpc, tpc);
		}

		*early_exit = true;
	}
	return 0;
}

static int gr_gp10b_get_cilp_preempt_pending_chid(struct gk20a *g, int *__chid)
{
	struct gr_ctx_desc *gr_ctx;
	struct channel_gk20a *ch;
	int chid;
	int ret = -EINVAL;

	chid = g->gr.t18x.cilp_preempt_pending_chid;

	ch = gk20a_channel_get(gk20a_fifo_channel_from_hw_chid(g, chid));
	if (!ch)
		return ret;

	gr_ctx = ch->ch_ctx.gr_ctx;

	if (gr_ctx->t18x.cilp_preempt_pending) {
		*__chid = chid;
		ret = 0;
	}

	gk20a_channel_put(ch);

	return ret;
}

static int gr_gp10b_handle_fecs_error(struct gk20a *g,
				struct channel_gk20a *__ch,
				struct gr_gk20a_isr_data *isr_data)
{
	u32 gr_fecs_intr = gk20a_readl(g, gr_fecs_host_int_status_r());
	struct channel_gk20a *ch;
	int chid = -1;
	int ret = 0;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr, "");

	/*
	 * INTR1 (bit 1 of the HOST_INT_STATUS_CTXSW_INTR)
	 * indicates that a CILP ctxsw save has finished
	 */
	if (gr_fecs_intr & gr_fecs_host_int_status_ctxsw_intr_f(2)) {
		gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr,
				"CILP: ctxsw save completed!\n");

		/* now clear the interrupt */
		gk20a_writel(g, gr_fecs_host_int_clear_r(),
				gr_fecs_host_int_clear_ctxsw_intr1_clear_f());

		ret = gr_gp10b_get_cilp_preempt_pending_chid(g, &chid);
		if (ret)
			goto clean_up;

		ch = gk20a_channel_get(
				gk20a_fifo_channel_from_hw_chid(g, chid));
		if (!ch)
			goto clean_up;


		/* set preempt_pending to false */
		ret = gr_gp10b_clear_cilp_preempt_pending(g, ch);
		if (ret) {
			gk20a_err(dev_from_gk20a(g), "CILP: error while unsetting CILP preempt pending!\n");
			gk20a_channel_put(ch);
			goto clean_up;
		}

		/* Post events to UMD */
		gk20a_dbg_gpu_post_events(ch);

		if (gk20a_is_channel_marked_as_tsg(ch)) {
			struct tsg_gk20a *tsg = &g->fifo.tsg[ch->tsgid];

			gk20a_tsg_event_id_post_event(tsg,
				NVGPU_IOCTL_CHANNEL_EVENT_ID_CILP_PREEMPTION_COMPLETE);
		} else {
			gk20a_channel_event_id_post_event(ch,
				NVGPU_IOCTL_CHANNEL_EVENT_ID_CILP_PREEMPTION_COMPLETE);
		}

		gk20a_channel_put(ch);
	}

clean_up:
	/* handle any remaining interrupts */
	return gk20a_gr_handle_fecs_error(g, __ch, isr_data);
}

static u32 gp10b_mask_hww_warp_esr(u32 hww_warp_esr)
{
	if (!(hww_warp_esr & gr_gpc0_tpc0_sm_hww_warp_esr_addr_valid_m()))
		hww_warp_esr = set_field(hww_warp_esr,
			gr_gpc0_tpc0_sm_hww_warp_esr_addr_error_type_m(),
			gr_gpc0_tpc0_sm_hww_warp_esr_addr_error_type_none_f());

	return hww_warp_esr;
}

static u32 get_ecc_override_val(struct gk20a *g)
{
	if (tegra_fuse_readl(FUSE_OPT_ECC_EN))
		return gk20a_readl(g, gr_fecs_feature_override_ecc_r());
	else
		return 0;
}

static bool gr_gp10b_suspend_context(struct channel_gk20a *ch,
				bool *cilp_preempt_pending)
{
	struct gk20a *g = ch->g;
	struct channel_ctx_gk20a *ch_ctx = &ch->ch_ctx;
	struct gr_ctx_desc *gr_ctx = ch_ctx->gr_ctx;
	bool ctx_resident = false;
	int err = 0;

	*cilp_preempt_pending = false;

	if (gk20a_is_channel_ctx_resident(ch)) {
		gk20a_suspend_all_sms(g, 0, false);

		if (gr_ctx->compute_preempt_mode == NVGPU_COMPUTE_PREEMPTION_MODE_CILP) {
			err = gr_gp10b_set_cilp_preempt_pending(g, ch);
			if (err)
				gk20a_err(dev_from_gk20a(g),
					"unable to set CILP preempt pending\n");
			else
				*cilp_preempt_pending = true;

			gk20a_resume_all_sms(g);
		}

		ctx_resident = true;
	} else {
		gk20a_disable_channel_tsg(g, ch);
	}

	return ctx_resident;
}

static int gr_gp10b_suspend_contexts(struct gk20a *g,
				struct dbg_session_gk20a *dbg_s,
				int *ctx_resident_ch_fd)
{
	u32 delay = GR_IDLE_CHECK_DEFAULT;
	bool cilp_preempt_pending = false;
	struct channel_gk20a *cilp_preempt_pending_ch = NULL;
	struct channel_gk20a *ch;
	struct dbg_session_channel_data *ch_data;
	int err = 0;
	int local_ctx_resident_ch_fd = -1;
	bool ctx_resident;

	mutex_lock(&g->dbg_sessions_lock);

	err = gr_gk20a_disable_ctxsw(g);
	if (err) {
		gk20a_err(dev_from_gk20a(g), "unable to stop gr ctxsw");
		mutex_unlock(&g->dbg_sessions_lock);
		goto clean_up;
	}

	mutex_lock(&dbg_s->ch_list_lock);

	list_for_each_entry(ch_data, &dbg_s->ch_list, ch_entry) {
		ch = g->fifo.channel + ch_data->chid;

		ctx_resident = gr_gp10b_suspend_context(ch,
					&cilp_preempt_pending);
		if (ctx_resident)
			local_ctx_resident_ch_fd = ch_data->channel_fd;
		if (cilp_preempt_pending)
			cilp_preempt_pending_ch = ch;
	}

	mutex_unlock(&dbg_s->ch_list_lock);

	err = gr_gk20a_enable_ctxsw(g);
	if (err) {
		mutex_unlock(&g->dbg_sessions_lock);
		goto clean_up;
	}

	mutex_unlock(&g->dbg_sessions_lock);

	if (cilp_preempt_pending_ch) {
		struct channel_ctx_gk20a *ch_ctx =
				&cilp_preempt_pending_ch->ch_ctx;
		struct gr_ctx_desc *gr_ctx = ch_ctx->gr_ctx;
		unsigned long end_jiffies = jiffies +
			msecs_to_jiffies(gk20a_get_gr_idle_timeout(g));

		gk20a_dbg(gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr,
			"CILP preempt pending, waiting %lu msecs for preemption",
			gk20a_get_gr_idle_timeout(g));

		do {
			if (!gr_ctx->t18x.cilp_preempt_pending)
				break;

			usleep_range(delay, delay * 2);
			delay = min_t(u32, delay << 1, GR_IDLE_CHECK_MAX);
		} while (time_before(jiffies, end_jiffies)
			|| !tegra_platform_is_silicon());

		/* If cilp is still pending at this point, timeout */
		if (gr_ctx->t18x.cilp_preempt_pending)
			err = -ETIMEDOUT;
	}

	*ctx_resident_ch_fd = local_ctx_resident_ch_fd;

clean_up:
	return err;
}

static int gr_gp10b_set_preemption_mode(struct channel_gk20a *ch,
					u32 graphics_preempt_mode,
					u32 compute_preempt_mode)
{
	struct gr_ctx_desc *gr_ctx = ch->ch_ctx.gr_ctx;
	struct channel_ctx_gk20a *ch_ctx = &ch->ch_ctx;
	struct gk20a *g = ch->g;
	struct tsg_gk20a *tsg;
	struct vm_gk20a *vm;
	struct mem_desc *mem = &gr_ctx->mem;
	u32 class;
	int err = 0;

	class = ch->obj_class;
	if (!class)
		return -EINVAL;

	/* preemption already set ? */
	if (gr_ctx->graphics_preempt_mode || gr_ctx->compute_preempt_mode)
		return -EINVAL;

	if (gk20a_is_channel_marked_as_tsg(ch)) {
		tsg = &g->fifo.tsg[ch->tsgid];
		vm = tsg->vm;
	} else {
		vm = ch->vm;
	}

	if (g->ops.gr.set_ctxsw_preemption_mode) {
		err = g->ops.gr.set_ctxsw_preemption_mode(g, gr_ctx, vm, class,
						graphics_preempt_mode, compute_preempt_mode);
		if (err) {
			gk20a_err(dev_from_gk20a(g),
					"set_ctxsw_preemption_mode failed");
			return err;
		}
	}

	if (gk20a_mem_begin(g, mem))
		return -ENOMEM;

	err = gk20a_disable_channel_tsg(g, ch);
	if (err)
		goto unmap_ctx;

	err = gk20a_fifo_preempt(g, ch);
	if (err)
		goto enable_ch;

	if (g->ops.gr.update_ctxsw_preemption_mode) {
		g->ops.gr.update_ctxsw_preemption_mode(ch->g, ch_ctx, mem);

		err = gr_gk20a_ctx_patch_write_begin(g, ch_ctx);
		if (err) {
			gk20a_err(dev_from_gk20a(g),
					"can't map patch context");
			goto enable_ch;
		}
		g->ops.gr.commit_global_cb_manager(g, ch, true);
		gr_gk20a_ctx_patch_write_end(g, ch_ctx);
	}

enable_ch:
	gk20a_enable_channel_tsg(g, ch);
unmap_ctx:
	gk20a_mem_end(g, mem);

	return err;
}

static int gr_gp10b_get_preemption_mode_flags(struct gk20a *g,
	struct nvgpu_preemption_modes_rec *preemption_modes_rec)
{
	preemption_modes_rec->graphics_preemption_mode_flags = (
			NVGPU_GRAPHICS_PREEMPTION_MODE_WFI |
			NVGPU_GRAPHICS_PREEMPTION_MODE_GFXP);
	preemption_modes_rec->compute_preemption_mode_flags = (
			NVGPU_COMPUTE_PREEMPTION_MODE_WFI |
			NVGPU_COMPUTE_PREEMPTION_MODE_CTA |
			NVGPU_COMPUTE_PREEMPTION_MODE_CILP);

	preemption_modes_rec->default_graphics_preempt_mode =
			NVGPU_GRAPHICS_PREEMPTION_MODE_WFI;
	preemption_modes_rec->default_compute_preempt_mode =
			NVGPU_COMPUTE_PREEMPTION_MODE_WFI;

	return 0;
}
static int gp10b_gr_fuse_override(struct gk20a *g)
{
	struct device_node *np = g->dev->of_node;
	u32 *fuses;
	int count, i;

	if (!np) /* may be pcie device */
		return 0;

	count = of_property_count_elems_of_size(np, "fuse-overrides", 8);
	if (count <= 0)
		return count;

	fuses = kmalloc(sizeof(u32) * count * 2, GFP_KERNEL);
	if (!fuses)
		return -ENOMEM;
	of_property_read_u32_array(np, "fuse-overrides", fuses, count * 2);
	for (i = 0; i < count; i++) {
		u32 fuse, value;

		fuse = fuses[2 * i];
		value = fuses[2 * i + 1];
		switch (fuse) {
		case GP10B_FUSE_OPT_ECC_EN:
			g->gr.t18x.fecs_feature_override_ecc_val = value;
			break;
		default:
			gk20a_err(dev_from_gk20a(g),
				"ignore unknown fuse override %08x", fuse);
			break;
		}
	}

	kfree(fuses);

	return 0;
}

static int gr_gp10b_init_preemption_state(struct gk20a *g)
{
	struct gk20a_platform *platform = gk20a_get_platform(g->dev);
	u32 debug_2;
	u64 sysclk_rate;
	u32 sysclk_cycles;

	sysclk_rate = platform->clk_get_rate(g->dev);
	sysclk_cycles = (u32)((sysclk_rate * NVGPU_GFXP_WFI_TIMEOUT_US) / 1000000ULL);
	gk20a_writel(g, gr_fe_gfxp_wfi_timeout_r(),
			gr_fe_gfxp_wfi_timeout_count_f(sysclk_cycles));

	debug_2 = gk20a_readl(g, gr_debug_2_r());
	debug_2 = set_field(debug_2,
			gr_debug_2_gfxp_wfi_always_injects_wfi_m(),
			gr_debug_2_gfxp_wfi_always_injects_wfi_enabled_f());
	gk20a_writel(g, gr_debug_2_r(), debug_2);

	return 0;
}

void gp10b_init_gr(struct gpu_ops *gops)
{
	gm20b_init_gr(gops);
	gops->gr.init_fs_state = gr_gp10b_init_fs_state;
	gops->gr.init_preemption_state = gr_gp10b_init_preemption_state;
	gops->gr.is_valid_class = gr_gp10b_is_valid_class;
	gops->gr.commit_global_cb_manager = gr_gp10b_commit_global_cb_manager;
	gops->gr.commit_global_pagepool = gr_gp10b_commit_global_pagepool;
	gops->gr.add_zbc_color = gr_gp10b_add_zbc_color;
	gops->gr.add_zbc_depth = gr_gp10b_add_zbc_depth;
	gops->gr.pagepool_default_size = gr_gp10b_pagepool_default_size;
	gops->gr.calc_global_ctx_buffer_size =
		gr_gp10b_calc_global_ctx_buffer_size;
	gops->gr.commit_global_attrib_cb = gr_gp10b_commit_global_attrib_cb;
	gops->gr.commit_global_bundle_cb = gr_gp10b_commit_global_bundle_cb;
	gops->gr.handle_sw_method = gr_gp10b_handle_sw_method;
	gops->gr.cb_size_default = gr_gp10b_cb_size_default;
	gops->gr.set_alpha_circular_buffer_size =
		gr_gp10b_set_alpha_circular_buffer_size;
	gops->gr.set_circular_buffer_size =
		gr_gp10b_set_circular_buffer_size;
	gops->gr.init_ctx_state = gr_gp10b_init_ctx_state;
	gops->gr.alloc_gr_ctx = gr_gp10b_alloc_gr_ctx;
	gops->gr.free_gr_ctx = gr_gp10b_free_gr_ctx;
	gops->gr.update_ctxsw_preemption_mode =
		gr_gp10b_update_ctxsw_preemption_mode;
	gops->gr.dump_gr_regs = gr_gp10b_dump_gr_status_regs;
	gops->gr.wait_empty = gr_gp10b_wait_empty;
	gops->gr.init_cyclestats = gr_gp10b_init_cyclestats;
	gops->gr.set_gpc_tpc_mask = gr_gp10b_set_gpc_tpc_mask;
	gops->gr.get_access_map = gr_gp10b_get_access_map;
	gops->gr.handle_sm_exception = gr_gp10b_handle_sm_exception;
	gops->gr.handle_tex_exception = gr_gp10b_handle_tex_exception;
	gops->gr.mask_hww_warp_esr = gp10b_mask_hww_warp_esr;
	gops->gr.pre_process_sm_exception =
		gr_gp10b_pre_process_sm_exception;
	gops->gr.handle_fecs_error = gr_gp10b_handle_fecs_error;
	gops->gr.create_gr_sysfs = gr_gp10b_create_sysfs;
	gops->gr.get_lrf_tex_ltc_dram_override = get_ecc_override_val;
	gops->gr.suspend_contexts = gr_gp10b_suspend_contexts;
	gops->gr.set_preemption_mode = gr_gp10b_set_preemption_mode;
	gops->gr.set_ctxsw_preemption_mode = gr_gp10b_set_ctxsw_preemption_mode;
	gops->gr.get_preemption_mode_flags = gr_gp10b_get_preemption_mode_flags;
	gops->gr.fuse_override = gp10b_gr_fuse_override;
	gops->gr.load_smid_config = gr_gp10b_load_smid_config;
}
