/*
 * arch/arm/mach-tegra/fuse.c
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *	Colin Cross <ccross@android.com>
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

unsigned long long tegra_chip_uid(void);
unsigned int tegra_spare_fuse(int bit);
int tegra_sku_id(void);
int tegra_cpu_process_id(void);
int tegra_core_process_id(void);
int tegra_soc_speedo_id(void);
void tegra_init_fuse(void);
void tegra_init_speedo_data(void);
u32 tegra_fuse_readl(unsigned long offset);
void tegra_fuse_writel(u32 value, unsigned long offset);
