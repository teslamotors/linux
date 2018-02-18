/*
 * GM20B GMMU
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _NVHOST_GM20B_MM
#define _NVHOST_GM20B_MM
struct gk20a;

#define PDE_ADDR_START(x, y)	((x) &  ~((0x1UL << (y)) - 1))
#define PDE_ADDR_END(x, y)	((x) | ((0x1UL << (y)) - 1))
#define VPR_INFO_FETCH_WAIT	(5)

void gm20b_init_mm(struct gpu_ops *gops);
int gm20b_mm_mmu_vpr_info_fetch(struct gk20a *g);
#endif
