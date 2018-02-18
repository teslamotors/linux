/*
 * drivers/video/tegra/dc/sn65dsi85_dsi2lvds.h
 *
 * Copyright (c) 2013-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author:
 *	Tow Wang <toww@nvidia.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __DRIVERS_VIDEO_TEGRA_DC_SN65DSI85_DSI2LVDS_H
#define __DRIVERS_VIDEO_TEGRA_DC_SN65DSI85_DSI2LVDS_H

struct tegra_dc_dsi2lvds_data {
	struct tegra_dc_dsi_data *dsi;
	struct i2c_client *client_i2c;
	struct regmap *regmap;
	bool dsi2lvds_enabled;
	struct mutex lock;
	int en_gpio; /* GPIO */
	int en_gpio_flags;
	int pll_refclk_cfg;
	int dsi_cfg1;
	int dsi_cha_clk_range;
	int dsi_chb_clk_range;
	int	lvds_format;
	int video_cha_line_low;
	int video_cha_line_high;
	int video_chb_line_low;
	int video_chb_line_high;
	int cha_vert_disp_size_low;
	int cha_vert_disp_size_high;
	int h_pulse_width_low;
	int h_pulse_width_high;
	int v_pulse_width_low;
	int v_pulse_width_high;
	int h_back_porch;
	int v_back_porch;
	int h_front_porch;
	int v_front_porch;
	struct dentry *debugdir;
};

#define  SN65DSI85_DEVICE_ID			0x00
#define  SN65DSI85_SOFT_RESET			0X09
#define  SN65DSI85_PLL_REFCLK_CFG		0x0A
#define  SN65DSI85_DIVIDER_MULTIPLIER		0x0B
#define  SN65DSI85_PLL_EN			0x0D
#define  SN65DSI85_DSI_CFG1			0x10
#define  SN65DSI85_DSI_CFG2			0x11
#define  SN65DSI85_DSI_CHA_CLK_RANGE		0x12
#define  SN65DSI85_DSI_CHB_CLK_RANGE		0x13
#define  SN65DSI85_LVDS_FORMAT			0x18
#define  SN65DSI85_LVDS_VOLTAGE			0x19
#define  SN65DSI85_LVDS_TERMINAL		0x1A
#define  SN65DSI85_LVDS_CM_ADJUST		0x1B
#define  SN65DSI85_VIDEO_CHA_LINE_LOW		0x20
#define  SN65DSI85_VIDEO_CHA_LINE_HIGH		0x21
#define  SN65DSI85_VIDEO_CHB_LINE_LOW		0x22
#define  SN65DSI85_VIDEO_CHB_LINE_HIGH		0x23
#define  SN65DSI85_CHA_VERT_DISP_SIZE_LOW	0x24
#define  SN65DSI85_CHA_VERT_DISP_SIZE_HIGH	0x25
#define  SN65DSI85_CHB_VERT_DISP_SIZE_LOW	0x26
#define  SN65DSI85_CHB_VERT_DISP_SIZE_HIGH	0x27
#define  SN65DSI85_CHA_SYNC_DELAY_LOW		0x28
#define  SN65DSI85_CHA_SYNC_DELAY_HIGH		0x29
#define  SN65DSI85_CHB_SYNC_DELAY_LOW		0x2A
#define  SN65DSI85_CHB_SYNC_DELAY_HIGH		0x2B
#define  SN65DSI85_CHA_HSYNC_PULSE_WIDTH_LOW	0x2C
#define  SN65DSI85_CHA_HSYNC_PULSE_WIDTH_HIGH	0x2D
#define  SN65DSI85_CHB_HSYNC_PULSE_WIDTH_LOW	0x2E
#define  SN65DSI85_CHB_HSYNC_PULSE_WIDTH_HIGH	0x2F
#define  SN65DSI85_CHA_VSYNC_PULSE_WIDTH_LOW	0x30
#define  SN65DSI85_CHA_VSYNC_PULSE_WIDTH_HIGH	0x31
#define  SN65DSI85_CHB_VSYNC_PULSE_WIDTH_LOW	0x32
#define  SN65DSI85_CHB_VSYNC_PULSE_WIDTH_HIGH	0x33
#define  SN65DSI85_CHA_HORIZONTAL_BACK_PORCH	0x34
#define  SN65DSI85_CHB_HORIZONTAL_BACK_PORCH	0x35
#define  SN65DSI85_CHA_VERTICAL_BACK_PORCH	0x36
#define  SN65DSI85_CHB_VERTICAL_BACK_PORCH	0x37
#define  SN65DSI85_CHA_HORIZONTAL_FRONT_PORCH	0x38
#define  SN65DSI85_CHB_HORIZONTAL_FRONT_PORCH	0x39
#define  SN65DSI85_CHA_VERTICAL_FRONT_PORCH	0x3A
#define  SN65DSI85_CHB_VERTICAL_FRONT_PORCH	0x3B
#define  SN65DSI85_COLOR_BAR_CFG		0x3C
#define  SN65DSI85_RIGHT_CROP			0x3D
#define  SN65DSI85_LEFT_CROP			0x3E
#define  SN65DSI85_IRQ_EN			0xE0
#define  SN65DSI85_CHA_IRQ_MASK			0xE1
#define  SN65DSI85_CHB_IRQ_MASK			0xE2
#define  SN65DSI85_CHA_IRQ_STATUS		0xE5
#define  SN65DSI85_CHB_IRQ_STATUS		0xE6

#define RETRYLOOP 2
#endif

