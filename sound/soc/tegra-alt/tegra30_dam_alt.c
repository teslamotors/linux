/*
 * tegra30_dam_alt.c - Tegra30 DAM driver
 *
 * Copyright (c) 2014-2015 NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/of_device.h>

#include "tegra30_xbar_alt.h"
#include "tegra30_dam_alt.h"

#define DRV_NAME "tegra30_dam"

#define TEGRA_DAM_CH_REG(id)	TEGRA_DAM_CH##id##_CONV

static const struct reg_default tegra30_dam_reg_defaults[] = {
	{TEGRA_DAM_CTRL, 0x00000000},
	{TEGRA_DAM_CLIP, 0x00000000},
	{TEGRA_DAM_CLIP_THRESHOLD, 0x007fffff},
	{TEGRA_DAM_AUDIOCIF_OUT_CTRL, 0x00001100},
	{TEGRA_DAM_CH0_CTRL, 0x00000000},
	{TEGRA_DAM_CH0_CONV, 0x00001000},
	{TEGRA_DAM_AUDIOCIF_CH0_CTRL, 0x00001100},
	{TEGRA_DAM_CH1_CTRL, 0x00000010},
	{TEGRA_DAM_CH1_CONV, 0x00001000},
	{TEGRA_DAM_AUDIOCIF_CH1_CTRL, 0x00001100},
};

static int tegra30_dam_farrow_param_lookup[10][3] = {
	/* fs_in, fs_out, farrow_param */
	{TEGRA_DAM_FS8, TEGRA_DAM_FS48, TEGRA_DAM_FARROW_PARAM_1},
	{TEGRA_DAM_FS8, TEGRA_DAM_FS44, TEGRA_DAM_FARROW_PARAM_2},
	{TEGRA_DAM_FS8, TEGRA_DAM_FS16, TEGRA_DAM_FARROW_PARAM_1},
	{TEGRA_DAM_FS16, TEGRA_DAM_FS48, TEGRA_DAM_FARROW_PARAM_1},
	{TEGRA_DAM_FS16, TEGRA_DAM_FS44, TEGRA_DAM_FARROW_PARAM_2},
	{TEGRA_DAM_FS16, TEGRA_DAM_FS8, TEGRA_DAM_FARROW_PARAM_1},
	{TEGRA_DAM_FS44, TEGRA_DAM_FS16, TEGRA_DAM_FARROW_PARAM_3},
	{TEGRA_DAM_FS44, TEGRA_DAM_FS8, TEGRA_DAM_FARROW_PARAM_3},
	{TEGRA_DAM_FS48, TEGRA_DAM_FS16, TEGRA_DAM_FARROW_PARAM_1},
	{TEGRA_DAM_FS48, TEGRA_DAM_FS8, TEGRA_DAM_FARROW_PARAM_1},
};

static int tegra30_dam_freq_lookup[8][3] = {
	/* fs_in, fs_out, num_filt_stages-1 */
	{TEGRA_DAM_FS16, TEGRA_DAM_FS48, 0},
	{TEGRA_DAM_FS8, TEGRA_DAM_FS48, 1},
	{TEGRA_DAM_FS8, TEGRA_DAM_FS44,	2},
	{TEGRA_DAM_FS16, TEGRA_DAM_FS44, 3},
	{TEGRA_DAM_FS48, TEGRA_DAM_FS16, 0},
	{TEGRA_DAM_FS48, TEGRA_DAM_FS8, 1},
	{TEGRA_DAM_FS44, TEGRA_DAM_FS16, 2},
	{TEGRA_DAM_FS44, TEGRA_DAM_FS8, 2},
};

static int coeff_ram_16_44[64] = {
				0x156105, /* IIR Filter + interpolation */
				0x0000d649,
				0x00e87afb, 0xff5f69d0, 0x003df3cf,
				0x007ce488, 0xff99a5c8, 0x0056a6a0,
				0x00344928, 0xffcba3e5, 0x006be470,
				0x00137aa7, 0xffe60276, 0x00773410,
				0x0005fa2a, 0xfff1ac11, 0x007c795b,
				0x00012d36, 0xfff5eca2, 0x007f10ef,
				0x2,
				0x21a102, /* IIR Filter + interplation */
				0x00000e00,
				0x00e2e000, 0xff6e1a00, 0x002aaa00,
				0x00610a00, 0xff5dda00, 0x003ccc00,
				0x00163a00, 0xff3c0400, 0x00633200,
				0x3,
				0x2c0204, /* Farrow Filter */
				0x000aaaab,
				0xffaaaaab,
				0xfffaaaab,
				0x00555555,
				0xff600000,
				0xfff55555,
				0x00155555,
				0x00055555,
				0xffeaaaab,
				0x00200000,
				0x005101, /* IIR Filter + Decimator */
				8252,
				16067893, -13754014, 5906912,
				13037808, -13709975, 7317389,
				1,
				0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0
};

