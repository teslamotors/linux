/*
 * Intel Broxton-P I2S Machine Driver for IVI reference platform
 *
 * Copyright (C) 2014-2015, Intel Corporation. All rights reserved.
 *
 * Modified from:
 *   Intel Skylake I2S Machine driver
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

/* NXP TDF8532 Amplifier Mute Pin */
#define GPIO_AMP_MUTE 322


static const struct snd_kcontrol_new broxton_controls[] = {
	SOC_DAPM_PIN_SWITCH("Speaker"),
};

/* mute speaker amplifier on/off depending on use */
static int amp_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int event)
{
	gpio_set_value(GPIO_AMP_MUTE, SND_SOC_DAPM_EVENT_ON(event));
	return 0;
}

static const struct snd_soc_dapm_widget broxton_widgets[] = {
	SND_SOC_DAPM_SPK("Speaker", amp_event),
	SND_SOC_DAPM_MIC("Dirana", NULL),
	SND_SOC_DAPM_HP("DiranaPb", NULL),
};

static const struct snd_soc_dapm_route broxton_gpmrb_map[] = {

	/* Speaker BE connections */
	{ "Speaker", NULL, "ssp4 Tx"},
	{ "ssp4 Tx", NULL, "codec0_out"},

	{ "dirana_in", NULL, "ssp2 Rx"},
	{ "ssp2 Rx", NULL, "Dirana"},

	{ "DiranaPb", NULL, "ssp2 Tx"},
	{ "ssp2 Tx", NULL, "dirana_out"},
};


static int broxton_gpmrb_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int ret = 0;

	return ret;
}

static struct snd_soc_ops broxton_gpmrb_ops = {
	.hw_params = broxton_gpmrb_hw_params,
};

/* broxton digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link broxton_gpmrb_dais[] = {
	/* Front End DAI links */
	{
		.name = "Speaker Port",
		.stream_name = "Speaker",
		.cpu_dai_name = "Speaker Pin",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
	},
	{
		.name = "Dirana Cp Port",
		.stream_name = "Dirana Cp",
		.cpu_dai_name = "Dirana Cp Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.init = NULL,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.nonatomic = 1,
		.dynamic = 1,
	},

	{
		.name = "Dirana Pb Port",
		.stream_name = "Dirana Pb",
		.cpu_dai_name = "Dirana Pb Pin",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
	},

	/* Back End DAI links */
	{
		/* SSP4 - Amplifier */
		.name = "SSP4-Codec",
		.be_id = 0,
		.cpu_dai_name = "SSP4 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.ignore_suspend = 1,
		.dpcm_playback = 1,
		.no_pcm = 1,
	},
	{
		/* SSP2 - Dirana */
		.name = "SSP2-Codec",
		.be_id = 1,
		.cpu_dai_name = "SSP2 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.ignore_suspend = 1,
		.dpcm_capture = 1,
		.dpcm_playback = 1,
		.no_pcm = 1,
	},
};

/* broxton audio machine driver for SPT + RT298S */
static struct snd_soc_card broxton_gpmrb = {
	.name = "broxton-gpmrb",
	.owner = THIS_MODULE,
	.dai_link = broxton_gpmrb_dais,
	.num_links = ARRAY_SIZE(broxton_gpmrb_dais),
	.controls = broxton_controls,
	.num_controls = ARRAY_SIZE(broxton_controls),
	.dapm_widgets = broxton_widgets,
	.num_dapm_widgets = ARRAY_SIZE(broxton_widgets),
	.dapm_routes = broxton_gpmrb_map,
	.num_dapm_routes = ARRAY_SIZE(broxton_gpmrb_map),
	.fully_routed = true,
};



static int broxton_audio_probe(struct platform_device *pdev)
{
	broxton_gpmrb.dev = &pdev->dev;
	return snd_soc_register_card(&broxton_gpmrb);
}

static int broxton_audio_remove(struct platform_device *pdev)
{
	snd_soc_unregister_card(&broxton_gpmrb);
	return 0;
}

static struct platform_driver broxton_audio = {
	.probe = broxton_audio_probe,
	.remove = broxton_audio_remove,
	.driver = {
	.name = "gpmrb_machine",
	},
};

module_platform_driver(broxton_audio)

/* Module information */
MODULE_DESCRIPTION("Intel SST Audio for Broxton GP MRB");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:gpmrb_machine");
