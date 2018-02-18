/*
 * escore-i2s.c  --  Audience eScore I2S interface
 *
 * Copyright 2011 Audience, Inc.
 *
 * Author: Greg Clemson <gclemson@audience.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/completion.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/kthread.h>
#include "escore.h"
#include "escore-list.h"

static int escore_i2s_set_sysclk(struct snd_soc_dai *dai, int clk_id,
				unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s()\n", __func__);

	if (escore_priv.i2s_dai_ops.set_sysclk)
		rc = escore_priv.i2s_dai_ops.set_sysclk(dai, clk_id,
				freq, dir);

	return rc;
}

static int escore_i2s_set_pll(struct snd_soc_dai *dai, int pll_id,
			     int source, unsigned int freq_in,
			     unsigned int freq_out)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s()\n", __func__);

	if (escore_priv.i2s_dai_ops.set_pll)
		rc = escore_priv.i2s_dai_ops.set_pll(dai, pll_id,
				source, freq_in, freq_out);

	return rc;
}

static int escore_i2s_set_clkdiv(struct snd_soc_dai *dai, int div_id,
				int div)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s()\n", __func__);

	if (escore_priv.i2s_dai_ops.set_clkdiv)
		rc = escore_priv.i2s_dai_ops.set_clkdiv(dai, div_id,
				div);

	return rc;
}

static int escore_i2s_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s()\n", __func__);

	if (escore_priv.i2s_dai_ops.set_fmt)
		rc = escore_priv.i2s_dai_ops.set_fmt(dai, fmt);

	return rc;
}

static int escore_i2s_set_tdm_slot(struct snd_soc_dai *dai,
				  unsigned int tx_mask, unsigned int rx_mask,
				  int slots, int slot_width)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s()\n", __func__);

	if (escore_priv.i2s_dai_ops.set_tdm_slot)
		rc = escore_priv.i2s_dai_ops.set_tdm_slot(dai, tx_mask,
				rx_mask, slots, slot_width);

	return rc;
}

static int escore_i2s_set_channel_map(struct snd_soc_dai *dai,
				     unsigned int tx_num, unsigned int *tx_slot,
				     unsigned int rx_num, unsigned int *rx_slot)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s()\n", __func__);

	if (escore_priv.i2s_dai_ops.set_channel_map)
		rc = escore_priv.i2s_dai_ops.set_channel_map(dai, tx_num,
				tx_slot, rx_num, rx_slot);

	return rc;
}

static int escore_i2s_set_tristate(struct snd_soc_dai *dai, int tristate)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s()\n", __func__);

	if (escore_priv.i2s_dai_ops.set_tristate)
		rc = escore_priv.i2s_dai_ops.set_tristate(dai, tristate);

	return rc;
}

#if defined(CONFIG_ARCH_MSM)
static int escore_i2s_get_channel_map(struct snd_soc_dai *dai,
				      unsigned int *tx_num,
				      unsigned int *tx_slot,
				      unsigned int *rx_num,
				      unsigned int *rx_slot)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s()\n", __func__);

	if (escore_priv.i2s_dai_ops.get_channel_map)
		rc = escore_priv.i2s_dai_ops.get_channel_map(dai, tx_num,
				tx_slot, rx_num, rx_slot);

	return rc;
}
#endif

static int escore_i2s_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s() : mute = %d\n", __func__, mute);

	if (escore_priv.i2s_dai_ops.digital_mute)
		rc = escore_priv.i2s_dai_ops.digital_mute(dai, mute);

	return rc;
}

static int escore_i2s_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s(): dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);

	if (escore_priv.i2s_dai_ops.startup)
		rc = escore_priv.i2s_dai_ops.startup(substream, dai);

	return rc;
}

static void escore_i2s_shutdown(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	dev_dbg(codec->dev, "%s(): dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);

	if (escore_priv.i2s_dai_ops.shutdown)
		escore_priv.i2s_dai_ops.shutdown(substream, dai);

}

static int escore_i2s_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s(): dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);

	dev_dbg(codec->dev, "%s(): params_channels(params) = %d\n", __func__,
		params_channels(params));


	if (escore_priv.i2s_dai_ops.hw_params)
		rc = escore_priv.i2s_dai_ops.hw_params(substream, params,
				dai);

	return rc;
}

static int escore_i2s_hw_free(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s() dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);

	if (escore_priv.i2s_dai_ops.hw_free)
		rc = escore_priv.i2s_dai_ops.hw_free(substream, dai);

	return rc;
}

static int escore_i2s_prepare(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s() dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);

	if (escore_priv.i2s_dai_ops.prepare)
		rc = escore_priv.i2s_dai_ops.prepare(substream, dai);

	return rc;
}

static int escore_i2s_trigger(struct snd_pcm_substream *substream,
			     int cmd, struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s() dai->name = %s, dai->id = %d\n", __func__,
			dai->name, dai->id);

	if (escore_priv.i2s_dai_ops.trigger)
		rc = escore_priv.i2s_dai_ops.trigger(substream, cmd, dai);

	return rc;
}

struct snd_soc_dai_ops escore_i2s_port_dai_ops = {
	.set_sysclk	= escore_i2s_set_sysclk,
	.set_pll	= escore_i2s_set_pll,
	.set_clkdiv	= escore_i2s_set_clkdiv,
	.set_fmt	= escore_i2s_set_fmt,
	.set_tdm_slot	= escore_i2s_set_tdm_slot,
	.set_channel_map	= escore_i2s_set_channel_map,
#if defined(CONFIG_ARCH_MSM)
	.get_channel_map	= escore_i2s_get_channel_map,
#endif
	.set_tristate	= escore_i2s_set_tristate,
	.digital_mute	= escore_i2s_digital_mute,
	.startup	= escore_i2s_startup,
	.shutdown	= escore_i2s_shutdown,
	.hw_params	= escore_i2s_hw_params,
	.hw_free	= escore_i2s_hw_free,
	.prepare	= escore_i2s_prepare,
	.trigger	= escore_i2s_trigger,
};


MODULE_DESCRIPTION("ASoC ES driver");
MODULE_AUTHOR("Greg Clemson <gclemson@audience.com>");
MODULE_LICENSE("GPL");
