/*
 * GM206 memory management
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

#include "gk20a/gk20a.h"
#include "gm20b/mm_gm20b.h"
#include "gm206/mm_gm206.h"

#include "hw_fbpa_gm206.h"
#include "hw_top_gm206.h"

static size_t gm206_mm_get_vidmem_size(struct gk20a *g)
{
	u32 fbpas = top_num_fbpas_value_v(
			gk20a_readl(g, top_num_fbpas_r()));
	u32 ram = fbpa_cstatus_ramamount_v(
			gk20a_readl(g, fbpa_cstatus_r()));
	return (size_t)fbpas * ram * SZ_1M;
}

void gm206_init_mm(struct gpu_ops *gops)
{
	gm20b_init_mm(gops);
	gops->mm.get_vidmem_size = gm206_mm_get_vidmem_size;
	gops->mm.get_physical_addr_bits = NULL;
}
