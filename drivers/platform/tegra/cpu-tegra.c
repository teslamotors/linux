/*
 * drivers/platform/tegra/cpu-tegra.c
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *	Colin Cross <ccross@google.com>
 *	Based on arch/arm/plat-omap/cpu-omap.c, (C) 2005 Nokia Corporation
 *
 * Copyright (C) 2010-2015 NVIDIA CORPORATION. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/suspend.h>
#include <linux/debugfs.h>
#include <linux/cpu.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_qos.h>
#include <linux/tegra_cluster_control.h>

#include <mach/edp.h>

#include <linux/platform/tegra/clock.h>
#include <linux/platform/tegra/cpu-tegra.h>
#include <linux/platform/tegra/dvfs.h>
#include "pm.h"

#include <trace/events/sysedp.h>

/* tegra throttling and edp governors require frequencies in the table
   to be in ascending order */
static struct cpufreq_frequency_table *freq_table;

static struct clk *cpu_clk;
static struct clk *emc_clk;

static unsigned long policy_max_speed[CONFIG_NR_CPUS];
static unsigned long target_cpu_speed[CONFIG_NR_CPUS];
static bool preserve_cpu_speed;
static DEFINE_MUTEX(tegra_cpu_lock);
static bool is_suspended;
static int suspend_index;
static unsigned int volt_capped_speed;
static struct pm_qos_request cpufreq_max_req;
static struct pm_qos_request cpufreq_min_req;

static unsigned int cur_cpupwr;
static unsigned int cur_cpupwr_freqcap;

static unsigned int force_cpupwr;
static unsigned int force_cpupwr_freqcap;

static bool force_policy_max;

static int force_policy_max_set(const char *arg, const struct kernel_param *kp)
{
	int ret;
	bool old_policy = force_policy_max;

	mutex_lock(&tegra_cpu_lock);

	ret = param_set_bool(arg, kp);
	if ((ret == 0) && (old_policy != force_policy_max))
		tegra_cpu_set_speed_cap_locked(NULL);

	mutex_unlock(&tegra_cpu_lock);
	return ret;
}

static int force_policy_max_get(char *buffer, const struct kernel_param *kp)
{
	return param_get_bool(buffer, kp);
}

static struct kernel_param_ops policy_ops = {
	.set = force_policy_max_set,
	.get = force_policy_max_get,
};
module_param_cb(force_policy_max, &policy_ops, &force_policy_max, 0644);


static unsigned int cpu_user_cap;

static inline void _cpu_user_cap_set_locked(void)
{
#ifndef CONFIG_TEGRA_CPU_CAP_EXACT_FREQ
	if (cpu_user_cap != 0) {
		int i;
		for (i = 0; freq_table[i].frequency != CPUFREQ_TABLE_END; i++) {
			if (freq_table[i].frequency > cpu_user_cap)
				break;
		}
		i = (i == 0) ? 0 : i - 1;
		cpu_user_cap = freq_table[i].frequency;
	}
#endif
	tegra_cpu_set_speed_cap_locked(NULL);
}

void tegra_cpu_user_cap_set(unsigned int speed_khz)
{
	mutex_lock(&tegra_cpu_lock);

	cpu_user_cap = speed_khz;
	_cpu_user_cap_set_locked();

	mutex_unlock(&tegra_cpu_lock);
}

static int cpu_user_cap_set(const char *arg, const struct kernel_param *kp)
{
	int ret;

	mutex_lock(&tegra_cpu_lock);

	ret = param_set_uint(arg, kp);
	if (ret == 0)
		_cpu_user_cap_set_locked();

	mutex_unlock(&tegra_cpu_lock);
	return ret;
}

static int cpu_user_cap_get(char *buffer, const struct kernel_param *kp)
{
	return param_get_uint(buffer, kp);
}

static struct kernel_param_ops cap_ops = {
	.set = cpu_user_cap_set,
	.get = cpu_user_cap_get,
};
module_param_cb(cpu_user_cap, &cap_ops, &cpu_user_cap, 0644);

static unsigned int user_cap_speed(unsigned int requested_speed)
{
	if ((cpu_user_cap) && (requested_speed > cpu_user_cap)) {
		pr_debug("%s: User cap throttling. %u khz to %u khz\n",
				__func__, requested_speed, cpu_user_cap);
		return cpu_user_cap;
	}
	return requested_speed;
}

#ifdef CONFIG_TEGRA_THERMAL_THROTTLE

static ssize_t show_throttle(struct cpufreq_policy *policy, char *buf)
{
	return sprintf(buf, "%u\n", tegra_is_throttling(NULL));
}

cpufreq_freq_attr_ro(throttle);

static ssize_t show_throttle_count(struct cpufreq_policy *policy, char *buf)
{
	int count;

	tegra_is_throttling(&count);
	return sprintf(buf, "%u\n", count);
}

static struct freq_attr _attr_throttle_count = {
	.attr = {.name = "throttle_count", .mode = 0444, },
	.show = show_throttle_count,
};

static struct attribute *new_attrs[] = {
	&_attr_throttle_count.attr,
	NULL
};

static struct attribute_group stats_attr_grp = {
	.attrs = new_attrs,
	.name = "stats"
};

#endif /* CONFIG_TEGRA_THERMAL_THROTTLE */

#ifdef CONFIG_TEGRA_HMP_CLUSTER_CONTROL
#define LP_TO_G_PERCENTAGE		50
static u32 lp_to_g_ratio = LP_TO_G_PERCENTAGE;
static u32 disable_virtualization;
static struct clk *lp_clock, *g_clock;
static int volt_cap_level;

unsigned long lp_to_virtual_gfreq(unsigned long lp_freq)
{
	if (disable_virtualization)
		return lp_freq;

	return (lp_freq / 100) * LP_TO_G_PERCENTAGE;
}

static unsigned long virtualg_to_lpfreq(unsigned long gfreq)
{
	if (disable_virtualization)
		return gfreq;

	return (gfreq / LP_TO_G_PERCENTAGE) * 100;
}

static unsigned long map_to_actual_cluster_freq(unsigned long gfreq)
{
	if (is_lp_cluster())
		return virtualg_to_lpfreq(gfreq);

	return gfreq;
}
#else
static inline unsigned long virtualg_to_lpfreq(unsigned long gfreq)
{ return gfreq; }
static inline unsigned long map_to_actual_cluster_freq(unsigned long gfreq)
{ return gfreq; }
#endif

