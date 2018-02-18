/*
 * tegra_p1889ref.c - Tegra P1889 Machine driver
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>

#include <sound/core.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include <mach/tegra_vcm30t124_pdata.h>

#include "tegra_asoc_utils_alt.h"
#include "tegra_asoc_machine_alt.h"
#include "tegra_asoc_hwdep_alt.h"

#define DRV_NAME "tegra-snd-p1889ref"

static struct snd_soc_dai_link *tegra_machine_dai_links;
static struct snd_soc_dai_link *tegra_p1889ref_codec_links;
static struct snd_soc_codec_conf *tegra_machine_codec_conf;
static struct snd_soc_codec_conf *tegra_p1889ref_codec_conf;
static unsigned int num_codec_links;

struct tegra_p1889ref {
	struct tegra_asoc_audio_clock_info audio_clock;
	struct tegra_vcm30t124_platform_data *pdata;
};

static unsigned int tegra_p1889ref_get_dai_link_idx(const char *codec_name)
{
	unsigned int idx = TEGRA124_XBAR_DAI_LINKS, i;

	for (i = 0; i < num_codec_links; i++) {
		if (tegra_machine_dai_links[idx + i].name)
			if (!strcmp(tegra_machine_dai_links[idx + i].name,
				codec_name)) {
				return idx + i;
			}
	}
	return idx;
}

static int tegra_p1889ref_audio_dsp_tdm1_hw_params(
					struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = codec_dai->codec;
	struct snd_soc_card *card = codec->card;
	unsigned int idx =
		tegra_p1889ref_get_dai_link_idx("p1889-audio-dsp-tdm1");
	struct snd_soc_pcm_stream *dai_params =
		(struct snd_soc_pcm_stream *)card->rtd[idx].dai_link->params;
	/* update dai params rate for audio dsp TDM link */
	dai_params->rate_min = params_rate(params);

	return 0;
}
static int tegra_p1889ref_audio_dsp_tdm2_hw_params(
					struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = codec_dai->codec;
	struct snd_soc_card *card = codec->card;
	unsigned int idx =
		tegra_p1889ref_get_dai_link_idx("p1889-audio-dsp-tdm2");
	struct snd_soc_pcm_stream *dai_params =
		(struct snd_soc_pcm_stream *)card->rtd[idx].dai_link->params;
	/* update dai params rate for audio dsp TDM link */
	dai_params->rate_min = params_rate(params);

	return 0;
}

static int tegra_p1889ref_spdif_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	/* dummy hw_params; clocks set in the init function */
	return 0;
}

static struct snd_soc_ops tegra_p1889ref_audio_dsp_tdm1_ops = {
	.hw_params = tegra_p1889ref_audio_dsp_tdm1_hw_params,
};

static struct snd_soc_ops tegra_p1889ref_audio_dsp_tdm2_ops = {
	.hw_params = tegra_p1889ref_audio_dsp_tdm2_hw_params,
};

static struct snd_soc_ops tegra_p1889ref_spdif_ops = {
	.hw_params = tegra_p1889ref_spdif_hw_params,
};

static const struct snd_soc_dapm_widget tegra_p1889ref_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone-x", NULL),
	SND_SOC_DAPM_HP("Headphone-y", NULL),
	SND_SOC_DAPM_HP("Headphone-z", NULL),
	SND_SOC_DAPM_LINE("LineIn-x", NULL),
	SND_SOC_DAPM_LINE("LineIn-y", NULL),
	SND_SOC_DAPM_LINE("LineIn-z", NULL),
};