static int coeff_ram_8_48[64] = {
				0x156105, /* interpolation + Filter */
				0x0000d649,
				0x00e87afb, 0xff5f69d0, 0x003df3cf,
				0x007ce488, 0xff99a5c8, 0x0056a6a0,
				0x00344928, 0xffcba3e5, 0x006be470,
				0x00137aa7, 0xffe60276, 0x00773410,
				0x0005fa2a, 0xfff1ac11, 0x007c795b,
				0x00012d36, 0xfff5eca2, 0x007f10ef,
				0x2, /* ouptut gain */
				0x00a102, /* filter + interpolation */
				0x00000e00,
				0x00e2e000, 0xff6e1a00, 0x002aaa00,
				0x00610a00, 0xff5dda00, 0x003ccc00,
				0x00163a00, 0xff3c0400, 0x00633200,
				0x3,
				0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0,
				0
};

static int coeff_ram_16_48[64] = {
				0x00a105, /* interpolation + Filter */
				1924,
				13390190, -13855175, 5952947,
				1289485, -12761191, 6540917,
				-4787304, -11454255, 7249439,
				-7239963, -10512732, 7776366,
				-8255332, -9999487, 8101770,
				-8632155, -9817625, 8305531,
				0x3, 0, 0,
				0, 0, 0,
				0, 0, 0,
				0, 0, 0,
				0, 0, 0,
				0, 0, 0,
				0, 0, 0,
				0, 0, 0,
				0, 0, 0,
				0, 0, 0,
				0, 0, 0,
				0, 0, 0,
				0, 0, 0,
				0, 0, 0,
				0, 0
};

static int coeff_ram_44_16[64] = {
				0x126104, /* IIR Filter + interpolation */
				2802,
				5762750, -14772125, 6628868,
				-9304342, -14504578, 7161825,
				-12409641, -14227678, 7732611,
				-13291674, -14077653, 8099947,
				-13563385, -14061743, 8309372,
				2,
				0x1d9204, /* Farrow Filter + Decimation */
				0x000aaaab,
				0xffaaaaab,
				0xfffaaaab,
				0x00555555,
				0xff600000,
				0xfff55555,
				0x00155555,
				0x00055555,
				0xffeaaaab,
				0x00200000,
				0x005105, /* IIR Filter + decimation */
				0x0000d649,
				0x00e87afb, 0xff5f69d0, 0x003df3cf,
				0x007ce488, 0xff99a5c8, 0x0056a6a0,
				0x00344928, 0xffcba3e5, 0x006be470,
				0x00137aa7, 0xffe60276, 0x00773410,
				0x0005fa2a, 0xfff1ac11, 0x007c795b,
				0x00012d36, 0xfff5eca2, 0x007f10ef,
				0x1,
				0, 0, 0, 0, 0,
				0, 0, 0, 0, 0,
				0, 0, 0, 0
};

static int coeff_ram_44_8[64] = {
				0x120104, /* IIR Filter */
				2802,
				5762750, -14772125, 6628868,
				-9304342, -14504578, 7161825,
				-12409641, -14227678, 7732611,
				-13291674, -14077653, 8099947,
				-13563385, -14061743, 8309372,
				1,
				0x1d9204, /* Farrwo Filter */
				0x000aaaab,
				0xffaaaaab,
				0xfffaaaab,
				0x00555555,
				0xff600000,
				0xfff55555,
				0x00155555,
				0x00055555,
				0xffeaaaab,
				0x00200000,
				0x005105, /* IIR Filter */
				0x0000d649,
				0x00e87afb, 0xff5f69d0, 0x003df3cf,
				0x007ce488, 0xff99a5c8, 0x0056a6a0,
				0x00344928, 0xffcba3e5, 0x006be470,
				0x00137aa7, 0xffe60276, 0x00773410,
				0x0005fa2a, 0xfff1ac11, 0x007c795b,
				0x00012d36, 0xfff5eca2, 0x007f10ef,
				0x1,
				0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0,
				0, 0
};

static int coeff_ram_48_16[64] = {
				0x009105, /* IIR FIlter + Decimation */
				1924,
				13390190, -13855175, 5952947,
				1289485, -12761191, 6540917,
				-4787304, -11454255, 7249439,
				-7239963, -10512732, 7776366,
				-8255332, -9999487, 8101770,
				-8632155, -9817625, 8305531,
				0x1,
				0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0
};

static int coeff_ram_48_8[64] = {
				0x0c9102,	/* IIR Filter + decimation */
				0x00000e00,
				0x00e2e000, 0xff6e1a00, 0x002aaa00,
				0x00610a00, 0xff5dda00, 0x003ccc00,
				0x00163a00, 0xff3c0400, 0x00633200,
				0x1,
				0x005105,   /* IIR Filter + Decimator */
				0x0000d649,
				0x00e87afb, 0xff5f69d0, 0x003df3cf,
				0x007ce488, 0xff99a5c8, 0x0056a6a0,
				0x00344928, 0xffcba3e5, 0x006be470,
				0x00137aa7, 0xffe60276, 0x00773410,
				0x0005fa2a, 0xfff1ac11, 0x007c795b,
				0x00012d36, 0xfff5eca2, 0x007f10ef,
				0x1, /* ouptut gain */
				0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0,
				0
};

