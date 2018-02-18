/*
 * tegra210_sfc_alt.c - Tegra210 SFC driver
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
#include <linux/of_device.h>
#include <linux/delay.h>

#include "tegra210_xbar_alt.h"
#include "tegra210_sfc_alt.h"

#define DRV_NAME "tegra210-sfc"

static const struct reg_default tegra210_sfc_reg_defaults[] = {
	{ TEGRA210_SFC_AXBAR_RX_INT_MASK, 0x00000001},
	{ TEGRA210_SFC_AXBAR_RX_CIF_CTRL, 0x00007700},
	{ TEGRA210_SFC_AXBAR_TX_INT_MASK, 0x00000001},
	{ TEGRA210_SFC_AXBAR_TX_CIF_CTRL, 0x00007700},
	{ TEGRA210_SFC_CG, 0x1},
	{ TEGRA210_SFC_AHUBRAMCTL_SFC_CTRL, 0x00004000},
};

static int tegra210_sfc_rates[] = {
	8000,	/* #define TEGRA210_SFC_FS8				0	*/
	11025,	/* #define TEGRA210_SFC_FS11_025		1	*/
	16000,	/* #define TEGRA210_SFC_FS16			2	*/
	22050,	/* #define TEGRA210_SFC_FS22_05			3	*/
	24000,	/* #define TEGRA210_SFC_FS24			4	*/
	32000,	/* #define TEGRA210_SFC_FS32			5	*/
	44100,	/* #define TEGRA210_SFC_FS44_1			6	*/
	48000,	/* #define TEGRA210_SFC_FS48			7	*/
	64000,	/* #define TEGRA210_SFC_FS64			8	*/
	88200,	/* #define TEGRA210_SFC_FS88_2			9	*/
	96000,	/* #define TEGRA210_SFC_FS96			10	*/
	176400,	/* #define TEGRA210_SFC_FS176_4			11	*/
	192000,	/* #define TEGRA210_SFC_FS192			12	*/
};

static int tegra210_sfc_runtime_suspend(struct device *dev)
{
	struct tegra210_sfc *sfc = dev_get_drvdata(dev);

	regcache_cache_only(sfc->regmap, true);
	regcache_mark_dirty(sfc->regmap);

	pm_runtime_put_sync(dev->parent);

	return 0;
}

static int tegra210_sfc_runtime_resume(struct device *dev)
{
	struct tegra210_sfc *sfc = dev_get_drvdata(dev);
	int ret;

	ret = pm_runtime_get_sync(dev->parent);
	if (ret < 0) {
		dev_err(dev, "parent get_sync failed: %d\n", ret);
		return ret;
	}

	regcache_cache_only(sfc->regmap, false);
	regcache_sync(sfc->regmap);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tegra210_sfc_suspend(struct device *dev)
{
	return 0;
}
#endif

static int tegra210_sfc_set_dai_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
	struct tegra210_sfc *sfc = snd_soc_dai_get_drvdata(dai);
	unsigned int value;

	switch (freq) {
	case 8000:
		value = TEGRA210_SFC_FS8;
		break;
	case 11025:
		value = TEGRA210_SFC_FS11_025;
		break;
	case 16000:
		value = TEGRA210_SFC_FS16;
		break;
	case 22050:
		value = TEGRA210_SFC_FS22_05;
		break;
	case 24000:
		value = TEGRA210_SFC_FS24;
		break;
	case 32000:
		value = TEGRA210_SFC_FS32;
		break;
	case 44100:
		value = TEGRA210_SFC_FS44_1;
		break;
	case 48000:
		value = TEGRA210_SFC_FS48;
		break;
	case 64000:
		value = TEGRA210_SFC_FS64;
		break;
	case 88200:
		value = TEGRA210_SFC_FS88_2;
		break;
	case 96000:
		value = TEGRA210_SFC_FS96;
		break;
	case 176400:
		value = TEGRA210_SFC_FS176_4;
		break;
	case 192000:
		value = TEGRA210_SFC_FS192;
		break;
	default:
		value = TEGRA210_SFC_FS8;
		break;
	}

	if (dir == SND_SOC_CLOCK_OUT)
		sfc->srate_out = value;
	else if (dir == SND_SOC_CLOCK_IN)
		sfc->srate_in = value;

	return 0;
}

