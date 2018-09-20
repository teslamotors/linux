/*
 * intel_tfm_governor.c: CPU/GPU Turbo frequency mode (TFM) governor for APL-I
 *
 * (C) Copyright 2018 Intel Corporation
 * Author: Wan Ahmad Zainie <wan.ahmad.zainie.wan.mohamad@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/sysfs.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/sched/clock.h>
#include <trace/events/power.h>
#include <uapi/linux/sched/types.h>

#include "intel_tfmg.h"

/******************** TFM Governor - GPU start ********************/
#if defined(CONFIG_DRM_I915) || defined(CONFIG_DRM_I915_MODULE)
struct gpudata *gpu_data;
static struct task_struct *gpu_thread;

struct gpu_work {
	struct delayed_work counter_work;
	int pstate;
};

struct gpu_work *gpu_workq;

static void update_gpu_counting(struct work_struct *work)
{
	uint64_t now;
	long long time_slice;
	struct gpu_work *gwork = container_of(work, typeof(*gpu_workq),
					      counter_work.work);

	now = sched_clock();

	if (!gpu_data->prev_pstate)
		gpu_data->prev_timestamp = now;

	time_slice = now - gpu_data->prev_timestamp;
	gpu_data->stats[gpu_data->prev_pstate] += time_slice;
	gpu_data->prev_timestamp = now;
	gpu_data->prev_pstate = gwork->pstate;

	if (gpu_data->turbo_flag) {
		pr_debug("intel-tfmg: drain GPU budget\n");
		gpu_data->time_in_turbo += time_slice;
		spin_lock(&gpu_data->gpu_lock);
		gpu_data->gpu_budget -= time_slice *
			(100 - tfmg_settings.sku->gpu_turbo_pct);
		spin_unlock(&gpu_data->gpu_lock);
	} else {
		pr_debug("intel-tfmg: recoup GPU budget\n");
		spin_lock(&gpu_data->gpu_lock);
		gpu_data->gpu_budget += time_slice *
			tfmg_settings.sku->gpu_turbo_pct;
		spin_unlock(&gpu_data->gpu_lock);
	}

	gpu_data->turbo_flag = (i915_gpu_pstate2freq(gwork->pstate) >
			(int) tfmg_settings.sku->gpu_high_freq) ? 1 : 0;

	gpu_workq->pstate = gwork->pstate;
	schedule_delayed_work(&gpu_workq->counter_work,
			      msecs_to_jiffies(tfmg_settings.gpu_timer.period));
}

static int gpu_freq_event(struct notifier_block *unused, unsigned long pstate,
			  void *ptr)
{
	if (pstate > gpu_data->max_pstate) {
		pr_warn("intel-tfmg: pstate = %lu is out of range\n", pstate);
		return NOTIFY_DONE;
	}

	/*
	 * Cancel any pending update and reschedule to update immediately
	 * with the current pstate.
	 */
	cancel_delayed_work(&gpu_workq->counter_work);
	gpu_workq->pstate = pstate;
	schedule_delayed_work(&gpu_workq->counter_work, 0);
	return NOTIFY_DONE;
}

static int gpu_timer_thread(void *arg)
{
	bool turbo_status = true;

	set_current_state(TASK_RUNNING);

	while (!kthread_should_stop()) {
		if (turbo_status &&
		    (gpu_data->gpu_budget <= tfmg_settings.gpu_min_budget)) {
			i915_gpu_turbo_disable();
			turbo_status = false;
		} else if (!turbo_status &&
			   (gpu_data->gpu_budget >=
			    tfmg_settings.gpu_min_budget +
			    tfmg_settings.g_hysteresis)) {
			i915_gpu_enable_turbo();
			turbo_status = true;
		}

		msleep_interruptible(tfmg_settings.gpu_timer.period);
	}

	return 0;
}

static struct notifier_block gpu_tfm_notifier = {
	.notifier_call = gpu_freq_event
};

void gpu_register_notification(void)
{
	i915_register_freq_notify(&gpu_tfm_notifier);
}

void gpu_unregister_notification(void)
{
	i915_unregister_freq_notify(&gpu_tfm_notifier);
	i915_gpu_turbo_disable();
	gpu_data->prev_pstate = 0;
}

static ssize_t show_gpu_freq_stats(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	ssize_t ret = 0;
	int i;
	int freq_khz;

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "MHz\tGPU\n");

	for (i = 0; i <= gpu_data->max_pstate; i++) {
		freq_khz = i915_gpu_pstate2freq(i);
		if (freq_khz >= 0) {
			if (ret >= PAGE_SIZE)
				break;
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"%d\t%llu\n", KHZ_TO_MHZ(freq_khz),
					gpu_data->stats[i] / NSEC_PER_MSEC);
		}
	}
	if (ret >= PAGE_SIZE)
		return PAGE_SIZE;

	return ret;
}

