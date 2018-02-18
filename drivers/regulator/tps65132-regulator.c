/*
 * TI TPS65132 Regulator driver
 *
 * Copyright (C) 2015 NVIDIA CORPORATION. All rights reserved.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/slab.h>
#include <linux/delay.h>

#define tps65132_rails(_name)	"tps65132-"#_name

#define TPS65132_REG_VPOS		0x00
#define TPS65132_REG_VNEG		0x01
#define TPS65132_REG_APPS_DISP_DISN	0x03
#define TPS65132_REG_CONTROL		0x0FF

#define TPS65132_VOUT_MASK		0x1F
#define TPS65132_VOUT_N_VOLTAGE		0x15
#define TPS65132_VOUT_VMIN		4000000
#define TPS65132_VOUT_VMAX		6000000
#define TPS65132_VOUT_STEP		100000

#define TPS65132_REG_APPS_DISN		BIT(0)
#define TPS65132_REG_APPS_DISP		BIT(1)

#define TPS65132_REGULATOR_ID_VPOS	0
#define TPS65132_REGULATOR_ID_VNEG	1
#define TPS65132_MAX_REGULATORS		2

#define TPS65132_ACT_DIS_TIME_SLACK		1000

struct tps65132_regulator_pdata {
	int enable_gpio;
	bool disable_active_discharge;
	struct regulator_init_data *ridata;
	int active_discharge_gpio;
	unsigned int active_discharge_time;
};

struct tps65132_regulator {
	struct device *dev;
	struct regmap *rmap;
	struct regulator_desc *rdesc[TPS65132_MAX_REGULATORS];
	struct tps65132_regulator_pdata reg_pdata[TPS65132_MAX_REGULATORS];
	struct regulator_dev *rdev[TPS65132_MAX_REGULATORS];
};

static int tps65132_post_enable(struct regulator_dev *rdev)
{
	struct tps65132_regulator *tps = rdev_get_drvdata(rdev);
	int id = rdev_get_id(rdev);
	unsigned int dis_mask;
	int ret;

	dis_mask = (id == TPS65132_REGULATOR_ID_VPOS) ?
			TPS65132_REG_APPS_DISP : TPS65132_REG_APPS_DISN;

	if (tps->reg_pdata[id].disable_active_discharge) {
		ret = regmap_update_bits(tps->rmap, TPS65132_REG_APPS_DISP_DISN,
			dis_mask, 0);
		if (ret < 0) {
			dev_err(tps->dev,
				"Reg APPS_DISP_DISN update failed: %d\n", ret);
			return ret;
		}
	}
	return 0;
}

static int tps65132_post_disable(struct regulator_dev *rdev)
{
	struct tps65132_regulator *tps = rdev_get_drvdata(rdev);
	int id = rdev_get_id(rdev);
	int act_dis_gpio = tps->reg_pdata[id].active_discharge_gpio;
	unsigned int act_dis_time_us = tps->reg_pdata[id].active_discharge_time;

	if (gpio_is_valid(act_dis_gpio)) {
		gpio_set_value_cansleep(act_dis_gpio, 1);
		usleep_range(act_dis_time_us,
			act_dis_time_us + TPS65132_ACT_DIS_TIME_SLACK);
		gpio_set_value_cansleep(act_dis_gpio, 0);
	}

	return 0;
}

static struct regulator_ops tps65132_regulator_ops = {
	.post_enable = tps65132_post_enable,
	.post_disable = tps65132_post_disable,
	.list_voltage = regulator_list_voltage_linear,
	.map_voltage = regulator_map_voltage_linear,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
};

#define TPS65132_REGULATOR_DESC(_id, _name)		\
	[TPS65132_REGULATOR_ID_##_id] = {		\
		.name = tps65132_rails(_name),		\
		.supply_name = "vin",			\
		.id = TPS65132_REGULATOR_ID_##_id,	\
		.ops = &tps65132_regulator_ops,		\
		.n_voltages = TPS65132_VOUT_N_VOLTAGE,	\
		.min_uV = TPS65132_VOUT_VMIN,		\
		.uV_step = TPS65132_VOUT_STEP,		\
		.enable_time = 500,			\
		.vsel_mask = TPS65132_VOUT_MASK,	\
		.vsel_reg = TPS65132_REG_##_id,		\
		.type = REGULATOR_VOLTAGE,		\
		.owner = THIS_MODULE,			\
	}

static struct regulator_desc tps65132_regs_desc[TPS65132_MAX_REGULATORS] = {
	TPS65132_REGULATOR_DESC(VPOS, outp),
	TPS65132_REGULATOR_DESC(VNEG, outn),
};

static struct of_regulator_match tps65132_regulator_matches[] = {
	{ .name = "outp", },
	{ .name = "outn", },
};

static int tps65132_get_regulator_dt_data(struct device *dev,
		struct tps65132_regulator *tps65132_regs)
{
	struct tps65132_regulator_pdata *rpdata;
	struct device_node *rnode;
	int id;
	int ret;

	ret = of_regulator_match(dev, dev->of_node, tps65132_regulator_matches,
			ARRAY_SIZE(tps65132_regulator_matches));
	if (ret < 0) {
		dev_err(dev, "node parsing for regulator failed: %d\n", ret);
		return ret;
	}

	for (id = 0; id < ARRAY_SIZE(tps65132_regulator_matches); ++id) {
		rpdata = &tps65132_regs->reg_pdata[id];
		rnode = tps65132_regulator_matches[id].of_node;
		if (!rnode)
			continue;

		rpdata->ridata = tps65132_regulator_matches[id].init_data;
		rpdata->enable_gpio = of_get_named_gpio(rnode,
						"ti,enable-gpio", 0);
		if (rpdata->enable_gpio == -EPROBE_DEFER)
			return -EPROBE_DEFER;

		rpdata->disable_active_discharge = of_property_read_bool(rnode,
						"ti,disable-active-discharge");

		rpdata->active_discharge_gpio = of_get_named_gpio(rnode,
						"ti,active-discharge-gpio", 0);
		if (rpdata->active_discharge_gpio == -EPROBE_DEFER) {
			return -EPROBE_DEFER;
		} else if (gpio_is_valid(rpdata->active_discharge_gpio)) {
			ret = of_property_read_u32(rnode,
				"ti,active-discharge-time",
				&rpdata->active_discharge_time);
			if (ret < 0) {
				dev_err(dev, "Discharge time read failed: %d\n",
					ret);
				return ret;
			}
		}
	}

	return 0;
}

static const struct regmap_range tps65132_no_reg_ranges[] = {
	regmap_reg_range(TPS65132_REG_APPS_DISP_DISN + 1,
			TPS65132_REG_CONTROL - 1),
};

static const struct regmap_access_table tps65132_no_reg_table = {
	.no_ranges = tps65132_no_reg_ranges,
	.n_no_ranges = ARRAY_SIZE(tps65132_no_reg_ranges),
};

static const struct regmap_config tps65132_regmap_config = {
	.reg_bits	= 8,
	.val_bits	= 8,
	.max_register	= TPS65132_REG_CONTROL + 1,
	.cache_type	= REGCACHE_NONE,
	.rd_table	= &tps65132_no_reg_table,
	.wr_table	= &tps65132_no_reg_table,
};

static int tps65132_probe(struct i2c_client *client,
			 const struct i2c_device_id *client_id)
{
	struct device *dev = &client->dev;
	struct tps65132_regulator_pdata *rpdata;
	struct regulator_config config = { };
	struct regulator_desc *rdesc;
	struct tps65132_regulator *tps;
	int id;
	int ret;

	tps = devm_kzalloc(dev, sizeof(*tps), GFP_KERNEL);
	if (!tps) {
		dev_err(dev, "memory alloc failed\n");
		return -ENOMEM;
	}

	ret = tps65132_get_regulator_dt_data(dev, tps);
	if (ret == -EPROBE_DEFER) {
		dev_err(dev, "Probe deffered\n");
		return ret;
	} else if (ret < 0) {
		dev_err(dev, "Reading data from DT failed: %d\n", ret);
		return ret;
	}

	tps->rmap = devm_regmap_init_i2c(client, &tps65132_regmap_config);
	if (IS_ERR(tps->rmap)) {
		ret = PTR_ERR(tps->rmap);
		dev_err(dev, "regmap init failed: %d\n", ret);
		return ret;
	}

	i2c_set_clientdata(client, tps);
	tps->dev = dev;

	for (id = 0; id < TPS65132_MAX_REGULATORS; ++id) {
		rdesc = &tps65132_regs_desc[id];
		tps->rdesc[id] = rdesc;
		rpdata = &tps->reg_pdata[id];

		config.regmap = tps->rmap;
		config.dev = dev;
		config.init_data = rpdata->ridata;
		config.driver_data = tps;
		config.of_node = tps65132_regulator_matches[id].of_node;

		if (gpio_is_valid(rpdata->enable_gpio)) {
			config.ena_gpio = rpdata->enable_gpio;
			if (rpdata->ridata &&
				(rpdata->ridata->constraints.always_on ||
					rpdata->ridata->constraints.boot_on))
				config.ena_gpio_flags = GPIOF_OUT_INIT_HIGH;
		}

		if (gpio_is_valid(rpdata->active_discharge_gpio)) {
			ret = devm_gpio_request_one(dev,
				rpdata->active_discharge_gpio,
				GPIOF_OUT_INIT_LOW, rdesc->name);
			if (ret < 0) {
				dev_err(dev,
					"act dis gpio req failed for %s: %d\n",
					rdesc->name, ret);
				return ret;
			}
		} else {
			dev_info(dev, "No active discharge gpio for regulator %s\n",
				rdesc->name);
		}

		tps->rdev[id] = devm_regulator_register(dev, rdesc, &config);
		if (IS_ERR(tps->rdev[id])) {
			ret = PTR_ERR(tps->rdev[id]);
			dev_err(dev, "regulator %s register failed: %d\n",
						rdesc->name, ret);
			return ret;
		}
	}
	return 0;
}

static const struct i2c_device_id tps65132_id[] = {
	{.name = "tps65132",},
	{},
};
MODULE_DEVICE_TABLE(i2c, tps65132_id);

static struct i2c_driver tps65132_i2c_driver = {
	.driver = {
		.name = "tps65132",
		.owner = THIS_MODULE,
	},
	.probe = tps65132_probe,
	.id_table = tps65132_id,
};

static int __init tps65132_init(void)
{
	return i2c_add_driver(&tps65132_i2c_driver);
}
subsys_initcall(tps65132_init);

static void __exit tps65132_exit(void)
{
	i2c_del_driver(&tps65132_i2c_driver);
}
module_exit(tps65132_exit);

MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("tps65132 regulator driver");
MODULE_VERSION("1.0");
