/*
 * Copyright (c) 2015--2016 Intel Corporation.
 *
 * Author: Wang, Zaikuo <zaikuo.wang@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __CRLMODULE_IMX135_IPU5_FPGA_CONFIGURATION_H_
#define __CRLMODULE_IMX135_IPU5_FPGA_CONFIGURATION_H_

#include "crlmodule-sensor-ds.h"

static struct crl_register_write_rep imx135_ipu5_fpga_pll_168mbps[] = {
	{ 0x011e, CRL_REG_LEN_08BIT, 0x13 },
	{ 0x011f, CRL_REG_LEN_08BIT, 0x33 },
	{ 0x0301, CRL_REG_LEN_08BIT, 0x05 },
	{ 0x0303, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0305, CRL_REG_LEN_08BIT, 0x0B },
	{ 0x0309, CRL_REG_LEN_08BIT, 0x05 },
	{ 0x030b, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x030c, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x030d, CRL_REG_LEN_08BIT, 0x6a },
	{ 0x030e, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x3a06, CRL_REG_LEN_08BIT, 0x11 },
	{ 0x0830, CRL_REG_LEN_08BIT, 0x57 },
	{ 0x0831, CRL_REG_LEN_08BIT, 0x0F },
	{ 0x0832, CRL_REG_LEN_08BIT, 0x2f },
	{ 0x0833, CRL_REG_LEN_08BIT, 0x17 },
	{ 0x0834, CRL_REG_LEN_08BIT, 0x0F },
	{ 0x0835, CRL_REG_LEN_08BIT, 0x0F },
	{ 0x0836, CRL_REG_LEN_08BIT, 0x3F },
	{ 0x0837, CRL_REG_LEN_08BIT, 0x1F },
	{ 0x0839, CRL_REG_LEN_08BIT, 0x1F },
	{ 0x083a, CRL_REG_LEN_08BIT, 0x17 },
	{ 0x083b, CRL_REG_LEN_08BIT, 0x02 },
};

static struct crl_register_write_rep imx135_ipu5_fpga_powerup_regset[] = {
	{ 0x0103, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0105, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0110, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0220, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x3302, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x3833, CRL_REG_LEN_08BIT, 0x20 },
	{ 0x3893, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3906, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x3907, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x391B, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x3C09, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x600A, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3008, CRL_REG_LEN_08BIT, 0xB0 },
	{ 0x320A, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x320D, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x3216, CRL_REG_LEN_08BIT, 0x2E },
	{ 0x322C, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x3409, CRL_REG_LEN_08BIT, 0x0C },
	{ 0x340C, CRL_REG_LEN_08BIT, 0x2D },
	{ 0x3411, CRL_REG_LEN_08BIT, 0x39 },
	{ 0x3414, CRL_REG_LEN_08BIT, 0x1E },
	{ 0x3427, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x3480, CRL_REG_LEN_08BIT, 0x1E },
	{ 0x3484, CRL_REG_LEN_08BIT, 0x1E },
	{ 0x3488, CRL_REG_LEN_08BIT, 0x1E },
	{ 0x348C, CRL_REG_LEN_08BIT, 0x1E },
	{ 0x3490, CRL_REG_LEN_08BIT, 0x1E },
	{ 0x3494, CRL_REG_LEN_08BIT, 0x1E },
	{ 0x3511, CRL_REG_LEN_08BIT, 0x8F },
	{ 0x364F, CRL_REG_LEN_08BIT, 0x2D },
	{ 0x0700, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3a63, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4100, CRL_REG_LEN_08BIT, 0xf8 },
	{ 0x4203, CRL_REG_LEN_08BIT, 0xff },
	{ 0x4344, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x441c, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x020e, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x020f, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0210, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0211, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0212, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0213, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0214, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0215, CRL_REG_LEN_08BIT, 0x00 },
};

static struct crl_register_write_rep imx135_ipu5_fpga_mode_1080p[] = {
	{ 0x0108, CRL_REG_LEN_08BIT, 0x03 }, /* lanes */
	{ 0x0381, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0383, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0385, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0387, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0390, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0391, CRL_REG_LEN_08BIT, 0x22 },
	{ 0x0392, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0401, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x0404, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0405, CRL_REG_LEN_08BIT, 0x11 },
	{ 0x4082, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4083, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x7006, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x0344, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0345, CRL_REG_LEN_08BIT, 0x2A },
	{ 0x0346, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0347, CRL_REG_LEN_08BIT, 0x8C },
	{ 0x0348, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x0349, CRL_REG_LEN_08BIT, 0x45 },
	{ 0x034A, CRL_REG_LEN_08BIT, 0x0A },
	{ 0x034B, CRL_REG_LEN_08BIT, 0xA7 },
	{ 0x034C, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x034D, CRL_REG_LEN_08BIT, 0x80 },
	{ 0x034E, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x034F, CRL_REG_LEN_08BIT, 0x38 },
	{ 0x0350, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0351, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0352, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0353, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0354, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x0355, CRL_REG_LEN_08BIT, 0x0E },
	{ 0x0356, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x0357, CRL_REG_LEN_08BIT, 0x8E },
	{ 0x301D, CRL_REG_LEN_08BIT, 0x30 },
	{ 0x3310, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x3311, CRL_REG_LEN_08BIT, 0x94 },
	{ 0x3312, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x3313, CRL_REG_LEN_08BIT, 0x48 },
	{ 0x331C, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x331D, CRL_REG_LEN_08BIT, 0xC8 },
	{ 0x4084, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x4085, CRL_REG_LEN_08BIT, 0x94 },
	{ 0x4086, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x4087, CRL_REG_LEN_08BIT, 0x48 },
	{ 0x4400, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0230, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0231, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0233, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0234, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0235, CRL_REG_LEN_08BIT, 0x40 },
	{ 0x0238, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0239, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x023B, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x023C, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x33B0, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x33B1, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x33B3, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x33B4, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x3800, CRL_REG_LEN_08BIT, 0x00 },
};

