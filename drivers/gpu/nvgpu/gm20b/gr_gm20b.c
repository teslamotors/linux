/*
 * GM20B GPC MMU
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
 */

#include <linux/types.h>
#include <linux/delay.h>	/* for mdelay */
#include <linux/io.h>
#include <linux/tegra-fuse.h>
#include <linux/vmalloc.h>

#include <dt-bindings/soc/gm20b-fuse.h>

#include "gk20a/gk20a.h"
#include "gk20a/gr_gk20a.h"

#include "gr_gm20b.h"
#include "hw_gr_gm20b.h"
#include "hw_fifo_gm20b.h"
#include "hw_fb_gm20b.h"
#include "hw_top_gm20b.h"
#include "hw_ltc_gm20b.h"
#include "hw_ctxsw_prog_gm20b.h"
#include "hw_fuse_gm20b.h"
#include "pmu_gm20b.h"
#include "acr_gm20b.h"

static void gr_gm20b_init_gpc_mmu(struct gk20a *g)
{
	u32 temp;

	gk20a_dbg_info("initialize gpc mmu");

	if (!g->ops.privsecurity) {
		/* Bypass MMU check for non-secure boot. For
		 * secure-boot,this register write has no-effect */
		gk20a_writel(g, fb_priv_mmu_phy_secure_r(), 0xffffffff);
	}
	temp = gk20a_readl(g, fb_mmu_ctrl_r());
	temp &= gr_gpcs_pri_mmu_ctrl_vm_pg_size_m() |
		gr_gpcs_pri_mmu_ctrl_use_pdb_big_page_size_m() |
		gr_gpcs_pri_mmu_ctrl_use_full_comp_tag_line_m() |
		gr_gpcs_pri_mmu_ctrl_vol_fault_m() |
		gr_gpcs_pri_mmu_ctrl_comp_fault_m() |
		gr_gpcs_pri_mmu_ctrl_miss_gran_m() |
		gr_gpcs_pri_mmu_ctrl_cache_mode_m() |
		gr_gpcs_pri_mmu_ctrl_mmu_aperture_m() |
		gr_gpcs_pri_mmu_ctrl_mmu_vol_m() |
		gr_gpcs_pri_mmu_ctrl_mmu_disable_m();
	gk20a_writel(g, gr_gpcs_pri_mmu_ctrl_r(), temp);
	gk20a_writel(g, gr_gpcs_pri_mmu_pm_unit_mask_r(), 0);
	gk20a_writel(g, gr_gpcs_pri_mmu_pm_req_mask_r(), 0);

	gk20a_writel(g, gr_gpcs_pri_mmu_debug_ctrl_r(),
			gk20a_readl(g, fb_mmu_debug_ctrl_r()));
	gk20a_writel(g, gr_gpcs_pri_mmu_debug_wr_r(),
			gk20a_readl(g, fb_mmu_debug_wr_r()));
	gk20a_writel(g, gr_gpcs_pri_mmu_debug_rd_r(),
			gk20a_readl(g, fb_mmu_debug_rd_r()));

	gk20a_writel(g, gr_gpcs_mmu_num_active_ltcs_r(),
		gk20a_readl(g, fb_fbhub_num_active_ltcs_r()));
}

static void gr_gm20b_bundle_cb_defaults(struct gk20a *g)
{
	struct gr_gk20a *gr = &g->gr;

	gr->bundle_cb_default_size =
		gr_scc_bundle_cb_size_div_256b__prod_v();
	gr->min_gpm_fifo_depth =
		gr_pd_ab_dist_cfg2_state_limit_min_gpm_fifo_depths_v();
	gr->bundle_cb_token_limit =
		gr_pd_ab_dist_cfg2_token_limit_init_v();
}

static void gr_gm20b_cb_size_default(struct gk20a *g)
{
	struct gr_gk20a *gr = &g->gr;

	if (!gr->attrib_cb_default_size)
		gr->attrib_cb_default_size =
			gr_gpc0_ppc0_cbm_beta_cb_size_v_default_v();
	gr->alpha_cb_default_size =
		gr_gpc0_ppc0_cbm_alpha_cb_size_v_default_v();
}

static int gr_gm20b_calc_global_ctx_buffer_size(struct gk20a *g)
{
	struct gr_gk20a *gr = &g->gr;
	int size;

	gr->attrib_cb_size = gr->attrib_cb_default_size
		+ (gr->attrib_cb_default_size >> 1);
	gr->alpha_cb_size = gr->alpha_cb_default_size
		+ (gr->alpha_cb_default_size >> 1);

	size = gr->attrib_cb_size *
		gr_gpc0_ppc0_cbm_beta_cb_size_v_granularity_v() *
		gr->max_tpc_count;

	size += gr->alpha_cb_size *
		gr_gpc0_ppc0_cbm_alpha_cb_size_v_granularity_v() *
		gr->max_tpc_count;

	return size;
}

void gr_gm20b_commit_global_attrib_cb(struct gk20a *g,
				      struct channel_ctx_gk20a *ch_ctx,
				      u64 addr, bool patch)
{
	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_setup_attrib_cb_base_r(),
		gr_gpcs_setup_attrib_cb_base_addr_39_12_f(addr) |
		gr_gpcs_setup_attrib_cb_base_valid_true_f(), patch);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_tpcs_pe_pin_cb_global_base_addr_r(),
		gr_gpcs_tpcs_pe_pin_cb_global_base_addr_v_f(addr) |
		gr_gpcs_tpcs_pe_pin_cb_global_base_addr_valid_true_f(), patch);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_tpcs_mpc_vtg_cb_global_base_addr_r(),
		gr_gpcs_tpcs_mpc_vtg_cb_global_base_addr_v_f(addr) |
		gr_gpcs_tpcs_mpc_vtg_cb_global_base_addr_valid_true_f(), patch);
}

