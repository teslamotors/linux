/*
 * GM20B FB
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

#ifndef _NVHOST_GM20B_FB
#define _NVHOST_GM20B_FB
struct gk20a;

void gm20b_init_fb(struct gpu_ops *gops);
void gm20b_init_uncompressed_kind_map(void);
void gm20b_init_kind_attr(void);
#endif
