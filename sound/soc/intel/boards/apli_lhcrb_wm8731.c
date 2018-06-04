/*
 * Intel Apli(Apollo Lake) I2S Machine Driver for
 * LF (Leaf Hill) reference platform
 *
 * Copyright (C) 2014-2015, Intel Corporation. All rights reserved.
 *
 * Modified from:
 *   Intel APLI I2S Machine driver
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
#include <linux/gpio.h>
#include <sound/pcm_params.h>
#include "../../codecs/wm8731.h"

static int apli_lhcrb_wm8731_startup(struct snd_pcm_substream *substream)
{
	int ret;
	static unsigned int rates[] = { 48000 };
	static unsigned int channels[] = {2};
	static u64 formats = SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE |
						SNDRV_PCM_FMTBIT_S32_LE;

	static struct snd_pcm_hw_constraint_list hw_constraints_rates = {
		.count = ARRAY_SIZE(rates),
		.list = rates,
		.mask = 0,
	};

	static struct snd_pcm_hw_constraint_list hw_constraints_channels = {
		.count = ARRAY_SIZE(channels),
		.list = channels,
		.mask = 0,
	};

	ret = snd_pcm_hw_constraint_list(substream->runtime, 0,
			SNDRV_PCM_HW_PARAM_RATE,
			&hw_constraints_rates);

	ret = snd_pcm_hw_constraint_list(substream->runtime, 0,
				SNDRV_PCM_HW_PARAM_CHANNELS,
				&hw_constraints_channels);

	ret = snd_pcm_hw_constraint_mask64(substream->runtime,
				SNDRV_PCM_HW_PARAM_FORMAT,
				formats);

	return ret;
}

static struct snd_soc_ops apli_lhcrb_wm8731_ops = {
	.startup = apli_lhcrb_wm8731_startup,
};

static const struct snd_kcontrol_new apli_controls[] = {
	SOC_DAPM_PIN_SWITCH("Headphone Jack"),
	SOC_DAPM_PIN_SWITCH("Mic Jack"),
};

static const struct snd_soc_dapm_widget apli_widgets[] = {
	SND_SOC_DAPM_SPK("SSP2 Speaker", NULL),
	SND_SOC_DAPM_MIC("SSP2 Mic", NULL),
	SND_SOC_DAPM_SPK("SSP4 Speaker", NULL),
	SND_SOC_DAPM_MIC("SSP4 Mic", NULL),
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
};

static const struct snd_soc_dapm_route apli_lhcrb_wm8731_map[] = {
	/* HP Jack */
	{"Headphone Jack", NULL, "LHPOUT"},
	{"Headphone Jack", NULL, "RHPOUT"},

	{"RHPOUT", NULL, "Playback"},
	{"LHPOUT", NULL, "Playback"},

	/* Mic Jack */
	{"Capture", NULL, "MICIN"},
	{"MICIN", NULL, "Mic Jack"},

	/* Codec BE connections */
	/* SSP2 follows Hardware pin naming */
	{"SSP2 Speaker", NULL, "ssp1 Tx"},
	{"ssp1 Tx", NULL, "codec0_out"},

	{"codec0_in", NULL, "ssp1 Rx"},
	{"ssp1 Rx", NULL, "SSP2 Mic"},

	/* SSP4 follows Hardware pin naming */
	{"SSP4 Speaker", NULL, "ssp3 Tx"},
	{"ssp3 Tx", NULL, "codec1_out"},

	{"codec1_in", NULL, "ssp3 Rx"},
	{"ssp3 Rx", NULL, "SSP4 Mic"},
};


static int apli_wm8731_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	/* set the codec system clock for DAC and ADC */
	ret = snd_soc_dai_set_sysclk(codec_dai, WM8731_SYSCLK_XTAL
		, 12288000, SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(rtd->dev, "snd_soc_dai_set_sysclk err = %d\n", ret);

	return ret;
}

static struct snd_soc_ops apli_wm8731_ops = {
	.hw_params = apli_wm8731_hw_params,
};


/* apli digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link apli_lhcrb_wm8731_dais[] = {
	/* Front End DAI links */
	{
		.name = "SSP2 Playback Port",
		.stream_name = "SSP2 Speaker",
		.cpu_dai_name = "System Pin",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
		.ops = &apli_lhcrb_wm8731_ops,
	},
	{
		.name = "SSP2 Capture Port",
		.stream_name = "SSP2 Mic",
		.cpu_dai_name = "System Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.init = NULL,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.nonatomic = 1,
		.dynamic = 1,
		.ops = &apli_lhcrb_wm8731_ops,
	},
	{
		.name = "SSP4 Playback Port",
		.stream_name = "wm8731 Headphone",
		.cpu_dai_name = "Deepbuffer Pin",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST
			, SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
		.ops = &apli_lhcrb_wm8731_ops,
	},
	{
		.name = "SSP4 Capture Port",
		.stream_name = "wm8731 Mic",
		.cpu_dai_name = "Reference Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.init = NULL,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.nonatomic = 1,
		.dynamic = 1,
		.ops = &apli_lhcrb_wm8731_ops,
	},
	/* Back End DAI links */
	{
		/* SSP1 - Codec */
		.name = "SSP1-Codec",
		.id = 0,
		.cpu_dai_name = "SSP1 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.ignore_suspend = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.no_pcm = 1,
		.init = NULL,
	},
	{
		/* SSP3 - Codec */
		.name = "SSP3-Codec",
		.id = 1,
		.cpu_dai_name = "SSP3 Pin",
		.codec_name = "wm8731.3-001a",
		.codec_dai_name = "wm8731-hifi",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.ops = &apli_wm8731_ops,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBS_CFS,
		.platform_name = "0000:00:0e.0",
		.ignore_suspend = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.no_pcm = 1,
		.init = NULL,
	},
};

/* apli audio machine driver for WM8731 Proto Board*/
static struct snd_soc_card apli_lhcrb_wm8731 = {
	.name = "apli-lhcrb-wm8731_i2s",
	.owner = THIS_MODULE,
	.dai_link = apli_lhcrb_wm8731_dais,
	.num_links = ARRAY_SIZE(apli_lhcrb_wm8731_dais),
	.controls = apli_controls,
	.num_controls = ARRAY_SIZE(apli_controls),
	.dapm_widgets = apli_widgets,
	.num_dapm_widgets = ARRAY_SIZE(apli_widgets),
	.dapm_routes = apli_lhcrb_wm8731_map,
	.num_dapm_routes = ARRAY_SIZE(apli_lhcrb_wm8731_map),
	.fully_routed = true,
};

static int apli_audio_probe(struct platform_device *pdev)
{
	apli_lhcrb_wm8731.dev = &pdev->dev;
	return snd_soc_register_card(&apli_lhcrb_wm8731);
}

static int apli_audio_remove(struct platform_device *pdev)
{
	snd_soc_unregister_card(&apli_lhcrb_wm8731);
	return 0;
}

static struct platform_driver apli_audio = {
	.probe = apli_audio_probe,
	.remove = apli_audio_remove,
	.driver = {
		.name = "lhcrb_wm8731_i2s",
	},
};

module_platform_driver(apli_audio)

/* Module information */
MODULE_DESCRIPTION("Intel Audio WM8731 Machine driver for APL-I LH CRB");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:lhcrb_wm8731_i2s");
