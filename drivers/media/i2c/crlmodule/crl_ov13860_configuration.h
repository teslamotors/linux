/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2015 - 2018 Intel Corporation
 *
 * Author: Kamal Ramamoorthy <kamalanathan.ramamoorthy@intel.com>
 *
 */

#ifndef __CRLMODULE_OV13860_CONFIGURATION_H_
#define __CRLMODULE_OV13860_CONFIGURATION_H_

#include "crlmodule-sensor-ds.h"

static struct crl_register_write_rep ov13860_pll_600mbps[] = {
	{ 0x0300, CRL_REG_LEN_08BIT, 0x00 },/* pll1_pre_div = default*/
	{ 0x0301, CRL_REG_LEN_08BIT, 0x00 },/* pll1_multiplier Bit[8-9] = default */
	{ 0x0302, CRL_REG_LEN_08BIT, 0x32 },/* pll1_multiplier Bit[0-7] = default */
	{ 0x0303, CRL_REG_LEN_08BIT, 0x01 },/* pll1_divm = /(1 + 1) */
	{ 0x0304, CRL_REG_LEN_08BIT, 0x07 },/* pll1_div_mipi = default */
	{ 0x0305, CRL_REG_LEN_08BIT, 0x01 },/* pll1 pix clock div */
	{ 0x0306, CRL_REG_LEN_08BIT, 0x01 },/* pll1 sys clock div */
	{ 0x0308, CRL_REG_LEN_08BIT, 0x00 },/* pll1 bypass = default */
	{ 0x0309, CRL_REG_LEN_08BIT, 0x01 },/* pll1 cp = default */
	{ 0x030A, CRL_REG_LEN_08BIT, 0x00 },/* pll1 ctr = default */
	{ 0x030B, CRL_REG_LEN_08BIT, 0x00 },/* pll2_pre_div = default */
	{ 0x030c, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x030D, CRL_REG_LEN_08BIT, 0x28 },/* pll2_r_divp = default */
	{ 0x030E, CRL_REG_LEN_08BIT, 0x02 },/* pll2_r_divs = default */
	{ 0x030F, CRL_REG_LEN_08BIT, 0x07 },/* pll2_r_divsp = default */
	{ 0x0310, CRL_REG_LEN_08BIT, 0x01 },/* pll2_cp = default */
	{ 0x0311, CRL_REG_LEN_08BIT, 0x00 },/* pll2_cp = default */
	{ 0x0312, CRL_REG_LEN_08BIT, 0x03 },
	{ 0x0313, CRL_REG_LEN_08BIT, 0x03 },
	{ 0x031B, CRL_REG_LEN_08BIT, 0x00 },/* pll1 rst = default */
	{ 0x031C, CRL_REG_LEN_08BIT, 0x00 },/* pll2 rst = default */
	{ 0x031E, CRL_REG_LEN_08BIT, 0x01 },/* pll ctr::mipi_bitsel_man = 1 */
	{ 0x4837, CRL_REG_LEN_08BIT, 0x1a },/* pclk period */
};

static struct crl_register_write_rep ov13860_pll_1200mbps[] = {
	{ 0x0300, CRL_REG_LEN_08BIT, 0x00 },/* pll1_pre_div = default*/
	{ 0x0301, CRL_REG_LEN_08BIT, 0x00 },/* pll1_multiplier Bit[8-9] = default */
	{ 0x0302, CRL_REG_LEN_08BIT, 0x32 },/* pll1_multiplier Bit[0-7] = default */
	{ 0x0303, CRL_REG_LEN_08BIT, 0x00 },/* pll1_divm = /(1 + 0) */
	{ 0x0304, CRL_REG_LEN_08BIT, 0x07 },/* pll1_div_mipi = default */
	{ 0x0305, CRL_REG_LEN_08BIT, 0x01 },/* pll1 pix clock div */
	{ 0x0306, CRL_REG_LEN_08BIT, 0x01 },/* pll1 sys clock div */
	{ 0x0308, CRL_REG_LEN_08BIT, 0x00 },/* pll1 bypass = default */
	{ 0x0309, CRL_REG_LEN_08BIT, 0x01 },/* pll1 cp = default */
	{ 0x030A, CRL_REG_LEN_08BIT, 0x00 },/* pll1 ctr = default */
	{ 0x030B, CRL_REG_LEN_08BIT, 0x00 },/* pll2_pre_div = default */
	{ 0x030c, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x030D, CRL_REG_LEN_08BIT, 0x28 },/* pll2_r_divp = default */
	{ 0x030E, CRL_REG_LEN_08BIT, 0x02 },/* pll2_r_divs = default */
	{ 0x030F, CRL_REG_LEN_08BIT, 0x07 },/* pll2_r_divsp = default */
	{ 0x0310, CRL_REG_LEN_08BIT, 0x01 },/* pll2_cp = default */
	{ 0x0311, CRL_REG_LEN_08BIT, 0x00 },/* pll2_cp = default */
	{ 0x0312, CRL_REG_LEN_08BIT, 0x03 },
	{ 0x0313, CRL_REG_LEN_08BIT, 0x03 },
	{ 0x031B, CRL_REG_LEN_08BIT, 0x00 },/* pll1 rst = default */
	{ 0x031C, CRL_REG_LEN_08BIT, 0x00 },/* pll2 rst = default */
	{ 0x031E, CRL_REG_LEN_08BIT, 0x01 },/* pll ctr::mipi_bitsel_man = 1 */
	{ 0x4837, CRL_REG_LEN_08BIT, 0x0d },/* pclk period */
};

static struct crl_register_write_rep ov13860_powerup_regset[] = {
	{ 0x3010, CRL_REG_LEN_08BIT, 0x01 }, /* MIPI PHY1 = 1 */

	/*
	 * MIPI sc ctrl = 1
	 * Bit [7:4] num lane
	 * Bit [0] phy pad enable
	*/
	{ 0x3012, CRL_REG_LEN_08BIT, 0x21 },

	{ 0x340C, CRL_REG_LEN_08BIT, 0xff },
	{ 0x340D, CRL_REG_LEN_08BIT, 0xff },

	/*
	 * R Manual
	 * Bit 0:aec_manual, Bit 1:acg_manual, Bit 2 vts manual
	 * Bit 4:delay option, Bit 5:gain delay option
	 */
	{ 0x3503, CRL_REG_LEN_08BIT, 0x00 },

	{ 0x3507, CRL_REG_LEN_08BIT, 0x00 },/* MEC Median Exposure Bit[15:8] */
	{ 0x3508, CRL_REG_LEN_08BIT, 0x00 },/* MEC Median Exposure Bit[7:8] */

	{ 0x3509, CRL_REG_LEN_08BIT, 0x12 },/* R CTRL9 = default */

	{ 0x350A, CRL_REG_LEN_08BIT, 0x00 },/* MEC Long gain [10:8] */
	{ 0x350B, CRL_REG_LEN_08BIT, 0xff },/* MEC Long gain [7:0] */

	{ 0x350F, CRL_REG_LEN_08BIT, 0x10 },/* Median gain [7:0] */

	{ 0x3541, CRL_REG_LEN_08BIT, 0x02 },/* MEC Short exposure [15:8] */
	{ 0x3542, CRL_REG_LEN_08BIT, 0x00 },/* Median gain [7:0] */
	{ 0x3543, CRL_REG_LEN_08BIT, 0x00 },/* Magic */

	/*
	 * HDR related setting
	 */
	{ 0x3547, CRL_REG_LEN_08BIT, 0x00 },/* Very short exposure */
	{ 0x3548, CRL_REG_LEN_08BIT, 0x00 },/* Very short exposure */
	{ 0x3549, CRL_REG_LEN_08BIT, 0x12 },/* Magic */
	{ 0x354B, CRL_REG_LEN_08BIT, 0x10 },/* MEC short gain [7:0] */
	{ 0x354F, CRL_REG_LEN_08BIT, 0x10 },/* MEC very short gain [7:0] */

