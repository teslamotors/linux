/*
 * arch/arm/mach-tegra/board-whistler.h
 *
 * Copyright (C) 2010 Google, Inc.
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

#ifndef _MACH_TEGRA_BOARD_WHISTLER_H
#define _MACH_TEGRA_BOARD_WHISTLER_H

int whistler_regulator_init(void);
int whistler_sdhci_init(void);
int whistler_pinmux_init(void);
int whistler_panel_init(void);
int whistler_kbc_init(void);
int whistler_sensors_init(void);
#endif
