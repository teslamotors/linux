/*
 * max16989-regulator.c -- max16989 regulator driver
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
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
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/of_regulator.h>
#include <linux/of_gpio.h>

/* Register definitions */
#define MAX16989_ID_REG				0x0
#define MAX16989_VIDMAX_REG			0x2
#define MAX16989_TCONFIG_REG			0x3
#define MAX16989_STATUS_REG			0x4
#define MAX16989_CONFIG_REG			0x5
#define MAX16989_SLEW_REG			0x6
#define MAX16989_VID_REG			0x7
#define MAX16989_TRACKVID_REG			0x2B
#define MAX16989_MAX_REG			(MAX16989_TRACKVID_REG + 1)

#define MAX16989_VID_MASK			0x7F
#define MAX16989_VOLTAGE_NSTEP			0x4E

#define MAX16989_CONFIG_VSTEP			BIT(7)
#define MAX16989_CONFIG_FPWM			BIT(3)
#define MAX16989_CONFIG_SS			BIT(2)
#define MAX16989_CONFIG_SYNC_OUTPUT		0x02

#define MAX16989_TCONFIG_ENTRK			BIT(7)

#define MAX16989_STATUS_INTERR			BIT(7)
#define MAX16989_STATUS_TRKERR			BIT(6)
#define MAX16989_STATUS_VRHOT			BIT(5)
#define MAX16989_STATUS_UV			BIT(4)
#define MAX16989_STATUS_OV			BIT(3)
#define MAX16989_STATUS_OC			BIT(2)
#define MAX16989_STATUS_VMERR			BIT(1)

struct max16989_regulator_platform_data {
	struct regulator_init_data *reg_init_data;
	int voltage_step_uv;
	bool enable_clock_ss;
	int enable_gpio;
	int fpwm_sync_gpio;
	int tracking_device_i2c_address;
	int slew_rate;
	bool enable_external_fpwm;
	bool enable_sync_output;
};

struct max16989_chip {
	struct device *dev;
	struct regulator_desc desc;
	struct regulator_dev *rdev;
	struct regmap *rmap;
	struct max16989_regulator_platform_data *pdata;
	bool vsel_volatile;
	int max_uv;
	int min_uv;
	int max_vout_vsel;
};

static int max16989_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct max16989_chip *mchip = rdev_get_drvdata(rdev);
	struct max16989_regulator_platform_data *pdata = mchip->pdata;
	int val;

	if (gpio_is_valid(pdata->fpwm_sync_gpio)) {
		val = (mode == REGULATOR_MODE_FAST) ? 1 : 0;
		gpio_set_value(pdata->fpwm_sync_gpio, val);
		return 0;
	}

	return 0;
}

static unsigned int max16989_get_mode(struct regulator_dev *rdev)
{
	struct max16989_chip *mchip = rdev_get_drvdata(rdev);
	struct max16989_regulator_platform_data *pdata = mchip->pdata;
	int val;
	unsigned int mode;

	if (gpio_is_valid(pdata->fpwm_sync_gpio)) {
		val = gpio_get_value(pdata->fpwm_sync_gpio);
		mode = (val == 1) ? REGULATOR_MODE_FAST :
					REGULATOR_MODE_NORMAL;
		return mode;
	}
	return REGULATOR_MODE_FAST;
}

static int max16989_set_voltage_time_sel(struct regulator_dev *rdev,
	unsigned int old_sel, unsigned int new_sel)
{
	int old_volt, new_volt;

	if (old_sel < new_sel)
		return regulator_set_voltage_time_sel(rdev, old_sel, new_sel);

	old_volt = regulator_list_voltage_linear(rdev, old_sel);
	new_volt = regulator_list_voltage_linear(rdev, new_sel);
	if (old_volt < 0)
		return old_volt;
	if (new_volt < 0)
		return new_volt;
	return DIV_ROUND_UP(abs(old_volt - new_volt), 1375);
}