static int tegra210_sfc_write_coeff_ram(struct tegra210_sfc *sfc)
{
	u32 *coeff_ram = NULL;

	switch (sfc->srate_in) {
	case TEGRA210_SFC_FS8:
		if (sfc->srate_out == TEGRA210_SFC_FS48)
			coeff_ram = coef_8to48;
		else if (sfc->srate_out == TEGRA210_SFC_FS44_1)
			coeff_ram = coef_8to44;
		else if (sfc->srate_out == TEGRA210_SFC_FS24)
			coeff_ram = coef_8to24;
		else if (sfc->srate_out == TEGRA210_SFC_FS16)
			coeff_ram = coef_8to16;
		break;

	case TEGRA210_SFC_FS16:
		if (sfc->srate_out == TEGRA210_SFC_FS48)
			coeff_ram = coef_16to48;
		else if (sfc->srate_out == TEGRA210_SFC_FS44_1)
			coeff_ram = coef_16to44;
		else if (sfc->srate_out == TEGRA210_SFC_FS24)
			coeff_ram = coef_16to24;
		else if (sfc->srate_out == TEGRA210_SFC_FS8)
			coeff_ram = coef_16to8;
		break;

	case TEGRA210_SFC_FS44_1:
		if (sfc->srate_out == TEGRA210_SFC_FS8)
			coeff_ram = coef_44to8;
		else if (sfc->srate_out == TEGRA210_SFC_FS16)
			coeff_ram = coef_44to16;
		else if (sfc->srate_out == TEGRA210_SFC_FS48)
			coeff_ram = coef_44to48;
		break;

	case TEGRA210_SFC_FS48:
		if (sfc->srate_out == TEGRA210_SFC_FS8)
			coeff_ram = coef_48to8;
		else if (sfc->srate_out == TEGRA210_SFC_FS16)
			coeff_ram = coef_48to16;
		else if (sfc->srate_out == TEGRA210_SFC_FS24)
			coeff_ram = coef_48to24;
		else if (sfc->srate_out == TEGRA210_SFC_FS44_1)
			coeff_ram = coef_48to44;
		else if (sfc->srate_out == TEGRA210_SFC_FS96)
			coeff_ram = coef_48to96;
		else if (sfc->srate_out == TEGRA210_SFC_FS192)
			coeff_ram = coef_48to192;
		break;

	case TEGRA210_SFC_FS11_025:
		if (sfc->srate_out == TEGRA210_SFC_FS48)
			coeff_ram = coef_11to48;
		else if (sfc->srate_out == TEGRA210_SFC_FS44_1)
			coeff_ram = coef_11to44;
		break;

	case TEGRA210_SFC_FS22_05:
		if (sfc->srate_out == TEGRA210_SFC_FS48)
			coeff_ram = coef_22to48;
		else if (sfc->srate_out == TEGRA210_SFC_FS44_1)
			coeff_ram = coef_22to44;
		break;

	case TEGRA210_SFC_FS24:
		if (sfc->srate_out == TEGRA210_SFC_FS48)
			coeff_ram = coef_24to48;
		else if (sfc->srate_out == TEGRA210_SFC_FS44_1)
			coeff_ram = coef_24to44;
		break;

	case TEGRA210_SFC_FS32:
		if (sfc->srate_out == TEGRA210_SFC_FS48)
			coeff_ram = coef_32to48;
		else if (sfc->srate_out == TEGRA210_SFC_FS44_1)
			coeff_ram = coef_32to44;
		break;

	case TEGRA210_SFC_FS88_2:
		if (sfc->srate_out == TEGRA210_SFC_FS48)
			coeff_ram = coef_88to48;
		else if (sfc->srate_out == TEGRA210_SFC_FS44_1)
			coeff_ram = coef_88to44;
		break;

	case TEGRA210_SFC_FS96:
		if (sfc->srate_out == TEGRA210_SFC_FS48)
			coeff_ram = coef_96to48;
		else if (sfc->srate_out == TEGRA210_SFC_FS44_1)
			coeff_ram = coef_96to44;
		break;

	case TEGRA210_SFC_FS176_4:
		if (sfc->srate_out == TEGRA210_SFC_FS48)
			coeff_ram = coef_176to48;
		else if (sfc->srate_out == TEGRA210_SFC_FS44_1)
			coeff_ram = coef_176to44;
		break;

	case TEGRA210_SFC_FS192:
		if (sfc->srate_out == TEGRA210_SFC_FS48)
			coeff_ram = coef_192to48;
		else if (sfc->srate_out == TEGRA210_SFC_FS44_1)
			coeff_ram = coef_192to44;
		break;

	default:
		pr_err("SFC input rate not supported: %d\n",
			-EINVAL);
		break;
	}

	if (coeff_ram) {
		tegra210_xbar_write_ahubram(sfc->regmap,
			TEGRA210_SFC_AHUBRAMCTL_SFC_CTRL,
			TEGRA210_SFC_AHUBRAMCTL_SFC_DATA,
			0, coeff_ram, TEGRA210_SFC_COEF_RAM_DEPTH);

		regmap_update_bits(sfc->regmap,
			TEGRA210_SFC_COEF_RAM,
			TEGRA210_SFC_COEF_RAM_COEF_RAM_EN,
			TEGRA210_SFC_COEF_RAM_COEF_RAM_EN);
	}

	return 0;
}

