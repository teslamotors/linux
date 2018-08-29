/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2016 - 2018 Intel Corporation
 *
 * Author: Yuning Pu <yuning.pu@intel.com>
 *
 */

#ifndef __CRLMODULE_IMX274_CONFIGURATION_H_
#define __CRLMODULE_IMX274_CONFIGURATION_H_

#include "crlmodule-sensor-ds.h"

#define IMX274_REG_STANDBY              0x3000  /* STBLOGIC STBMIPI STBDV */

#define IMX274_HMAX                      65535
#define IMX274_VMAX                      1048575
#define IMX274_MAX_SHS1                  65535
#define IMX274_MAX_SHS2                  65535
#define IMX274_MAX_RHS1                  65535

/* imx274 mode standby cancel sequence */
static struct crl_register_write_rep imx274_powerup_standby[] = {
	{IMX274_REG_STANDBY, CRL_REG_LEN_08BIT, 0x12},
};

/* 1440Mbps for imx274 4K 30fps 1080p 60fps */
static struct crl_register_write_rep imx274_pll_1440mbps[] = {
	{0x3120, CRL_REG_LEN_08BIT, 0xF0},
	{0x3121, CRL_REG_LEN_08BIT, 0x00},
	{0x3122, CRL_REG_LEN_08BIT, 0x02},
	{0x3129, CRL_REG_LEN_08BIT, 0x9C},
	{0x312A, CRL_REG_LEN_08BIT, 0x02},
	{0x312D, CRL_REG_LEN_08BIT, 0x02},
	{0x310B, CRL_REG_LEN_08BIT, 0x00}, /* PLL standby */
	{0x304C, CRL_REG_LEN_08BIT, 0x00},	/* PLSTMG01 */
	{0x304D, CRL_REG_LEN_08BIT, 0x03},
	{0x331C, CRL_REG_LEN_08BIT, 0x1A},
	{0x331D, CRL_REG_LEN_08BIT, 0x00},
	{0x3502, CRL_REG_LEN_08BIT, 0x02},
	{0x3529, CRL_REG_LEN_08BIT, 0x0E},
	{0x352A, CRL_REG_LEN_08BIT, 0x0E},
	{0x352B, CRL_REG_LEN_08BIT, 0x0E},
	{0x3538, CRL_REG_LEN_08BIT, 0x0E},
	{0x3539, CRL_REG_LEN_08BIT, 0x0E},
	{0x3553, CRL_REG_LEN_08BIT, 0x00},
	{0x357D, CRL_REG_LEN_08BIT, 0x05},
	{0x357F, CRL_REG_LEN_08BIT, 0x05},
	{0x3581, CRL_REG_LEN_08BIT, 0x04},
	{0x3583, CRL_REG_LEN_08BIT, 0x76},
	{0x3587, CRL_REG_LEN_08BIT, 0x01},
	{0x35BB, CRL_REG_LEN_08BIT, 0x0E},
	{0x35BC, CRL_REG_LEN_08BIT, 0x0E},
	{0x35BD, CRL_REG_LEN_08BIT, 0x0E},
	{0x35BE, CRL_REG_LEN_08BIT, 0x0E},
	{0x35BF, CRL_REG_LEN_08BIT, 0x0E},
	{0x366E, CRL_REG_LEN_08BIT, 0x00},
	{0x366F, CRL_REG_LEN_08BIT, 0x00},
	{0x3670, CRL_REG_LEN_08BIT, 0x00},
	{0x3671, CRL_REG_LEN_08BIT, 0x00},	/* PLSTMG01 */
	{0x30EE, CRL_REG_LEN_08BIT, 0x01},
	{0x3304, CRL_REG_LEN_08BIT, 0x32},	/* For Mipi */
	{0x3305, CRL_REG_LEN_08BIT, 0x00},
	{0x3306, CRL_REG_LEN_08BIT, 0x32},
	{0x3307, CRL_REG_LEN_08BIT, 0x00},
	{0x3590, CRL_REG_LEN_08BIT, 0x32},
	{0x3591, CRL_REG_LEN_08BIT, 0x00},
	{0x3686, CRL_REG_LEN_08BIT, 0x32},
	{0x3687, CRL_REG_LEN_08BIT, 0x00},
};