	/* Analog setting control */
	{ 0x3600, CRL_REG_LEN_08BIT, 0x41 },
	{ 0x3601, CRL_REG_LEN_08BIT, 0xd4 },
	{ 0x3603, CRL_REG_LEN_08BIT, 0x97 },
	{ 0x3604, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x360A, CRL_REG_LEN_08BIT, 0x35 },
	{ 0x360C, CRL_REG_LEN_08BIT, 0xA0 },
	{ 0x360D, CRL_REG_LEN_08BIT, 0x53 },
	{ 0x3618, CRL_REG_LEN_08BIT, 0x0C },
	{ 0x3620, CRL_REG_LEN_08BIT, 0x55 },
	{ 0x3622, CRL_REG_LEN_08BIT, 0x8C },
	{ 0x3623, CRL_REG_LEN_08BIT, 0x30 },
	{ 0x3628, CRL_REG_LEN_08BIT, 0xC0 },
	{ 0x3660, CRL_REG_LEN_08BIT, 0xC0 },
	{ 0x3662, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3663, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3664, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x366B, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3701, CRL_REG_LEN_08BIT, 0x20 },
	{ 0x3702, CRL_REG_LEN_08BIT, 0x30 },
	{ 0x3703, CRL_REG_LEN_08BIT, 0x3B },
	{ 0x3704, CRL_REG_LEN_08BIT, 0x26 },
	{ 0x3705, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x3706, CRL_REG_LEN_08BIT, 0x3F },
	{ 0x3708, CRL_REG_LEN_08BIT, 0x3C },
	{ 0x3709, CRL_REG_LEN_08BIT, 0x18 },
	{ 0x370E, CRL_REG_LEN_08BIT, 0x32 },
	{ 0x3710, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x3712, CRL_REG_LEN_08BIT, 0x12 },
	{ 0x3714, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3717, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3719, CRL_REG_LEN_08BIT, 0x03 },
	{ 0x371E, CRL_REG_LEN_08BIT, 0x31 },
	{ 0x371F, CRL_REG_LEN_08BIT, 0x7F },
	{ 0x3720, CRL_REG_LEN_08BIT, 0x18 },
	{ 0x3721, CRL_REG_LEN_08BIT, 0x0A },
	{ 0x3726, CRL_REG_LEN_08BIT, 0x22 },
	{ 0x3727, CRL_REG_LEN_08BIT, 0x44 },
	{ 0x3728, CRL_REG_LEN_08BIT, 0x40 },
	{ 0x3729, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x372A, CRL_REG_LEN_08BIT, 0x20 },
	{ 0x372B, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x372E, CRL_REG_LEN_08BIT, 0x2B },
	{ 0x3730, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3731, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3732, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3733, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3734, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3735, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3736, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3737, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3744, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x3745, CRL_REG_LEN_08BIT, 0x5E },
	{ 0x3746, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x3747, CRL_REG_LEN_08BIT, 0x1F },
	{ 0x3748, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x374A, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3760, CRL_REG_LEN_08BIT, 0xD1 },
	{ 0x3761, CRL_REG_LEN_08BIT, 0x31 },
	{ 0x3762, CRL_REG_LEN_08BIT, 0x53 },
	{ 0x3763, CRL_REG_LEN_08BIT, 0x14 },
	{ 0x3767, CRL_REG_LEN_08BIT, 0x24 },
	{ 0x3768, CRL_REG_LEN_08BIT, 0x0C },
	{ 0x3769, CRL_REG_LEN_08BIT, 0x24 },
	{ 0x376C, CRL_REG_LEN_08BIT, 0x43 },
	{ 0x376D, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x376E, CRL_REG_LEN_08BIT, 0x53 },
	{ 0x378C, CRL_REG_LEN_08BIT, 0x1F },
	{ 0x378D, CRL_REG_LEN_08BIT, 0x13 },
	{ 0x378F, CRL_REG_LEN_08BIT, 0x88 },
	{ 0x3790, CRL_REG_LEN_08BIT, 0x5A },
	{ 0x3791, CRL_REG_LEN_08BIT, 0x5A },
	{ 0x3792, CRL_REG_LEN_08BIT, 0x21 },
	{ 0x3794, CRL_REG_LEN_08BIT, 0x71 },
	{ 0x3796, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x379F, CRL_REG_LEN_08BIT, 0x3E },
	{ 0x37A0, CRL_REG_LEN_08BIT, 0x44 },
	{ 0x37A1, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x37A2, CRL_REG_LEN_08BIT, 0x44 },
	{ 0x37A3, CRL_REG_LEN_08BIT, 0x41 },
	{ 0x37A4, CRL_REG_LEN_08BIT, 0x88 },
	{ 0x37A5, CRL_REG_LEN_08BIT, 0xA9 },
	{ 0x37B3, CRL_REG_LEN_08BIT, 0xDC },
	{ 0x37B4, CRL_REG_LEN_08BIT, 0x0E },
	{ 0x37B7, CRL_REG_LEN_08BIT, 0x84 },
	{ 0x37B9, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x3821, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x382A, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x382F, CRL_REG_LEN_08BIT, 0x84 },
	{ 0x3835, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x3837, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x383C, CRL_REG_LEN_08BIT, 0x88 },
	{ 0x383D, CRL_REG_LEN_08BIT, 0xFF },
	{ 0x3845, CRL_REG_LEN_08BIT, 0x10 },

	{ 0x3D85, CRL_REG_LEN_08BIT, 0x16 },/* OTP_REGS */
	{ 0x3D8C, CRL_REG_LEN_08BIT, 0x79 },/* OTP_REGS */
	{ 0x3D8D, CRL_REG_LEN_08BIT, 0x7F },/* OTP_REGS */

	{ 0x4000, CRL_REG_LEN_08BIT, 0x17 },/* BLC_00 */

	/*
	 * Magic Registers
	 */
	{ 0x400F, CRL_REG_LEN_08BIT, 0x80 },
	{ 0x4011, CRL_REG_LEN_08BIT, 0xFB },
	{ 0x4017, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x401A, CRL_REG_LEN_08BIT, 0x0E },
	{ 0x4020, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x4022, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x4024, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x4026, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x4028, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x402A, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x402C, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x402E, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x4030, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x4032, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x4034, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x4036, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x4038, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x403A, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x403C, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x403E, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x4052, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4053, CRL_REG_LEN_08BIT, 0x80 },
	{ 0x4054, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4055, CRL_REG_LEN_08BIT, 0x80 },
	{ 0x4056, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4057, CRL_REG_LEN_08BIT, 0x80 },
	{ 0x4058, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4059, CRL_REG_LEN_08BIT, 0x80 },
	{ 0x4202, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4203, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4d00, CRL_REG_LEN_08BIT, 0x05 },
	{ 0x4d01, CRL_REG_LEN_08BIT, 0x05 },
	{ 0x4d02, CRL_REG_LEN_08BIT, 0xCA },
	{ 0x4d03, CRL_REG_LEN_08BIT, 0xD7 },
	{ 0x4d04, CRL_REG_LEN_08BIT, 0xAE },
	{ 0x4d05, CRL_REG_LEN_08BIT, 0x13 },
	{ 0x4813, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x4815, CRL_REG_LEN_08BIT, 0x40 },
	{ 0x4837, CRL_REG_LEN_08BIT, 0x0D },
	{ 0x486E, CRL_REG_LEN_08BIT, 0x03 },
	{ 0x4B01, CRL_REG_LEN_08BIT, 0x80 },
	{ 0x4B06, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4C01, CRL_REG_LEN_08BIT, 0xDF },

	/*
	 * DSP control related registers required for RAW
	 * Sensor path
	 */
	{ 0x5001, CRL_REG_LEN_08BIT, 0x40 },
	{ 0x5002, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x5003, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x5004, CRL_REG_LEN_08BIT, 0x80 },
	{ 0x5005, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x501D, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x501F, CRL_REG_LEN_08BIT, 0x06 },
	{ 0x5021, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x5022, CRL_REG_LEN_08BIT, 0x13 },
	{ 0x5058, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x5200, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x5209, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x520A, CRL_REG_LEN_08BIT, 0x80 },
	{ 0x520B, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x520C, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x520E, CRL_REG_LEN_08BIT, 0x34 },
	{ 0x5210, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x5211, CRL_REG_LEN_08BIT, 0xA0 },
	{ 0x5280, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x5292, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x5C80, CRL_REG_LEN_08BIT, 0x05 },
	{ 0x5C81, CRL_REG_LEN_08BIT, 0x90 },
	{ 0x5C82, CRL_REG_LEN_08BIT, 0x09 },
	{ 0x5C83, CRL_REG_LEN_08BIT, 0x5F },
	{ 0x5D00, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4001, CRL_REG_LEN_08BIT, 0x60 }, /* BLC control */

