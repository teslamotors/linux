/*
 * pinmux-fixed-regulator.c: Fixed regulator controler by pinmux
 *
 * Copyright (c) 2013 NVIDIA CORPORATION. All rights reserved.
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 * Based on fixed.c from
 * Mark Brown <broonie@opensource.wolfsonmicro.com>
 * Copyright 2008 Wolfson Microelectronics PLC.
 *
 * Copyright (c) 2009 Nokia Corporation
 * Roger Quadros <ext-roger.quadros@nokia.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>

struct pinmux_fixed_regulator_config {
	const char *supply_name;
	const char *input_supply;
	int microvolts;
	unsigned startup_delay;
	struct regulator_init_data *init_data;
	struct pinctrl *pinctrl;
	struct pinctrl_state *enable_state;
	struct pinctrl_state *disable_state;
	int ena_gpio;
	int enable_high;
};

struct pinmux_fixed_regulator_data {
	struct device *dev;
	struct regulator_dev *rdev;
	struct pinmux_fixed_regulator_config *config;
	struct regulator_desc desc;
	int microvolts;
	int current_state;
};

static struct pinmux_fixed_regulator_config
	*of_get_pinmux_fixed_regulator_config(struct device *dev)
{
	struct pinmux_fixed_regulator_config *config;
	struct device_node *np = dev->of_node;
	u32 delay;
	struct regulator_init_data *init_data;
	int ret;

	config = devm_kzalloc(dev, sizeof(*config), GFP_KERNEL);
	if (!config)
		return ERR_PTR(-ENOMEM);

	config->init_data = of_get_regulator_init_data(dev, dev->of_node);
	if (!config->init_data)
		return ERR_PTR(-EINVAL);

	init_data = config->init_data;
	init_data->constraints.apply_uV = 0;
	config->supply_name = init_data->constraints.name;
	if (init_data->constraints.min_uV == init_data->constraints.max_uV) {
		config->microvolts = init_data->constraints.min_uV;
	} else {
		dev_err(dev, "Pinmux Regulator has variable voltages\n");
		return ERR_PTR(-EINVAL);
	}

	config->ena_gpio = of_get_named_gpio(dev->of_node, "gpio", 0);
	if (config->ena_gpio == -EPROBE_DEFER)
		return ERR_PTR(-EPROBE_DEFER);

	if (of_property_read_bool(dev->of_node, "enable-active-high"))
		config->enable_high = 1;

	config->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(config->pinctrl)) {
		ret = PTR_ERR(config->pinctrl);
		dev_err(dev, "Pinctrl driver not available\n");
		return ERR_PTR(ret);
	}

	config->enable_state = pinctrl_lookup_state(config->pinctrl, "enable");
	if (IS_ERR(config->enable_state))
		config->enable_state = NULL;

	config->disable_state = pinctrl_lookup_state(config->pinctrl,
					"disable");
	if (IS_ERR(config->disable_state))
		config->disable_state = NULL;

	if (!config->enable_state && !config->disable_state) {
		dev_err(dev, "Pinconfig not found for enable/disable\n");
		return ERR_PTR(-EINVAL);
	}

	ret = of_property_read_u32(np, "startup-delay-us", &delay);
	if (!ret)
		config->startup_delay = delay;

	if (of_find_property(np, "vin-supply", NULL))
		config->input_supply = "vin";

	return config;
}

static int pinmux_fixed_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct pinmux_fixed_regulator_data *prdata = rdev_get_drvdata(rdev);

	if (prdata->microvolts)
		return prdata->microvolts;
	else
		return -EINVAL;
}

static int pinmux_fixed_regulator_list_voltage(struct regulator_dev *rdev,
				      unsigned selector)
{
	struct pinmux_fixed_regulator_data *prdata = rdev_get_drvdata(rdev);

	if (selector != 0)
		return -EINVAL;

	return prdata->microvolts;
}

static int pinmux_fixed_regulator_enable(struct regulator_dev *rdev)
{
	struct pinmux_fixed_regulator_data *prdata = rdev_get_drvdata(rdev);
	struct pinmux_fixed_regulator_config *config = prdata->config;
	int ret;

	if (config->enable_state) {
		ret = pinctrl_select_state(config->pinctrl,
					config->enable_state);
		if (ret < 0) {
			dev_err(prdata->dev,
				"Setting pin enable state failed: %d\n", ret);
			return ret;
		}
	}

	if (gpio_is_valid(config->ena_gpio))
		gpio_direction_output(config->ena_gpio, config->enable_high);

	prdata->current_state = 1;
	return 0;
}

static int pinmux_fixed_regulator_disable(struct regulator_dev *rdev)
{
	struct pinmux_fixed_regulator_data *prdata = rdev_get_drvdata(rdev);
	struct pinmux_fixed_regulator_config *config = prdata->config;
	int ret;

	if (gpio_is_valid(config->ena_gpio))
		gpio_direction_output(config->ena_gpio, !config->enable_high);

	if (config->disable_state) {
		ret = pinctrl_select_state(config->pinctrl,
					config->disable_state);
		if (ret < 0) {
			dev_err(prdata->dev,
				"Setting pin disable state failed: %d\n", ret);
			return ret;
		}
	}
	prdata->current_state = 0;
	return 0;
}

static int pinmux_fixed_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct pinmux_fixed_regulator_data *prdata = rdev_get_drvdata(rdev);

	return prdata->current_state;
}

static struct regulator_ops pinmux_fixed_regulator_ops = {
	.get_voltage = pinmux_fixed_regulator_get_voltage,
	.list_voltage = pinmux_fixed_regulator_list_voltage,
	.enable = pinmux_fixed_regulator_enable,
	.disable = pinmux_fixed_regulator_disable,
	.is_enabled = pinmux_fixed_regulator_is_enabled,
};

static int pinmux_fixed_regulator_probe(struct platform_device *pdev)
{
	struct pinmux_fixed_regulator_config *config;
	struct pinmux_fixed_regulator_data *prdata;
	struct regulator_config cfg = { };
	int ret;

	if (pdev->dev.of_node) {
		config = of_get_pinmux_fixed_regulator_config(&pdev->dev);
		if (IS_ERR(config))
			return PTR_ERR(config);
	} else {
		config = pdev->dev.platform_data;
	}

	if (!config)
		return -ENOMEM;

	prdata = devm_kzalloc(&pdev->dev, sizeof(*prdata), GFP_KERNEL);
	if (!prdata)
		return -ENOMEM;

	prdata->config = config;
	prdata->dev = &pdev->dev;
	prdata->desc.type = REGULATOR_VOLTAGE;
	prdata->desc.owner = THIS_MODULE;
	prdata->desc.ops = &pinmux_fixed_regulator_ops;
	prdata->desc.enable_time = config->startup_delay;

	if (gpio_is_valid(config->ena_gpio)) {
		ret = devm_gpio_request(&pdev->dev, config->ena_gpio,
						"pinctl-fixed-gpio");
		if (ret < 0) {
			dev_err(&pdev->dev, "gpio requeste failed: %d\n", ret);
			return ret;
		}
	}

	prdata->desc.name = kstrdup(config->supply_name, GFP_KERNEL);
	if (!prdata->desc.name) {
		dev_err(&pdev->dev, "Failed to allocate supply name\n");
		return -ENOMEM;
	}

	if (config->input_supply) {
		prdata->desc.supply_name = kstrdup(config->input_supply,
							GFP_KERNEL);
		if (!prdata->desc.supply_name) {
			dev_err(&pdev->dev,
				"Failed to allocate input supply\n");
			ret = -ENOMEM;
			goto err_name;
		}
	}

	if (config->microvolts)
		prdata->desc.n_voltages = 1;

	prdata->microvolts = config->microvolts;

	cfg.dev = &pdev->dev;
	cfg.init_data = config->init_data;
	cfg.driver_data = prdata;
	cfg.of_node = pdev->dev.of_node;

	prdata->rdev = devm_regulator_register(&pdev->dev, &prdata->desc, &cfg);
	if (IS_ERR(prdata->dev)) {
		ret = PTR_ERR(prdata->dev);
		dev_err(&pdev->dev, "Failed to register regulator: %d\n", ret);
		goto err_input;
	}

	platform_set_drvdata(pdev, prdata);
	return 0;

err_input:
	kfree(prdata->desc.supply_name);
err_name:
	kfree(prdata->desc.name);
	return ret;
}

static int pinmux_fixed_regulator_remove(struct platform_device *pdev)
{
	struct pinmux_fixed_regulator_data *prdata = platform_get_drvdata(pdev);

	kfree(prdata->desc.supply_name);
	kfree(prdata->desc.name);
	return 0;
}

static const struct of_device_id pinmux_of_match[] = {
	{ .compatible = "regulator-pinmux-fixed", },
	{},
};
MODULE_DEVICE_TABLE(of, pinmux_of_match);

static struct platform_driver regulator_pinmux_fixed_driver = {
	.probe		= pinmux_fixed_regulator_probe,
	.remove		= pinmux_fixed_regulator_remove,
	.driver		= {
		.name		= "regulator-pinmux-fixed",
		.owner		= THIS_MODULE,
		.of_match_table = pinmux_of_match,
	},
};

static int __init pinmux_fixed_regulator_sync_init(void)
{
	return platform_driver_register(&regulator_pinmux_fixed_driver);
}
subsys_initcall_sync(pinmux_fixed_regulator_sync_init);

static void __exit pinmux_fixed_regulator_exit(void)
{
	platform_driver_unregister(&regulator_pinmux_fixed_driver);
}
module_exit(pinmux_fixed_regulator_exit);

MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_DESCRIPTION("Pinmux Fixed regulator");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:pinimux-fixed-regulator");
