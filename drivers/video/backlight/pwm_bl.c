/*
 * linux/drivers/video/backlight/pwm_bl.c
 *
 * Copyright (c) 2013-2015, NVIDIA CORPORATION, All rights reserved.
 *
 * simple PWM based backlight control, board code has to setup
 * 1) pin configuration so PWM waveforms can output
 * 2) platform_data being correctly configured
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/gpio/consumer.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/pwm.h>
#include <linux/pwm_backlight.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include "../../../arch/arm/mach-tegra/board.h"

static void pwm_backlight_power_on(struct pwm_bl_data *pb, int brightness)
{
	int err;

	if (pb->enabled)
		return;

	if (pb->power_supply) {
		err = regulator_enable(pb->power_supply);
		if (err < 0)
			dev_err(pb->dev, "failed to enable power supply\n");
	}

	if (pb->enable_gpio)
		gpiod_set_value(pb->enable_gpio, 1);

	pwm_enable(pb->pwm);
	pb->enabled = true;
}

static void pwm_backlight_power_off(struct pwm_bl_data *pb)
{
	if (!pb->enabled)
		return;

	pwm_config(pb->pwm, 0, pb->period);
	pwm_disable(pb->pwm);

	if (pb->enable_gpio)
		gpiod_set_value(pb->enable_gpio, 0);

	if (pb->power_supply)
		regulator_disable(pb->power_supply);
	pb->enabled = false;
}

static int compute_duty_cycle(struct pwm_bl_data *pb, int brightness)
{
	unsigned int lth = pb->lth_brightness;
	int duty_cycle;

	if (pb->levels)
		duty_cycle = pb->levels[brightness];
	else
		duty_cycle = brightness;

	return (duty_cycle * (pb->period - lth) / pb->scale) + lth;
}

static int pwm_backlight_update_status(struct backlight_device *bl)
{
	struct pwm_bl_data *pb = bl_get_data(bl);
	int brightness = bl->props.brightness;
	int duty_cycle;

	if (bl->props.power != FB_BLANK_UNBLANK ||
	    bl->props.fb_blank != FB_BLANK_UNBLANK ||
	    bl->props.state & BL_CORE_FBBLANK)
		brightness = 0;

	if (pb->notify)
		brightness = pb->notify(pb->dev, brightness);

	if (brightness > 0) {
		duty_cycle = compute_duty_cycle(pb, brightness);
		pwm_config(pb->pwm, duty_cycle, pb->period);
		pwm_backlight_power_on(pb, brightness);
	} else
		pwm_backlight_power_off(pb);

	if (pb->notify_after)
		pb->notify_after(pb->dev, brightness);

	return 0;
}

static int pwm_backlight_check_fb(struct backlight_device *bl,
				  struct fb_info *info)
{
	struct pwm_bl_data *pb = bl_get_data(bl);

	return !pb->check_fb || pb->check_fb(pb->dev, info);
}

static const struct backlight_ops pwm_backlight_ops = {
	.update_status	= pwm_backlight_update_status,
	.check_fb	= pwm_backlight_check_fb,
};

static int pwm_backlight_notify(struct device *dev, int brightness)
{
	struct backlight_device *bl = dev_get_drvdata(dev);
	struct backlight_device_brightness_info bl_info;

	bl_info.dev = dev;
	bl_info.brightness = brightness;

	return backlight_device_notifier_call_chain(bl,
			BACKLIGHT_DEVICE_PRE_BRIGHTNESS_CHANGE,
			(void *)&bl_info);
}

static void pwm_backlight_notify_after(struct device *dev, int brightness)
{
	struct backlight_device *bl = dev_get_drvdata(dev);
	struct backlight_device_brightness_info bl_info;

	bl_info.dev = dev;
	bl_info.brightness = brightness;

	backlight_device_notifier_call_chain(bl,
			BACKLIGHT_DEVICE_POST_BRIGHTNESS_CHANGE,
			(void *)&bl_info);
}

#ifdef CONFIG_OF
static int pwm_backlight_parse_dt(struct device *dev,
				  struct platform_pwm_backlight_data *data,
				  const char *blnode_compatible,
				  struct device_node **target_bl_node)
{
	struct device_node *node = dev->of_node;
	struct device_node *bl_node = NULL;
	struct device_node *compat_node = NULL;
	struct property *prop;
	const __be32 *p;
	u32 u;
	int length;
	u32 value;
	int ret = 0;
	int n_bl_measured = 0;

	if (!node)
		return -ENODEV;

	/* If there's compat_node which is contained in
	 * backlight parent node, that means, there are
	 * multi pwm-bl device nodes and right one is
	 * chosen, with blnode_compatible */
	if (blnode_compatible)
		compat_node = of_find_compatible_node(node, NULL,
			blnode_compatible);

	if (!blnode_compatible || !compat_node)
		bl_node = node;
	else
		bl_node = compat_node;

	*target_bl_node = bl_node;

	/* determine the number of brightness levels */
	prop = of_find_property(bl_node, "brightness-levels", &length);
	if (!prop) {
		/* if brightness levels array is not defined,
		 * parse max brightness and default brightness,
		 * directly.
		 */
		ret = of_property_read_u32(bl_node, "max-brightness",
					   &value);
		if (ret < 0) {
			pr_info("fail to parse max-brightness\n");
			goto fail_parse_dt;
		}

		data->max_brightness = value;

#if defined(CONFIG_ANDROID) && defined(CONFIG_TEGRA_COMMON)
		if (get_androidboot_mode_charger())
			ret = of_property_read_u32(bl_node,
						   "default-charge-brightness",
						   &value);
		else
#endif
		ret = of_property_read_u32(bl_node, "default-brightness",
					   &value);
		if (ret < 0) {
			pr_info("fail to parse default-brightness\n");
			goto fail_parse_dt;
		}

		data->dft_brightness = value;
	} else {
		size_t size = 0;
		int item_counts;
		item_counts = length / sizeof(u32);
		if (item_counts > 0)
			size = sizeof(*data->levels) * item_counts;

		data->levels = devm_kzalloc(dev, size, GFP_KERNEL);
		if (!data->levels) {
			ret = -ENOMEM;
			goto fail_parse_dt;
		}

		ret = of_property_read_u32_array(bl_node,
						 "brightness-levels",
						 data->levels,
						 item_counts);
		if (ret < 0) {
			pr_info("fail to parse brightness-levels\n");
			goto fail_parse_dt;
		}

		/*
		 * default-brightness-level: the default brightness level
		 * (index into the array defined by the "brightness-levels"
		 * property)
		 */
		ret = of_property_read_u32(bl_node,
					   "default-brightness-level",
					   &value);
		if (ret < 0) {
			pr_info("fail to parse default-brightness-level\n");
			goto fail_parse_dt;
		}

		data->dft_brightness = data->levels[value];
		data->max_brightness = data->levels[item_counts - 1];
	}

	data->enable_gpio = -EINVAL;

	value = 0;
	ret = of_property_read_u32(bl_node, "lth-brightness",
		&value);
	data->lth_brightness = (unsigned int)value;

	data->pwm_gpio = of_get_named_gpio(bl_node, "pwm-gpio", 0);

	of_property_for_each_u32(bl_node, "bl-measured", prop, p, u)
		n_bl_measured++;
	if (n_bl_measured > 0) {
		data->bl_measured = devm_kzalloc(dev,
			sizeof(*data->bl_measured) * n_bl_measured, GFP_KERNEL);
		if (!data->bl_measured) {
			pr_err("bl_measured memory allocation failed\n");
			ret = -ENOMEM;
			goto fail_parse_dt;
		}
		n_bl_measured = 0;
		of_property_for_each_u32(bl_node,
			"bl-measured", prop, p, u)
			data->bl_measured[n_bl_measured++] = u;
	}
	of_node_put(compat_node);
	return 0;

