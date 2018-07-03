/*
 *  cnl_rt274.c - ASOC Machine driver for CNL
 *
 *  Copyright (C) 2016 Intel Corp
 *  Author: Guneshwor Singh <guneshwor.o.singh@intel.com>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/async.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/acpi.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <linux/input.h>

#include "../../codecs/hdac_hdmi.h"
#include "../../codecs/rt274.h"

#define CNL_FREQ_OUT		24000000
#define CNL_BE_FIXUP_RATE	48000
#define RT274_CODEC_DAI		"rt274-aif1"
#define CNL_NAME_SIZE		32
#define CNL_MAX_HDMI		3

static struct snd_soc_jack cnl_hdmi[CNL_MAX_HDMI];

struct cnl_hdmi_pcm {
	struct list_head head;
	struct snd_soc_dai *codec_dai;
	int device;
};

struct cnl_rt274_private {
	struct list_head hdmi_pcm_list;
	int pcm_count;
};

static struct snd_soc_dai *cnl_get_codec_dai(struct snd_soc_card *card,
						     const char *dai_name)
{
	struct snd_soc_pcm_runtime *rtd;

	list_for_each_entry(rtd, &card->rtd_list, list) {
		if (!strcmp(rtd->codec_dai->name, dai_name))
			return rtd->codec_dai;
	}

	return NULL;
}

static int cnl_rt274_clock_control(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int  event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	int ret = 0;
	int ratio = 100;

	struct snd_soc_dai *codec_dai = snd_soc_card_get_codec_dai(card,
							  RT274_CODEC_DAI);
	if (!codec_dai)
		return -EINVAL;

	ret = snd_soc_dai_set_sysclk(codec_dai, RT274_SCLK_S_PLL1,
							CNL_FREQ_OUT,
							SND_SOC_CLOCK_IN);
	if (ret) {
		dev_err(codec_dai->dev,
			"failed to enable PLL1: %d\n", ret);
	}

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		snd_soc_dai_set_bclk_ratio(codec_dai, ratio);

		ret = snd_soc_dai_set_pll(codec_dai, 0, RT274_PLL2_S_BCLK,
					  CNL_BE_FIXUP_RATE * ratio,
					  CNL_FREQ_OUT);
		if (ret) {
			dev_err(codec_dai->dev,
				"failed to enable PLL2: %d\n", ret);
			return ret;
		}
	}

	return ret;
}

static struct snd_soc_jack cnl_headset;

/* Headset jack detection DAPM pins */
static struct snd_soc_jack_pin cnl_headset_pins[] = {
	{
		.pin = "Mic Jack",
		.mask = SND_JACK_MICROPHONE,
	},
	{
		.pin = "Headphone Jack",
		.mask = SND_JACK_HEADPHONE,
	},
};

static const struct snd_kcontrol_new cnl_controls[] = {
	SOC_DAPM_PIN_SWITCH("Headphone Jack"),
	SOC_DAPM_PIN_SWITCH("Mic Jack"),
};

static const struct snd_soc_dapm_widget cnl_rt274_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
	SND_SOC_DAPM_MIC("SoC DMIC", NULL),
	SND_SOC_DAPM_SUPPLY("Platform Clock", SND_SOC_NOPM, 0, 0,
			cnl_rt274_clock_control, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMD),
};

static const struct snd_soc_pcm_stream dai_params_codec = {
	.formats = SNDRV_PCM_FMTBIT_S24_LE,
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
};

static const struct snd_soc_dapm_route cnl_map[] = {
	{"Headphone Jack", NULL, "HPO Pin"},
	{"MIC", NULL, "Mic Jack"},
	{"DMic", NULL, "SoC DMIC"},
	{"DMIC01 Rx", NULL, "Capture"},
	{"dmic01_hifi", NULL, "DMIC01 Rx"},

	/* ssp2 path */
	{"Dummy Playback", NULL, "ssp2 Tx"},
	{"ssp2 Tx", NULL, "ssp2_out"},

	{"ssp2 Rx", NULL, "Dummy Capture"},
	{"ssp2_in", NULL, "ssp2 Rx"},

	/* ssp1 path */
	{"Dummy Playback", NULL, "ssp1 Tx"},
	{"ssp1 Tx", NULL, "ssp1_out"},

	{"AIF1 Playback", NULL, "ssp0 Tx"},
	{"ssp0 Tx", NULL, "codec1_out"},
	{"ssp0 Tx", NULL, "codec0_out"},

	{"ssp0 Rx", NULL, "AIF1 Capture"},
	{"codec0_in", NULL, "ssp0 Rx"},

	{"Headphone Jack", NULL, "Platform Clock"},
	{"MIC", NULL, "Platform Clock"},

	{"hifi1", NULL, "iDisp1 Tx"},
	{"iDisp1 Tx", NULL, "iDisp1_out"},
	{"hifi2", NULL, "iDisp2 Tx"},
	{"iDisp2 Tx", NULL, "iDisp2_out"},
	{"hifi3", NULL, "iDisp3 Tx"},
	{"iDisp3 Tx", NULL, "iDisp3_out"},
};

