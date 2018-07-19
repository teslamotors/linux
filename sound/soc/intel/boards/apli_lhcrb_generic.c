/*
 * Intel Apli(Apollo Lake) I2S Machine Driver for
 * LF (Leaf Hill) reference platform
 *
 * Copyright (C) 2017, Intel Corporation. All rights reserved.
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
#include <linux/init.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <linux/gpio.h>
#include <sound/pcm_params.h>

#define NORTHWEST_BASE 0xD0C40000
#define NORTHWEST_GPIO_VALUE 0x44000400

#define WEST_BASE 0xd0c70000
#define WEST_GPIO_VALUE 0x44000800

/* NORTHWEST - GPIO 75 */
#define I2S_1 0x618
/* NORTHWEST - GPIO 89 */
#define I2S_3 0x688
/* WEST - GPIO 150*/
#define I2S_5 0x5A0
/* WEST - GPIO 146*/
#define I2S_6 0x580

/**
 *  apli_lhcrb_dummy_startup - machine stream startup operations
 *
 *  apli_lhcrb_dummy_startup is the called during the every playback/
 *  capture activity. All it does is to configure the mulitpile
 *  sample rate,channel number and bit rates (Audio formate).
 **/
static int apli_lhcrb_dummy_startup(struct snd_pcm_substream *substream)
{
	int ret;

	static unsigned int rates[] = {
		8000,
		16000,
		22050,
		24000,
		32000,
		44100,
		48000,
		96000,
		192000,
	};

	static unsigned int channels[] = {
		1,
		2,
		4,
		6,
		8,
	};

	static u64 formats = SNDRV_PCM_FMTBIT_S16_LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE;

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

static struct snd_soc_ops apli_lhcrb_dummy_ops = {
	.startup = apli_lhcrb_dummy_startup,
};

/**
 *  apli_ssp_northwest_gpio_init - codec/machine specific init
 *
 *  apli_ssp_northwest_gpio_init is the first routine called when
 *  the driver is loaded. All it does is to initialize the machine
 *  specific init (NORTHWEST Registers) like machine controls for SSP.
 **/
static int apli_ssp_northwest_gpio_init(struct snd_soc_pcm_runtime *rtd)
{
	char *gpio_addr = NULL;
	u32 gpio_value = NORTHWEST_GPIO_VALUE;

	switch (rtd->dai_link->id) {
	case 0:
		gpio_addr = (void *)ioremap_nocache(NORTHWEST_BASE
				+ I2S_1, 0x20);
		break;
	case 2:
		gpio_addr = (void *)ioremap_nocache(NORTHWEST_BASE
				+ I2S_3, 0x20);
		break;
	default:
		return -EINVAL;
	}

	if (gpio_addr == NULL)
		return -EIO;

	pr_info("%p has %#x\n", gpio_addr, *(u32 *)gpio_addr);

	memcpy_toio(gpio_addr, &gpio_value, sizeof(gpio_value));
	memcpy_toio(gpio_addr + 0x8, &gpio_value, sizeof(gpio_value));
	memcpy_toio(gpio_addr + 0x10, &gpio_value, sizeof(gpio_value));
	memcpy_toio(gpio_addr + 0x18, &gpio_value, sizeof(gpio_value));

	iounmap(gpio_addr);
	return 0;
}

/**
 *  apli_ssp_west_gpio_init - codec/machine specific init
 *
 *  apli_ssp_west_gpio_init is the first routine called when
 *  the driver is loaded. All it does is to initialize the machine
 *  specific init (WEST Registers) like machine controls for SSP.
 **/
static int apli_ssp_west_gpio_init(struct snd_soc_pcm_runtime *rtd)
{
	char *gpio_addr = NULL;
	u32 gpio_value = WEST_GPIO_VALUE;

	switch (rtd->dai_link->id) {
	case 4:
		gpio_addr = (void *)ioremap_nocache(WEST_BASE + I2S_5, 0x20);
		break;
	case 5:
		gpio_addr = (void *)ioremap_nocache(WEST_BASE + I2S_6, 0x20);
		break;
	default:
		return -EINVAL;
	}

	if (gpio_addr == NULL)
		return -EIO;

	pr_info("%p has %#x\n", gpio_addr, *(u32 *)gpio_addr);

	memcpy_toio(gpio_addr, &gpio_value, sizeof(gpio_value));
	memcpy_toio(gpio_addr + 0x8, &gpio_value, sizeof(gpio_value));
	memcpy_toio(gpio_addr + 0x10, &gpio_value, sizeof(gpio_value));
	memcpy_toio(gpio_addr + 0x18, &gpio_value, sizeof(gpio_value));

	iounmap(gpio_addr);
	return 0;
}

static const struct snd_kcontrol_new apli_controls[] = {
};

static const struct snd_soc_dapm_widget apli_widgets[] = {
	SND_SOC_DAPM_SPK("SSP1 Speaker", NULL),
	SND_SOC_DAPM_MIC("SSP1 Mic", NULL),
	SND_SOC_DAPM_SPK("SSP2 Speaker", NULL),
	SND_SOC_DAPM_MIC("SSP2 Mic", NULL),
	SND_SOC_DAPM_SPK("SSP3 Speaker", NULL),
	SND_SOC_DAPM_MIC("SSP3 Mic", NULL),
	SND_SOC_DAPM_SPK("SSP4 Speaker", NULL),
	SND_SOC_DAPM_MIC("SSP4 Mic", NULL),
	SND_SOC_DAPM_SPK("SSP5 Speaker", NULL),
	SND_SOC_DAPM_MIC("SSP5 Mic", NULL),
	SND_SOC_DAPM_SPK("SSP6 Speaker", NULL),
	SND_SOC_DAPM_MIC("SSP6 Mic", NULL),
};

static const struct snd_soc_dapm_route apli_lhcrb_dummy_map[] = {
	/* Codec BE connections */
	/* SSP1 follows Hardware pin naming */
	{"SSP1 Speaker", NULL, "ssp0 Tx"},
	{"ssp0 Tx", NULL, "codec0_out"},

	{"codec0_in", NULL, "ssp0 Rx"},
	{"ssp0 Rx", NULL, "SSP1 Mic"},

	/* SSP2 follows Hardware pin naming */
	{"SSP2 Speaker", NULL, "ssp1 Tx"},
	{"ssp1 Tx", NULL, "codec1_out"},

	{"codec1_in", NULL, "ssp1 Rx"},
	{"ssp1 Rx", NULL, "SSP2 Mic"},

	/* SSP3 follows Hardware pin naming */
	{"SSP3 Speaker", NULL, "ssp2 Tx"},
	{"ssp2 Tx", NULL, "codec2_out"},

	{"codec2_in", NULL, "ssp2 Rx"},
	{"ssp2 Rx", NULL, "SSP3 Mic"},

	/* SSP4 follows Hardware pin naming */
	{"SSP4 Speaker", NULL, "ssp3 Tx"},
	{"ssp3 Tx", NULL, "codec3_out"},

	{"codec3_in", NULL, "ssp3 Rx"},
	{"ssp3 Rx", NULL, "SSP4 Mic"},

	/* SSP5 follows Hardware pin naming */
	{"SSP5 Speaker", NULL, "ssp4 Tx"},
	{"ssp4 Tx", NULL, "codec4_out"},

	{"codec4_in", NULL, "ssp4 Rx"},
	{"ssp4 Rx", NULL, "SSP5 Mic"},

	/* SSP6 follows Hardware pin naming */
	{"SSP6 Speaker", NULL, "ssp5 Tx"},
	{"ssp5 Tx", NULL, "codec5_out"},

	{"codec5_in", NULL, "ssp5 Rx"},
	{"ssp5 Rx", NULL, "SSP6 Mic"},
};

/* apli digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link apli_lhcrb_dummy_dais[] = {
	/* Front End DAI links for Dummy Codec*/
	/* SSP1 - FE DAI Link */
	{
		.name = "SSP1 Playback Port",
		.stream_name = "SSP1 Speaker",
		.cpu_dai_name = "System Pin",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
		.ops = &apli_lhcrb_dummy_ops,
	},
	{
		.name = "SSP1 Capture Port",
		.stream_name = "SSP1 Mic",
		.cpu_dai_name = "System Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST
			, SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.nonatomic = 1,
		.dynamic = 1,
		.ops = &apli_lhcrb_dummy_ops,
	},
	/* SSP2 - FE DAI Link */
	{
		.name = "SSP2 Playback Port",
		.stream_name = "SSP2 Speaker",
		.cpu_dai_name = "System Pin 2",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
		.ops = &apli_lhcrb_dummy_ops,
	},
	{
		.name = "SSP2 Capture Port",
		.stream_name = "SSP2 Mic",
		.cpu_dai_name = "System Pin 2",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST
			, SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.nonatomic = 1,
		.dynamic = 1,
		.ops = &apli_lhcrb_dummy_ops,
	},
	/* SSP3 - FE DAI Link */
	{
		.name = "SSP3 Playback Port",
		.stream_name = "SSP3 Speaker",
		.cpu_dai_name = "System Pin 3",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
		.ops = &apli_lhcrb_dummy_ops,
	},
	{
		.name = "SSP3 Capture Port",
		.stream_name = "SSP3 Mic",
		.cpu_dai_name = "System Pin 3",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST
			, SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.nonatomic = 1,
		.dynamic = 1,
		.ops = &apli_lhcrb_dummy_ops,
	},
	/* SSP4 - FE DAI Link */
	{
		.name = "SSP4 Playback Port",
		.stream_name = "SSP4 Speaker",
		.cpu_dai_name = "System Pin 4",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST
			, SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
		.ops = &apli_lhcrb_dummy_ops,
	},
	{
		.name = "SSP4 Capture Port",
		.stream_name = "SSP4 Mic",
		.cpu_dai_name = "System Pin 4",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST
			, SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.nonatomic = 1,
		.dynamic = 1,
		.ops = &apli_lhcrb_dummy_ops,
	},
	/* SSP5 - FE DAI Link */
	{
		.name = "SSP5 Playback Port",
		.stream_name = "SSP5 Speaker",
		.cpu_dai_name = "System Pin 5",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST
			, SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
		.ops = &apli_lhcrb_dummy_ops,
	},
	{
		.name = "SSP5 Capture Port",
		.stream_name = "SSP5 Mic",
		.cpu_dai_name = "System Pin 5",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST
			, SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.nonatomic = 1,
		.dynamic = 1,
		.ops = &apli_lhcrb_dummy_ops,
	},
	/* SSP6 - FE DAI Link */
	{
		.name = "SSP6 Playback Port",
		.stream_name = "SSP6 Speaker",
		.cpu_dai_name = "System Pin 6",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST
			, SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
		.ops = &apli_lhcrb_dummy_ops,
	},
	{
		.name = "SSP6 Capture Port",
		.stream_name = "SSP6 Mic",
		.cpu_dai_name = "System Pin 6",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST
			, SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.nonatomic = 1,
		.dynamic = 1,
		.ops = &apli_lhcrb_dummy_ops,
	},

	/* Back End DAI links for Dummy Codec*/
	{
		/* SSP1 - Codec */
		.name = "SSP0-Codec",
		.id = 0,
		.cpu_dai_name = "SSP0 Pin",
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
		.init = apli_ssp_northwest_gpio_init,
	},
	{
		/* SSP2 - Codec */
		.name = "SSP1-Codec",
		.id = 1,
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
		.name = "SSP2-Codec",
		.id = 2,
		.cpu_dai_name = "SSP2 Pin",
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
		.init = apli_ssp_northwest_gpio_init,
	},
	{
		/* SSP4 - Codec */
		.name = "SSP3-Codec",
		.id = 3,
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
	{
		/* SSP5 - Codec */
		.name = "SSP4-Codec",
		.id = 4,
		.cpu_dai_name = "SSP4 Pin",
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
		.init = apli_ssp_west_gpio_init,
	},
	{
		/* SSP6 - Codec */
		.name = "SSP5-Codec",
		.id = 5,
		.cpu_dai_name = "SSP5 Pin",
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
		.init = apli_ssp_west_gpio_init,
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
		.name = "apli_lhcrb_generic_i2s",
	},
};

module_platform_driver(apli_audio)

/* Module information */
MODULE_AUTHOR("Vinod Kumar <vinod.kumarx.vinod.kumar@intel.com>");
MODULE_DESCRIPTION("Intel Audio dummy Machine Driver for APL-I LH CRB");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:apli_lhcrb_generic_i2s");
