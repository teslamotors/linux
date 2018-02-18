/*
 * tegra124_afc_alt.h - Definitions for Tegra124 AFC driver
 *
 * Copyright (c) 2014 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __TEGRA124_AFC_ALT_H__
#define __TEGRA124_AFC_ALT_H__

/* TEGRA_AFC_* register offsets from AFC base register */
#define TEGRA_AFC_CTRL					0x00
#define TEGRA_AFC_THRESHOLDS_I2S		0x04
#define TEGRA_AFC_THRESHOLDS_AFC		0x08
#define TEGRA_AFC_FLOW_STEP_SIZE		0x0c
#define TEGRA_AFC_AUDIOCIF_AFCTX_CTRL	0x10
#define TEGRA_AFC_AUDIOCIF_AFCRX_CTRL	0x14
#define TEGRA_AFC_FLOW_STATUS			0x18
#define TEGRA_AFC_FLOW_TOTAL			0x1c
#define TEGRA_AFC_FLOW_OVER				0x20
#define TEGRA_AFC_FLOW_UNDER			0x24
#define TEGRA_AFC_LCOEF_1_4_0			0x28
#define TEGRA_AFC_LCOEF_1_4_1			0x2c
#define TEGRA_AFC_LCOEF_1_4_2			0x30
#define TEGRA_AFC_LCOEF_1_4_3			0x34
#define TEGRA_AFC_LCOEF_1_4_4			0x38
#define TEGRA_AFC_LCOEF_1_4_5			0x3c
#define TEGRA_AFC_LCOEF_2_4_0			0x40
#define TEGRA_AFC_LCOEF_2_4_1			0x44
#define TEGRA_AFC_LCOEF_2_4_2			0x48
#define TEGRA_AFC_AFC_DEBUG				0x4c
#define TEGRA_AFC_CYA_0					0x50

/* Fields in TEGRA124_AFC_ENABLE */
#define TEGRA_AFC_EN_SHIFT				0
#define TEGRA_AFC_EN					(1 << TEGRA_AFC_EN_SHIFT)

#define TEGRA_AFC_DEST_I2S_ID_SHIFT				26
#define TEGRA_AFC_FIFO_HIGH_THRESHOLD_SHIFT		18
#define TEGRA_AFC_FIFO_START_THRESHOLD_SHIFT	9

struct tegra124_afc_soc_data {
	void (*set_audio_cif)(struct regmap *map,
			unsigned int reg,
			struct tegra30_xbar_cif_conf *conf);
};

struct tegra124_afc {
	struct clk *clk_afc;
	unsigned int destination_i2s;
	struct regmap *regmap;
	const struct tegra124_afc_soc_data *soc_data;
};

#endif

