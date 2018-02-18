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
 */

#include <linux/err.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

struct gps_wake_data {
	int gps_en;
	int gps_host_wake;
	int gps_host_wake_irq;
	struct class *gps_wake_class;
	struct device *gps_dev;
};

static irqreturn_t gps_hostwake_isr(int irq, void *dev_id)
{
	/* schedule a tasklet to handle the change in the host wake line */
	return IRQ_HANDLED;
}

static ssize_t gps_enable_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct gps_wake_data *gps_wake =
				(struct gps_wake_data *)dev_get_drvdata(dev);
	int state;

	state = gpio_get_value_cansleep(gps_wake->gps_en);

	return snprintf(buf, 1, "%d", state);
}

static ssize_t gps_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct gps_wake_data *gps_wake =
				(struct gps_wake_data *)dev_get_drvdata(dev);
	int state;

	if (kstrtoint(buf, 10, &state) < 0)
		return -EINVAL;

	gpio_set_value_cansleep(gps_wake->gps_en, state);

	return count;
}

static DEVICE_ATTR(gps_enable, S_IWUSR, gps_enable_show, gps_enable_store);

static int gps_wake_probe(struct platform_device *pdev)
{
	struct gps_wake_data *gps_wake;
	struct resource *res;
	struct device_node *node;
	int ret;

	gps_wake = devm_kmalloc(&pdev->dev, sizeof(*gps_wake), GFP_KERNEL);
	if (!gps_wake)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		node = pdev->dev.of_node;

		gps_wake->gps_en = of_get_named_gpio(node, "gps-en", 0);
		gps_wake->gps_host_wake = of_get_named_gpio(node,
						"gps-host-wake", 0);
		gps_wake->gps_host_wake_irq = platform_get_irq(pdev, 0);
	} else {
		res = platform_get_resource_byname(pdev, IORESOURCE_IO,
								"gps-en");
		if (res)
			gps_wake->gps_en = res->start;
		else
			gps_wake->gps_en = -1;

		res = platform_get_resource_byname(pdev, IORESOURCE_IO,
						"gps-host-wake");
		if (res)
			gps_wake->gps_host_wake = res->start;
		else
			gps_wake->gps_host_wake = -1;

		res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
							"gps-host-wake-irq");
		if (res)
			gps_wake->gps_host_wake_irq = res->start;
		else
			gps_wake->gps_host_wake_irq = -1;

	}

	if (gpio_is_valid(gps_wake->gps_en)) {
		/* Request gps_en gpio with output low as default direction */
		ret = devm_gpio_request_one(&pdev->dev, gps_wake->gps_en,
						GPIOF_OUT_INIT_LOW, "gps_en");
		if (ret) {
			pr_err("%s: Failed to request gps_en gpio, ret=%d\n",
								__func__, ret);
			goto free_res1;
		}

		gps_wake->gps_wake_class = class_create(THIS_MODULE,
								"gps_wake");
		if (IS_ERR(gps_wake->gps_wake_class)) {
			ret = PTR_ERR(gps_wake->gps_wake_class);
			pr_err("%s: Failed to create gps_wake class, ret=%d\n",
								__func__, ret);
			goto free_res1;
		}

		gps_wake->gps_dev = device_create(gps_wake->gps_wake_class,
					NULL, 0, gps_wake, "gps_device");
		if (IS_ERR(gps_wake->gps_dev)) {
			ret = PTR_ERR(gps_wake->gps_dev);
			pr_err("%s: Failed to create gps_device, ret=%d\n",
								__func__, ret);
			goto free_res2;
		}

		ret = device_create_file(gps_wake->gps_dev,
							&dev_attr_gps_enable);
		if (ret) {
			pr_err("%s: Failed to create device file, ret=%d\n",
								__func__, ret);
			goto free_res3;
		}
	} else {
		pr_debug("%s: gps_en is not a valid gpio\n", __func__);
	}

	if (gpio_is_valid(gps_wake->gps_host_wake)) {
		/* configure host_wake as input */
		ret = devm_gpio_request_one(&pdev->dev, gps_wake->gps_host_wake,
						GPIOF_IN, "gps_host_wake");
		if (ret) {
			pr_err("%s: request gps_host_wake gpio fail, ret=%d\n",
								__func__, ret);
			goto free_res4;
		}

		if (gps_wake->gps_host_wake_irq > -1) {
			pr_debug("%s: found gps_host_wake irq\n", __func__);
			ret = request_irq(gps_wake->gps_host_wake_irq,
				gps_hostwake_isr, IRQF_DISABLED |
				IRQF_TRIGGER_RISING, "gps hostwake", gps_wake);
			if (ret) {
				pr_err("%s: gps request_irq failed, ret=%d\n",
								__func__, ret);
				goto free_res4;
			}
			ret = device_init_wakeup(&pdev->dev, 1);
			if (ret) {
				pr_err("%s:device_init_wakeup failed, ret=%d\n",
								__func__, ret);
				goto free_res4;
			}
		} else {
			pr_debug("%s: not a valid gps irq\n", __func__);
		}

	} else {
		pr_debug("%s: gpio_host_wake is not a valid gpio\n", __func__);
	}

	dev_set_drvdata(&pdev->dev, gps_wake);
	pr_debug("driver successfully registered");

	return 0;