static int tegra_p1889ref_i2s_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_codec *codec = codec_dai->codec;
	struct snd_soc_card *card = codec->card;
	struct tegra_p1889ref *machine = snd_soc_card_get_drvdata(card);
	struct tegra_vcm30t124_platform_data *pdata = machine->pdata;
	struct snd_soc_pcm_stream *dai_params =
		(struct snd_soc_pcm_stream *)rtd->dai_link->params;
	const char *identifier = (const char *)rtd->dai_link->name;
	unsigned int idx = tegra_p1889ref_get_dai_link_idx(identifier);
	unsigned int fmt = rtd->dai_link->dai_fmt;
	unsigned int mclk, clk_out_rate, srate;
	int err = 0;

	/* Default sampling rate*/
	srate = dai_params->rate_min;
	clk_out_rate = srate * 256;
	mclk = clk_out_rate * 2;

	err = tegra_alt_asoc_utils_set_rate(&machine->audio_clock,
					srate, mclk, clk_out_rate);
	if (err < 0) {
		dev_err(card->dev, "Can't configure clocks\n");
		return err;
	}

	/* set sys clk */
	if (cpu_dai->driver->ops->set_sysclk) {
		err = snd_soc_dai_set_sysclk(cpu_dai, 0, srate,
						SND_SOC_CLOCK_OUT);
		err = snd_soc_dai_set_sysclk(cpu_dai, 0, srate,
						SND_SOC_CLOCK_IN);
		if (err < 0) {
			dev_err(card->dev, "%s cpu DAI srate not set\n",
				cpu_dai->name);
			return err;
		}
	}

	/* set bclk ratio */
	if (cpu_dai->driver->ops->set_bclk_ratio) {
		idx = idx - TEGRA124_XBAR_DAI_LINKS;
		err = snd_soc_dai_set_bclk_ratio(cpu_dai,
				pdata->dai_config[idx].bclk_ratio);
		if (err < 0) {
			dev_err(card->dev, "%s cpu DAI bclk not set\n",
				cpu_dai->name);
			return err;
		}
	}

	/* set tdm slot mask */
	if (cpu_dai->driver->ops->set_tdm_slot) {
		fmt = fmt & SND_SOC_DAIFMT_FORMAT_MASK;
		if ((fmt == SND_SOC_DAIFMT_DSP_A) ||
			(fmt == SND_SOC_DAIFMT_DSP_B)) {
			err = snd_soc_dai_set_tdm_slot(cpu_dai,
					pdata->dai_config[idx].tx_mask,
					pdata->dai_config[idx].rx_mask,
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

static int tegra_p1889ref_amx0_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *amx_dai = rtd->cpu_dai;
	struct tegra_p1889ref *machine = snd_soc_card_get_drvdata(rtd->card);
	unsigned int default_slot[32], i, j;
	unsigned int slot_size = 32;
	unsigned int *tx_slot;

	/* AMX0 slot map may be initialized through platform data */
	if (machine->pdata->num_amx) {
		tx_slot = machine->pdata->amx_config[0].slot_map;
		slot_size = machine->pdata->amx_config[0].slot_size;
	} else {
		for (i = 0, j = 0; i < slot_size; i += 8) {
			default_slot[i] = 0;
			default_slot[i + 1] = 0;
			default_slot[i + 2] = (j << 16) | (1 << 8) | 0;
			default_slot[i + 3] = (j << 16) | (1 << 8) | 1;
			default_slot[i + 4] = 0;
			default_slot[i + 5] = 0;
			default_slot[i + 6] = (j << 16) | (2 << 8) | 0;
			default_slot[i + 7] = (j << 16) | (2 << 8) | 1;
			j++;
		}
		tx_slot = default_slot;
	}

	if (amx_dai->driver->ops->set_channel_map)
		amx_dai->driver->ops->set_channel_map(amx_dai,
					slot_size, tx_slot, 0, 0);

	return 0;
}

static int tegra_p1889ref_amx1_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *amx_dai = rtd->cpu_dai;
	struct tegra_p1889ref *machine = snd_soc_card_get_drvdata(rtd->card);
	unsigned int default_slot[32], i, j;
	unsigned int slot_size = 32;
	unsigned int *tx_slot;

	/* AMX1 slot map may be initialized through platform data */
	if (machine->pdata->num_amx > 1) {
		tx_slot = machine->pdata->amx_config[1].slot_map;
		slot_size = machine->pdata->amx_config[1].slot_size;
	} else {
		for (i = 0, j = 0; i < slot_size; i += 8) {
			default_slot[i] = 0;
			default_slot[i + 1] = 0;
			default_slot[i + 2] = (j << 16) | (1 << 8) | 0;
			default_slot[i + 3] = (j << 16) | (1 << 8) | 1;
			default_slot[i + 4] = 0;
			default_slot[i + 5] = 0;
			default_slot[i + 6] = (j << 16) | (2 << 8) | 0;
			default_slot[i + 7] = (j << 16) | (2 << 8) | 1;
			j++;
		}
		tx_slot = default_slot;
	}

	if (amx_dai->driver->ops->set_channel_map)
		amx_dai->driver->ops->set_channel_map(amx_dai,
					slot_size, tx_slot, 0, 0);

	return 0;
}

static int tegra_p1889ref_adx0_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *adx_dai = rtd->codec_dai;
	struct tegra_p1889ref *machine = snd_soc_card_get_drvdata(rtd->card);
	unsigned int default_slot[32], i, j;
	unsigned int slot_size = 32;
	unsigned int *rx_slot;

	/* ADX0 slot map may be initialized through platform data */
	if (machine->pdata->num_adx) {
		rx_slot = machine->pdata->adx_config[0].slot_map;
		slot_size = machine->pdata->adx_config[0].slot_size;
	} else {
		for (i = 0, j = 0; i < slot_size; i += 8) {
			default_slot[i] = 0;
			default_slot[i + 1] = 0;
			default_slot[i + 2] = (j << 16) | (1 << 8) | 0;
			default_slot[i + 3] = (j << 16) | (1 << 8) | 1;
			default_slot[i + 4] = 0;
			default_slot[i + 5] = 0;
			default_slot[i + 6] = (j << 16) | (2 << 8) | 0;
			default_slot[i + 7] = (j << 16) | (2 << 8) | 1;
			j++;
		}
		rx_slot = default_slot;
	}

	if (adx_dai->driver->ops->set_channel_map)
		adx_dai->driver->ops->set_channel_map(adx_dai,
					0, 0, slot_size, rx_slot);

	return 0;
}

