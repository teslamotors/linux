/*
 * Copyright (c) 2014--2016 Intel Corporation.
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

#include <media/intel-ipu4-acpi.h>
#include <media/intel-ipu4-isys.h>
#include "../../../../include/media/as3638.h"

#define GPIO_BASE		429

#define AS3638_DEVEL

/*
 * Placeholder for possible bxt_rvp platform data for testing different
 * AOB etc. non bios supported configurations
 */
/* Special camera AOB not yet supported by ACPI */
/* #define CUSTOM_AOB */
#ifdef CUSTOM_AOB
#include "../../../../include/media/crlmodule.h"
#define OV8856_LANES		4
#define OV8856_I2C_ADDRESS	0x36
#define IMX318_LANES		4
#define IMX318_I2C_ADDRESS	0x1a

static struct crlmodule_platform_data ov8856_pdata = {
	.xshutdown = GPIO_BASE + 67,
	.lanes = OV8856_LANES,
	.ext_clk = 24000000,
	.module_name = "OV8856"
};
static struct intel_ipu4_isys_csi2_config ov8856_csi2_cfg = {
	.nlanes = OV8856_LANES,
	.port = 0,
};

static struct intel_ipu4_isys_subdev_info ov8856_crl_sd = {
	.csi2 = &ov8856_csi2_cfg,
	.i2c = {
		.board_info = {
			 I2C_BOARD_INFO(CRLMODULE_NAME, OV8856_I2C_ADDRESS),
			 .platform_data = &ov8856_pdata,
		},
		.i2c_adapter_id = 2,
	},
};

static struct intel_ipu4_isys_csi2_config imx318_csi2_cfg = {
	.nlanes = IMX318_LANES,
	.port = 4,
};

static struct crlmodule_platform_data imx318_crl_pdata = {
	.xshutdown = GPIO_BASE + 68,
	.lanes = IMX318_LANES,
	.ext_clk = 24000000,
	.module_name = "SONY318A",
};

static struct intel_ipu4_isys_subdev_info imx318_crl_sd = {
	.csi2 = &imx318_csi2_cfg,
	.i2c = {
		.board_info = {
			 I2C_BOARD_INFO(CRLMODULE_NAME, IMX318_I2C_ADDRESS),
			 .platform_data = &imx318_crl_pdata,
		},
		.i2c_adapter_id = 1,
	},
};


static struct intel_ipu4_isys_clk_mapping clk_mapping[] = {
	{ CLKDEV_INIT("1-001a",  NULL, NULL), "OSC_CLK_OUT0" }, /* imx318 */
	{ CLKDEV_INIT("2-0036",  NULL, NULL), "OSC_CLK_OUT1" }, /* ov8856 */
	{ CLKDEV_INIT(NULL, NULL, NULL), NULL }
};

static struct intel_ipu4_isys_subdev_pdata pdata = {
	.subdevs = (struct intel_ipu4_isys_subdev_info *[]) {
		&ov8856_crl_sd,
		&imx318_crl_sd,
		NULL,
	},
	.clk_map = clk_mapping,
};


#else

/* Move to ACPI asap */
const struct intel_ipu4_regulator imx230regulators[] = {
	{ "wcove_regulator", "VPROG5A", "VANA" },
	{ "wcove_regulator", "VFLEX", "VDIG" },
	{ "wcove_regulator", "VPROG4C", "VAF" },
	{ "wcove_regulator", "VPROG1C", "VIO" },
	{ NULL, NULL, NULL }
};
EXPORT_SYMBOL_GPL(imx230regulators);

/* Move to ACPI asap */
const struct intel_ipu4_regulator ov8858regulators[] = {
	{ "wcove_regulator", "VPROG4B", "VANA" },
	{ "wcove_regulator", "VPROG1E", "VDIG" },
	{ "wcove_regulator", "VPROG1C", "VIO" },
	{ NULL, NULL, NULL }
};
EXPORT_SYMBOL_GPL(ov8858regulators);

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

#endif
static void ipu4_quirk(struct pci_dev *pci_dev)
{
	pci_dev->dev.platform_data = &pdata;
}

static int __init intel_ipu4_pdata_init(void)
{
	struct pci_dev *pdev;

	/* BXT B0 / C0 */
	pdev = pci_get_device(PCI_VENDOR_ID_INTEL, 0x1a88, NULL);
	if (pdev)
		pdev->dev.platform_data = &pdata;

	return 0;
}

module_init(intel_ipu4_pdata_init);

/* BXT A0 PCI id is 0x4008 */
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, 0x4008, ipu4_quirk);
/* BXT B0 */
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, 0x1a88, ipu4_quirk);
MODULE_AUTHOR("Intel");
MODULE_LICENSE("GPL");
