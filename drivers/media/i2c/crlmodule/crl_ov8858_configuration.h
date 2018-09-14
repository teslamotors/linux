/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2014 - 2018 Intel Corporation
 *
 * Author: Vinod Govindapillai <vinod.govindapillai@intel.com>
 *
 */

#ifndef __CRLMODULE_OV8858_CONFIGURATION_H_
#define __CRLMODULE_OV8858_CONFIGURATION_H_

#include "crlmodule-nvm.h"
#include "crlmodule-sensor-ds.h"

static struct crl_register_write_rep ov8858_pll_360mbps[] = {
	{ 0x0300, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0301, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0302, CRL_REG_LEN_08BIT, 0x1e },/* pll1_multiplier = 30 */
	{ 0x0303, CRL_REG_LEN_08BIT, 0x00 },/* pll1_divm = /(1 + 0) */
	{ 0x0304, CRL_REG_LEN_08BIT, 0x03 },/* pll1_div_mipi = /8 */
	{ 0x0305, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0306, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x030A, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x030B, CRL_REG_LEN_08BIT, 0x01 },/* pll2_pre_div = /2 */
	{ 0x030c, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x030D, CRL_REG_LEN_08BIT, 0x44 },/* pll2_r_divp = 30 */
	{ 0x030E, CRL_REG_LEN_08BIT, 0x01 },/* pll2_r_divs = /2 */
	{ 0x030F, CRL_REG_LEN_08BIT, 0x04 },/* pll2_r_divsp = /(1 + 4) */
	/* pll2_pre_div0 = /1, pll2_r_divdac = /(1 + 1) */
	{ 0x0312, CRL_REG_LEN_08BIT, 0x02 },
	/* mipi_lane_mode = 1+3, mipi_lvds_sel = 1 = MIPI enable,
	 * r_phy_pd_mipi_man = 0, lane_dis_option = 0
	 */
	{ 0x3018, CRL_REG_LEN_08BIT, 0x72 },
};