static void gr_gm20b_commit_global_bundle_cb(struct gk20a *g,
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

static int gr_gm20b_commit_global_cb_manager(struct gk20a *g,
			struct channel_gk20a *c, bool patch)
{
	struct gr_gk20a *gr = &g->gr;
	struct channel_ctx_gk20a *ch_ctx = &c->ch_ctx;
	u32 attrib_offset_in_chunk = 0;
	u32 alpha_offset_in_chunk = 0;
	u32 pd_ab_max_output;
	u32 gpc_index, ppc_index;
	u32 cbm_cfg_size1, cbm_cfg_size2;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 ppc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_PPC_IN_GPC_STRIDE);
	u32 num_pes_per_gpc = nvgpu_get_litter_value(g,
			GPU_LIT_NUM_PES_PER_GPC);

	gk20a_dbg_fn("");

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_ds_tga_constraintlogic_r(),
		gr_ds_tga_constraintlogic_beta_cbsize_f(gr->attrib_cb_default_size) |
		gr_ds_tga_constraintlogic_alpha_cbsize_f(gr->alpha_cb_default_size),
		patch);

	pd_ab_max_output = (gr->alpha_cb_default_size *
		gr_gpc0_ppc0_cbm_beta_cb_size_v_granularity_v()) /
		gr_pd_ab_dist_cfg1_max_output_granularity_v();

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_pd_ab_dist_cfg1_r(),
		gr_pd_ab_dist_cfg1_max_output_f(pd_ab_max_output) |
		gr_pd_ab_dist_cfg1_max_batches_init_f(), patch);

	alpha_offset_in_chunk = attrib_offset_in_chunk +
		gr->tpc_count * gr->attrib_cb_size;

	for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++) {
		u32 temp = gpc_stride * gpc_index;
		u32 temp2 = num_pes_per_gpc * gpc_index;
		for (ppc_index = 0; ppc_index < gr->gpc_ppc_count[gpc_index];
		     ppc_index++) {
			cbm_cfg_size1 = gr->attrib_cb_default_size *
				gr->pes_tpc_count[ppc_index][gpc_index];
			cbm_cfg_size2 = gr->alpha_cb_default_size *
				gr->pes_tpc_count[ppc_index][gpc_index];

			gr_gk20a_ctx_patch_write(g, ch_ctx,
				gr_gpc0_ppc0_cbm_beta_cb_size_r() + temp +
				ppc_in_gpc_stride * ppc_index,
				cbm_cfg_size1, patch);

			gr_gk20a_ctx_patch_write(g, ch_ctx,
				gr_gpc0_ppc0_cbm_beta_cb_offset_r() + temp +
				ppc_in_gpc_stride * ppc_index,
				attrib_offset_in_chunk, patch);

			attrib_offset_in_chunk += gr->attrib_cb_size *
				gr->pes_tpc_count[ppc_index][gpc_index];

			gr_gk20a_ctx_patch_write(g, ch_ctx,
				gr_gpc0_ppc0_cbm_alpha_cb_size_r() + temp +
				ppc_in_gpc_stride * ppc_index,
				cbm_cfg_size2, patch);

			gr_gk20a_ctx_patch_write(g, ch_ctx,
				gr_gpc0_ppc0_cbm_alpha_cb_offset_r() + temp +
				ppc_in_gpc_stride * ppc_index,
				alpha_offset_in_chunk, patch);

			alpha_offset_in_chunk += gr->alpha_cb_size *
				gr->pes_tpc_count[ppc_index][gpc_index];

			gr_gk20a_ctx_patch_write(g, ch_ctx,
				gr_gpcs_swdx_tc_beta_cb_size_r(ppc_index + temp2),
				gr_gpcs_swdx_tc_beta_cb_size_v_f(cbm_cfg_size1) |
				gr_gpcs_swdx_tc_beta_cb_size_div3_f(cbm_cfg_size1/3),
				patch);
		}
	}

	return 0;
}

static void gr_gm20b_commit_global_pagepool(struct gk20a *g,
					    struct channel_ctx_gk20a *ch_ctx,
					    u64 addr, u32 size, bool patch)
{
	gr_gk20a_commit_global_pagepool(g, ch_ctx, addr, size, patch);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_swdx_rm_pagepool_r(),
		gr_gpcs_swdx_rm_pagepool_total_pages_f(size) |
		gr_gpcs_swdx_rm_pagepool_valid_true_f(), patch);

}

static int gr_gm20b_handle_sw_method(struct gk20a *g, u32 addr,
					  u32 class_num, u32 offset, u32 data)
{
	gk20a_dbg_fn("");

	if (class_num == MAXWELL_COMPUTE_B) {
		switch (offset << 2) {
		case NVB1C0_SET_SHADER_EXCEPTIONS:
			gk20a_gr_set_shader_exceptions(g, data);
			break;
		default:
			goto fail;
		}
	}

	if (class_num == MAXWELL_B) {
		switch (offset << 2) {
		case NVB197_SET_SHADER_EXCEPTIONS:
			gk20a_gr_set_shader_exceptions(g, data);
			break;
		case NVB197_SET_CIRCULAR_BUFFER_SIZE:
			g->ops.gr.set_circular_buffer_size(g, data);
			break;
		case NVB197_SET_ALPHA_CIRCULAR_BUFFER_SIZE:
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

static void gr_gm20b_set_alpha_circular_buffer_size(struct gk20a *g, u32 data)
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

static void gr_gm20b_set_circular_buffer_size(struct gk20a *g, u32 data)
{
	struct gr_gk20a *gr = &g->gr;
	u32 gpc_index, ppc_index, stride, val;
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

			val = gk20a_readl(g, gr_gpcs_swdx_tc_beta_cb_size_r(
						ppc_index + gpc_index));

			val = set_field(val,
				gr_gpcs_swdx_tc_beta_cb_size_v_m(),
				gr_gpcs_swdx_tc_beta_cb_size_v_f(cb_size *
					gr->gpc_ppc_count[gpc_index]));
			val = set_field(val,
				gr_gpcs_swdx_tc_beta_cb_size_div3_m(),
				gr_gpcs_swdx_tc_beta_cb_size_div3_f((cb_size *
					gr->gpc_ppc_count[gpc_index])/3));

			gk20a_writel(g, gr_gpcs_swdx_tc_beta_cb_size_r(
						ppc_index + gpc_index), val);
		}
	}
}

static void gr_gm20b_set_hww_esr_report_mask(struct gk20a *g)
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
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_mmu_fault_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_stack_overflow_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_geometry_sm_error_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_divergent_report_f());

	/* setup sm global esr report mask */
	gk20a_writel(g, gr_gpcs_tpcs_sm_hww_global_esr_report_mask_r(),
		gr_gpcs_tpcs_sm_hww_global_esr_report_mask_sm_to_sm_fault_report_f() |
		gr_gpcs_tpcs_sm_hww_global_esr_report_mask_multiple_warp_errors_report_f());
}

