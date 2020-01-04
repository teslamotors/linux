/*
 * tegra_soc_tdm.c  --  SoC audio for tegra
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

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>

static struct platform_device *tegra_spi_i2s_snd_device;
extern struct snd_soc_dai tegra_spi_i2s_dai;
extern struct snd_soc_platform tegra_soc_spi_i2s_platform;
extern struct snd_soc_dai tegra_codec_spi_i2s_dai;
extern struct snd_soc_codec_device soc_codec_dev_tegra_spi_i2s_codec;

static int tegra_soc_spi_i2s_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	int err;

	err = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S);
	if (err < 0) {
		printk(KERN_ERR "codec_dai fmt not set \n");
		return err;
	}
	return 0;
}

static struct snd_soc_ops tegra_soc_spi_i2s_ops = {
	.hw_params = tegra_soc_spi_i2s_hw_params,
};

static int tegra_codec_spi_i2s_init(struct snd_soc_codec *codec)
{
	return 0;
}

static struct snd_soc_dai_link tegra_spi_i2s_dai_link[] = {
	{
		.name = "SPI",
		.stream_name = "SPI",
		.cpu_dai = &tegra_spi_i2s_dai,
		.codec_dai = &tegra_codec_spi_i2s_dai,
		.init = tegra_codec_spi_i2s_init,
		.ops = &tegra_soc_spi_i2s_ops
	},
};

static struct snd_soc_card tegra_spi_i2s_card = {
	.name = "tegra",
	.platform = &tegra_soc_spi_i2s_platform,
	.dai_link = tegra_spi_i2s_dai_link,
	.num_links = 1,
};

static struct snd_soc_device tegra_spi_i2s_snd_devdata = {
	.card = &tegra_spi_i2s_card,
	.codec_dev = &soc_codec_dev_tegra_spi_i2s_codec,
};

static int __init tegra_spi_i2s_snd_init(void)
{
	int ret;

	tegra_spi_i2s_snd_device = platform_device_alloc("soc-audio", 1);
	if (!tegra_spi_i2s_snd_device)
		return -ENOMEM;

	platform_set_drvdata(tegra_spi_i2s_snd_device, &tegra_spi_i2s_snd_devdata);
	tegra_spi_i2s_snd_devdata.dev = &tegra_spi_i2s_snd_device->dev;

	ret = platform_device_add(tegra_spi_i2s_snd_device);
	if (ret) {
		printk(KERN_ERR "ap20 audio device could not be added \n");
		platform_device_put(tegra_spi_i2s_snd_device);
		return ret;
	}
	return ret;
}

static void __exit tegra_spi_i2s_snd_exit(void)
{
	platform_device_unregister(tegra_spi_i2s_snd_device);
}

module_init(tegra_spi_i2s_snd_init);
module_exit(tegra_spi_i2s_snd_exit);

/* Module information */
MODULE_DESCRIPTION("ALSA SoC TEGRA");
MODULE_LICENSE("GPL");
