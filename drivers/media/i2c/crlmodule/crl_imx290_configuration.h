/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2017 - 2018 Intel Corporation
 *
 * Author: Yuning Pu <yuning.pu@intel.com>
 *
 */

#ifndef __CRLMODULE_IMX290_CONFIGURATION_H_
#define __CRLMODULE_IMX290_CONFIGURATION_H_

#include "crlmodule-sensor-ds.h"

#define IMX290_REG_STANDBY			0x3000
#define IMX290_REG_XMSTA			0x3002

#define IMX290_HMAX				65535
#define IMX290_VMAX				131071
#define IMX290_MAX_SHS1				(IMX290_VMAX - 2)

static struct crl_register_write_rep imx290_pll_891mbps[] = {
	{0x3405, CRL_REG_LEN_08BIT, 0x00},	/* repetition */
	{0x3407, CRL_REG_LEN_08BIT, 0x03},	/* physical lane num(fixed) */
	{0x3009, CRL_REG_LEN_08BIT, 0x00},	/* FRSEL FDG_SEL */
	{0x300F, CRL_REG_LEN_08BIT, 0x00},	/* fixed setting */
	{0x3010, CRL_REG_LEN_08BIT, 0x21},
	{0x3012, CRL_REG_LEN_08BIT, 0x64},
	{0x3414, CRL_REG_LEN_08BIT, 0x0A},
	{0x3415, CRL_REG_LEN_08BIT, 0x00},
	{0x3016, CRL_REG_LEN_08BIT, 0x09},	/* changed */
	{0x3119, CRL_REG_LEN_08BIT, 0x9E},
	{0x311C, CRL_REG_LEN_08BIT, 0x1E},
	{0x311E, CRL_REG_LEN_08BIT, 0x08},
	{0x3128, CRL_REG_LEN_08BIT, 0x05},
	{0x332C, CRL_REG_LEN_08BIT, 0xD3},
	{0x332D, CRL_REG_LEN_08BIT, 0x10},
	{0x332E, CRL_REG_LEN_08BIT, 0x0D},
	{0x313D, CRL_REG_LEN_08BIT, 0x83},
	{0x3443, CRL_REG_LEN_08BIT, 0x03},	/* csi_lane_mode(fixed) */
	{0x3444, CRL_REG_LEN_08BIT, 0x20},	/* extck_freq */
	{0x3445, CRL_REG_LEN_08BIT, 0x25},
	{0x3446, CRL_REG_LEN_08BIT, 0x77},	/* tclkpost */
	{0x3447, CRL_REG_LEN_08BIT, 0x00},
	{0x3448, CRL_REG_LEN_08BIT, 0x67},	/* thszero */
	{0x3449, CRL_REG_LEN_08BIT, 0x00},
	{0x344A, CRL_REG_LEN_08BIT, 0x47},	/* thsprepare */
	{0x344B, CRL_REG_LEN_08BIT, 0x00},
	{0x344C, CRL_REG_LEN_08BIT, 0x37},	/* thstrail */
	{0x344D, CRL_REG_LEN_08BIT, 0x00},
	{0x344E, CRL_REG_LEN_08BIT, 0x3F},	/* thstrail */
	{0x344F, CRL_REG_LEN_08BIT, 0x00},
	{0x3150, CRL_REG_LEN_08BIT, 0x03},
	{0x3450, CRL_REG_LEN_08BIT, 0xFF},	/* tclkzero */
	{0x3451, CRL_REG_LEN_08BIT, 0x00},
	{0x3452, CRL_REG_LEN_08BIT, 0x3F},	/* tclkprepare */
	{0x3453, CRL_REG_LEN_08BIT, 0x00},
	{0x3454, CRL_REG_LEN_08BIT, 0x37},	/* tlpx */
	{0x3455, CRL_REG_LEN_08BIT, 0x00},
	{0x3358, CRL_REG_LEN_08BIT, 0x06},	/* fixed setting */
	{0x3359, CRL_REG_LEN_08BIT, 0xE1},
	{0x335A, CRL_REG_LEN_08BIT, 0x11},
	{0x305C, CRL_REG_LEN_08BIT, 0x18},	/* incksel1 */
	{0x305D, CRL_REG_LEN_08BIT, 0x03},	/* incksel2 */
	{0x305E, CRL_REG_LEN_08BIT, 0x20},	/* incksel3 */
	{0x315E, CRL_REG_LEN_08BIT, 0x1A},	/* incksel5 */
	{0x305F, CRL_REG_LEN_08BIT, 0x01},	/* incksel4 */
	{0x3360, CRL_REG_LEN_08BIT, 0x1E},
	{0x3361, CRL_REG_LEN_08BIT, 0x61},
	{0x3362, CRL_REG_LEN_08BIT, 0x10},
	{0x3164, CRL_REG_LEN_08BIT, 0x1A},	/* incksel6 */
	{0x3070, CRL_REG_LEN_08BIT, 0x02},
	{0x3071, CRL_REG_LEN_08BIT, 0x11},
	{0x317E, CRL_REG_LEN_08BIT, 0x00},
	{0x3480, CRL_REG_LEN_08BIT, 0x49},	/* inclsel7 */
	{0x309B, CRL_REG_LEN_08BIT, 0x10},
	{0x309C, CRL_REG_LEN_08BIT, 0x22},
	{0x30A2, CRL_REG_LEN_08BIT, 0x02},
	{0x30A6, CRL_REG_LEN_08BIT, 0x20},
	{0x30A8, CRL_REG_LEN_08BIT, 0x20},
	{0x30AA, CRL_REG_LEN_08BIT, 0x20},
	{0x30AC, CRL_REG_LEN_08BIT, 0x20},
	{0x30B0, CRL_REG_LEN_08BIT, 0x43},
	{0x33B0, CRL_REG_LEN_08BIT, 0x50},
	{0x33B2, CRL_REG_LEN_08BIT, 0x1A},
	{0x33B3, CRL_REG_LEN_08BIT, 0x04},
	{0x32B8, CRL_REG_LEN_08BIT, 0x50},
	{0x32B9, CRL_REG_LEN_08BIT, 0x10},
	{0x32BA, CRL_REG_LEN_08BIT, 0x00},
	{0x32BB, CRL_REG_LEN_08BIT, 0x04},
	{0x32C8, CRL_REG_LEN_08BIT, 0x50},
	{0x32C9, CRL_REG_LEN_08BIT, 0x10},
	{0x32CA, CRL_REG_LEN_08BIT, 0x00},
	{0x32CB, CRL_REG_LEN_08BIT, 0x04},
};

