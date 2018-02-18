/*
 * SoftDog-platform: A platform based Software Watchdog Device
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 * Based on the softdog by:
 *	(c) Copyright 1996 Alan Cox <alan@lxorguk.ukuu.org.uk>,
 *	All Rights Reserved.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/watchdog.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#define TIMER_MARGIN	60		/* Default is 60 seconds */
struct softdog_platform_wdt {
	struct watchdog_device wdt_dev;
	struct device *dev;
	unsigned int soft_margin;
	bool nowayout;
	int soft_noboot;
	int soft_panic;
	struct timer_list watchdog_ticktock;
	struct notifier_block nb;
	int is_stopped;
};

/* If the timer expires..  */
static void softdog_platform_watchdog_fire(unsigned long data)
{
	struct softdog_platform_wdt *swdt = (struct softdog_platform_wdt *)data;

	if (swdt->is_stopped)
		return;

	if (swdt->soft_noboot)
		dev_crit(swdt->dev, "Triggered - Reboot ignored\n");
	else if (swdt->soft_panic) {
		dev_crit(swdt->dev, "Initiating panic\n");
		panic("Software Watchdog Timer expired");
	} else {
		dev_crit(swdt->dev, "Initiating system reboot\n");
		emergency_restart();
		dev_crit(swdt->dev, "Reboot didn't ?????\n");
	}
}

/* Softdog operations */
static int softdog_platform_ping(struct watchdog_device *wdt)
{
	struct softdog_platform_wdt *swdt =  watchdog_get_drvdata(wdt);

	if (swdt->is_stopped)
		return 0;

	mod_timer(&swdt->watchdog_ticktock, jiffies+(wdt->timeout*HZ));
	return 0;
}

static int softdog_platform_start(struct watchdog_device *wdt)
{
	struct softdog_platform_wdt *swdt =  watchdog_get_drvdata(wdt);

	swdt->is_stopped = false;
	mod_timer(&swdt->watchdog_ticktock, jiffies+(wdt->timeout*HZ));
	return 0;
}

static int softdog_platform_stop(struct watchdog_device *wdt)
{
	struct softdog_platform_wdt *swdt =  watchdog_get_drvdata(wdt);

	swdt->is_stopped = true;
	return 0;
}

static int softdog_platform_set_timeout(struct watchdog_device *wdt,
	unsigned int t)
{
	wdt->timeout = t;
	return 0;
}

/* Notifier for system down */
static int softdog_platform_notify_sys(struct notifier_block *this,
	unsigned long code, void *ptr)
{
	struct softdog_platform_wdt *swdt = container_of(this,
					struct softdog_platform_wdt, nb);

	if (code == SYS_DOWN || code == SYS_HALT)
		/* Turn the WDT off */
		softdog_platform_stop(&swdt->wdt_dev);

	return NOTIFY_DONE;
}

static struct watchdog_info softdog_platform_info = {
	.identity = "Software Watchdog",
	.options = WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE,
};

static struct watchdog_ops softdog_platform_ops = {
	.owner = THIS_MODULE,
	.start = softdog_platform_start,
	.stop = softdog_platform_stop,
	.ping = softdog_platform_ping,
	.set_timeout = softdog_platform_set_timeout,
};

