/*
 * tegra_asoc_machine_alt.c - Tegra xbar dai link for machine drivers
 *
 * Copyright (c) 2014-2016 NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/module.h>
#include <linux/of.h>
#include <sound/soc.h>

#include "tegra_asoc_machine_alt.h"

#define MAX_STR_SIZE		20

static struct snd_soc_dai_link *tegra_asoc_machine_links;
static struct snd_soc_codec_conf *tegra_asoc_codec_conf;
static unsigned int *bclk_ratio;
static unsigned int *tx_mask;
static unsigned int *rx_mask;
static unsigned int num_dai_links;

static const struct snd_soc_pcm_stream default_link_params = {
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
};

static const struct snd_soc_pcm_stream tdm_link_params = {
	.formats = SNDRV_PCM_FMTBIT_S32_LE,
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 8,
	.channels_max = 8,
};

static const char * const bit_format[] = {
	"s8", "u8", "s16_le", "s16_be",
	"u16_le", "u16_be", "s24_le", "s24_be",
	"u24_le", "u24_be", "s32_le", "s32_be",
	"u32_le", "u32_be", "float_le", "float_be",

	"float64_le", "float64_be", "iec958_subframe_le", "iec958_subframe_be",
	"mu_law", "a_law", "ima_adpcm", "mpeg",
	"gsm", "", "", "",
	"", "", "", "special",

	"s24_3l", "s24_3be", "u24_3le", "u24_3be",
	"s20_3le", "s20_3be", "u20_3le", "u20_3be",
	"s18_3le", "s18_3b", "u18_3le", "u18_3be",
	"g723_24", "g723_24_1b", "g723_40", "g723_40_1b",

	"dsd_u8", "dsd_u16_le",
};

static struct snd_soc_dai_link
	tegra124_xbar_dai_links[TEGRA124_XBAR_DAI_LINKS] = {
	[TEGRA124_DAI_LINK_APBIF0] = {
		.name = "APBIF0 CIF",
		.stream_name = "APBIF0 CIF",
		.cpu_dai_name = "APBIF0",
		.codec_dai_name = "APBIF0",
		.cpu_name = "tegra30-ahub-apbif",
		.codec_name = "tegra30-ahub-xbar",
		.platform_name = "tegra30-ahub-apbif",
		.ignore_pmdown_time = 1,
	},
	[TEGRA124_DAI_LINK_APBIF1] = {
		.name = "APBIF1 CIF",
		.stream_name = "APBIF1 CIF",
		.cpu_dai_name = "APBIF1",
		.codec_dai_name = "APBIF1",
		.cpu_name = "tegra30-ahub-apbif",
		.codec_name = "tegra30-ahub-xbar",
		.platform_name = "tegra30-ahub-apbif",
		.ignore_pmdown_time = 1,
	},
	[TEGRA124_DAI_LINK_APBIF2] = {
		.name = "APBIF2 CIF",
		.stream_name = "APBIF2 CIF",
		.cpu_dai_name = "APBIF2",
		.codec_dai_name = "APBIF2",
		.cpu_name = "tegra30-ahub-apbif",
		.codec_name = "tegra30-ahub-xbar",
		.platform_name = "tegra30-ahub-apbif",
		.ignore_pmdown_time = 1,
	},
	[TEGRA124_DAI_LINK_APBIF3] = {
		.name = "APBIF3 CIF",
		.stream_name = "APBIF3 CIF",
		.cpu_dai_name = "APBIF3",
		.codec_dai_name = "APBIF3",
		.cpu_name = "tegra30-ahub-apbif",
		.codec_name = "tegra30-ahub-xbar",
		.platform_name = "tegra30-ahub-apbif",
		.ignore_pmdown_time = 1,
	},
	[TEGRA124_DAI_LINK_APBIF4] = {
		.name = "APBIF4 CIF",
		.stream_name = "APBIF4 CIF",
		.cpu_dai_name = "APBIF4",
		.codec_dai_name = "APBIF4",
		.cpu_name = "tegra30-ahub-apbif",
		.codec_name = "tegra30-ahub-xbar",
		.platform_name = "tegra30-ahub-apbif",
		.ignore_pmdown_time = 1,
	},
	[TEGRA124_DAI_LINK_APBIF5] = {
		.name = "APBIF5 CIF",
		.stream_name = "APBIF5 CIF",
		.cpu_dai_name = "APBIF5",
		.codec_dai_name = "APBIF5",
		.cpu_name = "tegra30-ahub-apbif",
		.codec_name = "tegra30-ahub-xbar",
		.platform_name = "tegra30-ahub-apbif",
		.ignore_pmdown_time = 1,
	},
	[TEGRA124_DAI_LINK_APBIF6] = {
		.name = "APBIF6 CIF",
		.stream_name = "APBIF6 CIF",
		.cpu_dai_name = "APBIF6",
		.codec_dai_name = "APBIF6",
		.cpu_name = "tegra30-ahub-apbif",
		.codec_name = "tegra30-ahub-xbar",
		.platform_name = "tegra30-ahub-apbif",
		.ignore_pmdown_time = 1,
	},
	[TEGRA124_DAI_LINK_APBIF7] = {
		.name = "APBIF7 CIF",
		.stream_name = "APBIF7 CIF",
		.cpu_dai_name = "APBIF7",
		.codec_dai_name = "APBIF7",
		.cpu_name = "tegra30-ahub-apbif",
		.codec_name = "tegra30-ahub-xbar",
		.platform_name = "tegra30-ahub-apbif",
		.ignore_pmdown_time = 1,
	},
	[TEGRA124_DAI_LINK_APBIF8] = {
		.name = "APBIF8 CIF",
		.stream_name = "APBIF8 CIF",
		.cpu_dai_name = "APBIF8",
		.codec_dai_name = "APBIF8",
		.cpu_name = "tegra30-ahub-apbif",
		.codec_name = "tegra30-ahub-xbar",
		.platform_name = "tegra30-ahub-apbif",
		.ignore_pmdown_time = 1,
	},
	[TEGRA124_DAI_LINK_APBIF9] = {
		.name = "APBIF9 CIF",
		.stream_name = "APBIF9 CIF",
		.cpu_dai_name = "APBIF9",
		.codec_dai_name = "APBIF9",
		.cpu_name = "tegra30-ahub-apbif",
		.codec_name = "tegra30-ahub-xbar",
		.platform_name = "tegra30-ahub-apbif",
		.ignore_pmdown_time = 1,
	},
	[TEGRA124_DAI_LINK_AMX0_0] = {
		.name = "AMX0 IN0",
		.stream_name = "AMX0 IN",
		.cpu_dai_name = "AMX0-0",
		.codec_dai_name = "IN0",
		.cpu_name = "tegra30-ahub-xbar",
		.codec_name = "tegra124-amx.0",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_AMX0_1] = {
		.name = "AMX0 IN1",
		.stream_name = "AMX0 IN",
		.cpu_dai_name = "AMX0-1",
		.codec_dai_name = "IN1",
		.cpu_name = "tegra30-ahub-xbar",
		.codec_name = "tegra124-amx.0",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_AMX0_2] = {
		.name = "AMX0 IN2",
		.stream_name = "AMX0 IN",
		.cpu_dai_name = "AMX0-2",
		.codec_dai_name = "IN2",
		.cpu_name = "tegra30-ahub-xbar",
		.codec_name = "tegra124-amx.0",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_AMX0_3] = {
		.name = "AMX0 IN3",
		.stream_name = "AMX0 IN",
		.cpu_dai_name = "AMX0-3",
		.codec_dai_name = "IN3",
		.cpu_name = "tegra30-ahub-xbar",
		.codec_name = "tegra124-amx.0",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_AMX0] = {
		.name = "AMX0 OUT",
		.stream_name = "AMX0 OUT",
		.cpu_dai_name = "OUT",
		.codec_dai_name = "AMX0",
		.cpu_name = "tegra124-amx.0",
		.codec_name = "tegra30-ahub-xbar",
		.params = &tdm_link_params,
	},
	[TEGRA124_DAI_LINK_AMX1_0] = {
		.name = "AMX1 IN0",
		.stream_name = "AMX1 IN",
		.cpu_dai_name = "AMX1-0",
		.codec_dai_name = "IN0",
		.cpu_name = "tegra30-ahub-xbar",
		.codec_name = "tegra124-amx.1",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_AMX1_1] = {
		.name = "AMX1 IN1",
		.stream_name = "AMX1 IN",
		.cpu_dai_name = "AMX1-1",
		.codec_dai_name = "IN1",
		.cpu_name = "tegra30-ahub-xbar",
		.codec_name = "tegra124-amx.1",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_AMX1_2] = {
		.name = "AMX1 IN2",
		.stream_name = "AMX1 IN",
		.cpu_dai_name = "AMX1-2",
		.codec_dai_name = "IN2",
		.cpu_name = "tegra30-ahub-xbar",
		.codec_name = "tegra124-amx.1",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_AMX1_3] = {
		.name = "AMX1 IN3",
		.stream_name = "AMX1 IN",
		.cpu_dai_name = "AMX1-3",
		.codec_dai_name = "IN3",
		.cpu_name = "tegra30-ahub-xbar",
		.codec_name = "tegra124-amx.1",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_AMX1] = {
		.name = "AMX1 OUT",
		.stream_name = "AMX1 OUT",
		.cpu_dai_name = "OUT",
		.codec_dai_name = "AMX1",
		.cpu_name = "tegra124-amx.1",
		.codec_name = "tegra30-ahub-xbar",
		.params = &tdm_link_params,
	},
	[TEGRA124_DAI_LINK_ADX0] = {
		.name = "ADX0 CIF",
		.stream_name = "ADX0 IN",
		.cpu_dai_name = "ADX0",
		.codec_dai_name = "IN",
		.cpu_name = "tegra30-ahub-xbar",
		.codec_name = "tegra124-adx.0",
		.params = &tdm_link_params,
	},
	[TEGRA124_DAI_LINK_ADX0_0] = {
		.name = "ADX0 OUT0",
		.stream_name = "ADX0 OUT",
		.cpu_dai_name = "OUT0",
		.codec_dai_name = "ADX0-0",
		.cpu_name = "tegra124-adx.0",
		.codec_name = "tegra30-ahub-xbar",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_ADX0_1] = {
		.name = "ADX0 OUT1",
		.stream_name = "ADX0 OUT",
		.cpu_dai_name = "OUT1",
		.codec_dai_name = "ADX0-1",
		.cpu_name = "tegra124-adx.0",
		.codec_name = "tegra30-ahub-xbar",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_ADX0_2] = {
		.name = "ADX0 OUT2",
		.stream_name = "ADX0 OUT",
		.cpu_dai_name = "OUT2",
		.codec_dai_name = "ADX0-2",
		.cpu_name = "tegra124-adx.0",
		.codec_name = "tegra30-ahub-xbar",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_ADX0_3] = {
		.name = "ADX0 OUT3",
		.stream_name = "ADX0 OUT",
		.cpu_dai_name = "OUT3",
		.codec_dai_name = "ADX0-3",
		.cpu_name = "tegra124-adx.0",
		.codec_name = "tegra30-ahub-xbar",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_ADX1] = {
		.name = "ADX1 CIF",
		.stream_name = "ADX1 IN",
		.cpu_dai_name = "ADX1",
		.codec_dai_name = "IN",
		.cpu_name = "tegra30-ahub-xbar",
		.codec_name = "tegra124-adx.1",
		.params = &tdm_link_params,
	},
	[TEGRA124_DAI_LINK_ADX1_0] = {
		.name = "ADX1 OUT0",
		.stream_name = "ADX1 OUT",
		.cpu_dai_name = "OUT0",
		.codec_dai_name = "ADX1-0",
		.cpu_name = "tegra124-adx.1",
		.codec_name = "tegra30-ahub-xbar",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_ADX1_1] = {
		.name = "ADX1 OUT1",
		.stream_name = "ADX1 OUT",
		.cpu_dai_name = "OUT1",
		.codec_dai_name = "ADX1-1",
		.cpu_name = "tegra124-adx.1",
		.codec_name = "tegra30-ahub-xbar",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_ADX1_2] = {
		.name = "ADX1 OUT2",
		.stream_name = "ADX1 OUT",
		.cpu_dai_name = "OUT2",
		.codec_dai_name = "ADX1-2",
		.cpu_name = "tegra124-adx.1",
		.codec_name = "tegra30-ahub-xbar",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_ADX1_3] = {
		.name = "ADX1 OUT3",
		.stream_name = "ADX1 OUT",
		.cpu_dai_name = "OUT3",
		.codec_dai_name = "ADX1-3",
		.cpu_name = "tegra124-adx.1",
		.codec_name = "tegra30-ahub-xbar",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_DAM0_0] = {
		.name = "DAM0 IN0",
		.stream_name = "DAM0 IN0",
		.cpu_dai_name = "DAM0-0",
		.codec_dai_name = "IN0",
		.cpu_name = "tegra30-ahub-xbar",
		.codec_name = "tegra30-dam.0",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_DAM0_1] = {
		.name = "DAM0 IN1",
		.stream_name = "DAM0 IN1",
		.cpu_dai_name = "DAM0-1",
		.codec_dai_name = "IN1",
		.cpu_name = "tegra30-ahub-xbar",
		.codec_name = "tegra30-dam.0",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_DAM0] = {
		.name = "DAM0 OUT",
		.stream_name = "DAM0 OUT",
		.cpu_dai_name = "OUT",
		.codec_dai_name = "DAM0",
		.cpu_name = "tegra30-dam.0",
		.codec_name = "tegra30-ahub-xbar",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_DAM1_0] = {
		.name = "DAM1 IN0",
		.stream_name = "DAM1 IN0",
		.cpu_dai_name = "DAM1-0",
		.codec_dai_name = "IN0",
		.cpu_name = "tegra30-ahub-xbar",
		.codec_name = "tegra30-dam.1",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_DAM1_1] = {
		.name = "DAM1 IN1",
		.stream_name = "DAM1 IN1",
		.cpu_dai_name = "DAM1-1",
		.codec_dai_name = "IN1",
		.cpu_name = "tegra30-ahub-xbar",
		.codec_name = "tegra30-dam.1",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_DAM1] = {
		.name = "DAM1 OUT",
		.stream_name = "DAM1 OUT",
		.cpu_dai_name = "OUT",
		.codec_dai_name = "DAM1",
		.cpu_name = "tegra30-dam.1",
		.codec_name = "tegra30-ahub-xbar",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_DAM2_0] = {
		.name = "DAM2 IN0",
		.stream_name = "DAM2 IN0",
		.cpu_dai_name = "DAM2-0",
		.codec_dai_name = "IN0",
		.cpu_name = "tegra30-ahub-xbar",
		.codec_name = "tegra30-dam.2",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_DAM2_1] = {
		.name = "DAM2 IN1",
		.stream_name = "DAM2 IN1",
		.cpu_dai_name = "DAM2-1",
		.codec_dai_name = "IN1",
		.cpu_name = "tegra30-ahub-xbar",
		.codec_name = "tegra30-dam.2",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_DAM2] = {
		.name = "DAM2 OUT",
		.stream_name = "DAM2 OUT",
		.cpu_dai_name = "OUT",
		.codec_dai_name = "DAM2",
		.cpu_name = "tegra30-dam.2",
		.codec_name = "tegra30-ahub-xbar",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_AFC0_RX] = {
		.name = "AFC0 RX",
		.stream_name = "AFC0 RX",
		.cpu_dai_name = "AFC0",
		.codec_dai_name = "AFC IN",
		.cpu_name = "tegra30-ahub-xbar",
		.codec_name = "tegra124-afc.0",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_AFC0_TX] = {
		.name = "AFC0 TX",
		.stream_name = "AFC0 TX",
		.cpu_dai_name = "AFC OUT",
		.codec_dai_name = "AFC0",
		.cpu_name = "tegra124-afc.0",
		.codec_name = "tegra30-ahub-xbar",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_AFC1_RX] = {
		.name = "AFC1 RX",
		.stream_name = "AFC1 RX",
		.cpu_dai_name = "AFC1",
		.codec_dai_name = "AFC IN",
		.cpu_name = "tegra30-ahub-xbar",
		.codec_name = "tegra124-afc.1",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_AFC1_TX] = {
		.name = "AFC1 TX",
		.stream_name = "AFC1 TX",
		.cpu_dai_name = "AFC OUT",
		.codec_dai_name = "AFC1",
		.cpu_name = "tegra124-afc.1",
		.codec_name = "tegra30-ahub-xbar",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_AFC2_RX] = {
		.name = "AFC2 RX",
		.stream_name = "AFC2 RX",
		.cpu_dai_name = "AFC2",
		.codec_dai_name = "AFC IN",
		.cpu_name = "tegra30-ahub-xbar",
		.codec_name = "tegra124-afc.2",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_AFC2_TX] = {
		.name = "AFC2 TX",
		.stream_name = "AFC2 TX",
		.cpu_dai_name = "AFC OUT",
		.codec_dai_name = "AFC2",
		.cpu_name = "tegra124-afc.2",
		.codec_name = "tegra30-ahub-xbar",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_AFC3_RX] = {
		.name = "AFC3 RX",
		.stream_name = "AFC3 RX",
		.cpu_dai_name = "AFC3",
		.codec_dai_name = "AFC IN",
		.cpu_name = "tegra30-ahub-xbar",
		.codec_name = "tegra124-afc.3",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_AFC3_TX] = {
		.name = "AFC3 TX",
		.stream_name = "AFC3 TX",
		.cpu_dai_name = "AFC OUT",
		.codec_dai_name = "AFC3",
		.cpu_name = "tegra124-afc.3",
		.codec_name = "tegra30-ahub-xbar",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_AFC4_RX] = {
		.name = "AFC4 RX",
		.stream_name = "AFC4 RX",
		.cpu_dai_name = "AFC4",
		.codec_dai_name = "AFC IN",
		.cpu_name = "tegra30-ahub-xbar",
		.codec_name = "tegra124-afc.4",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_AFC4_TX] = {
		.name = "AFC4 TX",
		.stream_name = "AFC4 TX",
		.cpu_dai_name = "AFC OUT",
		.codec_dai_name = "AFC4",
		.cpu_name = "tegra124-afc.4",
		.codec_name = "tegra30-ahub-xbar",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_AFC5_RX] = {
		.name = "AFC5 RX",
		.stream_name = "AFC5 RX",
		.cpu_dai_name = "AFC5",
		.codec_dai_name = "AFC IN",
		.cpu_name = "tegra30-ahub-xbar",
		.codec_name = "tegra124-afc.5",
		.params = &default_link_params,
	},
	[TEGRA124_DAI_LINK_AFC5_TX] = {
		.name = "AFC5 TX",
		.stream_name = "AFC5 TX",
		.cpu_dai_name = "AFC OUT",
		.codec_dai_name = "AFC5",
		.cpu_name = "tegra124-afc.5",
		.codec_name = "tegra30-ahub-xbar",
		.params = &default_link_params,
	},
};

static struct snd_soc_codec_conf
	tegra124_xbar_codec_conf[TEGRA124_XBAR_CODEC_CONF] = {
	[TEGRA124_CODEC_AMX0_CONF] = {
		.dev_name = "tegra124-amx.0",
		.name_prefix = "AMX0",
	},
	[TEGRA124_CODEC_AMX1_CONF] = {
		.dev_name = "tegra124-amx.1",
		.name_prefix = "AMX1",
	},
	[TEGRA124_CODEC_ADX0_CONF] = {
		.dev_name = "tegra124-adx.0",
		.name_prefix = "ADX0",
	},
	[TEGRA124_CODEC_ADX1_CONF] = {
		.dev_name = "tegra124-adx.1",
		.name_prefix = "ADX1",
	},
	[TEGRA124_CODEC_DAM0_CONF] = {
		.dev_name = "tegra30-dam.0",
		.name_prefix = "DAM0",
	},
	[TEGRA124_CODEC_DAM1_CONF] = {
		.dev_name = "tegra30-dam.1",
		.name_prefix = "DAM1",
	},
	[TEGRA124_CODEC_DAM2_CONF] = {
		.dev_name = "tegra30-dam.2",
		.name_prefix = "DAM2",
	},
	[TEGRA124_CODEC_AFC0_CONF] = {
		.dev_name = "tegra124-afc.0",
		.name_prefix = "AFC0",
	},
	[TEGRA124_CODEC_AFC1_CONF] = {
		.dev_name = "tegra124-afc.1",
		.name_prefix = "AFC1",
	},
	[TEGRA124_CODEC_AFC2_CONF] = {
		.dev_name = "tegra124-afc.2",
		.name_prefix = "AFC2",
	},
	[TEGRA124_CODEC_AFC3_CONF] = {
		.dev_name = "tegra124-afc.3",
		.name_prefix = "AFC3",
	},
	[TEGRA124_CODEC_AFC4_CONF] = {
		.dev_name = "tegra124-afc.4",
		.name_prefix = "AFC4",
	},
	[TEGRA124_CODEC_AFC5_CONF] = {
		.dev_name = "tegra124-afc.5",
		.name_prefix = "AFC5",
	},
	[TEGRA124_CODEC_I2S0_CONF] = {
		.dev_name = "tegra30-i2s.0",
		.name_prefix = "I2S0",
	},
	[TEGRA124_CODEC_I2S1_CONF] = {
		.dev_name = "tegra30-i2s.1",
		.name_prefix = "I2S1",
	},
	[TEGRA124_CODEC_I2S2_CONF] = {
		.dev_name = "tegra30-i2s.2",
		.name_prefix = "I2S2",
	},
	[TEGRA124_CODEC_I2S3_CONF] = {
		.dev_name = "tegra30-i2s.3",
		.name_prefix = "I2S3",
	},
	[TEGRA124_CODEC_I2S4_CONF] = {
		.dev_name = "tegra30-i2s.4",
		.name_prefix = "I2S4",
	},
	[TEGRA124_CODEC_SPDIF_CONF] = {
		.dev_name = "tegra30-spdif",
		.name_prefix = "SPDIF",
	},
};

static struct snd_soc_dai_link
	tegra210_xbar_dai_links[TEGRA210_XBAR_DAI_LINKS] = {
	[TEGRA210_DAI_LINK_ADMAIF1] = {
		.name = "ADMAIF1 CIF",
		.stream_name = "ADMAIF1 CIF",
		.cpu_dai_name = "ADMAIF1",
		.codec_dai_name = "ADMAIF1",
		.cpu_name = "tegra210-admaif",
		.codec_name = "tegra210-axbar",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADMAIF2] = {
		.name = "ADMAIF2 CIF",
		.stream_name = "ADMAIF2 CIF",
		.cpu_dai_name = "ADMAIF2",
		.codec_dai_name = "ADMAIF2",
		.cpu_name = "tegra210-admaif",
		.codec_name = "tegra210-axbar",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADMAIF3] = {
		.name = "ADMAIF3 CIF",
		.stream_name = "ADMAIF3 CIF",
		.cpu_dai_name = "ADMAIF3",
		.codec_dai_name = "ADMAIF3",
		.cpu_name = "tegra210-admaif",
		.codec_name = "tegra210-axbar",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADMAIF4] = {
		.name = "ADMAIF4 CIF",
		.stream_name = "ADMAIF4 CIF",
		.cpu_dai_name = "ADMAIF4",
		.codec_dai_name = "ADMAIF4",
		.cpu_name = "tegra210-admaif",
		.codec_name = "tegra210-axbar",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADMAIF5] = {
		.name = "ADMAIF5 CIF",
		.stream_name = "ADMAIF5 CIF",
		.cpu_dai_name = "ADMAIF5",
		.codec_dai_name = "ADMAIF5",
		.cpu_name = "tegra210-admaif",
		.codec_name = "tegra210-axbar",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADMAIF6] = {
		.name = "ADMAIF6 CIF",
		.stream_name = "ADMAIF6 CIF",
		.cpu_dai_name = "ADMAIF6",
		.codec_dai_name = "ADMAIF6",
		.cpu_name = "tegra210-admaif",
		.codec_name = "tegra210-axbar",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADMAIF7] = {
		.name = "ADMAIF7 CIF",
		.stream_name = "ADMAIF7 CIF",
		.cpu_dai_name = "ADMAIF7",
		.codec_dai_name = "ADMAIF7",
		.cpu_name = "tegra210-admaif",
		.codec_name = "tegra210-axbar",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADMAIF8] = {
		.name = "ADMAIF8 CIF",
		.stream_name = "ADMAIF8 CIF",
		.cpu_dai_name = "ADMAIF8",
		.codec_dai_name = "ADMAIF8",
		.cpu_name = "tegra210-admaif",
		.codec_name = "tegra210-axbar",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADMAIF9] = {
		.name = "ADMAIF9 CIF",
		.stream_name = "ADMAIF9 CIF",
		.cpu_dai_name = "ADMAIF9",
		.codec_dai_name = "ADMAIF9",
		.cpu_name = "tegra210-admaif",
		.codec_name = "tegra210-axbar",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADMAIF10] = {
		.name = "ADMAIF10 CIF",
		.stream_name = "ADMAIF10 CIF",
		.cpu_dai_name = "ADMAIF10",
		.codec_dai_name = "ADMAIF10",
		.cpu_name = "tegra210-admaif",
		.codec_name = "tegra210-axbar",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_AMX1_1] = {
		.name = "AMX1 IN1",
		.stream_name = "AMX1 IN",
		.cpu_dai_name = "AMX1-1",
		.codec_dai_name = "IN1",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-amx.0",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_AMX1_2] = {
		.name = "AMX1 IN2",
		.stream_name = "AMX1 IN",
		.cpu_dai_name = "AMX1-2",
		.codec_dai_name = "IN2",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-amx.0",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_AMX1_3] = {
		.name = "AMX1 IN3",
		.stream_name = "AMX1 IN",
		.cpu_dai_name = "AMX1-3",
		.codec_dai_name = "IN3",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-amx.0",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_AMX1_4] = {
		.name = "AMX1 IN4",
		.stream_name = "AMX1 IN",
		.cpu_dai_name = "AMX1-4",
		.codec_dai_name = "IN4",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-amx.0",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_AMX1] = {
		.name = "AMX1 CIF",
		.stream_name = "AMX1 CIF",
		.cpu_dai_name = "OUT",
		.codec_dai_name = "AMX1",
		.cpu_name = "tegra210-amx.0",
		.codec_name = "tegra210-axbar",
		.params = &tdm_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_AMX2_1] = {
		.name = "AMX2 IN1",
		.stream_name = "AMX2 IN",
		.cpu_dai_name = "AMX2-1",
		.codec_dai_name = "IN1",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-amx.1",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_AMX2_2] = {
		.name = "AMX2 IN2",
		.stream_name = "AMX2 IN",
		.cpu_dai_name = "AMX2-2",
		.codec_dai_name = "IN2",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-amx.1",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_AMX2_3] = {
		.name = "AMX2 IN3",
		.stream_name = "AMX2 IN",
		.cpu_dai_name = "AMX2-3",
		.codec_dai_name = "IN3",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-amx.1",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_AMX2_4] = {
		.name = "AMX2 IN4",
		.stream_name = "AMX2 IN",
		.cpu_dai_name = "AMX2-4",
		.codec_dai_name = "IN4",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-amx.1",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_AMX2] = {
		.name = "AMX2 CIF",
		.stream_name = "AMX2 CIF",
		.cpu_dai_name = "OUT",
		.codec_dai_name = "AMX2",
		.cpu_name = "tegra210-amx.1",
		.codec_name = "tegra210-axbar",
		.params = &tdm_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADX1] = {
		.name = "ADX1 CIF",
		.stream_name = "ADX1 IN",
		.cpu_dai_name = "ADX1",
		.codec_dai_name = "IN",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-adx.0",
		.params = &tdm_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADX1_1] = {
		.name = "ADX1 OUT1",
		.stream_name = "ADX1 OUT",
		.cpu_dai_name = "OUT1",
		.codec_dai_name = "ADX1-1",
		.cpu_name = "tegra210-adx.0",
		.codec_name = "tegra210-axbar",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADX1_2] = {
		.name = "ADX1 OUT2",
		.stream_name = "ADX1 OUT",
		.cpu_dai_name = "OUT2",
		.codec_dai_name = "ADX1-2",
		.cpu_name = "tegra210-adx.0",
		.codec_name = "tegra210-axbar",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADX1_3] = {
		.name = "ADX1 OUT3",
		.stream_name = "ADX1 OUT",
		.cpu_dai_name = "OUT3",
		.codec_dai_name = "ADX1-3",
		.cpu_name = "tegra210-adx.0",
		.codec_name = "tegra210-axbar",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADX1_4] = {
		.name = "ADX1 OUT4",
		.stream_name = "ADX1 OUT",
		.cpu_dai_name = "OUT4",
		.codec_dai_name = "ADX1-4",
		.cpu_name = "tegra210-adx.0",
		.codec_name = "tegra210-axbar",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADX2] = {
		.name = "ADX2 CIF",
		.stream_name = "ADX2 IN",
		.cpu_dai_name = "ADX2",
		.codec_dai_name = "IN",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-adx.1",
		.params = &tdm_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADX2_1] = {
		.name = "ADX2 OUT1",
		.stream_name = "ADX2 OUT",
		.cpu_dai_name = "OUT1",
		.codec_dai_name = "ADX2-1",
		.cpu_name = "tegra210-adx.1",
		.codec_name = "tegra210-axbar",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADX2_2] = {
		.name = "ADX2 OUT2",
		.stream_name = "ADX2 OUT",
		.cpu_dai_name = "OUT2",
		.codec_dai_name = "ADX2-2",
		.cpu_name = "tegra210-adx.1",
		.codec_name = "tegra210-axbar",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADX2_3] = {
		.name = "ADX2 OUT3",
		.stream_name = "ADX2 OUT",
		.cpu_dai_name = "OUT3",
		.codec_dai_name = "ADX2-3",
		.cpu_name = "tegra210-adx.1",
		.codec_name = "tegra210-axbar",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADX2_4] = {
		.name = "ADX2 OUT4",
		.stream_name = "ADX2 OUT",
		.cpu_dai_name = "OUT4",
		.codec_dai_name = "ADX2-4",
		.cpu_name = "tegra210-adx.1",
		.codec_name = "tegra210-axbar",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_MIXER1_RX1] = {
		.name = "MIXER1 RX1",
		.stream_name = "MIXER1 RX",
		.cpu_dai_name = "MIXER1-1",
		.codec_dai_name = "RX1",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-mixer",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_MIXER1_RX2] = {
		.name = "MIXER1 RX2",
		.stream_name = "MIXER1 RX",
		.cpu_dai_name = "MIXER1-2",
		.codec_dai_name = "RX2",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-mixer",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_MIXER1_RX3] = {
		.name = "MIXER1 RX3",
		.stream_name = "MIXER1 RX",
		.cpu_dai_name = "MIXER1-3",
		.codec_dai_name = "RX3",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-mixer",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_MIXER1_RX4] = {
		.name = "MIXER1 RX4",
		.stream_name = "MIXER1 RX",
		.cpu_dai_name = "MIXER1-4",
		.codec_dai_name = "RX4",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-mixer",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_MIXER1_RX5] = {
		.name = "MIXER1 RX5",
		.stream_name = "MIXER1 RX",
		.cpu_dai_name = "MIXER1-5",
		.codec_dai_name = "RX5",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-mixer",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_MIXER1_RX6] = {
		.name = "MIXER1 RX6",
		.stream_name = "MIXER1 RX",
		.cpu_dai_name = "MIXER1-6",
		.codec_dai_name = "RX6",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-mixer",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_MIXER1_RX7] = {
		.name = "MIXER1 RX7",
		.stream_name = "MIXER1 RX",
		.cpu_dai_name = "MIXER1-7",
		.codec_dai_name = "RX7",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-mixer",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_MIXER1_RX8] = {
		.name = "MIXER1 RX8",
		.stream_name = "MIXER1 RX",
		.cpu_dai_name = "MIXER1-8",
		.codec_dai_name = "RX8",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-mixer",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_MIXER1_RX9] = {
		.name = "MIXER1 RX9",
		.stream_name = "MIXER1 RX",
		.cpu_dai_name = "MIXER1-9",
		.codec_dai_name = "RX9",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-mixer",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_MIXER1_RX10] = {
		.name = "MIXER1 RX10",
		.stream_name = "MIXER1 RX",
		.cpu_dai_name = "MIXER1-10",
		.codec_dai_name = "RX10",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-mixer",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_MIXER1_TX1] = {
		.name = "MIXER1 TX1",
		.stream_name = "MIXER1 TX",
		.cpu_dai_name = "TX1",
		.codec_dai_name = "MIXER1-1",
		.cpu_name = "tegra210-mixer",
		.codec_name = "tegra210-axbar",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_MIXER1_TX2] = {
		.name = "MIXER1 TX2",
		.stream_name = "MIXER1 TX",
		.cpu_dai_name = "TX2",
		.codec_dai_name = "MIXER1-2",
		.cpu_name = "tegra210-mixer",
		.codec_name = "tegra210-axbar",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_MIXER1_TX3] = {
		.name = "MIXER1 TX3",
		.stream_name = "MIXER1 TX",
		.cpu_dai_name = "TX3",
		.codec_dai_name = "MIXER1-3",
		.cpu_name = "tegra210-mixer",
		.codec_name = "tegra210-axbar",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_MIXER1_TX4] = {
		.name = "MIXER1 TX4",
		.stream_name = "MIXER1 TX",
		.cpu_dai_name = "TX4",
		.codec_dai_name = "MIXER1-4",
		.cpu_name = "tegra210-mixer",
		.codec_name = "tegra210-axbar",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_MIXER1_TX5] = {
		.name = "MIXER1 TX5",
		.stream_name = "MIXER1 TX",
		.cpu_dai_name = "TX5",
		.codec_dai_name = "MIXER1-5",
		.cpu_name = "tegra210-mixer",
		.codec_name = "tegra210-axbar",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_SFC1_RX] = {
		.name = "SFC1 RX",
		.stream_name = "SFC1 RX",
		.cpu_dai_name = "SFC1",
		.codec_dai_name = "CIF",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-sfc.0",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_SFC1_TX] = {
		.name = "SFC1 TX",
		.stream_name = "SFC1 TX",
		.cpu_dai_name = "DAP",
		.codec_dai_name = "SFC1",
		.cpu_name = "tegra210-sfc.0",
		.codec_name = "tegra210-axbar",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_SFC2_RX] = {
		.name = "SFC2 RX",
		.stream_name = "SFC2 RX",
		.cpu_dai_name = "SFC2",
		.codec_dai_name = "CIF",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-sfc.1",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_SFC2_TX] = {
		.name = "SFC2 TX",
		.stream_name = "SFC2 TX",
		.cpu_dai_name = "DAP",
		.codec_dai_name = "SFC2",
		.cpu_name = "tegra210-sfc.1",
		.codec_name = "tegra210-axbar",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_SFC3_RX] = {
		.name = "SFC3 RX",
		.stream_name = "SFC3 RX",
		.cpu_dai_name = "SFC3",
		.codec_dai_name = "CIF",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-sfc.2",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_SFC3_TX] = {
		.name = "SFC3 TX",
		.stream_name = "SFC3 TX",
		.cpu_dai_name = "DAP",
		.codec_dai_name = "SFC3",
		.cpu_name = "tegra210-sfc.2",
		.codec_name = "tegra210-axbar",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_SFC4_RX] = {
		.name = "SFC4 RX",
		.stream_name = "SFC4 RX",
		.cpu_dai_name = "SFC4",
		.codec_dai_name = "CIF",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-sfc.3",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_SFC4_TX] = {
		.name = "SFC4 TX",
		.stream_name = "SFC4 TX",
		.cpu_dai_name = "DAP",
		.codec_dai_name = "SFC4",
		.cpu_name = "tegra210-sfc.3",
		.codec_name = "tegra210-axbar",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_AFC1_RX] = {
		.name = "AFC1 RX",
		.stream_name = "AFC1 RX",
		.cpu_dai_name = "AFC1",
		.codec_dai_name = "AFC IN",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-afc.0",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_AFC1_TX] = {
		.name = "AFC1 TX",
		.stream_name = "AFC1 TX",
		.cpu_dai_name = "AFC OUT",
		.codec_dai_name = "AFC1",
		.cpu_name = "tegra210-afc.0",
		.codec_name = "tegra210-axbar",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_AFC2_RX] = {
		.name = "AFC2 RX",
		.stream_name = "AFC2 RX",
		.cpu_dai_name = "AFC2",
		.codec_dai_name = "AFC IN",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-afc.1",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_AFC2_TX] = {
		.name = "AFC2 TX",
		.stream_name = "AFC2 TX",
		.cpu_dai_name = "AFC OUT",
		.codec_dai_name = "AFC2",
		.cpu_name = "tegra210-afc.1",
		.codec_name = "tegra210-axbar",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_AFC3_RX] = {
		.name = "AFC3 RX",
		.stream_name = "AFC3 RX",
		.cpu_dai_name = "AFC3",
		.codec_dai_name = "AFC IN",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-afc.2",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_AFC3_TX] = {
		.name = "AFC3 TX",
		.stream_name = "AFC3 TX",
		.cpu_dai_name = "AFC OUT",
		.codec_dai_name = "AFC3",
		.cpu_name = "tegra210-afc.2",
		.codec_name = "tegra210-axbar",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_AFC4_RX] = {
		.name = "AFC4 RX",
		.stream_name = "AFC4 RX",
		.cpu_dai_name = "AFC4",
		.codec_dai_name = "AFC IN",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-afc.3",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_AFC4_TX] = {
		.name = "AFC4 TX",
		.stream_name = "AFC4 TX",
		.cpu_dai_name = "AFC OUT",
		.codec_dai_name = "AFC4",
		.cpu_name = "tegra210-afc.3",
		.codec_name = "tegra210-axbar",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_AFC5_RX] = {
		.name = "AFC5 RX",
		.stream_name = "AFC5 RX",
		.cpu_dai_name = "AFC5",
		.codec_dai_name = "AFC IN",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-afc.4",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_AFC5_TX] = {
		.name = "AFC5 TX",
		.stream_name = "AFC5 TX",
		.cpu_dai_name = "AFC OUT",
		.codec_dai_name = "AFC5",
		.cpu_name = "tegra210-afc.4",
		.codec_name = "tegra210-axbar",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_AFC6_RX] = {
		.name = "AFC6 RX",
		.stream_name = "AFC6 RX",
		.cpu_dai_name = "AFC6",
		.codec_dai_name = "AFC IN",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-afc.5",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_AFC6_TX] = {
		.name = "AFC6 TX",
		.stream_name = "AFC6 TX",
		.cpu_dai_name = "AFC OUT",
		.codec_dai_name = "AFC6",
		.cpu_name = "tegra210-afc.5",
		.codec_name = "tegra210-axbar",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_MVC1_RX] = {
		.name = "MVC1 RX",
		.stream_name = "MVC1 RX",
		.cpu_dai_name = "MVC1",
		.codec_dai_name = "MVC IN",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-mvc.0",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_MVC1_TX] = {
		.name = "MVC1 TX",
		.stream_name = "MVC1 TX",
		.cpu_dai_name = "MVC OUT",
		.codec_dai_name = "MVC1",
		.cpu_name = "tegra210-mvc.0",
		.codec_name = "tegra210-axbar",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_MVC2_RX] = {
		.name = "MVC2 RX",
		.stream_name = "MVC2 RX",
		.cpu_dai_name = "MVC2",
		.codec_dai_name = "MVC IN",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-mvc.1",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_MVC2_TX] = {
		.name = "MVC2 TX",
		.stream_name = "AFC2 TX",
		.cpu_dai_name = "MVC OUT",
		.codec_dai_name = "MVC2",
		.cpu_name = "tegra210-mvc.1",
		.codec_name = "tegra210-axbar",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_OPE1_RX] = {
		.name = "OPE1 RX",
		.stream_name = "OPE1 RX",
		.cpu_dai_name = "OPE1",
		.codec_dai_name = "OPE IN",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-ope.0",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_OPE1_TX] = {
		.name = "OPE1 TX",
		.stream_name = "OPE1 TX",
		.cpu_dai_name = "OPE OUT",
		.codec_dai_name = "OPE1",
		.cpu_name = "tegra210-ope.0",
		.codec_name = "tegra210-axbar",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_OPE2_RX] = {
		.name = "OPE2 RX",
		.stream_name = "OPE2 RX",
		.cpu_dai_name = "OPE2",
		.codec_dai_name = "OPE IN",
		.cpu_name = "tegra210-axbar",
		.codec_name = "tegra210-ope.1",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_OPE2_TX] = {
		.name = "OPE2 TX",
		.stream_name = "OPE2 TX",
		.cpu_dai_name = "OPE OUT",
		.codec_dai_name = "OPE2",
		.cpu_name = "tegra210-ope.1",
		.codec_name = "tegra210-axbar",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADMAIF1_CODEC] = {
		.name = "ADMAIF1 CODEC",
		.stream_name = "ADMAIF1 CODEC",
		.cpu_dai_name = "ADMAIF1 CIF",
		.codec_dai_name = "ADMAIF1",
		.cpu_name = "tegra210-admaif",
		.codec_name = "tegra210-axbar",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADMAIF2_CODEC] = {
		.name = "ADMAIF2 CODEC",
		.stream_name = "ADMAIF2 CODEC",
		.cpu_dai_name = "ADMAIF2 CIF",
		.codec_dai_name = "ADMAIF2",
		.cpu_name = "tegra210-admaif",
		.codec_name = "tegra210-axbar",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADMAIF3_CODEC] = {
		.name = "ADMAIF3 CODEC",
		.stream_name = "ADMAIF3 CODEC",
		.cpu_dai_name = "ADMAIF3 CIF",
		.codec_dai_name = "ADMAIF3",
		.cpu_name = "tegra210-admaif",
		.codec_name = "tegra210-axbar",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADMAIF4_CODEC] = {
		.name = "ADMAIF4 CODEC",
		.stream_name = "ADMAIF4 CODEC",
		.cpu_dai_name = "ADMAIF4 CIF",
		.codec_dai_name = "ADMAIF4",
		.cpu_name = "tegra210-admaif",
		.codec_name = "tegra210-axbar",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADMAIF5_CODEC] = {
		.name = "ADMAIF5 CODEC",
		.stream_name = "ADMAIF5 CODEC",
		.cpu_dai_name = "ADMAIF5 CIF",
		.codec_dai_name = "ADMAIF5",
		.cpu_name = "tegra210-admaif",
		.codec_name = "tegra210-axbar",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADMAIF6_CODEC] = {
		.name = "ADMAIF6 CODEC",
		.stream_name = "ADMAIF6 CODEC",
		.cpu_dai_name = "ADMAIF6 CIF",
		.codec_dai_name = "ADMAIF6",
		.cpu_name = "tegra210-admaif",
		.codec_name = "tegra210-axbar",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADMAIF7_CODEC] = {
		.name = "ADMAIF7 CODEC",
		.stream_name = "ADMAIF7 CODEC",
		.cpu_dai_name = "ADMAIF7 CIF",
		.codec_dai_name = "ADMAIF7",
		.cpu_name = "tegra210-admaif",
		.codec_name = "tegra210-axbar",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADMAIF8_CODEC] = {
		.name = "ADMAIF8 CODEC",
		.stream_name = "ADMAIF8 CODEC",
		.cpu_dai_name = "ADMAIF8 CIF",
		.codec_dai_name = "ADMAIF8",
		.cpu_name = "tegra210-admaif",
		.codec_name = "tegra210-axbar",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADMAIF9_CODEC] = {
		.name = "ADMAIF9 CODEC",
		.stream_name = "ADMAIF9 CODEC",
		.cpu_dai_name = "ADMAIF9 CIF",
		.codec_dai_name = "ADMAIF9",
		.cpu_name = "tegra210-admaif",
		.codec_name = "tegra210-axbar",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADMAIF10_CODEC] = {
		.name = "ADMAIF10 CODEC",
		.stream_name = "ADMAIF10 CODEC",
		.cpu_dai_name = "ADMAIF10 CIF",
		.codec_dai_name = "ADMAIF10",
		.cpu_name = "tegra210-admaif",
		.codec_name = "tegra210-axbar",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADSP_ADMAIF1] = {
		.name = "ADSP ADMAIF1",
		.stream_name = "ADSP ADMAIF1",
		.cpu_dai_name = "ADSP-ADMAIF1",
		.codec_dai_name = "ADMAIF1 FIFO",
		.cpu_name = "adsp_audio.3",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADSP_ADMAIF2] = {
		.name = "ADSP ADMAIF2",
		.stream_name = "ADSP ADMAIF2",
		.cpu_dai_name = "ADSP-ADMAIF2",
		.codec_dai_name = "ADMAIF2 FIFO",
		.cpu_name = "adsp_audio.3",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADSP_ADMAIF3] = {
		.name = "ADSP ADMAIF3",
		.stream_name = "ADSP ADMAIF3",
		.cpu_dai_name = "ADSP-ADMAIF3",
		.codec_dai_name = "ADMAIF3 FIFO",
		.cpu_name = "adsp_audio.3",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADSP_ADMAIF4] = {
		.name = "ADSP ADMAIF4",
		.stream_name = "ADSP ADMAIF4",
		.cpu_dai_name = "ADSP-ADMAIF4",
		.codec_dai_name = "ADMAIF4 FIFO",
		.cpu_name = "adsp_audio.3",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADSP_ADMAIF5] = {
		.name = "ADSP ADMAIF5",
		.stream_name = "ADSP ADMAIF5",
		.cpu_dai_name = "ADSP-ADMAIF5",
		.codec_dai_name = "ADMAIF5 FIFO",
		.cpu_name = "adsp_audio.3",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADSP_ADMAIF6] = {
		.name = "ADSP ADMAIF6",
		.stream_name = "ADSP ADMAIF6",
		.cpu_dai_name = "ADSP-ADMAIF6",
		.codec_dai_name = "ADMAIF6 FIFO",
		.cpu_name = "adsp_audio.3",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADSP_ADMAIF7] = {
		.name = "ADSP ADMAIF7",
		.stream_name = "ADSP ADMAIF7",
		.cpu_dai_name = "ADSP-ADMAIF7",
		.codec_dai_name = "ADMAIF7 FIFO",
		.cpu_name = "adsp_audio.3",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADSP_ADMAIF8] = {
		.name = "ADSP ADMAIF8",
		.stream_name = "ADSP ADMAIF8",
		.cpu_dai_name = "ADSP-ADMAIF8",
		.codec_dai_name = "ADMAIF8 FIFO",
		.cpu_name = "adsp_audio.3",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADSP_ADMAIF9] = {
		.name = "ADSP ADMAIF9",
		.stream_name = "ADSP ADMAIF9",
		.cpu_dai_name = "ADSP-ADMAIF9",
		.codec_dai_name = "ADMAIF9 FIFO",
		.cpu_name = "adsp_audio.3",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADSP_ADMAIF10] = {
		.name = "ADSP ADMAIF10",
		.stream_name = "ADSP ADMAIF10",
		.cpu_dai_name = "ADSP-ADMAIF10",
		.codec_dai_name = "ADMAIF10 FIFO",
		.cpu_name = "adsp_audio.3",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADSP_PCM1] = {
		.name = "ADSP PCM1",
		.stream_name = "ADSP PCM1",
		.cpu_dai_name = "ADSP PCM1",
		.codec_dai_name = "ADSP-FE1",
		.cpu_name = "adsp_audio.3",
		.codec_name = "adsp_audio.3",
		.platform_name = "adsp_audio.3",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADSP_PCM2] = {
		.name = "ADSP PCM2",
		.stream_name = "ADSP PCM2",
		.cpu_dai_name = "ADSP PCM2",
		.codec_dai_name = "ADSP-FE2",
		.cpu_name = "adsp_audio.3",
		.codec_name = "adsp_audio.3",
		.platform_name = "adsp_audio.3",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADSP_COMPR1] = {
		.name = "ADSP COMPR1",
		.stream_name = "ADSP COMPR1",
		.cpu_dai_name = "ADSP COMPR1",
		.codec_dai_name = "ADSP-FE3",
		.cpu_name = "adsp_audio.3",
		.codec_name = "adsp_audio.3",
		.platform_name = "adsp_audio.3",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA210_DAI_LINK_ADSP_COMPR2] = {
		.name = "ADSP COMPR2",
		.stream_name = "ADSP COMPR2",
		.cpu_dai_name = "ADSP COMPR2",
		.codec_dai_name = "ADSP-FE4",
		.cpu_name = "adsp_audio.3",
		.codec_name = "adsp_audio.3",
		.platform_name = "adsp_audio.3",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_codec_conf
	tegra210_xbar_codec_conf[TEGRA210_XBAR_CODEC_CONF] = {
	[TEGRA210_CODEC_AMX1_CONF] = {
		.dev_name = "tegra210-amx.0",
		.name_prefix = "AMX1",
	},
	[TEGRA210_CODEC_AMX2_CONF] = {
		.dev_name = "tegra210-amx.1",
		.name_prefix = "AMX2",
	},
	[TEGRA210_CODEC_ADX1_CONF] = {
		.dev_name = "tegra210-adx.0",
		.name_prefix = "ADX1",
	},
	[TEGRA210_CODEC_ADX2_CONF] = {
		.dev_name = "tegra210-adx.1",
		.name_prefix = "ADX2",
	},
	[TEGRA210_CODEC_SFC1_CONF] = {
		.dev_name = "tegra210-sfc.0",
		.name_prefix = "SFC1",
	},
	[TEGRA210_CODEC_SFC2_CONF] = {
		.dev_name = "tegra210-sfc.1",
		.name_prefix = "SFC2",
	},
	[TEGRA210_CODEC_SFC3_CONF] = {
		.dev_name = "tegra210-sfc.2",
		.name_prefix = "SFC3",
	},
	[TEGRA210_CODEC_SFC4_CONF] = {
		.dev_name = "tegra210-sfc.3",
		.name_prefix = "SFC4",
	},
	[TEGRA210_CODEC_MVC1_CONF] = {
		.dev_name = "tegra210-mvc.0",
		.name_prefix = "MVC1",
	},
	[TEGRA210_CODEC_MVC2_CONF] = {
		.dev_name = "tegra210-mvc.1",
		.name_prefix = "MVC2",
	},
	[TEGRA210_CODEC_OPE1_CONF] = {
		.dev_name = "tegra210-ope.0",
		.name_prefix = "OPE1",
	},
	[TEGRA210_CODEC_OPE2_CONF] = {
		.dev_name = "tegra210-ope.1",
		.name_prefix = "OPE2",
	},
	[TEGRA210_CODEC_AFC1_CONF] = {
		.dev_name = "tegra210-afc.0",
		.name_prefix = "AFC1",
	},
	[TEGRA210_CODEC_AFC2_CONF] = {
		.dev_name = "tegra210-afc.1",
		.name_prefix = "AFC2",
	},
	[TEGRA210_CODEC_AFC3_CONF] = {
		.dev_name = "tegra210-afc.2",
		.name_prefix = "AFC3",
	},
	[TEGRA210_CODEC_AFC4_CONF] = {
		.dev_name = "tegra210-afc.3",
		.name_prefix = "AFC4",
	},
	[TEGRA210_CODEC_AFC5_CONF] = {
		.dev_name = "tegra210-afc.4",
		.name_prefix = "AFC5",
	},
	[TEGRA210_CODEC_AFC6_CONF] = {
		.dev_name = "tegra210-afc.5",
		.name_prefix = "AFC6",
	},
	[TEGRA210_CODEC_I2S1_CONF] = {
		.dev_name = "tegra210-i2s.0",
		.name_prefix = "I2S1",
	},
	[TEGRA210_CODEC_I2S2_CONF] = {
		.dev_name = "tegra210-i2s.1",
		.name_prefix = "I2S2",
	},
	[TEGRA210_CODEC_I2S3_CONF] = {
		.dev_name = "tegra210-i2s.2",
		.name_prefix = "I2S3",
	},
	[TEGRA210_CODEC_I2S4_CONF] = {
		.dev_name = "tegra210-i2s.3",
		.name_prefix = "I2S4",
	},
	[TEGRA210_CODEC_I2S5_CONF] = {
		.dev_name = "tegra210-i2s.4",
		.name_prefix = "I2S5",
	},
	[TEGRA210_CODEC_DMIC1_CONF] = {
		.dev_name = "tegra210-dmic.0",
		.name_prefix = "DMIC1",
	},
	[TEGRA210_CODEC_DMIC2_CONF] = {
		.dev_name = "tegra210-dmic.1",
		.name_prefix = "DMIC2",
	},
	[TEGRA210_CODEC_DMIC3_CONF] = {
		.dev_name = "tegra210-dmic.2",
		.name_prefix = "DMIC3",
	},
	[TEGRA210_CODEC_SPDIF_CONF] = {
		.dev_name = "tegra210-spdif",
		.name_prefix = "SPDIF",
	},
};

void tegra_machine_set_machine_links(
	struct snd_soc_dai_link *links)
{
	tegra_asoc_machine_links = links;
}
EXPORT_SYMBOL_GPL(tegra_machine_set_machine_links);

struct snd_soc_dai_link *tegra_machine_get_machine_links(void)
{
	return tegra_asoc_machine_links;
}
EXPORT_SYMBOL_GPL(tegra_machine_get_machine_links);

void tegra_machine_set_machine_codec_conf(
	struct snd_soc_codec_conf *codec_conf)
{
	tegra_asoc_codec_conf = codec_conf;
}
EXPORT_SYMBOL_GPL(tegra_machine_set_machine_codec_conf);

struct snd_soc_codec_conf *tegra_machine_get_machine_codec_conf(void)
{
	return tegra_asoc_codec_conf;
}
EXPORT_SYMBOL_GPL(tegra_machine_get_machine_codec_conf);

struct snd_soc_dai_link *tegra_machine_get_dai_link(void)
{
	struct snd_soc_dai_link *link = tegra124_xbar_dai_links;
	unsigned int size = TEGRA124_XBAR_DAI_LINKS;

	if (of_machine_is_compatible("nvidia,tegra210")) {
		link = tegra210_xbar_dai_links;
		size = TEGRA210_XBAR_DAI_LINKS;
	}

	if (tegra_asoc_machine_links)
		return tegra_asoc_machine_links;

	num_dai_links = size;

	tegra_asoc_machine_links = kzalloc(size *
		sizeof(struct snd_soc_dai_link), GFP_KERNEL);

	memcpy(tegra_asoc_machine_links, link,
		size * sizeof(struct snd_soc_dai_link));

	return tegra_asoc_machine_links;
}
EXPORT_SYMBOL_GPL(tegra_machine_get_dai_link);

void tegra_machine_remove_dai_link(void)
{
	kfree(tegra_asoc_machine_links);
	tegra_asoc_machine_links = NULL;
}
EXPORT_SYMBOL_GPL(tegra_machine_remove_dai_link);

/* @link: input structure to append
 * @link_size: size of the input structure
 * Returns the total size after appending
 */
