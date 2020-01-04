/*
 * tegra_soc_wm8903.c  --  SoC audio for tegra
 *
 * (c) 2010 Nvidia Graphics Pvt. Ltd.
 *  http://www.nvidia.com
 *
 * Copyright 2007 Wolfson Microelectronics PLC.
 * Author: Graeme Gregory
 *         graeme.gregory@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include "tegra_soc.h"
#include "../codecs/wm8903.h"

static struct platform_device *tegra_snd_device;

extern struct snd_soc_dai tegra_i2s_dai[];
extern struct snd_soc_platform tegra_soc_platform;

/* codec register values */
#define B07_INEMUTE		7
#define B06_VOL_M3DB		6
#define B00_IN_VOL		0
#define B00_INR_ENA		0
#define B01_INL_ENA		1
#define R06_MICBIAS_CTRL_0	6
#define B07_MICDET_HYST_ENA	7
#define B04_MICDET_THR		4
#define B02_MICSHORT_THR	2
#define B01_MICDET_ENA		1
#define B00_MICBIAS_ENA		0
#define B15_DRC_ENA		15
#define B03_DACL_ENA		3
#define B02_DACR_ENA		2
#define B01_ADCL_ENA		1
#define B00_ADCR_ENA		0
#define B06_IN_CM_ENA		6
#define B04_IP_SEL_N		4
#define B02_IP_SEL_P		2
#define B00_MODE 		0
#define B06_AIF_ADCL		7
#define B06_AIF_ADCR		6
#define B05_ADC_HPF_CUT		5
#define B04_ADC_HPF_ENA		4
#define B01_ADCL_DATINV		1
#define B00_ADCR_DATINV		0
#define R20_SIDETONE_CTRL	32
#define R29_DRC_1		41
#define SET_REG_VAL(r,m,l,v) (((r)&(~((m)<<(l))))|(((v)&(m))<<(l)))


static int tegra_hifi_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	int err;
	struct snd_soc_codec *codec = codec_dai->codec;
	int CtrlReg = 0;
	int VolumeCtrlReg = 0;
	int SidetoneCtrlReg = 0;
	int SideToneAtenuation = 0;

	err = snd_soc_dai_set_fmt(codec_dai,
					SND_SOC_DAIFMT_I2S | \
					SND_SOC_DAIFMT_NB_NF | \
					SND_SOC_DAIFMT_CBS_CFS);
	if (err < 0) {
		printk(KERN_ERR "codec_dai fmt not set \n");
		return err;
	}

	err = snd_soc_dai_set_fmt(cpu_dai,
					SND_SOC_DAIFMT_I2S | \
					SND_SOC_DAIFMT_NB_NF | \
					SND_SOC_DAIFMT_CBS_CFS);
	if (err < 0) {
		printk(KERN_ERR "cpu_dai fmt not set \n");
		return err;
	}

	err = snd_soc_dai_set_sysclk(codec_dai, 0, I2S_CLK, SND_SOC_CLOCK_IN);
	if (err < 0) {
		printk(KERN_ERR "codec_dai clock not set\n");
		return err;
	}

	err = snd_soc_dai_set_sysclk(cpu_dai, 0, I2S_CLK, SND_SOC_CLOCK_IN);
	if (err < 0) {
		printk(KERN_ERR "cpu_dai clock not set\n");
		return err;
	}

	if (substream->stream != SNDRV_PCM_STREAM_PLAYBACK) {
		snd_soc_write(codec, WM8903_ANALOGUE_LEFT_INPUT_0, 0X7);
		snd_soc_write(codec, WM8903_ANALOGUE_RIGHT_INPUT_0, 0X7);
		// Mic Bias enable
		CtrlReg = (0x1<<B00_MICBIAS_ENA) | (0x1<<B01_MICDET_ENA);
		snd_soc_write(codec, WM8903_MIC_BIAS_CONTROL_0, CtrlReg);
		// Enable DRC
		CtrlReg = snd_soc_read(codec, WM8903_DRC_0);
		CtrlReg |= (1<<B15_DRC_ENA);
		snd_soc_write(codec, WM8903_DRC_0, CtrlReg);
		// Single Ended Mic
		CtrlReg = (0x0<<B06_IN_CM_ENA) |
			(0x0<<B00_MODE) | (0x0<<B04_IP_SEL_N)
					| (0x1<<B02_IP_SEL_P);
		VolumeCtrlReg = (0x5 << B00_IN_VOL);
		// Mic Setting
		snd_soc_write(codec, WM8903_ANALOGUE_LEFT_INPUT_1, CtrlReg);
		snd_soc_write(codec, WM8903_ANALOGUE_RIGHT_INPUT_1, CtrlReg);
		// voulme for single ended mic
		snd_soc_write(codec, WM8903_ANALOGUE_LEFT_INPUT_0,
				VolumeCtrlReg);
		snd_soc_write(codec, WM8903_ANALOGUE_RIGHT_INPUT_0,
				VolumeCtrlReg);
		// replicate mic setting on both channels
		CtrlReg = snd_soc_read(codec, WM8903_AUDIO_INTERFACE_0);
		CtrlReg  = SET_REG_VAL(CtrlReg, 0x1, B06_AIF_ADCR, 0x0);
		CtrlReg  = SET_REG_VAL(CtrlReg, 0x1, B06_AIF_ADCL, 0x0);
		snd_soc_write(codec, WM8903_AUDIO_INTERFACE_0, CtrlReg);
		// Enable analog inputs
		CtrlReg = (0x1<<B01_INL_ENA) | (0x1<<B00_INR_ENA);
		snd_soc_write(codec, WM8903_POWER_MANAGEMENT_0, CtrlReg);
		// ADC Settings
		CtrlReg = snd_soc_read(codec, WM8903_ADC_DIGITAL_0);
		CtrlReg |= (0x1<<B04_ADC_HPF_ENA);
		snd_soc_write(codec, WM8903_ADC_DIGITAL_0, CtrlReg);
		SidetoneCtrlReg = 0;
		snd_soc_write(codec, R20_SIDETONE_CTRL, SidetoneCtrlReg);
		// Enable ADC
		CtrlReg = snd_soc_read(codec, WM8903_POWER_MANAGEMENT_6);
		CtrlReg |= (0x1<<B00_ADCR_ENA)|(0x1<<B01_ADCL_ENA);
		snd_soc_write(codec, WM8903_POWER_MANAGEMENT_6, CtrlReg);
		// Enable Sidetone
		SidetoneCtrlReg = (0x1<<2) | (0x2<<0);
		SideToneAtenuation = 12 ; // sidetone 0 db
		SidetoneCtrlReg |= (SideToneAtenuation<<8)
				| (SideToneAtenuation<<4);
		snd_soc_write(codec, R20_SIDETONE_CTRL, SidetoneCtrlReg);
		CtrlReg = snd_soc_read(codec, R29_DRC_1);
		CtrlReg |= 0x3; //mic volume 18 db
		snd_soc_write(codec, R29_DRC_1, CtrlReg);
	}

	return 0;
}