static unsigned int reg_mode;
static bool reg_mode_force_normal;

#ifdef CONFIG_TEGRA_EDP_LIMITS

static cpumask_t edp_cpumask;
static struct pm_qos_request edp_max_cpus;

static unsigned int get_edp_freq_limit(unsigned int nr_cpus)
{
	unsigned int limit = tegra_get_edp_max_freq(
		nr_cpus, is_lp_cluster() ? MODE_LP : MODE_G);

#ifndef CONFIG_TEGRA_EDP_EXACT_FREQ
	unsigned int i;
	for (i = 0; freq_table[i].frequency != CPUFREQ_TABLE_END; i++) {
		if (freq_table[i].frequency > limit) {
			break;
		}
	}
	BUG_ON(i == 0);	/* min freq above the limit or table empty */
	limit = freq_table[i-1].frequency;
#endif
	return limit;
}

/*
 * Returns the ideal nr of cpus to have online for maximum performance given
 * edp constraints at the current temperature
 */
static int get_edp_max_cpus(void)
{
	unsigned int i, limit;
	unsigned int max_mips = 0;
	int max_nr_cpus = PM_QOS_DEFAULT_VALUE;

	for (i = 1; i <= num_possible_cpus(); i++) {
		limit = get_edp_freq_limit(i);
		if (limit * i >= max_mips) {
			max_mips = limit * i;
			max_nr_cpus = i;
		}
	}
	return max_nr_cpus;
}

/*
 * Shouldn't be called with tegra_cpu_lock held. Will result in a deadlock
 * otherwise
 */
void tegra_edp_update_max_cpus(void)
{
	unsigned int max_cpus = get_edp_max_cpus();

	if (!pm_qos_request_active(&edp_max_cpus))
		pm_qos_add_request(&edp_max_cpus, PM_QOS_MAX_ONLINE_CPUS,
				   max_cpus);
	else
		pm_qos_update_request(&edp_max_cpus, max_cpus);
}

static unsigned int get_edp_limit_speed(unsigned int requested_speed)
{
	int edp_limit = get_edp_freq_limit(cpumask_weight(&edp_cpumask));

	if ((!edp_limit) || (requested_speed <= edp_limit))
		return requested_speed;
	else {
		pr_debug("%s: Edp throttling. %u khz down to %u khz\n",
				__func__, requested_speed, edp_limit);
		return edp_limit;
	}
}

static unsigned int sysedp_cap_speed(unsigned int requested_speed)
{
	unsigned int old_cpupwr_freqcap = cur_cpupwr_freqcap;
	unsigned eff_cpupwr = force_cpupwr ?: cur_cpupwr;

	if (force_cpupwr_freqcap)
		cur_cpupwr_freqcap = force_cpupwr_freqcap;
	else
		/* get the max freq given the power and online cpu count */
		cur_cpupwr_freqcap = tegra_get_sysedp_max_freq(eff_cpupwr,
					  cpumask_weight(&edp_cpumask),
					  is_lp_cluster() ? MODE_LP : MODE_G);

	if (cur_cpupwr_freqcap == 0)
		return requested_speed;

	if (cur_cpupwr_freqcap != old_cpupwr_freqcap) {
		if (IS_ENABLED(CONFIG_DEBUG_KERNEL)) {
			pr_debug("sysedp: ncpus %u, cpu %5u mW %u kHz\n",
				 cpumask_weight(&edp_cpumask), eff_cpupwr,
				 cur_cpupwr_freqcap);
		}
		trace_sysedp_max_cpu_pwr(cpumask_weight(&edp_cpumask),
					 eff_cpupwr, cur_cpupwr_freqcap);
	}

	if (cur_cpupwr_freqcap && requested_speed > cur_cpupwr_freqcap)
		return cur_cpupwr_freqcap;
	return requested_speed;
}

static int tegra_cpu_edp_notify(
	struct notifier_block *nb, unsigned long event, void *hcpu)
{
	int ret = 0;
	unsigned int cpu_speed, edp_speed, new_speed;
	int cpu = (long)hcpu;

	switch (event) {
	case CPU_UP_PREPARE:
		mutex_lock(&tegra_cpu_lock);
		cpu_set(cpu, edp_cpumask);

		cpu_speed = tegra_getspeed(0);
		edp_speed = get_edp_limit_speed(cpu_speed);
		new_speed = min_t(unsigned int, cpu_speed, edp_speed);
		new_speed = sysedp_cap_speed(new_speed);
		if (new_speed < cpu_speed ||
		    (!is_suspended && tegra_is_edp_reg_idle_supported())) {
			ret = tegra_cpu_set_speed_cap_locked(NULL);
			if (cur_cpupwr_freqcap >= edp_speed) {
				pr_debug("cpu-tegra:%sforce EDP limit %u kHz\n",
					 ret ? " failed to " : " ", new_speed);
			}
		}
		if (ret)
			cpu_clear(cpu, edp_cpumask);
		mutex_unlock(&tegra_cpu_lock);
		break;
	case CPU_DEAD:
		mutex_lock(&tegra_cpu_lock);
		cpu_clear(cpu, edp_cpumask);
		tegra_cpu_set_speed_cap_locked(NULL);
		mutex_unlock(&tegra_cpu_lock);
		break;
	}
	return notifier_from_errno(ret);
}

static struct notifier_block tegra_cpu_edp_notifier = {
	.notifier_call = tegra_cpu_edp_notify,
};

static void tegra_cpu_edp_init(bool resume)
{
	if (!cpumask_weight(&edp_cpumask))
		edp_cpumask = *cpu_online_mask;

	if (!tegra_is_cpu_edp_supported()) {
		if (!resume)
			pr_info("cpu-tegra: no EDP table is provided\n");
		return;
	}

	if (!resume) {
		register_hotcpu_notifier(&tegra_cpu_edp_notifier);
		pr_info("cpu-tegra: init EDP limit: %u MHz\n",
			get_edp_freq_limit(cpumask_weight(&edp_cpumask))/1000);
	}
}