/* 445Mbps for imx290 1080p 30fps */
static struct crl_register_write_rep imx290_pll_445mbps[] = {
	{0x3405, CRL_REG_LEN_08BIT, 0x20},	/* repetition */
	{0x3407, CRL_REG_LEN_08BIT, 0x03},	/* physical lane num(fixed) */
	{0x3009, CRL_REG_LEN_08BIT, 0x02},	/* FRSEL FDG_SEL */
	{0x300F, CRL_REG_LEN_08BIT, 0x00},	/* fixed setting */
	{0x3010, CRL_REG_LEN_08BIT, 0x21},
	{0x3012, CRL_REG_LEN_08BIT, 0x64},
	{0x3414, CRL_REG_LEN_08BIT, 0x0A},
	{0x3016, CRL_REG_LEN_08BIT, 0x09},	/* changed */
	{0x3119, CRL_REG_LEN_08BIT, 0x9E},
	{0x311C, CRL_REG_LEN_08BIT, 0x1E},
	{0x311E, CRL_REG_LEN_08BIT, 0x08},
	{0x3128, CRL_REG_LEN_08BIT, 0x05},
	{0x332C, CRL_REG_LEN_08BIT, 0xD3},
	{0x332D, CRL_REG_LEN_08BIT, 0x10},
	{0x332E, CRL_REG_LEN_08BIT, 0x0D},
	{0x313D, CRL_REG_LEN_08BIT, 0x83},
	{0x3443, CRL_REG_LEN_08BIT, 0x03},	/* csi_lane_mode(fixed) */
	{0x3444, CRL_REG_LEN_08BIT, 0x20},	/* extck_freq */
	{0x3445, CRL_REG_LEN_08BIT, 0x25},
	{0x3446, CRL_REG_LEN_08BIT, 0x47},	/* tclkpost */
	{0x3447, CRL_REG_LEN_08BIT, 0x00},
	{0x3448, CRL_REG_LEN_08BIT, 0x1F},	/* thszero */
	{0x3449, CRL_REG_LEN_08BIT, 0x00},
	{0x344A, CRL_REG_LEN_08BIT, 0x17},	/* thsprepare */
	{0x344B, CRL_REG_LEN_08BIT, 0x00},
	{0x344C, CRL_REG_LEN_08BIT, 0x0F},	/* thstrail */
	{0x344D, CRL_REG_LEN_08BIT, 0x00},
	{0x344E, CRL_REG_LEN_08BIT, 0x17},	/* thstrail */
	{0x344F, CRL_REG_LEN_08BIT, 0x00},
	{0x3150, CRL_REG_LEN_08BIT, 0x03},
	{0x3450, CRL_REG_LEN_08BIT, 0x47},	/* tclkzero */
	{0x3451, CRL_REG_LEN_08BIT, 0x00},
	{0x3452, CRL_REG_LEN_08BIT, 0x0F},	/* tclkprepare */
	{0x3453, CRL_REG_LEN_08BIT, 0x00},
	{0x3454, CRL_REG_LEN_08BIT, 0x0F},	/* tlpx */
	{0x3455, CRL_REG_LEN_08BIT, 0x00},
	{0x3358, CRL_REG_LEN_08BIT, 0x06},	/* fixed setting */
	{0x3359, CRL_REG_LEN_08BIT, 0xE1},
	{0x335A, CRL_REG_LEN_08BIT, 0x11},
	{0x305C, CRL_REG_LEN_08BIT, 0x18},	/* incksel1 */
	{0x305D, CRL_REG_LEN_08BIT, 0x03},	/* incksel2 */
	{0x305E, CRL_REG_LEN_08BIT, 0x20},	/* incksel3 */
	{0x315E, CRL_REG_LEN_08BIT, 0x1A},	/* incksel5 */
	{0x305F, CRL_REG_LEN_08BIT, 0x01},	/* incksel4 */
	{0x3360, CRL_REG_LEN_08BIT, 0x1E},
	{0x3361, CRL_REG_LEN_08BIT, 0x61},
	{0x3362, CRL_REG_LEN_08BIT, 0x10},
	{0x3164, CRL_REG_LEN_08BIT, 0x1A},	/* incksel6 */
	{0x3070, CRL_REG_LEN_08BIT, 0x02},
	{0x3071, CRL_REG_LEN_08BIT, 0x11},
	{0x317E, CRL_REG_LEN_08BIT, 0x00},
	{0x3480, CRL_REG_LEN_08BIT, 0x49},	/* inclsel7 */
	{0x309B, CRL_REG_LEN_08BIT, 0x10},
	{0x309C, CRL_REG_LEN_08BIT, 0x22},
	{0x30A2, CRL_REG_LEN_08BIT, 0x02},
	{0x30A6, CRL_REG_LEN_08BIT, 0x20},
	{0x30A8, CRL_REG_LEN_08BIT, 0x20},
	{0x30AA, CRL_REG_LEN_08BIT, 0x20},
	{0x30AC, CRL_REG_LEN_08BIT, 0x20},
	{0x30B0, CRL_REG_LEN_08BIT, 0x43},
	{0x33B0, CRL_REG_LEN_08BIT, 0x50},
	{0x33B2, CRL_REG_LEN_08BIT, 0x1A},
	{0x33B3, CRL_REG_LEN_08BIT, 0x04},
	{0x32B8, CRL_REG_LEN_08BIT, 0x50},
	{0x32B9, CRL_REG_LEN_08BIT, 0x10},
	{0x32BA, CRL_REG_LEN_08BIT, 0x00},
	{0x32BB, CRL_REG_LEN_08BIT, 0x04},
	{0x32C8, CRL_REG_LEN_08BIT, 0x50},
	{0x32C9, CRL_REG_LEN_08BIT, 0x10},
	{0x32CA, CRL_REG_LEN_08BIT, 0x00},
	{0x32CB, CRL_REG_LEN_08BIT, 0x04},
};