static struct regulator_ops max16989_ops = {
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.set_mode		= max16989_set_mode,
	.get_mode		= max16989_get_mode,
	.set_voltage_time_sel	= max16989_set_voltage_time_sel,
};
static int slew_table_startup[] = {
	22000, 11000, 5500, 11000, 5500, 22000, 22000, 11000, 5500, 5500};
static int slew_table_rising[] = {
	22000, 22000, 22000, 11000, 11000, 22000, 22000, 22000, 22000, 5500};

static int max16989_init(struct max16989_chip *mchip,
	struct max16989_regulator_platform_data *pdata)
{
	struct regulator_init_data *ridata = pdata->reg_init_data;
	unsigned int status;
	unsigned int tconfig, config;
	int startup, rising;
	unsigned int slew;
	int ret;
	int vsel;

	config = 0;
	if (pdata->voltage_step_uv == -EINVAL) {
		ret = regmap_read(mchip->rmap, MAX16989_CONFIG_REG, &config);
		if (ret < 0) {
			dev_err(mchip->dev, "CONFIG reg read failed: %d\n", ret);
			return ret;
		}
		pdata->voltage_step_uv = (config & MAX16989_CONFIG_VSTEP) ?
						12500 : 10000;

		pdata->enable_clock_ss = !!(config & MAX16989_CONFIG_SS);
		pdata->enable_sync_output = ((config & 0x3) ==
						MAX16989_CONFIG_SYNC_OUTPUT);
		config = 0;
	}

	if (pdata->voltage_step_uv == 12500)
		config |= MAX16989_CONFIG_VSTEP;
	if (pdata->enable_clock_ss)
		config |= MAX16989_CONFIG_SS;
	if (pdata->enable_sync_output)
		config |= MAX16989_CONFIG_SYNC_OUTPUT;
	else if (!pdata->enable_external_fpwm)
		config |= MAX16989_CONFIG_FPWM;

	if (gpio_is_valid(pdata->fpwm_sync_gpio)) {
		ret = devm_gpio_request_one(mchip->dev, pdata->fpwm_sync_gpio,
				GPIOF_OUT_INIT_HIGH, "max16989-fpwm");
		if (ret < 0) {
			dev_err(mchip->dev,
				"gpio_request for gpio %d failed: %d\n",
				pdata->fpwm_sync_gpio, ret);
			return ret;
		}
	}
	ret = regmap_write(mchip->rmap, MAX16989_CONFIG_REG, config);
	if (ret < 0) {
		dev_err(mchip->dev, "CONFIG write failed: %d\n", ret);
		return ret;
	}

	mchip->max_uv = (pdata->voltage_step_uv == 12500) ? 1587500 : 1270000;
	mchip->min_uv = (pdata->voltage_step_uv == 12500) ? 625000 : 500000;
	mchip->max_vout_vsel = MAX16989_VOLTAGE_NSTEP;
	if (ridata) {
		if (!ridata->constraints.max_uV ||
			(ridata->constraints.max_uV > mchip->max_uv))
			ridata->constraints.max_uV = mchip->max_uv;
		if (ridata->constraints.max_uV <= mchip->min_uv)
			ridata->constraints.max_uV = mchip->max_uv;

		if (!ridata->constraints.min_uV ||
			(ridata->constraints.min_uV < mchip->min_uv))
			ridata->constraints.min_uV = mchip->min_uv;
		if (ridata->constraints.min_uV > mchip->max_uv)
			ridata->constraints.min_uV = mchip->min_uv;

		vsel = DIV_ROUND_UP(ridata->constraints.max_uV - mchip->min_uv,
				pdata->voltage_step_uv) + 0x1;
		mchip->max_vout_vsel = vsel;
	}
	ret = regmap_write(mchip->rmap, MAX16989_VIDMAX_REG,
				mchip->max_vout_vsel);
	if (ret < 0) {
		dev_err(mchip->dev, "VMAX write failed: %d\n", ret);
		return ret;
	}