static struct crl_register_write_rep ov8858_powerup_regset[] = {
	/*Reset*/
	{ 0x0103, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0100, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3007, CRL_REG_LEN_08BIT, 0x80 },
	/* Npump clock div = /2, Ppump clock div = /4 */
	{ 0x3015, CRL_REG_LEN_08BIT, 0x01 },
	/* Clock switch output = normal, pclk_div = /1 */
	{ 0x3020, CRL_REG_LEN_08BIT, 0x93 },
	{ 0x3031, CRL_REG_LEN_08BIT, 0x0a },
	{ 0x3032, CRL_REG_LEN_08BIT, 0x80 },
	{ 0x3022, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x3034, CRL_REG_LEN_08BIT, 0x00 },
	/* sclk_div = /1, sclk_pre_div = /1, chip debug = 1 */
	{ 0x3106, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x3305, CRL_REG_LEN_08BIT, 0xF1 },
	{ 0x3307, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x3308, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3309, CRL_REG_LEN_08BIT, 0x28 },
	{ 0x330A, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x330B, CRL_REG_LEN_08BIT, 0x20 },
	{ 0x330C, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x330D, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x330E, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x330F, CRL_REG_LEN_08BIT, 0x40 },
	/*
	 * Digital fraction gain delay option = Delay 1 frame,
	 * Gain change delay option = Delay 1 frame,
	 * Gain delay option = Delay 1 frame,
	 * Gain manual as sensor gain = Input gain as real gain format,
	 * Exposure delay option (must be 0 = Delay 1 frame,
	 * Exposure change delay option (must be 0) = Delay 1 frame
	 */
	{ 0x3503, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3505, CRL_REG_LEN_08BIT, 0x80 },/* gain conversation option */
	/*
	 * [10:7] are integer gain, [6:0] are fraction gain. For example:
	 * 0x80 is 1x gain, CRL_REG_LEN_08BIT, 0x100 is 2x gain,
	 * CRL_REG_LEN_08BIT, 0x1C0 is 3.5x gain
	 */
	{ 0x3508, CRL_REG_LEN_08BIT, 0x02 },/* long gain = 0x0200 */
	{ 0x3509, CRL_REG_LEN_08BIT, 0x00 },/* long gain = 0x0200 */
	{ 0x350C, CRL_REG_LEN_08BIT, 0x00 },/* short gain = 0x0080 */
	{ 0x350D, CRL_REG_LEN_08BIT, 0x80 },/* short gain = 0x0080 */
	{ 0x3510, CRL_REG_LEN_08BIT, 0x00 },/* short exposure = 0x000200 */
	{ 0x3511, CRL_REG_LEN_08BIT, 0x02 },/* short exposure = 0x000200 */
	{ 0x3512, CRL_REG_LEN_08BIT, 0x00 },/* short exposure = 0x000200 */
	{ 0x3600, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3601, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3602, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3603, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3604, CRL_REG_LEN_08BIT, 0x22 },
	{ 0x3605, CRL_REG_LEN_08BIT, 0x30 },
	{ 0x3606, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3607, CRL_REG_LEN_08BIT, 0x20 },
	{ 0x3608, CRL_REG_LEN_08BIT, 0x11 },
	{ 0x3609, CRL_REG_LEN_08BIT, 0x28 },
	{ 0x360A, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x360B, CRL_REG_LEN_08BIT, 0x06 },
	{ 0x360C, CRL_REG_LEN_08BIT, 0xD4 },
	{ 0x360D, CRL_REG_LEN_08BIT, 0x40 },
	{ 0x360E, CRL_REG_LEN_08BIT, 0x0C },
	{ 0x360F, CRL_REG_LEN_08BIT, 0x20 },
	{ 0x3610, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x3611, CRL_REG_LEN_08BIT, 0x20 },
	{ 0x3612, CRL_REG_LEN_08BIT, 0x88 },
	{ 0x3613, CRL_REG_LEN_08BIT, 0x80 },
	{ 0x3614, CRL_REG_LEN_08BIT, 0x58 },
	{ 0x3615, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3616, CRL_REG_LEN_08BIT, 0x4A },
	{ 0x3617, CRL_REG_LEN_08BIT, 0x40 },
	{ 0x3618, CRL_REG_LEN_08BIT, 0x5a },
	{ 0x3619, CRL_REG_LEN_08BIT, 0x70 },
	{ 0x361A, CRL_REG_LEN_08BIT, 0x99 },
	{ 0x361B, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x361C, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x361D, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x361E, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x361F, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3638, CRL_REG_LEN_08BIT, 0xFF },
	{ 0x3633, CRL_REG_LEN_08BIT, 0x0f },
	{ 0x3634, CRL_REG_LEN_08BIT, 0x0f },
	{ 0x3635, CRL_REG_LEN_08BIT, 0x0f },
	{ 0x3636, CRL_REG_LEN_08BIT, 0x12 },
	{ 0x3645, CRL_REG_LEN_08BIT, 0x13 },
	{ 0x3646, CRL_REG_LEN_08BIT, 0x83 },
	{ 0x364A, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x3700, CRL_REG_LEN_08BIT, 0x30 },
	{ 0x3701, CRL_REG_LEN_08BIT, 0x18 },
	{ 0x3702, CRL_REG_LEN_08BIT, 0x50 },
	{ 0x3703, CRL_REG_LEN_08BIT, 0x32 },
	{ 0x3704, CRL_REG_LEN_08BIT, 0x28 },
	{ 0x3705, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3706, CRL_REG_LEN_08BIT, 0x82 },
	{ 0x3707, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x3708, CRL_REG_LEN_08BIT, 0x48 },
	{ 0x3709, CRL_REG_LEN_08BIT, 0x66 },
	{ 0x370A, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x370B, CRL_REG_LEN_08BIT, 0x82 },
	{ 0x370C, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x3712, CRL_REG_LEN_08BIT, 0x44 },
	{ 0x3714, CRL_REG_LEN_08BIT, 0x24 },
	{ 0x3718, CRL_REG_LEN_08BIT, 0x14 },
	{ 0x3719, CRL_REG_LEN_08BIT, 0x31 },
	{ 0x371E, CRL_REG_LEN_08BIT, 0x31 },
	{ 0x371F, CRL_REG_LEN_08BIT, 0x7F },
	{ 0x3720, CRL_REG_LEN_08BIT, 0x0A },
	{ 0x3721, CRL_REG_LEN_08BIT, 0x0A },
	{ 0x3724, CRL_REG_LEN_08BIT, 0x0C },
	{ 0x3725, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x3726, CRL_REG_LEN_08BIT, 0x0C },
	{ 0x3728, CRL_REG_LEN_08BIT, 0x0A },
	{ 0x3729, CRL_REG_LEN_08BIT, 0x03 },
	{ 0x372A, CRL_REG_LEN_08BIT, 0x06 },
	{ 0x372B, CRL_REG_LEN_08BIT, 0xA6 },
	{ 0x372C, CRL_REG_LEN_08BIT, 0xA6 },
	{ 0x372D, CRL_REG_LEN_08BIT, 0xA6 },
	{ 0x372E, CRL_REG_LEN_08BIT, 0x0C },
	{ 0x372F, CRL_REG_LEN_08BIT, 0x20 },
	{ 0x3730, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x3731, CRL_REG_LEN_08BIT, 0x0C },
	{ 0x3732, CRL_REG_LEN_08BIT, 0x28 },
	{ 0x3733, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x3734, CRL_REG_LEN_08BIT, 0x40 },
	{ 0x3736, CRL_REG_LEN_08BIT, 0x30 },
	{ 0x373A, CRL_REG_LEN_08BIT, 0x0A },
	{ 0x373B, CRL_REG_LEN_08BIT, 0x0B },
	{ 0x373C, CRL_REG_LEN_08BIT, 0x14 },
	{ 0x373E, CRL_REG_LEN_08BIT, 0x06 },
	{ 0x3750, CRL_REG_LEN_08BIT, 0x0a },
	{ 0x3751, CRL_REG_LEN_08BIT, 0x0e },
	{ 0x3755, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x3758, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3759, CRL_REG_LEN_08BIT, 0x4C },
	{ 0x375A, CRL_REG_LEN_08BIT, 0x0C },
	{ 0x375B, CRL_REG_LEN_08BIT, 0x26 },
	{ 0x375C, CRL_REG_LEN_08BIT, 0x20 },
	{ 0x375D, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x375E, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x375F, CRL_REG_LEN_08BIT, 0x28 },
	{ 0x3760, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3761, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3762, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3763, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3766, CRL_REG_LEN_08BIT, 0xFF },
	{ 0x376B, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3772, CRL_REG_LEN_08BIT, 0x46 },
	{ 0x3773, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x3774, CRL_REG_LEN_08BIT, 0x2C },
	{ 0x3775, CRL_REG_LEN_08BIT, 0x13 },
	{ 0x3776, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x3777, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3778, CRL_REG_LEN_08BIT, 0x17 },
	{ 0x37A0, CRL_REG_LEN_08BIT, 0x88 },
	{ 0x37A1, CRL_REG_LEN_08BIT, 0x7A },
	{ 0x37A2, CRL_REG_LEN_08BIT, 0x7A },
	{ 0x37A3, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x37A4, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x37A5, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x37A6, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x37A7, CRL_REG_LEN_08BIT, 0x88 },
	{ 0x37A8, CRL_REG_LEN_08BIT, 0x98 },
	{ 0x37A9, CRL_REG_LEN_08BIT, 0x98 },
	{ 0x37AA, CRL_REG_LEN_08BIT, 0x88 },
	{ 0x37AB, CRL_REG_LEN_08BIT, 0x5C },
	{ 0x37AC, CRL_REG_LEN_08BIT, 0x5C },
	{ 0x37AD, CRL_REG_LEN_08BIT, 0x55 },
	{ 0x37AE, CRL_REG_LEN_08BIT, 0x19 },
	{ 0x37AF, CRL_REG_LEN_08BIT, 0x19 },
	{ 0x37B0, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x37B1, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x37B2, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x37B3, CRL_REG_LEN_08BIT, 0x84 },
	{ 0x37B4, CRL_REG_LEN_08BIT, 0x84 },
	{ 0x37B5, CRL_REG_LEN_08BIT, 0x60 },
	{ 0x37B6, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x37B7, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x37B8, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x37B9, CRL_REG_LEN_08BIT, 0xFF },
	{ 0x3800, CRL_REG_LEN_08BIT, 0x00 },/* h_crop_start high */
	{ 0x3801, CRL_REG_LEN_08BIT, 0x0C },/* h_crop_start low */
	{ 0x3802, CRL_REG_LEN_08BIT, 0x00 },/* v_crop_start high */
	{ 0x3803, CRL_REG_LEN_08BIT, 0x0C },/* v_crop_start low */
	{ 0x3804, CRL_REG_LEN_08BIT, 0x0C },/* h_crop_end high */
	{ 0x3805, CRL_REG_LEN_08BIT, 0xD3 },/* h_crop_end low */
	{ 0x3806, CRL_REG_LEN_08BIT, 0x09 },/* v_crop_end high */
	{ 0x3807, CRL_REG_LEN_08BIT, 0xA3 },/* v_crop_end low */
	{ 0x3808, CRL_REG_LEN_08BIT, 0x0C },/* h_output_size high */
	{ 0x3809, CRL_REG_LEN_08BIT, 0xC0 },/* h_output_size low */
	{ 0x380A, CRL_REG_LEN_08BIT, 0x09 },/* v_output_size high */
	{ 0x380B, CRL_REG_LEN_08BIT, 0x90 },/* v_output_size low */
	{ 0x380C, CRL_REG_LEN_08BIT, 0x07 },/* horizontal timing size high */
	{ 0x380D, CRL_REG_LEN_08BIT, 0x94 },/* horizontal timing size low */
	{ 0x380E, CRL_REG_LEN_08BIT, 0x0A },/* vertical timing size high */
	{ 0x380F, CRL_REG_LEN_08BIT, 0x0D },/* vertical timing size low */
	{ 0x3810, CRL_REG_LEN_08BIT, 0x00 },/* h_win offset high */
	{ 0x3811, CRL_REG_LEN_08BIT, 0x04 },/* h_win offset low */
	{ 0x3812, CRL_REG_LEN_08BIT, 0x00 },/* v_win offset high */
	{ 0x3813, CRL_REG_LEN_08BIT, 0x02 },/* v_win offset low */
	{ 0x3814, CRL_REG_LEN_08BIT, 0x01 },/* h_odd_inc */
	{ 0x3815, CRL_REG_LEN_08BIT, 0x01 },/* h_even_inc */
	{ 0x3820, CRL_REG_LEN_08BIT, 0x00 },/* format1 */
	{ 0x3821, CRL_REG_LEN_08BIT, 0x40 },/* format2 */
	{ 0x382A, CRL_REG_LEN_08BIT, 0x01 },/* v_odd_inc */
	{ 0x382B, CRL_REG_LEN_08BIT, 0x01 },/* v_even_inc */
	{ 0x3830, CRL_REG_LEN_08BIT, 0x06 },
	{ 0x3836, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x3837, CRL_REG_LEN_08BIT, 0x18 },
	{ 0x3841, CRL_REG_LEN_08BIT, 0xFF },/* AUTO_SIZE_CTRL */
	{ 0x3846, CRL_REG_LEN_08BIT, 0x48 },
	{ 0x3D85, CRL_REG_LEN_08BIT, 0x16 },/* OTP_REG85 */
	{ 0x3D8C, CRL_REG_LEN_08BIT, 0x73 },
	{ 0x3D8D, CRL_REG_LEN_08BIT, 0xde },
	{ 0x3F08, CRL_REG_LEN_08BIT, 0x10 },/* PSRAM control register */
	{ 0x4000, CRL_REG_LEN_08BIT, 0xF1 },/* BLC CTRL00 = default */
	{ 0x4001, CRL_REG_LEN_08BIT, 0x00 },/* BLC CTRL01 */
	{ 0x4002, CRL_REG_LEN_08BIT, 0x27 },/* BLC offset = 0x27 */
	{ 0x4005, CRL_REG_LEN_08BIT, 0x10 },/* BLC target = 0x0010 */
	{ 0x4009, CRL_REG_LEN_08BIT, 0x81 },/* BLC CTRL09 */
	{ 0x400B, CRL_REG_LEN_08BIT, 0x0C },/* BLC CTRL0B = default */
	{ 0x4011, CRL_REG_LEN_08BIT, 0x20 },
	{ 0x401B, CRL_REG_LEN_08BIT, 0x00 },/* Zero line R coeff. = 0x0000 */
	{ 0x401D, CRL_REG_LEN_08BIT, 0x00 },/* Zero line T coeff. = 0x0000 */
	{ 0x401F, CRL_REG_LEN_08BIT, 0x00 },/* BLC CTRL1F */
	{ 0x4020, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4021, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x4022, CRL_REG_LEN_08BIT, 0x0C },
	{ 0x4023, CRL_REG_LEN_08BIT, 0x60 },
	{ 0x4024, CRL_REG_LEN_08BIT, 0x0f },
	{ 0x4025, CRL_REG_LEN_08BIT, 0x36 },
	{ 0x4026, CRL_REG_LEN_08BIT, 0x0f },
	{ 0x4027, CRL_REG_LEN_08BIT, 0x37 },
	{ 0x4028, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4029, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x402A, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x402B, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x402C, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x402D, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x402E, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x402F, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x4034, CRL_REG_LEN_08BIT, 0x3F },
	{ 0x403D, CRL_REG_LEN_08BIT, 0x04 },/* BLC CTRL3D */
	{ 0x4300, CRL_REG_LEN_08BIT, 0xFF },/* clip_max[11:4] = 0xFFF */
	{ 0x4301, CRL_REG_LEN_08BIT, 0x00 },/* clip_min[11:4] = 0 */
	{ 0x4302, CRL_REG_LEN_08BIT, 0x0F },/* clip_min/max[3:0] */
	{ 0x4316, CRL_REG_LEN_08BIT, 0x00 },/* CTRL16 = default */
	{ 0x4503, CRL_REG_LEN_08BIT, 0x18 },
	{ 0x4600, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x4601, CRL_REG_LEN_08BIT, 0x97 },
	/* wkup_dly = Mark1 wakeup delay/2^10 = 0x25 */
	{ 0x4808, CRL_REG_LEN_08BIT, 0x25 },
	{ 0x4816, CRL_REG_LEN_08BIT, 0x12 },/* Embedded data type */
	{ 0x5A08, CRL_REG_LEN_08BIT, 0x02 },/* Data in beginning of the frame */
	{ 0x5041, CRL_REG_LEN_08BIT, 0x01 },/* ISP CTRL41 - embedded data=on */
	{ 0x4307, CRL_REG_LEN_08BIT, 0x31 },/* Embedded_en */
	{ 0x481F, CRL_REG_LEN_08BIT, 0x32 },/* clk_prepare_min = 0x32 */
	{ 0x4837, CRL_REG_LEN_08BIT, 0x16 },/* pclk_period = 0x14 */
	{ 0x4850, CRL_REG_LEN_08BIT, 0x10 },/* LANE SEL01 */
	{ 0x4851, CRL_REG_LEN_08BIT, 0x32 },/* LANE SEL02 */
	{ 0x4B00, CRL_REG_LEN_08BIT, 0x2A },
	{ 0x4B0D, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4D00, CRL_REG_LEN_08BIT, 0x04 },/* TPM_CTRL_REG */
	{ 0x4D01, CRL_REG_LEN_08BIT, 0x18 },/* TPM_CTRL_REG */
	{ 0x4D02, CRL_REG_LEN_08BIT, 0xC3 },/* TPM_CTRL_REG */
	{ 0x4D03, CRL_REG_LEN_08BIT, 0xFF },/* TPM_CTRL_REG */
	{ 0x4D04, CRL_REG_LEN_08BIT, 0xFF },/* TPM_CTRL_REG */
	{ 0x4D05, CRL_REG_LEN_08BIT, 0xFF },/* TPM_CTRL_REG */
	/*
	 * Lens correction (LENC) function enable = 0
	 * Slave sensor AWB Gain function enable = 1
	 * Slave sensor AWB Statistics function enable = 1
	 * Master sensor AWB Gain function enable = 1
	 * Master sensor AWB Statistics function enable = 1
	 * Black DPC function enable = 1
	 * White DPC function enable =1
	 */
	{ 0x5000, CRL_REG_LEN_08BIT, 0x7E },
	{ 0x5001, CRL_REG_LEN_08BIT, 0x01 },/* BLC function enable = 1 */
	/*
	 * Horizontal scale function enable = 0
	 * WBMATCH bypass mode = Select slave sensor's gain
	 * WBMATCH function enable = 0
	 * Master MWB gain support RGBC = 0
	 * OTP_DPC function enable = 1
	 * Manual mode of VarioPixel function enable = 0
	 * Manual enable of VarioPixel function enable = 0
	 * Use VSYNC to latch ISP modules's function enable signals = 0
	 */
	{ 0x5002, CRL_REG_LEN_08BIT, 0x08 },
	/*
	 * Bypass all ISP modules after BLC module = 0
	 * DPC_DBC buffer control enable = 1
	 * WBMATCH VSYNC selection = Select master sensor's VSYNC fall
	 * Select master AWB gain to embed line = AWB gain before manual mode
	 * Enable BLC's input flip_i signal = 0
	 */
	{ 0x5003, CRL_REG_LEN_08BIT, 0x20 },
	{ 0x5041, CRL_REG_LEN_08BIT, 0x1D },/* ISP CTRL41 - embedded data=on */
	{ 0x5046, CRL_REG_LEN_08BIT, 0x12 },/* ISP CTRL46 = default */
	/*
	 * Tail enable = 1
	 * Saturate cross cluster enable = 1
	 * Remove cross cluster enable = 1
	 * Enable to remove connected defect pixels in same channel = 1
	 * Enable to remove connected defect pixels in different channel = 1
	 * Smooth enable, use average G for recovery = 1
	 * Black/white sensor mode enable = 0
	 * Manual mode enable = 0
	 */
	{ 0x5780, CRL_REG_LEN_08BIT, 0x3e },
	{ 0x5781, CRL_REG_LEN_08BIT, 0x0f },
	{ 0x5782, CRL_REG_LEN_08BIT, 0x44 },
	{ 0x5783, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x5784, CRL_REG_LEN_08BIT, 0x01 },/* DPC CTRL04 */
	{ 0x5785, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x5786, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x5787, CRL_REG_LEN_08BIT, 0x04 },/* DPC CTRL07 */
	{ 0x5788, CRL_REG_LEN_08BIT, 0x02 },/* DPC CTRL08 */
	{ 0x5789, CRL_REG_LEN_08BIT, 0x0f },
	{ 0x578A, CRL_REG_LEN_08BIT, 0xfd },/* DPC CTRL0A */
	{ 0x578B, CRL_REG_LEN_08BIT, 0xf5 },/* DPC CTRL0B */
	{ 0x578C, CRL_REG_LEN_08BIT, 0xf5 },/* DPC CTRL0C */
	{ 0x578D, CRL_REG_LEN_08BIT, 0x03 },/* DPC CTRL0D */
	{ 0x578E, CRL_REG_LEN_08BIT, 0x08 },/* DPC CTRL0E */
	{ 0x578F, CRL_REG_LEN_08BIT, 0x0c },/* DPC CTRL0F */
	{ 0x5790, CRL_REG_LEN_08BIT, 0x08 },/* DPC CTRL10 */
	{ 0x5791, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x5792, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x5793, CRL_REG_LEN_08BIT, 0x52 },
	{ 0x5794, CRL_REG_LEN_08BIT, 0xa3 },
	{ 0x586E, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x586F, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x58F8, CRL_REG_LEN_08BIT, 0x3d },
	{ 0x5871, CRL_REG_LEN_08BIT, 0x0d },
	{ 0x5870, CRL_REG_LEN_08BIT, 0x18 },
	{ 0x5901, CRL_REG_LEN_08BIT, 0x00 },/* VAP CTRL01 = default */
	{ 0x5B00, CRL_REG_LEN_08BIT, 0x02 },/* OTP CTRL00 */
	{ 0x5B01, CRL_REG_LEN_08BIT, 0x10 },/* OTP CTRL01 */
	{ 0x5B02, CRL_REG_LEN_08BIT, 0x03 },/* OTP CTRL02 */
	{ 0x5B03, CRL_REG_LEN_08BIT, 0xCF },/* OTP CTRL03 */
	{ 0x5B05, CRL_REG_LEN_08BIT, 0x6C },/* OTP CTRL05 = default */
	{ 0x5E00, CRL_REG_LEN_08BIT, 0x00 },/* PRE CTRL00 = default */
	{ 0x5E01, CRL_REG_LEN_08BIT, 0x41 },/* PRE_CTRL01 = default */
	{ 0x4825, CRL_REG_LEN_08BIT, 0x3a },
	{ 0x4826, CRL_REG_LEN_08BIT, 0x40 },
	{ 0x4808, CRL_REG_LEN_08BIT, 0x25 },
	{ 0x3763, CRL_REG_LEN_08BIT, 0x18 },
	{ 0x3768, CRL_REG_LEN_08BIT, 0xcc },
	{ 0x470b, CRL_REG_LEN_08BIT, 0x28 },
	{ 0x4202, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x400d, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x4040, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x403e, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x4041, CRL_REG_LEN_08BIT, 0xc6 },
	{ 0x400a, CRL_REG_LEN_08BIT, 0x01 },
};

