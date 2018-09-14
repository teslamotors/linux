/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2017 - 2018 Intel Corporation
 *
 * Author: Shuguang Gong <shuguang.gong@intel.com>
 *
 */

#ifndef __CRLMODULE_IMX185_CONFIGURATION_H_
#define __CRLMODULE_IMX185_CONFIGURATION_H_

#include "crlmodule-sensor-ds.h"

#define IMX185_REG_STANDBY		0x3000
#define IMX185_REG_XMSTA		0x3002
#define IMX185_REG_SW_RESET		0x3003

#define IMX185_HMAX			65535
#define IMX185_VMAX			131071
#define IMX185_MAX_SHS1			(IMX185_VMAX - 2)

struct crl_ctrl_data_pair ctrl_data_modes[] = {
	{
		.ctrl_id = V4L2_CID_WDR_MODE,
		.data = 0,
	},
	{
		.ctrl_id = V4L2_CID_WDR_MODE,
		.data = 1,
	},
};

/* 111Mbps for imx185 720p 30fps */
static struct crl_register_write_rep imx185_pll_111mbps[] = {
	{0x3009, CRL_REG_LEN_08BIT, 0x02},	/* frame speed */
	{0x300A, CRL_REG_LEN_08BIT, 0x3C},
	{0x300C, CRL_REG_LEN_08BIT, 0x00},
	{0x3018, CRL_REG_LEN_08BIT, 0xee},
	{0x3019, CRL_REG_LEN_08BIT, 0x02},
	{0x301b, CRL_REG_LEN_08BIT, 0xe4},
	{0x301c, CRL_REG_LEN_08BIT, 0x0C},
	{0x300F, CRL_REG_LEN_08BIT, 0x01},
	{0x3010, CRL_REG_LEN_08BIT, 0x39},
	{0x3012, CRL_REG_LEN_08BIT, 0x50},
	{0x3056, CRL_REG_LEN_08BIT, 0xC9},
	{0x3057, CRL_REG_LEN_08BIT, 0x64},
	{0x3065, CRL_REG_LEN_08BIT, 0x00},
	{0x3084, CRL_REG_LEN_08BIT, 0x0F},
	{0x3086, CRL_REG_LEN_08BIT, 0x10},
	{0x30CF, CRL_REG_LEN_08BIT, 0xE1},
	{0x30D0, CRL_REG_LEN_08BIT, 0x29},
	{0x30D2, CRL_REG_LEN_08BIT, 0x9B},
	{0x30D3, CRL_REG_LEN_08BIT, 0x01},
	{0x30E1, CRL_REG_LEN_08BIT, 0xFF},
	{0x3303, CRL_REG_LEN_08BIT, 0x20},	/* repetation */
	{0x3305, CRL_REG_LEN_08BIT, 0x03},	/* 1: 2lanes, 3: 4lanes */
	{0x332C, CRL_REG_LEN_08BIT, 0x28},	/* mipi timing */
	{0x332D, CRL_REG_LEN_08BIT, 0x20},
	{0x3341, CRL_REG_LEN_08BIT, 0x00},
	{0x3342, CRL_REG_LEN_08BIT, 0x1B},
	{0x3343, CRL_REG_LEN_08BIT, 0x58},
	{0x3344, CRL_REG_LEN_08BIT, 0x0C},
	{0x3345, CRL_REG_LEN_08BIT, 0x24},
	{0x3346, CRL_REG_LEN_08BIT, 0x10},
	{0x3347, CRL_REG_LEN_08BIT, 0x0B},
	{0x3348, CRL_REG_LEN_08BIT, 0x08},
	{0x3349, CRL_REG_LEN_08BIT, 0x30},
	{0x334A, CRL_REG_LEN_08BIT, 0x20},
};

/* 222Mbps for imx185 1080p 30fps */
static struct crl_register_write_rep imx185_pll_222mbps[] = {
	{0x3009, CRL_REG_LEN_08BIT, 0x02},	/* frame speed */
	{0x300A, CRL_REG_LEN_08BIT, 0x3C},
	{0x300C, CRL_REG_LEN_08BIT, 0x00},
	{0x301b, CRL_REG_LEN_08BIT, 0x98},
	{0x301c, CRL_REG_LEN_08BIT, 0x08},
	{0x300F, CRL_REG_LEN_08BIT, 0x01},
	{0x3010, CRL_REG_LEN_08BIT, 0x39},
	{0x3012, CRL_REG_LEN_08BIT, 0x50},
	{0x3056, CRL_REG_LEN_08BIT, 0xC9},
	{0x3057, CRL_REG_LEN_08BIT, 0x64},
	{0x3065, CRL_REG_LEN_08BIT, 0x00},
	{0x3084, CRL_REG_LEN_08BIT, 0x00},
	{0x3086, CRL_REG_LEN_08BIT, 0x01},
	{0x30CF, CRL_REG_LEN_08BIT, 0xD1},
	{0x30D0, CRL_REG_LEN_08BIT, 0x1B},
	{0x30D2, CRL_REG_LEN_08BIT, 0x5F},
	{0x30D3, CRL_REG_LEN_08BIT, 0x00},
	{0x30E1, CRL_REG_LEN_08BIT, 0xFF},
	{0x3303, CRL_REG_LEN_08BIT, 0x10},	/* repetation */
	{0x3305, CRL_REG_LEN_08BIT, 0x03},	/* 1: 2lanes, 3: 4lanes */
	{0x332C, CRL_REG_LEN_08BIT, 0x30},	/* mipi timing */
	{0x332D, CRL_REG_LEN_08BIT, 0x20},
	{0x3341, CRL_REG_LEN_08BIT, 0x00},
	{0x3342, CRL_REG_LEN_08BIT, 0x1B},
	{0x3343, CRL_REG_LEN_08BIT, 0x58},
	{0x3344, CRL_REG_LEN_08BIT, 0x10},
	{0x3345, CRL_REG_LEN_08BIT, 0x30},
	{0x3346, CRL_REG_LEN_08BIT, 0x18},
	{0x3347, CRL_REG_LEN_08BIT, 0x10},
	{0x3348, CRL_REG_LEN_08BIT, 0x10},
	{0x3349, CRL_REG_LEN_08BIT, 0x48},
	{0x334A, CRL_REG_LEN_08BIT, 0x28},
};

/* 445Mbps for imx185 1080p 60fps */
static struct crl_register_write_rep imx185_pll_445mbps[] = {
	{0x3009, CRL_REG_LEN_08BIT, 0x01},	/* frame speed */
	{0x300A, CRL_REG_LEN_08BIT, 0x3C},	/* BLK */
	{0x300C, CRL_REG_LEN_08BIT, 0x00},	/* fixed settings */
	{0x3018, CRL_REG_LEN_08BIT, 0x65},
	{0x3019, CRL_REG_LEN_08BIT, 0x04},
	{0x301B, CRL_REG_LEN_08BIT, 0x4C},
	{0x301C, CRL_REG_LEN_08BIT, 0x04},
	{0x300F, CRL_REG_LEN_08BIT, 0x01},
	{0x3010, CRL_REG_LEN_08BIT, 0x39},
	{0x3012, CRL_REG_LEN_08BIT, 0x50},
	{0x3056, CRL_REG_LEN_08BIT, 0xC9},
	{0x3057, CRL_REG_LEN_08BIT, 0x64},
	{0x3065, CRL_REG_LEN_08BIT, 0x20},
	{0x3084, CRL_REG_LEN_08BIT, 0x00},
	{0x3086, CRL_REG_LEN_08BIT, 0x01},
	{0x30CF, CRL_REG_LEN_08BIT, 0xD1},
	{0x30D0, CRL_REG_LEN_08BIT, 0x1B},
	{0x30D2, CRL_REG_LEN_08BIT, 0x5F},
	{0x30D3, CRL_REG_LEN_08BIT, 0x00},
	{0x30E1, CRL_REG_LEN_08BIT, 0xFF},
	{0x3303, CRL_REG_LEN_08BIT, 0x00},	/* repetation */
	{0x3305, CRL_REG_LEN_08BIT, 0x03},
	{0x332C, CRL_REG_LEN_08BIT, 0x40},	/* mipi timing */
	{0x332D, CRL_REG_LEN_08BIT, 0x20},
	{0x3341, CRL_REG_LEN_08BIT, 0x00},
	{0x3342, CRL_REG_LEN_08BIT, 0x1B},
	{0x3343, CRL_REG_LEN_08BIT, 0x68},
	{0x3344, CRL_REG_LEN_08BIT, 0x20},
	{0x3345, CRL_REG_LEN_08BIT, 0x40},
	{0x3346, CRL_REG_LEN_08BIT, 0x28},
	{0x3347, CRL_REG_LEN_08BIT, 0x20},
	{0x3348, CRL_REG_LEN_08BIT, 0x18},
	{0x3349, CRL_REG_LEN_08BIT, 0x78},
	{0x334A, CRL_REG_LEN_08BIT, 0x28},
};

static struct crl_register_write_rep imx185_fmt_raw10[] = {
	{0x333E, CRL_REG_LEN_08BIT, 0x0a},	/* FMT RAW10 */
	{0x333F, CRL_REG_LEN_08BIT, 0x0a},
};

static struct crl_register_write_rep imx185_fmt_raw12[] = {
	{0x333E, CRL_REG_LEN_08BIT, 0x0c},	/* FMT RAW12 */
	{0x333F, CRL_REG_LEN_08BIT, 0x0c},
};

static struct crl_register_write_rep imx185_powerup_standby[] = {
	{0x3000, CRL_REG_LEN_08BIT, 0x01},
	{0x00, CRL_REG_LEN_DELAY, 50, 0x00},
	{0x3002, CRL_REG_LEN_08BIT, 0x01},
	{0x00, CRL_REG_LEN_DELAY, 200, 0x00},
};

