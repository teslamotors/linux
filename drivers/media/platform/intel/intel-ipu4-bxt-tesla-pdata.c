/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * Author: Jouni Ukkonen <jouni.ukkonen@intel.com>
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
#include <media/crlmodule.h>
#include "ipu.h"

#if defined(CONFIG_VIDEO_TC3587_TI940) || defined(CONFIG_VIDEO_TC3587_TI940_MODULE)
#define I2C_NAME "tc3587"
#define TI_LVDS_LANES      2
#define TI_LVDS_I2C_ADDRESS 0x0e
#else
#define I2C_NAME "ti940"
#define TI_LVDS_LANES      4
#define TI_LVDS_I2C_ADDRESS 0x30
#endif

#define GPIO_BASE		434

static struct crlmodule_platform_data ti_lvds_pdata = {
	.ext_clk        = 24000000,     /* note, not CSI-clock rate */
	.lanes          = TI_LVDS_LANES,
	.module_name    = "TI LVDS",
};

static struct intel_ipu4_isys_csi2_config ti_lvds_csi2_cfg = {
	.nlanes = TI_LVDS_LANES,
	.port   = 0,
};

static struct intel_ipu4_isys_subdev_info ti_lvds_crl_sd = {
	.csi2 = &ti_lvds_csi2_cfg,
	.i2c = {
		.board_info = {
			 .type = I2C_NAME,
			 .addr = TI_LVDS_I2C_ADDRESS,
			 .platform_data = &ti_lvds_pdata,
		},
		.i2c_adapter_id = 3,
	}
};

/*
 * Map buttress output sensor clocks to sensors -
 * this should be coming from ACPI, in Gordon Peak
 * ADV7481 have its own oscillator, no buttres clock
 * needed.
 */
struct intel_ipu4_isys_clk_mapping clk_mapping[] = {
	{ CLKDEV_INIT(NULL, NULL, NULL), NULL }
};

static struct intel_ipu4_isys_subdev_pdata pdata = {
	.subdevs = (struct intel_ipu4_isys_subdev_info *[]) {
		&ti_lvds_crl_sd,
		NULL,
	},
	.clk_map = clk_mapping,
};

static void ipu4_quirk(struct pci_dev *pci_dev)
{
	pci_dev->dev.platform_data = &pdata;
}

DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, IPU_PCI_ID, ipu4_quirk);
