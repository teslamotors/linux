/*
 * Copyright (c) 2014-2015, NVIDIA Corporation. All rights reserved.
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

#include <linux/compiler.h>
#include <linux/cpu.h>
#include <linux/cpu_pm.h>
#include <linux/debugfs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/stop_machine.h>
#include <linux/tegra_cluster_control.h>
#include <soc/tegra/tegra_bpmp.h>
#include <asm/psci.h>
#include <asm/smp_plat.h>
#include <asm/suspend.h>
#include <trace/events/nvpower.h>

#include "sleep.h"

#define PSCI_NV_CPU_FORCE_SUSPEND 0x84001000

#define PSCI_RET_SUCCESS                0
#define PSCI_RET_EOPNOTSUPP             -1
#define PSCI_RET_EINVAL                 -2
#define PSCI_RET_EPERM                  -3

static DEFINE_MUTEX(cluster_switch_lock);
static unsigned long pg_core_arg, pg_cluster_arg;

static struct psci_power_state core_pg __initdata = {
	.type = PSCI_POWER_STATE_TYPE_POWER_DOWN,
	.id = 30,
	.affinity_level = 1,
};

static struct psci_power_state cluster_pg __initdata = {
	.type = PSCI_POWER_STATE_TYPE_POWER_DOWN,
	.id = 31,
	.affinity_level = 1,
};

static DEFINE_PER_CPU(struct cpu_stop_work, shutdown_core_work);

static int shutdown_core(void *info)
{
	uintptr_t target_cluster = (uintptr_t)info;
	unsigned long flag;

	if (target_cluster == is_lp_cluster())
		return 0;

	local_irq_save(flag);

	cpu_pm_enter();
	cpu_suspend(pg_core_arg);
	cpu_pm_exit();

	local_irq_restore(flag);

	return 0;
}

static void shutdown_cluster(void)
{
	cpu_pm_enter();

	cpu_suspend(pg_cluster_arg);

	cpu_pm_exit();
}

static BLOCKING_NOTIFIER_HEAD(cluster_switch_chain);

int register_cluster_switch_notifier(struct notifier_block *notifier)
{
	return blocking_notifier_chain_register(&cluster_switch_chain,
						notifier);
}

int unregister_cluster_switch_notifier(struct notifier_block *notifier)
{
	return blocking_notifier_chain_unregister(&cluster_switch_chain,
						notifier);
}

static int tegra_bpmp_switch_cluster(int cpu)
{
	int32_t mb = cpu_to_le32(cpu);
	int32_t on_cpus;

	if (tegra_bpmp_send_receive_atomic(MRQ_SWITCH_CLUSTER, &mb, sizeof(mb),
			&on_cpus, sizeof(on_cpus))) {
		WARN_ON(1);
		return -EFAULT;
	}

	return le32_to_cpu(on_cpus);
}

/* Must be called with the hotplug lock held */
static void switch_cluster(enum cluster val)
{
	struct cpumask mask;
	int bpmp_cpu_mask;
	int phys_cpu_id;
	uintptr_t target_cluster;
	unsigned long flag;
	int cpu;

	mutex_lock(&cluster_switch_lock);
	target_cluster = val;

	if (target_cluster == is_lp_cluster()) {
		mutex_unlock(&cluster_switch_lock);
		return;
	}

	blocking_notifier_call_chain(&cluster_switch_chain,
			TEGRA_CLUSTER_PRE_SWITCH, &val);

	preempt_disable();

	phys_cpu_id = cpu_logical_map(smp_processor_id());
	bpmp_cpu_mask = tegra_bpmp_switch_cluster(phys_cpu_id);

	cpumask_clear(&mask);

	if (bpmp_cpu_mask & 1)
		cpumask_set_cpu(0, &mask);
	if (bpmp_cpu_mask & 2)
		cpumask_set_cpu(1, &mask);
	if (bpmp_cpu_mask & 4)
		cpumask_set_cpu(2, &mask);
	if (bpmp_cpu_mask & 8)
		cpumask_set_cpu(3, &mask);

	cpumask_clear_cpu(smp_processor_id(), &mask);

	for_each_cpu(cpu, &mask)
		stop_one_cpu_nowait(cpu, shutdown_core, &target_cluster,
				&per_cpu(shutdown_core_work, cpu));

	local_irq_save(flag);
	shutdown_cluster();
	local_irq_restore(flag);

	preempt_enable();

	blocking_notifier_call_chain(&cluster_switch_chain,
			TEGRA_CLUSTER_POST_SWITCH, &val);

	mutex_unlock(&cluster_switch_lock);
}

int tegra_cluster_control(unsigned int us, unsigned int flags)
{
	int cluster_flag = flags & TEGRA_POWER_CLUSTER_MASK;

	if (cluster_flag == TEGRA_POWER_CLUSTER_G) {
		enum cluster current_cluster = is_lp_cluster();
		trace_nvcpu_clusterswitch(NVPOWER_CPU_CLUSTER_START,
					  current_cluster,
					  FAST_CLUSTER);
		switch_cluster(FAST_CLUSTER);
		trace_nvcpu_clusterswitch(NVPOWER_CPU_CLUSTER_DONE,
					  current_cluster,
					  FAST_CLUSTER);
	} else if (cluster_flag == TEGRA_POWER_CLUSTER_LP) {
		enum cluster current_cluster = is_lp_cluster();
		trace_nvcpu_clusterswitch(NVPOWER_CPU_CLUSTER_START,
					  current_cluster,
					  SLOW_CLUSTER);
		switch_cluster(SLOW_CLUSTER);
		trace_nvcpu_clusterswitch(NVPOWER_CPU_CLUSTER_DONE,
					  current_cluster,
					  SLOW_CLUSTER);
	}

	return 0;
}

static int __init tegra210_cluster_control_init(void)
{
	pg_core_arg = psci_power_state_pack(core_pg);
	pg_cluster_arg = psci_power_state_pack(cluster_pg);

	pr_info("Tegra210 cluster control initialized\n");

	return 0;
}

late_initcall(tegra210_cluster_control_init);