static struct crl_register_write_rep imx185_1312_728_27MHZ_CROPPING[] = {
	/* 0x02h */
	{0x3005, CRL_REG_LEN_08BIT, 0x00},	/* ADBIT: 10/12 ADBIT:
						10/12 , raw 10 */
	{0x3007, CRL_REG_LEN_08BIT, 0x60},	/* mode selection */
	{0x301D, CRL_REG_LEN_08BIT, 0x08},
	{0x301E, CRL_REG_LEN_08BIT, 0x02},
	{0x3044, CRL_REG_LEN_08BIT, 0xE1},
	{0x3048, CRL_REG_LEN_08BIT, 0x33},
	{0x305C, CRL_REG_LEN_08BIT, 0x2c},
	{0x305E, CRL_REG_LEN_08BIT, 0x21},
	{0x3063, CRL_REG_LEN_08BIT, 0x54},
	/* Crop settings */
	{0x3038, CRL_REG_LEN_08BIT, 0x00},	/* WPV = 0 */
	{0x3039, CRL_REG_LEN_08BIT, 0x00},
	{0x303A, CRL_REG_LEN_08BIT, 0xE0},	/* WV = PIC_SIZE + 8 */
	{0x303B, CRL_REG_LEN_08BIT, 0x02},
	{0x303C, CRL_REG_LEN_08BIT, 0x04},	/* WPH = 4 */
	{0x303D, CRL_REG_LEN_08BIT, 0x00},
	{0x303E, CRL_REG_LEN_08BIT, 0x1C},	/* Effective size = 1308*/
	{0x303F, CRL_REG_LEN_08BIT, 0x05},
	/* 0x03h */
	{0x311D, CRL_REG_LEN_08BIT, 0x0A},
	{0x3123, CRL_REG_LEN_08BIT, 0x0F},
	{0x3147, CRL_REG_LEN_08BIT, 0x87},
	{0x31E1, CRL_REG_LEN_08BIT, 0x9E},
	{0x31E2, CRL_REG_LEN_08BIT, 0x01},
	{0x31E5, CRL_REG_LEN_08BIT, 0x05},
	{0x31E6, CRL_REG_LEN_08BIT, 0x05},
	{0x31E7, CRL_REG_LEN_08BIT, 0x3A},
	{0x31E8, CRL_REG_LEN_08BIT, 0x3A},
	/* 0x04h */
	{0x3203, CRL_REG_LEN_08BIT, 0xC8},
	{0x3207, CRL_REG_LEN_08BIT, 0x54},
	{0x3213, CRL_REG_LEN_08BIT, 0x16},
	{0x3215, CRL_REG_LEN_08BIT, 0xF6},
	{0x321A, CRL_REG_LEN_08BIT, 0x14},
	{0x321B, CRL_REG_LEN_08BIT, 0x51},
	{0x3229, CRL_REG_LEN_08BIT, 0xE7},
	{0x322A, CRL_REG_LEN_08BIT, 0xF0},
	{0x322B, CRL_REG_LEN_08BIT, 0x10},
	{0x3231, CRL_REG_LEN_08BIT, 0xE7},
	{0x3232, CRL_REG_LEN_08BIT, 0xF0},
	{0x3233, CRL_REG_LEN_08BIT, 0x10},
	{0x323C, CRL_REG_LEN_08BIT, 0xE8},
	{0x323D, CRL_REG_LEN_08BIT, 0x70},
	{0x3243, CRL_REG_LEN_08BIT, 0x08},
	{0x3244, CRL_REG_LEN_08BIT, 0xE1},
	{0x3245, CRL_REG_LEN_08BIT, 0x10},
	{0x3247, CRL_REG_LEN_08BIT, 0xE7},
	{0x3248, CRL_REG_LEN_08BIT, 0x60},
	{0x3249, CRL_REG_LEN_08BIT, 0x1E},
	{0x324B, CRL_REG_LEN_08BIT, 0x00},
	{0x324C, CRL_REG_LEN_08BIT, 0x41},
	{0x3250, CRL_REG_LEN_08BIT, 0x30},
	{0x3251, CRL_REG_LEN_08BIT, 0x0A},
	{0x3252, CRL_REG_LEN_08BIT, 0xFF},
	{0x3253, CRL_REG_LEN_08BIT, 0xFF},
	{0x3254, CRL_REG_LEN_08BIT, 0xFF},
	{0x3255, CRL_REG_LEN_08BIT, 0x02},
	{0x3257, CRL_REG_LEN_08BIT, 0xF0},
	{0x325A, CRL_REG_LEN_08BIT, 0xA6},
	{0x325D, CRL_REG_LEN_08BIT, 0x14},
	{0x325E, CRL_REG_LEN_08BIT, 0x51},
	{0x3261, CRL_REG_LEN_08BIT, 0x61},
	{0x3266, CRL_REG_LEN_08BIT, 0x30},
	{0x3267, CRL_REG_LEN_08BIT, 0x05},
	{0x3275, CRL_REG_LEN_08BIT, 0xE7},
	{0x3281, CRL_REG_LEN_08BIT, 0xEA},
	{0x3282, CRL_REG_LEN_08BIT, 0x70},
	{0x3285, CRL_REG_LEN_08BIT, 0xFF},
	{0x328A, CRL_REG_LEN_08BIT, 0xF0},
	{0x328D, CRL_REG_LEN_08BIT, 0xB6},
	{0x328E, CRL_REG_LEN_08BIT, 0x40},
	{0x3290, CRL_REG_LEN_08BIT, 0x42},
	{0x3291, CRL_REG_LEN_08BIT, 0x51},
	{0x3292, CRL_REG_LEN_08BIT, 0x1E},
	{0x3294, CRL_REG_LEN_08BIT, 0xC4},
	{0x3295, CRL_REG_LEN_08BIT, 0x20},
	{0x3297, CRL_REG_LEN_08BIT, 0x50},
	{0x3298, CRL_REG_LEN_08BIT, 0x31},
	{0x3299, CRL_REG_LEN_08BIT, 0x1F},
	{0x329B, CRL_REG_LEN_08BIT, 0xC0},
	{0x329C, CRL_REG_LEN_08BIT, 0x60},
	{0x329E, CRL_REG_LEN_08BIT, 0x4C},
	{0x329F, CRL_REG_LEN_08BIT, 0x71},
	{0x32A0, CRL_REG_LEN_08BIT, 0x1F},
	{0x32A2, CRL_REG_LEN_08BIT, 0xB6},
	{0x32A3, CRL_REG_LEN_08BIT, 0xC0},
	{0x32A4, CRL_REG_LEN_08BIT, 0x0B},
	{0x32A9, CRL_REG_LEN_08BIT, 0x24},
	{0x32AA, CRL_REG_LEN_08BIT, 0x41},
	{0x32B0, CRL_REG_LEN_08BIT, 0x25},
	{0x32B1, CRL_REG_LEN_08BIT, 0x51},
	{0x32B7, CRL_REG_LEN_08BIT, 0x1C},
	{0x32B8, CRL_REG_LEN_08BIT, 0xC1},
	{0x32B9, CRL_REG_LEN_08BIT, 0x12},
	{0x32BE, CRL_REG_LEN_08BIT, 0x1D},
	{0x32BF, CRL_REG_LEN_08BIT, 0xD1},
	{0x32C0, CRL_REG_LEN_08BIT, 0x12},
	{0x32C2, CRL_REG_LEN_08BIT, 0xA8},
	{0x32C3, CRL_REG_LEN_08BIT, 0xC0},
	{0x32C4, CRL_REG_LEN_08BIT, 0x0A},
	{0x32C5, CRL_REG_LEN_08BIT, 0x1E},
	{0x32C6, CRL_REG_LEN_08BIT, 0x21},
	{0x32C9, CRL_REG_LEN_08BIT, 0xB0},
	{0x32CA, CRL_REG_LEN_08BIT, 0x40},
	{0x32CC, CRL_REG_LEN_08BIT, 0x26},
	{0x32CD, CRL_REG_LEN_08BIT, 0xA1},
	{0x32D0, CRL_REG_LEN_08BIT, 0xB6},
	{0x32D1, CRL_REG_LEN_08BIT, 0xC0},
	{0x32D2, CRL_REG_LEN_08BIT, 0x0B},
	{0x32D4, CRL_REG_LEN_08BIT, 0xE2},
	{0x32D5, CRL_REG_LEN_08BIT, 0x40},
	{0x32D8, CRL_REG_LEN_08BIT, 0x4E},
	{0x32D9, CRL_REG_LEN_08BIT, 0xA1},
	{0x32EC, CRL_REG_LEN_08BIT, 0xF0},
	/* 0x05h */
	{0x3316, CRL_REG_LEN_08BIT, 0x02},
	{0x3317, CRL_REG_LEN_08BIT, 0x02},
	{0x3318, CRL_REG_LEN_08BIT, 0xD8},	/* PIC_SIZE = 728 */
	{0x3319, CRL_REG_LEN_08BIT, 0x02},
	{0x334E, CRL_REG_LEN_08BIT, 0x3D},	/* INCL selection 27MHz */
	{0x334F, CRL_REG_LEN_08BIT, 0x01},
};

