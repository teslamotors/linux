/*
 * drivers/video/tegra/host/cpuaccess_t20.c
 *
 * Tegra Graphics Host Cpu Register Access
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

#include "../nvhost_cpuaccess.h"
#include "../dev.h"

#include "hardware_t20.h"

static int t20_cpuaccess_mutex_try_lock(struct nvhost_cpuaccess *ctx,
					unsigned int idx)
{
	struct nvhost_master *dev = cpuaccess_to_dev(ctx);
	void __iomem *sync_regs = dev->sync_aperture;
	/* mlock registers returns 0 when the lock is aquired.
	 * writing 0 clears the lock. */
	return !!readl(sync_regs + (HOST1X_SYNC_MLOCK_0 + idx * 4));
}

static void t20_cpuaccess_mutex_unlock(struct nvhost_cpuaccess *ctx,
				       unsigned int idx)
{
	struct nvhost_master *dev = cpuaccess_to_dev(ctx);
	void __iomem *sync_regs = dev->sync_aperture;

	writel(0, sync_regs + (HOST1X_SYNC_MLOCK_0 + idx * 4));
}

int nvhost_init_t20_cpuaccess_support(struct nvhost_master *host)
{
	host->nb_modules = NVHOST_MODULE_NUM;

	host->op.cpuaccess.mutex_try_lock = t20_cpuaccess_mutex_try_lock;
	host->op.cpuaccess.mutex_unlock = t20_cpuaccess_mutex_unlock;

	return 0;
}
