/*
 * tegra_asoc_machine_alt_t18x.c - Additional features for T186
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

#include <linux/module.h>
#include <sound/soc.h>
#include "tegra_asoc_machine_alt.h"
#include "tegra_asoc_machine_alt_t18x.h"

static struct snd_soc_pcm_stream default_link_params = {
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
};

static struct snd_soc_pcm_stream tdm_link_params = {
	.formats = SNDRV_PCM_FMTBIT_S32_LE,
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 8,
	.channels_max = 8,
};

static struct snd_soc_dai_link
	tegra186_xbar_dai_links[TEGRA186_XBAR_DAI_LINKS] = {
	[TEGRA186_DAI_LINK_ADMAIF1] = {
		.name = "ADMAIF1 CIF",
		.stream_name = "ADMAIF1 CIF",
		.cpu_dai_name = "ADMAIF1",
		.codec_dai_name = "ADMAIF1",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF2] = {
		.name = "ADMAIF2 CIF",
		.stream_name = "ADMAIF2 CIF",
		.cpu_dai_name = "ADMAIF2",
		.codec_dai_name = "ADMAIF2",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF3] = {
		.name = "ADMAIF3 CIF",
		.stream_name = "ADMAIF3 CIF",
		.cpu_dai_name = "ADMAIF3",
		.codec_dai_name = "ADMAIF3",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF4] = {
		.name = "ADMAIF4 CIF",
		.stream_name = "ADMAIF4 CIF",
		.cpu_dai_name = "ADMAIF4",
		.codec_dai_name = "ADMAIF4",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF5] = {
		.name = "ADMAIF5 CIF",
		.stream_name = "ADMAIF5 CIF",
		.cpu_dai_name = "ADMAIF5",
		.codec_dai_name = "ADMAIF5",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF6] = {
		.name = "ADMAIF6 CIF",
		.stream_name = "ADMAIF6 CIF",
		.cpu_dai_name = "ADMAIF6",
		.codec_dai_name = "ADMAIF6",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF7] = {
		.name = "ADMAIF7 CIF",
		.stream_name = "ADMAIF7 CIF",
		.cpu_dai_name = "ADMAIF7",
		.codec_dai_name = "ADMAIF7",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF8] = {
		.name = "ADMAIF8 CIF",
		.stream_name = "ADMAIF8 CIF",
		.cpu_dai_name = "ADMAIF8",
		.codec_dai_name = "ADMAIF8",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF9] = {
		.name = "ADMAIF9 CIF",
		.stream_name = "ADMAIF9 CIF",
		.cpu_dai_name = "ADMAIF9",
		.codec_dai_name = "ADMAIF9",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF10] = {
		.name = "ADMAIF10 CIF",
		.stream_name = "ADMAIF10 CIF",
		.cpu_dai_name = "ADMAIF10",
		.codec_dai_name = "ADMAIF10",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF11] = {
		.name = "ADMAIF11 CIF",
		.stream_name = "ADMAIF11 CIF",
		.cpu_dai_name = "ADMAIF11",
		.codec_dai_name = "ADMAIF11",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF12] = {
		.name = "ADMAIF12 CIF",
		.stream_name = "ADMAIF12 CIF",
		.cpu_dai_name = "ADMAIF12",
		.codec_dai_name = "ADMAIF12",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF13] = {
		.name = "ADMAIF13 CIF",
		.stream_name = "ADMAIF13 CIF",
		.cpu_dai_name = "ADMAIF13",
		.codec_dai_name = "ADMAIF13",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF14] = {
		.name = "ADMAIF14 CIF",
		.stream_name = "ADMAIF14 CIF",
		.cpu_dai_name = "ADMAIF14",
		.codec_dai_name = "ADMAIF14",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF15] = {
		.name = "ADMAIF15 CIF",
		.stream_name = "ADMAIF15 CIF",
		.cpu_dai_name = "ADMAIF15",
		.codec_dai_name = "ADMAIF15",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF16] = {
		.name = "ADMAIF16 CIF",
		.stream_name = "ADMAIF16 CIF",
		.cpu_dai_name = "ADMAIF16",
		.codec_dai_name = "ADMAIF16",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF17] = {
		.name = "ADMAIF17 CIF",
		.stream_name = "ADMAIF17 CIF",
		.cpu_dai_name = "ADMAIF17",
		.codec_dai_name = "ADMAIF17",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF18] = {
		.name = "ADMAIF18 CIF",
		.stream_name = "ADMAIF18 CIF",
		.cpu_dai_name = "ADMAIF18",
		.codec_dai_name = "ADMAIF18",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF19] = {
		.name = "ADMAIF19 CIF",
		.stream_name = "ADMAIF19 CIF",
		.cpu_dai_name = "ADMAIF19",
		.codec_dai_name = "ADMAIF19",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF20] = {
		.name = "ADMAIF20 CIF",
		.stream_name = "ADMAIF20 CIF",
		.cpu_dai_name = "ADMAIF20",
		.codec_dai_name = "ADMAIF20",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF1_CODEC] = {
		.name = "ADMAIF1 CODEC",
		.stream_name = "ADMAIF1 CODEC",
		.cpu_dai_name = "ADMAIF1 CIF",
		.codec_dai_name = "ADMAIF1",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF2_CODEC] = {
		.name = "ADMAIF2 CODEC",
		.stream_name = "ADMAIF2 CODEC",
		.cpu_dai_name = "ADMAIF2 CIF",
		.codec_dai_name = "ADMAIF2",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF3_CODEC] = {
		.name = "ADMAIF3 CODEC",
		.stream_name = "ADMAIF3 CODEC",
		.cpu_dai_name = "ADMAIF3 CIF",
		.codec_dai_name = "ADMAIF3",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF4_CODEC] = {
		.name = "ADMAIF4 CODEC",
		.stream_name = "ADMAIF4 CODEC",
		.cpu_dai_name = "ADMAIF4 CIF",
		.codec_dai_name = "ADMAIF4",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF5_CODEC] = {
		.name = "ADMAIF5 CODEC",
		.stream_name = "ADMAIF5 CODEC",
		.cpu_dai_name = "ADMAIF5 CIF",
		.codec_dai_name = "ADMAIF5",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF6_CODEC] = {
		.name = "ADMAIF6 CODEC",
		.stream_name = "ADMAIF6 CODEC",
		.cpu_dai_name = "ADMAIF6 CIF",
		.codec_dai_name = "ADMAIF6",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF7_CODEC] = {
		.name = "ADMAIF7 CODEC",
		.stream_name = "ADMAIF7 CODEC",
		.cpu_dai_name = "ADMAIF7 CIF",
		.codec_dai_name = "ADMAIF7",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF8_CODEC] = {
		.name = "ADMAIF8 CODEC",
		.stream_name = "ADMAIF8 CODEC",
		.cpu_dai_name = "ADMAIF8 CIF",
		.codec_dai_name = "ADMAIF8",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF9_CODEC] = {
		.name = "ADMAIF9 CODEC",
		.stream_name = "ADMAIF9 CODEC",
		.cpu_dai_name = "ADMAIF9 CIF",
		.codec_dai_name = "ADMAIF9",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF10_CODEC] = {
		.name = "ADMAIF10 CODEC",
		.stream_name = "ADMAIF10 CODEC",
		.cpu_dai_name = "ADMAIF10 CIF",
		.codec_dai_name = "ADMAIF10",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF11_CODEC] = {
		.name = "ADMAIF11 CODEC",
		.stream_name = "ADMAIF11 CODEC",
		.cpu_dai_name = "ADMAIF11 CIF",
		.codec_dai_name = "ADMAIF11",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF12_CODEC] = {
		.name = "ADMAIF12 CODEC",
		.stream_name = "ADMAIF12 CODEC",
		.cpu_dai_name = "ADMAIF12 CIF",
		.codec_dai_name = "ADMAIF12",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF13_CODEC] = {
		.name = "ADMAIF13 CODEC",
		.stream_name = "ADMAIF13 CODEC",
		.cpu_dai_name = "ADMAIF13 CIF",
		.codec_dai_name = "ADMAIF13",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF14_CODEC] = {
		.name = "ADMAIF14 CODEC",
		.stream_name = "ADMAIF14 CODEC",
		.cpu_dai_name = "ADMAIF14 CIF",
		.codec_dai_name = "ADMAIF14",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF15_CODEC] = {
		.name = "ADMAIF15 CODEC",
		.stream_name = "ADMAIF15 CODEC",
		.cpu_dai_name = "ADMAIF15 CIF",
		.codec_dai_name = "ADMAIF15",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF16_CODEC] = {
		.name = "ADMAIF16 CODEC",
		.stream_name = "ADMAIF16 CODEC",
		.cpu_dai_name = "ADMAIF16 CIF",
		.codec_dai_name = "ADMAIF16",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF17_CODEC] = {
		.name = "ADMAIF17 CODEC",
		.stream_name = "ADMAIF17 CODEC",
		.cpu_dai_name = "ADMAIF17 CIF",
		.codec_dai_name = "ADMAIF17",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF18_CODEC] = {
		.name = "ADMAIF18 CODEC",
		.stream_name = "ADMAIF18 CODEC",
		.cpu_dai_name = "ADMAIF18 CIF",
		.codec_dai_name = "ADMAIF18",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF19_CODEC] = {
		.name = "ADMAIF19 CODEC",
		.stream_name = "ADMAIF19 CODEC",
		.cpu_dai_name = "ADMAIF19 CIF",
		.codec_dai_name = "ADMAIF19",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADMAIF20_CODEC] = {
		.name = "ADMAIF20 CODEC",
		.stream_name = "ADMAIF20 CODEC",
		.cpu_dai_name = "ADMAIF20 CIF",
		.codec_dai_name = "ADMAIF20",
		.cpu_name = "tegra210-admaif",
		.codec_name = "2900800.ahub",
		.platform_name = "tegra210-admaif",
		.ignore_pmdown_time = 1,
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
#if defined(CONFIG_SND_SOC_TEGRA210_ADSP_ALT)
	[TEGRA186_DAI_LINK_ADSP_ADMAIF1] = {
		.name = "ADSP ADMAIF1",
		.stream_name = "ADSP ADMAIF1",
		.cpu_dai_name = "ADSP-ADMAIF1",
		.codec_dai_name = "ADMAIF1 FIFO",
		.cpu_name = "adsp_audio",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADSP_ADMAIF2] = {
		.name = "ADSP ADMAIF2",
		.stream_name = "ADSP ADMAIF2",
		.cpu_dai_name = "ADSP-ADMAIF2",
		.codec_dai_name = "ADMAIF2 FIFO",
		.cpu_name = "adsp_audio",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADSP_ADMAIF3] = {
		.name = "ADSP ADMAIF3",
		.stream_name = "ADSP ADMAIF3",
		.cpu_dai_name = "ADSP-ADMAIF3",
		.codec_dai_name = "ADMAIF3 FIFO",
		.cpu_name = "adsp_audio",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADSP_ADMAIF4] = {
		.name = "ADSP ADMAIF4",
		.stream_name = "ADSP ADMAIF4",
		.cpu_dai_name = "ADSP-ADMAIF4",
		.codec_dai_name = "ADMAIF4 FIFO",
		.cpu_name = "adsp_audio",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADSP_ADMAIF5] = {
		.name = "ADSP ADMAIF5",
		.stream_name = "ADSP ADMAIF5",
		.cpu_dai_name = "ADSP-ADMAIF5",
		.codec_dai_name = "ADMAIF5 FIFO",
		.cpu_name = "adsp_audio",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADSP_ADMAIF6] = {
		.name = "ADSP ADMAIF6",
		.stream_name = "ADSP ADMAIF6",
		.cpu_dai_name = "ADSP-ADMAIF6",
		.codec_dai_name = "ADMAIF6 FIFO",
		.cpu_name = "adsp_audio",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADSP_ADMAIF7] = {
		.name = "ADSP ADMAIF7",
		.stream_name = "ADSP ADMAIF7",
		.cpu_dai_name = "ADSP-ADMAIF7",
		.codec_dai_name = "ADMAIF7 FIFO",
		.cpu_name = "adsp_audio",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADSP_ADMAIF8] = {
		.name = "ADSP ADMAIF8",
		.stream_name = "ADSP ADMAIF8",
		.cpu_dai_name = "ADSP-ADMAIF8",
		.codec_dai_name = "ADMAIF8 FIFO",
		.cpu_name = "adsp_audio",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADSP_ADMAIF9] = {
		.name = "ADSP ADMAIF9",
		.stream_name = "ADSP ADMAIF9",
		.cpu_dai_name = "ADSP-ADMAIF9",
		.codec_dai_name = "ADMAIF9 FIFO",
		.cpu_name = "adsp_audio",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADSP_ADMAIF10] = {
		.name = "ADSP ADMAIF10",
		.stream_name = "ADSP ADMAIF10",
		.cpu_dai_name = "ADSP-ADMAIF10",
		.codec_dai_name = "ADMAIF10 FIFO",
		.cpu_name = "adsp_audio",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADSP_ADMAIF11] = {
		.name = "ADSP ADMAIF11",
		.stream_name = "ADSP ADMAIF11",
		.cpu_dai_name = "ADSP-ADMAIF11",
		.codec_dai_name = "ADMAIF11 FIFO",
		.cpu_name = "adsp_audio",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADSP_ADMAIF12] = {
		.name = "ADSP ADMAIF12",
		.stream_name = "ADSP ADMAIF12",
		.cpu_dai_name = "ADSP-ADMAIF12",
		.codec_dai_name = "ADMAIF12 FIFO",
		.cpu_name = "adsp_audio",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADSP_ADMAIF13] = {
		.name = "ADSP ADMAIF13",
		.stream_name = "ADSP ADMAIF13",
		.cpu_dai_name = "ADSP-ADMAIF13",
		.codec_dai_name = "ADMAIF13 FIFO",
		.cpu_name = "adsp_audio",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADSP_ADMAIF14] = {
		.name = "ADSP ADMAIF14",
		.stream_name = "ADSP ADMAIF14",
		.cpu_dai_name = "ADSP-ADMAIF14",
		.codec_dai_name = "ADMAIF14 FIFO",
		.cpu_name = "adsp_audio",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADSP_ADMAIF15] = {
		.name = "ADSP ADMAIF15",
		.stream_name = "ADSP ADMAIF15",
		.cpu_dai_name = "ADSP-ADMAIF15",
		.codec_dai_name = "ADMAIF15 FIFO",
		.cpu_name = "adsp_audio",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADSP_ADMAIF16] = {
		.name = "ADSP ADMAIF16",
		.stream_name = "ADSP ADMAIF16",
		.cpu_dai_name = "ADSP-ADMAIF16",
		.codec_dai_name = "ADMAIF16 FIFO",
		.cpu_name = "adsp_audio",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADSP_ADMAIF17] = {
		.name = "ADSP ADMAIF17",
		.stream_name = "ADSP ADMAIF17",
		.cpu_dai_name = "ADSP-ADMAIF17",
		.codec_dai_name = "ADMAIF17 FIFO",
		.cpu_name = "adsp_audio",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADSP_ADMAIF18] = {
		.name = "ADSP ADMAIF18",
		.stream_name = "ADSP ADMAIF18",
		.cpu_dai_name = "ADSP-ADMAIF18",
		.codec_dai_name = "ADMAIF18 FIFO",
		.cpu_name = "adsp_audio",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADSP_ADMAIF19] = {
		.name = "ADSP ADMAIF19",
		.stream_name = "ADSP ADMAIF19",
		.cpu_dai_name = "ADSP-ADMAIF19",
		.codec_dai_name = "ADMAIF19 FIFO",
		.cpu_name = "adsp_audio",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADSP_ADMAIF20] = {
		.name = "ADSP ADMAIF20",
		.stream_name = "ADSP ADMAIF20",
		.cpu_dai_name = "ADSP-ADMAIF20",
		.codec_dai_name = "ADMAIF20 FIFO",
		.cpu_name = "adsp_audio",
		.codec_name = "tegra210-admaif",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADSP_PCM1] = {
		.name = "ADSP PCM1",
		.stream_name = "ADSP PCM1",
		.cpu_dai_name = "ADSP PCM1",
		.codec_dai_name = "ADSP-FE1",
		.cpu_name = "adsp_audio",
		.codec_name = "adsp_audio",
		.platform_name = "adsp_audio",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADSP_PCM2] = {
		.name = "ADSP PCM2",
		.stream_name = "ADSP PCM2",
		.cpu_dai_name = "ADSP PCM2",
		.codec_dai_name = "ADSP-FE2",
		.cpu_name = "adsp_audio",
		.codec_name = "adsp_audio",
		.platform_name = "adsp_audio",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADSP_COMPR1] = {
		.name = "ADSP COMPR1",
		.stream_name = "ADSP COMPR1",
		.cpu_dai_name = "ADSP COMPR1",
		.codec_dai_name = "ADSP-FE3",
		.cpu_name = "adsp_audio",
		.codec_name = "adsp_audio",
		.platform_name = "adsp_audio",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADSP_COMPR2] = {
		.name = "ADSP COMPR2",
		.stream_name = "ADSP COMPR2",
		.cpu_dai_name = "ADSP COMPR2",
		.codec_dai_name = "ADSP-FE4",
		.cpu_name = "adsp_audio",
		.codec_name = "adsp_audio",
		.platform_name = "adsp_audio",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
#endif
	[TEGRA186_DAI_LINK_AMX1_1] = {
		.name = "AMX1 IN1",
		.stream_name = "AMX1 IN",
		.cpu_dai_name = "AMX1-1",
		.codec_dai_name = "IN1",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-amx.0",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AMX1_2] = {
		.name = "AMX1 IN2",
		.stream_name = "AMX1 IN",
		.cpu_dai_name = "AMX1-2",
		.codec_dai_name = "IN2",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-amx.0",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AMX1_3] = {
		.name = "AMX1 IN3",
		.stream_name = "AMX1 IN",
		.cpu_dai_name = "AMX1-3",
		.codec_dai_name = "IN3",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-amx.0",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AMX1_4] = {
		.name = "AMX1 IN4",
		.stream_name = "AMX1 IN",
		.cpu_dai_name = "AMX1-4",
		.codec_dai_name = "IN4",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-amx.0",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AMX1] = {
		.name = "AMX1 CIF",
		.stream_name = "AMX1 CIF",
		.cpu_dai_name = "OUT",
		.codec_dai_name = "AMX1",
		.cpu_name = "tegra210-amx.0",
		.codec_name = "2900800.ahub",
		.params = &tdm_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AMX2_1] = {
		.name = "AMX2 IN1",
		.stream_name = "AMX2 IN",
		.cpu_dai_name = "AMX2-1",
		.codec_dai_name = "IN1",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-amx.1",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AMX2_2] = {
		.name = "AMX2 IN2",
		.stream_name = "AMX2 IN",
		.cpu_dai_name = "AMX2-2",
		.codec_dai_name = "IN2",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-amx.1",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AMX2_3] = {
		.name = "AMX2 IN3",
		.stream_name = "AMX2 IN",
		.cpu_dai_name = "AMX2-3",
		.codec_dai_name = "IN3",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-amx.1",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AMX2_4] = {
		.name = "AMX2 IN4",
		.stream_name = "AMX2 IN",
		.cpu_dai_name = "AMX2-4",
		.codec_dai_name = "IN4",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-amx.1",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AMX2] = {
		.name = "AMX2 CIF",
		.stream_name = "AMX2 CIF",
		.cpu_dai_name = "OUT",
		.codec_dai_name = "AMX2",
		.cpu_name = "tegra210-amx.1",
		.codec_name = "2900800.ahub",
		.params = &tdm_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AMX3_1] = {
		.name = "AMX3 IN1",
		.stream_name = "AMX3 IN",
		.cpu_dai_name = "AMX3-1",
		.codec_dai_name = "IN1",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-amx.2",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AMX3_2] = {
		.name = "AMX3 IN2",
		.stream_name = "AMX3 IN",
		.cpu_dai_name = "AMX3-2",
		.codec_dai_name = "IN2",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-amx.2",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AMX3_3] = {
		.name = "AMX3 IN3",
		.stream_name = "AMX3 IN",
		.cpu_dai_name = "AMX3-3",
		.codec_dai_name = "IN3",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-amx.2",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AMX3_4] = {
		.name = "AMX3 IN4",
		.stream_name = "AMX3 IN",
		.cpu_dai_name = "AMX3-4",
		.codec_dai_name = "IN4",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-amx.2",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AMX3] = {
		.name = "AMX3 CIF",
		.stream_name = "AMX3 CIF",
		.cpu_dai_name = "OUT",
		.codec_dai_name = "AMX3",
		.cpu_name = "tegra210-amx.2",
		.codec_name = "2900800.ahub",
		.params = &tdm_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AMX4_1] = {
		.name = "AMX4 IN1",
		.stream_name = "AMX4 IN",
		.cpu_dai_name = "AMX4-1",
		.codec_dai_name = "IN1",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-amx.3",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AMX4_2] = {
		.name = "AMX4 IN2",
		.stream_name = "AMX4 IN",
		.cpu_dai_name = "AMX4-2",
		.codec_dai_name = "IN2",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-amx.3",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AMX4_3] = {
		.name = "AMX4 IN3",
		.stream_name = "AMX4 IN",
		.cpu_dai_name = "AMX4-3",
		.codec_dai_name = "IN3",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-amx.3",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AMX4_4] = {
		.name = "AMX4 IN4",
		.stream_name = "AMX4 IN",
		.cpu_dai_name = "AMX4-4",
		.codec_dai_name = "IN4",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-amx.3",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AMX4] = {
		.name = "AMX4 CIF",
		.stream_name = "AMX4 CIF",
		.cpu_dai_name = "OUT",
		.codec_dai_name = "AMX4",
		.cpu_name = "tegra210-amx.3",
		.codec_name = "2900800.ahub",
		.params = &tdm_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADX1] = {
		.name = "ADX1 CIF",
		.stream_name = "ADX1 IN",
		.cpu_dai_name = "ADX1",
		.codec_dai_name = "IN",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-adx.0",
		.params = &tdm_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADX1_1] = {
		.name = "ADX1 OUT1",
		.stream_name = "ADX1 OUT",
		.cpu_dai_name = "OUT1",
		.codec_dai_name = "ADX1-1",
		.cpu_name = "tegra210-adx.0",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADX1_2] = {
		.name = "ADX1 OUT2",
		.stream_name = "ADX1 OUT",
		.cpu_dai_name = "OUT2",
		.codec_dai_name = "ADX1-2",
		.cpu_name = "tegra210-adx.0",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADX1_3] = {
		.name = "ADX1 OUT3",
		.stream_name = "ADX1 OUT",
		.cpu_dai_name = "OUT3",
		.codec_dai_name = "ADX1-3",
		.cpu_name = "tegra210-adx.0",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADX1_4] = {
		.name = "ADX1 OUT4",
		.stream_name = "ADX1 OUT",
		.cpu_dai_name = "OUT4",
		.codec_dai_name = "ADX1-4",
		.cpu_name = "tegra210-adx.0",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADX2] = {
		.name = "ADX2 CIF",
		.stream_name = "ADX2 IN",
		.cpu_dai_name = "ADX2",
		.codec_dai_name = "IN",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-adx.1",
		.params = &tdm_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADX2_1] = {
		.name = "ADX2 OUT1",
		.stream_name = "ADX2 OUT",
		.cpu_dai_name = "OUT1",
		.codec_dai_name = "ADX2-1",
		.cpu_name = "tegra210-adx.1",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADX2_2] = {
		.name = "ADX2 OUT2",
		.stream_name = "ADX2 OUT",
		.cpu_dai_name = "OUT2",
		.codec_dai_name = "ADX2-2",
		.cpu_name = "tegra210-adx.1",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADX2_3] = {
		.name = "ADX2 OUT3",
		.stream_name = "ADX2 OUT",
		.cpu_dai_name = "OUT3",
		.codec_dai_name = "ADX2-3",
		.cpu_name = "tegra210-adx.1",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADX2_4] = {
		.name = "ADX2 OUT4",
		.stream_name = "ADX2 OUT",
		.cpu_dai_name = "OUT4",
		.codec_dai_name = "ADX2-4",
		.cpu_name = "tegra210-adx.1",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADX3] = {
		.name = "ADX3 CIF",
		.stream_name = "ADX3 IN",
		.cpu_dai_name = "ADX3",
		.codec_dai_name = "IN",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-adx.2",
		.params = &tdm_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADX3_1] = {
		.name = "ADX3 OUT1",
		.stream_name = "ADX3 OUT",
		.cpu_dai_name = "OUT1",
		.codec_dai_name = "ADX3-1",
		.cpu_name = "tegra210-adx.2",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADX3_2] = {
		.name = "ADX3 OUT2",
		.stream_name = "ADX3 OUT",
		.cpu_dai_name = "OUT2",
		.codec_dai_name = "ADX3-2",
		.cpu_name = "tegra210-adx.2",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADX3_3] = {
		.name = "ADX3 OUT3",
		.stream_name = "ADX3 OUT",
		.cpu_dai_name = "OUT3",
		.codec_dai_name = "ADX3-3",
		.cpu_name = "tegra210-adx.2",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADX3_4] = {
		.name = "ADX3 OUT4",
		.stream_name = "ADX3 OUT",
		.cpu_dai_name = "OUT4",
		.codec_dai_name = "ADX3-4",
		.cpu_name = "tegra210-adx.2",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADX4] = {
		.name = "ADX4 CIF",
		.stream_name = "ADX4 IN",
		.cpu_dai_name = "ADX4",
		.codec_dai_name = "IN",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-adx.3",
		.params = &tdm_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADX4_1] = {
		.name = "ADX4 OUT1",
		.stream_name = "ADX4 OUT",
		.cpu_dai_name = "OUT1",
		.codec_dai_name = "ADX4-1",
		.cpu_name = "tegra210-adx.3",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADX4_2] = {
		.name = "ADX4 OUT2",
		.stream_name = "ADX4 OUT",
		.cpu_dai_name = "OUT2",
		.codec_dai_name = "ADX4-2",
		.cpu_name = "tegra210-adx.3",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADX4_3] = {
		.name = "ADX4 OUT3",
		.stream_name = "ADX4 OUT",
		.cpu_dai_name = "OUT3",
		.codec_dai_name = "ADX4-3",
		.cpu_name = "tegra210-adx.3",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ADX4_4] = {
		.name = "ADX4 OUT4",
		.stream_name = "ADX4 OUT",
		.cpu_dai_name = "OUT4",
		.codec_dai_name = "ADX4-4",
		.cpu_name = "tegra210-adx.3",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_MIXER1_RX1] = {
		.name = "MIXER1 RX1",
		.stream_name = "MIXER1 RX",
		.cpu_dai_name = "MIXER1-1",
		.codec_dai_name = "RX1",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-mixer",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_MIXER1_RX2] = {
		.name = "MIXER1 RX2",
		.stream_name = "MIXER1 RX",
		.cpu_dai_name = "MIXER1-2",
		.codec_dai_name = "RX2",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-mixer",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_MIXER1_RX3] = {
		.name = "MIXER1 RX3",
		.stream_name = "MIXER1 RX",
		.cpu_dai_name = "MIXER1-3",
		.codec_dai_name = "RX3",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-mixer",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_MIXER1_RX4] = {
		.name = "MIXER1 RX4",
		.stream_name = "MIXER1 RX",
		.cpu_dai_name = "MIXER1-4",
		.codec_dai_name = "RX4",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-mixer",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_MIXER1_RX5] = {
		.name = "MIXER1 RX5",
		.stream_name = "MIXER1 RX",
		.cpu_dai_name = "MIXER1-5",
		.codec_dai_name = "RX5",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-mixer",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_MIXER1_RX6] = {
		.name = "MIXER1 RX6",
		.stream_name = "MIXER1 RX",
		.cpu_dai_name = "MIXER1-6",
		.codec_dai_name = "RX6",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-mixer",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_MIXER1_RX7] = {
		.name = "MIXER1 RX7",
		.stream_name = "MIXER1 RX",
		.cpu_dai_name = "MIXER1-7",
		.codec_dai_name = "RX7",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-mixer",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_MIXER1_RX8] = {
		.name = "MIXER1 RX8",
		.stream_name = "MIXER1 RX",
		.cpu_dai_name = "MIXER1-8",
		.codec_dai_name = "RX8",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-mixer",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_MIXER1_RX9] = {
		.name = "MIXER1 RX9",
		.stream_name = "MIXER1 RX",
		.cpu_dai_name = "MIXER1-9",
		.codec_dai_name = "RX9",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-mixer",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_MIXER1_RX10] = {
		.name = "MIXER1 RX10",
		.stream_name = "MIXER1 RX",
		.cpu_dai_name = "MIXER1-10",
		.codec_dai_name = "RX10",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-mixer",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_MIXER1_TX1] = {
		.name = "MIXER1 TX1",
		.stream_name = "MIXER1 TX",
		.cpu_dai_name = "TX1",
		.codec_dai_name = "MIXER1-1",
		.cpu_name = "tegra210-mixer",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_MIXER1_TX2] = {
		.name = "MIXER1 TX2",
		.stream_name = "MIXER1 TX",
		.cpu_dai_name = "TX2",
		.codec_dai_name = "MIXER1-2",
		.cpu_name = "tegra210-mixer",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_MIXER1_TX3] = {
		.name = "MIXER1 TX3",
		.stream_name = "MIXER1 TX",
		.cpu_dai_name = "TX3",
		.codec_dai_name = "MIXER1-3",
		.cpu_name = "tegra210-mixer",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_MIXER1_TX4] = {
		.name = "MIXER1 TX4",
		.stream_name = "MIXER1 TX",
		.cpu_dai_name = "TX4",
		.codec_dai_name = "MIXER1-4",
		.cpu_name = "tegra210-mixer",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_MIXER1_TX5] = {
		.name = "MIXER1 TX5",
		.stream_name = "MIXER1 TX",
		.cpu_dai_name = "TX5",
		.codec_dai_name = "MIXER1-5",
		.cpu_name = "tegra210-mixer",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_SFC1_RX] = {
		.name = "SFC1 RX",
		.stream_name = "SFC1 RX",
		.cpu_dai_name = "SFC1",
		.codec_dai_name = "CIF",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-sfc.0",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_SFC1_TX] = {
		.name = "SFC1 TX",
		.stream_name = "SFC1 TX",
		.cpu_dai_name = "DAP",
		.codec_dai_name = "SFC1",
		.cpu_name = "tegra210-sfc.0",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_SFC2_RX] = {
		.name = "SFC2 RX",
		.stream_name = "SFC2 RX",
		.cpu_dai_name = "SFC2",
		.codec_dai_name = "CIF",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-sfc.1",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_SFC2_TX] = {
		.name = "SFC2 TX",
		.stream_name = "SFC2 TX",
		.cpu_dai_name = "DAP",
		.codec_dai_name = "SFC2",
		.cpu_name = "tegra210-sfc.1",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_SFC3_RX] = {
		.name = "SFC3 RX",
		.stream_name = "SFC3 RX",
		.cpu_dai_name = "SFC3",
		.codec_dai_name = "CIF",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-sfc.2",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_SFC3_TX] = {
		.name = "SFC3 TX",
		.stream_name = "SFC3 TX",
		.cpu_dai_name = "DAP",
		.codec_dai_name = "SFC3",
		.cpu_name = "tegra210-sfc.2",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_SFC4_RX] = {
		.name = "SFC4 RX",
		.stream_name = "SFC4 RX",
		.cpu_dai_name = "SFC4",
		.codec_dai_name = "CIF",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-sfc.3",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_SFC4_TX] = {
		.name = "SFC4 TX",
		.stream_name = "SFC4 TX",
		.cpu_dai_name = "DAP",
		.codec_dai_name = "SFC4",
		.cpu_name = "tegra210-sfc.3",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AFC1_RX] = {
		.name = "AFC1 RX",
		.stream_name = "AFC1 RX",
		.cpu_dai_name = "AFC1",
		.codec_dai_name = "AFC IN",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-afc.0",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AFC1_TX] = {
		.name = "AFC1 TX",
		.stream_name = "AFC1 TX",
		.cpu_dai_name = "AFC OUT",
		.codec_dai_name = "AFC1",
		.cpu_name = "tegra210-afc.0",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AFC2_RX] = {
		.name = "AFC2 RX",
		.stream_name = "AFC2 RX",
		.cpu_dai_name = "AFC2",
		.codec_dai_name = "AFC IN",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-afc.1",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AFC2_TX] = {
		.name = "AFC2 TX",
		.stream_name = "AFC2 TX",
		.cpu_dai_name = "AFC OUT",
		.codec_dai_name = "AFC2",
		.cpu_name = "tegra210-afc.1",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AFC3_RX] = {
		.name = "AFC3 RX",
		.stream_name = "AFC3 RX",
		.cpu_dai_name = "AFC3",
		.codec_dai_name = "AFC IN",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-afc.2",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AFC3_TX] = {
		.name = "AFC3 TX",
		.stream_name = "AFC3 TX",
		.cpu_dai_name = "AFC OUT",
		.codec_dai_name = "AFC3",
		.cpu_name = "tegra210-afc.2",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AFC4_RX] = {
		.name = "AFC4 RX",
		.stream_name = "AFC4 RX",
		.cpu_dai_name = "AFC4",
		.codec_dai_name = "AFC IN",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-afc.3",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AFC4_TX] = {
		.name = "AFC4 TX",
		.stream_name = "AFC4 TX",
		.cpu_dai_name = "AFC OUT",
		.codec_dai_name = "AFC4",
		.cpu_name = "tegra210-afc.3",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AFC5_RX] = {
		.name = "AFC5 RX",
		.stream_name = "AFC5 RX",
		.cpu_dai_name = "AFC5",
		.codec_dai_name = "AFC IN",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-afc.4",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AFC5_TX] = {
		.name = "AFC5 TX",
		.stream_name = "AFC5 TX",
		.cpu_dai_name = "AFC OUT",
		.codec_dai_name = "AFC5",
		.cpu_name = "tegra210-afc.4",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AFC6_RX] = {
		.name = "AFC6 RX",
		.stream_name = "AFC6 RX",
		.cpu_dai_name = "AFC6",
		.codec_dai_name = "AFC IN",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-afc.5",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_AFC6_TX] = {
		.name = "AFC6 TX",
		.stream_name = "AFC6 TX",
		.cpu_dai_name = "AFC OUT",
		.codec_dai_name = "AFC6",
		.cpu_name = "tegra210-afc.5",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_MVC1_RX] = {
		.name = "MVC1 RX",
		.stream_name = "MVC1 RX",
		.cpu_dai_name = "MVC1",
		.codec_dai_name = "MVC IN",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-mvc.0",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_MVC1_TX] = {
		.name = "MVC1 TX",
		.stream_name = "MVC1 TX",
		.cpu_dai_name = "MVC OUT",
		.codec_dai_name = "MVC1",
		.cpu_name = "tegra210-mvc.0",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_MVC2_RX] = {
		.name = "MVC2 RX",
		.stream_name = "MVC2 RX",
		.cpu_dai_name = "MVC2",
		.codec_dai_name = "MVC IN",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-mvc.1",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_MVC2_TX] = {
		.name = "MVC2 TX",
		.stream_name = "MVC2 TX",
		.cpu_dai_name = "MVC OUT",
		.codec_dai_name = "MVC2",
		.cpu_name = "tegra210-mvc.1",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_OPE1_RX] = {
		.name = "OPE1 RX",
		.stream_name = "OPE1 RX",
		.cpu_dai_name = "OPE1",
		.codec_dai_name = "OPE IN",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra210-ope.0",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_OPE1_TX] = {
		.name = "OPE1 TX",
		.stream_name = "OPE1 TX",
		.cpu_dai_name = "OPE OUT",
		.codec_dai_name = "OPE1",
		.cpu_name = "tegra210-ope.0",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ASRC1_RX1] = {
		.name = "ASRC1 RX1",
		.stream_name = "ASRC1 RX",
		.cpu_dai_name = "ASRC1-1",
		.codec_dai_name = "RX1",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra186-asrc",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ASRC1_RX2] = {
		.name = "ASRC1 RX2",
		.stream_name = "ASRC1 RX",
		.cpu_dai_name = "ASRC1-2",
		.codec_dai_name = "RX2",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra186-asrc",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ASRC1_RX3] = {
		.name = "ASRC1 RX3",
		.stream_name = "ASRC1 RX",
		.cpu_dai_name = "ASRC1-3",
		.codec_dai_name = "RX3",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra186-asrc",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ASRC1_RX4] = {
		.name = "ASRC1 RX4",
		.stream_name = "ASRC1 RX",
		.cpu_dai_name = "ASRC1-4",
		.codec_dai_name = "RX4",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra186-asrc",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ASRC1_RX5] = {
		.name = "ASRC1 RX5",
		.stream_name = "ASRC1 RX",
		.cpu_dai_name = "ASRC1-5",
		.codec_dai_name = "RX5",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra186-asrc",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ASRC1_RX6] = {
		.name = "ASRC1 RX6",
		.stream_name = "ASRC1 RX",
		.cpu_dai_name = "ASRC1-6",
		.codec_dai_name = "RX6",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra186-asrc",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ASRC1_RX7] = {
		.name = "ASRC1 RX7",
		.stream_name = "ASRC1 RX",
		.cpu_dai_name = "ASRC1-7",
		.codec_dai_name = "RX7",
		.cpu_name = "2900800.ahub",
		.codec_name = "tegra186-asrc",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ASRC1_TX1] = {
		.name = "ASRC1 TX1",
		.stream_name = "ASRC1 TX",
		.cpu_dai_name = "TX1",
		.codec_dai_name = "ASRC1-1",
		.cpu_name = "tegra186-asrc",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ASRC1_TX2] = {
		.name = "ASRC1 TX2",
		.stream_name = "ASRC1 TX",
		.cpu_dai_name = "TX2",
		.codec_dai_name = "ASRC1-2",
		.cpu_name = "tegra186-asrc",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ASRC1_TX3] = {
		.name = "ASRC1 TX3",
		.stream_name = "ASRC1 TX",
		.cpu_dai_name = "TX3",
		.codec_dai_name = "ASRC1-3",
		.cpu_name = "tegra186-asrc",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ASRC1_TX4] = {
		.name = "ASRC1 TX4",
		.stream_name = "ASRC1 TX",
		.cpu_dai_name = "TX4",
		.codec_dai_name = "ASRC1-4",
		.cpu_name = "tegra186-asrc",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ASRC1_TX5] = {
		.name = "ASRC1 TX5",
		.stream_name = "ASRC1 TX",
		.cpu_dai_name = "TX5",
		.codec_dai_name = "ASRC1-5",
		.cpu_name = "tegra186-asrc",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
	[TEGRA186_DAI_LINK_ASRC1_TX6] = {
		.name = "ASRC1 TX6",
		.stream_name = "ASRC1 TX",
		.cpu_dai_name = "TX6",
		.codec_dai_name = "ASRC1-6",
		.cpu_name = "tegra186-asrc",
		.codec_name = "2900800.ahub",
		.params = &default_link_params,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_codec_conf
	tegra186_xbar_codec_conf[TEGRA186_XBAR_CODEC_CONF] = {
	[TEGRA186_CODEC_AMX1_CONF] = {
		.dev_name = "tegra210-amx.0",
		.name_prefix = "AMX1",
	},
	[TEGRA186_CODEC_AMX2_CONF] = {
		.dev_name = "tegra210-amx.1",
		.name_prefix = "AMX2",
	},
	[TEGRA186_CODEC_AMX3_CONF] = {
		.dev_name = "tegra210-amx.2",
		.name_prefix = "AMX3",
	},
	[TEGRA186_CODEC_AMX4_CONF] = {
		.dev_name = "tegra210-amx.3",
		.name_prefix = "AMX4",
	},
	[TEGRA186_CODEC_ADX1_CONF] = {
		.dev_name = "tegra210-adx.0",
		.name_prefix = "ADX1",
	},
	[TEGRA186_CODEC_ADX2_CONF] = {
		.dev_name = "tegra210-adx.1",
		.name_prefix = "ADX2",
	},
	[TEGRA186_CODEC_ADX3_CONF] = {
		.dev_name = "tegra210-adx.2",
		.name_prefix = "ADX3",
	},
	[TEGRA186_CODEC_ADX4_CONF] = {
		.dev_name = "tegra210-adx.3",
		.name_prefix = "ADX4",
	},
	[TEGRA186_CODEC_SFC1_CONF] = {
		.dev_name = "tegra210-sfc.0",
		.name_prefix = "SFC1",
	},
	[TEGRA186_CODEC_SFC2_CONF] = {
		.dev_name = "tegra210-sfc.1",
		.name_prefix = "SFC2",
	},
	[TEGRA186_CODEC_SFC3_CONF] = {
		.dev_name = "tegra210-sfc.2",
		.name_prefix = "SFC3",
	},
	[TEGRA186_CODEC_SFC4_CONF] = {
		.dev_name = "tegra210-sfc.3",
		.name_prefix = "SFC4",
	},
	[TEGRA186_CODEC_MVC1_CONF] = {
		.dev_name = "tegra210-mvc.0",
		.name_prefix = "MVC1",
	},
	[TEGRA186_CODEC_MVC2_CONF] = {
		.dev_name = "tegra210-mvc.1",
		.name_prefix = "MVC2",
	},
	[TEGRA186_CODEC_OPE1_CONF] = {
		.dev_name = "tegra210-ope.0",
		.name_prefix = "OPE1",
	},
	[TEGRA186_CODEC_AFC1_CONF] = {
		.dev_name = "tegra210-afc.0",
		.name_prefix = "AFC1",
	},
	[TEGRA186_CODEC_AFC2_CONF] = {
		.dev_name = "tegra210-afc.1",
		.name_prefix = "AFC2",
	},
	[TEGRA186_CODEC_AFC3_CONF] = {
		.dev_name = "tegra210-afc.2",
		.name_prefix = "AFC3",
	},
	[TEGRA186_CODEC_AFC4_CONF] = {
		.dev_name = "tegra210-afc.3",
		.name_prefix = "AFC4",
	},
	[TEGRA186_CODEC_AFC5_CONF] = {
		.dev_name = "tegra210-afc.4",
		.name_prefix = "AFC5",
	},
	[TEGRA186_CODEC_AFC6_CONF] = {
		.dev_name = "tegra210-afc.5",
		.name_prefix = "AFC6",
	},
	[TEGRA186_CODEC_I2S1_CONF] = {
		.dev_name = "tegra210-i2s.0",
		.name_prefix = "I2S1",
	},
	[TEGRA186_CODEC_I2S2_CONF] = {
		.dev_name = "tegra210-i2s.1",
		.name_prefix = "I2S2",
	},
	[TEGRA186_CODEC_I2S3_CONF] = {
		.dev_name = "tegra210-i2s.2",
		.name_prefix = "I2S3",
	},
	[TEGRA186_CODEC_I2S4_CONF] = {
		.dev_name = "tegra210-i2s.3",
		.name_prefix = "I2S4",
	},
	[TEGRA186_CODEC_I2S5_CONF] = {
		.dev_name = "tegra210-i2s.4",
		.name_prefix = "I2S5",
	},
	[TEGRA186_CODEC_I2S6_CONF] = {
		.dev_name = "tegra210-i2s.5",
		.name_prefix = "I2S6",
	},
	[TEGRA186_CODEC_DMIC1_CONF] = {
		.dev_name = "tegra210-dmic.0",
		.name_prefix = "DMIC1",
	},
	[TEGRA186_CODEC_DMIC2_CONF] = {
		.dev_name = "tegra210-dmic.1",
		.name_prefix = "DMIC2",
	},
	[TEGRA186_CODEC_DMIC3_CONF] = {
		.dev_name = "tegra210-dmic.2",
		.name_prefix = "DMIC3",
	},
	[TEGRA186_CODEC_DMIC4_CONF] = {
		.dev_name = "tegra210-dmic.3",
		.name_prefix = "DMIC4",
	},
	[TEGRA186_CODEC_SPDIF_CONF] = {
		.dev_name = "tegra210-spdif",
		.name_prefix = "SPDIF",
	},
	[TEGRA186_CODEC_DSPK1_CONF] = {
		.dev_name = "tegra186-dspk.0",
		.name_prefix = "DSPK1",
	},
	[TEGRA186_CODEC_DSPK2_CONF] = {
		.dev_name = "tegra186-dspk.1",
		.name_prefix = "DSPK2",
	},
	[TEGRA186_CODEC_ASRC1_CONF] = {
		.dev_name = "tegra186-asrc",
		.name_prefix = "ASRC1",
	},
};

struct snd_soc_dai_link *tegra_machine_get_dai_link_t18x(void)
{
	struct snd_soc_dai_link *link = tegra186_xbar_dai_links;
	unsigned int size = TEGRA186_XBAR_DAI_LINKS;
	struct snd_soc_dai_link *tegra_asoc_machine_links =
		tegra_machine_get_machine_links();

	if (tegra_asoc_machine_links)
		return tegra_asoc_machine_links;

	tegra_machine_set_num_dai_links(size);

	tegra_asoc_machine_links = kzalloc(size *
		sizeof(struct snd_soc_dai_link), GFP_KERNEL);

	memcpy(tegra_asoc_machine_links, link,
		size * sizeof(struct snd_soc_dai_link));

	tegra_machine_set_machine_links(tegra_asoc_machine_links);

	return tegra_asoc_machine_links;
}
EXPORT_SYMBOL_GPL(tegra_machine_get_dai_link_t18x);

int tegra_machine_append_dai_link_t18x(struct snd_soc_dai_link *link,
		unsigned int link_size)
{
	unsigned int size1 = tegra_machine_get_num_dai_links();
	unsigned int size2 = link_size;
	struct snd_soc_dai_link *tegra_asoc_machine_links =
		tegra_machine_get_machine_links();

	if (!tegra_asoc_machine_links) {
		if (link) {
			tegra_machine_set_machine_links(link);
			tegra_machine_set_num_dai_links(size2);
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
			tegra_machine_set_machine_links(
				tegra_asoc_machine_links);
			memcpy(&tegra_asoc_machine_links[size1], link,
				size2 * sizeof(struct snd_soc_dai_link));
			tegra_machine_set_num_dai_links(size1+size2);
			return size1+size2;
		} else {
			tegra_machine_set_num_dai_links(size1);
			return size1;
		}
	}
}
EXPORT_SYMBOL_GPL(tegra_machine_append_dai_link_t18x);

struct snd_soc_codec_conf *tegra_machine_get_codec_conf_t18x(void)
{
	struct snd_soc_codec_conf *conf = tegra186_xbar_codec_conf;
	struct snd_soc_codec_conf *tegra_asoc_codec_conf =
		tegra_machine_get_machine_codec_conf();
	unsigned int size = TEGRA186_XBAR_CODEC_CONF;

	if (tegra_asoc_codec_conf)
		return tegra_asoc_codec_conf;

	tegra_asoc_codec_conf = kzalloc(size *
		sizeof(struct snd_soc_codec_conf), GFP_KERNEL);

	memcpy(tegra_asoc_codec_conf, conf,
		size * sizeof(struct snd_soc_codec_conf));

	tegra_machine_set_machine_codec_conf(tegra_asoc_codec_conf);

	return tegra_asoc_codec_conf;
}
EXPORT_SYMBOL_GPL(tegra_machine_get_codec_conf_t18x);

int tegra_machine_append_codec_conf_t18x(struct snd_soc_codec_conf *conf,
		unsigned int conf_size)
{
	unsigned int size1 = TEGRA186_XBAR_CODEC_CONF;
	unsigned int size2 = conf_size;
	struct snd_soc_codec_conf *tegra_asoc_codec_conf =
		tegra_machine_get_machine_codec_conf();

	if (!tegra_asoc_codec_conf) {
		if (conf) {
			tegra_machine_set_machine_codec_conf(conf);
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
			tegra_machine_set_machine_codec_conf(
				tegra_asoc_codec_conf);
			memcpy(&tegra_asoc_codec_conf[size1], conf,
				size2 * sizeof(struct snd_soc_codec_conf));
			return size1+size2;
		} else
			return size1;
	}
}
EXPORT_SYMBOL_GPL(tegra_machine_append_codec_conf_t18x);

unsigned int tegra_machine_get_codec_dai_link_idx_t18x(const char *codec_name)
{
	unsigned int idx = TEGRA186_XBAR_DAI_LINKS;
	struct snd_soc_dai_link *tegra_asoc_machine_links =
		tegra_machine_get_machine_links();

	if (tegra_machine_get_num_dai_links() <= idx)
		goto err;

	while (idx < tegra_machine_get_num_dai_links()) {
		if (tegra_asoc_machine_links[idx].name)
			if (!strcmp(tegra_asoc_machine_links[idx].name,
				codec_name))
				return idx;
		idx++;
	}

err:
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(tegra_machine_get_codec_dai_link_idx_t18x);

unsigned int tegra_machine_get_bclk_ratio_t18x(
	struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai_link *codec_dai_link = rtd->dai_link;
	char *codec_name = (char *)codec_dai_link->name;
	unsigned int idx =
		tegra_machine_get_codec_dai_link_idx_t18x(codec_name);
	unsigned int *bclk_ratio =
		tegra_machine_get_bclk_ratio_array();

	if (idx == -EINVAL)
		goto err;

	if (!bclk_ratio)
		goto err;

	idx = idx - TEGRA186_XBAR_DAI_LINKS;

	return bclk_ratio[idx];

err:
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(tegra_machine_get_bclk_ratio_t18x);

unsigned int tegra_machine_get_rx_mask_t18x(
	struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai_link *codec_dai_link = rtd->dai_link;
	char *codec_name = (char *)codec_dai_link->name;
	unsigned int idx =
		tegra_machine_get_codec_dai_link_idx_t18x(codec_name);
	unsigned int *rx_mask =
		tegra_machine_get_rx_mask_array();
	if (idx == -EINVAL)
		goto err;

	if (!rx_mask)
		goto err;

	idx = idx - TEGRA186_XBAR_DAI_LINKS;

	return rx_mask[idx];

err:
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(tegra_machine_get_rx_mask_t18x);

unsigned int tegra_machine_get_tx_mask_t18x(
	struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai_link *codec_dai_link = rtd->dai_link;
	char *codec_name = (char *)codec_dai_link->name;
	unsigned int idx =
		tegra_machine_get_codec_dai_link_idx_t18x(codec_name);
	unsigned int *tx_mask =
		tegra_machine_get_tx_mask_array();

	if (idx == -EINVAL)
		goto err;

	if (!tx_mask)
		goto err;

	idx = idx - TEGRA186_XBAR_DAI_LINKS;

	return tx_mask[idx];

err:
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(tegra_machine_get_tx_mask_t18x);

MODULE_LICENSE("GPL v2");
