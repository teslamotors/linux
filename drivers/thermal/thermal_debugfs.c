/*
 * Copyright (c) 2015, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/err.h>
#include <linux/thermal.h>

static DEFINE_MUTEX(thermal_debug_lock);

static struct dentry *thermal_debugfs_root;

int debug_tz_match(struct thermal_zone_device *tz, void *data)
{
	long tz_id = (long)data;
	return tz->id == tz_id;
}

static ssize_t set_trip_write(struct file *file,
	const char __user *userbuf, size_t count, loff_t *ppos)
{
	char buf[40];
	long tz_id, trip, temperature;
	struct thermal_zone_device *tz;
	int ret = -ENODEV;

	if (sizeof(buf) <= count)
		return -EINVAL;

	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = '\0';
	strim(buf);

	if (sscanf(buf, "%ld_%ld_%ld", &tz_id, &trip, &temperature) != 3)
		return -EINVAL;

	mutex_lock(&thermal_debug_lock);

	tz = thermal_zone_device_find((void *)tz_id, debug_tz_match);

	if (tz) {
		ret = -EPERM;
		if (tz->ops->set_trip_temp)
			ret = tz->ops->set_trip_temp(tz, trip, temperature);
	}
	mutex_unlock(&thermal_debug_lock);
	return ret ? : count;
}

static const struct file_operations set_trip_fops = {
	.write		= set_trip_write,
};

static int __init thermal_debugfs_init(void)
{
	struct dentry *d;

	d = debugfs_create_dir("thermal", NULL);
	if (!d)
		return -ENOMEM;

	thermal_debugfs_root = d;

	d = debugfs_create_file("tz_trip_temperature", S_IWUSR,
				thermal_debugfs_root, NULL, &set_trip_fops);
	if (!d)
		goto err_out;

	return 0;
err_out:
	debugfs_remove_recursive(thermal_debugfs_root);
	thermal_debugfs_root = NULL;
	return -ENOMEM;
}
late_initcall(thermal_debugfs_init);
