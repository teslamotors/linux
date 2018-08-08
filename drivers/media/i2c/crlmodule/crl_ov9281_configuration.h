/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2018 Intel Corporation
 *
 * Author: Wu Xia <xia.wu@intel.com>
 *
 */

#ifndef __CRLMODULE_OV9281_CONFIGURATION_H_
#define __CRLMODULE_OV9281_CONFIGURATION_H_

#include "crlmodule-sensor-ds.h"

static struct crl_register_write_rep ov9281_pll_800mbps[] = {
	{ 0x0302, CRL_REG_LEN_08BIT, 0x32 },
	{ 0x030d, CRL_REG_LEN_08BIT, 0x50 },
	{ 0x030e, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x00, CRL_REG_LEN_DELAY, 10, 0x00},	/* Add a pre 10ms delay */
};

static struct crl_register_write_rep ov9281_powerup_regset[] = {
	{ 0x4f00, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x00, CRL_REG_LEN_DELAY, 10, 0x00},	/* Add a pre 10ms delay */
};

static struct crl_register_write_rep ov9281_mode_1m[] = {
	{ 0x3001, CRL_REG_LEN_08BIT, 0x60 },
	{ 0x3004, CRL_REG_LEN_08BIT, 0x00 },

	{ 0x3005, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3006, CRL_REG_LEN_08BIT, 0x04 },

	{ 0x3011, CRL_REG_LEN_08BIT, 0x0a },
	{ 0x3013, CRL_REG_LEN_08BIT, 0x18 },
	{ 0x3022, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x3030, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x3039, CRL_REG_LEN_08BIT, 0x32 },
	{ 0x303a, CRL_REG_LEN_08BIT, 0x00 },

	{ 0x3503, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x3505, CRL_REG_LEN_08BIT, 0x8c },
	{ 0x3507, CRL_REG_LEN_08BIT, 0x03 },
	{ 0x3508, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3610, CRL_REG_LEN_08BIT, 0x80 },
	{ 0x3611, CRL_REG_LEN_08BIT, 0xa0 },

	{ 0x3620, CRL_REG_LEN_08BIT, 0x6f },
	{ 0x3632, CRL_REG_LEN_08BIT, 0x56 },
	{ 0x3633, CRL_REG_LEN_08BIT, 0x78 },
	{ 0x3662, CRL_REG_LEN_08BIT, 0x05 },
	{ 0x3666, CRL_REG_LEN_08BIT, 0x00 },

	{ 0x366f, CRL_REG_LEN_08BIT, 0x5a },
	{ 0x3680, CRL_REG_LEN_08BIT, 0x84 },

	{ 0x3712, CRL_REG_LEN_08BIT, 0x80 },
	{ 0x372d, CRL_REG_LEN_08BIT, 0x22 },
	{ 0x3731, CRL_REG_LEN_08BIT, 0x80 },
	{ 0x3732, CRL_REG_LEN_08BIT, 0x30 },
	{ 0x3778, CRL_REG_LEN_08BIT, 0x00 },

	{ 0x377d, CRL_REG_LEN_08BIT, 0x22 },
	{ 0x3788, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x3789, CRL_REG_LEN_08BIT, 0xa4 },
	{ 0x378a, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x378b, CRL_REG_LEN_08BIT, 0x4a },
	{ 0x3799, CRL_REG_LEN_08BIT, 0x20 },

	/* window setting*/
	{ 0x3800, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3801, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x3802, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3803, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x3804, CRL_REG_LEN_08BIT, 0x05 },
	{ 0x3805, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x3806, CRL_REG_LEN_08BIT, 0x03 },
	{ 0x3807, CRL_REG_LEN_08BIT, 0x27 },

	{ 0x3808, CRL_REG_LEN_08BIT, 0x05 },
	{ 0x3809, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x380a, CRL_REG_LEN_08BIT, 0x03 },
	{ 0x380b, CRL_REG_LEN_08BIT, 0x20 },

	{ 0x380c, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x380d, CRL_REG_LEN_08BIT, 0xd8 },
	{ 0x380e, CRL_REG_LEN_08BIT, 0x03 },
	{ 0x380f, CRL_REG_LEN_08BIT, 0x8e },

	{ 0x3810, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3811, CRL_REG_LEN_08BIT, 0x00 },

	{ 0x3812, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3813, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3814, CRL_REG_LEN_08BIT, 0x11 },
	{ 0x3815, CRL_REG_LEN_08BIT, 0x11 },
	{ 0x3820, CRL_REG_LEN_08BIT, 0x40 },
	{ 0x3821, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3881, CRL_REG_LEN_08BIT, 0x42 },
	{ 0x38b1, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3920, CRL_REG_LEN_08BIT, 0xff },
	{ 0x4003, CRL_REG_LEN_08BIT, 0x40 },

