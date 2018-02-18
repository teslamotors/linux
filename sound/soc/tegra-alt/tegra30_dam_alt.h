/*
 * tegra30_dam_alt.h - Definitions for Tegra114 DAM driver
 *
 * Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHIN
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __TEGRA30_DAM_ALT_H__
#define __TEGRA30_DAM_ALT_H__

/* Register offsets from TEGRA_DAM*_BASE */
#define TEGRA_DAM_CTRL						0x00
#define TEGRA_DAM_CLIP						0x04
#define TEGRA_DAM_CLIP_THRESHOLD			0x08
#define TEGRA_DAM_AUDIOCIF_OUT_CTRL			0x0c

#define TEGRA_DAM_CH0_CTRL					0x10
#define TEGRA_DAM_CH0_CONV					0x14
#define TEGRA_DAM_AUDIOCIF_CH0_CTRL			0x1C

#define TEGRA_DAM_CH1_CTRL					0x20
#define TEGRA_DAM_CH1_CONV					0x24
#define TEGRA_DAM_AUDIOCIF_CH1_CTRL			0x2C

#define TEGRA_DAM_CH0_BIQUAD_FIXED_COEF		0xF0
#define TEGRA_DAM_FARROW_PARAM				0xF4
#define TEGRA_DAM_AUDIORAMCTL_DAM_CTRL		0xF8
#define TEGRA_DAM_AUDIORAMCTL_DAM_DATA		0xFC

/* Fields in TEGRA_DAM_CTRL */
#define TEGRA_DAM_CTRL_SOFT_RESET_SHIFT		31
#define TEGRA_DAM_CTRL_SOFT_RESET_EN		(1 << TEGRA_DAM_CTRL_SOFT_RESET_SHIFT)

#define TEGRA_DAM_CTRL_STEREO_SRC_SHIFT		2
#define TEGRA_DAM_CTRL_STEREO_SRC_EN_MASK	(1 << TEGRA_DAM_CTRL_STEREO_SRC_SHIFT)
#define TEGRA_DAM_CTRL_STEREO_SRC_EN		(1 << TEGRA_DAM_CTRL_STEREO_SRC_SHIFT)

#define TEGRA_DAM_CTRL_STEREO_MIX_SHIFT		3
#define TEGRA_DAM_CTRL_STEREO_MIX_EN_MASK	(1 << TEGRA_DAM_CTRL_STEREO_MIX_SHIFT)
#define TEGRA_DAM_CTRL_STEREO_MIX_EN		(1 << TEGRA_DAM_CTRL_STEREO_MIX_SHIFT)

#define TEGRA_DAM_CTRL_FSOUT_SHIFT			4
#define TEGRA_DAM_CTRL_FSOUT_MASK			(0xf << TEGRA_DAM_CTRL_FSOUT_SHIFT)

/* Fields in TEGRA_DAM_CH0_CTRL */
#define TEGRA_DAM_CH0_CTRL_FSIN_SHIFT		8
#define TEGRA_DAM_CH0_CTRL_FSIN_MASK		(0xf << TEGRA_DAM_CH0_CTRL_FSIN_SHIFT)

#define TEGRA_DAM_CH0_CTRL_COEF_RAM_SHIFT	15
#define TEGRA_DAM_CH0_CTRL_COEF_RAM_EN		(1 << TEGRA_DAM_CH0_CTRL_COEF_RAM_SHIFT)
#define TEGRA_DAM_CH0_CTRL_COEF_RAM_MASK	(1 << TEGRA_DAM_CH0_CTRL_COEF_RAM_SHIFT)

#define TEGRA_DAM_CH0_CTRL_FILT_STAGES_SHIFT	16
#define TEGRA_DAM_CH0_CTRL_FILT_STAGES_MASK		(0xf << TEGRA_DAM_CH0_CTRL_FILT_STAGES_SHIFT)

#define TEGRA_DAM_FS8						0
#define TEGRA_DAM_FS16						1
#define TEGRA_DAM_FS44						2
#define TEGRA_DAM_FS48						3
#define TEGRA_DAM_FS11						4
#define TEGRA_DAM_FS22						5
#define TEGRA_DAM_FS24						6
#define TEGRA_DAM_FS32						7
#define TEGRA_DAM_FS88						8
#define TEGRA_DAM_FS96						9
#define TEGRA_DAM_FS176						10
#define TEGRA_DAM_FS192						11

#define TEGRA_DAM_FARROW_PARAM_1			0
#define TEGRA_DAM_FARROW_PARAM_2			0xdee993a0
#define TEGRA_DAM_FARROW_PARAM_3			0xcccda093
#define TEGRA_DAM_DEFAULT_GAIN				0x1000

struct tegra30_dam_soc_data {
	void (*set_audio_cif)(struct regmap *map,
			unsigned int reg,
			struct tegra30_xbar_cif_conf *conf);
};

struct tegra30_dam {
	struct clk *clk_dam;
	struct regmap *regmap;
	int srate_in;
	int srate_out;
	int in_ch0_gain;
	int in_ch1_gain;
	const struct tegra30_dam_soc_data *soc_data;
};

#endif