static bool gr_gm20b_is_valid_class(struct gk20a *g, u32 class_num)
{
	bool valid = false;

	switch (class_num) {
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

	return valid;
}

/* Following are the blocks of registers that the ucode
 stores in the extended region.*/
/* ==  ctxsw_extended_sm_dsm_perf_counter_register_stride_v() ? */
static const u32 _num_sm_dsm_perf_regs;
/* ==  ctxsw_extended_sm_dsm_perf_counter_control_register_stride_v() ?*/
static const u32 _num_sm_dsm_perf_ctrl_regs = 2;
static u32 *_sm_dsm_perf_regs;
static u32 _sm_dsm_perf_ctrl_regs[2];

static void gr_gm20b_init_sm_dsm_reg_info(void)
{
	if (_sm_dsm_perf_ctrl_regs[0] != 0)
		return;

	_sm_dsm_perf_ctrl_regs[0] =
			      gr_pri_gpc0_tpc0_sm_dsm_perf_counter_control0_r();
	_sm_dsm_perf_ctrl_regs[1] =
			      gr_pri_gpc0_tpc0_sm_dsm_perf_counter_control5_r();
}

static void gr_gm20b_get_sm_dsm_perf_regs(struct gk20a *g,
					  u32 *num_sm_dsm_perf_regs,
					  u32 **sm_dsm_perf_regs,
					  u32 *perf_register_stride)
{
	*num_sm_dsm_perf_regs = _num_sm_dsm_perf_regs;
	*sm_dsm_perf_regs = _sm_dsm_perf_regs;
	*perf_register_stride = 0;
}

static void gr_gm20b_get_sm_dsm_perf_ctrl_regs(struct gk20a *g,
					       u32 *num_sm_dsm_perf_ctrl_regs,
					       u32 **sm_dsm_perf_ctrl_regs,
					       u32 *ctrl_register_stride)
{
	*num_sm_dsm_perf_ctrl_regs = _num_sm_dsm_perf_ctrl_regs;
	*sm_dsm_perf_ctrl_regs = _sm_dsm_perf_ctrl_regs;

	*ctrl_register_stride =
	    ctxsw_prog_extended_sm_dsm_perf_counter_control_register_stride_v();
}

static u32 gr_gm20b_get_gpc_tpc_mask(struct gk20a *g, u32 gpc_index)
{
	u32 val;
	struct gr_gk20a *gr = &g->gr;

	/* Toggle the bits of NV_FUSE_STATUS_OPT_TPC_GPC */
	val = gk20a_readl(g, fuse_status_opt_tpc_gpc_r(gpc_index));

	return (~val) & ((0x1 << gr->max_tpc_per_gpc_count) - 1);
}

static void gr_gm20b_set_gpc_tpc_mask(struct gk20a *g, u32 gpc_index)
{
	tegra_clk_writel(CLK_RST_CONTROLLER_MISC_CLK_ENB_0_ALL_VISIBLE,
			CLK_RST_CONTROLLER_MISC_CLK_ENB_0);

	tegra_fuse_writel(0x1, FUSE_FUSEBYPASS_0);
	tegra_fuse_writel(0x0, FUSE_WRITE_ACCESS_SW_0);

	if (g->gr.gpc_tpc_mask[gpc_index] == 0x1) {
		tegra_fuse_writel(0x0, FUSE_OPT_GPU_TPC0_DISABLE_0);
		tegra_fuse_writel(0x1, FUSE_OPT_GPU_TPC1_DISABLE_0);
	} else if (g->gr.gpc_tpc_mask[gpc_index] == 0x2) {
		tegra_fuse_writel(0x1, FUSE_OPT_GPU_TPC0_DISABLE_0);
		tegra_fuse_writel(0x0, FUSE_OPT_GPU_TPC1_DISABLE_0);
	} else {
		tegra_fuse_writel(0x0, FUSE_OPT_GPU_TPC0_DISABLE_0);
		tegra_fuse_writel(0x0, FUSE_OPT_GPU_TPC1_DISABLE_0);
	}
}

static void gr_gm20b_load_tpc_mask(struct gk20a *g)
{
	u32 pes_tpc_mask = 0, fuse_tpc_mask;
	u32 gpc, pes;
	u32 num_tpc_per_gpc = nvgpu_get_litter_value(g, GPU_LIT_NUM_TPC_PER_GPC);

	for (gpc = 0; gpc < g->gr.gpc_count; gpc++)
		for (pes = 0; pes < g->gr.pe_count_per_gpc; pes++) {
			pes_tpc_mask |= g->gr.pes_tpc_mask[pes][gpc] <<
					num_tpc_per_gpc * gpc;
		}

	fuse_tpc_mask = g->ops.gr.get_gpc_tpc_mask(g, 0);
	if (g->tpc_fs_mask_user && g->tpc_fs_mask_user != fuse_tpc_mask &&
		fuse_tpc_mask == (0x1 << g->gr.max_tpc_count) - 1) {
		u32 val = g->tpc_fs_mask_user;
		val &= (0x1 << g->gr.max_tpc_count) - 1;
		/* skip tpc to disable the other tpc cause channel timeout */
		val = (0x1 << hweight32(val)) - 1;
		gk20a_writel(g, gr_fe_tpc_fs_r(), val);
	} else {
		gk20a_writel(g, gr_fe_tpc_fs_r(), pes_tpc_mask);
	}
}

static void gr_gm20b_program_sm_id_numbering(struct gk20a *g,
					     u32 gpc, u32 tpc, u32 smid)
{
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 gpc_offset = gpc_stride * gpc;
	u32 tpc_offset = tpc_in_gpc_stride * tpc;

	gk20a_writel(g, gr_gpc0_tpc0_sm_cfg_r() + gpc_offset + tpc_offset,
			gr_gpc0_tpc0_sm_cfg_sm_id_f(smid));
	gk20a_writel(g, gr_gpc0_gpm_pd_sm_id_r(tpc) + gpc_offset,
			gr_gpc0_gpm_pd_sm_id_id_f(smid));
	gk20a_writel(g, gr_gpc0_tpc0_pe_cfg_smid_r() + gpc_offset + tpc_offset,
			gr_gpc0_tpc0_pe_cfg_smid_value_f(smid));
}

static int gr_gm20b_load_smid_config(struct gk20a *g)
{
	u32 *tpc_sm_id;
	u32 i, j;
	u32 tpc_index, gpc_index;

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

			tpc_sm_id[gpc_index] |= sm_id << tpc_index * bit_stride;
		}
		gk20a_writel(g, gr_cwd_gpc_tpc_id_r(i), reg);
	}

	for (i = 0; i < gr_cwd_sm_id__size_1_v(); i++)
		gk20a_writel(g, gr_cwd_sm_id_r(i), tpc_sm_id[i]);

	kfree(tpc_sm_id);

	return 0;
}

