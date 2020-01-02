/*
 * ALSA SoC dummy codec driver
 *
 *  This driver provides two dummy dais.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/initval.h>

#define DRV_NAME	"dummy-codec"

#define RATES	SNDRV_PCM_RATE_8000_192000
#define FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE)


static struct snd_soc_codec_driver soc_codec_dummy;

static struct snd_soc_dai_driver dummy_codec_dai[] = {
	{
		.name		= "dummy-aif1",
		.playback	= {
			.stream_name	= "AIF1 Playback",
			.channels_min	= 1,
			.channels_max	= 384,
			.rates		= RATES,
			.formats	= FORMATS,
		},
		.capture	= {
			.stream_name	= "AIF1 Capture",
			.channels_min	= 1,
			.channels_max	= 384,
			.rates		= RATES,
			.formats	= FORMATS,
		},
	}, {
		.name		= "dummy-aif2",
		.playback	= {
			.stream_name	= "AIF2 Playback",
			.channels_min	= 1,
			.channels_max	= 384,
			.rates		= RATES,
			.formats	= FORMATS,
		},
		.capture	= {
			.stream_name	= "AIF2 Capture",
			.channels_min	= 1,
			.channels_max	= 384,
			.rates		= RATES,
			.formats	= FORMATS,
		}
	}
};

static int dummy_codec_probe(struct platform_device *pdev)
{
	return snd_soc_register_codec(&pdev->dev, &soc_codec_dummy,
			dummy_codec_dai, ARRAY_SIZE(dummy_codec_dai));
}

static int dummy_codec_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id dummy_codec_of_match[] = {
	{ .compatible = "samsung,dummy-codec", },
	{},
};
MODULE_DEVICE_TABLE(of, dummy_codec_of_match);
#endif /* CONFIG_OF */

static struct platform_driver dummy_codec_driver = {
	.probe		= dummy_codec_probe,
	.remove		= dummy_codec_remove,
	.driver		= {
		.name	= DRV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(dummy_codec_of_match),
	},
};

module_platform_driver(dummy_codec_driver);

