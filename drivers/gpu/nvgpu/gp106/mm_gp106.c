/*
 * GP106 memory management
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
#include "gp10b/mm_gp10b.h"
#include "gp106/mm_gp106.h"

#include "hw_fb_gp106.h"

static size_t gp106_mm_get_vidmem_size(struct gk20a *g)
{
	u32 range = gk20a_readl(g, fb_mmu_local_memory_range_r());
	u32 mag = fb_mmu_local_memory_range_lower_mag_v(range);
	u32 scale = fb_mmu_local_memory_range_lower_scale_v(range);
	u32 ecc = fb_mmu_local_memory_range_ecc_mode_v(range);
	size_t bytes = ((size_t)mag << scale) * SZ_1M;

	if (ecc)
		bytes = bytes / 16 * 15;

	return bytes;
}

void gp106_init_mm(struct gpu_ops *gops)
{
	gp10b_init_mm(gops);
	gops->mm.get_vidmem_size = gp106_mm_get_vidmem_size;
	gops->mm.get_physical_addr_bits = NULL;
}
