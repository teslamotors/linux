/*
 * drivers/cpuquiet/cpuquiet-smp-hotplug.c
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/cpu.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/cpuquiet.h>
#include <linux/pm_qos.h>
#include <linux/debugfs.h>

#define INITIAL_STATE		CPQ_DISABLED
#define HOTPLUG_DELAY_MS	100

enum {
	CPQ_DISABLED = 0,
	CPQ_ENABLED,
};

static bool enable;
static unsigned int idle_bottom_freq;
static unsigned long hotplug_timeout;
static wait_queue_head_t wait_cpu;

static int cpq_state;
static DEFINE_MUTEX(cpuquiet_lock);
static DEFINE_MUTEX(cpq_lock_stats);

static struct kobject *auto_sysfs_kobject;
static struct work_struct idle_stop_work;
static struct work_struct apply_constraints_work;

static struct {
	cputime64_t time_up_total;
	u64 last_update;
	unsigned int up_down_count;
} hp_stats[CONFIG_NR_CPUS];

static void hp_init_stats(void)
{
	int i;
	u64 cur_jiffies = get_jiffies_64();

	mutex_lock(&cpq_lock_stats);

	for (i = 0; i < ARRAY_SIZE(hp_stats); i++) {
		hp_stats[i].time_up_total = 0;
		hp_stats[i].last_update = cur_jiffies;

		hp_stats[i].up_down_count = 0;
		if ((i < nr_cpu_ids) && cpu_online(i))
			hp_stats[i].up_down_count = 1;
	}

	mutex_unlock(&cpq_lock_stats);
}

/* Must be called with cpq_lock_stats held */
static void __hp_stats_update(unsigned int cpu, bool up)
{
	u64 cur_jiffies = get_jiffies_64();
	bool was_up;

	was_up = hp_stats[cpu].up_down_count & 0x1;

	if (was_up)
		hp_stats[cpu].time_up_total =
			hp_stats[cpu].time_up_total +
			(cur_jiffies - hp_stats[cpu].last_update);

	if (was_up != up) {
		hp_stats[cpu].up_down_count++;
		if ((hp_stats[cpu].up_down_count & 0x1) != up) {
			pr_err("hotplug stats out of sync with CPU%d",
			       (cpu < nr_cpu_ids) ?  cpu : 0);
			hp_stats[cpu].up_down_count ^=  0x1;
		}
	}
	hp_stats[cpu].last_update = cur_jiffies;
}

static void hp_stats_update(unsigned int cpu, bool up)
{
	mutex_lock(&cpq_lock_stats);

	__hp_stats_update(cpu, up);

	mutex_unlock(&cpq_lock_stats);
}

/**
 * violates_constraints - Checks to see if online/offlining cpus violates
 * pm_qos constraints
 *
 * @cpus: Number of cpus to online if > 0 or offline if < 0
 *
 * @ret: Number of cpus to online (> 0) or offline (< 0) to meet constraints
 */
static int violates_constraints(int cpus)
{
	int target_nr_cpus;
	int max_cpus = pm_qos_request(PM_QOS_MAX_ONLINE_CPUS);
	int min_cpus = pm_qos_request(PM_QOS_MIN_ONLINE_CPUS);

	if (min_cpus > num_possible_cpus())
		min_cpus = 0;

	if (max_cpus <= 0)
		max_cpus = num_present_cpus();

	target_nr_cpus = num_online_cpus();
	target_nr_cpus += cpus;

	if (target_nr_cpus < min_cpus)
		return min_cpus - target_nr_cpus;

	if (target_nr_cpus > max_cpus)
		return max_cpus - target_nr_cpus;

	return 0;
}

static void apply_constraints(struct work_struct *work)
{
	int action, cpu;
	bool up = true;
	struct cpumask cpu_online;
	struct device *dev;

	mutex_lock(&cpuquiet_lock);

	if (cpq_state == CPQ_DISABLED) {
		mutex_unlock(&cpuquiet_lock);
		return;
	}

	action = violates_constraints(0);

	if (action < 0) {
		up = false;
		action *= -1;
	}

	cpu_online = *cpu_online_mask;

	cpu = 0;
	for (; action > 0; action--) {
		if (up) {
			cpu = cpumask_next_zero(cpu, &cpu_online);
			if (cpu >= nr_cpu_ids)
				break;
			dev = get_cpu_device(cpu);
			device_online(dev);
			cpumask_set_cpu(cpu, &cpu_online);
		} else {
			cpu = cpumask_next(cpu, &cpu_online);
			if (cpu >= nr_cpu_ids)
				break;
			dev = get_cpu_device(cpu);
			device_offline(dev);
			cpumask_clear_cpu(cpu, &cpu_online);
		}
	}

	mutex_unlock(&cpuquiet_lock);
}

