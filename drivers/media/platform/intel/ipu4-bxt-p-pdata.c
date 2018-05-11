// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2015 - 2018 Intel Corporation

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/pci.h>

#include <media/ipu-isys.h>
#include <media/crlmodule.h>
#include <media/ti964.h>
#include <media/max9286.h>
#include "ipu.h"
#include <media/dw9714.h>
#define GPIO_BASE		422
#ifdef CONFIG_INTEL_IPU4_OV13858

#define DW9714_VCM_ADDR        0x0c
// for port 1
static struct dw9714_platform_data  dw9714_pdata = {
       .gpio_xsd = GPIO_BASE + 67,
};

static struct ipu_isys_subdev_info dw9714_common_sd = {
       .i2c = {
               .board_info = {
                       I2C_BOARD_INFO(DW9714_NAME,  DW9714_VCM_ADDR),
                       .platform_data = &dw9714_pdata,
               },
               .i2c_adapter_id = 4,
       }
};

// for port2
static struct dw9714_platform_data  dw9714_pdata_1 = {
       .gpio_xsd = GPIO_BASE + 64,
};

static struct ipu_isys_subdev_info dw9714_common_sd_1 = {
       .i2c = {
               .board_info = {
                       I2C_BOARD_INFO(DW9714_NAME,  DW9714_VCM_ADDR),
                       .platform_data = &dw9714_pdata_1,
               },
               .i2c_adapter_id = 2,
       }
};

#define OV13858_LANES           4
#define OV13858_I2C_ADDRESS     0x10

//for port1
static struct crlmodule_platform_data ov13858_pdata = {
	.xshutdown = GPIO_BASE + 67,
	.lanes = OV13858_LANES,
	.ext_clk = 19200000,
	.op_sys_clock = (uint64_t []){ 54000000 },
	.module_name = "OV13858",
	.id_string = "0xd8 0x55",
};

static struct ipu_isys_csi2_config ov13858_csi2_cfg = {
	.nlanes = OV13858_LANES,
	.port = 4,
};

static struct ipu_isys_subdev_info ov13858_crl_sd = {
	.csi2 = &ov13858_csi2_cfg,
	.i2c = {
		.board_info = {
			I2C_BOARD_INFO(CRLMODULE_NAME, OV13858_I2C_ADDRESS),
			.platform_data = &ov13858_pdata,
		},
		.i2c_adapter_id = 4,
	},
/*	.acpiname = "i2c-OVTIF858:00", */
};

// for port2
static struct crlmodule_platform_data ov13858_pdata_2 = {
	.xshutdown = GPIO_BASE + 64,
	.lanes = OV13858_LANES,
	.ext_clk = 19200000,
	.op_sys_clock = (uint64_t []){ 54000000 },
	.module_name = "OV13858-2",
	.id_string = "0xd8 0x55",
};

static struct ipu_isys_csi2_config ov13858_csi2_cfg_2 = {
	.nlanes = OV13858_LANES,
	.port = 0,
};

static struct ipu_isys_subdev_info ov13858_crl_sd_2 = {
	.csi2 = &ov13858_csi2_cfg_2,
	.i2c = {
		.board_info = {
			I2C_BOARD_INFO(CRLMODULE_NAME, OV13858_I2C_ADDRESS),
			.platform_data = &ov13858_pdata_2,
		},
		.i2c_adapter_id = 2,
	},
};

#endif

#ifdef CONFIG_INTEL_IPU4_OV2740
#define OV2740_LANES		2
#define OV2740_I2C_ADDRESS	0x36
static struct crlmodule_platform_data ov2740_pdata = {
	.xshutdown = GPIO_BASE + 64,
	.lanes = OV2740_LANES,
	.ext_clk = 19200000,
	.op_sys_clock = (uint64_t []){ 72000000 },
	.module_name = "INT3474",
	.id_string = "0x27 0x40",
};

static struct ipu_isys_csi2_config ov2740_csi2_cfg = {
	.nlanes = OV2740_LANES,
	.port = 0,
};

static struct ipu_isys_subdev_info ov2740_crl_sd = {
	.csi2 = &ov2740_csi2_cfg,
	.i2c = {
		.board_info = {
			 I2C_BOARD_INFO(CRLMODULE_NAME, OV2740_I2C_ADDRESS),
			.platform_data = &ov2740_pdata,
		},
		.i2c_adapter_id = 2,
	}
};
#endif

#ifdef CONFIG_INTEL_IPU4_IMX185
#define IMX185_LANES		4
#define IMX185_I2C_ADDRESS	0x1a

static struct crlmodule_platform_data imx185_pdata = {
	.xshutdown = GPIO_BASE + 71,
	.lanes = IMX185_LANES,
	.ext_clk = 27000000,
	.op_sys_clock = (uint64_t []){ 55687500, 111375000,
					111375000, 222750000 },
	.module_name = "IMX185",
	.id_string = "0x1 0x85",
};

