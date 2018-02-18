/*
 * Tegra Graphics Host Chip support module
 *
 * Copyright (c) 2012-2014, NVIDIA CORPORATION. All rights reserved.
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

#include <linux/errno.h>
#include <linux/types.h>
#include <linux/bug.h>
#include <linux/slab.h>
#include <linux/tegra-soc.h>

#include "host1x/host1x.h"
#include "chip_support.h"
#include "t124/t124.h"
#include "t210/t210.h"

static struct nvhost_chip_support *nvhost_chip_ops;

struct nvhost_chip_support *nvhost_get_chip_ops(void)
{
	return nvhost_chip_ops;
}

int nvhost_init_chip_support(struct nvhost_master *host)
{
	int err = 0;

	if (nvhost_chip_ops == NULL) {
		nvhost_chip_ops = kzalloc(sizeof(*nvhost_chip_ops), GFP_KERNEL);
		if (nvhost_chip_ops == NULL) {
			pr_err("%s: Cannot allocate nvhost_chip_support\n",
				__func__);
			return 0;
		}
	}

	if (!host->info.initialize_chip_support)
		return -ENODEV;

	err = host->info.initialize_chip_support(host, nvhost_chip_ops);

	return err;
}
