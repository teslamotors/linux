/*
 * drivers/cpuidle/cpuidle-tegra18x.c
 *
 * Copyright (C) 2015-2016 NVIDIA Corporation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/cpuidle.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/debugfs.h>
#include <linux/tegra-soc.h>
#include <linux/tegra-fuse.h>
#include <linux/tegra-mce.h>
#include <linux/tegra186-pm.h>
#include <linux/suspend.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/tick.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include "../../kernel/irq/internals.h"
#include <linux/pm_qos.h>
#include <linux/cpu_pm.h>
#include <linux/psci.h>
#include <linux/platform/tegra/tegra18_cpu_map.h>
#include <linux/version.h>

#include <asm/cpuidle.h>
#include <asm/suspend.h>
#include <asm/cputype.h> /* cpuid */
#include <asm/cpu.h>
#include <asm/arch_timer.h>
#include "../../drivers/cpuidle/dt_idle_states.h"
#include "../../kernel/time/tick-internal.h"

#define PSCI_STATE_ID_STATE_MASK        (0xf)
#define PSCI_STATE_ID_WKTIM_MASK        (~0xf000000f)
#define PSCI_STATE_TYPE_SHIFT           3
#define CORE_WAKE_MASK			0x180C
#define TEGRA186_DENVER_CPUIDLE_C7	2
#define TEGRA186_DENVER_CPUIDLE_C6	1
#define TEGRA186_A57_CPUIDLE_C7		1
#define DENVER_CLUSTER			0
#define A57_CLUSTER			1

static struct cpumask denver_cpumask;
static struct cpumask a57_cpumask;
static void cluster_state_init(void *data);
static u32 deepest_denver_cluster_state;
static u32 deepest_a57_cluster_state;
static u64 denver_idle_state;
static u64 a57_idle_state;
static u64 denver_cluster_idle_state;
static u64 a57_cluster_idle_state;
static u32 denver_testmode;
static u32 a57_testmode;
static struct cpuidle_driver t18x_denver_idle_driver;
static struct cpuidle_driver t18x_a57_idle_driver;
static int crossover_init(void);
static void program_cluster_state(void *data);
static u32 tsc_per_sec, nsec_per_tsc_tick;
static u32 tsc_per_usec;

#ifdef CPUIDLE_FLAG_TIME_VALID
#define DRIVER_FLAGS		CPUIDLE_FLAG_TIME_VALID
#else
#define DRIVER_FLAGS		0
#endif

static bool check_mce_version(void)
{
	u32 mce_version_major, mce_version_minor;

	tegra_mce_read_versions(&mce_version_major, &mce_version_minor);
	if (mce_version_major >= 2)
		return true;
	else
		return false;
}
static void tegra186_denver_enter_c6(u32 wake_time)
{
	tegra_mce_update_cstate_info(0, 0, 0, 0, CORE_WAKE_MASK, 1);
	tegra_mce_enter_cstate(TEGRA186_CPUIDLE_C6, wake_time);
	asm volatile("wfi\n");
}

static void tegra186_denver_enter_c7(u32 wake_time)
{
	/* Block all interrupts in the cpu core */
	local_irq_disable();
	local_fiq_disable();
	cpu_pm_enter();  /* power down notifier */
	arm_cpuidle_suspend(TEGRA186_DENVER_CPUIDLE_C7);
	cpu_pm_exit();
	local_fiq_enable();
	local_irq_enable();
}

static void tegra186_a57_enter_c7(u32 wake_time)
{
	cpu_pm_enter();  /* power down notifier */
	arm_cpuidle_suspend(TEGRA186_A57_CPUIDLE_C7);
	cpu_pm_exit();
}

