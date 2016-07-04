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
#include "../../codecs/tlv320aic3x.h"

static int apli_lfcrb_aic3107S_startup(struct snd_pcm_substream *substream)
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

static struct snd_soc_ops apli_lfcrb_aic3107S_ops = {
	.startup = apli_lfcrb_aic3107S_startup,
};

static const struct snd_kcontrol_new apli_controls[] = {
	SOC_DAPM_PIN_SWITCH("SSP1 Speaker"),
	SOC_DAPM_PIN_SWITCH("SSP1 Mic"),
	SOC_DAPM_PIN_SWITCH("SSP2 Speaker"),
	SOC_DAPM_PIN_SWITCH("SSP2 Mic"),
	SOC_DAPM_PIN_SWITCH("SSP4 Speaker"),
	SOC_DAPM_PIN_SWITCH("SSP4 Mic"),
	SOC_DAPM_PIN_SWITCH("Headphone Jack"),
	SOC_DAPM_PIN_SWITCH("Mic Jack"),
};

static const struct snd_soc_dapm_widget apli_widgets[] = {
	SND_SOC_DAPM_SPK("SSP1 Speaker", NULL),
	SND_SOC_DAPM_MIC("SSP1 Mic", NULL),
	SND_SOC_DAPM_SPK("SSP2 Speaker", NULL),
	SND_SOC_DAPM_MIC("SSP2 Mic", NULL),
	SND_SOC_DAPM_SPK("SSP4 Speaker", NULL),
	SND_SOC_DAPM_MIC("SSP4 Mic", NULL),
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
	SND_SOC_DAPM_LINE("Line In", NULL),
	SND_SOC_DAPM_LINE("Line Out", NULL),
};

static const struct snd_soc_dapm_route apli_lhcrb_aic3107_map[] = {
	/* HP Jack */
	{"Headphone Jack", NULL, "HPLOUT"},
	{"Headphone Jack", NULL, "HPROUT"},
	{"HPROUT", NULL, "Playback"},
	{"HPLOUT", NULL, "Playback"},

	{"Line Out", NULL, "LLOUT"},
	{"Line Out", NULL, "RLOUT"},

	{"SSP1 Speaker", NULL, "SPOP"},
	{"SPOP", NULL, "Playback"},
	{"SSP1 Speaker", NULL, "SPOM"},
	{"SPOM", NULL, "Playback"},

	/* Mic Jack */
	{"MIC3L", NULL, "Mic Jack"},
	{"MIC3R", NULL, "Mic Jack"},
	{"Mic Jack", NULL, "Mic Bias"},

	{"LINE1L", NULL, "Line In"},
	{"LINE1R", NULL, "Line In"},

	{"LINE2L", NULL, "Line In"},
	{"LINE2R", NULL, "Line In"},

	/* Codec BE connections */
	/* SSP1 follows Hardware pin naming */
	{"SSP1 Speaker", NULL, "ssp0 Tx"},
	{"ssp0 Tx", NULL, "codec2_out"},

	{"codec2_in", NULL, "ssp0 Rx"},
	{"ssp0 Rx", NULL, "SSP1 Mic"},

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

static int tlv320aic3107_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
	struct device_node *np = card->dev->of_node;
	int ret;

	/* Add specific widgets */
	snd_soc_dapm_new_controls(&card->dapm, apli_widgets,
				  ARRAY_SIZE(apli_widgets));

	if (np) {
		ret = snd_soc_of_parse_audio_routing(card, "ti,audio-routing");
		if (ret)
			return ret;
	} else {
		/* Set up specific audio path apli_lhcrb_aic3107_map */
		snd_soc_dapm_add_routes(&card->dapm, apli_lhcrb_aic3107_map,
					ARRAY_SIZE(apli_lhcrb_aic3107_map));
	}

	return 0;
}

static int apli_aic3107_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret = 0;
	unsigned int sysclk = 0;

	sysclk = 24576000;

	/* set the codec system clock */
	ret = snd_soc_dai_set_sysclk(codec_dai, 0, sysclk, SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(rtd->dev, "snd_soc_dai_set_sysclk err = %d\n", ret);

	return ret;
}

static struct snd_soc_ops apli_aic3107_ops = {
	.hw_params = apli_aic3107_hw_params,
};

static int aic3107_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			      struct snd_pcm_hw_params *params)
{
	struct snd_mask *fmt = hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT);

	/* set back end format to 32 bit */
	snd_mask_none(fmt);
	snd_mask_set(fmt, SNDRV_PCM_FORMAT_S32_LE);
	return 0;
}

