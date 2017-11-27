/*
 * svfpga-i2c.c -- SVFPGA ALSA SoC audio driver
 *
 * Copyright 2015 Intel, Inc.
 *
 * Author: Hardik Shah <hardik.t.shah@intel.com>
 * Dummy ASoC Codec Driver based Cirrus Logic CS42L42 Codec
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/sdw_bus.h>
#include <linux/gpio.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio/consumer.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include "svfpga.h"

extern const struct regmap_config svfpga_i2c_regmap;
extern const struct dev_pm_ops svfpga_runtime_pm;
static int svfpga_i2c_probe(struct i2c_client *i2c,
				 const struct i2c_device_id *i2c_id)
{
	struct regmap *regmap;

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EINVAL;

	regmap = devm_regmap_init_i2c(i2c, &svfpga_i2c_regmap);
	return svfpga_probe(&i2c->dev, regmap, NULL);
}

static int svfpga_i2c_remove(struct i2c_client *i2c)
{
	return svfpga_remove(&i2c->dev);
}

static const struct of_device_id svfpga_of_match[] = {
	{ .compatible = "svfpga", },
	{},
};
MODULE_DEVICE_TABLE(of, svfpga_of_match);


static const struct i2c_device_id svfpga_id[] = {
	{"svfpga", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, svfpga_id);

static struct i2c_driver svfpga_i2c_driver = {
	.driver = {
		   .name = "svfpga",
		   .owner = THIS_MODULE,
		   .pm = &svfpga_runtime_pm,
		   .of_match_table = svfpga_of_match,
		   },
	.id_table = svfpga_id,
	.probe = svfpga_i2c_probe,
	.remove = svfpga_i2c_remove,
};

module_i2c_driver(svfpga_i2c_driver);

MODULE_DESCRIPTION("ASoC SVFPGA driver I2C");
MODULE_DESCRIPTION("ASoC SVFPGA driver SDW");
MODULE_AUTHOR("Hardik shah, Intel Inc, <hardik.t.shah@intel.com");
MODULE_LICENSE("GPL");

