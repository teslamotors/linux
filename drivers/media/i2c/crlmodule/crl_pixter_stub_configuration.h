/* SPDX-License_Identifier: GPL-2.0 */
/* Copyright (C) 2016 - 2018 Intel Corporation
 *
 * Author: Wang, Zaikuo <zaikuo.wang@intel.com>
 *
 */

#ifndef __CRLMODULE_PIXTER_STUB_CONFIGURATION_H_
#define __CRLMODULE_PIXTER_STUB_CONFIGURATION_H_

#include "crlmodule-sensor-ds.h"

static struct crl_pll_configuration pixter_stub_pll_configurations[] = {
	{
		.input_clk = 24000000,
		.op_sys_clk = 400000000,
		.bitsperpixel = 8,
		.pixel_rate_csi = 800000000,
		.pixel_rate_pa = 800000000,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = 0,
		.pll_regs = NULL,
	},
	{
		.input_clk = 24000000,
		.op_sys_clk = 400000000,
		.bitsperpixel = 10,
		.pixel_rate_csi = 800000000,
		.pixel_rate_pa = 800000000,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = 0,
		.pll_regs = NULL,
	},
	{
		.input_clk = 24000000,
		.op_sys_clk = 400000000,
		.bitsperpixel = 12,
		.pixel_rate_csi = 800000000,
		.pixel_rate_pa = 800000000,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = 0,
		.pll_regs = NULL,
	},
};

static struct crl_subdev_rect_rep pixter_stub_720p_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect = { 0, 0, 100000, 100000 },
		.out_rect = { 0, 0, 100000, 100000 },
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.in_rect = { 0, 0, 100000, 100000 },
		.out_rect = { 0, 0, 1280, 720 },
	},
};

static struct crl_subdev_rect_rep pixter_stub_1080p_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect = { 0, 0, 100000, 100000 },
		.out_rect = { 0, 0, 100000, 100000 },
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.in_rect = { 0, 0, 100000, 100000 },
		.out_rect = { 0, 0, 1920, 1080 },
	},
};

static struct crl_subdev_rect_rep pixter_stub_1440p_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect = { 0, 0, 100000, 100000 },
		.out_rect = { 0, 0, 100000, 100000 },
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.in_rect = { 0, 0, 100000, 100000 },
		.out_rect = { 0, 0, 2560, 1440 },
	},
};

static struct crl_subdev_rect_rep pixter_stub_1836p_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect = { 0, 0, 100000, 100000 },
		.out_rect = { 0, 0, 100000, 100000 },
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.in_rect = { 0, 0, 100000, 100000 },
		.out_rect = { 0, 0, 3264, 1836 },
	},
};

static struct crl_subdev_rect_rep pixter_stub_1920p_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect = { 0, 0, 100000, 100000 },
		.out_rect = { 0, 0, 100000, 100000 },
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.in_rect = { 0, 0, 100000, 100000 },
		.out_rect = { 0, 0, 2560, 1920 },
	},
};

static struct crl_subdev_rect_rep pixter_stub_2304p_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect = { 0, 0, 100000, 100000 },
		.out_rect = { 0, 0, 100000, 100000 },
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.in_rect = { 0, 0, 100000, 100000 },
		.out_rect = { 0, 0, 4096, 2304 },
	},
};

static struct crl_subdev_rect_rep pixter_stub_2448p_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect = { 0, 0, 100000, 100000 },
		.out_rect = { 0, 0, 100000, 100000 },
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.in_rect = { 0, 0, 100000, 100000 },
		.out_rect = { 0, 0, 3264, 2448 },
	},
};

static struct crl_subdev_rect_rep pixter_stub_13m_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect = { 0, 0, 100000, 100000 },
		.out_rect = { 0, 0, 100000, 100000 },
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.in_rect = { 0, 0, 100000, 100000 },
		.out_rect = { 0, 0, 4096, 3072 },
	},
};