static struct crl_register_write_rep imx290_fmt_raw10[] = {
	{0x3005, CRL_REG_LEN_08BIT, 0x00},	/* ADBIT */
	{0x300A, CRL_REG_LEN_08BIT, 0x3C},	/* BLKLEVEL */
	{0x3129, CRL_REG_LEN_08BIT, 0x1D},	/* ADBIT1 */
	{0x3441, CRL_REG_LEN_08BIT, 0x0A},	/* CSI_DT_FMT */
	{0x3442, CRL_REG_LEN_08BIT, 0x0A},
	{0x3046, CRL_REG_LEN_08BIT, 0x00},	/* ODBIT OPORTSEL */
	{0x317C, CRL_REG_LEN_08BIT, 0x12},	/* ADBIT2 */
	{0x31EC, CRL_REG_LEN_08BIT, 0x37},	/* ADBIT3 */
};

static struct crl_register_write_rep imx290_fmt_raw12[] = {
	{0x3005, CRL_REG_LEN_08BIT, 0x01},	/* ADBIT  */
	{0x300A, CRL_REG_LEN_08BIT, 0xF0},	/* BLKLEVEL */
	{0x3129, CRL_REG_LEN_08BIT, 0x00},	/* ADBIT1 */
	{0x3441, CRL_REG_LEN_08BIT, 0x0C},	/* CSI_DT_FMT */
	{0x3442, CRL_REG_LEN_08BIT, 0x0C},
	{0x3046, CRL_REG_LEN_08BIT, 0x01},	/* ODBIT OPORTSEL */
	{0x317C, CRL_REG_LEN_08BIT, 0x00},	/* ADBIT2 */
	{0x31EC, CRL_REG_LEN_08BIT, 0x0E},	/* ADBIT3 */
};

static struct crl_register_write_rep imx290_powerup_standby[] = {
	{IMX290_REG_STANDBY, CRL_REG_LEN_08BIT, 0x01},
	{0x00, CRL_REG_LEN_DELAY, 20, 0x00},
	{IMX290_REG_XMSTA, CRL_REG_LEN_08BIT, 0x01},
};

