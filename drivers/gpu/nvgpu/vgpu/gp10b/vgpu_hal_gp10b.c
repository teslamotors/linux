/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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
#include "gp10b/hal_gp10b.h"
#include "vgpu_gr_gp10b.h"
#include "vgpu_fifo_gp10b.h"
#include "vgpu_mm_gp10b.h"
#include "nvgpu_gpuid_t18x.h"

int vgpu_gp10b_init_hal(struct gk20a *g)
{
	int err;

	gk20a_dbg_fn("");

	err = gp10b_init_hal(g);
	if (err)
		return err;

	vgpu_init_hal_common(g);
	vgpu_gp10b_init_gr_ops(&g->ops);
	vgpu_gp10b_init_fifo_ops(&g->ops);
	vgpu_gp10b_init_mm_ops(&g->ops);
	return 0;
}
