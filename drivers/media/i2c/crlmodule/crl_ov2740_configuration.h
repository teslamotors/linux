/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2017 - 2018 Intel Corporation
 *
 * Author: Roy Yang <royx.yang@intel.com>
 *
 */

#ifndef __CRLMODULE_OV2740_CONFIGURATION_H_
#define __CRLMODULE_OV2740_CONFIGURATION_H_

#include "crlmodule-nvm.h"
#include "crlmodule-sensor-ds.h"

static struct crl_register_write_rep ov2740_powerup_regset[] = {
	/*Reset*/
	{0x0103, CRL_REG_LEN_08BIT, 0x01},
	{0x0302, CRL_REG_LEN_08BIT, 0x4b},/* 26;1e */
	{0x030d, CRL_REG_LEN_08BIT, 0x4b},/* 26;1e */
	{0x030e, CRL_REG_LEN_08BIT, 0x02},
	{0x030a, CRL_REG_LEN_08BIT, 0x01},
	{0x0312, CRL_REG_LEN_08BIT, 0x11},/* 01 */
	{0x3000, CRL_REG_LEN_08BIT, 0x00},
	{0x3018, CRL_REG_LEN_08BIT, 0x32},/* 12(2 lane for 32; 1lane for 12) */
	{0x3031, CRL_REG_LEN_08BIT, 0x0a},
	{0x3080, CRL_REG_LEN_08BIT, 0x08},
	{0x3083, CRL_REG_LEN_08BIT, 0xB4},
	{0x3103, CRL_REG_LEN_08BIT, 0x00},
	{0x3104, CRL_REG_LEN_08BIT, 0x01},
	{0x3106, CRL_REG_LEN_08BIT, 0x01},
	{0x3500, CRL_REG_LEN_08BIT, 0x00},
	{0x3501, CRL_REG_LEN_08BIT, 0x44},
	{0x3502, CRL_REG_LEN_08BIT, 0x40},
	{0x3503, CRL_REG_LEN_08BIT, 0x88},
	{0x3507, CRL_REG_LEN_08BIT, 0x00},
	{0x3508, CRL_REG_LEN_08BIT, 0x00},
	{0x3509, CRL_REG_LEN_08BIT, 0x80},
	{0x350c, CRL_REG_LEN_08BIT, 0x00},
	{0x350d, CRL_REG_LEN_08BIT, 0x80},
	{0x3510, CRL_REG_LEN_08BIT, 0x00},
	{0x3511, CRL_REG_LEN_08BIT, 0x00},
	{0x3512, CRL_REG_LEN_08BIT, 0x20},
	{0x3632, CRL_REG_LEN_08BIT, 0x00},
	{0x3633, CRL_REG_LEN_08BIT, 0x10},
	{0x3634, CRL_REG_LEN_08BIT, 0x10},
	{0x3635, CRL_REG_LEN_08BIT, 0x10},
	{0x3645, CRL_REG_LEN_08BIT, 0x13},
	{0x3646, CRL_REG_LEN_08BIT, 0x81},
	{0x3636, CRL_REG_LEN_08BIT, 0x10},
	{0x3651, CRL_REG_LEN_08BIT, 0x0a},
	{0x3656, CRL_REG_LEN_08BIT, 0x02},
	{0x3659, CRL_REG_LEN_08BIT, 0x04},
	{0x365a, CRL_REG_LEN_08BIT, 0xda},
	{0x365b, CRL_REG_LEN_08BIT, 0xa2},
	{0x365c, CRL_REG_LEN_08BIT, 0x04},
	{0x365d, CRL_REG_LEN_08BIT, 0x1d},
	{0x365e, CRL_REG_LEN_08BIT, 0x1a},
	{0x3662, CRL_REG_LEN_08BIT, 0xd7},
	{0x3667, CRL_REG_LEN_08BIT, 0x78},
	{0x3669, CRL_REG_LEN_08BIT, 0x0a},
	{0x366a, CRL_REG_LEN_08BIT, 0x92},
	{0x3700, CRL_REG_LEN_08BIT, 0x54},
	{0x3702, CRL_REG_LEN_08BIT, 0x10},
	{0x3706, CRL_REG_LEN_08BIT, 0x42},
	{0x3709, CRL_REG_LEN_08BIT, 0x30},
	{0x370b, CRL_REG_LEN_08BIT, 0xc2},
	{0x3714, CRL_REG_LEN_08BIT, 0x63},
	{0x3715, CRL_REG_LEN_08BIT, 0x01},
	{0x3716, CRL_REG_LEN_08BIT, 0x00},
	{0x371a, CRL_REG_LEN_08BIT, 0x3e},
	{0x3732, CRL_REG_LEN_08BIT, 0x0e},
	{0x3733, CRL_REG_LEN_08BIT, 0x10},
	{0x375f, CRL_REG_LEN_08BIT, 0x0e},
	{0x3768, CRL_REG_LEN_08BIT, 0x30},
	{0x3769, CRL_REG_LEN_08BIT, 0x44},
	{0x376a, CRL_REG_LEN_08BIT, 0x22},
	{0x377b, CRL_REG_LEN_08BIT, 0x20},
	{0x377c, CRL_REG_LEN_08BIT, 0x00},
	{0x377d, CRL_REG_LEN_08BIT, 0x0c},
	{0x3798, CRL_REG_LEN_08BIT, 0x00},
	{0x37a1, CRL_REG_LEN_08BIT, 0x55},
	{0x37a8, CRL_REG_LEN_08BIT, 0x6d},
	{0x37c2, CRL_REG_LEN_08BIT, 0x04},
	{0x37c5, CRL_REG_LEN_08BIT, 0x00},
	{0x37c8, CRL_REG_LEN_08BIT, 0x00},
	{0x3800, CRL_REG_LEN_08BIT, 0x00},
	{0x3801, CRL_REG_LEN_08BIT, 0x00},
	{0x3802, CRL_REG_LEN_08BIT, 0x00},
	{0x3803, CRL_REG_LEN_08BIT, 0x00},
	{0x3804, CRL_REG_LEN_08BIT, 0x07},
	{0x3805, CRL_REG_LEN_08BIT, 0x8f},
	{0x3806, CRL_REG_LEN_08BIT, 0x04},
	{0x3807, CRL_REG_LEN_08BIT, 0x47},
	{0x3808, CRL_REG_LEN_08BIT, 0x07},
	{0x3809, CRL_REG_LEN_08BIT, 0x88},
	{0x380a, CRL_REG_LEN_08BIT, 0x04},
	{0x380b, CRL_REG_LEN_08BIT, 0x40},
	{0x380c, CRL_REG_LEN_08BIT, 0x08},
	{0x380d, CRL_REG_LEN_08BIT, 0x70},
	{0x380e, CRL_REG_LEN_08BIT, 0x04},
	{0x380f, CRL_REG_LEN_08BIT, 0x60},
	{0x3810, CRL_REG_LEN_08BIT, 0x00},
	{0x3811, CRL_REG_LEN_08BIT, 0x04},
	{0x3812, CRL_REG_LEN_08BIT, 0x00},
	{0x3813, CRL_REG_LEN_08BIT, 0x05},
	{0x3814, CRL_REG_LEN_08BIT, 0x01},
	{0x3815, CRL_REG_LEN_08BIT, 0x01},
	{0x3820, CRL_REG_LEN_08BIT, 0x80},
	{0x3821, CRL_REG_LEN_08BIT, 0x46},
	{0x3822, CRL_REG_LEN_08BIT, 0x84},
	{0x3829, CRL_REG_LEN_08BIT, 0x00},
	{0x382a, CRL_REG_LEN_08BIT, 0x01},
	{0x382b, CRL_REG_LEN_08BIT, 0x01},
	{0x3830, CRL_REG_LEN_08BIT, 0x04},
	{0x3836, CRL_REG_LEN_08BIT, 0x01},
	{0x3837, CRL_REG_LEN_08BIT, 0x08},
	{0x3839, CRL_REG_LEN_08BIT, 0x01},
	{0x383a, CRL_REG_LEN_08BIT, 0x00},
	{0x383b, CRL_REG_LEN_08BIT, 0x08},
	{0x383c, CRL_REG_LEN_08BIT, 0x00},
	{0x3f0b, CRL_REG_LEN_08BIT, 0x00},
	{0x4001, CRL_REG_LEN_08BIT, 0x20},
	{0x4009, CRL_REG_LEN_08BIT, 0x07},
	{0x4003, CRL_REG_LEN_08BIT, 0x10},
	{0x4010, CRL_REG_LEN_08BIT, 0xe0},
	{0x4016, CRL_REG_LEN_08BIT, 0x00},
	{0x4017, CRL_REG_LEN_08BIT, 0x10},
	{0x4044, CRL_REG_LEN_08BIT, 0x02},
	{0x4304, CRL_REG_LEN_08BIT, 0x08},
	{0x4307, CRL_REG_LEN_08BIT, 0x30},
	{0x4320, CRL_REG_LEN_08BIT, 0x80},
	{0x4322, CRL_REG_LEN_08BIT, 0x00},
	{0x4323, CRL_REG_LEN_08BIT, 0x00},
	{0x4324, CRL_REG_LEN_08BIT, 0x00},
	{0x4325, CRL_REG_LEN_08BIT, 0x00},
	{0x4326, CRL_REG_LEN_08BIT, 0x00},
	{0x4327, CRL_REG_LEN_08BIT, 0x00},
	{0x4328, CRL_REG_LEN_08BIT, 0x00},
	{0x4329, CRL_REG_LEN_08BIT, 0x00},
	{0x432c, CRL_REG_LEN_08BIT, 0x03},
	{0x432d, CRL_REG_LEN_08BIT, 0x81},
	{0x4501, CRL_REG_LEN_08BIT, 0x84},
	{0x4502, CRL_REG_LEN_08BIT, 0x40},
	{0x4503, CRL_REG_LEN_08BIT, 0x18},
	{0x4504, CRL_REG_LEN_08BIT, 0x04},
	{0x4508, CRL_REG_LEN_08BIT, 0x02},
	{0x4601, CRL_REG_LEN_08BIT, 0x10},
	{0x4800, CRL_REG_LEN_08BIT, 0x00},
	{0x4816, CRL_REG_LEN_08BIT, 0x52},
	{0x4837, CRL_REG_LEN_08BIT, 0x16},
	{0x5000, CRL_REG_LEN_08BIT, 0x7f},
	{0x5001, CRL_REG_LEN_08BIT, 0x00},
	{0x5005, CRL_REG_LEN_08BIT, 0x38},
	{0x501e, CRL_REG_LEN_08BIT, 0x0d},
	{0x5040, CRL_REG_LEN_08BIT, 0x00},
	{0x5901, CRL_REG_LEN_08BIT, 0x00},
	{0x3800, CRL_REG_LEN_08BIT, 0x00},
	{0x3801, CRL_REG_LEN_08BIT, 0x00},
	{0x3802, CRL_REG_LEN_08BIT, 0x00},
	{0x3803, CRL_REG_LEN_08BIT, 0x00},
	{0x3804, CRL_REG_LEN_08BIT, 0x07},
	{0x3805, CRL_REG_LEN_08BIT, 0x8f},
	{0x3806, CRL_REG_LEN_08BIT, 0x04},
	{0x3807, CRL_REG_LEN_08BIT, 0x47},
	{0x3808, CRL_REG_LEN_08BIT, 0x07},
	{0x3809, CRL_REG_LEN_08BIT, 0x8c},
	{0x380a, CRL_REG_LEN_08BIT, 0x04},
	{0x380b, CRL_REG_LEN_08BIT, 0x44},
	{0x3810, CRL_REG_LEN_08BIT, 0x00},
	{0x3811, CRL_REG_LEN_08BIT, 0x00},/* 00 */
	{0x3812, CRL_REG_LEN_08BIT, 0x00},
	{0x3813, CRL_REG_LEN_08BIT, 0x03},/* 00 */
	{0x4003, CRL_REG_LEN_08BIT, 0x40},/* set Black level to 0x40 */
};

