/*
 * drivers/cpuidle/cpuidle-denver.c
 *
 * Copyright (C) 2013-2014 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#define pr_fmt(fmt) "CPUidle T132: " fmt

#include <linux/kernel.h>
#include <linux/cpuidle.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/debugfs.h>
#include <linux/tegra-soc.h>
#include <linux/tegra-fuse.h>

#include <asm/cpuidle.h>
#include <asm/suspend.h>

#include "dt_idle_states.h"

static u32 pmstate_map[CPUIDLE_STATE_MAX] = { -1 };

void tegra_pd_in_idle(bool enable) {}

static int denver_enter_c_state(
		struct cpuidle_device *dev,
		struct cpuidle_driver *drv,
		int index)
{
	int ret = 0, statec1 = 0x0, statecc4 = 0x9;

	if (!index) {
		cpu_do_idle();
		return index;
	}

	if (!strcmp(drv->states[index].name, "c1")) {
		asm volatile("msr actlr_el1, %0\n" : : "r" (statec1));
		asm volatile("wfi\n");
		local_irq_enable();
	} else if (!strcmp(drv->states[index].name, "cc4")) {
		asm volatile("msr actlr_el1, %0\n" : : "r" (statecc4));
		asm volatile("wfi\n");
		local_irq_enable();
	} else {
		ret = -EOPNOTSUPP;
	}

	return ret ?: index;
}

static struct cpuidle_driver tegra132_idle_driver = {
		.name = "tegra132_idle",
		.owner = THIS_MODULE,
		/*
		 * State at index 0 is standby wfi and considered standard
		 * on all ARM platforms. If in some platforms simple wfi
		 * can't be used as "state 0", DT bindings must be implemented
		 * to work around this issue and allow installing a special
		 * handler for idle state index 0.
		 */
	.states[0] = {
		.enter                  = denver_enter_c_state,
		.exit_latency           = 1,
		.target_residency       = 1,
		.power_usage            = UINT_MAX,
		.flags                  = CPUIDLE_FLAG_TIME_VALID,
		.name                   = "WFI",
		.desc                   = "T132 WFI",
	}
};

static const struct of_device_id tegra132_idle_state_match[] = {
	{ .compatible = "nvidia,idle-state",
	  .data = denver_enter_c_state },
	{ },
};
#ifdef CONFIG_DEBUG_FS

static int debugfs_init(void)
{
	struct device_node *of_states;
	struct device_node *child;
	struct dentry *cpuidle_denver_dir;
	struct dentry *idle_node;
	u32 state_count = 0;
	u32 prop;

	cpuidle_denver_dir = debugfs_create_dir("cpuidle_denver", NULL);
	if (!cpuidle_denver_dir) {
		pr_err("%s: Couldn't create the \"cpuidle_denver\" debugfs "
		"node.\n",__func__);
		return -1;
	}
	of_states = of_find_node_by_name(NULL, "idle-states");
	if (!of_states)
		return -ENODEV;

	for_each_child_of_node(of_states, child) {

		if (of_property_read_u32(child, "pmstate", &prop) != 0)
			continue;

		if ((prop == 9) && (tegra_revision == TEGRA_REVISION_A01))
			prop = 0;

		/* Map index to the actual LP state */
		pmstate_map[state_count] = prop;

		/* Create a debugfs node for the idle state */
		idle_node = debugfs_create_dir(child->name,
					       cpuidle_denver_dir);
		if (!idle_node) {
				return -1;
		}
		if (!debugfs_create_x32("pmstate", S_IRUGO | S_IWUSR, idle_node,
				       &pmstate_map[state_count])) {
			pr_err("%s: Couldn't create the pmstate debugfs node"
			       "for %s.\n",__func__, child->name);
			return -1;
		}

		state_count++;
	}

	return 0;
}
#else
static inline int debugfs_init(void) { return 0; }
#endif

static int tegra132_cpuidle_probe(struct platform_device *pdev)
{
	struct cpuidle_driver *drv = &tegra132_idle_driver;
	int cpu, ret;

	/*
	* Initialize idle states data, starting at index 1.
	* This driver is DT only, if no DT idle states are detected (ret == 0)
	* let the driver initialization fail accordingly since there is no
	* reason to initialize the idle driver if only wfi is supported.
	*/

	ret = dt_init_idle_driver(drv, tegra132_idle_state_match, 1);
	if (ret <= 0) {
		if (ret)
		pr_err("failed to initialize idle states\n");
		return ret ? : -ENODEV;
	}

	/*
	 * Call arch CPU operations in order to initialize
	 * idle states suspend back-end specific data
	 */
	for_each_possible_cpu(cpu) {
		ret = arm_cpuidle_init(cpu);
		if (ret) {
			pr_err("CPU %d failed to init idle CPU ops\n", cpu);
			return ret;
		}
	}

	ret = cpuidle_register(drv, NULL);
	if (ret) {
		pr_err("failed to register cpuidle driver\n");
		return ret;
	}

	debugfs_init();

	return 0;
}

static const struct of_device_id tegra132_cpuidle_of[] = {
	{ .compatible = "nvidia,tegra132-cpuidle" },
	{}
};

static struct platform_driver tegra132_cpuidle_driver = {
	.probe = tegra132_cpuidle_probe,
	.driver = {
		.owner = THIS_MODULE,
		.name = "cpuidle-tegra132",
		.of_match_table = of_match_ptr(tegra132_cpuidle_of)
	}
};

module_platform_driver(tegra132_cpuidle_driver);