static int coeff_ram_8_44[64] = {
				0x0156105, /* IIR filter +interpllation */
				0x0000d649,
				0x00e87afb, 0xff5f69d0, 0x003df3cf,
				0x007ce488, 0xff99a5c8, 0x0056a6a0,
				0x00344928, 0xffcba3e5, 0x006be470,
				0x00137aa7, 0xffe60276, 0x00773410,
				0x0005fa2a, 0xfff1ac11, 0x007c795b,
				0x00012d36, 0xfff5eca2, 0x007f10ef,
				0x2, /* ouptut gain */
				0x21a102, /* filter + interp0lation */
				0x00000e00,
				0x00e2e000, 0xff6e1a00, 0x002aaa00,
				0x00610a00, 0xff5dda00, 0x003ccc00,
				0x00163a00, 0xff3c0400, 0x00633200,
				0x3,
				0x000204,
				0x000aaaab,
				0xffaaaaab,
				0xfffaaaab,
				0x00555555,
				0xff600000,
				0xfff55555,
				0x00155555,
				0x00055555,
				0xffeaaaab,
				0x00200000,
				0, 0,
				0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0
};

static int coeff_ram_8_16[64] = {
				0x00006105, /* interpolation + IIR Filter */
				0x0000d649, /* input gain */
				0x00e87afb, 0xff5f69d0, 0x003df3cf,
				0x007ce488, 0xff99a5c8, 0x0056a6a0,
				0x00344928, 0xffcba3e5, 0x006be470,
				0x00137aa7, 0xffe60276, 0x00773410,
				0x0005fa2a, 0xfff1ac11, 0x007c795b,
				0x00012d36, 0xfff5eca2, 0x007f10ef,
				0x00000002, /* ouptut gain */
};

static int coeff_ram_16_8[64] = {
				0x00005105, /* IIR Filter + Decimator */
				0x0000d649, /* input gain */
				0x00e87afb, 0xff5f69d0, 0x003df3cf,
				0x007ce488, 0xff99a5c8, 0x0056a6a0,
				0x00344928, 0xffcba3e5, 0x006be470,
				0x00137aa7, 0xffe60276, 0x00773410,
				0x0005fa2a, 0xfff1ac11, 0x007c795b,
				0x00012d36, 0xfff5eca2, 0x007f10ef,
				0x00000001, /* ouptut gain */
};

static int tegra30_dam_runtime_suspend(struct device *dev)
{
	struct tegra30_dam *dam = dev_get_drvdata(dev);

	regcache_cache_only(dam->regmap, true);

	clk_disable_unprepare(dam->clk_dam);

	return 0;
}

static int tegra30_dam_runtime_resume(struct device *dev)
{
	struct tegra30_dam *dam = dev_get_drvdata(dev);
	int ret;

	ret = clk_prepare_enable(dam->clk_dam);
	if (ret) {
		dev_err(dev, "clk_enable failed: %d\n", ret);
		return ret;
	}

	regcache_cache_only(dam->regmap, false);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tegra30_dam_suspend(struct device *dev)
{
	struct tegra30_dam *dam = dev_get_drvdata(dev);

	regcache_mark_dirty(dam->regmap);

	return 0;
}
#endif

static int tegra30_dam_set_dai_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
	struct tegra30_dam *dam = snd_soc_dai_get_drvdata(dai);
	unsigned int value;

	switch (freq) {
	case 8000:
		value = TEGRA_DAM_FS8;
		break;
	case 11025:
		value = TEGRA_DAM_FS11;
		break;
	case 16000:
		value = TEGRA_DAM_FS16;
		break;
	case 22050:
		value = TEGRA_DAM_FS22;
		break;
	case 24000:
		value = TEGRA_DAM_FS24;
		break;
	case 32000:
		value = TEGRA_DAM_FS32;
		break;
	case 44100:
		value = TEGRA_DAM_FS44;
		break;
	case 48000:
		value = TEGRA_DAM_FS48;
		break;
	case 88200:
		value = TEGRA_DAM_FS88;
		break;
	case 96000:
		value = TEGRA_DAM_FS96;
		break;
	case 17640:
		value = TEGRA_DAM_FS176;
		break;
	case 19200:
		value = TEGRA_DAM_FS192;
		break;
	default:
		value = TEGRA_DAM_FS8;
		break;
	}

	if (dir == SND_SOC_CLOCK_OUT)
		dam->srate_out = value;
	else if (dir == SND_SOC_CLOCK_IN)
		dam->srate_in = value;

	return 0;
}

