/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/*
 * Copyright (C) 2018 Intel Corporation
 */

#ifndef __CRLMODULE_MAGNA_TI964_CONFIGURATION_H_
#define __CRLMODULE_MAGNA_TI964_CONFIGURATION_H_

#include "crlmodule-sensor-ds.h"

#define TI964_I2C_PHY_ADDR      0x3d

static struct crl_pll_configuration magna_ti964_pll_configurations[] = {
        {
                .input_clk = 24000000,
                .op_sys_clk = 400000000,
                .bitsperpixel = 16,
                .pixel_rate_csi = 529000000,
                .pixel_rate_pa = 529000000, /* pixel_rate = MIPICLK*2 *4/12 */
                .csi_lanes = 4,
                .comp_items = 0,
                .ctrl_data = 0,
                .pll_regs_items = 0,
                .pll_regs = NULL,
        },
        {
                .input_clk = 24000000,
                .op_sys_clk = 400000000,
                .bitsperpixel = 10,
                .pixel_rate_csi = 529000000,
                .pixel_rate_pa = 529000000, /* pixel_rate = MIPICLK*2 *4/12 */
                .csi_lanes = 4,
                .comp_items = 0,
                .ctrl_data = 0,
                .pll_regs_items = 0,
                .pll_regs = NULL,
        },
        {
                .input_clk = 24000000,
                .op_sys_clk = 400000000,
                .bitsperpixel = 20,
                .pixel_rate_csi = 529000000,
                .pixel_rate_pa = 529000000, /* pixel_rate = MIPICLK*2 *4/12 */
                .csi_lanes = 4,
                .comp_items = 0,
                .ctrl_data = 0,
                .pll_regs_items = 0,
                .pll_regs = NULL,
        }
};

static struct crl_subdev_rect_rep magna_ti964_1280_720_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1280,
		.in_rect.height = 720,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1280,
		.out_rect.height = 720,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1280,
		.in_rect.height = 720,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1280,
		.out_rect.height = 720,
	},
};

static struct crl_register_write_rep magna_ti964_powerup_regs[] = {
        {0x4c, CRL_REG_LEN_08BIT, 0x1, TI964_I2C_PHY_ADDR}, /* Select RX port 0 */
};

static struct crl_register_write_rep magna_ti964_poweroff_regs[] = {
	{0x1, CRL_REG_LEN_08BIT, 0x20, TI964_I2C_PHY_ADDR},
};

static struct crl_mode_rep magna_ti964_modes[] = {
	{
		.sd_rects_items = ARRAY_SIZE(magna_ti964_1280_720_rects),
		.sd_rects = magna_ti964_1280_720_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 1280,
		.height = 720,
		.min_llp = 2250,
		.min_fll = 1320,
	},
};

static struct crl_sensor_subdev_config magna_ti964_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.name = "ti964",
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.name = "magna binner",
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "magna pixel array",
	}
};

static struct crl_sensor_limits magna_ti964_limits = {
	.x_addr_min = 0,
	.y_addr_min = 0,
	.x_addr_max = 1280,
	.y_addr_max = 720,
	.min_frame_length_lines = 240,
	.max_frame_length_lines = 65535,
	.min_line_length_pixels = 320,
	.max_line_length_pixels = 32752,
};

static struct crl_csi_data_fmt magna_ti964_crl_csi_data_fmt[] = {
	{
		.code = ICI_FORMAT_YUYV,
		.pixel_order = CRL_PIXEL_ORDER_IGNORE,
		.bits_per_pixel = 16,
	},
	{
		.code = ICI_FORMAT_UYVY,
		.pixel_order = CRL_PIXEL_ORDER_IGNORE,
		.bits_per_pixel = 16,
	},
};