/* Horizontal dumpy added 1097(1094+3) */
static struct crl_register_write_rep imx290_1948_1096_37MHZ_CROPPING[] = {
	/*TODO need  a test if necessary to open XMSTA*/
	{0x3000, CRL_REG_LEN_08BIT, 0x01},	/* reset to standby mode */
	{0x3002, CRL_REG_LEN_08BIT, 0x01},	/* default:reset slave mode */
	{0x3005, CRL_REG_LEN_08BIT, 0x01},	/* ADBIT */
	{0x3405, CRL_REG_LEN_08BIT, 0x20},	/* repetition */
	{0x3007, CRL_REG_LEN_08BIT, 0x04},	/* H/V verse and WINMODE */
	{0x3407, CRL_REG_LEN_08BIT, 0x03},	/* physical lane num(fixed) */
	{0x3009, CRL_REG_LEN_08BIT, 0x02},	/* FRSEL FDG_SEL */
	{0x300A, CRL_REG_LEN_08BIT, 0xF0},	/* BLKLEVEL */
	{0x300F, CRL_REG_LEN_08BIT, 0x00},	/* fixed setting */
	{0x3010, CRL_REG_LEN_08BIT, 0x21},
	{0x3012, CRL_REG_LEN_08BIT, 0x64},
	{0x3414, CRL_REG_LEN_08BIT, 0x0A},	/* OPB_SIZE_V */
	{0x3016, CRL_REG_LEN_08BIT, 0x09},
	{0x3018, CRL_REG_LEN_08BIT, 0x65},	/* VMAX */
	{0x3019, CRL_REG_LEN_08BIT, 0x04},
	{0x3418, CRL_REG_LEN_08BIT, 0x49},	/* Y_OUT_SIZE */
	{0x3419, CRL_REG_LEN_08BIT, 0x04},
	{0x3119, CRL_REG_LEN_08BIT, 0x9E},
	{0x301C, CRL_REG_LEN_08BIT, 0x30},	/* HMAX */
	{0x301D, CRL_REG_LEN_08BIT, 0x11},
	{0x311C, CRL_REG_LEN_08BIT, 0x1E},
	{0x311E, CRL_REG_LEN_08BIT, 0x08},
	{0x3128, CRL_REG_LEN_08BIT, 0x05},
	{0x3129, CRL_REG_LEN_08BIT, 0x00},	/* ADBIT1 */
	{0x332C, CRL_REG_LEN_08BIT, 0xD3},
	{0x332D, CRL_REG_LEN_08BIT, 0x10},
	{0x332E, CRL_REG_LEN_08BIT, 0x0D},
	{0x313D, CRL_REG_LEN_08BIT, 0x83},
	{0x3441, CRL_REG_LEN_08BIT, 0x0C},	/* CSI_DT_FMT */
	{0x3442, CRL_REG_LEN_08BIT, 0x0C},
	{0x3443, CRL_REG_LEN_08BIT, 0x03},	/* csi_lane_mode(fixed) */
	{0x3444, CRL_REG_LEN_08BIT, 0x20},	/* extck_freq */
	{0x3445, CRL_REG_LEN_08BIT, 0x25},
	{0x3046, CRL_REG_LEN_08BIT, 0x01},	/* ODBIT OPORTSEL */
	{0x3446, CRL_REG_LEN_08BIT, 0x47},	/* tclkpost */
	{0x3447, CRL_REG_LEN_08BIT, 0x00},
	{0x3448, CRL_REG_LEN_08BIT, 0x1F},	/* thszero */
	{0x3449, CRL_REG_LEN_08BIT, 0x00},
	{0x304B, CRL_REG_LEN_08BIT, 0x0A},	/* XH/VS OUTSEL */
	{0x344A, CRL_REG_LEN_08BIT, 0x17},	/* thsprepare */
	{0x344B, CRL_REG_LEN_08BIT, 0x00},
	{0x344C, CRL_REG_LEN_08BIT, 0x0F},	/* thstrail */
	{0x344D, CRL_REG_LEN_08BIT, 0x00},
	{0x344E, CRL_REG_LEN_08BIT, 0x17},	/* thstrail */
	{0x344F, CRL_REG_LEN_08BIT, 0x00},
	{0x3150, CRL_REG_LEN_08BIT, 0x03},
	{0x3450, CRL_REG_LEN_08BIT, 0x47},	/* tclkzero */
	{0x3451, CRL_REG_LEN_08BIT, 0x00},
	{0x3452, CRL_REG_LEN_08BIT, 0x0F},	/* tclkprepare */
	{0x3453, CRL_REG_LEN_08BIT, 0x00},
	{0x3454, CRL_REG_LEN_08BIT, 0x0F},	/* tlpx */
	{0x3455, CRL_REG_LEN_08BIT, 0x00},
	{0x3358, CRL_REG_LEN_08BIT, 0x06},	/* fixed setting */
	{0x3359, CRL_REG_LEN_08BIT, 0xE1},
	{0x335A, CRL_REG_LEN_08BIT, 0x11},
	{0x305C, CRL_REG_LEN_08BIT, 0x18},	/* incksel1 */
	{0x305D, CRL_REG_LEN_08BIT, 0x03},	/* incksel2 */
	{0x305E, CRL_REG_LEN_08BIT, 0x20},	/* incksel3 */
	{0x315E, CRL_REG_LEN_08BIT, 0x1A},	/* incksel5 */
	{0x305F, CRL_REG_LEN_08BIT, 0x01},	/* incksel4 */
	{0x3360, CRL_REG_LEN_08BIT, 0x1E},
	{0x3361, CRL_REG_LEN_08BIT, 0x61},
	{0x3362, CRL_REG_LEN_08BIT, 0x10},
	{0x3164, CRL_REG_LEN_08BIT, 0x1A},	/* incksel6 */
	{0x3070, CRL_REG_LEN_08BIT, 0x02},
	{0x3071, CRL_REG_LEN_08BIT, 0x11},
	{0x3472, CRL_REG_LEN_08BIT, 0x9C},	/* X_OUT_SIZE */
	{0x3473, CRL_REG_LEN_08BIT, 0x07},
	{0x317C, CRL_REG_LEN_08BIT, 0x00},	/* ADBIT2 */
	{0x317E, CRL_REG_LEN_08BIT, 0x00},
	{0x3480, CRL_REG_LEN_08BIT, 0x49},	/* inclsel7 */
	{0x309B, CRL_REG_LEN_08BIT, 0x10},
	{0x309C, CRL_REG_LEN_08BIT, 0x22},
	{0x30A2, CRL_REG_LEN_08BIT, 0x02},
	{0x30A6, CRL_REG_LEN_08BIT, 0x20},
	{0x30A8, CRL_REG_LEN_08BIT, 0x20},
	{0x30AA, CRL_REG_LEN_08BIT, 0x20},
	{0x30AC, CRL_REG_LEN_08BIT, 0x20},
	{0x30B0, CRL_REG_LEN_08BIT, 0x43},
	{0x33B0, CRL_REG_LEN_08BIT, 0x50},
	{0x33B2, CRL_REG_LEN_08BIT, 0x1A},
	{0x33B3, CRL_REG_LEN_08BIT, 0x04},
	{0x32B8, CRL_REG_LEN_08BIT, 0x50},
	{0x32B9, CRL_REG_LEN_08BIT, 0x10},
	{0x32BA, CRL_REG_LEN_08BIT, 0x00},
	{0x32BB, CRL_REG_LEN_08BIT, 0x04},
	{0x32C8, CRL_REG_LEN_08BIT, 0x50},
	{0x32C9, CRL_REG_LEN_08BIT, 0x10},
	{0x32CA, CRL_REG_LEN_08BIT, 0x00},
	{0x32CB, CRL_REG_LEN_08BIT, 0x04},
	{0x31EC, CRL_REG_LEN_08BIT, 0x0E},	/* ADBIT3 */
	/* WINDOW CROPPING */
	{0x303C, CRL_REG_LEN_08BIT, 0x01},
	{0x303D, CRL_REG_LEN_08BIT, 0x00},
	{0x303E, CRL_REG_LEN_08BIT, 0x48},
	{0x303F, CRL_REG_LEN_08BIT, 0x04},
};

