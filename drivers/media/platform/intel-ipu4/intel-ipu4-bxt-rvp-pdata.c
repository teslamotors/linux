/*
 * Copyright (c) 2014--2015 Intel Corporation.
 *
 * Author: Sakari Ailus <sakari.ailus@linux.intel.com>
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

#include <media/intel-ipu4-isys.h>
#include "../../../../include/media/as3638.h"

#define GPIO_BASE		429

#define AS3638_DEVEL

/*
 * Placeholder for possible bxt_rvp platform data for testing different
 * AOB etc. non bios supported configurations
 */

/*
 * Development time configuration
 *
 * as3638
 * - i2c: pmic_i2c_2: 0x30
 * - regulator: v_vsys (always on)
 * - Enable = Flash_RST_N = GP_CAMERASB00 = North 62
 * - Torch = Flash_Torch = GP_CAMERASB01 = North 63
 * - Strobe = Flash_Trig = GP_CAMERASB02 = North 64
 */
#ifdef AS3638_DEVEL
static struct as3638_platform_data as3638_pdata = {
	.gpio_reset = GPIO_BASE + 62,
	.gpio_torch = GPIO_BASE + 63,
	.gpio_strobe = GPIO_BASE + 64,
	.flash_max_brightness[AS3638_LED1] = AS3638_FLASH_MAX_BRIGHTNESS_LED1,
	.torch_max_brightness[AS3638_LED1] = AS3638_TORCH_MAX_BRIGHTNESS_LED1,
	.flash_max_brightness[AS3638_LED2] = AS3638_FLASH_MAX_BRIGHTNESS_LED2,
	.torch_max_brightness[AS3638_LED2] = AS3638_TORCH_MAX_BRIGHTNESS_LED2,
	.flash_max_brightness[AS3638_LED3] = AS3638_FLASH_MAX_BRIGHTNESS_LED3,
	.torch_max_brightness[AS3638_LED3] = AS3638_TORCH_MAX_BRIGHTNESS_LED3,
};

static struct intel_ipu4_isys_subdev_info as3638_sd = {
	.i2c = {
		.board_info = {
			 I2C_BOARD_INFO(AS3638_NAME,  AS3638_I2C_ADDR),
			 .platform_data = &as3638_pdata,
		},
		.i2c_adapter_id = 2,
	},
/*	.acpiname = "i2c-AMS3638:00", */
};
#endif

/*
 * Map buttress output sensor clocks to sensors defined here -
 */
static struct intel_ipu4_isys_clk_mapping clk_mapping[] = {
	{ CLKDEV_INIT(NULL, NULL, NULL), NULL }
};

static struct intel_ipu4_isys_subdev_pdata pdata = {
	.subdevs = (struct intel_ipu4_isys_subdev_info *[]) {
#ifdef AS3638_DEVEL
		&as3638_sd,
#endif
		NULL,
	},
	.clk_map = clk_mapping,
};

static void ipu4_quirk(struct pci_dev *pci_dev)
{
	pci_dev->dev.platform_data = &pdata;
}

/* BXT A0 PCI id is 0x4008 */
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, 0x4008, ipu4_quirk);
/* BXT B0 */
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, 0x1a88, ipu4_quirk);