static int cnl_rt274_init(struct snd_soc_pcm_runtime *runtime)
{
	int ret;
	struct snd_soc_codec *codec = runtime->codec;
	struct snd_soc_card *card = runtime->card;
	struct snd_soc_dai *codec_dai = runtime->codec_dai;

	ret = snd_soc_card_jack_new(runtime->card, "Headset",
		SND_JACK_HEADSET, &cnl_headset,
		cnl_headset_pins, ARRAY_SIZE(cnl_headset_pins));

	if (ret)
		return ret;

	snd_soc_codec_set_jack(codec, &cnl_headset, NULL);

	/* TDM 4 slots 24 bit, set Rx & Tx bitmask to 4 active slots */
	ret = snd_soc_dai_set_tdm_slot(codec_dai, 0xF, 0xF, 4, 24);
	if (ret < 0) {
		dev_err(runtime->dev, "can't set codec pcm format %d\n", ret);
		return ret;
	}

	card->dapm.idle_bias_off = true;

	return 0;
}

static int cnl_be_fixup(struct snd_soc_pcm_runtime *rtd,
			    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = CNL_BE_FIXUP_RATE;
	channels->min = channels->max = 2;
	snd_mask_none(hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT));
	snd_mask_set(hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT),
						SNDRV_PCM_FORMAT_S24_LE);

	return 0;
}

static int cnl_dmic_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);
	channels->min = channels->max = 2;

	return 0;
}

static const char pname[] = "0000:00:1f.3";
static const char cname[] = "i2c-INT34C2:00";

struct snd_soc_dai_link cnl_rt274_dailink[] = {
	{
		.name = "SSP0-Codec",
		.id = 1,
		.cpu_dai_name = "SSP0 Pin",
		.codec_name = cname,
		.codec_dai_name = "rt274-aif1",
		.platform_name = pname,
		.be_hw_params_fixup = cnl_be_fixup,
		.ignore_suspend = 1,
		.no_pcm = 1,
		.dai_fmt = SND_SOC_DAIFMT_DSP_A |
			SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.init = cnl_rt274_init,
	},
	{
		.name = "dmic01",
		.id = 2,
		.cpu_dai_name = "DMIC01 Pin",
		.codec_name = "dmic-codec",
		.codec_dai_name = "dmic-hifi",
		.platform_name = pname,
		.ignore_suspend = 1,
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_hw_params_fixup = cnl_dmic_fixup,
	},
};

static int
cnl_add_dai_link(struct snd_soc_card *card, struct snd_soc_dai_link *link)
{
	link->platform_name = pname;
	link->nonatomic = 1;

	return 0;
}

/* SoC card */
static struct snd_soc_card snd_soc_card_cnl = {
	.name = "cnl-audio",
	.dai_link = cnl_rt274_msic_dailink,
	.num_links = ARRAY_SIZE(cnl_rt274_msic_dailink),
	.dapm_widgets = cnl_rt274_widgets,
	.num_dapm_widgets = ARRAY_SIZE(cnl_rt274_widgets),
	.dapm_routes = cnl_map,
	.num_dapm_routes = ARRAY_SIZE(cnl_map),
	.controls = cnl_controls,
	.num_controls = ARRAY_SIZE(cnl_controls),
	.add_dai_link = cnl_add_dai_link,
	.fully_routed = true,
#if !IS_ENABLED(CONFIG_SND_SOC_SOF_INTEL)
	.add_dai_link = cnl_add_dai_link,
	.late_probe = cnl_card_late_probe,
#endif
};

static int snd_cnl_rt274_mc_probe(struct platform_device *pdev)
{
	struct cnl_rt274_private *ctx;

	ctx = devm_kzalloc(&pdev->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->pcm_count = ARRAY_SIZE(cnl_rt274_msic_dailink);
	INIT_LIST_HEAD(&ctx->hdmi_pcm_list);

	snd_soc_card_cnl.dev = &pdev->dev;
	snd_soc_card_set_drvdata(&snd_soc_card_cnl, ctx);

	return devm_snd_soc_register_card(&pdev->dev, &snd_soc_card_cnl);
}

static const struct platform_device_id cnl_board_ids[] = {
	{ .name = "cnl_rt274" },
	{ .name = "icl_rt274" },
	{ }
};

static struct platform_driver snd_cnl_rt274_driver = {
	.driver = {
		.name = "cnl_rt274",
		.pm = &snd_soc_pm_ops,
	},
	.probe = snd_cnl_rt274_mc_probe,
	.id_table = cnl_board_ids,
};

module_platform_driver(snd_cnl_rt274_driver);

MODULE_AUTHOR("Guneshwor Singh <guneshwor.o.singh@intel.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:cnl_rt274");
MODULE_ALIAS("platform:icl_rt274");
