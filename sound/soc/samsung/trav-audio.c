/*
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>

#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>

#include "i2s.h"
#include "i2s-regs.h"

/* trav use external clock from cdclk line */
#define TRAV_AUD_OSC_FREQ		24576000

static struct snd_soc_card trav;

struct trav_audio_priv {
	int slotnum;
	int external_clock_val;
	u32 i2s_tx_fifo;
	u32 i2s_rx_fifo;
	u32 i2s_tx_s_fifo;
	int relay_en_gpio;
	int amp_mute_gpio;
};

static void trav_enable_sysreg(bool on)
{
	pr_debug("%s: %s\n", __func__, on ? "on" : "off");

	/* todo -- Add sysreg PERIC TDM0 CORE_CLK_SEL configuration here */
}

/* trav_amp_mute_get - read the speaker mute setting.
 * @kcontrol: The control for the speaker gain.
 * @ucontrol: The value that needs to be updated.
 *
 * Read the value for the AMP gain control.
 */
static int trav_amp_mute_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct trav_audio_priv *priv = snd_soc_card_get_drvdata(card);
	ucontrol->value.integer.value[0] = gpio_get_value(priv->amp_mute_gpio);
	return 0;
}

/**
 * speaker_mute_put - set the speaker mute setting.
 * @kcontrol: The control for the speaker gain.
 * @ucontrol: The value that needs to be set.
 *
 * Set the value of the Amplifier gain from the specified
 * @ucontrol setting.
 */
static int trav_amp_mute_put(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct trav_audio_priv *priv = snd_soc_card_get_drvdata(card);
	gpio_set_value(priv->amp_mute_gpio, ucontrol->value.integer.value[0]);
	return 0;
}

static const struct snd_kcontrol_new amp_mute_controls[] = {
	SOC_SINGLE_EXT("Speaker Amplifier Mute", 0, 0, 1, 0,
		       trav_amp_mute_get, trav_amp_mute_put),
};
/*
 * trav I2S DAI operations. (AP master)
 */
