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

#define MODEL3_AMP1_NAME "i2c-TDA7802:00"
#define MODEL3_AMP2_NAME "i2c-TDA7802:01"
#define MODEL3_DAI_NAME "tda7802-oct"

static const struct snd_soc_dapm_widget model3_widgets[] = {
	SND_SOC_DAPM_SPK("A2B Bridge Tx", NULL),
	SND_SOC_DAPM_MIC("A2B Bridge Rx", NULL),
	SND_SOC_DAPM_SPK("HFP Tx", NULL),
	SND_SOC_DAPM_MIC("HFP Rx", NULL),
	SND_SOC_DAPM_MIC("Loopback", NULL),
#if IS_ENABLED(CONFIG_SND_SOC_INTEL_MODEL3_MACH_TDF8532_CODEC) || \
	IS_ENABLED(CONFIG_SND_SOC_INTEL_MODEL3_MACH_TDA7802_CODEC)
	SND_SOC_DAPM_SPK("Speaker", NULL),
#endif
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

static struct snd_soc_dai_link_component model3_codecs_components[] = {
	{ /* AMP1 */
		.name = MODEL3_AMP1_NAME,
		.dai_name = MODEL3_DAI_NAME,
	},
	{ /* AMP2 */
		.name = MODEL3_AMP2_NAME,
		.dai_name = MODEL3_DAI_NAME,
	},
};

static int model3_ssp5_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	const int rx_mask = 0, slots = 8, slot_width = 32;
	int tx_mask;
	int ret = 0, i;

	for (i = 0; i < rtd->num_codecs; i++) {
		struct snd_soc_dai *codec_dai = rtd->codec_dais[i];

		if (!strcmp(codec_dai->component->name, MODEL3_AMP1_NAME)) {
			/* Use channel 0-3 for the first amp */
			tx_mask = 0x0f;
		} else if (!strcmp(codec_dai->component->name,
				   MODEL3_AMP2_NAME)) {
			/* Use channel 4-7 for the second amp */
			tx_mask = 0xf0;
		} else {
			dev_err(rtd->dev,
				"could not match codec_dai %s to amp\n",
				codec_dai->component->name);
			return -EINVAL;
		}

		ret = snd_soc_dai_set_tdm_slot(codec_dai, tx_mask, rx_mask,
					       slots, slot_width);
		if (ret < 0) {
			dev_err(rtd->dev, "set TDM slot err:%d\n", ret);
			return ret;
		}
	}

	return ret;
}

static struct snd_soc_ops model3_ssp5_ops = {
	.hw_params = model3_ssp5_hw_params,
};

static struct snd_soc_dai_link model3_dais[] = {
	/* Front End DAI links */
	/* SSP5 - FE DAI Link */
	{
		.name = "Speaker Port",
		.stream_name = "Speaker",
		.cpu_dai_name = "System Pin 5",
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
		.cpu_dai_name = "System Pin 5",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.dpcm_capture = 1,
		.dynamic = 1,
		.nonatomic = 1,
	},
	/* SSP0 - FE DAI Link */
	{
		.name = "A2B Port",
		.stream_name = "A2B",
		.cpu_dai_name = "System Pin",
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
	/* SSP2 - FE DAI Link */
	{
		.name = "HFP Port",
		.stream_name = "HFP",
		.cpu_dai_name = "System Pin 2",
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
		.id = 1,
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
		.id = 1,
		.cpu_dai_name = "SSP2 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.dpcm_capture = 1,
		.dpcm_playback = 1,
		.no_pcm = 1,
	},
#ifdef CONFIG_SND_SOC_INTEL_MODEL3_MACH_TDF8532_CODEC
	{
		/* SSP4 - MRB amplifier */
		.name = "SSP4-Codec",
		.id = 0,
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
		.id = 1,
		.cpu_dai_name = "SSP5 Pin",
		.codecs = model3_codecs_components,
		.num_codecs = ARRAY_SIZE(model3_codecs_components),
		.ops = &model3_ssp5_ops,
		.platform_name = "0000:00:0e.0",
		.dpcm_playback = 1,
		.no_pcm = 1,
	},
#endif
	{
		/* SSP5 - loopback */
		.name = "SSP5-Codec-Loopback",
		.id = 1,
		.cpu_dai_name = "SSP5 Pin Loopback",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.dpcm_capture = 1,
		.no_pcm = 1,
	}
};


static struct snd_soc_card model3 = {
	.name = "model3",
	.owner = THIS_MODULE,
	.dai_link = model3_dais,
	.num_links = ARRAY_SIZE(model3_dais),
	.dapm_widgets = model3_widgets,
	.num_dapm_widgets = ARRAY_SIZE(model3_widgets),
	.dapm_routes = model3_map,
	.num_dapm_routes = ARRAY_SIZE(model3_map),
	.fully_routed = true,
};


static int model3_audio_probe(struct platform_device *pdev)
{
	model3.dev = &pdev->dev;
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
		.name = "model3",
	},
};

module_platform_driver(model3_audio)

MODULE_DESCRIPTION("Intel SST Audio for Tesla Model 3");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:model3");
