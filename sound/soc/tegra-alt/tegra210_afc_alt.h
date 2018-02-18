/*
 * tegra210_afc_alt.h - Definitions for Tegra210 AFC driver
 *
 * Copyright (c) 2014-2015 NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __TEGRA210_AFC_ALT_H__
#define __TEGRA210_AFC_ALT_H__

/*
 * AFC_AXBAR_RX registers are with respect to AXBAR.
 * The data is coming from AXBAR to AFC for playback.
 */
#define TEGRA210_AFC_AXBAR_RX_STATUS			0xc
#define TEGRA210_AFC_AXBAR_RX_CIF_CTRL			0x20
#define TEGRA210_AFC_AXBAR_RX_CYA				0x24

/*
 * AFC_AXBAR_TX registers are with respect to AXBAR.
 * The data is going out of AFC for playback.
 */
#define TEGRA210_AFC_AXBAR_TX_STATUS			0x4c
#define TEGRA210_AFC_AXBAR_TX_INT_STATUS		0x50
#define TEGRA210_AFC_AXBAR_TX_INT_MASK			0x54
#define TEGRA210_AFC_AXBAR_TX_INT_SET			0x58
#define TEGRA210_AFC_AXBAR_TX_INT_CLEAR			0x5c
#define TEGRA210_AFC_AXBAR_TX_CIF_CTRL			0x60
#define TEGRA210_AFC_AXBAR_TX_CYA				0x64

/* Register offsets from TEGRA210_AFC*_BASE */
#define TEGRA210_AFC_ENABLE			0x80
#define TEGRA210_AFC_SOFT_RESET		0x84
#define TEGRA210_AFC_CG				0x88
#define TEGRA210_AFC_STATUS			0x8c
#define TEGRA210_AFC_INT_STATUS		0x90
#define TEGRA210_AFC_INT_MASK		0x94
#define TEGRA210_AFC_INT_SET		0x98
#define TEGRA210_AFC_INT_CLEAR		0x9c

/* Miscellaneous AFC registers */
#define TEGRA210_AFC_DEST_I2S_PARAMS		0xa4
#define TEGRA210_AFC_TXCIF_FIFO_PARAMS		0xa8
#define TEGRA210_AFC_CLK_PPM_DIFF			0xac
#define TEGRA210_AFC_DBG_CTRL				0xb0
#define TEGRA210_AFC_TOTAL_SAMPLES			0xb4
#define TEGRA210_AFC_DECIMATION_SAMPLES		0xb8
#define TEGRA210_AFC_INTERPOLATION_SAMPLES	0xbc
#define TEGRA210_AFC_DBG_INTERNAL			0xc0

/* AFC coefficient registers */
#define TEGRA210_AFC_LCOEF_1_4_0		0xc4
#define TEGRA210_AFC_LCOEF_1_4_1		0xc8
#define TEGRA210_AFC_LCOEF_1_4_2		0xcc
#define TEGRA210_AFC_LCOEF_1_4_3		0xd0
#define TEGRA210_AFC_LCOEF_1_4_4		0xd4
#define TEGRA210_AFC_LCOEF_1_4_5		0xd8
#define TEGRA210_AFC_LCOEF_2_4_0		0xdc
#define TEGRA210_AFC_LCOEF_2_4_1		0xe0
#define TEGRA210_AFC_LCOEF_2_4_2		0xe4
#define TEGRA210_AFC_CYA				0xe8

/* Fields in TEGRA210_AFC_ENABLE */
#define TEGRA210_AFC_EN_SHIFT				0
#define TEGRA210_AFC_EN						(1 << TEGRA210_AFC_EN_SHIFT)

#define TEGRA210_AFC_DEST_I2S_ID_SHIFT			24
#define TEGRA210_AFC_FIFO_HIGH_THRESHOLD_SHIFT		16
#define TEGRA210_AFC_FIFO_START_THRESHOLD_SHIFT		8

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
#define XBAR_AFC_REG_OFFSET_DIVIDED_BY_4 0x34

#define MAX_NUM_I2S 5
#define MAX_NUM_AMX 2
#define MASK_AMX_TX 0x300

#define AFC_CLK_PPM_DIFF 50

#define CONFIG_AFC_DEST_PARAM(module_sel, module_id)\
	(module_id << TEGRA210_AFC_DEST_I2S_ID_SHIFT)

#define SET_AFC_DEST_PARAM(value)\
	regmap_write(afc->regmap,\
		TEGRA210_AFC_DEST_I2S_PARAMS, value)
#endif

struct tegra210_afc_soc_data {
	void (*set_audio_cif)(struct regmap *map,
			unsigned int reg,
			struct tegra210_xbar_cif_conf *conf);
};

struct tegra210_afc {
	unsigned int destination_i2s;
	struct regmap *regmap;
	const struct tegra210_afc_soc_data *soc_data;
};

unsigned int tegra210_afc_get_sfc_id(unsigned int afc_id);

#endif
