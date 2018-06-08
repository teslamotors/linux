/*
 * svfpga.c -- SVFPGA ALSA SoC audio driver
 *
 * Copyright 2015 Intel, Inc.
 *
 * Author: Hardik Shah <hardik.t.shah@intel.com>
 * Dummy ASoC Codec Driver based Cirrus Logic CS42L42 Codec
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

/* #define DEBUG 1 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/pm_runtime.h>
#include <linux/pm.h>
#include <linux/sdw_bus.h>
#include <linux/gpio.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio/consumer.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "svfpga.h"

struct sdw_stream_data {
	int stream_tag;
};



static const struct reg_default svfpga_reg_defaults[] = {
};

static bool svfpga_readable_register(struct device *dev, unsigned int reg)
{
	return false;
}

static bool svfpga_volatile_register(struct device *dev, unsigned int reg)
{
	return false;
}

static const struct regmap_range_cfg svfpga_page_range = {
	.name = "Pages",
	.range_min = 0,
	.range_max = 0,
	.selector_reg = SVFPGA_PAGE_REGISTER,
	.selector_mask = 0xff,
	.selector_shift = 0,
	.window_start = 0,
	.window_len = 256,
};

const struct regmap_config svfpga_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.readable_reg = svfpga_readable_register,
	.volatile_reg = svfpga_volatile_register,

	.ranges = &svfpga_page_range,
	.num_ranges = 1,

	.max_register = 0,
	.reg_defaults = svfpga_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(svfpga_reg_defaults),
	.cache_type = REGCACHE_RBTREE,
};
EXPORT_SYMBOL(svfpga_i2c_regmap);

const struct regmap_config svfpga_sdw_regmap = {
	.reg_bits = 32,
	.val_bits = 8,

	.readable_reg = svfpga_readable_register,
	.volatile_reg = svfpga_volatile_register,

	.max_register = SVFPGA_MAX_REGISTER,
	.reg_defaults = svfpga_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(svfpga_reg_defaults),
	.cache_type = REGCACHE_RBTREE,
};
EXPORT_SYMBOL(svfpga_sdw_regmap);

static int svfpga_hpdrv_evt(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	return 0;
}

static const struct snd_soc_dapm_widget svfpga_dapm_widgets[] = {
	SND_SOC_DAPM_OUTPUT("HP"),
	SND_SOC_DAPM_INPUT("I2NP"),
	SND_SOC_DAPM_AIF_IN("SDIN", NULL, 0, SVFPGA_ASP_CLK_CFG,
					SVFPGA_ASP_SCLK_EN_SHIFT, false),
	SND_SOC_DAPM_OUT_DRV_E("HPDRV", SND_SOC_NOPM, 0,
					0, NULL, 0, svfpga_hpdrv_evt,
					SND_SOC_DAPM_POST_PMU |
					SND_SOC_DAPM_PRE_PMD)
};

static const struct snd_soc_dapm_route svfpga_audio_map[] = {
	{"SDIN", NULL, "Playback"},
	{"HPDRV", NULL, "SDIN"},
	{"HP", NULL, "HPDRV"},
	{"Capture", NULL, "I2NP"},

};

static int svfpga_set_bias_level(struct snd_soc_codec *codec,
					enum snd_soc_bias_level level)
{
	return 0;
}

static const struct snd_soc_codec_driver soc_codec_dev_svfpga = {
	.set_bias_level = svfpga_set_bias_level,
	.component_driver = {
		.dapm_widgets = svfpga_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(svfpga_dapm_widgets),
		.dapm_routes = svfpga_audio_map,
		.num_dapm_routes = ARRAY_SIZE(svfpga_audio_map),
	},
};

static int svfpga_program_stream_tag(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai, int stream_tag)
{
	struct sdw_stream_data *stream_data;

	stream_data = kzalloc(sizeof(*stream_data), GFP_KERNEL);
	if (!stream_data)
		return -ENOMEM;
	stream_data->stream_tag = stream_tag;
	snd_soc_dai_set_dma_data(dai, substream, stream_data);
	return 0;
}

static int svfpga_remove_stream_tag(struct snd_pcm_substream *substream,
       struct snd_soc_dai *dai)

{
	struct sdw_stream_data *stream_data;

	stream_data = snd_soc_dai_get_dma_data(dai, substream);
	kfree(stream_data);
	return 0;
}



static int svfpga_pcm_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *params,
				     struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct svfpga_private *svfpga = snd_soc_codec_get_drvdata(codec);
	int retval;
	enum sdw_data_direction direction;
	struct sdw_stream_config stream_config;
	struct sdw_port_config port_config;
	struct sdw_port_cfg port_cfg;
	struct sdw_stream_data *stream;
	int port;
	int num_channels;
	int upscale_factor = 16;

	stream = snd_soc_dai_get_dma_data(dai, substream);

	if (!svfpga->sdw)
		return 0;

	/* SoundWire specific configuration */
	/* This code assumes port 1 for playback and port 2 for capture */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		direction = SDW_DATA_DIR_IN;
		port = 2;
	} else {
		direction = SDW_DATA_DIR_OUT;
		port = 0;
	}
	stream_config.frame_rate =  params_rate(params);
	stream_config.frame_rate *= upscale_factor;
	stream_config.channel_count = params_channels(params);
	stream_config.bps =
			snd_pcm_format_width(params_format(params));
	stream_config.bps = 1;
	stream_config.direction = direction;
	retval = sdw_config_stream(svfpga->sdw->mstr, svfpga->sdw, &stream_config,
							stream->stream_tag);
	if (retval) {
		dev_err(dai->dev, "Unable to configure the stream\n");
		return retval;
	}
	port_config.num_ports = 1;
	port_config.port_cfg = &port_cfg;
	port_cfg.port_num = port;
	num_channels = params_channels(params);
	port_cfg.ch_mask = (1 << (num_channels))  - 1;
	retval = sdw_config_port(svfpga->sdw->mstr, svfpga->sdw,
		&port_config, stream->stream_tag);
	if (retval) {
		dev_err(dai->dev, "Unable to configure port\n");
		return retval;
	}

	return retval;
}