	/*
	 * Magic Registers
	 */
	{ 0x560F, CRL_REG_LEN_08BIT, 0xFC },
	{ 0x5610, CRL_REG_LEN_08BIT, 0xF0 },
	{ 0x5611, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x562F, CRL_REG_LEN_08BIT, 0xFC },
	{ 0x5630, CRL_REG_LEN_08BIT, 0xF0 },
	{ 0x5631, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x564F, CRL_REG_LEN_08BIT, 0xFC },
	{ 0x5650, CRL_REG_LEN_08BIT, 0xF0 },
	{ 0x5651, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x566F, CRL_REG_LEN_08BIT, 0xFC },
	{ 0x5670, CRL_REG_LEN_08BIT, 0xF0 },
	{ 0x5671, CRL_REG_LEN_08BIT, 0x10 },
};

static struct crl_register_write_rep ov13860_mode_13m[] = {

	/*
	 * Exposure & Gain
	 */
	{ 0x3501, CRL_REG_LEN_08BIT, 0x0D },/* Long Exposure */
	{ 0x3502, CRL_REG_LEN_08BIT, 0x88 },/* Long Exposure */

	{ 0x370A, CRL_REG_LEN_08BIT, 0x23 },
	{ 0x372F, CRL_REG_LEN_08BIT, 0xA0 },

	/*
	 * Windowing
	 */
	{ 0x3800, CRL_REG_LEN_08BIT, 0x00 },/* h_crop_start high */
	{ 0x3801, CRL_REG_LEN_08BIT, 0x14 },/* h_crop_start low */
	{ 0x3802, CRL_REG_LEN_08BIT, 0x00 },/* v_crop_start high */
	{ 0x3803, CRL_REG_LEN_08BIT, 0x0C },/* v_crop_start low */
	{ 0x3804, CRL_REG_LEN_08BIT, 0x10 },/* h_crop_end high */
	{ 0x3805, CRL_REG_LEN_08BIT, 0x8B },/* h_crop_end low */
	{ 0x3806, CRL_REG_LEN_08BIT, 0x0C },/* v_crop_end high */
	{ 0x3807, CRL_REG_LEN_08BIT, 0x43 },/* v_crop_end low */
	{ 0x3808, CRL_REG_LEN_08BIT, 0x10 },/* h_output_size high 4208 x 3120 */
	{ 0x3809, CRL_REG_LEN_08BIT, 0x70 },/* h_output_size low */
	{ 0x380A, CRL_REG_LEN_08BIT, 0x0C },/* v_output_size high */
	{ 0x380B, CRL_REG_LEN_08BIT, 0x30 },/* v_output_size low */
	{ 0x3810, CRL_REG_LEN_08BIT, 0x00 },/* Manual horizontal window offset high */
	{ 0x3811, CRL_REG_LEN_08BIT, 0x04 },/* Manual horizontal window offset low */
	{ 0x3813, CRL_REG_LEN_08BIT, 0x04 },/* Manual vertical window offset low */
	{ 0x3814, CRL_REG_LEN_08BIT, 0x11 },/* Horizontal sub-sample odd inc */
	{ 0x3815, CRL_REG_LEN_08BIT, 0x11 },/* Vertical sub-sample odd inc */
	{ 0x383D, CRL_REG_LEN_08BIT, 0xFF },/* Vertical sub-sample odd inc */

	{ 0x3820, CRL_REG_LEN_08BIT, 0x00 },/* Binning */
	{ 0x3842, CRL_REG_LEN_08BIT, 0x00 },/* Binning */
	{ 0x5000, CRL_REG_LEN_08BIT, 0x99 },/* Binning */

	{ 0x3836, CRL_REG_LEN_08BIT, 0x0C }, /* ablc_use_num */
	{ 0x383C, CRL_REG_LEN_08BIT, 0x88 }, /* Boundary Pix num */

	{ 0x4008, CRL_REG_LEN_08BIT, 0x00 }, /* Magic */
	{ 0x4009, CRL_REG_LEN_08BIT, 0x13 }, /* Magic */
	{ 0x4019, CRL_REG_LEN_08BIT, 0x18 }, /* Magic */
	{ 0x4051, CRL_REG_LEN_08BIT, 0x03 }, /* Magic */
	{ 0x4066, CRL_REG_LEN_08BIT, 0x04 }, /* Magic */

	{ 0x5201, CRL_REG_LEN_08BIT, 0x80 }, /* Magic */
	{ 0x5204, CRL_REG_LEN_08BIT, 0x01 }, /* Magic */
	{ 0x5205, CRL_REG_LEN_08BIT, 0x00 }, /* Magic */
};

static struct crl_register_write_rep ov13860_mode_8m[] = {

	/*
	 * Exposure & Gain
	 */
	{ 0x3501, CRL_REG_LEN_08BIT, 0x0D },/* Long Exposure */
	{ 0x3502, CRL_REG_LEN_08BIT, 0x88 },/* Long Exposure */

	{ 0x370A, CRL_REG_LEN_08BIT, 0x23 },
	{ 0x372F, CRL_REG_LEN_08BIT, 0xA0 },

	/*
	 * Windowing
	 */
	{ 0x3800, CRL_REG_LEN_08BIT, 0x00 },/* h_crop_start high */
	{ 0x3801, CRL_REG_LEN_08BIT, 0x14 },/* h_crop_start low */
	{ 0x3802, CRL_REG_LEN_08BIT, 0x00 },/* v_crop_start high */
	{ 0x3803, CRL_REG_LEN_08BIT, 0x0C },/* v_crop_start low */
	{ 0x3804, CRL_REG_LEN_08BIT, 0x10 },/* h_crop_end high */
	{ 0x3805, CRL_REG_LEN_08BIT, 0x8B },/* h_crop_end low */
	{ 0x3806, CRL_REG_LEN_08BIT, 0x0C },/* v_crop_end high */
	{ 0x3807, CRL_REG_LEN_08BIT, 0x43 },/* v_crop_end low */
	{ 0x3808, CRL_REG_LEN_08BIT, 0x0C },/* h_output_size high 3264 x 2448 */
	{ 0x3809, CRL_REG_LEN_08BIT, 0xC0 },/* h_output_size low */
	{ 0x380A, CRL_REG_LEN_08BIT, 0x09 },/* v_output_size high */
	{ 0x380B, CRL_REG_LEN_08BIT, 0x90 },/* v_output_size low */
	{ 0x3810, CRL_REG_LEN_08BIT, 0x00 },/* Manual horizontal window offset high */
	{ 0x3811, CRL_REG_LEN_08BIT, 0x04 },/* Manual horizontal window offset low */
	{ 0x3813, CRL_REG_LEN_08BIT, 0x04 },/* Manual vertical window offset low */
	{ 0x3814, CRL_REG_LEN_08BIT, 0x11 },/* Horizontal sub-sample odd inc */
	{ 0x3815, CRL_REG_LEN_08BIT, 0x11 },/* Vertical sub-sample odd inc */
	{ 0x383D, CRL_REG_LEN_08BIT, 0xFF },/* Vertical sub-sample odd inc */

	{ 0x3820, CRL_REG_LEN_08BIT, 0x00 },/* Binning */
	{ 0x3842, CRL_REG_LEN_08BIT, 0x00 },/* Binning */
	{ 0x5000, CRL_REG_LEN_08BIT, 0x99 },/* Binning */

	{ 0x3836, CRL_REG_LEN_08BIT, 0x0C }, /* ablc_use_num */
	{ 0x383C, CRL_REG_LEN_08BIT, 0x88 }, /* Boundary Pix num */