static int tegra30_dam_get_filter_stages(struct tegra30_dam *dam)
{
	int i;

	for (i = 0; i < 8; i++)
		if ((tegra30_dam_freq_lookup[i][0] == dam->srate_in) &&
			(tegra30_dam_freq_lookup[i][1] == dam->srate_out))
			return tegra30_dam_freq_lookup[i][2];

	return 0;
}

static int tegra30_dam_get_farrow_param(struct tegra30_dam *dam)
{
	int i;

	for (i = 0; i < 10; i++)
		if ((tegra30_dam_farrow_param_lookup[i][0] == dam->srate_in) &&
			(tegra30_dam_farrow_param_lookup[i][1] == dam->srate_out))
			return tegra30_dam_farrow_param_lookup[i][2];

	return 1;
}

static void tegra30_dam_write_coeff_ram(struct tegra30_dam *dam)
{
	unsigned int val;
	int i, *coeff_ram = NULL;

	regmap_write(dam->regmap,
		TEGRA_DAM_AUDIORAMCTL_DAM_CTRL,
		0x00002000);

	switch (dam->srate_in) {
	case TEGRA_DAM_FS8:
		if (dam->srate_out == TEGRA_DAM_FS48)
			coeff_ram = coeff_ram_8_48;
		else if (dam->srate_out == TEGRA_DAM_FS44)
			coeff_ram = coeff_ram_8_44;
		else if (dam->srate_out == TEGRA_DAM_FS16)
			coeff_ram = coeff_ram_8_16;
		break;

	case TEGRA_DAM_FS16:
		if (dam->srate_out == TEGRA_DAM_FS48)
			coeff_ram = coeff_ram_16_48;
		else if (dam->srate_out == TEGRA_DAM_FS44)
			coeff_ram = coeff_ram_16_44;
		else if (dam->srate_out == TEGRA_DAM_FS8)
			coeff_ram = coeff_ram_16_8;
		break;

	case TEGRA_DAM_FS44:
		if (dam->srate_out == TEGRA_DAM_FS8)
			coeff_ram = coeff_ram_44_8;
		else if (dam->srate_out == TEGRA_DAM_FS16)
			coeff_ram = coeff_ram_44_16;
		break;

	case TEGRA_DAM_FS48:
		if (dam->srate_out == TEGRA_DAM_FS8)
			coeff_ram = coeff_ram_48_8;
		else if (dam->srate_out == TEGRA_DAM_FS16)
			coeff_ram = coeff_ram_48_16;
		break;

	default:
		break;
	}

	regmap_write(dam->regmap,
		TEGRA_DAM_AUDIORAMCTL_DAM_CTRL,
		0x00005000);

	if (coeff_ram) {
		for (i = 0; i < 64; i++) {
			val = coeff_ram[i];
			regmap_write(dam->regmap,
				TEGRA_DAM_AUDIORAMCTL_DAM_DATA,
				val);
		}
		/* enable coeff ram */
		regmap_update_bits(dam->regmap, TEGRA_DAM_CH0_CTRL,
				TEGRA_DAM_CH0_CTRL_COEF_RAM_MASK,
				TEGRA_DAM_CH0_CTRL_COEF_RAM_EN);
	}
}

static int tegra30_dam_set_audio_cif(struct tegra30_dam *dam,
				struct snd_pcm_hw_params *params,
				unsigned int reg)
{
	int channels, audio_bits;
	struct tegra30_xbar_cif_conf cif_conf;

	channels = params_channels(params);
	if (channels < 2)
		return -EINVAL;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S8:
		audio_bits = TEGRA30_AUDIOCIF_BITS_8;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		audio_bits = TEGRA30_AUDIOCIF_BITS_16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		audio_bits = TEGRA30_AUDIOCIF_BITS_24;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		audio_bits = TEGRA30_AUDIOCIF_BITS_32;
		break;
	default:
		return -EINVAL;
	}

	cif_conf.threshold = 0;
	cif_conf.audio_channels = channels;
	cif_conf.client_channels = channels;
	cif_conf.audio_bits = audio_bits;

	/* client_bits set to 32 for DAM operation */
	cif_conf.client_bits = TEGRA30_AUDIOCIF_BITS_32;
	cif_conf.expand = 0;
	cif_conf.stereo_conv = 0;
	cif_conf.replicate = 0;
	cif_conf.direction = 0;
	cif_conf.truncate = 0;
	cif_conf.mono_conv = 0;

	dam->soc_data->set_audio_cif(dam->regmap, reg, &cif_conf);

	return 0;
}