static const int tegra210_sfc_fmt_values[] = {
	0,
	TEGRA210_AUDIOCIF_BITS_16,
	TEGRA210_AUDIOCIF_BITS_32,
};

static int tegra210_sfc_set_audio_cif(struct tegra210_sfc *sfc,
				struct snd_pcm_hw_params *params,
				unsigned int reg)
{
	int channels, audio_bits;
	struct tegra210_xbar_cif_conf cif_conf;

	memset(&cif_conf, 0, sizeof(struct tegra210_xbar_cif_conf));

	channels = params_channels(params);
	if (channels < 2)
		return -EINVAL;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		audio_bits = TEGRA210_AUDIOCIF_BITS_16;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		audio_bits = TEGRA210_AUDIOCIF_BITS_32;
		break;
	default:
		return -EINVAL;
	}

	if (sfc->stereo_conv_input > 0 && 2 == channels &&
		(reg == TEGRA210_SFC_AXBAR_RX_CIF_CTRL)) {
		cif_conf.stereo_conv = sfc->stereo_conv_input - 1;
		cif_conf.client_channels = 1;
	} else if (sfc->mono_conv_output > 0 && 2 == channels &&
		(reg == TEGRA210_SFC_AXBAR_TX_CIF_CTRL)) {
		cif_conf.mono_conv = sfc->mono_conv_output - 1;
		cif_conf.client_channels = 1;
	} else {
		cif_conf.client_channels = channels;
	}

	cif_conf.audio_channels = channels;
	cif_conf.audio_bits = audio_bits;
	if (sfc->format_in && (reg == TEGRA210_SFC_AXBAR_RX_CIF_CTRL))
		cif_conf.audio_bits = tegra210_sfc_fmt_values[sfc->format_in];
	if (sfc->format_out && (reg == TEGRA210_SFC_AXBAR_TX_CIF_CTRL))
		cif_conf.audio_bits = tegra210_sfc_fmt_values[sfc->format_out];
	cif_conf.client_bits = TEGRA210_AUDIOCIF_BITS_32;

	sfc->soc_data->set_audio_cif(sfc->regmap, reg, &cif_conf);

	return 0;
}

static int tegra210_sfc_soft_reset(struct tegra210_sfc *sfc)
{
	u32 val;
	int cnt = 10;
	int ret = 0;

	regmap_update_bits(sfc->regmap,
			TEGRA210_SFC_SOFT_RESET,
			TEGRA210_SFC_SOFT_RESET_EN,
			1);
	do {
		udelay(100);
		regmap_read(sfc->regmap, TEGRA210_SFC_SOFT_RESET, &val);
	} while ((val & TEGRA210_SFC_SOFT_RESET_EN) && cnt--);
	if (!cnt)
		ret = -ETIMEDOUT;
	return ret;
}