static struct crl_register_write_rep ov2740_streamon_regs[] = {
	{ 0x0100, CRL_REG_LEN_08BIT, 0x01 }
};

static struct crl_register_write_rep ov2740_streamoff_regs[] = {
	{ 0x0100, CRL_REG_LEN_08BIT, 0x00 }
};

static struct crl_register_write_rep ov2740_data_fmt_width10[] = {
	{ 0x3031, CRL_REG_LEN_08BIT, 0x0a }
};

static struct crl_arithmetic_ops ov2740_vflip_ops[] = {
	{
		.op = CRL_BITWISE_LSHIFT,
		.operand.entity_val = 1,
	 },
};

static struct crl_arithmetic_ops ov2740_hflip_ops[] = {
	{
		.op = CRL_BITWISE_LSHIFT,
		.operand.entity_val = 1,
	 },
};

static struct crl_arithmetic_ops ov2740_exposure_ops[] = {
	{
		.op = CRL_BITWISE_LSHIFT,
		.operand.entity_val = 4,
	 },
};

static struct crl_dynamic_register_access ov2740_v_flip_regs[] = {
	{
		.address = 0x3820,
		.len = CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE,
		.ops_items = ARRAY_SIZE(ov2740_vflip_ops),
		.ops = ov2740_vflip_ops,
		.mask = 0x1,
	 },
};