static struct crl_register_write_rep ov8858_mode_8m_native[] = {
	{ 0x382d, CRL_REG_LEN_08BIT, 0x20 },
	{ 0x3808, CRL_REG_LEN_08BIT, 0x0C },/* h_output_size high 3264 x 2448 */
	{ 0x3809, CRL_REG_LEN_08BIT, 0xc0 },/* h_output_size low */
	{ 0x380A, CRL_REG_LEN_08BIT, 0x09 },/* v_output_size high */
	{ 0x380B, CRL_REG_LEN_08BIT, 0x90 },/* v_output_size low */
	{ 0x4022, CRL_REG_LEN_08BIT, 0x0C },
	{ 0x4023, CRL_REG_LEN_08BIT, 0x60 },
	{ 0x4600, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x4601, CRL_REG_LEN_08BIT, 0x97 },
};

static struct crl_register_write_rep ov8858_mode_6m_native[] = {
	{ 0x382d, CRL_REG_LEN_08BIT, 0x80 },
	{ 0x3808, CRL_REG_LEN_08BIT, 0x0C },/* h_output_size high 3264 x 1836 */
	{ 0x3809, CRL_REG_LEN_08BIT, 0xc0 },/* h_output_size low */
	{ 0x380A, CRL_REG_LEN_08BIT, 0x07 },/* v_output_size high */
	{ 0x380B, CRL_REG_LEN_08BIT, 0x2c },/* v_output_size low */
	{ 0x4022, CRL_REG_LEN_08BIT, 0x0C },
	{ 0x4023, CRL_REG_LEN_08BIT, 0x60 },
	{ 0x4600, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x4601, CRL_REG_LEN_08BIT, 0x97 },
};

