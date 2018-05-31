/* SPDX-License_Identifier: GPL-2.0 */
/* Copyright (C) 2017 - 2018 Intel Corporation
 *
 * Author: Alexei Zavjalov <alexei.zavjalov@intel.com>
 *
 */

#ifndef __CRLMODULE_IMX477_COMMON_REGS_H_
#define __CRLMODULE_IMX477_COMMON_REGS_H_

#include "crlmodule-sensor-ds.h"

#define IMX477_CAPTURE_MODE_MAX	8

static struct crl_dynamic_register_access imx477_fll_regs[] = {
	{
		.address = 0x0340,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	},
};

static struct crl_dynamic_register_access imx477_llp_regs[] = {
	{
		.address = 0x0342,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	},
};

static struct crl_dynamic_register_access imx477_exposure_regs[] = {
	{
		.address = 0x0202,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	}
};

static struct crl_dynamic_register_access imx477_ana_gain_global_regs[] = {
	{
		.address = 0x0204,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xfff,
	},
};

static struct crl_dynamic_register_access imx477_wdr_switch_regs[] = {
};

static struct crl_arithmetic_ops imx477_vflip_ops[] = {
	{
		.op = CRL_BITWISE_LSHIFT,
		.operand.entity_val = 1,
	},
};

static struct crl_dynamic_register_access imx477_h_flip_regs[] = {
	{
		.address = 0x0101,
		.len = CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE,
		.ops_items = 0,
		.ops = 0,
		.mask = 0x1,
	},
};

static struct crl_dynamic_register_access imx477_v_flip_regs[] = {
	{
		.address = 0x0101,
		.len = CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE,
		.ops_items = ARRAY_SIZE(imx477_vflip_ops),
		.ops = imx477_vflip_ops,
		.mask = 0x2,
	},
};

static struct crl_dynamic_register_access imx477_test_pattern_regs[] = {
	{
		.address = 0x0600,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	},
};

/* 1st exposure for DOL */
static struct crl_dynamic_register_access imx477_shs1_regs[] = {
	{
		.address = 0x00EA,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	},
};

/* 2nd exposure for DOL */
static struct crl_dynamic_register_access imx477_shs2_regs[] = {
	{
		.address = 0x00EC,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	},
};

/* 3rd exposure for DOL */
static struct crl_dynamic_register_access imx477_shs3_regs[] = {
	{
		.address = 0x00EE,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	},
};

/* Line number of 2nd frame readout start from XVS for DOL */
static struct crl_dynamic_register_access imx477_rhs1_regs[] = {
	{
		.address = 0x00E6,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	},
};

/* Line number of 3rd frame readout start from XVS for DOL */
static struct crl_dynamic_register_access imx477_rhs2_regs[] = {
	{
		.address = 0x00E8,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	},
};

/* set analog gain for 1st HDR frame  */
static struct crl_dynamic_register_access imx477_ana_gain_1st_regs[] = {
	{
		.address = 0x00F0,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	},
};

/* set analog gain for 2nd HDR frames  */
static struct crl_dynamic_register_access imx477_ana_gain_2nd_regs[] = {
	{
		.address = 0x00F2,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	},
};

/* set analog gain for 3rd HDR frames  */
static struct crl_dynamic_register_access imx477_ana_gain_3rd_regs[] = {
	{
		.address = 0x00F4,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	},
};

/* set digital gain for 1st HDR frames  */
static struct crl_dynamic_register_access imx477_dig_gain_1st_regs[] = {
	{
		.address = 0x00F6,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	},
};

/* set digital gain for 2nd HDR frames  */
static struct crl_dynamic_register_access imx477_dig_gain_2nd_regs[] = {
	{
		.address = 0x00F8,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	},
};

/* set digital gain for 3rd HDR frames  */
static struct crl_dynamic_register_access imx477_dig_gain_3rd_regs[] = {
	{
		.address = 0x00FA,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	},
};

static const char * const imx477_test_patterns[] = {
	"Disabled",
	"Solid Colour",
	"Eight Vertical Color Bars",
	"Fade to Grey Color Bars",
	"PN9",
};