static struct crl_register_write_rep imx185_1952_1096_27MHZ[] = {
	/* 0x02h */
	{0x3005, CRL_REG_LEN_08BIT, 0x01},	/* ADBIT: 10/12 */
	{0x3007, CRL_REG_LEN_08BIT, 0x10},	/* 1080p mode */
	{0x300A, CRL_REG_LEN_08BIT, 0xF0},
	{0x301D, CRL_REG_LEN_08BIT, 0x08},
	{0x301E, CRL_REG_LEN_08BIT, 0x02},
	{0x3048, CRL_REG_LEN_08BIT, 0x33},
	{0x305C, CRL_REG_LEN_08BIT, 0x2c},	/* INCLKSEL default */
	{0x305E, CRL_REG_LEN_08BIT, 0x21},
	{0x3063, CRL_REG_LEN_08BIT, 0x54},
	/* Crop settings */
	{0x3038, CRL_REG_LEN_08BIT, 0x00},	/* WPV = 0 */
	{0x3039, CRL_REG_LEN_08BIT, 0x00},
	{0x303A, CRL_REG_LEN_08BIT, 0xC0},	/* WV = PIC_SIZE + 8 */
	{0x303B, CRL_REG_LEN_08BIT, 0x04},
	{0x303C, CRL_REG_LEN_08BIT, 0x00},	/* WPH = 0 */
	{0x303D, CRL_REG_LEN_08BIT, 0x00},
	{0x303E, CRL_REG_LEN_08BIT, 0x9C},
	{0x303F, CRL_REG_LEN_08BIT, 0x07},
	/* 0x03h */
	{0x311D, CRL_REG_LEN_08BIT, 0x0A},
	{0x3123, CRL_REG_LEN_08BIT, 0x0F},
	{0x3126, CRL_REG_LEN_08BIT, 0xDF},
	{0x3147, CRL_REG_LEN_08BIT, 0x87},
	{0x31E0, CRL_REG_LEN_08BIT, 0x01},
	{0x31E1, CRL_REG_LEN_08BIT, 0x9E},
	{0x31E2, CRL_REG_LEN_08BIT, 0x01},
	{0x31E5, CRL_REG_LEN_08BIT, 0x05},
	{0x31E6, CRL_REG_LEN_08BIT, 0x05},
	{0x31E7, CRL_REG_LEN_08BIT, 0x3A},
	{0x31E8, CRL_REG_LEN_08BIT, 0x3A},
	/* 0x04h */
	{0x3203, CRL_REG_LEN_08BIT, 0xC8},
	{0x3207, CRL_REG_LEN_08BIT, 0x54},
	{0x3213, CRL_REG_LEN_08BIT, 0x16},
	{0x3215, CRL_REG_LEN_08BIT, 0xF6},
	{0x321A, CRL_REG_LEN_08BIT, 0x14},
	{0x321B, CRL_REG_LEN_08BIT, 0x51},
	{0x3229, CRL_REG_LEN_08BIT, 0xE7},
	{0x322A, CRL_REG_LEN_08BIT, 0xF0},
	{0x322B, CRL_REG_LEN_08BIT, 0x10},
	{0x3231, CRL_REG_LEN_08BIT, 0xE7},
	{0x3232, CRL_REG_LEN_08BIT, 0xF0},
	{0x3233, CRL_REG_LEN_08BIT, 0x10},
	{0x323C, CRL_REG_LEN_08BIT, 0xE8},
	{0x323D, CRL_REG_LEN_08BIT, 0x70},
	{0x3243, CRL_REG_LEN_08BIT, 0x08},
	{0x3244, CRL_REG_LEN_08BIT, 0xE1},
	{0x3245, CRL_REG_LEN_08BIT, 0x10},
	{0x3247, CRL_REG_LEN_08BIT, 0xE7},
	{0x3248, CRL_REG_LEN_08BIT, 0x60},
	{0x3249, CRL_REG_LEN_08BIT, 0x1E},
	{0x324B, CRL_REG_LEN_08BIT, 0x00},
	{0x324C, CRL_REG_LEN_08BIT, 0x41},
	{0x3250, CRL_REG_LEN_08BIT, 0x30},
	{0x3251, CRL_REG_LEN_08BIT, 0x0A},
	{0x3252, CRL_REG_LEN_08BIT, 0xFF},
	{0x3253, CRL_REG_LEN_08BIT, 0xFF},
	{0x3254, CRL_REG_LEN_08BIT, 0xFF},
	{0x3255, CRL_REG_LEN_08BIT, 0x02},
	{0x3257, CRL_REG_LEN_08BIT, 0xF0},
	{0x325A, CRL_REG_LEN_08BIT, 0xA6},
	{0x325D, CRL_REG_LEN_08BIT, 0x14},
	{0x325E, CRL_REG_LEN_08BIT, 0x51},
	{0x3261, CRL_REG_LEN_08BIT, 0x61},
	{0x3266, CRL_REG_LEN_08BIT, 0x30},
	{0x3267, CRL_REG_LEN_08BIT, 0x05},
	{0x3275, CRL_REG_LEN_08BIT, 0xE7},
	{0x3281, CRL_REG_LEN_08BIT, 0xEA},
	{0x3282, CRL_REG_LEN_08BIT, 0x70},
	{0x3285, CRL_REG_LEN_08BIT, 0xFF},
	{0x328A, CRL_REG_LEN_08BIT, 0xF0},
	{0x328D, CRL_REG_LEN_08BIT, 0xB6},
	{0x328E, CRL_REG_LEN_08BIT, 0x40},
	{0x3290, CRL_REG_LEN_08BIT, 0x42},
	{0x3291, CRL_REG_LEN_08BIT, 0x51},
	{0x3292, CRL_REG_LEN_08BIT, 0x1E},
	{0x3294, CRL_REG_LEN_08BIT, 0xC4},
	{0x3295, CRL_REG_LEN_08BIT, 0x20},
	{0x3297, CRL_REG_LEN_08BIT, 0x50},
	{0x3298, CRL_REG_LEN_08BIT, 0x31},
	{0x3299, CRL_REG_LEN_08BIT, 0x1F},
	{0x329B, CRL_REG_LEN_08BIT, 0xC0},
	{0x329C, CRL_REG_LEN_08BIT, 0x60},
	{0x329E, CRL_REG_LEN_08BIT, 0x4C},
	{0x329F, CRL_REG_LEN_08BIT, 0x71},
	{0x32A0, CRL_REG_LEN_08BIT, 0x1F},
	{0x32A2, CRL_REG_LEN_08BIT, 0xB6},
	{0x32A3, CRL_REG_LEN_08BIT, 0xC0},
	{0x32A4, CRL_REG_LEN_08BIT, 0x0B},
	{0x32A9, CRL_REG_LEN_08BIT, 0x24},
	{0x32AA, CRL_REG_LEN_08BIT, 0x41},
	{0x32B0, CRL_REG_LEN_08BIT, 0x25},
	{0x32B1, CRL_REG_LEN_08BIT, 0x51},
	{0x32B7, CRL_REG_LEN_08BIT, 0x1C},
	{0x32B8, CRL_REG_LEN_08BIT, 0xC1},
	{0x32B9, CRL_REG_LEN_08BIT, 0x12},
	{0x32BE, CRL_REG_LEN_08BIT, 0x1D},
	{0x32BF, CRL_REG_LEN_08BIT, 0xD1},
	{0x32C0, CRL_REG_LEN_08BIT, 0x12},
	{0x32C2, CRL_REG_LEN_08BIT, 0xA8},
	{0x32C3, CRL_REG_LEN_08BIT, 0xC0},
	{0x32C4, CRL_REG_LEN_08BIT, 0x0A},
	{0x32C5, CRL_REG_LEN_08BIT, 0x1E},
	{0x32C6, CRL_REG_LEN_08BIT, 0x21},
	{0x32C9, CRL_REG_LEN_08BIT, 0xB0},
	{0x32CA, CRL_REG_LEN_08BIT, 0x40},
	{0x32CC, CRL_REG_LEN_08BIT, 0x26},
	{0x32CD, CRL_REG_LEN_08BIT, 0xA1},
	{0x32D0, CRL_REG_LEN_08BIT, 0xB6},
	{0x32D1, CRL_REG_LEN_08BIT, 0xC0},
	{0x32D2, CRL_REG_LEN_08BIT, 0x0B},
	{0x32D4, CRL_REG_LEN_08BIT, 0xE2},
	{0x32D5, CRL_REG_LEN_08BIT, 0x40},
	{0x32D8, CRL_REG_LEN_08BIT, 0x4E},
	{0x32D9, CRL_REG_LEN_08BIT, 0xA1},
	{0x32EC, CRL_REG_LEN_08BIT, 0xF0},
	/* 0x05h */
	{0x3316, CRL_REG_LEN_08BIT, 0x04},
	{0x3317, CRL_REG_LEN_08BIT, 0x04},
	{0x3318, CRL_REG_LEN_08BIT, 0x48},	/* PIC_SIZE = 1096 */
	{0x3319, CRL_REG_LEN_08BIT, 0x04},
	{0x334E, CRL_REG_LEN_08BIT, 0x3D},	/* INCL selection 27MHz */
	{0x334F, CRL_REG_LEN_08BIT, 0x01},
};