int tegra_machine_append_dai_link(struct snd_soc_dai_link *link,
		unsigned int link_size)
{
	unsigned int size1 = of_machine_is_compatible("nvidia,tegra210") ?
				TEGRA210_XBAR_DAI_LINKS :
				TEGRA124_XBAR_DAI_LINKS;
	unsigned int size2 = link_size;

	if (!tegra_asoc_machine_links) {
		if (link) {
			tegra_asoc_machine_links = link;
			num_dai_links = size2;
			return size2;
		} else {
			return 0;
		}
	} else {
		if (link) {
			tegra_asoc_machine_links =
				(struct snd_soc_dai_link *) krealloc(
				tegra_asoc_machine_links, (size1 + size2) *
				sizeof(struct snd_soc_dai_link), GFP_KERNEL);
			memcpy(&tegra_asoc_machine_links[size1], link,
				size2 * sizeof(struct snd_soc_dai_link));
			num_dai_links = size1+size2;
			return size1+size2;
		} else {
			num_dai_links = size1;
			return size1;
		}
	}
}
EXPORT_SYMBOL_GPL(tegra_machine_append_dai_link);

void tegra_machine_set_dai_ops(int link, struct snd_soc_ops *ops)
{
	if (tegra_asoc_machine_links)
		tegra_asoc_machine_links[link].ops = ops;
}
EXPORT_SYMBOL_GPL(tegra_machine_set_dai_ops);

