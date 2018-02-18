/*
 * GM20B L2
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

#ifndef _NVHOST_GM20B_LTC
#define _NVHOST_GM20B_LTC
struct gpu_ops;

void gm20b_init_ltc(struct gpu_ops *gops);
void gm20b_ltc_init_fs_state(struct gk20a *g);
int gm20b_ltc_cbc_ctrl(struct gk20a *g, enum gk20a_cbc_op op,
		       u32 min, u32 max);
void gm20b_ltc_g_elpg_flush_locked(struct gk20a *g);
void gm20b_ltc_isr(struct gk20a *g);
u32 gm20b_ltc_cbc_fix_config(struct gk20a *g, int base);
void gm20b_flush_ltc(struct gk20a *g);
#endif
