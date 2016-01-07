/*
 * Copyright (c) 2015 Intel Corporation.
 *
 * Author: Jianxu Zheng <jian.xu.zheng@intel.com>
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
#include "../../pci/intel-ipu4/intel-ipu4.h"

#define OV13860_LANES		2
#define OV13860_I2C_ADDRESS	0x10

#define ADV7481_LANES		4
#define ADV7481_I2C_ADDRESS	0xe0

#define GPIO_BASE		422

/* The following OV13860 platform data is for camera AOB
 * used on Leafhill board (based on BXT-P).
 */
static struct crlmodule_platform_data ov13860_pdata = {
	.xshutdown = GPIO_BASE + 71,
	.lanes = OV13860_LANES,
	.ext_clk = 24000000,
	.op_sys_clock = (uint64_t []){ 600000000,
				       300000000},
	.module_name = "OV13860"
};

static struct intel_ipu4_isys_csi2_config ov13860_csi2_cfg = {
	.nlanes = OV13860_LANES,
	.port = 0,
};

static struct intel_ipu4_isys_subdev_info ov13860_crl_sd = {
	.csi2 = &ov13860_csi2_cfg,
	.i2c = {
		.board_info = {
			 I2C_BOARD_INFO(CRLMODULE_NAME, OV13860_I2C_ADDRESS),
			 .platform_data = &ov13860_pdata,
		},
		.i2c_adapter_id = 2,
	}
};

/*
 * The following adv7481 platform data is for Leaf Hill board(BXT-P).
 */
static struct crlmodule_platform_data adv7481_pdata = {
	/* FIXME: may need to revisit */
	.xshutdown = GPIO_BASE + 63,
	.lanes = ADV7481_LANES,
	/* FIXME: may need to revisit */
	.ext_clk = 24000000,
	.op_sys_clock = (uint64_t []){600000000},
	.module_name = "ADV7481"
};

static struct intel_ipu4_isys_csi2_config adv7481_csi2_cfg = {
	.nlanes = ADV7481_LANES,
	.port = 0,
};

static struct intel_ipu4_isys_subdev_info adv7481_crl_sd = {
	.csi2 = &adv7481_csi2_cfg,
	.i2c = {
		.board_info = {
			 .type = CRLMODULE_NAME,
			 .flags = I2C_CLIENT_TEN,
			 .addr = ADV7481_I2C_ADDRESS,
			 .platform_data = &adv7481_pdata,
		},
		.i2c_adapter_id = 2,
	}
};

/*
 * The following adv7481 evaluation board platform data is
 * for Leaf Hill board(BXT-P).
 */
static struct crlmodule_platform_data adv7481_eval_pdata = {
	/* FIXME: may need to revisit */
	.xshutdown = GPIO_BASE + 63,
	.lanes = ADV7481_LANES,
	/* FIXME: may need to revisit */
	.ext_clk = 24000000,
	.op_sys_clock = (uint64_t []){600000000},
	.module_name = "ADV7481_EVAL"
};

static struct intel_ipu4_isys_csi2_config adv7481_eval_csi2_cfg = {
	.nlanes = ADV7481_LANES,
	.port = 0,
};

static struct intel_ipu4_isys_subdev_info adv7481_eval_crl_sd = {
	.csi2 = &adv7481_eval_csi2_cfg,
	.i2c = {
		.board_info = {
			 .type = CRLMODULE_NAME,
			 .flags = I2C_CLIENT_TEN,
			 .addr = ADV7481_I2C_ADDRESS,
			 .platform_data = &adv7481_eval_pdata,
		},
		.i2c_adapter_id = 2,
	}
};
/*
 * Map buttress output sensor clocks to sensors -
 * this should be coming from ACPI
 */
struct intel_ipu4_isys_clk_mapping clk_mapping[] = {
	{ CLKDEV_INIT("2-0010", NULL, NULL), "OSC_CLK_OUT0" },
	{ CLKDEV_INIT("2-a0e0", NULL, NULL), "OSC_CLK_OUT0" },
	{ CLKDEV_INIT(NULL, NULL, NULL), NULL }
};

static struct intel_ipu4_isys_subdev_pdata pdata = {
	.subdevs = (struct intel_ipu4_isys_subdev_info *[]) {
#ifdef CONFIG_INTEL_IPU4_OV13860
		&ov13860_crl_sd,
#endif
#ifdef CONFIG_INTEL_IPU4_ADV7481
		&adv7481_crl_sd,
#endif
#ifdef CONFIG_INTEL_IPU4_ADV7481_EVAL
		&adv7481_eval_crl_sd,
#endif
		NULL,
	},
	.clk_map = clk_mapping,
};

static void ipu4_quirk(struct pci_dev *pci_dev)
{
	pci_dev->dev.platform_data = &pdata;
}

DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, INTEL_IPU4_HW_BXT_P_A0,
			ipu4_quirk);
