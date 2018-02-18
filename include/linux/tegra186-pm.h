/*
 * Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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

#ifndef _LINUX_TEGRA186_PM_H_
#define _LINUX_TEGRA186_PM_H_

/* Core states */
#define TEGRA186_CPUIDLE_C1     1
#define TEGRA186_CPUIDLE_C6     6
#define TEGRA186_CPUIDLE_C7     7

/* Cluster states 10-19 */
#define TEGRA186_CPUIDLE_CC6    6
#define TEGRA186_CPUIDLE_CC7    7

/* SoC states 20-29 */
#define TEGRA186_CPUIDLE_SC2    2
#define TEGRA186_CPUIDLE_SC3    3
#define TEGRA186_CPUIDLE_SC4    4
#define TEGRA186_CPUIDLE_SC7    7

#endif
