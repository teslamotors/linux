/*
 * tegra210_i2s.c - Tegra210 I2S driver
 *
 * Copyright (c) 2014-2016 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/pinconf-tegra.h>
#include <linux/regulator/consumer.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/tegra-powergate.h>
#include <linux/tegra-soc.h>
#include <linux/pm_domain.h>
#include <linux/tegra_pm_domains.h>

#include "tegra210_xbar_alt.h"
#include "tegra210_i2s_alt.h"

#define DRV_NAME "tegra210-i2s"

#if defined(CONFIG_ARCH_TEGRA_21X_SOC)
#ifdef CONFIG_PM_GENERIC_DOMAINS_OF
static struct of_device_id tegra_ape_pd[] = {
	{ .compatible = "nvidia,tegra210-ape-pd", },
	{},
};
#endif
#endif

static const struct reg_default tegra210_i2s_reg_defaults[] = {
	{ TEGRA210_I2S_AXBAR_RX_INT_MASK, 0x00000003},
	{ TEGRA210_I2S_AXBAR_RX_CIF_CTRL, 0x00007700},
	{ TEGRA210_I2S_AXBAR_TX_INT_MASK, 0x00000003},
	{ TEGRA210_I2S_AXBAR_TX_CIF_CTRL, 0x00007700},
	{ TEGRA210_I2S_CG, 0x1},
	{ TEGRA210_I2S_TIMING, 0x0000001f},
};

static void tegra210_i2s_set_slot_ctrl(struct regmap *regmap,
				unsigned int total_slots,
				unsigned int tx_slot_mask,
				unsigned int rx_slot_mask)
{
	regmap_write(regmap, TEGRA210_I2S_SLOT_CTRL, total_slots - 1);
	regmap_write(regmap, TEGRA210_I2S_AXBAR_TX_SLOT_CTRL,
		tx_slot_mask);
	regmap_write(regmap, TEGRA210_I2S_AXBAR_RX_SLOT_CTRL,
		rx_slot_mask);
}

static int tegra210_i2s_set_clock_rate(struct device *dev, int clock_rate)
{
	unsigned int val;
	struct tegra210_i2s *i2s = dev_get_drvdata(dev);
	int ret = 0;

	regmap_read(i2s->regmap, TEGRA210_I2S_CTRL, &val);

	if (!(tegra_platform_is_unit_fpga() || tegra_platform_is_fpga())) {
		if (((val & TEGRA210_I2S_CTRL_MASTER_EN_MASK) ==
				TEGRA210_I2S_CTRL_MASTER_EN)) {

			ret = clk_set_rate(i2s->clk_i2s_sync, clock_rate);
			if (ret) {
				dev_err(dev, "Can't set I2S sync clock rate\n");
				return ret;
			}
#if !defined(CONFIG_ARCH_TEGRA_21x_SOC)
			ret = clk_set_parent(i2s->clk_audio_sync,
					i2s->clk_i2s_sync);
			if (ret) {
				dev_err(dev,
					"Can't set parent of i2s audio sync clock\n");
				return ret;
			}
#endif
			ret = clk_set_parent(i2s->clk_i2s, i2s->clk_i2s_source);
			if (ret) {
				dev_err(dev, "Can't set parent of I2S clock\n");
				return ret;
			}

			ret = clk_set_rate(i2s->clk_i2s, clock_rate);
			if (ret) {
				dev_err(dev,
					"Can't set I2S clock rate: %d\n", ret);
				return ret;
			}
		}
	}

	return ret;
}

static int tegra210_i2s_sw_reset(struct tegra210_i2s *i2s,
				int direction, int timeout)
{
	unsigned int sw_reset_reg, sw_reset_mask, sw_reset_en, sw_reset_default;
	unsigned int tx_cif_ctrl, rx_cif_ctrl, tx_ctrl, rx_ctrl, ctrl, val;
	int wait = timeout;

	regmap_read(i2s->regmap, TEGRA210_I2S_AXBAR_TX_CIF_CTRL, &tx_cif_ctrl);
	regmap_read(i2s->regmap, TEGRA210_I2S_AXBAR_RX_CIF_CTRL, &rx_cif_ctrl);
	regmap_read(i2s->regmap, TEGRA210_I2S_AXBAR_TX_CTRL, &tx_ctrl);
	regmap_read(i2s->regmap, TEGRA210_I2S_AXBAR_RX_CTRL, &rx_ctrl);
	regmap_read(i2s->regmap, TEGRA210_I2S_CTRL, &ctrl);

	if (direction == SNDRV_PCM_STREAM_PLAYBACK) {
		sw_reset_reg = TEGRA210_I2S_AXBAR_RX_SOFT_RESET;
		sw_reset_mask = TEGRA210_I2S_AXBAR_RX_SOFT_RESET_MASK;
		sw_reset_en = TEGRA210_I2S_AXBAR_RX_SOFT_RESET_EN;
		sw_reset_default = TEGRA210_I2S_AXBAR_RX_SOFT_RESET_DEFAULT;
	} else {
		sw_reset_reg = TEGRA210_I2S_AXBAR_TX_SOFT_RESET;
		sw_reset_mask = TEGRA210_I2S_AXBAR_TX_SOFT_RESET_MASK;
		sw_reset_en = TEGRA210_I2S_AXBAR_TX_SOFT_RESET_EN;
		sw_reset_default = TEGRA210_I2S_AXBAR_TX_SOFT_RESET_DEFAULT;
	}

	regmap_update_bits(i2s->regmap, sw_reset_reg, sw_reset_mask, sw_reset_en);

	do {
		regmap_read(i2s->regmap, sw_reset_reg, &val);
		wait--;
		if (!wait)
			return -EINVAL;
	} while (val & sw_reset_mask);

	regmap_update_bits(i2s->regmap, sw_reset_reg, sw_reset_mask, sw_reset_default);

	regmap_write(i2s->regmap, TEGRA210_I2S_AXBAR_TX_CIF_CTRL, tx_cif_ctrl);
	regmap_write(i2s->regmap, TEGRA210_I2S_AXBAR_RX_CIF_CTRL, rx_cif_ctrl);
	regmap_write(i2s->regmap, TEGRA210_I2S_AXBAR_TX_CTRL, tx_ctrl);
	regmap_write(i2s->regmap, TEGRA210_I2S_AXBAR_RX_CTRL, rx_ctrl);
	regmap_write(i2s->regmap, TEGRA210_I2S_CTRL, ctrl);

	return 0;
}

static int tegra210_i2s_get_status(struct tegra210_i2s *i2s,
				int direction)
{
	unsigned int status_reg, val;

	status_reg = (direction == SNDRV_PCM_STREAM_PLAYBACK) ?
		TEGRA210_I2S_AXBAR_RX_STATUS :
		TEGRA210_I2S_AXBAR_TX_STATUS;

	regmap_read(i2s->regmap, status_reg, &val);
	val = val & 0x00000001;

	return val;
}

static int tegra210_i2s_rx_stop(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct device *dev = codec->dev;
	struct tegra210_i2s *i2s = dev_get_drvdata(dev);
	int dcnt = 10, ret;

	/* wait until I2S RX status is disabled;
	ADMAIF is first disabled followed by I2S */
	while (tegra210_i2s_get_status(i2s, SNDRV_PCM_STREAM_CAPTURE) &&
			dcnt--)
		udelay(100);

	/* HW needs sw reset to make sure previous transaction be clean */
	ret = tegra210_i2s_sw_reset(i2s, SNDRV_PCM_STREAM_CAPTURE, 0xffff);
	if (ret) {
		dev_err(dev, "Failed at I2S%d_RX sw reset\n", dev->id);
		return ret;
	}
	return 0;
}