static int tegra210_sfc_in_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct tegra210_sfc *sfc = snd_soc_dai_get_drvdata(dai);
	int ret;

	regmap_update_bits(sfc->regmap,
			TEGRA210_SFC_COEF_RAM,
			TEGRA210_SFC_COEF_RAM_COEF_RAM_EN,
			0);

	ret = tegra210_sfc_soft_reset(sfc);
	if (ret) {
		dev_err(dev, "SOFT_RESET error: %d\n", ret);
		return ret;
	}

	ret = tegra210_sfc_set_audio_cif(sfc, params,
				TEGRA210_SFC_AXBAR_RX_CIF_CTRL);
	if (ret) {
		dev_err(dev, "Can't set SFC RX CIF: %d\n", ret);
		return ret;
	}
	memcpy(&sfc->in_hw_params, params, sizeof(struct snd_pcm_hw_params));

	regmap_write(sfc->regmap, TEGRA210_SFC_AXBAR_RX_FREQ, sfc->srate_in);

	if (sfc->srate_in != sfc->srate_out)
		tegra210_sfc_write_coeff_ram(sfc);

	return ret;
}

static int tegra210_sfc_out_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct tegra210_sfc *sfc = snd_soc_dai_get_drvdata(dai);
	int ret;

	ret = tegra210_sfc_set_audio_cif(sfc, params,
				TEGRA210_SFC_AXBAR_TX_CIF_CTRL);
	if (ret) {
		dev_err(dev, "Can't set SFC TX CIF: %d\n", ret);
		return ret;
	}
	memcpy(&sfc->out_hw_params, params, sizeof(struct snd_pcm_hw_params));

	if (sfc->srate_out < 0) {
		dev_err(dev, "SFC%d output rate not set: %d\n",
			dev->id, -EINVAL);
		return -EINVAL;
	}

	regmap_write(sfc->regmap, TEGRA210_SFC_AXBAR_TX_FREQ, sfc->srate_out);
	return ret;
}

static int tegra210_sfc_rate_to_index(int rate)
{
	int index;
	for (index = 0; index < ARRAY_SIZE(tegra210_sfc_rates); index++) {
		if (rate == tegra210_sfc_rates[index])
			return index;
	}
	return -1;
}

static int tegra210_sfc_get_srate(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_sfc *sfc = snd_soc_codec_get_drvdata(codec);

	/* get the sfc output rate */
	if (strstr(kcontrol->id.name, "input"))
		ucontrol->value.integer.value[0] = tegra210_sfc_rates[sfc->srate_in];
	else if (strstr(kcontrol->id.name, "output"))
		ucontrol->value.integer.value[0] = tegra210_sfc_rates[sfc->srate_out];

	return 0;
}

static int tegra210_sfc_put_srate(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_sfc *sfc = snd_soc_codec_get_drvdata(codec);
	int srate = tegra210_sfc_rate_to_index(ucontrol->value.integer.value[0]);

	if ((srate < TEGRA210_SFC_FS8) || (srate > TEGRA210_SFC_FS192))
		return -EINVAL;

	/* Update the SFC input/output rate */
	if (strstr(kcontrol->id.name, "input"))
		sfc->srate_in = srate;
	else if (strstr(kcontrol->id.name, "output"))
		sfc->srate_out = srate;

	return 0;
}

static int tegra210_sfc_get_format(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_sfc *sfc = snd_soc_codec_get_drvdata(codec);

	/* get the format control flag */
	if (strstr(kcontrol->id.name, "input"))
		ucontrol->value.integer.value[0] = sfc->format_in;

	if (strstr(kcontrol->id.name, "output"))
		ucontrol->value.integer.value[0] = sfc->format_out;

	return 0;
}

static int tegra210_sfc_put_format(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_sfc *sfc = snd_soc_codec_get_drvdata(codec);

	/* set the format control flag */
	if (strstr(kcontrol->id.name, "input"))
		sfc->format_in = ucontrol->value.integer.value[0];

	if (strstr(kcontrol->id.name, "output"))
		sfc->format_out = ucontrol->value.integer.value[0];

	return 0;
}

