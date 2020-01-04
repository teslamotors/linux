/*
 * arch/arm/mach-tegra/include/mach/kbc.h
 *
 * Platform definitions for tegra-kbc keyboard input driver
 *
 * Copyright (c) 2010, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef ASMARM_ARCH_TEGRA_KBC_H
#define ASMARM_ARCH_TEGRA_KBC_H

#include <linux/types.h>

#define KBC_MAX_GPIO 24
#define KBC_MAX_KPENT 8

#define KBC_MAX_ROW 16
#define KBC_MAX_COL 8

#define KBC_MAX_KEY (KBC_MAX_ROW*KBC_MAX_COL)

struct tegra_kbc_pin_cfg {
	bool is_row;
	bool is_col;
	unsigned char num;
};

struct tegra_kbc_wake_key {
	u8 row:4;
	u8 col:4;
};

struct tegra_kbc_platform_data {
	unsigned int debounce_cnt;
	unsigned int repeat_cnt;
	int wake_cnt; /* 0:wake on any key >1:wake on wake_cfg */
	int *plain_keycode;
	int *fn_keycode;
	int filter_keys;
	struct tegra_kbc_pin_cfg pin_cfg[KBC_MAX_GPIO];
	struct tegra_kbc_wake_key *wake_cfg;
};
#endif

