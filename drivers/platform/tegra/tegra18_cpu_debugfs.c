/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/smp.h>
#include <asm/smp_plat.h>
#include <asm/traps.h>
#include <linux/debugfs.h>
#include <linux/cpu.h>
#include <linux/module.h>
#include <linux/platform/tegra/tegra18_cpu_map.h>
#include <linux/of_platform.h>
#include <linux/tegra-cpu.h>


static struct dentry *debugfs_dir;

static int __init tegra18_cpu_debugfs_init(void)
{
	struct dentry *denver_dir;
	struct dentry *a57_dir;
	char dest[128];
	char name[8];
	int cpu;

	debugfs_dir = debugfs_create_dir("cpu_topology", NULL);
	/* Save off the debugfs dir so that it can be removed */

	denver_dir = debugfs_create_dir("denver", debugfs_dir);
	a57_dir = debugfs_create_dir("a57", debugfs_dir);

	/* Create symlinks to the logical CPUs based on the cluster type and
	   the physical CPU number. */
	for_each_possible_cpu(cpu) {
		snprintf(dest, sizeof(dest),
			 "/sys/devices/system/cpu/cpu%d", cpu);
		snprintf(name, sizeof(name),
			 "cpu%d", tegra18_logical_to_cpu(cpu));
		if (tegra18_is_cpu_denver(cpu))
			debugfs_create_symlink(name, denver_dir, dest);
		else
			debugfs_create_symlink(name, a57_dir, dest);
	}
	return 0;
}
module_init(tegra18_cpu_debugfs_init);

static void tegra18_cpu_debugfs_exit(void)
{
	debugfs_remove_recursive(debugfs_dir);
}
module_exit(tegra18_cpu_debugfs_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Tegra T18x CPU Topology debugfs");
