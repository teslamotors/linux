/*
 * tegra_Soc_spdif.c  --  SoC audio for tegra
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

static struct platform_device *tegra_snd_device;

extern struct snd_soc_dai tegra_spdif_dai[];
extern struct snd_soc_platform tegra_spdif_soc_platform;
extern struct snd_soc_dai tegra_dummy_codec_stub_all_dais[];
extern struct snd_soc_codec_device soc_codec_dev_dummy_codec;

static int tegra_hifi_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	int err;

	err = snd_soc_dai_set_fmt(codec_dai,
				  SND_SOC_DAIFMT_NB_NF |
				  SND_SOC_DAIFMT_CBS_CFS);
	if (err < 0) {
		printk(KERN_ERR "codec_dai fmt not set \n");
		return err;
	}

	err = snd_soc_dai_set_fmt(cpu_dai,
				  SND_SOC_DAIFMT_NB_NF |
				  SND_SOC_DAIFMT_CBS_CFS);
	if (err < 0) {
		printk(KERN_ERR "cpu_dai fmt not set \n");
		return err;
	}

	err = snd_soc_dai_set_sysclk(codec_dai, 0,562200, SND_SOC_CLOCK_IN);
	if (err < 0) {
		printk(KERN_ERR "codec_dai clock not set\n");
		return err;
	}

	err = snd_soc_dai_set_sysclk(cpu_dai, 0, 562200, SND_SOC_CLOCK_IN);
	if (err < 0) {
		printk(KERN_ERR "cpu_dai clock not set\n");
		return err;
	}
	return 0;
}

static struct snd_soc_ops tegra_hifi_ops = {
	.hw_params = tegra_hifi_hw_params,
};

static int tegra_codec_init(struct snd_soc_codec *codec)
{

	return 0;
}

static struct snd_soc_dai_link tegra_spdif_soc_dai[] = {
	{
	 .name = "spdif",
	 .stream_name = "spdif",
	 .cpu_dai = &tegra_spdif_dai[0],
	 .codec_dai = &tegra_dummy_codec_stub_all_dais[3],
	 .init = tegra_codec_init,
	 .ops = &tegra_hifi_ops,
	 },

};

static struct snd_soc_card tegra_snd_soc = {
	.name = "tegra",
	.platform = &tegra_spdif_soc_platform,
	.dai_link = tegra_spdif_soc_dai,
	.num_links = ARRAY_SIZE(tegra_spdif_soc_dai),
};

static struct snd_soc_device tegra_snd_devdata = {
	.card = &tegra_snd_soc,
	.codec_dev = &soc_codec_dev_dummy_codec,
};

static int __init tegra_spdif_init(void)
{
	int ret = 0;

	tegra_snd_device = platform_device_alloc("soc-audio",2 );
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

static void __exit tegra_spdif_exit(void)
{
	tegra_controls_exit();
	platform_device_unregister(tegra_snd_device);
}

module_init(tegra_spdif_init);
module_exit(tegra_spdif_exit);

/* Module information */
MODULE_DESCRIPTION("Tegra ALSA SoC");
MODULE_LICENSE("GPL");