	{ 0x4008, CRL_REG_LEN_08BIT, 0x00 }, /* Magic */
	{ 0x4009, CRL_REG_LEN_08BIT, 0x13 }, /* Magic */
	{ 0x4019, CRL_REG_LEN_08BIT, 0x18 }, /* Magic */
	{ 0x4051, CRL_REG_LEN_08BIT, 0x03 }, /* Magic */
	{ 0x4066, CRL_REG_LEN_08BIT, 0x04 }, /* Magic */

	{ 0x5201, CRL_REG_LEN_08BIT, 0x80 }, /* Magic */
	{ 0x5204, CRL_REG_LEN_08BIT, 0x01 }, /* Magic */
	{ 0x5205, CRL_REG_LEN_08BIT, 0x00 }, /* Magic */
};

static struct crl_register_write_rep ov13860_mode_4k2k[] = {

	/*
	 * Exposure & Gain
	 */
	{ 0x3501, CRL_REG_LEN_08BIT, 0x0D },/* Long Exposure */
	{ 0x3502, CRL_REG_LEN_08BIT, 0x88 },/* Long Exposure */

	{ 0x370A, CRL_REG_LEN_08BIT, 0x23 },
	{ 0x372F, CRL_REG_LEN_08BIT, 0xA0 },

	/*
	 * Windowing
	 */
	{ 0x3800, CRL_REG_LEN_08BIT, 0x00 },/* h_crop_start high */
	{ 0x3801, CRL_REG_LEN_08BIT, 0x14 },/* h_crop_start low */
	{ 0x3802, CRL_REG_LEN_08BIT, 0x00 },/* v_crop_start high */
	{ 0x3803, CRL_REG_LEN_08BIT, 0x0C },/* v_crop_start low */
	{ 0x3804, CRL_REG_LEN_08BIT, 0x10 },/* h_crop_end high */
	{ 0x3805, CRL_REG_LEN_08BIT, 0x8B },/* h_crop_end low */
	{ 0x3806, CRL_REG_LEN_08BIT, 0x0C },/* v_crop_end high */
	{ 0x3807, CRL_REG_LEN_08BIT, 0x43 },/* v_crop_end low */
	{ 0x3808, CRL_REG_LEN_08BIT, 0x10 },/* h_output_size high 4096 x 2160 */
	{ 0x3809, CRL_REG_LEN_08BIT, 0x00 },/* h_output_size low */
	{ 0x380A, CRL_REG_LEN_08BIT, 0x08 },/* v_output_size high */
	{ 0x380B, CRL_REG_LEN_08BIT, 0x70 },/* v_output_size low */
	{ 0x3810, CRL_REG_LEN_08BIT, 0x00 },/* Manual horizontal window offset high */
	{ 0x3811, CRL_REG_LEN_08BIT, 0x04 },/* Manual horizontal window offset low */
	{ 0x3813, CRL_REG_LEN_08BIT, 0x04 },/* Manual vertical window offset low */
	{ 0x3814, CRL_REG_LEN_08BIT, 0x11 },/* Horizontal sub-sample odd inc */
	{ 0x3815, CRL_REG_LEN_08BIT, 0x11 },/* Vertical sub-sample odd inc */
	{ 0x383D, CRL_REG_LEN_08BIT, 0xFF },/* Vertical sub-sample odd inc */

	{ 0x3820, CRL_REG_LEN_08BIT, 0x00 },/* Binning */
	{ 0x3842, CRL_REG_LEN_08BIT, 0x00 },/* Binning */
	{ 0x5000, CRL_REG_LEN_08BIT, 0x99 },/* Binning */

	{ 0x3836, CRL_REG_LEN_08BIT, 0x0C }, /* ablc_use_num */
	{ 0x383C, CRL_REG_LEN_08BIT, 0x88 }, /* Boundary Pix num */

	{ 0x4008, CRL_REG_LEN_08BIT, 0x00 }, /* Magic */
	{ 0x4009, CRL_REG_LEN_08BIT, 0x13 }, /* Magic */
	{ 0x4019, CRL_REG_LEN_08BIT, 0x18 }, /* Magic */
	{ 0x4051, CRL_REG_LEN_08BIT, 0x03 }, /* Magic */
	{ 0x4066, CRL_REG_LEN_08BIT, 0x04 }, /* Magic */

	{ 0x5201, CRL_REG_LEN_08BIT, 0x80 }, /* Magic */
	{ 0x5204, CRL_REG_LEN_08BIT, 0x01 }, /* Magic */
	{ 0x5205, CRL_REG_LEN_08BIT, 0x00 }, /* Magic */
};

static struct crl_register_write_rep ov13860_mode_uhd[] = {

	/*
	 * Exposure & Gain
	 */
	{ 0x3501, CRL_REG_LEN_08BIT, 0x0D },/* Long Exposure */
	{ 0x3502, CRL_REG_LEN_08BIT, 0x88 },/* Long Exposure */

	{ 0x370A, CRL_REG_LEN_08BIT, 0x23 },
	{ 0x372F, CRL_REG_LEN_08BIT, 0xA0 },

	/*
	 * Windowing
	 */
	{ 0x3800, CRL_REG_LEN_08BIT, 0x00 },/* h_crop_start high */
	{ 0x3801, CRL_REG_LEN_08BIT, 0x14 },/* h_crop_start low */
	{ 0x3802, CRL_REG_LEN_08BIT, 0x00 },/* v_crop_start high */
	{ 0x3803, CRL_REG_LEN_08BIT, 0x0C },/* v_crop_start low */
	{ 0x3804, CRL_REG_LEN_08BIT, 0x10 },/* h_crop_end high */
	{ 0x3805, CRL_REG_LEN_08BIT, 0x8B },/* h_crop_end low */
	{ 0x3806, CRL_REG_LEN_08BIT, 0x0C },/* v_crop_end high */
	{ 0x3807, CRL_REG_LEN_08BIT, 0x43 },/* v_crop_end low */
	{ 0x3808, CRL_REG_LEN_08BIT, 0x0F },/* h_output_size high 3840 x 2160 */
	{ 0x3809, CRL_REG_LEN_08BIT, 0x00 },/* h_output_size low */
	{ 0x380A, CRL_REG_LEN_08BIT, 0x08 },/* v_output_size high */
	{ 0x380B, CRL_REG_LEN_08BIT, 0x70 },/* v_output_size low */
	{ 0x3810, CRL_REG_LEN_08BIT, 0x00 },/* Manual horizontal window offset high */
	{ 0x3811, CRL_REG_LEN_08BIT, 0x04 },/* Manual horizontal window offset low */
	{ 0x3813, CRL_REG_LEN_08BIT, 0x04 },/* Manual vertical window offset low */
	{ 0x3814, CRL_REG_LEN_08BIT, 0x11 },/* Horizontal sub-sample odd inc */
	{ 0x3815, CRL_REG_LEN_08BIT, 0x11 },/* Vertical sub-sample odd inc */
	{ 0x383D, CRL_REG_LEN_08BIT, 0xFF },/* Vertical sub-sample odd inc */

	{ 0x3820, CRL_REG_LEN_08BIT, 0x00 },/* Binning */
	{ 0x3842, CRL_REG_LEN_08BIT, 0x00 },/* Binning */
	{ 0x5000, CRL_REG_LEN_08BIT, 0x99 },/* Binning */

	{ 0x3836, CRL_REG_LEN_08BIT, 0x0C }, /* ablc_use_num */
	{ 0x383C, CRL_REG_LEN_08BIT, 0x88 }, /* Boundary Pix num */

	{ 0x4008, CRL_REG_LEN_08BIT, 0x00 }, /* Magic */
	{ 0x4009, CRL_REG_LEN_08BIT, 0x13 }, /* Magic */
	{ 0x4019, CRL_REG_LEN_08BIT, 0x18 }, /* Magic */
	{ 0x4051, CRL_REG_LEN_08BIT, 0x03 }, /* Magic */
	{ 0x4066, CRL_REG_LEN_08BIT, 0x04 }, /* Magic */

