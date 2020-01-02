/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/cpuidle.h>
#include <linux/slab.h>
#include <linux/sched/idle.h>
#include "cpuidle_profiler.h"

#include <asm/page.h>
#include <asm/cputype.h>
#include <asm/smp_plat.h>
#include <asm/topology.h>

#define MAX_CLUSTER             3
#define for_each_cluster(id)                                    \
        for ((id) = 0; (id) < MAX_CLUSTER; (id)++)

static bool profile_started;
static bool CPD_flag[MAX_CLUSTER];

/*
 * "profile_info" contains profiling data for per cpu idle state which
 * declared in cpuidle driver.
 */
static DEFINE_PER_CPU(struct cpuidle_profile_info, profile_info);

/*
 * "cpd_info" contains profiling data for CPD(Cluster Power Down).
 * Each cluster has one element in cpd_info[].
 */
static struct cpuidle_profile_info cpd_info[MAX_CLUSTER];


/************************************************************************
 *                              Profiling                               *
 ************************************************************************/
/*
 * If cpu does not enter idle state, cur_state has -EINVAL. By this,
 * profiler can be aware of cpu state.
 */
#define state_entered(state)	(((int)state < (int)0) ? 0 : 1)

static void enter_idle_state(struct cpuidle_profile_info *info,
					int state, ktime_t now)
{
	if (state_entered(info->cur_state))
		return;

	info->cur_state = state;
	info->last_entry_time = now;

	info->usage[state].entry_count++;
}

static void exit_idle_state(struct cpuidle_profile_info *info,
					int state, ktime_t now,
					int earlywakeup)
{
	s64 diff;
	if (!state_entered(info->cur_state))
		return;

	info->cur_state = -EINVAL;
	if (earlywakeup) {
		/*
		 * If cpu cannot enter power mode, residency time
		 * should not be updated.
		 */
		info->usage[state].early_wakeup_count++;
		return;
	}
	diff = ktime_to_us(ktime_sub(now, info->last_entry_time));
	info->usage[state].time += diff;
}

/*
 * CPD can be entered by many cpus.
 * The variables which contains these idle states need to keep
 * synchronization.
 */
static DEFINE_SPINLOCK(substate_lock);

void __cpuidle_profile_start(int cpu, int state, int substate)
{
	struct cpuidle_profile_info *info = &per_cpu(profile_info, cpu);
	ktime_t now = ktime_get();
	/*
	 * Start to profile idle state. profile_info is per-CPU variable,
	 * it does not need to synchronization.
	 */
	enter_idle_state(info, state, now);

	/* Start to profile Cluster Power Down */
	if (substate == PSCI_CLUSTER_SLEEP) {
		spin_lock(&substate_lock);
		info = &cpd_info[to_cluster(cpu)];
		enter_idle_state(info, 0, now);
		CPD_flag[to_cluster(cpu)] = 1;
		spin_unlock(&substate_lock);
	}
}

void cpuidle_profile_start(int cpu, int state, int substate)
{
	/*
	 * Return if profile is not started
	 */
	if (!profile_started)
		return;

	__cpuidle_profile_start(cpu, state, substate);
}

void __cpuidle_profile_finish(int cpu, int earlywakeup)
{
	struct cpuidle_profile_info *info = &per_cpu(profile_info, cpu);
	int state = info->cur_state;
	ktime_t now = ktime_get();
	exit_idle_state(info, state, now, earlywakeup);

	spin_lock(&substate_lock);
	/*
	 * CPD can be wakeup by any cpu from the cluster. We cannot predict
	 * which cpu wakeup from idle state, profiler always try to update
	 * residency time of subordinate state. To avoid duplicate updating,
	 * exit_idle_state() checks validation.
	 */
	if (CPD_flag[to_cluster(cpu)]) {
		info = &cpd_info[to_cluster(cpu)];
		exit_idle_state(info, info->cur_state, now, earlywakeup);
		CPD_flag[to_cluster(cpu)] = 0;
	}
	spin_unlock(&substate_lock);
}

void cpuidle_profile_finish(int cpu, int earlywakeup)
{
	/*
	 * Return if profile is not started
	 */
	if (!profile_started)
		return;

	__cpuidle_profile_finish(cpu, earlywakeup);
}

/************************************************************************
 *                              Show result                             *
 ************************************************************************/
static ktime_t profile_start_time;
static ktime_t profile_finish_time;
static s64 profile_time;

