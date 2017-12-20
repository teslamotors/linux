/*
 * drivers/video/tegra/host/dev.h
 *
 * Tegra Graphics Host Driver Entrypoint
 *
 * Copyright (c) 2010-2011, NVIDIA Corporation.
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

#ifndef __NVHOST_DEV_H
#define __NVHOST_DEV_H

#include "nvhost_acm.h"
#include "nvhost_syncpt.h"
#include "nvhost_intr.h"
#include "nvhost_cpuaccess.h"
#include "nvhost_channel.h"
#include "chip_support.h"

#define NVHOST_MAJOR 0 /* dynamic */
struct nvhost_hwctx;

struct nvhost_master {
	void __iomem *aperture;
	void __iomem *sync_aperture;
	struct resource *reg_mem;
	struct platform_device *pdev;
	struct class *nvhost_class;
	struct cdev cdev;
	struct device *ctrl;
	struct nvhost_syncpt syncpt;
	struct nvmap_client *nvmap;
	struct nvhost_cpuaccess cpuaccess;
	u32 nb_mlocks;
	struct nvhost_intr intr;
	struct nvhost_module mod;
	struct nvhost_channel *channels;
	u32 nb_channels;
	u32 nb_modules;

	u32 sync_queue_size;

	struct nvhost_chip_support op;
};

struct nvhost_userctx_timeout {
	u32 timeout;
	bool has_timedout;
	struct nvhost_hwctx *hwctx;
	int syncpt_id;
};

void nvhost_debug_init(struct nvhost_master *master);
void nvhost_debug_dump(struct nvhost_master *master);

extern pid_t nvhost_debug_null_kickoff_pid;

#endif