	{ 0x5201, CRL_REG_LEN_08BIT, 0x80 }, /* Magic */
	{ 0x5204, CRL_REG_LEN_08BIT, 0x01 }, /* Magic */
	{ 0x5205, CRL_REG_LEN_08BIT, 0x00 }, /* Magic */
};

static struct crl_register_write_rep ov13860_mode_6m[] = {

	/*
	 * Exposure & Gain
	 */
	{ 0x3501, CRL_REG_LEN_08BIT, 0x0D },/* Long Exposure */
	{ 0x3502, CRL_REG_LEN_08BIT, 0x88 },/* Long Exposure */

	{ 0x370A, CRL_REG_LEN_08BIT, 0x23 },
	{ 0x372F, CRL_REG_LEN_08BIT, 0xA0 },

	/*
	 * Windowing
	 */
	{ 0x3800, CRL_REG_LEN_08BIT, 0x00 },/* h_crop_start high */
	{ 0x3801, CRL_REG_LEN_08BIT, 0x14 },/* h_crop_start low */
	{ 0x3802, CRL_REG_LEN_08BIT, 0x00 },/* v_crop_start high */
	{ 0x3803, CRL_REG_LEN_08BIT, 0x0C },/* v_crop_start low */
	{ 0x3804, CRL_REG_LEN_08BIT, 0x10 },/* h_crop_end high */
	{ 0x3805, CRL_REG_LEN_08BIT, 0x8B },/* h_crop_end low */
	{ 0x3806, CRL_REG_LEN_08BIT, 0x0C },/* v_crop_end high */
	{ 0x3807, CRL_REG_LEN_08BIT, 0x43 },/* v_crop_end low */
	{ 0x3808, CRL_REG_LEN_08BIT, 0x0C },/* h_output_size high 3264 x 1836 */
	{ 0x3809, CRL_REG_LEN_08BIT, 0xC0 },/* h_output_size low */
	{ 0x380A, CRL_REG_LEN_08BIT, 0x07 },/* v_output_size high */
	{ 0x380B, CRL_REG_LEN_08BIT, 0x2C },/* v_output_size low */
	{ 0x3810, CRL_REG_LEN_08BIT, 0x00 },/* Manual horizontal window offset high */
	{ 0x3811, CRL_REG_LEN_08BIT, 0x04 },/* Manual horizontal window offset low */
	{ 0x3813, CRL_REG_LEN_08BIT, 0x04 },/* Manual vertical window offset low */
	{ 0x3814, CRL_REG_LEN_08BIT, 0x11 },/* Horizontal sub-sample odd inc */
	{ 0x3815, CRL_REG_LEN_08BIT, 0x11 },/* Vertical sub-sample odd inc */
	{ 0x383D, CRL_REG_LEN_08BIT, 0xFF },/* Vertical sub-sample odd inc */

	{ 0x3820, CRL_REG_LEN_08BIT, 0x00 },/* Binning */
	{ 0x3842, CRL_REG_LEN_08BIT, 0x00 },/* Binning */
	{ 0x5000, CRL_REG_LEN_08BIT, 0x99 },/* Binning */

	{ 0x3836, CRL_REG_LEN_08BIT, 0x0C }, /* ablc_use_num */
	{ 0x383C, CRL_REG_LEN_08BIT, 0x88 }, /* Boundary Pix num */

	{ 0x4008, CRL_REG_LEN_08BIT, 0x00 }, /* Magic */
	{ 0x4009, CRL_REG_LEN_08BIT, 0x13 }, /* Magic */
	{ 0x4019, CRL_REG_LEN_08BIT, 0x18 }, /* Magic */
	{ 0x4051, CRL_REG_LEN_08BIT, 0x03 }, /* Magic */
	{ 0x4066, CRL_REG_LEN_08BIT, 0x04 }, /* Magic */

	{ 0x5201, CRL_REG_LEN_08BIT, 0x80 }, /* Magic */
	{ 0x5204, CRL_REG_LEN_08BIT, 0x01 }, /* Magic */
	{ 0x5205, CRL_REG_LEN_08BIT, 0x00 }, /* Magic */
};

static struct crl_register_write_rep ov13860_mode_3m[] = {

	/*
	 * Exposure & Gain
	 */
	{ 0x3501, CRL_REG_LEN_08BIT, 0x06 },/* Long Exposure */
	{ 0x3502, CRL_REG_LEN_08BIT, 0xB8 },/* Long Exposure */

	{ 0x370A, CRL_REG_LEN_08BIT, 0x63 },
	{ 0x372F, CRL_REG_LEN_08BIT, 0x90 },

	/*
	 * Windowing
	 */
	{ 0x3800, CRL_REG_LEN_08BIT, 0x00 },/* h_crop_start high */
	{ 0x3801, CRL_REG_LEN_08BIT, 0x14 },/* h_crop_start low */
	{ 0x3802, CRL_REG_LEN_08BIT, 0x00 },/* v_crop_start high */
	{ 0x3803, CRL_REG_LEN_08BIT, 0x0C },/* v_crop_start low */
	{ 0x3804, CRL_REG_LEN_08BIT, 0x10 },/* h_crop_end high */
	{ 0x3805, CRL_REG_LEN_08BIT, 0x8B },/* h_crop_end low */
	{ 0x3806, CRL_REG_LEN_08BIT, 0x0C },/* v_crop_end high */
	{ 0x3807, CRL_REG_LEN_08BIT, 0x43 },/* v_crop_end low */
	{ 0x3808, CRL_REG_LEN_08BIT, 0x08 },/* h_output_size high 2048 x 1536 */
	{ 0x3809, CRL_REG_LEN_08BIT, 0x00 },/* h_output_size low */
	{ 0x380A, CRL_REG_LEN_08BIT, 0x06 },/* v_output_size high */
	{ 0x380B, CRL_REG_LEN_08BIT, 0x00 },/* v_output_size low */
	{ 0x3810, CRL_REG_LEN_08BIT, 0x00 },/* Manual horizontal window offset high */
	{ 0x3811, CRL_REG_LEN_08BIT, 0x04 },/* Manual horizontal window offset low */
	{ 0x3813, CRL_REG_LEN_08BIT, 0x04 },/* Manual vertical window offset low */
	{ 0x3814, CRL_REG_LEN_08BIT, 0x11 },/* Horizontal sub-sample odd inc */
	{ 0x3815, CRL_REG_LEN_08BIT, 0x31 },/* Vertical sub-sample odd inc */
	{ 0x383D, CRL_REG_LEN_08BIT, 0xFF },/* Vertical sub-sample odd inc */

	{ 0x3820, CRL_REG_LEN_08BIT, 0x02 },/* Binning */
	{ 0x3842, CRL_REG_LEN_08BIT, 0x40 },/* Binning */
	{ 0x5000, CRL_REG_LEN_08BIT, 0xD9 },/* Binning */

	{ 0x3836, CRL_REG_LEN_08BIT, 0x0C }, /* ablc_use_num */
	{ 0x383C, CRL_REG_LEN_08BIT, 0x48 }, /* Boundary Pix num */

	{ 0x4008, CRL_REG_LEN_08BIT, 0x02 }, /* Magic */
	{ 0x4009, CRL_REG_LEN_08BIT, 0x09 }, /* Magic */
	{ 0x4019, CRL_REG_LEN_08BIT, 0x0C }, /* Magic */
	{ 0x4051, CRL_REG_LEN_08BIT, 0x01 }, /* Magic */
	{ 0x4066, CRL_REG_LEN_08BIT, 0x02 }, /* Magic */

	{ 0x5201, CRL_REG_LEN_08BIT, 0x71 }, /* Magic */
	{ 0x5204, CRL_REG_LEN_08BIT, 0x00 }, /* Magic */
	{ 0x5205, CRL_REG_LEN_08BIT, 0x80 }, /* Magic */
};

static struct crl_register_write_rep ov13860_mode_1952_1088[] = {

	/*
	 * Exposure & Gain
	 */
	{ 0x3501, CRL_REG_LEN_08BIT, 0x06 },/* Long Exposure */
	{ 0x3502, CRL_REG_LEN_08BIT, 0xB8 },/* Long Exposure */