static struct crl_ctrl_data magna_ti964_ctrls[] = {
	{
		.sd_type = CRL_SUBDEV_TYPE_BINNER,
		.op_type = CRL_CTRL_SET_OP,
		.context = SENSOR_IDLE,
		.ctrl_id = ICI_EXT_SD_PARAM_ID_LINK_FREQ,
		.name = "CTRL_ID_LINK_FREQ",
		.type = CRL_CTRL_TYPE_MENU_INT,
		.data.int_menu.def = 0,
		.data.int_menu.max = 0,
		.data.int_menu.menu = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.regs_items = 0,
		.regs = 0,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_CTRL_GET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = ICI_EXT_SD_PARAM_ID_PIXEL_RATE,
		.name = "CTRL_ID_PIXEL_RATE_PA",
		.type = CRL_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = INT_MAX,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.regs_items = 0,
		.regs = 0,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_BINNER,
		.op_type = CRL_CTRL_GET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = ICI_EXT_SD_PARAM_ID_PIXEL_RATE,
		.name = "CTRL_ID_PIXEL_RATE_CSI",
		.type = CRL_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = INT_MAX,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.regs_items = 0,
		.regs = 0,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
};

struct crl_register_write_rep magna_ti964_streamon_regs[] = {
	{0x1f, CRL_REG_LEN_08BIT, 0x2, TI964_I2C_PHY_ADDR},
	{0x33, CRL_REG_LEN_08BIT, 0x1, TI964_I2C_PHY_ADDR},
	{0x6d, CRL_REG_LEN_08BIT, 0x7f, TI964_I2C_PHY_ADDR},
	{0x7c, CRL_REG_LEN_08BIT, 0x80, TI964_I2C_PHY_ADDR},
	{0x20, CRL_REG_LEN_08BIT, 0xe0, TI964_I2C_PHY_ADDR},
};

struct crl_register_write_rep magna_ti964_streamoff_regs[] = {
	{0x6d, CRL_REG_LEN_08BIT, 0x7f, TI964_I2C_PHY_ADDR},
	{0x7c, CRL_REG_LEN_08BIT, 0x81, TI964_I2C_PHY_ADDR},
	{0x20, CRL_REG_LEN_08BIT, 0xf0, TI964_I2C_PHY_ADDR},
};

struct crl_register_write_rep magna_ti964_onetime_init_regs[] = {
        {0x8, CRL_REG_LEN_08BIT, 0x1c, TI964_I2C_PHY_ADDR},
        {0xa, CRL_REG_LEN_08BIT, 0x79, TI964_I2C_PHY_ADDR},
        {0xb, CRL_REG_LEN_08BIT, 0x79, TI964_I2C_PHY_ADDR},
        {0xd, CRL_REG_LEN_08BIT, 0xb9, TI964_I2C_PHY_ADDR},
        {0x10, CRL_REG_LEN_08BIT, 0x91, TI964_I2C_PHY_ADDR},
        {0x11, CRL_REG_LEN_08BIT, 0x85, TI964_I2C_PHY_ADDR},
        {0x12, CRL_REG_LEN_08BIT, 0x89, TI964_I2C_PHY_ADDR},
        {0x13, CRL_REG_LEN_08BIT, 0xc1, TI964_I2C_PHY_ADDR},
        {0x17, CRL_REG_LEN_08BIT, 0xe1, TI964_I2C_PHY_ADDR},
        {0x18, CRL_REG_LEN_08BIT, 0x0, TI964_I2C_PHY_ADDR}, /* Disable frame sync. */
        {0x19, CRL_REG_LEN_08BIT, 0x0, TI964_I2C_PHY_ADDR}, /* Frame sync high time. */
        {0x1a, CRL_REG_LEN_08BIT, 0x2, TI964_I2C_PHY_ADDR},
        {0x1b, CRL_REG_LEN_08BIT, 0xa, TI964_I2C_PHY_ADDR}, /* Frame sync low time. */
        {0x1c, CRL_REG_LEN_08BIT, 0xd3, TI964_I2C_PHY_ADDR},
        {0x21, CRL_REG_LEN_08BIT, 0x43, TI964_I2C_PHY_ADDR}, /* Enable best effort mode. */
        {0xb0, CRL_REG_LEN_08BIT, 0x10, TI964_I2C_PHY_ADDR},
        {0xb1, CRL_REG_LEN_08BIT, 0x14, TI964_I2C_PHY_ADDR},
        {0xb2, CRL_REG_LEN_08BIT, 0x1f, TI964_I2C_PHY_ADDR},
        {0xb3, CRL_REG_LEN_08BIT, 0x8, TI964_I2C_PHY_ADDR},
        {0x32, CRL_REG_LEN_08BIT, 0x1, TI964_I2C_PHY_ADDR}, /* Select CSI port 0 */
        {0x4c, CRL_REG_LEN_08BIT, 0x1, TI964_I2C_PHY_ADDR}, /* Select RX port 0 */
        {0x58, CRL_REG_LEN_08BIT, 0x58, TI964_I2C_PHY_ADDR},
        {0x5c, CRL_REG_LEN_08BIT, 0x18, TI964_I2C_PHY_ADDR}, /* TI913 alias addr 0xc */
        {0x6d, CRL_REG_LEN_08BIT, 0x7f, TI964_I2C_PHY_ADDR},
        {0x70, CRL_REG_LEN_08BIT, 0x1e, TI964_I2C_PHY_ADDR}, /* YUV422_8 */
        {0x7c, CRL_REG_LEN_08BIT, 0x81, TI964_I2C_PHY_ADDR}, /* Use RAW10 8bit mode */
        {0xd2, CRL_REG_LEN_08BIT, 0x84, TI964_I2C_PHY_ADDR},
        {0x4c, CRL_REG_LEN_08BIT, 0x12, TI964_I2C_PHY_ADDR}, /* Select RX port 1 */
        {0x58, CRL_REG_LEN_08BIT, 0x58, TI964_I2C_PHY_ADDR},
        {0x5c, CRL_REG_LEN_08BIT, 0x1a, TI964_I2C_PHY_ADDR}, /* TI913 alias addr 0xd */
        {0x6d, CRL_REG_LEN_08BIT, 0x7f, TI964_I2C_PHY_ADDR},
        {0x70, CRL_REG_LEN_08BIT, 0x5e, TI964_I2C_PHY_ADDR}, /* YUV422_8 */
        {0x7c, CRL_REG_LEN_08BIT, 0x81, TI964_I2C_PHY_ADDR}, /* Use RAW10 8bit mode */
        {0xd2, CRL_REG_LEN_08BIT, 0x84, TI964_I2C_PHY_ADDR},
        {0x4c, CRL_REG_LEN_08BIT, 0x24, TI964_I2C_PHY_ADDR}, /* Select RX port 2*/
        {0x58, CRL_REG_LEN_08BIT, 0x58, TI964_I2C_PHY_ADDR},
        {0x5c, CRL_REG_LEN_08BIT, 0x1c, TI964_I2C_PHY_ADDR}, /* TI913 alias addr 0xe */
        {0x6d, CRL_REG_LEN_08BIT, 0x7f, TI964_I2C_PHY_ADDR},
        {0x70, CRL_REG_LEN_08BIT, 0x9e, TI964_I2C_PHY_ADDR}, /* YUV422_8 */
        {0x7c, CRL_REG_LEN_08BIT, 0x81, TI964_I2C_PHY_ADDR}, /* Use RAW10 8bit mode */
        {0xd2, CRL_REG_LEN_08BIT, 0x84, TI964_I2C_PHY_ADDR},
        {0x4c, CRL_REG_LEN_08BIT, 0x38, TI964_I2C_PHY_ADDR}, /* Select RX port3 */
        {0x58, CRL_REG_LEN_08BIT, 0x58, TI964_I2C_PHY_ADDR},
        {0x5c, CRL_REG_LEN_08BIT, 0x1e, TI964_I2C_PHY_ADDR}, /* TI913 alias addr 0xf */
        {0x6d, CRL_REG_LEN_08BIT, 0x7f, TI964_I2C_PHY_ADDR},
        {0x70, CRL_REG_LEN_08BIT, 0xde, TI964_I2C_PHY_ADDR}, /* YUV422_8 */
        {0x7c, CRL_REG_LEN_08BIT, 0x81, TI964_I2C_PHY_ADDR}, /* Use RAW10 8bit mode */
        {0xd2, CRL_REG_LEN_08BIT, 0x84, TI964_I2C_PHY_ADDR},
	{0x6e, CRL_REG_LEN_08BIT, 0x89, TI964_I2C_PHY_ADDR},
};

struct crl_sensor_configuration magna_ti964_crl_configuration = {