void tegra_machine_set_dai_compr_ops(int link, struct snd_soc_compr_ops *ops)
{
	if (tegra_asoc_machine_links)
		tegra_asoc_machine_links[link].compr_ops = ops;
}
EXPORT_SYMBOL_GPL(tegra_machine_set_dai_compr_ops);

void tegra_machine_set_dai_init(int link, void *ptr)
{
	if (tegra_asoc_machine_links)
		tegra_asoc_machine_links[link].init = ptr;
}
EXPORT_SYMBOL_GPL(tegra_machine_set_dai_init);

void tegra_machine_set_dai_params(int link,
		struct snd_soc_pcm_stream *params)
{
	if (tegra_asoc_machine_links && (NULL == params))
		tegra_asoc_machine_links[link].params = &default_link_params;
	else if (tegra_asoc_machine_links)
		tegra_asoc_machine_links[link].params = params;
}
EXPORT_SYMBOL_GPL(tegra_machine_set_dai_params);

void tegra_machine_set_dai_fmt(int link, unsigned int fmt)
{
	if (tegra_asoc_machine_links)
		tegra_asoc_machine_links[link].dai_fmt = fmt;
}
EXPORT_SYMBOL_GPL(tegra_machine_set_dai_fmt);

struct snd_soc_codec_conf *tegra_machine_get_codec_conf(void)
{
	struct snd_soc_codec_conf *conf = tegra124_xbar_codec_conf;
	unsigned int size = TEGRA124_XBAR_CODEC_CONF;

