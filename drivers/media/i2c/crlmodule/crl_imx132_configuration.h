/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2015 - 2018 Intel Corporation */

#ifndef __CRLMODULE_IMX132_CONFIGURATION_H_
#define __CRLMODULE_IMX132_CONFIGURATION_H_

#include "crlmodule-sensor-ds.h"

struct crl_register_write_rep imx132_powerup_regset[] = {
	{ 0x3087, CRL_REG_LEN_08BIT, 0x53 },
	{ 0x308B, CRL_REG_LEN_08BIT, 0x5A },
	{ 0x3094, CRL_REG_LEN_08BIT, 0x11 },
	{ 0x309D, CRL_REG_LEN_08BIT, 0xA4 },
	{ 0x30AA, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x30C6, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x30C7, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3118, CRL_REG_LEN_08BIT, 0x2F },
	{ 0x312A, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x312B, CRL_REG_LEN_08BIT, 0x0B },
	{ 0x312C, CRL_REG_LEN_08BIT, 0x0B },
	{ 0x312D, CRL_REG_LEN_08BIT, 0x13 },
	{ 0x303D, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x303E, CRL_REG_LEN_08BIT, 0x5A },
	{ 0x3040, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3041, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3048, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x304C, CRL_REG_LEN_08BIT, 0x2F },
	{ 0x304D, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x3064, CRL_REG_LEN_08BIT, 0x92 },
	{ 0x306A, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x309B, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x309E, CRL_REG_LEN_08BIT, 0x41 },
	{ 0x30A0, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x30A1, CRL_REG_LEN_08BIT, 0x0B },
	{ 0x30B2, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x30D5, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x30D6, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x30D7, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x30D8, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x30D9, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x30DA, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x30DB, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x30DC, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x30DD, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x30DE, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3102, CRL_REG_LEN_08BIT, 0x0C },
	{ 0x3103, CRL_REG_LEN_08BIT, 0x33 },
	{ 0x3104, CRL_REG_LEN_08BIT, 0x18 },
	{ 0x3105, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3106, CRL_REG_LEN_08BIT, 0x65 },
	{ 0x3107, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3108, CRL_REG_LEN_08BIT, 0x06 },
	{ 0x3109, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x310A, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x315C, CRL_REG_LEN_08BIT, 0x3D },
	{ 0x315D, CRL_REG_LEN_08BIT, 0x3C },
	{ 0x316E, CRL_REG_LEN_08BIT, 0x3E },
	{ 0x316F, CRL_REG_LEN_08BIT, 0x3D },
	{ 0x020e, CRL_REG_LEN_16BIT, 0x0100 },
	{ 0x0210, CRL_REG_LEN_16BIT, 0x01a0 },
	{ 0x0212, CRL_REG_LEN_16BIT, 0x0200 },
	{ 0x0214, CRL_REG_LEN_16BIT, 0x0100 },
	{ 0x0204, CRL_REG_LEN_16BIT, 0x0000 },
	{ 0x0202, CRL_REG_LEN_16BIT, 0x0000 },
	{ 0x0600, CRL_REG_LEN_16BIT, 0x0000 },
	{ 0x0602, CRL_REG_LEN_16BIT, 0x03ff },
	{ 0x0604, CRL_REG_LEN_16BIT, 0x03ff },
	{ 0x0606, CRL_REG_LEN_16BIT, 0x03ff },
	{ 0x0608, CRL_REG_LEN_16BIT, 0x03ff },
	{ 0x0100, CRL_REG_LEN_08BIT, 0x00 },
};