free_res4:
	if (gpio_is_valid(gps_wake->gps_en))
		device_remove_file(gps_wake->gps_dev, &dev_attr_gps_enable);
free_res3:
	if (gpio_is_valid(gps_wake->gps_en))
		device_destroy(gps_wake->gps_wake_class, 0);
free_res2:
	if (gpio_is_valid(gps_wake->gps_en))
		class_destroy(gps_wake->gps_wake_class);
free_res1:

	return -ENODEV;
}

static int gps_wake_remove(struct platform_device *pdev)
{
	struct gps_wake_data *gps_wake =
			(struct gps_wake_data *)dev_get_drvdata(&pdev->dev);

	if (gpio_is_valid(gps_wake->gps_en)) {
		device_remove_file(&pdev->dev, &dev_attr_gps_enable);
		device_destroy(gps_wake->gps_wake_class, 0);
		class_destroy(gps_wake->gps_wake_class);
	}
	if (gps_wake->gps_host_wake_irq > -1)
		free_irq(gps_wake->gps_host_wake_irq, NULL);

	return 0;
}

static int gps_wake_suspend(struct platform_device *pdev,
						pm_message_t state)
{
	struct gps_wake_data *gps_wake =
			(struct gps_wake_data *)dev_get_drvdata(&pdev->dev);

	if (gps_wake->gps_host_wake_irq > -1 && device_may_wakeup(&pdev->dev))
		enable_irq_wake(gps_wake->gps_host_wake_irq);
	return 0;
}

static int gps_wake_resume(struct platform_device *pdev)
{
	struct gps_wake_data *gps_wake =
			(struct gps_wake_data *)dev_get_drvdata(&pdev->dev);

	if (gps_wake->gps_host_wake_irq > -1 && device_may_wakeup(&pdev->dev))
		disable_irq_wake(gps_wake->gps_host_wake_irq);

	return 0;
}

static struct of_device_id gps_of_match[] = {
	{ .compatible = "nvidia,tegra-gps-wake", },
	{ },
};
MODULE_DEVICE_TABLE(of, gps_of_match);

static struct platform_driver gps_wake_driver = {
	.probe = gps_wake_probe,
	.remove = gps_wake_remove,
	.suspend = gps_wake_suspend,
	.resume = gps_wake_resume,
	.driver = {
		.name = "gps_wake",
		.of_match_table = of_match_ptr(gps_of_match),
		.owner = THIS_MODULE,
	},
};

module_platform_driver(gps_wake_driver);

MODULE_DESCRIPTION("GPS HOST WAKE");
MODULE_AUTHOR("NVIDIA");
MODULE_LICENSE("GPL v2");