static struct crl_register_write_rep imx274_3864_2202_RAW12_NORMAL[] = {
	{0x30E2, CRL_REG_LEN_08BIT, 0x00},	/* VCUTMODE */
	{0x3130, CRL_REG_LEN_08BIT, 0xAA},	/* WRITE_VSIZE */
	{0x3131, CRL_REG_LEN_08BIT, 0x08},
	{0x3132, CRL_REG_LEN_08BIT, 0x9A},	/* Y_OUT_SIZE */
	{0x3133, CRL_REG_LEN_08BIT, 0x08},
	{0x3004, CRL_REG_LEN_08BIT, 0x01},	/* MDSEL */
	{0x3005, CRL_REG_LEN_08BIT, 0x07},
	{0x3006, CRL_REG_LEN_08BIT, 0x00},
	{0x3007, CRL_REG_LEN_08BIT, 0x02},
	{0x3A41, CRL_REG_LEN_08BIT, 0x10},	/* MDSEL5 */
	{0x3342, CRL_REG_LEN_08BIT, 0xFF},	/* MDPLS01 */
	{0x3343, CRL_REG_LEN_08BIT, 0x01},
	{0x3344, CRL_REG_LEN_08BIT, 0xFF},
	{0x3345, CRL_REG_LEN_08BIT, 0x01},
	{0x3528, CRL_REG_LEN_08BIT, 0x0F},	/* MDPLS03 */
	{0x3A54, CRL_REG_LEN_08BIT, 0x18},	/* Metadata Size */
	{0x3A55, CRL_REG_LEN_08BIT, 0x0F},
	{0x3554, CRL_REG_LEN_08BIT, 0x00},
	{0x3555, CRL_REG_LEN_08BIT, 0x00},
	{0x3556, CRL_REG_LEN_08BIT, 0x00},
	{0x3557, CRL_REG_LEN_08BIT, 0x00},
	{0x3558, CRL_REG_LEN_08BIT, 0x00},
	{0x3559, CRL_REG_LEN_08BIT, 0x1F},
	{0x355A, CRL_REG_LEN_08BIT, 0x1F},
	{0x35BA, CRL_REG_LEN_08BIT, 0x0F},
	{0x366A, CRL_REG_LEN_08BIT, 0x00},
	{0x366B, CRL_REG_LEN_08BIT, 0x00},
	{0x366C, CRL_REG_LEN_08BIT, 0x00},
	{0x366D, CRL_REG_LEN_08BIT, 0x00},
	{0x33A6, CRL_REG_LEN_08BIT, 0x01},
	{0x306B, CRL_REG_LEN_08BIT, 0x07},	/* MDPLS17 */
	{0x3019, CRL_REG_LEN_08BIT, 0x00},	/* Disable DOL */
};

static struct crl_register_write_rep imx274_3864_2174_RAW10_NORMAL[] = {
	{0x30E2, CRL_REG_LEN_08BIT, 0x01},	/* VCUTMODE */
	{0x3130, CRL_REG_LEN_08BIT, 0x86},	/* WRITE_VSIZE */
	{0x3131, CRL_REG_LEN_08BIT, 0x08},
	{0x3132, CRL_REG_LEN_08BIT, 0x7E},	/* Y_OUT_SIZE */
	{0x3133, CRL_REG_LEN_08BIT, 0x08},
	{0x3004, CRL_REG_LEN_08BIT, 0x01},	/* MDSEL */
	{0x3005, CRL_REG_LEN_08BIT, 0x01},
	{0x3006, CRL_REG_LEN_08BIT, 0x00},
	{0x3007, CRL_REG_LEN_08BIT, 0x02},
	{0x3A41, CRL_REG_LEN_08BIT, 0x08},	/* MDSEL5 */
	{0x3342, CRL_REG_LEN_08BIT, 0x0A},	/* MDPLS01 */
	{0x3343, CRL_REG_LEN_08BIT, 0x00},
	{0x3344, CRL_REG_LEN_08BIT, 0x16},
	{0x3345, CRL_REG_LEN_08BIT, 0x00},
	{0x3528, CRL_REG_LEN_08BIT, 0x0E},	/* MDPLS03 */
	{0x3A54, CRL_REG_LEN_08BIT, 0x18},	/* Metadata Size */
	{0x3A55, CRL_REG_LEN_08BIT, 0x0F},
	{0x3554, CRL_REG_LEN_08BIT, 0x1F},
	{0x3555, CRL_REG_LEN_08BIT, 0x01},
	{0x3556, CRL_REG_LEN_08BIT, 0x01},
	{0x3557, CRL_REG_LEN_08BIT, 0x01},
	{0x3558, CRL_REG_LEN_08BIT, 0x01},
	{0x3559, CRL_REG_LEN_08BIT, 0x00},
	{0x355A, CRL_REG_LEN_08BIT, 0x00},
	{0x35BA, CRL_REG_LEN_08BIT, 0x0E},
	{0x366A, CRL_REG_LEN_08BIT, 0x1B},
	{0x366B, CRL_REG_LEN_08BIT, 0x1A},
	{0x366C, CRL_REG_LEN_08BIT, 0x19},
	{0x366D, CRL_REG_LEN_08BIT, 0x17},
	{0x33A6, CRL_REG_LEN_08BIT, 0x01},
	{0x306B, CRL_REG_LEN_08BIT, 0x05},	/* MDPLS17 */
	{0x3019, CRL_REG_LEN_08BIT, 0x00},	/* Disable DOL */
};

