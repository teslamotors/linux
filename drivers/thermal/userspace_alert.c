/*
 * drivers/thermal/userspace_alert.c
 *
 * Userspace thermal alert cooling device.
 *
 * Copyright (C) 2016 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>

/* Define cooling device alert state */
#define NO_ALERT	0
#define ALERT		1

/* Maximum throttle states supported */
#define CDEV_MAX_STATE	ALERT

struct usr_alrt_strct {
	unsigned long cur_state;
	struct thermal_cooling_device *cdev;
	uint32_t therm_alert;
	uint32_t alert_timeout_ms;
	wait_queue_head_t alert_wait_queue;
	struct mutex alert_timeout_lock;
	struct mutex alert_lock;
	bool syfs_created;
	bool syfs_symlink_created;
};

static int userspace_alert_send_uevent(struct thermal_cooling_device *tcd)
{
	char trip[25];
	char *envp[] = { trip, NULL };
	int ret = 0;

	snprintf(trip, sizeof(trip), "USERSPACE_ALERT=%s", "TRIPPED");

	ret = kobject_uevent_env(&tcd->device.kobj, KOBJ_CHANGE, envp);
	if (ret < 0)
		pr_err("userspace_alert_send_uevent: kobj_uevent error!\n");

	return ret;
}

/* Get maximum throttle state supported by cooling device. */
static int userspace_alert_cdev_max_state(struct thermal_cooling_device *tcd,
				   unsigned long *state)
{
	struct usr_alrt_strct *alert_data = tcd->devdata;

	if (!alert_data)
		return -EINVAL;

	mutex_lock(&alert_data->alert_lock);
	*state = CDEV_MAX_STATE;
	mutex_unlock(&alert_data->alert_lock);
	return 0;
}

/* Get current throttle state of the cooling device. */
static int userspace_alert_cdev_cur_state(struct thermal_cooling_device *tcd,
				   unsigned long *state)
{
	struct usr_alrt_strct *alert_data = tcd->devdata;

	if (!alert_data)
		return -EINVAL;

	mutex_lock(&alert_data->alert_lock);
	*state = alert_data->cur_state;
	mutex_unlock(&alert_data->alert_lock);

	return 0;
}

/* Set current throttle state of the cooling device. */
static int userspace_alert_cdev_set_state(struct thermal_cooling_device *tcd,
				   unsigned long state)
{
	struct usr_alrt_strct *alert_data = tcd->devdata;

	if (!alert_data)
		return -EINVAL;

	if (state == alert_data->cur_state)
		return 0;

	mutex_lock(&alert_data->alert_lock);
	alert_data->cur_state = state;

	if (state > 0) {
		/* Send 'uevent' notification to userspace */
		userspace_alert_send_uevent(tcd);
		alert_data->therm_alert = ALERT;

		/*
		 * Write barrier for other processes to see the therm_alert
		 * updated value.
		 */
		smp_wmb();

		/* Wakeup the processes waiting for the alert to occur */
		if (waitqueue_active(&alert_data->alert_wait_queue))
			wake_up_all(&alert_data->alert_wait_queue);

		pr_debug("thermal alert: USERSPACE_ALERT_TRIPPED!\n");
	} else {
		alert_data->therm_alert = NO_ALERT;
		/*
		 * Write barrier for other processes to see the therm_alert
		 * updated value.
		 */
		smp_wmb();
	}
	mutex_unlock(&alert_data->alert_lock);

	return 0;
}

/*
 * Blocks the read call until an alert occurs or the specified duration
 *  times out. When alert occurs or if alert is preset, then read return
 *  with value 1
 */
static ssize_t thermal_alert_block_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	uint32_t alert_timeout_ms;
	struct usr_alrt_strct *alert_data = dev_get_drvdata(dev);

	if (!alert_data)
		return -EINVAL;

	alert_timeout_ms = alert_data->alert_timeout_ms;
	if (alert_timeout_ms > 0)
		wait_event_interruptible_timeout(alert_data->alert_wait_queue,
			alert_data->therm_alert == ALERT,
			msecs_to_jiffies(alert_timeout_ms));
	else
		wait_event_interruptible(
				alert_data->alert_wait_queue,
				alert_data->therm_alert == ALERT);

	return sprintf(buf, "%d\n", alert_data->therm_alert);
}

static ssize_t thermal_alert_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct usr_alrt_strct *alert_data = dev_get_drvdata(dev);

	if (!alert_data)
		return -EINVAL;

	return sprintf(buf, "%d\n", alert_data->therm_alert);
}

