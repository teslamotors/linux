/*
 * tegra210_ope_alt.h - Definitions for Tegra210 OPE driver
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

#ifndef __TEGRA210_OPE_ALT_H__
#define __TEGRA210_OPE_ALT_H__

/* Register offsets from TEGRA210_OPE*_BASE */
/*
 * OPE_AXBAR_RX registers are with respect to AXBAR.
 * The data is coming from AXBAR to OPE for playback.
 */
#define TEGRA210_OPE_AXBAR_RX_STATUS		0xc
#define TEGRA210_OPE_AXBAR_RX_INT_STATUS	0x10
#define TEGRA210_OPE_AXBAR_RX_INT_MASK		0x14
#define TEGRA210_OPE_AXBAR_RX_INT_SET		0x18
#define TEGRA210_OPE_AXBAR_RX_INT_CLEAR		0x1c
#define TEGRA210_OPE_AXBAR_RX_CIF_CTRL		0x20

/*
 * OPE_AXBAR_TX registers are with respect to AXBAR.
 * The data is going out of OPE for playback.
 */
#define TEGRA210_OPE_AXBAR_TX_STATUS		0x4c
#define TEGRA210_OPE_AXBAR_TX_INT_STATUS	0x50
#define TEGRA210_OPE_AXBAR_TX_INT_MASK		0x54
#define TEGRA210_OPE_AXBAR_TX_INT_SET		0x58
#define TEGRA210_OPE_AXBAR_TX_INT_CLEAR		0x5c
#define TEGRA210_OPE_AXBAR_TX_CIF_CTRL		0x60

/* OPE Gloabal registers */
#define TEGRA210_OPE_ENABLE			0x80
#define TEGRA210_OPE_SOFT_RESET			0x84
#define TEGRA210_OPE_CG				0x88
#define TEGRA210_OPE_STATUS			0x8c
#define TEGRA210_OPE_INT_STATUS			0x90
#define TEGRA210_OPE_DIRECTION			0x94

/* Fields for TEGRA210_OPE_ENABLE */
#define TEGRA210_OPE_EN_SHIFT			0
#define TEGRA210_OPE_EN				(1 << TEGRA210_OPE_EN_SHIFT)

/* Fields for TEGRA210_OPE_SOFT_RESET */
#define TEGRA210_OPE_SOFT_RESET_SHIFT		0
#define TEGRA210_OPE_SOFT_RESET_EN		(1 << TEGRA210_OPE_SOFT_RESET_SHIFT)

/* Fields for TEGRA210_OPE_DIRECTION */
#define TEGRA210_OPE_DIRECTION_SHIFT		0
#define TEGRA210_OPE_DIRECTION_MASK		(1 << TEGRA210_OPE_DIRECTION_SHIFT)
#define TEGRA210_OPE_DIRECTION_MBDRC_TO_PEQ	(0 << TEGRA210_OPE_DIRECTION_SHIFT)
#define TEGRA210_OPE_DIRECTION_PEQ_TO_MBDRC	(1 << TEGRA210_OPE_DIRECTION_SHIFT)
/* OPE register definitions end here */

#define TEGRA210_PEQ_IORESOURCE_MEM 1
#define TEGRA210_MBDRC_IORESOURCE_MEM 2

struct tegra210_ope_module_soc_data {
	int (*init)(struct platform_device *pdev, int id);
	int (*codec_init)(struct snd_soc_codec *codec);
};

struct tegra210_ope_soc_data {
	void (*set_audio_cif)(struct regmap *map,
			unsigned int reg,
			struct tegra210_xbar_cif_conf *conf);
	struct tegra210_ope_module_soc_data peq_soc_data;
	struct tegra210_ope_module_soc_data mbdrc_soc_data;
};

struct tegra210_ope {
	struct regmap *regmap;
	struct regmap *peq_regmap;
	struct regmap *mbdrc_regmap;
	const struct tegra210_ope_soc_data *soc_data;
};

extern int tegra210_peq_init(struct platform_device *pdev, int id);
extern int tegra210_peq_codec_init(struct snd_soc_codec *codec);
extern int tegra210_mbdrc_init(struct platform_device *pdev, int id);
extern int tegra210_mbdrc_codec_init(struct snd_soc_codec *codec);
#endif
