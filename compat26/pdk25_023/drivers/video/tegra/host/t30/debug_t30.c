/*
 * drivers/video/tegra/host/t30/debug_t30.c
 *
 * Copyright (C) 2011 NVIDIA Corporation
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

#include "../dev.h"
#include "scale3d.h"
#include "../t20/t20.h"
#include "../chip_support.h"

int nvhost_init_t30_debug_support(struct nvhost_master *host)
{
	nvhost_init_t20_debug_support(host);
	host->op.debug.debug_init = nvhost_scale3d_debug_init;

	return 0;
}