static struct crl_register_write_rep ov8858_mode_8m_full[] = {
	{ 0x382d, CRL_REG_LEN_08BIT, 0x20 },
	{ 0x3808, CRL_REG_LEN_08BIT, 0x0C },/* h_output_size high 3280 x 2464 */
	{ 0x3809, CRL_REG_LEN_08BIT, 0xD0 },/* h_output_size low */
	{ 0x380A, CRL_REG_LEN_08BIT, 0x09 },/* v_output_size high */
	{ 0x380B, CRL_REG_LEN_08BIT, 0xA0 },/* v_output_size low */
	{ 0x4022, CRL_REG_LEN_08BIT, 0x0C },
	{ 0x4023, CRL_REG_LEN_08BIT, 0x60 },
	{ 0x4600, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x4601, CRL_REG_LEN_08BIT, 0x97 },
};

static struct crl_register_write_rep ov8858_mode_6m_full[] = {
	{ 0x382d, CRL_REG_LEN_08BIT, 0x80 },
	{ 0x3808, CRL_REG_LEN_08BIT, 0x0C },/* h_output_size high 3280 x 1852 */
	{ 0x3809, CRL_REG_LEN_08BIT, 0xD0 },/* h_output_size low */
	{ 0x380A, CRL_REG_LEN_08BIT, 0x07 },/* v_output_size high */
	{ 0x380B, CRL_REG_LEN_08BIT, 0x3c },/* v_output_size low */
	{ 0x4022, CRL_REG_LEN_08BIT, 0x0C },
	{ 0x4023, CRL_REG_LEN_08BIT, 0x60 },
	{ 0x4600, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x4601, CRL_REG_LEN_08BIT, 0x97 },
};