static struct crl_register_write_rep imx290_1952_3435_37MHZ_CROPPING[] = {
	/*TODO need  a test if necessary to open XMSTA*/
	{0x3000, CRL_REG_LEN_08BIT, 0x01},	/* reset to standby mode */
	{0x3002, CRL_REG_LEN_08BIT, 0x01},	/* default:reset to slave mode */
	{0x3005, CRL_REG_LEN_08BIT, 0x00},	/* ADBIT */
	{0x3405, CRL_REG_LEN_08BIT, 0x00},	/* repetition */
	{0x3106, CRL_REG_LEN_08BIT, 0x33},
	{0x3007, CRL_REG_LEN_08BIT, 0x00},	/* H/V verse and WINMODE */
	{0x3407, CRL_REG_LEN_08BIT, 0x03},	/* physical lane num(fixed) */
	{0x3009, CRL_REG_LEN_08BIT, 0x00},	/* FRSEL FDG_SEL */
	{0x300A, CRL_REG_LEN_08BIT, 0x3C},	/* BLKLEVEL */
	{0x300C, CRL_REG_LEN_08BIT, 0x21},
	{0x300F, CRL_REG_LEN_08BIT, 0x00},	/* fixed setting */
	{0x3010, CRL_REG_LEN_08BIT, 0x21},
	{0x3012, CRL_REG_LEN_08BIT, 0x64},
	{0x3414, CRL_REG_LEN_08BIT, 0x0A},	/* OPB_SIZE_V */
	{0x3415, CRL_REG_LEN_08BIT, 0x00},
	{0x3016, CRL_REG_LEN_08BIT, 0x09},
	{0x3018, CRL_REG_LEN_08BIT, 0x65},	/* VMAX */
	{0x3019, CRL_REG_LEN_08BIT, 0x04},
	{0x3418, CRL_REG_LEN_08BIT, 0x55},	/* Y_OUT_SIZE */
	{0x3419, CRL_REG_LEN_08BIT, 0x11},
	{0x3119, CRL_REG_LEN_08BIT, 0x9E},
	{0x301C, CRL_REG_LEN_08BIT, 0x4C},	/* HMAX */
	{0x301D, CRL_REG_LEN_08BIT, 0x04},
	{0x311C, CRL_REG_LEN_08BIT, 0x1E},
	{0x311E, CRL_REG_LEN_08BIT, 0x08},
	{0x3020, CRL_REG_LEN_08BIT, 0x04},	/* SHS1 */
	{0x3021, CRL_REG_LEN_08BIT, 0x00},
	{0x3024, CRL_REG_LEN_08BIT, 0x89},	/* SHS2 */
	{0x3025, CRL_REG_LEN_08BIT, 0x00},
	{0x3028, CRL_REG_LEN_08BIT, 0x93},	/* SHS3 */
	{0x3029, CRL_REG_LEN_08BIT, 0x01},
	{0x3128, CRL_REG_LEN_08BIT, 0x05},
	{0x3129, CRL_REG_LEN_08BIT, 0x1D},	/* ADBIT1 */
	{0x332C, CRL_REG_LEN_08BIT, 0xD3},
	{0x332D, CRL_REG_LEN_08BIT, 0x10},
	{0x332E, CRL_REG_LEN_08BIT, 0x0D},
	{0x3030, CRL_REG_LEN_08BIT, 0x85},	/* RHS1 */
	{0x3031, CRL_REG_LEN_08BIT, 0x00},
	{0x3034, CRL_REG_LEN_08BIT, 0x92},	/* RHS2 */
	{0x3035, CRL_REG_LEN_08BIT, 0x00},
	{0x313D, CRL_REG_LEN_08BIT, 0x83},
	{0x3441, CRL_REG_LEN_08BIT, 0x0A},	/* CSI_DT_FMT */
	{0x3442, CRL_REG_LEN_08BIT, 0x0A},
	{0x3443, CRL_REG_LEN_08BIT, 0x03},	/* csi_lane_mode(fixed) */
	{0x3444, CRL_REG_LEN_08BIT, 0x20},	/* extck_freq */
	{0x3045, CRL_REG_LEN_08BIT, 0x05},	/* DOL sp */
	{0x3445, CRL_REG_LEN_08BIT, 0x25},
	{0x3046, CRL_REG_LEN_08BIT, 0x00},	/* ODBIT OPORTSEL */
	{0x3446, CRL_REG_LEN_08BIT, 0x77},	/* tclkpost */
	{0x3447, CRL_REG_LEN_08BIT, 0x00},
	{0x3448, CRL_REG_LEN_08BIT, 0x67},	/* thszero */
	{0x3449, CRL_REG_LEN_08BIT, 0x00},
	{0x304B, CRL_REG_LEN_08BIT, 0x0A},	/* XH/VS OUTSEL */
	{0x344A, CRL_REG_LEN_08BIT, 0x47},	/* thsprepare */
	{0x344B, CRL_REG_LEN_08BIT, 0x00},
	{0x344C, CRL_REG_LEN_08BIT, 0x37},	/* thstrail */
	{0x344D, CRL_REG_LEN_08BIT, 0x00},
	{0x344E, CRL_REG_LEN_08BIT, 0x3F},	/* thstrail */
	{0x344F, CRL_REG_LEN_08BIT, 0x00},
	{0x3150, CRL_REG_LEN_08BIT, 0x03},
	{0x3450, CRL_REG_LEN_08BIT, 0xFF},	/* tclkzero */
	{0x3451, CRL_REG_LEN_08BIT, 0x00},
	{0x3452, CRL_REG_LEN_08BIT, 0x3F},	/* tclkprepare */
	{0x3453, CRL_REG_LEN_08BIT, 0x00},
	{0x3454, CRL_REG_LEN_08BIT, 0x37},	/* tlpx */
	{0x3455, CRL_REG_LEN_08BIT, 0x00},
	{0x3358, CRL_REG_LEN_08BIT, 0x06},	/* fixed setting */
	{0x3359, CRL_REG_LEN_08BIT, 0xE1},
	{0x335A, CRL_REG_LEN_08BIT, 0x11},
	{0x305C, CRL_REG_LEN_08BIT, 0x18},	/* incksel1 */
	{0x305D, CRL_REG_LEN_08BIT, 0x03},	/* incksel2 */
	{0x305E, CRL_REG_LEN_08BIT, 0x20},	/* incksel3 */
	{0x315E, CRL_REG_LEN_08BIT, 0x1A},	/* incksel5 */
	{0x305F, CRL_REG_LEN_08BIT, 0x01},	/* incksel4 */
	{0x3360, CRL_REG_LEN_08BIT, 0x1E},
	{0x3361, CRL_REG_LEN_08BIT, 0x61},
	{0x3362, CRL_REG_LEN_08BIT, 0x10},
	{0x3164, CRL_REG_LEN_08BIT, 0x1A},	/* incksel6 */
	{0x3070, CRL_REG_LEN_08BIT, 0x02},
	{0x3071, CRL_REG_LEN_08BIT, 0x11},
	{0x3472, CRL_REG_LEN_08BIT, 0xA0},	/* X_OUT_SIZE */
	{0x3473, CRL_REG_LEN_08BIT, 0x07},
	{0x347B, CRL_REG_LEN_08BIT, 0x23},
	{0x317C, CRL_REG_LEN_08BIT, 0x12},	/* ADBIT2 */
	{0x317E, CRL_REG_LEN_08BIT, 0x00},
	{0x3480, CRL_REG_LEN_08BIT, 0x49},	/* inclsel7 */
	{0x309B, CRL_REG_LEN_08BIT, 0x10},
	{0x309C, CRL_REG_LEN_08BIT, 0x22},
	{0x30A2, CRL_REG_LEN_08BIT, 0x02},
	{0x30A6, CRL_REG_LEN_08BIT, 0x20},
	{0x30A8, CRL_REG_LEN_08BIT, 0x20},
	{0x30AA, CRL_REG_LEN_08BIT, 0x20},
	{0x30AC, CRL_REG_LEN_08BIT, 0x20},
	{0x30B0, CRL_REG_LEN_08BIT, 0x43},
	{0x33B0, CRL_REG_LEN_08BIT, 0x50},
	{0x33B2, CRL_REG_LEN_08BIT, 0x1A},
	{0x33B3, CRL_REG_LEN_08BIT, 0x04},
	{0x32B8, CRL_REG_LEN_08BIT, 0x50},
	{0x32B9, CRL_REG_LEN_08BIT, 0x10},
	{0x32BA, CRL_REG_LEN_08BIT, 0x00},
	{0x32BB, CRL_REG_LEN_08BIT, 0x04},
	{0x32C8, CRL_REG_LEN_08BIT, 0x50},
	{0x32C9, CRL_REG_LEN_08BIT, 0x10},
	{0x32CA, CRL_REG_LEN_08BIT, 0x00},
	{0x32CB, CRL_REG_LEN_08BIT, 0x04},
	{0x31EC, CRL_REG_LEN_08BIT, 0x37},	/* ADBIT3 */
};