/* apli digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link apli_lhcrb_aic3107_dais[] = {
	/* Front End DAI links */
	{
		.name = "SSP1 Playback Port",
		.stream_name = "AIC3107 Playback",
		.cpu_dai_name = "System Pin 4",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
		.ops = &apli_lfcrb_aic3107S_ops,
	},
	{
		.name = "SSP1 Capture Port",
		.stream_name = "AIC3107 Capture",
		.cpu_dai_name = "System Pin 5",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.init = NULL,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.nonatomic = 1,
		.dynamic = 1,
		.ops = &apli_lfcrb_aic3107S_ops,
	},
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
		.ops = &apli_lfcrb_aic3107S_ops,
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
		.ignore_suspend = 1,
		.nonatomic = 1,
		.dynamic = 1,
		.ops = &apli_lfcrb_aic3107S_ops,
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
		.ops = &apli_lfcrb_aic3107S_ops,
	},
	{
		.name = "SSP4 Capture Port",
		.stream_name = "SSP4 Mic",
		.cpu_dai_name = "Reference Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.init = NULL,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.nonatomic = 1,
		.dynamic = 1,
		.ops = &apli_lfcrb_aic3107S_ops,
	},
	/* Back End DAI links */
	{
		/* SSP0 - Codec */
		.name = "SSP0-Codec",
		.be_id = 0,
		.cpu_dai_name = "SSP0 Pin",
		.codec_name = "tlv320aic3x-codec.3-0018",
		.codec_dai_name = "tlv320aic3x-hifi",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.ops = &apli_aic3107_ops,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBM_CFM,
		.platform_name = "0000:00:0e.0",
		.ignore_suspend = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.no_pcm = 1,
		.be_hw_params_fixup = aic3107_be_hw_params_fixup,
	},
	{
		/* SSP1 - Codec */
		.name = "SSP1-Codec",
		.be_id = 1,
		.cpu_dai_name = "SSP1 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
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
		.be_id = 2,
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

/* apli audio machine driver for aic3107 Proto Board*/
static struct snd_soc_card apli_lhcrb_aic3107 = {
	.name = "apli-lhcrb-aic3107_i2s",
	.owner = THIS_MODULE,
	.dai_link = apli_lhcrb_aic3107_dais,
	.num_links = ARRAY_SIZE(apli_lhcrb_aic3107_dais),
	.controls = apli_controls,
	.num_controls = ARRAY_SIZE(apli_controls),
	.dapm_widgets = apli_widgets,
	.num_dapm_widgets = ARRAY_SIZE(apli_widgets),
	.dapm_routes = apli_lhcrb_aic3107_map,
	.num_dapm_routes = ARRAY_SIZE(apli_lhcrb_aic3107_map),
	.fully_routed = true,
};

/* Northwest - GPIO 74 */
#define I2S_1_BASE 0xD0C40000
#define I2S_1 0x610
#define I2S_1_VALUE 0x44000400
static int apli_audio_probe(struct platform_device *pdev)
{
	char *gpio_addr;
	u32 gpio_value;

	gpio_addr = (void *)ioremap_nocache(I2S_1_BASE + I2S_1, 0x30);
	gpio_value = I2S_1_VALUE;

	if (gpio_addr == NULL)
		return -EIO;

	pr_info("%p has %#x\n", gpio_addr, *(u32 *)gpio_addr);

	memcpy_toio(gpio_addr, &gpio_value, sizeof(gpio_value));
	memcpy_toio(gpio_addr + 0x8, &gpio_value, sizeof(gpio_value));
	memcpy_toio(gpio_addr + 0x10, &gpio_value, sizeof(gpio_value));
	memcpy_toio(gpio_addr + 0x18, &gpio_value, sizeof(gpio_value));
	memcpy_toio(gpio_addr + 0x20, &gpio_value, sizeof(gpio_value));

	pr_info("%p has %#x\n", gpio_addr, *(u32 *)gpio_addr);
	pr_info("%p has %#x\n", gpio_addr + 0x8, *(u32 *)(gpio_addr + 0x8));
	pr_info("%p has %#x\n", gpio_addr + 0x10, *(u32 *)(gpio_addr + 0x10));
	pr_info("%p has %#x\n", gpio_addr + 0x18, *(u32 *)(gpio_addr + 0x18));
	pr_info("%p has %#x\n", gpio_addr + 0x20, *(u32 *)(gpio_addr + 0x18));

	iounmap(gpio_addr);
	apli_lhcrb_aic3107.dev = &pdev->dev;
	return snd_soc_register_card(&apli_lhcrb_aic3107);
}

static int apli_audio_remove(struct platform_device *pdev)
{
	snd_soc_unregister_card(&apli_lhcrb_aic3107);
	return 0;
}

static struct platform_driver apli_audio = {
	.probe = apli_audio_probe,
	.remove = apli_audio_remove,
	.driver = {
		.name = "lfcrb_aic3107S_i2s",
	},
};

module_platform_driver(apli_audio)

/* Module information */
MODULE_DESCRIPTION("Intel Audio aic3107 Machine driver for APL-I LH CRB");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:lfcrb_aic3107S_i2s");