static struct crl_register_write_rep imx135_ipu5_fpga_mode_720p[] = {
	{ 0x0108, CRL_REG_LEN_08BIT, 0x03 }, /* lanes */
	{ 0x0381, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0383, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0385, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0387, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0390, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0391, CRL_REG_LEN_08BIT, 0x22 },
	{ 0x0392, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0401, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x0404, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0405, CRL_REG_LEN_08BIT, 0x1A },
	{ 0x4082, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4083, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x7006, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x0344, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0345, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x0346, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0347, CRL_REG_LEN_08BIT, 0x7C },
	{ 0x0348, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x0349, CRL_REG_LEN_08BIT, 0x6B },
	{ 0x034A, CRL_REG_LEN_08BIT, 0x0A },
	{ 0x034B, CRL_REG_LEN_08BIT, 0xB3 },
	{ 0x034C, CRL_REG_LEN_08BIT, 0x05 },
	{ 0x034D, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x034E, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x034F, CRL_REG_LEN_08BIT, 0xD0 },
	{ 0x0350, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0351, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0352, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0353, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0354, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x0355, CRL_REG_LEN_08BIT, 0x34 },
	{ 0x0356, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x0357, CRL_REG_LEN_08BIT, 0x9C },
	{ 0x301D, CRL_REG_LEN_08BIT, 0x30 },
	{ 0x3310, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x3311, CRL_REG_LEN_08BIT, 0x94 },
	{ 0x3312, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x3313, CRL_REG_LEN_08BIT, 0x48 },
	{ 0x331C, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x331D, CRL_REG_LEN_08BIT, 0xC8 },
	{ 0x4084, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x4085, CRL_REG_LEN_08BIT, 0x94 },
	{ 0x4086, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x4087, CRL_REG_LEN_08BIT, 0x48 },
	{ 0x4400, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0230, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0231, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0233, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0234, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0235, CRL_REG_LEN_08BIT, 0x40 },
	{ 0x0238, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0239, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x023B, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x023C, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x33B0, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x33B1, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x33B3, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x33B4, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x3800, CRL_REG_LEN_08BIT, 0x00 },
};

static struct crl_register_write_rep imx135_ipu5_fpga_streamon_regs[] = {
	{ 0x0100, CRL_REG_LEN_08BIT, 0x01 },
};

