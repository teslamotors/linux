/*
 * tegra210_iqc_alt.h - Definitions for Tegra210 IQC driver
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

#ifndef __TEGRA210_IQC_ALT_H__
#define __TEGRA210_IQC_ALT_H__

#define TEGRA210_IQC_AXBAR_TX_STRIDE			0x04

/* Register offsets from TEGRA210_IQC*_BASE */

/*
 * IQC_AXBAR_TX registers are with respect to AXBAR.
 * The data is coming from IQC for record.
 */
#define TEGRA210_IQC_AXBAR_TX_STATUS			0x0c
#define TEGRA210_IQC_AXBAR_TX_INT_STATUS		0x10
#define TEGRA210_IQC_AXBAR_TX_INT_MASK			0x14
#define TEGRA210_IQC_AXBAR_TX_INT_SET			0x18
#define TEGRA210_IQC_AXBAR_TX_INT_CLEAR			0x1c
#define TEGRA210_IQC_AXBAR_TX_CIF_CTRL			0x20

#define TEGRA210_IQC_ENABLE						0x80
#define TEGRA210_IQC_SOFT_RESET					0x84
#define TEGRA210_IQC_CG							0x88
#define TEGRA210_IQC_STATUS						0x8c
#define TEGRA210_IQC_INT_STATUS					0x90
#define TEGRA210_IQC_CTRL						0xa0
#define TEGRA210_IQC_TIME_STAMP_STATUS_0		0xa4
#define TEGRA210_IQC_TIME_STAMP_STATUS_1		0xa8
#define TEGRA210_IQC_WS_EDGE_STATUS				0xac
#define TEGRA210_IQC_CYA						0xb0

/* Fields in TEGRA210_IQC_ENABLE */
#define TEGRA210_IQC_EN_SHIFT					0
#define TEGRA210_IQC_EN_MASK					(1 << TEGRA210_IQC_EN_SHIFT)
#define TEGRA210_IQC_EN							(1 << TEGRA210_IQC_EN_SHIFT)

/* Fields in TEGRA210_IQC_CTRL */
#define TEGRA210_IQC_CTRL_EDGE_CTRL_SHIFT		4
#define TEGRA210_IQC_CTRL_EDGE_CTRL_MASK		(1 << TEGRA210_IQC_CTRL_EDGE_CTRL_SHIFT)
#define TEGRA210_IQC_CTRL_EDGE_CTRL_NEG_EDGE	(1 << TEGRA210_IQC_CTRL_EDGE_CTRL_SHIFT)

#define TEGRA210_IQC_TIMESTAMP_SHIFT			5
#define TEGRA210_IQC_TIMESTAMP_MASK				(1 << TEGRA210_IQC_TIMESTAMP_SHIFT)
#define TEGRA210_IQC_TIMESTAMP_EN				(1 << TEGRA210_IQC_TIMESTAMP_SHIFT)

#define TEGRA210_IQC_WORD_SIZE_SHIFT			8
#define TEGRA210_IQC_WORD_SIZE_MASK				(0xf << TEGRA210_IQC_WORD_SIZE_SHIFT)

#define TEGRA210_IQC_WS_POLARITY_SHIFT			12
#define TEGRA210_IQC_WS_POLARITY_MASK			(1 << TEGRA210_IQC_WS_POLARITY_SHIFT)
#define TEGRA210_IQC_WS_POLARITY_HIGH			(1 << TEGRA210_IQC_WS_POLARITY_SHIFT)

#define TEGRA210_IQC_BIT_ORDER_SHIFT			16
#define TEGRA210_IQC_BIT_ORDER_MASK				(1 << TEGRA210_IQC_BIT_ORDER_SHIFT)
#define TEGRA210_IQC_LSB_FIRST					(1 << TEGRA210_IQC_BIT_ORDER_SHIFT)

#define TEGRA210_IQC_DATA_OFFSET_SHIFT			0
#define TEGRA210_IQC_DATA_OFFSET_MASK			(7 << TEGRA210_IQC_DATA_OFFSET_SHIFT)

struct tegra210_iqc_soc_data {
	void (*set_audio_cif)(struct regmap *map,
			unsigned int reg,
			struct tegra210_xbar_cif_conf *conf);
};

struct tegra210_iqc {
	struct clk *clk_iqc;
	struct regmap *regmap;
	unsigned int timestamp_enable;
	unsigned int data_offset;
	const struct tegra210_iqc_soc_data *soc_data;
};

#endif
