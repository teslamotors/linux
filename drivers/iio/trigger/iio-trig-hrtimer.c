/*
 * Industrial I/O - hrtimer trigger support
 *
 * Copyright 2013 STMicroelectronics Inc.
 * Denis Ciocca <denis.ciocca@st.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/slab.h>
#include <linux/list.h>

#include <linux/iio/iio.h>
#include <linux/iio/trigger.h>

struct iio_hrtimer_trigger_data {
	struct iio_trigger *trig;
	struct hrtimer timer;
	struct list_head l;
	ktime_t period;
	u16  freq;
	int id;
};

static LIST_HEAD(iio_hrtimer_trigger_list);
static DEFINE_MUTEX(iio_hrtimer_trigger_list_mut);

static int iio_hrtimer_trigger_probe(int id);
static int iio_hrtimer_trigger_remove(int id);

static ssize_t iio_sysfs_hrtimer_trig_add(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int ret;
	unsigned int input;

	ret = kstrtouint(buf, 10, &input);
	if (ret)
		return ret;

	ret = iio_hrtimer_trigger_probe(input);
	if (ret)
		return ret;

	return len;
}
static DEVICE_ATTR(add_trigger, S_IWUSR, NULL, &iio_sysfs_hrtimer_trig_add);

static ssize_t iio_sysfs_hrtimer_trig_remove(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int ret;
	unsigned int input;

	ret = kstrtouint(buf, 10, &input);
	if (ret)
		return ret;

	ret = iio_hrtimer_trigger_remove(input);
	if (ret)
		return ret;

	return len;
}
static DEVICE_ATTR(remove_trigger, S_IWUSR,
					NULL, &iio_sysfs_hrtimer_trig_remove);

static struct attribute *iio_hrtimer_trig_attrs[] = {
	&dev_attr_add_trigger.attr,
	&dev_attr_remove_trigger.attr,
	NULL,
};

static const struct attribute_group iio_hrtimer_trig_group = {
	.attrs = iio_hrtimer_trig_attrs,
};

static const struct attribute_group *iio_hrtimer_trig_groups[] = {
	&iio_hrtimer_trig_group,
	NULL,
};

static struct device iio_hrtimer_trig_dev = {
	.bus = &iio_bus_type,
	.groups = iio_hrtimer_trig_groups,
};

static int iio_hrtimer_trig_set_state(struct iio_trigger *trig, bool state)
{
	struct iio_hrtimer_trigger_data *trig_data =
						iio_trigger_get_drvdata(trig);

	if (trig_data->freq == 0)
		return -EINVAL;

	if (state)
		hrtimer_start(&trig_data->timer,
					trig_data->period, HRTIMER_MODE_REL);
	else
		hrtimer_cancel(&trig_data->timer);

	return 0;
}

static ssize_t iio_hrtimer_trigger_set_freq_value(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int ret;
	u16 frequency;
	struct iio_trigger *trig = to_iio_trigger(dev);
	struct iio_hrtimer_trigger_data *trig_data =
						iio_trigger_get_drvdata(trig);

	ret = kstrtou16(buf, 10, &frequency);
	if (ret < 0)
		return ret;

	trig_data->freq = frequency;

	if (frequency)
		trig_data->period =
				ktime_set(0, NSEC_PER_SEC / trig_data->freq);

	return len;
}

static ssize_t iio_hrtimer_trigger_get_freq_value(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct iio_trigger *trig = to_iio_trigger(dev);
	struct iio_hrtimer_trigger_data *trig_data =
						iio_trigger_get_drvdata(trig);

	return sprintf(buf, "%hu\n", trig_data->freq);
}

static DEVICE_ATTR(frequency, S_IWUSR | S_IRUGO,
					iio_hrtimer_trigger_get_freq_value,
					iio_hrtimer_trigger_set_freq_value);

static ssize_t iio_hrtimer_trigger_get_id(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct iio_trigger *trig = to_iio_trigger(dev);
	struct iio_hrtimer_trigger_data *trig_data =
						iio_trigger_get_drvdata(trig);

	return sprintf(buf, "%d\n", trig_data->id);
}

static DEVICE_ATTR(id, S_IRUGO, iio_hrtimer_trigger_get_id, NULL);

static struct attribute *iio_hrtimer_trigger_attrs[] = {
	&dev_attr_frequency.attr,
	&dev_attr_id.attr,
	NULL,
};

static const struct attribute_group iio_hrtimer_trigger_attr_group = {
	.attrs = iio_hrtimer_trigger_attrs,
};

static const struct attribute_group *iio_hrtimer_trigger_attr_groups[] = {
	&iio_hrtimer_trigger_attr_group,
	NULL,
};

static const struct iio_trigger_ops iio_hrtimer_trigger_ops = {
	.owner = THIS_MODULE,
	.set_trigger_state = &iio_hrtimer_trig_set_state,
};

enum hrtimer_restart iio_hrtimer_trigger_func(struct hrtimer *timer)
{
	struct iio_hrtimer_trigger_data *trig_data;
	unsigned long flags;

	trig_data = container_of(timer, struct iio_hrtimer_trigger_data, timer);

	hrtimer_forward_now(timer, trig_data->period);

	local_irq_save(flags);
	iio_trigger_poll(trig_data->trig);
	local_irq_restore(flags);

	return HRTIMER_RESTART;
}

static int iio_hrtimer_trigger_probe(int id)
{
	int err;
	bool foundit = false;
	struct iio_hrtimer_trigger_data *trig_data;

	mutex_lock(&iio_hrtimer_trigger_list_mut);
	list_for_each_entry(trig_data, &iio_hrtimer_trigger_list, l) {
		if (id == trig_data->id) {
			foundit = true;
			break;
		}
	}
	if (foundit) {
		err = -EINVAL;
		goto iio_hrtimer_mutex_unlock;
	}

	trig_data = kmalloc(sizeof(*trig_data), GFP_KERNEL);
	if (trig_data == NULL) {
		err = -ENOMEM;
		goto iio_hrtimer_mutex_unlock;
	}

	trig_data->id = id;
	trig_data->trig = iio_trigger_alloc("hrtimer_trig%d", id);
	if (!trig_data->trig) {
		err = -ENOMEM;
		goto iio_hrtimer_free_trig_data;
	}

	trig_data->trig->dev.groups = iio_hrtimer_trigger_attr_groups;
	trig_data->trig->ops = &iio_hrtimer_trigger_ops;
	trig_data->trig->dev.parent = &iio_hrtimer_trig_dev;
	iio_trigger_set_drvdata(trig_data->trig, trig_data);

	trig_data->freq = 0;
	hrtimer_init(&trig_data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	trig_data->timer.function = &iio_hrtimer_trigger_func;

	err = iio_trigger_register(trig_data->trig);
	if (err)
		goto iio_hrtimer_free_trig_data;

	list_add(&trig_data->l, &iio_hrtimer_trigger_list);
	__module_get(THIS_MODULE);
	mutex_unlock(&iio_hrtimer_trigger_list_mut);

	return 0;

iio_hrtimer_free_trig_data:
	kfree(trig_data);
iio_hrtimer_mutex_unlock:
	mutex_unlock(&iio_hrtimer_trigger_list_mut);
	return err;
}

static int iio_hrtimer_trigger_remove(int id)
{
	bool foundit = false;
	struct iio_hrtimer_trigger_data *trig_data;

	mutex_lock(&iio_hrtimer_trigger_list_mut);
	list_for_each_entry(trig_data, &iio_hrtimer_trigger_list, l) {
		if (id == trig_data->id) {
			foundit = true;
			break;
		}
	}
	if (!foundit) {
		mutex_unlock(&iio_hrtimer_trigger_list_mut);
		return -EINVAL;
	}

	iio_trigger_unregister(trig_data->trig);
	iio_trigger_free(trig_data->trig);

	list_del(&trig_data->l);
	kfree(trig_data);
	module_put(THIS_MODULE);
	mutex_unlock(&iio_hrtimer_trigger_list_mut);

	return 0;
}

static int __init iio_hrtimer_trig_init(void)
{
	device_initialize(&iio_hrtimer_trig_dev);
	dev_set_name(&iio_hrtimer_trig_dev, "iio_hrtimer_trigger");
	return device_add(&iio_hrtimer_trig_dev);
}
module_init(iio_hrtimer_trig_init);

static void __exit iio_hrtimer_trig_exit(void)
{
	device_unregister(&iio_hrtimer_trig_dev);
}
module_exit(iio_hrtimer_trig_exit);

MODULE_AUTHOR("Denis Ciocca <denis.ciocca@st.com>");
MODULE_DESCRIPTION("Hrtimer trigger for the iio subsystem");
MODULE_LICENSE("GPL v2");
