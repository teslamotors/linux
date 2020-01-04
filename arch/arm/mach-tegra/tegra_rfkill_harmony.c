/*
 * arch/arm/mach-tegra/tegra_rfkill_harmony.c
 *
 * RF kill device using NVIDIA Tegra Harmony platform
 *
 * Copyright (c) 2009, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/rfkill.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#include "gpio-names.h"

#define DRIVER_NAME    "tegra_rfkill"
#define DRIVER_DESC    "Nvidia Tegra rfkill"

#define HARMONY_BT_RST	TEGRA_GPIO_PU0

static struct rfkill *bt = NULL;

static int bluetooth_set_power(void *data, bool blocked)
{
	int status = 0;
	if (blocked) {
		status = gpio_direction_output(HARMONY_BT_RST, 0);
		if (status < 0) {
			printk(KERN_INFO "BT_RST gpio output set fail\n");
			goto end;
		}

		printk(KERN_INFO "Bluetooth: power OFF\n");
	} else {
		/* Murata bt chip reset sequence */
		status = gpio_direction_output(HARMONY_BT_RST, 0);
		if (status < 0) {
			printk(KERN_INFO "BT_RST gpio output set fail\n");
			goto end;
		}

		mdelay(5);

		status = gpio_direction_output(HARMONY_BT_RST, 1);
		if (status < 0) {
			printk(KERN_INFO "BT_RST gpio output set fail\n");
			goto end;
		}

		printk(KERN_INFO "Bluetooth: power ON\n");
	}

end:
	return status;
}

static const struct rfkill_ops bt_rfkill_ops = {
	.set_block = bluetooth_set_power,
};

static int tegra_rfkill_probe(struct platform_device *pdev)
{
	int status = 0;

	status = gpio_request(HARMONY_BT_RST, "bt_rst");
	if (status < 0) {
		printk(KERN_INFO "BT_RST gpio request fail\n");
		return status;
	}

	tegra_gpio_enable(HARMONY_BT_RST);
	gpio_export(HARMONY_BT_RST, 0);

	bt = rfkill_alloc("bt-rfkill", &pdev->dev, RFKILL_TYPE_BLUETOOTH,
			  &bt_rfkill_ops, NULL);
	if (!bt) {
		status = -ENOMEM;
		goto fail;
	}

	rfkill_init_sw_state(bt, true);

	status = rfkill_register(bt);
	if (status)
		goto fail;

	return status;

fail:
	gpio_free(HARMONY_BT_RST);

	if (bt)
		rfkill_destroy(bt);

	return status;
}

static int tegra_rfkill_remove(struct platform_device *pdev)
{
	rfkill_set_sw_state(bt, true);
	bluetooth_set_power(NULL, true);

	gpio_free(HARMONY_BT_RST);

	if (bt) {
		rfkill_unregister(bt);
		rfkill_destroy(bt);
	}

	return 0;
}

static struct platform_driver tegra_rfkill_driver = {
	.probe = tegra_rfkill_probe,
	.remove = tegra_rfkill_remove,
	.driver = {
		   .name = DRIVER_NAME,
		   .owner = THIS_MODULE,
		   },
};

static int __init tegra_rfkill_init(void)
{
	return platform_driver_register(&tegra_rfkill_driver);
}

static void __exit tegra_rfkill_exit(void)
{
	return platform_driver_unregister(&tegra_rfkill_driver);
}

module_init(tegra_rfkill_init);
module_exit(tegra_rfkill_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
