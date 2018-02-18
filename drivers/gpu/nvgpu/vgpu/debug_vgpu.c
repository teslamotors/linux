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

#include "vgpu/vgpu.h"

static void vgpu_debug_show_dump(struct gk20a *g, struct gk20a_debug_output *o)
{
	/* debug dump not supported */
}

void vgpu_init_debug_ops(struct gpu_ops *gops)
{
	gops->debug.show_dump = vgpu_debug_show_dump;
}