static struct crl_register_write_rep ov8858_mode_1080[] = {
	{ 0x382d, CRL_REG_LEN_08BIT, 0xa0 },
	{ 0x3808, CRL_REG_LEN_08BIT, 0x07 },/* h_output_size high*/
	{ 0x3809, CRL_REG_LEN_08BIT, 0x80 },/* h_output_size low */
	{ 0x380A, CRL_REG_LEN_08BIT, 0x04 },/* v_output_size high */
	{ 0x380B, CRL_REG_LEN_08BIT, 0x38 },/* v_output_size low */
	{ 0x4022, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x4023, CRL_REG_LEN_08BIT, 0x20 },
	{ 0x4600, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4601, CRL_REG_LEN_08BIT, 0xef },
};

static struct crl_register_write_rep ov8858_mode_1920x1440_crop[] = {
	{ 0x382d, CRL_REG_LEN_08BIT, 0xa0 },
	{ 0x3808, CRL_REG_LEN_08BIT, 0x07 },/* h_output_size high*/
	{ 0x3809, CRL_REG_LEN_08BIT, 0x80 },/* h_output_size low */
	{ 0x380A, CRL_REG_LEN_08BIT, 0x05 },/* v_output_size high */
	{ 0x380B, CRL_REG_LEN_08BIT, 0xA0 },/* v_output_size low */
	{ 0x4022, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x4023, CRL_REG_LEN_08BIT, 0x20 },
	{ 0x4600, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4601, CRL_REG_LEN_08BIT, 0xef },
};

static struct crl_register_write_rep ov8858_mode_1984x1116_crop[] = {
	{ 0x382d, CRL_REG_LEN_08BIT, 0xa0 },
	{ 0x3808, CRL_REG_LEN_08BIT, 0x07 },/* h_output_size high*/
	{ 0x3809, CRL_REG_LEN_08BIT, 0xC0 },/* h_output_size low */
	{ 0x380A, CRL_REG_LEN_08BIT, 0x04 },/* v_output_size high */
	{ 0x380B, CRL_REG_LEN_08BIT, 0x5C },/* v_output_size low */
	{ 0x4022, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x4023, CRL_REG_LEN_08BIT, 0x20 },
	{ 0x4600, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4601, CRL_REG_LEN_08BIT, 0xef },
};

static struct crl_register_write_rep ov8858_streamon_regs[] = {
	{ 0x0100, CRL_REG_LEN_08BIT, 0x01 }
};

static struct crl_register_write_rep ov8858_streamoff_regs[] = {
	{ 0x0100, CRL_REG_LEN_08BIT, 0x00 }
};