	tconfig = 0;
	if (pdata->tracking_device_i2c_address &&
			pdata->voltage_step_uv == 12500) {
		switch (pdata->tracking_device_i2c_address) {
		case 0x38:
			tconfig = 0;
			break;
		case 0x3C:
			tconfig = 1;
			break;
		case 0x78:
			tconfig = 2;
			break;
		case 0x7C:
			tconfig = 3;
			break;
		default:
			dev_err(mchip->dev,
				"Invalid tracking device address: %d\n",
				pdata->tracking_device_i2c_address);
			return -EINVAL;
		}
		tconfig = MAX16989_TCONFIG_ENTRK;
	}
	ret = regmap_write(mchip->rmap, MAX16989_TCONFIG_REG, tconfig);
	if (ret < 0) {
		dev_err(mchip->dev, "TCONFIG write failed: %d\n", ret);
		return ret;
	}

	if (pdata->slew_rate == -EINVAL) {
		slew = 0;
		ret = regmap_read(mchip->rmap, MAX16989_SLEW_REG, &slew);
		if (ret < 0) {
			dev_err(mchip->dev, "SLEW reg read failed: %d\n", ret);
			return ret;
		}

		slew = slew & 0xF;
		if (slew > 0x9)
			slew = 0x9;
		startup = slew_table_startup[slew];
		rising = slew_table_rising[slew];
		goto slew_done;
	}
	startup = 22000;
	rising = 22000;
	slew = 0;
	switch (pdata->slew_rate) {
	case 0:
		startup = 22000;
		rising = 22000;
		slew = 0;
		break;
	case 1:
		startup = 11000;
		rising = 22000;
		slew = 1;
		break;
	case 2:
		startup = 5500;
		rising = 22000;
		slew = 2;
		break;
	case 3:
		startup = 11000;
		rising = 11000;
		slew = 3;
		break;
	case 4:
		startup = 5500;
		rising = 11000;
		slew = 4;
		break;
	case 5:
		startup = 5500;
		rising = 5500;
		slew = 9;
		break;
	default:
		break;
	}
	ret = regmap_write(mchip->rmap, MAX16989_SLEW_REG, slew);
	if (ret < 0) {
		dev_err(mchip->dev, "SLEW write failed: %d\n", ret);
		return ret;
	}
slew_done:
	if (pdata->voltage_step_uv == 12500) {
		rising = DIV_ROUND_UP(rising * 5, 4);
		startup = DIV_ROUND_UP(startup * 5, 4);
	}

	mchip->desc.ramp_delay = rising;
	mchip->desc.enable_time = startup;

	ret = regmap_read(mchip->rmap, MAX16989_STATUS_REG, &status);

	if (ret < 0) {
		dev_err(mchip->dev, "STATUS reg read failed: %d\n", ret);
		return ret;
	}

	if (status & MAX16989_STATUS_INTERR)
		dev_err(mchip->dev, "Device Internal error\n");

	if (status & MAX16989_STATUS_TRKERR)
		dev_err(mchip->dev, "Device Tracking error\n");

	if (status & MAX16989_STATUS_VRHOT)
		dev_err(mchip->dev, "Device thermal shutdown indication\n");

	if (status & MAX16989_STATUS_UV)
		dev_err(mchip->dev, "Device VOUT undervoltage\n");

	if (status & MAX16989_STATUS_OV)
		dev_err(mchip->dev, "Device VOUT overvoltage\n");

	if (status & MAX16989_STATUS_OC)
		dev_err(mchip->dev, "Device VOUT overcurrent\n");

	if (status & MAX16989_STATUS_VMERR)
		dev_err(mchip->dev, "Device VOUTMAX error\n");
	return 0;
}

static const struct regmap_range max16989_rd_ranges[] = {
	regmap_reg_range(MAX16989_ID_REG, MAX16989_ID_REG),
	regmap_reg_range(MAX16989_VIDMAX_REG, MAX16989_VID_REG),
	regmap_reg_range(MAX16989_TRACKVID_REG, MAX16989_TRACKVID_REG),
};