static struct crl_register_write_rep imx274_3868_4536_RAW10_DOL[] = {
	{0x30E2, CRL_REG_LEN_08BIT, 0x01},	/* VCUTMODE */
	{0x3130, CRL_REG_LEN_08BIT, 0x86},	/* WRITE_VSIZE */
	{0x3131, CRL_REG_LEN_08BIT, 0x08},
	{0x3132, CRL_REG_LEN_08BIT, 0x8E},	/* Y_OUT_SIZE */
	{0x3133, CRL_REG_LEN_08BIT, 0x08},
	{0x3004, CRL_REG_LEN_08BIT, 0x06},	/* MDSEL */
	{0x3005, CRL_REG_LEN_08BIT, 0x01},
	{0x3006, CRL_REG_LEN_08BIT, 0x00},
	{0x3007, CRL_REG_LEN_08BIT, 0x02},
	{0x3A41, CRL_REG_LEN_08BIT, 0x00},	/* MDSEL5 */
	{0x3342, CRL_REG_LEN_08BIT, 0x0A},	/* MDPLS01 */
	{0x3343, CRL_REG_LEN_08BIT, 0x00},
	{0x3344, CRL_REG_LEN_08BIT, 0x16},
	{0x3345, CRL_REG_LEN_08BIT, 0x00},
	{0x3528, CRL_REG_LEN_08BIT, 0x0E},	/* MDPLS03 */
	{0x3A54, CRL_REG_LEN_08BIT, 0x1C},	/* Metadata Size */
	{0x3A55, CRL_REG_LEN_08BIT, 0x0F},
	{0x3554, CRL_REG_LEN_08BIT, 0x1F},
	{0x3555, CRL_REG_LEN_08BIT, 0x01},
	{0x3556, CRL_REG_LEN_08BIT, 0x01},
	{0x3557, CRL_REG_LEN_08BIT, 0x01},
	{0x3558, CRL_REG_LEN_08BIT, 0x01},
	{0x3559, CRL_REG_LEN_08BIT, 0x00},
	{0x355A, CRL_REG_LEN_08BIT, 0x00},
	{0x35BA, CRL_REG_LEN_08BIT, 0x0E},
	{0x366A, CRL_REG_LEN_08BIT, 0x1B},
	{0x366B, CRL_REG_LEN_08BIT, 0x1A},
	{0x366C, CRL_REG_LEN_08BIT, 0x19},
	{0x366D, CRL_REG_LEN_08BIT, 0x17},
	{0x33A6, CRL_REG_LEN_08BIT, 0x01},
	{0x306B, CRL_REG_LEN_08BIT, 0x05},	/* MDPLS17 */
	/* DOL mode settings */
	{0x3019, CRL_REG_LEN_08BIT, 0x01},	/* DOLMODE,DOLSCDEN,HINFOEN */
	{0x3041, CRL_REG_LEN_08BIT, 0x31},	/* DOLSET1 */
	{0x3042, CRL_REG_LEN_08BIT, 0x04},	/* HCYCLE */
	{0x3043, CRL_REG_LEN_08BIT, 0x01},
	{0x30E9, CRL_REG_LEN_08BIT, 0x01},	/* DOLSET2 */
};

static struct crl_register_write_rep imx274_1932_1094_RAW10_NORMAL[] = {
	{0x30E2, CRL_REG_LEN_08BIT, 0x02},	/* VCUTMODE */
	{0x3130, CRL_REG_LEN_08BIT, 0x4E},	/* WRITE_VSIZE */
	{0x3131, CRL_REG_LEN_08BIT, 0x04},
	{0x3132, CRL_REG_LEN_08BIT, 0x46},	/* Y_OUT_SIZE */
	{0x3133, CRL_REG_LEN_08BIT, 0x04},
	{0x3004, CRL_REG_LEN_08BIT, 0x02},	/* MDSEL */
	{0x3005, CRL_REG_LEN_08BIT, 0x21},
	{0x3006, CRL_REG_LEN_08BIT, 0x00},
	{0x3007, CRL_REG_LEN_08BIT, 0x11},
	{0x3A41, CRL_REG_LEN_08BIT, 0x08},	/* MDSEL5 */
	{0x3342, CRL_REG_LEN_08BIT, 0x0A},	/* MDPLS01 */
	{0x3343, CRL_REG_LEN_08BIT, 0x00},
	{0x3344, CRL_REG_LEN_08BIT, 0x1A},
	{0x3345, CRL_REG_LEN_08BIT, 0x00},
	{0x3528, CRL_REG_LEN_08BIT, 0x0E},	/* MDPLS03 */
	{0x3A54, CRL_REG_LEN_08BIT, 0x8C},	/* Metadata Size */
	{0x3A55, CRL_REG_LEN_08BIT, 0x07},
	{0x3554, CRL_REG_LEN_08BIT, 0x00},
	{0x3555, CRL_REG_LEN_08BIT, 0x01},
	{0x3556, CRL_REG_LEN_08BIT, 0x01},
	{0x3557, CRL_REG_LEN_08BIT, 0x01},
	{0x3558, CRL_REG_LEN_08BIT, 0x01},
	{0x3559, CRL_REG_LEN_08BIT, 0x00},
	{0x355A, CRL_REG_LEN_08BIT, 0x00},
	{0x35BA, CRL_REG_LEN_08BIT, 0x0E},
	{0x366A, CRL_REG_LEN_08BIT, 0x1B},
	{0x366B, CRL_REG_LEN_08BIT, 0x1A},
	{0x366C, CRL_REG_LEN_08BIT, 0x19},
	{0x366D, CRL_REG_LEN_08BIT, 0x17},
	{0x33A6, CRL_REG_LEN_08BIT, 0x01},
	{0x306B, CRL_REG_LEN_08BIT, 0x05},	/* MDPLS17 */
	{0x3019, CRL_REG_LEN_08BIT, 0x00},	/* Disable DOL */
};