static struct ipu_isys_csi2_config imx185_csi2_cfg = {
	.nlanes = IMX185_LANES,
	.port = 0,
};

static struct ipu_isys_subdev_info imx185_crl_sd = {
	.csi2 = &imx185_csi2_cfg,
	.i2c = {
		.board_info = {
			 I2C_BOARD_INFO(CRLMODULE_NAME, IMX185_I2C_ADDRESS),
			.platform_data = &imx185_pdata,
		},
		.i2c_adapter_id = 2,
	}
};
#endif

#ifdef CONFIG_INTEL_IPU4_IMX185_LI
#define IMX185_LI_LANES		4
#define IMX185_LI_I2C_ADDRESS	0x1a

static struct crlmodule_platform_data imx185_li_pdata = {
	.xshutdown = GPIO_BASE + 67,
	.lanes = IMX185_LI_LANES,
	.ext_clk = 27000000,
	.op_sys_clock = (uint64_t []){ 55687500, 111375000,
					111375000, 222750000 },
	.module_name = "IMX185",
	.id_string = "0x1 0x85",
};

static struct ipu_isys_csi2_config imx185_li_csi2_cfg = {
	.nlanes = IMX185_LI_LANES,
	.port = 4,
};

static struct ipu_isys_subdev_info imx185_li_crl_sd = {
	.csi2 = &imx185_li_csi2_cfg,
	.i2c = {
		.board_info = {
			 I2C_BOARD_INFO(CRLMODULE_NAME, IMX185_LI_I2C_ADDRESS),
			.platform_data = &imx185_li_pdata,
		},
		.i2c_adapter_id = 4,
	}
};
#endif

#ifdef CONFIG_INTEL_IPU4_AR023Z
#define AR023Z_MIPI_LANES	2
/* Toshiba TC358778 Parallel-MIPI Bridge */
#define TC358778_I2C_ADDRESS	0x0e

static struct crlmodule_platform_data ar023z_pdata = {
	.xshutdown = GPIO_BASE + 64,
	.lanes = AR023Z_MIPI_LANES,
	.ext_clk = 27000000,
	.op_sys_clock = (uint64_t []){317250000},
	.module_name = "AR023Z",
	.id_string = "0x4401 0x64",
};

static struct ipu_isys_csi2_config ar023z_csi2_cfg = {
	.nlanes = AR023Z_MIPI_LANES,
	.port = 0,
};

static struct ipu_isys_subdev_info ar023z_crl_sd = {
	.csi2 = &ar023z_csi2_cfg,
	.i2c = {
		.board_info = {
			I2C_BOARD_INFO(CRLMODULE_NAME, TC358778_I2C_ADDRESS),
			.platform_data = &ar023z_pdata,
		},
		.i2c_adapter_id = 2,
	}
};

static struct crlmodule_platform_data ar023z_b_pdata = {
	.xshutdown = GPIO_BASE + 67,
	.lanes = AR023Z_MIPI_LANES,
	.ext_clk = 27000000,
	.op_sys_clock = (uint64_t []){317250000},
	.module_name = "AR023Z",
	.id_string = "0x4401 0x64",
};

static struct ipu_isys_csi2_config ar023z_b_csi2_cfg = {
	.nlanes = AR023Z_MIPI_LANES,
	.port = 4,
};

static struct ipu_isys_subdev_info ar023z_b_crl_sd = {
	.csi2 = &ar023z_b_csi2_cfg,
	.i2c = {
		.board_info = {
			I2C_BOARD_INFO(CRLMODULE_NAME, TC358778_I2C_ADDRESS),
			.platform_data = &ar023z_b_pdata,
		},
		.i2c_adapter_id = 4,
	}
};
#endif

#ifdef CONFIG_INTEL_IPU4_IMX477
#define IMX477_LANES	   2

#define IMX477_I2C_ADDRESS 0x10

static struct crlmodule_platform_data imx477_pdata_master = {
	.xshutdown = GPIO_BASE + 64,
	.lanes = IMX477_LANES,
	.ext_clk = 19200000,
	.op_sys_clock = (uint64_t []){600000000},
	.module_name = "IMX477-MASTER",
	.id_string = "0x4 0x77",
};

static struct ipu_isys_csi2_config imx477_csi2_cfg_master = {
	.nlanes = IMX477_LANES,
	.port = 0,
};

static struct ipu_isys_subdev_info imx477_crl_sd_master = {
	.csi2 = &imx477_csi2_cfg_master,
	.i2c = {
		.board_info = {
			I2C_BOARD_INFO(CRLMODULE_NAME, IMX477_I2C_ADDRESS),
			.platform_data = &imx477_pdata_master,
		},
		.i2c_adapter_id = 2,
	}
};

static struct crlmodule_platform_data imx477_pdata_slave_1 = {
	.xshutdown = GPIO_BASE + 67,
	.lanes = IMX477_LANES,
	.ext_clk = 19200000,
	.op_sys_clock = (uint64_t []){600000000},
	.module_name = "IMX477-SLAVE-1",
	.id_string = "0x4 0x77",
};