static int t18x_denver_enter_state(
		struct cpuidle_device *dev,
		struct cpuidle_driver *drv,
		int index)
{
	u32 wake_time;
	struct timespec t;
	/* Todo: Based on future need, we might need the var latency_req. */
	/* int latency_req = pm_qos_request(PM_QOS_CPU_DMA_LATENCY);*/

	t = ktime_to_timespec(tick_nohz_get_sleep_length());
	wake_time = t.tv_sec * tsc_per_sec + t.tv_nsec / nsec_per_tsc_tick;

	/* Todo: Based on the Latency number reprogram deepest */
	/*       CC state allowed if needed*/

	if (tegra_cpu_is_asim()) {
		asm volatile("wfi\n");
		return index;
	}

	if (denver_testmode) {
		tegra_mce_update_cstate_info(denver_cluster_idle_state,
				0, 0, 0, 0, 0);
		if (denver_idle_state >= t18x_denver_idle_driver.state_count) {
			pr_err("%s: Requested invalid forced idle state\n",
				__func__);
			index = t18x_denver_idle_driver.state_count;
		} else
			index = denver_idle_state;

		wake_time = 0xFFFFEEEE;
	}

	if (index == TEGRA186_DENVER_CPUIDLE_C7)
		tegra186_denver_enter_c7(wake_time);
	else if (index == TEGRA186_DENVER_CPUIDLE_C6)
		tegra186_denver_enter_c6(wake_time);
	else
		asm volatile("wfi\n");

	/* Todo: can we query the last entered state from mce */
	/*       to update the return value?                  */
	return index;
}

static int t18x_a57_enter_state(
		struct cpuidle_device *dev,
		struct cpuidle_driver *drv,
		int index)
{
	u32 wake_time;
	struct timespec t;
	/* Todo: Based on future need, we might need the var latency_req. */
	/* int latency_req = pm_qos_request(PM_QOS_CPU_DMA_LATENCY);*/

	t = ktime_to_timespec(tick_nohz_get_sleep_length());
	wake_time = t.tv_sec * tsc_per_sec + t.tv_nsec / nsec_per_tsc_tick;

	/* Todo: Based on the Latency number reprogram deepest */
	/*       CC state allowed if needed*/

	if (tegra_cpu_is_asim()) {
		asm volatile("wfi\n");
		return index;
	}

	if (a57_testmode) {
		tegra_mce_update_cstate_info(a57_cluster_idle_state,
				0, 0, 0, 0, 0);
		if (a57_idle_state >= t18x_a57_idle_driver.state_count) {
			pr_err("%s: Requested invalid forced idle state\n",
				__func__);
			index = t18x_a57_idle_driver.state_count;
		} else
			index = a57_idle_state;

		wake_time = 0xFFFFEEEE;
	}

	if (index == TEGRA186_A57_CPUIDLE_C7) {
		tegra186_a57_enter_c7(wake_time);
	} else
		asm volatile("wfi\n");

	return index;
}

static u32 t18x_make_power_state(u32 state)
{
	u32 wake_time;
	struct timespec t;

	t = ktime_to_timespec(tick_nohz_get_sleep_length());
	wake_time = t.tv_sec * tsc_per_sec + t.tv_nsec / nsec_per_tsc_tick;

	if (denver_testmode || a57_testmode)
		wake_time = 0xFFFFEEEE;
	state = state | ((wake_time << 4) & PSCI_STATE_ID_WKTIM_MASK);

	return state;
}

static struct cpuidle_driver t18x_denver_idle_driver = {
	.name = "tegra18x_idle_denver",
	.owner = THIS_MODULE,
	/*
	 * State at index 0 is standby wfi and considered standard
	 * on all ARM platforms. If in some platforms simple wfi
	 * can't be used as "state 0", DT bindings must be implemented
	 * to work around this issue and allow installing a special
	 * handler for idle state index 0.
	 */
	.states[0] = {
		.enter			= t18x_denver_enter_state,
		.exit_latency		= 1,
		.target_residency	= 1,
		.power_usage		= UINT_MAX,
		.flags			= DRIVER_FLAGS,
		.name			= "C1",
		.desc			= "c1-cpu-clockgated",
	}
};

static struct cpuidle_driver t18x_a57_idle_driver = {
	.name = "tegra18x_idle_a57",
	.owner = THIS_MODULE,
	.states[0] = {
                .enter                  = t18x_a57_enter_state,
                .exit_latency           = 1,
                .target_residency       = 1,
                .power_usage            = UINT_MAX,
		.flags			= DRIVER_FLAGS,
                .name                   = "C1",
                .desc                   = "c1-cpu-clockgated",
        }
};

static bool is_timer_irq(struct irq_desc *desc)
{
        return desc && desc->action && (desc->action->flags & IRQF_TIMER);
}

