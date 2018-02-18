/*
 * tegra_vcm30t124.c - Tegra VCM30 T124 Machine driver
 *
 * Copyright (c) 2013-2014 NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>

#include <sound/core.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include <mach/tegra_vcm30t124_pdata.h>

#include "../codecs/wm8731.h"
#include "../codecs/ad193x.h"

#include "tegra_asoc_utils_alt.h"
#include "tegra_asoc_machine_alt.h"
#include "tegra_asoc_hwdep_alt.h"

#define DRV_NAME "tegra-snd-vcm30t124"

#define CODEC_NAME      "spdif-dit.0"
#define CODEC_DAI_NAME  "dit-hifi"
#define LINK_CPU_NAME   "tegra30-ahub-xbar"
#define MAX_APBIF_ID 10

#define GPIO_PR0 136
#define CODEC_TO_DAP 0
#define DAP_TO_CODEC 1

static struct snd_soc_dai_link *tegra_machine_dai_links;
static struct snd_soc_dai_link *tegra_vcm30t124_codec_links;
static struct snd_soc_codec_conf *tegra_machine_codec_conf;
static struct snd_soc_codec_conf *tegra_vcm30t124_codec_conf;
static unsigned int num_codec_links;

struct tegra_vcm30t124 {
	struct tegra_asoc_audio_clock_info audio_clock;
	int gpio_dap_direction;
	int wm_rate_via_kcontrol;
	int ad_rate_via_kcontrol;
	int ak_rate_via_kcontrol;
	struct i2c_client *max9485_client;
	struct tegra_vcm30t124_platform_data *pdata;
};

static struct i2c_board_info max9485_info = {
	.type = "max9485",
};

#define MAX9485_MCLK_FREQ_163840 0x31
#define MAX9485_MCLK_FREQ_112896 0x22
#define MAX9485_MCLK_FREQ_122880 0x23
#define MAX9485_MCLK_FREQ_225792 0x32
#define MAX9485_MCLK_FREQ_245760 0x33

static void set_max9485_clk(struct i2c_client *i2s, int mclk)
{
	char clk;

	switch (mclk) {
	case 16384000:
		clk =  MAX9485_MCLK_FREQ_163840;
		break;
	case 11289600:
		clk = MAX9485_MCLK_FREQ_112896;
		break;
	case 12288000:
		clk = MAX9485_MCLK_FREQ_122880;
		break;
	case 22579200:
		clk = MAX9485_MCLK_FREQ_225792;
		break;
	case 24576000:
		clk = MAX9485_MCLK_FREQ_245760;
		break;
	default:
		return;
	}
	i2c_master_send(i2s, &clk, 1);
}

static unsigned int tegra_vcm30t124_get_dai_link_idx(const char *codec_name)
{
	unsigned int idx = TEGRA124_XBAR_DAI_LINKS, i;

	for (i = 0; i < num_codec_links; i++) {
		if (tegra_machine_dai_links[idx + i].name)
			if (!strcmp(tegra_machine_dai_links[idx + i].name,
				codec_name)) {
				return idx + i;
			}
	}
	return idx;
}

static int tegra_vcm30t124_ak4618_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = codec_dai->codec;
	struct snd_soc_card *card = codec->card;
	struct tegra_vcm30t124 *machine = snd_soc_card_get_drvdata(card);
	unsigned int idx = tegra_vcm30t124_get_dai_link_idx("ak-playback");
	struct snd_soc_pcm_stream *dai_params =
		(struct snd_soc_pcm_stream *)card->rtd[idx].dai_link->params;
	unsigned int fmt = card->rtd[idx].dai_link->dai_fmt;
	int mclk, clk_out_rate;
	int err;

	/* rate is either supplied by pcm params or via kcontrol */
	if (!machine->ak_rate_via_kcontrol)
		dai_params->rate_min = params_rate(params);

	switch (dai_params->rate_min) {
	case 64000:
	case 96000:
		clk_out_rate = dai_params->rate_min * 256;
		mclk = 12288000 * 2;
		break;
	case 88200:
		clk_out_rate = dai_params->rate_min * 256;
		mclk = 11289600 * 2;
		break;
	case 8000:
	case 16000:
	case 32000:
	case 48000:
	default:
		clk_out_rate = dai_params->rate_min * 512;
		/*
		 * MCLK is pll_a_out, it is a source clock of ahub.
		 * So it need to be faster than BCLK in slave mode.
		 */
		mclk = 12288000 * 2;
		break;
	case 44100:
		clk_out_rate = dai_params->rate_min * 512;
		/*
		 * MCLK is pll_a_out, it is a source clock of ahub.
		 * So it need to be faster than BCLK in slave mode.
		 */
		mclk = 11289600 * 2;
		break;
	case 17640:
		clk_out_rate = dai_params->rate_min * 128;
		mclk = 11289600 * 2;
		break;
	case 19200:
		clk_out_rate = dai_params->rate_min * 128;
		mclk = 12288000 * 2;
		break;
	}

	err = tegra_alt_asoc_utils_set_rate(&machine->audio_clock,
				dai_params->rate_min, mclk, clk_out_rate);
	if (err < 0) {
		dev_err(card->dev, "Can't configure clocks\n");
		return err;
	}

	err = snd_soc_dai_set_sysclk(card->rtd[idx].codec_dai,
			0, clk_out_rate, SND_SOC_CLOCK_IN);
	if (err < 0) {
		dev_err(card->dev, "x codec_dai clock not set\n");
		return err;
	}

	if ((fmt & SND_SOC_DAIFMT_FORMAT_MASK) == SND_SOC_DAIFMT_DSP_A) {
		snd_soc_dai_set_tdm_slot(card->rtd[idx].codec_dai,
					0, 0, dai_params->channels_min, 32);
	} else {
		dai_params->channels_min = params_channels(params);
		dai_params->formats = (1ULL << (params_format(params)));
	}

	return 0;
}

