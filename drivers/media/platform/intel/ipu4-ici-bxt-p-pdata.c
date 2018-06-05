// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
/*
 * Copyright (C) 2018 Intel Corporation
 */

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/pci.h>

#include <media/ipu-isys.h>
#include <media/crlmodule-lite.h>
#include <media/ti964.h>
#include "ipu.h"

#define GPIO_BASE			422

#ifdef CONFIG_INTEL_IPU4_ADV7481

#define ADV7481_CVBS_LANES		1
#define ADV7481_HDMI_LANES		4
#define ADV7481_HDMI_I2C_ADDRESS	0xe0
#define ADV7481_CVBS_I2C_ADDRESS	0xe1
static struct crlmodule_lite_platform_data adv7481_hdmi_pdata_lite = {
#if (!IS_ENABLED(CONFIG_VIDEO_INTEL_UOS))
// 	xshutdown GPIO pin unavailable on ACRN UOS
	.xshutdown = GPIO_BASE + 63,
#endif
	.lanes = ADV7481_HDMI_LANES,
	.ext_clk = 24000000,
	.op_sys_clock = (uint64_t []){600000000},
	.module_name = "ADV7481 HDMI"
};
static struct ipu_isys_csi2_config adv7481_hdmi_csi2_cfg = {
	.nlanes = ADV7481_HDMI_LANES,
	.port = 0,
};
static struct ipu_isys_subdev_info adv7481_hdmi_crl_sd_lite = {
	.csi2 = &adv7481_hdmi_csi2_cfg,
	.i2c = {
		.board_info = {
			 .type = CRLMODULE_LITE_NAME,
			 .flags = I2C_CLIENT_TEN,
			 .addr = ADV7481_HDMI_I2C_ADDRESS,
			 .platform_data = &adv7481_hdmi_pdata_lite,
		},
		.i2c_adapter_id = CONFIG_INTEL_IPU4_ADV7481_I2C_ID,
	}
};

static struct crlmodule_lite_platform_data adv7481_cvbs_pdata_lite = {
#if (!IS_ENABLED(CONFIG_VIDEO_INTEL_UOS))
// 	xshutdown GPIO pin unavailable on ACRN UOS
	.xshutdown = GPIO_BASE + 63,
#endif
	.lanes = ADV7481_CVBS_LANES,
	.ext_clk = 24000000,
	.op_sys_clock = (uint64_t []){600000000},
	.module_name = "ADV7481 CVBS"
};
static struct ipu_isys_csi2_config adv7481_cvbs_csi2_cfg = {
	.nlanes = ADV7481_CVBS_LANES,
	.port = 4,
};
static struct ipu_isys_subdev_info adv7481_cvbs_crl_sd_lite = {
	.csi2 = &adv7481_cvbs_csi2_cfg,
	.i2c = {
		.board_info = {
			 .type = CRLMODULE_LITE_NAME,
			 .flags = I2C_CLIENT_TEN,
			 .addr = ADV7481_CVBS_I2C_ADDRESS,
			 .platform_data = &adv7481_cvbs_pdata_lite,
		},
		.i2c_adapter_id = CONFIG_INTEL_IPU4_ADV7481_I2C_ID,
	}
};
#endif

#ifdef CONFIG_INTEL_IPU4_ADV7481_EVAL

#define ADV7481_LANES			4
//below i2c address is dummy one, to be able to register single ADV7481 chip as two sensors
#define ADV7481_I2C_ADDRESS		0xe0
#define ADV7481B_I2C_ADDRESS		0xe2

static struct crlmodule_lite_platform_data adv7481_eval_pdata_lite = {
	.xshutdown = GPIO_BASE + 63,
	.lanes = ADV7481_HDMI_LANES,
	.ext_clk = 24000000,
	.op_sys_clock = (uint64_t []){600000000},
	.module_name = "ADV7481_EVAL"
};
static struct ipu_isys_csi2_config adv7481_eval_csi2_cfg = {
	.nlanes = ADV7481_LANES,
	.port = 0,
};
static struct ipu_isys_subdev_info adv7481_eval_crl_sd_lite = {
	.csi2 = &adv7481_eval_csi2_cfg,
	.i2c = {
		.board_info = {
			 .type = CRLMODULE_LITE_NAME,
			 .flags = I2C_CLIENT_TEN,
			 .addr = ADV7481_I2C_ADDRESS,
			 .platform_data = &adv7481_eval_pdata_lite,
		},
		.i2c_adapter_id = 2,
	}
};