static ssize_t show_tfm_gpu_budget_ms(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	ssize_t ret = 0;

	ret = sprintf(buf, "%lld\n", gpu_data->gpu_budget /
			(100 * NSEC_PER_MSEC));

	return ret;
}

static ssize_t store_tfm_gpu_budget_ms(struct kobject *kobj,
		struct attribute *attr, const char *buf, size_t count)
{
	int ret;
	long input;

	ret = kstrtol(buf, 10, &input);
	if (ret)
		return -EINVAL;

	spin_lock(&gpu_data->gpu_lock);
	gpu_data->gpu_budget = input * 100 * NSEC_PER_MSEC;
	spin_unlock(&gpu_data->gpu_lock);

	return count;
}

static ssize_t show_gpu_tfm_time_ms(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	ssize_t ret = 0;

	ret = sprintf(buf, "%lld\n", gpu_data->time_in_turbo / NSEC_PER_MSEC);

	return ret;
}

define_one_global_ro(gpu_freq_stats);
define_one_global_ro(gpu_tfm_time_ms);
define_one_global_rw(tfm_gpu_budget_ms);

static struct attribute *gpu_root_attributes[] = {
	&gpu_freq_stats.attr,
	NULL
};

static struct attribute *gpu_stats_attributes[] = {
	&gpu_tfm_time_ms.attr,
	NULL
};

static struct attribute *gpu_params_attributes[] = {
	&tfm_gpu_budget_ms.attr,
	NULL
};

static const struct attribute_group gpu_stats_group = {
	.attrs = gpu_stats_attributes,
};

static const struct attribute_group gpu_root_group = {
	.attrs = gpu_root_attributes,
};

static const struct attribute_group gpu_params_group = {
	.attrs = gpu_params_attributes,
};

int gpu_fstats_expose(struct tfmg_fstats *tfmg_kobjs)
{
	int ret = 0;

	ret = sysfs_create_group(&tfmg_kobjs->root_kobj, &gpu_root_group);
	if (ret < 0)
		return ret;

	ret = sysfs_create_group(tfmg_kobjs->stats_kobj, &gpu_stats_group);
	if (ret < 0)
		goto gpu_sysfs_err1;

	ret = sysfs_create_group(tfmg_kobjs->params_kobj, &gpu_params_group);
	if (ret < 0)
		goto gpu_sysfs_err2;

	return 0;

gpu_sysfs_err2:
	sysfs_remove_group(tfmg_kobjs->stats_kobj, &gpu_stats_group);
gpu_sysfs_err1:
	sysfs_remove_group(&tfmg_kobjs->root_kobj, &gpu_root_group);
	return ret;
}

void gpu_fstats_cleanup(struct tfmg_fstats *tfmg_kobjs)
{
	sysfs_remove_group(tfmg_kobjs->stats_kobj, &gpu_params_group);
	sysfs_remove_group(tfmg_kobjs->params_kobj, &gpu_stats_group);
	sysfs_remove_group(&tfmg_kobjs->root_kobj, &gpu_root_group);
}

int gpu_manager_init(void)
{
	int ret = 0;
	struct sched_param sp = {
		.sched_priority =
			(tfmg_settings.gpu_timer.policy == SCHED_NORMAL) ?
			0 : tfmg_settings.gpu_timer.prio
	};

	gpu_data = kzalloc(sizeof(struct gpudata), GFP_KERNEL);
	if (!gpu_data)
		return -ENOMEM;

	gpu_workq = kzalloc(sizeof(struct gpu_work), GFP_KERNEL);
	if (!gpu_workq) {
		kfree(gpu_data);
		return -ENOMEM;
	}

	gpu_data->max_pstate = i915_gpu_get_max_pstate();
	gpu_data->stats = kzalloc(sizeof(uint64_t) *
			(gpu_data->max_pstate + 1), GFP_KERNEL);
	if (!gpu_data->stats) {
		ret = -ENOMEM;
		goto gpu_manager_err1;
	}
	spin_lock_init(&gpu_data->gpu_lock);

	/* Schedule the initial counter update */
	INIT_DELAYED_WORK(&gpu_workq->counter_work, update_gpu_counting);
	gpu_workq->pstate = 0;
	schedule_delayed_work(&gpu_workq->counter_work,
			      msecs_to_jiffies(tfmg_settings.gpu_timer.period));

	/* Set up thread to monitor budget */
	gpu_thread = kthread_create(gpu_timer_thread, NULL, "gpu_timer_t");
	if (IS_ERR(gpu_thread)) {
		pr_err("intel-tfmg: Couldn't create gpu timer thread\n");
		ret = PTR_ERR(gpu_thread);
		goto gpu_manager_err2;
	}

	/*
	 * Setting up the schedule may not be considered fatal, specifically
	 * we ignore the EPERM errors since those indicate we're trying
	 * to do something that we shouldn't here.
	 */
	ret = sched_setscheduler(gpu_thread, tfmg_settings.gpu_timer.policy,
				 &sp);
	if (ret && ret != -EPERM) {
		pr_err("intel-tfmg: Setscheduler failed with %d\n", ret);
		goto gpu_manager_err3;
	}

	if (tfmg_settings.gpu_timer.policy == SCHED_NORMAL)
		set_user_nice(gpu_thread, tfmg_settings.gpu_timer.prio);

	wake_up_process(gpu_thread);

	gpu_register_notification();

	return 0;

gpu_manager_err3:
	kthread_stop(gpu_thread);
	cancel_delayed_work(&gpu_workq->counter_work);
gpu_manager_err2:
	kfree(gpu_data->stats);
gpu_manager_err1:
	kfree(gpu_data);
	kfree(gpu_workq);
	return ret;
}

