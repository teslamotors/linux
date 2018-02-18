/*
 *
 * Copyright (c) 2013-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <media/imx214.h>
#include <media/ov5693.h>
#include <media/camera_common.h>
#include <media/soc_camera.h>
#include <media/soc_camera_platform.h>
#include <media/tegra_v4l2_camera.h>
/*
 * Soc Camera platform driver for testing
 */
#if IS_ENABLED(CONFIG_SOC_CAMERA_PLATFORM)
static struct platform_device *tegra_soc_pdev;
static int tegra_soc_soc_camera_add(struct soc_camera_device *icd);
static void tegra_soc_soc_camera_del(struct soc_camera_device *icd);

static int tegra_soc_soc_camera_set_capture(
		struct soc_camera_platform_info *info,
		int enable)
{
	/* TODO: probably add clk opertaion here */
	return 0; /* camera sensor always enabled */
}

static struct soc_camera_platform_info tegra_soc_soc_camera_info = {
	.format_name = "RGB4",
	.format_depth = 32,
	.format = {
		.code = V4L2_MBUS_FMT_RGBA8888_4X8_LE,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.field = V4L2_FIELD_NONE,
		.width = 1280,
		.height = 720,
	},
	.set_capture = tegra_soc_soc_camera_set_capture,
};

static struct tegra_camera_platform_data tegra_soc_camera_platform_data = {
	.port                   = TEGRA_CAMERA_PORT_CSI_A,
	.lanes                  = 4,
};

static struct soc_camera_link tegra_soc_soc_camera_link = {
	.bus_id         = 0, /* This must match the .id of tegra_vi01_device */
	.add_device     = tegra_soc_soc_camera_add,
	.del_device     = tegra_soc_soc_camera_del,
	.module_name    = "soc_camera_platform",
	.priv		= &tegra_soc_camera_platform_data,
	.dev_priv	= &tegra_soc_soc_camera_info,
};

static void tegra_soc_soc_camera_release(struct device *dev)
{
	soc_camera_platform_release(&tegra_soc_pdev);
}

static int tegra_soc_soc_camera_add(struct soc_camera_device *icd)
{
	return soc_camera_platform_add(icd, &tegra_soc_pdev,
			&tegra_soc_soc_camera_link,
			tegra_soc_soc_camera_release, 0);
}

static void tegra_soc_soc_camera_del(struct soc_camera_device *icd)
{
	soc_camera_platform_del(icd, tegra_soc_pdev,
				&tegra_soc_soc_camera_link);
}

static struct platform_device tegra_soc_soc_camera_device = {
	.name   = "soc-camera-pdrv",
	.dev    = {
		.platform_data = &tegra_soc_soc_camera_link,
	},
};
#endif

#if IS_ENABLED(CONFIG_SOC_CAMERA_OV23850)
static int tegra_soc_ov23850_power(struct device *dev, int enable)
{
	return 0;
}

static struct i2c_board_info tegra_soc_ov23850_a_camera_i2c_device = {
	I2C_BOARD_INFO("ov23850_v4l2", 0x10),
};

static struct tegra_camera_platform_data tegra_soc_ov23850_a_pdata = {
	.flip_v			= 0,
	.flip_h			= 0,
	.port			= TEGRA_CAMERA_PORT_CSI_A,
	.lanes			= 4,
	.continuous_clk		= 0,
};

static struct soc_camera_link ov23850_a_iclink = {
	.bus_id		= 0, /* This must match the .id of tegra_vi01_device */
	.board_info	= &tegra_soc_ov23850_a_camera_i2c_device,
	.module_name	= "ov23850_v4l2",
	.i2c_adapter_id	= 2, /* VI2 I2C controller */
	.power		= tegra_soc_ov23850_power,
	.priv		= &tegra_soc_ov23850_a_pdata,
};

static struct platform_device tegra_soc_ov23850_a_soc_camera_device = {
	.name	= "soc-camera-pdrv",
	.id	= 0,
	.dev	= {
		.platform_data = &ov23850_a_iclink,
	},
};

static struct i2c_board_info tegra_soc_ov23850_c_camera_i2c_device = {
	I2C_BOARD_INFO("ov23850_v4l2", 0x36),
};

static struct tegra_camera_platform_data tegra_soc_ov23850_c_pdata = {
	.flip_v			= 0,
	.flip_h			= 0,
	.port			= TEGRA_CAMERA_PORT_CSI_C,
	.lanes			= 4,
	.continuous_clk		= 0,
};

static struct soc_camera_link ov23850_c_iclink = {
	.bus_id		= 0, /* This must match the .id of tegra_vi01_device */
	.board_info	= &tegra_soc_ov23850_c_camera_i2c_device,
	.module_name	= "ov23850_v4l2",
	.i2c_adapter_id	= 1, /* VI2 I2C controller */
	.power		= tegra_soc_ov23850_power,
	.priv		= &tegra_soc_ov23850_c_pdata,
};

static struct platform_device tegra_soc_ov23850_c_soc_camera_device = {
	.name	= "soc-camera-pdrv",
	.id	= 1,
	.dev	= {
		.platform_data = &ov23850_c_iclink,
	},
};
#endif

static int __init tegra_soc_camera_init(void)
{
#if IS_ENABLED(CONFIG_SOC_CAMERA_PLATFORM)
	platform_device_register(&tegra_soc_soc_camera_device);
#endif

#if IS_ENABLED(CONFIG_SOC_CAMERA_OV23850)
	if (of_machine_is_compatible("nvidia,tegra186")) {
		platform_device_register(
			&tegra_soc_ov23850_a_soc_camera_device);
		platform_device_register(
			&tegra_soc_ov23850_c_soc_camera_device);
	}
#endif

	return 0;
}

static void __exit tegra_soc_camera_exit(void)
{
}

module_init(tegra_soc_camera_init);
module_exit(tegra_soc_camera_exit);
