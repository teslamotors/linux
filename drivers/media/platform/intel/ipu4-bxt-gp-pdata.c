/*
 * Copyright (c) 2016--2017 Intel Corporation.
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

#include <media/ipu-isys.h>
#include <media/crlmodule.h>
#include "ipu.h"

#define ADV7481_HDMI_LANES      4
#define ADV7481_HDMI_I2C_ADDRESS 0xe0

#define ADV7481_LANES		1
/*
 * below i2c address is dummy one, to be able to register single
 * ADV7481 chip as two sensors
 */
#define ADV7481_I2C_ADDRESS	0xe1


#define GPIO_BASE		434


static struct crlmodule_platform_data adv7481_cvbs_pdata = {
	.ext_clk = 286363636,
	.xshutdown = GPIO_BASE + 64, /*dummy for now*/
	.lanes = ADV7481_LANES,
	.module_name = "ADV7481 CVBS"
};

static struct ipu_isys_csi2_config adv7481_cvbs_csi2_cfg = {
	.nlanes = ADV7481_LANES,
	.port = 4,
};

static struct ipu_isys_subdev_info adv7481_cvbs_crl_sd = {
	.csi2 = &adv7481_cvbs_csi2_cfg,
	.i2c = {
		.board_info = {
			 .type = CRLMODULE_NAME,
			 .flags = I2C_CLIENT_TEN,
			 .addr = ADV7481_I2C_ADDRESS,
			 .platform_data = &adv7481_cvbs_pdata,
		},
		.i2c_adapter_id = 0,
	}
};

static struct crlmodule_platform_data adv7481_hdmi_pdata = {
	/* FIXME: may need to revisit */
	.ext_clk = 286363636,
	.xshutdown = GPIO_BASE + 30,
	.lanes = ADV7481_HDMI_LANES,
	.module_name = "ADV7481 HDMI",
	.crl_irq_pin = GPIO_BASE + 22,
	.irq_pin_flags = (IRQF_TRIGGER_RISING | IRQF_ONESHOT),
	.irq_pin_name = "ADV7481_HDMI_IRQ",
};

static struct ipu_isys_csi2_config adv7481_hdmi_csi2_cfg = {
	.nlanes = ADV7481_HDMI_LANES,
	.port = 0,
};

static struct ipu_isys_subdev_info adv7481_hdmi_crl_sd = {
	.csi2 = &adv7481_hdmi_csi2_cfg,
	.i2c = {
		.board_info = {
			 .type = CRLMODULE_NAME,
			 .flags = I2C_CLIENT_TEN,
			 .addr = ADV7481_HDMI_I2C_ADDRESS,
			 .platform_data = &adv7481_hdmi_pdata,
		},
		.i2c_adapter_id = 0,
	}
};



/*
 * Map buttress output sensor clocks to sensors -
 * this should be coming from ACPI, in Gordon Peak
 * ADV7481 have its own oscillator, no buttres clock
 * needed.
 */
struct ipu_isys_clk_mapping gp_mapping[] = {
	{ CLKDEV_INIT(NULL, NULL, NULL), NULL }
};

static struct ipu_isys_subdev_pdata pdata = {
	.subdevs = (struct ipu_isys_subdev_info *[]) {
		&adv7481_hdmi_crl_sd,
		&adv7481_cvbs_crl_sd,
		NULL,
	},
	.clk_map = gp_mapping,
};

static void ipu4_quirk(struct pci_dev *pci_dev)
{
	pci_dev->dev.platform_data = &pdata;
}

DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, IPU_PCI_ID,
			ipu4_quirk);