void gpu_manager_cleanup(void)
{
	gpu_unregister_notification();
	kthread_stop(gpu_thread);
	cancel_delayed_work(&gpu_workq->counter_work);
	kfree(gpu_data->stats);
	kfree(gpu_data);
	kfree(gpu_workq);
}
#endif
/******************** TFM Governor - GPU end ********************/

/******************** TFM Governor - CPU start ********************/
#ifdef CONFIG_X86_INTEL_PSTATE
static DEFINE_SPINLOCK(notifier_lock);

struct cpudata *cpu_data;

static int cpu_in_turbo(void)
{
	int cpu;

	for (cpu = 0; cpu < cpu_data->num_cpus; cpu++) {
		if (cpu_data->cpu_info[cpu]->turbo_flag)
			return 1;
	}
	return 0;
}

static void update_budget(long long time_slice)
{
	if (cpu_in_turbo()) {
		pr_debug("intel-tfmg: drain CPU budget\n");
		cpu_data->time_in_turbo += time_slice;
		spin_lock(&cpu_data->cpu_lock);
		cpu_data->cpu_budget -= time_slice *
			(100 - tfmg_settings.sku->cpu_turbo_pct);
		spin_unlock(&cpu_data->cpu_lock);

		if (cpu_data->cpu_budget <= tfmg_settings.cpu_min_budget) {
			pr_debug("intel-tfmg: request CPU TFM disable\n");
			pstate_cpu_disable_turbo_usage();
		}
	} else {
		pr_debug("intel-tfmg: recoup CPU budget\n");
		spin_lock(&cpu_data->cpu_lock);
		cpu_data->cpu_budget += time_slice *
			tfmg_settings.sku->cpu_turbo_pct;
		spin_unlock(&cpu_data->cpu_lock);

		if (cpu_data->cpu_budget >= tfmg_settings.cpu_min_budget +
				tfmg_settings.g_hysteresis) {
			pr_debug("intel-tfmg: request CPU TFM enable\n");
			pstate_cpu_enable_turbo_usage();
		}
	}
}

static int cpu_freq_event(struct notifier_block *unused, unsigned long pstate,
				void *ptr)
{
	int cpu;
	struct cpu_info *cpu_info;
	uint64_t now;
	long long time_slice;
	static uint64_t prev_timestamp;
	int freq;

	cpu = (unsigned long) (int *) ptr;

	if (cpu < 0 || cpu >= cpu_data->num_cpus) {
		pr_warn("intel-tfmg: cpu = %d is out of range\n", cpu);
		return NOTIFY_DONE;
	}

	if (pstate > cpu_data->max_pstate) {
		pr_warn("intel-tfmg: pstate = %lu is out of range\n", pstate);
		return NOTIFY_DONE;
	}

	cpu_info = cpu_data->cpu_info[cpu];
	now = sched_clock();
	freq = pstate_cpu_pstate2freq(pstate);

	spin_lock(&notifier_lock);
	time_slice = now - prev_timestamp;
	if (time_slice < 0) {
		spin_unlock(&notifier_lock);
		return NOTIFY_DONE;
	}
	prev_timestamp = now;
	spin_unlock(&notifier_lock);

	if (!cpu_info->prev_pstate) {
		cpu_info->prev_pstate = pstate;
		cpu_info->prev_timestamp = now;
		cpu_info->turbo_flag = (freq >
			(int) tfmg_settings.sku->cpu_high_freq) ? 1 : 0;

		return NOTIFY_DONE;
	}

	spin_lock(&cpu_info->core_lock);
	cpu_info->stats[cpu_info->prev_pstate] += now -
		cpu_info->prev_timestamp;
	spin_unlock(&cpu_info->core_lock);
	cpu_info->prev_timestamp = now;
	cpu_info->prev_pstate = pstate;

	update_budget(time_slice);

	cpu_info->turbo_flag = (freq >
		(int) tfmg_settings.sku->cpu_high_freq) ? 1 : 0;

	return NOTIFY_DONE;
}

