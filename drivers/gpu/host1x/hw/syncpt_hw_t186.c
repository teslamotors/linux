/*
 * Tegra host1x Syncpoints
 *
 * Copyright (c) 2016, NVIDIA Corporation.
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

#include <linux/io.h>

#include "dev.h"
#include "syncpt.h"

/*
 * Write the current syncpoint value back to hw.
 */
static void syncpt_restore(struct host1x_syncpt *sp)
{
	struct host1x *host = sp->host;
	int min = host1x_syncpt_read_min(sp);
	host1x_sync_writel(host, min, HOST1X_SYNC_SYNCPT(sp->id));
}

/*
 * Write the current waitbase value back to hw.
 */
static void syncpt_restore_wait_base(struct host1x_syncpt *sp)
{
	struct host1x *host = sp->host;
	host1x_sync_writel(host, sp->base_val,
			   HOST1X_SYNC_SYNCPT_BASE(sp->id));
}

/*
 * Read waitbase value from hw.
 */
static void syncpt_read_wait_base(struct host1x_syncpt *sp)
{
	struct host1x *host = sp->host;
	sp->base_val =
		host1x_sync_readl(host, HOST1X_SYNC_SYNCPT_BASE(sp->id));
}

/*
 * Updates the last value read from hardware.
 */
static u32 syncpt_load(struct host1x_syncpt *sp)
{
	struct host1x *host = sp->host;
	u32 old, live;

	/* Loop in case there's a race writing to min_val */
	do {
		old = host1x_syncpt_read_min(sp);
		live = host1x_sync_readl(host, HOST1X_SYNC_SYNCPT(sp->id));
	} while ((u32)atomic_cmpxchg(&sp->min_val, old, live) != old);

	if (!host1x_syncpt_check_max(sp, live))
		dev_err(host->dev, "%s failed: id=%u, min=%d, max=%d\n",
			__func__, sp->id, host1x_syncpt_read_min(sp),
			host1x_syncpt_read_max(sp));

	return live;
}

/*
 * Write a cpu syncpoint increment to the hardware, without touching
 * the cache.
 */
static int syncpt_cpu_incr(struct host1x_syncpt *sp)
{
	struct host1x *host = sp->host;
	u32 reg_offset = sp->id / 32;

	if (!host1x_syncpt_client_managed(sp) &&
	    host1x_syncpt_idle(sp))
		return -EINVAL;
	host1x_sync_writel(host, BIT_MASK(sp->id),
			   HOST1X_SYNC_SYNCPT_CPU_INCR(reg_offset));
	wmb();

	return 0;
}

/* remove a wait pointed to by patch_addr */
static int syncpt_patch_wait(struct host1x_syncpt *sp, void *patch_addr)
{
	u32 override = host1x_class_host_wait_syncpt(
		HOST1X_SYNCPT_RESERVED, 0);

	*((u32 *)patch_addr) = override;
	return 0;
}

static void syncpt_get_mutex_owner(struct host1x_syncpt *sp,
				   unsigned int mutex_id, bool *cpu, bool *ch,
				   unsigned int *chid)
{
	struct host1x *host = sp->host;
	u32 owner;

	owner = host1x_sync_readl(host, host1x_sync_common_mlock_r(mutex_id));
	*chid = HOST1X_SYNC_COMMON_MLOCK_CH_V(owner);
	*ch = HOST1X_SYNC_COMMON_MLOCK_LOCKED_V(owner);
	*cpu = false;
}

static const struct host1x_syncpt_ops host1x_syncpt_t186_ops = {
	.restore = syncpt_restore,
	.restore_wait_base = syncpt_restore_wait_base,
	.load_wait_base = syncpt_read_wait_base,
	.load = syncpt_load,
	.cpu_incr = syncpt_cpu_incr,
	.patch_wait = syncpt_patch_wait,
	.get_mutex_owner = syncpt_get_mutex_owner,
};
