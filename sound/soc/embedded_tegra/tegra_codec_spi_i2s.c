/*
 * tegra_codec_spi.c
 *
 * ALSA SOC driver for NVIDIA Tegra SoCs
 *
 * Copyright (C) 2010 NVIDIA Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "tegra_soc.h"

static int tegra_codec_spi_i2s_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	return 0;
}

static int tegra_codec_spi_i2s_mute(struct snd_soc_dai *dai, int mute)
{
	return 0;
}

static int tegra_codec_spi_i2s_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	return 0;
}

static int tegra_codec_spi_i2s_set_dai_clkdiv(struct snd_soc_dai *codec_dai,
		int div_id, int div)
{
	return 0;
}

static int tegra_codec_spi_i2s_set_dai_pll(struct snd_soc_dai *codec_dai,
		int pll_id, int source, unsigned int freq_in, unsigned int freq_out)
{
	return 0;
}

static int tegra_codec_spi_i2s_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static struct snd_soc_dai_ops tegra_codec_spi_i2s_dai_ops = {
	.hw_params = tegra_codec_spi_i2s_hw_params,
	.digital_mute = tegra_codec_spi_i2s_mute,
	.set_fmt = tegra_codec_spi_i2s_set_dai_fmt,
	.set_clkdiv = tegra_codec_spi_i2s_set_dai_clkdiv,
	.set_pll = tegra_codec_spi_i2s_set_dai_pll,
	.set_sysclk = tegra_codec_spi_i2s_set_dai_sysclk,
};

struct snd_soc_dai tegra_codec_spi_i2s_dai = {
	.name = "tegra-codec-spi-i2s",
	.playback = {
		.stream_name    = "Playback",
		.channels_min   = 2,
		.channels_max   = 2,
		.rates          = SNDRV_PCM_RATE_8000,
		.formats        = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.capture = {
		.stream_name    = "Capture",
		.channels_min   = 2,
		.channels_max   = 2,
		.rates          = SNDRV_PCM_RATE_8000,
		.formats        = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.ops = &tegra_codec_spi_i2s_dai_ops,
};
EXPORT_SYMBOL_GPL(tegra_codec_spi_i2s_dai);

static int __init tegra_codec_spi_i2s_init(void)
{
	return snd_soc_register_dai(&tegra_codec_spi_i2s_dai);
}

static void __exit tegra_codec_spi_i2s_exit(void)
{
	snd_soc_unregister_dai(&tegra_codec_spi_i2s_dai);
}

module_init(tegra_codec_spi_i2s_init);
module_exit(tegra_codec_spi_i2s_exit);

static int tegra_codec_spi_i2s_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec;
	int ret = 0;

	socdev->card->codec = kzalloc(sizeof(struct snd_soc_codec), GFP_KERNEL);
	if (!socdev->card->codec)
		return -ENOMEM;

	codec = socdev->card->codec;
	mutex_init(&codec->mutex);

	codec->name = "tegra-codec-spi-i2s";
	codec->owner = THIS_MODULE;
	codec->dai = &tegra_codec_spi_i2s_dai;
	codec->num_dai = 1;
	codec->write = NULL;
	codec->read = NULL;
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		printk(KERN_ERR "codec: failed to create pcms\n");
		kfree(socdev->card->codec);
	}
	return ret;
}

static int tegra_codec_spi_i2s_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;

	if (!codec)
		return 0;

	snd_soc_free_pcms(socdev);
	kfree(socdev->card->codec);

	return 0;
}

struct snd_soc_codec_device soc_codec_dev_tegra_spi_i2s_codec = {
	.probe = tegra_codec_spi_i2s_probe,
	.remove = tegra_codec_spi_i2s_remove,
	.suspend = NULL,
	.resume = NULL,
};
EXPORT_SYMBOL_GPL(soc_codec_dev_tegra_spi_i2s_codec);

/* Module information */
MODULE_DESCRIPTION("Tegra Codec Interface");
MODULE_LICENSE("GPL");
