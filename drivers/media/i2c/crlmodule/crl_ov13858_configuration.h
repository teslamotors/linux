/*
 * Copyright (c) 2017 Intel Corporation.
 *
 * Author: Lance Hou <lance.hou@intel.com>

 *
 */

#ifndef __CRLMODULE_ov13858_CONFIGURATION_H_
#define __CRLMODULE_ov13858_CONFIGURATION_H_

#include "crlmodule-sensor-ds.h"
#ifdef CONFIG_CRLMODULE_RD_NVM_TO_VCM
#include "crlmodule-nvm.h"
#endif

static struct crl_register_write_rep ov13858_powerup_regset[] = {
	{0x0103, CRL_REG_LEN_08BIT, 0x01},
	{0x0300, CRL_REG_LEN_08BIT, 0x07},
	{0x0301, CRL_REG_LEN_08BIT, 0x01},
	{0x0302, CRL_REG_LEN_08BIT, 0xc2},
	{0x0303, CRL_REG_LEN_08BIT, 0x00},
	{0x0304, CRL_REG_LEN_08BIT, 0x00},
	{0x0305, CRL_REG_LEN_08BIT, 0x01},
	{0x030b, CRL_REG_LEN_08BIT, 0x05},
	{0x030c, CRL_REG_LEN_08BIT, 0x01},
	{0x030d, CRL_REG_LEN_08BIT, 0x0e},
	{0x3022, CRL_REG_LEN_08BIT, 0x01},
	{0x3013, CRL_REG_LEN_08BIT, 0x32},
	{0x3016, CRL_REG_LEN_08BIT, 0x72},
	{0x301b, CRL_REG_LEN_08BIT, 0xF0},
	{0x301f, CRL_REG_LEN_08BIT, 0xd0},
	{0x3106, CRL_REG_LEN_08BIT, 0x15},
	{0x3107, CRL_REG_LEN_08BIT, 0x23},
	{0x3500, CRL_REG_LEN_08BIT, 0x00},
	{0x3501, CRL_REG_LEN_08BIT, 0x80},
	{0x3502, CRL_REG_LEN_08BIT, 0x00},
	{0x3508, CRL_REG_LEN_08BIT, 0x02},
	{0x3509, CRL_REG_LEN_08BIT, 0x00},
	{0x350a, CRL_REG_LEN_08BIT, 0x00},
	{0x350e, CRL_REG_LEN_08BIT, 0x00},
	{0x3510, CRL_REG_LEN_08BIT, 0x00},
	{0x3511, CRL_REG_LEN_08BIT, 0x02},
	{0x3512, CRL_REG_LEN_08BIT, 0x00},
	{0x3600, CRL_REG_LEN_08BIT, 0x2b},
	{0x3601, CRL_REG_LEN_08BIT, 0x52},
	{0x3602, CRL_REG_LEN_08BIT, 0x60},
	{0x3612, CRL_REG_LEN_08BIT, 0x05},
	{0x3613, CRL_REG_LEN_08BIT, 0xa4},
	{0x3620, CRL_REG_LEN_08BIT, 0x80},
	{0x3621, CRL_REG_LEN_08BIT, 0x08},
	{0x3622, CRL_REG_LEN_08BIT, 0x30},
	{0x3624, CRL_REG_LEN_08BIT, 0x1c},
	{0x3640, CRL_REG_LEN_08BIT, 0x08},
	{0x3641, CRL_REG_LEN_08BIT, 0x70},
	{0x3661, CRL_REG_LEN_08BIT, 0x80},
	{0x3662, CRL_REG_LEN_08BIT, 0x12},
	{0x3664, CRL_REG_LEN_08BIT, 0x73},
	{0x3665, CRL_REG_LEN_08BIT, 0xa7},
	{0x366e, CRL_REG_LEN_08BIT, 0xff},
	{0x366f, CRL_REG_LEN_08BIT, 0xf4},
	{0x3674, CRL_REG_LEN_08BIT, 0x00},
	{0x3679, CRL_REG_LEN_08BIT, 0x0c},
	{0x367f, CRL_REG_LEN_08BIT, 0x01},
	{0x3680, CRL_REG_LEN_08BIT, 0x0c},
	{0x3681, CRL_REG_LEN_08BIT, 0x60},
	{0x3682, CRL_REG_LEN_08BIT, 0x17},
	{0x3683, CRL_REG_LEN_08BIT, 0xa9},
	{0x3684, CRL_REG_LEN_08BIT, 0x9a},
	{0x3709, CRL_REG_LEN_08BIT, 0x68},
	{0x3714, CRL_REG_LEN_08BIT, 0x24},
	{0x371a, CRL_REG_LEN_08BIT, 0x3e},
	{0x3737, CRL_REG_LEN_08BIT, 0x04},
	{0x3738, CRL_REG_LEN_08BIT, 0xcc},
	{0x3739, CRL_REG_LEN_08BIT, 0x12},
	{0x373d, CRL_REG_LEN_08BIT, 0x26},
	{0x3764, CRL_REG_LEN_08BIT, 0x20},
	{0x3765, CRL_REG_LEN_08BIT, 0x20},
	{0x37a1, CRL_REG_LEN_08BIT, 0x36},
	{0x37a8, CRL_REG_LEN_08BIT, 0x3b},
	{0x37ab, CRL_REG_LEN_08BIT, 0x31},
	{0x37c2, CRL_REG_LEN_08BIT, 0x04},
	{0x37c3, CRL_REG_LEN_08BIT, 0xf1},
	{0x37c5, CRL_REG_LEN_08BIT, 0x00},
	{0x37d8, CRL_REG_LEN_08BIT, 0x03},
	{0x37d9, CRL_REG_LEN_08BIT, 0x0c},
	{0x37da, CRL_REG_LEN_08BIT, 0xc2},
	{0x37dc, CRL_REG_LEN_08BIT, 0x02},
	{0x37e0, CRL_REG_LEN_08BIT, 0x00},
	{0x37e1, CRL_REG_LEN_08BIT, 0x0a},
	{0x37e2, CRL_REG_LEN_08BIT, 0x14},
	{0x37e3, CRL_REG_LEN_08BIT, 0x04},
	{0x37e4, CRL_REG_LEN_08BIT, 0x2A},
	{0x37e5, CRL_REG_LEN_08BIT, 0x03},
	{0x37e6, CRL_REG_LEN_08BIT, 0x04},
	{0x3800, CRL_REG_LEN_08BIT, 0x00},
	{0x3801, CRL_REG_LEN_08BIT, 0x00},
	{0x3802, CRL_REG_LEN_08BIT, 0x00},
	{0x3803, CRL_REG_LEN_08BIT, 0x08},
	{0x3804, CRL_REG_LEN_08BIT, 0x10},
	{0x3805, CRL_REG_LEN_08BIT, 0x9f},
	{0x3806, CRL_REG_LEN_08BIT, 0x0c},
	{0x3807, CRL_REG_LEN_08BIT, 0x57},
	{0x3808, CRL_REG_LEN_08BIT, 0x10},
	{0x3809, CRL_REG_LEN_08BIT, 0x80},
	{0x380a, CRL_REG_LEN_08BIT, 0x0c},
	{0x380b, CRL_REG_LEN_08BIT, 0x40},
	{0x380c, CRL_REG_LEN_08BIT, 0x04},
	{0x380d, CRL_REG_LEN_08BIT, 0x62},
	{0x380e, CRL_REG_LEN_08BIT, 0x0c},
	{0x380f, CRL_REG_LEN_08BIT, 0x8e},
	{0x3811, CRL_REG_LEN_08BIT, 0x10},
	{0x3813, CRL_REG_LEN_08BIT, 0x08},
	{0x3814, CRL_REG_LEN_08BIT, 0x01},
	{0x3815, CRL_REG_LEN_08BIT, 0x01},
	{0x3816, CRL_REG_LEN_08BIT, 0x01},
	{0x3817, CRL_REG_LEN_08BIT, 0x01},
	{0x3820, CRL_REG_LEN_08BIT, 0xa8},
	{0x3821, CRL_REG_LEN_08BIT, 0x00},
	{0x3822, CRL_REG_LEN_08BIT, 0xc2},
	{0x3823, CRL_REG_LEN_08BIT, 0x18},
	{0x3826, CRL_REG_LEN_08BIT, 0x11},
	{0x3827, CRL_REG_LEN_08BIT, 0x1c},
	{0x3829, CRL_REG_LEN_08BIT, 0x03},
	{0x3832, CRL_REG_LEN_08BIT, 0x00},
	{0x3c80, CRL_REG_LEN_08BIT, 0x00},
	{0x3c87, CRL_REG_LEN_08BIT, 0x01},
	{0x3c8c, CRL_REG_LEN_08BIT, 0x19},
	{0x3c8d, CRL_REG_LEN_08BIT, 0x1c},
	{0x3c90, CRL_REG_LEN_08BIT, 0x00},
	{0x3c91, CRL_REG_LEN_08BIT, 0x00},
	{0x3c92, CRL_REG_LEN_08BIT, 0x00},
	{0x3c93, CRL_REG_LEN_08BIT, 0x00},
	{0x3c94, CRL_REG_LEN_08BIT, 0x40},
	{0x3c95, CRL_REG_LEN_08BIT, 0x54},
	{0x3c96, CRL_REG_LEN_08BIT, 0x34},
	{0x3c97, CRL_REG_LEN_08BIT, 0x04},
	{0x3c98, CRL_REG_LEN_08BIT, 0x00},
	{0x3d8c, CRL_REG_LEN_08BIT, 0x73},
	{0x3d8d, CRL_REG_LEN_08BIT, 0xc0},
	{0x3f00, CRL_REG_LEN_08BIT, 0x0b},
	{0x3f03, CRL_REG_LEN_08BIT, 0x00},
	{0x4001, CRL_REG_LEN_08BIT, 0xe0},
	{0x4008, CRL_REG_LEN_08BIT, 0x00},
	{0x4009, CRL_REG_LEN_08BIT, 0x0f},
	{0x4011, CRL_REG_LEN_08BIT, 0xf0},
	{0x4017, CRL_REG_LEN_08BIT, 0x08},
	{0x4050, CRL_REG_LEN_08BIT, 0x04},
	{0x4051, CRL_REG_LEN_08BIT, 0x0b},
	{0x4052, CRL_REG_LEN_08BIT, 0x00},
	{0x4053, CRL_REG_LEN_08BIT, 0x80},
	{0x4054, CRL_REG_LEN_08BIT, 0x00},
	{0x4055, CRL_REG_LEN_08BIT, 0x80},
	{0x4056, CRL_REG_LEN_08BIT, 0x00},
	{0x4057, CRL_REG_LEN_08BIT, 0x80},
	{0x4058, CRL_REG_LEN_08BIT, 0x00},
	{0x4059, CRL_REG_LEN_08BIT, 0x80},
	{0x405e, CRL_REG_LEN_08BIT, 0x20},
	{0x4500, CRL_REG_LEN_08BIT, 0x07},
	{0x4503, CRL_REG_LEN_08BIT, 0x00},
	{0x450a, CRL_REG_LEN_08BIT, 0x04},
	{0x4809, CRL_REG_LEN_08BIT, 0x04},
	{0x480c, CRL_REG_LEN_08BIT, 0x12},
	{0x481f, CRL_REG_LEN_08BIT, 0x30},
	{0x4833, CRL_REG_LEN_08BIT, 0x10},
	{0x4837, CRL_REG_LEN_08BIT, 0x0e},
	{0x4902, CRL_REG_LEN_08BIT, 0x01},
	{0x4d00, CRL_REG_LEN_08BIT, 0x03},
	{0x4d01, CRL_REG_LEN_08BIT, 0xc9},
	{0x4d02, CRL_REG_LEN_08BIT, 0xbc},
	{0x4d03, CRL_REG_LEN_08BIT, 0xd7},
	{0x4d04, CRL_REG_LEN_08BIT, 0xf0},
	{0x4d05, CRL_REG_LEN_08BIT, 0xa2},
	{0x5000, CRL_REG_LEN_08BIT, 0xfD},
	{0x5001, CRL_REG_LEN_08BIT, 0x01},
	{0x5040, CRL_REG_LEN_08BIT, 0x39},
	{0x5041, CRL_REG_LEN_08BIT, 0x10},
	{0x5042, CRL_REG_LEN_08BIT, 0x10},
	{0x5043, CRL_REG_LEN_08BIT, 0x84},
	{0x5044, CRL_REG_LEN_08BIT, 0x62},
	{0x5180, CRL_REG_LEN_08BIT, 0x00},
	{0x5181, CRL_REG_LEN_08BIT, 0x10},
	{0x5182, CRL_REG_LEN_08BIT, 0x02},
	{0x5183, CRL_REG_LEN_08BIT, 0x0f},
	{0x5200, CRL_REG_LEN_08BIT, 0x1b},
	{0x520b, CRL_REG_LEN_08BIT, 0x07},
	{0x520c, CRL_REG_LEN_08BIT, 0x0f},
	{0x5300, CRL_REG_LEN_08BIT, 0x04},
	{0x5301, CRL_REG_LEN_08BIT, 0x0C},
	{0x5302, CRL_REG_LEN_08BIT, 0x0C},
	{0x5303, CRL_REG_LEN_08BIT, 0x0f},
	{0x5304, CRL_REG_LEN_08BIT, 0x00},
	{0x5305, CRL_REG_LEN_08BIT, 0x70},
	{0x5306, CRL_REG_LEN_08BIT, 0x00},
	{0x5307, CRL_REG_LEN_08BIT, 0x80},
	{0x5308, CRL_REG_LEN_08BIT, 0x00},
	{0x5309, CRL_REG_LEN_08BIT, 0xa5},
	{0x530a, CRL_REG_LEN_08BIT, 0x00},
	{0x530b, CRL_REG_LEN_08BIT, 0xd3},
	{0x530c, CRL_REG_LEN_08BIT, 0x00},
	{0x530d, CRL_REG_LEN_08BIT, 0xf0},
	{0x530e, CRL_REG_LEN_08BIT, 0x01},
	{0x530f, CRL_REG_LEN_08BIT, 0x10},
	{0x5310, CRL_REG_LEN_08BIT, 0x01},
	{0x5311, CRL_REG_LEN_08BIT, 0x20},
	{0x5312, CRL_REG_LEN_08BIT, 0x01},
	{0x5313, CRL_REG_LEN_08BIT, 0x20},
	{0x5314, CRL_REG_LEN_08BIT, 0x01},
	{0x5315, CRL_REG_LEN_08BIT, 0x20},
	{0x5316, CRL_REG_LEN_08BIT, 0x08},
	{0x5317, CRL_REG_LEN_08BIT, 0x08},
	{0x5318, CRL_REG_LEN_08BIT, 0x10},
	{0x5319, CRL_REG_LEN_08BIT, 0x88},
	{0x531a, CRL_REG_LEN_08BIT, 0x88},
	{0x531b, CRL_REG_LEN_08BIT, 0xa9},
	{0x531c, CRL_REG_LEN_08BIT, 0xaa},
	{0x531d, CRL_REG_LEN_08BIT, 0x0a},
	{0x5405, CRL_REG_LEN_08BIT, 0x02},
	{0x5406, CRL_REG_LEN_08BIT, 0x67},
	{0x5407, CRL_REG_LEN_08BIT, 0x01},
	{0x5408, CRL_REG_LEN_08BIT, 0x4a},
};

