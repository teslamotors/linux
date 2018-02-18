/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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
 */

#include <linux/err.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/tachometer.h>
#include <linux/slab.h>
#include <linux/of.h>


static void tach_dev_release(struct device *dev);
int tachometer_read_rpm(struct tachometer_dev *tach);
int tachometer_set_winlen(struct tachometer_dev *tach, u8 win_len);

static ssize_t show_rpm(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tachometer_dev *tach = dev_get_drvdata(dev);
	int rpm = 0;

	rpm = tachometer_read_rpm(tach);

	return snprintf(buf, PAGE_SIZE, "%d\n", rpm);
}
static DEVICE_ATTR(show_rpm, S_IRUGO, show_rpm, NULL);

static ssize_t show_win_len(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tachometer_dev *tach = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", tach->win_len);
}

static ssize_t set_win_len(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct tachometer_dev *tach = dev_get_drvdata(dev);
	int err;
	u8 win_len;

	err = kstrtou8(buf, 10, &win_len);
	if (err)
		return -EINVAL;

	err = tachometer_set_winlen(tach, win_len);
	if (err)
		return -EINVAL;

	return count;
}
static DEVICE_ATTR(win_len, S_IRUGO | S_IWUSR, show_win_len, set_win_len);

static struct class tachometer_class = {
	.name		= "tachometer",
	.owner		= THIS_MODULE,
	.dev_release	= tach_dev_release,
};

void tachometer_unregister(struct tachometer_dev *tach_dev)
{
	kfree(tach_dev);
}
EXPORT_SYMBOL(tachometer_unregister);

static int dev_tachometer_match(struct device *dev, void *res, void *data)
{
	struct tachometer_dev **r = res;

	if (!r || !*r) {
		WARN_ON(!r || !*r);
		return 0;
	}
	return *r == data;
}

static void devm_tachometer_dev_release(struct device *dev, void *res)
{
	tachometer_unregister(*(struct tachometer_dev **)res);
}

void devm_tachometer_unregister(struct device *dev,
		struct tachometer_dev *tach_dev)
{
	int ret;

	ret = devres_release(dev, devm_tachometer_dev_release,
			dev_tachometer_match, tach_dev);
	if (ret != 0)
		WARN_ON(ret);
}
EXPORT_SYMBOL(devm_tachometer_unregister);

static struct tachometer_dev *of_tach_get(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct tachometer_dev *tach;

	tach = kzalloc(sizeof(*tach), GFP_KERNEL);
	if (IS_ERR(tach))
		return ERR_PTR(-ENOMEM);

	of_property_read_u32(np, "sampling-window", &tach->win_len);
	of_property_read_u32(np, "min-rps", &tach->min_rps);
	of_property_read_u32(np, "max-rps", &tach->max_rps);
	of_property_read_u32(np, "pulse-per-rev", &tach->pulse_per_rev);

	return tach;
}

struct tachometer_dev *tachometer_get(struct device *dev, const char *name)
{
	struct tachometer_dev *tach_dev;

	if (!dev || !dev->of_node) {
		pr_err("%s: Not enough info is provided\n", __func__);
		return ERR_PTR(-ENODEV);
	}

	tach_dev = of_tach_get(dev);

	return tach_dev;
}
EXPORT_SYMBOL(tachometer_get);

void tachometer_put(struct tachometer_dev *tach_dev)
{
	kfree(tach_dev);
}
EXPORT_SYMBOL(tachometer_put);

int tachometer_set_winlen(struct tachometer_dev *tach, u8 win_len)
{
	int ret = -EINVAL;

	if (!tach)
		return ret;

	mutex_lock(&tach->mutex);
	if (tach->desc->ops->set_winlen)
		ret = tach->desc->ops->set_winlen(tach, win_len);
	mutex_unlock(&tach->mutex);

	return ret;
}
EXPORT_SYMBOL(tachometer_set_winlen);

