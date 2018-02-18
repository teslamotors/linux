/*
 * pwm-regulator: PWM based regulator configuration.
 *
 * Copyright (c) 2013 NVIDIA CORPORATION. All rights reserved.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/slab.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>

struct pwm_regulator {
	struct device		*dev;
	struct regulator_desc	desc;
	struct regulator_dev	*rdev;
	struct pwm_device	*pwm;
	struct regulator_init_data *rinit_data;
	unsigned int		period;
	unsigned int		pulse_time;
	unsigned int		min_uV;
	unsigned int		max_uV;
	unsigned int		uV_step;
	unsigned int		n_voltages;
	unsigned int		curr_selector;
	int			enable_gpio;
	int			idle_gpio;
	int			standby_gpio;
	unsigned int		voltage_time_sel;
};

static int pwm_regulator_set_voltage_sel(
		struct regulator_dev *rdev, unsigned selector)
{
	struct pwm_regulator *preg = rdev_get_drvdata(rdev);
	int ret;
	int duty_cycle;

	duty_cycle = selector * preg->pulse_time;
	ret = pwm_config(preg->pwm, duty_cycle, preg->period);
	if (ret < 0) {
		dev_err(preg->dev, "pwm config failed: %d\n", ret);
		return ret;
	}

	if (duty_cycle) {
		ret = pwm_enable(preg->pwm);
		if (ret < 0) {
			dev_err(preg->dev, "pwm enable failed: %d\n", ret);
			return ret;
		}
	} else {
		pwm_disable(preg->pwm);
	}

	preg->curr_selector = selector;
	return ret;
}

static int pwm_regulator_get_voltage_sel(struct regulator_dev *rdev)
{
	struct pwm_regulator *preg = rdev_get_drvdata(rdev);

	return preg->curr_selector;
}

static int pwm_regulator_set_mode(struct regulator_dev *rdev,
	unsigned int mode)
{
	struct pwm_regulator *preg = rdev_get_drvdata(rdev);

	if (!gpio_is_valid(preg->idle_gpio))
		return -EINVAL;

	switch (mode) {
	case REGULATOR_MODE_IDLE:
		gpio_set_value(preg->idle_gpio, 0);
		break;
	case REGULATOR_MODE_NORMAL:
		gpio_set_value(preg->idle_gpio, 1);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static unsigned int pwm_regulator_get_mode(struct regulator_dev *rdev)
{
	struct pwm_regulator *preg = rdev_get_drvdata(rdev);

	if (!gpio_is_valid(preg->idle_gpio))
		return 0;

	if (gpio_get_value(preg->idle_gpio))
		return REGULATOR_MODE_NORMAL;
	else
		return REGULATOR_MODE_IDLE;
}

static int pwm_regulator_set_voltage_time_sel(struct regulator_dev *rdev,
		unsigned int old_selector, unsigned int new_selector)
{
	struct pwm_regulator *preg = rdev_get_drvdata(rdev);

	if (preg->voltage_time_sel)
		return preg->voltage_time_sel;
	return regulator_set_voltage_time_sel(rdev, old_selector, new_selector);
}

static struct regulator_ops pwm_regulator_ops = {
	.set_voltage_sel = pwm_regulator_set_voltage_sel,
	.get_voltage_sel = pwm_regulator_get_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	.map_voltage = regulator_map_voltage_linear,
	.set_voltage_time_sel   = pwm_regulator_set_voltage_time_sel,
	.set_mode = pwm_regulator_set_mode,
	.get_mode = pwm_regulator_get_mode,
};

static int pwm_regulator_parse_dt(struct device *dev,
		struct pwm_regulator *preg)
{
	struct device_node *node = dev->of_node;
	u32 pval;
	int ret;

	preg->rinit_data =  of_get_regulator_init_data(dev, dev->of_node);
	if (!preg->rinit_data) {
		dev_err(dev, "Not able to get OF regulator init data\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node, "regulator-n-voltages", &pval);
	if (!ret) {
		preg->n_voltages = pval;
	} else {
		dev_err(dev, "Number of voltages is missing\n");
		return ret;
	}

	ret = of_property_read_u32(node, "voltage-time-sel", &pval);
	if (!ret)
		preg->voltage_time_sel = pval;

	preg->enable_gpio = of_get_named_gpio(node, "enable-gpio", 0);
	if ((preg->enable_gpio < 0) && (preg->enable_gpio != -ENOENT)) {
		dev_err(dev, "Enable gpio not available, %d\n",
			preg->enable_gpio);
		return preg->enable_gpio;
	}

	preg->idle_gpio = of_get_named_gpio(node, "idle-gpio", 0);
	if ((preg->idle_gpio < 0) && (preg->idle_gpio != -ENOENT)) {
		dev_err(dev, "Idle gpio not available, %d\n", preg->idle_gpio);
		return preg->idle_gpio;
	}

	preg->standby_gpio = of_get_named_gpio(node, "standby-gpio", 0);
	if ((preg->standby_gpio < 0) && (preg->standby_gpio != -ENOENT)) {
		dev_err(dev, "Standby gpio not available, %d\n",
			preg->standby_gpio);
		return preg->standby_gpio;
	}

	preg->min_uV = preg->rinit_data->constraints.min_uV;
	preg->max_uV = preg->rinit_data->constraints.max_uV;
	return 0;
}

static int pwm_regulator_verify_patform_data(struct pwm_regulator *preg)
{
	struct device *dev = preg->dev;

	preg->period = pwm_get_period(preg->pwm);
	if (!preg->period)
		return -EINVAL;

	if (preg->n_voltages < 2) {
		dev_err(dev, "Number of volatges is not correct\n");
		return -EINVAL;
	}

	if (preg->period % (preg->n_voltages - 1)) {
		dev_err(dev, "PWM Period must multiple of n_voltages\n");
		return -EINVAL;
	}
	preg->pulse_time = preg->period / (preg->n_voltages - 1);
	if (!preg->pulse_time) {
		dev_err(dev, "Pulse time is invalid\n");
		return -EINVAL;
	}

	if ((preg->max_uV - preg->min_uV) % (preg->n_voltages - 1)) {
		dev_err(dev, "Min/Max is not proper to get step voltage\n");
		return -EINVAL;
	}
	preg->uV_step = (preg->max_uV - preg->min_uV) / (preg->n_voltages - 1);
	return 0;
}
static int pwm_regulator_probe(struct platform_device *pdev)
{
	struct pwm_regulator *preg;
	struct regulator_config config = { };
	int ret;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "Not DT registartion\n");
		return -ENODEV;
	}

	preg = devm_kzalloc(&pdev->dev, sizeof(*preg), GFP_KERNEL);
	if (!preg) {
		dev_err(&pdev->dev, "Memory allocation failed\n");
		return -ENOMEM;
	}
	preg->dev = &pdev->dev;

	ret = pwm_regulator_parse_dt(&pdev->dev, preg);
	if (ret < 0) {
		dev_err(&pdev->dev, "DT parsing is failed: %d\n", ret);
		return ret;
	}

	preg->pwm = devm_pwm_get(&pdev->dev, NULL);
	if (IS_ERR(preg->pwm)) {
		ret = PTR_ERR(preg->pwm);
		if (ret == -EPROBE_DEFER)
			dev_info(&pdev->dev, "PWM request deferred\n");
		else
			dev_err(&pdev->dev, "PWM request failed: %d\n", ret);
		return ret;
	}

	ret = pwm_regulator_verify_patform_data(preg);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"PWM regulator param verification failed: %d\n", ret);
		return ret;
	}

	if (gpio_is_valid(preg->idle_gpio)) {
		ret = devm_gpio_request_one(&pdev->dev, preg->idle_gpio,
				GPIOF_OUT_INIT_HIGH, "pwm-reg-idle-gpio");
		if (ret < 0) {
			dev_err(&pdev->dev, "Idle gpio request failed\n");
			return ret;
		}
		preg->rinit_data->constraints.valid_modes_mask |=
						REGULATOR_MODE_IDLE |
						REGULATOR_MODE_NORMAL;
		preg->rinit_data->constraints.valid_ops_mask |=
						REGULATOR_CHANGE_MODE;
	}

	preg->desc.name = "regulator-pwm";
	preg->desc.id = -1;
	preg->desc.ops = &pwm_regulator_ops;
	preg->desc.type = REGULATOR_VOLTAGE;
	preg->desc.owner = THIS_MODULE;

	preg->desc.min_uV = preg->min_uV;
	preg->desc.uV_step = preg->uV_step;
	preg->desc.linear_min_sel = 0;
	preg->desc.n_voltages = preg->n_voltages;

	if (gpio_is_valid(preg->enable_gpio)) {
		config.ena_gpio = preg->enable_gpio;
		if (preg->rinit_data->constraints.always_on ||
			preg->rinit_data->constraints.boot_on)
			config.ena_gpio_flags = GPIOF_OUT_INIT_HIGH;
	}

	config.dev = &pdev->dev;
	config.init_data = preg->rinit_data;
	config.driver_data = preg;
	config.of_node = pdev->dev.of_node;

	preg->rdev = devm_regulator_register(&pdev->dev, &preg->desc, &config);
	if (IS_ERR(preg->rdev)) {
		dev_err(preg->dev, "failed to register regulator %s\n",
			preg->desc.name);
		return PTR_ERR(preg->rdev);
	}

	platform_set_drvdata(pdev, preg);
	return 0;
}


#ifdef CONFIG_PM_SLEEP
static int pwm_regulator_resume(struct device *dev)
{
	struct pwm_regulator *preg = dev_get_drvdata(dev);
	pwm_regulator_set_voltage_sel(preg->rdev, preg->curr_selector);
	return 0;
}
#endif
static const struct dev_pm_ops pwm_regulator_pm_ops = {
#ifdef CONFIG_PM_SLEEP
	.resume_noirq_early = pwm_regulator_resume,
#endif
};

static const struct of_device_id pwm_regulator_of_match[] = {
	{ .compatible = "regulator-pwm", },
	{},
};
MODULE_DEVICE_TABLE(of, pwm_regulator_of_match);

static struct platform_driver pwm_regulator_driver = {
	.driver = {
		.name	= "regulator-pwm",
		.owner  = THIS_MODULE,
		.pm = &pwm_regulator_pm_ops,
		.of_match_table = pwm_regulator_of_match,
	},
	.probe	= pwm_regulator_probe,
};

static int __init pwm_regulator_init(void)
{
	return platform_driver_register(&pwm_regulator_driver);
}
subsys_initcall_sync(pwm_regulator_init);

static void __exit pwm_regulator_exit(void)
{
	platform_driver_unregister(&pwm_regulator_driver);
}
module_exit(pwm_regulator_exit);

MODULE_DESCRIPTION("PWM based regulator Driver");
MODULE_ALIAS("platform:regulator-pwm");
MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_LICENSE("GPL v2");