static struct crl_v4l2_ctrl imx477_v4l2_ctrls[] = {
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
		.ctrl_id = V4L2_CID_FRAME_LENGTH_LINES,
		.name = "Frame length lines",
		.type = CRL_V4L2_CTRL_TYPE_CUSTOM,
		.data.std_data.min = 720,
		.data.std_data.max = 131071,
		.data.std_data.step = 1,
		.data.std_data.def = 8209,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx477_fll_regs),
		.regs = imx477_fll_regs,
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
		.data.std_data.min = 1280,
		.data.std_data.max = 65535,
		.data.std_data.step = 1,
		.data.std_data.def = 14612,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx477_llp_regs),
		.regs = imx477_llp_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_HFLIP,
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.name = "V4L2_CID_HFLIP",
		.data.std_data.min = 0,
		.data.std_data.max = 1,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx477_h_flip_regs),
		.regs = imx477_h_flip_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_VFLIP,
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.name = "V4L2_CID_VFLIP",
		.data.std_data.min = 0,
		.data.std_data.max = 1,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx477_v_flip_regs),
		.regs = imx477_v_flip_regs,
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
		.data.v4l2_menu_items.menu = imx477_test_patterns,
		.data.v4l2_menu_items.size = ARRAY_SIZE(imx477_test_patterns),
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx477_test_pattern_regs),
		.regs = imx477_test_pattern_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_ANALOGUE_GAIN,
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.name = "V4L2_CID_ANALOGUE_GAIN",
		.data.std_data.min = 0,
		.data.std_data.max = 0x978,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx477_ana_gain_global_regs),
		.regs = imx477_ana_gain_global_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_EXPOSURE,
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.name = "V4L2_CID_EXPOSURE",
		.data.std_data.min = 0,
		.data.std_data.max = 65500,
		.data.std_data.step = 1,
		.data.std_data.def = 5500,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx477_exposure_regs),
		.regs = imx477_exposure_regs,
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
		.regs_items = ARRAY_SIZE(imx477_wdr_switch_regs),
		.regs = imx477_wdr_switch_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = CRL_CID_EXPOSURE_SHS1,
		.type = CRL_V4L2_CTRL_TYPE_CUSTOM,
		.name = "CRL_CID_EXPOSURE_SHS1",
		.data.std_data.min = 4,
		.data.std_data.max = 65500,
		.data.std_data.step = 1,
		.data.std_data.def = 0X5500,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx477_shs1_regs),
		.regs = imx477_shs1_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = CRL_CID_EXPOSURE_SHS2,
		.type = CRL_V4L2_CTRL_TYPE_CUSTOM,
		.name = "CRL_CID_EXPOSURE_SHS2",
		.data.std_data.min = 4,
		.data.std_data.max = 65500,
		.data.std_data.step = 1,
		.data.std_data.def = 0X500,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx477_shs2_regs),
		.regs = imx477_shs2_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = CRL_CID_EXPOSURE_SHS3,
		.type = CRL_V4L2_CTRL_TYPE_CUSTOM,
		.name = "CRL_CID_EXPOSURE_SHS3",
		.data.std_data.min = 4,
		.data.std_data.max = 65500,
		.data.std_data.step = 1,
		.data.std_data.def = 0X1000,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx477_shs3_regs),
		.regs = imx477_shs3_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_BINNER,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = CRL_CID_SENSOR_MODE,
		.name = "CRL_CID_SENSOR_MODE",
		.type = CRL_V4L2_CTRL_TYPE_CUSTOM,
		.data.std_data.min = 0,
		.data.std_data.max = IMX477_CAPTURE_MODE_MAX - 1,
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
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = CRL_CID_EXPOSURE_RHS1,
		.name = "CRL_CID_EXPOSURE_RHS1",
		.type = CRL_V4L2_CTRL_TYPE_CUSTOM,
		.data.std_data.min = 6,
		.data.std_data.max = 65535,
		.data.std_data.step = 1,
		.data.std_data.def = 0x1000,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx477_rhs1_regs),
		.regs = imx477_rhs1_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = CRL_CID_EXPOSURE_RHS2,
		.name = "CRL_CID_EXPOSURE_RHS2",
		.type = CRL_V4L2_CTRL_TYPE_CUSTOM,
		.data.std_data.min = 6,
		.data.std_data.max = 65535,
		.data.std_data.step = 1,
		.data.std_data.def = 0x1500,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx477_rhs2_regs),
		.regs = imx477_rhs2_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = CRL_CID_ANALOG_GAIN_L,
		.name = "CRL_CID_ANALOG_GAIN_L",
		.type = CRL_V4L2_CTRL_TYPE_CUSTOM,
		.data.std_data.min = 0,
		.data.std_data.max = 0x978,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx477_ana_gain_1st_regs),
		.regs = imx477_ana_gain_1st_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = CRL_CID_ANALOG_GAIN_S,
		.name = "CRL_CID_ANALOG_GAIN_S",
		.type = CRL_V4L2_CTRL_TYPE_CUSTOM,
		.data.std_data.min = 0,
		.data.std_data.max = 0x978,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx477_ana_gain_2nd_regs),
		.regs = imx477_ana_gain_2nd_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = CRL_CID_ANALOG_GAIN_VS,
		.name = "CRL_CID_ANALOG_GAIN_VS",
		.type = CRL_V4L2_CTRL_TYPE_CUSTOM,
		.data.std_data.min = 0,
		.data.std_data.max = 0x978,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx477_ana_gain_3rd_regs),
		.regs = imx477_ana_gain_3rd_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = CRL_CID_DIGITAL_GAIN_L,
		.name = "CRL_CID_DIGITAL_GAIN_L",
		.type = CRL_V4L2_CTRL_TYPE_CUSTOM,
		.data.std_data.min = 0,
		.data.std_data.max = 0x978,
		.data.std_data.step = 1,
		.data.std_data.def = 64,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx477_dig_gain_1st_regs),
		.regs = imx477_dig_gain_1st_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = CRL_CID_DIGITAL_GAIN_S,
		.name = "CRL_CID_DIGITAL_GAIN_S",
		.type = CRL_V4L2_CTRL_TYPE_CUSTOM,
		.data.std_data.min = 0,
		.data.std_data.max = 0x978,
		.data.std_data.step = 1,
		.data.std_data.def = 64,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx477_dig_gain_2nd_regs),
		.regs = imx477_dig_gain_2nd_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = CRL_CID_DIGITAL_GAIN_VS,
		.name = "CRL_CID_DIGITAL_GAIN_VS",
		.type = CRL_V4L2_CTRL_TYPE_CUSTOM,
		.data.std_data.min = 0,
		.data.std_data.max = 0x978,
		.data.std_data.step = 1,
		.data.std_data.def = 64,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx477_dig_gain_3rd_regs),
		.regs = imx477_dig_gain_3rd_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
	},
};