static int tegra210_i2s_tx_stop(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct device *dev = codec->dev;
	struct tegra210_i2s *i2s = dev_get_drvdata(dev);
	int dcnt = 10, ret;

	/* wait until  I2S TX status is disabled;
	ADMAIF is first disabled followed by I2S */
	while (tegra210_i2s_get_status(i2s, SNDRV_PCM_STREAM_PLAYBACK) &&
			dcnt--)
		udelay(100);

	/* HW needs sw reset to make sure previous transaction be clean */
	ret = tegra210_i2s_sw_reset(i2s, SNDRV_PCM_STREAM_PLAYBACK, 0xffff);
	if (ret) {
		dev_err(dev, "Failed at I2S%d_TX sw reset\n", dev->id);
		return ret;
	}
	return 0;
}

static int tegra210_i2s_runtime_suspend(struct device *dev)
{
	struct tegra210_i2s *i2s = dev_get_drvdata(dev);
	int ret;

	if (!(tegra_platform_is_unit_fpga() || tegra_platform_is_fpga())) {
		if (!IS_ERR(i2s->pin_idle_state)) {
			ret = pinctrl_select_state(
				i2s->pinctrl, i2s->pin_idle_state);
			if (ret < 0)
				dev_err(dev, "setting dap pinctrl idle state failed\n");
		}

		if (i2s->num_supplies > 0) {
			ret = regulator_bulk_disable(i2s->num_supplies,
								i2s->supplies);
			if (ret < 0)
				dev_err(dev, "failed to disable i2s io regulator\n");
		}

		regcache_cache_only(i2s->regmap, true);
		regcache_mark_dirty(i2s->regmap);

		clk_disable_unprepare(i2s->clk_i2s);
	} else
		regcache_cache_only(i2s->regmap, true);

	pm_runtime_put_sync(dev->parent);

	return 0;
}