static struct crl_dynamic_register_access ov2740_h_flip_regs[] = {
	{
		.address = 0x3821,
		.len = CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE,
		.ops_items = ARRAY_SIZE(ov2740_hflip_ops),
		.ops = ov2740_hflip_ops,
		.mask = 0x1,
	 },
};

static struct crl_dynamic_register_access ov2740_dig_gain_regs[] = {
	{
		.address = 0x500A,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	},
	{
		.address = 0x500C,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	},
	{
		.address = 0x500E,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	},
};

struct crl_register_write_rep ov2740_poweroff_regset[] = {
	{ 0x0103, CRL_REG_LEN_08BIT, 0x01  },
};

static struct crl_dynamic_register_access ov2740_ana_gain_global_regs[] = {
	{
		.address = 0x3508,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0x7ff,
	 },
};

static struct crl_dynamic_register_access ov2740_exposure_regs[] = {
	{
		.address = 0x3500,
		.len = CRL_REG_LEN_24BIT,
		.ops_items = ARRAY_SIZE(ov2740_exposure_ops),
		.ops = ov2740_exposure_ops,
		.mask = 0x0ffff0,
	 },
};

static struct crl_dynamic_register_access ov2740_vblank_regs[] = {
	{
		.address = 0x380E,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0x7fff,
	 },
};

