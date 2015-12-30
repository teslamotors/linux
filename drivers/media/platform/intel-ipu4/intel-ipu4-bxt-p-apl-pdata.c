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
#include <media/smiapp.h>
#include "../../../../include/media/crlmodule.h"
#include "../../../../include/media/lm3643.h"

#define IMX135_LANES 		4
#define IMX135_I2C_ADDRESS 	0x10

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

static struct crlmodule_platform_data imx135_pdata = {
	.xshutdown = GPIO_BASE + 55,
	.lanes = IMX135_LANES,
	.ext_clk = 24000000,
	.op_sys_clock = (uint64_t []){ 1804800000 / IMX135_LANES,
				       1368000000 / IMX135_LANES,
				       838400000 / IMX135_LANES,
				       0 },
};

static struct intel_ipu4_isys_csi2_config imx135_csi2_cfg = {
	.nlanes = IMX135_LANES,
	.port = 4,
};

static struct intel_ipu4_isys_subdev_info imx135_sd = {
	.csi2 = &imx135_csi2_cfg,
	.i2c = {
		.board_info = {
			 I2C_BOARD_INFO(CRLMODULE_NAME, IMX135_I2C_ADDRESS),
			 .platform_data = &imx135_pdata,
		},
		.i2c_adapter_id = 2,
	},
	.acpiname = "i2c-INT3471:00",
};

/*
 * Map buttress output sensor clocks to sensors -
 * this should be coming from ACPI
 */
static struct intel_ipu4_isys_clk_mapping clk_mapping[] = {
	{ CLKDEV_INIT("i2c-INT3471:00",  NULL, NULL), "OSC_CLK_OUT2" },
	{ CLKDEV_INIT(NULL, NULL, NULL), NULL }
};

static struct intel_ipu4_isys_subdev_pdata pdata = {
	.subdevs = (struct intel_ipu4_isys_subdev_info *[]) {
		&imx135_sd,
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