static int tegra210_sfc_init_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int tegra210_sfc_init_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_sfc *sfc = snd_soc_codec_get_drvdata(codec);
	int init = ucontrol->value.enumerated.item[0];
	int ret = 0;
	int is_enabled = 0;

	if (!init)
		return ret;

	dev_dbg(codec->dev, "%s: inrate %d outrate %d\n",
		__func__, sfc->srate_in, sfc->srate_out);

	ret = pm_runtime_get_sync(codec->dev->parent);
	if (ret < 0) {
		dev_err(codec->dev, "parent get_sync failed: %d\n", ret);
		return ret;
	}

	regmap_read(sfc->regmap, TEGRA210_SFC_ENABLE, &is_enabled);

	if (is_enabled) {
		u32 val;
		int cnt = 100;

		regmap_write(sfc->regmap, TEGRA210_SFC_ENABLE, 0);

		regmap_read(sfc->regmap, TEGRA210_SFC_STATUS, &val);
		while ((val & 1) && cnt--) {
			udelay(100);
			regmap_read(sfc->regmap, TEGRA210_SFC_STATUS, &val);
		}

		if (!cnt)
			dev_warn(codec->dev, "SFC disable timeout\n");

		regmap_update_bits(sfc->regmap,
				TEGRA210_SFC_COEF_RAM,
				TEGRA210_SFC_COEF_RAM_COEF_RAM_EN,
				0);

		ret = tegra210_sfc_soft_reset(sfc);
		if (ret) {
			dev_err(codec->dev, "SOFT_RESET error: %d\n", ret);
			return ret;
		}

		ret = tegra210_sfc_set_audio_cif(sfc, &sfc->in_hw_params,
					TEGRA210_SFC_AXBAR_RX_CIF_CTRL);
		if (ret) {
			dev_err(codec->dev, "Can't set SFC RX CIF: %d\n", ret);
			return ret;
		}

		ret = tegra210_sfc_set_audio_cif(sfc, &sfc->out_hw_params,
						TEGRA210_SFC_AXBAR_TX_CIF_CTRL);
		if (ret) {
			dev_err(codec->dev, "Can't set SFC TX CIF: %d\n", ret);
			return ret;
		}

		regmap_write(sfc->regmap, TEGRA210_SFC_AXBAR_RX_FREQ, sfc->srate_in);
		regmap_write(sfc->regmap, TEGRA210_SFC_AXBAR_TX_FREQ, sfc->srate_out);

		if (sfc->srate_in != sfc->srate_out)
			tegra210_sfc_write_coeff_ram(sfc);

		regmap_write(sfc->regmap, TEGRA210_SFC_ENABLE, 1);
	}

	pm_runtime_put(codec->dev->parent);

	return 0;
}

static int tegra210_sfc_get_stereo_conv(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_sfc *sfc = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = sfc->stereo_conv_input;
	return 0;
}

static int tegra210_sfc_put_stereo_conv(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_sfc *sfc = snd_soc_codec_get_drvdata(codec);

	sfc->stereo_conv_input = ucontrol->value.integer.value[0];
	return 0;
}

static int tegra210_sfc_get_mono_conv(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_sfc *sfc = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = sfc->mono_conv_output;
	return 0;
}

static int tegra210_sfc_put_mono_conv(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_sfc *sfc = snd_soc_codec_get_drvdata(codec);

	sfc->mono_conv_output = ucontrol->value.integer.value[0];
	return 0;
}

static int tegra210_sfc_codec_probe(struct snd_soc_codec *codec)
{
	struct tegra210_sfc *sfc = snd_soc_codec_get_drvdata(codec);

	codec->control_data = sfc->regmap;

	return 0;
}

static struct snd_soc_dai_ops tegra210_sfc_in_dai_ops = {
	.hw_params	= tegra210_sfc_in_hw_params,
	.set_sysclk	= tegra210_sfc_set_dai_sysclk,
};