int gr_gm20b_init_fs_state(struct gk20a *g)
{
	gk20a_dbg_fn("");

	gr_gk20a_init_fs_state(g);

	gr_gm20b_load_tpc_mask(g);

	gk20a_writel(g, gr_bes_zrop_settings_r(),
		     gr_bes_zrop_settings_num_active_ltcs_f(g->ltc_count));
	gk20a_writel(g, gr_bes_crop_settings_r(),
		     gr_bes_crop_settings_num_active_ltcs_f(g->ltc_count));

	gk20a_writel(g, gr_bes_crop_debug3_r(),
		     gk20a_readl(g, gr_be0_crop_debug3_r()) |
		     gr_bes_crop_debug3_comp_vdc_4to2_disable_m());

	g->ops.gr.load_smid_config(g);

	return 0;
}

static int gr_gm20b_load_ctxsw_ucode_segments(struct gk20a *g, u64 addr_base,
	struct gk20a_ctxsw_ucode_segments *segments, u32 reg_offset)
{
	gk20a_writel(g, reg_offset + gr_fecs_dmactl_r(),
			gr_fecs_dmactl_require_ctx_f(0));

	/* Copy falcon bootloader into dmem */
	gr_gk20a_load_ctxsw_ucode_header(g, addr_base, segments, reg_offset);
	gr_gk20a_load_ctxsw_ucode_boot(g, addr_base, segments, reg_offset);

	/* start the falcon immediately if PRIV security is disabled*/
	if (!g->ops.privsecurity) {
		gk20a_writel(g, reg_offset + gr_fecs_cpuctl_r(),
				gr_fecs_cpuctl_startcpu_f(0x01));
	}

	return 0;
}

static bool gr_gm20b_is_tpc_addr_shared(struct gk20a *g, u32 addr)
{
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 tpc_in_gpc_shared_base = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_SHARED_BASE);
	return (addr >= tpc_in_gpc_shared_base) &&
		(addr < (tpc_in_gpc_shared_base +
			 tpc_in_gpc_stride));
}

static bool gr_gm20b_is_tpc_addr(struct gk20a *g, u32 addr)
{
	u32 tpc_in_gpc_base = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_BASE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 num_tpc_per_gpc = nvgpu_get_litter_value(g, GPU_LIT_NUM_TPC_PER_GPC);
	return ((addr >= tpc_in_gpc_base) &&
		(addr < tpc_in_gpc_base +
		 (num_tpc_per_gpc * tpc_in_gpc_stride)))
		|| gr_gm20b_is_tpc_addr_shared(g, addr);
}

static u32 gr_gm20b_get_tpc_num(struct gk20a *g, u32 addr)
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

#ifdef CONFIG_TEGRA_ACR
static void gr_gm20b_load_gpccs_with_bootloader(struct gk20a *g)
{
	struct gk20a_ctxsw_ucode_info *ucode_info = &g->ctxsw_ucode_info;
	u64 addr_base = ucode_info->surface_desc.gpu_va;

	gr_gk20a_load_falcon_bind_instblk(g);

	g->ops.gr.falcon_load_ucode(g, addr_base,
		&g->ctxsw_ucode_info.gpccs,
		gr_gpcs_gpccs_falcon_hwcfg_r() -
		gr_fecs_falcon_hwcfg_r());
}

