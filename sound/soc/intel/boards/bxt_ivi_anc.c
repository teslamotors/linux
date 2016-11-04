/*
 * Intel Broxton-P I2S ULL Machine Driver
 *
 * Copyright (C) 2017, Intel Corporation. All rights reserved.
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

static const struct snd_soc_pcm_stream media1_out_params = {
	.formats = SNDRV_PCM_FMTBIT_S32_LE,
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 3,
	.channels_max = 3,
};

static const struct snd_soc_pcm_stream codec1_in_params = {
	.formats = SNDRV_PCM_FMTBIT_S32_LE,
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 6,
	.channels_max = 6,
};

static const struct snd_soc_pcm_stream codec0_in_params = {
	.formats = SNDRV_PCM_FMTBIT_S32_LE,
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 1,
	.channels_max = 1,
};

static const struct snd_soc_dapm_widget broxton_widgets[] = {
	SND_SOC_DAPM_SPK("DummySpeaker1", NULL),
	SND_SOC_DAPM_SPK("DummySpeaker2", NULL),
	SND_SOC_DAPM_SPK("DummySpeaker3", NULL),
	SND_SOC_DAPM_SPK("DummySpeaker4", NULL),
	SND_SOC_DAPM_MIC("DummyMIC0", NULL),
	SND_SOC_DAPM_MIC("DummyMIC2", NULL),
	SND_SOC_DAPM_MIC("DummyMIC4", NULL),
};

static const struct snd_soc_dapm_route bxtp_ull_map[] = {
	{"8ch_pt_in3", NULL, "ssp0 Rx" },
	{"ssp0 Rx", NULL, "Dummy Capture" },
	{"Dummy Capture", NULL, "DummyMIC0"},

	{"DummySpeaker2", NULL, "Dummy Playback2"},
	{"Dummy Playback2", NULL, "ssp2 Tx"},
	{"ssp2 Tx", NULL, "8ch_pt_out2"},

	{"DummySpeaker1", NULL, "Dummy Playback1"},
	{"Dummy Playback1", NULL, "ssp1 Tx"},
	{"ssp1 Tx", NULL, "8ch_pt_out3"},

	{"8ch_pt_in2", NULL, "ssp2 Rx" },
	{"ssp2 Rx", NULL, "Dummy Capture2" },
	{"Dummy Capture2", NULL, "DummyMIC2"},

	{"DummySpeaker4", NULL, "Dummy Playback4"},
	{"Dummy Playback4", NULL, "ssp4 Tx"},
	{"ssp4 Tx", NULL, "8ch_pt_out"},

	{"8ch_pt_in", NULL, "ssp4 Rx" },
	{"ssp4 Rx", NULL, "Dummy Capture4" },
	{"Dummy Capture4", NULL, "DummyMIC4"},

	/* (ANC) Codec1_in - Loop pipe */
	{ "codec1_in", NULL, "ssp0-b Rx" },
	{ "ssp0-b Rx", NULL, "Dummy Capture" },

	/* Codec0_in - Loop pipe */
	{ "codec0_in", NULL, "ssp2-b Rx" },
	{ "ssp2-b Rx", NULL, "Dummy Capture2" },

	/* Media1_out Loop Path */
	{"DummySpeaker3", NULL, "Dummy Playback3"},
	{ "Dummy Playback3", NULL, "ssp1-b Tx"},
	{ "ssp1-b Tx", NULL, "media1_out"},
};

