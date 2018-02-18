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

#include "gm20b/hal_gm20b.h"
#include "vgpu/vgpu.h"
#include "vgpu_gr_gm20b.h"

int vgpu_gm20b_init_hal(struct gk20a *g)
{
	int err;

	err = gm20b_init_hal(g);
	if (err)
		return err;
	vgpu_init_hal_common(g);
	vgpu_gm20b_init_gr_ops(&g->ops);

	return 0;
}
