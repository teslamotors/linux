/*
 * drivers/platform/tegra/tegra_bootloader_debug_init.c
 *
 * Copyright (C) 2014-2015 NVIDIA Corporation. All rights reserved.
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

#include <linux/types.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/memblock.h>
#include <asm/page.h>

phys_addr_t tegra_bl_debug_data_start = 0;
EXPORT_SYMBOL(tegra_bl_debug_data_start);

phys_addr_t tegra_bl_debug_data_size = 0;
EXPORT_SYMBOL(tegra_bl_debug_data_size);

phys_addr_t tegra_bl_prof_start = 0;
EXPORT_SYMBOL(tegra_bl_prof_start);

phys_addr_t tegra_bl_prof_size = 0;
EXPORT_SYMBOL(tegra_bl_prof_size);

static int __init tegra_bl_prof_arg(char *option)
{
	char *p = option;

	tegra_bl_prof_size = memparse(p, &p);

	if (!p)
		return -EINVAL;
	if (*p != '@')
		return -EINVAL;

	tegra_bl_prof_start = memparse(p + 1, &p);

	if (!tegra_bl_prof_size || !tegra_bl_prof_start) {
		tegra_bl_prof_size = 0;
		tegra_bl_prof_start = 0;
		return -ENXIO;
	}

	if (pfn_valid(__phys_to_pfn(tegra_bl_prof_start))) {
		if (memblock_reserve(tegra_bl_prof_start, tegra_bl_prof_size)) {
			pr_err("Failed to reserve bl_prof_data %08llx@%08llx\n",
				(u64)tegra_bl_prof_size,
				(u64)tegra_bl_prof_start);
			tegra_bl_prof_size = 0;
			tegra_bl_prof_start = 0;
			return -ENXIO;
		}
	}

	return 0;
}

early_param("bl_prof_dataptr", tegra_bl_prof_arg);

static int __init tegra_bl_debug_data_arg(char *options)
{
	char *p = options;

	tegra_bl_debug_data_size = memparse(p, &p);

	if (!p)
		return -EINVAL;
	if (*p != '@')
		return -EINVAL;

	tegra_bl_debug_data_start = memparse(p + 1, &p);

	if (!tegra_bl_debug_data_size || !tegra_bl_debug_data_start) {
		tegra_bl_debug_data_size = 0;
		tegra_bl_debug_data_start = 0;
		return -ENXIO;
	}

	if (pfn_valid(__phys_to_pfn(tegra_bl_debug_data_start))) {
		if (memblock_reserve(tegra_bl_debug_data_start,
					tegra_bl_debug_data_size)) {
			pr_err("Failed to reserve bl_debug_data %08llx@%08llx\n",
				(u64)tegra_bl_debug_data_size,
				(u64)tegra_bl_debug_data_start);
			tegra_bl_debug_data_size = 0;
			tegra_bl_debug_data_start = 0;
			return -ENXIO;
		}
	}

	return 0;
}
early_param("bl_debug_data", tegra_bl_debug_data_arg);