static struct crl_register_write_rep imx274_1932_1094_RAW12_NORMAL[] = {
	{0x30E2, CRL_REG_LEN_08BIT, 0x02},	/* VCUTMODE */
	{0x3130, CRL_REG_LEN_08BIT, 0x4E},	/* WRITE_VSIZE */
	{0x3131, CRL_REG_LEN_08BIT, 0x04},
	{0x3132, CRL_REG_LEN_08BIT, 0x46},	/* Y_OUT_SIZE */
	{0x3133, CRL_REG_LEN_08BIT, 0x04},
	{0x3004, CRL_REG_LEN_08BIT, 0x02},	/* MDSEL */
	{0x3005, CRL_REG_LEN_08BIT, 0x27},
	{0x3006, CRL_REG_LEN_08BIT, 0x00},
	{0x3007, CRL_REG_LEN_08BIT, 0x11},
	{0x3A41, CRL_REG_LEN_08BIT, 0x08},	/* MDSEL5 */
	{0x3342, CRL_REG_LEN_08BIT, 0xFF},	/* MDPLS01 */
	{0x3343, CRL_REG_LEN_08BIT, 0x01},
	{0x3344, CRL_REG_LEN_08BIT, 0xFF},
	{0x3345, CRL_REG_LEN_08BIT, 0x01},
	{0x3528, CRL_REG_LEN_08BIT, 0x0F},	/* MDPLS03 */
	{0x3A54, CRL_REG_LEN_08BIT, 0x8C},	/* Metadata Size */
	{0x3A55, CRL_REG_LEN_08BIT, 0x07},
	{0x3554, CRL_REG_LEN_08BIT, 0x00},
	{0x3555, CRL_REG_LEN_08BIT, 0x00},
	{0x3556, CRL_REG_LEN_08BIT, 0x00},
	{0x3557, CRL_REG_LEN_08BIT, 0x00},
	{0x3558, CRL_REG_LEN_08BIT, 0x00},
	{0x3559, CRL_REG_LEN_08BIT, 0x1F},
	{0x355A, CRL_REG_LEN_08BIT, 0x1F},
	{0x35BA, CRL_REG_LEN_08BIT, 0x0F},
	{0x366A, CRL_REG_LEN_08BIT, 0x00},
	{0x366B, CRL_REG_LEN_08BIT, 0x00},
	{0x366C, CRL_REG_LEN_08BIT, 0x00},
	{0x366D, CRL_REG_LEN_08BIT, 0x00},
	{0x33A6, CRL_REG_LEN_08BIT, 0x01},
	{0x306B, CRL_REG_LEN_08BIT, 0x07},	/* MDPLS17 */
	{0x3019, CRL_REG_LEN_08BIT, 0x00},	/* Disable DOL */
};

static struct crl_register_write_rep imx274_1936_2376_RAW10_DOL[] = {
	{0x30E2, CRL_REG_LEN_08BIT, 0x02},	/* VCUTMODE */
	{0x3130, CRL_REG_LEN_08BIT, 0x4E},	/* WRITE_VSIZE */
	{0x3131, CRL_REG_LEN_08BIT, 0x04},
	{0x3132, CRL_REG_LEN_08BIT, 0x54},	/* Y_OUT_SIZE */
	{0x3133, CRL_REG_LEN_08BIT, 0x04},
	{0x3004, CRL_REG_LEN_08BIT, 0x07},	/* MDSEL */
	{0x3005, CRL_REG_LEN_08BIT, 0x21},
	{0x3006, CRL_REG_LEN_08BIT, 0x00},
	{0x3007, CRL_REG_LEN_08BIT, 0x11},
	{0x3A41, CRL_REG_LEN_08BIT, 0x08},	/* MDSEL5 */
	{0x3342, CRL_REG_LEN_08BIT, 0x0A},	/* MDPLS01 */
	{0x3343, CRL_REG_LEN_08BIT, 0x00},
	{0x3344, CRL_REG_LEN_08BIT, 0x1A},
	{0x3345, CRL_REG_LEN_08BIT, 0x00},
	{0x3528, CRL_REG_LEN_08BIT, 0x0E},	/* MDPLS03 */
	{0x3A54, CRL_REG_LEN_08BIT, 0x90},	/* Metadata Size */
	{0x3A55, CRL_REG_LEN_08BIT, 0x07},
	{0x3554, CRL_REG_LEN_08BIT, 0x00},
	{0x3555, CRL_REG_LEN_08BIT, 0x01},
	{0x3556, CRL_REG_LEN_08BIT, 0x01},
	{0x3557, CRL_REG_LEN_08BIT, 0x01},
	{0x3558, CRL_REG_LEN_08BIT, 0x01},
	{0x3559, CRL_REG_LEN_08BIT, 0x00},
	{0x355A, CRL_REG_LEN_08BIT, 0x00},
	{0x35BA, CRL_REG_LEN_08BIT, 0x0E},
	{0x366A, CRL_REG_LEN_08BIT, 0x1B},
	{0x366B, CRL_REG_LEN_08BIT, 0x1A},
	{0x366C, CRL_REG_LEN_08BIT, 0x19},
	{0x366D, CRL_REG_LEN_08BIT, 0x17},
	{0x33A6, CRL_REG_LEN_08BIT, 0x01},
	{0x306B, CRL_REG_LEN_08BIT, 0x05},	/* MDPLS17 */
	/* DOL mode settings */
	{0x3019, CRL_REG_LEN_08BIT, 0x01},	/* DOLMODE,DOLSCDEN,HINFOEN */
	{0x3041, CRL_REG_LEN_08BIT, 0x31},	/* DOLSET1 */
	{0x3042, CRL_REG_LEN_08BIT, 0x04},	/* HCYCLE */
	{0x3043, CRL_REG_LEN_08BIT, 0x01},
	{0x30E9, CRL_REG_LEN_08BIT, 0x01},	/* DOLSET2 */
};