static int tegra_vcm30t124_wm8731_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = codec_dai->codec;
	struct snd_soc_card *card = codec->card;
	struct tegra_vcm30t124 *machine = snd_soc_card_get_drvdata(card);
	unsigned int idx = tegra_vcm30t124_get_dai_link_idx("wm-playback");
	struct snd_soc_pcm_stream *dai_params =
		(struct snd_soc_pcm_stream *)card->rtd[idx].dai_link->params;
	int mclk, clk_out_rate;
	int err;

	/* rate is either supplied by pcm params or via kcontrol */
	if (!machine->wm_rate_via_kcontrol)
		dai_params->rate_min = params_rate(params);

	dai_params->channels_min = params_channels(params);
	dai_params->formats = (1ULL << (params_format(params)));

	switch (dai_params->rate_min) {
	case 64000:
	case 88200:
	case 96000:
		clk_out_rate = 128 * dai_params->rate_min;
		mclk = clk_out_rate * 2;
		break;
	case 8000:
	case 16000:
	case 32000:
	case 48000:
	default:
		clk_out_rate = 12288000;
		/*
		 * MCLK is pll_a_out, it is a source clock of ahub.
		 * So it need to be faster than BCLK in slave mode.
		 */
		mclk = 12288000 * 2;
		break;
	case 44100:
		clk_out_rate = 11289600;
		/*
		 * MCLK is pll_a_out, it is a source clock of ahub.
		 * So it need to be faster than BCLK in slave mode.
		 */
		mclk = 11289600 * 2;
		break;
	}

	err = tegra_alt_asoc_utils_set_rate(&machine->audio_clock,
			dai_params->rate_min, mclk, clk_out_rate);
	if (err < 0) {
		dev_err(card->dev, "Can't configure clocks\n");
		return err;
	}

	err = snd_soc_dai_set_sysclk(card->rtd[idx].codec_dai,
			WM8731_SYSCLK_MCLK, clk_out_rate, SND_SOC_CLOCK_IN);
	if (err < 0) {
		dev_err(card->dev, "wm8731 codec_dai clock not set\n");
		return err;
	}

	return 0;
}

static int tegra_vcm30t124_ad1937_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = codec_dai->codec;
	struct snd_soc_card *card = codec->card;
	struct tegra_vcm30t124 *machine = snd_soc_card_get_drvdata(card);
	unsigned int idx = tegra_vcm30t124_get_dai_link_idx("ad-playback");
	struct snd_soc_pcm_stream *dai_params =
		(struct snd_soc_pcm_stream *)card->rtd[idx].dai_link->params;
	unsigned int fmt = card->rtd[idx].dai_link->dai_fmt;
	int mclk, clk_out_rate, val;
	int err;

	/* rate is either supplied by pcm params or via kcontrol */
	if (!machine->ad_rate_via_kcontrol)
		dai_params->rate_min = params_rate(params);

	switch (dai_params->rate_min) {
	case 64000:
	case 88200:
	case 96000:
		clk_out_rate = dai_params->rate_min * 128;
		break;
	default:
		clk_out_rate = dai_params->rate_min * 256;
		break;
	}

	mclk = clk_out_rate * 2;

	set_max9485_clk(machine->max9485_client, mclk);

	err = snd_soc_dai_set_sysclk(card->rtd[idx].codec_dai,
			0, mclk,
			SND_SOC_CLOCK_IN);
	if (err < 0) {
		dev_err(card->dev, "y codec_dai clock not set\n");
		return err;
	}

	/*
	 * AD193X driver enables both DAC and ADC as MASTER
	 * so both ADC and DAC drive LRCLK and BCLK and it causes
	 * noise. To solve this, we need to disable one of them.
	 */
	if ((fmt & SND_SOC_DAIFMT_MASTER_MASK) == SND_SOC_DAIFMT_CBM_CFM) {
		val = snd_soc_read(card->rtd[idx].codec_dai->codec,
				AD193X_DAC_CTRL1);
		val &= ~AD193X_DAC_LCR_MASTER;
		val &= ~AD193X_DAC_BCLK_MASTER;
		snd_soc_write(card->rtd[idx].codec_dai->codec,
				AD193X_DAC_CTRL1, val);
	}

	return 0;
}

static int tegra_vcm30t124_spdif_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	/* dummy hw_params; clocks set in the init function */

	return 0;
}

static int tegra_vcm30t124_bt_sco_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = codec_dai->codec;
	struct snd_soc_card *card = codec->card;
	unsigned int idx = tegra_vcm30t124_get_dai_link_idx("bt-playback");
	struct snd_soc_pcm_stream *dai_params =
		(struct snd_soc_pcm_stream *)card->rtd[idx].dai_link->params;

	dai_params->rate_min = params_rate(params);
	dai_params->channels_min = params_channels(params);
	dai_params->formats = (1ULL << (params_format(params)));

	return 0;
}

static struct snd_soc_ops tegra_vcm30t124_ak4618_ops = {
	.hw_params = tegra_vcm30t124_ak4618_hw_params,
};

static struct snd_soc_ops tegra_vcm30t124_wm8731_ops = {
	.hw_params = tegra_vcm30t124_wm8731_hw_params,
};

static struct snd_soc_ops tegra_vcm30t124_ad1937_ops = {
	.hw_params = tegra_vcm30t124_ad1937_hw_params,
};

static struct snd_soc_ops tegra_vcm30t124_spdif_ops = {
	.hw_params = tegra_vcm30t124_spdif_hw_params,
};

static struct snd_soc_ops tegra_vcm30t124_bt_sco_ops = {
	.hw_params = tegra_vcm30t124_bt_sco_hw_params,
};

static const struct snd_soc_dapm_widget tegra_vcm30t124_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone-x", NULL),
	SND_SOC_DAPM_HP("Headphone-y", NULL),
	SND_SOC_DAPM_MIC("Mic-x", NULL),
	SND_SOC_DAPM_LINE("LineIn-x", NULL),
	SND_SOC_DAPM_LINE("LineIn-y", NULL),
	SND_SOC_DAPM_SPK("Spdif-out", NULL),
	SND_SOC_DAPM_LINE("Spdif-in", NULL),
	SND_SOC_DAPM_SPK("BT-out", NULL),
	SND_SOC_DAPM_LINE("BT-in", NULL),
};

static int tegra_vcm30t124_wm8731_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *wm8731_dai = rtd->codec_dai;
	struct snd_soc_dai *i2s_dai = rtd->cpu_dai;
	struct snd_soc_codec *codec = wm8731_dai->codec;
	struct snd_soc_card *card = codec->card;
	struct tegra_vcm30t124 *machine = snd_soc_card_get_drvdata(card);
	struct tegra_vcm30t124_platform_data *pdata = machine->pdata;
	struct snd_soc_pcm_stream *dai_params =
		(struct snd_soc_pcm_stream *)rtd->dai_link->params;
	const char *identifier = (const char *)rtd->dai_link->name;
	unsigned int idx = tegra_vcm30t124_get_dai_link_idx(identifier);
	unsigned int clk_out, mclk;
	int err;

	idx = idx - TEGRA124_XBAR_DAI_LINKS;
	clk_out = dai_params->rate_min * 256;
	mclk = clk_out * 2;

	tegra_alt_asoc_utils_set_parent(&machine->audio_clock, true);

	/* wm8731 needs mclk from tegra */
	err = tegra_alt_asoc_utils_set_rate(&machine->audio_clock,
					dai_params->rate_min, mclk, clk_out);
	if (err < 0) {
		dev_err(card->dev, "Can't configure clocks\n");
		return err;
	}

	err = snd_soc_dai_set_sysclk(wm8731_dai, WM8731_SYSCLK_MCLK, clk_out,
					SND_SOC_CLOCK_IN);
	if (err < 0) {
		dev_err(card->dev, "wm8731 clock not set %d\n", clk_out);
		return err;
	}

	if (i2s_dai->driver->ops->set_bclk_ratio) {
		err = snd_soc_dai_set_bclk_ratio(i2s_dai,
				pdata->dai_config[idx].bclk_ratio);
		if (err < 0) {
			dev_err(card->dev, "%s cpu DAI bclk not set\n",
				i2s_dai->name);
			return err;
		}
	}

	return 0;
}