static struct crl_register_write_rep ov8858_data_fmt_width10[] = {
	{ 0x3031, CRL_REG_LEN_08BIT, 0x0a }
};

static struct crl_arithmetic_ops ov8858_vflip_ops[] = {
	{
		.op = CRL_BITWISE_LSHIFT,
		.operand.entity_val = 1,
	 },
};

static struct crl_arithmetic_ops ov8858_hflip_ops[] = {
	{
		.op = CRL_BITWISE_LSHIFT,
		.operand.entity_val = 1,
	 },
};

static struct crl_arithmetic_ops ov8858_hblank_ops[] = {
	{
		.op = CRL_BITWISE_RSHIFT,
		.operand.entity_val = 1,
	 },
};

static struct crl_arithmetic_ops ov8858_exposure_ops[] = {
	{
		.op = CRL_BITWISE_LSHIFT,
		.operand.entity_val = 4,
	 },
};

static struct crl_dynamic_register_access ov8858_v_flip_regs[] = {
	{
		.address = 0x3820,
		.len = CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE,
		.ops_items = ARRAY_SIZE(ov8858_vflip_ops),
		.ops = ov8858_vflip_ops,
		.mask = 0x2,
	 },
};

static struct crl_dynamic_register_access ov8858_dig_gain_regs[] = {
	{
		.address = 0x5032,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	},
	{
		.address = 0x5034,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	},
	{
		.address = 0x5036,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	},
};

static struct crl_dynamic_register_access ov8858_h_flip_regs[] = {
	{
		.address = 0x3821,
		.len = CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE,
		.ops_items = ARRAY_SIZE(ov8858_hflip_ops),
		.ops = ov8858_hflip_ops,
		.mask = 0x2,
	 },
};

struct crl_register_write_rep ov8858_poweroff_regset[] = {
	{ 0x0103, CRL_REG_LEN_08BIT, 0x01  },
};

static struct crl_dynamic_register_access ov8858_ana_gain_global_regs[] = {
	{
		.address = 0x3508,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0x7ff,
	 },
};

static struct crl_dynamic_register_access ov8858_exposure_regs[] = {
	{
		.address = 0x3500,
		.len = CRL_REG_LEN_24BIT,
		.ops_items = ARRAY_SIZE(ov8858_exposure_ops),
		.ops = ov8858_exposure_ops,
		.mask = 0x0ffff0,
	 },
};

static struct crl_dynamic_register_access ov8858_vblank_regs[] = {
	{
		.address = 0x380E,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	 },
};

static struct crl_dynamic_register_access ov8858_hblank_regs[] = {
	{
		.address = 0x380C,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = ARRAY_SIZE(ov8858_hblank_ops),
		.ops = ov8858_hblank_ops,
		.mask = 0xffff,
	 },
};

static struct crl_sensor_detect_config ov8858_sensor_detect_regset[] = {
	{
		.reg = { 0x300B, CRL_REG_LEN_08BIT, 0x000000ff  },
		.width = 7,
	 },
	{
		.reg = { 0x300C, CRL_REG_LEN_08BIT, 0x000000ff  },
		.width = 7,
	 },
};

static struct crl_pll_configuration ov8858_pll_configurations[] = {
	{
		.input_clk = 24000000,
		.op_sys_clk = 360000000,
		.bitsperpixel = 10,
		.pixel_rate_csi = 180000000,
		.pixel_rate_pa = 290133334,
		.csi_lanes = 4,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = ARRAY_SIZE(ov8858_pll_360mbps),
		.pll_regs = ov8858_pll_360mbps,
	 },

};

static struct crl_subdev_rect_rep ov8858_8m_rects_native[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3264,
		.in_rect.height = 2448,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3264,
		.out_rect.height = 2448,
	 },
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3264,
		.in_rect.height = 2448,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3264,
		.out_rect.height = 2448,
	 },
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3264,
		.in_rect.height = 2448,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3264,
		.out_rect.height = 2448,
	 },
};

static struct crl_subdev_rect_rep ov8858_6m_rects_native[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3264,
		.in_rect.height = 2448,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3264,
		.out_rect.height = 2448,
	 },
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3264,
		.in_rect.height = 2448,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3264,
		.out_rect.height = 2448,
	 },
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3264,
		.in_rect.height = 2448,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3264,
		.out_rect.height = 1836,
	 },
};


static struct crl_subdev_rect_rep ov8858_8m_rects_full[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3280,
		.in_rect.height = 2464,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3280,
		.out_rect.height = 2464,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3280,
		.in_rect.height = 2464,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3280,
		.out_rect.height = 2464,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3280,
		.in_rect.height = 2464,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3280,
		.out_rect.height = 2464,
	},
};

static struct crl_subdev_rect_rep ov8858_6m_rects_full[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3280,
		.in_rect.height = 2464,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3280,
		.out_rect.height = 2464,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3280,
		.in_rect.height = 2464,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3280,
		.out_rect.height = 2464,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3280,
		.in_rect.height = 2464,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3280,
		.out_rect.height = 1852,
	},
};

static struct crl_subdev_rect_rep ov8858_1080_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3264,
		.in_rect.height = 2448,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3264,
		.out_rect.height = 2448,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3264,
		.in_rect.height = 2448,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3264,
		.out_rect.height = 2448,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3264,
		.in_rect.height = 2448,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1920,
		.out_rect.height = 1080,
	},
};

static struct crl_subdev_rect_rep ov8858_1920x1440_rects_crop[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3264,
		.in_rect.height = 2448,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3264,
		.out_rect.height = 2448,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3264,
		.in_rect.height = 2448,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3264,
		.out_rect.height = 2448,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3264,
		.in_rect.height = 2448,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1920,
		.out_rect.height = 1440,
	},
};

static struct crl_subdev_rect_rep ov8858_1984x1116_rects_crop[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3264,
		.in_rect.height = 2448,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3264,
		.out_rect.height = 2448,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3264,
		.in_rect.height = 2448,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3264,
		.out_rect.height = 2448,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3264,
		.in_rect.height = 2448,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1984,
		.out_rect.height = 1116,
	},
};