static int update_core_config(unsigned int cpu, bool up)
{
	int err;
	struct device *dev = get_cpu_device(cpu);
	mutex_lock(&cpuquiet_lock);

	if (cpq_state == CPQ_DISABLED || !cpu || cpu >= nr_cpu_ids) {
		err = -EINVAL;
		goto ret;
	}

	/* CPU is already in requested state */
	if (cpu_online(cpu) == up) {
		err = 0;
		goto ret;
	}

	if (up) {
		err = violates_constraints(1);
		if (err)
			goto ret;

		err = device_online(dev);
	} else {
		err = violates_constraints(-1);
		if (err)
			goto ret;

		err = device_offline(dev);
	}
ret:
	mutex_unlock(&cpuquiet_lock);

	return err;
}

static int quiesence_cpu(unsigned int cpunumber, bool sync)
{
	int err = 0;

	err = update_core_config(cpunumber, false);
	if (err || !sync)
		return err;

	err = wait_event_interruptible_timeout(wait_cpu,
					       !cpu_online(cpunumber),
					       hotplug_timeout);

	if (err < 0)
		return err;

	if (err > 0)
		return 0;
	else
		return -ETIMEDOUT;
}

static int wake_cpu(unsigned int cpunumber, bool sync)
{
	int err = 0;

	err = update_core_config(cpunumber, true);
	if (err || !sync)
		return err;

	err = wait_event_interruptible_timeout(wait_cpu, cpu_online(cpunumber),
					       hotplug_timeout);

	if (err < 0)
		return err;

	if (err > 0)
		return 0;
	else
		return -ETIMEDOUT;
}

static int min_cpus_notify(struct notifier_block *nb, unsigned long n,
					void *p)
{
	schedule_work(&apply_constraints_work);

	return NOTIFY_OK;
}

static int max_cpus_notify(struct notifier_block *nb, unsigned long n,
					void *p)
{
	schedule_work(&apply_constraints_work);

	return NOTIFY_OK;
}

static void idle_stop_governor(struct work_struct *work)
{
	unsigned int freq;

	if (cpq_state == CPQ_DISABLED)
		return;

	freq = cpufreq_get(0);

	if (freq <= idle_bottom_freq && num_online_cpus() == 1)
		cpuquiet_device_busy();
	else
		cpuquiet_device_free();
}

static int cpu_online_notify(struct notifier_block *nfb,
				unsigned long action, void *hcpu)
{
	unsigned long cpu = (unsigned long)hcpu;

	switch (action) {
	case CPU_POST_DEAD:
		hp_stats_update(cpu, false);
		schedule_work(&idle_stop_work);
		break;
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		hp_stats_update(cpu, true);
		schedule_work(&idle_stop_work);
		break;
	}

	return NOTIFY_OK;
}

static int cpufreq_notify(struct notifier_block *nfb, unsigned long val,
				void *data)
{
	if (val == CPUFREQ_POSTCHANGE)
		schedule_work(&idle_stop_work);

	return NOTIFY_OK;
}

static void delay_callback(struct cpuquiet_attribute *attr)
{
	unsigned long val;

	if (attr) {
		val = (*((unsigned long *)(attr->param)));
		(*((unsigned long *)(attr->param))) = msecs_to_jiffies(val);
	}
}

static void idle_bottom_freq_callback(struct cpuquiet_attribute *attr)
{
	schedule_work(&idle_stop_work);
}

static void enable_callback(struct cpuquiet_attribute *attr)
{
	int target_state = enable ? CPQ_ENABLED : CPQ_DISABLED;

	mutex_lock(&cpuquiet_lock);

	cpq_state = target_state;

	mutex_unlock(&cpuquiet_lock);

	schedule_work(&apply_constraints_work);

	schedule_work(&idle_stop_work);
}

static struct notifier_block cpu_online_notifier = {
	.notifier_call = cpu_online_notify,
};

static struct notifier_block min_cpus_notifier = {
	.notifier_call = min_cpus_notify,
};

static struct notifier_block max_cpus_notifier = {
	.notifier_call = max_cpus_notify,
};

static struct notifier_block cpufreq_notifier_block = {
	.notifier_call = cpufreq_notify,
};

CPQ_ATTRIBUTE(idle_bottom_freq, 0644, uint, idle_bottom_freq_callback);
CPQ_ATTRIBUTE(hotplug_timeout, 0644, ulong, delay_callback);
CPQ_ATTRIBUTE(enable, 0644, bool, enable_callback);

static struct attribute *auto_attributes[] = {
	&idle_bottom_freq_attr.attr,
	&enable_attr.attr,
	&hotplug_timeout_attr.attr,
	NULL,
};

static const struct sysfs_ops auto_sysfs_ops = {
	.show = cpuquiet_auto_sysfs_show,
	.store = cpuquiet_auto_sysfs_store,
};