static int tegra30_dam_set_in0_hw_params(struct tegra30_dam *dam)
{
	unsigned int filt_stages;
	int farrow_param;
	/* program coeff ram */
	tegra30_dam_write_coeff_ram(dam);

	if (dam->srate_in < 0) {
		pr_err("DAM input rate not set: %d\n",
			-EINVAL);
		return -EINVAL;
	}

	/* set input rate */
	regmap_update_bits(dam->regmap, TEGRA_DAM_CH0_CTRL,
		TEGRA_DAM_CH0_CTRL_FSIN_MASK,
		dam->srate_in << TEGRA_DAM_CH0_CTRL_FSIN_SHIFT);

	/* set IIR filter stages */
	filt_stages = tegra30_dam_get_filter_stages(dam);
	regmap_update_bits(dam->regmap, TEGRA_DAM_CH0_CTRL,
		TEGRA_DAM_CH0_CTRL_FILT_STAGES_MASK,
		filt_stages << TEGRA_DAM_CH0_CTRL_FILT_STAGES_SHIFT);

	/* set farrow param */
	farrow_param = tegra30_dam_get_farrow_param(dam);
	regmap_write(dam->regmap, TEGRA_DAM_FARROW_PARAM, farrow_param);

	return 0;
}

static int tegra30_dam_in0_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct tegra30_dam *dam = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	/* set rx cif */
	ret = tegra30_dam_set_audio_cif(dam, params,
				TEGRA_DAM_AUDIOCIF_CH0_CTRL);
	if (ret) {
		dev_err(dev, "Can't set DAM%d RX CIF: %d\n",
			dev->id, ret);
		return ret;
	}

	ret = tegra30_dam_set_in0_hw_params(dam);
	if (ret) {
		dev_err(dev, "Error in DAM%d in0_hw_params\n", dev->id);
		return -EINVAL;
	}

	return ret;
}

static int tegra30_dam_in1_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct tegra30_dam *dam = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	ret = tegra30_dam_set_audio_cif(dam, params,
				TEGRA_DAM_AUDIOCIF_CH1_CTRL);
	if (ret) {
		dev_err(dev, "Can't set DAM%d RX CIF: %d\n",
			dev->id, ret);
	}

	return ret;
}

static int tegra30_dam_set_out_hw_params(struct tegra30_dam *dam)
{
	if (dam->srate_out < 0) {
		pr_err("DAM output rate not set: %d\n",
			-EINVAL);
		return -EINVAL;
	}

	/* set output rate */
	regmap_update_bits(dam->regmap, TEGRA_DAM_CTRL,
		TEGRA_DAM_CTRL_FSOUT_MASK,
		dam->srate_out << TEGRA_DAM_CTRL_FSOUT_SHIFT);

	/* clear previous SRC/MIX settings if any */
	regmap_update_bits(dam->regmap, TEGRA_DAM_CTRL,
		TEGRA_DAM_CTRL_STEREO_MIX_EN_MASK, 0);
	regmap_update_bits(dam->regmap, TEGRA_DAM_CTRL,
		TEGRA_DAM_CTRL_STEREO_SRC_EN_MASK, 0);

	if (dam->srate_out == dam->srate_in)
		/* enable stereo MIX in bypass mode */
		regmap_update_bits(dam->regmap, TEGRA_DAM_CTRL,
			TEGRA_DAM_CTRL_STEREO_MIX_EN_MASK,
			TEGRA_DAM_CTRL_STEREO_MIX_EN);
	else
		/* enable stereo SRC */
		regmap_update_bits(dam->regmap, TEGRA_DAM_CTRL,
			TEGRA_DAM_CTRL_STEREO_SRC_EN_MASK,
			TEGRA_DAM_CTRL_STEREO_SRC_EN);

	return 0;

}
static int tegra30_dam_out_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct tegra30_dam *dam = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	/* set tx cif */
	ret = tegra30_dam_set_audio_cif(dam, params,
				TEGRA_DAM_AUDIOCIF_OUT_CTRL);
	if (ret) {
		dev_err(dev, "Can't set DAM%d TX CIF: %d\n",
			dev->id, ret);
		return ret;
	}

	ret = tegra30_dam_set_out_hw_params(dam);
	if (ret) {
		dev_err(dev, "error in DAM%d out_hw_params\n", dev->id);
		return -EINVAL;
	}

	return ret;
}

static int tegra30_dam_get_out_srate(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra30_dam *dam = snd_soc_codec_get_drvdata(codec);

	/* get the dam output rate */
	ucontrol->value.integer.value[0] = dam->srate_out + 1;

	return 0;
}

static int tegra30_dam_put_out_srate(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra30_dam *dam = snd_soc_codec_get_drvdata(codec);

	/* update the dam output rate */
	dam->srate_out = ucontrol->value.integer.value[0] - 1;

	return 0;
}

static int tegra30_dam_get_in_srate(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra30_dam *dam = snd_soc_codec_get_drvdata(codec);

	/* get the dam input rate */
	ucontrol->value.integer.value[0] = dam->srate_in + 1;

	return 0;
}

static int tegra30_dam_put_in_srate(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra30_dam *dam = snd_soc_codec_get_drvdata(codec);

	/* update the dam input rate */
	dam->srate_in = ucontrol->value.integer.value[0] - 1;

	return 0;
}