static int gr_gm20b_load_ctxsw_ucode(struct gk20a *g)
{
	u32 err, flags;
	u32 reg_offset = gr_gpcs_gpccs_falcon_hwcfg_r() -
	  gr_fecs_falcon_hwcfg_r();
	u8 falcon_id_mask = 0;

	gk20a_dbg_fn("");

	if (tegra_platform_is_linsim()) {
		gk20a_writel(g, gr_fecs_ctxsw_mailbox_r(7),
			gr_fecs_ctxsw_mailbox_value_f(0xc0de7777));
		gk20a_writel(g, gr_gpccs_ctxsw_mailbox_r(7),
			gr_gpccs_ctxsw_mailbox_value_f(0xc0de7777));
	}

	flags = PMU_ACR_CMD_BOOTSTRAP_FALCON_FLAGS_RESET_YES;
	g->ops.pmu.lsfloadedfalconid = 0;
	if (g->ops.pmu.fecsbootstrapdone) {
		/* this must be recovery so bootstrap fecs and gpccs */
		if (!g->ops.securegpccs) {
			gr_gm20b_load_gpccs_with_bootloader(g);
			err = g->ops.pmu.load_lsfalcon_ucode(g,
					(1 << LSF_FALCON_ID_FECS));
		} else {
			/* bind WPR VA inst block */
			gr_gk20a_load_falcon_bind_instblk(g);
			err = g->ops.pmu.load_lsfalcon_ucode(g,
				(1 << LSF_FALCON_ID_FECS) |
				(1 << LSF_FALCON_ID_GPCCS));
		}
		if (err) {
			gk20a_err(dev_from_gk20a(g),
				"Unable to recover GR falcon");
			return err;
		}

	} else {
		/* cold boot or rg exit */
		g->ops.pmu.fecsbootstrapdone = true;
		if (!g->ops.securegpccs) {
			gr_gm20b_load_gpccs_with_bootloader(g);
		} else {
			/* bind WPR VA inst block */
			gr_gk20a_load_falcon_bind_instblk(g);
			if (g->ops.pmu.is_lazy_bootstrap(LSF_FALCON_ID_FECS))
				falcon_id_mask |= (1 << LSF_FALCON_ID_FECS);
			if (g->ops.pmu.is_lazy_bootstrap(LSF_FALCON_ID_GPCCS))
				falcon_id_mask |= (1 << LSF_FALCON_ID_GPCCS);

			err = g->ops.pmu.load_lsfalcon_ucode(g, falcon_id_mask);

			if (err) {
				gk20a_err(dev_from_gk20a(g),
						"Unable to boot GPCCS\n");
				return err;
			}
		}
	}

	/*start gpccs */
	if (g->ops.securegpccs) {
		gk20a_writel(g, reg_offset +
			gr_fecs_cpuctl_alias_r(),
			gr_gpccs_cpuctl_startcpu_f(1));
	} else {
		gk20a_writel(g, gr_gpccs_dmactl_r(),
			gr_gpccs_dmactl_require_ctx_f(0));
		gk20a_writel(g, gr_gpccs_cpuctl_r(),
			gr_gpccs_cpuctl_startcpu_f(1));
	}
	/* start fecs */
	gk20a_writel(g, gr_fecs_ctxsw_mailbox_clear_r(0), ~0x0);
	gk20a_writel(g, gr_fecs_ctxsw_mailbox_r(1), 0x1);
	gk20a_writel(g, gr_fecs_ctxsw_mailbox_clear_r(6), 0xffffffff);
	gk20a_writel(g, gr_fecs_cpuctl_alias_r(),
			gr_fecs_cpuctl_startcpu_f(1));
	gk20a_dbg_fn("done");

	return 0;
}
#else

static int gr_gm20b_load_ctxsw_ucode(struct gk20a *g)
{
	return -EPERM;
}

#endif

static void gr_gm20b_detect_sm_arch(struct gk20a *g)
{
	u32 v = gk20a_readl(g, gr_gpc0_tpc0_sm_arch_r());

	g->gpu_characteristics.sm_arch_spa_version =
		gr_gpc0_tpc0_sm_arch_spa_version_v(v);
	g->gpu_characteristics.sm_arch_sm_version =
		gr_gpc0_tpc0_sm_arch_sm_version_v(v);
	g->gpu_characteristics.sm_arch_warp_count =
		gr_gpc0_tpc0_sm_arch_warp_count_v(v);
}

static u32 gr_gm20b_pagepool_default_size(struct gk20a *g)
{
	return gr_scc_pagepool_total_pages_hwmax_value_v();
}

static int gr_gm20b_alloc_gr_ctx(struct gk20a *g,
			  struct gr_ctx_desc **gr_ctx, struct vm_gk20a *vm,
			  u32 class,
			  u32 flags)
{
	int err;

	gk20a_dbg_fn("");

	err = gr_gk20a_alloc_gr_ctx(g, gr_ctx, vm, class, flags);
	if (err)
		return err;

	if (class == MAXWELL_COMPUTE_B)
		(*gr_ctx)->compute_preempt_mode = NVGPU_COMPUTE_PREEMPTION_MODE_CTA;

	gk20a_dbg_fn("done");

	return 0;
}

static void gr_gm20b_update_ctxsw_preemption_mode(struct gk20a *g,
		struct channel_ctx_gk20a *ch_ctx,
		struct mem_desc *mem)
{
	struct gr_ctx_desc *gr_ctx = ch_ctx->gr_ctx;
	u32 cta_preempt_option =
		ctxsw_prog_main_image_preemption_options_control_cta_enabled_f();

	gk20a_dbg_fn("");

	if (gr_ctx->compute_preempt_mode == NVGPU_COMPUTE_PREEMPTION_MODE_CTA) {
		gk20a_dbg_info("CTA: %x", cta_preempt_option);
		gk20a_mem_wr(g, mem,
				ctxsw_prog_main_image_preemption_options_o(),
				cta_preempt_option);
	}

	gk20a_dbg_fn("done");
}

