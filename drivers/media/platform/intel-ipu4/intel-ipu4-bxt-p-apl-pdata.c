/*
 * Copyright (c) 2015 Intel Corporation.
 *
 * Author: Samu Onkalo <samu.onkalo@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/gpio-regulator.h>
#include <linux/regulator/machine.h>

#include <media/intel-ipu4-isys.h>

#define GPIO_BASE		434

static struct regulator_consumer_supply cam_reg_consumers[] = {
	REGULATOR_SUPPLY("vana", "i2c-INT3471:00"),
};

static struct regulator_init_data cam_reg_init_data = {
	.constraints = {
		.input_uV       = 3300000,
		.min_uV         = 2800000,
		.max_uV         = 2800000,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = ARRAY_SIZE(cam_reg_consumers),
	.consumer_supplies      = cam_reg_consumers,
};

/* GPIO Regulator for CAM-C port in kilshon based camera AOB */
static struct gpio_regulator_config kilshoncamc_info = {
	.supply_name = "kilshon-camc-aob",

	.enable_gpio = GPIO_BASE + 60,
	.enable_high = 1,
	.enabled_at_boot = 1,

	.nr_gpios = 0,
	.nr_states = 0,

	.type = REGULATOR_VOLTAGE,
	.init_data = &cam_reg_init_data,
};

static struct platform_device kilshoncamc = {
	.name = "gpio-regulator",
	.id   = -1,
	.dev  = {
		.platform_data = &kilshoncamc_info,
	},
};

/*
 * Map buttress output sensor clocks to sensors -
 */
static struct intel_ipu4_isys_clk_mapping clk_mapping[] = {
	{ CLKDEV_INIT(NULL, NULL, NULL), NULL }
};

static struct intel_ipu4_isys_subdev_pdata pdata = {
	.subdevs = (struct intel_ipu4_isys_subdev_info *[]) {
		NULL,
	},
	.clk_map = clk_mapping,
};

static struct platform_device *cam_regs[] __initdata = {
	&kilshoncamc,
};

static void ipu4_quirk(struct pci_dev *pci_dev)
{
	platform_add_devices(cam_regs,
			     ARRAY_SIZE(cam_regs));
	pci_dev->dev.platform_data = &pdata;
}

DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, 0x5a88, ipu4_quirk);