static void suspend_all_device_irqs(void)
{
        struct irq_desc *desc;
        int irq;

        for_each_irq_desc(irq, desc) {
                unsigned long flags;

                /* Don't disable the 'wakeup' interrupt */
                if (is_timer_irq(desc))
                        continue;

                raw_spin_lock_irqsave(&desc->lock, flags);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 3, 0)
		__disable_irq(desc, irq);
#else
		__disable_irq(desc);
#endif
                raw_spin_unlock_irqrestore(&desc->lock, flags);
        }

        for_each_irq_desc(irq, desc) {
                if (is_timer_irq(desc))
                        continue;

		synchronize_irq(irq);
        }
}

static void resume_all_device_irqs(void)
{
        struct irq_desc *desc;
        int irq;

        for_each_irq_desc(irq, desc) {
                unsigned long flags;

                if (is_timer_irq(desc))
                        continue;

                raw_spin_lock_irqsave(&desc->lock, flags);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 3, 0)
		__enable_irq(desc, irq);
#else
		__enable_irq(desc);
#endif
                raw_spin_unlock_irqrestore(&desc->lock, flags);
        }
}

static struct dentry *cpuidle_debugfs_denver;
static struct dentry *cpuidle_debugfs_a57;

static int denver_idle_write(void *data, u64 val)
{
        unsigned long timer_interval_us = (ulong)val;
        ktime_t time, interval, sleep;
	u32 pmstate;
	u32 wake_time;

	val = (val * 1000) / nsec_per_tsc_tick;
	if (val > 0xffffffff)
		val = 0xffffffff;
	wake_time = val;

        if (denver_idle_state >= t18x_denver_idle_driver.state_count) {
                pr_err("%s: Requested invalid forced idle state\n", __func__);
                return -EINVAL;
        }

        suspend_all_device_irqs();
        preempt_disable();
        tick_nohz_idle_enter();
        stop_critical_timings();
        local_fiq_disable();
        local_irq_disable();

        interval = ktime_set(0, (NSEC_PER_USEC * timer_interval_us));

        time = ktime_get();
        sleep = ktime_add(time, interval);
        tick_program_event(sleep, true);

	pmstate = denver_idle_state;
	tegra_mce_update_cstate_info(denver_cluster_idle_state,0,0,0,0,0);

	if (pmstate == TEGRA186_DENVER_CPUIDLE_C7)
		tegra186_denver_enter_c7(wake_time);
	else if (pmstate == TEGRA186_DENVER_CPUIDLE_C6)
		tegra186_denver_enter_c6(wake_time);
	else
		asm volatile("wfi\n");

        sleep = ktime_sub(ktime_get(), time);
        time = ktime_sub(sleep, interval);
        trace_printk("idle: %lld, exit latency: %lld\n", sleep.tv64, time.tv64);

        local_irq_enable();
        local_fiq_enable();
        start_critical_timings();
        tick_nohz_idle_exit();
        preempt_enable_no_resched();
        resume_all_device_irqs();

        return 0;
}

static int a57_idle_write(void *data, u64 val)
{
        unsigned long timer_interval_us = (ulong)val;
        ktime_t time, interval, sleep;
	u32 pmstate;
	u32 wake_time;

	val = (val * 1000) / nsec_per_tsc_tick;
	if (val > 0xffffffff)
		val = 0xffffffff;
	wake_time = val;

        if (a57_idle_state >= t18x_a57_idle_driver.state_count) {
                pr_err("%s: Requested invalid forced idle state\n", __func__);
                return -EINVAL;
        }

        suspend_all_device_irqs();
        preempt_disable();

        tick_nohz_idle_enter();
        stop_critical_timings();
        local_fiq_disable();
        local_irq_disable();

        interval = ktime_set(0, (NSEC_PER_USEC * timer_interval_us));

        time = ktime_get();
        sleep = ktime_add(time, interval);
        tick_program_event(sleep, true);

	pmstate = a57_idle_state;
	tegra_mce_update_cstate_info(a57_cluster_idle_state,0,0,0,0,0);

	if (pmstate == TEGRA186_A57_CPUIDLE_C7)
		tegra186_a57_enter_c7(wake_time);
	else
		asm volatile("wfi\n");

        sleep = ktime_sub(ktime_get(), time);
        time = ktime_sub(sleep, interval);
        trace_printk("idle: %lld, exit latency: %lld\n", sleep.tv64, time.tv64);

        local_irq_enable();
        local_fiq_enable();
        start_critical_timings();
        tick_nohz_idle_exit();
        preempt_enable_no_resched();
        resume_all_device_irqs();

        return 0;
}

