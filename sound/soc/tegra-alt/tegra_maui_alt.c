/*
 * tegra_maui_alt.c - Tegra maui Machine driver
 *
 * Copyright (c) 2013-2015 NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/tegra-pmc.h>
#include <linux/pm_runtime.h>

#include <sound/core.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <asm-generic/gpio.h>
#include <sound/soc.h>
#include "../codecs/ad193x.h"

#include "tegra_asoc_utils_alt.h"
#include "tegra_asoc_machine_alt.h"
#include "tegra_asoc_hwdep_alt.h"

#define DRV_NAME "tegra-snd-maui"

#define MAX_AMX_SLOT_SIZE 64
#define MAX_ADX_SLOT_SIZE 64
#define DEFAULT_AMX_SLOT_SIZE 32
#define DEFAULT_ADX_SLOT_SIZE 32
#define MAX_AMX_NUM 2
#define MAX_ADX_NUM 2

struct tegra_maui_amx_adx_conf {
	unsigned int num_amx;
	unsigned int num_adx;
	unsigned int amx_slot_size[MAX_AMX_NUM];
	unsigned int adx_slot_size[MAX_ADX_NUM];
};

struct tegra_maui {
	struct tegra_asoc_audio_clock_info audio_clock;
	struct tegra_maui_amx_adx_conf amx_adx_conf;
	int rate_via_kcontrol;
	unsigned int num_codec_links;
};

static struct snd_soc_pcm_stream tegra_maui_amx_input_params[] = {
	[0] = {
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.rate_min = 48000,
		.rate_max = 48000,
		.channels_min = 2,
		.channels_max = 2,
	},
	[1] = {
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.rate_min = 48000,
		.rate_max = 48000,
		.channels_min = 2,
		.channels_max = 2,
	},
	[2] = {
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.rate_min = 48000,
		.rate_max = 48000,
		.channels_min = 2,
		.channels_max = 2,
	},
	[3] = {
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.rate_min = 48000,
		.rate_max = 48000,
		.channels_min = 2,
		.channels_max = 2,
	},
};

static struct snd_soc_pcm_stream tegra_maui_amx_output_params[] = {
	[0] = {
		.formats = SNDRV_PCM_FMTBIT_S32_LE,
		.rate_min = 48000,
		.rate_max = 48000,
		.channels_min = 8,
		.channels_max = 8,
	},
};

static struct snd_soc_pcm_stream tegra_maui_adx_output_params[] = {
	[0] = {
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.rate_min = 48000,
		.rate_max = 48000,
		.channels_min = 2,
		.channels_max = 2,
	},
	[1] = {
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.rate_min = 48000,
		.rate_max = 48000,
		.channels_min = 2,
		.channels_max = 2,
	},
	[2] = {
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.rate_min = 48000,
		.rate_max = 48000,
		.channels_min = 2,
		.channels_max = 2,
	},
	[3] = {
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.rate_min = 48000,
		.rate_max = 48000,
		.channels_min = 2,
		.channels_max = 2,
	},
};

static struct snd_soc_pcm_stream tegra_maui_adx_input_params[] = {
	[0] = {
		.formats = SNDRV_PCM_FMTBIT_S32_LE,
		.rate_min = 48000,
		.rate_max = 48000,
		.channels_min = 8,
		.channels_max = 8,
	},
};

static int tegra_maui_spdif_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_card *card = rtd->card;
	struct tegra_maui *machine = snd_soc_card_get_drvdata(card);
	struct snd_soc_pcm_stream *dai_params =
		(struct snd_soc_pcm_stream *)rtd->dai_link->params;
	struct device_node *np =
		(struct device_node *)rtd->dai_link->cpu_of_node;
	struct device_node *parentnp = np->parent;
	const char *identifier = (const char *)rtd->dai_link->name;
	unsigned int idx = tegra_machine_get_codec_dai_link_idx(identifier);
	unsigned int fmt = rtd->dai_link->dai_fmt;
	unsigned int mclk, clk_out_rate;
	unsigned int tx_mask = (1<<8) - 1, rx_mask = (1<<8) - 1;
	int err = 0;

	/* Default sampling rate*/
	clk_out_rate = dai_params->rate_min * 256;
	mclk = clk_out_rate * 2;

	err = tegra_alt_asoc_utils_set_rate(&machine->audio_clock,
		dai_params->rate_min, mclk, clk_out_rate);
	if (err < 0) {
		dev_err(card->dev, "Can't configure clocks\n");
		return err;
	}

	/* set sys clk */
	if (cpu_dai->driver->ops->set_sysclk) {
		err = snd_soc_dai_set_sysclk(cpu_dai, 0, dai_params->rate_min,
						SND_SOC_CLOCK_OUT);
		err = snd_soc_dai_set_sysclk(cpu_dai, 0, dai_params->rate_min,
						SND_SOC_CLOCK_IN);
		if (err < 0) {
			dev_err(card->dev, "%s cpu DAI rate not set\n",
				cpu_dai->name);
			return err;
		}
	}

	/* set bclk ratio */
	if (cpu_dai->driver->ops->set_bclk_ratio) {
		idx = idx - TEGRA210_XBAR_DAI_LINKS;
		err = snd_soc_dai_set_bclk_ratio(cpu_dai,
			tegra_machine_get_bclk_ratio(rtd));
		if (err < 0) {
			dev_err(card->dev, "%s cpu DAI bclk not set\n",
				cpu_dai->name);
			return err;
		}
	}

	if (parentnp) {
		of_property_read_u32(np, "tx-mask", (u32 *)&tx_mask);
		of_property_read_u32(np, "rx-mask", (u32 *)&rx_mask);
	}

	/* set tdm slot mask */
	if (cpu_dai->driver->ops->set_tdm_slot) {
		fmt = fmt & SND_SOC_DAIFMT_FORMAT_MASK;
		if ((fmt == SND_SOC_DAIFMT_DSP_A) ||
			(fmt == SND_SOC_DAIFMT_DSP_B)) {
			err = snd_soc_dai_set_tdm_slot(cpu_dai,
					tx_mask,
					rx_mask,
					0, 0);
			if (err < 0) {
				dev_err(card->dev,
					"%s cpu DAI slot mask not set\n",
					cpu_dai->name);
			}
		}
	}
	return err;
}


