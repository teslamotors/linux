/*
 * tegra186_afc_alt.c - Additional features for T186
 *
 * Copyright (c) 2015-2016 NVIDIA CORPORATION.  All rights reserved.
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
#include <sound/soc.h>
#include <linux/regmap.h>

#include "tegra210_xbar_alt.h"
#include "tegra210_afc_alt.h"
#include "tegra186_afc_alt.h"

static unsigned int tegra186_afc_get_i2s_id(unsigned int afc_id)
{
	unsigned int i2s_reg, i2s_val, amx_reg, amx_val, i, j;

	for (i = 1; i < MAX_NUM_I2S; i++) {
		i2s_val = 0;
		i2s_reg = TEGRA210_XBAR_PART1_RX +
			TEGRA210_XBAR_RX_STRIDE * (i + 0xF);
		tegra210_xbar_read_reg(i2s_reg, &i2s_val);
		if ((i2s_val >> 24) & (1 << afc_id)) {
			return i;
		} else if (i2s_val & MASK_AMX_TX) {
			for (j = 1; j < 9; j++) { /* AMX1/2 */
				amx_val = 0;
				amx_reg = TEGRA210_XBAR_PART1_RX +
					TEGRA210_XBAR_RX_STRIDE * (j + 0x4F);
				tegra210_xbar_read_reg(amx_reg, &amx_val);
				if ((amx_val >> 24) & (1 << afc_id))
					return i;
			}
			for (j = 1; j < 5; j++) { /* AMX3 */
				amx_val = 0;
				amx_reg = TEGRA210_XBAR_PART1_RX +
					TEGRA210_XBAR_RX_STRIDE *
					(j + 0x57);
				tegra210_xbar_read_reg(amx_reg, &amx_val);
				if ((amx_val >> 24) & (1 << afc_id))
					return i;
			}
			for (j = 1; j < 5; j++) { /* AMX4 */
				amx_val = 0;
				amx_reg = TEGRA210_XBAR_PART1_RX +
					TEGRA210_XBAR_RX_STRIDE * (j + 0x63);
				tegra210_xbar_read_reg(amx_reg, &amx_val);
				if ((amx_val >> 24) & (1 << afc_id))
					return i;
			}
		}
	}
	return 0;
}

/* returns the destination dspk id connected along the AFC path */
static unsigned int tegra186_afc_get_dspk_id(unsigned int afc_id)
{
	unsigned int dspk_reg, dspk_val, amx_reg, amx_val, i, j;

	for (i = 1; i < MAX_NUM_DSPK; i++) {
		dspk_val = 0;
		dspk_reg = TEGRA210_XBAR_PART1_RX +
			TEGRA210_XBAR_RX_STRIDE * (i + 0x2F);
		tegra210_xbar_read_reg(dspk_reg, &dspk_val);
		if ((dspk_val >> 24) & (1 << afc_id)) {
			return i;
		} else if (dspk_val & MASK_AMX_TX) {
			for (j = 1; j < 9; j++) {
				amx_val = 0;
				amx_reg = TEGRA210_XBAR_PART1_RX +
					TEGRA210_XBAR_RX_STRIDE * (j + 0x4F);
				tegra210_xbar_read_reg(amx_reg, &amx_val);
				if ((amx_val >> 24) & (1 << afc_id))
					return i;
			}
			for (j = 1; j < 5; j++) {
				amx_val = 0;
				amx_reg = TEGRA210_XBAR_PART1_RX +
					TEGRA210_XBAR_RX_STRIDE * (j + 0x57);
				tegra210_xbar_read_reg(amx_reg, &amx_val);
				if ((amx_val >> 24) & (1 << afc_id))
					return i;
			}
			for (j = 1; j < 5; j++) {
				amx_val = 0;
				amx_reg = TEGRA210_XBAR_PART1_RX +
					TEGRA210_XBAR_RX_STRIDE * (j + 0x63);
				tegra210_xbar_read_reg(amx_reg, &amx_val);
				if ((amx_val >> 24) & (1 << afc_id))
					return i;
			}
		}
	}
	return 0;
}

int tegra210_afc_set_thresholds(struct tegra210_afc *afc,
				unsigned int afc_id)
{
	unsigned int module_id, value = 0;
	unsigned int module_sel = 0;

	if (tegra210_afc_get_sfc_id(afc_id)) {
		/* TODO program thresholds using SRC_BURST */
	} else {
		value = 4 << TEGRA210_AFC_FIFO_HIGH_THRESHOLD_SHIFT;
		value |= 3 << TEGRA210_AFC_FIFO_START_THRESHOLD_SHIFT;
		value |= 2;
	}

	if (value)
		regmap_write(afc->regmap, TEGRA210_AFC_TXCIF_FIFO_PARAMS,
				value);

	module_id = tegra186_afc_get_i2s_id(afc_id);

	if (!module_id) {
		module_sel = 1;
		module_id = tegra186_afc_get_dspk_id(afc_id);
		if (!module_id)
			return -EINVAL;
	}

	value |= CONFIG_AFC_DEST_PARAM(module_sel, module_id);
	SET_AFC_DEST_PARAM(value);

	return 0;
}
EXPORT_SYMBOL_GPL(tegra210_afc_set_thresholds);