static int tegra_p1889ref_adx1_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *adx_dai = rtd->codec_dai;
	struct tegra_p1889ref *machine = snd_soc_card_get_drvdata(rtd->card);
	unsigned int default_slot[32], i, j;
	unsigned int slot_size = 32;
	unsigned int *rx_slot;

	/* ADX1 slot map may be initialized through platform data */
	if (machine->pdata->num_adx > 1) {
		rx_slot = machine->pdata->adx_config[1].slot_map;
		slot_size = machine->pdata->adx_config[1].slot_size;
	} else {
		for (i = 0, j = 0; i < slot_size; i += 8) {
			default_slot[i] = 0;
			default_slot[i + 1] = 0;
			default_slot[i + 2] = (j << 16) | (1 << 8) | 0;
			default_slot[i + 3] = (j << 16) | (1 << 8) | 1;
			default_slot[i + 4] = 0;
			default_slot[i + 5] = 0;
			default_slot[i + 6] = (j << 16) | (2 << 8) | 0;
			default_slot[i + 7] = (j << 16) | (2 << 8) | 1;
			j++;
		}
		rx_slot = default_slot;
	}

	if (adx_dai->driver->ops->set_channel_map)
		adx_dai->driver->ops->set_channel_map(adx_dai,
					0, 0, slot_size, rx_slot);

	return 0;
}

static int tegra_p1889ref_dam0_init(struct snd_soc_pcm_runtime *rtd)
{
	struct tegra_p1889ref *machine = snd_soc_card_get_drvdata(rtd->card);
	unsigned int in_srate = 8000;
	int err;

	/* DAM input rate may be initialized through platform data */
	if (machine->pdata->num_dam)
		in_srate = machine->pdata->dam_in_srate[0];

	err = snd_soc_dai_set_sysclk(rtd->codec_dai, 0, in_srate,
					SND_SOC_CLOCK_IN);

	return 0;
}

static int tegra_p1889ref_dam1_init(struct snd_soc_pcm_runtime *rtd)
{
	struct tegra_p1889ref *machine = snd_soc_card_get_drvdata(rtd->card);
	unsigned int in_srate = 8000;
	int err;

	/* DAM input rate may be initialized through platform data */
	if (machine->pdata->num_dam > 1)
		in_srate = machine->pdata->dam_in_srate[1];

	err = snd_soc_dai_set_sysclk(rtd->codec_dai, 0, in_srate,
					SND_SOC_CLOCK_IN);

	return 0;
}