static int tegra210_i2s_runtime_resume(struct device *dev)
{
	struct tegra210_i2s *i2s = dev_get_drvdata(dev);
	int ret;

	ret = pm_runtime_get_sync(dev->parent);
	if (ret < 0) {
		dev_err(dev, "parent get_sync failed: %d\n", ret);
		return ret;
	}

	if (!(tegra_platform_is_unit_fpga() || tegra_platform_is_fpga())) {
		if (!IS_ERR(i2s->pin_default_state)) {
			ret = pinctrl_select_state(i2s->pinctrl,
						i2s->pin_default_state);
			if (ret < 0)
				dev_err(dev, "setting dap pinctrl default state failed\n");
		}

		if (i2s->num_supplies > 0) {
			ret = regulator_bulk_enable(i2s->num_supplies,
								i2s->supplies);
			if (ret < 0)
				dev_err(dev, "failed to enable i2s io regulator\n");
		}

		ret = clk_prepare_enable(i2s->clk_i2s);
		if (ret) {
			dev_err(dev, "clk_enable failed: %d\n", ret);
			return ret;
		}
	}

	regcache_cache_only(i2s->regmap, false);
	regcache_sync(i2s->regmap);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tegra210_i2s_suspend(struct device *dev)
{
	return 0;
}
#endif

static int tegra210_i2s_set_fmt(struct snd_soc_dai *dai,
				unsigned int fmt)
{
	struct tegra210_i2s *i2s = snd_soc_dai_get_drvdata(dai);
	unsigned int mask, val, data_offset;

	mask = TEGRA210_I2S_CTRL_EDGE_CTRL_MASK;
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		val = TEGRA210_I2S_CTRL_EDGE_CTRL_POS_EDGE;
		val |= TEGRA210_I2S_CTRL_LRCK_POLARITY_LOW;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		val = TEGRA210_I2S_CTRL_EDGE_CTRL_POS_EDGE;
		val |= TEGRA210_I2S_CTRL_LRCK_POLARITY_HIGH;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		val = TEGRA210_I2S_CTRL_EDGE_CTRL_NEG_EDGE;
		val |= TEGRA210_I2S_CTRL_LRCK_POLARITY_LOW;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		val = TEGRA210_I2S_CTRL_EDGE_CTRL_NEG_EDGE;
		val |= TEGRA210_I2S_CTRL_LRCK_POLARITY_HIGH;
		break;
	default:
		return -EINVAL;
	}

	mask |= TEGRA210_I2S_CTRL_MASTER_EN_MASK;
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		val &= ~(TEGRA210_I2S_CTRL_MASTER_EN);
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		val |= TEGRA210_I2S_CTRL_MASTER_EN;
		break;
	default:
		return -EINVAL;
	}

	mask |= TEGRA210_I2S_CTRL_FRAME_FORMAT_MASK |
		TEGRA210_I2S_CTRL_LRCK_POLARITY_MASK;
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_DSP_A:
		val |= TEGRA210_I2S_CTRL_FRAME_FORMAT_FSYNC_MODE;
		data_offset = 1;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		val |= TEGRA210_I2S_CTRL_FRAME_FORMAT_FSYNC_MODE;
		data_offset = 0;
		break;
	/* I2S mode has data offset of 1 */
	case SND_SOC_DAIFMT_I2S:
		val |= TEGRA210_I2S_CTRL_FRAME_FORMAT_LRCK_MODE;
		data_offset = 1;
		break;
	/* LJ/RJ mode assumed to operate at bclk = 64fs */
	case SND_SOC_DAIFMT_RIGHT_J:
		val |= TEGRA210_I2S_CTRL_FRAME_FORMAT_LRCK_MODE;
		data_offset = 16;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		val |= TEGRA210_I2S_CTRL_FRAME_FORMAT_LRCK_MODE;
		data_offset = 0;
		break;
	default:
		return -EINVAL;
	}

	pm_runtime_get_sync(dai->dev);
	regmap_update_bits(i2s->regmap, TEGRA210_I2S_CTRL, mask, val);
	/* FIXME: global enabling */
	regmap_update_bits(i2s->regmap, TEGRA210_I2S_ENABLE,
					TEGRA210_I2S_EN_MASK, TEGRA210_I2S_EN);
	/* set I2S data offset */
	regmap_update_bits(i2s->regmap, TEGRA210_I2S_AXBAR_TX_CTRL,
		TEGRA210_I2S_AXBAR_TX_CTRL_DATA_OFFSET_MASK,
		(data_offset << TEGRA210_I2S_AXBAR_TX_CTRL_DATA_OFFSET_SHIFT));
	regmap_update_bits(i2s->regmap, TEGRA210_I2S_AXBAR_RX_CTRL,
		TEGRA210_I2S_AXBAR_RX_CTRL_DATA_OFFSET_MASK,
		(data_offset << TEGRA210_I2S_AXBAR_RX_CTRL_DATA_OFFSET_SHIFT));
	regmap_update_bits(i2s->regmap, TEGRA210_I2S_CTRL,
		TEGRA210_I2S_CTRL_FSYNC_WIDTH_MASK,
		i2s->fsync_width << TEGRA210_I2S_CTRL_FSYNC_WIDTH_SHIFT);
	pm_runtime_put(dai->dev);

	return 0;
}

static int tegra210_i2s_set_tdm_slot(struct snd_soc_dai *dai,
		unsigned int tx_mask, unsigned int rx_mask,
		int slots, int slot_width)
{
	struct tegra210_i2s *i2s = snd_soc_dai_get_drvdata(dai);

	/* copy the required tx and rx mask */
	i2s->tx_mask = (tx_mask > 0xFFFF) ? 0xFFFF : tx_mask;
	i2s->rx_mask = (rx_mask > 0xFFFF) ? 0xFFFF : rx_mask;

	return 0;
}

static int tegra210_i2s_set_dai_bclk_ratio(struct snd_soc_dai *dai,
		unsigned int ratio)
{
	struct tegra210_i2s *i2s = snd_soc_dai_get_drvdata(dai);
	i2s->bclk_ratio = ratio;
	return 0;
}

static int tegra210_i2s_get_format(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_i2s *i2s = snd_soc_codec_get_drvdata(codec);

	/* get the format control flag */
	if (strstr(kcontrol->id.name, "input"))
		ucontrol->value.integer.value[0] = i2s->format_in;
	else if (strstr(kcontrol->id.name, "codec"))
		ucontrol->value.integer.value[0] = i2s->codec_bit_format;
	return 0;
}

