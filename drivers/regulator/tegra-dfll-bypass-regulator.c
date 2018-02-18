/*
 *
 * Copyright (c) 2013-2014 NVIDIA CORPORATION. All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/tegra-dfll-bypass-regulator.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>

struct tegra_dfll_bypass_regulator {
	struct device *dev;
	struct regulator_desc desc;
	struct tegra_dfll_bypass_platform_data *pdata;
};

static int tegra_dfll_bypass_set_voltage(struct regulator_dev *reg,
		int min_uV, int max_uV, unsigned int *selector)
{
	int ret, delay;
	struct tegra_dfll_bypass_regulator *tdb = rdev_get_drvdata(reg);

	/*
	 * Before DFLL is ready, only initial voltage is supplied
	 * (may not be on exact selector boundary)
	 */
	if (!tdb->pdata->set_bypass_sel || !tdb->pdata->dfll_data) {
		if (min_uV == tdb->pdata->reg_init_data->constraints.init_uV) {
			*selector = regulator_map_voltage_linear(
				reg, min_uV, min_uV + tdb->desc.uV_step);
			return 0;
		}
		return -EINVAL;
	}

	ret = regulator_map_voltage_linear(reg, min_uV, max_uV);
	if (ret < 0) {
		dev_err(tdb->dev, "failed map [%duV, %duV]\n", min_uV, max_uV);
		return ret;
	}

	*selector = ret;
	ret = tdb->pdata->set_bypass_sel(tdb->pdata->dfll_data, *selector);
	if (ret) {
		dev_err(tdb->dev, "failed to set selector %u\n", *selector);
		return ret;
	}

	/* add voltage settling delay */
	delay = tdb->pdata->voltage_time_sel;
	if (delay > 1000)
		mdelay(delay / 1000);
	udelay(delay % 1000);

	return 0;
}

static int tegra_dfll_bypass_get_voltage(struct regulator_dev *reg)
{
	int sel;
	struct tegra_dfll_bypass_regulator *tdb = rdev_get_drvdata(reg);

	/* Before DFLL is ready, only initial voltage is supplied */
	if (!tdb->pdata->get_bypass_sel || !tdb->pdata->dfll_data)
		return tdb->pdata->reg_init_data->constraints.init_uV;

	sel = tdb->pdata->get_bypass_sel(tdb->pdata->dfll_data);
	if (sel < 0) {
		dev_err(tdb->dev, "failed to get selector\n");
		return sel;
	}

	return regulator_list_voltage_linear(reg, sel);
}

static int tegra_dfll_bypass_set_voltage_time_sel(struct regulator_dev *reg,
	unsigned int old_selector, unsigned int new_selector)
{
	struct tegra_dfll_bypass_regulator *tdb = rdev_get_drvdata(reg);
	return tdb->pdata->voltage_time_sel;
}

