/*
 * Copyright (C) 2015 NVIDIA Corporation.  All rights reserved.
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

#include "gk20a/gk20a.h"
#include "debug_gm20b.h"

void gm20b_init_debug_ops(struct gpu_ops *gops)
{
	gops->debug.show_dump = gk20a_debug_show_dump;
}