static int calculate_percent(s64 residency)
{
	if (!residency)
		return 0;

	residency *= 100;
	do_div(residency, profile_time);

	return residency;
}

static unsigned long long sum_idle_time(int cpu)
{
	int i;
	unsigned long long idle_time = 0;
	struct cpuidle_profile_info *info = &per_cpu(profile_info, cpu);

	for (i = 0; i < info->state_count; i++)
		idle_time += info->usage[i].time;

	return idle_time;
}

static int total_idle_ratio(int cpu)
{
	return calculate_percent(sum_idle_time(cpu));
}

static void show_result(void)
{
	int i,cpu;
	struct cpuidle_profile_info *info;
	int state_count;

	pr_info("#############################################################\n");
	pr_info("Profiling Time : %lluus\n", profile_time);

	pr_info("\n");

	pr_info("[total idle ratio]\n");
	pr_info("#cpu      #time    #ratio\n");
	for_each_possible_cpu(cpu)
		pr_info("cpu%d %10lluus   %3u%%\n", cpu,
			sum_idle_time(cpu), total_idle_ratio(cpu));

	pr_info("\n");

	/*
	 * All profile_info has same state_count. As a representative,
	 * cpu0's is used.
	 */
	state_count = per_cpu(profile_info, 0).state_count;

	for (i = 0; i < state_count; i++) {
		pr_info("[state%d]\n", i);
		pr_info("#cpu   #entry   #early      #time    #ratio\n");
		for_each_possible_cpu(cpu) {
			info = &per_cpu(profile_info, cpu);
			pr_info("cpu%d   %5u   %5u   %10lluus   %3u%%\n", cpu,
				info->usage[i].entry_count,
				info->usage[i].early_wakeup_count,
				info->usage[i].time,
				calculate_percent(info->usage[i].time));
		}

		pr_info("\n");
	}

	pr_info("[Cluster Power Down]\n");
	pr_info("#cluster     #entry   #early      #time     #ratio\n");
	for_each_cluster(i) {
		pr_info("cluster%d   %5u   %5u   %10lluus    %3u%%\n",
			i,
			cpd_info[i].usage->entry_count,
			cpd_info[i].usage->early_wakeup_count,
			cpd_info[i].usage->time,
			calculate_percent(cpd_info[i].usage->time));
	}

	pr_info("\n");
	pr_info("#############################################################\n");
}

/************************************************************************
 *                            Profile control                           *
 ************************************************************************/
static void clear_time(ktime_t *time)
{
	time = 0;
}

static void clear_profile_info(struct cpuidle_profile_info *info)
{
	memset(info->usage, 0,
		sizeof(struct cpuidle_profile_state_usage) * info->state_count);

	clear_time(&info->last_entry_time);
	info->cur_state = -EINVAL;
}

static void reset_profile_record(void)
{
	int i;
	clear_time(&profile_start_time);
	clear_time(&profile_finish_time);

	for_each_possible_cpu(i)
		clear_profile_info(&per_cpu(profile_info, i));

	for_each_cluster(i)
		clear_profile_info(&cpd_info[i]);
}

static void call_cpu_start_profile(void *p) {};
static void call_cpu_finish_profile(void *p) {};

static void cpuidle_profile_main_start(void)
{
	int i;
	if (profile_started) {
		pr_err("cpuidle profile is ongoing\n");
		return;
	}
	reset_profile_record();
	profile_start_time = ktime_get();

	profile_started = 1;
	for(i = 0; i < MAX_CLUSTER; i++) {
		CPD_flag[i] = 0;
	}

	/* Wakeup all cpus and clear own profile data to start profile */
	preempt_disable();
	smp_call_function(call_cpu_start_profile, NULL, 1);
	wake_up_if_idle(0);
	wake_up_if_idle(4);
	wake_up_if_idle(8);
	preempt_enable();
}

static void cpuidle_profile_main_finish(void)
{
	if (!profile_started) {
		pr_err("CPUIDLE profile does not start yet\n");
		return;
	}

	/* Wakeup all cpus to update own profile data to finish profile */
	preempt_disable();
	smp_call_function(call_cpu_finish_profile, NULL, 1);
	wake_up_if_idle(0);
	wake_up_if_idle(4);
	wake_up_if_idle(8);
	preempt_enable();

	profile_started = 0;

	profile_finish_time = ktime_get();
	profile_time = ktime_to_us(ktime_sub(profile_finish_time,
						profile_start_time));

	show_result();
}