static struct crl_register_write_rep imx185_1952_1096_BUILD_IN_WDR_27MHZ[] = {
	/* 0x02h */
	{0x3005, CRL_REG_LEN_08BIT, 0x01},	/* ADBIT: 10/12 */
	{0x3007, CRL_REG_LEN_08BIT, 0x10},	/* mode selection */
	{0x300A, CRL_REG_LEN_08BIT, 0xF0},
	{0x300C, CRL_REG_LEN_08BIT, 0x02},
	{0x300F, CRL_REG_LEN_08BIT, 0x05},
	{0x3010, CRL_REG_LEN_08BIT, 0x38},
	{0x3012, CRL_REG_LEN_08BIT, 0x0F},
	{0x301B, CRL_REG_LEN_08BIT, 0x98},
	{0x301C, CRL_REG_LEN_08BIT, 0x08},
	{0x301D, CRL_REG_LEN_08BIT, 0x08},
	{0x301E, CRL_REG_LEN_08BIT, 0x02},
	{0x3048, CRL_REG_LEN_08BIT, 0x33},
	{0x3056, CRL_REG_LEN_08BIT, 0xC9},
	{0x3057, CRL_REG_LEN_08BIT, 0x64},
	{0x305C, CRL_REG_LEN_08BIT, 0x2c},	/* INCLKSEL default */
	{0x305E, CRL_REG_LEN_08BIT, 0x21},
	{0x3063, CRL_REG_LEN_08BIT, 0x54},
	/* Crop settings */
	{0x3038, CRL_REG_LEN_08BIT, 0x00},	/* WPV = 0 */
	{0x3039, CRL_REG_LEN_08BIT, 0x00},
	{0x303A, CRL_REG_LEN_08BIT, 0x4C},	/* WV = PIC_SIZE + 8 */
	{0x303B, CRL_REG_LEN_08BIT, 0x04},
	{0x303C, CRL_REG_LEN_08BIT, 0x00},	/* WPH = 0 */
	{0x303D, CRL_REG_LEN_08BIT, 0x00},
	{0x303E, CRL_REG_LEN_08BIT, 0x9C},	/* Effective size = 1948*/
	{0x303F, CRL_REG_LEN_08BIT, 0x07},
	/* 0x03h */
	{0x311D, CRL_REG_LEN_08BIT, 0x0A},
	{0x3123, CRL_REG_LEN_08BIT, 0x0F},
	{0x3126, CRL_REG_LEN_08BIT, 0xDF},
	{0x3147, CRL_REG_LEN_08BIT, 0x87},
	{0x31E0, CRL_REG_LEN_08BIT, 0x01},
	{0x31E1, CRL_REG_LEN_08BIT, 0x9E},
	{0x31E2, CRL_REG_LEN_08BIT, 0x01},
	{0x31E5, CRL_REG_LEN_08BIT, 0x05},
	{0x31E6, CRL_REG_LEN_08BIT, 0x05},
	{0x31E7, CRL_REG_LEN_08BIT, 0x3A},
	{0x31E8, CRL_REG_LEN_08BIT, 0x3A},
	/* 0x04h */
	{0x3203, CRL_REG_LEN_08BIT, 0xC8},
	{0x3207, CRL_REG_LEN_08BIT, 0x54},
	{0x3213, CRL_REG_LEN_08BIT, 0x16},
	{0x3215, CRL_REG_LEN_08BIT, 0xF6},
	{0x321A, CRL_REG_LEN_08BIT, 0x14},
	{0x321B, CRL_REG_LEN_08BIT, 0x51},
	{0x3229, CRL_REG_LEN_08BIT, 0xE7},
	{0x322A, CRL_REG_LEN_08BIT, 0xF0},
	{0x322B, CRL_REG_LEN_08BIT, 0x10},
	{0x3231, CRL_REG_LEN_08BIT, 0xE7},
	{0x3232, CRL_REG_LEN_08BIT, 0xF0},
	{0x3233, CRL_REG_LEN_08BIT, 0x10},
	{0x323C, CRL_REG_LEN_08BIT, 0xE8},
	{0x323D, CRL_REG_LEN_08BIT, 0x70},
	{0x3243, CRL_REG_LEN_08BIT, 0x08},
	{0x3244, CRL_REG_LEN_08BIT, 0xE1},
	{0x3245, CRL_REG_LEN_08BIT, 0x10},
	{0x3247, CRL_REG_LEN_08BIT, 0xE7},
	{0x3248, CRL_REG_LEN_08BIT, 0x60},
	{0x3249, CRL_REG_LEN_08BIT, 0x1E},
	{0x324B, CRL_REG_LEN_08BIT, 0x00},
	{0x324C, CRL_REG_LEN_08BIT, 0x41},
	{0x3250, CRL_REG_LEN_08BIT, 0x30},
	{0x3251, CRL_REG_LEN_08BIT, 0x0A},
	{0x3252, CRL_REG_LEN_08BIT, 0xFF},
	{0x3253, CRL_REG_LEN_08BIT, 0xFF},
	{0x3254, CRL_REG_LEN_08BIT, 0xFF},
	{0x3255, CRL_REG_LEN_08BIT, 0x02},
	{0x3257, CRL_REG_LEN_08BIT, 0xF0},
	{0x325A, CRL_REG_LEN_08BIT, 0xA6},
	{0x325D, CRL_REG_LEN_08BIT, 0x14},
	{0x325E, CRL_REG_LEN_08BIT, 0x51},
	{0x3261, CRL_REG_LEN_08BIT, 0x61},
	{0x3266, CRL_REG_LEN_08BIT, 0x30},
	{0x3267, CRL_REG_LEN_08BIT, 0x05},
	{0x3275, CRL_REG_LEN_08BIT, 0xE7},
	{0x3281, CRL_REG_LEN_08BIT, 0xEA},
	{0x3282, CRL_REG_LEN_08BIT, 0x70},
	{0x3285, CRL_REG_LEN_08BIT, 0xFF},
	{0x328A, CRL_REG_LEN_08BIT, 0xF0},
	{0x328D, CRL_REG_LEN_08BIT, 0xB6},
	{0x328E, CRL_REG_LEN_08BIT, 0x40},
	{0x3290, CRL_REG_LEN_08BIT, 0x42},
	{0x3291, CRL_REG_LEN_08BIT, 0x51},
	{0x3292, CRL_REG_LEN_08BIT, 0x1E},
	{0x3294, CRL_REG_LEN_08BIT, 0xC4},
	{0x3295, CRL_REG_LEN_08BIT, 0x20},
	{0x3297, CRL_REG_LEN_08BIT, 0x50},
	{0x3298, CRL_REG_LEN_08BIT, 0x31},
	{0x3299, CRL_REG_LEN_08BIT, 0x1F},
	{0x329B, CRL_REG_LEN_08BIT, 0xC0},
	{0x329C, CRL_REG_LEN_08BIT, 0x60},
	{0x329E, CRL_REG_LEN_08BIT, 0x4C},
	{0x329F, CRL_REG_LEN_08BIT, 0x71},
	{0x32A0, CRL_REG_LEN_08BIT, 0x1F},
	{0x32A2, CRL_REG_LEN_08BIT, 0xB6},
	{0x32A3, CRL_REG_LEN_08BIT, 0xC0},
	{0x32A4, CRL_REG_LEN_08BIT, 0x0B},
	{0x32A9, CRL_REG_LEN_08BIT, 0x24},
	{0x32AA, CRL_REG_LEN_08BIT, 0x41},
	{0x32B0, CRL_REG_LEN_08BIT, 0x25},
	{0x32B1, CRL_REG_LEN_08BIT, 0x51},
	{0x32B7, CRL_REG_LEN_08BIT, 0x1C},
	{0x32B8, CRL_REG_LEN_08BIT, 0xC1},
	{0x32B9, CRL_REG_LEN_08BIT, 0x12},
	{0x32BE, CRL_REG_LEN_08BIT, 0x1D},
	{0x32BF, CRL_REG_LEN_08BIT, 0xD1},
	{0x32C0, CRL_REG_LEN_08BIT, 0x12},
	{0x32C2, CRL_REG_LEN_08BIT, 0xA8},
	{0x32C3, CRL_REG_LEN_08BIT, 0xC0},
	{0x32C4, CRL_REG_LEN_08BIT, 0x0A},
	{0x32C5, CRL_REG_LEN_08BIT, 0x1E},
	{0x32C6, CRL_REG_LEN_08BIT, 0x21},
	{0x32C9, CRL_REG_LEN_08BIT, 0xB0},
	{0x32CA, CRL_REG_LEN_08BIT, 0x40},
	{0x32CC, CRL_REG_LEN_08BIT, 0x26},
	{0x32CD, CRL_REG_LEN_08BIT, 0xA1},
	{0x32D0, CRL_REG_LEN_08BIT, 0xB6},
	{0x32D1, CRL_REG_LEN_08BIT, 0xC0},
	{0x32D2, CRL_REG_LEN_08BIT, 0x0B},
	{0x32D4, CRL_REG_LEN_08BIT, 0xE2},
	{0x32D5, CRL_REG_LEN_08BIT, 0x40},
	{0x32D8, CRL_REG_LEN_08BIT, 0x4E},
	{0x32D9, CRL_REG_LEN_08BIT, 0xA1},
	{0x32EC, CRL_REG_LEN_08BIT, 0xF0},
	/* 0x05h */
	{0x3303, CRL_REG_LEN_08BIT, 0x10},	/* repetation wdr */
	{0x3314, CRL_REG_LEN_08BIT, 0x08},
	{0x3316, CRL_REG_LEN_08BIT, 0x04},
	{0x3317, CRL_REG_LEN_08BIT, 0x04},
	{0x3318, CRL_REG_LEN_08BIT, 0x48},	/* PIC_SIZE = 1096 */
	{0x3319, CRL_REG_LEN_08BIT, 0x04},
	{0x334E, CRL_REG_LEN_08BIT, 0x3D},	/* INCL selection 27MHz */
	{0x334F, CRL_REG_LEN_08BIT, 0x01},
};

