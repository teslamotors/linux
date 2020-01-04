/*
 * dummy_codec.c
 *
 * dummy codec driver for NVIDIA Tegra SoCs
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

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/moduleparam.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/initval.h>

#include "asm/mach-types.h"

static int tegra_dummy_codec_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *params,
				       struct snd_soc_dai *dai)
{
	return 0;
}

static int tegra_dummy_codec_mute(struct snd_soc_dai *dai, int mute)
{
	return 0;
}

static int tegra_dummy_codec_set_dai_fmt(struct snd_soc_dai *codec_dai,
					 unsigned int fmt)
{
	return 0;
}

static int tegra_dummy_codec_set_dai_clkdiv(struct snd_soc_dai *codec_dai,
					    int div_id, int div)
{
	return 0;
}

static int tegra_dummy_codec_set_dai_pll(struct snd_soc_dai *codec_dai,
					 int pll_id, int source,
					 unsigned int freq_in,
					 unsigned int freq_out)
{
	return 0;
}

static int tegra_dummy_codec_set_dai_sysclk(struct snd_soc_dai *codec_dai,
					    int clk_id, unsigned int freq,
					    int dir)
{
	return 0;
}

static struct snd_soc_dai_ops tegra_dummy_codec_stub_dai_ops = {
	.hw_params = tegra_dummy_codec_hw_params,
	.digital_mute = tegra_dummy_codec_mute,
	.set_fmt = tegra_dummy_codec_set_dai_fmt,
	.set_clkdiv = tegra_dummy_codec_set_dai_clkdiv,
	.set_pll = tegra_dummy_codec_set_dai_pll,
	.set_sysclk = tegra_dummy_codec_set_dai_sysclk,
};

#define APXX_I2S_RATES (SNDRV_PCM_RATE_44100|SNDRV_PCM_RATE_48000)
#define APXX_SPDIF_RATES (SNDRV_PCM_RATE_44100|SNDRV_PCM_RATE_48000|SNDRV_PCM_RATE_88200)

struct snd_soc_dai tegra_dummy_codec_stub_all_dais[] = {
	{
	 .name = "tegra-dummy-codec-i2s",
	 .id = 1,
	 .playback = {
		      .stream_name = "Playback",
		      .channels_min = 2,
		      .channels_max = 2,
		      .rates = APXX_I2S_RATES,
		      .formats =
		      SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
		      SNDRV_PCM_FMTBIT_S32_LE,
		      },
	 .capture = {
		     .stream_name = "Capture",
		     .channels_min = 2,
		     .channels_max = 2,
		     .rates = APXX_I2S_RATES,
		     .formats =
		     SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
		     SNDRV_PCM_FMTBIT_S32_LE,
		     },
	 .ops = &tegra_dummy_codec_stub_dai_ops,
	 },

	{.name = "tegra-dummy-codec-tdm1",
	 .id = 2,
	 .playback = {
		      .stream_name = "Playback",
		      .channels_min = 8,
		      .channels_max = 8,
		      .rates = APXX_I2S_RATES,
		      .formats =
		      SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
		      SNDRV_PCM_FMTBIT_S32_LE,
		      },
	 .capture = {
		     .stream_name = "Capture",
		     .channels_min = 8,
		     .channels_max = 8,
		     .rates = APXX_I2S_RATES,
		     .formats =
		     SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
		     SNDRV_PCM_FMTBIT_S32_LE,
		     },
	 .ops = &tegra_dummy_codec_stub_dai_ops,
	 },
	{.name = "tegra-dummy-codec-tdm2",
	 .id = 3,
	 .playback = {
		      .stream_name = "Playback",
		      .channels_min = 16,
		      .channels_max = 16,
		      .rates = APXX_I2S_RATES,
		      .formats =
		      SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
		      SNDRV_PCM_FMTBIT_S32_LE,
		      },
	 .capture = {
		     .stream_name = "Capture",
		     .channels_min = 16,
		     .channels_max = 16,
		     .rates = APXX_I2S_RATES,
		     .formats =
		     SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
		     SNDRV_PCM_FMTBIT_S32_LE,
		     },
	 .ops = &tegra_dummy_codec_stub_dai_ops,
	 },
#ifdef CONFIG_EMBEDDED_TEGRA_ALSA_SPDIF
	{
		.name = "tegra-dummy-codec-spdif",
		.id = 4,
		.playback = {
			.stream_name = "Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = APXX_SPDIF_RATES,
			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
						SNDRV_PCM_FMTBIT_S32_LE,
		},

		.capture = {
			.stream_name = "Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = APXX_SPDIF_RATES,
			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
						SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &tegra_dummy_codec_stub_dai_ops,
	},
#endif // CONFIG_EMBEDDED_TEGRA_ALSA_SPDIF
};

EXPORT_SYMBOL_GPL(tegra_dummy_codec_stub_all_dais);

static int __init dit_modinit(void)
{
	return snd_soc_register_dais(&tegra_dummy_codec_stub_all_dais[0],
				     ARRAY_SIZE
				     (tegra_dummy_codec_stub_all_dais));
}

static void __exit dit_exit(void)
{
	snd_soc_unregister_dais(&tegra_dummy_codec_stub_all_dais[0],
				ARRAY_SIZE(tegra_dummy_codec_stub_all_dais));
}

module_init(dit_modinit);
module_exit(dit_exit);

static int codec_soc_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec;
	int ret = 0;

	socdev->card->codec = kzalloc(sizeof(struct snd_soc_codec), GFP_KERNEL);
	if (!socdev->card->codec)
		return -ENOMEM;

	codec = socdev->card->codec;
	mutex_init(&codec->mutex);

	codec->name = "tegra-dummy-codec";
	codec->owner = THIS_MODULE;
	codec->dai = &tegra_dummy_codec_stub_all_dais[0];
	codec->num_dai = sizeof(tegra_dummy_codec_stub_all_dais)/sizeof(struct snd_soc_dai);
	codec->write = NULL;
	codec->read = NULL;
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);
	/* Register PCMs. */
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		printk(KERN_ERR "codec: failed to create pcms\n");
		goto pcm_err;
	}

	return ret;

pcm_err:
	kfree(socdev->card->codec);

	return ret;
}

static int codec_soc_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;

	if (!codec)
		return 0;

	snd_soc_free_pcms(socdev);
	kfree(socdev->card->codec);

	return 0;
}

#define codec_soc_suspend NULL
#define codec_soc_resume NULL

struct snd_soc_codec_device soc_codec_dev_dummy_codec = {
	.probe = codec_soc_probe,
	.remove = codec_soc_remove,
	.suspend = codec_soc_suspend,
	.resume = codec_soc_resume,
};

EXPORT_SYMBOL_GPL(soc_codec_dev_dummy_codec);

/* Module information */
MODULE_DESCRIPTION("Tegra Codec Interface");
MODULE_LICENSE("GPL");