static int tegra_vcm30t124_ad1937_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *ad1937_dai = rtd->codec_dai;
	struct snd_soc_dai *i2s_dai = rtd->cpu_dai;
	struct snd_soc_codec *codec = ad1937_dai->codec;
	struct snd_soc_card *card = codec->card;
	struct tegra_vcm30t124 *machine = snd_soc_card_get_drvdata(card);
	struct tegra_vcm30t124_platform_data *pdata = machine->pdata;
	struct snd_soc_pcm_stream *dai_params =
		(struct snd_soc_pcm_stream *)rtd->dai_link->params;
	const char *identifier = (const char *)rtd->dai_link->name;
	unsigned int idx = tegra_vcm30t124_get_dai_link_idx(identifier);
	unsigned int fmt = rtd->dai_link->dai_fmt;
	unsigned int mclk;
	int err;

	idx = idx - TEGRA124_XBAR_DAI_LINKS;
	mclk = dai_params->rate_min * 512;

	if ((fmt & SND_SOC_DAIFMT_MASTER_MASK) == SND_SOC_DAIFMT_CBM_CFM) {
		/* direct MCLK mode in AD1937, mclk needs to be srate * 512 */
		set_max9485_clk(machine->max9485_client, mclk);
		err = snd_soc_dai_set_sysclk(ad1937_dai, 0, mclk,
						SND_SOC_CLOCK_IN);
		if (err < 0) {
			dev_err(card->dev, "ad1937 clock not set\n");
			return err;
		}

		snd_soc_write(ad1937_dai->codec, AD193X_PLL_CLK_CTRL1, 0x03);

	} else {
		/* set PLL_SRC with LRCLK for AD1937 slave mode */
		snd_soc_write(ad1937_dai->codec, AD193X_PLL_CLK_CTRL0, 0xb9);
	}

	if (i2s_dai->driver->ops->set_bclk_ratio) {
		err = snd_soc_dai_set_bclk_ratio(i2s_dai,
				pdata->dai_config[idx].bclk_ratio);
		if (err < 0) {
			dev_err(card->dev, "%s cpu DAI bclk not set\n",
				i2s_dai->name);
			return err;
		}
	}

	if ((fmt & SND_SOC_DAIFMT_FORMAT_MASK) == SND_SOC_DAIFMT_DSP_A) {
		snd_soc_dai_set_tdm_slot(ad1937_dai, 0, 0, 8, 0);
		snd_soc_dai_set_tdm_slot(i2s_dai,
				pdata->dai_config[idx].tx_mask,
				pdata->dai_config[idx].rx_mask,
				0, 0);
	}

	return 0;
}

static int tegra_vcm30t124_ad1937_late_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *ad1937_dai = rtd->codec_dai;
	unsigned int fmt = rtd->dai_link->dai_fmt;
	unsigned int val;

	if ((fmt & SND_SOC_DAIFMT_MASTER_MASK) == SND_SOC_DAIFMT_CBM_CFM) {
		/*
		 * AD193X driver enables both DAC and ADC as MASTER
		 * so both ADC and DAC drive LRCLK and BCLK and it causes
		 * noise. To solve this, we need to disable one of them.
		 */
		val = snd_soc_read(ad1937_dai->codec, AD193X_DAC_CTRL1);
		val &= ~AD193X_DAC_LCR_MASTER;
		val &= ~AD193X_DAC_BCLK_MASTER;
		snd_soc_write(ad1937_dai->codec, AD193X_DAC_CTRL1, val);
	}

	return 0;
}

static int tegra_vcm30t124_ak4618_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_codec *codec = codec_dai->codec;
	struct snd_soc_card *card = codec->card;
	struct tegra_vcm30t124 *machine = snd_soc_card_get_drvdata(card);
	struct tegra_vcm30t124_platform_data *pdata = machine->pdata;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_pcm_stream *dai_params =
		(struct snd_soc_pcm_stream *)rtd->dai_link->params;
	const char *identifier = (const char *)rtd->dai_link->name;
	unsigned int idx = tegra_vcm30t124_get_dai_link_idx(identifier);
	unsigned int fmt = rtd->dai_link->dai_fmt;
	unsigned int clk_out, mclk, srate;
	int err;

	/* Default sampling rate*/
	idx = idx - TEGRA124_XBAR_DAI_LINKS;
	srate = dai_params->rate_min;
	clk_out = srate * 512;
	mclk = clk_out;

	tegra_alt_asoc_utils_set_parent(&machine->audio_clock, true);

	/* ak4618 needs mclk from tegra */
	err = tegra_alt_asoc_utils_set_rate(&machine->audio_clock,
					srate, mclk, clk_out);
	if (err < 0) {
		dev_err(card->dev, "Can't configure clocks\n");
		return err;
	}

	err = snd_soc_dai_set_sysclk(codec_dai, 0, clk_out,
					SND_SOC_CLOCK_IN);
	if (err < 0) {
		dev_err(card->dev, "ak4618 clock not set\n");
		return err;
	}

	if (cpu_dai->driver->ops->set_bclk_ratio) {
		err = snd_soc_dai_set_bclk_ratio(cpu_dai,
				pdata->dai_config[idx].bclk_ratio);
		if (err < 0) {
			dev_err(card->dev, "%s cpu DAI bclk not set\n",
				cpu_dai->name);
			return err;
		}
	}

	if ((fmt & SND_SOC_DAIFMT_FORMAT_MASK) == SND_SOC_DAIFMT_DSP_A) {
		snd_soc_dai_set_tdm_slot(codec_dai, 0, 0, 8, 32);
		snd_soc_dai_set_tdm_slot(cpu_dai,
				pdata->dai_config[idx].tx_mask,
				pdata->dai_config[idx].rx_mask,
				0, 0);
	}

	snd_soc_dapm_force_enable_pin(dapm, "x MICBIAS");

	return 0;
}