static int tegra210_i2s_put_format(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_i2s *i2s = snd_soc_codec_get_drvdata(codec);

	/* set the format control flag */
	if (strstr(kcontrol->id.name, "input"))
		i2s->format_in = ucontrol->value.integer.value[0];
	else if (strstr(kcontrol->id.name, "codec"))
		i2s->codec_bit_format = ucontrol->value.integer.value[0];

	return 0;
}

static const char * const tegra210_i2s_format_text[] = {
	"None",
	"16",
	"32",
};

static const int tegra210_i2s_fmt_values[] = {
	0,
	TEGRA210_AUDIOCIF_BITS_16,
	TEGRA210_AUDIOCIF_BITS_32,
};

static const int tegra210_i2s_sample_size[] = {
	0,
	16,
	32,
};

static const struct soc_enum tegra210_i2s_format_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
		ARRAY_SIZE(tegra210_i2s_format_text),
		tegra210_i2s_format_text);

static int tegra210_i2s_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct tegra210_i2s *i2s = snd_soc_dai_get_drvdata(dai);
	unsigned int mask, val, reg, frame_format;
	int ret, sample_size, channels, srate, i2sclock, bitcnt;
	struct tegra210_xbar_cif_conf cif_conf;

	memset(&cif_conf, 0, sizeof(struct tegra210_xbar_cif_conf));

	channels = params_channels(params);
	if (channels < 1) {
		dev_err(dev, "Doesn't support %d channels\n", channels);
		return -EINVAL;
	}

	mask = TEGRA210_I2S_CTRL_BIT_SIZE_MASK;
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S8:
		val = TEGRA210_I2S_CTRL_BIT_SIZE_8;
		sample_size = 8;
		cif_conf.audio_bits = TEGRA210_AUDIOCIF_BITS_8;
		cif_conf.client_bits = TEGRA210_AUDIOCIF_BITS_8;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		val = TEGRA210_I2S_CTRL_BIT_SIZE_16;
		sample_size = 16;
		cif_conf.audio_bits = TEGRA210_AUDIOCIF_BITS_16;
		cif_conf.client_bits = TEGRA210_AUDIOCIF_BITS_16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		val = TEGRA210_I2S_CTRL_BIT_SIZE_24;
		sample_size = 24;
		cif_conf.audio_bits = TEGRA210_AUDIOCIF_BITS_24;
		cif_conf.client_bits = TEGRA210_AUDIOCIF_BITS_24;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		val = TEGRA210_I2S_CTRL_BIT_SIZE_32;
		sample_size = 32;
		cif_conf.audio_bits = TEGRA210_AUDIOCIF_BITS_32;
		cif_conf.client_bits = TEGRA210_AUDIOCIF_BITS_32;
		break;
	default:
		dev_err(dev, "Wrong format!\n");
		return -EINVAL;
	}

	if (i2s->codec_bit_format) {
		val = tegra210_i2s_fmt_values[i2s->codec_bit_format];
		sample_size = tegra210_i2s_sample_size[i2s->codec_bit_format];
		cif_conf.client_bits = tegra210_i2s_fmt_values[i2s->codec_bit_format];
	}

	regmap_update_bits(i2s->regmap, TEGRA210_I2S_CTRL, mask, val);

	srate = params_rate(params);

	regmap_read(i2s->regmap, TEGRA210_I2S_CTRL, &val);

	frame_format = val & TEGRA210_I2S_CTRL_FRAME_FORMAT_MASK;

	if (frame_format == TEGRA210_I2S_CTRL_FRAME_FORMAT_FSYNC_MODE) {
		i2s->soc_data->set_slot_ctrl(i2s->regmap, channels,
			i2s->tx_mask, i2s->rx_mask);
		/* FIXUP : I2S fifo threshold set to 3 when AFC is connected */
		cif_conf.threshold = 3;
		cif_conf.audio_channels = channels;
		cif_conf.client_channels = channels;
	} else {
		cif_conf.threshold = 3;
		cif_conf.audio_channels = channels;
		cif_conf.client_channels = (channels == 1) ? 2 : channels;
	}

	i2sclock = srate * sample_size * cif_conf.client_channels;

	if (i2s->bclk_ratio != 0)
		i2sclock *= i2s->bclk_ratio;

	bitcnt = (i2sclock / srate) - 1;

	if ((bitcnt < 0) ||
		(bitcnt > TEGRA210_I2S_TIMING_CHANNEL_BIT_CNT_MASK)) {
		dev_err(dev, "Can't set channel bit count\n");
		return -EINVAL;
	}

	ret = tegra210_i2s_set_clock_rate(dev, i2sclock);
	if (ret) {
		dev_err(dev, "Can't set I2S clock rate: %d\n", ret);
		return ret;
	}

	if ((frame_format == TEGRA210_I2S_FRAME_FORMAT_LRCK) && (channels > 1)) {
		val = (bitcnt >> 1) <<
			TEGRA210_I2S_TIMING_CHANNEL_BIT_CNT_SHIFT;
	} else
		val = bitcnt << TEGRA210_I2S_TIMING_CHANNEL_BIT_CNT_SHIFT;

	if (i2sclock % (2 * srate))
		val |= TEGRA210_I2S_TIMING_NON_SYM_EN;

	regmap_write(i2s->regmap, TEGRA210_I2S_TIMING, val);

	/* As a COCEC DAI, CAPTURE is transmit */
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		reg = TEGRA210_I2S_AXBAR_RX_CIF_CTRL;
	else {
		if (i2s->format_in)
			cif_conf.audio_bits =
				tegra210_i2s_fmt_values[i2s->format_in];
		reg = TEGRA210_I2S_AXBAR_TX_CIF_CTRL;
	}

	i2s->soc_data->set_audio_cif(i2s->regmap, reg, &cif_conf);

	if (i2s->enable_cya)
		regmap_write(i2s->regmap, TEGRA210_I2S_CYA, 1);

	return 0;
}