/*
 * Set the time out duration(in ms) to wait when alert is not preset.
 * When alert occurs or if alert is already preset, this value has
 * no significance
 */
static ssize_t thermal_alert_block_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	long val;
	struct usr_alrt_strct *alert_data = dev_get_drvdata(dev);

	if (!alert_data)
		return -EINVAL;

	if (kstrtol(buf, 0, &val))
		return -EINVAL;

	mutex_lock(&alert_data->alert_timeout_lock);
	alert_data->alert_timeout_ms = val;
	mutex_unlock(&alert_data->alert_timeout_lock);

	return count;
}

/*
 * Cooling device operations.
 */
static struct thermal_cooling_device_ops userspace_alert_cdev_ops = {
	.get_max_state = userspace_alert_cdev_max_state,
	.get_cur_state = userspace_alert_cdev_cur_state,
	.set_cur_state = userspace_alert_cdev_set_state,
};

static DEVICE_ATTR(thermal_alert_block, S_IRUGO | S_IWUSR,
			thermal_alert_block_show, thermal_alert_block_store);
static DEVICE_ATTR(thermal_alert, S_IRUGO, thermal_alert_show, NULL);

static const struct attribute *userspace_cdev_attr[] = {
	&dev_attr_thermal_alert_block.attr,
	&dev_attr_thermal_alert.attr,
	NULL,
};

static int userspace_alert_therm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const char *str;
	char *cdev_type;
	struct usr_alrt_strct *alert_data;
	struct device_node *np = dev->of_node;

	alert_data = devm_kzalloc(dev, sizeof(struct usr_alrt_strct),
					GFP_KERNEL);
	if (!alert_data)
		return -ENOMEM;

	if (of_property_read_string(np, "cdev-type", &str) == 0) {
		cdev_type = (char *)str;
	} else {
		dev_err(dev, "missing cdev-type\n");
		return -EINVAL;
	}

	alert_data->cur_state = NO_ALERT;
	alert_data->therm_alert = 0;
	alert_data->alert_timeout_ms = 0;
	alert_data->syfs_created = 0;
	alert_data->syfs_symlink_created = 0;
	mutex_init(&alert_data->alert_lock);
	mutex_init(&alert_data->alert_timeout_lock);
	init_waitqueue_head(&alert_data->alert_wait_queue);
	dev_set_drvdata(dev, alert_data);

	alert_data->cdev = thermal_cooling_device_register(cdev_type,
			alert_data, &userspace_alert_cdev_ops);

	if (IS_ERR(alert_data->cdev))
		return PTR_ERR(alert_data->cdev);

	if (sysfs_create_files(&dev->kobj, userspace_cdev_attr) < 0)
		pr_warn("\n userspace alert cdev: failed to create sysfs files\n");
	else
		alert_data->syfs_created = 1;


	if (sysfs_create_link(&alert_data->cdev->device.kobj, &dev->kobj,
				"userspace_alert") < 0)
		pr_warn("\n userspace alert cdev: failed to create sysfs symlink\n");
	else
		alert_data->syfs_symlink_created = 1;

	pr_info("%s cooling device registered.\n", cdev_type);
	return 0;
}

static int  userspace_alert_therm_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct usr_alrt_strct *alert_data = dev_get_drvdata(dev);
	struct thermal_cooling_device *cdev;

	if (alert_data) {
		cdev = alert_data->cdev;
		if ((cdev != NULL) && (alert_data->syfs_symlink_created))
			sysfs_remove_link(&cdev->device.kobj,
						"userspace_alert");
		if (alert_data->syfs_created)
			sysfs_remove_files(&dev->kobj, userspace_cdev_attr);
		mutex_destroy(&alert_data->alert_lock);
		mutex_destroy(&alert_data->alert_timeout_lock);
		thermal_cooling_device_unregister(alert_data->cdev);
	}
	return 0;
}

static const struct of_device_id userspace_therm_of_match[] = {
	{ .compatible = "userspace-therm-alert", },
	{ },
};

static struct platform_driver userspace_therm_driver = {
	.driver = {
		.name = "userspace-therm-alert",
		.owner = THIS_MODULE,
		.of_match_table = userspace_therm_of_match,
	},
	.probe = userspace_alert_therm_probe,
	.remove = userspace_alert_therm_remove,
};

module_platform_driver(userspace_therm_driver);

MODULE_DESCRIPTION("Userspace thermal alert cooling device module");
MODULE_AUTHOR("NVIDIA");
MODULE_LICENSE("GPL v2");