static int tegra_maui_amx_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
	struct tegra_maui *machine = snd_soc_card_get_drvdata(card);
	struct snd_soc_dai *amx_dai = rtd->cpu_dai;
	struct device_node *np = rtd->card->dev->of_node;
	unsigned int tx_slot[MAX_AMX_SLOT_SIZE];
	unsigned int i, j, default_slot_mode = 0;
	unsigned int slot_size = machine->amx_adx_conf.amx_slot_size[0];

	if (machine->amx_adx_conf.num_amx && slot_size) {
		if (of_property_read_u32_array(np,
			"nvidia,amx-slot-map", tx_slot, slot_size))
			default_slot_mode = 1;
	} else
		default_slot_mode = 1;

	if (default_slot_mode) {
		slot_size = DEFAULT_AMX_SLOT_SIZE;
		for (i = 0, j = 0; i < slot_size; i += 8) {
			tx_slot[i] = 0;
			tx_slot[i + 1] = 0;
			tx_slot[i + 2] = (j << 16) | (1 << 8) | 0;
			tx_slot[i + 3] = (j << 16) | (1 << 8) | 1;
			tx_slot[i + 4] = 0;
			tx_slot[i + 5] = 0;
			tx_slot[i + 6] = (j << 16) | (2 << 8) | 0;
			tx_slot[i + 7] = (j << 16) | (2 << 8) | 1;
			j++;
		}
	}

	if (amx_dai->driver->ops->set_channel_map)
		amx_dai->driver->ops->set_channel_map(amx_dai,
			slot_size, tx_slot, 0, 0);

	return 0;
}