static int tegra124_virt_dam_get_out_srate(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return tegra30_dam_get_out_srate(kcontrol, ucontrol);
}

static int tegra124_virt_dam_put_out_srate(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra30_dam *dam = snd_soc_codec_get_drvdata(codec);
	int ret;

	/* update the dam output rate */
	dam->srate_out = ucontrol->value.integer.value[0] - 1;

	ret = tegra30_dam_set_out_hw_params(dam);
	if (ret) {
		pr_err("Error in set_out_hw_params\n");
		return -EINVAL;
	}

	return 0;
}

static int tegra124_virt_dam_get_in_srate(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return tegra30_dam_get_in_srate(kcontrol, ucontrol);
}

static int tegra124_virt_dam_put_in_srate(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra30_dam *dam = snd_soc_codec_get_drvdata(codec);
	int ret;

	/* update the dam input rate */
	dam->srate_in = ucontrol->value.integer.value[0] - 1;

	ret = tegra30_dam_set_in0_hw_params(dam);
	if (ret) {
			pr_err("Error in setting tegra30 in0_hw_params\n");
			return -EINVAL;
	}

	return 0;
}

static int tegra124_virt_dam_get_ch_gain(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra30_dam *dam = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = dam->in_ch0_gain;

	return 0;
}

static int tegra124_virt_dam_put_ch_gain(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra30_dam *dam = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	dam->in_ch0_gain = ucontrol->value.integer.value[0];

	regmap_write(dam->regmap, reg, dam->in_ch0_gain);

	return 0;
}

static int tegra30_dam_codec_probe(struct snd_soc_codec *codec)
{
	struct tegra30_dam *dam = snd_soc_codec_get_drvdata(codec);

	codec->control_data = dam->regmap;

	return 0;
}

static struct snd_soc_dai_ops tegra30_dam_out_dai_ops = {
	.hw_params	= tegra30_dam_out_hw_params,
};

static struct snd_soc_dai_ops tegra30_dam_in0_dai_ops = {
	.hw_params	= tegra30_dam_in0_hw_params,
	.set_sysclk = tegra30_dam_set_dai_sysclk,
};

static struct snd_soc_dai_ops tegra30_dam_in1_dai_ops = {
	.hw_params	= tegra30_dam_in1_hw_params,
};

#define IN_DAI(id, dai_ops)						\
	{							\
		.name = "IN" #id,				\
		.playback = {					\
			.stream_name = "IN" #id " Receive",	\
			.channels_min = 1,			\
			.channels_max = 2,			\
			.rates = SNDRV_PCM_RATE_8000_96000,	\
			.formats = SNDRV_PCM_FMTBIT_S8 |	\
				SNDRV_PCM_FMTBIT_S16_LE |	\
				SNDRV_PCM_FMTBIT_S24_LE |	\
				SNDRV_PCM_FMTBIT_S32_LE,	\
		},						\
		.ops = dai_ops,		\
	}

#define OUT_DAI(sname, dai_ops)					\
	{							\
		.name = #sname,					\
		.capture = {					\
			.stream_name = #sname " Transmit",	\
			.channels_min = 1,			\
			.channels_max = 2,			\
			.rates = SNDRV_PCM_RATE_8000_96000,	\
			.formats = SNDRV_PCM_FMTBIT_S8 |	\
				SNDRV_PCM_FMTBIT_S16_LE |	\
				SNDRV_PCM_FMTBIT_S24_LE |	\
				SNDRV_PCM_FMTBIT_S32_LE,	\
		},						\
		.ops = dai_ops,					\
	}

static struct snd_soc_dai_driver tegra30_dam_dais[] = {
	IN_DAI(0, &tegra30_dam_in0_dai_ops),
	IN_DAI(1, &tegra30_dam_in1_dai_ops),
	OUT_DAI(OUT, &tegra30_dam_out_dai_ops),
};

static const struct snd_soc_dapm_widget tegra30_dam_widgets[] = {
	SND_SOC_DAPM_AIF_IN("IN0", NULL, 0, TEGRA_DAM_CH0_CTRL, 0, 0),
	SND_SOC_DAPM_AIF_IN("IN1", NULL, 0, TEGRA_DAM_CH1_CTRL, 0, 0),
	SND_SOC_DAPM_AIF_OUT("OUT", NULL, 0, TEGRA_DAM_CTRL, 0, 0),
};

