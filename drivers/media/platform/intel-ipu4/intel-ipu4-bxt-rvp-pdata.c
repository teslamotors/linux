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
#include <media/smiapp.h>
#include "../../../../include/media/crlmodule.h"
#include "../../../../include/media/lm3643.h"

/*
 * BXT RVP IMX214 is conneced to port 0 and have 4 lanes and
 * IMX132 to port 1 and have 2 lanes. Clock is fixed 24MHz.
 *
imx214
- i2c: pmic_i2c_1: 0x1a
- vdig: whiskey cove: vflexfb/vflexlx_1/vflexlx_2
- vana: whiskey cove: vprog4c
- vif: whiskey cove: vprog1c
- xshutdown: bxt: GP_CAMERASB03
- extclk: bxt: OSC_CLK_OUT_0

imx214 vcm
- i2c: pmic_i2c_1: 0x0c
- xshutdown: bxt: GP_CAMERASB03
- regulator: whiskey cove: vprog5a

imx214 eeprom
- i2c: pmic_i2c_1: 0x50-0x57

imx132
- i2c: pmic_i2c_2: 0x36
- extclk: bxt: OSC_CLK_OUT_1
- vdig: whiskey cove: vprog1e
- vana: whiskey cove: vprog4b
- vif: whiskey cove: vprog1c

lm3643
- i2c: pmic_i2c_2: 0x67 (ultra white); 0x63 (warm white)
- regulator: v_vsys (always on?; input to whiskey cove as well)
- hwen: bxt: GP_CAMERASB00
 */

/* DW9714 lens driver definitions */
#define DW9714_VCM_ADDR         0x0c
#define DW9714_NAME             "dw9714"

#define LC898122_VCM_ADDR         0x24
#define LC898122_NAME             "lc898122"

#define IMX214_LANES		4
#define IMX214_I2C_ADDRESS	0x1a

#define IMX132_LANES		1
#define IMX132_I2C_ADDRESS	0x36

#ifdef FABC_AOB
#define IMX230_LANES		4
#else
#define IMX230_LANES		2
#endif
#define IMX230_I2C_ADDRESS	0x1a

#define OV8858_LANES		4
#define OV8858_I2C_ADDRESS	0x10

#define GPIO_BASE		429


static struct crlmodule_platform_data imx214_pdata = {
	.xshutdown = GPIO_BASE + 65,
	.lanes = IMX214_LANES,
	.ext_clk = 24000000,
	.op_sys_clock = (uint64_t []){ 1344000000 / IMX214_LANES,
				       1296000000 / IMX214_LANES,
				       0 },
	.module_name = "SONY214A",
};

static struct intel_ipu4_isys_csi2_config imx214_csi2_cfg = {
	.nlanes = IMX214_LANES,
	.port = 0,
};

static struct intel_ipu4_isys_subdev_info imx214_sd = {
	.csi2 = &imx214_csi2_cfg,
	.i2c = {
		.board_info = {
			 I2C_BOARD_INFO(CRLMODULE_NAME, IMX214_I2C_ADDRESS),
			 .platform_data = &imx214_pdata,
		},
		.i2c_adapter_id = 1,
	},
	.acpiname = "i2c-SONY214A:00",
};

static struct lm3643_platform_data lm3643_pdata = {
	.gpio_reset = GPIO_BASE + 62,
	.flash_max_brightness = 500, /*500mA from both outputs = 1A*/
	.torch_max_brightness = 89, /*89mA*/
};

static struct intel_ipu4_isys_subdev_info lm3643_sd = {
	.i2c = {
		.board_info = {
			 I2C_BOARD_INFO(LM3643_NAME,  LM3643_I2C_ADDR),
			 .platform_data = &lm3643_pdata,
		},
		.i2c_adapter_id = 2,
	},
};

static struct intel_ipu4_isys_subdev_info lm3643a_sd = {
	.i2c = {
		.board_info = {
			 I2C_BOARD_INFO(LM3643_NAME,  LM3643_I2C_ADDR_REVA),
			 .platform_data = &lm3643_pdata,
		},
		.i2c_adapter_id = 2,
	},
	.acpiname = "i2c-TXNW3643:00",
};

static struct smiapp_platform_data imx132_pdata = {
	.xshutdown = GPIO_BASE + 67,
	.lanes = IMX132_LANES,
	.ext_clk = 24000000,
	/* FIXME: this clock is not possible on 24 MHz. */
	.op_sys_clock = (uint64_t []){ 312000000 / IMX132_LANES, 0 },
};

static struct intel_ipu4_isys_csi2_config imx132_csi2_cfg = {
	.nlanes = IMX132_LANES,
	.port = 1,
};

