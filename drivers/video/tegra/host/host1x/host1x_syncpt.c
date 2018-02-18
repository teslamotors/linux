/*
 * drivers/video/tegra/host/host1x/host1x_syncpt.c
 *
 * Tegra Graphics Host Syncpoints for HOST1X
 *
 * Copyright (c) 2010-2016, NVIDIA Corporation. All rights reserved.
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

#include <linux/nvhost_ioctl.h>
#include <linux/io.h>
#include <trace/events/nvhost.h>
#include "nvhost_syncpt.h"
#include "nvhost_acm.h"
#include "host1x.h"
#include "chip_support.h"

/**
 * Write the current syncpoint value back to hw.
 */
static void t20_syncpt_reset(struct nvhost_syncpt *sp, u32 id)
{
	struct nvhost_master *dev = syncpt_to_dev(sp);
	int min = nvhost_syncpt_read_min(sp, id);
	host1x_sync_writel(dev, (host1x_sync_syncpt_0_r() + id * 4), min);
}

/**
 * Updates the last value read from hardware.
 * (was nvhost_syncpt_update_min)
 */
static u32 t20_syncpt_update_min(struct nvhost_syncpt *sp, u32 id)
{
	struct nvhost_master *dev = syncpt_to_dev(sp);
	u32 old, live;

	do {
		old = nvhost_syncpt_read_min(sp, id);
		live = host1x_sync_readl(dev,
				(host1x_sync_syncpt_0_r() + id * 4));
	} while ((u32)atomic_cmpxchg(&sp->min_val[id], old, live) != old);

	return live;
}

/**
 * Write a cpu syncpoint increment to the hardware, without touching
 * the cache. Caller is responsible for host being powered.
 */
static void t20_syncpt_cpu_incr(struct nvhost_syncpt *sp, u32 id)
{
	struct nvhost_master *dev = syncpt_to_dev(sp);
	u32 reg_offset = id / 32;

	if (!nvhost_syncpt_client_managed(sp, id)
			&& nvhost_syncpt_min_eq_max(sp, id)) {
		dev_err(&syncpt_to_dev(sp)->dev->dev,
			"Trying to increment syncpoint id %d beyond max\n",
			id);
		nvhost_debug_dump(syncpt_to_dev(sp));
		return;
	}
	host1x_sync_writel(dev,
		host1x_sync_syncpt_cpu_incr_r() + reg_offset * 4, bit_mask(id));
}

/* remove a wait pointed to by patch_addr */
static int host1x_syncpt_patch_wait(struct nvhost_syncpt *sp,
		void __iomem *patch_addr)
{
	u32 current_value = __raw_readl(patch_addr);
	bool obsolete = !!((current_value >> 24) & 0xff);

	/* Is wait 16bit or 32bit? */
	if (obsolete) {
		/* 16bit. Replace a single word */
		u32 override = nvhost_class_host_wait_syncpt(
					nvhost_syncpt_graphics_host_sp(sp), 0);

		__raw_writel(override, patch_addr);
	} else {
		/* 32bit. Replace two words */
		__raw_writel(0, patch_addr - 4);
		__raw_writel(nvhost_syncpt_graphics_host_sp(sp), patch_addr);
	}

	return 0;
}


static const char *t20_syncpt_name(struct nvhost_syncpt *sp, u32 id)
{
	const char *name = sp->syncpt_names[id];
	return name ? name : "";
}

static int syncpt_mutex_try_lock(struct nvhost_syncpt *sp,
		unsigned int idx)
{
	struct nvhost_master *dev = syncpt_to_dev(sp);
	/* mlock registers returns 0 when the lock is aquired.
	 * writing 0 clears the lock. */
	return !!host1x_sync_readl(dev,
			(host1x_sync_mlock_0_r() + idx * 4));
}

static void syncpt_mutex_unlock(struct nvhost_syncpt *sp,
	       unsigned int idx)
{
	struct nvhost_master *dev = syncpt_to_dev(sp);

	host1x_sync_writel(dev, (host1x_sync_mlock_0_r() + idx * 4), 0);
}

static void syncpt_mutex_owner(struct nvhost_syncpt *sp,
				unsigned int idx,
				bool *cpu, bool *ch,
				unsigned int *chid)
{
	struct nvhost_master *dev = syncpt_to_dev(sp);
	u32 owner = host1x_sync_readl(dev,
			host1x_sync_mlock_owner_0_r() + idx * 4);

	*chid = host1x_sync_mlock_owner_0_mlock_owner_chid_0_v(owner);
	*cpu = host1x_sync_mlock_owner_0_mlock_cpu_owns_0_v(owner);
	*ch = host1x_sync_mlock_owner_0_mlock_ch_owns_0_v(owner);
}

static const struct nvhost_syncpt_ops host1x_syncpt_ops = {
	.reset = t20_syncpt_reset,
	.update_min = t20_syncpt_update_min,
	.cpu_incr = t20_syncpt_cpu_incr,
	.patch_wait = host1x_syncpt_patch_wait,
	.name = t20_syncpt_name,
	.mutex_try_lock = syncpt_mutex_try_lock,
	.mutex_unlock_nvh = syncpt_mutex_unlock,
	.mutex_owner = syncpt_mutex_owner,
};
