/*
 * Intel Broxton-P I2S ULL Machine Driver
 *
 * Copyright (C) 2015-2016, Intel Corporation. All rights reserved.
 *
 * Modified from:
 *   Intel Broxton I2S Machine driver
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
#include <sound/pcm_params.h>

/* widgets*/
static const struct snd_soc_dapm_widget widgets[] = {
	SND_SOC_DAPM_SPK("DummySpeaker1", NULL),
	SND_SOC_DAPM_SPK("DummySpeaker2", NULL),
	SND_SOC_DAPM_MIC("DummyMic1", NULL),
};

/* route map */
static const struct snd_soc_dapm_route map[] = {

	/* heartbeat capture */
	{"ssp0_in", NULL, "ssp0 Rx"},
	{"ssp0 Rx", NULL, "DummyMic1"},

	/* playback */
	{"DummySpeaker2", NULL, "ssp2 Tx"},
	{"ssp2 Tx", NULL, "ssp2_out"},

	/* DSP loop in */
	{"codec0_in", NULL, "ssp0-b Rx"},
	{"ssp0-b Rx", NULL, "Dummy Capture"},

	/* DSP loop out */
	{"DummySpeaker1", NULL, "Dummy Playback1"},
	{"Dummy Playback1", NULL, "ssp1 Tx"},
	{"ssp1 Tx", NULL, "ssp1_out"}
};

static const struct snd_soc_pcm_stream codec0_in_params = {
	.formats = SNDRV_PCM_FMTBIT_S32_LE,
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 1,
	.channels_max = 1,
};

static const struct snd_soc_pcm_stream codec0_out_params = {
	.formats = SNDRV_PCM_FMTBIT_S32_LE,
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 1,
	.channels_max = 1,
};

/* DAI (digital audio interface) glue- connect codec - CPU */
static struct snd_soc_dai_link dais[] = {
	/* front ends */
	{
		.name = "Playback",
		.stream_name = "ULL playback",
		.cpu_dai_name = "System Pin 2",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai2",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
	},
	{
		.name = "Capture (heartbeat)",
		.stream_name = "ULL capture (heartbeat)",
		.cpu_dai_name = "System Pin",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai3",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_capture = 1,
	},
	/* DSP loop */
	{
		.name = "Bxtn SSP0 Port",
		.stream_name = "Bxtn SSP0",
		.cpu_dai_name = "SSP0-B Pin",
		.platform_name = "0000:00:0e.0",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.params = &codec0_in_params,
		.dsp_loopback = 1,
	},
	{
		.name = "Bxtn SSP1 port",
		.stream_name = "Bxtn SSP1",
		.cpu_dai_name = "SSP1 Pin",
		.platform_name = "0000:00:0e.0",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai1",
		.params = &codec0_out_params,
		.dsp_loopback = 1,
	},
	/* back ends */
	{
		.name = "Playback BE",
		.be_id = 2,
		.cpu_dai_name = "SSP2 Pin",
		.platform_name = "0000:00:0e.0",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai2",
		.dai_fmt = SND_SOC_DAIFMT_I2S,
		.dpcm_playback = 1,
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
	},
	{
		.name = "Capture heartbeat BE",
		.be_id = 3,
		.cpu_dai_name = "SSP0 Pin",
		.platform_name = "0000:00:0e.0",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai3",
		.dai_fmt = SND_SOC_DAIFMT_I2S,
		.dpcm_capture = 1,
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
	},
	/* FDK probes */
	{
		.name = "Bxt Compress Probe playback",
		.stream_name = "Probe Playback",
		.cpu_dai_name = "Compress Probe0 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai4",
		.platform_name = "0000:00:0e.0",
		.init = NULL,
		.nonatomic = 1,
	},
	{
		.name = "Bxt Compress Probe capture",
		.stream_name = "Probe Capture",
		.cpu_dai_name = "Compress Probe1 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai4",
		.platform_name = "0000:00:0e.0",
		.init = NULL,
		.nonatomic = 1,
	}
};

/* pass all info to card */
static struct snd_soc_card audio_card = {
	.name = "Audio card",
	.owner = THIS_MODULE,
	.dai_link = dais,
	.num_links = ARRAY_SIZE(dais),
	.dapm_widgets = widgets,
	.num_dapm_widgets = ARRAY_SIZE(widgets),
	.dapm_routes = map,
	.num_dapm_routes = ARRAY_SIZE(map),
	.fully_routed = false,
};

/* register card */
static int broxton_audio_probe(struct platform_device *pdev)
{
	audio_card.dev = &pdev->dev;
	return snd_soc_register_card(&audio_card);
}

static int broxton_audio_remove(struct platform_device *pdev)
{
	snd_soc_unregister_card(&audio_card);
	return 0;
}

static struct platform_driver broxton_audio = {
	.probe = broxton_audio_probe,
	.remove = broxton_audio_remove,
	.driver = {
		.name = "bxt_ull",
		.pm = &snd_soc_pm_ops,
	},
};

module_platform_driver(broxton_audio);

MODULE_AUTHOR("Jakub Dorzak <jakubx.dorzak@intel.com>");
MODULE_DESCRIPTION("Intel SST Audio for Broxton ULL");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:bxt_ull");
