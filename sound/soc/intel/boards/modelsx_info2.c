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

#define MODELSX_INFO2_AMP1_NAME "i2c-TDA7802:00"
#define MODELSX_INFO2_AMP2_NAME "i2c-TDA7802:01"
#define MODELSX_INFO2_AMP3_NAME "i2c-TDA7802:02"
#define MODELSX_INFO2_AMP4_NAME "i2c-TDA7802:03"
#define MODELSX_INFO2_DAI_NAME_MASTER "tda7802-oct-m"
#define MODELSX_INFO2_DAI_NAME_SLAVE "tda7802-oct-s"

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

static struct snd_soc_dai_link_component
modelsx_info2_ssp0_codec_components[] = {
	{ /* AMP3 */
		.name = MODELSX_INFO2_AMP3_NAME,
		.dai_name = MODELSX_INFO2_DAI_NAME_SLAVE,
	},
	{ /* AMP4 */
		.name = MODELSX_INFO2_AMP4_NAME,
		.dai_name = MODELSX_INFO2_DAI_NAME_SLAVE,
	},
};

static struct snd_soc_dai_link_component
modelsx_info2_ssp5_codec_components[] = {
	{ /* AMP1 */
		.name = MODELSX_INFO2_AMP1_NAME,
		.dai_name = MODELSX_INFO2_DAI_NAME_MASTER,
	},
	{ /* AMP2 */
		.name = MODELSX_INFO2_AMP2_NAME,
		.dai_name = MODELSX_INFO2_DAI_NAME_MASTER,
	},
};

static int _modelsx_info2_multicodec_hw_params(
		struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai_link_component components[2])
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	const int rx_mask = 0, slots = 8, slot_width = 32;
	int tx_mask;
	int ret = 0, i;

	for (i = 0; i < rtd->num_codecs; i++) {
		struct snd_soc_dai *codec_dai = rtd->codec_dais[i];

		if (!strcmp(codec_dai->component->name, components[0].name)) {
			/* Use channel 0-3 for the first amp */
			tx_mask = 0x0f;
		} else if (!strcmp(codec_dai->component->name,
				   components[1].name)) {
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

static int modelsx_info2_ssp0_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	return _modelsx_info2_multicodec_hw_params(substream, params,
			 modelsx_info2_ssp0_codec_components);
}

static struct snd_soc_ops modelsx_info2_ssp0_ops = {
	.hw_params = modelsx_info2_ssp0_hw_params,
};

static int modelsx_info2_ssp5_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	return _modelsx_info2_multicodec_hw_params(substream, params,
			modelsx_info2_ssp5_codec_components);
}

static struct snd_soc_ops modelsx_info2_ssp5_ops = {
	.hw_params = modelsx_info2_ssp5_hw_params,
};

static struct snd_soc_dai_link modelsx_info2_dais[] = {
	/* Front End DAI links */
	/* SSP5 - FE DAI Link */
	{
		.name = "AmpM Port",
		.stream_name = "AmpM",
		.cpu_dai_name = "System Pin 5",
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
	/* SSP0 - FE DAI Link */
	{
		.name = "AmpS Port",
		.stream_name = "AmpS",
		.cpu_dai_name = "System Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.dpcm_playback = 1,
		.dynamic = 1,
		.nonatomic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
	},
	/* SSP1 - FE DAI Link
	 * Note: Uses System Pin capture stream - there is no System Capture 1.
	 */
	{
		.name = "Mic Port",
		.stream_name = "Mic",
		.cpu_dai_name = "System Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.dpcm_capture = 1,
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
		/* SSP0 - Slave amplifier */
		.name = "SSP0-Codec",
		.id = 1,
		.cpu_dai_name = "SSP0 Pin",
		.codecs = modelsx_info2_ssp0_codec_components,
		.num_codecs = ARRAY_SIZE(modelsx_info2_ssp0_codec_components),
		.ops = &modelsx_info2_ssp0_ops,
		.dpcm_playback = 1,
		.no_pcm = 1,
	},
	{
		/* SSP1 - Microphone */
		.name = "SSP1-Codec",
		.id = 1,
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
		.id = 1,
		.cpu_dai_name = "SSP2 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.dpcm_capture = 1,
		.dpcm_playback = 1,
		.no_pcm = 1,
	},
	{
		/* SSP5 - Master amplifier */
		.name = "SSP5-Codec",
		.id = 1,
		.cpu_dai_name = "SSP5 Pin",
		.codecs = modelsx_info2_ssp5_codec_components,
		.num_codecs = ARRAY_SIZE(modelsx_info2_ssp5_codec_components),
		.ops = &modelsx_info2_ssp5_ops,
		.platform_name = "0000:00:0e.0",
		.dpcm_playback = 1,
		.no_pcm = 1,
	},
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
	},
};

static struct snd_soc_card modelsx_info2 = {
	.name = "modelsx_info2",
	.owner = THIS_MODULE,
	.dai_link = modelsx_info2_dais,
	.num_links = ARRAY_SIZE(modelsx_info2_dais),
	.dapm_widgets = modelsx_info2_widgets,
	.num_dapm_widgets = ARRAY_SIZE(modelsx_info2_widgets),
	.dapm_routes = modelsx_info2_map,
	.num_dapm_routes = ARRAY_SIZE(modelsx_info2_map),
	.fully_routed = true,
};

static int modelsx_info2_audio_probe(struct platform_device *pdev)
{
	modelsx_info2.dev = &pdev->dev;
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
		.name = "modelsx_info2",
	},
};

module_platform_driver(modelsx_info2_audio)

MODULE_DESCRIPTION("Intel SST Audio for Tesla Model SX Infotainment 2");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:modelsx_info2");