static struct kobj_type ktype_sysfs = {
	.sysfs_ops = &auto_sysfs_ops,
	.default_attrs = auto_attributes,
};

static int auto_sysfs(void)
{
	int err;

	auto_sysfs_kobject = kzalloc(sizeof(*auto_sysfs_kobject),
					GFP_KERNEL);

	if (!auto_sysfs_kobject)
		return -ENOMEM;

	err = cpuquiet_kobject_init(auto_sysfs_kobject, &ktype_sysfs,
				"tegra_cpuquiet");

	if (err)
		kfree(auto_sysfs_kobject);

	return err;
}

static struct cpuquiet_driver cpuquiet_driver = {
	.name                   = "cpq-smp-hotplug",
	.quiesence_cpu          = quiesence_cpu,
	.wake_cpu               = wake_cpu,
};

#ifdef CONFIG_DEBUG_FS

static struct dentry *hp_debugfs_root;

static int hp_stats_show(struct seq_file *s, void *data)
{
	int i;
	u64 cur_jiffies = get_jiffies_64();

	mutex_lock(&cpq_lock_stats);

	for (i = 0; i < ARRAY_SIZE(hp_stats); i++) {
		bool was_up = (hp_stats[i].up_down_count & 0x1);
		__hp_stats_update(i, was_up);
	}

	mutex_unlock(&cpq_lock_stats);

	seq_printf(s, "%-15s ", "cpu:");
	for (i = 0; i < ARRAY_SIZE(hp_stats); i++)
		seq_printf(s, "%-9d ", i);

	seq_puts(s, "\n");

	seq_printf(s, "%-15s ", "transitions:");
	for (i = 0; i < ARRAY_SIZE(hp_stats); i++)
		seq_printf(s, "%-10u ", hp_stats[i].up_down_count);

	seq_puts(s, "\n");

	seq_printf(s, "%-15s ", "time plugged:");
	for (i = 0; i < ARRAY_SIZE(hp_stats); i++) {
		seq_printf(s, "%-10llu ",
			   cputime64_to_clock_t(hp_stats[i].time_up_total));
	}
	seq_puts(s, "\n");

	seq_printf(s, "%-15s %llu\n", "time-stamp:",
		   cputime64_to_clock_t(cur_jiffies));

	return 0;
}

static int hp_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, hp_stats_show, inode->i_private);
}


static const struct file_operations hp_stats_fops = {
	.open		= hp_stats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};


static int __init cpuquiet_debug_init(void)
{

	hp_debugfs_root = debugfs_create_dir("tegra_hotplug", NULL);
	if (!hp_debugfs_root)
		return -ENOMEM;

	if (!debugfs_create_file(
		"stats", S_IRUGO, hp_debugfs_root, NULL, &hp_stats_fops))
		goto err_out;

	return 0;

err_out:
	debugfs_remove_recursive(hp_debugfs_root);
	return -ENOMEM;
}

late_initcall(cpuquiet_debug_init);

#endif /* CONFIG_DEBUG_FS */

static int cpuquiet_init(void)
{
	int err;

	init_waitqueue_head(&wait_cpu);
	INIT_WORK(&idle_stop_work, &idle_stop_governor);
	INIT_WORK(&apply_constraints_work, &apply_constraints);

	hotplug_timeout = msecs_to_jiffies(HOTPLUG_DELAY_MS);

	cpq_state = INITIAL_STATE;
	enable = cpq_state == CPQ_DISABLED ? false : true;
	hp_init_stats();

	pr_info("Tegra cpuquiet initialized: %s\n",
		(cpq_state == CPQ_DISABLED) ? "disabled" : "enabled");

	if (pm_qos_add_notifier(PM_QOS_MIN_ONLINE_CPUS, &min_cpus_notifier))
		pr_err("%s: Failed to register min cpus PM QoS notifier\n",
			__func__);

	if (pm_qos_add_notifier(PM_QOS_MAX_ONLINE_CPUS, &max_cpus_notifier))
		pr_err("%s: Failed to register max cpus PM QoS notifier\n",
			__func__);

	register_hotcpu_notifier(&cpu_online_notifier);

	cpufreq_register_notifier(&cpufreq_notifier_block,
					CPUFREQ_TRANSITION_NOTIFIER);

	err = cpuquiet_register_driver(&cpuquiet_driver);
	if (err)
		return err;

	err = auto_sysfs();
	if (err)
		cpuquiet_unregister_driver(&cpuquiet_driver);

	return err;
}

static void cpuquiet_exit(void)
{
	cpuquiet_unregister_driver(&cpuquiet_driver);
	kobject_put(auto_sysfs_kobject);
#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(hp_debugfs_root);
#endif
}

MODULE_DESCRIPTION("generic cpuquiet driver for SMP systems with CPU hotplug");
MODULE_LICENSE("GPL");
module_init(cpuquiet_init);
module_exit(cpuquiet_exit);