static struct notifier_block cpu_tfm_notifier = {
	.notifier_call = cpu_freq_event
};

static void idle_event(void *ignore, unsigned int state, unsigned int cpu_id)
{
	uint64_t now;
	struct cpu_info *info;
	long long time_slice;

	if (cpu_id >= cpu_data->num_cpus) {
		pr_warn("intel-tfmg: cpu = %d is out of range\n", cpu_id);
		return;
	}
	now = sched_clock();
	info = cpu_data->cpu_info[cpu_id];

	if (!info->prev_timestamp)
		return;

	time_slice = now - info->prev_timestamp;
	info->prev_timestamp = now;
	if (time_slice > 0) {
		if (state == (unsigned int) -1) {
			info->cstate += time_slice;
		} else if (info->prev_pstate) {
			spin_lock(&info->core_lock);
			info->stats[info->prev_pstate] += time_slice;
			spin_unlock(&info->core_lock);
		}
	}
}

void cpu_register_notification(void)
{
	pstate_register_freq_notify(&cpu_tfm_notifier);
	register_trace_cpu_idle(idle_event, 0);
}

void cpu_unregister_notification(void)
{
	int cpu;

	pstate_cpu_disable_turbo_usage();
	unregister_trace_cpu_idle(idle_event, 0);
	pstate_unregister_freq_notify(&cpu_tfm_notifier);
	for (cpu = 0; cpu < cpu_data->num_cpus; cpu++)
		cpu_data->cpu_info[cpu]->prev_pstate = 0;
}

static ssize_t show_cpu_freq_stats(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	ssize_t ret = 0;
	int i, j;
	int freq_khz;

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "MHz");

	for (i = 0; i < cpu_data->num_cpus; i++) {
		if (ret >= PAGE_SIZE)
			break;
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "\tCore#%d", i);
	}
	if (ret >= PAGE_SIZE)
		return PAGE_SIZE;

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");

	for (i = 0; i <= cpu_data->max_pstate; i++) {
		freq_khz = pstate_cpu_pstate2freq(i);
		if (freq_khz >= 0) {
			if (ret >= PAGE_SIZE)
				break;
			ret += snprintf(buf + ret, PAGE_SIZE - ret, "%d",
					KHZ_TO_MHZ(freq_khz));
			for (j = 0; j < cpu_data->num_cpus; j++) {
				if (ret >= PAGE_SIZE)
					break;
				ret += snprintf(buf + ret, PAGE_SIZE - ret,
						"\t%llu",
						cpu_data->cpu_info[j]->stats[i]
						/ NSEC_PER_MSEC);
			}
			if (ret >= PAGE_SIZE)
				break;
			ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
		}
	}
	if (ret >= PAGE_SIZE)
		return PAGE_SIZE;
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "C-State");

	for (i = 0; i < cpu_data->num_cpus; i++) {
		if (ret >= PAGE_SIZE)
			break;
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "\t%llu",
				cpu_data->cpu_info[i]->cstate / NSEC_PER_MSEC);
	}
	if (ret >= PAGE_SIZE)
		return PAGE_SIZE;

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");

	if (ret >= PAGE_SIZE)
		return PAGE_SIZE;

	return ret;
}

static ssize_t show_cpu_tfm_time_ms(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	ssize_t ret;

	ret = sprintf(buf, "%llu\n", cpu_data->time_in_turbo / NSEC_PER_MSEC);

	return ret;
}

static ssize_t show_tfm_cpu_budget_ms(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	ssize_t ret = 0;

	ret = sprintf(buf, "%lld\n", cpu_data->cpu_budget /
			(100 * NSEC_PER_MSEC));

	return ret;
}

static ssize_t store_tfm_cpu_budget_ms(struct kobject *kobj,
		struct attribute *attr, const char *buf, size_t count)
{
	int ret;
	long input;

	ret = kstrtol(buf, 10, &input);
	if (ret)
		return -EINVAL;