fail_parse_dt:
	of_node_put(compat_node);
	return ret;
}

static struct of_device_id pwm_backlight_of_match[] = {
	{ .compatible = "pwm-backlight" },
	{ }
};

MODULE_DEVICE_TABLE(of, pwm_backlight_of_match);
#else
static int pwm_backlight_parse_dt(struct device *dev,
				  struct platform_pwm_backlight_data *data,
				  const char *blnode_compatible,
				  struct device_node **target_bl_node)
{
	return -ENODEV;
}
#endif

static int pwm_backlight_probe(struct platform_device *pdev)
{
	struct platform_pwm_backlight_data *data = dev_get_platdata(&pdev->dev);
	struct device_node *np = pdev->dev.of_node;
	struct platform_pwm_backlight_data defdata;
	struct backlight_properties props;
	struct backlight_device *bl;
	struct pwm_bl_data *pb;
	struct device_node *target_bl_node = NULL;
	int ret;
	const char *blnode_compatible = NULL;

	if (!np && !pdev->dev.platform_data) {
		dev_err(&pdev->dev, "no platform data for pwm_bl\n");
		return -ENOENT;
	}

	if (np) {
		struct pwm_bl_data_dt_ops *pops;
		pops = (struct pwm_bl_data_dt_ops *)platform_get_drvdata(pdev);
		memset(&defdata, 0, sizeof(defdata));
		if (pops) {
			defdata.init = pops->init;
			defdata.notify = pops->notify;
			defdata.notify_after = pops->notify_after;
			defdata.check_fb = pops->check_fb;
			defdata.exit = pops->exit;
			blnode_compatible = pops->blnode_compatible;
		} else {
			defdata.notify = pwm_backlight_notify;
			defdata.notify_after = pwm_backlight_notify_after;
		}

		ret = pwm_backlight_parse_dt(&pdev->dev, &defdata,
			blnode_compatible, &target_bl_node);
		if (ret < 0) {
			dev_err(&pdev->dev, "fail to find platform data\n");
			return ret;
		}
		data = &defdata;

		/* initialize dev drv data */
		platform_set_drvdata(pdev, NULL);
	}

	if (data->init) {
		ret = data->init(&pdev->dev);
		if (ret < 0)
			return ret;
	}

	pb = devm_kzalloc(&pdev->dev, sizeof(*pb), GFP_KERNEL);
	if (!pb) {
		ret = -ENOMEM;
		goto err_alloc;
	}

	if (data->levels) {
		unsigned int i;

		for (i = 0; i <= data->max_brightness; i++)
			if (data->levels[i] > pb->scale)
				pb->scale = data->levels[i];

		pb->levels = data->levels;
	} else
		pb->scale = data->max_brightness;

	pb->notify = data->notify;
	pb->notify_after = data->notify_after;
	pb->bl_measured = data->bl_measured;
	pb->check_fb = data->check_fb;
	pb->exit = data->exit;
	pb->dev = &pdev->dev;
	pb->pwm_gpio = data->pwm_gpio;
	pb->enabled = false;

	pb->enable_gpio = devm_gpiod_get_optional(&pdev->dev, "enable");
	if (IS_ERR(pb->enable_gpio)) {
		ret = PTR_ERR(pb->enable_gpio);
		goto err_alloc;
	}

	/*
	 * Compatibility fallback for drivers still using the integer GPIO
	 * platform data. Must go away soon.
	 */
	if (!pb->enable_gpio && gpio_is_valid(data->enable_gpio)) {
		ret = devm_gpio_request_one(&pdev->dev, data->enable_gpio,
					    GPIOF_OUT_INIT_HIGH, "enable");
		if (ret < 0) {
			dev_err(&pdev->dev, "failed to request GPIO#%d: %d\n",
				data->enable_gpio, ret);
			goto err_alloc;
		}

		pb->enable_gpio = gpio_to_desc(data->enable_gpio);
	}

	if (pb->enable_gpio)
		gpiod_direction_output(pb->enable_gpio, 1);

	pb->power_supply = devm_regulator_get(&pdev->dev, "power");
	if (IS_ERR(pb->power_supply)) {
		dev_info(&pdev->dev, "no power regulator found, assuming the"
				"power supply is controlled in somewhere else like"
				"in panel drivers\n");
		pb->power_supply = NULL;
	}

	/*
	 * For DT case, devm_pwm_get will finally call of_pwm_get.
	 * It is not necessary to parse data->pwm_id value from separate
	 * device tree property since in of_pwm_get, we will use 1st argument
	 * of pwms property for pwm_id, global PWM device index.
	 */

	pb->pwm = devm_pwm_get(&pdev->dev, NULL);
	if (IS_ERR(pb->pwm)) {
		dev_info(&pdev->dev,
			"PWM request fail by devm_pwm_get, trying of_pwm_get\n");
		pb->pwm = of_pwm_get(target_bl_node, NULL);
		if (IS_ERR(pb->pwm)) {
			dev_info(&pdev->dev, "Trying PWM req with legacy API\n");
			pb->pwm = pwm_request(data->pwm_id, "pwm-backlight");
			if (IS_ERR(pb->pwm)) {
				dev_err(&pdev->dev,
					"unable to request legacy PWM\n");
				ret = PTR_ERR(pb->pwm);
				goto err_alloc;
			}
		}
	}

	dev_dbg(&pdev->dev, "got pwm for backlight\n");

	/*
	 * The DT case will not set pwm_period_ns. Instead, it stores the
	 * period, parsed from the DT, in the PWM device. In other words,
	 * the 2nd argument of pwms property indicates pwm_period in
	 * nonoseconds. For the non-DT case, set the period from
	 * platform data.
	 */
	pb->period = pwm_get_period(pb->pwm);
	if (!pb->period && (data->pwm_period_ns > 0)) {
		pb->period = data->pwm_period_ns;
		pwm_set_period(pb->pwm, data->pwm_period_ns);
	}

	pb->lth_brightness = data->lth_brightness * (pb->period / pb->scale);

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = data->max_brightness;

	if (gpio_is_valid(pb->pwm_gpio)) {
		ret = gpio_request(pb->pwm_gpio, "disp_bl");
		if (ret)
			dev_err(&pdev->dev, "backlight gpio request failed\n");
	}

	bl = backlight_device_register(dev_name(&pdev->dev), &pdev->dev, pb,
				       &pwm_backlight_ops, &props);
	if (IS_ERR(bl)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		ret = PTR_ERR(bl);
		goto err_alloc;
	}

	if (data->dft_brightness > data->max_brightness) {
		dev_warn(&pdev->dev,
			 "invalid dft brightness: %u, using max one %u\n",
			 data->dft_brightness, data->max_brightness);
		data->dft_brightness = data->max_brightness;
	}

	platform_set_drvdata(pdev, bl);
	bl->props.brightness = data->dft_brightness;
	backlight_update_status(bl);

	if (gpio_is_valid(pb->pwm_gpio))
		gpio_free(pb->pwm_gpio);

	return 0;

err_alloc:
	if (data->exit)
		data->exit(&pdev->dev);
	return ret;
}