static int trav_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int bfs, rfs, ret;
	unsigned long rclk;
	struct snd_soc_card *card = rtd->card;
	struct trav_audio_priv *priv = snd_soc_card_get_drvdata(card);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U24:
	case SNDRV_PCM_FORMAT_S24:
		bfs = 48;
		break;
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		bfs = 32;
		break;
	default:
		return -EINVAL;
	}

	switch (params_rate(params)) {
	case 16000:
	case 22050:
	case 24000:
	case 32000:
	case 44100:
	case 48000:
	case 88200:
	case 96000:
		if (bfs == 48)
			rfs = 384;
		else
			rfs = 256;
		break;
	case 64000:
		rfs = 384;
		break;
	case 8000:
	case 11025:
	case 12000:
		if (bfs == 48)
			rfs = 768;
		else
			rfs = 512;
		break;
	default:
		return -EINVAL;
	}

	rclk = params_rate(params) * rfs;

	/* SYSREG PERIC TDM0 CORE CLOCK CONFIGURATION */
	trav_enable_sysreg(true);

	/* Set CPU DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
					rfs, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
					0, MOD_OPCLK_PCLK);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_0,
					0, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
	if (ret < 0)
		return ret;

	/* Set Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBS_CFS);

	if (ret < 0)
		return ret;


	ret = snd_soc_dai_set_sysclk(codec_dai, 0, rclk, SND_SOC_CLOCK_IN);

	if (gpio_is_valid(priv->relay_en_gpio))
		gpio_direction_output(priv->relay_en_gpio, 1);

	return 0;
}

static int trav_startup(struct snd_pcm_substream *substream)
{
	struct dma_chan *chan = snd_dmaengine_pcm_get_chan(substream);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct i2s_dai *i2s = snd_soc_dai_get_drvdata(cpu_dai);
	struct i2s_dai *other = get_other_dai(i2s);
	struct trav_audio_priv *priv = snd_soc_card_get_drvdata(card);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (is_secondary(i2s)) {
			other->sec_dai->dma_playback.addr =
				iommu_dma_map_resource(chan->device->dev,
				(phys_addr_t)priv->i2s_tx_s_fifo,
				0x4, DMA_FROM_DEVICE, 0);
		} else{
			i2s->dma_playback.addr = iommu_dma_map_resource(
						chan->device->dev,
						(phys_addr_t)priv->i2s_tx_fifo,
						0x4, DMA_FROM_DEVICE, 0);
		}
	} else
		i2s->dma_capture.addr = iommu_dma_map_resource(
						chan->device->dev,
						(phys_addr_t)priv->i2s_rx_fifo,
						0x4, DMA_TO_DEVICE, 0);

	return 0;
}

static void trav_shutdown(struct snd_pcm_substream *substream)
{
	struct dma_chan *chan = snd_dmaengine_pcm_get_chan(substream);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct i2s_dai *i2s = snd_soc_dai_get_drvdata(cpu_dai);
	struct i2s_dai *other = get_other_dai(i2s);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (is_secondary(i2s))
			iommu_dma_unmap_resource(chan->device->dev,
				other->sec_dai->dma_playback.addr, 0x4,
				DMA_FROM_DEVICE, 0);
		else
			iommu_dma_unmap_resource(chan->device->dev,
				i2s->dma_playback.addr, 0x4,
				DMA_FROM_DEVICE, 0);
	} else
		iommu_dma_unmap_resource(chan->device->dev,
				i2s->dma_capture.addr, 0x4, DMA_TO_DEVICE, 0);

}

int trav_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct trav_audio_priv *priv = snd_soc_card_get_drvdata(card);

	if (gpio_is_valid(priv->relay_en_gpio))
		gpio_direction_output(priv->relay_en_gpio, 0);

	return 0;
}

static const struct snd_soc_ops trav_ops = {
	.hw_params = trav_hw_params,
	.hw_free   = trav_hw_free,
	.startup   = trav_startup,
	.shutdown  = trav_shutdown,
};

static struct snd_soc_dai_link trav_dai[] = {
	{ /* Primary DAI i/f */
		.name = "TRAV AUDIO PRI",
		.stream_name = "i2s0-pri",
		.codec_dai_name = "tlv320aic3x-hifi",
		.ops = &trav_ops,
	}
};

static int trav_suspend_post(struct snd_soc_card *card)
{
	trav_enable_sysreg(false);
	return 0;
}

static int trav_resume_pre(struct snd_soc_card *card)
{
	trav_enable_sysreg(true);
	return 0;
}

static struct snd_soc_card trav = {
	.name = "TRAV-I2S",
	.owner = THIS_MODULE,
	.suspend_post = trav_suspend_post,
	.resume_pre = trav_resume_pre,
	.dai_link = trav_dai,
	.num_links = ARRAY_SIZE(trav_dai),
};

