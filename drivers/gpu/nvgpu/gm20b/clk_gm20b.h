/*
 * GM20B Graphics
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
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef _NVHOST_CLK_GM20B_H_
#define _NVHOST_CLK_GM20B_H_

#include <linux/mutex.h>

#ifdef CONFIG_TEGRA_CLK_FRAMEWORK
void gm20b_init_clk_ops(struct gpu_ops *gops);
#else
static inline void gm20b_init_clk_ops(struct gpu_ops *gops) {}
#endif

#endif /* _NVHOST_CLK_GM20B_H_ */