static int gr_gm20b_dump_gr_status_regs(struct gk20a *g,
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

static int gr_gm20b_update_pc_sampling(struct channel_gk20a *c,
				       bool enable)
{
	struct channel_ctx_gk20a *ch_ctx = &c->ch_ctx;
	struct mem_desc *mem;
	u32 v;

	gk20a_dbg_fn("");

	if (!ch_ctx || !ch_ctx->gr_ctx || c->vpr)
		return -EINVAL;

	mem = &ch_ctx->gr_ctx->mem;

	if (gk20a_mem_begin(c->g, mem))
		return -ENOMEM;

	v = gk20a_mem_rd(c->g, mem, ctxsw_prog_main_image_pm_o());
	v &= ~ctxsw_prog_main_image_pm_pc_sampling_m();
	v |= ctxsw_prog_main_image_pm_pc_sampling_f(enable);
	gk20a_mem_wr(c->g, mem, ctxsw_prog_main_image_pm_o(), v);

	gk20a_mem_end(c->g, mem);

	gk20a_dbg_fn("done");

	return 0;
}

static u32 gr_gm20b_get_fbp_en_mask(struct gk20a *g)
{
	u32 fbp_en_mask, opt_fbio;
	u32 tmp, max_fbps_count;

	tmp = gk20a_readl(g, top_num_fbps_r());
	max_fbps_count = top_num_fbps_value_v(tmp);

	opt_fbio = gk20a_readl(g, fuse_status_opt_fbio_r());
	fbp_en_mask =
		((1 << max_fbps_count) - 1) ^
		fuse_status_opt_fbio_data_v(opt_fbio);
	return fbp_en_mask;
}

static u32 gr_gm20b_get_max_ltc_per_fbp(struct gk20a *g)
{
	u32 ltc_per_fbp, reg;
	reg = gk20a_readl(g,  top_ltc_per_fbp_r());
	ltc_per_fbp = top_ltc_per_fbp_value_v(reg);
	return ltc_per_fbp;
}

static u32 gr_gm20b_get_max_lts_per_ltc(struct gk20a *g)
{
	u32 lts_per_ltc, reg;
	reg = gk20a_readl(g,  top_slices_per_ltc_r());
	lts_per_ltc = top_slices_per_ltc_value_v(reg);
	return lts_per_ltc;
}

static u32 *gr_gm20b_rop_l2_en_mask(struct gk20a *g)
{
	struct gr_gk20a *gr = &g->gr;
	u32 i, tmp, max_fbps_count, max_ltc_per_fbp;
	u32 rop_l2_all_en;

	tmp = gk20a_readl(g, top_num_fbps_r());
	max_fbps_count = top_num_fbps_value_v(tmp);
	max_ltc_per_fbp = gr_gm20b_get_max_ltc_per_fbp(g);
	rop_l2_all_en = (1 << max_ltc_per_fbp) - 1;

	/* mask of Rop_L2 for each FBP */
	for (i = 0; i < max_fbps_count; i++) {
		tmp = gk20a_readl(g, fuse_status_opt_rop_l2_fbp_r(i));
		gr->fbp_rop_l2_en_mask[i] = rop_l2_all_en ^ tmp;
	}

	return gr->fbp_rop_l2_en_mask;
}

static u32 gr_gm20b_get_max_fbps_count(struct gk20a *g)
{
	u32 tmp, max_fbps_count;
	tmp = gk20a_readl(g, top_num_fbps_r());
	max_fbps_count = top_num_fbps_value_v(tmp);
	return max_fbps_count;
}

static void gr_gm20b_init_cyclestats(struct gk20a *g)
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

static void gr_gm20b_enable_cde_in_fecs(struct gk20a *g, struct mem_desc *mem)
{
	u32 cde_v;

	cde_v = gk20a_mem_rd(g, mem, ctxsw_prog_main_image_ctl_o());
	cde_v |=  ctxsw_prog_main_image_ctl_cde_enabled_f();
	gk20a_mem_wr(g, mem, ctxsw_prog_main_image_ctl_o(), cde_v);
}

static void gr_gm20b_bpt_reg_info(struct gk20a *g, struct warpstate *w_state)
{
	/* Check if we have at least one valid warp */
	/* get paused state on maxwell */
	struct gr_gk20a *gr = &g->gr;
	u32 gpc, tpc, sm_id;
	u32  tpc_offset, gpc_offset, reg_offset;
	u64 warps_valid = 0, warps_paused = 0, warps_trapped = 0;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);

	/* for maxwell & kepler */
	u32 numSmPerTpc = 1;
	u32 numWarpPerTpc = g->gpu_characteristics.sm_arch_warp_count * numSmPerTpc;

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


		if (numWarpPerTpc > 64) {
			/* 64 bit read */
			warps_valid = (u64)gk20a_readl(g, gr_gpc0_tpc0_sm_warp_valid_mask_2_r() + reg_offset + 4) << 32;
			warps_valid |= gk20a_readl(g, gr_gpc0_tpc0_sm_warp_valid_mask_2_r() + reg_offset);

			/* 64 bit read */
			warps_paused = (u64)gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_bpt_pause_mask_2_r() + reg_offset + 4) << 32;
			warps_paused |= gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_bpt_pause_mask_2_r() + reg_offset);

			/* 64 bit read */
			warps_trapped = (u64)gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_bpt_trap_mask_2_r() + reg_offset + 4) << 32;
			warps_trapped |= gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_bpt_trap_mask_2_r() + reg_offset);

			w_state[sm_id].valid_warps[1] = warps_valid;
			w_state[sm_id].trapped_warps[1] = warps_trapped;
			w_state[sm_id].paused_warps[1] = warps_paused;
		}
	}


	/* Only for debug purpose */
	for (sm_id = 0; sm_id < gr->no_of_sm; sm_id++) {
		gk20a_dbg_fn("w_state[%d].valid_warps[0]: %llx\n",
						sm_id, w_state[sm_id].valid_warps[0]);
		gk20a_dbg_fn("w_state[%d].valid_warps[1]: %llx\n",
						sm_id, w_state[sm_id].valid_warps[1]);

		gk20a_dbg_fn("w_state[%d].trapped_warps[0]: %llx\n",
							sm_id, w_state[sm_id].trapped_warps[0]);
		gk20a_dbg_fn("w_state[%d].trapped_warps[1]: %llx\n",
						sm_id, w_state[sm_id].trapped_warps[1]);

		gk20a_dbg_fn("w_state[%d].paused_warps[0]: %llx\n",
						sm_id, w_state[sm_id].paused_warps[0]);
		gk20a_dbg_fn("w_state[%d].paused_warps[1]: %llx\n",
						sm_id, w_state[sm_id].paused_warps[1]);
	}
}

static void gr_gm20b_get_access_map(struct gk20a *g,
				   u32 **whitelist, int *num_entries)
{
	static u32 wl_addr_gm20b[] = {
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

	*whitelist = wl_addr_gm20b;
	*num_entries = ARRAY_SIZE(wl_addr_gm20b);
}

static int gm20b_gr_record_sm_error_state(struct gk20a *g, u32 gpc, u32 tpc)
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
	gr->sm_error_states[sm_id].hww_warp_esr_pc = gk20a_readl(g,
			gr_gpc0_tpc0_sm_hww_warp_esr_pc_r() + offset);
	gr->sm_error_states[sm_id].hww_global_esr_report_mask = gk20a_readl(g,
		       gr_gpc0_tpc0_sm_hww_global_esr_report_mask_r() + offset);
	gr->sm_error_states[sm_id].hww_warp_esr_report_mask = gk20a_readl(g,
			gr_gpc0_tpc0_sm_hww_warp_esr_report_mask_r() + offset);

	mutex_unlock(&g->dbg_sessions_lock);

	return 0;
}

