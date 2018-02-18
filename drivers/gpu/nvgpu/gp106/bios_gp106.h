/*
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

#ifndef NVGPU_BIOS_GP106_H
#define NVGPU_BIOS_GP106_H

struct gpu_ops;

#define INIT_DONE 0x71
#define INIT_RESUME 0x72
#define INIT_CONDITION 0x75
#define INIT_XMEMSEL_ZM_NV_REG_ARRAY 0x8f

struct condition_entry {
	u32 cond_addr;
	u32 cond_mask;
	u32 cond_compare;
} __packed;

void gp106_init_bios(struct gpu_ops *gops);
#endif