static struct crl_register_write_rep imx477_streamon_regs[] = {
	{0x0100, CRL_REG_LEN_08BIT, 0x01},
	{0x00, CRL_REG_LEN_DELAY, 20, 0x00}, /* Delay 20ms */
};

static struct crl_register_write_rep imx477_streamoff_regs[] = {
	{0x0100, CRL_REG_LEN_08BIT, 0x00},
	{0x00, CRL_REG_LEN_DELAY, 20, 0x00}, /* Delay 20ms */
};

static struct crl_register_write_rep imx477_fmt_raw10[] = {
	{0x0112, CRL_REG_LEN_08BIT, 0x0a}, /* FMT RAW10 */
	{0x0113, CRL_REG_LEN_08BIT, 0x0a},
	{0x3F0D, CRL_REG_LEN_08BIT, 0x00},
	{0x00FC, CRL_REG_LEN_08BIT, 0x0A}, /* The output data fmt for CSI: RAW10 */
	{0x00FD, CRL_REG_LEN_08BIT, 0x0A}, /* The output data fmt for CSI: RAW10 */
	{0x00FE, CRL_REG_LEN_08BIT, 0x0A}, /* The output data fmt for CSI: RAW10 */
	{0x00FF, CRL_REG_LEN_08BIT, 0x0A}, /* The output data fmt for CSI: RAW10 */
};

static struct crl_register_write_rep imx477_fmt_raw12[] = {
	{0x0112, CRL_REG_LEN_08BIT, 0x0c}, /* FMT RAW12 */
	{0x0113, CRL_REG_LEN_08BIT, 0x0c},
	{0x3F0D, CRL_REG_LEN_08BIT, 0x01},
};