static int tegra_dfll_bypass_set_mode(struct regulator_dev *reg,
					unsigned int mode)
{
	struct tegra_dfll_bypass_regulator *tdb = rdev_get_drvdata(reg);

	if (!gpio_is_valid(tdb->pdata->msel_gpio))
		return -EINVAL;

	switch (mode) {
	case REGULATOR_MODE_IDLE:
		gpio_set_value(tdb->pdata->msel_gpio, 0);
		break;
	case REGULATOR_MODE_NORMAL:
		gpio_set_value(tdb->pdata->msel_gpio, 1);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static unsigned int tegra_dfll_bypass_get_mode(struct regulator_dev *reg)
{
	struct tegra_dfll_bypass_regulator *tdb = rdev_get_drvdata(reg);

	if (!gpio_is_valid(tdb->pdata->msel_gpio))
		return 0;

	if (gpio_get_value(tdb->pdata->msel_gpio))
		return REGULATOR_MODE_NORMAL;
	else
		return REGULATOR_MODE_IDLE;
}

static int tegra_dfll_bypass_set_vsel_volatile(struct regulator_dev *reg,
					       bool is_volatile)
{
	return is_volatile ? 0 : -EINVAL;	/* no VSEL cache */
}

static struct regulator_ops tegra_dfll_bypass_rops = {
	.set_voltage = tegra_dfll_bypass_set_voltage,
	.get_voltage = tegra_dfll_bypass_get_voltage,
	.list_voltage = regulator_list_voltage_linear,
	.map_voltage = regulator_map_voltage_linear,
	.set_voltage_time_sel = tegra_dfll_bypass_set_voltage_time_sel,
	.set_mode = tegra_dfll_bypass_set_mode,
	.get_mode = tegra_dfll_bypass_get_mode,
	.set_vsel_volatile = tegra_dfll_bypass_set_vsel_volatile,
};

static struct of_device_id of_tegra_dfll_bypass_pwm_match_tbl[] = {
	{ .compatible = "nvidia,tegra124-dfll-pwm", },
	{ .compatible = "nvidia,tegra132-dfll-pwm", },
	{ .compatible = "nvidia,tegra210-dfll-pwm", },
};

static struct of_device_id of_tegra_dfll_bypass_regulator_match_tbl[] = {
	{ .compatible = "regulator-pwm", },
};

static int tegra_dfll_bypass_parse_dt(struct device *dev,
	struct device_node *dn, struct tegra_dfll_bypass_platform_data *pdata)
{
	u32 val;
	int ret;
	struct regulation_constraints *c = &pdata->reg_init_data->constraints;

	c->valid_modes_mask |= REGULATOR_MODE_NORMAL;

	ret = of_property_read_u32(dn, "regulator-n-voltages", &val);
	if (ret || (val <= 1)) {
		dev_err(dev, "%s n-voltages\n", ret ? "missing" : "invalid");
		return -EINVAL;
	}
	pdata->n_voltages = val;

	ret = (c->max_uV - c->min_uV) / (val - 1);
	if (ret <= 0) {
		dev_err(dev, "invalid uV_step %d\n", ret);
		return -EINVAL;
	}
	pdata->uV_step = ret;

	ret = of_property_read_u32(dn, "voltage-time-sel", &val);
	if (!ret)
		pdata->voltage_time_sel = val;

	/* if error = negative/invalid gpio will be /ignored */
	pdata->msel_gpio = of_get_named_gpio(dn, "idle-gpio", 0);
	if (pdata->msel_gpio >= 0) {
		c->valid_modes_mask |= REGULATOR_MODE_IDLE;
		c->valid_ops_mask |= REGULATOR_CHANGE_MODE;
	}

	return 0;
}

static int tegra_dfll_bypass_match_pwm(
	struct device *dev, struct device_node *dn)
{
	int ret;
	struct of_phandle_args args;

	ret = of_parse_phandle_with_args(dn, "pwms", "#pwm-cells", 0, &args);
	if (ret) {
		dev_err(dev, "failed to parse pwms property\n");
		return ret;
	}

	ret = of_match_node(of_tegra_dfll_bypass_pwm_match_tbl, args.np) ?
		0 : -ENODEV;

	of_node_put(args.np);
	return ret;
}

static int tegra_dfll_bypass_probe(struct platform_device *pdev)
{
	int ret;
	struct tegra_dfll_bypass_platform_data *pdata = pdev->dev.platform_data;
	struct device_node *dn = pdev->dev.of_node;
	struct tegra_dfll_bypass_regulator *tdb = NULL;
	struct regulator_init_data *init_data = NULL;
	struct regulator_config config = { };
	struct regulator_dev *rdev;

	if (!pdata && dn) {
		ret = tegra_dfll_bypass_match_pwm(&pdev->dev, dn);
		if (ret)
			return ret;

		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			dev_err(&pdev->dev, "failed to allocate pdata\n");
			ret = -ENOMEM;
			goto err_out;
		}
		init_data = of_get_regulator_init_data(&pdev->dev, dn);
		if (!init_data) {
			dev_err(&pdev->dev, "failed to allocate init data\n");
			ret = -ENOMEM;
			goto err_out;
		}
		pdata->reg_init_data = init_data;
		ret = tegra_dfll_bypass_parse_dt(&pdev->dev, dn, pdata);
		if (ret) {
			dev_err(&pdev->dev, "failed to parse dt\n");
			goto err_out;
		}
	} else if (!pdata) {
		dev_err(&pdev->dev, "No platform data\n");
		return -ENODATA;
	}

	tdb = devm_kzalloc(&pdev->dev, sizeof(*tdb), GFP_KERNEL);
	if (!tdb) {
		dev_err(&pdev->dev, "failed to allocate regulator\n");
		ret = -ENOMEM;
		goto err_out;
	}

	tdb->dev = &pdev->dev;
	tdb->pdata = pdata;

	tdb->desc.name = "DFLL_BYPASS";
	tdb->desc.id = 0;
	tdb->desc.ops = &tegra_dfll_bypass_rops;
	tdb->desc.type = REGULATOR_VOLTAGE;
	tdb->desc.owner = THIS_MODULE;

	tdb->desc.min_uV = pdata->reg_init_data->constraints.min_uV;
	tdb->desc.uV_step = pdata->uV_step;
	tdb->desc.linear_min_sel = pdata->linear_min_sel;
	tdb->desc.n_voltages = pdata->n_voltages;

	config.dev = &pdev->dev;
	config.init_data = pdata->reg_init_data;
	config.driver_data = tdb;
	config.of_node = dn;

	rdev = regulator_register(&tdb->desc, &config);
	if (IS_ERR(rdev)) {
		dev_err(tdb->dev, "failed to register regulator %s\n",
			tdb->desc.name);
		ret = PTR_ERR(rdev);
		goto err_out;
	}

	if (gpio_is_valid(tdb->pdata->msel_gpio)) {
		if (gpio_request_one(pdata->msel_gpio, GPIOF_INIT_HIGH,
					"CPUREG_MODE_SEL")) {
			dev_err(&pdev->dev, "MODE_SEL gpio request failed\n");
			tdb->pdata->msel_gpio = -EINVAL;
		}
	}

	pdev->dev.platform_data = pdata;
	platform_set_drvdata(pdev, rdev);
	return 0;

err_out:
	if (tdb)
		devm_kfree(&pdev->dev, tdb);
	if (!pdev->dev.platform_data) {
		if (init_data)
			devm_kfree(&pdev->dev, init_data);
		if (pdata)
			devm_kfree(&pdev->dev, pdata);
	}
	return ret;
}

static int tegra_dfll_bypass_remove(struct platform_device *pdev)
{
	struct regulator_dev *rdev = platform_get_drvdata(pdev);
	struct tegra_dfll_bypass_regulator *tdb = rdev_get_drvdata(rdev);

	if (gpio_is_valid(tdb->pdata->msel_gpio))
		gpio_free(tdb->pdata->msel_gpio);
	regulator_unregister(rdev);
	return 0;
}

static struct platform_driver tegra_dfll_bypass_driver = {
	.driver = {
		.name	= "tegra_dfll_bypass",
		.of_match_table = of_tegra_dfll_bypass_regulator_match_tbl,
		.owner  = THIS_MODULE,
	},
	.probe	= tegra_dfll_bypass_probe,
	.remove	= tegra_dfll_bypass_remove,
};

static int __init tegra_dfll_bypass_init(void)
{
	return platform_driver_register(&tegra_dfll_bypass_driver);
}
subsys_initcall_sync(tegra_dfll_bypass_init);

static void __exit tegra_dfll_bypass_exit(void)
{
	platform_driver_unregister(&tegra_dfll_bypass_driver);
}
module_exit(tegra_dfll_bypass_exit);