static const struct regmap_access_table max16989_rd_table = {
	.yes_ranges = max16989_rd_ranges,
	.n_yes_ranges = ARRAY_SIZE(max16989_rd_ranges),
};

static const struct regmap_range max16989_wr_ranges[] = {
	regmap_reg_range(MAX16989_VIDMAX_REG, MAX16989_TCONFIG_REG),
	regmap_reg_range(MAX16989_CONFIG_REG, MAX16989_VID_REG),
	regmap_reg_range(MAX16989_TRACKVID_REG, MAX16989_TRACKVID_REG),
};

static const struct regmap_access_table max16989_wr_table = {
	.yes_ranges = max16989_wr_ranges,
	.n_yes_ranges = ARRAY_SIZE(max16989_wr_ranges),
};

static bool max16989_is_volatile_reg(struct device *dev, unsigned int reg)
{
	struct max16989_chip *mchip = dev_get_drvdata(dev);

	switch (reg) {
	case MAX16989_STATUS_REG:
	case MAX16989_TRACKVID_REG:
		return true;
	case MAX16989_VID_REG:
		return mchip->vsel_volatile;
	default:
		return false;
	}
}

static int max16989_reg_volatile_set(struct device *dev, unsigned int reg,
			bool is_volatile)
{
	struct max16989_chip *mchip = dev_get_drvdata(dev);

	switch (reg) {
	case MAX16989_VID_REG:
		mchip->vsel_volatile = is_volatile;
		return 0;
	default:
		return -EINVAL;
	}
}

static const struct regmap_config max16989_regmap_config = {
	.reg_bits		= 8,
	.val_bits		= 8,
	.rd_table		= &max16989_rd_table,
	.wr_table		= &max16989_wr_table,
	.volatile_reg		= max16989_is_volatile_reg,
	.reg_volatile_set	= max16989_reg_volatile_set,
	.max_register		= MAX16989_MAX_REG - 1,
	.cache_type		= REGCACHE_RBTREE,
};

static const struct of_device_id max16989_of_match[] = {
	{ .compatible = "maxim,max16989", },
	{ .compatible = "maxim,max16989-new", },
	{},
};
MODULE_DEVICE_TABLE(of, max16989_of_match);

static struct max16989_regulator_platform_data *
	of_get_max16989_platform_data(struct device *dev)
{
	struct max16989_regulator_platform_data *pdata;
	struct device_node *np = dev->of_node;
	u32 pval;
	int ret;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "Memory alloc failed for platform data\n");
		return NULL;
	}

	pdata->reg_init_data = of_get_regulator_init_data(dev, dev->of_node);
	if (!pdata->reg_init_data) {
		dev_err(dev, "Not able to get OF regulator init data\n");
		return NULL;
	}

	ret = of_property_read_u32(np, "regulator-voltage-steps", &pval);
	pdata->voltage_step_uv = (ret) ? -EINVAL : pval;

	pdata->enable_sync_output = of_property_read_bool(np,
				"maxim,enable-sync-output");

	pdata->enable_clock_ss = of_property_read_bool(np,
				"maxim,enable-clk-spread-spectrum");

	pdata->enable_gpio = of_get_named_gpio(np, "maxim,enable-gpio", 0);

	pdata->fpwm_sync_gpio = of_get_named_gpio(np,
			"maxim,fpwm-sync-gpio", 0);
	if (pdata->fpwm_sync_gpio == -EINVAL)
		pdata->enable_external_fpwm = of_property_read_bool(np,
						"maxim,enable-external-fpwm");
	if (gpio_is_valid(pdata->fpwm_sync_gpio))
		pdata->enable_external_fpwm = true;

	ret = of_property_read_u32(np, "maxim,tracking-device-i2c-address",
				&pval);
	pdata->tracking_device_i2c_address = (ret) ? 0 : pval;

	ret = of_property_read_u32(np, "maxim,slew-rate", &pval);
	pdata->slew_rate = (ret) ? -EINVAL : pval;
	return pdata;
}