static struct crl_csi_data_fmt imx477_crl_csi_data_fmt[] = {
	{
		.code = MEDIA_BUS_FMT_SGRBG10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.bits_per_pixel = 10,
		.regs_items = ARRAY_SIZE(imx477_fmt_raw10),
		.regs = imx477_fmt_raw10,
	},
	{
		.code = MEDIA_BUS_FMT_SRGGB10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,
		.bits_per_pixel = 10,
		.regs_items = ARRAY_SIZE(imx477_fmt_raw10),
		.regs = imx477_fmt_raw10,
	},
	{
		.code = MEDIA_BUS_FMT_SBGGR10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_BGGR,
		.bits_per_pixel = 10,
		.regs_items = ARRAY_SIZE(imx477_fmt_raw10),
		.regs = imx477_fmt_raw10,
	},
	{
		.code = MEDIA_BUS_FMT_SGBRG10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_GBRG,
		.bits_per_pixel = 10,
		.regs_items = ARRAY_SIZE(imx477_fmt_raw10),
		.regs = imx477_fmt_raw10,
	},
	{
		.code = MEDIA_BUS_FMT_SRGGB12_1X12,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,
		.bits_per_pixel = 12,
		.regs_items = ARRAY_SIZE(imx477_fmt_raw12),
		.regs = imx477_fmt_raw12,
	},
};

static struct crl_subdev_rect_rep imx477_4056_3040_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4056,
		.in_rect.height = 3040,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 4056,
		.out_rect.height = 3040,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4056,
		.in_rect.height = 3040,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 4056,
		.out_rect.height = 3040,
	}
};

static struct crl_subdev_rect_rep imx477_4056_2288_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4056,
		.in_rect.height = 3040,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 4056,
		.out_rect.height = 3040,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4056,
		.in_rect.height = 3040,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 4056,
		.out_rect.height = 2288,
	}
};

static struct crl_subdev_rect_rep imx477_2832_1632_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4056,
		.in_rect.height = 3040,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 4056,
		.out_rect.height = 3040,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4056,
		.in_rect.height = 3040,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 2832,
		.out_rect.height = 1632,
	}
};

static struct crl_subdev_rect_rep imx477_2028_1128_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4056,
		.in_rect.height = 3040,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 4056,
		.out_rect.height = 3040,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4056,
		.in_rect.height = 3040,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 2028,
		.out_rect.height = 1128,
	}
};

static struct crl_subdev_rect_rep imx477_1296_768_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4056,
		.in_rect.height = 3040,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 4056,
		.out_rect.height = 3040,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4056,
		.in_rect.height = 3040,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1296,
		.out_rect.height = 768,
	}
};

static struct crl_subdev_rect_rep imx477_656_512_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4056,
		.in_rect.height = 3040,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 4056,
		.out_rect.height = 3040,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4056,
		.in_rect.height = 3040,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 656,
		.out_rect.height = 512,
	}
};

static struct crl_register_write_rep imx477_pll_1200mbps[] = {
	/* MIPI Settings */
	{0x0114, CRL_REG_LEN_08BIT, 0x01}, /* 2-lane Mode */

