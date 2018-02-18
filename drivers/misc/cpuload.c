/*
 * drivers/misc/cpuload.c
 *
 * Copyright (c) 2012-2016, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/tick.h>
#include <asm/cputime.h>

static unsigned int enabled;
/* Consider IO as busy */
static unsigned long io_is_busy;

static inline cputime64_t get_cpu_iowait_time(
	unsigned int cpu, cputime64_t *wall)
{
	u64 iowait_time = get_cpu_iowait_time_us(cpu, wall);

	if (iowait_time == -1ULL)
		return 0;

	return iowait_time;
}

#define DECL_CPULOAD_ATTR(name) \
static ssize_t show_##name(struct kobject *kobj, \
	struct attribute *attr, char *buf) \
{ \
	return sprintf(buf, "%lu\n", name); \
} \
\
static ssize_t store_##name(struct kobject *kobj,\
		struct attribute *attr, const char *buf, size_t count) \
{ \
	int ret; \
	unsigned long val; \
\
	ret = kstrtoul(buf, 0, &val); \
	if (ret < 0) \
		return ret; \
	name = val; \
	return count; \
} \
\
static struct global_attr name##_attr = __ATTR(name, 0644, \
		show_##name, store_##name);

static ssize_t show_cpus_online(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	unsigned int i, t;
	const cpumask_t *cpus = cpu_online_mask;

	i = 0;
	for_each_cpu_mask(t, *cpus)
		i++;

	return sprintf(buf, "%u\n", i);
}

static struct global_attr cpus_online_attr = __ATTR(cpus_online, 0444,
		show_cpus_online, NULL);

static ssize_t show_cpu_usage(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	unsigned int t, len, total = 0;
	const cpumask_t *cpus = cpu_online_mask;

	for_each_cpu_mask(t, *cpus) {
		/* XXX: Remove the hardcoded 0 once readers are updated */
		len = sprintf(buf, "%u %u %llu %llu %llu\n",
			      t, 0,
			      ktime_to_us(ktime_get()),
			      get_cpu_idle_time_us(t, NULL),
			      get_cpu_iowait_time_us(t, NULL));
		total += len;
		buf = &buf[len];
	}

	return total;
}

static struct global_attr cpu_usage_attr = __ATTR(cpu_usage, 0444,
		show_cpu_usage, NULL);

static ssize_t show_enable(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", enabled);
}

/* XXX: Remove the now dummy enable variable once readers are updated */
static ssize_t store_enable(struct kobject *kobj,
		struct attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned long val;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;
	enabled = !!val;	/* normalize user input */

	return count;
}
static struct global_attr enable_attr = __ATTR(enable, 0644,
		show_enable, store_enable);

DECL_CPULOAD_ATTR(io_is_busy)

static struct attribute *cpuload_attributes[] = {
	&io_is_busy_attr.attr,
	&cpus_online_attr.attr,
	&cpu_usage_attr.attr,
	&enable_attr.attr,
	NULL,
};

static struct attribute_group cpuload_attr_group = {
	.attrs = cpuload_attributes,
	.name = "cpuload",
};

static int __init cpuload_monitor_init(void)
{
	int rc;
	rc = cpufreq_get_global_kobject();
	if (rc)
		return rc;

	/* XXX: Move this node into a different syfs path */
	rc = sysfs_create_group(cpufreq_global_kobject,
			&cpuload_attr_group);
	return rc;
}

module_init(cpuload_monitor_init);

static void __exit cpuload_monitor_exit(void)
{
	sysfs_remove_group(cpufreq_global_kobject,
			&cpuload_attr_group);
	cpufreq_put_global_kobject();
}

module_exit(cpuload_monitor_exit);

MODULE_AUTHOR("Ilan Aelion <iaelion@nvidia.com>");
MODULE_DESCRIPTION("'cpuload_monitor' - A cpu load monitor");
MODULE_LICENSE("GPL");