static struct crl_register_write_rep ov13858_3864x2202_RAW10_NORMAL[] = {
	{0x0100,CRL_REG_LEN_08BIT, 0x00},
	{0x0300,CRL_REG_LEN_08BIT, 0x07},
	{0x0301,CRL_REG_LEN_08BIT, 0x01},
	{0x0302,CRL_REG_LEN_08BIT, 0xc2},
	{0x0303,CRL_REG_LEN_08BIT, 0x00},
	{0x030b,CRL_REG_LEN_08BIT, 0x05},
	{0x030c,CRL_REG_LEN_08BIT, 0x01},
	{0x030d,CRL_REG_LEN_08BIT, 0x0e},
	{0x0312,CRL_REG_LEN_08BIT, 0x01},
	{0x3662,CRL_REG_LEN_08BIT, 0x12},
	{0x3714,CRL_REG_LEN_08BIT, 0x24},
	{0x3737,CRL_REG_LEN_08BIT, 0x04},
	{0x3739,CRL_REG_LEN_08BIT, 0x12},
	{0x37c2,CRL_REG_LEN_08BIT, 0x04},
	{0x37d9,CRL_REG_LEN_08BIT, 0x0c},
	{0x37e3,CRL_REG_LEN_08BIT, 0x04},
	{0x37e4,CRL_REG_LEN_08BIT, 0x2a},
	{0x37e6,CRL_REG_LEN_08BIT, 0x04},
	{0x3800,CRL_REG_LEN_08BIT, 0x00},
	{0x3801,CRL_REG_LEN_08BIT, 0x00},
	{0x3802,CRL_REG_LEN_08BIT, 0x00},
	{0x3803,CRL_REG_LEN_08BIT, 0x08},
	{0x3804,CRL_REG_LEN_08BIT, 0x10},
	{0x3805,CRL_REG_LEN_08BIT, 0x9f},
	{0x3806,CRL_REG_LEN_08BIT, 0x0c},
	{0x3807,CRL_REG_LEN_08BIT, 0x57},
	{0x3808,CRL_REG_LEN_08BIT, 0x0f},
	{0x3809,CRL_REG_LEN_08BIT, 0x18},
	{0x380a,CRL_REG_LEN_08BIT, 0x08},
	{0x380b,CRL_REG_LEN_08BIT, 0x9a},
	{0x380c,CRL_REG_LEN_08BIT, 0x04},
	{0x380d,CRL_REG_LEN_08BIT, 0x62},
	{0x380e,CRL_REG_LEN_08BIT, 0x0c},
	{0x380f,CRL_REG_LEN_08BIT, 0x8e},
	{0x3810,CRL_REG_LEN_08BIT, 0x00},
	{0x3811,CRL_REG_LEN_08BIT, 0xc5},  //default C4 for BGGR X-axis
	{0x3812,CRL_REG_LEN_08BIT, 0x01},
	{0x3813,CRL_REG_LEN_08BIT, 0xdb},   //default da for BGGR Y-axis
	{0x3814,CRL_REG_LEN_08BIT, 0x01},
	{0x3815,CRL_REG_LEN_08BIT, 0x01},
	{0x3816,CRL_REG_LEN_08BIT, 0x01},
	{0x3817,CRL_REG_LEN_08BIT, 0x01},
	{0x3820,CRL_REG_LEN_08BIT, 0xa8},
	{0x3821,CRL_REG_LEN_08BIT, 0x00},
	{0x3826,CRL_REG_LEN_08BIT, 0x11},
	{0x3827,CRL_REG_LEN_08BIT, 0x1c},
	{0x3829,CRL_REG_LEN_08BIT, 0x03},
	{0x4009,CRL_REG_LEN_08BIT, 0x0f},
	{0x4837,CRL_REG_LEN_08BIT, 0x0e},
	{0x4050,CRL_REG_LEN_08BIT, 0x04},
	{0x4051,CRL_REG_LEN_08BIT, 0x0b},
	{0x4902,CRL_REG_LEN_08BIT, 0x01},
	{0x5000,CRL_REG_LEN_08BIT, 0xfd},
	{0x5001,CRL_REG_LEN_08BIT, 0x01},
	{0x0100,CRL_REG_LEN_08BIT, 0x01},
};

