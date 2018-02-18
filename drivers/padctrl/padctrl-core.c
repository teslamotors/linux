/*
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
 */

#include <linux/module.h>
#include <linux/list.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/padctrl/padctrl.h>

static DEFINE_MUTEX(padctrl_dev_list_mutex);
static LIST_HEAD(padctrl_dev_list);

static atomic_t padctrl_no = ATOMIC_INIT(0);

struct padctrl_dev {
	struct device dev;
	struct list_head list;
	struct list_head consumer_list;
	struct padctrl_desc *desc;
	struct padctrl_config config;
	struct mutex mutex;
	void *drv_data;
};

struct padctrl {
	struct list_head list;
	struct padctrl_dev *pad_dev;
	int index;
};

static void padctrl_dev_release(struct device *dev)
{
	struct padctrl_dev *pdev = dev_get_drvdata(dev);

	kfree(pdev);
}

static struct class padctrl_class = {
	.name = "padctrl",
	.dev_release = padctrl_dev_release,
};

static struct padctrl *of_pad_get(struct device *dev, struct device_node *np,
				  const char *name)
{
	struct of_phandle_args npspec;
	struct padctrl *pad;
	struct padctrl_dev *pad_dev;
	int index = 0;
	int cindex = -1;
	int ret;

	if (name)
		index = of_property_match_string(np, "pad-names", name);

	ret = of_parse_phandle_with_args(np, "pad-controllers",
			"#padcontroller-cells", index, &npspec);

	if (ret < 0) {
		dev_err(dev, "Not found padcontroller handles: %d\n", ret);
		return ERR_PTR(ret);
	}

	list_for_each_entry(pad_dev, &padctrl_dev_list, list) {
		if (pad_dev->dev.of_node == npspec.np) {
			cindex = npspec.args_count ? npspec.args[0] : 0;
			goto out;
		}
	}
	return ERR_PTR(-ENODEV);

out:
	pad = kzalloc(sizeof(*pad), GFP_KERNEL);
	if (!pad)
		return ERR_PTR(-ENOMEM);

	pad->pad_dev = pad_dev;
	pad->index = cindex;
	INIT_LIST_HEAD(&pad->list);
	mutex_lock(&pad_dev->mutex);
	list_add(&pad->list, &pad_dev->consumer_list);
	mutex_unlock(&pad_dev->mutex);
	return pad;
}

struct padctrl *padctrl_get(struct device *dev, const char *name)
{
	struct padctrl *pad;

	if (!dev || !dev->of_node) {
		pr_err("%s: not enough information provided\n", __func__);
		return ERR_PTR(-ENODEV);
	}

	mutex_lock(&padctrl_dev_list_mutex);
	pad = of_pad_get(dev, dev->of_node, name);
	mutex_unlock(&padctrl_dev_list_mutex);
	return pad;
}
EXPORT_SYMBOL(padctrl_get);

void padctrl_put(struct padctrl *pctrl)
{
	if (pctrl) {
		mutex_lock(&pctrl->pad_dev->mutex);
		list_del(&pctrl->list);
		mutex_unlock(&pctrl->pad_dev->mutex);
		kfree(pctrl);
	}
}
EXPORT_SYMBOL(padctrl_put);

static void devm_padctrl_release(struct device *dev, void *res)
{
	padctrl_put(*(struct padctrl **)res);
}

struct padctrl *devm_padctrl_get(struct device *dev, const char *name)
{
	struct padctrl **ptr, *padctrl;

