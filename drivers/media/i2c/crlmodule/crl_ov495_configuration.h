/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2017 - 2018 Intel Corporation
 *
 * Author: Ying Chang <ying.chang@intel.com>
 *         Meng J Chen <meng.j.chen@intel.com>
 *         Zhaox Li <zhaox.li@intel.com>
 *
 */

#ifndef __CRLMODULE_OV495_CONFIGURATION_H_
#define __CRLMODULE_OV495_CONFIGURATION_H_

#include "crlmodule-sensor-ds.h"

struct crl_sensor_detect_config ov495_sensor_detect_regset[] = {
	{
		.reg = {0x3000, CRL_REG_LEN_08BIT, 0xFF},
		.width = 8,
	},
	{
		.reg = {0x3001, CRL_REG_LEN_08BIT, 0xFF},
		.width = 8,
	},
	{
		.reg = {0x3002, CRL_REG_LEN_08BIT, 0xFF},
		.width = 8,
	},
	{
		.reg = {0x3003, CRL_REG_LEN_08BIT, 0xFF},
		.width = 8,
	},
};

static struct crl_pll_configuration ov495_pll_configurations[] = {
	{
		.input_clk = 27000000,
		.op_sys_clk = 400000000,
		.bitsperpixel = 16,
		.pixel_rate_csi = 108000000,
		.pixel_rate_pa = 108000000, /* pixel_rate = op_sys_clk*2 *csi_lanes/bitsperpixel */
		.csi_lanes = 4,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = 0,
		.pll_regs = 0,
	},
};

static struct crl_subdev_rect_rep ov495_1280_1080_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1280,
		.in_rect.height = 1080,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1280,
		.out_rect.height = 1080,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1280,
		.in_rect.height = 1080,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1280,
		.out_rect.height = 1080,
	},
};

static struct crl_subdev_rect_rep ov495_1920_1080_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1920,
		.in_rect.height = 1080,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1920,
		.out_rect.height = 1080,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1920,
		.in_rect.height = 1080,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1920,
		.out_rect.height = 1080,
	},
};

static struct crl_register_write_rep ov495_1920x1080_regs[] = {
	{0x3516, CRL_REG_LEN_08BIT, 0x00},
	{0x354d, CRL_REG_LEN_08BIT, 0x10},
	{0x354a, CRL_REG_LEN_08BIT, 0x1d},
	{0x0500, CRL_REG_LEN_08BIT, 0x00},
	{0x30c0, CRL_REG_LEN_08BIT, 0xe2},
	{0x0000, CRL_REG_LEN_DELAY, 0x0a},

	{0x3516, CRL_REG_LEN_08BIT, 0x00},
	{0x354d, CRL_REG_LEN_08BIT, 0x10},
	{0x354a, CRL_REG_LEN_08BIT, 0x1d},
	{0x0500, CRL_REG_LEN_08BIT, 0x01},
	{0x30c0, CRL_REG_LEN_08BIT, 0xe2},
	{0x0000, CRL_REG_LEN_DELAY, 0x0a},
};

static struct crl_register_write_rep ov495_1280x1080_regs[] = {
	{0x3516, CRL_REG_LEN_08BIT, 0x00},
	{0x354d, CRL_REG_LEN_08BIT, 0x10},
	{0x354a, CRL_REG_LEN_08BIT, 0x1d},
	{0x7800, CRL_REG_LEN_08BIT, 0x00},
	{0x0500, CRL_REG_LEN_08BIT, 0x00},
	{0x0501, CRL_REG_LEN_08BIT, 0x01},
	{0x0502, CRL_REG_LEN_08BIT, 0x01},
	{0x0503, CRL_REG_LEN_08BIT, 0x40},
	{0x0504, CRL_REG_LEN_08BIT, 0x00},
	{0x0505, CRL_REG_LEN_08BIT, 0x00},
	{0x0506, CRL_REG_LEN_08BIT, 0x05},
	{0x0507, CRL_REG_LEN_08BIT, 0x00},
	{0x0508, CRL_REG_LEN_08BIT, 0x04},
	{0x0509, CRL_REG_LEN_08BIT, 0x38},
	{0x30c0, CRL_REG_LEN_08BIT, 0xc3},
	{0x0000, CRL_REG_LEN_DELAY, 0x0a},
};

static struct crl_arithmetic_ops ov495_frame_desc_width_ops[] = {
	{
		.op = CRL_ASSIGNMENT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_VAR_REF,
		.operand.entity_val = CRL_VAR_REF_OUTPUT_WIDTH,
	},
};

static struct crl_arithmetic_ops ov495_frame_desc_height_ops[] = {
	{
		.op = CRL_ASSIGNMENT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
		.operand.entity_val = 1,
	},
};