	spin_lock(&cpu_data->cpu_lock);
	cpu_data->cpu_budget = input * 100 * NSEC_PER_MSEC;
	spin_unlock(&cpu_data->cpu_lock);

	return count;
}

define_one_global_ro(cpu_freq_stats);
define_one_global_ro(cpu_tfm_time_ms);
define_one_global_rw(tfm_cpu_budget_ms);

static struct attribute *cpu_root_attributes[] = {
	&cpu_freq_stats.attr,
	NULL
};

static struct attribute *cpu_stats_attributes[] = {
	&cpu_tfm_time_ms.attr,
	NULL
};

static struct attribute *cpu_params_attributes[] = {
	&tfm_cpu_budget_ms.attr,
	NULL
};

static const struct attribute_group cpu_stats_group = {
	.attrs = cpu_stats_attributes,
};

static const struct attribute_group cpu_root_group = {
	.attrs = cpu_root_attributes,
};

static const struct attribute_group cpu_params_group = {
	.attrs = cpu_params_attributes,
};

int cpu_fstats_expose(struct tfmg_fstats *tfmg_kobjs)
{
	int ret = 0;

	ret = sysfs_create_group(&tfmg_kobjs->root_kobj, &cpu_root_group);
	if (ret < 0)
		return ret;

	ret = sysfs_create_group(tfmg_kobjs->stats_kobj, &cpu_stats_group);
	if (ret < 0)
		goto cpu_sysfs_err1;

	ret = sysfs_create_group(tfmg_kobjs->params_kobj, &cpu_params_group);
	if (ret < 0)
		goto cpu_sysfs_err2;

	return 0;

cpu_sysfs_err2:
	sysfs_remove_group(tfmg_kobjs->stats_kobj, &cpu_stats_group);
cpu_sysfs_err1:
	sysfs_remove_group(&tfmg_kobjs->root_kobj, &cpu_root_group);
	return ret;
}

void cpu_fstats_cleanup(struct tfmg_fstats *tfmg_kobjs)
{
	sysfs_remove_group(tfmg_kobjs->stats_kobj, &cpu_params_group);
	sysfs_remove_group(tfmg_kobjs->params_kobj, &cpu_stats_group);
	sysfs_remove_group(&tfmg_kobjs->root_kobj, &cpu_root_group);
}

int cpu_manager_init(void)
{
	int ret = 0;
	int cpu;

	cpu_data = kzalloc(sizeof(struct cpudata), GFP_KERNEL);
	if (!cpu_data)
		return -ENOMEM;

	cpu_data->num_cpus = (num_present_cpus() > 0) ? num_present_cpus() :
		num_possible_cpus();
	if (cpu_data->num_cpus < 1) {
		pr_err("intel-tfmg: invalid number of cpu (%d)\n",
			 cpu_data->num_cpus);
		ret = -ENODEV;
		goto cpu_manager_err1;
	}
	pr_info("intel-tfmg: detected %d cpus\n", cpu_data->num_cpus);

	cpu_data->max_pstate = pstate_cpu_get_max_pstate();

	cpu_data->cpu_info = vzalloc(sizeof(void *) * cpu_data->num_cpus);
	if (!cpu_data->cpu_info) {
		ret = -ENOMEM;
		goto cpu_manager_err1;
	}

	for (cpu = 0; cpu < cpu_data->num_cpus; cpu++) {
		cpu_data->cpu_info[cpu] = kzalloc(sizeof(struct cpu_info),
						  GFP_KERNEL);
		if (!cpu_data->cpu_info[cpu]) {
			ret = -ENOMEM;
			goto cpu_manager_err2;
		}

		cpu_data->cpu_info[cpu]->stats = kzalloc(sizeof(uint64_t) *
				(cpu_data->max_pstate + 1), GFP_KERNEL);
		if (!cpu_data->cpu_info[cpu]->stats) {
			ret = -ENOMEM;
			goto cpu_manager_err2;
		}
		spin_lock_init(&cpu_data->cpu_info[cpu]->core_lock);
	}
	spin_lock_init(&cpu_data->cpu_lock);
	cpu_register_notification();

	return 0;

cpu_manager_err2:
	for (cpu = 0; cpu < cpu_data->num_cpus; cpu++) {
		if (cpu_data->cpu_info[cpu]) {
			kfree(cpu_data->cpu_info[cpu]->stats);
			kfree(cpu_data->cpu_info[cpu]);
		}
	}
	vfree(cpu_data->cpu_info);
cpu_manager_err1:
	kfree(cpu_data);
	return ret;
}