	{ 0x4008, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x4009, CRL_REG_LEN_08BIT, 0x0b },
	{ 0x400c, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x400d, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x4010, CRL_REG_LEN_08BIT, 0x40 },
	{ 0x4043, CRL_REG_LEN_08BIT, 0x40 },
	{ 0x4307, CRL_REG_LEN_08BIT, 0x30 },

	{ 0x4317, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4501, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4507, CRL_REG_LEN_08BIT, 0x00 },

	{ 0x4509, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x450a, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x4601, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x470f, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4f07, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4800, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x5000, CRL_REG_LEN_08BIT, 0x9f },
	{ 0x5001, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x5e00, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x5d00, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x5d01, CRL_REG_LEN_08BIT, 0x00 },
};

static struct crl_register_write_rep ov9281_streamon_regs[] = {
	{ 0x00, CRL_REG_LEN_DELAY, 10, 0x00},	/* Add a pre 10ms delay */
	{ 0x0100, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x00, CRL_REG_LEN_DELAY, 10, 0x00},	/* Add a pre 10ms delay */
};

static struct crl_register_write_rep ov9281_streamoff_regs[] = {
	{ 0x00, CRL_REG_LEN_DELAY, 10, 0x00},	/* Add a pre 10ms delay */
	{ 0x0100, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x00, CRL_REG_LEN_DELAY, 10, 0x00},	/* Add a pre 10ms delay */
};

struct crl_register_write_rep ov9281_poweroff_regset[] = {
	{ 0x4f00, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x00, CRL_REG_LEN_DELAY, 10, 0x00},	/* Add a pre 10ms delay */
};

static struct crl_dynamic_register_access ov9281_ana_gain_global_regs[] = {
	{
		.address = 0x3509,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xff,
	 },
};

static struct crl_arithmetic_ops ov9281_expol_ops[] = {
	{
		.op = CRL_BITWISE_LSHIFT,
		.operand.entity_val = 4,
	},
};

static struct crl_arithmetic_ops ov9281_expom_ops[] = {
	{
		.op = CRL_BITWISE_LSHIFT,
		.operand.entity_val = 4,
	},
	{
		.op = CRL_BITWISE_RSHIFT,
		.operand.entity_val = 8,
	},
};

static struct crl_arithmetic_ops ov9281_expoh_ops[] = {
	{
		.op = CRL_BITWISE_LSHIFT,
		.operand.entity_val = 4,
	},
	{
		.op = CRL_BITWISE_RSHIFT,
		.operand.entity_val = 16,
	},
};

static struct crl_dynamic_register_access ov9281_exposure_regs[] = {
	{
		.address = 0x3502,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = ARRAY_SIZE(ov9281_expol_ops),
		.ops = ov9281_expol_ops,
		.mask = 0xff,
	},
	{
		.address = 0x3501,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = ARRAY_SIZE(ov9281_expom_ops),
		.ops = ov9281_expom_ops,
		.mask = 0xff,
	},
	{
		.address = 0x3500,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = ARRAY_SIZE(ov9281_expoh_ops),
		.ops = ov9281_expoh_ops,
		.mask = 0x0f,
	},
};

static struct crl_sensor_detect_config ov9281_sensor_detect_regset[] = {
	{
		.reg = { 0x300A, CRL_REG_LEN_08BIT, 0x000000ff  },
		.width = 7,
	},
	{
		.reg = { 0x300B, CRL_REG_LEN_08BIT, 0x000000ff  },
		.width = 7,
	},
};

static struct crl_pll_configuration ov9281_pll_configurations[] = {
	{
		.input_clk = 24000000,
		.op_sys_clk = 400000000,
		.bitsperpixel = 10,
		.pixel_rate_csi = 80000000,
		.pixel_rate_pa = 80000000,
		.csi_lanes = 2,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = ARRAY_SIZE(ov9281_pll_800mbps),
		.pll_regs = ov9281_pll_800mbps,
	 },
};

static struct crl_subdev_rect_rep ov9281_1m_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1280,
		.in_rect.height = 800,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1280,
		.out_rect.height = 800,
	 },
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1280,
		.in_rect.height = 800,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1280,
		.out_rect.height = 800,
	 },
};

static struct crl_mode_rep ov9281_modes[] = {
	{
		.sd_rects_items = ARRAY_SIZE(ov9281_1m_rects),
		.sd_rects = ov9281_1m_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 1280,
		.height = 800,
		.min_llp = 728,
		.min_fll = 910,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(ov9281_mode_1m),
		.mode_regs = ov9281_mode_1m,
	 },
};