static struct crl_register_write_rep ov13858_streamon_regs[] = {
	{ 0x0100, CRL_REG_LEN_08BIT, 0x01 }
};

static struct crl_register_write_rep ov13858_streamoff_regs[] = {
	{ 0x0100, CRL_REG_LEN_08BIT, 0x00 }
};

static struct crl_arithmetic_ops ov13858_vflip_ops[] = {
	{
		.op = CRL_BITWISE_LSHIFT,
		.operand.entity_val = 1,
	 },
};

static struct crl_arithmetic_ops ov13858_hflip_ops[] = {
	{
		.op = CRL_BITWISE_LSHIFT,
		.operand.entity_val = 1,
	 },
};

static struct crl_arithmetic_ops ov13858_exposure_ops[] = {
	{
		.op = CRL_BITWISE_LSHIFT,
		.operand.entity_val = 4,
	}
};

static struct crl_dynamic_register_access ov13858_v_flip_regs[] = {
	{
		.address = 0x3820,
		.len = CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE,
		.ops_items = ARRAY_SIZE(ov13858_vflip_ops),
		.ops = ov13858_vflip_ops,
		.mask = 0x2,
	 },
};

static struct crl_dynamic_register_access ov13858_h_flip_regs[] = {
	{
		.address = 0x3821,
		.len = CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE,
		.ops_items = ARRAY_SIZE(ov13858_hflip_ops),
		.ops = ov13858_hflip_ops,
		.mask = 0x2,
	 },
};

