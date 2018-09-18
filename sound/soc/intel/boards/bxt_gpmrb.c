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
#include <linux/i2c.h>
#include <sound/pcm_params.h>

#define CHANNELS_MONO 1
#define CHANNELS_STEREO 2
#define CHANNELS_QUAD 4
#define CHANNELS_EIGHT 8

static struct snd_soc_dai_link broxton_gpmrb_dais[];

enum {
	BXT_AUDIO_SPEAKER_PB = 0,
	BXT_AUDIO_TUNER_CP,
	BXT_AUDIO_AUX_CP,
	BXT_AUDIO_MIC_CP,
	BXT_AUDIO_DIRANA_PB,
	BXT_AUDIO_TESTPIN_PB,
	BXT_AUDIO_TESTPIN_CP,
	BXT_AUDIO_BT_CP,
	BXT_AUDIO_BT_PB,
	BXT_AUDIO_MODEM_CP,
	BXT_AUDIO_MODEM_PB,
	BXT_AUDIO_HDMI_CP,
};

static const struct snd_kcontrol_new broxton_controls[] = {
	SOC_DAPM_PIN_SWITCH("Speaker"),
};

static const struct snd_soc_dapm_widget broxton_widgets[] = {
	SND_SOC_DAPM_SPK("Speaker", NULL),
	SND_SOC_DAPM_MIC("DiranaTunerCp", NULL),
	SND_SOC_DAPM_MIC("DiranaAuxCp", NULL),
	SND_SOC_DAPM_MIC("DiranaMicCp", NULL),
	SND_SOC_DAPM_HP("DiranaPb", NULL),
	SND_SOC_DAPM_MIC("HdmiIn", NULL),
	SND_SOC_DAPM_MIC("TestPinCp", NULL),
	SND_SOC_DAPM_HP("TestPinPb", NULL),
	SND_SOC_DAPM_MIC("BtHfpDl", NULL),
	SND_SOC_DAPM_HP("BtHfpUl", NULL),
	SND_SOC_DAPM_MIC("ModemDl", NULL),
	SND_SOC_DAPM_HP("ModemUl", NULL),
};

static const struct snd_soc_dapm_route broxton_gpmrb_map[] = {

	/* Speaker BE connections */
	{ "Speaker", NULL, "ssp4 Tx"},
	{ "ssp4 Tx", NULL, "codec0_out"},

	{ "tuner_in", NULL, "ssp2 Rx"},
	{ "ssp2 Rx", NULL, "DiranaTunerCp"},

	{ "aux_in", NULL, "ssp2 Rx"},
	{ "ssp2 Rx", NULL, "DiranaAuxCp"},

	{ "mic_in", NULL, "ssp2 Rx"},
	{ "ssp2 Rx", NULL, "DiranaMicCp"},

	{ "DiranaPb", NULL, "ssp2 Tx"},
	{ "ssp2 Tx", NULL, "dirana_out"},

	{ "hdmi_ssp1_in", NULL, "ssp1 Rx"},
	{ "ssp1 Rx", NULL, "HdmiIn"},

	{ "TestPin_ssp5_in", NULL, "ssp5 Rx"},
	{ "ssp5 Rx", NULL, "TestPinCp"},

	{ "TestPinPb", NULL, "ssp5 Tx"},
	{ "ssp5 Tx", NULL, "TestPin_ssp5_out"},

	{ "BtHfp_ssp0_in", NULL, "ssp0 Rx"},
	{ "ssp0 Rx", NULL, "BtHfpDl"},

	{ "BtHfpUl", NULL, "ssp0 Tx"},
	{ "ssp0 Tx", NULL, "BtHfp_ssp0_out"},

	{ "Modem_ssp3_in", NULL, "ssp3 Rx"},
	{ "ssp3 Rx", NULL, "ModemDl"},

	{ "ModemUl", NULL, "ssp3 Tx"},
	{ "ssp3 Tx", NULL, "Modem_ssp3_out"},
};

static unsigned int bt_rates[] = { 8000, 16000 };

static struct snd_pcm_hw_constraint_list constraints_bt_rates = {
	.count = ARRAY_SIZE(bt_rates),
	.list = bt_rates,
	.mask = 0,
};