static int tegra210_i2s_codec_probe(struct snd_soc_codec *codec)
{
	struct tegra210_i2s *i2s = snd_soc_codec_get_drvdata(codec);

	codec->control_data = i2s->regmap;

	return 0;
}

static int _tegra210_i2s_slcg_notifier(struct notifier_block *nb,
	unsigned long unused0, void *unused1)
{
	struct tegra210_i2s *i2s = container_of(nb, struct tegra210_i2s,
								slgc_notifier);
	unsigned int mask, val, i2s_ctrl;

	/* Save the I2S CTRL before implement MBIST WAR */
	regmap_read(i2s->regmap, TEGRA210_I2S_CTRL, &i2s_ctrl);

	mask = TEGRA210_I2S_CTRL_MASTER_EN_MASK;
	val = TEGRA210_I2S_CTRL_MASTER_EN;
	/* Set I2S controller in master mode */
	regmap_update_bits(i2s->regmap, TEGRA210_I2S_CTRL, mask, val);
	/* Disable slcg, wait a while and re-enable it */
	regmap_write(i2s->regmap, TEGRA210_I2S_CG, 0);
	regmap_read(i2s->regmap, TEGRA210_I2S_CG, &val);

	udelay(1);

	regmap_write(i2s->regmap, TEGRA210_I2S_CG, 1);

	/* Restore the I2S CTRL */
	regmap_write(i2s->regmap, TEGRA210_I2S_CTRL, i2s_ctrl);

	return NOTIFY_OK;
}


static struct snd_soc_dai_ops tegra210_i2s_dai_ops = {
	.set_fmt	= tegra210_i2s_set_fmt,
	.hw_params	= tegra210_i2s_hw_params,
	.set_bclk_ratio	= tegra210_i2s_set_dai_bclk_ratio,
	.set_tdm_slot	= tegra210_i2s_set_tdm_slot,
};

static struct snd_soc_dai_driver tegra210_i2s_dais[] = {
	{
		.name = "CIF",
		.playback = {
			.stream_name = "CIF Receive",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S8 |
				SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_S32_LE,
		},
		.capture = {
			.stream_name = "CIF Transmit",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S8 |
				SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_S32_LE,
		},
	},
	{
		.name = "DAP",
		.playback = {
			.stream_name = "DAP Receive",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S8 |
				SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_S32_LE,
		},
		.capture = {
			.stream_name = "DAP Transmit",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S8 |
				SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &tegra210_i2s_dai_ops,
		.symmetric_rates = 1,
	}
};

static int tegra210_i2s_loopback_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_i2s *i2s = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = i2s->loopback;

	return 0;
}

static int tegra210_i2s_loopback_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_i2s *i2s = snd_soc_codec_get_drvdata(codec);

	i2s->loopback = ucontrol->value.integer.value[0];

	pm_runtime_get_sync(codec->dev);
	regmap_update_bits(i2s->regmap, TEGRA210_I2S_CTRL,
		TEGRA210_I2S_CTRL_LPBK_MASK,
		i2s->loopback << TEGRA210_I2S_CTRL_LPBK_SHIFT);
	pm_runtime_put(codec->dev);

	return 0;
}

static const struct snd_kcontrol_new tegra210_i2s_controls[] = {
	SOC_SINGLE_EXT("Loopback", SND_SOC_NOPM, 0, 1, 0,
		tegra210_i2s_loopback_get, tegra210_i2s_loopback_put),
	SOC_ENUM_EXT("input bit format", tegra210_i2s_format_enum,
		tegra210_i2s_get_format, tegra210_i2s_put_format),
	SOC_ENUM_EXT("codec bit format", tegra210_i2s_format_enum,
		tegra210_i2s_get_format, tegra210_i2s_put_format),
};

static const struct snd_soc_dapm_widget tegra210_i2s_widgets[] = {
	SND_SOC_DAPM_AIF_IN("CIF RX", NULL, 0, SND_SOC_NOPM,
				0, 0),
	SND_SOC_DAPM_AIF_OUT("CIF TX", NULL, 0, SND_SOC_NOPM,
				0, 0),
	SND_SOC_DAPM_AIF_IN_E("DAP RX", NULL, 0, TEGRA210_I2S_AXBAR_TX_ENABLE,
				TEGRA210_I2S_AXBAR_TX_EN_SHIFT, 0,
				tegra210_i2s_rx_stop, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_AIF_OUT_E("DAP TX", NULL, 0, TEGRA210_I2S_AXBAR_RX_ENABLE,
				TEGRA210_I2S_AXBAR_RX_EN_SHIFT, 0,
				tegra210_i2s_tx_stop, SND_SOC_DAPM_PRE_PMU),
};

static const struct snd_soc_dapm_route tegra210_i2s_routes[] = {
	{ "CIF RX",       NULL, "CIF Receive" },
	{ "DAP TX",       NULL, "CIF RX" },
	{ "DAP Transmit", NULL, "DAP TX" },

	{ "DAP RX",       NULL, "DAP Receive" },
	{ "CIF TX",       NULL, "DAP RX" },
	{ "CIF Transmit", NULL, "CIF TX" },
};

static struct snd_soc_codec_driver tegra210_i2s_codec = {
	.probe = tegra210_i2s_codec_probe,
	.dapm_widgets = tegra210_i2s_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tegra210_i2s_widgets),
	.dapm_routes = tegra210_i2s_routes,
	.num_dapm_routes = ARRAY_SIZE(tegra210_i2s_routes),
	.controls = tegra210_i2s_controls,
	.num_controls = ARRAY_SIZE(tegra210_i2s_controls),
	.idle_bias_off = 1,
};