static int tegra_vcm30t124_spdif_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_codec *codec = codec_dai->codec;
	struct snd_soc_card *card = codec->card;
	struct tegra_vcm30t124 *machine = snd_soc_card_get_drvdata(card);
	struct tegra_vcm30t124_platform_data *pdata = machine->pdata;
	struct snd_soc_pcm_stream *dai_params =
		(struct snd_soc_pcm_stream *)rtd->dai_link->params;
	const char *identifier = (const char *)rtd->dai_link->name;
	unsigned int idx = tegra_vcm30t124_get_dai_link_idx(identifier);
	unsigned int fmt = rtd->dai_link->dai_fmt;
	unsigned int mclk, clk_out_rate, srate;
	int err = 0;

	/* Default sampling rate*/
	idx = idx - TEGRA124_XBAR_DAI_LINKS;
	srate = dai_params->rate_min;
	clk_out_rate = srate * 256;
	mclk = clk_out_rate * 2;

	err = tegra_alt_asoc_utils_set_rate(&machine->audio_clock,
					srate, mclk, clk_out_rate);
	if (err < 0) {
		dev_err(card->dev, "Can't configure clocks\n");
		return err;
	}

	/* set sys clk */
	if (cpu_dai->driver->ops->set_sysclk) {
		err = snd_soc_dai_set_sysclk(cpu_dai, 0, srate,
						SND_SOC_CLOCK_OUT);
		err = snd_soc_dai_set_sysclk(cpu_dai, 0, srate,
						SND_SOC_CLOCK_IN);
		if (err < 0) {
			dev_err(card->dev, "%s cpu DAI srate not set\n",
				cpu_dai->name);
			return err;
		}
	}

	/* set bclk ratio */
	if (cpu_dai->driver->ops->set_bclk_ratio) {
		err = snd_soc_dai_set_bclk_ratio(cpu_dai,
				pdata->dai_config[idx].bclk_ratio);
		if (err < 0) {
			dev_err(card->dev, "%s cpu DAI bclk not set\n",
				cpu_dai->name);
			return err;
		}
	}

	/* set tdm slot mask */
	if (cpu_dai->driver->ops->set_tdm_slot) {
		fmt = fmt & SND_SOC_DAIFMT_FORMAT_MASK;
		if ((fmt == SND_SOC_DAIFMT_DSP_A) ||
			(fmt == SND_SOC_DAIFMT_DSP_B)) {
			err = snd_soc_dai_set_tdm_slot(cpu_dai,
					pdata->dai_config[idx].tx_mask,
					pdata->dai_config[idx].rx_mask,
					0, 0);
			if (err < 0) {
				dev_err(card->dev,
					"%s cpu DAI slot mask not set\n",
					cpu_dai->name);
			}
		}
	}
	return err;
}

static int tegra_vcm30t124_bt_sco_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_codec *codec = codec_dai->codec;
	struct snd_soc_card *card = codec->card;
	struct tegra_vcm30t124 *machine = snd_soc_card_get_drvdata(card);
	struct tegra_vcm30t124_platform_data *pdata = machine->pdata;
	const char *identifier = (const char *)rtd->dai_link->name;
	unsigned int idx = tegra_vcm30t124_get_dai_link_idx(identifier);
	unsigned int fmt = rtd->dai_link->dai_fmt;
	int err = 0;

	idx = idx - TEGRA124_XBAR_DAI_LINKS;

	if (cpu_dai->driver->ops->set_bclk_ratio) {
		err = snd_soc_dai_set_bclk_ratio(cpu_dai,
				pdata->dai_config[idx].bclk_ratio);
		if (err < 0) {
			dev_err(card->dev, "%s cpu DAI bclk not set\n",
				cpu_dai->name);
			return err;
		}
	}

	/* set tdm slot mask */
	if (cpu_dai->driver->ops->set_tdm_slot) {
		fmt = fmt & SND_SOC_DAIFMT_FORMAT_MASK;
		if ((fmt == SND_SOC_DAIFMT_DSP_A) ||
			(fmt == SND_SOC_DAIFMT_DSP_B)) {
			err = snd_soc_dai_set_tdm_slot(cpu_dai,
					pdata->dai_config[idx].tx_mask,
					pdata->dai_config[idx].rx_mask,
					0, 0);
			if (err < 0) {
				dev_err(card->dev,
					"%s cpu DAI slot mask not set\n",
					cpu_dai->name);
			}
		}
	}
	return err;
}

static int tegra_vcm30t124_i2s4_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = codec_dai->codec;
	struct snd_soc_card *card = codec->card;
	struct tegra_vcm30t124 *machine = snd_soc_card_get_drvdata(card);
	unsigned int fmt = rtd->dai_link->dai_fmt;

	if ((fmt & SND_SOC_DAIFMT_MASTER_MASK) == SND_SOC_DAIFMT_CBM_CFM)
		/* set SCLK, FS direction from codec to dap */
		gpio_direction_output(machine->gpio_dap_direction,
					CODEC_TO_DAP);
	else
		/* set SCLK, FS direction from dap to codec */
		gpio_direction_output(machine->gpio_dap_direction,
					DAP_TO_CODEC);

	return 0;
}

static int tegra_vcm30t124_amx0_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *amx_dai = rtd->cpu_dai;
	struct tegra_vcm30t124 *machine = snd_soc_card_get_drvdata(rtd->card);
	unsigned int default_slot[32], i, j;
	unsigned int slot_size = 32;
	unsigned int *tx_slot;

	/* AMX0 slot map may be initialized through platform data */
	if (machine->pdata->num_amx) {
		tx_slot = machine->pdata->amx_config[0].slot_map;
		slot_size = machine->pdata->amx_config[0].slot_size;
	} else {
		for (i = 0, j = 0; i < slot_size; i += 8) {
			default_slot[i] = 0;
			default_slot[i + 1] = 0;
			default_slot[i + 2] = (j << 16) | (1 << 8) | 0;
			default_slot[i + 3] = (j << 16) | (1 << 8) | 1;
			default_slot[i + 4] = 0;
			default_slot[i + 5] = 0;
			default_slot[i + 6] = (j << 16) | (2 << 8) | 0;
			default_slot[i + 7] = (j << 16) | (2 << 8) | 1;
			j++;
		}
		tx_slot = default_slot;
	}

	if (amx_dai->driver->ops->set_channel_map)
		amx_dai->driver->ops->set_channel_map(amx_dai,
					slot_size, tx_slot, 0, 0);

	return 0;
}