static const struct snd_soc_dapm_widget tegra124_virt_dam_widgets[] = {
	SND_SOC_DAPM_AIF_IN("IN0", NULL, 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("IN1", NULL, 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("OUT", NULL, 0, TEGRA_DAM_CTRL, 0, 0),
};

static const struct snd_soc_dapm_route tegra30_dam_routes[] = {
	{ "IN0",       NULL, "IN0 Receive" },
	{ "IN1",       NULL, "IN1 Receive" },
	{ "OUT",       NULL, "IN0" },
	{ "OUT",       NULL, "IN1" },
	{ "OUT Transmit", NULL, "OUT" },
};

static const char * const tegra30_dam_srate_text[] = {
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

static const struct soc_enum tegra30_dam_srate =
	SOC_ENUM_SINGLE_EXT(13, tegra30_dam_srate_text);

static const struct snd_kcontrol_new tegra30_dam_controls[] = {
	SOC_ENUM_EXT("output rate", tegra30_dam_srate,
		tegra30_dam_get_out_srate, tegra30_dam_put_out_srate),
	SOC_ENUM_EXT("input rate", tegra30_dam_srate,
		tegra30_dam_get_in_srate, tegra30_dam_put_in_srate),
};

static const struct snd_kcontrol_new tegra124_virt_dam_controls[] = {
	SOC_ENUM_EXT("output rate", tegra30_dam_srate,
		tegra124_virt_dam_get_out_srate,
		tegra124_virt_dam_put_out_srate),
	SOC_ENUM_EXT("input rate", tegra30_dam_srate,
		tegra124_virt_dam_get_in_srate,
		tegra124_virt_dam_put_in_srate),
	SOC_SINGLE_EXT("ch0 gain", TEGRA_DAM_CH_REG(0), 0, 0xFFFF, 0,
		tegra124_virt_dam_get_ch_gain, tegra124_virt_dam_put_ch_gain),
	SOC_SINGLE_EXT("ch1 gain", TEGRA_DAM_CH_REG(1), 0, 0xFFFF, 0,
		tegra124_virt_dam_get_ch_gain, tegra124_virt_dam_put_ch_gain),
};

static struct snd_soc_codec_driver tegra30_dam_codec = {
	.probe = tegra30_dam_codec_probe,
	.dapm_widgets = tegra30_dam_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tegra30_dam_widgets),
	.dapm_routes = tegra30_dam_routes,
	.num_dapm_routes = ARRAY_SIZE(tegra30_dam_routes),
	.controls = tegra30_dam_controls,
	.num_controls = ARRAY_SIZE(tegra30_dam_controls),
	.idle_bias_off = 1,
};

static bool tegra30_dam_wr_rd_reg(struct device *dev,
				unsigned int reg)
{
	switch (reg) {
	case TEGRA_DAM_CTRL:
	case TEGRA_DAM_CLIP:
	case TEGRA_DAM_CLIP_THRESHOLD:
	case TEGRA_DAM_AUDIOCIF_OUT_CTRL:
	case TEGRA_DAM_CH0_CTRL:
	case TEGRA_DAM_CH0_CONV:
	case TEGRA_DAM_AUDIOCIF_CH0_CTRL:
	case TEGRA_DAM_CH1_CTRL:
	case TEGRA_DAM_CH1_CONV:
	case TEGRA_DAM_AUDIOCIF_CH1_CTRL:
	case TEGRA_DAM_CH0_BIQUAD_FIXED_COEF:
	case TEGRA_DAM_FARROW_PARAM:
	case TEGRA_DAM_AUDIORAMCTL_DAM_CTRL:
	case TEGRA_DAM_AUDIORAMCTL_DAM_DATA:
		return true;
	default:
		return false;
	};
}

static bool tegra30_dam_volatile_reg(struct device *dev,
				unsigned int reg)
{
	switch (reg) {
	case TEGRA_DAM_CTRL:
	case TEGRA_DAM_AUDIORAMCTL_DAM_CTRL:
	case TEGRA_DAM_AUDIORAMCTL_DAM_DATA:
		return true;
	default:
		return false;
	};
}

static const struct regmap_config tegra30_dam_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = TEGRA_DAM_AUDIORAMCTL_DAM_DATA,
	.writeable_reg = tegra30_dam_wr_rd_reg,
	.readable_reg = tegra30_dam_wr_rd_reg,
	.volatile_reg = tegra30_dam_volatile_reg,
	.cache_type = REGCACHE_FLAT,
	.reg_defaults = tegra30_dam_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(tegra30_dam_reg_defaults),
};

static const struct tegra30_dam_soc_data soc_data_tegra30 = {
	.set_audio_cif = tegra30_xbar_set_cif
};

static const struct tegra30_dam_soc_data soc_data_tegra114 = {
	.set_audio_cif = tegra30_xbar_set_cif
};

static const struct tegra30_dam_soc_data soc_data_tegra124 = {
	.set_audio_cif = tegra124_xbar_set_cif
};

static const struct of_device_id tegra30_dam_of_match[] = {
	{ .compatible = "nvidia,tegra30-dam", .data = &soc_data_tegra30 },
	{ .compatible = "nvidia,tegra114-dam", .data = &soc_data_tegra114 },
	{ .compatible = "nvidia,tegra124-dam", .data = &soc_data_tegra124 },
	{ .compatible = "nvidia,tegra124-virt-dam",
					.data = &soc_data_tegra124 },
	{},
};

static int tegra30_dam_platform_probe(struct platform_device *pdev)
{
	struct tegra30_dam *dam;
	struct resource *mem, *memregion;
	void __iomem *regs;
	int ret;
	const struct of_device_id *match;
	struct tegra30_dam_soc_data *soc_data;

	match = of_match_device(tegra30_dam_of_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "Error: No device match found\n");
		ret = -ENODEV;
		goto err;
	}
	soc_data = (struct tegra30_dam_soc_data *)match->data;

	dam = devm_kzalloc(&pdev->dev, sizeof(struct tegra30_dam), GFP_KERNEL);
	if (!dam) {
		dev_err(&pdev->dev, "Can't allocate tegra30_dam\n");
		ret = -ENOMEM;
		goto err;
	}
	dev_set_drvdata(&pdev->dev, dam);

	dam->soc_data = soc_data;

	/* initialize default srate (in = 8khz; out = 48khz) */
	dam->srate_in = TEGRA_DAM_FS8;
	dam->srate_out = TEGRA_DAM_FS48;

	dam->clk_dam = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(dam->clk_dam)) {
		dev_err(&pdev->dev, "Can't retrieve tegra30_dam clock\n");
		ret = PTR_ERR(dam->clk_dam);
		goto err;
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "No memory resource\n");
		ret = -ENODEV;
		goto err_clk_put;
	}

	memregion = devm_request_mem_region(&pdev->dev, mem->start,
					    resource_size(mem), DRV_NAME);
	if (!memregion) {
		dev_err(&pdev->dev, "Memory region already claimed\n");
		ret = -EBUSY;
		goto err_clk_put;
	}

	regs = devm_ioremap(&pdev->dev, mem->start, resource_size(mem));
	if (!regs) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENOMEM;
		goto err_clk_put;
	}

	dam->regmap = devm_regmap_init_mmio(&pdev->dev, regs,
					    &tegra30_dam_regmap_config);
	if (IS_ERR(dam->regmap)) {
		dev_err(&pdev->dev, "regmap init failed\n");
		ret = PTR_ERR(dam->regmap);
		goto err_clk_put;
	}
	regcache_cache_only(dam->regmap, true);

	if (of_property_read_u32(pdev->dev.of_node,
		"nvidia,ahub-dam-id", &pdev->dev.id) < 0) {
		dev_err(&pdev->dev, "Missing property nvidia,ahub-dam-id\n");
		ret = -ENODEV;
		goto err_clk_put;
	}

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		ret = tegra30_dam_runtime_resume(&pdev->dev);
		if (ret)
			goto err_pm_disable;
	}

	/* Set default dam in ch gain value */
	dam->in_ch0_gain = dam->in_ch1_gain = TEGRA_DAM_DEFAULT_GAIN;

	if (of_device_is_compatible(pdev->dev.of_node,
					"nvidia,tegra124-virt-dam")) {
		tegra30_dam_codec.dapm_widgets =
				tegra124_virt_dam_widgets;
		tegra30_dam_codec.num_dapm_widgets =
				ARRAY_SIZE(tegra124_virt_dam_widgets);
		tegra30_dam_codec.controls =
				tegra124_virt_dam_controls;
		tegra30_dam_codec.num_controls =
				ARRAY_SIZE(tegra124_virt_dam_controls);
	}
	ret = snd_soc_register_codec(&pdev->dev, &tegra30_dam_codec,
				     tegra30_dam_dais,
				     ARRAY_SIZE(tegra30_dam_dais));
	if (ret != 0) {
		dev_err(&pdev->dev, "Could not register CODEC: %d\n", ret);
		goto err_suspend;
	}

	return 0;