static int trav_audio_probe(struct platform_device *pdev)
{
	int n, ret;
	struct device_node *np = pdev->dev.of_node;
	struct snd_soc_card *card = &trav;
	struct trav_audio_priv *priv;

	card->dev = &pdev->dev;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(&pdev->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	snd_soc_card_set_drvdata(card, (void *)priv);

	for (n = 0; np && n < ARRAY_SIZE(trav_dai); n++) {
		if (!trav_dai[n].cpu_dai_name) {
			trav_dai[n].cpu_of_node = of_parse_phandle(np,
					"samsung,audio-cpu", n);

			if (!trav_dai[n].cpu_of_node) {
				dev_err(&pdev->dev,
				"Property 'samsung,audio-cpu' missing or invalid\n");
				ret = -EINVAL;
			}
		}

		if (!trav_dai[n].platform_name)
			trav_dai[n].platform_of_node = trav_dai[n].cpu_of_node;

		trav_dai[n].codec_name = NULL;
		trav_dai[n].codec_of_node = of_parse_phandle(np,
				"samsung,audio-codec", n);
		if (!trav_dai[0].codec_of_node) {
			dev_err(&pdev->dev,
			"Property 'samsung,audio-codec' missing or invalid\n");
			ret = -EINVAL;
		}
	}

	of_property_read_u32(np, "samsung,tdm-slotnum", &priv->slotnum);
	of_property_read_u32(np, "samsung,i2s-tx-fifo", &priv->i2s_tx_fifo);
	of_property_read_u32(np, "samsung,i2s-rx-fifo", &priv->i2s_rx_fifo);
	of_property_read_u32(np, "samsung,i2s-tx-s-fifo", &priv->i2s_tx_s_fifo);

	ret = of_get_named_gpio(np, "relay-gpio", 0);
	if (ret >= 0) {
		priv->relay_en_gpio = ret;
	} else {
		if (ret != -ENOENT) {
			dev_err(&pdev->dev, "of_get_named_gpio for relay-gpio failed:%d\n",
					ret);
		}
		priv->relay_en_gpio = -1;
	}

	ret = of_get_named_gpio(np, "mute-gpio", 0);
	if (ret >= 0) {
		priv->amp_mute_gpio = ret;
	} else {
		if (ret != -ENOENT) {
			dev_err(&pdev->dev, "of_get_named_gpio for mute-gpio failed:%d\n",
					ret);
		}
		priv->amp_mute_gpio = -1;
	}

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card() failed:%d\n", ret);
		goto err;
	}

	if (gpio_is_valid(priv->relay_en_gpio)) {
		ret = gpio_request(priv->relay_en_gpio, "audio relay");
		if (ret != 0) {
			dev_err(&pdev->dev, "gpio_request for gpio %d failed:%d\n",
					priv->relay_en_gpio, ret);

			goto err;
		}

		gpio_direction_output(priv->relay_en_gpio, 0);
	}

	if (gpio_is_valid(priv->amp_mute_gpio)) {
		ret = gpio_request(priv->amp_mute_gpio, "amp mute");
		if (ret != 0) {
			dev_err(&pdev->dev, "gpio_request for gpio %d failed:%d\n",
                    priv->amp_mute_gpio, ret);

			goto err;
		}

        /* Keep the amplifier muted by default to avoid clicks */
		gpio_direction_output(priv->amp_mute_gpio, 1);
	}

	/* Add controls */
    ret = snd_soc_add_card_controls(card, amp_mute_controls,
                                ARRAY_SIZE(amp_mute_controls));
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: Failed to add amp-mute-controls (%d)!\n",
				__func__, ret);
        goto err;
	}
	return ret;

err:
	if (gpio_is_valid(priv->relay_en_gpio))
		gpio_free(priv->relay_en_gpio);

	if (gpio_is_valid(priv->amp_mute_gpio))
		gpio_free(priv->amp_mute_gpio);

	return ret;
}

static int trav_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct trav_audio_priv *priv = snd_soc_card_get_drvdata(card);

	snd_soc_unregister_card(card);
	if (gpio_is_valid(priv->relay_en_gpio))
		gpio_free(priv->relay_en_gpio);

	if (gpio_is_valid(priv->amp_mute_gpio))
		gpio_free(priv->amp_mute_gpio);

	kfree(priv);
	return 0;
}

static void trav_audio_shutdown(struct platform_device *pdev)
{
    /* Clean up memory allocated for sound controls during probe */
    trav_audio_remove(pdev);
}

static const struct of_device_id samsung_audio_of_match[] = {
	{ .compatible = "samsung,trav-audio", },
	{},
};
MODULE_DEVICE_TABLE(of, samsung_audio_of_match);

static struct platform_driver trav_audio_driver = {
	.driver		= {
		.name	= "trav-audio",
		.owner	= THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = of_match_ptr(samsung_audio_of_match),
	},
	.probe		= trav_audio_probe,
	.remove		= trav_audio_remove,
    .shutdown   = trav_audio_shutdown,
};

module_platform_driver(trav_audio_driver);

MODULE_DESCRIPTION("ALSA SoC trav audio");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:trav-audio");