	{ 0x370A, CRL_REG_LEN_08BIT, 0x63 },
	{ 0x372F, CRL_REG_LEN_08BIT, 0x90 },

	/*
	 * Windowing
	 */
	{ 0x3800, CRL_REG_LEN_08BIT, 0x00 },/* h_crop_start high */
	{ 0x3801, CRL_REG_LEN_08BIT, 0x14 },/* h_crop_start low */
	{ 0x3802, CRL_REG_LEN_08BIT, 0x00 },/* v_crop_start high */
	{ 0x3803, CRL_REG_LEN_08BIT, 0x0C },/* v_crop_start low */
	{ 0x3804, CRL_REG_LEN_08BIT, 0x10 },/* h_crop_end high */
	{ 0x3805, CRL_REG_LEN_08BIT, 0x8B },/* h_crop_end low */
	{ 0x3806, CRL_REG_LEN_08BIT, 0x0C },/* v_crop_end high */
	{ 0x3807, CRL_REG_LEN_08BIT, 0x43 },/* v_crop_end low */
	{ 0x3808, CRL_REG_LEN_08BIT, 0x07 },/* h_output_size high 1952 x 1088 */
	{ 0x3809, CRL_REG_LEN_08BIT, 0xA0 },/* h_output_size low */
	{ 0x380A, CRL_REG_LEN_08BIT, 0x04 },/* v_output_size high */
	{ 0x380B, CRL_REG_LEN_08BIT, 0x40 },/* v_output_size low */
	{ 0x3810, CRL_REG_LEN_08BIT, 0x00 },/* Manual horizontal window offset high */
	{ 0x3811, CRL_REG_LEN_08BIT, 0x04 },/* Manual horizontal window offset low */
	{ 0x3813, CRL_REG_LEN_08BIT, 0x04 },/* Manual vertical window offset low */
	{ 0x3814, CRL_REG_LEN_08BIT, 0x11 },/* Horizontal sub-sample odd inc */
	{ 0x3815, CRL_REG_LEN_08BIT, 0x31 },/* Vertical sub-sample odd inc */
	{ 0x383D, CRL_REG_LEN_08BIT, 0xFF },/* Vertical sub-sample odd inc */

	{ 0x3820, CRL_REG_LEN_08BIT, 0x02 },/* Binning */
	{ 0x3842, CRL_REG_LEN_08BIT, 0x40 },/* Binning */
	{ 0x5000, CRL_REG_LEN_08BIT, 0xD9 },/* Binning */

	{ 0x3836, CRL_REG_LEN_08BIT, 0x0C }, /* ablc_use_num */
	{ 0x383C, CRL_REG_LEN_08BIT, 0x48 }, /* Boundary Pix num */

	{ 0x4008, CRL_REG_LEN_08BIT, 0x02 }, /* Magic */
	{ 0x4009, CRL_REG_LEN_08BIT, 0x09 }, /* Magic */
	{ 0x4019, CRL_REG_LEN_08BIT, 0x0C }, /* Magic */
	{ 0x4051, CRL_REG_LEN_08BIT, 0x01 }, /* Magic */
	{ 0x4066, CRL_REG_LEN_08BIT, 0x02 }, /* Magic */

	{ 0x5201, CRL_REG_LEN_08BIT, 0x71 }, /* Magic */
	{ 0x5204, CRL_REG_LEN_08BIT, 0x00 }, /* Magic */
	{ 0x5205, CRL_REG_LEN_08BIT, 0x80 }, /* Magic */
};

static struct crl_register_write_rep ov13860_mode_720[] = {

	/*
	 * Exposure & Gain
	 */
	{ 0x3501, CRL_REG_LEN_08BIT, 0x03 },/* Long Exposure */
	{ 0x3502, CRL_REG_LEN_08BIT, 0x44 },/* Long Exposure */

	{ 0x370A, CRL_REG_LEN_08BIT, 0x63 },
	{ 0x372F, CRL_REG_LEN_08BIT, 0x90 },

	/*
	 * Windowing
	 */
	{ 0x3800, CRL_REG_LEN_08BIT, 0x00 },/* h_crop_start high */
	{ 0x3801, CRL_REG_LEN_08BIT, 0x14 },/* h_crop_start low */
	{ 0x3802, CRL_REG_LEN_08BIT, 0x00 },/* v_crop_start high */
	{ 0x3803, CRL_REG_LEN_08BIT, 0x0C },/* v_crop_start low */
	{ 0x3804, CRL_REG_LEN_08BIT, 0x10 },/* h_crop_end high */
	{ 0x3805, CRL_REG_LEN_08BIT, 0x8B },/* h_crop_end low */
	{ 0x3806, CRL_REG_LEN_08BIT, 0x0C },/* v_crop_end high */
	{ 0x3807, CRL_REG_LEN_08BIT, 0x43 },/* v_crop_end low */
	{ 0x3808, CRL_REG_LEN_08BIT, 0x05 },/* h_output_size high 1280 x 720 */
	{ 0x3809, CRL_REG_LEN_08BIT, 0x00 },/* h_output_size low */
	{ 0x380A, CRL_REG_LEN_08BIT, 0x02 },/* v_output_size high */
	{ 0x380B, CRL_REG_LEN_08BIT, 0xD0 },/* v_output_size low */
	{ 0x3810, CRL_REG_LEN_08BIT, 0x00 },/* Manual horizontal window offset high */
	{ 0x3811, CRL_REG_LEN_08BIT, 0x04 },/* Manual horizontal window offset low */
	{ 0x3813, CRL_REG_LEN_08BIT, 0x04 },/* Manual vertical window offset low */
	{ 0x3814, CRL_REG_LEN_08BIT, 0x11 },/* Horizontal sub-sample odd inc */
	{ 0x3815, CRL_REG_LEN_08BIT, 0x31 },/* Vertical sub-sample odd inc */
	{ 0x383D, CRL_REG_LEN_08BIT, 0xFF },/* Vertical sub-sample odd inc */

	{ 0x3820, CRL_REG_LEN_08BIT, 0x02 },/* Binning */
	{ 0x3842, CRL_REG_LEN_08BIT, 0x40 },/* Binning */
	{ 0x5000, CRL_REG_LEN_08BIT, 0xD9 },/* Binning */

	{ 0x3836, CRL_REG_LEN_08BIT, 0x0C }, /* ablc_use_num */
	{ 0x383C, CRL_REG_LEN_08BIT, 0x48 }, /* Boundary Pix num */

	{ 0x4008, CRL_REG_LEN_08BIT, 0x02 }, /* Magic */
	{ 0x4009, CRL_REG_LEN_08BIT, 0x09 }, /* Magic */
	{ 0x4019, CRL_REG_LEN_08BIT, 0x0C }, /* Magic */
	{ 0x4051, CRL_REG_LEN_08BIT, 0x01 }, /* Magic */
	{ 0x4066, CRL_REG_LEN_08BIT, 0x02 }, /* Magic */

	{ 0x5201, CRL_REG_LEN_08BIT, 0x71 }, /* Magic */
	{ 0x5204, CRL_REG_LEN_08BIT, 0x00 }, /* Magic */
	{ 0x5205, CRL_REG_LEN_08BIT, 0x80 }, /* Magic */
};

static struct crl_register_write_rep ov13860_mode_480[] = {

	/*
	 * Exposure & Gain
	 */
	{ 0x3501, CRL_REG_LEN_08BIT, 0x03 },/* Long Exposure */
	{ 0x3502, CRL_REG_LEN_08BIT, 0x44 },/* Long Exposure */

	{ 0x370A, CRL_REG_LEN_08BIT, 0x63 },
	{ 0x372F, CRL_REG_LEN_08BIT, 0x90 },