static void tegra_cpu_edp_exit(void)
{
	unregister_hotcpu_notifier(&tegra_cpu_edp_notifier);
}

static unsigned int cpu_reg_mode_predict_idle_limit(void)
{
	unsigned int cpus, limit;

	if (!tegra_is_edp_reg_idle_supported())
		return 0;

	cpus = cpumask_weight(&edp_cpumask);
	BUG_ON(cpus == 0);
	limit = tegra_get_reg_idle_freq(cpus);
	return limit ? : 1; /* bump 0 to 1kHz to differentiate from no-table */
}

static int cpu_reg_mode_limits_init(void)
{
	if (!tegra_is_edp_reg_idle_supported()) {
		reg_mode = -ENOENT;
		return -ENOENT;
	}

	reg_mode_force_normal = false;
	return 0;
}

int tegra_cpu_reg_mode_force_normal(bool force)
{
	int ret = 0;

	mutex_lock(&tegra_cpu_lock);
	if (tegra_is_edp_reg_idle_supported()) {
		reg_mode_force_normal = force;
		ret = tegra_cpu_set_speed_cap_locked(NULL);
	}
	mutex_unlock(&tegra_cpu_lock);
	return ret;
}

int tegra_update_cpu_edp_limits(void)
{
	int ret;

	mutex_lock(&tegra_cpu_lock);
	tegra_recalculate_cpu_edp_limits();
	cpu_reg_mode_limits_init();
	ret = tegra_cpu_set_speed_cap_locked(NULL);
	mutex_unlock(&tegra_cpu_lock);

	return ret;
}

#ifdef CONFIG_DEBUG_FS

static int reg_mode_force_normal_get(void *data, u64 *val)
{
	*val = (u64)reg_mode_force_normal;
	return 0;
}
static int reg_mode_force_normal_set(void *data, u64 val)
{
	return tegra_cpu_reg_mode_force_normal(val);
}
DEFINE_SIMPLE_ATTRIBUTE(force_normal_fops, reg_mode_force_normal_get,
			reg_mode_force_normal_set, "%llu\n");