static struct crl_register_write_rep imx290_streamon_regs[] = {
	{IMX290_REG_STANDBY, CRL_REG_LEN_08BIT, 0x00},
	{0x00, CRL_REG_LEN_DELAY, 50, 0x00},	/* Add a 50ms delay */
	{IMX290_REG_XMSTA, CRL_REG_LEN_08BIT, 0x00},
};

static struct crl_register_write_rep imx290_streamoff_regs[] = {
	{IMX290_REG_STANDBY, CRL_REG_LEN_08BIT, 0x01},
	{IMX290_REG_XMSTA, CRL_REG_LEN_08BIT, 0x01},
};

static struct crl_arithmetic_ops imx290_hflip_ops[] = {
	{
		.op = CRL_BITWISE_LSHIFT,
		.operand.entity_val = 1,
	}
};

static struct crl_dynamic_register_access imx290_h_flip_regs[] = {
	{
		.address = 0x3007,
		.len = CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE,
		.ops_items = ARRAY_SIZE(imx290_hflip_ops),
		.ops = imx290_hflip_ops,
		.mask = 0x2,
	}
};

static struct crl_dynamic_register_access imx290_v_flip_regs[] = {
	{
		.address = 0x3007,
		.len = CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE,
		.ops_items = 0,
		.ops = 0,
		.mask = 0x1,
	}
};

static struct crl_dynamic_register_access imx290_ana_gain_global_regs[] = {
	{
		.address = 0x3014,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xff,
	},
};

/* shs1[17:0] = fll - exposure - 1 */
static struct crl_arithmetic_ops imx290_shs1_lsb_ops[] = {
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

static struct crl_arithmetic_ops imx290_shs1_msb0_ops[] = {
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

static struct crl_arithmetic_ops imx290_shs1_msb1_ops[] = {
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
	},
	{
		.op = CRL_BITWISE_AND,
		.operand.entity_val = 0x03,
	}
};

static struct crl_dynamic_register_access imx290_shs1_regs[] = {
	{
		.address = 0x3020,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = ARRAY_SIZE(imx290_shs1_lsb_ops),
		.ops = imx290_shs1_lsb_ops,
		.mask = 0xff,
	},
	{
		.address = 0x3021,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = ARRAY_SIZE(imx290_shs1_msb0_ops),
		.ops = imx290_shs1_msb0_ops,
		.mask = 0xff,
	},
	{
		.address = 0x3022,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = ARRAY_SIZE(imx290_shs1_msb1_ops),
		.ops = imx290_shs1_msb1_ops,
		.mask = 0xff,
	},
};

static struct crl_arithmetic_ops imx290_fll_msb_ops[] = {
	{
		.op = CRL_BITWISE_RSHIFT,
		.operand.entity_val = 8,
	}
};

static struct crl_arithmetic_ops imx290_fll_hsb_ops[] = {
	{
		.op = CRL_BITWISE_RSHIFT,
		.operand.entity_val = 16,
	}
};

static struct crl_dynamic_register_access imx290_fll_regs[] = {
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
		.ops_items = ARRAY_SIZE(imx290_fll_msb_ops),
		.ops = imx290_fll_msb_ops,
		.mask = 0xff,
	},
	{
		.address = 0x301A,
		.len = CRL_REG_LEN_08BIT,
		.ops_items = ARRAY_SIZE(imx290_fll_hsb_ops),
		.ops = imx290_fll_hsb_ops,
		.mask = 0x3,
	},
};