static int broxton_gpmrb_bt_modem_startup(struct snd_pcm_substream *substream)
{
	int ret;

	ret = snd_pcm_hw_constraint_single(substream->runtime,
			SNDRV_PCM_HW_PARAM_CHANNELS, CHANNELS_MONO);

	if (ret < 0)
		goto out;

	ret = snd_pcm_hw_constraint_list(substream->runtime, 0,
			SNDRV_PCM_HW_PARAM_RATE, &constraints_bt_rates);

	if (ret < 0)
		goto out;

	ret = snd_pcm_hw_constraint_mask64(substream->runtime,
			SNDRV_PCM_HW_PARAM_FORMAT, SNDRV_PCM_FMTBIT_S32_LE);

	if (ret < 0)
		goto out;

out:
	return ret;
}

static struct snd_soc_ops broxton_gpmrb_bt_modem_ops = {
	.startup = broxton_gpmrb_bt_modem_startup,
};

static int broxton_gpmrb_dirana_startup(struct snd_pcm_substream *substream)
{
	int ret;
	char *stream_id = substream->pcm->id;
	const char *mic_id = broxton_gpmrb_dais[BXT_AUDIO_MIC_CP].stream_name;

	if (strncmp(stream_id, mic_id, strlen(mic_id)) == 0)
		ret = snd_pcm_hw_constraint_single(substream->runtime,
				SNDRV_PCM_HW_PARAM_CHANNELS, CHANNELS_MONO);
	else
		ret = snd_pcm_hw_constraint_single(substream->runtime,
				SNDRV_PCM_HW_PARAM_CHANNELS, CHANNELS_STEREO);

	if (ret < 0)
		goto out;

	ret = snd_pcm_hw_constraint_mask64(substream->runtime,
			SNDRV_PCM_HW_PARAM_FORMAT, SNDRV_PCM_FMTBIT_S32_LE);

	if (ret < 0)
		goto out;

	ret = snd_pcm_hw_constraint_single(substream->runtime,
			SNDRV_PCM_HW_PARAM_RATE, 48000);


out:
	return 0;
}

static struct snd_soc_ops broxton_gpmrb_dirana_ops = {
	.startup = broxton_gpmrb_dirana_startup,
};

static int broxton_gpmrb_hdmi_startup(struct snd_pcm_substream *substream)
{
	int ret;

	ret = snd_pcm_hw_constraint_minmax(substream->runtime,
			SNDRV_PCM_HW_PARAM_CHANNELS, CHANNELS_STEREO,
			CHANNELS_EIGHT);

	if (ret < 0)
		goto out;

	ret = snd_pcm_hw_constraint_single(substream->runtime,
			SNDRV_PCM_HW_PARAM_RATE, 48000);

	if (ret < 0)
		goto out;

	ret = snd_pcm_hw_constraint_mask64(substream->runtime,
			SNDRV_PCM_HW_PARAM_FORMAT, SNDRV_PCM_FMTBIT_S32_LE);

	if (ret < 0)
		goto out;

out:
	return ret;
}

static struct snd_soc_ops broxton_gpmrb_hdmi_ops = {
	.startup = broxton_gpmrb_hdmi_startup,
};

static int broxton_gpmrb_tdf8532_startup(struct snd_pcm_substream *substream)
{
	int ret;

	ret = snd_pcm_hw_constraint_single(substream->runtime,
			SNDRV_PCM_HW_PARAM_CHANNELS, CHANNELS_QUAD);

	if (ret < 0)
		goto out;

	ret = snd_pcm_hw_constraint_single(substream->runtime,
			SNDRV_PCM_HW_PARAM_RATE, 48000);

	if (ret < 0)
		goto out;

	ret = snd_pcm_hw_constraint_mask64(substream->runtime,
			SNDRV_PCM_HW_PARAM_FORMAT, SNDRV_PCM_FMTBIT_S32_LE);

	if (ret < 0)
		goto out;

out:
	return ret;
}