struct xover_smp_call_data {
	int index;
	int value;
};

static void program_single_crossover(void *data)
{
	struct xover_smp_call_data *xover_data =
		(struct xover_smp_call_data *)data;
	tegra_mce_update_crossover_time(xover_data->index,
					xover_data->value * tsc_per_usec);
}

static int setup_crossover(int cluster, int index, int value)
{
	struct xover_smp_call_data xover_data;

	xover_data.index = index;
	xover_data.value = value;

	if (cluster == DENVER_CLUSTER)
		on_each_cpu_mask(&denver_cpumask, program_single_crossover,
			&xover_data, 1);
	else
		on_each_cpu_mask(&a57_cpumask, program_single_crossover,
			&xover_data, 1);
	return 0;
}

static int a57_cc6_write(void *data, u64 val)
{
	return setup_crossover(A57_CLUSTER, TEGRA_MCE_XOVER_CC1_CC6, (u32) val);
}

static int a57_cc7_write(void *data, u64 val)
{
	return setup_crossover(A57_CLUSTER, TEGRA_MCE_XOVER_CC1_CC7, (u32) val);
}

static int denver_c6_write(void *data, u64 val)
{
	return setup_crossover(DENVER_CLUSTER, TEGRA_MCE_XOVER_C1_C6,
		(u32) val);
}

static int denver_cc6_write(void *data, u64 val)
{
	return setup_crossover(DENVER_CLUSTER, TEGRA_MCE_XOVER_CC1_CC6,
		(u32) val);
}

static int denver_cc7_write(void *data, u64 val)
{
	return setup_crossover(DENVER_CLUSTER, TEGRA_MCE_XOVER_CC1_CC7,
		(u32) val);
}

static int denver_set_testmode(void *data, u64 val)
{
	denver_testmode = (u32)val;
	if (denver_testmode) {
		setup_crossover(DENVER_CLUSTER, TEGRA_MCE_XOVER_C1_C6, 0);
		setup_crossover(DENVER_CLUSTER, TEGRA_MCE_XOVER_CC1_CC6, 0);
		setup_crossover(DENVER_CLUSTER, TEGRA_MCE_XOVER_CC1_CC7, 0);
	} else {
		/* Restore the cluster state */
		smp_call_function_any(&denver_cpumask,
			program_cluster_state,
			&deepest_denver_cluster_state, 1);
		/* Restore the crossover values */
		crossover_init();
	}
	return 0;
}

static int a57_set_testmode(void *data, u64 val)
{
	a57_testmode = (u32)val;
	if (a57_testmode) {
		setup_crossover(A57_CLUSTER, TEGRA_MCE_XOVER_CC1_CC6, 0);
		setup_crossover(A57_CLUSTER, TEGRA_MCE_XOVER_CC1_CC7, 0);
	} else {
		/* Restore the cluster state */
		smp_call_function_any(&a57_cpumask,
			program_cluster_state,
			&deepest_a57_cluster_state, 1);
		/* Restore the crossover values */
		crossover_init();
	}
	return 0;
}

static int denver_cc_set(void *data, u64 val)
{
	deepest_denver_cluster_state = (u32)val;
	smp_call_function_any(&denver_cpumask, program_cluster_state,
			&deepest_denver_cluster_state, 1);
	return 0;
}

static int denver_cc_get(void *data, u64 *val)
{
	*val = (u64) deepest_denver_cluster_state;
	return 0;
}

static int a57_cc_set(void *data, u64 val)
{
	deepest_a57_cluster_state = (u32)val;
	smp_call_function_any(&a57_cpumask, program_cluster_state,
			&deepest_a57_cluster_state, 1);
	return 0;
}

