/*
 * drivers/video/tegra/dc/dpaux.h
 *
 * Copyright (c) 2014 - 2016, NVIDIA CORPORATION, All rights reserved.
 * Author: Animesh Kishore <ankishore@nvidia.com>
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

#ifndef __DRIVER_VIDEO_TEGRA_DC_DPAUX_H__
#define __DRIVER_VIDEO_TEGRA_DC_DPAUX_H__

enum tegra_dpaux_pad_mode {
	TEGRA_DPAUX_PAD_MODE_AUX = 0,
	TEGRA_DPAUX_PAD_MODE_I2C = 1,
};

enum tegra_dpaux_instance {
	TEGRA_DPAUX_INSTANCE_0 = 0,
	TEGRA_DPAUX_INSTANCE_1 = 1,
	TEGRA_DPAUX_INSTANCE_N,
};

void tegra_set_dpaux_addr(void __iomem *dpaux_base,
				enum tegra_dpaux_instance id);
int tegra_dpaux_clk_en(struct device_node *np, enum tegra_dpaux_instance id);
void tegra_dpaux_clk_dis(struct device_node *np, enum tegra_dpaux_instance id);
void tegra_dpaux_pad_power(struct tegra_dc *dc,
			enum tegra_dpaux_instance id,
			bool on);
void tegra_dpaux_config_pad_mode(struct tegra_dc *dc,
				enum tegra_dpaux_instance id,
				enum tegra_dpaux_pad_mode mode);
void tegra_dpaux_prod_set_for_dp(struct tegra_dc *dc);
void tegra_dpaux_prod_set_for_hdmi(struct tegra_dc *dc);
#endif
