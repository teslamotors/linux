/*
 * NVIDIA Gamepad Reset Driver for NVIDIA Shield-2
 *
 * Copyright (c) 2014, NVIDIA Corporation. All Rights Reserved.
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
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#define DRIVER_AUTHOR "Ankita Garg, ankitag@nvidia.com"
#define DRIVER_DESC "NVIDIA Shield Gamepad Reset Driver"

#define RESET_DELAY	25

struct gamepad_info {
	int reset_gpio;
};

#ifndef CONFIG_OF
struct gamepad_rst_platform_data {
	int reset_gpio;
};
#endif

static ssize_t set_gamepad_reset(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	struct gamepad_info *data = dev_get_drvdata(dev);
	int gamepad_rst_gpio = data->reset_gpio;
	long unsigned int value;

	if (kstrtoul(buf, 10, &value))
		return count;

	if (!value)
		return count;

	ret = gpio_request(gamepad_rst_gpio, "GAMEPAD_RST");
	if (ret < 0) {
		dev_err(dev, "%s: gpio_request failed %d\n", __func__, ret);
		return ret;
	}

	ret = gpio_direction_output(gamepad_rst_gpio, 1);
	if (ret < 0) {
		gpio_free(gamepad_rst_gpio);
		dev_err(dev, "%s: gpio_direction_output failed %d\n",
			__func__, ret);
		return ret;
	}

	gpio_set_value(gamepad_rst_gpio, 0);
	udelay(RESET_DELAY);
	gpio_set_value(gamepad_rst_gpio, 1);
	gpio_free(gamepad_rst_gpio);

	dev_dbg(dev, "%s: done\n", __func__);

	return count;
}

static DEVICE_ATTR(gamepad_reset, S_IWUSR | S_IWGRP, NULL, set_gamepad_reset);

static int gamepad_reset_remove(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &dev_attr_gamepad_reset);
	return 0;
}

int gamepad_reset_gpio = -1;
void gamepad_reset_war(void)
{
	int ret;

	ret = gpio_request(gamepad_reset_gpio, "GAMEPAD_RST");
	if (ret < 0) {
		pr_err("%s: gpio_request failed %d\n", __func__, ret);
		return;
	}

	ret = gpio_direction_output(gamepad_reset_gpio, 1);
	if (ret < 0) {
		gpio_free(gamepad_reset_gpio);
		pr_err("%s: gpio_direction_output failed %d\n", __func__, ret);
		return;
	}

	pr_info("%s: xusb WAR - resetting gamepad\n", __func__);
	gpio_set_value(gamepad_reset_gpio, 0);
	udelay(RESET_DELAY);
	gpio_set_value(gamepad_reset_gpio, 1);
	gpio_free(gamepad_reset_gpio);
}
EXPORT_SYMBOL(gamepad_reset_war);

static int gamepad_reset_probe(struct platform_device *pdev)
{
	int ret;
	u32 value;
	struct device *dev = &pdev->dev;
	struct gamepad_info *data;
#ifdef CONFIG_OF
	struct device_node *node = pdev->dev.of_node;

	if (!node) {
		pr_err("gamepad_reset: dev of node NULL\n");
		return -EINVAL;
	}

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		pr_err("gamepad_reset: Failed to allocate memory\n");
		return -ENOMEM;
	}

	ret = of_property_read_u32(node, "reset_gpio", &value);
	if (ret) {
		pr_err("gamepad_reset: dev of_property_read failed\n");
		return -EIO;
	}
	data->reset_gpio = (int)value;
#else
	struct gamepad_rst_platform_data *pdata = pdev->dev.platform_data;

	if (!pdata)
		return -EINVAL;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		pr_err("gamepad_reset: Failed to allocate memory\n");
		return -ENOMEM;
	}
	data->reset_gpio = pdata->reset_gpio;
#endif

	platform_set_drvdata(pdev, data);

	ret = device_create_file(dev, &dev_attr_gamepad_reset);
	if (ret)
		dev_err(dev, "Failed to create gamepad reset file\n");

	dev_info(dev, "gamepad_reset: probed successfully\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id of_gamepad_reset_match[] = {
	{ .compatible = "loki-gamepad-reset", },
	{},
};
#endif

static struct platform_driver gamepad_reset_driver = {
	.probe = gamepad_reset_probe,
	.remove = gamepad_reset_remove,
	.driver = {
		.name = "gamepad_reset",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_gamepad_reset_match,
#endif
	}
};

static int __init gamepad_reset_init(void)
{
	int ret;

	ret = platform_driver_register(&gamepad_reset_driver);
	if (ret	< 0)
		pr_err("%s: Failed to register gamepad_reset driver\n",
			__func__);

	return ret;
}

static void __exit gamepad_reset_exit(void)
{
	platform_driver_unregister(&gamepad_reset_driver);
}

module_init(gamepad_reset_init);
module_exit(gamepad_reset_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