static struct crl_sensor_subdev_config ov9281_sensor_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.name = "ov9281 binner",
	 },
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "ov9281 pixel array",
	 },
};

static struct crl_sensor_limits ov9281_sensor_limits = {
	.x_addr_min = 0,
	.y_addr_min = 0,
	.x_addr_max = 1280,
	.y_addr_max = 800,
	.min_frame_length_lines = 910,
	.max_frame_length_lines = 910,
	.min_line_length_pixels = 728,
	.max_line_length_pixels = 728,
	.scaler_m_min = 16,
	.scaler_m_max = 16,
	.scaler_n_min = 16,
	.scaler_n_max = 16,
	.min_even_inc = 1,
	.max_even_inc = 1,
	.min_odd_inc = 1,
	.max_odd_inc = 3,
};

static struct crl_flip_data ov9281_flip_configurations[] = {
	{
		.flip = CRL_FLIP_DEFAULT_NONE,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
	 },
	{
		.flip = CRL_FLIP_HFLIP,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
	 },
	{
		.flip = CRL_FLIP_VFLIP,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
	 },
	{
		.flip = CRL_FLIP_HFLIP_VFLIP,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
	 },
};

static struct crl_csi_data_fmt ov9281_crl_csi_data_fmt[] = {
	{
		.code = MEDIA_BUS_FMT_SGRBG10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.bits_per_pixel = 10,
		.regs_items = 0,
		.regs = 0,
	},
};

static struct crl_v4l2_ctrl ov9281_v4l2_ctrls[] = {
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
		.data.std_data.max = 0xFF,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(ov9281_ana_gain_global_regs),
		.regs = ov9281_ana_gain_global_regs,
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
		.data.std_data.max = 885,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(ov9281_exposure_regs),
		.regs = ov9281_exposure_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
	 },
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_FRAME_LENGTH_LINES,
		.name = "Frame length lines",
		.type = CRL_V4L2_CTRL_TYPE_CUSTOM,
		.data.std_data.min = 910,
		.data.std_data.max = 910,
		.data.std_data.step = 1,
		.data.std_data.def = 910,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
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
		.data.std_data.min = 728,
		.data.std_data.max = 728,
		.data.std_data.step = 1,
		.data.std_data.def = 728,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.dep_items = 0,
		.dep_ctrls = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
	},
};

/* Power items, they are enabled in the order they are listed here */
static struct crl_power_seq_entity ov9281_power_items[] = {
	{
		.type = CRL_POWER_ETY_CLK_FRAMEWORK,
		.val = 24000000,
		.delay = 500,
	},
	{
		.type = CRL_POWER_ETY_GPIO_FROM_PDATA,
		.val = 1,
		.delay = 5000,
	},
};

struct crl_sensor_configuration ov9281_crl_configuration = {

	.power_items = ARRAY_SIZE(ov9281_power_items),
	.power_entities = ov9281_power_items,

	.powerup_regs_items = ARRAY_SIZE(ov9281_powerup_regset),
	.powerup_regs = ov9281_powerup_regset,

	.poweroff_regs_items = 0,
	.poweroff_regs = 0,

	.id_reg_items = ARRAY_SIZE(ov9281_sensor_detect_regset),
	.id_regs = ov9281_sensor_detect_regset,

	.subdev_items = ARRAY_SIZE(ov9281_sensor_subdevs),
	.subdevs = ov9281_sensor_subdevs,

	.sensor_limits = &ov9281_sensor_limits,

	.pll_config_items = ARRAY_SIZE(ov9281_pll_configurations),
	.pll_configs = ov9281_pll_configurations,

	.modes_items = ARRAY_SIZE(ov9281_modes),
	.modes = ov9281_modes,

	.streamon_regs_items = ARRAY_SIZE(ov9281_streamon_regs),
	.streamon_regs = ov9281_streamon_regs,

	.streamoff_regs_items = ARRAY_SIZE(ov9281_streamoff_regs),
	.streamoff_regs = ov9281_streamoff_regs,

	.v4l2_ctrls_items = ARRAY_SIZE(ov9281_v4l2_ctrls),
	.v4l2_ctrl_bank = ov9281_v4l2_ctrls,

	.csi_fmts_items = ARRAY_SIZE(ov9281_crl_csi_data_fmt),
	.csi_fmts = ov9281_crl_csi_data_fmt,

	.flip_items = ARRAY_SIZE(ov9281_flip_configurations),
	.flip_data = ov9281_flip_configurations,
};

#endif  /* __CRLMODULE_OV9281_CONFIGURATION_H_ */