static int max16989_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct max16989_chip *mchip;
	struct max16989_regulator_platform_data *pdata;
	struct regulator_init_data *ridata;
	struct regulator_dev *rdev;
	struct regulator_config rconfig = { };
	int ret;

	if (!client->dev.of_node) {
		dev_err(&client->dev, "Supported only from DT\n");
		return -ENODEV;
	}

	pdata = of_get_max16989_platform_data(&client->dev);
	if (!pdata) {
		dev_err(&client->dev, "No Platform data\n");
		return -EINVAL;
	}

	if (pdata->enable_gpio == -EPROBE_DEFER)
		return -EPROBE_DEFER;

	mchip = devm_kzalloc(&client->dev, sizeof(*mchip), GFP_KERNEL);
	if (!mchip) {
		dev_err(&client->dev, "Memory allocation failed\n");
		return -ENOMEM;
	}

	mchip->dev = &client->dev;
	mchip->pdata = pdata;
	i2c_set_clientdata(client, mchip);

	mchip->rmap = devm_regmap_init_i2c(client, &max16989_regmap_config);
	if (IS_ERR(mchip->rmap)) {
		ret = PTR_ERR(mchip->rmap);
		dev_err(&client->dev, "regmap init failed: %d\n", ret);
		return ret;
	}

	ret = max16989_init(mchip, pdata);
	if (ret < 0) {
		dev_err(mchip->dev, "Init failed: %d\n", ret);
		return ret;
	}

	mchip->desc.name = id->name;
	mchip->desc.id = 0;
	mchip->desc.ops = &max16989_ops;
	mchip->desc.type = REGULATOR_VOLTAGE;
	mchip->desc.owner = THIS_MODULE;
	mchip->desc.vsel_reg = MAX16989_VID_REG;
	mchip->desc.vsel_mask = MAX16989_VID_MASK;
	mchip->desc.uV_step = pdata->voltage_step_uv;
	mchip->desc.min_uV = mchip->min_uv;
	mchip->desc.n_voltages = mchip->max_vout_vsel;
	mchip->desc.linear_min_sel = 1;

	/* Register the regulators */
	rconfig.dev = &client->dev;
	rconfig.of_node = client->dev.of_node;
	rconfig.init_data = pdata->reg_init_data;
	rconfig.driver_data = mchip;
	rconfig.regmap = mchip->rmap;
	if (gpio_is_valid(pdata->enable_gpio)) {
		ridata = pdata->reg_init_data;
		rconfig.ena_gpio = pdata->enable_gpio;
		rconfig.ena_gpio_flags = GPIOF_OUT_INIT_LOW;
		if (ridata && (ridata->constraints.always_on ||
				ridata->constraints.boot_on))
			rconfig.ena_gpio_flags = GPIOF_OUT_INIT_HIGH;
	}

	rdev = devm_regulator_register(&client->dev, &mchip->desc, &rconfig);
	if (IS_ERR(rdev)) {
		ret = PTR_ERR(rdev);
		dev_err(mchip->dev, "regulator register failed: %d\n", ret);
		return ret;
	}

	mchip->rdev = rdev;
	return 0;
}

static const struct i2c_device_id max16989_id[] = {
	{.name = "max16989",},
	{.name = "max16989-new",},
	{},
};
MODULE_DEVICE_TABLE(i2c, max16989_id);

static struct i2c_driver max16989_i2c_driver = {
	.driver = {
		.name = "max16989",
		.owner = THIS_MODULE,
		.of_match_table = max16989_of_match,
	},
	.probe = max16989_probe,
	.id_table = max16989_id,
};

static int __init max16989_drv_init(void)
{
	return i2c_add_driver(&max16989_i2c_driver);
}
subsys_initcall(max16989_drv_init);

static void __exit max16989_drv_cleanup(void)
{
	i2c_del_driver(&max16989_i2c_driver);
}
module_exit(max16989_drv_cleanup);

MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_DESCRIPTION("MAX16989 voltage regulator driver");
MODULE_LICENSE("GPL v2");