static struct crl_mode_rep pixter_stub_modes[] = {
	{
		.sd_rects_items = ARRAY_SIZE(pixter_stub_720p_rects),
		.sd_rects = pixter_stub_720p_rects,
		.scale_m = 78,
		.width = 1280,
		.height = 720,
		.min_llp = 6024,
		.min_fll = 1660,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = 0,
		.mode_regs = NULL,
	},
	{
		.sd_rects_items = ARRAY_SIZE(pixter_stub_1080p_rects),
		.sd_rects = pixter_stub_1080p_rects,
		.scale_m = 52,
		.width = 1920,
		.height = 1080,
		.min_llp = 6024,
		.min_fll = 1660,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = 0,
		.mode_regs = NULL,
	},
	{
		.sd_rects_items = ARRAY_SIZE(pixter_stub_1440p_rects),
		.sd_rects = pixter_stub_1440p_rects,
		.scale_m = 39,
		.width = 2560,
		.height = 1440,
		.min_llp = 6024,
		.min_fll = 1660,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = 0,
		.mode_regs = NULL,
	},
	{
		.sd_rects_items = ARRAY_SIZE(pixter_stub_1836p_rects),
		.sd_rects = pixter_stub_1836p_rects,
		.scale_m = 30,
		.width = 3264,
		.height = 1836,
		.min_llp = 6024,
		.min_fll = 1900,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = 0,
		.mode_regs = NULL,
	},
	{
		.sd_rects_items = ARRAY_SIZE(pixter_stub_1920p_rects),
		.sd_rects = pixter_stub_1920p_rects,
		.scale_m = 39,
		.width = 2560,
		.height = 1920,
		.min_llp = 6024,
		.min_fll = 2000,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = 0,
		.mode_regs = NULL,
	},
	{
		.sd_rects_items = ARRAY_SIZE(pixter_stub_2304p_rects),
		.sd_rects = pixter_stub_2304p_rects,
		.scale_m = 24,
		.width = 4096,
		.height = 2304,
		.min_llp = 6024,
		.min_fll = 2400,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = 0,
		.mode_regs = NULL,
	},
	{
		.sd_rects_items = ARRAY_SIZE(pixter_stub_2448p_rects),
		.sd_rects = pixter_stub_2448p_rects,
		.scale_m = 30,
		.width = 3264,
		.height = 2448,
		.min_llp = 6024,
		.min_fll = 2600,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = 0,
		.mode_regs = NULL,
	},
	{
		.sd_rects_items = ARRAY_SIZE(pixter_stub_13m_rects),
		.sd_rects = pixter_stub_13m_rects,
		.scale_m = 24,
		.width = 4096,
		.height = 3072,
		.min_llp = 6024,
		.min_fll = 3200,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = 0,
		.mode_regs = NULL,
	},
};

static struct crl_sensor_subdev_config pixter_stub_sensor_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.name = "pixter_stub scaler",
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "pixter_stub pixel array",
	},
};

static struct crl_sensor_subdev_config pixter_stub_b_sensor_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.name = "pixter_stubB scaler",
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "pixter_stubB pixel array",
	},
};

static struct crl_sensor_subdev_config pixter_stub_c_sensor_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.name = "pixter_stubC scaler",
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "pixter_stubC pixel array",
	},
};

static struct crl_sensor_subdev_config pixter_stub_d_sensor_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.name = "pixter_stubD scaler",
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "pixter_stubD pixel array",
	},
};

static struct crl_sensor_subdev_config pixter_stub_e_sensor_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.name = "pixter_stubE scaler",
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "pixter_stubE pixel array",
	},
};

static struct crl_sensor_subdev_config pixter_stub_f_sensor_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.name = "pixter_stubF scaler",
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "pixter_stubF pixel array",
	},
};

static struct crl_sensor_subdev_config pixter_stub_g_sensor_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.name = "pixter_stubG scaler",
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "pixter_stubG pixel array",
	},
};

static struct crl_sensor_subdev_config pixter_stub_h_sensor_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.name = "pixter_stubH scaler",
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "pixter_stubH pixel array",
	},
};

static struct crl_sensor_limits pixter_stub_sensor_limits = {
	.x_addr_min = 0,
	.y_addr_min = 0,
	.x_addr_max = 100000,
	.y_addr_max = 100000,
	.min_frame_length_lines = 160,
	.max_frame_length_lines = 100000,
	.min_line_length_pixels = 6024,
	.max_line_length_pixels = 100000,
	.scaler_m_min = 1,
	.scaler_m_max = 1,
	.scaler_n_min = 1,
	.scaler_n_max = 1,
	.min_even_inc = 1,
	.max_even_inc = 1,
	.min_odd_inc = 1,
	.max_odd_inc = 1,
};

static struct crl_flip_data pixter_stub_flip_configurations[] = {
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
		.pixel_order = CRL_PIXEL_ORDER_GBRG,
	},
	{
		.flip = CRL_FLIP_HFLIP_VFLIP,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,
	},
};

