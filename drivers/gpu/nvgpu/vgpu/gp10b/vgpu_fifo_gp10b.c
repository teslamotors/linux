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

#include "vgpu_fifo_gp10b.h"

static int vgpu_gp10b_fifo_init_engine_info(struct fifo_gk20a *f)
{
	struct fifo_engine_info_gk20a *gr_info;
	struct fifo_engine_info_gk20a *ce_info;
	const u32 gr_sw_id = ENGINE_GR_GK20A;
	const u32 ce_sw_id = ENGINE_GRCE_GK20A;

	gk20a_dbg_fn("");

	f->num_engines = 2;

	gr_info = &f->engine_info[0];

	/* FIXME: retrieve this from server */
	gr_info->runlist_id = 0;
	gr_info->engine_enum = gr_sw_id;
	f->active_engines_list[0] = 0;

	ce_info = &f->engine_info[1];
	ce_info->runlist_id = 0;
	ce_info->inst_id = 0;
	ce_info->engine_enum = ce_sw_id;
	f->active_engines_list[1] = 1;

	return 0;
}

void vgpu_gp10b_init_fifo_ops(struct gpu_ops *gops)
{
	/* syncpoint protection not supported yet */
	gops->fifo.init_engine_info = vgpu_gp10b_fifo_init_engine_info;
	gops->fifo.resetup_ramfc = NULL;
}