	if (of_machine_is_compatible("nvidia,tegra210")) {
		conf = tegra210_xbar_codec_conf;
		size = TEGRA210_XBAR_CODEC_CONF;
	}

	if (tegra_asoc_codec_conf)
		return tegra_asoc_codec_conf;

	tegra_asoc_codec_conf = kzalloc(size *
		sizeof(struct snd_soc_codec_conf), GFP_KERNEL);

	memcpy(tegra_asoc_codec_conf, conf,
		size * sizeof(struct snd_soc_codec_conf));

	return tegra_asoc_codec_conf;
}
EXPORT_SYMBOL_GPL(tegra_machine_get_codec_conf);

void tegra_machine_remove_codec_conf(void)
{
	kfree(tegra_asoc_codec_conf);
	tegra_asoc_codec_conf = NULL;
}
EXPORT_SYMBOL_GPL(tegra_machine_remove_codec_conf);

/* @link: input structure to append
 * @link_size: size of the input structure
 * Returns the total size after appending
 */
int tegra_machine_append_codec_conf(struct snd_soc_codec_conf *conf,
		unsigned int conf_size)
{
	unsigned int size1 = of_machine_is_compatible("nvidia,tegra210") ?
				TEGRA210_XBAR_CODEC_CONF :
				TEGRA124_XBAR_CODEC_CONF;
	unsigned int size2 = conf_size;

	if (!tegra_asoc_codec_conf) {
		if (conf) {
			tegra_asoc_codec_conf = conf;
			return size2;
		} else {
			return 0;
		}
	} else {
		if (conf) {
			tegra_asoc_codec_conf =
				(struct snd_soc_codec_conf *) krealloc(
				tegra_asoc_codec_conf, (size1 + size2) *
				sizeof(struct snd_soc_codec_conf), GFP_KERNEL);
			memcpy(&tegra_asoc_codec_conf[size1], conf,
				size2 * sizeof(struct snd_soc_codec_conf));
			return size1+size2;
		} else {
			return size1;
		}
	}
}
EXPORT_SYMBOL_GPL(tegra_machine_append_codec_conf);


