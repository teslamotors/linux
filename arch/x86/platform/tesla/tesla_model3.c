/* tesla_model3.c
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

/* Consumers of the BMP-DISP-5V supply. */
static struct regulator_consumer_supply reg_disp_5v_consumers[] = {
	/* ti949 has an HDMI input that needs BMP-DISP-5V.
	 */
	{ .supply = "vcc_hdmi", .dev_name = "i2c-TI949:00", },
};

/* Consumers of the 3V3-VID supply. */
static struct regulator_consumer_supply reg_3v3_consumers[] = {
	/* ti940 supply comes from 1V2-CSI and 1V8-VID.
	 * 1V2-CSI comes from 3V3-VID.
	 * 1V8-VID comes from 3V3-VID.
	 */
	{ .supply = "vcc", .dev_name = "3-0030", },

	/* ti949 supply comes from 3V3-VID and 1V8-VID.
	 * 1V8-VID comes from 3V3-VID.
	 */
	{ .supply = "vcc", .dev_name = "i2c-TI949:00", },
};

/* Consumers of the BMP-DISP-PWR supply. */
static struct regulator_consumer_supply reg_disp_pwr_consumers[] = {
	/* ti948 lives in a unit which needs the 12v BMP-DISP-PWR supply.
	 */
	{ .supply = "vcc_disp", .dev_name = "i2c-TI948:00", },
};

/* Consumers of the AMPS-nEN "enable" gpio
 * AMPS-nEN is sent to both TDA7802 amplifiers.
 */
static struct regulator_consumer_supply reg_amps_nen_consumers[] = {
	{ .supply = "enable", .dev_name = "i2c-TDA7802:00", },
	{ .supply = "enable", .dev_name = "i2c-TDA7802:01", },
};

/* Initialisation config for a GPIO-controlled regulator. */
struct tesla_reg_init_config {
	const char *supply_name;
	struct regulator_consumer_supply *consumers;
	const struct regulation_constraints *constraints;
	unsigned int nr_consumers;
	unsigned int gpio;
	unsigned int enable_high;
	unsigned int enabled_at_boot;
};

/* Constraints for BMP-DISP-5V supply. */
static const struct regulation_constraints reg_disp_5v_constraints = {
	.input_uV = 5000000,
	.min_uV = 5000000,
	.max_uV = 5000000,
	.valid_ops_mask = REGULATOR_CHANGE_STATUS,
};

/* Constraints for 3V3-VID supply. */
static const struct regulation_constraints reg_3v3_constraints = {
	.input_uV = 3300000,
	.min_uV = 3300000,
	.max_uV = 3300000,
	.valid_ops_mask = REGULATOR_CHANGE_STATUS,
};

/* Constraints for BMP-DISP-PWR supply. */
static const struct regulation_constraints reg_disp_pwr_constraints = {
	.input_uV = 12000000,
	.min_uV = 12000000,
	.max_uV = 12000000,
	.valid_ops_mask = REGULATOR_CHANGE_STATUS,
};

/* Initialisation config for the BMP-DISP-5V supply. */
static const struct tesla_reg_init_config __initdata init_reg_disp_5v = {
	.gpio = 367,
	.supply_name = "BMP-DISP-5V-EN",
	.constraints = &reg_disp_5v_constraints,
	.consumers = reg_disp_5v_consumers,
	.nr_consumers = ARRAY_SIZE(reg_disp_5v_consumers),
	.enable_high = 1,
	.enabled_at_boot = 1,
};

/* Initialisation config for the 3V3-VID supply. */
static const struct tesla_reg_init_config __initdata init_reg_3v3 = {
	.gpio = 464,
	.supply_name = "3V3-VID-EN",
	.constraints = &reg_3v3_constraints,
	.consumers = reg_3v3_consumers,
	.nr_consumers = ARRAY_SIZE(reg_3v3_consumers),
	.enable_high = 1,
	.enabled_at_boot = 1,
};

/* Initialisation config for the BMP-DISP-PWR supply. */
static const struct tesla_reg_init_config __initdata init_reg_disp_pwr = {
	.gpio = 467,
	.supply_name = "BMP-DISP-PWR-EN",
	.constraints = &reg_disp_pwr_constraints,
	.consumers = reg_disp_pwr_consumers,
	.nr_consumers = ARRAY_SIZE(reg_disp_pwr_consumers),
	.enable_high = 1,
	.enabled_at_boot = 1,
};

/* Initialisation config for the AMPS-nEN 3V3 gpio */
static const struct tesla_reg_init_config __initdata init_reg_amps_nen = {
	.gpio = 448,
	.supply_name = "AMPS-nEN",
	.constraints = &reg_3v3_constraints,
	.consumers = reg_amps_nen_consumers,
	.nr_consumers = ARRAY_SIZE(reg_amps_nen_consumers),
	.enable_high = 0,
	.enabled_at_boot = 0,
};

struct tesla_reg {
	struct platform_device dev;
	struct gpio_regulator_config reg_gpio;
	struct regulator_init_data reg_init;
};

/* Static counter for number of calls to `tesla_add_gpio_reg`, meaning each
 * platform_device created gets a unique ID. */
static int reg_id;

static void tesla_gpio_dev_release(struct device *dev)
{
	/* We aren't cleaning up, because there's no point; the device lasts
	 * until shutdown.
	 */
}

static int tesla_add_gpio_reg(const struct tesla_reg_init_config *reg_cfg)
{
	struct tesla_reg *reg;
	int ret;

	pr_info("Adding regulator %s (gpio %d)\n",
			reg_cfg->supply_name, reg_cfg->gpio);

	/* This isn't ever freed, but the lifetime is until shutdown so
	 * we don't need to care.
	 */
	reg = kzalloc(sizeof(struct tesla_reg), GFP_KERNEL);
	if (!reg) {
		pr_err("%s: failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

	reg->reg_init.constraints = *reg_cfg->constraints;
	reg->reg_init.consumer_supplies = reg_cfg->consumers;
	reg->reg_init.num_consumer_supplies = reg_cfg->nr_consumers;

	reg->reg_gpio.supply_name = reg_cfg->supply_name;
	reg->reg_gpio.enable_gpio = reg_cfg->gpio;
	reg->reg_gpio.enable_high = reg_cfg->enable_high;
	reg->reg_gpio.enabled_at_boot = reg_cfg->enabled_at_boot;
	reg->reg_gpio.type = REGULATOR_VOLTAGE;
	reg->reg_gpio.init_data = &reg->reg_init;

	reg->dev.name = "gpio-regulator";
	reg->dev.id = reg_id++;
	reg->dev.dev.platform_data = &reg->reg_gpio;
	reg->dev.dev.release = tesla_gpio_dev_release;

	ret = platform_device_register(&reg->dev);
	if (ret != 0) {
		pr_err("%s: failed to register device (%d)\n", __func__, ret);
		kfree(reg);
		return ret;
	}

	return 0;
}

static int __init tesla_model3_init(void)
{
	pr_info("Tesla Model3 hardware core support\n");
#ifdef M3_DISPLAY_REGULATOR
	tesla_add_gpio_reg(&init_reg_disp_5v);
	tesla_add_gpio_reg(&init_reg_3v3);
	tesla_add_gpio_reg(&init_reg_disp_pwr);
#endif
	tesla_add_gpio_reg(&init_reg_amps_nen);

	return 0;
}
subsys_initcall(tesla_model3_init);