struct crl_register_write_rep ov13858_poweroff_regset[] = {
	{ 0x0103, CRL_REG_LEN_08BIT, 0x01  },
};

#ifdef CONFIG_CRLMODULE_RD_NVM_TO_VCM
static struct crl_nvm_blob ov13858_nvm_blobs[] = {
        { 0x50, 0x00, 0x100 },
};
#endif

static struct crl_dynamic_register_access ov13858_ana_gain_global_regs[] = {
	{
		.address = 0x3508,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0x7ff,
	 },
};

static struct crl_dynamic_register_access ov13858_exposure_regs[] = {
	{
		.address = 0x3500,
		.len = CRL_REG_LEN_24BIT,
		.ops_items = ARRAY_SIZE(ov13858_exposure_ops),
		.ops = ov13858_exposure_ops,
		.mask = 0x0ffff0,
	}
};

static struct crl_dynamic_register_access ov13858_dig_gain_regs[] = {
	{
		.address = 0x5100,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0x7fff,
	},
	{
		.address = 0x5102,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0x7fff,
	},
	{
		.address = 0x5104,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0x7fff,
	}
};

static struct crl_dynamic_register_access ov13858_vblank_regs[] = {
	{
		.address = 0x380E,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	 },
};

static struct crl_dynamic_register_access ov13858_hblank_regs[] = {
	{
		.address = 0x380C,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	 },
};