static int tegra_machine_get_format(u64 *p_formats, char *fmt)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(bit_format); i++) {
		if (strcmp(bit_format[i], fmt) == 0) {
			*p_formats = 1 << i;
			return 0;
		}
	}
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(tegra_machine_get_format);

struct snd_soc_dai_link *tegra_machine_new_codec_links(
	struct platform_device *pdev,
	struct snd_soc_dai_link *tegra_codec_links,
	unsigned int *pnum_codec_links)
{
	unsigned int i, j, num_codec_links;
	struct device_node *np = pdev->dev.of_node, *subnp;
	struct snd_soc_pcm_stream *params;
	char dai_link_name[MAX_STR_SIZE];
	char *str;
	const char *prefix;
	struct device_node *bitclkmaster = NULL, *framemaster = NULL;

	if (tegra_codec_links)
		return tegra_codec_links;

	if (!np)
		goto err;

	if (of_property_read_u32(np,
		"nvidia,num-codec-link", (u32 *)&num_codec_links)) {
		dev_err(&pdev->dev,
			"Property 'nvidia,num-codec-link' missing or invalid\n");
		goto err;
	}

	tegra_codec_links = devm_kzalloc(&pdev->dev, 2 * num_codec_links *
		sizeof(struct snd_soc_dai_link), GFP_KERNEL);
	if (!tegra_codec_links) {
		dev_err(&pdev->dev, "Can't allocate tegra_codec_links\n");
		goto err;
	}

	if (bclk_ratio == NULL) {
		bclk_ratio = devm_kzalloc(&pdev->dev, num_codec_links *
			sizeof(unsigned int), GFP_KERNEL);
		if (!bclk_ratio) {
			dev_err(&pdev->dev, "Can't allocate bclk_ratio\n");
			goto err;
		}
	}

	if (rx_mask == NULL) {
		rx_mask = devm_kzalloc(&pdev->dev, num_codec_links *
			sizeof(unsigned int), GFP_KERNEL);
		if (!rx_mask) {
			dev_err(&pdev->dev, "Can't allocate rx_mask\n");
			goto err;
		}
	}

	if (tx_mask == NULL) {
		tx_mask = devm_kzalloc(&pdev->dev, num_codec_links *
			sizeof(unsigned int), GFP_KERNEL);
		if (!tx_mask) {
			dev_err(&pdev->dev, "Can't allocate tx_mask\n");
			goto err;
		}
	}
	/* variable i is for DAP and j is for CIF */
	for (i = 0, j = num_codec_links; i < num_codec_links; i++, j++) {
		memset((void *)dai_link_name, '\0', MAX_STR_SIZE);
		sprintf(dai_link_name, "nvidia,dai-link-%d", i+1);
		subnp = of_get_child_by_name(np, dai_link_name);
		if (subnp) {
			tegra_codec_links[i].codec_of_node =
				of_parse_phandle(subnp, "codec-dai", 0);
			if (!tegra_codec_links[i].codec_of_node) {
				dev_err(&pdev->dev,
					"Property '%s.codec-dai' missing or invalid\n",
					dai_link_name);
				goto err;
			}

			tegra_codec_links[i].cpu_of_node
				= of_parse_phandle(subnp, "cpu-dai", 0);
			if (!tegra_codec_links[i].cpu_of_node) {
				dev_err(&pdev->dev,
					"Property '%s.cpu-dai' missing or invalid\n",
					dai_link_name);
				goto err;
			}

			/* DAP configuration */
			if (of_property_read_string(subnp, "name-prefix",
				&prefix)) {
				dev_err(&pdev->dev,
					"Property 'name-prefix' missing or invalid\n");
				goto err;
			}

			if (of_property_read_string(subnp, "link-name",
				&tegra_codec_links[i].name)) {
				dev_err(&pdev->dev,
					"Property 'link-name' missing or invalid\n");
				goto err;
			}

			tegra_codec_links[i].stream_name = "Playback";
			tegra_codec_links[i].ignore_suspend =
				of_property_read_bool(subnp, "ignore_suspend");

#ifdef CONFIG_SND_SOC_TEGRA186_DSPK_ALT
			/* special case to handle specifically for dspk, connected to
			two mono amplifiers */
			if (!strcmp(tegra_codec_links[i].name, "dspk-playback-r"))
				tegra_codec_links[i].cpu_dai_name = "DAP Right";
			else if (!strcmp(tegra_codec_links[i].name, "dspk-playback-l"))
				tegra_codec_links[i].cpu_dai_name = "DAP Left";
			else
#endif
				tegra_codec_links[i].cpu_dai_name = "DAP";

			if (of_property_read_string(subnp, "codec-dai-name",
				&tegra_codec_links[i].codec_dai_name)) {
				dev_err(&pdev->dev,
					"Property 'codec-dai-name' missing or invalid\n");
				goto err;
			}
			tegra_codec_links[i].dai_fmt =
				snd_soc_of_parse_daifmt(subnp, NULL,
					&bitclkmaster, &framemaster);

			params = devm_kzalloc(&pdev->dev,
				sizeof(struct snd_soc_pcm_stream), GFP_KERNEL);

			if (of_property_read_string(subnp,
				"bit-format", (const char **)&str)) {
				dev_err(&pdev->dev,
					"Property 'bit-format' missing or invalid\n");
				goto err;
			}
			if (tegra_machine_get_format(&params->formats, str)) {
				dev_err(&pdev->dev,
					"Wrong codec format\n");
				goto err;
			}

			if (of_property_read_u32(subnp,
				"srate", &params->rate_min)) {
				dev_err(&pdev->dev,
					"Property 'srate' missing or invalid\n");
				goto err;
			}
			params->rate_max = params->rate_min;

			if (of_property_read_u32(subnp,
				"num-channel", &params->channels_min)) {
				dev_err(&pdev->dev,
					"Property 'num-channel' missing or invalid\n");
				goto err;
			}
			params->channels_max = params->channels_min;
			tegra_codec_links[i].params = params;

			of_property_read_u32(subnp,
				"bclk_ratio", (u32 *)&bclk_ratio[i]);

			of_property_read_u32(subnp,
				"rx-mask", (u32 *)&rx_mask[i]);
			of_property_read_u32(subnp,
				"tx-mask", (u32 *)&tx_mask[i]);

			/* CIF configuration */
			tegra_codec_links[j].codec_of_node =
				tegra_codec_links[i].cpu_of_node;
			tegra_codec_links[j].cpu_of_node =
				of_parse_phandle(np, "nvidia,xbar", 0);
			if (!tegra_codec_links[j].cpu_of_node) {
				dev_err(&pdev->dev,
					"Property 'nvidia,xbar' missing or invalid\n");
				goto err;
			}

#ifdef CONFIG_SND_SOC_TEGRA186_DSPK_ALT
			if (!strcmp(tegra_codec_links[i].name, "dspk-playback-r"))
				tegra_codec_links[j].codec_dai_name = "CIF Right";
			else if (!strcmp(tegra_codec_links[i].name, "dspk-playback-l"))
				tegra_codec_links[j].codec_dai_name = "CIF Left";
			else
#endif
				tegra_codec_links[j].codec_dai_name = "CIF";

			if (of_property_read_string(subnp, "cpu-dai-name",
				&tegra_codec_links[j].cpu_dai_name)) {
				dev_err(&pdev->dev,
					"Property 'cpu-dai-name' missing or invalid\n");
				goto err;
			}

			str = devm_kzalloc(&pdev->dev,
				sizeof(tegra_codec_links[j].cpu_dai_name) +
				1 + sizeof(tegra_codec_links[j].codec_dai_name),
				GFP_KERNEL);
			str = strcat(str, tegra_codec_links[j].cpu_dai_name);
			str = strcat(str, " ");
			str = strcat(str, tegra_codec_links[j].codec_dai_name);

			tegra_codec_links[j].name =
				tegra_codec_links[j].stream_name = str;
			tegra_codec_links[j].params =
				tegra_codec_links[i].params;
			tegra_codec_links[j].ignore_suspend = 1;
		} else {
			dev_err(&pdev->dev,
				"Property '%s' missing or invalid\n",
				dai_link_name);
			goto err;
		}
	}

	*pnum_codec_links = num_codec_links;

	return tegra_codec_links;
err:
	return NULL;
}
EXPORT_SYMBOL_GPL(tegra_machine_new_codec_links);