err_suspend:
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra30_dam_runtime_suspend(&pdev->dev);
err_pm_disable:
	pm_runtime_disable(&pdev->dev);
err_clk_put:
	devm_clk_put(&pdev->dev, dam->clk_dam);
err:
	return ret;
}

static int tegra30_dam_platform_remove(struct platform_device *pdev)
{
	struct tegra30_dam *dam = dev_get_drvdata(&pdev->dev);

	snd_soc_unregister_codec(&pdev->dev);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra30_dam_runtime_suspend(&pdev->dev);

	devm_clk_put(&pdev->dev, dam->clk_dam);

	return 0;
}

static const struct dev_pm_ops tegra30_dam_pm_ops = {
	SET_RUNTIME_PM_OPS(tegra30_dam_runtime_suspend,
			   tegra30_dam_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(tegra30_dam_suspend,
			   NULL)
};

static struct platform_driver tegra30_dam_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = tegra30_dam_of_match,
		.pm = &tegra30_dam_pm_ops,
	},
	.probe = tegra30_dam_platform_probe,
	.remove = tegra30_dam_platform_remove,
};
module_platform_driver(tegra30_dam_driver);

MODULE_AUTHOR("Arun S L <aruns@nvidia.com>");
MODULE_DESCRIPTION("Tegra30 DAM ASoC driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, tegra30_dam_of_match);