void cpu_manager_cleanup(void)
{
	int cpu;

	cpu_unregister_notification();

	if (cpu_data->cpu_info) {
		for (cpu = 0; cpu < cpu_data->num_cpus; cpu++) {
			if (cpu_data->cpu_info[cpu]) {
				kfree(cpu_data->cpu_info[cpu]->stats);
				kfree(cpu_data->cpu_info[cpu]);
			}
		}
		vfree(cpu_data->cpu_info);
	}
	kfree(cpu_data);
}
#endif
/******************** TFM Governor - CPU end ********************/

struct tfmg_settings tfmg_settings = {
	.g_control = 0,
	.g_hysteresis = HYSTERESIS_DEF,
};

static struct tfmg_fstats tfmg_fstats;

struct sku_data sku_array[] = {
	{"premium_var1", 10, 1900, 2300,  10, 600, 750},
	{"premium",	  3, 1900, 2300,  17, 600, 750},
	{"high",	 50, 1600, 1900,  50, 500, 650},
	{"mid",		 50, 1600, 1700,  50, 400, 550},
	{"low",		 50, 1300, 1700,  50, 400, 550},
	{"debugging",	100, 1900, 2300, 100, 600, 750},
	{"fallback",	 50, 1900, 2300,  50, 600, 750},
	{NULL,		  0,    0,    0,   0,   0,   0},
};

static char *sku_param = "fallback";
module_param_named(sku, sku_param, charp, 0444);
MODULE_PARM_DESC(sku, "Select the sku based settings; "
		 "[premium|premium_var1|high|mid|low|debugging|fallback]");

static char *sched_param;
module_param_named(sched, sched_param, charp, 0444);
MODULE_PARM_DESC(sched, "GPU timer settings <prio><policy>; "
		"policy = r | o; "
		"'r' is used for round robin scheduler, prio is 1..99; "
		"'o' is used for default scheduler, prio is -19..+19; "
		"(default: '1r')");

static int g_period;
module_param_named(gpu_period, g_period, int, 0444);
MODULE_PARM_DESC(gpu_period, "GPU thread period (in ms)");

static int check_setting(int32_t value, int32_t min, int32_t max)
{
	return (value < min || value > max) ? 1 : 0;
}

static int check_gpu_params(void)
{
	int last_index;
	long prio;
	int ret;

	if (!sched_param)
		sched_param = kstrdup(GPU_SCHED_DEF, GFP_KERNEL);

	last_index = strlen(sched_param) - 1;

	if (sched_param[last_index] == 'r') {
		tfmg_settings.gpu_timer.policy = SCHED_RR;
		sched_param[last_index] = '\0';
		ret = kstrtol(sched_param, 10, &prio);
		if (ret)
			return ret;
		if (check_setting(prio, 1, 99) != 0) {
			pr_err("intel-tfmg: prio=%ld is out of range\n", prio);
			return -EINVAL;
		}
	} else if (sched_param[last_index] == 'o') {
		tfmg_settings.gpu_timer.policy = SCHED_NORMAL;
		sched_param[last_index] = '\0';
		ret = kstrtol(sched_param, 10, &prio);
		if (ret)
			return ret;
		if (check_setting(prio, -19, 19) != 0) {
			pr_err("intel-tfmg: prio=%ld is out of range\n", prio);
			return -EINVAL;
		}
	} else {
		pr_err("intel-tfmg: invalid scheduler policy\n");
		return -EINVAL;
	}

	tfmg_settings.gpu_timer.prio = prio;

	tfmg_settings.gpu_timer.period = (g_period) ?
		g_period : GPU_THREAD_PERIOD_DEF;

	if (check_setting(tfmg_settings.gpu_timer.period, 1, 100000) != 0) {
		pr_err("intel-tfmg: GPU timer period out of range 1..100000\n");
		return -EINVAL;
	}

	return 0;
}

