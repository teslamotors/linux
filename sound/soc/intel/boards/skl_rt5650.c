/*
 * Intel Skylake ALC5650 Machine Driver
 *
 * Copyright (C) 2014, Intel Corporation. All rights reserved.
 *
 * Modified from:
 *   skl_rt286.c
 *
 *   Copyright (C) 2013, Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/pcm_params.h>

#include "../../codecs/rt5645.h"

#define SKL_PLAT_CLK_3_HZ       19200000
#define SKL_CODEC_DAI   "rt5645-aif1"

static struct snd_soc_jack skylake_headset;
static struct snd_soc_card skylake_rt5650;

/* Headset jack detection DAPM pins */
static struct snd_soc_jack_pin skylake_headset_pins[] = {
	{
		.pin = "Mic Jack",
		.mask = SND_JACK_MICROPHONE,
	},
	{
		.pin = "Headphone Jack",
		.mask = SND_JACK_HEADPHONE,
	},
};

static inline struct snd_soc_dai *skl_get_codec_dai(struct snd_soc_card *card)
{
	int i;

	for (i = 0; i < card->num_rtd; i++) {
		struct snd_soc_pcm_runtime *rtd;

		rtd = card->rtd + i;
		if (!strncmp(rtd->codec_dai->name, SKL_CODEC_DAI,
			strlen(SKL_CODEC_DAI)))
			return rtd->codec_dai;
	}

	return NULL;
}

static int codec_clock_control(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *k, int  event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct snd_soc_dai *codec_dai;
	int ret;

	codec_dai = skl_get_codec_dai(card);
	if (!codec_dai) {
		dev_err(card->dev, "Codec dai not found; Unable to set codec clock\n");
		return -EIO;
	}

	if (!SND_SOC_DAPM_EVENT_OFF(event))
		return 0;

	/*
	 * Set codec sysclk source to its internal clock because codec PLL will
	 * be off when idle and MCLK will also be off by ACPI when codec is
	 * runtime suspended. Codec needs clock for jack detection and button
	 * press.
	 */
	ret = snd_soc_dai_set_sysclk(codec_dai, RT5645_SCLK_S_RCCLK,
		0, SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(card->dev, "can't set codec sysclk: %d\n", ret);

	return ret;
}

static const struct snd_kcontrol_new skylake_controls[] = {
	SOC_DAPM_PIN_SWITCH("Ext Spk"),
	SOC_DAPM_PIN_SWITCH("Headphone Jack"),
	SOC_DAPM_PIN_SWITCH("Mic Jack"),
};

static const struct snd_soc_dapm_widget skylake_widgets[] = {
	SND_SOC_DAPM_SPK("Ext Spk", NULL),
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
	SND_SOC_DAPM_SUPPLY("Codec Clock", SND_SOC_NOPM, 0, 0,
			codec_clock_control, SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIC("SoC DMIC", NULL),
};

static const struct snd_soc_dapm_route skylake_rt5650_map[] = {
	{ "IN1P", NULL, "Mic Jack"},
	{ "IN1N", NULL, "Mic Jack"},

	{ "Headphone Jack", NULL, "HPOL"},
	{ "Headphone Jack", NULL, "HPOR"},

	{ "Ext Spk", NULL, "SPOL"},
	{ "Ext Spk", NULL, "SPOR"},

	{ "DMIC AIF", NULL, "SoC DMIC"},

	{ "Headphone Jack", NULL, "Codec Clock"},
	{ "Mic Jack", NULL, "Codec Clock"},
	{ "Ext Spk", NULL, "Codec Clock"},

	/* CODEC BE connections */
	{ "AIF1 Playback", NULL, "ssp0 Tx"},
	{ "ssp0 Tx", NULL, "codec0_out"},
	{ "ssp0 Tx", NULL, "codec1_out"},

	{ "codec0_in", NULL, "ssp0 Rx" },
	{ "codec1_in", NULL, "ssp0 Rx" },
	{ "ssp0 Rx", NULL, "AIF1 Capture" },

	{ "dmic01_hifi", NULL, "DMIC01 Rx" },
	{ "DMIC01 Rx", NULL, "Dummy Capture" },

	{ "hif1", NULL, "iDisp Tx"},
	{ "iDisp Tx", NULL, "iDisp_out"},

};

static int skylake_rt5650_codec_init(struct snd_soc_pcm_runtime *runtime)
{
	int ret;
	int jack_type;
	struct snd_soc_codec *codec = runtime->codec;
	struct snd_soc_dai *codec_dai = runtime->codec_dai;
	struct skl_mc_private *ctx = snd_soc_card_get_drvdata(runtime->card);

	/* Select clk_i2s1_asrc as ASRC clock source */
	rt5645_sel_asrc_clk_src(codec,
				RT5645_DA_STEREO_FILTER |
				RT5645_DA_MONO_L_FILTER |
				RT5645_DA_MONO_R_FILTER |
				RT5645_AD_STEREO_FILTER,
				RT5645_CLK_SEL_I2S1_ASRC);

	jack_type = SND_JACK_HEADPHONE | SND_JACK_MICROPHONE |
			SND_JACK_BTN_0 | SND_JACK_BTN_1 |
			SND_JACK_BTN_2 | SND_JACK_BTN_3;

	ret = snd_soc_card_jack_new(runtime->card, "Headset Jack",
		jack_type, &skylake_headset, skylake_headset_pins,
		ARRAY_SIZE(skylake_headset_pins));

	if (ret) {
		dev_err(runtime->dev, "Headset jack creation failed %d\n", ret);
		return ret;
	}

	rt5645_set_jack_detect(codec, &skylake_headset, &skylake_headset,
				&skylake_headset);

	return ret;
}

static int skylake_ssp0_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);
	struct snd_mask *fmt = hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT);


	/* The ADSP will covert the FE rate to 48k, stereo */
	rate->min = rate->max = 48000;
	channels->min = channels->max = 2;

	/* set SSP0 to 24 bit */
	snd_mask_none(fmt);
	snd_mask_set(fmt, SNDRV_PCM_FORMAT_S24_LE);

	return 0;
}