static struct crl_dynamic_register_access imx290_llp_regs[] = {
	{
		.address = 0x301C,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	}
};

static struct crl_sensor_detect_config imx290_sensor_detect_regset[] = {
	{
		.reg = { 0x348F, CRL_REG_LEN_08BIT, 0x000000ff },
		.width = 7,
	},
	{
		.reg = { 0x348E, CRL_REG_LEN_08BIT, 0x000000ff },
		.width = 7,
	},
};

static struct crl_pll_configuration imx290_pll_configurations[] = {
	/*
	 * IMX290 supports only 37.125MHz and 72.25MHz input clocks.
	 * IPU4 supports up to 38.4MHz, however the sensor module we use
	 * has its own oscillator.
	 * The "input_clk" value is specified here for the reference.
	 */
	{
		.input_clk = 37125000,
		.op_sys_clk = 222750000,/* 445500000/2 */
		.bitsperpixel = 12,
		.pixel_rate_csi = 148500000,
		.pixel_rate_pa = 148500000,
		.csi_lanes = 4,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = ARRAY_SIZE(imx290_pll_445mbps),
		.pll_regs = imx290_pll_445mbps,
	},
	{
		.input_clk = 37125000,
		.op_sys_clk = 445500000,/* 891000000/2 */
		.bitsperpixel = 10,
		.pixel_rate_csi = 356400000,
		.pixel_rate_pa = 356400000,
		.csi_lanes = 4,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = ARRAY_SIZE(imx290_pll_891mbps),
		.pll_regs = imx290_pll_891mbps,
	}
};

/* Temporary use a single rect range */
static struct crl_subdev_rect_rep imx290_1948_1096_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1952,
		.in_rect.height = 3435,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1952,
		.out_rect.height = 3435,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1952,
		.in_rect.height = 3435,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1948,
		.out_rect.height = 1096,
	 }
};

static struct crl_subdev_rect_rep imx290_1952_3435_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1952,
		.in_rect.height = 3435,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1952,
		.out_rect.height = 3435,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1952,
		.in_rect.height = 3435,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1952,
		.out_rect.height = 3435,
	 }
};

static struct crl_mode_rep imx290_modes[] = {
	{
		.sd_rects_items = ARRAY_SIZE(imx290_1948_1096_rects),
		.sd_rects = imx290_1948_1096_rects,
		.binn_hor = 1,
		.binn_vert = 3,
		.scale_m = 1,
		.width = 1948,
		.height = 1096,
		.min_llp = 2220,
		.min_fll = 1112,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(imx290_1948_1096_37MHZ_CROPPING),
		.mode_regs = imx290_1948_1096_37MHZ_CROPPING,
		},
	{
		.sd_rects_items = ARRAY_SIZE(imx290_1952_3435_rects),
		.sd_rects = imx290_1952_3435_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 1952,
		.height = 3435,
		.min_llp = 2220,
		.min_fll = 1112,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(imx290_1952_3435_37MHZ_CROPPING),
		.mode_regs = imx290_1952_3435_37MHZ_CROPPING,
	},
};

struct crl_sensor_subdev_config imx290_sensor_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.name = "imx290 binner",
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "imx290 pixel array",
	}
};

static struct crl_sensor_limits imx290_sensor_limits = {
	.x_addr_min = 0,
	.y_addr_min = 0,
	.x_addr_max = 1952,
	.y_addr_max = 3435,
	.min_frame_length_lines = 320,
	.max_frame_length_lines = 65535,
	.min_line_length_pixels = 380,
	.max_line_length_pixels = 32752,
};

