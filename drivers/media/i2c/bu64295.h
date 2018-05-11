/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2016 - 2018 Intel Corporation */
#ifndef __BU64295_H__
#define __BU64295_H__

#include <linux/types.h>

#define BU64295_VCM_ADDR	0x0c
#define BU64295_NAME		"bu64295"

enum bu64295_vcm_mode {
	BU64295_DIRECT = 0x0,	/* direct control */
	BU64295_ISRC = 0x1,	/* intelligent slew rate control */
};

#define BU64295_MAX_FOCUS_POS	1023

#define BU64295_TDAC_ADDR	0x0
#define BU64295_ISRC_ADDR	0x1
#define BU64295_RF_ADDR		0x2
#define BU64295_ST_ADDR		0x3
#define BU64295_SR_ADDR		0x4
#define BU64295_T1_ADDR		0x5
#define BU64295_T2_ADDR		0x6

#define BU64295_PS_ON		0x0
#define BU64295_PS_OFF		0x1

#define BU64295_SR_P8		0x1
#define BU64295_RF_81HZ		0x7F

#define VCM_VAL(ps, en, addr, mode, data) (u16)((ps << 15) |        \
		(en << 14) | (addr << 11) | (mode << 10) | (data & 0x3FF))

#endif