static struct crl_register_write_rep imx185_1952_1208_27MHZ[] = {
	/* 0x02h */
	{0x3005, CRL_REG_LEN_08BIT, 0x00},	/* ADBIT: 10/12 */
	{0x3007, CRL_REG_LEN_08BIT, 0x00},	/* WUXGA cropping */
	{0x3018, CRL_REG_LEN_08BIT, 0x28},
	{0x3019, CRL_REG_LEN_08BIT, 0x05},
	{0x301B, CRL_REG_LEN_08BIT, 0x53},
	{0x301C, CRL_REG_LEN_08BIT, 0x07},
	{0x301D, CRL_REG_LEN_08BIT, 0x08},
	{0x301E, CRL_REG_LEN_08BIT, 0x02},
	{0x3048, CRL_REG_LEN_08BIT, 0x33},
	{0x305C, CRL_REG_LEN_08BIT, 0x2c},	/* INCLKSEL default */
	{0x305E, CRL_REG_LEN_08BIT, 0x21},
	{0x3063, CRL_REG_LEN_08BIT, 0x54},
	/* Crop settings */
	{0x3038, CRL_REG_LEN_08BIT, 0x00},	/* WPV = 0 */
	{0x3039, CRL_REG_LEN_08BIT, 0x00},
	{0x303A, CRL_REG_LEN_08BIT, 0xC0},	/* WV = PIC_SIZE + 8 */
	{0x303B, CRL_REG_LEN_08BIT, 0x04},
	{0x303C, CRL_REG_LEN_08BIT, 0x00},	/* WPH = 0 */
	{0x303D, CRL_REG_LEN_08BIT, 0x00},
	{0x303E, CRL_REG_LEN_08BIT, 0x9C},
	{0x303F, CRL_REG_LEN_08BIT, 0x07},
	/* 0x03h */
	{0x311D, CRL_REG_LEN_08BIT, 0x0A},
	{0x3123, CRL_REG_LEN_08BIT, 0x0F},
	{0x3126, CRL_REG_LEN_08BIT, 0x00},
	{0x3147, CRL_REG_LEN_08BIT, 0x87},
	{0x31E0, CRL_REG_LEN_08BIT, 0x00},
	{0x31E1, CRL_REG_LEN_08BIT, 0x9E},
	{0x31E2, CRL_REG_LEN_08BIT, 0x01},
	{0x31E5, CRL_REG_LEN_08BIT, 0x05},
	{0x31E6, CRL_REG_LEN_08BIT, 0x05},
	{0x31E7, CRL_REG_LEN_08BIT, 0x3A},
	{0x31E8, CRL_REG_LEN_08BIT, 0x3A},
	/* 0x04h */
	{0x3203, CRL_REG_LEN_08BIT, 0xC8},
	{0x3207, CRL_REG_LEN_08BIT, 0x54},
	{0x3213, CRL_REG_LEN_08BIT, 0x16},
	{0x3215, CRL_REG_LEN_08BIT, 0xF6},
	{0x321A, CRL_REG_LEN_08BIT, 0x14},
	{0x321B, CRL_REG_LEN_08BIT, 0x51},
	{0x3229, CRL_REG_LEN_08BIT, 0xE7},
	{0x322A, CRL_REG_LEN_08BIT, 0xF0},
	{0x322B, CRL_REG_LEN_08BIT, 0x10},
	{0x3231, CRL_REG_LEN_08BIT, 0xE7},
	{0x3232, CRL_REG_LEN_08BIT, 0xF0},
	{0x3233, CRL_REG_LEN_08BIT, 0x10},
	{0x323C, CRL_REG_LEN_08BIT, 0xE8},
	{0x323D, CRL_REG_LEN_08BIT, 0x70},
	{0x3243, CRL_REG_LEN_08BIT, 0x08},
	{0x3244, CRL_REG_LEN_08BIT, 0xE1},
	{0x3245, CRL_REG_LEN_08BIT, 0x10},
	{0x3247, CRL_REG_LEN_08BIT, 0xE7},
	{0x3248, CRL_REG_LEN_08BIT, 0x60},
	{0x3249, CRL_REG_LEN_08BIT, 0x1E},
	{0x324B, CRL_REG_LEN_08BIT, 0x00},
	{0x324C, CRL_REG_LEN_08BIT, 0x41},
	{0x3250, CRL_REG_LEN_08BIT, 0x30},
	{0x3251, CRL_REG_LEN_08BIT, 0x0A},
	{0x3252, CRL_REG_LEN_08BIT, 0xFF},
	{0x3253, CRL_REG_LEN_08BIT, 0xFF},
	{0x3254, CRL_REG_LEN_08BIT, 0xFF},
	{0x3255, CRL_REG_LEN_08BIT, 0x02},
	{0x3257, CRL_REG_LEN_08BIT, 0xF0},
	{0x325A, CRL_REG_LEN_08BIT, 0xA6},
	{0x325D, CRL_REG_LEN_08BIT, 0x14},
	{0x325E, CRL_REG_LEN_08BIT, 0x51},
	{0x3261, CRL_REG_LEN_08BIT, 0x61},
	{0x3266, CRL_REG_LEN_08BIT, 0x30},
	{0x3267, CRL_REG_LEN_08BIT, 0x05},
	{0x3275, CRL_REG_LEN_08BIT, 0xE7},
	{0x3281, CRL_REG_LEN_08BIT, 0xEA},
	{0x3282, CRL_REG_LEN_08BIT, 0x70},
	{0x3285, CRL_REG_LEN_08BIT, 0xFF},
	{0x328A, CRL_REG_LEN_08BIT, 0xF0},
	{0x328D, CRL_REG_LEN_08BIT, 0xB6},
	{0x328E, CRL_REG_LEN_08BIT, 0x40},
	{0x3290, CRL_REG_LEN_08BIT, 0x42},
	{0x3291, CRL_REG_LEN_08BIT, 0x51},
	{0x3292, CRL_REG_LEN_08BIT, 0x1E},
	{0x3294, CRL_REG_LEN_08BIT, 0xC4},
	{0x3295, CRL_REG_LEN_08BIT, 0x20},
	{0x3297, CRL_REG_LEN_08BIT, 0x50},
	{0x3298, CRL_REG_LEN_08BIT, 0x31},
	{0x3299, CRL_REG_LEN_08BIT, 0x1F},
	{0x329B, CRL_REG_LEN_08BIT, 0xC0},
	{0x329C, CRL_REG_LEN_08BIT, 0x60},
	{0x329E, CRL_REG_LEN_08BIT, 0x4C},
	{0x329F, CRL_REG_LEN_08BIT, 0x71},
	{0x32A0, CRL_REG_LEN_08BIT, 0x1F},
	{0x32A2, CRL_REG_LEN_08BIT, 0xB6},
	{0x32A3, CRL_REG_LEN_08BIT, 0xC0},
	{0x32A4, CRL_REG_LEN_08BIT, 0x0B},
	{0x32A9, CRL_REG_LEN_08BIT, 0x24},
	{0x32AA, CRL_REG_LEN_08BIT, 0x41},
	{0x32B0, CRL_REG_LEN_08BIT, 0x25},
	{0x32B1, CRL_REG_LEN_08BIT, 0x51},
	{0x32B7, CRL_REG_LEN_08BIT, 0x1C},
	{0x32B8, CRL_REG_LEN_08BIT, 0xC1},
	{0x32B9, CRL_REG_LEN_08BIT, 0x12},
	{0x32BE, CRL_REG_LEN_08BIT, 0x1D},
	{0x32BF, CRL_REG_LEN_08BIT, 0xD1},
	{0x32C0, CRL_REG_LEN_08BIT, 0x12},
	{0x32C2, CRL_REG_LEN_08BIT, 0xA8},
	{0x32C3, CRL_REG_LEN_08BIT, 0xC0},
	{0x32C4, CRL_REG_LEN_08BIT, 0x0A},
	{0x32C5, CRL_REG_LEN_08BIT, 0x1E},
	{0x32C6, CRL_REG_LEN_08BIT, 0x21},
	{0x32C9, CRL_REG_LEN_08BIT, 0xB0},
	{0x32CA, CRL_REG_LEN_08BIT, 0x40},
	{0x32CC, CRL_REG_LEN_08BIT, 0x26},
	{0x32CD, CRL_REG_LEN_08BIT, 0xA1},
	{0x32D0, CRL_REG_LEN_08BIT, 0xB6},
	{0x32D1, CRL_REG_LEN_08BIT, 0xC0},
	{0x32D2, CRL_REG_LEN_08BIT, 0x0B},
	{0x32D4, CRL_REG_LEN_08BIT, 0xE2},
	{0x32D5, CRL_REG_LEN_08BIT, 0x40},
	{0x32D8, CRL_REG_LEN_08BIT, 0x4E},
	{0x32D9, CRL_REG_LEN_08BIT, 0xA1},
	{0x32EC, CRL_REG_LEN_08BIT, 0xF0},
	/* 0x05h */
	{0x3316, CRL_REG_LEN_08BIT, 0x04},
	{0x3317, CRL_REG_LEN_08BIT, 0x04},
	{0x3318, CRL_REG_LEN_08BIT, 0xB8},	/* PIC_SIZE = 1208 */
	{0x3319, CRL_REG_LEN_08BIT, 0x04},
	{0x334E, CRL_REG_LEN_08BIT, 0x3D},	/* INCL selection 27MHz */
	{0x334F, CRL_REG_LEN_08BIT, 0x01},
};