static int a57_cc_get(void *data, u64 *val)
{
	*val = (u64) deepest_a57_cluster_state;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(duration_us_denver_fops, NULL,
						denver_idle_write, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(duration_us_a57_fops, NULL, a57_idle_write, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(denver_xover_c6_fops, NULL,
						denver_c6_write, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(denver_xover_cc6_fops, NULL,
						denver_cc6_write, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(denver_xover_cc7_fops, NULL,
						denver_cc7_write, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(denver_cc_state_fops, denver_cc_get,
						denver_cc_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(a57_xover_cc6_fops, NULL, a57_cc6_write, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(a57_xover_cc7_fops, NULL, a57_cc7_write, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(denver_testmode_fops, NULL,
					denver_set_testmode, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(a57_testmode_fops, NULL,
					a57_set_testmode, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(a57_cc_state_fops, a57_cc_get,
						a57_cc_set, "%llu\n");

static int cpuidle_debugfs_init(void)
{
	struct dentry *dfs_file;

	cpuidle_debugfs_denver = debugfs_create_dir("cpuidle_denver", NULL);
	if (!cpuidle_debugfs_denver)
		goto err_out;

	dfs_file = debugfs_create_u64("forced_idle_state", 0644,
		cpuidle_debugfs_denver, &denver_idle_state);

	if (!dfs_file)
		goto err_out;

	dfs_file = debugfs_create_u64("forced_cluster_idle_state", 0644,
		cpuidle_debugfs_denver, &denver_cluster_idle_state);

	if (!dfs_file)
		goto err_out;

	dfs_file = debugfs_create_file("forced_idle_duration_us", 0644,
		cpuidle_debugfs_denver, NULL, &duration_us_denver_fops);

	if (!dfs_file)
		goto err_out;

	dfs_file = debugfs_create_file("testmode", 0644,
		cpuidle_debugfs_denver, NULL, &denver_testmode_fops);
	if (!dfs_file)
		goto err_out;

	dfs_file = debugfs_create_file("crossover_c1_c6", 0644,
		cpuidle_debugfs_denver, NULL, &denver_xover_c6_fops);
	if (!dfs_file)
		goto err_out;

	dfs_file = debugfs_create_file("crossover_cc1_cc6", 0644,
		cpuidle_debugfs_denver, NULL, &denver_xover_cc6_fops);
	if (!dfs_file)
		goto err_out;

	dfs_file = debugfs_create_file("crossover_cc1_cc7", 0644,
		cpuidle_debugfs_denver, NULL, &denver_xover_cc7_fops);
	if (!dfs_file)
		goto err_out;

	dfs_file = debugfs_create_file("deepest_cc_state", 0644,
		cpuidle_debugfs_denver, NULL, &denver_cc_state_fops);
	if (!dfs_file)
		goto err_out;

	cpuidle_debugfs_a57 = debugfs_create_dir("cpuidle_a57", NULL);
	if (!cpuidle_debugfs_a57)
		goto err_out;

	dfs_file = debugfs_create_u64("forced_idle_state", 0644,
		cpuidle_debugfs_a57, &a57_idle_state);

	if (!dfs_file)
		goto err_out;

	dfs_file = debugfs_create_file("testmode", 0644,
		cpuidle_debugfs_a57, NULL, &a57_testmode_fops);
	if (!dfs_file)
		goto err_out;

	dfs_file = debugfs_create_u64("forced_cluster_idle_state", 0644,
		cpuidle_debugfs_a57, &a57_cluster_idle_state);

	if (!dfs_file)
		goto err_out;

	dfs_file = debugfs_create_file("forced_idle_duration_us", 0644,
		cpuidle_debugfs_a57, NULL, &duration_us_a57_fops);

	if (!dfs_file)
		goto err_out;

	dfs_file = debugfs_create_file("crossover_cc1_cc6", 0644,
		cpuidle_debugfs_a57, NULL, &a57_xover_cc6_fops);
	if (!dfs_file)
		goto err_out;

	dfs_file = debugfs_create_file("crossover_cc1_cc7", 0644,
		cpuidle_debugfs_a57, NULL, &a57_xover_cc7_fops);
	if (!dfs_file)
		goto err_out;

	dfs_file = debugfs_create_file("deepest_cc_state", 0644,
		cpuidle_debugfs_a57, NULL, &a57_cc_state_fops);
	if (!dfs_file)
		goto err_out;

	return 0;

err_out:
	pr_err("%s: Couldn't create debugfs node for cpuidle\n", __func__);
	debugfs_remove_recursive(cpuidle_debugfs_denver);
	debugfs_remove_recursive(cpuidle_debugfs_a57);
	return -ENOMEM;
}

static const struct of_device_id tegra18x_a57_idle_state_match[] = {
	{ .compatible = "nvidia,tegra186-cpuidle-a57",
	  .data = t18x_a57_enter_state },
	{ },
};

static const struct of_device_id tegra18x_denver_idle_state_match[] = {
	{ .compatible = "nvidia,tegra186-cpuidle-denver",
	  .data = t18x_denver_enter_state },
	{ },
};

static void cluster_state_init(void *data)
{
	u32 power = UINT_MAX;
	u32 value, pmstate, deepest_pmstate = 0;
	struct device_node *of_states = (struct device_node *)data;
	struct device_node *child;
	int err;

	for_each_child_of_node(of_states, child) {
		if (of_property_match_string(child, "status", "okay"))
			continue;
		err = of_property_read_u32(child, "power", &value);
		if (err) {
			pr_warn(" %s missing power property\n",
				child->full_name);
			continue;
		}
		err = of_property_read_u32(child, "pmstate", &pmstate);
		if (err) {
			pr_warn(" %s missing pmstate property\n",
				child->full_name);
			continue;
		}
		/* Enable the deepest power state */
		if (value > power)
			continue;
		power = value;
		deepest_pmstate = pmstate;
	}
	tegra_mce_update_cstate_info(deepest_pmstate, 0, 0, 0, 0, 0);

	if (tegra18_is_cpu_arm(smp_processor_id()))
		deepest_a57_cluster_state = deepest_pmstate;
	else
		deepest_denver_cluster_state = deepest_pmstate;
}

struct xover_table {
	char *name;
	int index;
};

static void send_crossover(void *data)
{
	struct device_node *child;
	struct device_node *of_states = (struct device_node *)data;
	u32 value;
	int i;

	struct xover_table table1[] = {
		{"crossover_c1_c6", TEGRA_MCE_XOVER_C1_C6},
		{"crossover_cc1_cc6", TEGRA_MCE_XOVER_CC1_CC6},
		{"crossover_cc1_cc7", TEGRA_MCE_XOVER_CC1_CC7},
	};

	for_each_child_of_node(of_states, child)
		for (i = 0; i < TEGRA_MCE_XOVER_MAX; i++) {
			if (of_property_read_u32(child,
				table1[i].name, &value) == 0)
				tegra_mce_update_crossover_time
					(table1[i].index, value * tsc_per_usec);
	}
}

static int crossover_init(void)
{
	struct device_node *denver_xover;
	struct device_node *a57_xover;

	if (!check_mce_version()) {
		pr_err("WARNING: cpuidle: skipping crossover programming."
			" Incompatible MCE version.\n");
		return -ENODEV;
	}

	denver_xover = of_find_node_by_name(NULL,
			"denver_crossover_thresholds");
	a57_xover = of_find_node_by_name(NULL, "a57_crossover_thresholds");

	pr_debug("cpuidle: Init Power Crossover thresholds.\n");
	if (!a57_xover)
		pr_err("WARNING: cpuidle: %s: DT entry missing for A57"
			" thresholds\n", __func__);
	else
		on_each_cpu_mask(&a57_cpumask, send_crossover,
			a57_xover, 1);

	if (!denver_xover)
		pr_err("WARNING: cpuidle: %s: DT entry missing for Denver"
			" thresholds\n", __func__);
	else
		on_each_cpu_mask(&denver_cpumask, send_crossover,
			denver_xover, 1);

	return 0;
}

static void program_cluster_state(void *data)
{
	u32 *cluster_state = (u32 *)data;
	tegra_mce_update_cstate_info(*cluster_state, 0, 0, 0, 0, 0);
}

static int tegra_suspend_notify_callback(struct notifier_block *nb,
	unsigned long action, void *pcpu)
{
	switch (action) {
	case PM_POST_SUSPEND:
	/* Re-program deepest allowed cluster power state after system */
	/* resumes from SC7 */
		smp_call_function_any(&denver_cpumask, program_cluster_state,
			&deepest_denver_cluster_state, 1);
		smp_call_function_any(&a57_cpumask, program_cluster_state,
			&deepest_a57_cluster_state, 1);
		break;
	}
	return NOTIFY_OK;
}

static int tegra_cpu_notify_callback(struct notifier_block *nb,
	unsigned long action, void *pcpu)
{
	int cpu = (long) pcpu;

	switch (action) {
	case CPU_ONLINE:
	/* Re-program deepest allowed cluster power state after core */
	/* in that cluster is onlined. */
		if (tegra18_is_cpu_arm(cpu))
			smp_call_function_any(&a57_cpumask,
				program_cluster_state,
				&deepest_a57_cluster_state, 1);
		else
			smp_call_function_any(&denver_cpumask,
				program_cluster_state,
				&deepest_denver_cluster_state, 1);
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block cpu_on_notifier = {
	.notifier_call = tegra_cpu_notify_callback,
};

static struct notifier_block suspend_notifier = {
	.notifier_call = tegra_suspend_notify_callback,
};

static int __init tegra18x_cpuidle_probe(struct platform_device *pdev)
{
	int cpu_number;
	struct device_node *a57_cluster_states;
	struct device_node *denver_cluster_states;
	int err;

	if (!check_mce_version()) {
		pr_err("cpuidle: failed to register."
			" Incompatible MCE version.\n");
		return -ENODEV;
	}

	tsc_per_sec = arch_timer_get_cntfrq();
	nsec_per_tsc_tick = 1000000000/tsc_per_sec;
	tsc_per_usec = tsc_per_sec / 1000000;

	cpumask_clear(&denver_cpumask);
	cpumask_clear(&a57_cpumask);

	for_each_online_cpu(cpu_number) {
		if (tegra18_is_cpu_arm(cpu_number))
			cpumask_set_cpu(cpu_number, &a57_cpumask);
		else
			cpumask_set_cpu(cpu_number, &denver_cpumask);

		err = arm_cpuidle_init(cpu_number);
		if (err) {
			pr_err("cpuidle: failed to register cpuidle driver\n");
			return err;
		}
	}

	crossover_init();

	a57_cluster_states =
		of_find_node_by_name(NULL, "a57_cluster_power_states");
	denver_cluster_states =
		of_find_node_by_name(NULL, "denver_cluster_power_states");

	if (!cpumask_empty(&denver_cpumask)) {
		/* Denver cluster cpuidle init */
		pr_info("cpuidle: Initializing cpuidle driver init for "
				"Denver cluster\n");
		extended_ops.make_power_state = t18x_make_power_state;

		smp_call_function_any(&denver_cpumask, cluster_state_init,
			denver_cluster_states, 1);

		t18x_denver_idle_driver.cpumask = &denver_cpumask;
		err = dt_init_idle_driver(&t18x_denver_idle_driver,
			tegra18x_denver_idle_state_match, 1);
		if (err <=0) {
			pr_err("cpuidle: failed to init idle states for denver\n");
			return err? : -ENODEV;
		}
		err = cpuidle_register(&t18x_denver_idle_driver, NULL);

		if (err) {
			pr_err("%s: failed to init denver cpuidle power"
				" states.\n", __func__);
			return err;
		}
	}

	if (!cpumask_empty(&a57_cpumask)) {
		/* A57 cluster cpuidle init */
		pr_info("cpuidle: Initializing cpuidle driver init for "
				"A57 cluster\n");
		extended_ops.make_power_state = t18x_make_power_state;

		smp_call_function_any(&a57_cpumask, cluster_state_init,
			a57_cluster_states, 1);

		t18x_a57_idle_driver.cpumask = &a57_cpumask;
		err = dt_init_idle_driver(&t18x_a57_idle_driver,
			tegra18x_a57_idle_state_match, 1);
		if (err <=0) {
			pr_err("cpuidle: failed to init idle states for A57\n");
			return err? : -ENODEV;
		}
		err = cpuidle_register(&t18x_a57_idle_driver, NULL);

		if (err) {
			pr_err("%s: failed to init a57 cpuidle power"
				" states.\n", __func__);
			return err;
		}
	}

	cpuidle_debugfs_init();
	register_cpu_notifier(&cpu_on_notifier);
	register_pm_notifier(&suspend_notifier);
	return 0;
}

static const struct of_device_id tegra18x_cpuidle_of[] __initconst = {
        { .compatible = "nvidia,tegra18x-cpuidle" },
        {}
};

static struct platform_driver tegra18x_cpuidle_driver __refdata = {
        .probe = tegra18x_cpuidle_probe,
        .driver = {
                .owner = THIS_MODULE,
                .name = "cpuidle-tegra18x",
                .of_match_table = of_match_ptr(tegra18x_cpuidle_of)
        }
};

module_platform_driver(tegra18x_cpuidle_driver);
