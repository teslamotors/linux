/*
 * VBUS and ID based OTG cable extcon driver.
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 * Based on extcon-gpio driver by
 *	Copyright (C) 2008 Google, Inc.
 *	Author: Mike Lockwood <lockwood@android.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/extcon.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/iio/consumer.h>

static char const *extcon_otg_cable[] = {
	[0] = "USB",
	[1] = "USB-Host",
	NULL,
};

struct extcon_otg_platform_data {
	const char *name;
	int vbus_gpio;
	int id_gpio;
	unsigned long debounce;
	unsigned long irq_flags;
	int vbus_auto_hw;
	int vbus_presence_threshold;
};

struct extcon_otg_data {
	struct device *dev;
	int vbus_irq;
	int id_irq;
	struct extcon_dev *edev;
	struct delayed_work work;
	unsigned long debounce_jiffies;
	struct extcon_otg_platform_data *pdata;
	int last_cable_state;
	struct iio_channel *vbus_channel;
};

static void extcon_otg_work(struct work_struct *work)
{
	struct extcon_otg_data	*data =
		container_of(to_delayed_work(work), struct extcon_otg_data,
			     work);
	int new_cable_state = 0;
	int ret, vbus;
	int vbus_state = 0;
	int id_state = 0;

	if (!data->vbus_channel) {
		data->vbus_channel = iio_channel_get(data->dev, "vbus");
		if (IS_ERR(data->vbus_channel)) {
			ret = PTR_ERR(data->vbus_channel);
			dev_dbg(data->dev, "Failed to get vbus channel: %d\n",
				ret);
			data->vbus_channel = NULL;
			if (ret == -EPROBE_DEFER)
				schedule_delayed_work(&data->work,
					msecs_to_jiffies(1000));
			return;
		} else {
			dev_info(data->dev, "IIO channel get success\n");
		}
	}

	id_state = gpio_get_value_cansleep(data->pdata->id_gpio);
	id_state &= 0x1;
	if (!id_state) {
		dev_info(data->dev, "ID state ground = %d\n", id_state);
		new_cable_state = 2;
		goto done;
	}

	ret = iio_read_channel_processed(data->vbus_channel, &vbus);
	if (ret < 0) {
		dev_err(data->dev, "Not able to read VBUS IIO channel: %d\n",
			ret);
		goto done;
	}
	dev_info(data->dev, "vbus_voltage = %dmV , vbus_threshold = %dmV\n",
			vbus, data->pdata->vbus_presence_threshold);

	vbus_state = gpio_get_value_cansleep(data->pdata->vbus_gpio);
	vbus_state &= 0x1;
	if (!vbus_state)
		new_cable_state = 1;

done:
	dev_info(data->dev, "ID:VBUS: %d:%d, Cable state %d\n",
			id_state, vbus_state, new_cable_state);
	if (data->last_cable_state != new_cable_state)
		extcon_set_state(data->edev, new_cable_state);
	data->last_cable_state = new_cable_state;
}

static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
	struct extcon_otg_data *extcon_data = dev_id;

	if (extcon_data->debounce_jiffies)
		schedule_delayed_work(&extcon_data->work,
			extcon_data->debounce_jiffies);
	else
		extcon_otg_work(&extcon_data->work.work);

	return IRQ_HANDLED;
}

static struct extcon_otg_platform_data *of_get_platform_data(
		struct platform_device *pdev)
{
	struct extcon_otg_platform_data *pdata;
	struct device_node *np = pdev->dev.of_node;
	int gpio;
	u32 pval;
	int ret;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	of_property_read_string(np, "extcon-otg,name", &pdata->name);
	if (!pdata->name)
		pdata->name = np->name;

	gpio = of_get_named_gpio(np, "vbus-gpio", 0);
	if (gpio < 0)
		return ERR_PTR(gpio);
	pdata->vbus_gpio = gpio;

	gpio = of_get_named_gpio(np, "id-gpio", 0);
	if (gpio < 0)
		return ERR_PTR(gpio);
	pdata->id_gpio = gpio;

	ret = of_property_read_u32(np, "irq-flags", &pval);
	if (!ret)
		pdata->irq_flags = pval;
	else
		pdata->irq_flags = IRQF_TRIGGER_RISING |
						IRQF_TRIGGER_FALLING;

	ret = of_property_read_u32(np, "debounce", &pval);
	if (!ret)
		pdata->debounce = pval;

	pdata->vbus_auto_hw = of_property_read_bool(np, "vbus-auto-hw");

	ret = of_property_read_u32(np, "vbus-presence-threshold", &pval);
	if (!ret)
		pdata->vbus_presence_threshold = pval;

	return pdata;
}

static int extcon_otg_probe(struct platform_device *pdev)
{
	struct extcon_otg_platform_data *pdata = pdev->dev.platform_data;
	struct extcon_otg_data *extcon_data;
	int ret = 0;

	if (!pdata && pdev->dev.of_node) {
		pdata = of_get_platform_data(pdev);
		if (IS_ERR(pdata))
			return PTR_ERR(pdata);
	}

	if (!pdata)
		return -EBUSY;

	if (!pdata->irq_flags) {
		dev_err(&pdev->dev, "IRQ flag is not specified.\n");
		return -EINVAL;
	}

	extcon_data = devm_kzalloc(&pdev->dev, sizeof(struct extcon_otg_data),
				   GFP_KERNEL);
	if (!extcon_data)
		return -ENOMEM;

	extcon_data->dev = &pdev->dev;
	extcon_data->edev = devm_extcon_dev_allocate(&pdev->dev, extcon_otg_cable);
	if (IS_ERR(extcon_data->edev)) {
		dev_err(&pdev->dev, "failed to allocate extcon device\n");
		return -ENOMEM;
	}

	extcon_data->edev->name = pdata->name;
	extcon_data->debounce_jiffies = msecs_to_jiffies(pdata->debounce);
	extcon_data->pdata = pdata;
	extcon_data->last_cable_state = -1;

	ret = devm_extcon_dev_register(&pdev->dev, extcon_data->edev);
	if (ret < 0)
		return ret;

	extcon_data->vbus_channel = iio_channel_get(&pdev->dev, "vbus");
	if (IS_ERR(extcon_data->vbus_channel)) {
		ret = PTR_ERR(extcon_data->vbus_channel);
		dev_dbg(&pdev->dev, "Failed to get vbus channel: %d\n",
			ret);
		extcon_data->vbus_channel = NULL;
	} else {
		dev_info(extcon_data->dev, "IIO channel get success\n");
	}

	INIT_DELAYED_WORK(&extcon_data->work, extcon_otg_work);

	ret = devm_gpio_request_one(&pdev->dev, extcon_data->pdata->vbus_gpio,
			GPIOF_DIR_IN, "extcon-otg-vbus-gpio");
	if (ret < 0)
		goto free_iio;

	extcon_data->vbus_irq = gpio_to_irq(extcon_data->pdata->vbus_gpio);
	if (extcon_data->vbus_irq < 0) {
		ret = extcon_data->vbus_irq;
		goto free_iio;
	}

	ret = request_any_context_irq(extcon_data->vbus_irq, gpio_irq_handler,
			pdata->irq_flags, "extcon-otg-vbus-irq", extcon_data);
	if (ret < 0)
		goto free_iio;

	ret = devm_gpio_request_one(&pdev->dev, extcon_data->pdata->id_gpio,
			GPIOF_DIR_IN, "extcon-otg-id-gpio");
	if (ret < 0)
		goto free_vbus_irq;

	extcon_data->id_irq = gpio_to_irq(extcon_data->pdata->id_gpio);
	if (extcon_data->id_irq < 0) {
		ret = extcon_data->id_irq;
		goto free_vbus_irq;
	}

	ret = request_any_context_irq(extcon_data->id_irq, gpio_irq_handler,
			pdata->irq_flags, "extcon-otg-id-irq", extcon_data);
	if (ret < 0)
		goto free_vbus_irq;

	platform_set_drvdata(pdev, extcon_data);
	device_set_wakeup_capable(extcon_data->dev, true);

	/* Perform initial detection */
	if (!extcon_data->vbus_channel) {
		extcon_set_state(extcon_data->edev, 0);
		schedule_delayed_work(&extcon_data->work,
				msecs_to_jiffies(1000));
	} else {
		extcon_otg_work(&extcon_data->work.work);
	}

	return 0;

