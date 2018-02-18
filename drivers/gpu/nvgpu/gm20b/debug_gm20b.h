/*
 * GM20B Debug functionality
 *
 * Copyright (C) 2015 NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _DEBUG_GM20B_H_
#define _DEBUG_GM20B_H_

struct gpu_ops;

void gm20b_init_debug_ops(struct gpu_ops *gops);

#endif
