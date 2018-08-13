/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/*
 * Copyright (C) 2018 Intel Corporation
 */

#ifndef __CRLMODULE_ADV7481_CVBS_CONFIGURATION_H_
#define __CRLMODULE_ADV7481_CVBS_CONFIGURATION_H_

#include "crlmodule-sensor-ds.h"

static struct crl_register_write_rep adv7481_cvbs_powerup_regset[] = {
	{0x0E, CRL_REG_LEN_08BIT, 0xFF, 0xE0}, //LLC/PIX/AUD/SPI PINS TRISTATED
	{0x0F, CRL_REG_LEN_08BIT, 0x00, 0xF2}, //Exit Power Down Mode
	{0x52, CRL_REG_LEN_08BIT, 0xCD, 0xF2}, //ADI Required Write
	{0x00, CRL_REG_LEN_08BIT, 0x00, 0xF2}, //INSEL = CVBS in on Ain 1
	{0x0E, CRL_REG_LEN_08BIT, 0x80, 0xF2}, //ADI Required Write
	{0x9C, CRL_REG_LEN_08BIT, 0x00, 0xF2}, //ADI Required Write
	{0x9C, CRL_REG_LEN_08BIT, 0xFF, 0xF2}, //ADI Required Write
	{0x0E, CRL_REG_LEN_08BIT, 0x00, 0xF2}, //ADI Required Write
	{0x80, CRL_REG_LEN_08BIT, 0x51, 0xF2}, //ADI Required Write
	{0x81, CRL_REG_LEN_08BIT, 0x51, 0xF2}, //ADI Required Write
	{0x82, CRL_REG_LEN_08BIT, 0x68, 0xF2}, //ADI Required Write
	{0x03, CRL_REG_LEN_08BIT, 0x42, 0xF2}, //Tri-S Output Drivers, PwrDwn 656 pads
	{0x04, CRL_REG_LEN_08BIT, 0x07, 0xF2}, //Power-up INTRQ pad, & Enable SFL
	{0x13, CRL_REG_LEN_08BIT, 0x00, 0xF2}, //ADI Required Write
	{0x17, CRL_REG_LEN_08BIT, 0x41, 0xF2}, //Select SH1
	{0x31, CRL_REG_LEN_08BIT, 0x12, 0xF2}, //ADI Required Write
	{0x10, CRL_REG_LEN_08BIT, 0xC0, 0xE0}, //Enable 1-Lane MIPI Tx, enable pixel output and route SD through Pixel port
	{0x00, CRL_REG_LEN_08BIT, 0x81, 0x90}, //Enable 1-lane MIPI
	{0x00, CRL_REG_LEN_08BIT, 0xA1, 0x90}, //Set Auto DPHY Timing
	{0xF0, CRL_REG_LEN_08BIT, 0x00, 0x94}, //ADI Required Write
	{0xD2, CRL_REG_LEN_08BIT, 0x40, 0x90}, //ADI Required Write
	{0xC4, CRL_REG_LEN_08BIT, 0x0A, 0x90}, //ADI Required Write
	{0x71, CRL_REG_LEN_08BIT, 0x33, 0x90}, //ADI Required Write
	{0x72, CRL_REG_LEN_08BIT, 0x11, 0x90}, //ADI Required Write
	{0xF0, CRL_REG_LEN_08BIT, 0x00, 0x90}, //i2c_dphy_pwdn - 1'b0
	{0x31, CRL_REG_LEN_08BIT, 0x82, 0x90}, //ADI Required Write
	{0x1E, CRL_REG_LEN_08BIT, 0xC0, 0x90}, //ADI Required Write
};


static struct crl_register_write_rep adv7481_cvbs_streamon_regs[] = {
	{0xC1, CRL_REG_LEN_08BIT, 0x2B, 0x90}, //ADI Required Write
	{0x00, CRL_REG_LEN_DELAY, 0x01, 0x00},
	{0xDA, CRL_REG_LEN_08BIT, 0x01, 0x90}, //i2c_mipi_pll_en - 1'b1
	{0x00, CRL_REG_LEN_DELAY, 0x02, 0x00},
	{0x00, CRL_REG_LEN_08BIT, 0x21, 0x90}, //Power-up CSI-TX 21
	{0x00, CRL_REG_LEN_DELAY, 0x01, 0x00},
	{0x31, CRL_REG_LEN_08BIT, 0x80, 0x90}, //ADI Required Write
};

static struct crl_register_write_rep adv7481_cvbs_streamoff_regs[] = {
	{0x31, CRL_REG_LEN_08BIT, 0x82, 0x90}, /* ADI Recommended Write */
	{0x1E, CRL_REG_LEN_08BIT, 0x00, 0x90}, /* Reset the clock Lane */
	{0x00, CRL_REG_LEN_08BIT, 0x81, 0x90},
	{0xDA, CRL_REG_LEN_08BIT, 0x00, 0x90}, /* i2c_mipi_pll_en - 1'b0 Disable MIPI PLL */
	{0xC1, CRL_REG_LEN_08BIT, 0x3B, 0x90},
};

static const s64 adv7481_cvbs_op_sys_clock[] =  {130000000};

