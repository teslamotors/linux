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
* @file           intel_tfm_core.c
* @ingroup        intel-tfm-governor
* @brief          CPU/GPU Turbo frequency mode (TFM) governor
*
* Goal of the TFM governor module is to manage the TFM usage and budget
* over device lifetime to keep the quality and reliability goals of the
* Apollo Lake silicon.
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/sched.h>
#include <linux/cpufreq.h>

#include "intel_tfm.h"

struct tfmg_settings tfmg_settings = {
	.g_control = 0,
	.g_hysteresis = HYSTERESIS_DEF,
};

static struct tfmg_fstats tfmg_fstats;

struct sku_data sku_array[] = {
	{"premium_var1", 10, 1900, 2200, 10, 600, 750},
	{"premium",       3, 1900, 2300, 17, 600, 750},
	{NULL,		      0,    0,  0,    0,   0,   0},
};

static char *sku_param;
module_param_named(sku, sku_param, charp, 0444);
MODULE_PARM_DESC(sku, "Select the sku based settings [premium|premium_var1");

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

MODULE_LICENSE("GPL and additional rights");
module_init(tfmg_init);
module_exit(tfmg_exit);