static struct crl_mode_rep ov8858_modes[] = {
	{
		.sd_rects_items = ARRAY_SIZE(ov8858_8m_rects_native),
		.sd_rects = ov8858_8m_rects_native,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 3264,
		.height = 2448,
		.min_llp = 3880,
		.min_fll = 2474,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(ov8858_mode_8m_native),
		.mode_regs = ov8858_mode_8m_native,
	},
	{
		.sd_rects_items = ARRAY_SIZE(ov8858_6m_rects_native),
		.sd_rects = ov8858_6m_rects_native,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 3264,
		.height = 1836,
		.min_llp = 5132,
		.min_fll = 1872,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(ov8858_mode_6m_native),
		.mode_regs = ov8858_mode_6m_native,
	},
	{
		.sd_rects_items = ARRAY_SIZE(ov8858_8m_rects_full),
		.sd_rects = ov8858_8m_rects_full,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 3280,
		.height = 2464,
		.min_llp = 3880,
		.min_fll = 2474,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(ov8858_mode_8m_full),
		.mode_regs = ov8858_mode_8m_full,
	},
	{
		.sd_rects_items = ARRAY_SIZE(ov8858_6m_rects_full),
		.sd_rects = ov8858_6m_rects_full,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 3280,
		.height = 1852,
		.min_llp = 5132,
		.min_fll = 1872,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(ov8858_mode_6m_full),
		.mode_regs = ov8858_mode_6m_full,
	},
	{
		.sd_rects_items = ARRAY_SIZE(ov8858_1080_rects),
		.sd_rects = ov8858_1080_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 1920,
		.height = 1080,
		.min_llp = 4284,
		.min_fll = 1120,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(ov8858_mode_1080),
		.mode_regs = ov8858_mode_1080,
	},
	{
		.sd_rects_items = ARRAY_SIZE(ov8858_1920x1440_rects_crop),
		.sd_rects = ov8858_1920x1440_rects_crop,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 1920,
		.height = 1440,
		.min_llp = 3880,
		.min_fll = 1480,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(ov8858_mode_1920x1440_crop),
		.mode_regs = ov8858_mode_1920x1440_crop,
	},
	{
		.sd_rects_items = ARRAY_SIZE(ov8858_1984x1116_rects_crop),
		.sd_rects = ov8858_1984x1116_rects_crop,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 1984,
		.height = 1116,
		.min_llp = 3880,
		.min_fll = 1120,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(ov8858_mode_1984x1116_crop),
		.mode_regs = ov8858_mode_1984x1116_crop,
	},

};

static struct crl_sensor_subdev_config ov8858_sensor_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.name = "ov8858 scaler",
	 },
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.name = "ov8858 binner",
	 },
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "ov8858 pixel array",
	 },
};

static struct crl_sensor_limits ov8858_sensor_limits = {
	.x_addr_min = 0,
	.y_addr_min = 0,
	.x_addr_max = 3280,
	.y_addr_max = 2464,
	.min_frame_length_lines = 160,
	.max_frame_length_lines = 65535,
	.min_line_length_pixels = 3880,
	.max_line_length_pixels = 32752,
	.scaler_m_min = 16,
	.scaler_m_max = 255,
	.scaler_n_min = 16,
	.scaler_n_max = 16,
	.min_even_inc = 1,
	.max_even_inc = 1,
	.min_odd_inc = 1,
	.max_odd_inc = 3,
};