static int skylake_rt5650_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	/* set codec PLL source to the 19.2MHz platform clock (MCLK) */
	ret = snd_soc_dai_set_pll(codec_dai, 0, RT5645_PLL1_S_MCLK,
					SKL_PLAT_CLK_3_HZ,
					params_rate(params) * 512);
	if (ret < 0) {
		dev_err(rtd->dev, "can't set codec pll: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(codec_dai, RT5645_SCLK_S_PLL1,
				params_rate(params) * 512, SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(rtd->dev, "can't set codec sysclk: %d\n", ret);

	return ret;
}

static struct snd_soc_ops skylake_be_rt5650_ops = {
	.hw_params = skylake_rt5650_hw_params,
};

/* skylake digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link skylake_rt5650_dais[] = {
	/* Front End DAI links */
	{
		.name = "Skl Audio Port",
		.stream_name = "Audio",
		.cpu_dai_name = "System Pin",
		.platform_name = "0000:00:1f.3",
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
		.nonatomic = 1,
	},
	{
		.name = "Skl Audio Capture Port",
		.stream_name = "Audio Record",
		.cpu_dai_name = "System Pin",
		.platform_name = "0000:00:1f.3",
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_capture = 1,
		.nonatomic = 1,
	},
	{
		.name = "Skl Audio Reference cap",
		.stream_name = "refcap",
		.cpu_dai_name = "Reference Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:1f.3",
		.init = NULL,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.nonatomic = 1,
		.dynamic = 1,
	},
	{
		.name = "Skl HDMI Port",
		.stream_name = "Hdmi",
		.cpu_dai_name = "System Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:1f.3",
		.dpcm_playback = 1,
		.init = NULL,
		.ignore_suspend = 1,
		.dynamic = 1,
	},

	/* Back End DAI links */
	{
		/* SSP0 - Codec */
		.name = "SSP0-Codec",
		.be_id = 0,
		.cpu_dai_name = "SSP0 Pin",
		.platform_name = "0000:00:1f.3",
		.no_pcm = 1,
		.codec_name = "i2c-10EC5650:00",
		.codec_dai_name = "rt5645-aif1",
		.init = skylake_rt5650_codec_init,
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = skylake_ssp0_fixup,
		.ops = &skylake_be_rt5650_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "dmic01",
		.be_id = 1,
		.cpu_dai_name = "DMIC01 Pin",
		.codec_name = "dmic-codec",
		.codec_dai_name = "dmic-hifi",
		.platform_name = "0000:00:1f.3",
		.ignore_suspend = 1,
		.dpcm_capture = 1,
		.no_pcm = 1,
	},
};

/* skylake audio machine driver for SPT + ALC5650 */
static struct snd_soc_card skylake_rt5650 = {
	.name = "skylakert5650",
	.owner = THIS_MODULE,
	.dai_link = skylake_rt5650_dais,
	.num_links = ARRAY_SIZE(skylake_rt5650_dais),
	.controls = skylake_controls,
	.num_controls = ARRAY_SIZE(skylake_controls),
	.dapm_widgets = skylake_widgets,
	.num_dapm_widgets = ARRAY_SIZE(skylake_widgets),
	.dapm_routes = skylake_rt5650_map,
	.num_dapm_routes = ARRAY_SIZE(skylake_rt5650_map),
};

static int skylake_audio_probe(struct platform_device *pdev)
{
	/* register the soc card*/
	skylake_rt5650.dev = &pdev->dev;

	return snd_soc_register_card(&skylake_rt5650);
}

static int skylake_audio_remove(struct platform_device *pdev)
{
	snd_soc_unregister_card(&skylake_rt5650);

	return 0;
}

static struct platform_driver skylake_audio = {
	.probe = skylake_audio_probe,
	.remove = skylake_audio_remove,
	.driver = {
		.name = "skl_alc5650_i2s",
	},
};

module_platform_driver(skylake_audio)

/* Module information */
MODULE_AUTHOR("Sathyanarayana Nujella <sathyanarayana.nujella@intel.com>");
MODULE_DESCRIPTION("ASoC Intel Audio driver for SKL with ALC5650 in I2S Mode");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:skl_alc5650_i2s");