static struct crl_register_write_rep imx185_1952_1208_BUILD_IN_WDR_27MHZ[] = {
	/* 0x02h */
	{0x3005, CRL_REG_LEN_08BIT, 0x00},	/* ADBIT: 10/12 */
	{0x3007, CRL_REG_LEN_08BIT, 0x00},	/* WUXGA cropping */
	{0x300C, CRL_REG_LEN_08BIT, 0x02},
	{0x300F, CRL_REG_LEN_08BIT, 0x05},
	{0x3010, CRL_REG_LEN_08BIT, 0x38},
	{0x3012, CRL_REG_LEN_08BIT, 0x0F},
	{0x3018, CRL_REG_LEN_08BIT, 0x98},
	{0x3019, CRL_REG_LEN_08BIT, 0x08},
	{0x301B, CRL_REG_LEN_08BIT, 0x65},
	{0x301C, CRL_REG_LEN_08BIT, 0x04},
	{0x301D, CRL_REG_LEN_08BIT, 0x08},
	{0x301E, CRL_REG_LEN_08BIT, 0x02},
	{0x3048, CRL_REG_LEN_08BIT, 0x33},
	{0x3056, CRL_REG_LEN_08BIT, 0xC9},
	{0x3057, CRL_REG_LEN_08BIT, 0x33},
	{0x305C, CRL_REG_LEN_08BIT, 0x2c},	/* INCLKSEL default */
	{0x305E, CRL_REG_LEN_08BIT, 0x21},
	{0x3063, CRL_REG_LEN_08BIT, 0x54},
	{0x30E1, CRL_REG_LEN_08BIT, 0xE1},
	/* Crop settings */
	{0x3038, CRL_REG_LEN_08BIT, 0x00},	/* WPV = 0 */
	{0x3039, CRL_REG_LEN_08BIT, 0x00},
	{0x303A, CRL_REG_LEN_08BIT, 0xC9},
	{0x303B, CRL_REG_LEN_08BIT, 0x04},
	{0x303C, CRL_REG_LEN_08BIT, 0x00},
	{0x303D, CRL_REG_LEN_08BIT, 0x00},
	{0x303E, CRL_REG_LEN_08BIT, 0x9C},
	{0x303F, CRL_REG_LEN_08BIT, 0x07},
	/* 0x03h */
	{0x311D, CRL_REG_LEN_08BIT, 0x0A},
	{0x3123, CRL_REG_LEN_08BIT, 0x0F},
	{0x3126, CRL_REG_LEN_08BIT, 0xDF},
	{0x3147, CRL_REG_LEN_08BIT, 0x87},
	{0x31E0, CRL_REG_LEN_08BIT, 0x01},
	{0x31E1, CRL_REG_LEN_08BIT, 0x9E},
	{0x31E2, CRL_REG_LEN_08BIT, 0x01},
	{0x31E5, CRL_REG_LEN_08BIT, 0x05},
	{0x31E6, CRL_REG_LEN_08BIT, 0x05},
	{0x31E7, CRL_REG_LEN_08BIT, 0x3A},
	{0x31E8, CRL_REG_LEN_08BIT, 0x3A},
	/* 0x04h */
	{0x3203, CRL_REG_LEN_08BIT, 0xC8},
	{0x3207, CRL_REG_LEN_08BIT, 0x54},
	{0x3213, CRL_REG_LEN_08BIT, 0x16},
	{0x3215, CRL_REG_LEN_08BIT, 0xF6},
	{0x321A, CRL_REG_LEN_08BIT, 0x14},
	{0x321B, CRL_REG_LEN_08BIT, 0x51},
	{0x3229, CRL_REG_LEN_08BIT, 0xE7},
	{0x322A, CRL_REG_LEN_08BIT, 0xF0},
	{0x322B, CRL_REG_LEN_08BIT, 0x10},
	{0x3231, CRL_REG_LEN_08BIT, 0xE7},
	{0x3232, CRL_REG_LEN_08BIT, 0xF0},
	{0x3233, CRL_REG_LEN_08BIT, 0x10},
	{0x323C, CRL_REG_LEN_08BIT, 0xE8},
	{0x323D, CRL_REG_LEN_08BIT, 0x70},
	{0x3243, CRL_REG_LEN_08BIT, 0x08},
	{0x3244, CRL_REG_LEN_08BIT, 0xE1},
	{0x3245, CRL_REG_LEN_08BIT, 0x10},
	{0x3247, CRL_REG_LEN_08BIT, 0xE7},
	{0x3248, CRL_REG_LEN_08BIT, 0x60},
	{0x3249, CRL_REG_LEN_08BIT, 0x1E},
	{0x324B, CRL_REG_LEN_08BIT, 0x00},
	{0x324C, CRL_REG_LEN_08BIT, 0x41},
	{0x3250, CRL_REG_LEN_08BIT, 0x30},
	{0x3251, CRL_REG_LEN_08BIT, 0x0A},
	{0x3252, CRL_REG_LEN_08BIT, 0xFF},
	{0x3253, CRL_REG_LEN_08BIT, 0xFF},
	{0x3254, CRL_REG_LEN_08BIT, 0xFF},
	{0x3255, CRL_REG_LEN_08BIT, 0x02},
	{0x3257, CRL_REG_LEN_08BIT, 0xF0},
	{0x325A, CRL_REG_LEN_08BIT, 0xA6},
	{0x325D, CRL_REG_LEN_08BIT, 0x14},
	{0x325E, CRL_REG_LEN_08BIT, 0x51},
	{0x3261, CRL_REG_LEN_08BIT, 0x61},
	{0x3266, CRL_REG_LEN_08BIT, 0x30},
	{0x3267, CRL_REG_LEN_08BIT, 0x05},
	{0x3275, CRL_REG_LEN_08BIT, 0xE7},
	{0x3281, CRL_REG_LEN_08BIT, 0xEA},
	{0x3282, CRL_REG_LEN_08BIT, 0x70},
	{0x3285, CRL_REG_LEN_08BIT, 0xFF},
	{0x328A, CRL_REG_LEN_08BIT, 0xF0},
	{0x328D, CRL_REG_LEN_08BIT, 0xB6},
	{0x328E, CRL_REG_LEN_08BIT, 0x40},
	{0x3290, CRL_REG_LEN_08BIT, 0x42},
	{0x3291, CRL_REG_LEN_08BIT, 0x51},
	{0x3292, CRL_REG_LEN_08BIT, 0x1E},
	{0x3294, CRL_REG_LEN_08BIT, 0xC4},
	{0x3295, CRL_REG_LEN_08BIT, 0x20},
	{0x3297, CRL_REG_LEN_08BIT, 0x50},
	{0x3298, CRL_REG_LEN_08BIT, 0x31},
	{0x3299, CRL_REG_LEN_08BIT, 0x1F},
	{0x329B, CRL_REG_LEN_08BIT, 0xC0},
	{0x329C, CRL_REG_LEN_08BIT, 0x60},
	{0x329E, CRL_REG_LEN_08BIT, 0x4C},
	{0x329F, CRL_REG_LEN_08BIT, 0x71},
	{0x32A0, CRL_REG_LEN_08BIT, 0x1F},
	{0x32A2, CRL_REG_LEN_08BIT, 0xB6},
	{0x32A3, CRL_REG_LEN_08BIT, 0xC0},
	{0x32A4, CRL_REG_LEN_08BIT, 0x0B},
	{0x32A9, CRL_REG_LEN_08BIT, 0x24},
	{0x32AA, CRL_REG_LEN_08BIT, 0x41},
	{0x32B0, CRL_REG_LEN_08BIT, 0x25},
	{0x32B1, CRL_REG_LEN_08BIT, 0x51},
	{0x32B7, CRL_REG_LEN_08BIT, 0x1C},
	{0x32B8, CRL_REG_LEN_08BIT, 0xC1},
	{0x32B9, CRL_REG_LEN_08BIT, 0x12},
	{0x32BE, CRL_REG_LEN_08BIT, 0x1D},
	{0x32BF, CRL_REG_LEN_08BIT, 0xD1},
	{0x32C0, CRL_REG_LEN_08BIT, 0x12},
	{0x32C2, CRL_REG_LEN_08BIT, 0xA8},
	{0x32C3, CRL_REG_LEN_08BIT, 0xC0},
	{0x32C4, CRL_REG_LEN_08BIT, 0x0A},
	{0x32C5, CRL_REG_LEN_08BIT, 0x1E},
	{0x32C6, CRL_REG_LEN_08BIT, 0x21},
	{0x32C9, CRL_REG_LEN_08BIT, 0xB0},
	{0x32CA, CRL_REG_LEN_08BIT, 0x40},
	{0x32CC, CRL_REG_LEN_08BIT, 0x26},
	{0x32CD, CRL_REG_LEN_08BIT, 0xA1},
	{0x32D0, CRL_REG_LEN_08BIT, 0xB6},
	{0x32D1, CRL_REG_LEN_08BIT, 0xC0},
	{0x32D2, CRL_REG_LEN_08BIT, 0x0B},
	{0x32D4, CRL_REG_LEN_08BIT, 0xE2},
	{0x32D5, CRL_REG_LEN_08BIT, 0x40},
	{0x32D8, CRL_REG_LEN_08BIT, 0x4E},
	{0x32D9, CRL_REG_LEN_08BIT, 0xA1},
	{0x32EC, CRL_REG_LEN_08BIT, 0xF0},
	/* 0x05h */
	{0x3303, CRL_REG_LEN_08BIT, 0x00},
	{0x3314, CRL_REG_LEN_08BIT, 0x08},
	{0x3316, CRL_REG_LEN_08BIT, 0x04},
	{0x3317, CRL_REG_LEN_08BIT, 0x04},
	{0x3318, CRL_REG_LEN_08BIT, 0xB8},	/* PIC_SIZE = 1208 */
	{0x3319, CRL_REG_LEN_08BIT, 0x04},
	{0x334E, CRL_REG_LEN_08BIT, 0x3D},	/* INCL selection 27MHz */
	{0x334F, CRL_REG_LEN_08BIT, 0x01},
};

static struct crl_register_write_rep imx185_streamon_regs[] = {
	{IMX185_REG_STANDBY, CRL_REG_LEN_08BIT, 0x00},
	{0x00, CRL_REG_LEN_DELAY, 30, 0x00}, /* Delay 30ms */
	{IMX185_REG_XMSTA, CRL_REG_LEN_08BIT, 0x00},
	{0x00, CRL_REG_LEN_DELAY, 30, 0x00}, /* Delay 30ms */
};

static struct crl_register_write_rep imx185_streamoff_regs[] = {
	{IMX185_REG_STANDBY, CRL_REG_LEN_08BIT, 0x01},
	{0x00, CRL_REG_LEN_DELAY, 30, 0x00}, /* Delay 30ms */
	{IMX185_REG_XMSTA, CRL_REG_LEN_08BIT, 0x01},
	{0x00, CRL_REG_LEN_DELAY, 30, 0x00}, /* Delay 30ms */
};

static struct crl_arithmetic_ops imx185_hflip_ops[] = {
	{
		.op = CRL_BITWISE_LSHIFT,
		.operand.entity_val = 1,
	}
};

/* shs1 = fll - exposure -1 */
static struct crl_arithmetic_ops imx185_shs1_lsb_ops[] = {
	{
		.op = CRL_SUBTRACT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CTRL_VAL,
		.operand.entity_val = V4L2_CID_FRAME_LENGTH_LINES,
	},
	{
		.op = CRL_SUBTRACT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
		.operand.entity_val = 1,
	}
};

static struct crl_arithmetic_ops imx185_shs1_msb_ops[] = {
	{
		.op = CRL_SUBTRACT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CTRL_VAL,
		.operand.entity_val = V4L2_CID_FRAME_LENGTH_LINES,
	},
	{
		.op = CRL_SUBTRACT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
		.operand.entity_val = 1,
	},
	{
		.op = CRL_BITWISE_RSHIFT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
		.operand.entity_val = 8,
	}
};

static struct crl_arithmetic_ops imx185_shs1_hsb_ops[] = {
	{
		.op = CRL_SUBTRACT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CTRL_VAL,
		.operand.entity_val = V4L2_CID_FRAME_LENGTH_LINES,
	},
	{
		.op = CRL_SUBTRACT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
		.operand.entity_val = 1,
	},
	{
		.op = CRL_BITWISE_RSHIFT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
		.operand.entity_val = 16,
	}
};

/* shs2 = fll - exposure * 16 -1 */
static struct crl_arithmetic_ops imx185_shs2_lsb_ops[] = {
	{
		.op = CRL_BITWISE_LSHIFT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
		.operand.entity_val = 4,
	},
	{
		.op = CRL_SUBTRACT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CTRL_VAL,
		.operand.entity_val = V4L2_CID_FRAME_LENGTH_LINES,
	},
	{
		.op = CRL_SUBTRACT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
		.operand.entity_val = 1,
	}
};

