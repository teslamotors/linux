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

#define DEF_BT_RATE_INDEX 0x0

static int dummy_codecs;
module_param(dummy_codecs, int, 0444);
MODULE_PARM_DESC(dummy_codecs, "Set all DAI link codecs to dummy");

struct bxtp_gpmrb_prv {
	int srate;
};

static struct snd_soc_dai_link broxton_gpmrb_dais[];

enum {
	BXT_AUDIO_SPEAKER = 0,
	BXT_AUDIO_EARLY,
	BXT_AUDIO_TUNER,
	BXT_AUDIO_AUX,
	BXT_AUDIO_MIC,
	BXT_AUDIO_DIRANA,
	BXT_AUDIO_TESTPIN,
	BXT_AUDIO_HFP1,
	BXT_AUDIO_HFP2,
	BXT_AUDIO_HDMI,
};


static unsigned int gpmrb_bt_rates[] = {
	8000,
	16000,
};

/* sound card controls */
static const char * const bt_rate[] = {"8K", "16K"};

static const struct soc_enum bt_rate_enum =
	SOC_ENUM_SINGLE_EXT(2, bt_rate);

static int bt_sample_rate_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct bxtp_gpmrb_prv *drv = snd_soc_card_get_drvdata(card);

	ucontrol->value.integer.value[0] = drv->srate;
	return 0;
}

static int bt_sample_rate_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct bxtp_gpmrb_prv *drv = snd_soc_card_get_drvdata(card);

	if (ucontrol->value.integer.value[0] == drv->srate)
		return 0;

	drv->srate = ucontrol->value.integer.value[0];
	return 0;

}
static const struct snd_kcontrol_new broxton_controls[] = {
	SOC_DAPM_PIN_SWITCH("Speaker"),
	SOC_ENUM_EXT("BT Rate", bt_rate_enum,
			bt_sample_rate_get, bt_sample_rate_put),
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
	SND_SOC_DAPM_SPK("Hfp1Pb", NULL),
	SND_SOC_DAPM_MIC("Hfp1Cp", NULL),
	SND_SOC_DAPM_SPK("Hfp2Pb", NULL),
	SND_SOC_DAPM_MIC("Hfp2Cp", NULL),
};

static const struct snd_soc_dapm_route broxton_gpmrb_map[] = {

	/* Speaker BE connections */
	{ "Speaker", NULL, "ssp4 Tx"},
	{ "ssp4 Tx", NULL, "codec0_out"},

	{ "tuner_in", NULL, "ssp2-b Rx"},
	{ "ssp2-b Rx", NULL, "DiranaTunerCp"},

	{ "aux_in", NULL, "ssp2-c Rx"},
	{ "ssp2-c Rx", NULL, "DiranaAuxCp"},

	{ "mic_in", NULL, "ssp2-d Rx"},
	{ "ssp2-d Rx", NULL, "DiranaMicCp"},

	{ "DiranaPb", NULL, "ssp2 Tx"},
	{ "ssp2 Tx", NULL, "dirana_out"},

	{ "hdmi_ssp1_in", NULL, "ssp1 Rx"},
	{ "ssp1 Rx", NULL, "HdmiIn"},

	{ "TestPin_ssp5_in", NULL, "ssp5 Rx"},
	{ "ssp5 Rx", NULL, "TestPinCp"},

	{ "TestPinPb", NULL, "ssp5 Tx"},
	{ "ssp5 Tx", NULL, "TestPin_ssp5_out"},

	{ "hfp1_in", NULL, "ssp0 Rx"},
	{ "ssp0 Rx", NULL, "Hfp1Cp"},

	{ "Hfp1Pb", NULL, "ssp0 Tx"},
	{ "ssp0 Tx", NULL, "hfp1_out"},

	{ "hfp2_in", NULL, "ssp0 Rx"},
	{ "ssp0 Rx", NULL, "Hfp2Cp"},

	{ "Hfp2Pb", NULL, "ssp0 Tx"},
	{ "ssp0 Tx", NULL, "hfp2_out"},
};