static struct crl_flip_data imx290_flip_configurations[] = {
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

static struct crl_csi_data_fmt imx290_crl_csi_data_fmt[] = {
	{
		.code = MEDIA_BUS_FMT_SGRBG10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.bits_per_pixel = 10,
		.regs_items = ARRAY_SIZE(imx290_fmt_raw10),
		.regs = imx290_fmt_raw10,
	},
	{
		.code = MEDIA_BUS_FMT_SRGGB10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,
		.bits_per_pixel = 10,
		.regs_items = ARRAY_SIZE(imx290_fmt_raw10),
		.regs = imx290_fmt_raw10,
	},
	{
		.code = MEDIA_BUS_FMT_SBGGR10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_BGGR,
		.bits_per_pixel = 10,
		.regs_items = ARRAY_SIZE(imx290_fmt_raw10),
		.regs = imx290_fmt_raw10,
	},
	{
		.code = MEDIA_BUS_FMT_SGBRG10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_GBRG,
		.bits_per_pixel = 10,
		.regs_items = ARRAY_SIZE(imx290_fmt_raw10),
		.regs = imx290_fmt_raw10,
	},
	{
		.code = MEDIA_BUS_FMT_SGRBG12_1X12,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.bits_per_pixel = 12,
		.regs_items = ARRAY_SIZE(imx290_fmt_raw12),
		.regs = imx290_fmt_raw12,
	},
	{
		.code = MEDIA_BUS_FMT_SRGGB12_1X12,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,	/*default pixel order*/
		.bits_per_pixel = 12,
		.regs_items = ARRAY_SIZE(imx290_fmt_raw12),
		.regs = imx290_fmt_raw12,
	},
	{
		.code = MEDIA_BUS_FMT_SBGGR12_1X12,
		.pixel_order = CRL_PIXEL_ORDER_BGGR,
		.bits_per_pixel = 12,
		.regs_items = ARRAY_SIZE(imx290_fmt_raw12),
		.regs = imx290_fmt_raw12,
	},
	{
		.code = MEDIA_BUS_FMT_SGBRG12_1X12,
		.pixel_order = CRL_PIXEL_ORDER_GBRG,
		.bits_per_pixel = 12,
		.regs_items = ARRAY_SIZE(imx290_fmt_raw12),
		.regs = imx290_fmt_raw12,
	}
};

static struct crl_v4l2_ctrl imx290_v4l2_ctrls[] = {
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
		.regs_items = ARRAY_SIZE(imx290_h_flip_regs),
		.regs = imx290_h_flip_regs,
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
		.regs_items = ARRAY_SIZE(imx290_v_flip_regs),
		.regs = imx290_v_flip_regs,
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
		.data.std_data.max = 240,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx290_ana_gain_global_regs),
		.regs = imx290_ana_gain_global_regs,
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
		.data.std_data.max = IMX290_MAX_SHS1,
		.data.std_data.step = 1,
		.data.std_data.def = 0x264,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx290_shs1_regs),
		.regs = imx290_shs1_regs,
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
		.data.std_data.max = IMX290_VMAX,
		.data.std_data.step = 1,
		.data.std_data.def = 1097,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx290_fll_regs),
		.regs = imx290_fll_regs,
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
		.data.std_data.min = 1948,
		.data.std_data.max = IMX290_HMAX,
		.data.std_data.step = 1,
		.data.std_data.def = 1948,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx290_llp_regs),
		.regs = imx290_llp_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
	},
};

static struct crl_arithmetic_ops imx290_frame_desc_width_ops[] = {
	{
		.op = CRL_ASSIGNMENT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_VAR_REF,
		.operand.entity_val = CRL_VAR_REF_OUTPUT_WIDTH,
	},
};

static struct crl_arithmetic_ops imx290_frame_desc_height_ops[] = {
	{
		.op = CRL_ASSIGNMENT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
		.operand.entity_val = 1,
	},
};

static struct crl_frame_desc imx290_frame_desc[] = {
	{
		.flags.entity_val = 0,
		.bpp.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_VAR_REF,
		.bpp.entity_val = CRL_VAR_REF_BITSPERPIXEL,
		.pixelcode.entity_val = MEDIA_BUS_FMT_FIXED,
		.length.entity_val = 0,
		.start_line.entity_val = 0,
		.start_pixel.entity_val = 0,
		.width = {
			.ops_items = ARRAY_SIZE(imx290_frame_desc_width_ops),
			.ops = imx290_frame_desc_width_ops,
			 },
		.height = {
			.ops_items = ARRAY_SIZE(imx290_frame_desc_height_ops),
			.ops = imx290_frame_desc_height_ops,
			},
		.csi2_channel.entity_val = 0,
		.csi2_data_type.entity_val = 0x12,
	},
};

static struct crl_power_seq_entity imx290_power_items[] = {
	/* If your sensor uses IPU reference clock, make sure it's enabled here. */
	{
		.type = CRL_POWER_ETY_GPIO_FROM_PDATA,
		.val = 1,
	},
};

struct crl_sensor_configuration imx290_crl_configuration = {

		.power_items = ARRAY_SIZE(imx290_power_items),
		.power_entities = imx290_power_items,

		.powerup_regs_items = ARRAY_SIZE(imx290_powerup_standby),
		.powerup_regs = imx290_powerup_standby,

		.poweroff_regs_items = 0,
		.poweroff_regs = 0,

		.id_reg_items = ARRAY_SIZE(imx290_sensor_detect_regset),
		.id_regs = imx290_sensor_detect_regset,

		.subdev_items = ARRAY_SIZE(imx290_sensor_subdevs),
		.subdevs = imx290_sensor_subdevs,

		.sensor_limits = &imx290_sensor_limits,

		.pll_config_items = ARRAY_SIZE(imx290_pll_configurations),
		.pll_configs = imx290_pll_configurations,

		.modes_items = ARRAY_SIZE(imx290_modes),
		.modes = imx290_modes,

		.streamon_regs_items = ARRAY_SIZE(imx290_streamon_regs),
		.streamon_regs = imx290_streamon_regs,

		.streamoff_regs_items = ARRAY_SIZE(imx290_streamoff_regs),
		.streamoff_regs = imx290_streamoff_regs,

		.v4l2_ctrls_items = ARRAY_SIZE(imx290_v4l2_ctrls),
		.v4l2_ctrl_bank = imx290_v4l2_ctrls,

		.csi_fmts_items = ARRAY_SIZE(imx290_crl_csi_data_fmt),
		.csi_fmts = imx290_crl_csi_data_fmt,

		.flip_items = ARRAY_SIZE(imx290_flip_configurations),
		.flip_data = imx290_flip_configurations,

		.frame_desc_entries = ARRAY_SIZE(imx290_frame_desc),
		.frame_desc_type = CRL_V4L2_MBUS_FRAME_DESC_TYPE_CSI2,
		.frame_desc = imx290_frame_desc,
};

#endif  /* __CRLMODULE_IMX274_CONFIGURATION_H_ */