static int pwm_backlight_remove(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct pwm_bl_data *pb = bl_get_data(bl);

	backlight_device_unregister(bl);
	pwm_backlight_power_off(pb);

	if (pb->exit)
		pb->exit(&pdev->dev);

	return 0;
}

static void pwm_backlight_shutdown(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct pwm_bl_data *pb = bl_get_data(bl);

	pwm_backlight_power_off(pb);
}

#ifdef CONFIG_PM_SLEEP
static int pwm_backlight_suspend(struct device *dev)
{
	struct backlight_device *bl = dev_get_drvdata(dev);
	struct pwm_bl_data *pb = bl_get_data(bl);

	if (pb->notify)
		pb->notify(pb->dev, 0);

	pwm_backlight_power_off(pb);

	if (pb->notify_after)
		pb->notify_after(pb->dev, 0);

	return 0;
}

static int pwm_backlight_resume(struct device *dev)
{
	struct backlight_device *bl = dev_get_drvdata(dev);

	backlight_update_status(bl);

	return 0;
}
#endif

static const struct dev_pm_ops pwm_backlight_pm_ops = {
#ifdef CONFIG_PM_SLEEP
	.suspend = pwm_backlight_suspend,
	.resume = pwm_backlight_resume,
	.poweroff = pwm_backlight_suspend,
	.restore = pwm_backlight_resume,
#endif
};

static struct platform_driver pwm_backlight_driver = {
	.driver		= {
		.name		= "pwm-backlight",
		.pm		= &pwm_backlight_pm_ops,
		.of_match_table	= of_match_ptr(pwm_backlight_of_match),
	},
	.probe		= pwm_backlight_probe,
	.remove		= pwm_backlight_remove,
	.shutdown	= pwm_backlight_shutdown,
};

module_platform_driver(pwm_backlight_driver);

MODULE_DESCRIPTION("PWM based Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:pwm-backlight");