int tachometer_read_rpm(struct tachometer_dev *tach)
{
	int ret = -EINVAL;

	if (!tach)
		return ret;

	mutex_lock(&tach->mutex);
	if (tach->desc->ops->read_rpm)
		ret = tach->desc->ops->read_rpm(tach);
	mutex_unlock(&tach->mutex);

	return ret;
}
EXPORT_SYMBOL(tachometer_read_rpm);

static void devm_tachometer_release(struct device *dev, void *res)
{
	tachometer_put(*(struct tachometer_dev **)res);
}

struct tachometer_dev *devm_tachometer_get(struct device *dev,
		const char *name)
{
	struct tachometer_dev *tach_dev, **ptr;

	ptr = devres_alloc(devm_tachometer_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	tach_dev = tachometer_get(dev, name);
	if (!IS_ERR(tach_dev)) {
		*ptr = tach_dev;
		devres_add(dev, ptr);
	} else {
		devres_free(ptr);
	}

	return tach_dev;
}
EXPORT_SYMBOL(devm_tachometer_get);

struct tachometer_dev *tachometer_register(struct device *dev,
		const struct tachometer_desc *desc,
		struct tachometer_config *config)
{
	struct tachometer_dev *tach_dev;
	int ret;

	if (!desc || !config)
		return ERR_PTR(-EINVAL);

	WARN_ON(!desc->ops->read_rpm || !desc->ops->read_winlen);

	tach_dev = of_tach_get(dev);
	if (!tach_dev)
		return ERR_PTR(-ENOMEM);

	mutex_init(&tach_dev->mutex);

	/* register with sysfs */
	tach_dev->desc = desc;
	tach_dev->dev.class = &tachometer_class;
	tach_dev->dev.of_node = config->of_node;
	tach_dev->dev.parent = dev;
	dev_set_name(&tach_dev->dev, "%s", desc->name);

	ret = device_register(&tach_dev->dev);
	if (ret) {
		dev_info(&tach_dev->dev,
			"Tachometer driver %s registeration failed\n",
			desc->name);
		put_device(&tach_dev->dev);
		kfree(tach_dev);
		return ERR_PTR(ret);
	}

	dev_set_drvdata(&tach_dev->dev, tach_dev);
	device_create_file(&tach_dev->dev, &dev_attr_show_rpm);
	device_create_file(&tach_dev->dev, &dev_attr_win_len);
	dev_info(&tach_dev->dev, "Tachometer driver %s registered\n",
			desc->name);
	return tach_dev;
}

struct tachometer_dev *devm_tachometer_register(struct device *dev,
		const struct tachometer_desc *desc,
		struct tachometer_config *config)
{
	struct tachometer_dev *tach_dev, **ptr;

	ptr = devres_alloc(devm_tachometer_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	tach_dev = tachometer_register(dev, desc, config);
	if (!IS_ERR(tach_dev)) {
		*ptr = tach_dev;
		devres_add(dev, ptr);
	} else {
		devres_free(ptr);
	}

	return tach_dev;
}
EXPORT_SYMBOL(devm_tachometer_register);

static void tach_dev_release(struct device *dev)
{
	struct tachometer_dev *tach_dev = dev_get_drvdata(dev);

	kfree(tach_dev);
}

void tachometer_set_drvdata(struct tachometer_dev *tach_dev, void *drv_data)
{
	tach_dev->drv_data = drv_data;
}
EXPORT_SYMBOL(tachometer_set_drvdata);

void *tachometer_get_drvdata(struct tachometer_dev *tach_dev)
{
	return tach_dev->drv_data;
}
EXPORT_SYMBOL(tachometer_get_drvdata);

static int __init tachometer_init(void)
{
	int ret;

	ret = class_register(&tachometer_class);
	if (ret)
		pr_err("ERROR: tachometer class create failed: %d\n", ret);
	return ret;
}
core_initcall(tachometer_init);

MODULE_DESCRIPTION("Tachometer core driver");
MODULE_AUTHOR("R Raj Kumar <rrajk@nvidia.com>");
MODULE_LICENSE("GPL v2");
