/*
 * Tesla I2S Machine Driver for Model SX Infotainment 2
 *
 * Copyright (C) 2017, Tesla Motors Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * General Public License for more details.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <sound/pcm_params.h>

static const struct snd_soc_dapm_widget modelsx_info2_widgets[] = {
	SND_SOC_DAPM_SPK("AmpS Codec", NULL),
	SND_SOC_DAPM_SPK("HFP Tx", NULL),
	SND_SOC_DAPM_MIC("HFP Rx", NULL),
	SND_SOC_DAPM_MIC("AmpM Loopback", NULL),
	SND_SOC_DAPM_MIC("Mic Codec", NULL),
	SND_SOC_DAPM_SPK("AmpM Codec", NULL),
};

static const struct snd_soc_dapm_route modelsx_info2_map[] = {
	{ "AmpS Codec", NULL, "ssp0 Tx" },
	{ "ssp0 Tx", NULL, "amps_out" },

	{ "HFP Tx", NULL, "ssp2 Tx" },
	{ "ssp2 Tx", NULL, "hfp_out" },

	{ "hfp_in", NULL, "ssp2 Rx" },
	{ "ssp2 Rx", NULL, "HFP Rx" },

	{ "mic_in", NULL, "ssp1 Rx" },
	{ "ssp1 Rx", NULL, "Mic Codec" },

	{ "ampm_in", NULL, "ssp5 Rx" },
	{ "ssp5 Rx", NULL, "AmpM Loopback" },

	{ "AmpM Codec", NULL, "ssp5 Tx" },
	{ "ssp5 Tx", NULL, "ampm_out" },
};

struct modelsx_info2_prv {
	int srate;
};

#define DEFAULT_HFP_RATE_INDEX 1

static unsigned int modelsx_info2_hfp_rates[] = {
	8000,
	16000,
};

static const char * const hfp_rate[] = { "8kHz", "16kHz" };

static const struct soc_enum hfp_rate_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(hfp_rate), hfp_rate);

static int hfp_sample_rate_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct modelsx_info2_prv *drv = snd_soc_card_get_drvdata(card);

	ucontrol->value.integer.value[0] = drv->srate;
	return 0;
}

static int hfp_sample_rate_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct modelsx_info2_prv *drv = snd_soc_card_get_drvdata(card);

	if (ucontrol->value.integer.value[0] == drv->srate)
		return 0;

	drv->srate = ucontrol->value.integer.value[0];
	return 0;
}

static const struct snd_kcontrol_new modelsx_info2_snd_controls[] = {
	SOC_ENUM_EXT("HFP Rate", hfp_rate_enum,
		     hfp_sample_rate_get, hfp_sample_rate_put),
};

static int modelsx_info2_ssp2_fixup(struct snd_soc_pcm_runtime *rtd,
				    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate =
		hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);

	struct snd_soc_card *card =  rtd->card;
	struct modelsx_info2_prv *drv = snd_soc_card_get_drvdata(card);

	struct snd_mask *fmt= hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT);

	/* SSP2 operates with a BT Transceiver */
	rate->min = rate->max = modelsx_info2_hfp_rates[drv->srate];

	snd_mask_none(fmt);
	snd_mask_set(fmt, SNDRV_PCM_FORMAT_S16_LE);

	return 0;
}

static struct snd_soc_dai_link modelsx_info2_dais[] = {
	/* Front End DAI links */
	{
		.name = "AmpM Port",
		.stream_name = "AmpM",
		.cpu_dai_name = "AmpM Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.dynamic = 1,
		.nonatomic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
	},
	{
		.name = "AmpS Port",
		.stream_name = "AmpS",
		.cpu_dai_name = "AmpS Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.dpcm_playback = 1,
		.dynamic = 1,
		.nonatomic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
	},
	{
		.name = "Mic Port",
		.stream_name = "Mic",
		.cpu_dai_name = "Mic Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.dpcm_capture = 1,
		.dynamic = 1,
		.nonatomic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
	},
	{
		.name = "HFP Port",
		.stream_name = "HFP",
		.cpu_dai_name = "HFP Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.dpcm_capture = 1,
		.dpcm_playback = 1,
		.dynamic = 1,
		.nonatomic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
	},

	/* Back End DAI links */
	{
		/* SSP0 - Slave amplifier */
		.name = "SSP0-Codec",
		.be_id = 1,
		.cpu_dai_name = "SSP0 Pin",
		.codec_name = "tda7802-codec.4-006e",
		.codec_dai_name = "tda7802-oct-s",
		.dpcm_playback = 1,
		.no_pcm = 1,
	},
	{
		/* SSP1 - Microphone */
		.name = "SSP1-Codec",
		.be_id = 1,
		.cpu_dai_name = "SSP1 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.dpcm_capture = 1,
		.no_pcm = 1,
	},
	{
		/* SSP2 - HFP */
		.name = "SSP2-Codec",
		.be_id = 1,
		.cpu_dai_name = "SSP2 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.dpcm_capture = 1,
		.dpcm_playback = 1,
		.be_hw_params_fixup = modelsx_info2_ssp2_fixup,
		.no_pcm = 1,
	},
	{
		/* SSP5 - Master amplifier */
		.name = "SSP5-Codec",
		.be_id = 1,
		.cpu_dai_name = "SSP5 Pin",
		.codec_name = "tda7802-codec.4-006c",
		.codec_dai_name = "tda7802-oct-m",
		.platform_name = "0000:00:0e.0",
		.dpcm_playback = 1,
		.no_pcm = 1,
	},
	{
		/* SSP5 - Loopback */
		.name = "SSP5-Codec-Loopback",
		.be_id = 1,
		.cpu_dai_name = "SSP5 Pin Loopback",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.dpcm_capture = 1,
		.no_pcm = 1,
	},
};

static struct snd_soc_card modelsx_info2 = {
	.name = "modelsx_info2",
	.owner = THIS_MODULE,
	.dai_link = modelsx_info2_dais,
	.num_links = ARRAY_SIZE(modelsx_info2_dais),
	.controls = modelsx_info2_snd_controls,
	.num_controls = ARRAY_SIZE(modelsx_info2_snd_controls),
	.dapm_widgets = modelsx_info2_widgets,
	.num_dapm_widgets = ARRAY_SIZE(modelsx_info2_widgets),
	.dapm_routes = modelsx_info2_map,
	.num_dapm_routes = ARRAY_SIZE(modelsx_info2_map),
	.fully_routed = true,
};

static int modelsx_info2_audio_probe(struct platform_device *pdev)
{
	struct modelsx_info2_prv *drv;

	modelsx_info2.dev = &pdev->dev;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	drv->srate = DEFAULT_HFP_RATE_INDEX;
	snd_soc_card_set_drvdata(&modelsx_info2, drv);

	return snd_soc_register_card(&modelsx_info2);
}

static int modelsx_info2_audio_remove(struct platform_device *pdev)
{
	snd_soc_unregister_card(&modelsx_info2);
	return 0;
}

static struct platform_driver modelsx_info2_audio = {
	.probe = modelsx_info2_audio_probe,
	.remove = modelsx_info2_audio_remove,
	.driver = {
		/* If you change this name you'll also need to change skl.c */
		.name = "gpmrb_machine",
	},
};

module_platform_driver(modelsx_info2_audio)

MODULE_DESCRIPTION("Intel SST Audio for Tesla Model SX Infotainment 2");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:gpmrb_machine");
