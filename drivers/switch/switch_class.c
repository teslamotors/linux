/*
 *  drivers/switch/switch_class.c
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/switch.h>
#include <linux/suspend.h>

struct class *switch_class;
static atomic_t device_count;

static ssize_t state_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct switch_dev *sdev = (struct switch_dev *)
		dev_get_drvdata(dev);

	if (sdev->print_state) {
		int ret = sdev->print_state(sdev, buf);
		if (ret >= 0)
			return ret;
	}
	return sprintf(buf, "%d\n", sdev->state);
}

static ssize_t name_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct switch_dev *sdev = (struct switch_dev *)
		dev_get_drvdata(dev);

	if (sdev->print_name) {
		int ret = sdev->print_name(sdev, buf);
		if (ret >= 0)
			return ret;
	}
	return sprintf(buf, "%s\n", sdev->name);
}

static DEVICE_ATTR(state, S_IRUGO, state_show, NULL);
static DEVICE_ATTR(name, S_IRUGO, name_show, NULL);

static ssize_t uevent_in_suspend_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct switch_dev *sdev = dev_get_drvdata(dev);

	return sprintf(buf, "%c\n", sdev->uevent_in_suspend ? 'Y' : 'N');
}

static ssize_t uevent_in_suspend_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct switch_dev *sdev = dev_get_drvdata(dev);
	bool uevent_in_suspend;
	int ret;
	unsigned long flags;

	ret = strtobool(buf, &uevent_in_suspend);
	if (ret)
		return ret;

	spin_lock_irqsave(&sdev->lock, flags);
	sdev->uevent_in_suspend = uevent_in_suspend;
	spin_unlock_irqrestore(&sdev->lock, flags);

	return count;
}
static DEVICE_ATTR_RW(uevent_in_suspend);

void switch_set_state(struct switch_dev *sdev, int state)
{
	char name_buf[120];
	char state_buf[120];
	char *prop_buf;
	char *envp[3];
	int env_offset = 0;
	int length;
	unsigned long flags;

	/* Store a new state in the last_state_in_suspend while suspending.
	 * It will be handled after resume. */
	spin_lock_irqsave(&sdev->lock, flags);
	if (sdev->is_suspend && !sdev->uevent_in_suspend) {
		sdev->last_state_in_suspend = state;
		spin_unlock_irqrestore(&sdev->lock, flags);
		dev_info(sdev->dev,
			"%s: didn't send uevent (%d %d) due to suspending\n",
			__func__, state, sdev->state);
		return;
	}
	spin_unlock_irqrestore(&sdev->lock, flags);

	if (sdev->state != state) {
		sdev->state = state;

		prop_buf = (char *)get_zeroed_page(GFP_KERNEL);
		if (prop_buf) {
			length = name_show(sdev->dev, NULL, prop_buf);
			if (length > 0) {
				if (prop_buf[length - 1] == '\n')
					prop_buf[length - 1] = 0;
				snprintf(name_buf, sizeof(name_buf),
					"SWITCH_NAME=%s", prop_buf);
				envp[env_offset++] = name_buf;
			}
			length = state_show(sdev->dev, NULL, prop_buf);
			if (length > 0) {
				if (prop_buf[length - 1] == '\n')
					prop_buf[length - 1] = 0;
				snprintf(state_buf, sizeof(state_buf),
					"SWITCH_STATE=%s", prop_buf);
				envp[env_offset++] = state_buf;
			}
			envp[env_offset] = NULL;
			kobject_uevent_env(&sdev->dev->kobj, KOBJ_CHANGE, envp);
			free_page((unsigned long)prop_buf);
		} else {
			printk(KERN_ERR "out of memory in switch_set_state\n");
			kobject_uevent(&sdev->dev->kobj, KOBJ_CHANGE);
		}
	}
}
EXPORT_SYMBOL_GPL(switch_set_state);

