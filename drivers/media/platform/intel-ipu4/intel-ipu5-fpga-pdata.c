/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * Author: Wang, Zaikuo <zaikuo.wang@intel.com>
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
#include "../../pci/intel-ipu4/intel-ipu4.h"

#define GPIO_BASE		429

#define PIXTER_STUB_LANES	4
#define PIXTER_STUB_I2C_ADDRESS	0x7D

static struct crlmodule_platform_data pixter_stub_pdata = {
	.xshutdown = GPIO_BASE + 64,
	.lanes = PIXTER_STUB_LANES,
	.ext_clk = 24000000,
	.module_name = "PIXTER_STUB"
};

static struct intel_ipu4_isys_csi2_config pixter_stub_csi2_cfg = {
	.nlanes = PIXTER_STUB_LANES,
	.port = 0,
};

static struct intel_ipu4_isys_subdev_info pixter_stub_crl_sd = {
	.csi2 = &pixter_stub_csi2_cfg,
	.i2c = {
		.board_info = {
			I2C_BOARD_INFO(CRLMODULE_NAME, PIXTER_STUB_I2C_ADDRESS),
			.platform_data = &pixter_stub_pdata,
		},
		.i2c_adapter_id = 1,
	}
};

static struct intel_ipu4_isys_clk_mapping clk_mapping[] = {
	{ CLKDEV_INIT("1-007d",  NULL, NULL), "OSC_CLK_OUT0" },
	{ CLKDEV_INIT(NULL, NULL, NULL), NULL }
};

static struct intel_ipu4_isys_subdev_pdata pdata = {
	.subdevs = (struct intel_ipu4_isys_subdev_info *[]) {
		&pixter_stub_crl_sd,
		NULL,
	},
	.clk_map = clk_mapping,
};

static void ipu5_quirk(struct pci_dev *pci_dev)
{
	pr_info("FPGA platform data PCI quirk for IPU5\n");
	pci_dev->dev.platform_data = &pdata;
}

DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL,
	INTEL_IPU5_HW_FPGA_A0, ipu5_quirk);