static struct crl_sensor_detect_config ov13858_sensor_detect_regset[] = {
	{
		.reg = { 0x300B, CRL_REG_LEN_08BIT, 0x000000ff  },
		.width = 7,
	 },
	{
		.reg = { 0x300C, CRL_REG_LEN_08BIT, 0x000000ff  },
		.width = 7,
	 },
};

static struct crl_pll_configuration ov13858_pll_configurations[] = {
	{
		.input_clk = 19200000,
		.op_sys_clk = 54000000,
		.bitsperpixel = 10,
		.pixel_rate_csi = 43200000,
		.pixel_rate_pa = 43200000,
		.csi_lanes= 4,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = 0,
		.pll_regs = 0,
	 },
};

static struct crl_subdev_rect_rep ov13858_13m_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4224,
		.in_rect.height = 3136,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 4224,
		.out_rect.height = 3136,
	 },
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4224,
		.in_rect.height = 3136,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 4224,
		.out_rect.height = 3136,
	 },
};

static struct crl_subdev_rect_rep ov13858_2202_rects[] = {
	{
                .subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
                .in_rect.left = 0,
                .in_rect.top = 0,
                .in_rect.width = 4224,
                .in_rect.height = 3136,
                .out_rect.left = 0,
                .out_rect.top = 0,
                .out_rect.width = 4224,
                .out_rect.height = 3136,
         },
        {
                .subdev_type = CRL_SUBDEV_TYPE_BINNER,
                .in_rect.left = 0,
                .in_rect.top = 0,
                .in_rect.width = 4224,
                .in_rect.height = 3136,
                .out_rect.left = 0,
                .out_rect.top = 0,
                .out_rect.width = 3864,
                .out_rect.height = 2202,
         },
};