static int tegra_vcm30t124_amx1_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *amx_dai = rtd->cpu_dai;
	struct tegra_vcm30t124 *machine = snd_soc_card_get_drvdata(rtd->card);
	unsigned int default_slot[32], i, j;
	unsigned int slot_size = 32;
	unsigned int *tx_slot;

	/* AMX1 slot map may be initialized through platform data */
	if (machine->pdata->num_amx > 1) {
		tx_slot = machine->pdata->amx_config[1].slot_map;
		slot_size = machine->pdata->amx_config[1].slot_size;
	} else {
		for (i = 0, j = 0; i < slot_size; i += 8) {
			default_slot[i] = 0;
			default_slot[i + 1] = 0;
			default_slot[i + 2] = (j << 16) | (1 << 8) | 0;
			default_slot[i + 3] = (j << 16) | (1 << 8) | 1;
			default_slot[i + 4] = 0;
			default_slot[i + 5] = 0;
			default_slot[i + 6] = (j << 16) | (2 << 8) | 0;
			default_slot[i + 7] = (j << 16) | (2 << 8) | 1;
			j++;
		}
		tx_slot = default_slot;
	}

	if (amx_dai->driver->ops->set_channel_map)
		amx_dai->driver->ops->set_channel_map(amx_dai,
					slot_size, tx_slot, 0, 0);

	return 0;
}

static int tegra_vcm30t124_adx0_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *adx_dai = rtd->codec_dai;
	struct tegra_vcm30t124 *machine = snd_soc_card_get_drvdata(rtd->card);
	unsigned int default_slot[32], i, j;
	unsigned int slot_size = 32;
	unsigned int *rx_slot;

	/* ADX0 slot map may be initialized through platform data */
	if (machine->pdata->num_adx) {
		rx_slot = machine->pdata->adx_config[0].slot_map;
		slot_size = machine->pdata->adx_config[0].slot_size;
	} else {
		for (i = 0, j = 0; i < slot_size; i += 8) {
			default_slot[i] = 0;
			default_slot[i + 1] = 0;
			default_slot[i + 2] = (j << 16) | (1 << 8) | 0;
			default_slot[i + 3] = (j << 16) | (1 << 8) | 1;
			default_slot[i + 4] = 0;
			default_slot[i + 5] = 0;
			default_slot[i + 6] = (j << 16) | (2 << 8) | 0;
			default_slot[i + 7] = (j << 16) | (2 << 8) | 1;
			j++;
		}
		rx_slot = default_slot;
	}

	if (adx_dai->driver->ops->set_channel_map)
		adx_dai->driver->ops->set_channel_map(adx_dai,
					0, 0, slot_size, rx_slot);

	return 0;
}

static int tegra_vcm30t124_adx1_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *adx_dai = rtd->codec_dai;
	struct tegra_vcm30t124 *machine = snd_soc_card_get_drvdata(rtd->card);
	unsigned int default_slot[32], i, j;
	unsigned int slot_size = 32;
	unsigned int *rx_slot;

	/* ADX1 slot map may be initialized through platform data */
	if (machine->pdata->num_adx > 1) {
		rx_slot = machine->pdata->adx_config[1].slot_map;
		slot_size = machine->pdata->adx_config[1].slot_size;
	} else {
		for (i = 0, j = 0; i < slot_size; i += 8) {
			default_slot[i] = 0;
			default_slot[i + 1] = 0;
			default_slot[i + 2] = (j << 16) | (1 << 8) | 0;
			default_slot[i + 3] = (j << 16) | (1 << 8) | 1;
			default_slot[i + 4] = 0;
			default_slot[i + 5] = 0;
			default_slot[i + 6] = (j << 16) | (2 << 8) | 0;
			default_slot[i + 7] = (j << 16) | (2 << 8) | 1;
			j++;
		}
		rx_slot = default_slot;
	}

	if (adx_dai->driver->ops->set_channel_map)
		adx_dai->driver->ops->set_channel_map(adx_dai,
					0, 0, slot_size, rx_slot);

	return 0;
}

static int tegra_vcm30t124_dam0_init(struct snd_soc_pcm_runtime *rtd)
{
	struct tegra_vcm30t124 *machine = snd_soc_card_get_drvdata(rtd->card);
	unsigned int in_srate = 8000;
	int err;

	/* DAM input rate may be initialized through platform data */
	if (machine->pdata->num_dam)
		in_srate = machine->pdata->dam_in_srate[0];

	err = snd_soc_dai_set_sysclk(rtd->codec_dai, 0, in_srate,
					SND_SOC_CLOCK_IN);

	return 0;
}

static int tegra_vcm30t124_dam1_init(struct snd_soc_pcm_runtime *rtd)
{
	struct tegra_vcm30t124 *machine = snd_soc_card_get_drvdata(rtd->card);
	unsigned int in_srate = 8000;
	int err;

	/* DAM input rate may be initialized through platform data */
	if (machine->pdata->num_dam > 1)
		in_srate = machine->pdata->dam_in_srate[1];

	err = snd_soc_dai_set_sysclk(rtd->codec_dai, 0, in_srate,
					SND_SOC_CLOCK_IN);

	return 0;
}

static int tegra_vcm30t124_dam2_init(struct snd_soc_pcm_runtime *rtd)
{
	struct tegra_vcm30t124 *machine = snd_soc_card_get_drvdata(rtd->card);
	unsigned int in_srate = 8000;
	int err;

	/* DAM input rate may be initialized through platform data */
	if (machine->pdata->num_dam > 2)
		in_srate = machine->pdata->dam_in_srate[2];

	err = snd_soc_dai_set_sysclk(rtd->codec_dai, 0, in_srate,
					SND_SOC_CLOCK_IN);

	return 0;
}

static void tegra_vcm30t124_new_codec_links(
		struct tegra_vcm30t124_platform_data *pdata)
{
	int i, j;

	if (tegra_vcm30t124_codec_links)
		return;

	num_codec_links = pdata->dev_num;

	tegra_vcm30t124_codec_links = kzalloc(2 * num_codec_links *
		sizeof(struct snd_soc_dai_link), GFP_KERNEL);

