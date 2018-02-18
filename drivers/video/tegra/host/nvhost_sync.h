/*
 * Tegra Graphics Host Syncpoint Integration to linux/sync Framework
 *
 * Copyright (c) 2013-2015, NVIDIA Corporation.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __NVHOST_SYNC_H
#define __NVHOST_SYNC_H

#include <linux/types.h>

#ifdef __KERNEL__

#include "../drivers/staging/android/sync.h"

struct nvhost_syncpt;
struct nvhost_ctrl_sync_fence_info;
struct nvhost_sync_timeline;
struct nvhost_sync_pt;

#ifdef CONFIG_TEGRA_GRHOST_SYNC
struct nvhost_sync_timeline *nvhost_sync_timeline_create(
		struct nvhost_syncpt *sp,
		int id);

void nvhost_sync_pt_signal(struct nvhost_sync_pt *pt, u64 timestamp);

#else
static inline struct nvhost_sync_timeline *nvhost_sync_timeline_create(
		struct nvhost_syncpt *sp,
		int id)
{
	return NULL;
}

static inline void nvhost_sync_pt_signal(struct nvhost_sync_pt *pt,
					 u64 timestamp)
{
	return;
}

#endif

#endif /* __KERNEL __ */

#endif /* __NVHOST_SYNC_H */