/* broxton digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link bxtp_ull_dais[] = {
	{
		.name = "Bxt Audio Port 3",
		.stream_name = "Stereo-16K SSP4",
		.cpu_dai_name = "System Pin 3",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "Bxt Audio Port 4",
		.stream_name = "5-ch SSP1",
		.cpu_dai_name = "System Pin 4",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
	},
	{
		.name = "Bxt Audio Port 5",
		.stream_name = "SSP2 Stream",
		.cpu_dai_name = "System Pin 5",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "Bxt Audio Port 6",
		.stream_name = "8-Ch SSP0",
		.cpu_dai_name = "System Pin 6",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_capture = 1,
	},
	/* Probe DAI Links */
	{
		.name = "Bxt Compress Probe playback",
		.stream_name = "Probe Playback",
		.cpu_dai_name = "Compress Probe0 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.init = NULL,
		.nonatomic = 1,
	},
	{
		.name = "Bxt Compress Probe capture",
		.stream_name = "Probe Capture",
		.cpu_dai_name = "Compress Probe1 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.init = NULL,
		.nonatomic = 1,
	},
	/* Trace Buffer DAI links */
	{
		.name = "Bxt Trace Buffer0",
		.stream_name = "Core 0 Trace Buffer",
		.cpu_dai_name = "TraceBuffer0 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.capture_only = true,
		.ignore_suspend = 1,
	},
	{
		.name = "Bxt Trace Buffer1",
		.stream_name = "Core 1 Trace Buffer",
		.cpu_dai_name = "TraceBuffer1 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.capture_only = true,
		.ignore_suspend = 1,
	},
		/* CODEC<->CODEC link */
	{
		.name = "Bxtn SSP0 Port",
		.stream_name = "Bxtn SSP0",
		.cpu_dai_name = "SSP0-B Pin",
		.platform_name = "0000:00:0e.0",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.params = &codec1_in_params,
		.dsp_loopback = true,
	},

	{
		.name = "Bxtn SSP2 port",
		.stream_name = "Bxtn SSP2",
		.cpu_dai_name = "SSP2-B Pin",
		.platform_name = "0000:00:0e.0",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.params = &codec0_in_params,
		.dsp_loopback = true,
	},

	{
		.name = "Bxtn SSP1 port",
		.stream_name = "Bxtn SSP2",
		.cpu_dai_name = "SSP1-B Pin",
		.platform_name = "0000:00:0e.0",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.params = &media1_out_params,
		.dsp_loopback = true,
	},

	/* Back End DAI links */
	{
		/* SSP4 - Codec */
		.name = "SSP4-Codec",
		.cpu_dai_name = "SSP4 Pin",
		.platform_name = "0000:00:0e.0",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		/* SSP1 - Codec */
		.name = "SSP1-Codec",
		.cpu_dai_name = "SSP1 Pin",
		.platform_name = "0000:00:0e.0",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.dpcm_playback = 1,
	},
	{
		/* SSP2 - Codec */
		.name = "SSP2-Codec",
		.cpu_dai_name = "SSP2 Pin",
		.platform_name = "0000:00:0e.0",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		/* SSP0 - Codec */
		.name = "SSP0-Codec",
		.cpu_dai_name = "SSP0 Pin",
		.platform_name = "0000:00:0e.0",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.dpcm_capture = 1,
	},
};

static int bxt_add_dai_link(struct snd_soc_card *card,
			struct snd_soc_dai_link *link)
{
	link->platform_name = "0000:00:0e.0";
	link->nonatomic = 1;
	return 0;
}

/* broxton audio machine driver for ULL Dummy Codec*/
static struct snd_soc_card bxtp_ull = {
	.name = "bxtp-ull",
	.owner = THIS_MODULE,
	.dai_link = bxtp_ull_dais,
	.num_links = ARRAY_SIZE(bxtp_ull_dais),
	.dapm_widgets = broxton_widgets,
	.num_dapm_widgets = ARRAY_SIZE(broxton_widgets),
	.dapm_routes = bxtp_ull_map,
	.num_dapm_routes = ARRAY_SIZE(bxtp_ull_map),
	.fully_routed = false,
	.add_dai_link = bxt_add_dai_link,
};

static int broxton_audio_probe(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s registering %s\n", __func__, pdev->name);
	bxtp_ull.dev = &pdev->dev;
	return snd_soc_register_card(&bxtp_ull);
}

static int broxton_audio_remove(struct platform_device *pdev)
{
	snd_soc_unregister_card(&bxtp_ull);
	return 0;
}

static struct platform_driver broxton_audio = {
	.probe = broxton_audio_probe,
	.remove = broxton_audio_remove,
	.driver = {
		.name = "bxt_ivi_ull",
		.pm = &snd_soc_pm_ops,
	},
};

module_platform_driver(broxton_audio)

/* Module information */
MODULE_DESCRIPTION("Intel SST Audio for Broxton ULL Machine");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:bxtp_i2s_ull");