static int create_switch_class(void)
{
	if (!switch_class) {
		switch_class = class_create(THIS_MODULE, "switch");
		if (IS_ERR(switch_class))
			return PTR_ERR(switch_class);
		atomic_set(&device_count, 0);
	}

	return 0;
}

static int switch_pm_notify(struct notifier_block *nb,
			    unsigned long event, void *data)
{
	struct switch_dev *sdev = container_of(nb, struct switch_dev, pm_nb);
	unsigned long flags;

	if (event == PM_SUSPEND_PREPARE) {
		spin_lock_irqsave(&sdev->lock, flags);
		sdev->is_suspend = true;
		if (!sdev->uevent_in_suspend)
			sdev->last_state_in_suspend = sdev->state;
		spin_unlock_irqrestore(&sdev->lock, flags);
	} else if (event == PM_POST_SUSPEND) {
		spin_lock_irqsave(&sdev->lock, flags);
		sdev->is_suspend = false;
		spin_unlock_irqrestore(&sdev->lock, flags);
		if (!sdev->uevent_in_suspend)
			switch_set_state(sdev, sdev->last_state_in_suspend);
	}

	return NOTIFY_OK;
}

static struct notifier_block switch_pm_nb = {
	.notifier_call = switch_pm_notify,
	.priority = -1,
};

int switch_dev_register(struct switch_dev *sdev)
{
	int ret;

	if (!switch_class) {
		ret = create_switch_class();
		if (ret < 0)
			return ret;
	}

	sdev->index = atomic_inc_return(&device_count);
	sdev->dev = device_create(switch_class, NULL,
		MKDEV(0, sdev->index), NULL, sdev->name);
	if (IS_ERR(sdev->dev))
		return PTR_ERR(sdev->dev);

	ret = device_create_file(sdev->dev, &dev_attr_state);
	if (ret < 0)
		goto err_create_file_1;
	ret = device_create_file(sdev->dev, &dev_attr_name);
	if (ret < 0)
		goto err_create_file_2;

	ret = device_create_file(sdev->dev, &dev_attr_uevent_in_suspend);
	if (ret < 0)
		goto err_create_file_3;

	dev_set_drvdata(sdev->dev, sdev);
	sdev->state = 0;

	spin_lock_init(&sdev->lock);
	sdev->uevent_in_suspend = true;
	sdev->is_suspend = false;
	sdev->pm_nb = switch_pm_nb;
	ret = register_pm_notifier(&sdev->pm_nb);
	if (ret < 0)
		goto err_register_pm_notifier;

	return 0;

err_register_pm_notifier:
	device_remove_file(sdev->dev, &dev_attr_uevent_in_suspend);
err_create_file_3:
	device_remove_file(sdev->dev, &dev_attr_name);
err_create_file_2:
	device_remove_file(sdev->dev, &dev_attr_state);
err_create_file_1:
	device_destroy(switch_class, MKDEV(0, sdev->index));
	printk(KERN_ERR "switch: Failed to register driver %s\n", sdev->name);

	return ret;
}
EXPORT_SYMBOL_GPL(switch_dev_register);

void switch_dev_unregister(struct switch_dev *sdev)
{
	unregister_pm_notifier(&sdev->pm_nb);
	device_remove_file(sdev->dev, &dev_attr_uevent_in_suspend);
	device_remove_file(sdev->dev, &dev_attr_name);
	device_remove_file(sdev->dev, &dev_attr_state);
	dev_set_drvdata(sdev->dev, NULL);
	device_destroy(switch_class, MKDEV(0, sdev->index));
}
EXPORT_SYMBOL_GPL(switch_dev_unregister);

static int __init switch_class_init(void)
{
	return create_switch_class();
}

static void __exit switch_class_exit(void)
{
	class_destroy(switch_class);
}

module_init(switch_class_init);
module_exit(switch_class_exit);

MODULE_AUTHOR("Mike Lockwood <lockwood@android.com>");
MODULE_DESCRIPTION("Switch class driver");
MODULE_LICENSE("GPL");