static struct crl_arithmetic_ops imx185_shs2_msb_ops[] = {
	{
		.op = CRL_BITWISE_LSHIFT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
		.operand.entity_val = 4,
	},
	{
		.op = CRL_SUBTRACT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CTRL_VAL,
		.operand.entity_val = V4L2_CID_FRAME_LENGTH_LINES,
	},
	{
		.op = CRL_SUBTRACT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
		.operand.entity_val = 1,
	},
	{
		.op = CRL_BITWISE_RSHIFT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
		.operand.entity_val = 8,
	}
};

static struct crl_arithmetic_ops imx185_shs2_hsb_ops[] = {
	{
		.op = CRL_BITWISE_LSHIFT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
		.operand.entity_val = 4,
	},
	{
		.op = CRL_SUBTRACT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CTRL_VAL,
		.operand.entity_val = V4L2_CID_FRAME_LENGTH_LINES,
	},
	{
		.op = CRL_SUBTRACT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
		.operand.entity_val = 1,
	},
	{
		.op = CRL_BITWISE_RSHIFT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
		.operand.entity_val = 16,
	}
};

static struct crl_arithmetic_ops imx185_fll_msb_ops[] = {
	{
		.op = CRL_BITWISE_RSHIFT,
		.operand.entity_val = 8,
	}
};

static struct crl_arithmetic_ops imx185_llp_msb_ops[] = {
	{
		.op = CRL_BITWISE_RSHIFT,
		.operand.entity_val = 8,
	}
};

static struct crl_arithmetic_ops imx185_fll_hsb_ops[] = {
	{
		.op = CRL_BITWISE_RSHIFT,
		.operand.entity_val = 16,
	}
};

static struct crl_dynamic_register_access imx185_h_flip_regs[] = {
	{
		.address = 0x3007,
		.len = CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE,
		.ops_items = ARRAY_SIZE(imx185_hflip_ops),
		.ops = imx185_hflip_ops,
		.mask = 0x2,
	}
};

static struct crl_dynamic_register_access imx185_v_flip_regs[] = {
	{
		.address = 0x3007,
		.len = CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE,
		.ops_items = 0,
		.ops = 0,
		.mask = 0x1,
	}
};

static struct crl_dynamic_register_access imx185_ana_gain_global_regs[] = {
	{
		.address = 0x3014,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xff,
	 }
};

static struct crl_dynamic_register_access imx185_shs_regs[] = {
	/*
	 * Use 8bits access since 24bits or 32bits access will fail
	 * TODO: root cause the 24bits and 32bits access issues
	 */
	{
		.address = 0x3020,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = ARRAY_SIZE(imx185_shs1_lsb_ops),
		.ops = imx185_shs1_lsb_ops,
		.mask = 0xff,
	},
	{
		.address = 0x3021,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = ARRAY_SIZE(imx185_shs1_msb_ops),
		.ops = imx185_shs1_msb_ops,
		.mask = 0xff,
	},
	{
		.address = 0x3022,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = ARRAY_SIZE(imx185_shs1_hsb_ops),
		.ops = imx185_shs1_hsb_ops,
		.mask = 0x1,
	},
	{
		.address = 0x3023,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = ARRAY_SIZE(imx185_shs2_lsb_ops),
		.ops = imx185_shs2_lsb_ops,
		.mask = 0xff,
	},
	{
		.address = 0x3024,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = ARRAY_SIZE(imx185_shs2_msb_ops),
		.ops = imx185_shs2_msb_ops,
		.mask = 0xff,
	},
	{
		.address = 0x3025,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = ARRAY_SIZE(imx185_shs2_hsb_ops),
		.ops = imx185_shs2_hsb_ops,
		.mask = 0x1,
	}
};

static struct crl_dynamic_register_access imx185_fll_regs[] = {
	/*
	 * Use 8bits access since 24bits or 32bits access will fail
	 * TODO: root cause the 24bits and 32bits access issues
	 */
	{
		.address = 0x3018,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xff,
	},
	{
		.address = 0x3019,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = ARRAY_SIZE(imx185_fll_msb_ops),
		.ops = imx185_fll_msb_ops,
		.mask = 0xff,
	},
	{
		.address = 0x301a,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = ARRAY_SIZE(imx185_fll_hsb_ops),
		.ops = imx185_fll_hsb_ops,
		.mask = 0x1,
	},
};

static struct crl_dynamic_register_access imx185_llp_regs[] = {
	{
		.address = 0x301b,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xff,
	},
	{
		.address = 0x301c,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = ARRAY_SIZE(imx185_llp_msb_ops),
		.ops = imx185_llp_msb_ops,
		.mask = 0xff,
	},
};

/* ctrl-val == 1 ? 1 * 0x02 : 0 * 0x02 -> 2 and 0 */
static struct crl_arithmetic_ops imx185_wdr_switch_r300c_ops[] = {
	{
		.op = CRL_MULTIPLY,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
		.operand.entity_val = 0x02,
	}
};

/* ctrl-val == 1 ? (1 * 0x04 + 0x1) : (0 * 0x04 + 0x1) -> 0x05 and 0x01 */
static struct crl_arithmetic_ops imx185_wdr_switch_r300f_ops[] = {
	{
		.op = CRL_MULTIPLY,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
		.operand.entity_val = 0x04,
	},
	{
		.op = CRL_ADD,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
		.operand.entity_val = 0x01,
	}
};

/* ctrl-val == 1 ? (0x39 - 1 * 0x01) : (0x39 - 0 * 0x01) -> 0x38 and 0x39 */
static struct crl_arithmetic_ops imx185_wdr_switch_r3010_ops[] = {
	{
		.op = CRL_MULTIPLY,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
		.operand.entity_val = 0x01,
	},
	{
		.op = CRL_SUBTRACT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
		.operand.entity_val = 0x39,
	}
};

/* ctrl-val == 1 ? (0x50 - 1 * 0x41) : (0x50 - 0 * 0x41) -> 0x0f and 0x50 */
static struct crl_arithmetic_ops imx185_wdr_switch_r3012_ops[] = {
	{
		.op = CRL_MULTIPLY,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
		.operand.entity_val = 0x41,
	},
	{
		.op = CRL_SUBTRACT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
		.operand.entity_val = 0x50,
	}
};

static struct crl_dynamic_register_access imx185_wdr_switch_regs[] = {
	{ 0x300c, CRL_REG_LEN_08BIT, 0xff,
		ARRAY_SIZE(imx185_wdr_switch_r300c_ops),
		imx185_wdr_switch_r300c_ops, 0 },
	{ 0x300f, CRL_REG_LEN_08BIT, 0xff,
		ARRAY_SIZE(imx185_wdr_switch_r300f_ops),
		imx185_wdr_switch_r300f_ops, 0 },
	{ 0x3010, CRL_REG_LEN_08BIT, 0xff,
		ARRAY_SIZE(imx185_wdr_switch_r3010_ops),
		imx185_wdr_switch_r3010_ops, 0 },
	{ 0x3012, CRL_REG_LEN_08BIT, 0xff,
		ARRAY_SIZE(imx185_wdr_switch_r3012_ops),
		imx185_wdr_switch_r3012_ops, 0 },
};

/* Needed for acpi support for runtime detection */
static struct crl_sensor_detect_config imx185_sensor_detect_regset[] = {
	{
		.reg = { 0x3385, CRL_REG_LEN_08BIT, 0x000000ff },
		.width = 7,
	},
	{
		.reg = { 0x3384, CRL_REG_LEN_08BIT, 0x000000ff },
		.width = 7,
	}
};

static struct crl_pll_configuration imx185_pll_configurations[] = {
	{
		.input_clk = 27000000,
		.op_sys_clk = 56250000,
		.bitsperpixel = 10,
		.pixel_rate_csi = 45000000,
		.pixel_rate_pa = 45000000, /* pixel_rate = MIPICLK*2 *4/10 */
		.csi_lanes = 4,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = ARRAY_SIZE(imx185_pll_111mbps),
		.pll_regs = imx185_pll_111mbps,
	},
	{
		.input_clk = 27000000,
		.op_sys_clk = 112500000,
		.bitsperpixel = 10,
		.pixel_rate_csi = 90000000,
		.pixel_rate_pa = 90000000,
		.csi_lanes = 4,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = ARRAY_SIZE(imx185_pll_222mbps),
		.pll_regs = imx185_pll_222mbps,
	},
	{
		.input_clk = 27000000,
		.op_sys_clk = 112500000,
		.bitsperpixel = 12,
		.pixel_rate_csi = 75000000,
		.pixel_rate_pa = 75000000,
		.csi_lanes = 4,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = ARRAY_SIZE(imx185_pll_222mbps),
		.pll_regs = imx185_pll_222mbps,
	},
	{
		.input_clk = 27000000,
		.op_sys_clk = 225000000,
		.bitsperpixel = 12,
		.pixel_rate_csi = 150000000,
		.pixel_rate_pa = 150000000,
		.csi_lanes = 4,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = ARRAY_SIZE(imx185_pll_445mbps),
		.pll_regs = imx185_pll_445mbps,
	}
};

static struct crl_subdev_rect_rep imx185_1952_1208_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1952,
		.in_rect.height = 1208,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1952,
		.out_rect.height = 1208,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1952,
		.in_rect.height = 1208,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1952,
		.out_rect.height = 1208,
	}
};

static struct crl_subdev_rect_rep imx185_1952_1096_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1952,
		.in_rect.height = 1208,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1952,
		.out_rect.height = 1208,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1952,
		.in_rect.height = 1208,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1952,
		.out_rect.height = 1096,
	}
};

static struct crl_subdev_rect_rep imx185_1312_728_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1952,
		.in_rect.height = 1208,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1952,
		.out_rect.height = 1208,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1952,
		.in_rect.height = 1208,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1312,
		.out_rect.height = 728,
	 }
};