static struct ipu_isys_csi2_config imx477_csi2_cfg_slave_1 = {
	.nlanes = IMX477_LANES,
	.port = 4,
};

static struct ipu_isys_subdev_info imx477_crl_sd_slave_1 = {
	.csi2 = &imx477_csi2_cfg_slave_1,
	.i2c = {
		.board_info = {
			I2C_BOARD_INFO(CRLMODULE_NAME, IMX477_I2C_ADDRESS),
			.platform_data = &imx477_pdata_slave_1,
		},
		.i2c_adapter_id = 4,
	}
};
#endif

#ifdef CONFIG_INTEL_IPU4_IMX274

#define IMX274_LANES		4
#define IMX274_I2C_ADDRESS	0x1a

static struct crlmodule_platform_data imx274_pdata = {
	.xshutdown = GPIO_BASE + 64,
	.lanes = IMX274_LANES,
	.ext_clk = 24000000,
	.op_sys_clock = (uint64_t []){720000000},
	.module_name = "IMX274",
	.id_string = "0x6 0x9",
};

static struct ipu_isys_csi2_config imx274_csi2_cfg = {
	.nlanes = IMX274_LANES,
	.port = 0,
};

static struct ipu_isys_subdev_info imx274_crl_sd = {
	.csi2 = &imx274_csi2_cfg,
	.i2c = {
		.board_info = {
			I2C_BOARD_INFO(CRLMODULE_NAME, IMX274_I2C_ADDRESS),
			.platform_data = &imx274_pdata
		},
		.i2c_adapter_id = 2,
	}
};

static struct crlmodule_platform_data imx274_b_pdata = {
	.xshutdown = GPIO_BASE + 67,
	.lanes = IMX274_LANES,
	.ext_clk = 24000000,
	.op_sys_clock = (uint64_t []){720000000},
	.module_name = "IMX274",
	.id_string = "0x6 0x9",
};

static struct ipu_isys_csi2_config imx274_b_csi2_cfg = {
	.nlanes = IMX274_LANES,
	.port = 4,
};

static struct ipu_isys_subdev_info imx274_b_crl_sd = {
	.csi2 = &imx274_b_csi2_cfg,
	.i2c = {
		.board_info = {
			I2C_BOARD_INFO(CRLMODULE_NAME, IMX274_I2C_ADDRESS),
			.platform_data = &imx274_b_pdata
		},
		.i2c_adapter_id = 4,
	}
};
#endif

#ifdef CONFIG_INTEL_IPU4_IMX290

#define IMX290_LANES		4
#define IMX290_I2C_ADDRESS	0x1a

static struct crlmodule_platform_data imx290_pdata = {
	.xshutdown = GPIO_BASE + 64,
	.lanes = IMX290_LANES,
	.ext_clk = 37125000,
	.op_sys_clock = (uint64_t []){222750000, 445500000},
	.module_name = "IMX290",
	.id_string = "0x2 0x90",
};

static struct ipu_isys_csi2_config imx290_csi2_cfg = {
	.nlanes = IMX290_LANES,
	.port = 0,
};

static struct ipu_isys_subdev_info imx290_crl_sd = {
	.csi2 = &imx290_csi2_cfg,
	.i2c = {
		.board_info = {
			I2C_BOARD_INFO(CRLMODULE_NAME, IMX290_I2C_ADDRESS),
			.platform_data = &imx290_pdata
		},
		.i2c_adapter_id = 2,
	}
};
#endif

#ifdef CONFIG_INTEL_IPU4_OV13860

#define OV13860_LANES		2
#define OV13860_I2C_ADDRESS	0x10

static struct crlmodule_platform_data ov13860_pdata = {
	.xshutdown = GPIO_BASE + 71,
	.lanes = OV13860_LANES,
	.ext_clk = 24000000,
	.op_sys_clock = (uint64_t []){ 600000000, 300000000},
	.module_name = "OV13860"
};

static struct ipu_isys_csi2_config ov13860_csi2_cfg = {
	.nlanes = OV13860_LANES,
	.port = 0,
};

static struct ipu_isys_subdev_info ov13860_crl_sd = {
	.csi2 = &ov13860_csi2_cfg,
	.i2c = {
		.board_info = {
			 I2C_BOARD_INFO(CRLMODULE_NAME, OV13860_I2C_ADDRESS),
			 .platform_data = &ov13860_pdata,
		},
		.i2c_adapter_id = 2,
	}
};
#endif

#ifdef CONFIG_INTEL_IPU4_OV9281

#define OV9281_LANES		2
#define OV9281_I2C_ADDRESS	0x10