static int tegra_p1889ref_dam2_init(struct snd_soc_pcm_runtime *rtd)
{
	struct tegra_p1889ref *machine = snd_soc_card_get_drvdata(rtd->card);
	unsigned int in_srate = 8000;
	int err;

	/* DAM input rate may be initialized through platform data */
	if (machine->pdata->num_dam > 2)
		in_srate = machine->pdata->dam_in_srate[2];

	err = snd_soc_dai_set_sysclk(rtd->codec_dai, 0, in_srate,
					SND_SOC_CLOCK_IN);

	return 0;
}

static void tegra_p1889ref_new_codec_links(
		struct tegra_vcm30t124_platform_data *pdata)
{
	int i, j;

	if (tegra_p1889ref_codec_links)
		return;

	num_codec_links = pdata->dev_num;

	tegra_p1889ref_codec_links = kzalloc(2 * num_codec_links *
		sizeof(struct snd_soc_dai_link), GFP_KERNEL);

	for (i = 0, j = num_codec_links; i < num_codec_links; i++, j++) {
		/* initialize DAI links on DAP side */
		tegra_p1889ref_codec_links[i].name =
			pdata->dai_config[i].link_name;
		tegra_p1889ref_codec_links[i].stream_name = "Playback";
		tegra_p1889ref_codec_links[i].cpu_dai_name = "DAP";
		tegra_p1889ref_codec_links[i].codec_dai_name =
			pdata->dai_config[i].codec_dai_name;
		tegra_p1889ref_codec_links[i].cpu_name =
			pdata->dai_config[i].cpu_name;
		tegra_p1889ref_codec_links[i].codec_name =
			pdata->dai_config[i].codec_name;
		tegra_p1889ref_codec_links[i].params =
			&pdata->dai_config[i].params;
		tegra_p1889ref_codec_links[i].dai_fmt =
			pdata->dai_config[i].dai_fmt;
		tegra_p1889ref_codec_links[i].init =
			tegra_p1889ref_i2s_dai_init;

		/* initialize DAI links on CIF side */
		tegra_p1889ref_codec_links[j].name =
			pdata->dai_config[i].cpu_dai_name;
		tegra_p1889ref_codec_links[j].stream_name =
			pdata->dai_config[i].cpu_dai_name;
		tegra_p1889ref_codec_links[j].cpu_dai_name =
			pdata->dai_config[i].cpu_dai_name;
		tegra_p1889ref_codec_links[j].codec_dai_name = "CIF";
		tegra_p1889ref_codec_links[j].cpu_name = "tegra30-ahub-xbar";
		tegra_p1889ref_codec_links[j].codec_name =
			pdata->dai_config[i].cpu_name;
		tegra_p1889ref_codec_links[j].params =
			&pdata->dai_config[i].params;
	}
}

static void tegra_p1889ref_free_codec_links(void)
{
	kfree(tegra_p1889ref_codec_links);
	tegra_p1889ref_codec_links = NULL;
}

static void tegra_p1889ref_new_codec_conf(
		struct tegra_vcm30t124_platform_data *pdata)
{
	int i;

	if (tegra_p1889ref_codec_conf)
		return;

	num_codec_links = pdata->dev_num;

	tegra_p1889ref_codec_conf = kzalloc(num_codec_links *
		sizeof(struct snd_soc_codec_conf), GFP_KERNEL);

	for (i = 0; i < num_codec_links; i++) {
		tegra_p1889ref_codec_conf[i].dev_name =
			pdata->dai_config[i].codec_name;
		tegra_p1889ref_codec_conf[i].name_prefix =
			pdata->dai_config[i].codec_prefix;
	}
}

static void tegra_p1889ref_free_codec_conf(void)
{
	kfree(tegra_p1889ref_codec_conf);
	tegra_p1889ref_codec_conf = NULL;
}