static int assert_settings(void)
{
	struct sku_data *s;

	for (s = sku_array; s->name; s++) {
		if (sku_param && !strcmp(s->name, sku_param)) {
			tfmg_settings.sku = s;
			break;
		}
	}

	if (!tfmg_settings.sku) {
		pr_err("intel-tfmg: invalid sku parameter\n");
		return -1;
	}

	if (check_gpu_params() != 0)
		return -1;

	tfmg_settings.gpu_min_budget =
		max(GPU_MIN_BUDGET, tfmg_settings.gpu_timer.period);
	tfmg_settings.cpu_min_budget =
		max(CPU_MIN_BUDGET, pstate_cpu_get_sample());

	if (check_setting(tfmg_settings.sku->cpu_turbo_pct, 0, 100) != 0) {
		pr_err("intel-tfmg: CPU turbo budget out of range 0..100\n");
		return -1;
	}
	if (check_setting(tfmg_settings.cpu_min_budget, 1, 100000) != 0) {
		pr_warn("intel-tfmg: CPU minimum budget out of range 1..100000\n");
		tfmg_settings.cpu_min_budget = 1000;
	}
	if (check_setting(tfmg_settings.sku->cpu_high_freq, 100, 4000) != 0) {
		pr_err("intel-tfmg: CPU HFM freq out of range 100..4000\n");
		return -1;
	}
	if (check_setting(tfmg_settings.sku->cpu_turbo_freq, 100, 4000) != 0) {
		pr_err("intel-tfmg: CPU TFM freq out of range 100..4000\n");
		return -1;
	}
	if (check_setting(tfmg_settings.sku->gpu_turbo_pct, 0, 100) != 0) {
		pr_err("intel-tfmg: GPU turbo budget out of range 0..100\n");
		return -1;
	}
	if (check_setting(tfmg_settings.gpu_min_budget, 1, 100000) != 0) {
		pr_warn("intel-tfmg: GPU minimum budget out of range 1..100000\n");
		tfmg_settings.gpu_min_budget = 1000;
	}
	if (check_setting(tfmg_settings.sku->gpu_high_freq, 100, 4000) != 0) {
		pr_err("intel-tfmg: GPU HFM freq out of range 100..4000\n");
		return -1;
	}
	if (check_setting(tfmg_settings.sku->gpu_turbo_freq, 100, 4000) != 0) {
		pr_err("intel-tfmg: GPU TFM freq out of range 100..4000\n");
		return -1;
	}

	return 0;
}

static void log_settings(void)
{
	pr_debug("intel-tfmg: CPU turbo budget: %u%%\n",
			tfmg_settings.sku->cpu_turbo_pct);
	pr_debug("intel-tfmg: CPU min budget: %lld ms\n",
			tfmg_settings.cpu_min_budget);
	pr_debug("intel-tfmg: CPU HFM freq: %u MHz\n",
			tfmg_settings.sku->cpu_high_freq);
	pr_debug("intel-tfmg: CPU TFM freq: %u MHz\n",
			tfmg_settings.sku->cpu_turbo_freq);
	pr_debug("intel-tfmg: GPU turbo budget: %u%%\n",
			tfmg_settings.sku->gpu_turbo_pct);
	pr_debug("intel-tfmg: GPU min budget: %lld ms\n",
			tfmg_settings.gpu_min_budget);
	pr_debug("intel-tfmg: GPU HFM freq: %u MHz\n",
			tfmg_settings.sku->gpu_high_freq);
	pr_debug("intel-tfmg: GPU TFM freq: %u MHz\n",
			tfmg_settings.sku->gpu_turbo_freq);
}

static void adopt_settings(void)
{
	/* CPU/GPU hysteresis ms to ns * 100 % */
	tfmg_settings.g_hysteresis *= NSEC_PER_MSEC * 100;
	/* CPU budget: convert ms to ns * 100 % */
	tfmg_settings.cpu_min_budget *= NSEC_PER_MSEC * 100;
	/* CPU high freq: convert MHz to kHz */
	tfmg_settings.sku->cpu_high_freq =
		MHZ_TO_KHZ(tfmg_settings.sku->cpu_high_freq);
	/* CPU turbo freq: convert MHz to kHz */
	tfmg_settings.sku->cpu_turbo_freq =
		MHZ_TO_KHZ(tfmg_settings.sku->cpu_turbo_freq);
	/* GPU budget: convert ms to ns * 100 % */
	tfmg_settings.gpu_min_budget *= NSEC_PER_MSEC * 100;
	/* GPU high freq: convert MHz to kHz */
	tfmg_settings.sku->gpu_high_freq =
		MHZ_TO_KHZ(tfmg_settings.sku->gpu_high_freq);
	/* GPU turbo freq: convert MHz to kHz*/
	tfmg_settings.sku->gpu_turbo_freq =
		MHZ_TO_KHZ(tfmg_settings.sku->gpu_turbo_freq);
}

static struct bus_type tfmg_subsys = {
	.name = "intel_tfmgov",
};

static void tfmg_sysfs_cleanup(void)
{
	if (tfmg_fstats.stats_kobj)
		kobject_put(tfmg_fstats.stats_kobj);

	if (tfmg_fstats.params_kobj)
		kobject_put(tfmg_fstats.params_kobj);

	if (&tfmg_fstats.root_kobj)
		bus_unregister(&tfmg_subsys);
}

