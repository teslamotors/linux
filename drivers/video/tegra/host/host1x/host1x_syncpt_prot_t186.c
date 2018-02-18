/*
 * Tegra Graphics Host Syncpt Protection
 *
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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

#include "bus_client_t186.h"
#include "nvhost_syncpt.h"
#include "chip_support.h"

static void t186_syncpt_reset(struct nvhost_syncpt *sp, u32 id)
{
	struct nvhost_master *dev = syncpt_to_dev(sp);
	int min = nvhost_syncpt_read_min(sp, id);

	/* move syncpoint to VM */
	host1x_hypervisor_writel(dev->dev,
			(host1x_sync_syncpt_vm_0_r() + id * 4),
			nvhost_host1x_get_vmid(dev->dev));

	/* reserve it for channel 63 */
	host1x_writel(dev->dev,
		(host1x_sync_syncpt_ch_app_0_r() + id * 4),
		host1x_sync_syncpt_ch_app_0_syncpt_ch_f(63));

	/* enable protection */
	host1x_hypervisor_writel(dev->dev,
		host1x_sync_syncpt_prot_en_0_r(),
		host1x_sync_syncpt_prot_en_0_ch_en_f(1));

	/* restore current min value */
	host1x_writel(dev->dev, (host1x_sync_syncpt_0_r() + id * 4), min);

}

static int t186_syncpt_mark_used(struct nvhost_syncpt *sp,
				 u32 chid, u32 syncptid)
{
	struct nvhost_master *dev = syncpt_to_dev(sp);

	host1x_writel(dev->dev,
		(host1x_sync_syncpt_ch_app_0_r() + syncptid * 4),
		host1x_sync_syncpt_ch_app_0_syncpt_ch_f(chid));
	return 0;
}

static int t186_syncpt_mark_unused(struct nvhost_syncpt *sp, u32 syncptid)
{
	struct nvhost_master *dev = syncpt_to_dev(sp);

	host1x_writel(dev->dev,
		(host1x_sync_syncpt_ch_app_0_r() + syncptid * 4),
		host1x_sync_syncpt_ch_app_0_syncpt_ch_f(0xff));
	return 0;
}


static void t186_syncpt_mutex_owner(struct nvhost_syncpt *sp,
				unsigned int idx,
				bool *cpu, bool *ch,
				unsigned int *chid)
{
	struct nvhost_master *dev = syncpt_to_dev(sp);
	u32 owner = host1x_hypervisor_readl(dev->dev,
		host1x_sync_common_mlock_r() + idx * 4);

	*chid = host1x_sync_common_mlock_ch_v(owner);
	*cpu = false;
	*ch = host1x_sync_common_mlock_locked_v(owner);
}