int svfpga_pcm_hw_free(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct svfpga_private *svfpga = snd_soc_codec_get_drvdata(codec);
	struct sdw_stream_data *stream = snd_soc_dai_get_dma_data(dai,
			substream);
	if (!svfpga->sdw)
		return 0;
	sdw_release_stream(svfpga->sdw->mstr, svfpga->sdw, stream->stream_tag);
	return 0;
}

static struct snd_soc_dai_ops svfpga_ops = {
	.hw_params	= svfpga_pcm_hw_params,
	.hw_free	= svfpga_pcm_hw_free,
	.program_stream_tag     = svfpga_program_stream_tag,
	.remove_stream_tag      = svfpga_remove_stream_tag,
};

static struct snd_soc_dai_driver svfpga_dai = {
		.name = "svfpga",
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &svfpga_ops,
};

int svfpga_probe(struct device *dev, struct regmap *regmap,
					struct sdw_slave *slave)
{
	struct svfpga_private *svfpga;
	int ret;
	struct sdw_msg rd_msg, wr_msg;
	u8 rbuf[1] = {0}, wbuf[1] = {0};


	svfpga = devm_kzalloc(dev, sizeof(struct svfpga_private),
			       GFP_KERNEL);
	if (!svfpga)
		return -ENOMEM;

	dev_set_drvdata(dev, svfpga);

	svfpga->regmap = regmap;
	svfpga->sdw = slave;

	ret =  snd_soc_register_codec(dev,
			&soc_codec_dev_svfpga, &svfpga_dai, 1);
	dev_info(&slave->dev, "Configuring codec for pattern\n");