static struct snd_soc_dai_ops tegra210_sfc_out_dai_ops = {
	.hw_params	= tegra210_sfc_out_hw_params,
};

static struct snd_soc_dai_driver tegra210_sfc_dais[] = {
	{
		.name = "CIF",
		.playback = {
			.stream_name = "SFC Receive",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_96000,
			.formats = SNDRV_PCM_FMTBIT_S8 |
				SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &tegra210_sfc_in_dai_ops,
	},
	{
		.name = "DAP",
		.capture = {
			.stream_name = "SFC Transmit",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_96000,
			.formats = SNDRV_PCM_FMTBIT_S8 |
				SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &tegra210_sfc_out_dai_ops,
	}
};

static const struct snd_soc_dapm_widget tegra210_sfc_widgets[] = {
	SND_SOC_DAPM_AIF_IN("SFC RX", NULL, 0, SND_SOC_NOPM,
				0, 0),
	SND_SOC_DAPM_AIF_OUT("SFC TX", NULL, 0, TEGRA210_SFC_ENABLE,
				TEGRA210_SFC_EN_SHIFT, 0),
};

static const struct snd_soc_dapm_route tegra210_sfc_routes[] = {
	{ "SFC RX",       NULL, "SFC Receive" },
	{ "SFC TX",       NULL, "SFC RX" },
	{ "SFC Transmit", NULL, "SFC TX" },
};

static const char * const tegra210_sfc_format_text[] = {
	"None",
	"16",
	"32",
};

static const char * const tegra210_sfc_stereo_conv_text[] = {
	"None", "CH0", "CH1", "AVG",
};

static const char * const tegra210_sfc_mono_conv_text[] = {
	"None", "ZERO", "COPY",
};

static const struct soc_enum tegra210_sfc_format_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
		ARRAY_SIZE(tegra210_sfc_format_text),
		tegra210_sfc_format_text);

static const struct soc_enum tegra210_sfc_stereo_conv_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
		ARRAY_SIZE(tegra210_sfc_stereo_conv_text),
		tegra210_sfc_stereo_conv_text);

static const struct soc_enum tegra210_sfc_mono_conv_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
		ARRAY_SIZE(tegra210_sfc_mono_conv_text),
		tegra210_sfc_mono_conv_text);

static const struct snd_kcontrol_new tegra210_sfc_controls[] = {
	SOC_SINGLE_EXT("input rate", 0, 0, 192000, 0,
		tegra210_sfc_get_srate, tegra210_sfc_put_srate),
	SOC_SINGLE_EXT("output rate", 0, 0, 192000, 0,
		tegra210_sfc_get_srate, tegra210_sfc_put_srate),
	SOC_ENUM_EXT("input bit format", tegra210_sfc_format_enum,
		tegra210_sfc_get_format, tegra210_sfc_put_format),
	SOC_ENUM_EXT("output bit format", tegra210_sfc_format_enum,
		tegra210_sfc_get_format, tegra210_sfc_put_format),
	SOC_SINGLE_EXT("init", 0, 0, 1, 0,
		tegra210_sfc_init_get, tegra210_sfc_init_put),
	SOC_ENUM_EXT("input stereo conv", tegra210_sfc_stereo_conv_enum,
		tegra210_sfc_get_stereo_conv, tegra210_sfc_put_stereo_conv),
	SOC_ENUM_EXT("output mono conv", tegra210_sfc_mono_conv_enum,
		tegra210_sfc_get_mono_conv, tegra210_sfc_put_mono_conv),
};

static struct snd_soc_codec_driver tegra210_sfc_codec = {
	.probe = tegra210_sfc_codec_probe,
	.dapm_widgets = tegra210_sfc_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tegra210_sfc_widgets),
	.dapm_routes = tegra210_sfc_routes,
	.num_dapm_routes = ARRAY_SIZE(tegra210_sfc_routes),
	.controls = tegra210_sfc_controls,
	.num_controls = ARRAY_SIZE(tegra210_sfc_controls),
	.idle_bias_off = 1,
};