	.powerup_regs_items = ARRAY_SIZE(magna_ti964_powerup_regs),
	.powerup_regs = magna_ti964_powerup_regs,

	.poweroff_regs_items = ARRAY_SIZE(magna_ti964_poweroff_regs),
	.poweroff_regs = magna_ti964_poweroff_regs,

        .onetime_init_regs_items = ARRAY_SIZE(magna_ti964_onetime_init_regs),
        .onetime_init_regs = magna_ti964_onetime_init_regs,

	.subdev_items = ARRAY_SIZE(magna_ti964_subdevs),
	.subdevs = magna_ti964_subdevs,

	.pll_config_items = ARRAY_SIZE(magna_ti964_pll_configurations),
	.pll_configs = magna_ti964_pll_configurations,

	.sensor_limits = &magna_ti964_limits,

	.modes_items = ARRAY_SIZE(magna_ti964_modes),
	.modes = magna_ti964_modes,

	.streamon_regs_items = ARRAY_SIZE(magna_ti964_streamon_regs),
	.streamon_regs = magna_ti964_streamon_regs,

	.streamoff_regs_items = ARRAY_SIZE(magna_ti964_streamoff_regs),
	.streamoff_regs = magna_ti964_streamoff_regs,

	.ctrl_items = ARRAY_SIZE(magna_ti964_ctrls),
	.ctrl_bank = magna_ti964_ctrls,

	.csi_fmts_items = ARRAY_SIZE(magna_ti964_crl_csi_data_fmt),
	.csi_fmts = magna_ti964_crl_csi_data_fmt,

	.addr_len = CRL_ADDR_8BIT,
};

#endif  /* __CRLMODULE_MAGNA_TI964_CONFIGURATION_H_ */
