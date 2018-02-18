/*
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

#include "vgpu/vgpu.h"
#include "vgpu_gr_gp10b.h"
#include "vgpu/gm20b/vgpu_gr_gm20b.h"

#include "gp10b/hw_gr_gp10b.h"

static void vgpu_gr_gp10b_free_gr_ctx(struct gk20a *g, struct vm_gk20a *vm,
				struct gr_ctx_desc *gr_ctx)
{
	struct gk20a_platform *platform = gk20a_get_platform(g->dev);
	struct tegra_vgpu_cmd_msg msg = {0};
	struct tegra_vgpu_gr_ctx_params *p = &msg.params.gr_ctx;
	int err;

	gk20a_dbg_fn("");

	if (!gr_ctx || !gr_ctx->mem.gpu_va)
		return;

	msg.cmd = TEGRA_VGPU_CMD_GR_CTX_FREE;
	msg.handle = platform->virt_handle;
	p->gr_ctx_handle = gr_ctx->virt_ctx;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	WARN_ON(err || msg.ret);

	gk20a_vm_free_va(vm, gr_ctx->mem.gpu_va, gr_ctx->mem.size, 0);

	gk20a_gmmu_unmap_free(vm, &gr_ctx->t18x.pagepool_ctxsw_buffer);
	gk20a_gmmu_unmap_free(vm, &gr_ctx->t18x.betacb_ctxsw_buffer);
	gk20a_gmmu_unmap_free(vm, &gr_ctx->t18x.spill_ctxsw_buffer);
	gk20a_gmmu_unmap_free(vm, &gr_ctx->t18x.preempt_ctxsw_buffer);

	kfree(gr_ctx);
}

static int vgpu_gr_gp10b_alloc_gr_ctx(struct gk20a *g,
				struct gr_ctx_desc **__gr_ctx,
				struct vm_gk20a *vm,
				u32 class,
				u32 flags)
{
	struct gk20a_platform *platform = gk20a_get_platform(g->dev);
	struct tegra_vgpu_cmd_msg msg = {0};
	struct tegra_vgpu_gr_bind_ctxsw_buffers_params *p =
			&msg.params.gr_bind_ctxsw_buffers;
	struct gr_ctx_desc *gr_ctx;
	int err;

	gk20a_dbg_fn("");

	WARN_ON(TEGRA_VGPU_GR_BIND_CTXSW_BUFFER_MAX !=
		TEGRA_VGPU_GR_BIND_CTXSW_BUFFER_LAST);

	err = vgpu_gr_alloc_gr_ctx(g, __gr_ctx, vm, class, flags);
	if (err)
		return err;

	gr_ctx = *__gr_ctx;

	if (class == PASCAL_A && g->gr.t18x.ctx_vars.force_preemption_gfxp)
		flags |= NVGPU_ALLOC_OBJ_FLAGS_GFXP;

	if (class == PASCAL_COMPUTE_A &&
			g->gr.t18x.ctx_vars.force_preemption_cilp)
		flags |= NVGPU_ALLOC_OBJ_FLAGS_CILP;

	if (flags & NVGPU_ALLOC_OBJ_FLAGS_GFXP) {
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
		struct mem_desc *desc;

		attrib_cb_size = ALIGN(attrib_cb_size, 128);

		gk20a_dbg_info("gfxp context preempt size=%d",
			g->gr.t18x.ctx_vars.preempt_image_size);
		gk20a_dbg_info("gfxp context spill size=%d", spill_size);
		gk20a_dbg_info("gfxp context pagepool size=%d", pagepool_size);
		gk20a_dbg_info("gfxp context attrib cb size=%d",
			attrib_cb_size);

		err = gr_gp10b_alloc_buffer(vm,
					g->gr.t18x.ctx_vars.preempt_image_size,
					&gr_ctx->t18x.preempt_ctxsw_buffer);
		if (err) {
			err = -ENOMEM;
			goto fail;
		}
		desc = &gr_ctx->t18x.preempt_ctxsw_buffer;
		p->gpu_va[TEGRA_VGPU_GR_BIND_CTXSW_BUFFER_MAIN] = desc->gpu_va;
		p->size[TEGRA_VGPU_GR_BIND_CTXSW_BUFFER_MAIN] = desc->size;

		err = gr_gp10b_alloc_buffer(vm,
					spill_size,
					&gr_ctx->t18x.spill_ctxsw_buffer);
		if (err) {
			err = -ENOMEM;
			goto fail;
		}
		desc = &gr_ctx->t18x.spill_ctxsw_buffer;
		p->gpu_va[TEGRA_VGPU_GR_BIND_CTXSW_BUFFER_SPILL] = desc->gpu_va;
		p->size[TEGRA_VGPU_GR_BIND_CTXSW_BUFFER_SPILL] = desc->size;

		err = gr_gp10b_alloc_buffer(vm,
					pagepool_size,
					&gr_ctx->t18x.pagepool_ctxsw_buffer);
		if (err) {
			err = -ENOMEM;
			goto fail;
		}
		desc = &gr_ctx->t18x.pagepool_ctxsw_buffer;
		p->gpu_va[TEGRA_VGPU_GR_BIND_CTXSW_BUFFER_PAGEPOOL] =
			desc->gpu_va;
		p->size[TEGRA_VGPU_GR_BIND_CTXSW_BUFFER_PAGEPOOL] = desc->size;

		err = gr_gp10b_alloc_buffer(vm,
					attrib_cb_size,
					&gr_ctx->t18x.betacb_ctxsw_buffer);
		if (err) {
			err = -ENOMEM;
			goto fail;
		}
		desc = &gr_ctx->t18x.betacb_ctxsw_buffer;
		p->gpu_va[TEGRA_VGPU_GR_BIND_CTXSW_BUFFER_BETACB] =
			desc->gpu_va;
		p->size[TEGRA_VGPU_GR_BIND_CTXSW_BUFFER_BETACB] = desc->size;

		gr_ctx->graphics_preempt_mode = NVGPU_GRAPHICS_PREEMPTION_MODE_GFXP;
		p->mode = TEGRA_VGPU_GR_CTXSW_PREEMPTION_MODE_GFX_GFXP;
	}

	if (class == PASCAL_COMPUTE_A) {
		if (flags & NVGPU_ALLOC_OBJ_FLAGS_CILP) {
			gr_ctx->compute_preempt_mode = NVGPU_COMPUTE_PREEMPTION_MODE_CILP;
			p->mode = TEGRA_VGPU_GR_CTXSW_PREEMPTION_MODE_COMPUTE_CILP;
		} else {
			gr_ctx->compute_preempt_mode = NVGPU_COMPUTE_PREEMPTION_MODE_CTA;
			p->mode = TEGRA_VGPU_GR_CTXSW_PREEMPTION_MODE_COMPUTE_CTA;
		}
	}

	if (gr_ctx->graphics_preempt_mode || gr_ctx->compute_preempt_mode) {
		msg.cmd = TEGRA_VGPU_CMD_CHANNEL_BIND_GR_CTXSW_BUFFERS;
		msg.handle = platform->virt_handle;
		p->gr_ctx_handle = gr_ctx->virt_ctx;
		err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
		if (err || msg.ret) {
			err = -ENOMEM;
			goto fail;
		}
	}

	gk20a_dbg_fn("done");
	return err;

fail:
	vgpu_gr_gp10b_free_gr_ctx(g, vm, gr_ctx);
	return err;
}

static int vgpu_gr_gp10b_init_ctx_state(struct gk20a *g)
{
	struct gk20a_platform *platform = gk20a_get_platform(g->dev);
	int err;

	gk20a_dbg_fn("");

	err = vgpu_gr_init_ctx_state(g);
	if (err)
		return err;

	vgpu_get_attribute(platform->virt_handle,
			TEGRA_VGPU_ATTRIB_PREEMPT_CTX_SIZE,
			&g->gr.t18x.ctx_vars.preempt_image_size);
	if (!g->gr.t18x.ctx_vars.preempt_image_size)
		return -ENXIO;

	return 0;
}

void vgpu_gp10b_init_gr_ops(struct gpu_ops *gops)
{
	vgpu_gm20b_init_gr_ops(gops);
	gops->gr.alloc_gr_ctx = vgpu_gr_gp10b_alloc_gr_ctx;
	gops->gr.free_gr_ctx = vgpu_gr_gp10b_free_gr_ctx;
	gops->gr.init_ctx_state = vgpu_gr_gp10b_init_ctx_state;
}