static struct crl_frame_desc ov495_frame_desc[] = {
	{
		.flags.entity_val = 0,
		.bpp.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_VAR_REF,
		.bpp.entity_val = CRL_VAR_REF_BITSPERPIXEL,
		.pixelcode.entity_val = MEDIA_BUS_FMT_FIXED,
		.length.entity_val = 0,
		.start_line.entity_val = 0,
		.start_pixel.entity_val = 0,
		.width = {
			.ops_items = ARRAY_SIZE(ov495_frame_desc_width_ops),
			.ops = ov495_frame_desc_width_ops,
		},
		.height = {
			.ops_items = ARRAY_SIZE(ov495_frame_desc_height_ops),
			.ops = ov495_frame_desc_height_ops,
		},
		.csi2_channel.entity_val = 0,
		.csi2_data_type.entity_val = 0x12,
	},
	{
		.flags.entity_val = 0,
		.bpp.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_VAR_REF,
		.bpp.entity_val = CRL_VAR_REF_BITSPERPIXEL,
		.pixelcode.entity_val = MEDIA_BUS_FMT_FIXED,
		.length.entity_val = 0,
		.start_line.entity_val = 0,
		.start_pixel.entity_val = 0,
		.width = {
			.ops_items = ARRAY_SIZE(ov495_frame_desc_width_ops),
			.ops = ov495_frame_desc_width_ops,
		},
		.height = {
			.ops_items = ARRAY_SIZE(ov495_frame_desc_height_ops),
			.ops = ov495_frame_desc_height_ops,
		},
		.csi2_channel.entity_val = 1,
		.csi2_data_type.entity_val = 0x12,
	},
	{
		.flags.entity_val = 0,
		.bpp.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_VAR_REF,
		.bpp.entity_val = CRL_VAR_REF_BITSPERPIXEL,
		.pixelcode.entity_val = MEDIA_BUS_FMT_FIXED,
		.length.entity_val = 0,
		.start_line.entity_val = 0,
		.start_pixel.entity_val = 0,
		.width = {
			.ops_items = ARRAY_SIZE(ov495_frame_desc_width_ops),
			.ops = ov495_frame_desc_width_ops,
		},
		.height = {
			.ops_items = ARRAY_SIZE(ov495_frame_desc_height_ops),
			.ops = ov495_frame_desc_height_ops,
		},
		.csi2_channel.entity_val = 2,
		.csi2_data_type.entity_val = 0x12,
	},
	{
		.flags.entity_val = 0,
		.bpp.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_VAR_REF,
		.bpp.entity_val = CRL_VAR_REF_BITSPERPIXEL,
		.pixelcode.entity_val = MEDIA_BUS_FMT_FIXED,
		.length.entity_val = 0,
		.start_line.entity_val = 0,
		.start_pixel.entity_val = 0,
		.width = {
			.ops_items = ARRAY_SIZE(ov495_frame_desc_width_ops),
			.ops = ov495_frame_desc_width_ops,
		},
		.height = {
			.ops_items = ARRAY_SIZE(ov495_frame_desc_height_ops),
			.ops = ov495_frame_desc_height_ops,
		},
		.csi2_channel.entity_val = 3,
		.csi2_data_type.entity_val = 0x12,
	},
};

static struct crl_mode_rep ov495_modes[] = {
	{
		.sd_rects_items = ARRAY_SIZE(ov495_1280_1080_rects),
		.sd_rects = ov495_1280_1080_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 1280,
		.height = 1080,
		.min_llp = 2250,
		.min_fll = 1320,
		.mode_regs_items = ARRAY_SIZE(ov495_1280x1080_regs),
		.mode_regs = ov495_1280x1080_regs,
	},
	{
		.sd_rects_items = ARRAY_SIZE(ov495_1920_1080_rects),
		.sd_rects = ov495_1920_1080_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 1920,
		.height = 1080,
		.min_llp = 2250,
		.min_fll = 1320,
		.mode_regs_items = ARRAY_SIZE(ov495_1920x1080_regs),
		.mode_regs = ov495_1920x1080_regs,
	},
};

static struct crl_sensor_subdev_config ov495_sensor_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.name = "ov495 binner",
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "ov495 pixel array",
	}
};

static struct crl_sensor_limits ov495_sensor_limits = {
	.x_addr_min = 0,
	.y_addr_min = 0,
	.x_addr_max = 1920,
	.y_addr_max = 1080,
	.min_frame_length_lines = 240,
	.max_frame_length_lines = 65535,
	.min_line_length_pixels = 320,
	.max_line_length_pixels = 32752,
};

static struct crl_csi_data_fmt ov495_crl_csi_data_fmt[] = {
	{
		.code = MEDIA_BUS_FMT_YUYV8_1X16,
		.pixel_order = CRL_PIXEL_ORDER_IGNORE,
		.bits_per_pixel = 16,
	},
	{
		.code = MEDIA_BUS_FMT_UYVY8_1X16,
		.pixel_order = CRL_PIXEL_ORDER_IGNORE,
		.bits_per_pixel = 16,
	},
};

static struct crl_v4l2_ctrl ov495_v4l2_ctrls[] = {
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
};

struct crl_sensor_configuration ov495_crl_configuration = {

	.subdev_items = ARRAY_SIZE(ov495_sensor_subdevs),
	.subdevs = ov495_sensor_subdevs,

	.pll_config_items = ARRAY_SIZE(ov495_pll_configurations),
	.pll_configs = ov495_pll_configurations,

	.id_reg_items = ARRAY_SIZE(ov495_sensor_detect_regset),
	.id_regs = ov495_sensor_detect_regset,

	.sensor_limits = &ov495_sensor_limits,

	.modes_items = ARRAY_SIZE(ov495_modes),
	.modes = ov495_modes,

	.streamon_regs_items = 0,
	.streamon_regs = 0,

	.streamoff_regs_items = 0,
	.streamoff_regs = 0,

	.v4l2_ctrls_items = ARRAY_SIZE(ov495_v4l2_ctrls),
	.v4l2_ctrl_bank = ov495_v4l2_ctrls,

	.csi_fmts_items = ARRAY_SIZE(ov495_crl_csi_data_fmt),
	.csi_fmts = ov495_crl_csi_data_fmt,

	.frame_desc_entries = ARRAY_SIZE(ov495_frame_desc),
	.frame_desc_type = CRL_V4L2_MBUS_FRAME_DESC_TYPE_CSI2,
	.frame_desc = ov495_frame_desc,
};

#endif  /* __CRLMODULE_OV495_CONFIGURATION_H_ */