/*
	.input_clk = 24000000,
	.op_sys_clk = 405000000,
	.bitsperpixel = 10,
	.pixel_rate_csi = 810000000,
	.pixel_rate_pa = 768000000,
	.comp_items = 0,
	.ctrl_data = 0,
	.pll_regs_items = ARRAY_SIZE(imx132_pll_384),
	.pll_regs = imx132_pll_384,
*/
struct crl_register_write_rep imx132_pll_405[] = {
	/* PLL setting */
	{ 0x0305, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x0307, CRL_REG_LEN_08BIT, 0x87 },
	{ 0x30A4, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x303C, CRL_REG_LEN_08BIT, 0x4B },
	/* Global timing */
	{ 0x3304, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x3305, CRL_REG_LEN_08BIT, 0x06 },
	{ 0x3306, CRL_REG_LEN_08BIT, 0x19 },
	{ 0x3307, CRL_REG_LEN_08BIT, 0x03 },
	{ 0x3308, CRL_REG_LEN_08BIT, 0x0F },
	{ 0x3309, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x330A, CRL_REG_LEN_08BIT, 0x0C },
	{ 0x330B, CRL_REG_LEN_08BIT, 0x06 },
	{ 0x330C, CRL_REG_LEN_08BIT, 0x0B },
	{ 0x330D, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x330E, CRL_REG_LEN_08BIT, 0x03 },
	{ 0x3318, CRL_REG_LEN_08BIT, 0x62 },
	{ 0x3322, CRL_REG_LEN_08BIT, 0x09 },
	{ 0x3342, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3348, CRL_REG_LEN_08BIT, 0xE0 },
	{ 0x3301, CRL_REG_LEN_08BIT, 0x00 }, /* Lanes = 2*/
};

struct crl_register_write_rep imx132_pll_312[] = {
	/* PLL setting */
	{ 0x0305, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0307, CRL_REG_LEN_08BIT, 0x0D },
	{ 0x30A4, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x303C, CRL_REG_LEN_08BIT, 0x4B },
	/* Global timing */
	{ 0x3304, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x3305, CRL_REG_LEN_08BIT, 0x06 },
	{ 0x3306, CRL_REG_LEN_08BIT, 0x19 },
	{ 0x3307, CRL_REG_LEN_08BIT, 0x03 },
	{ 0x3308, CRL_REG_LEN_08BIT, 0x0F },
	{ 0x3309, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x330A, CRL_REG_LEN_08BIT, 0x0C },
	{ 0x330B, CRL_REG_LEN_08BIT, 0x06 },
	{ 0x330C, CRL_REG_LEN_08BIT, 0x0B },
	{ 0x330D, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x330E, CRL_REG_LEN_08BIT, 0x03 },
	{ 0x3318, CRL_REG_LEN_08BIT, 0x62 },
	{ 0x3322, CRL_REG_LEN_08BIT, 0x09 },
	{ 0x3342, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3348, CRL_REG_LEN_08BIT, 0xE0 },
	{ 0x3301, CRL_REG_LEN_08BIT, 0x01 }, /* Lanes = 1*/
};

struct crl_register_write_rep imx132_mode_1080P[] = {
	{0x0344, CRL_REG_LEN_08BIT, 0x00},
	{0x0345, CRL_REG_LEN_08BIT, 0x14},
	{0x0346, CRL_REG_LEN_08BIT, 0x00},
	{0x0347, CRL_REG_LEN_08BIT, 0x32},
	{0x0348, CRL_REG_LEN_08BIT, 0x07},
	{0x0349, CRL_REG_LEN_08BIT, 0xA3},
	{0x034A, CRL_REG_LEN_08BIT, 0x04},
	{0x034B, CRL_REG_LEN_08BIT, 0x79},
	{0x034C, CRL_REG_LEN_08BIT, 0x07},
	{0x034D, CRL_REG_LEN_08BIT, 0x90},
	{0x034E, CRL_REG_LEN_08BIT, 0x04},
	{0x034F, CRL_REG_LEN_08BIT, 0x48},
	{0x0381, CRL_REG_LEN_08BIT, 0x01},
	{0x0383, CRL_REG_LEN_08BIT, 0x01},
	{0x0385, CRL_REG_LEN_08BIT, 0x01},
	{0x0387, CRL_REG_LEN_08BIT, 0x01},
};

