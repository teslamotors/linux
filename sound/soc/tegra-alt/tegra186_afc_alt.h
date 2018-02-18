/*
 * tegra186_afc_alt.h - Additional defines for T186
 *
 * Copyright (c) 2015 NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __TEGRA186_AFC_ALT_H__
#define __TEGRA186_AFC_ALT_H__

#define TEGRA186_AFC_MODULE_SELECT_SHIFT		27
#define TEGRA186_AFC_DEST_MODULE_ID_SHIFT		24
#define TEGRA186_AFC_DEST_OUTPUT_MODULE_PARAMS	0xa4

#define XBAR_AFC_REG_OFFSET_DIVIDED_BY_4 0x38

#define MAX_NUM_I2S 6
#define MAX_NUM_AMX 4
#define MAX_NUM_DSPK 2
#define MASK_AMX_TX 0xF00

#define AFC_CLK_PPM_DIFF 15

#define CONFIG_AFC_DEST_PARAM(module_sel, module_id)\
	((module_sel << TEGRA186_AFC_MODULE_SELECT_SHIFT) |\
	(module_id << TEGRA186_AFC_DEST_MODULE_ID_SHIFT))

#define SET_AFC_DEST_PARAM(value)\
	regmap_write(afc->regmap,\
		TEGRA186_AFC_DEST_OUTPUT_MODULE_PARAMS, value)

int tegra210_afc_set_thresholds(struct tegra210_afc *afc,
				unsigned int afc_id);

#endif
