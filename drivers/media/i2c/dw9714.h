/*
 * Copyright (c) 2015 Intel Corporation.
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
 * Based on ATOMISP dw9714 implementation by
 * Huang Shenbo <shenbo.huang@intel.com.
 */

#ifndef __DW9714_H__
#define __DW9714_H__

#include <linux/types.h>


#define DW9714_VCM_ADDR		0x0c
#define DW9714_NAME 		"dw9714"

enum dw9714_vcm_mode {
	DW9714_DIRECT = 0x1,	/* direct control */
	DW9714_LSC = 0x2,	/* linear slope control */
	DW9714_DLC = 0x3,	/* dual level control */
};

#define DW9714_INVALID_CONFIG	0xffffffff
#define DW9714_MAX_FOCUS_POS	1023


/* MCLK[1:0] = 01 T_SRC[4:0] = 00001 S[3:0] = 0111 */
#define DELAY_PER_STEP_NS	1000000
#define DELAY_MAX_PER_STEP_NS	(1000000 * 1023)

#define DLC_ENABLE 1
#define DLC_DISABLE 0
#define VCM_PROTECTION_OFF	0xeca3
#define VCM_PROTECTION_ON	0xdc51
#define VCM_DEFAULT_S 0x0

#define VCM_STEP_S(a) (u8)((a) & 0xf)
#define VCM_STEP_MCLK(a) (u8)(((a) >> 4) & 0x3)
#define VCM_DLC_MCLK(dlc, mclk) (u16)(((dlc) << 3) | (mclk) | 0xa104)
#define VCM_TSRC(tsrc) (u16)((tsrc) << 3 | 0xf200)
#define VCM_VAL(data, s) (u16)((data) << 4 | (s))
#define DIRECT_VCM VCM_DLC_MCLK(0, 0)

#endif
