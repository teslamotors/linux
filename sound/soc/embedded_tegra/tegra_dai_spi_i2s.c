/*
 * tegra_spi_i2s.c  --  ALSA Soc Audio Layer
 *
 * (c) 2010 Nvidia Graphics Pvt. Ltd.
 *  http://www.nvidia.com
 *
 * (c) 2006 Wolfson Microelectronics PLC.
 * Graeme Gregory graeme.gregory@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 * (c) 2004-2005 Simtec Electronics
 *    http://armlinux.simtec.co.uk/
 *    Ben Dooks <ben@simtec.co.uk>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */
#include "tegra_soc.h"

static int tegra_spi_i2s_dai_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	default:
		return -EINVAL;
	}

	switch (params_rate(params)) {
	case 8000:
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int tegra_spi_i2s_dai_probe(struct platform_device *pdev,
		struct snd_soc_dai *dai)
{
	return 0;
}

static struct snd_soc_dai_ops tegra_spi_i2s_dai_ops = {
	.hw_params = tegra_spi_i2s_dai_hw_params,
};

struct snd_soc_dai tegra_spi_i2s_dai = {
	.name = "tegra-spi-i2s",
	.id = 0,
	.probe = tegra_spi_i2s_dai_probe,
	.playback = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000,
		.formats =
		SNDRV_PCM_FMTBIT_S16_LE,
	},
	.capture = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000,
		.formats =
		SNDRV_PCM_FMTBIT_S16_LE,
	},
	.ops = &tegra_spi_i2s_dai_ops,
};

EXPORT_SYMBOL_GPL(tegra_spi_i2s_dai);

static int __init tegra_spi_i2s_dai_init(void)
{
	return snd_soc_register_dai(&tegra_spi_i2s_dai);
}

module_init(tegra_spi_i2s_dai_init);

static void __exit tegra_spi_i2s_dai_exit(void)
{
	snd_soc_unregister_dai(&tegra_spi_i2s_dai);
}

module_exit(tegra_spi_i2s_dai_exit);

/* Module information */
MODULE_DESCRIPTION("Tegra SPI-I2S SoC Interface");
MODULE_LICENSE("GPL");
