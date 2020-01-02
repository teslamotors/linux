/*
 * Tesla I2S Machine Driver for Model 3
 *
 * Copyright (C) 2016, Tesla Motors Corporation. All rights reserved.
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

static const struct snd_soc_dapm_widget model3_widgets[] = {
	SND_SOC_DAPM_SPK("A2B Bridge Tx", NULL),
	SND_SOC_DAPM_MIC("A2B Bridge Rx", NULL),
	SND_SOC_DAPM_SPK("HFP Tx", NULL),
	SND_SOC_DAPM_MIC("HFP Rx", NULL),
	SND_SOC_DAPM_MIC("Loopback", NULL),
	SND_SOC_DAPM_SPK("Base Speakers", NULL),
};


static const struct snd_soc_dapm_route model3_map[] = {
	{ "A2B Bridge Tx", NULL, "ssp0 Tx" },
	{ "ssp0 Tx", NULL, "a2b_out" },

	{ "a2b_in", NULL, "ssp0 Rx" },
	{ "ssp0 Rx", NULL, "A2B Bridge Rx" },

	{ "HFP Tx", NULL, "ssp2 Tx" },
	{ "ssp2 Tx", NULL, "hfp_out" },

	{ "hfp_in", NULL, "ssp2 Rx" },
	{ "ssp2 Rx", NULL, "HFP Rx" },

	{ "loopback_in", NULL, "ssp5 Rx" },
	{ "ssp5 Rx", NULL, "Loopback" },

#ifdef CONFIG_SND_SOC_INTEL_MODEL3_MACH_TDF8532_CODEC
	{ "Speaker", NULL, "ssp4 Tx" },
	{ "ssp4 Tx", NULL, "codec0_out" },
#endif

#ifdef CONFIG_SND_SOC_INTEL_MODEL3_MACH_TDA7802_CODEC
	{ "Speaker", NULL, "ssp5 Tx" },
	{ "ssp5 Tx", NULL, "codec0_out" },
#endif

	{ "Base Speakers", NULL, "Base Amp" },
};


struct model3_prv {
	int srate;
};


#define DEFAULT_HFP_RATE_INDEX 1

static unsigned int model3_hfp_rates[] = {
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
	struct model3_prv *drv = snd_soc_card_get_drvdata(card);

	ucontrol->value.integer.value[0] = drv->srate;
	return 0;
}


static int hfp_sample_rate_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct model3_prv *drv = snd_soc_card_get_drvdata(card);

	if (ucontrol->value.integer.value[0] == drv->srate)
		return 0;

	drv->srate = ucontrol->value.integer.value[0];
	return 0;
}


static const struct snd_kcontrol_new model3_snd_controls[] = {
	SOC_ENUM_EXT("HFP Rate", hfp_rate_enum,
		     hfp_sample_rate_get, hfp_sample_rate_put),
};



static int model3_ssp2_fixup(struct snd_soc_pcm_runtime *rtd,
			     struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate =
		hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);

	struct snd_soc_card *card =  rtd->card;
	struct model3_prv *drv = snd_soc_card_get_drvdata(card);

	struct snd_mask *fmt= hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT);

	/* SSP2 operates with a BT Transceiver */
	rate->min = rate->max = model3_hfp_rates[drv->srate];

	snd_mask_none(fmt);
	snd_mask_set(fmt, SNDRV_PCM_FORMAT_S16_LE);

	return 0;
}


static struct snd_soc_dai_link model3_dais[] = {
	/* Front End DAI links */
	{
		.name = "Speaker Port",
		.stream_name = "Speaker",
		.cpu_dai_name = "Speaker Pin",
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
		.name = "Loopback Port",
		.stream_name = "Loopback",
		.cpu_dai_name = "Loopback Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.dpcm_capture = 1,
		.dynamic = 1,
		.nonatomic = 1,
	},
	{
		.name = "A2B Port",
		.stream_name = "A2B",
		.cpu_dai_name = "A2B Pin",
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
		/* SSP0 - A2B */
		.name = "SSP0-Codec",
		.be_id = 1,
		.cpu_dai_name = "SSP0 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.dpcm_capture = 1,
		.dpcm_playback = 1,
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
		.be_hw_params_fixup = model3_ssp2_fixup,
		.no_pcm = 1,
	},
#ifdef CONFIG_SND_SOC_INTEL_MODEL3_MACH_TDF8532_CODEC
	{
		/* SSP4 - MRB amplifier */
		.name = "SSP4-Codec",
		.be_id = 0,
		.cpu_dai_name = "SSP4 Pin",
		.codec_name = "i2c-INT34C3:00",
		.codec_dai_name = "tdf8532-hifi",
		.platform_name = "0000:00:0e.0",
		.dpcm_playback = 1,
		.no_pcm = 1,
	},
#endif
#ifdef CONFIG_SND_SOC_INTEL_MODEL3_MACH_TDA7802_CODEC
	{
		/* SSP5 - Base amplifier */
		.name = "SSP5-Codec",
		.be_id = 1,
		.cpu_dai_name = "SSP5 Pin",
		/* The ALSA code determines the codec_name in this
		 * structure. It is derived from the codec name, and a
		 * hardware identifier. */
		.codec_name = "tda7802-codec.4-006e",
		.codec_dai_name = "tda7802-oct",
		.platform_name = "0000:00:0e.0",
		.dpcm_playback = 1,
		.no_pcm = 1,
	},
#endif
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


static struct snd_soc_card model3 = {
	.name = "model3",
	.owner = THIS_MODULE,
	.dai_link = model3_dais,
	.num_links = ARRAY_SIZE(model3_dais),
	.controls = model3_snd_controls,
	.num_controls = ARRAY_SIZE(model3_snd_controls),
	.dapm_widgets = model3_widgets,
	.num_dapm_widgets = ARRAY_SIZE(model3_widgets),
	.dapm_routes = model3_map,
	.num_dapm_routes = ARRAY_SIZE(model3_map),
	.fully_routed = true,
};


static int model3_audio_probe(struct platform_device *pdev)
{
	struct model3_prv *drv;

	model3.dev = &pdev->dev;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	drv->srate = DEFAULT_HFP_RATE_INDEX;
	snd_soc_card_set_drvdata(&model3, drv);

	return snd_soc_register_card(&model3);
}

static int model3_audio_remove(struct platform_device *pdev)
{
	snd_soc_unregister_card(&model3);
	return 0;
}


static struct platform_driver model3_audio = {
	.probe = model3_audio_probe,
	.remove = model3_audio_remove,
	.driver = {
		/* If you change this name you'll also need to change skl.c */
		.name = "gpmrb_machine",
	},
};

module_platform_driver(model3_audio)

MODULE_DESCRIPTION("Intel SST Audio for Tesla Model 3");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:gpmrb_machine");