struct snd_soc_codec_conf *tegra_machine_new_codec_conf(
	struct platform_device *pdev,
	struct snd_soc_codec_conf *tegra_codec_conf,
	unsigned int *pnum_codec_links)
{
	unsigned int i, num_codec_links;
	struct device_node *np = pdev->dev.of_node, *subnp;
	const struct device_node *of_node;
	char dai_link_name[MAX_STR_SIZE];

	if (tegra_codec_conf)
		return tegra_codec_conf;

	if (!np)
		goto err;

	if (of_property_read_u32(np,
		"nvidia,num-codec-link", (u32 *)&num_codec_links)) {
		dev_err(&pdev->dev,
			"Property 'nvidia,num-codec-link' missing or invalid\n");
		goto err;
	}

	tegra_codec_conf =  devm_kzalloc(&pdev->dev, num_codec_links *
		sizeof(struct snd_soc_codec_conf), GFP_KERNEL);

	for (i = 0; i < num_codec_links; i++) {
		memset((void *)dai_link_name, '\0', MAX_STR_SIZE);
		sprintf(dai_link_name, "nvidia,dai-link-%d", i+1);
		subnp = of_get_child_by_name(np, dai_link_name);
		if (subnp) {
			of_node = of_parse_phandle(subnp, "codec-dai", 0);

			/* specify device by DT/OF node,
			   rather than device name */
			tegra_codec_conf[i].dev_name = NULL;
			tegra_codec_conf[i].of_node = of_node;

			if (of_property_read_string(subnp, "name-prefix",
				&tegra_codec_conf[i].name_prefix)) {
				dev_err(&pdev->dev,
					"Property 'name-prefix' missing or invalid\n");
				goto err;
			}
		}
	}

	*pnum_codec_links = num_codec_links;

	return tegra_codec_conf;
err:
	return NULL;
}
EXPORT_SYMBOL_GPL(tegra_machine_new_codec_conf);

