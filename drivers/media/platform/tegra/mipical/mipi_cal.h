/*
 * drivers/misc/mipi_cal.h
 *
 * Copyright (c) 2016, NVIDIA CORPORATION, All rights reserved.
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

#ifndef MIPI_CAL_H
#define MIPI_CAL_H

#define DSID	(1 << 31)
#define DSIC	(1 << 30)
#define DSIB	(1 << 29)
#define DSIA	(1 << 28)
#define CSIF	(1 << 25)
#define CSIE	(1 << 24)
#define CSID	(1 << 23)
#define CSIC	(1 << 22)
#define CSIB	(1 << 21)
#define CSIA	(1 << 20)

#if defined(CONFIG_ARCH_TEGRA_21x_SOC) || defined(CONFIG_ARCH_TEGRA_18x_SOC)
int tegra_mipi_bias_pad_enable(void);
int tegra_mipi_bias_pad_disable(void);
int tegra_mipi_calibration(int lanes);
int tegra_mipi_select_mode(int mode);
#else
static int tegra_mipi_bias_pad_enable(void) {return 0; }
static int tegra_mipi_bias_pad_disable(void) {return 0; }
static int tegra_mipi_calibration(int lanes) {return 0; }
static int tegra_mipi_select_mode(int mode) {return 0; }
#endif

#endif
