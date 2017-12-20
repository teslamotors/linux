/*
 * arch/arm/mach-tegra/tegra_smmu.h
 *
 * Copyright (C) 2011 NVIDIA Corporation.
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
struct tegra_smmu_window {
	unsigned long start;
	unsigned long end;
};

extern struct tegra_smmu_window *tegra_smmu_window(int wnum);
extern int tegra_smmu_window_count(void);