	/*
	 * Windowing
	 */
	{ 0x3800, CRL_REG_LEN_08BIT, 0x00 },/* h_crop_start high */
	{ 0x3801, CRL_REG_LEN_08BIT, 0x14 },/* h_crop_start low */
	{ 0x3802, CRL_REG_LEN_08BIT, 0x00 },/* v_crop_start high */
	{ 0x3803, CRL_REG_LEN_08BIT, 0x0C },/* v_crop_start low */
	{ 0x3804, CRL_REG_LEN_08BIT, 0x10 },/* h_crop_end high */
	{ 0x3805, CRL_REG_LEN_08BIT, 0x8B },/* h_crop_end low */
	{ 0x3806, CRL_REG_LEN_08BIT, 0x0C },/* v_crop_end high */
	{ 0x3807, CRL_REG_LEN_08BIT, 0x43 },/* v_crop_end low */
	{ 0x3808, CRL_REG_LEN_08BIT, 0x02 },/* h_output_size high 640 x 480 */
	{ 0x3809, CRL_REG_LEN_08BIT, 0x80 },/* h_output_size low */
	{ 0x380A, CRL_REG_LEN_08BIT, 0x01 },/* v_output_size high */
	{ 0x380B, CRL_REG_LEN_08BIT, 0xE0 },/* v_output_size low */
	{ 0x3810, CRL_REG_LEN_08BIT, 0x00 },/* Manual horizontal window offset high */
	{ 0x3811, CRL_REG_LEN_08BIT, 0x04 },/* Manual horizontal window offset low */
	{ 0x3813, CRL_REG_LEN_08BIT, 0x04 },/* Manual vertical window offset low */
	{ 0x3814, CRL_REG_LEN_08BIT, 0x11 },/* Horizontal sub-sample odd inc */
	{ 0x3815, CRL_REG_LEN_08BIT, 0x31 },/* Vertical sub-sample odd inc */
	{ 0x383D, CRL_REG_LEN_08BIT, 0xFF },/* Vertical sub-sample odd inc */

	{ 0x3820, CRL_REG_LEN_08BIT, 0x02 },/* Binning */
	{ 0x3842, CRL_REG_LEN_08BIT, 0x40 },/* Binning */
	{ 0x5000, CRL_REG_LEN_08BIT, 0xD9 },/* Binning */

	{ 0x3836, CRL_REG_LEN_08BIT, 0x0C }, /* ablc_use_num */
	{ 0x383C, CRL_REG_LEN_08BIT, 0x48 }, /* Boundary Pix num */

	{ 0x4008, CRL_REG_LEN_08BIT, 0x02 }, /* Magic */
	{ 0x4009, CRL_REG_LEN_08BIT, 0x09 }, /* Magic */
	{ 0x4019, CRL_REG_LEN_08BIT, 0x0C }, /* Magic */
	{ 0x4051, CRL_REG_LEN_08BIT, 0x01 }, /* Magic */
	{ 0x4066, CRL_REG_LEN_08BIT, 0x02 }, /* Magic */

	{ 0x5201, CRL_REG_LEN_08BIT, 0x71 }, /* Magic */
	{ 0x5204, CRL_REG_LEN_08BIT, 0x00 }, /* Magic */
	{ 0x5205, CRL_REG_LEN_08BIT, 0x80 }, /* Magic */
};

static struct crl_register_write_rep ov13860_streamon_regs[] = {
	{ 0x0100, CRL_REG_LEN_08BIT, 0x01 }
};

static struct crl_register_write_rep ov13860_streamoff_regs[] = {
	{ 0x0100, CRL_REG_LEN_08BIT, 0x00 }
};

static struct crl_arithmetic_ops ov13860_vflip_ops[] = {
	{
		.op = CRL_BITWISE_LSHIFT,
		.operand.entity_val = 1,
	 },
};

static struct crl_arithmetic_ops ov13860_hflip_ops[] = {
	{
		.op = CRL_BITWISE_LSHIFT,
		.operand.entity_val = 1,
	 },
};

static struct crl_dynamic_register_access ov13860_v_flip_regs[] = {
	{
		.address = 0x3820,
		.len = CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE,
		.ops_items = ARRAY_SIZE(ov13860_vflip_ops),
		.ops = ov13860_vflip_ops,
		.mask = 0x2,
	 },
};

static struct crl_dynamic_register_access ov13860_h_flip_regs[] = {
	{
		.address = 0x3821,
		.len = CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE,
		.ops_items = ARRAY_SIZE(ov13860_hflip_ops),
		.ops = ov13860_hflip_ops,
		.mask = 0x2,
	 },
};

struct crl_register_write_rep ov13860_poweroff_regset[] = {
	{ 0x0103, CRL_REG_LEN_08BIT, 0x01  },
};

static struct crl_dynamic_register_access ov13860_ana_gain_global_regs[] = {
	{
		.address = 0x350A,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0x7ff,
	 },
};

static struct crl_dynamic_register_access ov13860_exposure_regs[] = {
	{
		.address = 0x3501,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	}
};

static struct crl_dynamic_register_access ov13860_vblank_regs[] = {
	{
		.address = 0x380E,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	 },
};

static struct crl_dynamic_register_access ov13860_hblank_regs[] = {
	{
		.address = 0x380C,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	 },
};

static struct crl_sensor_detect_config ov13860_sensor_detect_regset[] = {
	{
		.reg = { 0x300A, CRL_REG_LEN_08BIT, 0x000000ff  },
		.width = 7,
	 },
	{
		.reg = { 0x300B, CRL_REG_LEN_08BIT, 0x000000ff  },
		.width = 7,
	 },
	{
		.reg = { 0x300C, CRL_REG_LEN_08BIT, 0x000000ff  },
		.width = 7,
	 },
};

static struct crl_pll_configuration ov13860_pll_configurations[] = {
	{
		.input_clk = 24000000,
		.op_sys_clk = 600000000,
		.bitsperpixel = 10,
		.pixel_rate_csi = 150000000,
		.pixel_rate_pa = 240000000,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = ARRAY_SIZE(ov13860_pll_1200mbps),
		.pll_regs = ov13860_pll_1200mbps,
	 },
	{
		.input_clk = 24000000,
		.op_sys_clk = 300000000,
		.bitsperpixel = 10,
		.pixel_rate_csi = 75000000,
		.pixel_rate_pa = 240000000,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = ARRAY_SIZE(ov13860_pll_600mbps),
		.pll_regs = ov13860_pll_600mbps,
	 }
};

static struct crl_subdev_rect_rep ov13860_13m_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4224,
		.in_rect.height = 3120,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 4224,
		.out_rect.height = 3120,
	 },
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4224,
		.in_rect.height = 3120,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 4208,
		.out_rect.height = 3120,
	 },
};

static struct crl_subdev_rect_rep ov13860_8m_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4224,
		.in_rect.height = 3120,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 4224,
		.out_rect.height = 3120,
	 },
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4224,
		.in_rect.height = 3120,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3280,
		.out_rect.height = 2448,
	 },
};

static struct crl_subdev_rect_rep ov13860_4k2k_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4224,
		.in_rect.height = 3120,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 4224,
		.out_rect.height = 3120,
	 },
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4224,
		.in_rect.height = 3120,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 4096,
		.out_rect.height = 2160,
	 },
};

static struct crl_subdev_rect_rep ov13860_uhd_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4224,
		.in_rect.height = 3120,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 4224,
		.out_rect.height = 3120,
	 },
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4224,
		.in_rect.height = 3120,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3840,
		.out_rect.height = 2160,
	 },
};

static struct crl_subdev_rect_rep ov13860_6m_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4224,
		.in_rect.height = 3120,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 4224,
		.out_rect.height = 3120,
	 },
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4224,
		.in_rect.height = 3120,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3280,
		.out_rect.height = 1836,
	 },
};

static struct crl_subdev_rect_rep ov13860_3m_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4224,
		.in_rect.height = 3120,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 4224,
		.out_rect.height = 3120,
	 },
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4224,
		.in_rect.height = 3120,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 2048,
		.out_rect.height = 1536,
	 },
};

static struct crl_subdev_rect_rep ov13860_1952_1088_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4224,
		.in_rect.height = 3120,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 4224,
		.out_rect.height = 3120,
	 },
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4224,
		.in_rect.height = 3120,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1952,
		.out_rect.height = 1088,
	 },
};