static int softdog_platform_probe(struct platform_device *pdev)
{
	struct softdog_platform_wdt *swdt;
	struct device_node *np = pdev->dev.of_node;
	u32 pval;
	int ret;

	if (!np) {
		dev_err(&pdev->dev, "Only DT registration supported\n");
		return -ENODEV;
	}

	swdt = devm_kzalloc(&pdev->dev, sizeof(*swdt), GFP_KERNEL);
	if (!swdt)
		return -ENOMEM;

	swdt->nowayout = of_property_read_bool(np, "watchdog,nowayout");
	swdt->soft_noboot = !of_property_read_bool(np, "watchdog,reboot");
	swdt->soft_panic = of_property_read_bool(np, "watchdog,panic");
	ret = of_property_read_u32(np, "watchdog,margin", &pval);
	if (!ret)
		swdt->soft_margin = pval;
	else
		swdt->soft_margin = TIMER_MARGIN;
	/*
	 * Check that the soft_margin value is within it's range;
	 * if not reset to the default
	 */
	if (swdt->soft_margin < 1 || swdt->soft_margin > 65535) {
		dev_err(&pdev->dev,
			"soft_margin must be 0 to 65536, using %d\n",
			TIMER_MARGIN);
		return -EINVAL;
	}

	swdt->wdt_dev.timeout = swdt->soft_margin;
	swdt->nb.notifier_call = softdog_platform_notify_sys;
	swdt->dev = &pdev->dev;
	watchdog_set_nowayout(&swdt->wdt_dev, swdt->nowayout);
	watchdog_set_drvdata(&swdt->wdt_dev, swdt);
	platform_set_drvdata(pdev, swdt);

	swdt->wdt_dev.info = &softdog_platform_info;
	swdt->wdt_dev.ops = &softdog_platform_ops;
	swdt->wdt_dev.min_timeout = 1;
	swdt->wdt_dev.max_timeout = 0xFFFF;

	setup_timer(&swdt->watchdog_ticktock, softdog_platform_watchdog_fire,
				(unsigned long)swdt);

	ret = register_reboot_notifier(&swdt->nb);
	if (ret) {
		dev_err(&pdev->dev,
			"cannot register reboot notifier (err=%d)\n", ret);
		goto timer_del;
	}

	ret = watchdog_register_device(&swdt->wdt_dev);
	if (ret) {
		dev_err(&pdev->dev, "Watchdog register failed: %d\n", ret);
		goto reboot_unreg;
	}

	dev_info(&pdev->dev, "Software Watchdog Timer: initialized\n");
	return 0;

reboot_unreg:
	unregister_reboot_notifier(&swdt->nb);
timer_del:
	del_timer_sync(&swdt->watchdog_ticktock);
	return ret;
}

static int softdog_platform_remove(struct platform_device *pdev)
{
	struct softdog_platform_wdt *swdt = platform_get_drvdata(pdev);

	del_timer_sync(&swdt->watchdog_ticktock);
	watchdog_unregister_device(&swdt->wdt_dev);
	unregister_reboot_notifier(&swdt->nb);
	return 0;
}

static void softdog_platform_shutdown(struct platform_device *pdev)
{
	struct softdog_platform_wdt *swdt = platform_get_drvdata(pdev);

	del_timer_sync(&swdt->watchdog_ticktock);
}

#ifdef CONFIG_PM_SLEEP
static int softdog_platform_suspend(struct device *dev)
{
	struct softdog_platform_wdt *swdt = dev_get_drvdata(dev);
	int ret;

	ret = softdog_platform_stop(&swdt->wdt_dev);
	if (ret < 0)
		dev_err(swdt->dev, "wdt stop failed: %d\n", ret);
	return 0;
}

static int softdog_platform_resume(struct device *dev)
{
	struct softdog_platform_wdt *swdt = dev_get_drvdata(dev);
	int ret;

	ret = softdog_platform_start(&swdt->wdt_dev);
	if (ret < 0)
		dev_err(swdt->dev, "wdt start failed: %d\n", ret);
	return 0;
}
#endif

static const struct dev_pm_ops softdog_platform_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(softdog_platform_suspend,
			softdog_platform_resume)
};

static struct of_device_id softdog_platform_of_match[] = {
	{ .compatible = "softdog-platform", },
	{},
};
MODULE_DEVICE_TABLE(of, softdog_platform_of_match);

static struct platform_driver softdog_platform_driver = {
	.driver = {
		.name = "softdog-platform",
		.owner = THIS_MODULE,
		.of_match_table = softdog_platform_of_match,
		.pm = &softdog_platform_pm_ops,
	},
	.probe = softdog_platform_probe,
	.remove = softdog_platform_remove,
	.shutdown = softdog_platform_shutdown,
};

module_platform_driver(softdog_platform_driver);

MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_DESCRIPTION("Software Watchdog Platform Device Driver");
MODULE_LICENSE("GPL v2");