static int reg_mode_get(void *data, u64 *val)
{
	*val = (u64)reg_mode;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(reg_mode_fops, reg_mode_get, NULL, "0x%llx\n");

static int cpu_edp_limit_get(void *data, u64 *val)
{
	*val = (u64)tegra_get_edp_max_freq(cpumask_weight(&edp_cpumask),
					   is_lp_cluster() ? MODE_LP : MODE_G);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(cpu_edp_limit_fops, cpu_edp_limit_get, NULL, "%lld\n");

static int __init tegra_edp_debug_init(struct dentry *cpu_tegra_debugfs_root)
{
	if (!debugfs_create_file("reg_mode_force_normal", 0644,
			cpu_tegra_debugfs_root, NULL, &force_normal_fops))
		return -ENOMEM;

	if (!debugfs_create_file("reg_mode", 0444,
			cpu_tegra_debugfs_root, NULL, &reg_mode_fops))
		return -ENOMEM;

	if (!debugfs_create_file("cpu_edp_limit", 0444,
			cpu_tegra_debugfs_root, NULL, &cpu_edp_limit_fops))
		return -ENOMEM;

	return 0;
}
#endif

#else	/* CONFIG_TEGRA_EDP_LIMITS */
#define get_edp_limit_speed(requested_speed) (requested_speed)
#define tegra_cpu_edp_init(resume)
#define tegra_cpu_edp_exit()
#define tegra_edp_debug_init(cpu_tegra_debugfs_root) (0)
#define cpu_reg_mode_predict_idle_limit() (0)
#endif	/* CONFIG_TEGRA_EDP_LIMITS */

#ifdef CONFIG_DEBUG_FS

static int cpu_edp_safe_get(void *data, u64 *val)
{
	struct clk *cluster_clk = data;
	*val = clk_get_cpu_edp_safe_rate(cluster_clk);
	return 0;
}
static int cpu_edp_safe_set(void *data, u64 val)
{
	struct clk *cluster_clk = data;
	return clk_config_cpu_edp_safe_rate(cpu_clk, cluster_clk, val);
}
DEFINE_SIMPLE_ATTRIBUTE(cpu_edp_safe_fops, cpu_edp_safe_get,
			cpu_edp_safe_set, "%llu\n");

static int __init tegra_cap_debugfs_init(struct dentry *cpu_tegra_debugfs_root)
{
	if (!debugfs_create_u32("cap_kHz", 0444, cpu_tegra_debugfs_root,
				&volt_capped_speed))
		return -ENOMEM;

#ifdef CONFIG_TEGRA_HMP_CLUSTER_CONTROL
	if (!debugfs_create_u32("cap_mv", 0444, cpu_tegra_debugfs_root,
				&volt_cap_level))
		return -ENOMEM;
#endif
	return 0;
}

static int __init tegra_virt_debugfs_init(struct dentry *cpu_tegra_debugfs_root)
{
#ifdef CONFIG_TEGRA_HMP_CLUSTER_CONTROL
	if (!debugfs_create_u32("lp_to_gratio", 0444,
			cpu_tegra_debugfs_root, &lp_to_g_ratio))
		return -ENOMEM;

	if (!debugfs_create_bool("disable_virtualization", 0644,
			cpu_tegra_debugfs_root, &disable_virtualization))
		return -ENOMEM;

	if (!debugfs_create_file("cpu_g_edp_safe_rate", 0644,
			cpu_tegra_debugfs_root, g_clock, &cpu_edp_safe_fops))
		return -ENOMEM;

	if (!debugfs_create_file("cpu_lp_edp_safe_rate", 0644,
			cpu_tegra_debugfs_root, lp_clock, &cpu_edp_safe_fops))
		return -ENOMEM;
#endif
	return 0;
}

static int force_cpu_unsigned_set(void *data, u64 val)
{
	unsigned *var = data;

	mutex_lock(&tegra_cpu_lock);

	if (*var != val) {
		*var = val;
		tegra_cpu_set_speed_cap_locked(NULL);
	}

	mutex_unlock(&tegra_cpu_lock);

	return 0;
}

static int force_cpu_unsigned_get(void *data, u64 *val)
{
	unsigned *var = data;
	*val = *var;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(force_cpu_unsigned_fops,
			force_cpu_unsigned_get,
			force_cpu_unsigned_set, "%llu\n");

static int status_show(struct seq_file *file, void *data)
{
	mutex_lock(&tegra_cpu_lock);

#ifdef CONFIG_TEGRA_EDP_LIMITS
	seq_printf(file, "cpus online : %u\n", cpumask_weight(&edp_cpumask));
#endif
	seq_printf(file, "cpu power   : %u%s\n", force_cpupwr ?: cur_cpupwr,
		   force_cpupwr ? "(forced)" : "");
	seq_printf(file, "cpu cap     : %u kHz\n", cur_cpupwr_freqcap);

	mutex_unlock(&tegra_cpu_lock);
	return 0;
}

static int status_open(struct inode *inode, struct file *file)
{
	return single_open(file, status_show, inode->i_private);
}

static const struct file_operations status_fops = {
	.open = status_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static struct dentry *cpu_tegra_debugfs_root;

static int __init tegra_cpu_debug_init(void)
{
	struct dentry *sysedp_capping_dir;

	cpu_tegra_debugfs_root = debugfs_create_dir("cpu-tegra", NULL);

	if (!cpu_tegra_debugfs_root)
		return -ENOMEM;

	if (tegra_edp_debug_init(cpu_tegra_debugfs_root))
		goto err_out;

	if (tegra_cap_debugfs_init(cpu_tegra_debugfs_root))
		goto err_out;

	if (tegra_virt_debugfs_init(cpu_tegra_debugfs_root))
		goto err_out;

	sysedp_capping_dir = debugfs_create_dir("sysedp-capping",
						cpu_tegra_debugfs_root);
	if (!sysedp_capping_dir)
		goto err_out;

	if (!debugfs_create_file("force_cpu_freq_cap", S_IRUGO | S_IWUSR,
				 sysedp_capping_dir, &force_cpupwr_freqcap,
				 &force_cpu_unsigned_fops))
		goto err_out;

	if (!debugfs_create_file("force_cpu_power_cap", S_IRUGO | S_IWUSR,
				 sysedp_capping_dir, &force_cpupwr,
				 &force_cpu_unsigned_fops))
		goto err_out;

	if (!debugfs_create_file("status", S_IRUGO,
				 sysedp_capping_dir,
				 NULL, &status_fops))
		goto err_out;

	return 0;

err_out:
	debugfs_remove_recursive(cpu_tegra_debugfs_root);
	return -ENOMEM;
}

static void __exit tegra_cpu_debug_exit(void)
{
	debugfs_remove_recursive(cpu_tegra_debugfs_root);
}

late_initcall(tegra_cpu_debug_init);
module_exit(tegra_cpu_debug_exit);
#endif /* CONFIG_DEBUG_FS */

static int tegra_verify_speed(struct cpufreq_policy *policy)
{
	return cpufreq_frequency_table_verify(policy, freq_table);
}



static unsigned int tegra_getspeed_actual(void)
{
	return clk_get_rate(cpu_clk) / 1000;
}

unsigned int tegra_getspeed(unsigned int cpu)
{
	unsigned long rate;
	unsigned int on_lp_cluster;
	unsigned long flags;

	if (cpu >= CONFIG_NR_CPUS)
		return 0;

	if (!cpu_clk)
		return 0;

	/*
	 * Explicitly holding the cpu_clk lock across get_rate and is_lp_cluster
	 * check to prevent a racy getspeed. Cannot hold tegra_cpu_lock here.
	 */
	clk_lock_save(cpu_clk, &flags);

	rate = clk_get_rate_locked(cpu_clk) / 1000;

	on_lp_cluster = is_lp_cluster();

	clk_unlock_restore(cpu_clk, &flags);

	if (on_lp_cluster)
		rate = lp_to_virtual_gfreq(rate);

	return rate;
}

/**
 * tegra_update_cpu_speed - Sets given real rate for the current cluster
 * @rate - Actual rate to set for the current cluster
 * Returns 0 on success, -ve on failure
 * Must be called with tegra_cpu_lock held
 */
int tegra_update_cpu_speed(unsigned long rate)
{
	int ret = 0;
	struct cpufreq_freqs freqs;
	struct cpufreq_freqs actual_freqs;
	struct cpufreq_policy *policy = cpufreq_cpu_get(0); /* boot CPU */
	unsigned int mode, mode_limit;

	if (!cpu_clk)
		return -EINVAL;

	freqs.old = tegra_getspeed(0);
	actual_freqs.new = rate;
	actual_freqs.old = tegra_getspeed_actual();
	freqs.new = rate;
	if (is_lp_cluster())
		freqs.new = lp_to_virtual_gfreq(rate);

	rate = clk_round_rate(cpu_clk, actual_freqs.new * 1000UL);
	if (!IS_ERR_VALUE(rate))
		actual_freqs.new = rate / 1000;

	mode_limit = cpu_reg_mode_predict_idle_limit();
	mode = REGULATOR_MODE_NORMAL;
	if (mode_limit && (mode_limit < actual_freqs.new ||
	    reg_mode_force_normal)) {
		mode_limit = 0;		/* prevent further mode controls */
		if (reg_mode != mode) {
			ret = tegra_dvfs_rail_set_mode(tegra_cpu_rail, mode);
			if (ret)
				goto _err;
			reg_mode = mode;
		}
	}

	/* Only bail out if both virt and actual new/old freqs are the same. */
	if (actual_freqs.old == actual_freqs.new && freqs.old == freqs.new)
		goto _out;

	/*
	 * Vote on memory bus frequency based on cpu frequency
	 * This sets the minimum frequency, display or avp may request higher
	 */
	if (actual_freqs.old < actual_freqs.new) {
		ret = tegra_update_mselect_rate(actual_freqs.new);
		if (ret) {
			pr_err("cpu-tegra: Failed to scale mselect for cpu frequency %u kHz\n",
				actual_freqs.new);
			goto _err;
		}
	}

	if (freqs.old < freqs.new) {
		if (emc_clk) {
			ret = clk_set_rate(emc_clk,
					   tegra_emc_cpu_limit(freqs.new));
			if (ret) {
				pr_err("cpu-tegra: Failed to scale emc for cpu frequency %u kHz\n",
					freqs.new);
				goto _err;
			}
		}
	}

	if (policy && freqs.old != freqs.new)
		cpufreq_freq_transition_begin(policy, &freqs);

#ifdef CONFIG_CPU_FREQ_DEBUG
	printk(KERN_DEBUG "cpufreq-tegra: transition: %u --> %u\n",
	       freqs.old, freqs.new);
#endif

	ret = clk_set_rate(cpu_clk, actual_freqs.new * 1000);

	if (policy && freqs.old != freqs.new) {
		freqs.new = tegra_getspeed(0);
		cpufreq_freq_transition_end(policy, &freqs, 0);
	}

	if (ret) {
		pr_err("cpu-tegra: Failed to set cpu frequency to %d kHz\n",
			actual_freqs.new);
		goto _err;
	}

	if (actual_freqs.old > actual_freqs.new)
		tegra_update_mselect_rate(actual_freqs.new);

	if (emc_clk && freqs.old > freqs.new)
		clk_set_rate(emc_clk, tegra_emc_cpu_limit(freqs.new));

_out:
	mode = REGULATOR_MODE_IDLE;
	if ((mode_limit >= actual_freqs.new) && (reg_mode != mode))
		if (!tegra_dvfs_rail_set_mode(tegra_cpu_rail, mode))
			reg_mode = mode;

_err:
	if (policy)
		cpufreq_cpu_put(policy);
	return ret;
}

#ifdef CONFIG_TEGRA_HMP_CLUSTER_CONTROL

static int update_volt_cap(struct clk *cluster_clk, int level,
			   unsigned int *capped_speed)
{
#ifndef CONFIG_TEGRA_CPU_VOLT_CAP
	unsigned long cap_rate, flags;

	/*
	 * If CPU is on DFLL clock, cap level is already applied by DFLL control
	 * loop directly - no cap speed to be set. Otherwise, convert cap level
	 * to cap speed. Hold cpu_clk lock to avoid race with rail mode change.
	 */
	clk_lock_save(cpu_clk, &flags);
	if (!level || tegra_dvfs_rail_is_dfll_mode(tegra_cpu_rail))
		cap_rate = 0;
	else
		cap_rate = tegra_dvfs_predict_hz_at_mv_max_tfloor(
			cluster_clk, level);
	clk_unlock_restore(cpu_clk, &flags);

	if (!IS_ERR_VALUE(cap_rate)) {
		volt_cap_level = level;
		*capped_speed = cap_rate / 1000;
		pr_debug("%s: Updated capped speed to %ukHz (cap level %dmV)\n",
			__func__, *capped_speed, level);
		return 0;
	}

	pr_err("%s: Failed to find capped speed for cap level %dmV\n",
		__func__, level);
#endif
	return -EINVAL;
}

int tegra_cpu_volt_cap_apply(int *cap_idx, int new_idx, int level)
{
	int ret;
	unsigned int capped_speed;
	struct clk *cluster_clk;

	mutex_lock(&tegra_cpu_lock);
	if (cap_idx)
		*cap_idx = new_idx;	/* protect index update by cpu mutex */
	else
		level = volt_cap_level; /* keep current level if no index*/

	cluster_clk = is_lp_cluster() ? lp_clock : g_clock;
	ret = update_volt_cap(cluster_clk, level, &capped_speed);
	if (!ret) {
		if (volt_capped_speed != capped_speed) {
			volt_capped_speed = capped_speed;
			tegra_cpu_set_speed_cap_locked(NULL);
		}
	}
	mutex_unlock(&tegra_cpu_lock);

	return ret;
}

static int tegra_cluster_switch_locked(struct clk *cpu_clk, struct clk *new_clk)
{
	int ret;
	unsigned int capped_speed;

	ret = clk_set_parent(cpu_clk, new_clk);
	if (ret)
		return ret;

	if (!update_volt_cap(new_clk, volt_cap_level, &capped_speed))
		volt_capped_speed = capped_speed;

	tegra_cpu_set_speed_cap_locked(NULL);

	return 0;
}

/* Explicitly hold the lock here to protect manual cluster switch requests */
int tegra_cluster_switch(struct clk *cpu_clk, struct clk *new_clk)
{
	int ret;

	/* Order hotplug lock before cpufreq lock. Deadlock otherwise. */
	get_online_cpus();

	mutex_lock(&tegra_cpu_lock);
	ret = tegra_cluster_switch_locked(cpu_clk, new_clk);
	mutex_unlock(&tegra_cpu_lock);

	put_online_cpus();

	return ret;
}

void tegra_cluster_switch_prolog(unsigned int flags) {}
void tegra_cluster_switch_epilog(unsigned int flags) {}

#define UP_DELAY_MS		70
#define DOWN_DELAY_MS		100

static int target_state;
static unsigned int up_delay;
static unsigned int down_delay;
static unsigned int auto_cluster_enable;
static unsigned int idle_top_freq;
static unsigned int idle_bottom_freq;
static struct delayed_work cluster_switch_work;

static void queue_clusterswitch(bool up)
{
	unsigned long delay_jif;

	cancel_delayed_work(&cluster_switch_work);

	delay_jif = msecs_to_jiffies(down_delay);

	if (up)
		delay_jif = msecs_to_jiffies(up_delay);

	schedule_delayed_work(&cluster_switch_work, delay_jif);
}

static void cluster_switch_worker(struct work_struct *work)
{
	int current_state;
	int err;
	struct clk *target_clk = NULL;

	get_online_cpus();
	mutex_lock(&tegra_cpu_lock);

	if (!auto_cluster_enable)
		goto out;

	current_state = is_lp_cluster() ? SLOW_CLUSTER : FAST_CLUSTER;

	if (current_state != target_state) {
		if (target_state  == SLOW_CLUSTER)
			target_clk = lp_clock;
		else
			target_clk = g_clock;
	}

	if (target_clk) {
		err = tegra_cluster_switch_locked(cpu_clk, target_clk);
		if (err) {
			pr_err("%s: Cluster switch to %d failed:%d\n",
				__func__, target_state, err);
			target_state = is_lp_cluster() ?
					SLOW_CLUSTER : FAST_CLUSTER;
		}
	}
out:
	mutex_unlock(&tegra_cpu_lock);
	put_online_cpus();
}

/* Must be called with tegra_cpu_lock held */
static void tegra_auto_cluster_switch(void)
{
	unsigned int cpu_freq = tegra_getspeed(0);

	if (!auto_cluster_enable)
		return;

	if (is_lp_cluster()) {
		if (cpu_freq >= idle_top_freq && target_state != FAST_CLUSTER) {
			target_state = FAST_CLUSTER;
			queue_clusterswitch(true);
		} else if (cpu_freq < idle_top_freq &&
			   target_state == FAST_CLUSTER) {
			target_state = SLOW_CLUSTER;
			cancel_delayed_work(&cluster_switch_work);
		}
	} else {
		if (cpu_freq <= idle_bottom_freq &&
		    target_state !=  SLOW_CLUSTER) {
			target_state = SLOW_CLUSTER;
			queue_clusterswitch(false);
		} else if (cpu_freq > idle_bottom_freq &&
			   target_state == SLOW_CLUSTER) {
			target_state = FAST_CLUSTER;
			cancel_delayed_work(&cluster_switch_work);
		}
	}
}

#define show(filename, param)			\
static ssize_t show_##filename						\
(struct kobject *kobj, struct kobj_attribute *attr, char *buf)		\
{									\
	return scnprintf(buf, PAGE_SIZE, "%u\n", (param));		\
}									\

#define store(filename, param)			\
static ssize_t store_##filename						\
(struct kobject *kobj, struct kobj_attribute *attr,			\
const char *buf, size_t count)						\
{									\
	unsigned int val;						\
	if (kstrtouint(buf, 0, &val))					\
		return -EINVAL;						\
	param = val;							\
	return count;							\
}									\

#define cputegra_attr_cust(attr, filename, show, store)			\
static struct kobj_attribute attr =					\
	__ATTR(filename, 0644, show, store);				\

#define cputegra_attr(attr, filename, param)				\
show(filename, param);							\
store(filename, param);							\
cputegra_attr_cust(attr, filename, show_##filename, store_##filename)	\


show(enable, auto_cluster_enable);
static ssize_t store_enable(struct kobject *kobj, struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	unsigned int val;

	if (kstrtouint(buf, 0, &val))
		return -EINVAL;

	if (val > 1)
		return -EINVAL;

	mutex_lock(&tegra_cpu_lock);
	if (val == auto_cluster_enable) {
		mutex_unlock(&tegra_cpu_lock);
		return count;
	}
	auto_cluster_enable = val;
	if (!val)
		cancel_delayed_work(&cluster_switch_work);
	else
		tegra_auto_cluster_switch();
	mutex_unlock(&tegra_cpu_lock);

	return count;
}

cputegra_attr_cust(enable, enable, show_enable, store_enable);
cputegra_attr(updelay, up_delay_msec, up_delay);
cputegra_attr(downdelay, down_delay_msec, down_delay);
cputegra_attr(top_freq, idle_top_freq, idle_top_freq);
cputegra_attr(bottom_freq, idle_bottom_freq, idle_bottom_freq);

static const struct attribute *tegra_auto_cluster_switch_attrs[] = {
	&enable.attr,
	&updelay.attr,
	&downdelay.attr,
	&top_freq.attr,
	&bottom_freq.attr,
	NULL,
};

static struct kobject *tegra_auto_cluster_switch_kobj;

static int tegra_auto_cluster_switch_init(void)
{
	int ret;

	INIT_DELAYED_WORK(&cluster_switch_work,
				cluster_switch_worker);
	lp_clock = tegra_get_clock_by_name("cpu_lp");
	g_clock = tegra_get_clock_by_name("cpu_g");

	if (!g_clock || !lp_clock) {
		pr_err("%s: Clock not present\n", __func__);
		return -EINVAL;
	}
	target_state = is_lp_cluster() ? SLOW_CLUSTER : FAST_CLUSTER;
	idle_top_freq = lp_to_virtual_gfreq(clk_get_max_rate(lp_clock)) / 1000;
	idle_bottom_freq = clk_get_min_rate(g_clock) / 1000;
	up_delay = UP_DELAY_MS;
	down_delay = DOWN_DELAY_MS;

	tegra_auto_cluster_switch_kobj =
		kobject_create_and_add("tegra_auto_cluster_switch",
				kernel_kobj);

	if (!tegra_auto_cluster_switch_kobj) {
		pr_err("%s: Couldn't create kobj\n", __func__);
		return -ENOMEM;
	}

	ret = sysfs_create_files(tegra_auto_cluster_switch_kobj,
				tegra_auto_cluster_switch_attrs);
	if (ret) {
		pr_err("%s: Couldn't create sysfs files\n", __func__);
		return ret;
	}

	return 0;
}

static void tegra_auto_cluster_switch_exit(void)
{
	if (tegra_auto_cluster_switch_kobj)
		sysfs_remove_files(tegra_auto_cluster_switch_kobj,
			tegra_auto_cluster_switch_attrs);
}

#else
static inline int tegra_auto_cluster_switch_init(void)
{ return 0; }
static inline void tegra_auto_cluster_switch(void) {}
static inline void tegra_auto_cluster_switch_exit(void) {}
#endif

static unsigned long tegra_cpu_highest_speed(void) {
	unsigned long policy_max = ULONG_MAX;
	unsigned long rate = 0;
	int i;

	for_each_online_cpu(i) {
		if (force_policy_max)
			policy_max = min(policy_max, policy_max_speed[i]);
		rate = max(rate, target_cpu_speed[i]);
	}
	rate = min(rate, policy_max);
	return rate;
}

void tegra_cpu_set_volt_cap(unsigned int cap)
{
	mutex_lock(&tegra_cpu_lock);
	if (cap != volt_capped_speed) {
		volt_capped_speed = cap;
		tegra_cpu_set_speed_cap_locked(NULL);
	}
	mutex_unlock(&tegra_cpu_lock);
	if (cap)
		pr_debug("tegra_cpu:volt limit to %u Khz\n", cap);
	else
		pr_debug("tegra_cpu:volt limit removed\n");
}

static unsigned int volt_cap_speed(unsigned int requested_speed)
{
	if (volt_capped_speed && requested_speed > volt_capped_speed) {
		pr_debug("%s: Volt cap throttling %u khz to %u khz\n",
				__func__, requested_speed, volt_capped_speed);
		return volt_capped_speed;
	}
	return requested_speed;
}

/* Must be called with tegra_cpu_lock held */
int tegra_cpu_set_speed_cap_locked(unsigned int *speed_cap)
{
	int ret = 0;
	unsigned int new_speed = tegra_cpu_highest_speed();
	BUG_ON(!mutex_is_locked(&tegra_cpu_lock));

	if (is_suspended)
		return -EBUSY;

	if (!cpu_clk)
		return -EINVAL;

	/* Caps based on virtual G-cpu freq */
	new_speed = tegra_throttle_governor_speed(new_speed);
	new_speed = user_cap_speed(new_speed);

	/* Caps based on actual cluster freq */
	new_speed = map_to_actual_cluster_freq(new_speed);
#ifdef CONFIG_TEGRA_EDP_LIMITS
	new_speed = get_edp_limit_speed(new_speed);
	new_speed = sysedp_cap_speed(new_speed);
#endif
	new_speed = volt_cap_speed(new_speed);
	if (speed_cap)
		*speed_cap = new_speed;

	ret = tegra_update_cpu_speed(new_speed);
	if (ret == 0) {
		tegra_auto_hotplug_governor(new_speed, false);
		tegra_auto_cluster_switch();
	}
	return ret;
}

int tegra_cpu_set_speed_cap(unsigned int *speed_cap)
{
	int ret;
	mutex_lock(&tegra_cpu_lock);
	ret = tegra_cpu_set_speed_cap_locked(speed_cap);
	mutex_unlock(&tegra_cpu_lock);
	return ret;
}


int tegra_suspended_target(unsigned int target_freq)
{
	unsigned int new_speed = target_freq;

	if (!is_suspended)
		return -EBUSY;

	/* apply only "hard" caps */
	new_speed = tegra_throttle_governor_speed(new_speed);
	new_speed = map_to_actual_cluster_freq(new_speed);
	new_speed = get_edp_limit_speed(new_speed);

	return tegra_update_cpu_speed(new_speed);
}

static int tegra_target(struct cpufreq_policy *policy,
		       unsigned int target_freq,
		       unsigned int relation)
{
	int idx;
	unsigned int freq;
	unsigned int new_speed;
	int ret = 0;

	mutex_lock(&tegra_cpu_lock);

	ret = cpufreq_frequency_table_target(policy, freq_table, target_freq,
		relation, &idx);
	if (ret)
		goto _out;

	freq = freq_table[idx].frequency;

	target_cpu_speed[policy->cpu] = freq;
	ret = tegra_cpu_set_speed_cap_locked(&new_speed);
_out:
	mutex_unlock(&tegra_cpu_lock);

	return ret;
}

static int max_cpu_power_notify(struct notifier_block *b,
				unsigned long cpupwr, void *v)
{

	pr_debug("PM QoS Max CPU Power Notify: %lu\n", cpupwr);

	mutex_lock(&tegra_cpu_lock);
	/* store the new cpu power*/
	cur_cpupwr = cpupwr;

	tegra_cpu_set_speed_cap_locked(NULL);
	mutex_unlock(&tegra_cpu_lock);

	return NOTIFY_OK;
}

static struct notifier_block max_cpu_pwr_notifier = {
	.notifier_call = max_cpu_power_notify,
};

static int tegra_pm_notify(struct notifier_block *nb, unsigned long event,
	void *dummy)
{
	unsigned long rate;

	if (event == PM_SUSPEND_PREPARE) {
		if (!preserve_cpu_speed) {
			pm_qos_update_request(&cpufreq_min_req,
				freq_table[suspend_index].frequency);
			pm_qos_update_request(&cpufreq_max_req,
				freq_table[suspend_index].frequency);
		}

		mutex_lock(&tegra_cpu_lock);
		is_suspended = true;
#ifdef CONFIG_TEGRA_EDP_LIMITS
		if (tegra_is_edp_reg_idle_supported())
			reg_mode_force_normal = true;
#endif
		rate = freq_table[suspend_index].frequency;
		rate = map_to_actual_cluster_freq(rate);
		pr_info("Tegra cpufreq suspend: setting frequency to %lu kHz\n",
			rate);
		tegra_update_cpu_speed(rate);
		tegra_auto_hotplug_governor(rate, true);
		tegra_auto_cluster_switch();
		mutex_unlock(&tegra_cpu_lock);
	} else if (event == PM_POST_SUSPEND) {
		unsigned int freq;

		mutex_lock(&tegra_cpu_lock);
		is_suspended = false;
		reg_mode_force_normal = false;
		tegra_cpu_edp_init(true);
		tegra_cpu_set_speed_cap_locked(&freq);
		pr_info("Tegra cpufreq resume: restoring frequency to %u kHz\n",
			freq);
		mutex_unlock(&tegra_cpu_lock);

		if (!preserve_cpu_speed) {
			pm_qos_update_request(&cpufreq_max_req,
				PM_QOS_CPU_FREQ_MAX_DEFAULT_VALUE);
			pm_qos_update_request(&cpufreq_min_req,
				PM_QOS_CPU_FREQ_MIN_DEFAULT_VALUE);
		}
	}

	return NOTIFY_OK;
}

static struct notifier_block tegra_cpu_pm_notifier = {
	.notifier_call = tegra_pm_notify,
};

static int tegra_cpu_init(struct cpufreq_policy *policy)
{
	int idx, ret;
	unsigned int freq, rate;

	if (policy->cpu >= CONFIG_NR_CPUS)
		return -EINVAL;

	freq = tegra_getspeed(policy->cpu);
	if (emc_clk) {
		clk_set_rate(emc_clk, tegra_emc_cpu_limit(freq));
		clk_prepare_enable(emc_clk);
	}

	cpufreq_table_validate_and_show(policy, freq_table);

	/* clip boot frequency to table entry */
	ret = cpufreq_frequency_table_target(policy, freq_table, freq,
		CPUFREQ_RELATION_H, &idx);
	if (!ret && (freq != freq_table[idx].frequency)) {
		rate = freq_table[idx].frequency;
		rate = map_to_actual_cluster_freq(rate);
		ret = tegra_update_cpu_speed(rate);
		if (!ret)
			freq = freq_table[idx].frequency;
	}
	policy->cur = freq;
	target_cpu_speed[policy->cpu] = policy->cur;

	/* FIXME: what's the actual transition time? */
	policy->cpuinfo.transition_latency = 300 * 1000;

	cpumask_copy(policy->cpus, cpu_possible_mask);

	return 0;
}

static int tegra_cpu_exit(struct cpufreq_policy *policy)
{
	cpufreq_frequency_table_cpuinfo(policy, freq_table);
	if (emc_clk) {
		clk_disable_unprepare(emc_clk);
		clk_put(emc_clk);
	}
	clk_put(cpu_clk);
	return 0;
}

static int tegra_cpufreq_policy_notifier(
	struct notifier_block *nb, unsigned long event, void *data)
{
#ifdef CONFIG_TEGRA_THERMAL_THROTTLE
	static int once = 1;
#endif
	int i, ret;
	struct cpufreq_policy *policy = data;

	if (event == CPUFREQ_NOTIFY) {
		ret = cpufreq_frequency_table_target(policy, freq_table,
			policy->max, CPUFREQ_RELATION_H, &i);
		policy_max_speed[policy->cpu] =
			ret ? policy->max : freq_table[i].frequency;

#ifdef CONFIG_TEGRA_THERMAL_THROTTLE
		if (once &&
		    sysfs_merge_group(&policy->kobj, &stats_attr_grp) == 0)
			once = 0;
#endif
	}
	return NOTIFY_OK;
}

static struct notifier_block tegra_cpufreq_policy_nb = {
	.notifier_call = tegra_cpufreq_policy_notifier,
};

static struct freq_attr *tegra_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
#ifdef CONFIG_TEGRA_THERMAL_THROTTLE
	&throttle,
#endif
	NULL,
};

static struct cpufreq_driver tegra_cpufreq_driver = {
	.verify		= tegra_verify_speed,
	.target		= tegra_target,
	.get		= tegra_getspeed,
	.init		= tegra_cpu_init,
	.exit		= tegra_cpu_exit,
	.name		= "tegra",
	.attr		= tegra_cpufreq_attr,
};

static int __init tegra_cpufreq_init(void)
{
	int ret = 0;
	unsigned int cpu_suspend_freq = 0;
	unsigned int i;

	struct tegra_cpufreq_table_data *table_data =
		tegra_cpufreq_table_get();
	if (IS_ERR_OR_NULL(table_data))
		return -EINVAL;

	freq_table = table_data->freq_table;

	preserve_cpu_speed = table_data->preserve_across_suspend;
	cpu_suspend_freq = tegra_cpu_suspend_freq();
	if (cpu_suspend_freq == 0) {
		suspend_index = table_data->suspend_index;
	} else {
		for (i = 0; freq_table[i].frequency != CPUFREQ_TABLE_END; i++) {
			if (freq_table[i].frequency >= cpu_suspend_freq)
				break;
		}
		suspend_index = i;
	}

	cpu_clk = clk_get_sys(NULL, "cpu");
	if (IS_ERR(cpu_clk))
		return PTR_ERR(cpu_clk);

	clk_prepare_enable(cpu_clk);

	emc_clk = clk_get_sys("tegra-cpu", "cpu_emc");
	if (IS_ERR(emc_clk))
		emc_clk = NULL;

	ret = tegra_auto_hotplug_init(&tegra_cpu_lock);
	if (ret)
		return ret;

	ret = tegra_auto_cluster_switch_init();
	if (ret)
		return ret;

	mutex_lock(&tegra_cpu_lock);
	tegra_cpu_edp_init(false);
	mutex_unlock(&tegra_cpu_lock);

	cur_cpupwr = PM_QOS_CPU_POWER_MAX_DEFAULT_VALUE;
	cur_cpupwr_freqcap = PM_QOS_CPU_FREQ_MAX_DEFAULT_VALUE;

	ret = pm_qos_add_notifier(PM_QOS_MAX_CPU_POWER,
				  &max_cpu_pwr_notifier);
	if (ret)
		return ret;

	pm_qos_add_request(&cpufreq_max_req, PM_QOS_CPU_FREQ_MAX,
		PM_QOS_CPU_FREQ_MAX_DEFAULT_VALUE);
	pm_qos_add_request(&cpufreq_min_req, PM_QOS_CPU_FREQ_MIN,
		PM_QOS_CPU_FREQ_MIN_DEFAULT_VALUE);

	ret = register_pm_notifier(&tegra_cpu_pm_notifier);

	if (ret)
		return ret;

	ret = cpufreq_register_notifier(
		&tegra_cpufreq_policy_nb, CPUFREQ_POLICY_NOTIFIER);

	if (ret)
		return ret;

	return cpufreq_register_driver(&tegra_cpufreq_driver);
}

#ifdef CONFIG_TEGRA_CPU_FREQ_GOVERNOR_KERNEL_START
static int __init tegra_cpufreq_governor_start(void)
{
	/*
	 * At this point, the full range of clocks should be available,
	 * scaling up can start: set the CPU frequency, maximum possible
	 */
	struct cpufreq_policy *policy;

	policy = cpufreq_cpu_get(0);
	if (!policy || tegra_target(policy, UINT_MAX, CPUFREQ_RELATION_H))
		pr_warn("Failed to set maximum possible CPU frequency\n");
	else
		pr_info("Set maximum possible CPU frequency %u\n",
			tegra_getspeed_actual());

	if (policy)
		cpufreq_cpu_put(policy);

	return 0;
}

late_initcall_sync(tegra_cpufreq_governor_start);
#endif

static void __exit tegra_cpufreq_exit(void)
{
	tegra_cpu_edp_exit();
	tegra_auto_hotplug_exit();
	tegra_auto_cluster_switch_exit();
	cpufreq_unregister_driver(&tegra_cpufreq_driver);
	cpufreq_unregister_notifier(
		&tegra_cpufreq_policy_nb, CPUFREQ_POLICY_NOTIFIER);
}


MODULE_AUTHOR("Colin Cross <ccross@android.com>");
MODULE_DESCRIPTION("cpufreq driver for Nvidia Tegra2");
MODULE_LICENSE("GPL");
module_init(tegra_cpufreq_init);
module_exit(tegra_cpufreq_exit);
