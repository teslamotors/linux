/*
 * arch/arm64/mach-tegra/board-t210.h
 *
 * NVIDIA Tegra210 device tree board support
 *
 * Copyright (C) 2014-2015 NVIDIA Corporation. All rights reserved.
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

#ifndef _MACH_TEGRA_BOARD_T210_H_
#define _MACH_TEGRA_BOARD_T210_H_

int t210_emc_init(void);
int tegra21_emc_init(void);
int t210ref_camera_init(void);

#endif