static struct crl_dynamic_register_access ov2740_hblank_regs[] = {
	{
		.address = 0x380C,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	 },
};

static struct crl_sensor_detect_config ov2740_sensor_detect_regset[] = {
	{
		.reg = { 0x300B, CRL_REG_LEN_08BIT, 0x000000ff  },
		.width = 7,
	 },
	{
		.reg = { 0x300C, CRL_REG_LEN_08BIT, 0x000000ff  },
		.width = 7,
	 },
};

static struct crl_pll_configuration ov2740_pll_configurations[] = {
	{
		.input_clk = 19200000,
		.op_sys_clk = 72000000,
		.bitsperpixel = 10,
		.pixel_rate_csi = 72000000,
		.pixel_rate_pa = 72000000,
		.csi_lanes = 2,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = 0,
		.pll_regs = 0,
	 },

};

static struct crl_subdev_rect_rep ov2740_1932x1092_rects_native[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1932,
		.in_rect.height = 1092,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1932,
		.out_rect.height = 1092,
	 },
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1932,
		.in_rect.height = 1092,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1932,
		.out_rect.height = 1092,
	 },
};

static struct crl_mode_rep ov2740_modes[] = {
	{
		.sd_rects_items = ARRAY_SIZE(ov2740_1932x1092_rects_native),
		.sd_rects = ov2740_1932x1092_rects_native,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 1932,
		.height = 1092,
		.min_llp = 2160,
		.min_fll = 1120,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = 0,
		.mode_regs = 0,
	},
};

