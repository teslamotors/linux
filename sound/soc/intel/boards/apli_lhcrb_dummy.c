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

static int apli_lfcrb_dummy_startup(struct snd_pcm_substream *substream)
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

static struct snd_soc_ops apli_lfcrb_dummy_ops = {
	.startup = apli_lfcrb_dummy_startup,
};

static const struct snd_kcontrol_new apli_controls[] = {
	SOC_DAPM_PIN_SWITCH("SSP2 Speaker"),
	SOC_DAPM_PIN_SWITCH("SSP2 Mic"),
	SOC_DAPM_PIN_SWITCH("SSP4 Speaker"),
	SOC_DAPM_PIN_SWITCH("SSP4 Mic"),
};

static const struct snd_soc_dapm_widget apli_widgets[] = {
	SND_SOC_DAPM_SPK("SSP2 Speaker", NULL),
	SND_SOC_DAPM_MIC("SSP2 Mic", NULL),
	SND_SOC_DAPM_SPK("SSP4 Speaker", NULL),
	SND_SOC_DAPM_MIC("SSP4 Mic", NULL),
};

static const struct snd_soc_dapm_route apli_lhcrb_dummy_map[] = {
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

/* apli digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link apli_lhcrb_dummy_dais[] = {
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
		.ops = &apli_lfcrb_dummy_ops,
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
		.trigger = {SND_SOC_DPCM_TRIGGER_POST
			, SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.nonatomic = 1,
		.dynamic = 1,
		.ops = &apli_lfcrb_dummy_ops,
	},
	{
		.name = "SSP4 Playback Port",
		.stream_name = "SSP4 Speaker",
		.cpu_dai_name = "Deepbuffer Pin",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST
			, SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
		.ops = &apli_lfcrb_dummy_ops,
	},
	{
		.name = "SSP4 Capture Port",
		.stream_name = "SSP4 Mic",
		.cpu_dai_name = "Reference Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.init = NULL,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST
			, SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.nonatomic = 1,
		.dynamic = 1,
		.ops = &apli_lfcrb_dummy_ops,
	},
	/* Back End DAI links */
	{
		/* SSP1 - Codec */
		.name = "SSP1-Codec",
		.be_id = 0,
		.cpu_dai_name = "SSP1 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
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
	{
		/* SSP3 - Codec */
		.name = "SSP3-Codec",
		.be_id = 1,
		.cpu_dai_name = "SSP3 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
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

/* apli audio machine driver for dummy Proto Board*/
static struct snd_soc_card apli_lhcrb_dummy = {
	.name = "apli-lhcrb-dummy_i2s",
	.owner = THIS_MODULE,
	.dai_link = apli_lhcrb_dummy_dais,
	.num_links = ARRAY_SIZE(apli_lhcrb_dummy_dais),
	.controls = apli_controls,
	.num_controls = ARRAY_SIZE(apli_controls),
	.dapm_widgets = apli_widgets,
	.num_dapm_widgets = ARRAY_SIZE(apli_widgets),
	.dapm_routes = apli_lhcrb_dummy_map,
	.num_dapm_routes = ARRAY_SIZE(apli_lhcrb_dummy_map),
	.fully_routed = true,
};

static int apli_audio_probe(struct platform_device *pdev)
{
	apli_lhcrb_dummy.dev = &pdev->dev;
	return snd_soc_register_card(&apli_lhcrb_dummy);
}

static int apli_audio_remove(struct platform_device *pdev)
{
	snd_soc_unregister_card(&apli_lhcrb_dummy);
	return 0;
}

static struct platform_driver apli_audio = {
	.probe = apli_audio_probe,
	.remove = apli_audio_remove,
	.driver = {
		.name = "lfcrb_dummy_i2s",
	},
};

module_platform_driver(apli_audio)

/* Module information */
MODULE_DESCRIPTION("Intel Audio dummy Machine Driver for APL-I LH CRB");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:lfcrb_dummy_i2s");