static bool tegra210_i2s_wr_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA210_I2S_AXBAR_RX_ENABLE:
	case TEGRA210_I2S_AXBAR_RX_SOFT_RESET:
	case TEGRA210_I2S_AXBAR_RX_INT_MASK:
	case TEGRA210_I2S_AXBAR_RX_INT_SET:
	case TEGRA210_I2S_AXBAR_RX_INT_CLEAR:
	case TEGRA210_I2S_AXBAR_RX_CIF_CTRL:
	case TEGRA210_I2S_AXBAR_RX_CTRL:
	case TEGRA210_I2S_AXBAR_RX_SLOT_CTRL:
	case TEGRA210_I2S_AXBAR_RX_CLK_TRIM:
	case TEGRA210_I2S_AXBAR_TX_ENABLE:
	case TEGRA210_I2S_AXBAR_TX_SOFT_RESET:
	case TEGRA210_I2S_AXBAR_TX_INT_MASK:
	case TEGRA210_I2S_AXBAR_TX_INT_SET:
	case TEGRA210_I2S_AXBAR_TX_INT_CLEAR:
	case TEGRA210_I2S_AXBAR_TX_CIF_CTRL:
	case TEGRA210_I2S_AXBAR_TX_CTRL:
	case TEGRA210_I2S_AXBAR_TX_SLOT_CTRL:
	case TEGRA210_I2S_AXBAR_TX_CLK_TRIM:
	case TEGRA210_I2S_ENABLE:
	case TEGRA210_I2S_SOFT_RESET:
	case TEGRA210_I2S_CG:
	case TEGRA210_I2S_CTRL:
	case TEGRA210_I2S_TIMING:
	case TEGRA210_I2S_SLOT_CTRL:
	case TEGRA210_I2S_CLK_TRIM:
	case TEGRA210_I2S_CYA:
		return true;
	default:
		return false;
	};
}

static bool tegra210_i2s_rd_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA210_I2S_AXBAR_RX_STATUS:
	case TEGRA210_I2S_AXBAR_RX_CIF_FIFO_STATUS:
	case TEGRA210_I2S_AXBAR_RX_ENABLE:
	case TEGRA210_I2S_AXBAR_RX_INT_MASK:
	case TEGRA210_I2S_AXBAR_RX_INT_SET:
	case TEGRA210_I2S_AXBAR_RX_INT_CLEAR:
	case TEGRA210_I2S_AXBAR_RX_CIF_CTRL:
	case TEGRA210_I2S_AXBAR_RX_CTRL:
	case TEGRA210_I2S_AXBAR_RX_SLOT_CTRL:
	case TEGRA210_I2S_AXBAR_RX_CLK_TRIM:
	case TEGRA210_I2S_AXBAR_RX_INT_STATUS:
	case TEGRA210_I2S_AXBAR_RX_SOFT_RESET:
	case TEGRA210_I2S_AXBAR_TX_STATUS:
	case TEGRA210_I2S_AXBAR_TX_CIF_FIFO_STATUS:
	case TEGRA210_I2S_AXBAR_TX_ENABLE:
	case TEGRA210_I2S_AXBAR_TX_INT_MASK:
	case TEGRA210_I2S_AXBAR_TX_INT_SET:
	case TEGRA210_I2S_AXBAR_TX_INT_CLEAR:
	case TEGRA210_I2S_AXBAR_TX_CIF_CTRL:
	case TEGRA210_I2S_AXBAR_TX_CTRL:
	case TEGRA210_I2S_AXBAR_TX_SLOT_CTRL:
	case TEGRA210_I2S_AXBAR_TX_CLK_TRIM:
	case TEGRA210_I2S_AXBAR_TX_INT_STATUS:
	case TEGRA210_I2S_AXBAR_TX_SOFT_RESET:
	case TEGRA210_I2S_ENABLE:
	case TEGRA210_I2S_STATUS:
	case TEGRA210_I2S_SOFT_RESET:
	case TEGRA210_I2S_CG:
	case TEGRA210_I2S_CTRL:
	case TEGRA210_I2S_TIMING:
	case TEGRA210_I2S_SLOT_CTRL:
	case TEGRA210_I2S_CLK_TRIM:
	case TEGRA210_I2S_INT_STATUS:
	case TEGRA210_I2S_CYA:
		return true;
	default:
		return false;
	};
}

static bool tegra210_i2s_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA210_I2S_AXBAR_RX_INT_STATUS:
	case TEGRA210_I2S_AXBAR_RX_STATUS:
	case TEGRA210_I2S_AXBAR_TX_STATUS:
	case TEGRA210_I2S_AXBAR_TX_INT_STATUS:
	case TEGRA210_I2S_INT_STATUS:
	case TEGRA210_I2S_AXBAR_RX_SOFT_RESET:
	case TEGRA210_I2S_AXBAR_TX_SOFT_RESET:
		return true;
	default:
		return false;
	};
}