static struct crl_sensor_subdev_config ov2740_sensor_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.name = "ov2740 binner",
	 },
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "ov2740 pixel array",
	 },
};

static struct crl_sensor_limits ov2740_sensor_limits = {
	.x_addr_min = 0,
	.y_addr_min = 0,
	.x_addr_max = 1932,
	.y_addr_max = 1092,
	.min_frame_length_lines = 1120,
	.max_frame_length_lines = 32767,
	.min_line_length_pixels = 2160,
	.max_line_length_pixels = 65535,
};

static struct crl_flip_data ov2740_flip_configurations[] = {
	{
		.flip = CRL_FLIP_DEFAULT_NONE,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
	 },
	{
		.flip = CRL_FLIP_VFLIP,
		.pixel_order = CRL_PIXEL_ORDER_BGGR,
	 },
	{
		.flip = CRL_FLIP_HFLIP,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,
	 },
	{
		.flip = CRL_FLIP_HFLIP_VFLIP,
		.pixel_order = CRL_PIXEL_ORDER_GBRG,
	 },
};

static struct crl_csi_data_fmt ov2740_crl_csi_data_fmt[] = {
	{
		.code = MEDIA_BUS_FMT_SGRBG10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.bits_per_pixel = 10,
		.regs_items = 1,
		.regs = ov2740_data_fmt_width10,
	 },
	{
		.code = MEDIA_BUS_FMT_SRGGB10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,
		.regs_items = 1,
		.bits_per_pixel = 10,
		.regs = ov2740_data_fmt_width10,
	 },
	{
		.code = MEDIA_BUS_FMT_SBGGR10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_BGGR,
		.regs_items = 1,
		.bits_per_pixel = 10,
		.regs = ov2740_data_fmt_width10,
	 },
	{
		.code = MEDIA_BUS_FMT_SGBRG10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_GBRG,
		.regs_items = 1,
		.bits_per_pixel = 10,
		.regs = ov2740_data_fmt_width10,
	 },
};

static struct crl_v4l2_ctrl ov2740_v4l2_ctrls[] = {
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
		.data.std_data.max = 4096,
		.data.std_data.step = 1,
		.data.std_data.def = 128,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(ov2740_ana_gain_global_regs),
		.regs = ov2740_ana_gain_global_regs,
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
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(ov2740_exposure_regs),
		.regs = ov2740_exposure_regs,
		.dep_items = 0, /* FLL is changes automatically */
		.dep_ctrls = 0,
	 },
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_HFLIP,
		.name = "V4L2_CID_HFLIP",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = 1,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(ov2740_h_flip_regs),
		.regs = ov2740_h_flip_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
	 },
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_VFLIP,
		.name = "V4L2_CID_VFLIP",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = 1,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(ov2740_v_flip_regs),
		.regs = ov2740_v_flip_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_FRAME_LENGTH_LINES,
		.name = "Frame Length Lines",
		.type = CRL_V4L2_CTRL_TYPE_CUSTOM,
		.data.std_data.min = 1120,
		.data.std_data.max = 32767,
		.data.std_data.step = 1,
		.data.std_data.def = 1120,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(ov2740_vblank_regs),
		.regs = ov2740_vblank_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_LINE_LENGTH_PIXELS,
		.name = "Line Length Pixels",
		.type = CRL_V4L2_CTRL_TYPE_CUSTOM,
		.data.std_data.min = 2160,
		.data.std_data.max = 65535,
		.data.std_data.step = 1,
		.data.std_data.def = 2160,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(ov2740_hblank_regs),
		.regs = ov2740_hblank_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_GAIN,
		.name = "Digital Gain",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = 4095,
		.data.std_data.step = 1,
		.data.std_data.def = 1024,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(ov2740_dig_gain_regs),
		.regs = ov2740_dig_gain_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_BINNER,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_WDR_MODE,
		.name = "V4L2_CID_WDR_MODE",
		.type = CRL_V4L2_CTRL_TYPE_CUSTOM,
		.data.std_data.min = 0,
		.data.std_data.max = 1,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_MODE_SELECTION,
		.ctrl = 0,
		.regs_items = 0,
		.regs = 0,
		.dep_items = 0,
		.dep_ctrls = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
	},
};