static ssize_t show_tfm_control(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	ssize_t ret;

	ret = sprintf(buf, "%d\n", tfmg_settings.g_control);

	return ret;
}

static ssize_t store_tfm_control(struct kobject *kobj,
		struct attribute *attr, const char *buf, size_t count)
{
	int ret;
	long input;

	ret = kstrtol(buf, 10, &input);
	if (ret || (input != 0 && input != 1))
		return -EINVAL;

	if (tfmg_settings.g_control != input) {
		if (input) {
			cpu_register_notification();
			gpu_register_notification();
		} else {
			cpu_unregister_notification();
			gpu_unregister_notification();
		}
		tfmg_settings.g_control = input;
	}
	return count;
}

static ssize_t show_tfm_hysteresis_ms(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	ssize_t ret;

	ret = sprintf(buf, "%lld\n", tfmg_settings.g_hysteresis /
			(100 * NSEC_PER_MSEC));

	return ret;
}

static ssize_t store_tfm_hysteresis_ms(struct kobject *kobj,
		struct attribute *attr, const char *buf, size_t count)
{
	int ret;
	long input;

	ret = kstrtol(buf, 10, &input);
	if (ret)
		return -EINVAL;

	tfmg_settings.g_hysteresis = input * 100 * NSEC_PER_MSEC;

	return count;
}

define_one_global_rw(tfm_control);
define_one_global_rw(tfm_hysteresis_ms);

static struct attribute *global_params_attributes[] = {
	&tfm_control.attr,
	&tfm_hysteresis_ms.attr,
	NULL
};

static const struct attribute_group global_params_group = {
	.attrs = global_params_attributes,
};

static int tfmg_sysfs_init(void)
{
	int ret = 0;

	ret = subsys_virtual_register(&tfmg_subsys, NULL);
	if (ret < 0)
		return ret;

	tfmg_fstats.root_kobj = tfmg_subsys.dev_root->kobj;

	tfmg_fstats.stats_kobj = kobject_create_and_add("stats",
			&tfmg_fstats.root_kobj);
	if (!tfmg_fstats.stats_kobj) {
		tfmg_sysfs_cleanup();
		return -ENOMEM;
	}

	tfmg_fstats.params_kobj = kobject_create_and_add("params",
			&tfmg_fstats.root_kobj);
	if (!tfmg_fstats.stats_kobj) {
		tfmg_sysfs_cleanup();
		return -ENOMEM;
	}

	ret = sysfs_create_group(tfmg_fstats.params_kobj, &global_params_group);
	if (ret < 0) {
		tfmg_sysfs_cleanup();
		return ret;
	}

	return 0;
}

static int __init tfmg_init(void)
{
	int ret = 0;

	if (assert_settings() < 0)
		return -EINVAL;

	log_settings();

	adopt_settings();

	ret = tfmg_sysfs_init();
	if (ret < 0)
		return ret;

	ret = cpu_manager_init();
	if (ret < 0) {
		pr_err("intel-tfmg: CPU manager init failed\n");
		goto tfmg_err1;
	}

	ret = cpu_fstats_expose(&tfmg_fstats);
	if (ret < 0) {
		pr_err("intel-tfmg: Exporting CPU sysfs params failed\n");
		goto tfmg_err2;
	}

	ret = gpu_manager_init();
	if (ret < 0) {
		pr_err("intel-tfmg: GPU manager init failed\n");
		goto tfmg_err3;
	}

	ret = gpu_fstats_expose(&tfmg_fstats);
	if (ret < 0) {
		pr_err("intel-tfmg: Exporting GPU sysfs params failed\n");
		goto tfmg_err4;
	}

	tfmg_settings.g_control = 1;

	pr_debug("intel-tfmg: TFM governor init\n");

	return 0;

tfmg_err4:
	gpu_manager_cleanup();
tfmg_err3:
	cpu_fstats_cleanup(&tfmg_fstats);
tfmg_err2:
	cpu_manager_cleanup();
tfmg_err1:
	sysfs_remove_group(tfmg_fstats.stats_kobj, &global_params_group);
	tfmg_sysfs_cleanup();
	return ret;
}
module_init(tfmg_init);

static void __exit tfmg_exit(void)
{
	cpu_manager_cleanup();
	cpu_fstats_cleanup(&tfmg_fstats);

	gpu_manager_cleanup();
	gpu_fstats_cleanup(&tfmg_fstats);

	sysfs_remove_group(tfmg_fstats.stats_kobj, &global_params_group);
	tfmg_sysfs_cleanup();

	pr_debug("intel-tfmg: TFM governor exit\n");
}
module_exit(tfmg_exit);

MODULE_LICENSE("GPL v2");
