/*
 * drivers/video/tegra/host/t20/t20.c
 *
 * Tegra Graphics Init for T20 Architecture Chips
 *
 * Copyright (c) 2011, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "../dev.h"

#include "t20.h"

int nvhost_init_t20_support(struct nvhost_master *host)
{
	int err;

	/* don't worry about cleaning up on failure... "remove" does it. */
	err = nvhost_init_t20_channel_support(host);
	if (err)
		return err;
	err = nvhost_init_t20_cdma_support(host);
	if (err)
		return err;
	err = nvhost_init_t20_debug_support(host);
	if (err)
		return err;
	err = nvhost_init_t20_syncpt_support(host);
	if (err)
		return err;
	err = nvhost_init_t20_intr_support(host);
	if (err)
		return err;
	err = nvhost_init_t20_cpuaccess_support(host);
	if (err)
		return err;
	return 0;
}