static bool tegra210_sfc_wr_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA210_SFC_AXBAR_RX_INT_MASK:
	case TEGRA210_SFC_AXBAR_RX_INT_SET:
	case TEGRA210_SFC_AXBAR_RX_INT_CLEAR:
	case TEGRA210_SFC_AXBAR_RX_CIF_CTRL:
	case TEGRA210_SFC_AXBAR_RX_FREQ:

	case TEGRA210_SFC_AXBAR_TX_INT_MASK:
	case TEGRA210_SFC_AXBAR_TX_INT_SET:
	case TEGRA210_SFC_AXBAR_TX_INT_CLEAR:
	case TEGRA210_SFC_AXBAR_TX_CIF_CTRL:
	case TEGRA210_SFC_AXBAR_TX_FREQ:

	case TEGRA210_SFC_ENABLE:
	case TEGRA210_SFC_SOFT_RESET:
	case TEGRA210_SFC_CG:
	case TEGRA210_SFC_COEF_RAM:
	case TEGRA210_SFC_AHUBRAMCTL_SFC_CTRL:
	case TEGRA210_SFC_AHUBRAMCTL_SFC_DATA:
		return true;
	default:
		return false;
	};
}

static bool tegra210_sfc_rd_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA210_SFC_AXBAR_RX_STATUS:
	case TEGRA210_SFC_AXBAR_RX_INT_STATUS:
	case TEGRA210_SFC_AXBAR_RX_INT_MASK:
	case TEGRA210_SFC_AXBAR_RX_INT_SET:
	case TEGRA210_SFC_AXBAR_RX_INT_CLEAR:
	case TEGRA210_SFC_AXBAR_RX_CIF_CTRL:
	case TEGRA210_SFC_AXBAR_RX_FREQ:

	case TEGRA210_SFC_AXBAR_TX_STATUS:
	case TEGRA210_SFC_AXBAR_TX_INT_STATUS:
	case TEGRA210_SFC_AXBAR_TX_INT_MASK:
	case TEGRA210_SFC_AXBAR_TX_INT_SET:
	case TEGRA210_SFC_AXBAR_TX_INT_CLEAR:
	case TEGRA210_SFC_AXBAR_TX_CIF_CTRL:
	case TEGRA210_SFC_AXBAR_TX_FREQ:

	case TEGRA210_SFC_ENABLE:
	case TEGRA210_SFC_SOFT_RESET:
	case TEGRA210_SFC_CG:
	case TEGRA210_SFC_STATUS:
	case TEGRA210_SFC_INT_STATUS:
	case TEGRA210_SFC_COEF_RAM:
	case TEGRA210_SFC_AHUBRAMCTL_SFC_CTRL:
	case TEGRA210_SFC_AHUBRAMCTL_SFC_DATA:
		return true;
	default:
		return false;
	};
}

static bool tegra210_sfc_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA210_SFC_AXBAR_RX_STATUS:
	case TEGRA210_SFC_AXBAR_RX_INT_STATUS:
	case TEGRA210_SFC_AXBAR_RX_INT_SET:

	case TEGRA210_SFC_AXBAR_TX_STATUS:
	case TEGRA210_SFC_AXBAR_TX_INT_STATUS:
	case TEGRA210_SFC_AXBAR_TX_INT_SET:

	case TEGRA210_SFC_SOFT_RESET:
	case TEGRA210_SFC_STATUS:
	case TEGRA210_SFC_INT_STATUS:
	case TEGRA210_SFC_AHUBRAMCTL_SFC_CTRL:
	case TEGRA210_SFC_AHUBRAMCTL_SFC_DATA:
		return true;
	default:
		return false;
	};
}

static bool tegra210_sfc_precious_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA210_SFC_AHUBRAMCTL_SFC_DATA:
		return true;
	default:
		return false;
	};
}

static const struct regmap_config tegra210_sfc_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = TEGRA210_SFC_AHUBRAMCTL_SFC_DATA,
	.writeable_reg = tegra210_sfc_wr_reg,
	.readable_reg = tegra210_sfc_rd_reg,
	.volatile_reg = tegra210_sfc_volatile_reg,
	.precious_reg = tegra210_sfc_precious_reg,
	.reg_defaults = tegra210_sfc_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(tegra210_sfc_reg_defaults),
	.cache_type = REGCACHE_FLAT,
};