static int tegra_maui_adx_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
	struct tegra_maui *machine = snd_soc_card_get_drvdata(card);
	struct snd_soc_dai *adx_dai = rtd->codec_dai;
	struct device_node *np = rtd->card->dev->of_node;
	unsigned int rx_slot[MAX_ADX_SLOT_SIZE];
	unsigned int i, j, default_slot_mode = 0;
	unsigned int slot_size = machine->amx_adx_conf.adx_slot_size[0];

	if (machine->amx_adx_conf.num_adx && slot_size) {
		if (of_property_read_u32_array(np,
			"nvidia,adx-slot-map", rx_slot, slot_size))
			default_slot_mode = 1;
	} else
		default_slot_mode = 1;

	if (default_slot_mode) {
		slot_size = DEFAULT_ADX_SLOT_SIZE;
		for (i = 0, j = 0; i < slot_size; i += 8) {
			rx_slot[i] = 0;
			rx_slot[i + 1] = 0;
			rx_slot[i + 2] = (j << 16) | (1 << 8) | 0;
			rx_slot[i + 3] = (j << 16) | (1 << 8) | 1;
			rx_slot[i + 4] = 0;
			rx_slot[i + 5] = 0;
			rx_slot[i + 6] = (j << 16) | (2 << 8) | 0;
			rx_slot[i + 7] = (j << 16) | (2 << 8) | 1;
			j++;
		}
	}

	if (adx_dai->driver->ops->set_channel_map)
		adx_dai->driver->ops->set_channel_map(adx_dai,
			0, 0, slot_size, rx_slot);

	return 0;
}

static int tegra_maui_sfc1_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	unsigned int in_srate, out_srate;
	int err;

	in_srate = 48000;
	out_srate = 8000;

	err = snd_soc_dai_set_sysclk(codec_dai, 0, out_srate,
					SND_SOC_CLOCK_OUT);
	err = snd_soc_dai_set_sysclk(codec_dai, 0, in_srate,
					SND_SOC_CLOCK_IN);
	return 0;
}

static int tegra_maui_sfc2_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	unsigned int in_srate, out_srate;
	int err;

	in_srate = 8000;
	out_srate = 48000;

	err = snd_soc_dai_set_sysclk(codec_dai, 0, out_srate,
					SND_SOC_CLOCK_OUT);
	err = snd_soc_dai_set_sysclk(codec_dai, 0, in_srate,
					SND_SOC_CLOCK_IN);
	return 0;
}

static int tegra_maui_sfc3_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	unsigned int in_srate, out_srate;
	int err;

	in_srate = 48000;
	out_srate = 8000;

	err = snd_soc_dai_set_sysclk(codec_dai, 0, out_srate,
					SND_SOC_CLOCK_OUT);
	err = snd_soc_dai_set_sysclk(codec_dai, 0, in_srate,
					SND_SOC_CLOCK_IN);
	return 0;
}

static int tegra_maui_sfc4_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	unsigned int in_srate, out_srate;
	int err;

	in_srate = 8000;
	out_srate = 48000;

	err = snd_soc_dai_set_sysclk(codec_dai, 0, out_srate,
					SND_SOC_CLOCK_OUT);
	err = snd_soc_dai_set_sysclk(codec_dai, 0, in_srate,
					SND_SOC_CLOCK_IN);
	return 0;
}

static int tegra_maui_spdif_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	/* dummy hw_params; clocks set in the init function */

	return 0;
}

static struct snd_soc_ops tegra_maui_spdif_ops = {
	.hw_params = tegra_maui_spdif_hw_params,
};

static const struct snd_soc_dapm_widget tegra_maui_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone-y", NULL),
	SND_SOC_DAPM_LINE("LineIn-y", NULL),
};

static const struct snd_soc_dapm_route tegra_maui_audio_map[] = {
};

static int tegra_maui_suspend_pre(struct snd_soc_card *card)
{
	unsigned int idx;

	/* DAPM dai link stream work for non pcm links */
	for (idx = 0; idx < card->num_rtd; idx++) {
		if (card->rtd[idx].dai_link->params)
			INIT_DELAYED_WORK(&card->rtd[idx].delayed_work, NULL);
	}

	return 0;
}