	for (i = 0, j = num_codec_links; i < num_codec_links; i++, j++) {
		/* initialize DAI links on DAP side */
		tegra_vcm30t124_codec_links[i].name =
			pdata->dai_config[i].link_name;
		tegra_vcm30t124_codec_links[i].stream_name = "Playback";
		tegra_vcm30t124_codec_links[i].cpu_dai_name = "DAP";
		tegra_vcm30t124_codec_links[i].codec_dai_name =
			pdata->dai_config[i].codec_dai_name;
		tegra_vcm30t124_codec_links[i].cpu_name =
			pdata->dai_config[i].cpu_name;
		tegra_vcm30t124_codec_links[i].codec_name =
			pdata->dai_config[i].codec_name;
		tegra_vcm30t124_codec_links[i].params =
			&pdata->dai_config[i].params;
		tegra_vcm30t124_codec_links[i].dai_fmt =
			pdata->dai_config[i].dai_fmt;
		if (tegra_vcm30t124_codec_links[i].name) {
			if (strstr(tegra_vcm30t124_codec_links[i].name,
				"ad-playback"))
				tegra_vcm30t124_codec_links[i].init =
					tegra_vcm30t124_ad1937_init;
			else if (strstr(tegra_vcm30t124_codec_links[i].name,
				"wm-playback"))
				tegra_vcm30t124_codec_links[i].init =
					tegra_vcm30t124_wm8731_init;
			else if (strstr(tegra_vcm30t124_codec_links[i].name,
				"ak-playback"))
				tegra_vcm30t124_codec_links[i].init =
					tegra_vcm30t124_ak4618_init;
			else if (strstr(tegra_vcm30t124_codec_links[i].name,
				"bt-playback"))
				tegra_vcm30t124_codec_links[i].init =
					tegra_vcm30t124_bt_sco_init;
			else if (strstr(tegra_vcm30t124_codec_links[i].name,
				"spdif-playback"))
				tegra_vcm30t124_codec_links[i].init =
					tegra_vcm30t124_spdif_init;
			else if (strstr(tegra_vcm30t124_codec_links[i].name,
				"vc-playback"))
				tegra_vcm30t124_codec_links[i].init =
					tegra_vcm30t124_spdif_init;
			else
				tegra_vcm30t124_codec_links[i].init =
					tegra_vcm30t124_spdif_init;
		}

		/* initialize DAI links on CIF side */
		tegra_vcm30t124_codec_links[j].name =
			pdata->dai_config[i].cpu_dai_name;
		tegra_vcm30t124_codec_links[j].stream_name =
			pdata->dai_config[i].cpu_dai_name;
		tegra_vcm30t124_codec_links[j].cpu_dai_name =
			pdata->dai_config[i].cpu_dai_name;
		tegra_vcm30t124_codec_links[j].codec_dai_name = "CIF";
		tegra_vcm30t124_codec_links[j].cpu_name = "tegra30-ahub-xbar";
		tegra_vcm30t124_codec_links[j].codec_name =
			pdata->dai_config[i].cpu_name;
		tegra_vcm30t124_codec_links[j].params =
			&pdata->dai_config[i].params;
		if (pdata->dai_config[i].cpu_dai_name) {
			if (!strcmp(pdata->dai_config[i].cpu_dai_name,
				"I2S4")) {
				tegra_vcm30t124_codec_links[j].init =
					tegra_vcm30t124_i2s4_init;
				tegra_vcm30t124_codec_links[j].dai_fmt =
					pdata->dai_config[i].dai_fmt;
			}
		}
	}
}
static void tegra_vcm30t124_free_codec_links(void)
{
	kfree(tegra_vcm30t124_codec_links);
	tegra_vcm30t124_codec_links = NULL;
}
static void tegra_vcm30t124_new_codec_conf(
		struct tegra_vcm30t124_platform_data *pdata)
{
	int i;

	if (tegra_vcm30t124_codec_conf)
		return;

	num_codec_links = pdata->dev_num;

	tegra_vcm30t124_codec_conf = kzalloc(num_codec_links *
		sizeof(struct snd_soc_codec_conf), GFP_KERNEL);

	for (i = 0; i < num_codec_links; i++) {
		tegra_vcm30t124_codec_conf[i].dev_name =
			pdata->dai_config[i].codec_name;
		tegra_vcm30t124_codec_conf[i].name_prefix =
			pdata->dai_config[i].codec_prefix;
	}
}
static void tegra_vcm30t124_free_codec_conf(void)
{
	kfree(tegra_vcm30t124_codec_conf);
	tegra_vcm30t124_codec_conf = NULL;
}

static int tegra_vcm30t124_suspend_pre(struct snd_soc_card *card)
{
	unsigned int idx;

	/* DAPM dai link stream work for non pcm links */
	for (idx = 0; idx < card->num_rtd; idx++) {
		if (card->rtd[idx].dai_link->params)
			INIT_DELAYED_WORK(&card->rtd[idx].delayed_work, NULL);
	}
	return 0;
}

static const int tegra_vcm30t124_srate_values[] = {
	0,
	8000,
	16000,
	44100,
	48000,
	11025,
	22050,
	24000,
	32000,
	88200,
	96000,
	176000,
	192000,
};

static const char * const tegra_vcm30t124_srate_text[] = {
	"None",
	"8kHz",
	"16kHz",
	"44kHz",
	"48kHz",
	"11kHz",
	"22kHz",
	"24kHz",
	"32kHz",
	"88kHz",
	"96kHz",
	"176kHz",
	"192kHz",
};

static int tegra_vcm30t124_wm8731_get_rate(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct tegra_vcm30t124 *machine = snd_soc_card_get_drvdata(card);

	ucontrol->value.integer.value[0] = machine->wm_rate_via_kcontrol;

	return 0;
}

static int tegra_vcm30t124_wm8731_put_rate(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct tegra_vcm30t124 *machine = snd_soc_card_get_drvdata(card);
	unsigned int idx = tegra_vcm30t124_get_dai_link_idx("wm-playback");
	struct snd_soc_pcm_stream *dai_params =
		(struct snd_soc_pcm_stream *)card->dai_link[idx].params;

	/* set the rate control flag */
	machine->wm_rate_via_kcontrol = ucontrol->value.integer.value[0];

	/* update the dai params rate */
	dai_params->rate_min =
		tegra_vcm30t124_srate_values[machine->wm_rate_via_kcontrol];

	return 0;
}

static int tegra_vcm30t124_ad1937_get_rate(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct tegra_vcm30t124 *machine = snd_soc_card_get_drvdata(card);

	ucontrol->value.integer.value[0] = machine->ad_rate_via_kcontrol;

	return 0;
}

static int tegra_vcm30t124_ad1937_put_rate(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct tegra_vcm30t124 *machine = snd_soc_card_get_drvdata(card);
	unsigned int idx = tegra_vcm30t124_get_dai_link_idx("ad-playback");
	struct snd_soc_pcm_stream *dai_params =
		(struct snd_soc_pcm_stream *)card->dai_link[idx].params;

	/* set the rate control flag */
	machine->ad_rate_via_kcontrol = ucontrol->value.integer.value[0];

	/* update the dai params rate */
	dai_params->rate_min =
		tegra_vcm30t124_srate_values[machine->ad_rate_via_kcontrol];

	return 0;
}

static int tegra_vcm30t124_ak4618_get_rate(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct tegra_vcm30t124 *machine = snd_soc_card_get_drvdata(card);

	ucontrol->value.integer.value[0] = machine->ak_rate_via_kcontrol;

	return 0;
}