static struct crl_mode_rep imx185_modes[] = {
	{
		.sd_rects_items = ARRAY_SIZE(imx185_1952_1208_rects),
		.sd_rects = imx185_1952_1208_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 1952,
		.height = 1208,
		.min_llp = 2250,
		.min_fll = 1333,
		.comp_items = 1,
		.ctrl_data = &ctrl_data_modes[0],
		.mode_regs_items = ARRAY_SIZE(imx185_1952_1208_27MHZ),
		.mode_regs = imx185_1952_1208_27MHZ,
	},
	{
		.sd_rects_items = ARRAY_SIZE(imx185_1952_1208_rects),
		.sd_rects = imx185_1952_1208_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 1952,
		.height = 1208,
		.min_llp = 2250,
		.min_fll = 1333,
		.comp_items = 1,
		.ctrl_data = &ctrl_data_modes[1],
		.mode_regs_items =
				ARRAY_SIZE(imx185_1952_1208_BUILD_IN_WDR_27MHZ),
		.mode_regs = imx185_1952_1208_BUILD_IN_WDR_27MHZ,
	},
	{
		.sd_rects_items = ARRAY_SIZE(imx185_1952_1096_rects),
		.sd_rects = imx185_1952_1096_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 1952,
		.height = 1096,
		.min_llp = 2200,
		.min_fll = 1135,
		.comp_items = 1,
		.ctrl_data = &ctrl_data_modes[0],
		.mode_regs_items = ARRAY_SIZE(imx185_1952_1096_27MHZ),
		.mode_regs = imx185_1952_1096_27MHZ,
	},
	{
		.sd_rects_items = ARRAY_SIZE(imx185_1952_1096_rects),
		.sd_rects = imx185_1952_1096_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 1952,
		.height = 1096,
		.min_llp = 2200,
		.min_fll = 1135,
		.comp_items = 1,
		.ctrl_data = &ctrl_data_modes[1],
		.mode_regs_items =
				ARRAY_SIZE(imx185_1952_1096_BUILD_IN_WDR_27MHZ),
		.mode_regs = imx185_1952_1096_BUILD_IN_WDR_27MHZ,
	},
	{
		.sd_rects_items = ARRAY_SIZE(imx185_1312_728_rects),
		.sd_rects = imx185_1312_728_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 1312,
		.height = 728,
		.min_llp = 1300,
		.min_fll = 787,
		.comp_items = 1,
		.ctrl_data = &ctrl_data_modes[0],
		.mode_regs_items = ARRAY_SIZE(imx185_1312_728_27MHZ_CROPPING),
		.mode_regs = imx185_1312_728_27MHZ_CROPPING,
	 }
};

static struct crl_sensor_subdev_config imx185_sensor_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.name = "imx185 binner",
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "imx185 pixel array",
	}
};

static struct crl_sensor_limits imx185_sensor_limits = {
	.x_addr_min = 0,
	.y_addr_min = 0,
	.x_addr_max = 1952,
	.y_addr_max = 1208,
	.min_frame_length_lines = 320,
	.max_frame_length_lines = 65535,
	.min_line_length_pixels = 380,
	.max_line_length_pixels = 32752,
};

static struct crl_flip_data imx185_flip_configurations[] = {
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
	}
};

static struct crl_csi_data_fmt imx185_crl_csi_data_fmt[] = {
	{
		.code = MEDIA_BUS_FMT_SGRBG10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.bits_per_pixel = 10,
		.regs_items = ARRAY_SIZE(imx185_fmt_raw10),
		.regs = imx185_fmt_raw10,
	},
	{
		.code = MEDIA_BUS_FMT_SRGGB10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,
		.bits_per_pixel = 10,
		.regs_items = ARRAY_SIZE(imx185_fmt_raw10),
		.regs = imx185_fmt_raw10,
	},
	{
		.code = MEDIA_BUS_FMT_SBGGR10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_BGGR,
		.bits_per_pixel = 10,
		.regs_items = ARRAY_SIZE(imx185_fmt_raw10),
		.regs = imx185_fmt_raw10,
	},
	{
		.code = MEDIA_BUS_FMT_SGBRG10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_GBRG,
		.bits_per_pixel = 10,
		.regs_items = ARRAY_SIZE(imx185_fmt_raw10),
		.regs = imx185_fmt_raw10,
	},
	{
		.code = MEDIA_BUS_FMT_SGRBG12_1X12,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.bits_per_pixel = 12,
		.regs_items = ARRAY_SIZE(imx185_fmt_raw12),
		.regs = imx185_fmt_raw12,
	},
	{
		.code = MEDIA_BUS_FMT_SRGGB12_1X12,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,
		.bits_per_pixel = 12,
		.regs_items = ARRAY_SIZE(imx185_fmt_raw12),
		.regs = imx185_fmt_raw12,
	},
	{
		.code = MEDIA_BUS_FMT_SBGGR12_1X12,
		.pixel_order = CRL_PIXEL_ORDER_BGGR,
		.bits_per_pixel = 12,
		.regs_items = ARRAY_SIZE(imx185_fmt_raw12),
		.regs = imx185_fmt_raw12,
	},
	{
		.code = MEDIA_BUS_FMT_SGBRG12_1X12,
		.pixel_order = CRL_PIXEL_ORDER_GBRG,
		.bits_per_pixel = 12,
		.regs_items = ARRAY_SIZE(imx185_fmt_raw12),
		.regs = imx185_fmt_raw12,
	}
};

static struct crl_v4l2_ctrl imx185_v4l2_ctrls[] = {
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
		.regs_items = ARRAY_SIZE(imx185_h_flip_regs),
		.regs = imx185_h_flip_regs,
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
		.regs_items = ARRAY_SIZE(imx185_v_flip_regs),
		.regs = imx185_v_flip_regs,
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
		.data.std_data.max = 160,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx185_ana_gain_global_regs),
		.regs = imx185_ana_gain_global_regs,
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
		.data.std_data.max = IMX185_MAX_SHS1,
		.data.std_data.step = 1,
		.data.std_data.def = 0x47,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx185_shs_regs),
		.regs = imx185_shs_regs,
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
		.data.std_data.max = IMX185_VMAX,
		.data.std_data.step = 1,
		.data.std_data.def = 0x465,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx185_fll_regs),
		.regs = imx185_fll_regs,
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
		.data.std_data.min = 0x898,
		.data.std_data.max = IMX185_HMAX,
		.data.std_data.step = 1,
		.data.std_data.def = 0x898,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx185_llp_regs),
		.regs = imx185_llp_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
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
		.regs_items = ARRAY_SIZE(imx185_wdr_switch_regs),
		.regs = imx185_wdr_switch_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
	},
};

static struct crl_arithmetic_ops imx185_frame_desc_width_ops[] = {
	{
		 .op = CRL_ASSIGNMENT,
		 .operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_VAR_REF,
		 .operand.entity_val = CRL_VAR_REF_OUTPUT_WIDTH,
	},
};

static struct crl_arithmetic_ops imx185_frame_desc_height_ops[] = {
	{
		 .op = CRL_ASSIGNMENT,
		 .operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
		 .operand.entity_val = 1,
	},
};

static struct crl_frame_desc imx185_frame_desc[] = {
	{
		.flags.entity_val = 0,
		.bpp.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_VAR_REF,
		.bpp.entity_val = CRL_VAR_REF_BITSPERPIXEL,
		.pixelcode.entity_val = MEDIA_BUS_FMT_FIXED,
		.length.entity_val = 0,
		.start_line.entity_val = 0,
		.start_pixel.entity_val = 0,
		.width = {
			 .ops_items = ARRAY_SIZE(imx185_frame_desc_width_ops),
			 .ops = imx185_frame_desc_width_ops,
			 },
		.height = {
			  .ops_items = ARRAY_SIZE(imx185_frame_desc_height_ops),
			  .ops = imx185_frame_desc_height_ops,
			  },
		.csi2_channel.entity_val = 0,
		.csi2_data_type.entity_val = 0x12,
	},
};

/* Power items, they are enabled in the order they are listed here */
static struct crl_power_seq_entity imx185_power_items[] = {
	{
		.type = CRL_POWER_ETY_CLK_FRAMEWORK,
		.val = 27000000,
	},
	{
		.type = CRL_POWER_ETY_GPIO_FROM_PDATA,
		.val = 1,
	},
};

struct crl_sensor_configuration imx185_crl_configuration = {

	.power_items = ARRAY_SIZE(imx185_power_items),
	.power_entities = imx185_power_items,

	.powerup_regs_items = ARRAY_SIZE(imx185_powerup_standby),
	.powerup_regs = imx185_powerup_standby,

	.poweroff_regs_items = 0,
	.poweroff_regs = 0,

	.id_reg_items = ARRAY_SIZE(imx185_sensor_detect_regset),
	.id_regs = imx185_sensor_detect_regset,

	.subdev_items = ARRAY_SIZE(imx185_sensor_subdevs),
	.subdevs = imx185_sensor_subdevs,

	.sensor_limits = &imx185_sensor_limits,

	.pll_config_items = ARRAY_SIZE(imx185_pll_configurations),
	.pll_configs = imx185_pll_configurations,

	.modes_items = ARRAY_SIZE(imx185_modes),
	.modes = imx185_modes,

	.streamon_regs_items = ARRAY_SIZE(imx185_streamon_regs),
	.streamon_regs = imx185_streamon_regs,

	.streamoff_regs_items = ARRAY_SIZE(imx185_streamoff_regs),
	.streamoff_regs = imx185_streamoff_regs,

	.v4l2_ctrls_items = ARRAY_SIZE(imx185_v4l2_ctrls),
	.v4l2_ctrl_bank = imx185_v4l2_ctrls,

	.csi_fmts_items = ARRAY_SIZE(imx185_crl_csi_data_fmt),
	.csi_fmts = imx185_crl_csi_data_fmt,

	.flip_items = ARRAY_SIZE(imx185_flip_configurations),
	.flip_data = imx185_flip_configurations,

	.frame_desc_entries = ARRAY_SIZE(imx185_frame_desc),
	.frame_desc_type = CRL_V4L2_MBUS_FRAME_DESC_TYPE_CSI2,
	.frame_desc = imx185_frame_desc,
};

#endif  /* __CRLMODULE_IMX185_CONFIGURATION_H_ */
