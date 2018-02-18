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

#ifndef _NVGPU_GR_GP106_H_
#define _NVGPU_GR_GP106_H_

enum {
	PASCAL_B                 = 0xC197,
	PASCAL_COMPUTE_B         = 0xC1C0,
};

void gp106_init_gr(struct gpu_ops *gops);

#endif