static int tegra_vcm30t124_ak4618_put_rate(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct tegra_vcm30t124 *machine = snd_soc_card_get_drvdata(card);
	unsigned int idx = tegra_vcm30t124_get_dai_link_idx("ak-playback");
	struct snd_soc_pcm_stream *dai_params =
		(struct snd_soc_pcm_stream *)card->dai_link[idx].params;

	/* set the rate control flag */
	machine->ak_rate_via_kcontrol = ucontrol->value.integer.value[0];

	/* update the dai params rate */
	dai_params->rate_min =
		tegra_vcm30t124_srate_values[machine->ak_rate_via_kcontrol];

	return 0;
}

static int tegra_vcm30t124_late_probe(struct snd_soc_card *card)
{
	unsigned int idx = tegra_vcm30t124_get_dai_link_idx("ad-playback");
	/*
	 * AD193X driver enables both DAC and ADC as MASTER in
	 * ad193x_set_dai_fmt function which is called during the
	 * card registration. If both ADC and DAC drive LRCLK and
	 * BCLK, it causes noise. To solve this, one of them is
	 * disabled in tegra_vcm30t124_ad1937_late_init function.
	 * This late_init function needs to be called from the card
	 * late_probe so that it is executed after ad193x_set_dai_fmt.
	 */
	return tegra_vcm30t124_ad1937_late_init(card->rtd + idx);
}

static int tegra_vcm30t124_remove(struct snd_soc_card *card)
{
	return 0;
}

static const struct soc_enum tegra_vcm30t124_codec_rate =
	SOC_ENUM_SINGLE_EXT(13, tegra_vcm30t124_srate_text);

static const struct snd_kcontrol_new tegra_vcm30t124_controls[] = {
	SOC_ENUM_EXT("codec-wm rate", tegra_vcm30t124_codec_rate,
		tegra_vcm30t124_wm8731_get_rate,
		tegra_vcm30t124_wm8731_put_rate),
	SOC_ENUM_EXT("codec-ad rate", tegra_vcm30t124_codec_rate,
		tegra_vcm30t124_ad1937_get_rate,
		tegra_vcm30t124_ad1937_put_rate),
	SOC_ENUM_EXT("codec-ak rate", tegra_vcm30t124_codec_rate,
		tegra_vcm30t124_ak4618_get_rate,
		tegra_vcm30t124_ak4618_put_rate),
};

static struct snd_soc_card snd_soc_tegra_vcm30t124 = {
	.name = "tegra-generic",
	.owner = THIS_MODULE,
	.late_probe = tegra_vcm30t124_late_probe,
	.remove = tegra_vcm30t124_remove,
	.suspend_pre = tegra_vcm30t124_suspend_pre,
	.dapm_widgets = tegra_vcm30t124_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tegra_vcm30t124_dapm_widgets),
	.controls = tegra_vcm30t124_controls,
	.num_controls = ARRAY_SIZE(tegra_vcm30t124_controls),
	.fully_routed = true,
};

