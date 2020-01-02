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
* @file           intel_tfm_gpu.c
* @ingroup        intel-tfm-governor
* @brief          This file implements a GPU turbo usage control
*/

#if defined(CONFIG_DRM_I915) || defined(CONFIG_DRM_I915_MODULE)
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sysfs.h>
#include <linux/workqueue.h>

#include "intel_tfm.h"

static DEFINE_MUTEX(notifier_lock);

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
			   (gpu_data->gpu_budget >= tfmg_settings.gpu_min_budget +
			    tfmg_settings.g_hysteresis)) {
			i915_gpu_turbo_enable();
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
#else
int gpu_manager_init(void) { return 0; };
void gpu_manager_cleanup(void) {};
void gpu_register_notification(void) {};
void gpu_unregister_notification(void) {};
int gpu_fstats_expose(struct tfmg_fstats *fstats) { return 0; };
void gpu_fstats_cleanup(struct tfmg_fstats *fstats) {};
#endif
