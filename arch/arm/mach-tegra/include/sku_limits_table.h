/*
 * arch/arm/mach-tegra/include/sku_limits_table.h
 *
 * Copyright (c) 2010, NVIDIA Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _MACH_TEGRA_BOARD_SKULIMITS_H
#define _MACH_TEGRA_BOARD_SKULIMITS_H

struct skued_core_limit {
	int sku;
	int numcore;
};

#define NUMCORE_LIMITS \
		{0x00, 2},  /*  0 - Dev (AP20 Unrestricted) */ \
		{0x01, 2},  /*  1 - Dev (AP20 Reserved)     */ \
		{0x02, 2},  /*  2 - Dev (AP20 Reserved)     */ \
		{0x03, 2},  /*  3 - Dev (AP20 Reserved)     */ \
		{0x04, 2},  /*  4 - T240                    */ \
		{0x05, 2},  /*  5 - Dev (AP20 Reserved)     */ \
		{0x06, 2},  /*  6 - Dev (AP20 Reserved)     */ \
		{0x07, 2},  /*  7 - AP20                    */ \
		{0x08, 2},  /*  8 - T20                     */ \
		{0x09, 2},  /*  9 - Dev (AP20 Reserved)     */ \
		{0x0a, 2},  /* 10 - Dev (AP20 Reserved)     */ \
		{0x0b, 2},  /* 11 - Dev (AP20 Reserved)     */ \
		{0x0c, 2},  /* 12 - Dev (AP20 Reserved)     */ \
		{0x0d, 2},  /* 13 - Dev (AP20 Reserved)     */ \
		{0x0e, 2},  /* 14 - Dev (AP20 Reserved)     */ \
		{0x0f, 2},  /* 15 - AP20H                   */ \
		{0x10, 2},  /* 16 - Dev (AP20 Reserved)     */ \
		{0x11, 2},  /* 17 - Dev (AP20 Reserved)     */ \
		{0x12, 2},  /* 18 - Dev (AP20 Reserved)     */ \
		{0x13, 2},  /* 19 - Dev (AP20 Reserved)     */ \
		{0x14, 2},  /* 20 - Dev (AP20 Reserved)     */ \
		{0x15, 2},  /* 21 - Dev (AP20 Reserved)     */ \
		{0x16, 2},  /* 22 - Dev (AP20 Reserved)     */ \
		{0x17, 2},  /* 23 - AP25                    */ \
		{0x18, 2},  /* 24 - TP25                    */ \
		{0x20, 1},  /*  20 - T20                    */ \
		{0x24, 1},  /*  24 - T20                    */ \
		{0x28, 2},  /*  28 - T20                    */ \
		{0x2C, 2},  /*  2C - T20-A03                */ \
		{0x60, 2},  /*  60 - T20                    */ \
		{0xFF, 2}  /*  FF - Dev (AP20 Reserved)    */
#endif