free_vbus_irq:
	free_irq(extcon_data->vbus_irq, extcon_data);
free_iio:
	iio_channel_release(extcon_data->vbus_channel);
	return ret;
}

static int extcon_otg_remove(struct platform_device *pdev)
{
	struct extcon_otg_data *extcon_data = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&extcon_data->work);
	free_irq(extcon_data->id_irq, extcon_data);
	free_irq(extcon_data->vbus_irq, extcon_data);
	iio_channel_release(extcon_data->vbus_channel);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int extcon_otg_suspend(struct device *dev)
{
	struct extcon_otg_data *extcon_data = dev_get_drvdata(dev);

	cancel_delayed_work_sync(&extcon_data->work);
	if (device_may_wakeup(extcon_data->dev)) {
		enable_irq_wake(extcon_data->vbus_irq);
		enable_irq_wake(extcon_data->id_irq);
	}

	return 0;
}

static int extcon_otg_resume(struct device *dev)
{
	struct extcon_otg_data *extcon_data = dev_get_drvdata(dev);

	if (device_may_wakeup(extcon_data->dev)) {
		disable_irq_wake(extcon_data->vbus_irq);
		disable_irq_wake(extcon_data->id_irq);
	}

	return 0;
}
#endif /* CONFIG_PM_SLEEP */

static SIMPLE_DEV_PM_OPS(extcon_otg_pm_ops, extcon_otg_suspend,
						extcon_otg_resume);

static struct of_device_id of_extcon_gpio_tbl[] = {
	{ .compatible = "extcon-otg", },
	{ /* end */ }
};
MODULE_DEVICE_TABLE(of, of_extcon_gpio_tbl);

static struct platform_driver extcon_otg_driver = {
	.probe		= extcon_otg_probe,
	.remove		= extcon_otg_remove,
	.driver		= {
		.name	= "extcon-otg",
		.owner	= THIS_MODULE,
		.of_match_table = of_extcon_gpio_tbl,
		.pm = &extcon_otg_pm_ops,
	},
};

static int __init gpio_extcon_otg_driver_init(void)
{
	return platform_driver_register(&extcon_otg_driver);
}
subsys_initcall_sync(gpio_extcon_otg_driver_init);

static void __exit gpio_extcon_otg_driver_exit(void)
{
	platform_driver_unregister(&extcon_otg_driver);
}
module_exit(gpio_extcon_otg_driver_exit);


MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_DESCRIPTION("GPIO/IIO channel based OTG extcon driver");
MODULE_LICENSE("GPL v2");