static struct crl_register_write_rep imx274_1288_738_RAW10_NORMAL[] = {
	{0x30E2, CRL_REG_LEN_08BIT, 0x03},	/* VCUTMODE */
	{0x3130, CRL_REG_LEN_08BIT, 0xE2},	/* WRITE_VSIZE */
	{0x3131, CRL_REG_LEN_08BIT, 0x02},
	{0x3132, CRL_REG_LEN_08BIT, 0xDE},	/* Y_OUT_SIZE */
	{0x3133, CRL_REG_LEN_08BIT, 0x02},
	{0x3004, CRL_REG_LEN_08BIT, 0x03},	/* MDSEL */
	{0x3005, CRL_REG_LEN_08BIT, 0x31},
	{0x3006, CRL_REG_LEN_08BIT, 0x00},
	{0x3007, CRL_REG_LEN_08BIT, 0x09},
	{0x3A41, CRL_REG_LEN_08BIT, 0x04},	/* MDSEL5 */
	{0x3342, CRL_REG_LEN_08BIT, 0x0A},	/* MDPLS01 */
	{0x3343, CRL_REG_LEN_08BIT, 0x00},
	{0x3344, CRL_REG_LEN_08BIT, 0x1B},
	{0x3345, CRL_REG_LEN_08BIT, 0x00},
	{0x3528, CRL_REG_LEN_08BIT, 0x0E},	/* MDPLS03 */
	{0x3A54, CRL_REG_LEN_08BIT, 0x8C},	/* Metadata Size */
	{0x3A55, CRL_REG_LEN_08BIT, 0x00},
	{0x3554, CRL_REG_LEN_08BIT, 0x00},
	{0x3555, CRL_REG_LEN_08BIT, 0x01},
	{0x3556, CRL_REG_LEN_08BIT, 0x01},
	{0x3557, CRL_REG_LEN_08BIT, 0x01},
	{0x3558, CRL_REG_LEN_08BIT, 0x01},
	{0x3559, CRL_REG_LEN_08BIT, 0x00},
	{0x355A, CRL_REG_LEN_08BIT, 0x00},
	{0x35BA, CRL_REG_LEN_08BIT, 0x0E},
	{0x366A, CRL_REG_LEN_08BIT, 0x1B},
	{0x366B, CRL_REG_LEN_08BIT, 0x19},
	{0x366C, CRL_REG_LEN_08BIT, 0x17},
	{0x366D, CRL_REG_LEN_08BIT, 0x17},
	{0x33A6, CRL_REG_LEN_08BIT, 0x01},
	{0x306B, CRL_REG_LEN_08BIT, 0x05},	/* MDPLS17 */
	{0x3019, CRL_REG_LEN_08BIT, 0x00},	/* Disable DOL */
};

static struct crl_register_write_rep imx274_streamon_regs[] = {
	{0x00, CRL_REG_LEN_DELAY, 10, 0x00},	/* Add a pre 10ms delay */
	{IMX274_REG_STANDBY, CRL_REG_LEN_08BIT, 0x00},
	{0x303E, CRL_REG_LEN_08BIT, 0x02},
	{0x00, CRL_REG_LEN_DELAY, 7, 0x00},	/* Add a 7ms delay */
	{0x30F4, CRL_REG_LEN_08BIT, 0x00},
	{0x3018, CRL_REG_LEN_08BIT, 0x02},
};

static struct crl_register_write_rep imx274_streamoff_regs[] = {
	{0x00, CRL_REG_LEN_DELAY, 10, 0x00},	/* Add a pre 10ms delay */
	{IMX274_REG_STANDBY, CRL_REG_LEN_08BIT, 0x01},
	{0x303E, CRL_REG_LEN_08BIT, 0x02},
	{0x00, CRL_REG_LEN_DELAY, 7, 0x00},	/* Add a delay */
	{0x30F4, CRL_REG_LEN_08BIT, 0x01},
	{0x3018, CRL_REG_LEN_08BIT, 0x02},
};

static struct crl_arithmetic_ops imx274_rshift8_ops[] = {
	{
		.op = CRL_BITWISE_RSHIFT,
		.operand.entity_val = 8,
	}
};

static struct crl_arithmetic_ops imx274_rshift16_ops[] = {
	{
		.op = CRL_BITWISE_RSHIFT,
		.operand.entity_val = 16,
	}
};

static struct crl_arithmetic_ops imx274_nan_gain_ops[] = {
	{
		.op = CRL_BITWISE_RSHIFT,
		.operand.entity_val = 8,
	},
	{
		.op = CRL_BITWISE_AND,
		.operand.entity_val = 0x07,
	}
};

/* imx274 use register PGC[10:0] 300A 300B to indicate analog gain */
static struct crl_dynamic_register_access imx274_ana_gain_global_regs[] = {
	{
		.address = 0x300A,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = 0,
		.ops = 0,
	},
	{
		.address = 0x300B,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = ARRAY_SIZE(imx274_nan_gain_ops),
		.ops = imx274_nan_gain_ops,
	},
};

static struct crl_dynamic_register_access imx274_dig_gain_regs[] = {
	{
		.address = 0x3012,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xf,
	},
};

/* shr = fll - exposure */
static struct crl_arithmetic_ops imx274_shr_lsb_ops[] = {
	{
		.op = CRL_SUBTRACT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CTRL_VAL,
		.operand.entity_val = V4L2_CID_FRAME_LENGTH_LINES,
	}
};

static struct crl_arithmetic_ops imx274_shr_msb_ops[] = {
	{
		.op = CRL_SUBTRACT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CTRL_VAL,
		.operand.entity_val = V4L2_CID_FRAME_LENGTH_LINES,
	},
	{
		.op = CRL_BITWISE_RSHIFT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
		.operand.entity_val = 8,
	}
};

static struct crl_dynamic_register_access imx274_shr_regs[] = {
	{
		.address = 0x300C,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = ARRAY_SIZE(imx274_shr_lsb_ops),
		.ops = imx274_shr_lsb_ops,
		.mask = 0xff,
	},
	{
		.address = 0x300D,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = ARRAY_SIZE(imx274_shr_msb_ops),
		.ops = imx274_shr_msb_ops,
		.mask = 0xff,
	},
};

