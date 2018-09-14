// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 Intel Corporation

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/module.h>

#include <media/ipu-isys.h>
#include <media/max9286.h>
#include "ipu.h"
#include <media/crlmodule.h>

#define IMX355_LANES		4
#define IMX355_I2C_ADDRESS	0x1a
#define IMX319_LANES		4
#define IMX319_I2C_ADDRESS	0x10
#define AK7375_I2C_ADDRESS	0xc

static struct ipu_isys_csi2_config imx355_csi2_cfg = {
	.nlanes = IMX355_LANES,
	.port = 4, /* WF camera */
};

static struct ipu_isys_subdev_info imx355_sd = {
	.csi2 = &imx355_csi2_cfg,
	.i2c = {
		.board_info = {
			I2C_BOARD_INFO("imx355", IMX355_I2C_ADDRESS),
		},
		.i2c_adapter_id = 9,
	}
};

/* FIXME: Remove this after hardware transition. */
static struct ipu_isys_subdev_info imx355_sd2 = {
	.csi2 = &imx355_csi2_cfg,
	.i2c = {
		.board_info = {
			 I2C_BOARD_INFO("imx355", 0x10),
		},
		.i2c_adapter_id = 9,
	}
};

static struct ipu_isys_subdev_info ak7375_sd = {
	.i2c = {
		.board_info = {
			I2C_BOARD_INFO("ak7375", AK7375_I2C_ADDRESS),
		},
		.i2c_adapter_id = 9,
	}
};

static struct ipu_isys_csi2_config imx319_csi2_cfg = {
	.nlanes = IMX319_LANES,
	.port = 0, /* UF camera */
};

static struct ipu_isys_subdev_info imx319_sd = {
	.csi2 = &imx319_csi2_cfg,
	.i2c = {
		.board_info = {
			I2C_BOARD_INFO("imx319", IMX319_I2C_ADDRESS),
		},
		.i2c_adapter_id = 8,
	}
};

#ifdef CONFIG_INTEL_IPU4_AR0231AT
#define AR0231AT_LANES            4
#define AR0231ATA_I2C_ADDRESS      0x11
#define AR0231ATB_I2C_ADDRESS      0x12
#define AR0231ATC_I2C_ADDRESS      0x13
#define AR0231ATD_I2C_ADDRESS      0x14

static struct crlmodule_platform_data ar0231at_pdata = {
	.lanes = AR0231AT_LANES,
	.ext_clk = 24000000,
	.op_sys_clock = (uint64_t[]){ 264000000 },
	.module_name = "AR0231AT",
};
#endif

#if IS_ENABLED(CONFIG_VIDEO_MAX9286)
#define DS_MAX9286_LANES                4
#define DS_MAX9286_I2C_ADAPTER_B        3
#define DS_MAX9286_I2C_ADDRESS          0x48

static struct ipu_isys_csi2_config max9286_b_csi2_cfg = {
	.nlanes = DS_MAX9286_LANES,
	.port = 4,
};

struct max9286_subdev_i2c_info max9286_b_subdevs[] = {
#ifdef CONFIG_INTEL_IPU4_AR0231AT
		{
			.board_info = {
				.type = CRLMODULE_NAME,
				.addr = AR0231ATA_I2C_ADDRESS,
				.platform_data = &ar0231at_pdata,
			},
			.i2c_adapter_id = DS_MAX9286_I2C_ADAPTER_B,
		},
		{
			.board_info = {
				.type = CRLMODULE_NAME,
				.addr = AR0231ATB_I2C_ADDRESS,
				.platform_data = &ar0231at_pdata,
			},
			.i2c_adapter_id = DS_MAX9286_I2C_ADAPTER_B,
		},
		{
			.board_info = {
				.type = CRLMODULE_NAME,
				.addr = AR0231ATC_I2C_ADDRESS,
				.platform_data = &ar0231at_pdata,
			},
			.i2c_adapter_id = DS_MAX9286_I2C_ADAPTER_B,
		},
		{
			.board_info = {
				.type = CRLMODULE_NAME,
				.addr = AR0231ATD_I2C_ADDRESS,
				.platform_data = &ar0231at_pdata,
			},
			.i2c_adapter_id = DS_MAX9286_I2C_ADAPTER_B,
		},
#endif
};

static struct max9286_pdata max9286_b_pdata = {
	.subdev_info = max9286_b_subdevs,
	.subdev_num = ARRAY_SIZE(max9286_b_subdevs),
	.reset_gpio = 195,
};

static struct ipu_isys_subdev_info max9286_b_sd = {
	.csi2 = &max9286_b_csi2_cfg,
	.i2c = {
		.board_info = {
			.type = "max9286",
			.addr = DS_MAX9286_I2C_ADDRESS,
			.platform_data = &max9286_b_pdata,
		},
		.i2c_adapter_id = DS_MAX9286_I2C_ADAPTER_B,
	}
};
#endif

static struct ipu_isys_clk_mapping clk_mapping[] = {
	{ CLKDEV_INIT("3-0048", NULL, NULL), "OSC_CLK_OUT1" },
	{ CLKDEV_INIT(NULL, NULL, NULL), NULL }
};

static struct ipu_isys_subdev_pdata pdata = {
	.subdevs = (struct ipu_isys_subdev_info *[]) {
		&imx355_sd,
		&imx355_sd2,
		&imx319_sd,
		&ak7375_sd,
#if IS_ENABLED(CONFIG_VIDEO_MAX9286)
		&max9286_b_sd,
#endif
		NULL,
	},
	.clk_map = clk_mapping,
};

static void ipu4p_quirk(struct pci_dev *pci_dev)
{
	pr_info("Intel platform data PCI quirk for IPU4P CNL\n");
	pci_dev->dev.platform_data = &pdata;
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, IPU_PCI_ID, ipu4p_quirk);

MODULE_AUTHOR("Bingbu Cao <bingbu.cao@intel.com>");
MODULE_AUTHOR("Qiu, Tianshu <tian.shu.qiu@intel.com>");
MODULE_AUTHOR("Kun Jiang <kun.jiang@intel.com>");
MODULE_LICENSE("GPL");