struct crl_register_write_rep imx132_mode_1636x1096[] = {
	{0x0344, CRL_REG_LEN_08BIT, 0x00},
	{0x0345, CRL_REG_LEN_08BIT, 0xAA},
	{0x0346, CRL_REG_LEN_08BIT, 0x00},
	{0x0347, CRL_REG_LEN_08BIT, 0x32},
	{0x0348, CRL_REG_LEN_08BIT, 0x07},
	{0x0349, CRL_REG_LEN_08BIT, 0x0D},
	{0x034A, CRL_REG_LEN_08BIT, 0x04},
	{0x034B, CRL_REG_LEN_08BIT, 0x79},
	{0x034C, CRL_REG_LEN_08BIT, 0x06},
	{0x034D, CRL_REG_LEN_08BIT, 0x64},
	{0x034E, CRL_REG_LEN_08BIT, 0x04},
	{0x034F, CRL_REG_LEN_08BIT, 0x48},
	{0x0381, CRL_REG_LEN_08BIT, 0x01},
	{0x0383, CRL_REG_LEN_08BIT, 0x01},
	{0x0385, CRL_REG_LEN_08BIT, 0x01},
	{0x0387, CRL_REG_LEN_08BIT, 0x01},
};

struct crl_register_write_rep imx132_fll_regs[] = {
	{ 0x0340, CRL_REG_LEN_16BIT, 0x045c }, /* LLP and FLL */
};

struct crl_register_write_rep imx132_llp_regs[] = {
	{ 0x0342, CRL_REG_LEN_16BIT, 0x08fc }, /* LLP and FLL */
};

struct crl_register_write_rep imx132_streamon_regs[] = {
	{ 0x0100, CRL_REG_LEN_08BIT, 0x01 }
};

struct crl_register_write_rep imx132_streamoff_regs[] = {
	{ 0x0100, CRL_REG_LEN_08BIT, 0x00 }
};

struct crl_register_write_rep imx132_data_fmt_width10[] = {
	{ 0x0112, CRL_REG_LEN_16BIT, 0x0a0a }
};

struct crl_register_write_rep imx132_data_fmt_width8[] = {
	{ 0x0112, CRL_REG_LEN_16BIT, 0x0808 }
};

struct crl_subdev_rect_rep imx132_1080P_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1976,
		.in_rect.height = 1200,
		.out_rect.left = 20,
		.out_rect.top = 50,
		.out_rect.width = 1936,
		.out_rect.height = 1096,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1936,
		.in_rect.height = 1096,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1936,
		.out_rect.height = 1096,
	},
};

struct crl_subdev_rect_rep imx132_1636x1096_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1976,
		.in_rect.height = 1200,
		.out_rect.left = 170,
		.out_rect.top = 50,
		.out_rect.width = 1636,
		.out_rect.height = 1096,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1636,
		.in_rect.height = 1096,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1636,
		.out_rect.height = 1096,
	},
};

struct crl_mode_rep imx132_modes[] = {
	{
		.sd_rects_items = ARRAY_SIZE(imx132_1636x1096_rects),
		.sd_rects = imx132_1636x1096_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 1636,
		.height = 1096,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(imx132_mode_1636x1096),
		.mode_regs = imx132_mode_1636x1096,
	},
	{
		.sd_rects_items = ARRAY_SIZE(imx132_1080P_rects),
		.sd_rects = imx132_1080P_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 1936,
		.height = 1096,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(imx132_mode_1080P),
		.mode_regs = imx132_mode_1080P,
	},
};

struct crl_register_write_rep imx132_poweroff_regset[] = {
	{ 0x0103, CRL_REG_LEN_08BIT, 0x01 },
};

struct crl_sensor_detect_config imx132_sensor_detect_regset[] = {
	{
		.reg = { 0x0003, CRL_REG_LEN_08BIT, 0x000000ff },
		.width = 5,
	},
	{
		.reg = { 0x0000, CRL_REG_LEN_16BIT, 0x0000ffff },
		.width = 7,
	},
};

struct crl_sensor_subdev_config imx132_sensor_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.name = "imx132 binner",
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "imx132 pixel array",
	},
};

struct crl_pll_configuration imx132_pll_configurations[] = {
	{
		.input_clk = 24000000,
		.op_sys_clk = 312000000,
		.bitsperpixel = 8,
		.pixel_rate_csi = 624000000,
		.pixel_rate_pa = 624000000,
		.csi_lanes = 1,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = ARRAY_SIZE(imx132_pll_312),
		.pll_regs = imx132_pll_312,
	},
	{
		.input_clk = 24000000,
		.op_sys_clk = 405000000,
		.bitsperpixel = 10,
		.pixel_rate_csi = 810000000,
		.pixel_rate_pa = 810000000,
		.csi_lanes = 2,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = ARRAY_SIZE(imx132_pll_405),
		.pll_regs = imx132_pll_405,
	},
};