static struct crl_register_write_rep imx135_ipu5_fpga_streamoff_regs[] = {
	{ 0x0100, CRL_REG_LEN_08BIT, 0x00 }
};

static struct crl_register_write_rep imx135_ipu5_fpga_data_fmt_width10[] = {
	{ 0x0112, CRL_REG_LEN_16BIT, 0x0a0a }
};

static struct crl_register_write_rep imx135_ipu5_fpga_data_fmt_width8[] = {
	{ 0x0112, CRL_REG_LEN_16BIT, 0x0808 }
};

static struct crl_register_write_rep imx135_ipu5_fpga_poweroff_regset[] = {
	{ 0x0103, CRL_REG_LEN_08BIT, 0x01 },
};

static struct crl_arithmetic_ops imx135_ipu5_fpga_vflip_ops[] = {
	{
		.op = CRL_BITWISE_LSHIFT,
		.operand.entity_val = 1,
	},
};

static struct crl_dynamic_register_access imx135_ipu5_fpga_h_flip_regs[] = {
	{
		.address = 0x0101,
		.len = CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE,
		.ops_items = 0,
		.ops = 0,
		.mask = 0x1,
	},
};

static struct crl_dynamic_register_access imx135_ipu5_fpga_v_flip_regs[] = {
	{
		.address = 0x0101,
		.len = CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE,
		.ops_items = ARRAY_SIZE(imx135_ipu5_fpga_vflip_ops),
		.ops = imx135_ipu5_fpga_vflip_ops,
		.mask = 0x2,
	},
};


static struct crl_dynamic_register_access imx135_ipu5_fpga_ana_gain_regs[] = {
	{
		.address = 0x0205,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xff,
	},
};

static struct crl_dynamic_register_access imx135_ipu5_fpga_exposure_regs[] = {
	{
		.address = 0x0202,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	}
};

static struct crl_dynamic_register_access imx135_ipu5_fpga_dig_gain_regs[] = {
	{
		.address = 0x020e,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xfff,
	},
	{
		.address = 0x0210,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xfff,
	},
	{
		.address = 0x0212,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xfff,
	},
	{
		.address = 0x0214,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xfff,
	},
};

static struct crl_dynamic_register_access imx135_ipu5_fpga_vblank_regs[] = {
	{
		.address = 0x0340,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	},
};

static struct crl_dynamic_register_access imx135_ipu5_fpga_hblank_regs[] = {
	{
		.address = 0x0342,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	},
};
static struct crl_sensor_detect_config imx135_ipu5_fpga_sensor_detect_reg[] = {
	{
		.reg = { 0x0019, CRL_REG_LEN_08BIT, 0x000000ff },
		.width = 7,
	},
	{
		.reg = { 0x0016, CRL_REG_LEN_16BIT, 0x0000ffff },
		.width = 7,
	},
};

static struct crl_pll_configuration imx135_ipu5_fpga_pll_configurations[] = {
	{
		.input_clk = 24000000,
		.op_sys_clk = 168000000,
		.pixel_rate_csi = 134400000,
		.pixel_rate_pa = 134400000,
		.bitsperpixel = 10,
		.comp_items = 0,
		.ctrl_data = 0,
		.csi_lanes = 4,
		.pll_regs_items = ARRAY_SIZE(imx135_ipu5_fpga_pll_168mbps),
		.pll_regs = imx135_ipu5_fpga_pll_168mbps,
	},
};

static struct crl_subdev_rect_rep imx135_ipu5_fpga_mode_1080p_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4208,
		.in_rect.height = 3120,
		.out_rect.left = 42,
		.out_rect.top = 396,
		.out_rect.width = 4124,
		.out_rect.height = 2332,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4124,
		.in_rect.height = 2332,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 2062,
		.out_rect.height = 1166,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 2062,
		.in_rect.height = 1166,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1920,
		.out_rect.height = 1080,
	},
};