static struct crlmodule_lite_platform_data adv7481b_eval_pdata_lite = {
	.xshutdown = GPIO_BASE + 63,
	.lanes = ADV7481_LANES,
	.ext_clk = 24000000,
	.op_sys_clock = (uint64_t []){600000000},
	.module_name = "ADV7481B_EVAL"
};
static struct ipu_isys_csi2_config adv7481b_eval_csi2_cfg = {
	.nlanes = ADV7481_LANES,
	.port = 4,
};
static struct ipu_isys_subdev_info adv7481b_eval_crl_sd_lite = {
	.csi2 = &adv7481b_eval_csi2_cfg,
	.i2c = {
		.board_info = {
			 .type = CRLMODULE_LITE_NAME,
			 .flags = I2C_CLIENT_TEN,
			 .addr = ADV7481B_I2C_ADDRESS,
			 .platform_data = &adv7481b_eval_pdata_lite,
		},
		.i2c_adapter_id = 2,
	}
};
#endif

#ifdef CONFIG_INTEL_IPU4_MAGNA_TI964

#define MAGNA_TI964_MIPI_LANES		4
#define TI964_I2C_ADDRESS		0x3d
static struct crlmodule_lite_platform_data magna_ti964_pdata = {
	.xshutdown = GPIO_BASE + 63,
	.lanes = MAGNA_TI964_MIPI_LANES,
	.ext_clk = 24000000,
        .op_sys_clock = (uint64_t []){ 400000000 },
	.module_name = "MAGNA_TI964",
};
static struct ipu_isys_csi2_config magna_ti964_csi2_cfg = {
	.nlanes = MAGNA_TI964_MIPI_LANES,
	.port = 0,
};
static struct ipu_isys_subdev_info magna_ti964_crl_sd = {
	.csi2 = &magna_ti964_csi2_cfg,
	.i2c = {
		.board_info = {
			I2C_BOARD_INFO(CRLMODULE_LITE_NAME, TI964_I2C_ADDRESS),
			.platform_data = &magna_ti964_pdata,
               },
		.i2c_adapter_id = 0,
       }
};

#endif

/*
 * Map buttress output sensor clocks to sensors -
 * this should be coming from ACPI
 */
struct ipu_isys_clk_mapping p_mapping[] = {
	{ CLKDEV_INIT("0-003d", NULL, NULL), "OSC_CLK_OUT1" },
	{ CLKDEV_INIT("0-00e1", NULL, NULL), "OSC_CLK_OUT0" },
	{ CLKDEV_INIT("0-00e0", NULL, NULL), "OSC_CLK_OUT1" },
	{ CLKDEV_INIT("2-a0e0", NULL, NULL), "OSC_CLK_OUT0" },
	{ CLKDEV_INIT("2-a0e2", NULL, NULL), "OSC_CLK_OUT1" },
	{ CLKDEV_INIT(NULL, NULL, NULL), NULL }
};

static struct ipu_isys_subdev_pdata pdata = {
	.subdevs = (struct ipu_isys_subdev_info *[]) {
#ifdef CONFIG_INTEL_IPU4_ADV7481
		&adv7481_cvbs_crl_sd_lite,
		&adv7481_hdmi_crl_sd_lite,
#endif
#ifdef CONFIG_INTEL_IPU4_ADV7481_EVAL
		&adv7481_eval_crl_sd_lite,
		&adv7481b_eval_crl_sd_lite,
#endif
#ifdef CONFIG_INTEL_IPU4_MAGNA_TI964
               &magna_ti964_crl_sd,
#endif
		NULL,
	},
	.clk_map = p_mapping,
};

static void ipu4_quirk(struct pci_dev *pci_dev)
{
	pci_dev->dev.platform_data = &pdata;
}

DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, IPU_PCI_ID,
			ipu4_quirk);