static const struct regmap_config tegra210_i2s_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = TEGRA210_I2S_CYA,
	.writeable_reg = tegra210_i2s_wr_reg,
	.readable_reg = tegra210_i2s_rd_reg,
	.volatile_reg = tegra210_i2s_volatile_reg,
	.reg_defaults = tegra210_i2s_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(tegra210_i2s_reg_defaults),
	.cache_type = REGCACHE_FLAT,
};

static const struct tegra210_i2s_soc_data soc_data_tegra210 = {
	.set_audio_cif = tegra210_xbar_set_cif,
	.set_slot_ctrl = tegra210_i2s_set_slot_ctrl,
};

static const struct of_device_id tegra210_i2s_of_match[] = {
	{ .compatible = "nvidia,tegra210-i2s", .data = &soc_data_tegra210 },
	{},
};

static int tegra210_i2s_platform_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct device_node *np = pdev->dev.of_node;
	struct tegra210_i2s_soc_data *soc_data;
	struct tegra210_i2s *i2s;
	struct resource *mem, *memregion;
	struct property *prop;
	void __iomem *regs;
	int ret = 0, count = 0, num_supplies;
	const char *supply, *prod_name;
#if defined(CONFIG_ARCH_TEGRA_21X_SOC)
	int partition_id;

#ifdef CONFIG_PM_GENERIC_DOMAINS_OF
	partition_id = tegra_pd_get_powergate_id(tegra_ape_pd);
	if (partition_id < 0)
		return -EINVAL;
#else
	partition_id = TEGRA_POWERGATE_APE;
#endif
#endif

	match = of_match_device(tegra210_i2s_of_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "Error: No device match found\n");
		ret = -ENODEV;
		goto err;
	}
	soc_data = (struct tegra210_i2s_soc_data *)match->data;

	i2s = devm_kzalloc(&pdev->dev, sizeof(struct tegra210_i2s), GFP_KERNEL);
	if (!i2s) {
		dev_err(&pdev->dev, "Can't allocate i2s\n");
		ret = -ENOMEM;
		goto err;
	}

	i2s->soc_data = soc_data;
	i2s->tx_mask = i2s->rx_mask = 0xFFFF;
	i2s->bclk_ratio = 2;
	i2s->enable_cya = false;
	i2s->loopback = 0;

	if (!(tegra_platform_is_unit_fpga() || tegra_platform_is_fpga())) {
		i2s->clk_i2s = devm_clk_get(&pdev->dev, NULL);
		if (IS_ERR(i2s->clk_i2s)) {
			dev_err(&pdev->dev, "Can't retrieve i2s clock\n");
			ret = PTR_ERR(i2s->clk_i2s);
			goto err;
		}

		i2s->clk_i2s_sync = devm_clk_get(&pdev->dev, "ext_audio_sync");
		if (IS_ERR(i2s->clk_i2s_sync)) {
			dev_err(&pdev->dev, "Can't retrieve i2s_sync clock\n");
			ret = PTR_ERR(i2s->clk_i2s_sync);
			goto err_clk_put;
		}

		i2s->clk_audio_sync = devm_clk_get(&pdev->dev, "audio_sync");
		if (IS_ERR(i2s->clk_audio_sync)) {
			dev_err(&pdev->dev, "Can't retrieve audio sync clock\n");
			ret = PTR_ERR(i2s->clk_audio_sync);
			goto err_i2s_sync_clk_put;
		}

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
		i2s->clk_i2s_source = clk_get_sys(NULL, "pll_a_out0");
#else
		i2s->clk_i2s_source = devm_clk_get(&pdev->dev, "pll_a_out0");
#endif
		if (IS_ERR(i2s->clk_i2s_source)) {
			dev_err(&pdev->dev, "Can't retrieve pll_a_out0 clock\n");
			ret = PTR_ERR(i2s->clk_i2s_source);
			goto err_audio_sync_clk_put;
		}
	}
	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "No memory resource\n");
		ret = -ENODEV;
		goto err_pll_a_out0_clk_put;
	}

	memregion = devm_request_mem_region(&pdev->dev, mem->start,
					    resource_size(mem), pdev->name);
	if (!memregion) {
		dev_err(&pdev->dev, "Memory region already claimed\n");
		ret = -EBUSY;
		goto err_pll_a_out0_clk_put;
	}

	regs = devm_ioremap(&pdev->dev, mem->start, resource_size(mem));
	if (!regs) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENOMEM;
		goto err_pll_a_out0_clk_put;
	}

	i2s->regmap = devm_regmap_init_mmio(&pdev->dev, regs,
					    &tegra210_i2s_regmap_config);
	if (IS_ERR(i2s->regmap)) {
		dev_err(&pdev->dev, "regmap init failed\n");
		ret = PTR_ERR(i2s->regmap);
		goto err_pll_a_out0_clk_put;
	}

	i2s->slgc_notifier.notifier_call = _tegra210_i2s_slcg_notifier;
#if defined(CONFIG_ARCH_TEGRA_21X_SOC)
	slcg_register_notifier(partition_id, &i2s->slgc_notifier);