static int tegra_vcm30t124_driver_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_tegra_vcm30t124;
	struct tegra_vcm30t124 *machine;
	int ret = 0, i, j, length;

	machine = devm_kzalloc(&pdev->dev, sizeof(struct tegra_vcm30t124),
			       GFP_KERNEL);
	if (!machine) {
		dev_err(&pdev->dev, "Can't allocate tegra_vcm30t124 struct\n");
		ret = -ENOMEM;
		goto err;
	}

	machine->pdata = (struct tegra_vcm30t124_platform_data *)
		of_get_property(pdev->dev.of_node, "platform_data", &length);
	if (NULL == machine->pdata) {
		dev_err(&pdev->dev, "platform data could not be retrieved from dt\n");
		return -ENODEV;
	}

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, machine);

	/* get the xbar dai link/codec conf structure */
	tegra_machine_dai_links = tegra_machine_get_dai_link();
	if (!tegra_machine_dai_links)
		goto err_alloc_dai_link;
	tegra_machine_codec_conf = tegra_machine_get_codec_conf();
	if (!tegra_machine_codec_conf)
		goto err_alloc_dai_link;

	/* set AMX/ADX dai_init */
	tegra_machine_set_dai_init(TEGRA124_DAI_LINK_AMX0,
		&tegra_vcm30t124_amx0_init);
	tegra_machine_set_dai_init(TEGRA124_DAI_LINK_AMX1,
		&tegra_vcm30t124_amx1_init);
	tegra_machine_set_dai_init(TEGRA124_DAI_LINK_ADX0,
		&tegra_vcm30t124_adx0_init);
	tegra_machine_set_dai_init(TEGRA124_DAI_LINK_ADX1,
		&tegra_vcm30t124_adx1_init);

    /* set AMX/ADX params */
	if (machine->pdata->num_amx) {
		switch (machine->pdata->num_amx) {
		case 2:
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_AMX1_0,
				&machine->pdata->amx_config[1].in_params[0]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_AMX1_1,
				&machine->pdata->amx_config[1].in_params[1]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_AMX1_2,
				&machine->pdata->amx_config[1].in_params[2]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_AMX1_3,
				&machine->pdata->amx_config[1].in_params[3]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_AMX1,
				&machine->pdata->amx_config[1].out_params[0]);
		case 1:
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_AMX0_0,
				&machine->pdata->amx_config[0].in_params[0]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_AMX0_1,
				&machine->pdata->amx_config[0].in_params[1]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_AMX0_2,
				&machine->pdata->amx_config[0].in_params[2]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_AMX0_3,
				&machine->pdata->amx_config[0].in_params[3]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_AMX0,
				&machine->pdata->amx_config[0].out_params[0]);
			break;
		default:
			break;
		}
	}

	if (machine->pdata->num_adx) {
		switch (machine->pdata->num_adx) {
		case 2:
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_ADX1_0,
				&machine->pdata->adx_config[1].out_params[0]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_ADX1_1,
				&machine->pdata->adx_config[1].out_params[1]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_ADX1_2,
				&machine->pdata->adx_config[1].out_params[2]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_ADX1_3,
				&machine->pdata->adx_config[1].out_params[3]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_ADX1,
				&machine->pdata->adx_config[1].in_params[0]);
		case 1:
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_ADX0_0,
				&machine->pdata->adx_config[0].out_params[0]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_ADX0_1,
				&machine->pdata->adx_config[0].out_params[1]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_ADX0_2,
				&machine->pdata->adx_config[0].out_params[2]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_ADX0_3,
				&machine->pdata->adx_config[0].out_params[3]);
			tegra_machine_set_dai_params(TEGRA124_DAI_LINK_ADX0,
				&machine->pdata->adx_config[0].in_params[0]);
			break;
		default:
			break;
		}
	}

	/* set DAM dai_init */
	tegra_machine_set_dai_init(TEGRA124_DAI_LINK_DAM0_0,
		&tegra_vcm30t124_dam0_init);
	tegra_machine_set_dai_init(TEGRA124_DAI_LINK_DAM1_0,
		&tegra_vcm30t124_dam1_init);
	tegra_machine_set_dai_init(TEGRA124_DAI_LINK_DAM2_0,
		&tegra_vcm30t124_dam2_init);

	/* get on board codec dai_links/conf */
	tegra_vcm30t124_new_codec_links(machine->pdata);
	tegra_vcm30t124_new_codec_conf(machine->pdata);

	/* set APBIF dai_ops */
	for (i = TEGRA124_DAI_LINK_APBIF0; i <= TEGRA124_DAI_LINK_APBIF9; i++)
		tegra_machine_set_dai_ops(i, &tegra_vcm30t124_spdif_ops);

	for (i = 0; i < num_codec_links; i++) {
		if (machine->pdata->dai_config[i].link_name) {
			if (!strcmp(machine->pdata->dai_config[i].link_name,
				"ad-playback")) {
				for (j = TEGRA124_DAI_LINK_APBIF0;
					j <= TEGRA124_DAI_LINK_APBIF3; j++)
					tegra_machine_set_dai_ops(j,
						&tegra_vcm30t124_ad1937_ops);
			} else if (!strcmp(
				machine->pdata->dai_config[i].link_name,
				"ak-playback")) {
				for (j = TEGRA124_DAI_LINK_APBIF4;
					j <= TEGRA124_DAI_LINK_APBIF7; j++)
					tegra_machine_set_dai_ops(j,
						&tegra_vcm30t124_ak4618_ops);
			} else if (!strcmp(
				machine->pdata->dai_config[i].link_name,
				"wm-playback")) {
					tegra_machine_set_dai_ops(
						TEGRA124_DAI_LINK_APBIF4,
						&tegra_vcm30t124_wm8731_ops);
			} else if (!strcmp(
				machine->pdata->dai_config[i].link_name,
				"bt-playback")) {
					tegra_machine_set_dai_ops(
						TEGRA124_DAI_LINK_APBIF8,
						&tegra_vcm30t124_bt_sco_ops);
			}
		}
	}

	/* append vcm30t124 specific dai_links */
	card->num_links =
		tegra_machine_append_dai_link(tegra_vcm30t124_codec_links,
			2 * num_codec_links);
	tegra_machine_dai_links = tegra_machine_get_dai_link();

	/* update APBIF dai links for virtualization only */
	if (of_find_compatible_node(NULL, NULL,
		"nvidia,tegra124-virt-ahub-master")) {
		for (i = 0; i < MAX_APBIF_ID; i++) {
			tegra_machine_dai_links[i].codec_dai_name =
				CODEC_DAI_NAME;
			tegra_machine_dai_links[i].codec_name = CODEC_NAME;
			tegra_machine_dai_links[i].cpu_name = LINK_CPU_NAME;
			tegra_machine_dai_links[i].platform_name = NULL;
			tegra_machine_set_dai_params(i, NULL);
		}
	}
	card->dai_link = tegra_machine_dai_links;

	/* append vcm30t124 specific codec_conf */
	card->num_configs =
		tegra_machine_append_codec_conf(tegra_vcm30t124_codec_conf,
			num_codec_links);
	tegra_machine_codec_conf = tegra_machine_get_codec_conf();
	card->codec_conf = tegra_machine_codec_conf;

	/* remove vcm30t124 specific dai links and conf */
	tegra_vcm30t124_free_codec_links();
	tegra_vcm30t124_free_codec_conf();

	machine->gpio_dap_direction = GPIO_PR0;
	machine->wm_rate_via_kcontrol = 0;
	machine->ad_rate_via_kcontrol = 0;
	machine->ak_rate_via_kcontrol = 0;
	card->dapm_routes = machine->pdata->dapm_routes;
	card->num_dapm_routes = machine->pdata->num_dapm_routes;

	if (machine->pdata->card_name)
		card->name = machine->pdata->card_name;

	max9485_info.addr = (unsigned int)machine->pdata->priv_data;

	if (max9485_info.addr) {
		machine->max9485_client = i2c_new_device(i2c_get_adapter(0),
							&max9485_info);
		if (!machine->max9485_client) {
			dev_err(&pdev->dev, "cannot get i2c device for max9485\n");
			goto err;
		}
	}

	ret = devm_gpio_request(&pdev->dev, machine->gpio_dap_direction,
				"dap_dir_control");
	if (ret) {
		dev_err(&pdev->dev, "cannot get dap_dir_control gpio\n");
		goto err_i2c_unregister;
	}

	ret = tegra_alt_asoc_utils_init(&machine->audio_clock,
					&pdev->dev,
					card);
	if (ret)
		goto err_gpio_free;

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err_fini_utils;
	}

	ret = tegra_asoc_hwdep_create(card);
	if (ret) {
		dev_err(&pdev->dev, "can't create tegra_machine_hwdep (%d)\n",
			ret);
		goto err_unregister_card;
	}

	return 0;
err_unregister_card:
	snd_soc_unregister_card(card);
err_fini_utils:
	tegra_alt_asoc_utils_fini(&machine->audio_clock);
err_gpio_free:
	devm_gpio_free(&pdev->dev, machine->gpio_dap_direction);
err_i2c_unregister:
	if (machine->max9485_client)
		i2c_unregister_device(machine->max9485_client);
err_alloc_dai_link:
	tegra_machine_remove_dai_link();
	tegra_machine_remove_codec_conf();
err:
	return ret;
}

static int tegra_vcm30t124_driver_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct tegra_vcm30t124 *machine = snd_soc_card_get_drvdata(card);

	snd_soc_unregister_card(card);

	tegra_machine_remove_dai_link();
	tegra_machine_remove_codec_conf();

	tegra_alt_asoc_utils_fini(&machine->audio_clock);
	devm_gpio_free(&pdev->dev, machine->gpio_dap_direction);
	if (machine->max9485_client)
		i2c_unregister_device(machine->max9485_client);

	return 0;
}

static const struct of_device_id tegra_vcm30t124_of_match[] = {
	{ .compatible = "nvidia,tegra-audio-vcm30t124", },
	{},
};

static struct platform_driver tegra_vcm30t124_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = tegra_vcm30t124_of_match,
	},
	.probe = tegra_vcm30t124_driver_probe,
	.remove = tegra_vcm30t124_driver_remove,
};
module_platform_driver(tegra_vcm30t124_driver);

MODULE_AUTHOR("Songhee Baek <sbaek@nvidia.com>");
MODULE_DESCRIPTION("Tegra+VCM30T124 machine ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, tegra_vcm30t124_of_match);