static int gm20b_gr_update_sm_error_state(struct gk20a *g,
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
	gr->sm_error_states[sm_id].hww_warp_esr_pc =
			sm_error_state->hww_warp_esr_pc;
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
		gk20a_writel(g, gr_gpc0_tpc0_sm_hww_warp_esr_pc_r() + offset,
				gr->sm_error_states[sm_id].hww_warp_esr_pc);
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

static int gm20b_gr_clear_sm_error_state(struct gk20a *g,
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

static int gr_gm20b_get_preemption_mode_flags(struct gk20a *g,
		struct nvgpu_preemption_modes_rec *preemption_modes_rec)
{
	preemption_modes_rec->graphics_preemption_mode_flags =
			NVGPU_GRAPHICS_PREEMPTION_MODE_WFI;
	preemption_modes_rec->compute_preemption_mode_flags = (
			NVGPU_COMPUTE_PREEMPTION_MODE_WFI |
			NVGPU_COMPUTE_PREEMPTION_MODE_CTA);

	preemption_modes_rec->default_graphics_preempt_mode =
			NVGPU_GRAPHICS_PREEMPTION_MODE_WFI;
	preemption_modes_rec->default_compute_preempt_mode =
			NVGPU_COMPUTE_PREEMPTION_MODE_CTA;

	return 0;
}

static int gm20b_gr_tpc_disable_override(struct gk20a *g, u32 mask)
{
	if (!mask)
		return 0;

	g->tpc_fs_mask_user = ~mask;

	return 0;
}

static int gm20b_gr_fuse_override(struct gk20a *g)
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
		case GM20B_FUSE_OPT_TPC_DISABLE:
			gm20b_gr_tpc_disable_override(g, value);
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

static bool gr_gm20b_is_ltcs_ltss_addr(struct gk20a *g, u32 addr)
{
	u32 ltc_shared_base = ltc_ltcs_ltss_v();
	u32 lts_stride = nvgpu_get_litter_value(g, GPU_LIT_LTS_STRIDE);

	return (addr >= ltc_shared_base) &&
		(addr < (ltc_shared_base + lts_stride));
}

static bool gr_gm20b_is_ltcn_ltss_addr(struct gk20a *g, u32 addr)
{
	u32 lts_shared_base = ltc_ltc0_ltss_v();
	u32 lts_stride = nvgpu_get_litter_value(g, GPU_LIT_LTS_STRIDE);
	u32 addr_mask = nvgpu_get_litter_value(g, GPU_LIT_LTC_STRIDE) - 1;
	u32 base_offset = lts_shared_base & addr_mask;
	u32 end_offset = base_offset + lts_stride;

	return (!gr_gm20b_is_ltcs_ltss_addr(g, addr)) &&
		((addr & addr_mask) >= base_offset) &&
		((addr & addr_mask) < end_offset);
}

static void gr_gm20b_update_ltc_lts_addr(struct gk20a *g, u32 addr, u32 ltc_num,
					u32 *priv_addr_table,
					u32 *priv_addr_table_index)
{
	u32 num_ltc_slices = g->ops.gr.get_max_lts_per_ltc(g);
	u32 index = *priv_addr_table_index;
	u32 lts_num;
	u32 ltc_stride = nvgpu_get_litter_value(g, GPU_LIT_LTC_STRIDE);
	u32 lts_stride = nvgpu_get_litter_value(g, GPU_LIT_LTS_STRIDE);

	for (lts_num = 0; lts_num < num_ltc_slices; lts_num++)
		priv_addr_table[index++] = ltc_ltc0_lts0_v() +
						ltc_num * ltc_stride +
						lts_num * lts_stride +
						(addr & (lts_stride - 1));

	*priv_addr_table_index = index;
}

static void gr_gm20b_split_lts_broadcast_addr(struct gk20a *g, u32 addr,
					u32 *priv_addr_table,
					u32 *priv_addr_table_index)
{
	u32 num_ltc = g->ltc_count;
	u32 i, start, ltc_num = 0;
	u32 pltcg_base = ltc_pltcg_base_v();
	u32 ltc_stride = nvgpu_get_litter_value(g, GPU_LIT_LTC_STRIDE);

	for (i = 0; i < num_ltc; i++) {
		start = pltcg_base + i * ltc_stride;
		if ((addr >= start) && (addr < (start + ltc_stride))) {
			ltc_num = i;
			break;
		}
	}
	gr_gm20b_update_ltc_lts_addr(g, addr, ltc_num, priv_addr_table,
				priv_addr_table_index);
}

static void gr_gm20b_split_ltc_broadcast_addr(struct gk20a *g, u32 addr,
					u32 *priv_addr_table,
					u32 *priv_addr_table_index)
{
	u32 num_ltc = g->ltc_count;
	u32 ltc_num;

	for (ltc_num = 0; ltc_num < num_ltc; ltc_num++)
		gr_gm20b_update_ltc_lts_addr(g, addr, ltc_num,
					priv_addr_table, priv_addr_table_index);
}

void gm20b_init_gr(struct gpu_ops *gops)
{
	gops->gr.init_gpc_mmu = gr_gm20b_init_gpc_mmu;
	gops->gr.bundle_cb_defaults = gr_gm20b_bundle_cb_defaults;
	gops->gr.cb_size_default = gr_gm20b_cb_size_default;
	gops->gr.calc_global_ctx_buffer_size =
		gr_gm20b_calc_global_ctx_buffer_size;
	gops->gr.commit_global_attrib_cb = gr_gm20b_commit_global_attrib_cb;
	gops->gr.commit_global_bundle_cb = gr_gm20b_commit_global_bundle_cb;
	gops->gr.commit_global_cb_manager = gr_gm20b_commit_global_cb_manager;
	gops->gr.commit_global_pagepool = gr_gm20b_commit_global_pagepool;
	gops->gr.handle_sw_method = gr_gm20b_handle_sw_method;
	gops->gr.set_alpha_circular_buffer_size = gr_gm20b_set_alpha_circular_buffer_size;
	gops->gr.set_circular_buffer_size = gr_gm20b_set_circular_buffer_size;
	gops->gr.enable_hww_exceptions = gr_gk20a_enable_hww_exceptions;
	gops->gr.is_valid_class = gr_gm20b_is_valid_class;
	gops->gr.get_sm_dsm_perf_regs = gr_gm20b_get_sm_dsm_perf_regs;
	gops->gr.get_sm_dsm_perf_ctrl_regs = gr_gm20b_get_sm_dsm_perf_ctrl_regs;
	gops->gr.init_fs_state = gr_gm20b_init_fs_state;
	gops->gr.set_hww_esr_report_mask = gr_gm20b_set_hww_esr_report_mask;
	gops->gr.falcon_load_ucode = gr_gm20b_load_ctxsw_ucode_segments;
	if (gops->privsecurity)
		gops->gr.load_ctxsw_ucode = gr_gm20b_load_ctxsw_ucode;
	else
		gops->gr.load_ctxsw_ucode = gr_gk20a_load_ctxsw_ucode;
	gops->gr.set_gpc_tpc_mask = gr_gm20b_set_gpc_tpc_mask;
	gops->gr.get_gpc_tpc_mask = gr_gm20b_get_gpc_tpc_mask;
	gops->gr.free_channel_ctx = gk20a_free_channel_ctx;
	gops->gr.alloc_obj_ctx = gk20a_alloc_obj_ctx;
	gops->gr.bind_ctxsw_zcull = gr_gk20a_bind_ctxsw_zcull;
	gops->gr.get_zcull_info = gr_gk20a_get_zcull_info;
	gops->gr.is_tpc_addr = gr_gm20b_is_tpc_addr;
	gops->gr.get_tpc_num = gr_gm20b_get_tpc_num;
	gops->gr.detect_sm_arch = gr_gm20b_detect_sm_arch;
	gops->gr.add_zbc_color = gr_gk20a_add_zbc_color;
	gops->gr.add_zbc_depth = gr_gk20a_add_zbc_depth;
	gops->gr.zbc_set_table = gk20a_gr_zbc_set_table;
	gops->gr.zbc_query_table = gr_gk20a_query_zbc;
	gops->gr.pmu_save_zbc = gk20a_pmu_save_zbc;
	gops->gr.add_zbc = gr_gk20a_add_zbc;
	gops->gr.pagepool_default_size = gr_gm20b_pagepool_default_size;
	gops->gr.init_ctx_state = gr_gk20a_init_ctx_state;
	gops->gr.alloc_gr_ctx = gr_gm20b_alloc_gr_ctx;
	gops->gr.free_gr_ctx = gr_gk20a_free_gr_ctx;
	gops->gr.update_ctxsw_preemption_mode =
		gr_gm20b_update_ctxsw_preemption_mode;
	gops->gr.dump_gr_regs = gr_gm20b_dump_gr_status_regs;
	gops->gr.update_pc_sampling = gr_gm20b_update_pc_sampling;
	gops->gr.get_fbp_en_mask = gr_gm20b_get_fbp_en_mask;
	gops->gr.get_max_ltc_per_fbp = gr_gm20b_get_max_ltc_per_fbp;
	gops->gr.get_max_lts_per_ltc = gr_gm20b_get_max_lts_per_ltc;
	gops->gr.get_rop_l2_en_mask = gr_gm20b_rop_l2_en_mask;
	gops->gr.get_max_fbps_count = gr_gm20b_get_max_fbps_count;
	gops->gr.init_sm_dsm_reg_info = gr_gm20b_init_sm_dsm_reg_info;
	gops->gr.wait_empty = gr_gk20a_wait_idle;
	gops->gr.init_cyclestats = gr_gm20b_init_cyclestats;
	gops->gr.set_sm_debug_mode = gr_gk20a_set_sm_debug_mode;
	gops->gr.enable_cde_in_fecs = gr_gm20b_enable_cde_in_fecs;
	gops->gr.bpt_reg_info = gr_gm20b_bpt_reg_info;
	gops->gr.get_access_map = gr_gm20b_get_access_map;
	gops->gr.handle_fecs_error = gk20a_gr_handle_fecs_error;
	gops->gr.mask_hww_warp_esr = gk20a_mask_hww_warp_esr;
	gops->gr.handle_sm_exception = gr_gk20a_handle_sm_exception;
	gops->gr.handle_tex_exception = gr_gk20a_handle_tex_exception;
	gops->gr.get_lrf_tex_ltc_dram_override = NULL;
	gops->gr.update_smpc_ctxsw_mode = gr_gk20a_update_smpc_ctxsw_mode;
	gops->gr.update_hwpm_ctxsw_mode = gr_gk20a_update_hwpm_ctxsw_mode;
	gops->gr.record_sm_error_state = gm20b_gr_record_sm_error_state;
	gops->gr.update_sm_error_state = gm20b_gr_update_sm_error_state;
	gops->gr.clear_sm_error_state = gm20b_gr_clear_sm_error_state;
	gops->gr.suspend_contexts = gr_gk20a_suspend_contexts;
	gops->gr.get_preemption_mode_flags = gr_gm20b_get_preemption_mode_flags;
	gops->gr.fuse_override = gm20b_gr_fuse_override;
	gops->gr.load_smid_config = gr_gm20b_load_smid_config;
	gops->gr.program_sm_id_numbering = gr_gm20b_program_sm_id_numbering;
	gops->gr.is_ltcs_ltss_addr = gr_gm20b_is_ltcs_ltss_addr;
	gops->gr.is_ltcn_ltss_addr = gr_gm20b_is_ltcn_ltss_addr;
	gops->gr.split_lts_broadcast_addr = gr_gm20b_split_lts_broadcast_addr;
	gops->gr.split_ltc_broadcast_addr = gr_gm20b_split_ltc_broadcast_addr;
}