static int broxton_ssp0_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	struct snd_soc_card *card =  rtd->card;
	struct bxtp_gpmrb_prv *drv = snd_soc_card_get_drvdata(card);
	/* SSP0 operates with a BT Transceiver */
	rate->min = rate->max = gpmrb_bt_rates[drv->srate];

	return 0;
}

static int broxton_gpmrb_bt_startup(struct snd_pcm_substream *substream)
{
	int ret;

	ret = snd_pcm_hw_constraint_single(substream->runtime,
			SNDRV_PCM_HW_PARAM_CHANNELS, CHANNELS_MONO);

	if (ret < 0)
		goto out;


	ret = snd_pcm_hw_constraint_single(substream->runtime,
		SNDRV_PCM_HW_PARAM_RATE, 24000);

	if (ret < 0)
		goto out;
	ret = snd_pcm_hw_constraint_mask64(substream->runtime,
		SNDRV_PCM_HW_PARAM_FORMAT, SNDRV_PCM_FMTBIT_S16_LE);

	if (ret < 0)
		goto out;

out:
	return ret;
}

static struct snd_soc_ops broxton_gpmrb_bt_ops = {
	.startup = broxton_gpmrb_bt_startup,
};

static int broxton_gpmrb_dirana_startup(struct snd_pcm_substream *substream)
{
	int ret;
	char *stream_id = substream->pcm->id;
	const char *mic_id = broxton_gpmrb_dais[BXT_AUDIO_MIC].stream_name;

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
	[BXT_AUDIO_SPEAKER] = {
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
	[BXT_AUDIO_EARLY] = {
		.name = "Early Port",
		.stream_name = "Early",
		.cpu_dai_name = "System Pin 9",
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
	[BXT_AUDIO_TUNER] = {
		.name = "Dirana Tuner Port",
		.stream_name = "Dirana Tuner",
		.cpu_dai_name = "System Pin 3",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.init = NULL,
		.dpcm_capture = 1,
		.nonatomic = 1,
		.dynamic = 1,
		.ops = &broxton_gpmrb_dirana_ops,
	},
	[BXT_AUDIO_AUX] = {
		.name = "Dirana Aux Port",
		.stream_name = "Dirana Aux",
		.cpu_dai_name = "System Pin 4",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.init = NULL,
		.dpcm_capture = 1,
		.nonatomic = 1,
		.dynamic = 1,
		.ops = &broxton_gpmrb_dirana_ops,
	},
	[BXT_AUDIO_MIC] = {
		.name = "Dirana Mic Port",
		.stream_name = "Dirana Mic",
		.cpu_dai_name = "System Pin 5",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.init = NULL,
		.dpcm_capture = 1,
		.nonatomic = 1,
		.dynamic = 1,
		.ops = &broxton_gpmrb_dirana_ops,
	},
	[BXT_AUDIO_DIRANA] = {
		.name = "Dirana Port",
		.stream_name = "Dirana",
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
	[BXT_AUDIO_TESTPIN] = {
		.name = "TestPin Port",
		.stream_name = "TestPin",
		.cpu_dai_name = "System Pin 6",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.init = NULL,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.symmetric_rates = 1,
		.symmetric_channels = 1,
		.symmetric_samplebits = 1,
		.nonatomic = 1,
		.dynamic = 1,
	},
	[BXT_AUDIO_HFP1] = {
		.name = "HFP1 Port",
		.stream_name = "HFP1",
		.cpu_dai_name = "System Pin 7",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.init = NULL,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.symmetric_rates = 1,
		.symmetric_channels = 1,
		.symmetric_samplebits = 1,
		.nonatomic = 1,
		.dynamic = 1,
		.ops = &broxton_gpmrb_bt_ops,
	},
	[BXT_AUDIO_HFP2] = {
		.name = "HFP2 Port",
		.stream_name = "HFP2",
		.cpu_dai_name = "System Pin 8",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.init = NULL,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.symmetric_rates = 1,
		.symmetric_channels = 1,
		.symmetric_samplebits = 1,
		.nonatomic = 1,
		.dynamic = 1,
		.ops = &broxton_gpmrb_bt_ops,
	},
	[BXT_AUDIO_HDMI] = {
		.name = "HDMI Port",
		.stream_name = "HDMI",
		.cpu_dai_name = "HDMI2 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.init = NULL,
		.dpcm_capture = 1,
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
	},
	{
		.name = "Bxt Trace Buffer1",
		.stream_name = "Core 1 Trace Buffer",
		.cpu_dai_name = "TraceBuffer1 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.capture_only = true,
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
		.id = 1,
		.cpu_dai_name = "SSP0 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.dpcm_capture = 1,
		.dpcm_playback = 1,
		.no_pcm = 1,
		.be_hw_params_fixup = broxton_ssp0_fixup,
	},
	{
		/* SSP1 - HDMI-In */
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
		/* SSP2 - Dirana */
		.name = "SSP2-Codec",
		.id = 1,
		.cpu_dai_name = "SSP2 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.dpcm_playback = 1,
		.no_pcm = 1,
	},
	{
		/* SSP2-B - Dirana */
		.name = "SSP2-B-Codec",
		.id = 1,
		.cpu_dai_name = "SSP2-B Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.dpcm_capture = 1,
		.no_pcm = 1,
	},
	{
		/* SSP2-C - Dirana */
		.name = "SSP2-C-Codec",
		.id = 1,
		.cpu_dai_name = "SSP2-C Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.dpcm_capture = 1,
		.no_pcm = 1,
	},
	{
		/* SSP2-D - Dirana */
		.name = "SSP2-D-Codec",
		.id = 1,
		.cpu_dai_name = "SSP2-D Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.dpcm_capture = 1,
		.no_pcm = 1,
	},
	{
		/* SSP4 - Amplifier */
		.name = "SSP4-Codec",
		.id = 0,
		.cpu_dai_name = "SSP4 Pin",
		.codec_name = "i2c-INT34C3:00",
		.codec_dai_name = "tdf8532-hifi",
		.platform_name = "0000:00:0e.0",
		.dpcm_playback = 1,
		.no_pcm = 1,
	},
	{
		/* SSP5 - TestPin */
		.name = "SSP5-Codec",
		.id = 1,
		.cpu_dai_name = "SSP5 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
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

static void broxton_set_dummy_codecs(void)
{
	int i;

	for (i = 0; i < broxton_gpmrb.num_links; i++) {
		broxton_gpmrb.dai_link[i].codec_name = "snd-soc-dummy";
		broxton_gpmrb.dai_link[i].codec_dai_name = "snd-soc-dummy-dai";
	}

	dev_info(broxton_gpmrb.dev, "Codecs set to dummy\n");
}

static int broxton_audio_probe(struct platform_device *pdev)
{
	int ret_val;
	struct bxtp_gpmrb_prv *drv;

	broxton_gpmrb.dev = &pdev->dev;

	if (dummy_codecs)
		broxton_set_dummy_codecs();

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	drv->srate = DEF_BT_RATE_INDEX;
	snd_soc_card_set_drvdata(&broxton_gpmrb, drv);

	ret_val = snd_soc_register_card(&broxton_gpmrb);
	if (ret_val) {
		dev_dbg(&pdev->dev, "snd_soc_register_card failed %d\n",
								 ret_val);
		return ret_val;
	}

	platform_set_drvdata(pdev, &broxton_gpmrb);

	return ret_val;
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
		.pm = &snd_soc_pm_ops,
	},
};

module_platform_driver(broxton_audio)

/* Module information */
MODULE_DESCRIPTION("Intel SST Audio for Broxton GP MRB");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:gpmrb_machine");