/* Short exposure for DOL */
static struct crl_dynamic_register_access imx274_shs1_regs[] = {
	{
		.address = 0x302E,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xff,
	},
	{
		.address = 0x302F,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = ARRAY_SIZE(imx274_rshift8_ops),
		.ops = imx274_rshift8_ops,
		.mask = 0xff,
	},
};

/* Long exposure for DOL */
static struct crl_dynamic_register_access imx274_shs2_regs[] = {
	{
		.address = 0x3030,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xff,
	},
	{
		.address = 0x3031,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = ARRAY_SIZE(imx274_rshift8_ops),
		.ops = imx274_rshift8_ops,
		.mask = 0xff,
	},
};

static struct crl_dynamic_register_access imx274_rhs1_regs[] = {
	{
		.address = 0x3032,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xff,
	},
	{
		.address = 0x3033,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = ARRAY_SIZE(imx274_rshift8_ops),
		.ops = imx274_rshift8_ops,
		.mask = 0xff,
	},
};

static struct crl_dynamic_register_access imx274_fll_regs[] = {
	/*
	 * Use 8bits access since 24bits or 32bits access will fail
	 * TODO: root cause the 24bits and 32bits access issues
	 */
	{
		.address = 0x30F8,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xff,
	},
	{
		.address = 0x30F9,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = ARRAY_SIZE(imx274_rshift8_ops),
		.ops = imx274_rshift8_ops,
		.mask = 0xff,
	},
	{
		.address = 0x30FA,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = ARRAY_SIZE(imx274_rshift16_ops),
		.ops = imx274_rshift16_ops,
		.mask = 0xf,
	},
};

static struct crl_dynamic_register_access imx274_llp_regs[] = {
	{
		.address = 0x30F6,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xff,
	},
	{
		.address = 0x30F7,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = ARRAY_SIZE(imx274_rshift8_ops),
		.ops = imx274_rshift8_ops,
		.mask = 0xff,
	},
};

static struct crl_sensor_detect_config imx274_sensor_detect_regset[] = {
	{
		.reg = { 0x30F8, CRL_REG_LEN_08BIT, 0x000000ff },
		.width = 7,
	},
	{
		.reg = { 0x30F9, CRL_REG_LEN_08BIT, 0x000000ff },
		.width = 7,
	},
};

static struct crl_pll_configuration imx274_pll_configurations[] = {
	{
		.input_clk = 24000000,
		.op_sys_clk = 720000000,	/* 1440000000/2 */
		.bitsperpixel = 10,
		.pixel_rate_csi = 72000000,
		.pixel_rate_pa = 72000000,	/* 72MHz */
		.csi_lanes = 4,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = ARRAY_SIZE(imx274_pll_1440mbps),
		.pll_regs = imx274_pll_1440mbps,
	},
	{
		.input_clk = 24000000,
		.op_sys_clk = 720000000,	/* 1440000000/2 */
		.bitsperpixel = 12,
		.pixel_rate_csi = 72000000,
		.pixel_rate_pa = 72000000,	/* 72MHz */
		.csi_lanes = 4,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = ARRAY_SIZE(imx274_pll_1440mbps),
		.pll_regs = imx274_pll_1440mbps,
	}
};

static struct crl_subdev_rect_rep imx274_3864_2202_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3868,
		.in_rect.height = 4536,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3868,
		.out_rect.height = 4536,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3868,
		.in_rect.height = 4536,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3864,
		.out_rect.height = 2202,
	}
};

static struct crl_subdev_rect_rep imx274_3864_2174_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3868,
		.in_rect.height = 4536,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3868,
		.out_rect.height = 4536,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3868,
		.in_rect.height = 4536,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3864,
		.out_rect.height = 2174,
	}
};

/* DOL pixel array includes 4 pixel sync code each line */
static struct crl_subdev_rect_rep imx274_3868_4536_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3868,
		.in_rect.height = 4536,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3868,
		.out_rect.height = 4536,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3868,
		.in_rect.height = 4536,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3868,
		.out_rect.height = 4536,
	}
};

static struct crl_subdev_rect_rep imx274_1932_1094_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3868,
		.in_rect.height = 4536,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3868,
		.out_rect.height = 4536,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3868,
		.in_rect.height = 4536,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1932,
		.out_rect.height = 1094,
	}
};

/* DOL pixel array includes 4 pixel sync code each line */
static struct crl_subdev_rect_rep imx274_1936_2376_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3868,
		.in_rect.height = 4536,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3868,
		.out_rect.height = 4536,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3868,
		.in_rect.height = 4536,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1936,
		.out_rect.height = 2376,
	}
};

static struct crl_subdev_rect_rep imx274_1288_738_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3868,
		.in_rect.height = 4536,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 3868,
		.out_rect.height = 4536,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 3868,
		.in_rect.height = 4536,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1288,
		.out_rect.height = 738,
	}
};

