/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2017 - 2018 Intel Corporation
 *
 * Author: Kishore Bodke <kishore.k.bodke@intel.com>
 *
 */

#ifndef __CRLMODULE_MAGNA_CONFIGURATION_H_
#define __CRLMODULE_MAGNA_CONFIGURATION_H_

#include "crlmodule-sensor-ds.h"

static struct crl_pll_configuration magna_pll_configurations[] = {
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

static struct crl_subdev_rect_rep magna_1280_720_rects[] = {
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

static struct crl_mode_rep magna_modes[] = {
	{
		.sd_rects_items = ARRAY_SIZE(magna_1280_720_rects),
		.sd_rects = magna_1280_720_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 1280,
		.height = 720,
		.min_llp = 2250,
		.min_fll = 1320,
	},
};

static struct crl_sensor_subdev_config magna_sensor_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.name = "magna binner",
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "magna pixel array",
	}
};

static struct crl_sensor_limits magna_sensor_limits = {
	.x_addr_min = 0,
	.y_addr_min = 0,
	.x_addr_max = 1280,
	.y_addr_max = 720,
	.min_frame_length_lines = 240,
	.max_frame_length_lines = 65535,
	.min_line_length_pixels = 320,
	.max_line_length_pixels = 32752,
};

static struct crl_csi_data_fmt magna_crl_csi_data_fmt[] = {
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

static struct crl_v4l2_ctrl magna_v4l2_ctrls[] = {
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

struct crl_sensor_configuration magna_crl_configuration = {

	.subdev_items = ARRAY_SIZE(magna_sensor_subdevs),
	.subdevs = magna_sensor_subdevs,

	.pll_config_items = ARRAY_SIZE(magna_pll_configurations),
	.pll_configs = magna_pll_configurations,

	.sensor_limits = &magna_sensor_limits,

	.modes_items = ARRAY_SIZE(magna_modes),
	.modes = magna_modes,

	.streamon_regs_items = 0,
	.streamon_regs = 0,

	.streamoff_regs_items = 0,
	.streamoff_regs = 0,

	.v4l2_ctrls_items = ARRAY_SIZE(magna_v4l2_ctrls),
	.v4l2_ctrl_bank = magna_v4l2_ctrls,

	.csi_fmts_items = ARRAY_SIZE(magna_crl_csi_data_fmt),
	.csi_fmts = magna_crl_csi_data_fmt,

};

#endif  /* __CRLMODULE_MAGNA_CONFIGURATION_H_ */
