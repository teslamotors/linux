/*
 * tegra186_xbar_alt.h - Additional defines for T186
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

#ifndef __TEGRA186_XBAR_ALT_H__
#define __TEGRA186_XBAR_ALT_H__

#define DRV_NAME_T18X "2900800.ahub"

#define NUM_DAIS		110
#define NUM_MUX_WIDGETS		81
#define NUM_MUX_INPUT	84	/* size of TEGRA210_ROUTES + TEGRA186_ROUTES */

#define TEGRA186_XBAR_PART3_RX					0x600
#define TEGRA186_XBAR_AUDIO_RX_COUNT			115
#define TEGRA186_AUDIOCIF_CTRL_FIFO_SIZE_DOWNSHIFT_SHIFT		2

#define MAX_REGISTER_ADDR (TEGRA186_XBAR_PART3_RX +\
	(TEGRA210_XBAR_RX_STRIDE * (TEGRA186_XBAR_AUDIO_RX_COUNT - 1)))

#define TEGRA210_XBAR_REG_MASK_0	0xF3FFFFF
#define TEGRA210_XBAR_REG_MASK_1	0x3F310F1F
#define TEGRA210_XBAR_REG_MASK_2	0xFF3CF311
#define TEGRA210_XBAR_REG_MASK_3	0x3F0F00FF
#define TEGRA210_XBAR_UPDATE_MAX_REG	4

#define ADMAIF_BASE_ADDR	0x0290F000
#define I2S1_BASE_ADDR		0x02901000
#define I2S2_BASE_ADDR		0x02901100
#define I2S3_BASE_ADDR		0x02901200
#define I2S4_BASE_ADDR		0x02901300
#define I2S5_BASE_ADDR		0x02901400
#define I2S6_BASE_ADDR		0x02901500
#define AMX1_BASE_ADDR		0x02903000
#define AMX2_BASE_ADDR		0x02903100
#define AMX3_BASE_ADDR		0x02903200
#define AMX4_BASE_ADDR		0x02903300
#define ADX1_BASE_ADDR		0x02903800
#define ADX2_BASE_ADDR		0x02903900
#define ADX3_BASE_ADDR		0x02903a00
#define ADX4_BASE_ADDR		0x02903b00
#define AFC1_BASE_ADDR		0x02907000
#define AFC2_BASE_ADDR		0x02907100
#define AFC3_BASE_ADDR		0x02907200
#define AFC4_BASE_ADDR		0x02907300
#define AFC5_BASE_ADDR		0x02907400
#define AFC6_BASE_ADDR		0x02907500
#define SFC1_BASE_ADDR		0x02902000
#define SFC2_BASE_ADDR		0x02902200
#define SFC3_BASE_ADDR		0x02902400
#define SFC4_BASE_ADDR		0x02902600
#define MVC1_BASE_ADDR		0x0290A000
#define MVC2_BASE_ADDR		0x0290A200
#define IQC1_BASE_ADDR		0x0290E000
#define IQC2_BASE_ADDR		0x0290E200
#define DMIC1_BASE_ADDR	0x02904000
#define DMIC2_BASE_ADDR	0x02904100
#define DMIC3_BASE_ADDR	0x02904200
#define DMIC4_BASE_ADDR	0x02904300
#define OPE1_BASE_ADDR		0x02908000
#define AMIXER1_BASE_ADDR	0x0290BB00
#define SPDIF1_BASE_ADDR	0x02906000
#define ASRC1_BASE_ADDR	0x02910000
#define ARAD1_BASE_ADDR	0x0290E400
#define DSPK1_BASE_ADDR	0x02905000
#define DSPK2_BASE_ADDR	0x02905100

int tegra186_xbar_registration(struct platform_device *pdev);
int tegra210_xbar_codec_probe(struct snd_soc_codec *codec);
int tegra210_xbar_get_value_enum(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);
int tegra210_xbar_put_value_enum(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);

#endif