static struct crl_subdev_rect_rep imx135_ipu5_fpga_mode_720p_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4208,
		.in_rect.height = 3120,
		.out_rect.left = 4,
		.out_rect.top = 380,
		.out_rect.width = 4200,
		.out_rect.height = 2360,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4200,
		.in_rect.height = 2360,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 2100,
		.out_rect.height = 1180,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 2100,
		.in_rect.height = 1180,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1280,
		.out_rect.height = 720,
	},
};

static struct crl_mode_rep imx135_ipu5_fpga_modes[] = {
	{
		.sd_rects_items =
			ARRAY_SIZE(imx135_ipu5_fpga_mode_1080p_rects),
		.sd_rects = imx135_ipu5_fpga_mode_1080p_rects,
		.binn_hor = 2,
		.binn_vert = 2,
		.scale_m = 17,
		.width = 1920,
		.height = 1080,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items =
			ARRAY_SIZE(imx135_ipu5_fpga_mode_1080p),
		.mode_regs = imx135_ipu5_fpga_mode_1080p,
	},
	{
		.sd_rects_items =
			ARRAY_SIZE(imx135_ipu5_fpga_mode_720p_rects),
		.sd_rects = imx135_ipu5_fpga_mode_720p_rects,
		.binn_hor = 2,
		.binn_vert = 2,
		.scale_m = 26,
		.width = 1280,
		.height = 720,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(imx135_ipu5_fpga_mode_720p),
		.mode_regs = imx135_ipu5_fpga_mode_720p,
	},
};

static struct crl_sensor_subdev_config imx135_ipu5_fpga_sensor_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.name = "imx135 scaler",
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.name = "imx135 binner",
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "imx135 pixel array",
	},
};

static struct crl_sensor_limits imx135_ipu5_fpga_sensor_limits = {
	.x_addr_min = 0,
	.y_addr_min = 0,
	.x_addr_max = 4208,
	.y_addr_max = 3120,
	.min_frame_length_lines = 160,
	.max_frame_length_lines = 65535,
	.min_line_length_pixels = 4572,
	.max_line_length_pixels = 32752,
	.scaler_m_min = 16,
	.scaler_m_max = 255,
	.min_even_inc = 1,
	.max_even_inc = 1,
	.min_odd_inc = 1,
	.max_odd_inc = 3,
};

static struct crl_flip_data imx135_ipu5_fpga_flip_configurations[] = {
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

static struct crl_csi_data_fmt imx135_ipu5_fpga_crl_csi_data_fmt[] = {
	{
		.code = MEDIA_BUS_FMT_SGRBG10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.bits_per_pixel = 10,
		.regs_items = 1,
		.regs = imx135_ipu5_fpga_data_fmt_width10,
	},
	{
		.code = MEDIA_BUS_FMT_SRGGB10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,
		.regs_items = 1,
		.bits_per_pixel = 10,
		.regs = imx135_ipu5_fpga_data_fmt_width10,
	},
	{
		.code = MEDIA_BUS_FMT_SBGGR10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_BGGR,
		.regs_items = 1,
		.bits_per_pixel = 10,
		.regs = imx135_ipu5_fpga_data_fmt_width10,
	},
	{
		.code = MEDIA_BUS_FMT_SGBRG10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_GBRG,
		.regs_items = 1,
		.bits_per_pixel = 10,
		.regs = imx135_ipu5_fpga_data_fmt_width10,
	},
	{
		.code = MEDIA_BUS_FMT_SGRBG8_1X8,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.regs_items = 1,
		.bits_per_pixel = 8,
		.regs = imx135_ipu5_fpga_data_fmt_width8,
	},
	{
		.code = MEDIA_BUS_FMT_SRGGB8_1X8,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,
		.regs_items = 1,
		.bits_per_pixel = 8,
		.regs = imx135_ipu5_fpga_data_fmt_width8,
	},
	{
		.code = MEDIA_BUS_FMT_SBGGR8_1X8,
		.pixel_order = CRL_PIXEL_ORDER_BGGR,
		.regs_items = 1,
		.bits_per_pixel = 8,
		.regs = imx135_ipu5_fpga_data_fmt_width8,
	},
	{
		.code = MEDIA_BUS_FMT_SGBRG8_1X8,
		.pixel_order = CRL_PIXEL_ORDER_GBRG,
		.regs_items = 1,
		.bits_per_pixel = 8,
		.regs = imx135_ipu5_fpga_data_fmt_width8,
	},
};

static struct crl_v4l2_ctrl imx135_ipu5_fpga_v4l2_ctrls[] = {
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
		.sd_type = CRL_SUBDEV_TYPE_SCALER,
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
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.name = "V4L2_CID_ANALOGUE_GAIN",
		.data.std_data.min = 0,
		.data.std_data.max = 224,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx135_ipu5_fpga_ana_gain_regs),
		.regs = imx135_ipu5_fpga_ana_gain_regs,
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
		.data.std_data.def = 1216,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx135_ipu5_fpga_exposure_regs),
		.regs = imx135_ipu5_fpga_exposure_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
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
		.regs_items = ARRAY_SIZE(imx135_ipu5_fpga_h_flip_regs),
		.regs = imx135_ipu5_fpga_h_flip_regs,
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
		.regs_items = ARRAY_SIZE(imx135_ipu5_fpga_v_flip_regs),
		.regs = imx135_ipu5_fpga_v_flip_regs,
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
		.data.std_data.def = 1220,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx135_ipu5_fpga_vblank_regs),
		.regs = imx135_ipu5_fpga_vblank_regs,
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
		.data.std_data.min = 4280,
		.data.std_data.max = 65520,
		.data.std_data.step = 1,
		.data.std_data.def = 18288,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx135_ipu5_fpga_hblank_regs),
		.regs = imx135_ipu5_fpga_hblank_regs,
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
		.data.std_data.def = 256,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx135_ipu5_fpga_dig_gain_regs),
		.regs = imx135_ipu5_fpga_dig_gain_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
};

