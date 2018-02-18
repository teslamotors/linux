/*
 * Tegra SATA AUX interface from tegra misc
 *
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef __LINUX_TEGRA_SATA_AUX_H__
#define __LINUX_TEGRA_SATA_AUX_H__

struct tegra_sata_aux;

enum tegra_sata_aux_reg {
	TEGRA_SATA_AUX_PAD_PLL_CTRL0 = 0,
	TEGRA_SATA_AUX_PAD_PLL_CTRL1,
	TEGRA_SATA_AUX_PAD_PLL_CTRL2,
	TEGRA_SATA_AUX_MISC_CTRL1,
	TEGRA_SATA_AUX_RX_STAT_INT,
	TEGRA_SATA_AUX_RX_STAT_SET,
	TEGRA_SATA_AUX_RX_STAT_CLR,
	TEGRA_SATA_AUX_SPARE_CFG0,
	TEGRA_SATA_AUX_SPARE_CFG1,
};

unsigned long tegra_sata_aux_readl(struct tegra_sata_aux *sata_aux,
		enum tegra_sata_aux_reg reg_id);
void tegra_sata_aux_writel(struct tegra_sata_aux *sata_aux,
		enum tegra_sata_aux_reg reg_id, unsigned long val);
void tegra_sata_aux_update(struct tegra_sata_aux *sata_aux,
		enum tegra_sata_aux_reg reg_id, unsigned long mask,
		unsigned long val);

struct tegra_sata_aux *tegra_sata_aux_get(struct platform_device *pdev);
static inline void tegra_sata_aux_put(struct tegra_sata_aux *sata_aux)
{
}

#endif	/* __LINUX_TEGRA_SATA_AUX_H__ */