#endif


	regcache_cache_only(i2s->regmap, true);

	if (of_property_read_u32(np, "nvidia,ahub-i2s-id",
				&pdev->dev.id) < 0) {
		dev_err(&pdev->dev,
			"Missing property nvidia,ahub-i2s-id\n");
		ret = -ENODEV;
		goto err_pll_a_out0_clk_put;
	}

	if (of_property_read_u32(pdev->dev.of_node,
		"fsync-width", &i2s->fsync_width) < 0) {
		dev_warn(&pdev->dev, "Missing prop fsync-width for I2S%d\n",
			pdev->dev.id);
		i2s->fsync_width = 31;
	}

	if (i2s->fsync_width > 255) {
		dev_warn(&pdev->dev, "Default fsync-width to 31 for I2S%d\n",
			pdev->dev.id);
		i2s->fsync_width = 31;
	}

	i2s->enable_cya =
		of_property_read_bool(pdev->dev.of_node,
			"enable-cya");

	if (!(tegra_platform_is_unit_fpga() || tegra_platform_is_fpga())) {
		if (of_property_read_string(np, "prod-name", &prod_name) == 0)
			tegra_pinctrl_config_prod(&pdev->dev, prod_name);

		num_supplies =
			of_property_count_strings(np, "regulator-supplies");
		if (num_supplies > 0) {
			i2s->num_supplies = num_supplies;
			i2s->supplies = devm_kzalloc(&pdev->dev, num_supplies *
					sizeof(*i2s->supplies), GFP_KERNEL);
			if (!i2s->supplies) {
				ret = -ENOMEM;
				goto err_pll_a_out0_clk_put;
			}
			of_property_for_each_string(np,
				"regulator-supplies", prop, supply)
				i2s->supplies[count++].supply = supply;

			ret = devm_regulator_bulk_get(
					&pdev->dev, i2s->num_supplies,
					i2s->supplies);
			if (ret) {
				dev_err(&pdev->dev,
					"Failed to get supplies: %d\n", ret);
				return ret;
			}
		}

		i2s->pinctrl = devm_pinctrl_get(&pdev->dev);
		if (IS_ERR(i2s->pinctrl)) {
			dev_warn(&pdev->dev, "Missing pinctrl device\n");
			goto err_dap;
		}

		i2s->pin_default_state = pinctrl_lookup_state(i2s->pinctrl,
									"dap_active");
		if (IS_ERR(i2s->pin_default_state)) {
			dev_warn(&pdev->dev, "Missing dap-active state\n");
			goto err_dap;
		}

		i2s->pin_idle_state = pinctrl_lookup_state(i2s->pinctrl,
								"dap_inactive");
		if (IS_ERR(i2s->pin_idle_state)) {
			dev_warn(&pdev->dev, "Missing dap-inactive state\n");
			goto err_dap;
		}

		ret = pinctrl_select_state(i2s->pinctrl, i2s->pin_idle_state);
		if (ret < 0) {
			dev_err(&pdev->dev, "setting state failed\n");
			goto err_dap;
		}
	}
err_dap:
	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		ret = tegra210_i2s_runtime_resume(&pdev->dev);
		if (ret)
			goto err_pm_disable;
	}

	ret = snd_soc_register_codec(&pdev->dev, &tegra210_i2s_codec,
				     tegra210_i2s_dais,
				     ARRAY_SIZE(tegra210_i2s_dais));
	if (ret != 0) {
		dev_err(&pdev->dev, "Could not register CODEC: %d\n", ret);
		goto err_suspend;
	}

	dev_set_drvdata(&pdev->dev, i2s);

	return 0;

err_suspend:
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra210_i2s_runtime_suspend(&pdev->dev);
err_pm_disable:
	pm_runtime_disable(&pdev->dev);
err_pll_a_out0_clk_put:
	if (!(tegra_platform_is_unit_fpga() || tegra_platform_is_fpga()))
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
		clk_put(i2s->clk_i2s_source);
#else
		devm_clk_put(&pdev->dev, i2s->clk_i2s_source);
#endif
err_audio_sync_clk_put:
	devm_clk_put(&pdev->dev, i2s->clk_audio_sync);
err_i2s_sync_clk_put:
	devm_clk_put(&pdev->dev, i2s->clk_i2s_sync);
err_clk_put:
	devm_clk_put(&pdev->dev, i2s->clk_i2s);
err:
	return ret;
}

static int tegra210_i2s_platform_remove(struct platform_device *pdev)
{
	struct tegra210_i2s *i2s = dev_get_drvdata(&pdev->dev);

	snd_soc_unregister_codec(&pdev->dev);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra210_i2s_runtime_suspend(&pdev->dev);

	if (!(tegra_platform_is_unit_fpga() || tegra_platform_is_fpga())) {
		devm_clk_put(&pdev->dev, i2s->clk_i2s);
		devm_clk_put(&pdev->dev, i2s->clk_audio_sync);
		devm_clk_put(&pdev->dev, i2s->clk_i2s_sync);
		clk_put(i2s->clk_i2s_source);
	}

	return 0;
}

static const struct dev_pm_ops tegra210_i2s_pm_ops = {
	SET_RUNTIME_PM_OPS(tegra210_i2s_runtime_suspend,
			   tegra210_i2s_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(tegra210_i2s_suspend, NULL)
};

static struct platform_driver tegra210_i2s_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = tegra210_i2s_of_match,
		.pm = &tegra210_i2s_pm_ops,
	},
	.probe = tegra210_i2s_platform_probe,
	.remove = tegra210_i2s_platform_remove,
};
module_platform_driver(tegra210_i2s_driver)

MODULE_AUTHOR("Songhee Baek <sbaek@nvidia.com>");
MODULE_DESCRIPTION("Tegra210 I2S ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, tegra210_i2s_of_match);