struct crl_sensor_limits imx132_sensor_limits = {
	.x_addr_min = 0,
	.y_addr_min = 0,
	.x_addr_max = 1976,
	.y_addr_max = 1200,
	.min_frame_length_lines = 202,
	.max_frame_length_lines = 65535,
	.min_line_length_pixels = 560,
	.max_line_length_pixels = 65520,
};

struct crl_flip_data imx132_flip_configurations[] = {
	{
		.flip = CRL_FLIP_DEFAULT_NONE,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,
	},
	{
		.flip = CRL_FLIP_HFLIP,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
	},
	{
		.flip = CRL_FLIP_VFLIP,
		.pixel_order = CRL_PIXEL_ORDER_GBRG,
	},
	{
		.flip = CRL_FLIP_HFLIP_VFLIP,
		.pixel_order = CRL_PIXEL_ORDER_BGGR,
	},
};

struct crl_csi_data_fmt imx132_crl_csi_data_fmt[] = {
	{
		.code = MEDIA_BUS_FMT_SGRBG10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.bits_per_pixel = 10,
		.regs_items = 1,
		.regs = imx132_data_fmt_width10,
	},
	{
		.code = MEDIA_BUS_FMT_SRGGB10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,
		.regs_items = 1,
		.bits_per_pixel = 10,
		.regs = imx132_data_fmt_width10,
	},
	{
		.code = MEDIA_BUS_FMT_SBGGR10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_BGGR,
		.regs_items = 1,
		.bits_per_pixel = 10,
		.regs = imx132_data_fmt_width10,
	},
	{
		.code = MEDIA_BUS_FMT_SGBRG10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_GBRG,
		.regs_items = 1,
		.bits_per_pixel = 10,
		.regs = imx132_data_fmt_width10,
	},
	{
		.code = MEDIA_BUS_FMT_SGRBG8_1X8,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.regs_items = 1,
		.bits_per_pixel = 8,
		.regs = imx132_data_fmt_width8,
	},
	{
		.code = MEDIA_BUS_FMT_SRGGB8_1X8,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,
		.regs_items = 1,
		.bits_per_pixel = 8,
		.regs = imx132_data_fmt_width8,
	},
	{
		.code = MEDIA_BUS_FMT_SBGGR8_1X8,
		.pixel_order = CRL_PIXEL_ORDER_BGGR,
		.regs_items = 1,
		.bits_per_pixel = 8,
		.regs = imx132_data_fmt_width8,
	},
	{
		.code = MEDIA_BUS_FMT_SGBRG8_1X8,
		.pixel_order = CRL_PIXEL_ORDER_GBRG,
		.regs_items = 1,
		.bits_per_pixel = 8,
		.regs = imx132_data_fmt_width8,
	},
};

struct crl_dynamic_register_access imx132_flip_regs[] = {
	{
		.address = 0x0101,
		.len = CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE,
		.ops_items = 0,
		.ops = 0,
	},
};


struct crl_dynamic_register_access imx132_ana_gain_global_regs[] = {
	{
		.address = 0x0204,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
	},
};

struct crl_dynamic_register_access imx132_exposure_regs[] = {
	{
		.address = 0x0202,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
	}
};

struct crl_dynamic_register_access imx132_vblank_regs[] = {
	{
		.address = 0x0340,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
	},
};

struct crl_dynamic_register_access imx132_hblank_regs[] = {
	{
		.address = 0x0342,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
	},
};

static struct crl_dynamic_register_access imx132_test_pattern_regs[] = {
	{
		.address = 0x0600,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
	},
};

static const char * const imx132_test_patterns[] = {
	"Disabled",
	"Solid Colour",
	"Eight Vertical Colour Bars",
	"Fade to Gray",
	"PN9",
};