	/* Clock Setting */
	{0x0301, CRL_REG_LEN_08BIT, 0x05}, /* The Pixel Clock Divider for IVTS   */
	{0x0303, CRL_REG_LEN_08BIT, 0x02}, /* The System Clock Divider for IVTS  */
	{0x0305, CRL_REG_LEN_08BIT, 0x03}, /* The pre-PLL Clock Divider for IVTS */
	{0x0306, CRL_REG_LEN_08BIT, 0x01}, /* The PLL multiplier for IVTS [10:8] */
	{0x0307, CRL_REG_LEN_08BIT, 0x48}, /* The PLL multiplier for IVTS [7:0]  */
	{0x0309, CRL_REG_LEN_08BIT, 0x0A}, /* The Pixel Clock Divider for IOPS   */
	{0x030B, CRL_REG_LEN_08BIT, 0x01}, /* The System Clock Divider for IOPS  */
	{0x030D, CRL_REG_LEN_08BIT, 0x02}, /* The pre-PLL Clock Divider for IOPS */
	{0x030E, CRL_REG_LEN_08BIT, 0x00}, /* The PLL multiplier for IOPS [10:8] */
	{0x030F, CRL_REG_LEN_08BIT, 0x7D}, /* The PLL multiplier for IOPS [7:0]  */
	{0x0310, CRL_REG_LEN_08BIT, 0x01}, /* PLL mode select: Dual Mode         */
	{0x0820, CRL_REG_LEN_08BIT, 0x09}, /* Output Data Rate, Mbps [31:24]     */
	{0x0821, CRL_REG_LEN_08BIT, 0x60}, /* Output Data Rate, Mbps [23:16]     */
	{0x0822, CRL_REG_LEN_08BIT, 0x00}, /* Output Data Rate, Mbps [15:8]      */
	{0x0823, CRL_REG_LEN_08BIT, 0x00}, /* Output Data Rate, Mbps [7:0]       */

	/* Global Timing Setting */
	{0x080A, CRL_REG_LEN_08BIT, 0x00}, /* MIPI Global Timing (Tclk) [9:8]    */
	{0x080B, CRL_REG_LEN_08BIT, 0x87}, /* MIPI Global Timing (Tclk) [7:0]    */
	{0x080C, CRL_REG_LEN_08BIT, 0x00}, /* MIPI Global Timing (ths_prepare)   */
	{0x080D, CRL_REG_LEN_08BIT, 0x4F}, /* MIPI Global Timing (ths_prepare)   */
	{0x080E, CRL_REG_LEN_08BIT, 0x00}, /* MIPI Global Timing (ths_zero_min)  */
	{0x080F, CRL_REG_LEN_08BIT, 0x87}, /* MIPI Global Timing (ths_zero_min)  */
	{0x0810, CRL_REG_LEN_08BIT, 0x00}, /* MIPI Global Timing (ths_trail)     */
	{0x0811, CRL_REG_LEN_08BIT, 0x5F}, /* MIPI Global Timing (ths_trail)     */
	{0x0812, CRL_REG_LEN_08BIT, 0x00}, /* MIPI Global Timing (Tclk_trail_min)*/
	{0x0813, CRL_REG_LEN_08BIT, 0x5F}, /* MIPI Global Timing (Tclk_trail_min)*/
	{0x0814, CRL_REG_LEN_08BIT, 0x00}, /* MIPI Global Timing (Tclk_prepare)  */
	{0x0815, CRL_REG_LEN_08BIT, 0x4F}, /* MIPI Global Timing (Tclk_prepare)  */
	{0x0816, CRL_REG_LEN_08BIT, 0x01}, /* MIPI Global Timing (Tclk_zero)     */
	{0x0817, CRL_REG_LEN_08BIT, 0x3F}, /* MIPI Global Timing (Tclk_zero)     */
	{0x0818, CRL_REG_LEN_08BIT, 0x00}, /* MIPI Global Timing (Tlpx)          */
	{0x0819, CRL_REG_LEN_08BIT, 0x3F}, /* MIPI Global Timing (Tlpx)          */
	{0xE04C, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0xE04D, CRL_REG_LEN_08BIT, 0x87}, /* Undocumented */
	{0xE04E, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0xE04F, CRL_REG_LEN_08BIT, 0x1F}, /* Undocumented */