static struct crl_mode_rep ov13858_modes[] = {
	{
		.sd_rects_items = ARRAY_SIZE(ov13858_13m_rects),
		.sd_rects = ov13858_13m_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 4224,
		.height = 3136,
		.min_llp = 1122,
		.min_fll = 3214,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = 0,
		.mode_regs = 0,
	},
	{
                .sd_rects_items = ARRAY_SIZE(ov13858_2202_rects),
                .sd_rects = ov13858_2202_rects,
                .binn_hor = 1,
                .binn_vert = 1,
                .scale_m = 1,
                .width = 3864,
                .height = 2202,
                .min_llp = 1122,
                .min_fll = 3214,
                .comp_items = 0,
                .ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(ov13858_3864x2202_RAW10_NORMAL),
                .mode_regs = ov13858_3864x2202_RAW10_NORMAL,
	},
};

static struct crl_sensor_subdev_config ov13858_sensor_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.name = "ov13858 binner",
	 },
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "ov13858 pixel array",
	 },
};

static struct crl_sensor_limits ov13858_sensor_limits = {
	.x_addr_min = 0,
	.y_addr_min = 0,
	.x_addr_max = 4224,
	.y_addr_max = 3136,
	.min_frame_length_lines = 3214,
	.max_frame_length_lines = 32767,
	.min_line_length_pixels = 1122,
	.max_line_length_pixels = 65535,
	.scaler_m_min = 16,
	.scaler_m_max = 16,
	.scaler_n_min = 16,
	.scaler_n_max = 16,
	.min_even_inc = 1,
	.max_even_inc = 1,
	.min_odd_inc = 1,
	.max_odd_inc = 3,
};

static struct crl_flip_data ov13858_flip_configurations[] = {
	{
		.flip = CRL_FLIP_DEFAULT_NONE,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,
	 },
	{
		.flip = CRL_FLIP_HFLIP,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,
	 },
	{
		.flip = CRL_FLIP_VFLIP,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,
	 },
	{
		.flip = CRL_FLIP_HFLIP_VFLIP,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,
	 },
};