static struct snd_soc_ops tegra_hifi_ops = {
	.hw_params = tegra_hifi_hw_params,
};

static int tegra_codec_init(struct snd_soc_codec *codec)
{
	return tegra_controls_init(codec);
}

static struct snd_soc_dai_link tegra_soc_dai[] = {
	{
		.name = "WM8903",
		.stream_name = "WM8903 HiFi",
		.cpu_dai = &tegra_i2s_dai[0],
		.codec_dai = &wm8903_dai,
		.init = tegra_codec_init,
		.ops = &tegra_hifi_ops,
	},
	{
		.name = "WM8903",
		.stream_name = "WM8903 Voice",
		.cpu_dai = &tegra_i2s_dai[1],
		.codec_dai = &wm8903_dai,
		.init = tegra_codec_init,
		.ops = &tegra_hifi_ops,
	},

};

static struct snd_soc_card tegra_snd_soc = {
	.name = "tegra",
	.platform = &tegra_soc_platform,
	.dai_link = tegra_soc_dai,
	.num_links = ARRAY_SIZE(tegra_soc_dai),
};


static struct snd_soc_device tegra_snd_devdata = {
	.card = &tegra_snd_soc,
	.codec_dev = &soc_codec_dev_wm8903,
};

static int __init tegra_init(void)
{
	int ret = 0;

	tegra_snd_device = platform_device_alloc("soc-audio", -1);
	if (!tegra_snd_device) {
		pr_err("failed to allocate soc-audio \n");
		return ENOMEM;
	}

	platform_set_drvdata(tegra_snd_device, &tegra_snd_devdata);
	tegra_snd_devdata.dev = &tegra_snd_device->dev;
	ret = platform_device_add(tegra_snd_device);
	if (ret) {
		pr_err("audio device could not be added \n");
		goto fail;
	}

	return 0;

fail:
	if (tegra_snd_device) {
		platform_device_put(tegra_snd_device);
		tegra_snd_device = 0;
	}

	return ret;
}

static void __exit tegra_exit(void)
{
	tegra_controls_exit();
	platform_device_unregister(tegra_snd_device);
}

module_init(tegra_init);
module_exit(tegra_exit);

/* Module information */
MODULE_DESCRIPTION("Tegra ALSA SoC");
MODULE_LICENSE("GPL");
