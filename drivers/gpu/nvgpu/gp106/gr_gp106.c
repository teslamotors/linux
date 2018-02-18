/*
 * GP106 GPU GR
 *
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

#include "gk20a/gk20a.h" /* FERMI and MAXWELL classes defined here */

#include "gk20a/gr_gk20a.h"

#include "gm20b/gr_gm20b.h" /* for MAXWELL classes */
#include "gp10b/gr_gp10b.h"
#include "gr_gp106.h"
#include "hw_gr_gp106.h"

static bool gr_gp106_is_valid_class(struct gk20a *g, u32 class_num)
{
	bool valid = false;

	switch (class_num) {
	case PASCAL_COMPUTE_A:
	case PASCAL_COMPUTE_B:
	case PASCAL_A:
	case PASCAL_B:
	case PASCAL_DMA_COPY_A:
	case PASCAL_DMA_COPY_B:
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

static u32 gr_gp106_pagepool_default_size(struct gk20a *g)
{
	return gr_scc_pagepool_total_pages_hwmax_value_v();
}

static void gr_gp106_set_go_idle_timeout(struct gk20a *g, u32 data)
{
	gk20a_writel(g, gr_fe_go_idle_timeout_r(), data);
}


static int gr_gp106_handle_sw_method(struct gk20a *g, u32 addr,
				     u32 class_num, u32 offset, u32 data)
{
	gk20a_dbg_fn("");

	if (class_num == PASCAL_COMPUTE_B) {
		switch (offset << 2) {
		case NVC0C0_SET_SHADER_EXCEPTIONS:
			gk20a_gr_set_shader_exceptions(g, data);
			break;
		default:
			goto fail;
		}
	}

	if (class_num == PASCAL_B) {
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
			gr_gp106_set_go_idle_timeout(g, data);
			break;
		default:
			goto fail;
		}
	}
	return 0;

fail:
	return -EINVAL;
}

static void gr_gp106_cb_size_default(struct gk20a *g)
{
	struct gr_gk20a *gr = &g->gr;

	if (!gr->attrib_cb_default_size)
		gr->attrib_cb_default_size = 0x800;
	gr->alpha_cb_default_size =
		gr_gpc0_ppc0_cbm_alpha_cb_size_v_default_v();
}

static int gr_gp106_set_ctxsw_preemption_mode(struct gk20a *g,
				struct gr_ctx_desc *gr_ctx,
				struct vm_gk20a *vm, u32 class,
				u32 graphics_preempt_mode,
				u32 compute_preempt_mode)
{
	int err = 0;

	if (class == PASCAL_B && g->gr.t18x.ctx_vars.force_preemption_gfxp)
		graphics_preempt_mode = NVGPU_GRAPHICS_PREEMPTION_MODE_GFXP;

	if (class == PASCAL_COMPUTE_B &&
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

	if (class == PASCAL_COMPUTE_B) {
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

void gp106_init_gr(struct gpu_ops *gops)
{
	gp10b_init_gr(gops);
	gops->gr.is_valid_class = gr_gp106_is_valid_class;
	gops->gr.pagepool_default_size = gr_gp106_pagepool_default_size;
	gops->gr.handle_sw_method = gr_gp106_handle_sw_method;
	gops->gr.cb_size_default = gr_gp106_cb_size_default;
	gops->gr.init_preemption_state = NULL;
	gops->gr.set_ctxsw_preemption_mode = gr_gp106_set_ctxsw_preemption_mode;
	gops->gr.create_gr_sysfs = NULL;

}