/*********************************************************************
 *                          Sysfs interface                          *
 *********************************************************************/
static ssize_t show_sysfs_result(struct kobject *kobj,
				     struct kobj_attribute *attr,
				     char *buf)
{
	int ret = 0;
	int i, cpu;
	struct cpuidle_profile_info *info;
	int state_count;
	if (profile_started) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"CPUIDLE profile is ongoing\n");
		return ret;
	}
	if (profile_time == 0) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"CPUIDLE profiler has not started yet\n");
		return ret;
	}
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"#############################################################\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"Profiling Time : %lluus\n", profile_time);

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"\n");

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"[total idle ratio]\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"#cpu      #time    #ratio\n");
	for_each_possible_cpu(cpu)
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"cpu%d %10lluus   %3u%%\n",
			cpu, sum_idle_time(cpu), total_idle_ratio(cpu));

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"\n");

	/*
	 * All profile_info has same state_count. As a representative,
	 * cpu0's is used.
	 */
	state_count = per_cpu(profile_info, 0).state_count;

	for (i = 0; i < state_count; i++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"[state%d]\n", i);
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"#cpu   #entry   #early      #time    #ratio\n");
		for_each_possible_cpu(cpu) {
			info = &per_cpu(profile_info, cpu);
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"cpu%d   %5u   %5u   %10lluus   %3u%%\n",
				cpu,
				info->usage[i].entry_count,
				info->usage[i].early_wakeup_count,
				info->usage[i].time,
				calculate_percent(info->usage[i].time));
		}

		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"\n");
	}

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"[CPD] - Cluster Power Down\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"#cluster     #entry   #early      #time     #ratio\n");
	for_each_cluster(i) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"cluster%d   %5u   %5u   %10lluus    %3u%%\n",
			i,
			cpd_info[i].usage->entry_count,
			cpd_info[i].usage->early_wakeup_count,
			cpd_info[i].usage->time,
			calculate_percent(cpd_info[i].usage->time));
	}

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"#############################################################\n");
	return ret;
}

static ssize_t show_cpuidle_profile(struct kobject *kobj,
				     struct kobj_attribute *attr,
				     char *buf)
{
	int ret = 0;
	if (profile_started)
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"CPUIDLE profile is ongoing\n");
	else
		ret = show_sysfs_result(kobj, attr, buf);


	return ret;
}

static ssize_t store_cpuidle_profile(struct kobject *kobj,
				      struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	int input;
	if (!sscanf(buf, "%1d", &input))
		return -EINVAL;

	if (!!input)
		cpuidle_profile_main_start();
	else
		cpuidle_profile_main_finish();
	return count;
}

static struct kobj_attribute cpuidle_profile_attr =
	__ATTR(profile, 0664, show_cpuidle_profile, store_cpuidle_profile);

static struct attribute *cpuidle_profile_attrs[] = {
	&cpuidle_profile_attr.attr,
	NULL,
};

static const struct attribute_group cpuidle_profile_group = {
	.attrs = cpuidle_profile_attrs,
};

/*********************************************************************
 *                   Initialize cpuidle profiler                     *
 *********************************************************************/
static void __init cpuidle_profile_info_init(struct cpuidle_profile_info *info,
						int state_count)
{
	int size = sizeof(struct cpuidle_profile_state_usage) * state_count;
	info->state_count = state_count;
	info->usage = kmalloc(size, GFP_KERNEL);
	if (!info->usage) {
		pr_err("%s:%d: Memory allocation failed\n", __func__, __LINE__);
		return;
	}
}

void __init cpuidle_profile_register(struct cpuidle_driver *drv)
{
	int idle_state_count = drv->state_count;
	int i;
	/* Initialize each cpuidle state information */
	for_each_possible_cpu(i)
		cpuidle_profile_info_init(&per_cpu(profile_info, i),
						idle_state_count);

	/* Initiailize CPD(Cluster Power Down) information */
	for_each_cluster(i)
		cpuidle_profile_info_init(&cpd_info[i], 1);
}

static int __init cpuidle_profile_init(void)
{
	struct class *class;
	struct device *dev;
	class = class_create(THIS_MODULE, "cpuidle");
	dev = device_create(class, NULL, 0, NULL, "cpuidle_profiler");

	if (sysfs_create_group(&dev->kobj, &cpuidle_profile_group)) {
		pr_err("CPUIDLE Profiler : error to create sysfs\n");
		return -EINVAL;
	}

	return 0;
}
late_initcall(cpuidle_profile_init);
