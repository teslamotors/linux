/*
 * Copyright (C) 2010 Google, Inc.
 * Copyright (c) 2010-2014, NVIDIA CORPORATION. All rights reserved.
 *
 * Author:
 *	Colin Cross <ccross@android.com>
 *	Sumit Sharma <sumsharma@nvidia.com>
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

#include <linux/tegra-soc.h>
#include <linux/sysfs.h>
#include <linux/platform_device.h>

#ifndef __TEGRA_FUSE_H
#define __TEGRA_FUSE_H

#define FUSE_FT_REV		0x128
#define FUSE_CP_REV		0x190

#define FUSE_SKU_USB_CALIB_0	0x1f0
#define FUSE_USB_CALIB_EXT_0	0x350

unsigned long long tegra_chip_uid(void);
void tegra_init_fuse(void);
bool tegra_spare_fuse(int bit);

u32 tegra_fuse_readl(unsigned long offset);
void tegra_fuse_writel(u32 val, unsigned long offset);

extern enum tegra_revision tegra_chip_get_revision(void);

extern int (*tegra_fuse_regulator_en)(int);
int tegra_soc_speedo_id(void);
void tegra_init_speedo_data(void);
int tegra_cpu_process_id(void);
int tegra_core_process_id(void);
int tegra_gpu_process_id(void);
int tegra_get_age(void);
#if defined(CONFIG_ARCH_TEGRA_12x_SOC) && !defined(CONFIG_ARCH_TEGRA_13x_SOC)
bool tegra_is_soc_automotive_speedo(void);
#else
static inline bool tegra_is_soc_automotive_speedo(void)
{
	return 0;
}
#endif

int tegra_package_id(void);
int tegra_cpu_speedo_id(void);
int tegra_cpu_speedo_mv(void);
int tegra_cpu_speedo_value(void);
int tegra_core_speedo_mv(void);
int tegra_core_speedo_min_mv(void);
int tegra_gpu_speedo_id(void);
int tegra_get_sku_override(void);
int tegra_get_cpu_iddq_value(void);
int tegra_get_chip_personality(void);

#if defined(CONFIG_ARCH_TEGRA_12x_SOC) || defined(CONFIG_ARCH_TEGRA_21x_SOC)
int tegra_cpu_speedo_0_value(void);
int tegra_cpu_speedo_1_value(void);
int tegra_soc_speedo_0_value(void);
int tegra_soc_speedo_1_value(void);
int tegra_soc_speedo_2_value(void);
int tegra_get_soc_iddq_value(void);
int tegra_get_gpu_iddq_value(void);
int tegra_gpu_speedo_value(void);
#endif

int tegra_fuse_get_tsensor_calib(int index, u32 *calib);
int tegra_fuse_calib_base_get_cp(u32 *base_cp, s32 *shifted_cp);
int tegra_fuse_calib_base_get_ft(u32 *base_ft, s32 *shifted_ft);
int tegra_fuse_calib_gpcpll_get_adc(int *slope_uv, int *intercept_uv);
bool tegra_fuse_can_use_na_gpcpll(void);

#endif /* TEGRA_FUSE_H */