static struct crl_pll_configuration adv7481_cvbs_pll_configurations[] = {
	{
		.input_clk = 286363636,
		.op_sys_clk = 216000000,
		.bitsperpixel = 16,
		.pixel_rate_csi = 130000000,
		.pixel_rate_pa = 130000000,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = 0,
		.pll_regs = NULL,
	},
	{
		.input_clk = 24000000,
		.op_sys_clk = 130000000,
		.bitsperpixel = 16,
		.pixel_rate_csi = 130000000,
		.pixel_rate_pa = 130000000,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = 0,
		.pll_regs = NULL,
	 },

};

static struct crl_subdev_rect_rep adv7481_cvbs_ntsc_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 720,
		.in_rect.height = 240,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 720,
		.out_rect.height = 240,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 720,
		.in_rect.height = 240,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 720,
		.out_rect.height = 240,
	},
};

static struct crl_mode_rep adv7481_cvbs_modes[] = {
	{
		.sd_rects_items = ARRAY_SIZE(adv7481_cvbs_ntsc_rects),
		.sd_rects = adv7481_cvbs_ntsc_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 720,
		.height = 240,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = 0,
		.mode_regs = 0,
	},
};

static struct crl_sensor_subdev_config adv7481_cvbs_sensor_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.name = "adv7481 cvbs binner",
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "adv7481 cvbs pixel array",
	},
};

static struct crl_sensor_limits adv7481_cvbs_sensor_limits = {
	.x_addr_min = 0,
	.y_addr_min = 0,
	.x_addr_max = 720,
	.y_addr_max = 240,
	.min_frame_length_lines = 160,
	.max_frame_length_lines = 65535,
	.min_line_length_pixels = 6024,
	.max_line_length_pixels = 32752,
	.scaler_m_min = 1,
	.scaler_m_max = 1,
	.scaler_n_min = 1,
	.scaler_n_max = 1,
	.min_even_inc = 1,
	.max_even_inc = 1,
	.min_odd_inc = 1,
	.max_odd_inc = 1,
};

static struct crl_csi_data_fmt adv7481_cvbs_crl_csi_data_fmt[] = {
	{
		.code = ICI_FORMAT_UYVY,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.bits_per_pixel = 16,
		.regs_items = 0,
		.regs = NULL,
	},
};

static struct crl_ctrl_data adv7481_cvbs_ctrls[] = {
	{
		.sd_type = CRL_SUBDEV_TYPE_BINNER,
		.op_type = CRL_CTRL_SET_OP,
		.context = SENSOR_IDLE,
		.ctrl_id = ICI_EXT_SD_PARAM_ID_LINK_FREQ,
		.name = "CTRL_ID_LINK_FREQ",
		.type = CRL_CTRL_TYPE_MENU_INT,
		.data.int_menu.def = 0,
		.data.int_menu.max = ARRAY_SIZE(adv7481_cvbs_pll_configurations) - 1,
		.data.int_menu.menu = adv7481_cvbs_op_sys_clock,
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
		.data.std_data.max = 0,
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
		.data.std_data.max = 0,
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

static struct crl_sensor_configuration adv7481_cvbs_crl_configuration = {
	.sensor_init = NULL,
	.sensor_cleanup = NULL,

	.onetime_init_regs_items = 0,	//one time initialization is done by HDMI part
	.onetime_init_regs = NULL,

	.powerup_regs_items = ARRAY_SIZE(adv7481_cvbs_powerup_regset),
	.powerup_regs = adv7481_cvbs_powerup_regset,

	.poweroff_regs_items = ARRAY_SIZE(adv7481_cvbs_streamoff_regs),
	.poweroff_regs = adv7481_cvbs_streamoff_regs,

	.id_reg_items = 0,
	.id_regs = NULL,

	.subdev_items = ARRAY_SIZE(adv7481_cvbs_sensor_subdevs),
	.subdevs = adv7481_cvbs_sensor_subdevs,

	.sensor_limits = &adv7481_cvbs_sensor_limits,

	.pll_config_items = ARRAY_SIZE(adv7481_cvbs_pll_configurations),
	.pll_configs = adv7481_cvbs_pll_configurations,
	.op_sys_clk = adv7481_cvbs_op_sys_clock,

	.modes_items = ARRAY_SIZE(adv7481_cvbs_modes),
	.modes = adv7481_cvbs_modes,

	.streamon_regs_items = ARRAY_SIZE(adv7481_cvbs_streamon_regs),
	.streamon_regs = adv7481_cvbs_streamon_regs,

	.streamoff_regs_items = ARRAY_SIZE(adv7481_cvbs_streamoff_regs),
	.streamoff_regs = adv7481_cvbs_streamoff_regs,

	.ctrl_items = ARRAY_SIZE(adv7481_cvbs_ctrls),
	.ctrl_bank = adv7481_cvbs_ctrls,

	.csi_fmts_items = ARRAY_SIZE(adv7481_cvbs_crl_csi_data_fmt),
	.csi_fmts = adv7481_cvbs_crl_csi_data_fmt,

	.addr_len = CRL_ADDR_7BIT,
};

#endif  /* __CRLMODULE_ADV7481_CVBS_CONFIGURATION_H_ */
