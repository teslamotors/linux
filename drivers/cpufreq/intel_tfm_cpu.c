/* ****************************************************************************

  MIT License

  Copyright (c) 2017 Intel Corporation

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

  *****************************************************************************
*/
/**
* @file           intel_tfm_cpu.c
* @ingroup        intel-tfm-governor
* @brief          This file implements a CPU turbo usage control
*/

#ifdef CONFIG_X86_INTEL_PSTATE
#include <linux/cpufreq.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <trace/events/power.h>

#include "intel_tfm.h"

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
#else
int cpu_manager_init(void) { return 0; };
void cpu_manager_cleanup(void) {};
void cpu_register_notification(void) {};
void cpu_unregister_notification(void) {};
int pstate_cpu_get_sample(void) { return 0; };
int cpu_fstats_expose(struct tfmg_fstats *fstats) { return 0; };
void cpu_fstats_cleanup(struct tfmg_fstats *fstats) {};
#endif