static struct snd_soc_ops broxton_gpmrb_tdf8532_ops = {
	.startup = broxton_gpmrb_tdf8532_startup,
};
/* broxton digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link broxton_gpmrb_dais[] = {
	/* Front End DAI links */
	[BXT_AUDIO_SPEAKER_PB] = {
		.name = "Speaker Port",
		.stream_name = "Speaker",
		.cpu_dai_name = "System Pin 2",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
		.ops = &broxton_gpmrb_tdf8532_ops,
	},
	[BXT_AUDIO_TUNER_CP] = {
		.name = "Dirana Tuner Cp Port",
		.stream_name = "Dirana Tuner Cp",
		.cpu_dai_name = "System Pin 3",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.init = NULL,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.nonatomic = 1,
		.dynamic = 1,
		.ops = &broxton_gpmrb_dirana_ops,
	},
	[BXT_AUDIO_AUX_CP] = {
		.name = "Dirana Aux Cp Port",
		.stream_name = "Dirana Aux Cp",
		.cpu_dai_name = "System Pin 4",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.init = NULL,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.nonatomic = 1,
		.dynamic = 1,
		.ops = &broxton_gpmrb_dirana_ops,
	},
	[BXT_AUDIO_MIC_CP] = {
		.name = "Dirana Mic Cp Port",
		.stream_name = "Dirana Mic Cp",
		.cpu_dai_name = "System Pin 5",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.init = NULL,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.nonatomic = 1,
		.dynamic = 1,
		.ops = &broxton_gpmrb_dirana_ops,
	},
	[BXT_AUDIO_DIRANA_PB] = {
		.name = "Dirana Pb Port",
		.stream_name = "Dirana Pb",
		.cpu_dai_name = "System Pin",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
		.ops = &broxton_gpmrb_dirana_ops,
	},
	[BXT_AUDIO_TESTPIN_PB] = {
		.name = "TestPin Cp Port",
		.stream_name = "TestPin Cp",
		.cpu_dai_name = "System Pin 6",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.init = NULL,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.nonatomic = 1,
		.dynamic = 1,
	},
	[BXT_AUDIO_TESTPIN_CP] = {
		.name = "TestPin Pb Port",
		.stream_name = "TestPin Pb",
		.cpu_dai_name = "System Pin 6",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
	},
	[BXT_AUDIO_BT_CP] = {
		.name = "BtHfp Cp Port",
		.stream_name = "BtHfp Cp",
		.cpu_dai_name = "System Pin 7",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.init = NULL,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.nonatomic = 1,
		.dynamic = 1,
		.ops = &broxton_gpmrb_bt_modem_ops,
	},
	[BXT_AUDIO_BT_PB] = {
		.name = "BtHfp Pb Port",
		.stream_name = "BtHfp Pb",
		.cpu_dai_name = "System Pin 7",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
		.ops = &broxton_gpmrb_bt_modem_ops,
	},
	[BXT_AUDIO_MODEM_CP] = {
		.name = "Modem Cp Port",
		.stream_name = "Modem Cp",
		.cpu_dai_name = "System Pin 8",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.init = NULL,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.nonatomic = 1,
		.dynamic = 1,
		.ops = &broxton_gpmrb_bt_modem_ops,
	},
	[BXT_AUDIO_MODEM_PB] = {
		.name = "Modem Pb Port",
		.stream_name = "Modem Pb",
		.cpu_dai_name = "System Pin 8",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
		.ops = &broxton_gpmrb_bt_modem_ops,
	},
	[BXT_AUDIO_HDMI_CP] = {
		.name = "HDMI Cp Port",
		.stream_name = "HDMI Cp",
		.cpu_dai_name = "HDMI2 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.init = NULL,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.nonatomic = 1,
		.dynamic = 1,
		.ops = &broxton_gpmrb_hdmi_ops,
	},
	/* Trace Buffer & Probing DAI links */
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
	/* Back End DAI links */
	{
		/* SSP0 - BT */
		.name = "SSP0-Codec",
		.be_id = 1,
		.cpu_dai_name = "SSP0 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.ignore_suspend = 1,
		.dpcm_capture = 1,
		.dpcm_playback = 1,
		.no_pcm = 1,
	},
	{
		/* SSP1 - HDMI-In */
		.name = "SSP1-Codec",
		.be_id = 1,
		.cpu_dai_name = "SSP1 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.ignore_suspend = 1,
		.dpcm_capture = 1,
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
	{
		/* SSP3 - Modem */
		.name = "SSP3-Codec",
		.be_id = 1,
		.cpu_dai_name = "SSP3 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.ignore_suspend = 1,
		.dpcm_capture = 1,
		.dpcm_playback = 1,
		.no_pcm = 1,
	},
	{
		/* SSP4 - Amplifier */
		.name = "SSP4-Codec",
		.be_id = 0,
		.cpu_dai_name = "SSP4 Pin",
		.codec_name = "i2c-INT34C3:00",
		.codec_dai_name = "tdf8532-hifi",
		.platform_name = "0000:00:0e.0",
		.ignore_suspend = 1,
		.dpcm_playback = 1,
		.no_pcm = 1,
	},
	{
		/* SSP5 - TestPin */
		.name = "SSP5-Codec",
		.be_id = 1,
		.cpu_dai_name = "SSP5 Pin",
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