	wr_msg.ssp_tag = 0;
	wr_msg.flag = SDW_MSG_FLAG_WRITE;
	wr_msg.addr = 0x8;
	wr_msg.len = 1;
	wr_msg.buf = wbuf;
	wr_msg.slave_addr = slave->slv_number;
	wr_msg.addr_page1 = 0x0;
	wr_msg.addr_page2 = 0x0;
	wbuf[0] = 0x0;
	ret = sdw_slave_transfer(slave->mstr, &wr_msg, 1);
	if (ret != 1) {
		ret = -EINVAL;
		dev_err(&slave->dev, "Interrupt status read failed for slave %x\n", slave->slv_number);
		goto out;
	}
	wr_msg.addr = 0x1804;
	wbuf[0] = 0x1;
	ret = sdw_slave_transfer(slave->mstr, &wr_msg, 1);
	if (ret != 1) {
		ret = -EINVAL;
		dev_err(&slave->dev, "Interrupt status read failed for slave %x\n", slave->slv_number);
		goto out;
	}
	rd_msg.ssp_tag = 0;
	rd_msg.flag = SDW_MSG_FLAG_READ;
	rd_msg.addr = 0x180b;
	rd_msg.len = 1;
	rd_msg.buf = rbuf;
	rd_msg.slave_addr = slave->slv_number;
	rd_msg.addr_page1 = 0x0;
	rd_msg.addr_page2 = 0x0;
	ret = sdw_slave_transfer(slave->mstr, &rd_msg, 1);
	if (ret != 1) {
		ret = -EINVAL;
		dev_err(&slave->dev, "Interrupt status read failed for slave %x\n", slave->slv_number);
		goto out;
	}
	wr_msg.addr = 0x180b;
	wbuf[0] = 0x1a;
	ret = sdw_slave_transfer(slave->mstr, &wr_msg, 1);
	if (ret != 1) {
		ret = -EINVAL;
		dev_err(&slave->dev, "Interrupt status read failed for slave %x\n", slave->slv_number);
		goto out;
	}
	wr_msg.addr = 0x180c;
	wbuf[0] = 0x0;
	ret = sdw_slave_transfer(slave->mstr, &wr_msg, 1);
	if (ret != 1) {
		ret = -EINVAL;
		dev_err(&slave->dev, "Interrupt status read failed for slave %x\n", slave->slv_number);
		goto out;
	}
	wr_msg.addr = 0x180d;
	wbuf[0] = 0x0;
	ret = sdw_slave_transfer(slave->mstr, &wr_msg, 1);
	if (ret != 1) {
		ret = -EINVAL;
		dev_err(&slave->dev, "Interrupt status read failed for slave %x\n", slave->slv_number);
		goto out;
	}
	wr_msg.addr = 0x180e;
	wbuf[0] = 0x0;
	ret = sdw_slave_transfer(slave->mstr, &wr_msg, 1);
	if (ret != 1) {
		ret = -EINVAL;
		dev_err(&slave->dev, "Interrupt status read failed for slave %x\n", slave->slv_number);
		goto out;
	}
	wr_msg.addr = 0x180f;
	wbuf[0] = 0x0;
	ret = sdw_slave_transfer(slave->mstr, &wr_msg, 1);
	if (ret != 1) {
		ret = -EINVAL;
		dev_err(&slave->dev, "Interrupt status read failed for slave %x\n", slave->slv_number);
		goto out;
	}
	wr_msg.addr = 0x1810;
	wbuf[0] = 0xb8;
	ret = sdw_slave_transfer(slave->mstr, &wr_msg, 1);
	if (ret != 1) {
		ret = -EINVAL;
		dev_err(&slave->dev, "Interrupt status read failed for slave %x\n", slave->slv_number);
		goto out;
	}
	wr_msg.addr = 0x1811;
	wbuf[0] = 0xd;
	ret = sdw_slave_transfer(slave->mstr, &wr_msg, 1);
	if (ret != 1) {
		ret = -EINVAL;
		dev_err(&slave->dev, "Interrupt status read failed for slave %x\n", slave->slv_number);
		goto out;
	}
	wr_msg.addr = 0x1812;
	wbuf[0] = 0x0;
	ret = sdw_slave_transfer(slave->mstr, &wr_msg, 1);
	if (ret != 1) {
		ret = -EINVAL;
		dev_err(&slave->dev, "Interrupt status read failed for slave %x\n", slave->slv_number);
		goto out;
	}
	wr_msg.addr = 0x1813;
	wbuf[0] = 0x0;
	ret = sdw_slave_transfer(slave->mstr, &wr_msg, 1);
	if (ret != 1) {
		ret = -EINVAL;
		dev_err(&slave->dev, "Interrupt status read failed for slave %x\n", slave->slv_number);
		goto out;
	}
	wr_msg.addr = 0x181c;
	wbuf[0] = 0x6e;
	ret = sdw_slave_transfer(slave->mstr, &wr_msg, 1);
	if (ret != 1) {
		ret = -EINVAL;
		dev_err(&slave->dev, "Interrupt status read failed for slave %x\n", slave->slv_number);
		goto out;
	}
	wr_msg.addr = 0x181d;
	wbuf[0] = 0x3;
	ret = sdw_slave_transfer(slave->mstr, &wr_msg, 1);
	if (ret != 1) {
		ret = -EINVAL;
		dev_err(&slave->dev, "Interrupt status read failed for slave %x\n", slave->slv_number);
		goto out;
	}
	wr_msg.addr = 0x181e;
	wbuf[0] = 0x0;
	ret = sdw_slave_transfer(slave->mstr, &wr_msg, 1);
	if (ret != 1) {
		ret = -EINVAL;
		dev_err(&slave->dev, "Interrupt status read failed for slave %x\n", slave->slv_number);
		goto out;
	}
	wr_msg.addr = 0x181f;
	wbuf[0] = 0x0;
	ret = sdw_slave_transfer(slave->mstr, &wr_msg, 1);
	if (ret != 1) {
		ret = -EINVAL;
		dev_err(&slave->dev, "Interrupt status read failed for slave %x\n", slave->slv_number);
		goto out;
	}
	wr_msg.addr = 0x180b;
	wbuf[0] = 0x1b;
	ret = sdw_slave_transfer(slave->mstr, &wr_msg, 1);
	if (ret != 1) {
		ret = -EINVAL;
		dev_err(&slave->dev, "Interrupt status read failed for slave %x\n", slave->slv_number);
		goto out;
	}
	rd_msg.addr = 0x180b;
	rbuf[0] = 0x0;
	ret = sdw_slave_transfer(slave->mstr, &rd_msg, 1);
	if (ret != 1) {
		ret = -EINVAL;
		dev_err(&slave->dev, "Interrupt status read failed for slave %x\n", slave->slv_number);
		goto out;
	}
	dev_info(&slave->dev, "Codec programmed with Value %x\n", rbuf[0]);
	return ret;
out:
	return -EIO;
}
EXPORT_SYMBOL(svfpga_probe);

int svfpga_remove(struct device *dev)
{

	snd_soc_unregister_codec(dev);

	dev_info(dev, "Removing\n");

	return 0;
}
EXPORT_SYMBOL(svfpga_remove);

#ifdef CONFIG_PM
static int svfpga_runtime_suspend(struct device *dev)
{
	return 0;
}

static int svfpga_runtime_resume(struct device *dev)
{

	return 0;
}
#endif

const struct dev_pm_ops svfpga_runtime_pm = {
	SET_RUNTIME_PM_OPS(svfpga_runtime_suspend, svfpga_runtime_resume,
			   NULL)
};
EXPORT_SYMBOL(svfpga_runtime_pm);

MODULE_DESCRIPTION("ASoC SVFPGA driver");
MODULE_DESCRIPTION("ASoC SVFPGA driver SDW");
MODULE_AUTHOR("Hardik shah, Intel Inc, <hardik.t.shah@intel.com");
MODULE_LICENSE("GPL");