static struct crl_csi_data_fmt ov13858_crl_csi_data_fmt[] = {
	{
		.code = MEDIA_BUS_FMT_SGRBG10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.bits_per_pixel = 10,
		.regs_items = 0,
		.regs = 0,
	},
	{
		.code = MEDIA_BUS_FMT_SRGGB10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,
		.bits_per_pixel = 10,
		.regs_items = 0,
		.regs = 0,
	},
	{
		.code = MEDIA_BUS_FMT_SBGGR10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_BGGR,
		.bits_per_pixel = 10,
		.regs_items = 0,
		.regs = 0,
	},
	{
		.code = MEDIA_BUS_FMT_SGBRG10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_GBRG,
		.regs_items = 1,
		.bits_per_pixel = 10,
		.regs = 0,
	},
};

static struct crl_v4l2_ctrl ov13858_v4l2_ctrls[] = {
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
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(ov13858_ana_gain_global_regs),
		.regs = ov13858_ana_gain_global_regs,
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
		.data.std_data.max = 65535,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(ov13858_exposure_regs),
		.regs = ov13858_exposure_regs,
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
		.regs_items = ARRAY_SIZE(ov13858_h_flip_regs),
		.regs = ov13858_h_flip_regs,
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
		.regs_items = ARRAY_SIZE(ov13858_v_flip_regs),
		.regs = ov13858_v_flip_regs,
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
		.data.std_data.min = 3214,
		.data.std_data.max = 32767,
		.data.std_data.step = 1,
		.data.std_data.def = 3214,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items =  ARRAY_SIZE(ov13858_vblank_regs),
		.regs =  ov13858_vblank_regs,
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
		.data.std_data.min = 1122,
		.data.std_data.max = 65535,
		.data.std_data.step = 1,
		.data.std_data.def = 1122,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(ov13858_hblank_regs),
		.regs = ov13858_hblank_regs,
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
		.data.std_data.max = 16384,
		.data.std_data.step = 1,
		.data.std_data.def = 1024,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(ov13858_dig_gain_regs),
		.regs = ov13858_dig_gain_regs,
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
static struct crl_register_write_rep ov13858_otp_preop_regset[] = {
        /*sensor OTP module check*/
        { 0x5000, CRL_REG_LEN_08BIT, 0x5f },
        /* Manual mode, program disable */
        { 0x3D84, CRL_REG_LEN_08BIT, 0xC0 },
        /* Manual OTP start address for access */
        { 0x3D88, CRL_REG_LEN_08BIT, 0x72},
        { 0x3D89, CRL_REG_LEN_08BIT, 0x20},
        /* Manual OTP end address for access */
        { 0x3D8A, CRL_REG_LEN_08BIT, 0x72},
        { 0x3D8B, CRL_REG_LEN_08BIT, 0x21},
        /*streaming on*/
        { 0x0100, CRL_REG_LEN_08BIT, 0x01 },
        /* OTP load enable */
        { 0x3D81, CRL_REG_LEN_08BIT, 0x31 },
        /* Wait for the data to load into the buffer */
        { 0x0000, CRL_REG_LEN_DELAY, 0x14 },
};

static struct crl_register_write_rep ov13858_otp_postop_regset[] = {
        { 0x0100, CRL_REG_LEN_08BIT, 0x00 }, /* Stop streaming */
};

static struct crl_register_write_rep ov13858_otp_mode_regset[] = {
        { 0x5000, CRL_REG_LEN_08BIT, 0x7F }, /*ISP Control*/
        { 0x5040, CRL_REG_LEN_08BIT, 0xA8 }, /*Set Test Mode*/
};

struct crl_register_read_rep ov13858_sensor_otp_read_regset[] = {
        { 0x7220, CRL_REG_LEN_16BIT, 0x0000ffff },
};
#endif

#if 0
static struct crl_arithmetic_ops ov13858_frame_desc_width_ops[] = {
	{
	 .op = CRL_ASSIGNMENT,
	 .operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_VAR_REF,
	 .operand.entity_val = CRL_VAR_REF_OUTPUT_WIDTH,
	},
};

static struct crl_arithmetic_ops ov13858_frame_desc_height_ops[] = {
	{
	 .op = CRL_ASSIGNMENT,
	 .operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
	 .operand.entity_val = 1,
	},
};

static struct crl_frame_desc ov13858_frame_desc[] = {
	{
		.flags.entity_val = 0,
		.bpp.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_VAR_REF,
		.bpp.entity_val = CRL_VAR_REF_BITSPERPIXEL,
		.pixelcode.entity_val = MEDIA_BUS_FMT_FIXED,
		.length.entity_val = 0,
		.start_line.entity_val = 0,
		.start_pixel.entity_val = 0,
		.width = {
			 .ops_items = ARRAY_SIZE(ov13858_frame_desc_width_ops),
			 .ops = ov13858_frame_desc_width_ops,
			 },
		.height = {
			  .ops_items = ARRAY_SIZE(ov13858_frame_desc_height_ops),
			  .ops = ov13858_frame_desc_height_ops,
			  },
		.csi2_channel.entity_val = 0,
		.csi2_data_type.entity_val = 0x12,
	},
};
#endif

/* Power items, they are enabled in the order they are listed here */
static struct crl_power_seq_entity ov13858_power_items[] = {
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

struct crl_sensor_configuration ov13858_crl_configuration = {