	ptr = devres_alloc(devm_padctrl_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	padctrl = padctrl_get(dev, name);
	if (!IS_ERR(padctrl)) {
		*ptr = padctrl;
		devres_add(dev, ptr);
	} else {
		devres_free(ptr);
	}

	return padctrl;
}
EXPORT_SYMBOL(devm_padctrl_get);

struct padctrl *devm_padctrl_get_from_node(struct device *dev,
					   struct device_node *np,
					   const char *name)
{
	struct padctrl **ptr, *padctrl;

	if (!dev || !np) {
		pr_err("%s: not enough information provided\n", __func__);
		return ERR_PTR(-ENODEV);
	}

	ptr = devres_alloc(devm_padctrl_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	mutex_lock(&padctrl_dev_list_mutex);
	padctrl = of_pad_get(dev, np, name);
	mutex_unlock(&padctrl_dev_list_mutex);

	if (IS_ERR(padctrl)) {
		devres_free(ptr);
		return padctrl;
	}

	*ptr = padctrl;
	devres_add(dev, ptr);

	return padctrl;
}
EXPORT_SYMBOL(devm_padctrl_get_from_node);

int padctrl_set_voltage(struct padctrl *pad, u32 voltage)
{
	struct padctrl_dev *pad_dev;
	int ret = -EINVAL;


	if (!pad || !pad->pad_dev)
		return -EINVAL;

	pad_dev = pad->pad_dev;
	mutex_lock(&pad_dev->mutex);
	if (pad_dev->desc->ops->set_voltage)
		ret = pad_dev->desc->ops->set_voltage(pad->pad_dev,
				pad->index, voltage);
	mutex_unlock(&pad_dev->mutex);

	return ret;
}
EXPORT_SYMBOL(padctrl_set_voltage);

int padctrl_get_voltage(struct padctrl *pad, u32 *voltage)
{
	struct padctrl_dev *pad_dev;
	int ret = -EINVAL;

	if (!pad || !pad->pad_dev)
		return -EINVAL;

	pad_dev = pad->pad_dev;
	mutex_lock(&pad_dev->mutex);
	if (pad_dev->desc->ops->get_voltage)
		ret = pad_dev->desc->ops->get_voltage(pad->pad_dev,
				pad->index, voltage);
	mutex_unlock(&pad_dev->mutex);

	return ret;
}
EXPORT_SYMBOL(padctrl_get_voltage);

int padctrl_power_enable(struct padctrl *pad)
{
	struct padctrl_dev *pad_dev;
	int ret = -EINVAL;

	if (!pad || !pad->pad_dev)
		return -EINVAL;

	pad_dev = pad->pad_dev;
	mutex_lock(&pad_dev->mutex);
	if (pad_dev->desc->ops->power_enable)
		ret = pad_dev->desc->ops->power_enable(pad->pad_dev,
				pad->index);
	mutex_unlock(&pad_dev->mutex);

	return ret;
}
EXPORT_SYMBOL(padctrl_power_enable);

int padctrl_power_disable(struct padctrl *pad)
{
	struct padctrl_dev *pad_dev;
	int ret = -EINVAL;

	if (!pad || !pad->pad_dev)
		return -EINVAL;

	pad_dev = pad->pad_dev;
	mutex_lock(&pad_dev->mutex);
	if (pad_dev->desc->ops->power_disable)
		ret = pad_dev->desc->ops->power_disable(pad->pad_dev,
				pad->index);
	mutex_unlock(&pad_dev->mutex);

	return ret;
}
EXPORT_SYMBOL(padctrl_power_disable);

struct padctrl_dev *padctrl_register(struct device *dev,
	struct padctrl_desc *desc, struct padctrl_config *config)
{
	struct padctrl_dev *pad_dev;
	int ret;

	if (!desc || !config)
		return ERR_PTR(-EINVAL);

	WARN_ON(!desc->ops->set_voltage || !desc->ops->get_voltage);

	pad_dev = kzalloc(sizeof(*pad_dev), GFP_KERNEL);
	if (!pad_dev)
		return ERR_PTR(-ENOMEM);

	mutex_init(&pad_dev->mutex);
	INIT_LIST_HEAD(&pad_dev->consumer_list);
	INIT_LIST_HEAD(&pad_dev->list);
	pad_dev->desc = desc;

	/* register with sysfs */
	pad_dev->dev.class = &padctrl_class;
	pad_dev->dev.of_node = config->of_node;
	pad_dev->dev.parent = dev;
	dev_set_name(&pad_dev->dev, "padctrl.%d",
			atomic_inc_return(&padctrl_no) - 1);
	ret = device_register(&pad_dev->dev);
	if (ret) {
		put_device(&pad_dev->dev);
		goto clean;
	}
	dev_set_drvdata(&pad_dev->dev, pad_dev);

	mutex_lock(&padctrl_dev_list_mutex);
	list_add(&pad_dev->list, &padctrl_dev_list);
	mutex_unlock(&padctrl_dev_list_mutex);
	dev_info(&pad_dev->dev, "Pad control driver %s registered\n",
			desc->name);
	return pad_dev;

clean:
	kfree(pad_dev);
	return ERR_PTR(ret);
}

void padctrl_unregister(struct padctrl_dev *pad_dev)
{
	if (pad_dev) {
		mutex_lock(&padctrl_dev_list_mutex);
		list_del(&pad_dev->list);
		mutex_unlock(&padctrl_dev_list_mutex);
		kfree(pad_dev);
	}
}
EXPORT_SYMBOL(padctrl_unregister);

void padctrl_set_drvdata(struct padctrl_dev *pad_dev, void *drv_data)
{
	pad_dev->drv_data = drv_data;
}
EXPORT_SYMBOL(padctrl_set_drvdata);

void *padctrl_get_drvdata(struct padctrl_dev *pad_dev)
{
	return pad_dev->drv_data;
}
EXPORT_SYMBOL(padctrl_get_drvdata);

static void devm_padctrl_dev_release(struct device *dev, void *res)
{
	padctrl_unregister(*(struct padctrl_dev **)res);
}

struct padctrl_dev *devm_padctrl_register(struct device *dev,
		struct padctrl_desc *desc, struct padctrl_config *config)
{
	struct padctrl_dev **ptr, *pad_dev;

	ptr = devres_alloc(devm_padctrl_dev_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	pad_dev = padctrl_register(dev, desc, config);
	if (!IS_ERR(pad_dev)) {
		*ptr = pad_dev;
		devres_add(dev, ptr);
	} else {
		devres_free(ptr);
	}
	return pad_dev;
}
EXPORT_SYMBOL(devm_padctrl_register);

static int devm_padctrl_match(struct device *dev, void *res, void *data)
{
	struct padctrl_dev **r = res;

	if (!r || !*r) {
		WARN_ON(!r || !*r);
		return 0;
	}
	return *r == data;
}

void devm_padctrl_unregister(struct device *dev, struct padctrl_dev *pad_dev)
{
	int ret;

	ret = devres_release(dev, devm_padctrl_dev_release,
					devm_padctrl_match, pad_dev);
	if (ret != 0)
		WARN_ON(ret);
}
EXPORT_SYMBOL(devm_padctrl_unregister);

static int __init padctrl_init(void)
{
	int ret;

	ret = class_register(&padctrl_class);
	if (ret)
		pr_err("ERROR: Padcontrol class create failed: %d\n", ret);
	return ret;
}
core_initcall(padctrl_init);