static struct crl_mode_rep imx274_modes[] = {
	{
		/* mode 0 12bit all pixel scan per datasheet */
		.sd_rects_items = ARRAY_SIZE(imx274_3864_2202_rects),
		.sd_rects = imx274_3864_2202_rects,
		.binn_hor = 1,
		.binn_vert = 2,
		.scale_m = 1,
		.width = 3864,
		.height = 2202,
		.min_llp = 493, /* 01EDh */
		.min_fll = 4868, /* default 30fps */
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items =
				ARRAY_SIZE(imx274_3864_2202_RAW12_NORMAL),
		.mode_regs = imx274_3864_2202_RAW12_NORMAL,
	},
	{
		/* mode 1 10bit all pixel scan per datasheet */
		.sd_rects_items = ARRAY_SIZE(imx274_3864_2174_rects),
		.sd_rects = imx274_3864_2174_rects,
		.binn_hor = 1,
		.binn_vert = 2,
		.scale_m = 1,
		.width = 3864,
		.height = 2174,
		.min_llp = 493, /* 01EDh */
		.min_fll = 4868, /* default 30fps */
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items =
				ARRAY_SIZE(imx274_3864_2174_RAW10_NORMAL),
		.mode_regs = imx274_3864_2174_RAW10_NORMAL,
	},
	{
		/* mode 1 DOL 10bit per datasheet */
		.sd_rects_items = ARRAY_SIZE(imx274_3868_4536_rects),
		.sd_rects = imx274_3868_4536_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 3868,
		.height = 4536, /* 2*(2160+22+VBP) */
		.min_llp = 1052, /* 041Ch */
		.min_fll = 2281, /* 30fps */
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items =
				ARRAY_SIZE(imx274_3868_4536_RAW10_DOL),
		.mode_regs = imx274_3868_4536_RAW10_DOL,
	},
	{
		/* mode 3 10bit all pixel scan per datasheet */
		.sd_rects_items = ARRAY_SIZE(imx274_1932_1094_rects),
		.sd_rects = imx274_1932_1094_rects,
		.binn_hor = 2,
		.binn_vert = 4,
		.scale_m = 1,
		.width = 1932,
		.height = 1094,
		.min_llp = 493, /* 01EDh */
		.min_fll = 4868, /* default 30fps */
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(
				imx274_1932_1094_RAW10_NORMAL),
		.mode_regs = imx274_1932_1094_RAW10_NORMAL,
	},
	{
		/* mode 3 12bit all pixel scan per datasheet */
		.sd_rects_items = ARRAY_SIZE(imx274_1932_1094_rects),
		.sd_rects = imx274_1932_1094_rects,
		.binn_hor = 2,
		.binn_vert = 4,
		.scale_m = 1,
		.width = 1932,
		.height = 1094,
		.min_llp = 493, /* 01EDh */
		.min_fll = 4868, /* default 30fps */
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(
				imx274_1932_1094_RAW12_NORMAL),
		.mode_regs = imx274_1932_1094_RAW12_NORMAL,
	},
	{
		/* mode 3 DOL bit10 per datasheet */
		.sd_rects_items = ARRAY_SIZE(imx274_1936_2376_rects),
		.sd_rects = imx274_1936_2376_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 1936,
		.height = 2376,
		.min_llp = 1052, /* 041Ch */
		.min_fll = 2281, /* 30fps */
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(
				imx274_1936_2376_RAW10_DOL),
		.mode_regs = imx274_1936_2376_RAW10_DOL,
	},
	{
		/* mode 5 bit10 per datasheet */
		.sd_rects_items = ARRAY_SIZE(imx274_1288_738_rects),
		.sd_rects = imx274_1288_738_rects,
		.binn_hor = 3,
		.binn_vert = 6,
		.scale_m = 1,
		.width = 1288,
		.height = 738,
		.min_llp = 260,
		.min_fll = 2310,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(
				imx274_1288_738_RAW10_NORMAL),
		.mode_regs = imx274_1288_738_RAW10_NORMAL,
	},
};

struct crl_sensor_subdev_config imx274_sensor_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.name = "imx274 binner",
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "imx274 pixel array",
	}
};

static struct crl_sensor_limits imx274_sensor_limits = {
		.x_addr_min = 0,
		.y_addr_min = 0,
		.x_addr_max = 3868, /* pixel area length and width */
		.y_addr_max = 4536,
		.min_frame_length_lines = 1111,
		.max_frame_length_lines = 65535,
		.min_line_length_pixels = 260,
		.max_line_length_pixels = 32752,
};