static struct snd_soc_card snd_soc_tegra_maui = {
	.name = "tegra-maui",
	.owner = THIS_MODULE,
	.suspend_pre = tegra_maui_suspend_pre,
	.dapm_widgets = tegra_maui_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tegra_maui_dapm_widgets),
	.fully_routed = true,
};

static int tegra_maui_driver_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct snd_soc_card *card = &snd_soc_tegra_maui;
	struct tegra_maui *machine;
	struct snd_soc_dai_link *tegra_machine_dai_links = NULL;
	struct snd_soc_dai_link *tegra_maui_codec_links = NULL;
	struct snd_soc_codec_conf *tegra_machine_codec_conf = NULL;
	struct snd_soc_codec_conf *tegra_maui_codec_conf = NULL;
	int ret = 0, i;

	machine = devm_kzalloc(&pdev->dev, sizeof(struct tegra_maui),
			       GFP_KERNEL);
	if (!machine) {
		dev_err(&pdev->dev, "Can't allocate tegra_maui struct\n");
		ret = -ENOMEM;
		goto err;
	}

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, machine);

	if (np) {
		ret = snd_soc_of_parse_card_name(card, "nvidia,model");
		if (ret)
			goto err;

		ret = snd_soc_of_parse_audio_routing(card,
					"nvidia,audio-routing");
		if (ret)
			goto err;

		if (of_property_read_u32(np, "nvidia,num-amx",
			(u32 *)&machine->amx_adx_conf.num_amx))
			machine->amx_adx_conf.num_amx = 0;
		if (of_property_read_u32_array(np, "nvidia,amx-slot-size",
			(u32 *)machine->amx_adx_conf.amx_slot_size,
			MAX_AMX_NUM) ||
			!machine->amx_adx_conf.num_amx) {
			for (i = 0; i < MAX_AMX_NUM; i++)
				machine->amx_adx_conf.amx_slot_size[i] = 0;
		}

		if (of_property_read_u32(np, "nvidia,num-adx",
			(u32 *)&machine->amx_adx_conf.num_adx))
			machine->amx_adx_conf.num_adx = 0;
		if (of_property_read_u32_array(np, "nvidia,adx-slot-size",
			(u32 *)machine->amx_adx_conf.adx_slot_size,
			MAX_ADX_NUM) ||
			!machine->amx_adx_conf.num_adx) {
			for (i = 0; i < MAX_ADX_NUM; i++)
				machine->amx_adx_conf.adx_slot_size[i] = 0;
		}
	}

	tegra_maui_codec_links = tegra_machine_new_codec_links(pdev,
		tegra_maui_codec_links,
		&machine->num_codec_links);
	if (!tegra_maui_codec_links)
		goto err_alloc_dai_link;

	tegra_maui_codec_conf = tegra_machine_new_codec_conf(pdev,
		tegra_maui_codec_conf,
		&machine->num_codec_links);
	if (!tegra_maui_codec_conf)
		goto err_alloc_dai_link;

	/* get the xbar dai link/codec conf structure */
	tegra_machine_dai_links = tegra_machine_get_dai_link();
	if (!tegra_machine_dai_links)
		goto err_alloc_dai_link;
	tegra_machine_codec_conf = tegra_machine_get_codec_conf();
	if (!tegra_machine_codec_conf)
		goto err_alloc_dai_link;

	/* set AMX/ADX dai_init */
	tegra_machine_set_dai_init(TEGRA210_DAI_LINK_AMX1,
		&tegra_maui_amx_dai_init);
	tegra_machine_set_dai_init(TEGRA210_DAI_LINK_ADX1,
		&tegra_maui_adx_dai_init);
	tegra_machine_set_dai_init(TEGRA210_DAI_LINK_AMX2,
		&tegra_maui_amx_dai_init);
	tegra_machine_set_dai_init(TEGRA210_DAI_LINK_ADX2,
		&tegra_maui_adx_dai_init);

	/* set codec init */
	for (i = 0; i < machine->num_codec_links; i++) {
		if (tegra_maui_codec_links[i].name) {
			if (strstr(tegra_maui_codec_links[i].name,
				"dif-playback"))
				tegra_maui_codec_links[i].init =
					tegra_maui_spdif_init;
		}
	}

	/* set AMX/ADX params */
	if (machine->amx_adx_conf.num_amx) {
		switch (machine->amx_adx_conf.num_amx) {
		case 2:
			tegra_machine_set_dai_params(TEGRA210_DAI_LINK_AMX2_1,
				(struct snd_soc_pcm_stream *)
				&tegra_maui_amx_input_params[0]);
			tegra_machine_set_dai_params(TEGRA210_DAI_LINK_AMX2_2,
				(struct snd_soc_pcm_stream *)
				&tegra_maui_amx_input_params[1]);
			tegra_machine_set_dai_params(TEGRA210_DAI_LINK_AMX2_3,
				(struct snd_soc_pcm_stream *)
				&tegra_maui_amx_input_params[2]);
			tegra_machine_set_dai_params(TEGRA210_DAI_LINK_AMX2_4,
				(struct snd_soc_pcm_stream *)
				&tegra_maui_amx_input_params[3]);
			tegra_machine_set_dai_params(TEGRA210_DAI_LINK_AMX2,
				(struct snd_soc_pcm_stream *)
				&tegra_maui_amx_output_params[0]);
		case 1:
			tegra_machine_set_dai_params(TEGRA210_DAI_LINK_AMX1_1,
				(struct snd_soc_pcm_stream *)
				&tegra_maui_amx_input_params[0]);
			tegra_machine_set_dai_params(TEGRA210_DAI_LINK_AMX1_2,
				(struct snd_soc_pcm_stream *)
				&tegra_maui_amx_input_params[1]);
			tegra_machine_set_dai_params(TEGRA210_DAI_LINK_AMX1_3,
				(struct snd_soc_pcm_stream *)
				&tegra_maui_amx_input_params[2]);
			tegra_machine_set_dai_params(TEGRA210_DAI_LINK_AMX1_4,
				(struct snd_soc_pcm_stream *)
				&tegra_maui_amx_input_params[3]);
			tegra_machine_set_dai_params(TEGRA210_DAI_LINK_AMX1,
				(struct snd_soc_pcm_stream *)
				&tegra_maui_amx_output_params[0]);
			break;
		default:
			break;
		}
	}

	if (machine->amx_adx_conf.num_adx) {
		switch (machine->amx_adx_conf.num_adx) {
		case 2:
			tegra_machine_set_dai_params(TEGRA210_DAI_LINK_ADX2_1,
				(struct snd_soc_pcm_stream *)
				&tegra_maui_adx_output_params[0]);
			tegra_machine_set_dai_params(TEGRA210_DAI_LINK_ADX2_2,
				(struct snd_soc_pcm_stream *)
				&tegra_maui_adx_output_params[1]);
			tegra_machine_set_dai_params(TEGRA210_DAI_LINK_ADX2_3,
				(struct snd_soc_pcm_stream *)
				&tegra_maui_adx_output_params[2]);
			tegra_machine_set_dai_params(TEGRA210_DAI_LINK_ADX2_4,
				(struct snd_soc_pcm_stream *)
				&tegra_maui_adx_output_params[3]);
			tegra_machine_set_dai_params(TEGRA210_DAI_LINK_ADX2,
				(struct snd_soc_pcm_stream *)
				&tegra_maui_adx_input_params[0]);
		case 1:
			tegra_machine_set_dai_params(TEGRA210_DAI_LINK_ADX1_1,
				(struct snd_soc_pcm_stream *)
				&tegra_maui_adx_output_params[0]);
			tegra_machine_set_dai_params(TEGRA210_DAI_LINK_ADX1_2,
				(struct snd_soc_pcm_stream *)
				&tegra_maui_adx_output_params[1]);
			tegra_machine_set_dai_params(TEGRA210_DAI_LINK_ADX1_3,
				(struct snd_soc_pcm_stream *)
				&tegra_maui_adx_output_params[2]);
			tegra_machine_set_dai_params(TEGRA210_DAI_LINK_ADX1_4,
				(struct snd_soc_pcm_stream *)
				&tegra_maui_adx_output_params[3]);
			tegra_machine_set_dai_params(TEGRA210_DAI_LINK_ADX1,
				(struct snd_soc_pcm_stream *)
				&tegra_maui_adx_input_params[0]);
			break;
		default:
			break;
		}
	}

	/* set sfc dai_init */
	tegra_machine_set_dai_init(TEGRA210_DAI_LINK_SFC1_RX,
		&tegra_maui_sfc1_init);	/* in - 8000Hz, out - 48000Hz */
	tegra_machine_set_dai_init(TEGRA210_DAI_LINK_SFC2_RX,
		&tegra_maui_sfc2_init);	/* in - 48000Hz, out - 8000Hz */
	tegra_machine_set_dai_init(TEGRA210_DAI_LINK_SFC3_RX,
		&tegra_maui_sfc3_init);	/* in - 8000Hz, out - 48000Hz */
	tegra_machine_set_dai_init(TEGRA210_DAI_LINK_SFC4_RX,
		&tegra_maui_sfc4_init);	/* in - 48000Hz, out - 8000Hz */

	/* set ADSP PCM/COMPR */
	for (i = TEGRA210_DAI_LINK_ADSP_PCM1;
		i <= TEGRA210_DAI_LINK_ADSP_COMPR2; i++) {
		tegra_machine_set_dai_ops(i, &tegra_maui_spdif_ops);
	}

	/* set ADMAIF dai_ops */
	for (i = TEGRA210_DAI_LINK_ADMAIF1;
		i <= TEGRA210_DAI_LINK_ADMAIF10; i++)
		tegra_machine_set_dai_ops(i, &tegra_maui_spdif_ops);

	/* append maui specific dai_links */
	card->num_links =
		tegra_machine_append_dai_link(tegra_maui_codec_links,
			2 * machine->num_codec_links);
	tegra_machine_dai_links = tegra_machine_get_dai_link();
	card->dai_link = tegra_machine_dai_links;

	/* append maui specific codec_conf */
	card->num_configs =
		tegra_machine_append_codec_conf(tegra_maui_codec_conf,
			machine->num_codec_links);
	tegra_machine_codec_conf = tegra_machine_get_codec_conf();
	card->codec_conf = tegra_machine_codec_conf;

	ret = tegra_alt_asoc_utils_init(&machine->audio_clock,
					&pdev->dev,
					card);
	if (ret)
		goto err_alloc_dai_link;

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err_register_card;
	}

	ret = tegra_asoc_hwdep_create(card);
	if (ret) {
		dev_err(&pdev->dev, "can't create tegra_machine_hwdep (%d)\n",
			ret);
		goto err_unregister_card;
	}

	return 0;

err_unregister_card:
	snd_soc_unregister_card(card);

err_register_card:
err_alloc_dai_link:
	tegra_machine_remove_dai_link();
	tegra_machine_remove_codec_conf();
err:
	return ret;
}

static int tegra_maui_driver_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);

	tegra_machine_remove_dai_link();
	tegra_machine_remove_codec_conf();

	return 0;
}

static const struct of_device_id tegra_maui_of_match[] = {
	{ .compatible = "nvidia,tegra-audio-maui", },
	{},
};

static struct platform_driver tegra_maui_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = tegra_maui_of_match,
	},
	.probe = tegra_maui_driver_probe,
	.remove = tegra_maui_driver_remove,
};
module_platform_driver(tegra_maui_driver);

MODULE_AUTHOR("Dipesh Gandhi <dipeshg@nvidia.com>");
MODULE_DESCRIPTION("Tegra+maui machine ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, tegra_maui_of_match);
