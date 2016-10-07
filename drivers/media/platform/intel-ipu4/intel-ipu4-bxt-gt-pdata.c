/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * Author: Tommi Franttila <tommi.franttila@intel.com>
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
#include "../../../../include/media/crlmodule.h"

/* DW9714 lens driver definitions */
#define DW9714_VCM_ADDR         0x0c
#define DW9714_NAME             "dw9714"

#define IMX214_LANES		4
#define IMX214_I2C_ADDRESS	0x10

#define OV5670_LANES		2
#define OV5670_I2C_ADDRESS	0x36

#define GPIO_BASE		429

static struct intel_ipu4_isys_csi2_config imx214_csi2_cfg = {
	.nlanes = IMX214_LANES,
	.port = 0,
};

static struct crlmodule_platform_data imx214_pdata = {
	.xshutdown = GPIO_BASE + 73,
	.lanes = IMX214_LANES,
	.ext_clk = 24000000,
	.module_name = "IMX214",
};

static struct intel_ipu4_isys_subdev_info imx214_sd = {
	.csi2 = &imx214_csi2_cfg,
	.i2c = {
		.board_info = {
			 I2C_BOARD_INFO(CRLMODULE_NAME, IMX214_I2C_ADDRESS),
			 .platform_data = &imx214_pdata,
		},
		.i2c_adapter_id = 8,
	},
};

static struct intel_ipu4_isys_csi2_config ov5670_csi2_cfg = {
	.nlanes = OV5670_LANES,
	.port = 4,
};

static struct crlmodule_platform_data ov5670_pdata = {
	.xshutdown = GPIO_BASE + 69,
	.lanes = OV5670_LANES,
	.ext_clk = 24000000,
	.module_name = "OV5670"
};

static struct intel_ipu4_isys_subdev_info ov5670_sd = {
	.csi2 = &ov5670_csi2_cfg,
	.i2c = {
		.board_info = {
			 I2C_BOARD_INFO(CRLMODULE_NAME, OV5670_I2C_ADDRESS),
			 .platform_data = &ov5670_pdata,
		},
		.i2c_adapter_id = 7,
	},
};

static struct intel_ipu4_isys_subdev_info dw9714_sd = {
	.i2c = {
		.board_info = {
			I2C_BOARD_INFO(DW9714_NAME,  DW9714_VCM_ADDR),
		},
		.i2c_adapter_id = 8,
	}
};

/*
 * Map buttress output sensor clocks to sensors -
 * this should be coming from ACPI
 */
static struct intel_ipu4_isys_clk_mapping clk_mapping[] = {
	{ CLKDEV_INIT("8-0010", NULL, NULL), "OSC_CLK_OUT0" },  /* imx214 */
	{ CLKDEV_INIT("7-0036",  NULL, NULL), "OSC_CLK_OUT2" }, /* ov5670 */
	{ CLKDEV_INIT(NULL, NULL, NULL), NULL }
};

static struct intel_ipu4_isys_subdev_pdata pdata = {
	.subdevs = (struct intel_ipu4_isys_subdev_info *[]) {
		&imx214_sd,
		&dw9714_sd,
		&ov5670_sd,
		NULL,
	},
	.clk_map = clk_mapping,
};

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

/* BXT B0 PCI id is 0x1a88 */
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, 0x1a88, ipu4_quirk);

MODULE_AUTHOR("Intel");
MODULE_LICENSE("GPL");