	/* Output Data Select Setting */
	{0x3E20, CRL_REG_LEN_08BIT, 0x01}, /* Undocumented */
	{0x3E37, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */

	/* PowerSave Setting */
	{0x3F50, CRL_REG_LEN_08BIT, 0x00}, /* Power save: Disable */
	{0x3F56, CRL_REG_LEN_08BIT, 0x01},
	{0x3F57, CRL_REG_LEN_08BIT, 0x4F},
};

static struct crl_pll_configuration imx477_pll_configurations[] = {
	{
		.input_clk = 19200000,
		.op_sys_clk = 600000000, /* 1200mbps / 2 */
		.bitsperpixel = 10,
		.pixel_rate_csi = 240000000,
		 /* pixel_rate = (MIPICLK*2 * CSILANES)/10 */
		.pixel_rate_pa = 240000000,
		.csi_lanes = 2,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = ARRAY_SIZE(imx477_pll_1200mbps),
		.pll_regs = imx477_pll_1200mbps,
	},
	{
		.input_clk = 19200000,
		.op_sys_clk = 600000000, /* 1200mbps / 2 */
		.bitsperpixel = 12,
		.pixel_rate_csi = 240000000,
		 /* pixel_rate = (MIPICLK*2 * CSILANES)/10 */
		.pixel_rate_pa = 240000000,
		.csi_lanes = 2,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = ARRAY_SIZE(imx477_pll_1200mbps),
		.pll_regs = imx477_pll_1200mbps,
	},
};

static struct crl_sensor_subdev_config imx477_sensor_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.name = "imx477 binner",
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "imx477 pixel array",
	}
};

static struct crl_sensor_limits imx477_sensor_limits = {
	.x_addr_min = 0,
	.y_addr_min = 0,
	.x_addr_max = 4056,
	.y_addr_max = 3040,
	.min_frame_length_lines = 320,
	.max_frame_length_lines = 65535,
	.min_line_length_pixels = 380,
	.max_line_length_pixels = 32752,
};

static struct crl_sensor_detect_config imx477_sensor_detect_regset[] = {
	{
		.reg = { 0x0016, CRL_REG_LEN_08BIT, 0x000000ff },
		.width = 7,
	},
	{
		.reg = { 0x0017, CRL_REG_LEN_08BIT, 0x000000ff },
		.width = 7,
	}
};

static struct crl_register_write_rep imx477_powerup_standby[] = {
	{0x0100, CRL_REG_LEN_08BIT, 0x00},
	{0x00, CRL_REG_LEN_DELAY, 20, 0x00}, /* Delay 20ms */
};

/* Power items, they are enabled in the order they are listed here */
static struct crl_power_seq_entity imx477_power_items[] = {
	{
		.type = CRL_POWER_ETY_CLK_FRAMEWORK,
		.val = 19200000,
	},
	{
		.type = CRL_POWER_ETY_GPIO_FROM_PDATA,
		.val = 1,
		.undo_val = 1,
	},
};

static struct crl_arithmetic_ops imx477_frame_desc_width_ops[] = {
	{
		.op = CRL_ASSIGNMENT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_VAR_REF,
		.operand.entity_val = CRL_VAR_REF_OUTPUT_WIDTH,
	},
};

static struct crl_arithmetic_ops imx477_frame_desc_height_ops[] = {
	{
		.op = CRL_ASSIGNMENT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
		.operand.entity_val = 1,
	},
};

static struct crl_frame_desc imx477_frame_desc[] = {
	{
		.flags.entity_val = 0,
		.bpp.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_VAR_REF,
		.bpp.entity_val = CRL_VAR_REF_BITSPERPIXEL,
		.pixelcode.entity_val = MEDIA_BUS_FMT_FIXED,
		.length.entity_val = 0,
		.start_line.entity_val = 0,
		.start_pixel.entity_val = 0,
		.width = {
			.ops_items = ARRAY_SIZE(imx477_frame_desc_width_ops),
			.ops = imx477_frame_desc_width_ops,
		},
		.height = {
			.ops_items = ARRAY_SIZE(imx477_frame_desc_height_ops),
			.ops = imx477_frame_desc_height_ops,
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
			.ops_items = ARRAY_SIZE(imx477_frame_desc_width_ops),
			.ops = imx477_frame_desc_width_ops,
		},
		.height = {
			.ops_items = ARRAY_SIZE(imx477_frame_desc_height_ops),
			.ops = imx477_frame_desc_height_ops,
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
			.ops_items = ARRAY_SIZE(imx477_frame_desc_width_ops),
			.ops = imx477_frame_desc_width_ops,
		},
		.height = {
			.ops_items = ARRAY_SIZE(imx477_frame_desc_height_ops),
			.ops = imx477_frame_desc_height_ops,
		},
		.csi2_channel.entity_val = 2,
		.csi2_data_type.entity_val = 0x12,
	},
};

#endif  /* __CRLMODULE_IMX477_COMMON_REGS_H_ */
