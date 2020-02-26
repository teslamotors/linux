/* tesla_info2.c
 *
 * Copyright 2018 Codethink Ltd.
 * Copyirght 2018 Tesla
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
*/

#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>

#include <linux/regulator/driver.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/gpio-regulator.h>

/* The tc3587 is an indirect consumer of this, because it needs the 1V8-VID
 * supply which comes from the 3V3-VID supply enabled by 3V3-VID-EN. */
static struct regulator_consumer_supply reg_video_consumers[] = {
	{ .supply = "vcc", .dev_name = "3-000e", },	/* tc3587 */
	{ .supply = "vcc", .dev_name = "2-0028", },	/* anx1122 */
};

/* Camera Serial Interface */
static struct regulator_consumer_supply reg_csi_consumers[] = {
	{ .supply = "vcc_csi", .dev_name = "3-000e", }  /* tc3587 */
};

static struct regulator_consumer_supply reg_camera_consumers[] = {
	{ .supply = "vcc_camera", .dev_name = "3-000e", }  /* tc3587 */
};

/* Consumers of the AMPS-nEN "enable" gpio. AMPS-nEN is sent to all TDA7802
 * amplifiers. */
static struct regulator_consumer_supply reg_amps_nen_consumers[] = {
	{ .supply = "enable", .dev_name = "i2c-TDA7802:00", },
	{ .supply = "enable", .dev_name = "i2c-TDA7802:01", },
	{ .supply = "enable", .dev_name = "i2c-TDA7802:02", },
	{ .supply = "enable", .dev_name = "i2c-TDA7802:03", },
};

struct tesla_reg_init {
	struct regulator_consumer_supply	*consumers;
	struct regulation_constraints		*constraints;
	unsigned int				nr_consumers;
	unsigned int				gpio;
	const char				*supply_name;
	const char				*parent;
	unsigned int				enable_high;
};
//struct gpio_regulator_config

static struct regulation_constraints default_constraints = {
	.input_uV	= 3300000,
	.min_uV		= 3300000,
	.max_uV		= 3300000,
	.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
};

static struct tesla_reg_init __initdata init_reg_video = {
	.gpio		= 464,
	.supply_name	= "3V3-VID-EN",
	.constraints	= &default_constraints,
	.consumers	= reg_video_consumers,
	.nr_consumers	= ARRAY_SIZE(reg_video_consumers),
	.enable_high = 1,
};

/* note, the csi is partly covered under the 3V3-VID-EN as this enables
 * power to the TC3587. The MAX device requires both the 3V3 for the camera
 * and the 1V8 from the video (fed directly from 3V3 video)
 */
static struct tesla_reg_init __initdata init_reg_csi = {
	.gpio		= 330,
	.supply_name	= "3V3-BUC-EN",
	.parent		= "3V3-VID-EN",	/* needed for 1V8 vid too */
	.constraints	= &default_constraints,
	.consumers	= reg_csi_consumers,
	.nr_consumers	= ARRAY_SIZE(reg_csi_consumers),
	.enable_high = 1,
};

static struct tesla_reg_init __initdata init_reg_camera = {
	.gpio		= 449,
	.supply_name	= "BMP-BUC-PWR-EN",
	.constraints	= &default_constraints,
	.consumers	= reg_camera_consumers,
	.nr_consumers	= ARRAY_SIZE(reg_camera_consumers),
	.enable_high = 1,
};

/* Initialisation config for the AMPS-nEN 3V3 gpio */
static struct tesla_reg_init __initdata init_reg_amps_nen = {
	.gpio = 447,
	.supply_name = "AMPS-nEN",
	.constraints = &default_constraints,
	.consumers = reg_amps_nen_consumers,
	.nr_consumers = ARRAY_SIZE(reg_amps_nen_consumers),
	.enable_high = 0,
};

struct tesla_reg {
	struct platform_device		dev;
	struct gpio_regulator_config	reg_gpio;
	struct regulator_init_data	reg_init;
};

static int reg_id;

static void tesla_gpio_dev_release(struct device *dev)
{
	// todo //
}

static int tesla_add_gpio_reg(struct tesla_reg_init *treg)
{
	struct regulator_init_data *init;
	struct gpio_regulator_config *gpio;
	struct tesla_reg *reg;
	int ret;

	pr_info("Adding regulator %s (gpio %d)\n", treg->supply_name, treg->gpio);

	reg = kzalloc(sizeof(struct tesla_reg), GFP_KERNEL);
	if (!reg) {
		pr_err("%s: failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

	init = &reg->reg_init;
	init->constraints = *treg->constraints;
	init->consumer_supplies = treg->consumers;
	init->num_consumer_supplies = treg->nr_consumers;

	gpio = &reg->reg_gpio;
	gpio->supply_name = treg->supply_name;
	gpio->enable_gpio = treg->gpio;
	gpio->enable_high = treg->enable_high;
	gpio->enabled_at_boot = 0;
	gpio->type = REGULATOR_VOLTAGE;
	gpio->init_data = init;

	reg->dev.name = "gpio-regulator";
	reg->dev.id = reg_id++;
	reg->dev.dev.platform_data = &reg->reg_gpio;
	reg->dev.dev.release = tesla_gpio_dev_release;

	ret = platform_device_register(&reg->dev);
	if (ret != 0) {
		pr_err("%s: failed to register device (%d)\n", __func__, ret);
		return ret;
	}

	return 0;
}

static int __init tesla_info2_init(void)
{
	printk("Tesla Info2 hardware core support\n");

	tesla_add_gpio_reg(&init_reg_video);
	tesla_add_gpio_reg(&init_reg_csi);
	tesla_add_gpio_reg(&init_reg_camera);
	tesla_add_gpio_reg(&init_reg_amps_nen);

	return 0;
}
subsys_initcall(tesla_info2_init);