#ifdef CONFIG_VIDEO_CRLMODULE_OTP_VALIDATE
static struct crl_register_write_rep ov2740_otp_preop_regset[] = {
	/*sensor OTP module check*/
	{ 0x5000, CRL_REG_LEN_08BIT, 0x5f },
	/* Manual mode, program disable */
	{ 0x3D84, CRL_REG_LEN_08BIT, 0xC0 },
	/* Manual OTP start address for access */
	{ 0x3D88, CRL_REG_LEN_08BIT, 0x70},
	{ 0x3D89, CRL_REG_LEN_08BIT, 0x10},
	/* Manual OTP end address for access */
	{ 0x3D8A, CRL_REG_LEN_08BIT, 0x70},
	{ 0x3D8B, CRL_REG_LEN_08BIT, 0x11},
	/*streaming on*/
	{ 0x0100, CRL_REG_LEN_08BIT, 0x01 },
	/* OTP load enable */
	{ 0x3D81, CRL_REG_LEN_08BIT, 0x31 },
	/* Wait for the data to load into the buffer */
	{ 0x0000, CRL_REG_LEN_DELAY, 0x14 },
};

static struct crl_register_write_rep ov2740_otp_postop_regset[] = {
	{ 0x0100, CRL_REG_LEN_08BIT, 0x00 }, /* Stop streaming */
};

static struct crl_register_write_rep ov2740_otp_mode_regset[] = {
	{ 0x5000, CRL_REG_LEN_08BIT, 0x7F }, /*ISP Control*/
	{ 0x5040, CRL_REG_LEN_08BIT, 0xA8 }, /*Set Test Mode*/
};

struct crl_register_read_rep ov2740_sensor_otp_read_regset[] = {
	{ 0x7010, CRL_REG_LEN_16BIT, 0x0000ffff },
};
#endif

static struct crl_arithmetic_ops ov2740_frame_desc_width_ops[] = {
	{
	 .op = CRL_ASSIGNMENT,
	 .operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_VAR_REF,
	 .operand.entity_val = CRL_VAR_REF_OUTPUT_WIDTH,
	},
};

static struct crl_arithmetic_ops ov2740_frame_desc_height_ops[] = {
	{
	 .op = CRL_ASSIGNMENT,
	 .operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
	 .operand.entity_val = 1,
	},
};

static struct crl_frame_desc ov2740_frame_desc[] = {
	{
		.flags.entity_val = 0,
		.bpp.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_VAR_REF,
		.bpp.entity_val = CRL_VAR_REF_BITSPERPIXEL,
		.pixelcode.entity_val = MEDIA_BUS_FMT_FIXED,
		.length.entity_val = 0,
		.start_line.entity_val = 0,
		.start_pixel.entity_val = 0,
		.width = {
			 .ops_items = ARRAY_SIZE(ov2740_frame_desc_width_ops),
			 .ops = ov2740_frame_desc_width_ops,
			 },
		.height = {
			  .ops_items = ARRAY_SIZE(ov2740_frame_desc_height_ops),
			  .ops = ov2740_frame_desc_height_ops,
			  },
		.csi2_channel.entity_val = 0,
		.csi2_data_type.entity_val = 0x12,
	},
};