static struct intel_ipu4_isys_subdev_info imx132_sd = {
	.csi2 = &imx132_csi2_cfg,
	.i2c = {
		.board_info = {
			 I2C_BOARD_INFO(SMIAPP_NAME, IMX132_I2C_ADDRESS),
			 .platform_data = &imx132_pdata,
		},
		.i2c_adapter_id = 2,
	},
	.acpiname = "i2c-SONY132A:00",
};

static struct intel_ipu4_isys_subdev_info dw9714_sd = {
	.i2c = {
		.board_info = {
			I2C_BOARD_INFO(DW9714_NAME,  DW9714_VCM_ADDR),
		},
		.i2c_adapter_id = 1,
	}
};
static struct intel_ipu4_isys_subdev_info lc898122_sd = {
	.i2c = {
		.board_info = {
			I2C_BOARD_INFO(LC898122_NAME,  LC898122_VCM_ADDR),
		},
		.i2c_adapter_id = 1,
	}
};

static struct crlmodule_platform_data ov8858_pdata = {
	.xshutdown = GPIO_BASE + 67,
	.lanes = OV8858_LANES,
	.ext_clk = 24000000,
	.op_sys_clock = (uint64_t []){ 1440000000 / OV8858_LANES,
				       0 },
	.module_name = "OV8858"
};
static struct intel_ipu4_isys_csi2_config ov8858_csi2_cfg = {
	.nlanes = OV8858_LANES,
	.port = 0,
};

static struct intel_ipu4_isys_subdev_info ov8858_crl_sd = {
	.csi2 = &ov8858_csi2_cfg,
	.i2c = {
		.board_info = {
			 I2C_BOARD_INFO(CRLMODULE_NAME, OV8858_I2C_ADDRESS),
			 .platform_data = &ov8858_pdata,
		},
		.i2c_adapter_id = 2,
	},
	.acpiname = "i2c-INT3477:00",
};

#ifdef FABC_AOB
static struct intel_ipu4_isys_csi2_config imx230_csi2_cfg = {
	.nlanes = IMX230_LANES,
	.port = 4,
};

static struct crlmodule_platform_data imx230_crl_pdata = {
	.xshutdown = GPIO_BASE + 65,
	.lanes = IMX230_LANES,
	.ext_clk = 24000000,
	.op_sys_clock = (uint64_t []){ 2998400000 / IMX230_LANES },
	.module_name = "SONY230A",
};

static struct intel_ipu4_isys_subdev_info imx230_crl_sd = {
	.csi2 = &imx230_csi2_cfg,
	.i2c = {
		.board_info = {
			 I2C_BOARD_INFO(CRLMODULE_NAME, IMX230_I2C_ADDRESS),
			 .platform_data = &imx230_crl_pdata,
		},
		.i2c_adapter_id = 1,
	},
	.acpiname = "i2c-SONY230A:00",
};
#else
static struct intel_ipu4_isys_csi2_config imx230_csi2_cfg = {
	.nlanes = IMX230_LANES,
	.port = 1,
};

static struct crlmodule_platform_data imx230_crl_pdata = {
	.xshutdown = GPIO_BASE + 65,
	.lanes = IMX230_LANES,
	.ext_clk = 24000000,
	.op_sys_clock = (uint64_t []){ 1499200000 / IMX230_LANES },
	.module_name = "SONY230A",
};

static struct intel_ipu4_isys_subdev_info imx230_crl_sd = {
	.csi2 = &imx230_csi2_cfg,
	.i2c = {
		.board_info = {
			 I2C_BOARD_INFO(CRLMODULE_NAME, IMX230_I2C_ADDRESS),
			 .platform_data = &imx230_crl_pdata,
		},
		.i2c_adapter_id = 1,
	},
	.acpiname = "i2c-SONY230A:00",
};
#endif

/*
 * Map buttress output sensor clocks to sensors -
 * this should be coming from ACPI
 */
static struct intel_ipu4_isys_clk_mapping clk_mapping[] = {
	{ CLKDEV_INIT("i2c-SONY214A:00", NULL, NULL), "OSC_CLK_OUT0" },
	{ CLKDEV_INIT("i2c-SONY132A:00", NULL, NULL), "OSC_CLK_OUT1" },
	{ CLKDEV_INIT("i2c-SONY230A:00", NULL, NULL), "OSC_CLK_OUT0" },
	{ CLKDEV_INIT("i2c-INT3477:00",  NULL, NULL), "OSC_CLK_OUT1" },
	{ CLKDEV_INIT(NULL, NULL, NULL), NULL }
};

static struct intel_ipu4_isys_subdev_pdata pdata = {
	.subdevs = (struct intel_ipu4_isys_subdev_info *[]) {
		&ov8858_crl_sd,
		&imx230_crl_sd,
		&imx214_sd,
		&dw9714_sd,
		&lc898122_sd,
		&imx132_sd,
		&lm3643_sd,
		&lm3643a_sd,
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