static struct crl_sensor_configuration imx135_ipu5_fpga_crl_configuration = {

	.powerup_regs_items = ARRAY_SIZE(imx135_ipu5_fpga_powerup_regset),
	.powerup_regs = imx135_ipu5_fpga_powerup_regset,

	.poweroff_regs_items = ARRAY_SIZE(imx135_ipu5_fpga_poweroff_regset),
	.poweroff_regs = imx135_ipu5_fpga_poweroff_regset,

	/*
	* TODO: add power_item
	* .power_items = xxx,
	* .power_entities = xxx,
	* On fpga currenlty, using CRDG's PMIC configuration
	*/

	.id_reg_items = ARRAY_SIZE(imx135_ipu5_fpga_sensor_detect_reg),
	.id_regs = imx135_ipu5_fpga_sensor_detect_reg,

	.subdev_items = ARRAY_SIZE(imx135_ipu5_fpga_sensor_subdevs),
	.subdevs = imx135_ipu5_fpga_sensor_subdevs,

	.sensor_limits = &imx135_ipu5_fpga_sensor_limits,

	.pll_config_items = ARRAY_SIZE(imx135_ipu5_fpga_pll_configurations),
	.pll_configs = imx135_ipu5_fpga_pll_configurations,

	.modes_items = ARRAY_SIZE(imx135_ipu5_fpga_modes),
	.modes = imx135_ipu5_fpga_modes,

	.streamon_regs_items = ARRAY_SIZE(imx135_ipu5_fpga_streamon_regs),
	.streamon_regs = imx135_ipu5_fpga_streamon_regs,

	.streamoff_regs_items = ARRAY_SIZE(imx135_ipu5_fpga_streamoff_regs),
	.streamoff_regs = imx135_ipu5_fpga_streamoff_regs,

	.v4l2_ctrls_items = ARRAY_SIZE(imx135_ipu5_fpga_v4l2_ctrls),
	.v4l2_ctrl_bank = imx135_ipu5_fpga_v4l2_ctrls,

	.csi_fmts_items = ARRAY_SIZE(imx135_ipu5_fpga_crl_csi_data_fmt),
	.csi_fmts = imx135_ipu5_fpga_crl_csi_data_fmt,

	.flip_items = ARRAY_SIZE(imx135_ipu5_fpga_flip_configurations),
	.flip_data = imx135_ipu5_fpga_flip_configurations,
};

#endif  /* __CRLMODULE_IMX135_IPU5_FPGA_CONFIGURATION_H_ */