	.power_items = ARRAY_SIZE(ov13858_power_items),
	.power_entities = ov13858_power_items,

	.powerup_regs_items = ARRAY_SIZE(ov13858_powerup_regset),
	.powerup_regs = ov13858_powerup_regset,

	.poweroff_regs_items = 0,
	.poweroff_regs = 0,

	.id_reg_items = ARRAY_SIZE(ov13858_sensor_detect_regset),
	.id_regs = ov13858_sensor_detect_regset,

	.subdev_items = ARRAY_SIZE(ov13858_sensor_subdevs),
	.subdevs = ov13858_sensor_subdevs,

	.sensor_limits = &ov13858_sensor_limits,

	.pll_config_items = ARRAY_SIZE(ov13858_pll_configurations),
	.pll_configs = ov13858_pll_configurations,

	.modes_items = ARRAY_SIZE(ov13858_modes),
	.modes = ov13858_modes,

	.streamon_regs_items = ARRAY_SIZE(ov13858_streamon_regs),
	.streamon_regs = ov13858_streamon_regs,

	.streamoff_regs_items = ARRAY_SIZE(ov13858_streamoff_regs),
	.streamoff_regs = ov13858_streamoff_regs,

	.v4l2_ctrls_items = ARRAY_SIZE(ov13858_v4l2_ctrls),
	.v4l2_ctrl_bank = ov13858_v4l2_ctrls,

	.csi_fmts_items = ARRAY_SIZE(ov13858_crl_csi_data_fmt),
	.csi_fmts = ov13858_crl_csi_data_fmt,

	.flip_items = ARRAY_SIZE(ov13858_flip_configurations),
	.flip_data = ov13858_flip_configurations,

#ifdef CONFIG_CRLMODULE_RD_NVM_TO_VCM
       .crl_nvm_info.nvm_flags = CRL_NVM_ADDR_MODE_8BIT,
       .crl_nvm_info.nvm_preop_regs_items = 0,
       .crl_nvm_info.nvm_postop_regs_items = 0,
       .crl_nvm_info.nvm_blobs_items = ARRAY_SIZE(ov13858_nvm_blobs),
       .crl_nvm_info.nvm_config = ov13858_nvm_blobs,
#endif

#ifdef CONFIG_VIDEO_CRLMODULE_OTP_VALIDATE
        .crl_otp_info.otp_preop_regs_items =
                ARRAY_SIZE(ov13858_otp_preop_regset),
        .crl_otp_info.otp_preop_regs = ov13858_otp_preop_regset,
        .crl_otp_info.otp_postop_regs_items =
                ARRAY_SIZE(ov13858_otp_postop_regset),
        .crl_otp_info.otp_postop_regs = ov13858_otp_postop_regset,

        .crl_otp_info.otp_mode_regs_items =
                ARRAY_SIZE(ov13858_otp_mode_regset),
        .crl_otp_info.otp_mode_regs = ov13858_otp_mode_regset,

        .crl_otp_info.otp_read_regs_items =
                ARRAY_SIZE(ov13858_sensor_otp_read_regset),
        .crl_otp_info.otp_read_regs = ov13858_sensor_otp_read_regset,

        .crl_otp_info.otp_id = 0x5168, /*chicony module*/
#endif



};

#endif  /* __CRLMODULE_ov13858_CONFIGURATION_H_ */