static struct crl_flip_data ov8858_flip_configurations[] = {
	{
		.flip = CRL_FLIP_DEFAULT_NONE,
		.pixel_order = CRL_PIXEL_ORDER_BGGR,
	 },
	{
		.flip = CRL_FLIP_VFLIP,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
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

static struct crl_csi_data_fmt ov8858_crl_csi_data_fmt[] = {
	{
		.code = MEDIA_BUS_FMT_SGRBG10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.bits_per_pixel = 10,
		.regs_items = 1,
		.regs = ov8858_data_fmt_width10,
	 },
	{
		.code = MEDIA_BUS_FMT_SRGGB10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,
		.regs_items = 1,
		.bits_per_pixel = 10,
		.regs = ov8858_data_fmt_width10,
	 },
	{
		.code = MEDIA_BUS_FMT_SBGGR10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_BGGR,
		.regs_items = 1,
		.bits_per_pixel = 10,
		.regs = ov8858_data_fmt_width10,
	 },
	{
		.code = MEDIA_BUS_FMT_SGBRG10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_GBRG,
		.regs_items = 1,
		.bits_per_pixel = 10,
		.regs = ov8858_data_fmt_width10,
	 },
};

static struct crl_v4l2_ctrl ov8858_v4l2_ctrls[] = {
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
		.name = "V4L2_CID_ANALOGUE_GAIN",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = 4096,
		.data.std_data.step = 1,
		.data.std_data.def = 128,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(ov8858_ana_gain_global_regs),
		.regs = ov8858_ana_gain_global_regs,
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
		.regs_items = ARRAY_SIZE(ov8858_exposure_regs),
		.regs = ov8858_exposure_regs,
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
		.regs_items = ARRAY_SIZE(ov8858_h_flip_regs),
		.regs = ov8858_h_flip_regs,
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
		.regs_items = ARRAY_SIZE(ov8858_v_flip_regs),
		.regs = ov8858_v_flip_regs,
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
		.data.std_data.min = 160,
		.data.std_data.max = 65535,
		.data.std_data.step = 1,
		.data.std_data.def = 2474,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(ov8858_vblank_regs),
		.regs = ov8858_vblank_regs,
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
		.data.std_data.min = 1024,
		.data.std_data.max = 65520,
		.data.std_data.step = 1,
		.data.std_data.def = 3880,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(ov8858_hblank_regs),
		.regs = ov8858_hblank_regs,
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
		.regs_items = ARRAY_SIZE(ov8858_dig_gain_regs),
		.regs = ov8858_dig_gain_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
};

#define OV8858_OTP_START_ADDR 0x7010
#define OV8858_OTP_END_ADDR 0x7186

#define OV8858_OTP_LEN (OV8858_OTP_END_ADDR - OV8858_OTP_START_ADDR + 1)
#define OV8858_OTP_L_ADDR(x) (x & 0xff)
#define OV8858_OTP_H_ADDR(x) ((x >> 8) & 0xff)

static struct crl_register_write_rep ov8858_nvm_preop_regset[] = {
	/* Start streaming */
	{ 0x0100, CRL_REG_LEN_08BIT, 0x01 },
	/* Manual mode, program disable */
	{ 0x3D84, CRL_REG_LEN_08BIT, 0xC0 },
	/* Manual OTP start address for access */
	{ 0x3D88, CRL_REG_LEN_08BIT, OV8858_OTP_H_ADDR(OV8858_OTP_START_ADDR)},
	{ 0x0D89, CRL_REG_LEN_08BIT, OV8858_OTP_L_ADDR(OV8858_OTP_START_ADDR)},
	/* Manual OTP end address for access */
	{ 0x3D8A, CRL_REG_LEN_08BIT, OV8858_OTP_H_ADDR(OV8858_OTP_END_ADDR)},
	{ 0x3D8B, CRL_REG_LEN_08BIT, OV8858_OTP_L_ADDR(OV8858_OTP_END_ADDR)},
	/* OTP load enable */
	{ 0x3D81, CRL_REG_LEN_08BIT, 0x01 },
	/* Wait for the data to load into the buffer */
	{ 0x0000, CRL_REG_LEN_DELAY, 0x05 },
};

static struct crl_register_write_rep ov8858_nvm_postop_regset[] = {
	{ 0x0100, CRL_REG_LEN_08BIT, 0x00 }, /* Stop streaming */
};

static struct crl_nvm_blob ov8858_nvm_blobs[] = {
	{CRL_I2C_ADDRESS_NO_OVERRIDE, OV8858_OTP_START_ADDR, OV8858_OTP_LEN },
};

static struct crl_arithmetic_ops ov8858_frame_desc_width_ops[] = {
	{
	 .op = CRL_ASSIGNMENT,
	 .operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_VAR_REF,
	 .operand.entity_val = CRL_VAR_REF_OUTPUT_WIDTH,
	},
};

static struct crl_arithmetic_ops ov8858_frame_desc_height_ops[] = {
	{
	 .op = CRL_ASSIGNMENT,
	 .operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
	 .operand.entity_val = 1,
	},
};

static struct crl_frame_desc ov8858_frame_desc[] = {
	{
		.flags.entity_val = 0,
		.bpp.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_VAR_REF,
		.bpp.entity_val = CRL_VAR_REF_BITSPERPIXEL,
		.pixelcode.entity_val = MEDIA_BUS_FMT_FIXED,
		.length.entity_val = 0,
		.start_line.entity_val = 0,
		.start_pixel.entity_val = 0,
		.width = {
			 .ops_items = ARRAY_SIZE(ov8858_frame_desc_width_ops),
			 .ops = ov8858_frame_desc_width_ops,
			 },
		.height = {
			  .ops_items = ARRAY_SIZE(ov8858_frame_desc_height_ops),
			  .ops = ov8858_frame_desc_height_ops,
			  },
		.csi2_channel.entity_val = 0,
		.csi2_data_type.entity_val = 0x12,
	},
};

/* Power items, they are enabled in the order they are listed here */
static struct crl_power_seq_entity ov8858_power_items[] = {
	{
		.type = CRL_POWER_ETY_REGULATOR_FRAMEWORK,
		.ent_name = "VANA",
		.val = 2800000,
		.delay = 0,
	},
	{
		.type = CRL_POWER_ETY_REGULATOR_FRAMEWORK,
		.ent_name = "VDIG",
		.val = 1200000,
		.delay = 0,
	},
	{
		.type = CRL_POWER_ETY_REGULATOR_FRAMEWORK,
		.ent_name = "VIO",
		.val = 1800000,
		.delay = 0,
	},
	{
		.type = CRL_POWER_ETY_CLK_FRAMEWORK,
		.val = 24000000,
	},
	{
		.type = CRL_POWER_ETY_GPIO_FROM_PDATA,
		.val = 1,
		.delay = 10000,
	},
};

static struct crl_sensor_configuration ov8858_crl_configuration = {

	.power_items = ARRAY_SIZE(ov8858_power_items),
	.power_entities = ov8858_power_items,

	.powerup_regs_items = ARRAY_SIZE(ov8858_powerup_regset),
	.powerup_regs = ov8858_powerup_regset,

	.poweroff_regs_items = 0,
	.poweroff_regs = 0,


	.id_reg_items = ARRAY_SIZE(ov8858_sensor_detect_regset),
	.id_regs = ov8858_sensor_detect_regset,

	.subdev_items = ARRAY_SIZE(ov8858_sensor_subdevs),
	.subdevs = ov8858_sensor_subdevs,

	.sensor_limits = &ov8858_sensor_limits,

	.pll_config_items = ARRAY_SIZE(ov8858_pll_configurations),
	.pll_configs = ov8858_pll_configurations,

	.modes_items = ARRAY_SIZE(ov8858_modes),
	.modes = ov8858_modes,

	.streamon_regs_items = ARRAY_SIZE(ov8858_streamon_regs),
	.streamon_regs = ov8858_streamon_regs,

	.streamoff_regs_items = ARRAY_SIZE(ov8858_streamoff_regs),
	.streamoff_regs = ov8858_streamoff_regs,

	.v4l2_ctrls_items = ARRAY_SIZE(ov8858_v4l2_ctrls),
	.v4l2_ctrl_bank = ov8858_v4l2_ctrls,

	.csi_fmts_items = ARRAY_SIZE(ov8858_crl_csi_data_fmt),
	.csi_fmts = ov8858_crl_csi_data_fmt,

	.flip_items = ARRAY_SIZE(ov8858_flip_configurations),
	.flip_data = ov8858_flip_configurations,

	.crl_nvm_info.nvm_flags = CRL_NVM_ADDR_MODE_16BIT,
	.crl_nvm_info.nvm_preop_regs_items =
		ARRAY_SIZE(ov8858_nvm_preop_regset),
	.crl_nvm_info.nvm_preop_regs = ov8858_nvm_preop_regset,
	.crl_nvm_info.nvm_postop_regs_items =
		ARRAY_SIZE(ov8858_nvm_postop_regset),
	.crl_nvm_info.nvm_postop_regs = ov8858_nvm_postop_regset,
	.crl_nvm_info.nvm_blobs_items = ARRAY_SIZE(ov8858_nvm_blobs),
	.crl_nvm_info.nvm_config = ov8858_nvm_blobs,

	.frame_desc_entries = ARRAY_SIZE(ov8858_frame_desc),
	.frame_desc_type = CRL_V4L2_MBUS_FRAME_DESC_TYPE_CSI2,
	.frame_desc = ov8858_frame_desc,

	.msr_file_name = "00ov8858.bxt_rvp.drvb",
};

#endif  /* __CRLMODULE_OV8858_CONFIGURATION_H_ */
