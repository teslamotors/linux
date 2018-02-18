/*
 * arch/arm/mach-tegra/include/mach/tegra_vcm30t124_pdata.h
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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

#include <sound/pcm.h>
#include <sound/soc.h>

#define	MAX_I2S_DEVICES			5
#define	MAX_AMX_DEVICES			2
#define	MAX_ADX_DEVICES			2

/* dai_link_config - configures the necessary DAI link
 */
struct dai_link_config {
	const char *link_name;
	const char *cpu_name;
	const char *codec_name;
	const char *codec_prefix;
	const char *codec_dai_name;
	const char *cpu_dai_name;
	unsigned int dai_fmt;
	unsigned int bclk_ratio;
	unsigned int tx_mask;
	unsigned int rx_mask;
	struct snd_soc_pcm_stream params;
};

/* amx_adx_config - configures TDM slot map and params
 */
struct amx_adx_config {
	unsigned int *slot_map;
	unsigned int slot_size;
	struct snd_soc_pcm_stream *in_params;
	struct snd_soc_pcm_stream *out_params;
};

/* tegra_vcm30t124_platform_data - platform specific data
 * @config: configuration for each codec device
 * @dapm_routes: DAPM routes for the platform
 * @num_dapm_routes: number of DAPM routes
 * @dam_in_srate: DAM input sampling rates
 * @num_dam: number of DAMs to be initialized
 * @amx_slot_map: AMX slot map table
 * @num_amx: number of AMXs to be initialized
 * @adx_slot_map: ADX slot map table
 * @num_adx: number of ADXs to be initialized
 * @dev_num: number of 'config' devices initialized
 * @priv_data: platform specific private data if any
 * @card_name: sound card name needed (default: tegra-generic)
 */
struct tegra_vcm30t124_platform_data {
	struct dai_link_config dai_config[MAX_I2S_DEVICES];
	unsigned int dev_num;
	struct amx_adx_config amx_config[MAX_AMX_DEVICES];
	unsigned int num_amx;
	struct amx_adx_config adx_config[MAX_ADX_DEVICES];
	unsigned int num_adx;
	const struct snd_soc_dapm_route *dapm_routes;
	unsigned int num_dapm_routes;
	unsigned int *dam_in_srate;
	unsigned int num_dam;
	void *priv_data;
	const char *card_name;
};