/* Power items, they are enabled in the order they are listed here */
static struct crl_power_seq_entity ov2740_power_items[] = {
	{
		.type = CRL_POWER_ETY_CLK_FRAMEWORK,
		.val = 19200000,
	},
	{
		.type = CRL_POWER_ETY_GPIO_FROM_PDATA,
		.val = 1,
		.undo_val = 0,
		.delay = 1000,
	},
};

static struct crl_sensor_configuration ov2740_crl_configuration = {

	.power_items = ARRAY_SIZE(ov2740_power_items),
	.power_entities = ov2740_power_items,

	.powerup_regs_items = ARRAY_SIZE(ov2740_powerup_regset),
	.powerup_regs = ov2740_powerup_regset,

	.poweroff_regs_items = 0,
	.poweroff_regs = 0,


	.id_reg_items = ARRAY_SIZE(ov2740_sensor_detect_regset),
	.id_regs = ov2740_sensor_detect_regset,

	.subdev_items = ARRAY_SIZE(ov2740_sensor_subdevs),
	.subdevs = ov2740_sensor_subdevs,

	.sensor_limits = &ov2740_sensor_limits,

	.pll_config_items = ARRAY_SIZE(ov2740_pll_configurations),
	.pll_configs = ov2740_pll_configurations,

	.modes_items = ARRAY_SIZE(ov2740_modes),
	.modes = ov2740_modes,

	.streamon_regs_items = ARRAY_SIZE(ov2740_streamon_regs),
	.streamon_regs = ov2740_streamon_regs,

	.streamoff_regs_items = ARRAY_SIZE(ov2740_streamoff_regs),
	.streamoff_regs = ov2740_streamoff_regs,

	.v4l2_ctrls_items = ARRAY_SIZE(ov2740_v4l2_ctrls),
	.v4l2_ctrl_bank = ov2740_v4l2_ctrls,

	.csi_fmts_items = ARRAY_SIZE(ov2740_crl_csi_data_fmt),
	.csi_fmts = ov2740_crl_csi_data_fmt,

	.flip_items = ARRAY_SIZE(ov2740_flip_configurations),
	.flip_data = ov2740_flip_configurations,

	.crl_nvm_info.nvm_flags = CRL_NVM_ADDR_MODE_16BIT,
	.crl_nvm_info.nvm_preop_regs_items = 0,
	.crl_nvm_info.nvm_postop_regs_items = 0,
	.crl_nvm_info.nvm_blobs_items = 0,

#ifdef CONFIG_VIDEO_CRLMODULE_OTP_VALIDATE
	.crl_otp_info.otp_preop_regs_items =
		ARRAY_SIZE(ov2740_otp_preop_regset),
	.crl_otp_info.otp_preop_regs = ov2740_otp_preop_regset,
	.crl_otp_info.otp_postop_regs_items =
		ARRAY_SIZE(ov2740_otp_postop_regset),
	.crl_otp_info.otp_postop_regs = ov2740_otp_postop_regset,

	.crl_otp_info.otp_mode_regs_items =
		ARRAY_SIZE(ov2740_otp_mode_regset),
	.crl_otp_info.otp_mode_regs = ov2740_otp_mode_regset,

	.crl_otp_info.otp_read_regs_items =
		ARRAY_SIZE(ov2740_sensor_otp_read_regset),
	.crl_otp_info.otp_read_regs = ov2740_sensor_otp_read_regset,

	.crl_otp_info.otp_id = 0x5168, /*chicony module*/
#endif
	.frame_desc_entries = ARRAY_SIZE(ov2740_frame_desc),
	.frame_desc_type = CRL_V4L2_MBUS_FRAME_DESC_TYPE_CSI2,
	.frame_desc = ov2740_frame_desc,
};

#endif  /* __CRLMODULE_OV2740_CONFIGURATION_H_ */