static struct crl_flip_data imx274_flip_configurations[] = {
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

static struct crl_csi_data_fmt imx274_crl_csi_data_fmt[] = {
	{
		.code = MEDIA_BUS_FMT_SGRBG10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.bits_per_pixel = 10,
		.regs_items = 0,
		.regs = 0,
	},
	{
		.code = MEDIA_BUS_FMT_SRGGB10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,  /* default order */
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
		.bits_per_pixel = 10,
		.regs_items = 0,
		.regs = 0,
	},
	{
		.code = MEDIA_BUS_FMT_SGRBG12_1X12,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.bits_per_pixel = 12,
		.regs_items = 0,
		.regs = 0,
	},
	{
		.code = MEDIA_BUS_FMT_SRGGB12_1X12,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,	/* default order */
		.bits_per_pixel = 12,
		.regs_items = 0,
		.regs = 0,
	},
	{
		.code = MEDIA_BUS_FMT_SBGGR12_1X12,
		.pixel_order = CRL_PIXEL_ORDER_BGGR,
		.bits_per_pixel = 12,
		.regs_items = 0,
		.regs = 0,
	},
	{
		.code = MEDIA_BUS_FMT_SGBRG12_1X12,
		.pixel_order = CRL_PIXEL_ORDER_GBRG,
		.bits_per_pixel = 12,
		.regs_items = 0,
		.regs = 0,
	}
};

static struct crl_v4l2_ctrl imx274_v4l2_ctrls[] = {
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
		.data.std_data.min = 1111,
		.data.std_data.max = IMX274_VMAX,
		.data.std_data.step = 1,
		.data.std_data.def = 1111,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx274_fll_regs),
		.regs = imx274_fll_regs,
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
		.data.std_data.min = 260,
		.data.std_data.max = IMX274_HMAX,
		.data.std_data.step = 1,
		.data.std_data.def = 260,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx274_llp_regs),
		.regs = imx274_llp_regs,
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
		.data.std_data.max = 6,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx274_dig_gain_regs),
		.regs = imx274_dig_gain_regs,
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
		.data.std_data.max = 0x7A5,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx274_ana_gain_global_regs),
		.regs = imx274_ana_gain_global_regs,
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
		.data.std_data.min = 4,
		.data.std_data.max = IMX274_MAX_SHS2,
		.data.std_data.step = 1,
		.data.std_data.def = 0x400,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx274_shr_regs),
		.regs = imx274_shr_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = CRL_CID_EXPOSURE_SHS1,
		.name = "CRL_CID_EXPOSURE_SHS1",
		.type = CRL_V4L2_CTRL_TYPE_CUSTOM,
		.data.std_data.min = 4,
		.data.std_data.max = IMX274_MAX_SHS1,
		.data.std_data.step = 1,
		.data.std_data.def = 0x06,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx274_shs1_regs),
		.regs = imx274_shs1_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = CRL_CID_EXPOSURE_SHS2,
		.name = "CRL_CID_EXPOSURE_SHS2",
		.type = CRL_V4L2_CTRL_TYPE_CUSTOM,
		.data.std_data.min = 10,
		.data.std_data.max = IMX274_MAX_SHS2,
		.data.std_data.step = 1,
		.data.std_data.def = 0x2d,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx274_shs2_regs),
		.regs = imx274_shs2_regs,
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
		.data.std_data.max = IMX274_MAX_RHS1,
		.data.std_data.step = 1,
		.data.std_data.def = 0x56, /* Fixed to 86 by default */
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx274_rhs1_regs),
		.regs = imx274_rhs1_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_BINNER,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = CRL_CID_SENSOR_MODE,
		.name = "CRL_CID_SENSOR_MODE",
		.type = CRL_V4L2_CTRL_TYPE_CUSTOM,
		.data.std_data.min = 0,
		.data.std_data.max = ARRAY_SIZE(imx274_modes) - 1,
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

static struct crl_arithmetic_ops imx274_frame_desc_width_ops[] = {
	{
		.op = CRL_ASSIGNMENT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_VAR_REF,
		.operand.entity_val = CRL_VAR_REF_OUTPUT_WIDTH,
	},
};

static struct crl_arithmetic_ops imx274_frame_desc_height_ops[] = {
	{
		.op = CRL_ASSIGNMENT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
		.operand.entity_val = 1,
	},
};

static struct crl_frame_desc imx274_frame_desc[] = {
	{
		.flags.entity_val = 0,
		.bpp.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_VAR_REF,
		.bpp.entity_val = CRL_VAR_REF_BITSPERPIXEL,
		.pixelcode.entity_val = MEDIA_BUS_FMT_FIXED,
		.length.entity_val = 0,
		.start_line.entity_val = 0,
		.start_pixel.entity_val = 0,
		.width = {
			.ops_items = ARRAY_SIZE(imx274_frame_desc_width_ops),
			.ops = imx274_frame_desc_width_ops,
			},
		.height = {
			.ops_items = ARRAY_SIZE(imx274_frame_desc_height_ops),
			.ops = imx274_frame_desc_height_ops,
			},
			.csi2_channel.entity_val = 0,
			.csi2_data_type.entity_val = 0x12,
	},
};

static struct crl_power_seq_entity imx274_power_items[] = {
	{
		.type = CRL_POWER_ETY_CLK_FRAMEWORK,
		.val = 24000000,
	},
	{
		.type = CRL_POWER_ETY_GPIO_FROM_PDATA,
		.val = 1,
	},
};

struct crl_sensor_configuration imx274_crl_configuration = {

	.power_items = ARRAY_SIZE(imx274_power_items),
	.power_entities = imx274_power_items,

	.powerup_regs_items = ARRAY_SIZE(imx274_powerup_standby),
	.powerup_regs = imx274_powerup_standby,

	.poweroff_regs_items = 0,
	.poweroff_regs = 0,

	.id_reg_items = ARRAY_SIZE(imx274_sensor_detect_regset),
	.id_regs = imx274_sensor_detect_regset,

	.subdev_items = ARRAY_SIZE(imx274_sensor_subdevs),
	.subdevs = imx274_sensor_subdevs,

	.sensor_limits = &imx274_sensor_limits,

	.pll_config_items = ARRAY_SIZE(imx274_pll_configurations),
	.pll_configs = imx274_pll_configurations,

	.modes_items = ARRAY_SIZE(imx274_modes),
	.modes = imx274_modes,

	.streamon_regs_items = ARRAY_SIZE(imx274_streamon_regs),
	.streamon_regs = imx274_streamon_regs,

	.streamoff_regs_items = ARRAY_SIZE(imx274_streamoff_regs),
	.streamoff_regs = imx274_streamoff_regs,

	.v4l2_ctrls_items = ARRAY_SIZE(imx274_v4l2_ctrls),
	.v4l2_ctrl_bank = imx274_v4l2_ctrls,

	.csi_fmts_items = ARRAY_SIZE(imx274_crl_csi_data_fmt),
	.csi_fmts = imx274_crl_csi_data_fmt,

	.flip_items = ARRAY_SIZE(imx274_flip_configurations),
	.flip_data = imx274_flip_configurations,

	.frame_desc_entries = ARRAY_SIZE(imx274_frame_desc),
	.frame_desc_type = CRL_V4L2_MBUS_FRAME_DESC_TYPE_CSI2,
	.frame_desc = imx274_frame_desc,

};

#endif  /* __CRLMODULE_IMX274_CONFIGURATION_H_ */