static struct crl_subdev_rect_rep ov13860_720_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4224,
		.in_rect.height = 3120,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 4224,
		.out_rect.height = 3120,
	 },
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4224,
		.in_rect.height = 3120,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1280,
		.out_rect.height = 720,
	 },
};

static struct crl_subdev_rect_rep ov13860_480_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4224,
		.in_rect.height = 3120,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 4224,
		.out_rect.height = 3120,
	 },
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 4224,
		.in_rect.height = 3120,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 640,
		.out_rect.height = 480,
	 },
};

static struct crl_mode_rep ov13860_modes[] = {
	{
		.sd_rects_items = ARRAY_SIZE(ov13860_13m_rects),
		.sd_rects = ov13860_13m_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 4208,
		.height = 3120,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(ov13860_mode_13m),
		.mode_regs = ov13860_mode_13m,
	 },
		{
		.sd_rects_items = ARRAY_SIZE(ov13860_8m_rects),
		.sd_rects = ov13860_8m_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 3280,
		.height = 2448,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(ov13860_mode_8m),
		.mode_regs = ov13860_mode_8m,
	 },
		{
		.sd_rects_items = ARRAY_SIZE(ov13860_4k2k_rects),
		.sd_rects = ov13860_4k2k_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 4096,
		.height = 2160,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(ov13860_mode_4k2k),
		.mode_regs = ov13860_mode_4k2k,
	 },
		{
		.sd_rects_items = ARRAY_SIZE(ov13860_uhd_rects),
		.sd_rects = ov13860_uhd_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 3840,
		.height = 2160,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(ov13860_mode_uhd),
		.mode_regs = ov13860_mode_uhd,
	 },
		{
		.sd_rects_items = ARRAY_SIZE(ov13860_6m_rects),
		.sd_rects = ov13860_6m_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 3280,
		.height = 1836,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(ov13860_mode_6m),
		.mode_regs = ov13860_mode_6m,
	 },
		{
		.sd_rects_items = ARRAY_SIZE(ov13860_3m_rects),
		.sd_rects = ov13860_3m_rects,
		.binn_hor = 2,
		.binn_vert = 2,
		.scale_m = 1,
		.width = 2048,
		.height = 1536,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(ov13860_mode_3m),
		.mode_regs = ov13860_mode_3m,
	 },
		{
		.sd_rects_items = ARRAY_SIZE(ov13860_1952_1088_rects),
		.sd_rects = ov13860_1952_1088_rects,
		.binn_hor = 2,
		.binn_vert = 2,
		.scale_m = 1,
		.width = 1952,
		.height = 1088,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(ov13860_mode_1952_1088),
		.mode_regs = ov13860_mode_1952_1088,
	 },
		{
		.sd_rects_items = ARRAY_SIZE(ov13860_720_rects),
		.sd_rects = ov13860_720_rects,
		.binn_hor = 2,
		.binn_vert = 2,
		.scale_m = 1,
		.width = 1280,
		.height = 720,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(ov13860_mode_720),
		.mode_regs = ov13860_mode_720,
	 },
		{
		.sd_rects_items = ARRAY_SIZE(ov13860_480_rects),
		.sd_rects = ov13860_480_rects,
		.binn_hor = 2,
		.binn_vert = 2,
		.scale_m = 1,
		.width = 640,
		.height = 480,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(ov13860_mode_480),
		.mode_regs = ov13860_mode_480,
	 },
};

static struct crl_sensor_subdev_config ov13860_sensor_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.name = "ov13860 binner",
	 },
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "ov13860 pixel array",
	 },
};

static struct crl_sensor_limits ov13860_sensor_limits = {
	.x_addr_min = 0,
	.y_addr_min = 0,
	.x_addr_max = 4224,
	.y_addr_max = 3120,
	.min_frame_length_lines = 160,
	.max_frame_length_lines = 65535,
	.min_line_length_pixels = 6024,
	.max_line_length_pixels = 32752,
	.scaler_m_min = 16,
	.scaler_m_max = 16,
	.scaler_n_min = 16,
	.scaler_n_max = 16,
	.min_even_inc = 1,
	.max_even_inc = 1,
	.min_odd_inc = 1,
	.max_odd_inc = 3,
};

static struct crl_flip_data ov13860_flip_configurations[] = {
	{
		.flip = CRL_FLIP_DEFAULT_NONE,
		.pixel_order = CRL_PIXEL_ORDER_BGGR,
	 },
	{
		.flip = CRL_FLIP_HFLIP,
		.pixel_order = CRL_PIXEL_ORDER_BGGR,
	 },
	{
		.flip = CRL_FLIP_VFLIP,
		.pixel_order = CRL_PIXEL_ORDER_BGGR,
	 },
	{
		.flip = CRL_FLIP_HFLIP_VFLIP,
		.pixel_order = CRL_PIXEL_ORDER_BGGR,
	 },
};

static struct crl_csi_data_fmt ov13860_crl_csi_data_fmt[] = {
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

static struct crl_v4l2_ctrl ov13860_v4l2_ctrls[] = {
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
		.regs_items = ARRAY_SIZE(ov13860_ana_gain_global_regs),
		.regs = ov13860_ana_gain_global_regs,
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
		.regs_items = ARRAY_SIZE(ov13860_exposure_regs),
		.regs = ov13860_exposure_regs,
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
		.regs_items = ARRAY_SIZE(ov13860_h_flip_regs),
		.regs = ov13860_h_flip_regs,
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
		.regs_items = ARRAY_SIZE(ov13860_v_flip_regs),
		.regs = ov13860_v_flip_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
	 },
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_VBLANK,
		.name = "V4L2_CID_VBLANK",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = 65535,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(ov13860_vblank_regs),
		.regs = ov13860_vblank_regs,
		.dep_items = 0, /* FLL changed automatically */
		.dep_ctrls = 0,
	 },
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_HBLANK,
		.name = "V4L2_CID_HBLANK",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = 65520,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(ov13860_hblank_regs),
		.regs = ov13860_hblank_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
	 },
};

/* Power items, they are enabled in the order they are listed here */
static struct crl_power_seq_entity ov13860_power_items[] = {
	{
		.type = CRL_POWER_ETY_CLK_FRAMEWORK,
		.val = 24000000,
	},
	{
		.type = CRL_POWER_ETY_GPIO_FROM_PDATA,
		.val = 1,
	},
};

struct crl_sensor_configuration ov13860_crl_configuration = {

	.power_items = ARRAY_SIZE(ov13860_power_items),
	.power_entities = ov13860_power_items,

	.powerup_regs_items = ARRAY_SIZE(ov13860_powerup_regset),
	.powerup_regs = ov13860_powerup_regset,

	.poweroff_regs_items = 0,
	.poweroff_regs = 0,

	.id_reg_items = ARRAY_SIZE(ov13860_sensor_detect_regset),
	.id_regs = ov13860_sensor_detect_regset,

	.subdev_items = ARRAY_SIZE(ov13860_sensor_subdevs),
	.subdevs = ov13860_sensor_subdevs,

	.sensor_limits = &ov13860_sensor_limits,

	.pll_config_items = ARRAY_SIZE(ov13860_pll_configurations),
	.pll_configs = ov13860_pll_configurations,

	.modes_items = ARRAY_SIZE(ov13860_modes),
	.modes = ov13860_modes,

	.streamon_regs_items = ARRAY_SIZE(ov13860_streamon_regs),
	.streamon_regs = ov13860_streamon_regs,

	.streamoff_regs_items = ARRAY_SIZE(ov13860_streamoff_regs),
	.streamoff_regs = ov13860_streamoff_regs,

	.v4l2_ctrls_items = ARRAY_SIZE(ov13860_v4l2_ctrls),
	.v4l2_ctrl_bank = ov13860_v4l2_ctrls,

	.csi_fmts_items = ARRAY_SIZE(ov13860_crl_csi_data_fmt),
	.csi_fmts = ov13860_crl_csi_data_fmt,

	.flip_items = ARRAY_SIZE(ov13860_flip_configurations),
	.flip_data = ov13860_flip_configurations,
};

#endif  /* __CRLMODULE_OV13860_CONFIGURATION_H_ */


