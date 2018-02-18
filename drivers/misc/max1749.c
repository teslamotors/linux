/*
 * drivers/misc/max1749.c
 *
 * Driver for MAX1749, vibrator motor driver.
 *
 * Copyright (c) 2011-2013 NVIDIA Corporation, All Rights Reserved.
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

#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>

#include <linux/slab.h>

#include "../staging/android/timed_output.h"

struct vibrator_data {
	struct device *dev;
	struct timed_output_dev to_dev;
	struct hrtimer timer;
	struct regulator *regulator;
	struct work_struct work;
	bool vibrator_on;
};

static void vibrator_start(struct vibrator_data *vdata)
{
	if (!vdata->vibrator_on) {
		int ret;

		ret = regulator_enable(vdata->regulator);
		if (ret < 0)
			dev_err(vdata->dev, "Regulator can not enabled: %d\n",
				ret);
		vdata->vibrator_on = true;
	}
}

static void vibrator_stop(struct vibrator_data *vdata)
{
	int ret;

	if (vdata->vibrator_on) {
		ret = regulator_is_enabled(vdata->regulator);
		if (ret > 0) {
			regulator_disable(vdata->regulator);
			vdata->vibrator_on = false;
		}
	}
}

static void vibrator_work_func(struct work_struct *work)
{
	struct vibrator_data *vdata = container_of(work, struct vibrator_data,
					work);
	vibrator_stop(vdata);
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	struct vibrator_data *vdata = container_of(timer, struct vibrator_data,
					timer);
	schedule_work(&vdata->work);
	return HRTIMER_NORESTART;
}

/*
 * Timeout value can be changed from sysfs entry
 * created by timed_output_dev.
 * echo 100 > /sys/class/timed_output/vibrator/enable
 */
static void vibrator_enable(struct timed_output_dev *to_dev, int value)
{
	struct vibrator_data *vdata = container_of(to_dev, struct vibrator_data,
						to_dev);

	hrtimer_cancel(&vdata->timer);
	if (value > 0) {
		vibrator_start(vdata);
		hrtimer_start(&vdata->timer,
			  ktime_set(value / 1000, (value % 1000) * 1000000),
			  HRTIMER_MODE_REL);
	} else {
		vibrator_stop(vdata);
	}
	return;
}

static int vibrator_get_time(struct timed_output_dev *to_dev)
{
	struct vibrator_data *vdata = container_of(to_dev, struct vibrator_data,
						to_dev);

	if (hrtimer_active(&vdata->timer)) {
		ktime_t r = hrtimer_get_remaining(&vdata->timer);
		struct timeval t = ktime_to_timeval(r);
		return t.tv_sec * 1000 + t.tv_usec / 1000;
	}

	return 0;
}

static struct timed_output_dev vibrator_dev = {
	.name		= "vibrator",
	.get_time	= vibrator_get_time,
	.enable		= vibrator_enable,
};

static int vibrator_probe(struct platform_device *pdev)
{
	struct vibrator_data *vdata;
	struct device *dev = &pdev->dev;
	int ret;

	vdata = devm_kzalloc(&pdev->dev, sizeof(*vdata), GFP_KERNEL);
	if (!vdata)
		return -ENOMEM;

	vdata->regulator = devm_regulator_get(&pdev->dev, "vdd-vbrtr");
	if (IS_ERR(vdata->regulator)) {
		ret = PTR_ERR(vdata->regulator);
		dev_err(dev, "Couldn't get regulator vdd-vbrtr: %d\n", ret);
		return ret;
	}

	vdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, vdata);

	hrtimer_init(&vdata->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	/* Intialize the work queue */
	INIT_WORK(&vdata->work, vibrator_work_func);
	vdata->timer.function = vibrator_timer_func;
	vdata->to_dev = vibrator_dev;
	vdata->vibrator_on = false;
	ret = timed_output_dev_register(&vdata->to_dev);
	if (ret < 0) {
		dev_err(dev, "Timeout device registration failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static int vibrator_remove(struct platform_device *pdev)
{
	struct vibrator_data *vdata = platform_get_drvdata(pdev);

	timed_output_dev_unregister(&vdata->to_dev);
	return 0;
}

static struct of_device_id of_vibrator_match_tbl[] = {
	{ .compatible = "nvidia,vibrator", },
	{ /* end */ }
};
MODULE_DEVICE_TABLE(of, of_vibrator_match_tbl);

static struct platform_driver vibrator_driver = {
	.probe = vibrator_probe,
	.remove = vibrator_remove,
	.driver = {
		.name = "tegra-vibrator",
		.owner = THIS_MODULE,
		.of_match_table =  of_vibrator_match_tbl,
	},
};

module_platform_driver(vibrator_driver);

MODULE_DESCRIPTION("timed output vibrator device");
MODULE_AUTHOR("GPL");