/* This function is valid when dai_link is initiated from the DT */
unsigned int tegra_machine_get_codec_dai_link_idx(const char *codec_name)
{
	unsigned int idx = of_machine_is_compatible("nvidia,tegra210") ?
				TEGRA210_XBAR_DAI_LINKS :
				TEGRA124_XBAR_DAI_LINKS;

	if (num_dai_links <= idx)
		goto err;

	while (idx < num_dai_links) {
		if (tegra_asoc_machine_links[idx].name)
			if (!strcmp(tegra_asoc_machine_links[idx].name,
				codec_name))
				return idx;
		idx++;
	}

err:
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(tegra_machine_get_codec_dai_link_idx);

unsigned int tegra_machine_get_bclk_ratio(
	struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai_link *codec_dai_link = rtd->dai_link;
	char *codec_name = (char *)codec_dai_link->name;
	unsigned int idx =
		tegra_machine_get_codec_dai_link_idx(codec_name);

	if (idx == -EINVAL)
		goto err;

	if (!bclk_ratio)
		goto err;

	idx = idx - (of_machine_is_compatible("nvidia,tegra210") ?
			TEGRA210_XBAR_DAI_LINKS :
			TEGRA124_XBAR_DAI_LINKS);

	return bclk_ratio[idx];

err:
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(tegra_machine_get_bclk_ratio);

unsigned int tegra_machine_get_rx_mask(
	struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai_link *codec_dai_link = rtd->dai_link;
	char *codec_name = (char *)codec_dai_link->name;
	unsigned int idx =
		tegra_machine_get_codec_dai_link_idx(codec_name);

	if (idx == -EINVAL)
		goto err;

	if (!rx_mask)
		goto err;

	idx = idx - (of_machine_is_compatible("nvidia,tegra210") ?
			TEGRA210_XBAR_DAI_LINKS :
			TEGRA124_XBAR_DAI_LINKS);

	return rx_mask[idx];

err:
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(tegra_machine_get_rx_mask);

unsigned int tegra_machine_get_tx_mask(
	struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai_link *codec_dai_link = rtd->dai_link;
	char *codec_name = (char *)codec_dai_link->name;
	unsigned int idx =
		tegra_machine_get_codec_dai_link_idx(codec_name);

	if (idx == -EINVAL)
		goto err;

	if (!tx_mask)
		goto err;

	idx = idx - (of_machine_is_compatible("nvidia,tegra210") ?
			TEGRA210_XBAR_DAI_LINKS :
			TEGRA124_XBAR_DAI_LINKS);

	return tx_mask[idx];

err:
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(tegra_machine_get_tx_mask);

void tegra_machine_set_num_dai_links(unsigned int val)
{
	num_dai_links = val;
}
EXPORT_SYMBOL_GPL(tegra_machine_set_num_dai_links);

unsigned int tegra_machine_get_num_dai_links(void)
{
	return num_dai_links;
}
EXPORT_SYMBOL_GPL(tegra_machine_get_num_dai_links);

unsigned int *tegra_machine_get_bclk_ratio_array(void)
{
	return bclk_ratio;
}
EXPORT_SYMBOL_GPL(tegra_machine_get_bclk_ratio_array);

unsigned int *tegra_machine_get_rx_mask_array(void)
{
	return rx_mask;
}
EXPORT_SYMBOL_GPL(tegra_machine_get_rx_mask_array);
unsigned int *tegra_machine_get_tx_mask_array(void)
{
	return tx_mask;
}
EXPORT_SYMBOL_GPL(tegra_machine_get_tx_mask_array);
MODULE_AUTHOR("Arun Shamanna Lakshmi <aruns@nvidia.com>");
MODULE_AUTHOR("Junghyun Kim <juskim@nvidia.com>");