static struct crl_csi_data_fmt pixter_stub_crl_csi_data_fmt[] = {
	{
		.code = MEDIA_BUS_FMT_SGRBG8_1X8,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.bits_per_pixel = 8,
	},
	{
		.code = MEDIA_BUS_FMT_SRGGB8_1X8,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,
		.bits_per_pixel = 8,
	},
	{
		.code = MEDIA_BUS_FMT_SBGGR8_1X8,
		.pixel_order = CRL_PIXEL_ORDER_BGGR,
		.bits_per_pixel = 8,
	},
	{
		.code = MEDIA_BUS_FMT_SGBRG8_1X8,
		.pixel_order = CRL_PIXEL_ORDER_GBRG,
		.bits_per_pixel = 8,
	},
	{
		.code = MEDIA_BUS_FMT_UYVY8_1X16,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.bits_per_pixel = 16,
	},
	{
		.code = MEDIA_BUS_FMT_YUYV8_1X16,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.bits_per_pixel = 16,
	},
	{
		.code = MEDIA_BUS_FMT_RGB565_1X16,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.bits_per_pixel = 16,
	},
	{
		.code = MEDIA_BUS_FMT_RGB888_1X24,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.bits_per_pixel = 24,
	},
	{
		.code = MEDIA_BUS_FMT_SGRBG10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.bits_per_pixel = 10,
	},
	{
		.code = MEDIA_BUS_FMT_SRGGB10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,
		.bits_per_pixel = 10,
	},
	{
		.code = MEDIA_BUS_FMT_SBGGR10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_BGGR,
		.bits_per_pixel = 10,
	},
	{
		.code = MEDIA_BUS_FMT_SGBRG10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_GBRG,
		.bits_per_pixel = 10,
	},
	{
		.code = MEDIA_BUS_FMT_SGRBG12_1X12,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.bits_per_pixel = 12,
	},
	{
		.code = MEDIA_BUS_FMT_SRGGB12_1X12,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,
		.bits_per_pixel = 12,
	},
	{
		.code = MEDIA_BUS_FMT_SBGGR12_1X12,
		.pixel_order = CRL_PIXEL_ORDER_BGGR,
		.bits_per_pixel = 12,
	},
	{
		.code = MEDIA_BUS_FMT_SGBRG12_1X12,
		.pixel_order = CRL_PIXEL_ORDER_GBRG,
		.bits_per_pixel = 12,
	},
};

static struct crl_v4l2_ctrl pixter_stub_v4l2_ctrls[] = {
	{
		.sd_type = CRL_SUBDEV_TYPE_SCALER,
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
		.ctrl = 0,
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
		.data.std_data.max = 800000000,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = 0,
		.regs = 0,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_SCALER,
		.op_type = CRL_V4L2_CTRL_GET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_PIXEL_RATE,
		.name = "V4L2_CID_PIXEL_RATE_CSI",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = 800000000,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
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
		.regs_items = 0,
		.regs = 0,
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
		.regs_items = 0,
		.regs = 0,
		.dep_items = 0,
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
		.regs_items = 0,
		.regs = 0,
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
		.regs_items = 0,
		.regs = 0,
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
		.data.std_data.min = 160,
		.data.std_data.max = 65535,
		.data.std_data.step = 1,
		.data.std_data.def = 4130,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = 0,
		.regs = 0,
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
		.data.std_data.min = 6024,
		.data.std_data.max = 65520,
		.data.std_data.step = 1,
		.data.std_data.def = 6024,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = 0,
		.regs = 0,
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
		.regs_items = 0,
		.regs = 0,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
};

#define DEFINE_PIXTER_CRL_CONFIGURATION(port) \
static struct crl_sensor_configuration pixter_##port##_crl_configuration = { \
	.powerup_regs_items = 0, \
	.powerup_regs = NULL, \
\
	.poweroff_regs_items = 0, \
	.poweroff_regs = NULL, \
\
	.id_reg_items = 0, \
	.id_regs = NULL, \
\
	.subdev_items = ARRAY_SIZE(pixter_##port##_sensor_subdevs), \
	.subdevs = pixter_##port##_sensor_subdevs, \
\
	.sensor_limits = &pixter_stub_sensor_limits, \
\
	.pll_config_items = ARRAY_SIZE(pixter_stub_pll_configurations), \
	.pll_configs = pixter_stub_pll_configurations, \
\
	.modes_items = ARRAY_SIZE(pixter_stub_modes), \
	.modes = pixter_stub_modes, \
\
	.streamon_regs_items = 0, \
	.streamon_regs = NULL, \
\
	.streamoff_regs_items = 0, \
	.streamoff_regs = NULL, \
\
	.v4l2_ctrls_items = ARRAY_SIZE(pixter_stub_v4l2_ctrls), \
	.v4l2_ctrl_bank = pixter_stub_v4l2_ctrls, \
\
	.flip_items = ARRAY_SIZE(pixter_stub_flip_configurations), \
	.flip_data = pixter_stub_flip_configurations, \
\
	.csi_fmts_items = ARRAY_SIZE(pixter_stub_crl_csi_data_fmt), \
	.csi_fmts = pixter_stub_crl_csi_data_fmt, \
}
DEFINE_PIXTER_CRL_CONFIGURATION(stub);
DEFINE_PIXTER_CRL_CONFIGURATION(stub_b);
DEFINE_PIXTER_CRL_CONFIGURATION(stub_c);
DEFINE_PIXTER_CRL_CONFIGURATION(stub_d);
DEFINE_PIXTER_CRL_CONFIGURATION(stub_e);
DEFINE_PIXTER_CRL_CONFIGURATION(stub_f);
DEFINE_PIXTER_CRL_CONFIGURATION(stub_g);
DEFINE_PIXTER_CRL_CONFIGURATION(stub_h);


#endif  /* __CRLMODULE_PIXTER_STUB_CONFIGURATION_H_ */