struct crl_v4l2_ctrl imx132_v4l2_ctrls[] = {
	{
		.sd_type = CRL_SUBDEV_TYPE_BINNER,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_IDLE,
		.ctrl_id = V4L2_CID_LINK_FREQ,
		.name = "V4L2_CID_LINK_FREQ",
		.type = CRL_V4L2_CTRL_TYPE_MENU_INT,
		.data.v4l2_int_menu.def = 0,
		.data.v4l2_int_menu.max = 0,
		.data.v4l2_int_menu.menu = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.regs_items = 0,
		.regs = 0,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_GET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_PIXEL_RATE,
		.name = "V4L2_CID_PIXEL_RATE_PA",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
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
		.op_type = CRL_V4L2_CTRL_GET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_PIXEL_RATE,
		.name = "V4L2_CID_PIXEL_RATE_CSI",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
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
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_ANALOGUE_GAIN,
		.name = "V4L2_CID_ANALOGUE_GAIN",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = 220,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.regs_items = ARRAY_SIZE(imx132_ana_gain_global_regs),
		.regs = imx132_ana_gain_global_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_EXPOSURE,
		.name = "V4L2_CID_EXPOSURE",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = 65500,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.regs_items = ARRAY_SIZE(imx132_exposure_regs),
		.regs = imx132_exposure_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_IDLE,
		.ctrl_id = V4L2_CID_HFLIP,
		.name = "V4L2_CID_HFLIP",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = 1,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.regs_items = ARRAY_SIZE(imx132_flip_regs),
		.regs = imx132_flip_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_IDLE,
		.ctrl_id = V4L2_CID_VFLIP,
		.name = "V4L2_CID_VFLIP",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = 1,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.regs_items = ARRAY_SIZE(imx132_flip_regs),
		.regs = imx132_flip_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_VBLANK,
		.name = "V4L2_CID_VBLANK",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.data.std_data.min = -65535,
		.data.std_data.max = 65535,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.regs_items = ARRAY_SIZE(imx132_vblank_regs),
		.regs = imx132_vblank_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_IDLE,
		.ctrl_id = V4L2_CID_HBLANK,
		.name = "V4L2_CID_HBLANK",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = 65520,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.regs_items = ARRAY_SIZE(imx132_hblank_regs),
		.regs = imx132_hblank_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_TEST_PATTERN,
		.name = "V4L2_CID_TEST_PATTERN",
		.type = CRL_V4L2_CTRL_TYPE_MENU_ITEMS,
		.data.v4l2_menu_items.menu = imx132_test_patterns,
		.data.v4l2_menu_items.size = ARRAY_SIZE(imx132_test_patterns),
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx132_test_pattern_regs),
		.regs = imx132_test_pattern_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
};

struct crl_sensor_configuration imx132_crl_configuration = {

	.powerup_regs_items = ARRAY_SIZE(imx132_powerup_regset),
	.powerup_regs = imx132_powerup_regset,

	.poweroff_regs_items = ARRAY_SIZE(imx132_poweroff_regset),
	.poweroff_regs = imx132_poweroff_regset,

	.id_reg_items = ARRAY_SIZE(imx132_sensor_detect_regset),
	.id_regs = imx132_sensor_detect_regset,

	.subdev_items = ARRAY_SIZE(imx132_sensor_subdevs),
	.subdevs = imx132_sensor_subdevs,

	.sensor_limits = &imx132_sensor_limits,

	.pll_config_items = ARRAY_SIZE(imx132_pll_configurations),
	.pll_configs = imx132_pll_configurations,

	.modes_items = ARRAY_SIZE(imx132_modes),
	.modes = imx132_modes,

	.streamon_regs_items = ARRAY_SIZE(imx132_streamon_regs),
	.streamon_regs = imx132_streamon_regs,

	.streamoff_regs_items = ARRAY_SIZE(imx132_streamoff_regs),
	.streamoff_regs = imx132_streamoff_regs,

	.v4l2_ctrls_items = ARRAY_SIZE(imx132_v4l2_ctrls),
	.v4l2_ctrl_bank = imx132_v4l2_ctrls,

	.csi_fmts_items = ARRAY_SIZE(imx132_crl_csi_data_fmt),
	.csi_fmts = imx132_crl_csi_data_fmt,

	.flip_items = ARRAY_SIZE(imx132_flip_configurations),
	.flip_data = imx132_flip_configurations,
};

#endif /* __CRLMODULE_IMX132_CONFIGURATION_H_ */