static struct crlmodule_platform_data ov9281_pdata = {
	.xshutdown = GPIO_BASE + 71,
	.lanes = OV9281_LANES,
	.ext_clk = 24000000,
	.op_sys_clock = (uint64_t []){400000000},
	.module_name = "OV9281",
	.id_string = "0x92 0x81",
};

static struct ipu_isys_csi2_config ov9281_csi2_cfg = {
	.nlanes = OV9281_LANES,
	.port = 0,
};

static struct ipu_isys_subdev_info ov9281_crl_sd = {
	.csi2 = &ov9281_csi2_cfg,
	.i2c = {
		.board_info = {
			 I2C_BOARD_INFO(CRLMODULE_NAME, OV9281_I2C_ADDRESS),
			 .platform_data = &ov9281_pdata,
		},
		.i2c_adapter_id = 0,
	}
};
#endif

#if IS_ENABLED(CONFIG_VIDEO_BU64295)

#define BU64295_VCM_ADDR	0x0c
#define BU64295_NAME		"bu64295"

static struct ipu_isys_subdev_info bu64295_sd = {
	.i2c = {
		.board_info = {
			I2C_BOARD_INFO(BU64295_NAME,  BU64295_VCM_ADDR),
		},
		.i2c_adapter_id = 2,
	}
};
#endif

#ifdef CONFIG_INTEL_IPU4_ADV7481

#define ADV7481_LANES		4
#define ADV7481_I2C_ADDRESS	0xe0
#define ADV7481B_I2C_ADDRESS	0xe2

static struct crlmodule_platform_data adv7481_pdata = {
	.xshutdown = GPIO_BASE + 63,
	.lanes = ADV7481_LANES,
	.ext_clk = 24000000,
	.op_sys_clock = (uint64_t []){600000000},
	.module_name = "ADV7481"
};

static struct ipu_isys_csi2_config adv7481_csi2_cfg = {
	.nlanes = ADV7481_LANES,
	.port = 0,
};