static const struct tegra210_sfc_soc_data soc_data_tegra210 = {
	.set_audio_cif = tegra210_xbar_set_cif,
};

static const struct of_device_id tegra210_sfc_of_match[] = {
	{ .compatible = "nvidia,tegra210-sfc", .data = &soc_data_tegra210 },
	{},
};

static int tegra210_sfc_platform_probe(struct platform_device *pdev)
{
	struct tegra210_sfc *sfc;
	struct resource *mem, *memregion;
	void __iomem *regs;
	int ret = 0;
	const struct of_device_id *match;
	struct tegra210_sfc_soc_data *soc_data;

	match = of_match_device(tegra210_sfc_of_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "Error: No device match found\n");
		ret = -ENODEV;
		goto err;
	}
	soc_data = (struct tegra210_sfc_soc_data *)match->data;

	sfc = devm_kzalloc(&pdev->dev, sizeof(struct tegra210_sfc), GFP_KERNEL);
	if (!sfc) {
		dev_err(&pdev->dev, "Can't allocate sfc\n");
		ret = -ENOMEM;
		goto err;
	}

	sfc->soc_data = soc_data;

	/* initialize default output srate */
	sfc->srate_out = TEGRA210_SFC_FS48;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "No memory resource\n");
		ret = -ENODEV;
		goto err;
	}

	memregion = devm_request_mem_region(&pdev->dev, mem->start,
					    resource_size(mem), pdev->name);
	if (!memregion) {
		dev_err(&pdev->dev, "Memory region already claimed\n");
		ret = -EBUSY;
		goto err;
	}

	regs = devm_ioremap(&pdev->dev, mem->start, resource_size(mem));
	if (!regs) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENOMEM;
		goto err;
	}

	sfc->regmap = devm_regmap_init_mmio(&pdev->dev, regs,
					    &tegra210_sfc_regmap_config);
	if (IS_ERR(sfc->regmap)) {
		dev_err(&pdev->dev, "regmap init failed\n");
		ret = PTR_ERR(sfc->regmap);
		goto err;
	}
	regcache_cache_only(sfc->regmap, true);

	if (of_property_read_u32(pdev->dev.of_node,
				"nvidia,ahub-sfc-id",
				&pdev->dev.id) < 0) {
		dev_err(&pdev->dev,
			"Missing property nvidia,ahub-sfc-id\n");
		ret = -ENODEV;
		goto err;
	}

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		ret = tegra210_sfc_runtime_resume(&pdev->dev);
		if (ret)
			goto err_pm_disable;
	}

	ret = snd_soc_register_codec(&pdev->dev, &tegra210_sfc_codec,
				     tegra210_sfc_dais,
				     ARRAY_SIZE(tegra210_sfc_dais));
	if (ret != 0) {
		dev_err(&pdev->dev, "Could not register CODEC: %d\n", ret);
		goto err_suspend;
	}

	dev_set_drvdata(&pdev->dev, sfc);

	return 0;

err_suspend:
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra210_sfc_runtime_suspend(&pdev->dev);
err_pm_disable:
	pm_runtime_disable(&pdev->dev);
err:
	return ret;
}

static int tegra210_sfc_platform_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra210_sfc_runtime_suspend(&pdev->dev);

	return 0;
}

static const struct dev_pm_ops tegra210_sfc_pm_ops = {
	SET_RUNTIME_PM_OPS(tegra210_sfc_runtime_suspend,
			   tegra210_sfc_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(tegra210_sfc_suspend, NULL)
};

static struct platform_driver tegra210_sfc_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = tegra210_sfc_of_match,
		.pm = &tegra210_sfc_pm_ops,
	},
	.probe = tegra210_sfc_platform_probe,
	.remove = tegra210_sfc_platform_remove,
};
module_platform_driver(tegra210_sfc_driver)

MODULE_AUTHOR("Arun Shamanna Lakshmi <aruns@nvidia.com>");
MODULE_DESCRIPTION("Tegra210 SFC ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, tegra210_sfc_of_match);