static int tegra_p1889ref_suspend_pre(struct snd_soc_card *card)
{
	unsigned int idx;

	/* DAPM dai link stream work for non pcm links */
	for (idx = 0; idx < card->num_rtd; idx++) {
		if (card->rtd[idx].dai_link->params)
			INIT_DELAYED_WORK(&card->rtd[idx].delayed_work, NULL);
	}
	return 0;
}

static struct snd_soc_card snd_soc_tegra_p1889ref = {
	.name = "dirana3",
	.owner = THIS_MODULE,
	.suspend_pre = tegra_p1889ref_suspend_pre,
	.dapm_widgets = tegra_p1889ref_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tegra_p1889ref_dapm_widgets),
	.fully_routed = true,
};

static int tegra_p1889ref_driver_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_tegra_p1889ref;
	struct tegra_p1889ref *machine;
	int ret = 0, i, j;

	machine = devm_kzalloc(&pdev->dev, sizeof(struct tegra_p1889ref),
			       GFP_KERNEL);
	if (!machine) {
		dev_err(&pdev->dev, "Can't allocate tegra_p1889ref struct\n");
		ret = -ENOMEM;
		goto err;
	}
	machine->pdata = pdev->dev.platform_data;

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, machine);

	/* get the xbar dai link/codec conf structure */
	tegra_machine_dai_links = tegra_machine_get_dai_link();
	if (!tegra_machine_dai_links)
		goto err_alloc_dai_link;
	tegra_machine_codec_conf = tegra_machine_get_codec_conf();
	if (!tegra_machine_codec_conf)
		goto err_alloc_dai_link;

	/* set AMX/ADX dai_init */
	tegra_machine_set_dai_init(TEGRA124_DAI_LINK_AMX0,
		&tegra_p1889ref_amx0_init);
	tegra_machine_set_dai_init(TEGRA124_DAI_LINK_AMX1,
		&tegra_p1889ref_amx1_init);
	tegra_machine_set_dai_init(TEGRA124_DAI_LINK_ADX0,
		&tegra_p1889ref_adx0_init);
	tegra_machine_set_dai_init(TEGRA124_DAI_LINK_ADX1,
		&tegra_p1889ref_adx1_init);

    /* set AMX/ADX params */
	if (machine->pdata->num_amx) {
		switch (machine->pdata->num_amx) {
		case 2:
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_AMX1_0,
				&machine->pdata->amx_config[1].in_params[0]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_AMX1_1,
				&machine->pdata->amx_config[1].in_params[1]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_AMX1_2,
				&machine->pdata->amx_config[1].in_params[2]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_AMX1_3,
				&machine->pdata->amx_config[1].in_params[3]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_AMX1,
				&machine->pdata->amx_config[1].out_params[0]);
		case 1:
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_AMX0_0,
				&machine->pdata->amx_config[0].in_params[0]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_AMX0_1,
				&machine->pdata->amx_config[0].in_params[1]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_AMX0_2,
				&machine->pdata->amx_config[0].in_params[2]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_AMX0_3,
				&machine->pdata->amx_config[0].in_params[3]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_AMX0,
				&machine->pdata->amx_config[0].out_params[0]);
			break;
		default:
			break;
		}
	}

	if (machine->pdata->num_adx) {
		switch (machine->pdata->num_adx) {
		case 2:
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_ADX1_0,
				&machine->pdata->adx_config[1].out_params[0]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_ADX1_1,
				&machine->pdata->adx_config[1].out_params[1]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_ADX1_2,
				&machine->pdata->adx_config[1].out_params[2]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_ADX1_3,
				&machine->pdata->adx_config[1].out_params[3]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_ADX1,
				&machine->pdata->adx_config[1].in_params[0]);
		case 1:
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_ADX0_0,
				&machine->pdata->adx_config[0].out_params[0]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_ADX0_1,
				&machine->pdata->adx_config[0].out_params[1]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_ADX0_2,
				&machine->pdata->adx_config[0].out_params[2]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_ADX0_3,
				&machine->pdata->adx_config[0].out_params[3]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_ADX0,
				&machine->pdata->adx_config[0].in_params[0]);
			break;
		default:
			break;
		}
	}

	/* set DAM dai_init */
	tegra_machine_set_dai_init(TEGRA124_DAI_LINK_DAM0_0,
		&tegra_p1889ref_dam0_init);
	tegra_machine_set_dai_init(TEGRA124_DAI_LINK_DAM1_0,
		&tegra_p1889ref_dam1_init);
	tegra_machine_set_dai_init(TEGRA124_DAI_LINK_DAM2_0,
		&tegra_p1889ref_dam2_init);

	/* get on board codec dai_links/conf */
	tegra_p1889ref_new_codec_links(machine->pdata);
	tegra_p1889ref_new_codec_conf(machine->pdata);

	/* set APBIF dai_ops */
	for (i = TEGRA124_DAI_LINK_APBIF0; i <= TEGRA124_DAI_LINK_APBIF9; i++)
		tegra_machine_set_dai_ops(i, &tegra_p1889ref_spdif_ops);

	for (i = 0; i < num_codec_links; i++) {
		if (machine->pdata->dai_config[i].link_name) {
			if (!strcmp(machine->pdata->dai_config[i].link_name,
				"p1889-audio-dsp-tdm1")) {
				for (j = TEGRA124_DAI_LINK_APBIF0;
					j <= TEGRA124_DAI_LINK_APBIF3; j++)
					tegra_machine_set_dai_ops(j,
					&tegra_p1889ref_audio_dsp_tdm1_ops);
			} else if (!strcmp(
				machine->pdata->dai_config[i].link_name,
				"p1889-audio-dsp-tdm2")) {
				for (j = TEGRA124_DAI_LINK_APBIF4;
					j <= TEGRA124_DAI_LINK_APBIF7; j++)
					tegra_machine_set_dai_ops(j,
					&tegra_p1889ref_audio_dsp_tdm2_ops);
			}
		}
	}

	/* append p1889ref specific dai_links */
	card->num_links =
		tegra_machine_append_dai_link(tegra_p1889ref_codec_links,
			2 * num_codec_links);
	tegra_machine_dai_links = tegra_machine_get_dai_link();
	card->dai_link = tegra_machine_dai_links;

	/* append p1889ref specific codec_conf */
	card->num_configs =
		tegra_machine_append_codec_conf(tegra_p1889ref_codec_conf,
			num_codec_links);
	tegra_machine_codec_conf = tegra_machine_get_codec_conf();
	card->codec_conf = tegra_machine_codec_conf;

	/* remove p1889ref specific dai links and conf */
	tegra_p1889ref_free_codec_links();
	tegra_p1889ref_free_codec_conf();

	card->dapm_routes = machine->pdata->dapm_routes;
	card->num_dapm_routes = machine->pdata->num_dapm_routes;

	if (machine->pdata->card_name)
		card->name = machine->pdata->card_name;

	ret = tegra_alt_asoc_utils_init(&machine->audio_clock,
					&pdev->dev,
					card);
	if (ret)
		goto err_alloc_dai_link;

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err_fini_utils;
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
err_fini_utils:
	tegra_alt_asoc_utils_fini(&machine->audio_clock);
err_alloc_dai_link:
	tegra_machine_remove_dai_link();
	tegra_machine_remove_codec_conf();
err:
	return ret;
}

static int tegra_p1889ref_driver_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct tegra_p1889ref *machine = snd_soc_card_get_drvdata(card);

	snd_soc_unregister_card(card);

	tegra_machine_remove_dai_link();
	tegra_machine_remove_codec_conf();

	tegra_alt_asoc_utils_fini(&machine->audio_clock);

	return 0;
}

static const struct of_device_id tegra_p1889ref_of_match[] = {
	{ .compatible = "nvidia,tegra-audio-p1889ref", },
	{},
};

static struct platform_driver tegra_p1889ref_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = tegra_p1889ref_of_match,
	},
	.probe = tegra_p1889ref_driver_probe,
	.remove = tegra_p1889ref_driver_remove,
};
module_platform_driver(tegra_p1889ref_driver);

MODULE_AUTHOR("Arun S L <aruns@nvidia.com>");
MODULE_DESCRIPTION("Tegra P1889 ASoC Machine Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, tegra_p1889ref_of_match);