static struct ipu_isys_subdev_info adv7481_crl_sd = {
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
#endif

#ifdef CONFIG_INTEL_IPU4_ADV7481_EVAL

#define ADV7481_LANES		4
#define ADV7481_I2C_ADDRESS	0xe0
#define ADV7481B_I2C_ADDRESS	0xe2

static struct crlmodule_platform_data adv7481_eval_pdata = {
	.xshutdown = GPIO_BASE + 63,
	.lanes = ADV7481_LANES,
	.ext_clk = 24000000,
	.op_sys_clock = (uint64_t []){600000000},
	.module_name = "ADV7481_EVAL"
};

static struct ipu_isys_csi2_config adv7481_eval_csi2_cfg = {
	.nlanes = ADV7481_LANES,
	.port = 0,
};

static struct ipu_isys_subdev_info adv7481_eval_crl_sd = {
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

static struct crlmodule_platform_data adv7481b_eval_pdata = {
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

static struct ipu_isys_subdev_info adv7481b_eval_crl_sd = {
	.csi2 = &adv7481b_eval_csi2_cfg,
	.i2c = {
		.board_info = {
			 .type = CRLMODULE_NAME,
			 .flags = I2C_CLIENT_TEN,
			 .addr = ADV7481B_I2C_ADDRESS,
			 .platform_data = &adv7481b_eval_pdata,
		},
		.i2c_adapter_id = 2,
	}
};
#endif

#if IS_ENABLED(CONFIG_VIDEO_AGGREGATOR_STUB)

#define VIDEO_AGGRE_LANES	4
#define VIDEO_AGGRE_I2C_ADDRESS	0x3b
#define VIDEO_AGGRE_B_I2C_ADDRESS	0x3c

static struct ipu_isys_csi2_config video_aggre_csi2_cfg = {
	.nlanes = VIDEO_AGGRE_LANES,
	.port = 0,
};

static struct ipu_isys_subdev_info video_aggre_stub_sd = {
	.csi2 = &video_aggre_csi2_cfg,
	.i2c = {
		.board_info = {
			 .type = "video-aggre",
			 .addr = VIDEO_AGGRE_I2C_ADDRESS,
		},
		.i2c_adapter_id = 2,
	}
};

static struct ipu_isys_csi2_config video_aggre_b_csi2_cfg = {
	.nlanes = VIDEO_AGGRE_LANES,
	.port = 4,
};

static struct ipu_isys_subdev_info video_aggre_b_stub_sd = {
	.csi2 = &video_aggre_b_csi2_cfg,
	.i2c = {
		.board_info = {
			 .type = "video-aggre",
			 .addr = VIDEO_AGGRE_B_I2C_ADDRESS,
		},
		.i2c_adapter_id = 2,
	}
};
#endif

#if IS_ENABLED(CONFIG_INTEL_IPU4_MAGNA)
#define MAGNA_LANES		4
#define MAGNA_PHY_ADDR	0x60 /* 0x30 for 7bit addr */
#define MAGNA_ADDRESS_A	0x61

static struct crlmodule_platform_data magna_pdata = {
	.lanes = MAGNA_LANES,
	.ext_clk = 24000000,
	.op_sys_clock = (uint64_t []){ 400000000 },
	.module_name = "MAGNA",
	.id_string = "0xa6 0x35",
	/*
	 * The pin number of xshutdown will be determined
	 * and replaced inside TI964 driver.
	 * The number here stands for which GPIO to connect with.
	 * 1 means to connect sensor xshutdown to GPIO1
	 */
	.xshutdown = 1,
	/*
	 * this flags indicates the expected polarity for the LineValid
	 * indication received in Raw mode.
	 * 1 means LineValid is high for the duration of the video frame.
	 */
	.high_framevalid_flags = 1,
};
#endif

#if IS_ENABLED(CONFIG_INTEL_IPU4_OV10635)
#define OV10635_LANES		4
#define OV10635_I2C_PHY_ADDR	0x60 /* 0x30 for 7bit addr */
#define OV10635A_I2C_ADDRESS	0x61
#define OV10635B_I2C_ADDRESS	0x62
#define OV10635C_I2C_ADDRESS	0x63
#define OV10635D_I2C_ADDRESS	0x64

static struct crlmodule_platform_data ov10635_pdata = {
	.lanes = OV10635_LANES,
	.ext_clk = 24000000,
	.op_sys_clock = (uint64_t []){ 400000000 },
	.module_name = "OV10635",
	.id_string = "0xa6 0x35",
	/*
	 * The pin number of xshutdown will be determined
	 * and replaced inside TI964 driver.
	 * The number here stands for which GPIO to connect with.
	 * 1 means to connect sensor xshutdown to GPIO1
	 */
	.xshutdown = 1,
};
#endif

#if IS_ENABLED(CONFIG_INTEL_IPU4_OV10640)
#define OV10640_LANES			4
#define OV10640_I2C_PHY_ADDR	0x60 /* 0x30 for 7bit addr */
#define OV10640A_I2C_ADDRESS	0x61
#define OV10640B_I2C_ADDRESS	0x62
#define OV10640C_I2C_ADDRESS	0x63
#define OV10640D_I2C_ADDRESS	0x64

static struct crlmodule_platform_data ov10640_pdata = {
	.lanes = OV10640_LANES,
	.ext_clk = 24000000,
	.op_sys_clock = (uint64_t []){ 400000000 },
	.module_name = "OV10640",
	.id_string = "0xa6 0x40",
	/*
	 * The pin number of xshutdown will be determined
	 * and replaced inside TI964 driver.
	 * The number here stands for which GPIO to connect with.
	 * 1 means to connect sensor xshutdown to GPIO1
	 */
	.xshutdown = 1,
};
#endif

#if IS_ENABLED(CONFIG_VIDEO_TI964)
#define TI964_I2C_ADAPTER	0
#define TI964_I2C_ADAPTER_2	7
#define TI964_I2C_ADDRESS	0x3d
#define TI964_LANES		4

static struct ipu_isys_csi2_config ti964_csi2_cfg = {
	.nlanes = TI964_LANES,
	.port = 0,
};

static struct ipu_isys_csi2_config ti964_csi2_cfg_2 = {
	.nlanes = TI964_LANES,
	.port = 4,
};

static struct ti964_subdev_info ti964_subdevs[] = {
#ifdef CONFIG_INTEL_IPU4_OV10635
	{
		.board_info = {
			.type = CRLMODULE_NAME,
			.addr = OV10635A_I2C_ADDRESS,
			.platform_data = &ov10635_pdata,
		},
		.i2c_adapter_id = TI964_I2C_ADAPTER,
		.rx_port = 0,
		.phy_i2c_addr = OV10635_I2C_PHY_ADDR,
	},
	{
		.board_info = {
			.type = CRLMODULE_NAME,
			.addr = OV10635B_I2C_ADDRESS,
			.platform_data = &ov10635_pdata,
		},
		.i2c_adapter_id = TI964_I2C_ADAPTER,
		.rx_port = 1,
		.phy_i2c_addr = OV10635_I2C_PHY_ADDR,
	},
	{
		.board_info = {
			.type = CRLMODULE_NAME,
			.addr = OV10635C_I2C_ADDRESS,
			.platform_data = &ov10635_pdata,
		},
		.i2c_adapter_id = TI964_I2C_ADAPTER,
		.rx_port = 2,
		.phy_i2c_addr = OV10635_I2C_PHY_ADDR,
	},
	{
		.board_info = {
			.type = CRLMODULE_NAME,
			.addr = OV10635D_I2C_ADDRESS,
			.platform_data = &ov10635_pdata,
		},
		.i2c_adapter_id = TI964_I2C_ADAPTER,
		.rx_port = 3,
		.phy_i2c_addr = OV10635_I2C_PHY_ADDR,
	},
#endif
#ifdef CONFIG_INTEL_IPU4_OV10640
	{
		.board_info = {
			.type = CRLMODULE_NAME,
			.addr = OV10640A_I2C_ADDRESS,
			.platform_data = &ov10640_pdata,
		},
		.i2c_adapter_id = TI964_I2C_ADAPTER,
		.rx_port = 0,
		.phy_i2c_addr = OV10640_I2C_PHY_ADDR,
	},
	{
		.board_info = {
			.type = CRLMODULE_NAME,
			.addr = OV10640B_I2C_ADDRESS,
			.platform_data = &ov10640_pdata,
		},
		.i2c_adapter_id = TI964_I2C_ADAPTER,
		.rx_port = 1,
		.phy_i2c_addr = OV10640_I2C_PHY_ADDR,
	},
	{
		.board_info = {
			.type = CRLMODULE_NAME,
			.addr = OV10640C_I2C_ADDRESS,
			.platform_data = &ov10640_pdata,
		},
		.i2c_adapter_id = TI964_I2C_ADAPTER,
		.rx_port = 2,
		.phy_i2c_addr = OV10640_I2C_PHY_ADDR,
	},
	{
		.board_info = {
			.type = CRLMODULE_NAME,
			.addr = OV10640D_I2C_ADDRESS,
			.platform_data = &ov10640_pdata,
		},
		.i2c_adapter_id = TI964_I2C_ADAPTER,
		.rx_port = 3,
		.phy_i2c_addr = OV10640_I2C_PHY_ADDR,
	},
#endif
#ifdef CONFIG_INTEL_IPU4_MAGNA
	{
		.board_info = {
			.type = CRLMODULE_NAME,
			.addr = MAGNA_ADDRESS_A,
			.platform_data = &magna_pdata,
		},
		.i2c_adapter_id = TI964_I2C_ADAPTER,
		.rx_port = 0,
		.phy_i2c_addr = MAGNA_PHY_ADDR,
	},
#endif
};

static struct ti964_subdev_info ti964_subdevs_2[] = {
#ifdef CONFIG_INTEL_IPU4_OV10635
	{
		.board_info = {
			.type = CRLMODULE_NAME,
			.addr = OV10635A_I2C_ADDRESS,
			.platform_data = &ov10635_pdata,
		},
		.i2c_adapter_id = TI964_I2C_ADAPTER_2,
		.rx_port = 0,
		.phy_i2c_addr = OV10635_I2C_PHY_ADDR,
	},
	{
		.board_info = {
			.type = CRLMODULE_NAME,
			.addr = OV10635B_I2C_ADDRESS,
			.platform_data = &ov10635_pdata,
		},
		.i2c_adapter_id = TI964_I2C_ADAPTER_2,
		.rx_port = 1,
		.phy_i2c_addr = OV10635_I2C_PHY_ADDR,
	},
	{
		.board_info = {
			.type = CRLMODULE_NAME,
			.addr = OV10635C_I2C_ADDRESS,
			.platform_data = &ov10635_pdata,
		},
		.i2c_adapter_id = TI964_I2C_ADAPTER_2,
		.rx_port = 2,
		.phy_i2c_addr = OV10635_I2C_PHY_ADDR,
	},
	{
		.board_info = {
			.type = CRLMODULE_NAME,
			.addr = OV10635D_I2C_ADDRESS,
			.platform_data = &ov10635_pdata,
		},
		.i2c_adapter_id = TI964_I2C_ADAPTER_2,
		.rx_port = 3,
		.phy_i2c_addr = OV10635_I2C_PHY_ADDR,
	},
#endif
#ifdef CONFIG_INTEL_IPU4_OV10640
	{
		.board_info = {
			.type = CRLMODULE_NAME,
			.addr = OV10640A_I2C_ADDRESS,
			.platform_data = &ov10640_pdata,
		},
		.i2c_adapter_id = TI964_I2C_ADAPTER_2,
		.rx_port = 0,
		.phy_i2c_addr = OV10640_I2C_PHY_ADDR,
	},
	{
		.board_info = {
			.type = CRLMODULE_NAME,
			.addr = OV10640B_I2C_ADDRESS,
			.platform_data = &ov10640_pdata,
		},
		.i2c_adapter_id = TI964_I2C_ADAPTER_2,
		.rx_port = 1,
		.phy_i2c_addr = OV10640_I2C_PHY_ADDR,
	},
	{
		.board_info = {
			.type = CRLMODULE_NAME,
			.addr = OV10640C_I2C_ADDRESS,
			.platform_data = &ov10640_pdata,
		},
		.i2c_adapter_id = TI964_I2C_ADAPTER_2,
		.rx_port = 2,
		.phy_i2c_addr = OV10640_I2C_PHY_ADDR,
	},
	{
		.board_info = {
			.type = CRLMODULE_NAME,
			.addr = OV10640D_I2C_ADDRESS,
			.platform_data = &ov10640_pdata,
		},
		.i2c_adapter_id = TI964_I2C_ADAPTER_2,
		.rx_port = 3,
		.phy_i2c_addr = OV10640_I2C_PHY_ADDR,
	},
#endif
};

static struct ti964_pdata ti964_pdata = {
	.subdev_info = ti964_subdevs,
	.subdev_num = ARRAY_SIZE(ti964_subdevs),
	.reset_gpio = GPIO_BASE + 63,
};

static struct ipu_isys_subdev_info ti964_sd = {
	.csi2 = &ti964_csi2_cfg,
	.i2c = {
		.board_info = {
			 .type = "ti964",
			 .addr = TI964_I2C_ADDRESS,
			 .platform_data = &ti964_pdata,
		},
		.i2c_adapter_id = TI964_I2C_ADAPTER,
	}
};

static struct ti964_pdata ti964_pdata_2 = {
	.subdev_info = ti964_subdevs_2,
	.subdev_num = ARRAY_SIZE(ti964_subdevs_2),
	.reset_gpio = GPIO_BASE + 66,
};

static struct ipu_isys_subdev_info ti964_sd_2 = {
	.csi2 = &ti964_csi2_cfg_2,
	.i2c = {
		.board_info = {
			 .type = "ti964",
			 .addr = TI964_I2C_ADDRESS,
			 .platform_data = &ti964_pdata_2,
		},
		.i2c_adapter_id = TI964_I2C_ADAPTER_2,
	}
};
#endif

#ifdef CONFIG_INTEL_IPU4_OV2775
#define OV2775_LANES	   2
#define OV2775_I2C_ADAPTER     3
#define OV2775_I2C_ADDRESS     0x6C

static struct crlmodule_platform_data ov2775_pdata = {
	.lanes = OV2775_LANES,
	.ext_clk = 24000000,
	.op_sys_clock = (uint64_t []){ 480000000 },
	.module_name = "OV2775",
	.id_string = "0x27 0x70",
	/*
	 * The pin number of xshutdown will be determined
	 * and replaced inside TI960 driver.
	 * The number here stands for which GPIO to connect with.
	 * 1 means to connect sensor xshutdown to GPIO1
	 */
	.xshutdown = 1,
};

static struct ipu_isys_csi2_config ov2775_csi2_cfg = {
	.nlanes = OV2775_LANES,
	.port = 4,
};

static struct ipu_isys_subdev_info ov2775_crl_sd = {
	.csi2 = &ov2775_csi2_cfg,
	.i2c = {
		.board_info = {
			I2C_BOARD_INFO(CRLMODULE_NAME, OV2775_I2C_ADDRESS),
			.platform_data = &ov2775_pdata,
		},
		.i2c_adapter_id = OV2775_I2C_ADAPTER,
	}
};
#endif

#ifdef CONFIG_INTEL_IPU4_AR0231AT
#define AR0231AT_LANES            4
#define AR0231ATA_I2C_ADDRESS      0x11
#define AR0231ATB_I2C_ADDRESS      0x12
#define AR0231ATC_I2C_ADDRESS      0x13
#define AR0231ATD_I2C_ADDRESS      0x14

static struct crlmodule_platform_data ar0231at_pdata = {
	.lanes = AR0231AT_LANES,
	.ext_clk = 27000000,
	.op_sys_clock = (uint64_t[]){ 87750000 },
	.module_name = "AR0231AT",
};
#endif

#if IS_ENABLED(CONFIG_VIDEO_MAX9286)
#define DS_MAX9286_LANES                4
#define DS_MAX9286_I2C_ADAPTER          4
#define DS_MAX9286_I2C_ADDRESS          0x48

static struct ipu_isys_csi2_config max9286_csi2_cfg = {
	.nlanes = DS_MAX9286_LANES,
	.port = 4,
};

static struct max9286_subdev_i2c_info max9286_subdevs[] = {
#ifdef CONFIG_INTEL_IPU4_AR0231AT
		{
			.board_info = {
				.type = CRLMODULE_NAME,
				.addr = AR0231ATA_I2C_ADDRESS,
				.platform_data = &ar0231at_pdata,
			},
			.i2c_adapter_id = DS_MAX9286_I2C_ADAPTER,
		},
		{
			.board_info = {
				.type = CRLMODULE_NAME,
				.addr = AR0231ATB_I2C_ADDRESS,
				.platform_data = &ar0231at_pdata,
			},
			.i2c_adapter_id = DS_MAX9286_I2C_ADAPTER,
		},
		{
			.board_info = {
				.type = CRLMODULE_NAME,
				.addr = AR0231ATC_I2C_ADDRESS,
				.platform_data = &ar0231at_pdata,
			},
			.i2c_adapter_id = DS_MAX9286_I2C_ADAPTER,
		},
		{
			.board_info = {
				.type = CRLMODULE_NAME,
				.addr = AR0231ATD_I2C_ADDRESS,
				.platform_data = &ar0231at_pdata,
			},
			.i2c_adapter_id = DS_MAX9286_I2C_ADAPTER,
		},
#endif
};

static struct max9286_pdata max9286_pdata = {
	.subdev_info = max9286_subdevs,
	.subdev_num = ARRAY_SIZE(max9286_subdevs),
	.reset_gpio = GPIO_BASE + 63,
};

static struct ipu_isys_subdev_info max9286_sd = {
	.csi2 = &max9286_csi2_cfg,
	.i2c = {
		.board_info = {
			.type = "max9286",
			.addr = DS_MAX9286_I2C_ADDRESS,
			.platform_data = &max9286_pdata,
		},
		.i2c_adapter_id = DS_MAX9286_I2C_ADAPTER,
	}
};
#endif

/*
 * Map buttress output sensor clocks to sensors -
 * this should be coming from ACPI
 */
static struct ipu_isys_clk_mapping clk_mapping[] = {
	{ CLKDEV_INIT("2-0036", NULL, NULL), "OSC_CLK_OUT0" },
	{ CLKDEV_INIT("2-001a", NULL, NULL), "OSC_CLK_OUT0" },
	{ CLKDEV_INIT("4-001a", NULL, NULL), "OSC_CLK_OUT1" },
	{ CLKDEV_INIT("2-0010", NULL, NULL), "OSC_CLK_OUT0" },
	{ CLKDEV_INIT("4-0010", NULL, NULL), "OSC_CLK_OUT1" },
	{ CLKDEV_INIT("2-a0e0", NULL, NULL), "OSC_CLK_OUT0" },
	{ CLKDEV_INIT("2-a0e2", NULL, NULL), "OSC_CLK_OUT0" },
	{ CLKDEV_INIT("0-0010", NULL, NULL), "OSC_CLK_OUT0" },
	{ CLKDEV_INIT("2-000e", NULL, NULL), "OSC_CLK_OUT0" },
	{ CLKDEV_INIT("4-000e", NULL, NULL), "OSC_CLK_OUT1" },
	{ CLKDEV_INIT("0-0048", NULL, NULL), "OSC_CLK_OUT0" },
	{ CLKDEV_INIT("4-0048", NULL, NULL), "OSC_CLK_OUT1" },
	{ CLKDEV_INIT(NULL, NULL, NULL), NULL }
};

static struct ipu_isys_subdev_pdata pdata = {
	.subdevs = (struct ipu_isys_subdev_info *[]) {
#ifdef CONFIG_INTEL_IPU4_OV13858
		&ov13858_crl_sd,
		&dw9714_common_sd,
        &ov13858_crl_sd_2,
        &dw9714_common_sd_1,
#endif
#ifdef CONFIG_INTEL_IPU4_OV2740
		&ov2740_crl_sd,
#endif
#ifdef CONFIG_INTEL_IPU4_IMX185
		&imx185_crl_sd,
#endif
#ifdef CONFIG_INTEL_IPU4_IMX185_LI
		&imx185_li_crl_sd,
#endif
#ifdef CONFIG_INTEL_IPU4_AR023Z
		&ar023z_crl_sd,
		&ar023z_b_crl_sd,
#endif
#ifdef CONFIG_INTEL_IPU4_IMX477
		&imx477_crl_sd_slave_1,
		&imx477_crl_sd_master,
#endif
#ifdef CONFIG_INTEL_IPU4_IMX274
		&imx274_crl_sd,
		&imx274_b_crl_sd,
#endif
#ifdef CONFIG_INTEL_IPU4_IMX290
		&imx290_crl_sd,
#endif
#ifdef CONFIG_INTEL_IPU4_OV13860
		&ov13860_crl_sd,
#endif
#ifdef CONFIG_INTEL_IPU4_OV9281
		&ov9281_crl_sd,
#endif
#if IS_ENABLED(CONFIG_VIDEO_BU64295)
		&bu64295_sd,
#endif
#ifdef CONFIG_INTEL_IPU4_ADV7481
		&adv7481_crl_sd,
#endif
#ifdef CONFIG_INTEL_IPU4_ADV7481_EVAL
		&adv7481_eval_crl_sd,
		&adv7481b_eval_crl_sd,
#endif
#if IS_ENABLED(CONFIG_VIDEO_AGGREGATOR_STUB)
		&video_aggre_stub_sd,
		&video_aggre_b_stub_sd,
#endif
#if IS_ENABLED(CONFIG_VIDEO_TI964)
		&ti964_sd,
		&ti964_sd_2,
#endif
#ifdef CONFIG_INTEL_IPU4_OV2775
		&ov2775_crl_sd,
#endif
#if IS_ENABLED(CONFIG_VIDEO_MAX9286)
		&max9286_sd,
#endif
		NULL,
	},
	.clk_map = clk_mapping,
};

static void ipu4_quirk(struct pci_dev *pci_dev)
{
	pci_dev->dev.platform_data = &pdata;
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, IPU_PCI_ID, ipu4_quirk);
