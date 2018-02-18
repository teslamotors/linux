/*
 * tegra210_mixer_alt.c - Tegra210 MIXER driver
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
#include <linux/of_device.h>

#include "tegra210_xbar_alt.h"
#include "tegra210_mixer_alt.h"

#define DRV_NAME "tegra210_mixer"

#define MIXER_RX_REG(reg, id) (reg + (id * TEGRA210_MIXER_AXBAR_RX_STRIDE))
#define MIXER_TX_REG(reg, id) (reg + (id * TEGRA210_MIXER_AXBAR_TX_STRIDE))

#define MIXER_GAIN_CONFIG_RAM_ADDR(id)	\
	(TEGRA210_MIXER_AHUBRAMCTL_GAIN_CONFIG_RAM_ADDR_0 +	\
		id*TEGRA210_MIXER_AHUBRAMCTL_GAIN_CONFIG_RAM_ADDR_STRIDE)

#define MIXER_RX_REG_DEFAULTS(id) \
	{ MIXER_RX_REG(TEGRA210_MIXER_AXBAR_RX1_CIF_CTRL, id), 0x00007700}, \
	{ MIXER_RX_REG(TEGRA210_MIXER_AXBAR_RX1_CTRL, id), 0x00010823}, \
	{ MIXER_RX_REG(TEGRA210_MIXER_AXBAR_RX1_PEAK_CTRL, id), 0x000012c0}

#define MIXER_TX_REG_DEFAULTS(id) \
	{ MIXER_TX_REG(TEGRA210_MIXER_AXBAR_TX1_INT_MASK, id), 0x00000001}, \
	{ MIXER_TX_REG(TEGRA210_MIXER_AXBAR_TX1_CIF_CTRL, id), 0x00007700}

static const struct reg_default tegra210_mixer_reg_defaults[] = {
	MIXER_RX_REG_DEFAULTS(0),
	MIXER_RX_REG_DEFAULTS(1),
	MIXER_RX_REG_DEFAULTS(2),
	MIXER_RX_REG_DEFAULTS(3),
	MIXER_RX_REG_DEFAULTS(4),
	MIXER_RX_REG_DEFAULTS(5),
	MIXER_RX_REG_DEFAULTS(6),
	MIXER_RX_REG_DEFAULTS(7),
	MIXER_RX_REG_DEFAULTS(8),
	MIXER_RX_REG_DEFAULTS(9),

	MIXER_TX_REG_DEFAULTS(0),
	MIXER_TX_REG_DEFAULTS(1),
	MIXER_TX_REG_DEFAULTS(2),
	MIXER_TX_REG_DEFAULTS(3),
	MIXER_TX_REG_DEFAULTS(4),

	{ TEGRA210_MIXER_CG, 0x00000001},
	{ TEGRA210_MIXER_AHUBRAMCTL_GAIN_CONFIG_RAM_CTRL, 0x00004000},
	{ TEGRA210_MIXER_AHUBRAMCTL_PEAKM_RAM_CTRL, 0x00004000},
};

static int tegra210_mixer_runtime_suspend(struct device *dev)
{
	struct tegra210_mixer *mixer = dev_get_drvdata(dev);

	regcache_cache_only(mixer->regmap, true);
	regcache_mark_dirty(mixer->regmap);

	pm_runtime_put_sync(dev->parent);

	return 0;
}

static int tegra210_mixer_runtime_resume(struct device *dev)
{
	struct tegra210_mixer *mixer = dev_get_drvdata(dev);
	int ret;

	ret = pm_runtime_get_sync(dev->parent);
	if (ret < 0) {
		dev_err(dev, "parent get_sync failed: %d\n", ret);
		return ret;
	}

	regcache_cache_only(mixer->regmap, false);
	regcache_sync(mixer->regmap);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tegra210_mixer_suspend(struct device *dev)
{
	return 0;
}
#endif

static int tegra210_mixer_write_ram(struct tegra210_mixer *mixer,
				unsigned int addr,
				unsigned int val)
{
	unsigned int reg, value, wait = 0xffff;

	/* check if busy */
	do {
		regmap_read(mixer->regmap,
				TEGRA210_MIXER_AHUBRAMCTL_GAIN_CONFIG_RAM_CTRL, &value);
		wait--;
		if (!wait)
			return -EINVAL;
	} while (value & 0x80000000);
	value = 0;

	reg = (addr << TEGRA210_MIXER_AHUBRAMCTL_GAIN_CONFIG_RAM_CTRL_RAM_ADDR_SHIFT) &
			TEGRA210_MIXER_AHUBRAMCTL_GAIN_CONFIG_RAM_CTRL_RAM_ADDR_MASK;
	reg |= TEGRA210_MIXER_AHUBRAMCTL_GAIN_CONFIG_RAM_CTRL_ADDR_INIT_EN;
	reg |= TEGRA210_MIXER_AHUBRAMCTL_GAIN_CONFIG_RAM_CTRL_RW_WRITE;
	reg |= TEGRA210_MIXER_AHUBRAMCTL_GAIN_CONFIG_RAM_CTRL_SEQ_ACCESS_EN;

	regmap_write(mixer->regmap,
		TEGRA210_MIXER_AHUBRAMCTL_GAIN_CONFIG_RAM_CTRL, reg);
	regmap_write(mixer->regmap,
		TEGRA210_MIXER_AHUBRAMCTL_GAIN_CONFIG_RAM_DATA, val);

	return 0;
}

static int tegra210_mixer_get_gain(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int tegra210_mixer_put_gain(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_mixer *mixer = snd_soc_codec_get_drvdata(codec);
	unsigned int reg = mc->reg;
	unsigned int ret, i;

	pm_runtime_get_sync(codec->dev);
	/* write default gain config poly coefficients */
	for (i = 0; i < 10; i++)
		tegra210_mixer_write_ram(mixer, reg + i, mixer->gain_coeff[i]);

	/* set duration parameter */
	if (strstr(kcontrol->id.name, "Instant")) {
		for (; i < 14; i++)
			tegra210_mixer_write_ram(mixer, reg + i, 1);
	} else {
		for (; i < 14; i++)
			tegra210_mixer_write_ram(mixer, reg + i,
				mixer->gain_coeff[i]);
	}

	/* write new gain and trigger config */
	ret = tegra210_mixer_write_ram(mixer, reg + 0x09,
				ucontrol->value.integer.value[0]);
	ret |= tegra210_mixer_write_ram(mixer, reg + 0x0f,
				ucontrol->value.integer.value[0]);
	pm_runtime_put(codec->dev);

	/* save gain */
	i = (reg - TEGRA210_MIXER_AHUBRAMCTL_GAIN_CONFIG_RAM_ADDR_0) /
		TEGRA210_MIXER_AHUBRAMCTL_GAIN_CONFIG_RAM_ADDR_STRIDE;
	mixer->gain_value[i] = ucontrol->value.integer.value[0];

	return ret;
}

static int tegra210_mixer_set_audio_cif(struct tegra210_mixer *mixer,
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

	cif_conf.audio_channels = channels;
	cif_conf.client_channels = channels;
	cif_conf.audio_bits = audio_bits;
	cif_conf.client_bits = audio_bits;

	mixer->soc_data->set_audio_cif(mixer->regmap, reg, &cif_conf);
	return 0;
}


static int tegra210_mixer_in_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct tegra210_mixer *mixer = snd_soc_dai_get_drvdata(dai);
	int ret, i;

	ret = tegra210_mixer_set_audio_cif(mixer, params,
				TEGRA210_MIXER_AXBAR_RX1_CIF_CTRL +
				(dai->id * TEGRA210_MIXER_AXBAR_RX_STRIDE));

	/* write the gain config poly coefficients */
	for (i = 0; i < 14; i++) {
		tegra210_mixer_write_ram(mixer,
			MIXER_GAIN_CONFIG_RAM_ADDR(dai->id) + i,
			mixer->gain_coeff[i]);
	}

	/* write saved gain */
	ret = tegra210_mixer_write_ram(mixer,
			MIXER_GAIN_CONFIG_RAM_ADDR(dai->id) + 0x09,
			mixer->gain_value[dai->id]);

	/* trigger the polynomial configuration */
	tegra210_mixer_write_ram(mixer,
		MIXER_GAIN_CONFIG_RAM_ADDR(dai->id) + 0xf,
		0x01);

	return ret;
}

static int tegra210_mixer_out_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct tegra210_mixer *mixer = snd_soc_dai_get_drvdata(dai);
	int ret;

	ret = tegra210_mixer_set_audio_cif(mixer, params,
				TEGRA210_MIXER_AXBAR_TX1_CIF_CTRL +
				((dai->id-10) * TEGRA210_MIXER_AXBAR_TX_STRIDE));

	return ret;
}

static int tegra210_mixer_codec_probe(struct snd_soc_codec *codec)
{
	struct tegra210_mixer *mixer = snd_soc_codec_get_drvdata(codec);

	codec->control_data = mixer->regmap;

	return 0;
}

static struct snd_soc_dai_ops tegra210_mixer_out_dai_ops = {
	.hw_params	= tegra210_mixer_out_hw_params,
};

static struct snd_soc_dai_ops tegra210_mixer_in_dai_ops = {
	.hw_params	= tegra210_mixer_in_hw_params,
};

#define IN_DAI(sname, id, dai_ops)						\
	{							\
		.name = #sname #id,					\
		.playback = {					\
			.stream_name = #sname #id " Receive",	\
			.channels_min = 1,			\
			.channels_max = 8,		\
			.rates = SNDRV_PCM_RATE_8000_192000,		\
			.formats = SNDRV_PCM_FMTBIT_S8 |		\
				SNDRV_PCM_FMTBIT_S16_LE |		\
				SNDRV_PCM_FMTBIT_S24_LE |		\
				SNDRV_PCM_FMTBIT_S32_LE,		\
		},						\
		.ops = dai_ops,		\
	}

#define OUT_DAI(sname, id, dai_ops)					\
	{							\
		.name = #sname #id,					\
		.capture = {					\
			.stream_name = #sname #id " Transmit",	\
			.channels_min = 1,			\
			.channels_max = 8,			\
			.rates = SNDRV_PCM_RATE_8000_192000,		\
			.formats = SNDRV_PCM_FMTBIT_S8 |		\
				SNDRV_PCM_FMTBIT_S16_LE |		\
				SNDRV_PCM_FMTBIT_S24_LE |		\
				SNDRV_PCM_FMTBIT_S32_LE,		\
		},						\
		.ops = dai_ops,		\
	}

static struct snd_soc_dai_driver tegra210_mixer_dais[] = {
	IN_DAI(RX, 1, &tegra210_mixer_in_dai_ops),
	IN_DAI(RX, 2, &tegra210_mixer_in_dai_ops),
	IN_DAI(RX, 3, &tegra210_mixer_in_dai_ops),
	IN_DAI(RX, 4, &tegra210_mixer_in_dai_ops),
	IN_DAI(RX, 5, &tegra210_mixer_in_dai_ops),
	IN_DAI(RX, 6, &tegra210_mixer_in_dai_ops),
	IN_DAI(RX, 7, &tegra210_mixer_in_dai_ops),
	IN_DAI(RX, 8, &tegra210_mixer_in_dai_ops),
	IN_DAI(RX, 9, &tegra210_mixer_in_dai_ops),
	IN_DAI(RX, 10, &tegra210_mixer_in_dai_ops),
	OUT_DAI(TX, 1, &tegra210_mixer_out_dai_ops),
	OUT_DAI(TX, 2, &tegra210_mixer_out_dai_ops),
	OUT_DAI(TX, 3, &tegra210_mixer_out_dai_ops),
	OUT_DAI(TX, 4, &tegra210_mixer_out_dai_ops),
	OUT_DAI(TX, 5, &tegra210_mixer_out_dai_ops),
};

#define ADDER_CTRL_DECL(name, reg)	\
	static const struct snd_kcontrol_new name[] = {	\
		SOC_DAPM_SINGLE("RX1", reg, 0, 1, 0),	\
		SOC_DAPM_SINGLE("RX2", reg, 1, 1, 0),	\
		SOC_DAPM_SINGLE("RX3", reg, 2, 1, 0),	\
		SOC_DAPM_SINGLE("RX4", reg, 3, 1, 0),	\
		SOC_DAPM_SINGLE("RX5", reg, 4, 1, 0),	\
		SOC_DAPM_SINGLE("RX6", reg, 5, 1, 0),	\
		SOC_DAPM_SINGLE("RX7", reg, 6, 1, 0),	\
		SOC_DAPM_SINGLE("RX8", reg, 7, 1, 0),	\
		SOC_DAPM_SINGLE("RX9", reg, 8, 1, 0),	\
		SOC_DAPM_SINGLE("RX10", reg, 9, 1, 0),	\
	}

ADDER_CTRL_DECL(adder1, TEGRA210_MIXER_AXBAR_TX1_ADDER_CONFIG);
ADDER_CTRL_DECL(adder2, TEGRA210_MIXER_AXBAR_TX2_ADDER_CONFIG);
ADDER_CTRL_DECL(adder3, TEGRA210_MIXER_AXBAR_TX3_ADDER_CONFIG);
ADDER_CTRL_DECL(adder4, TEGRA210_MIXER_AXBAR_TX4_ADDER_CONFIG);
ADDER_CTRL_DECL(adder5, TEGRA210_MIXER_AXBAR_TX5_ADDER_CONFIG);

static const struct snd_kcontrol_new tegra210_mixer_gain_ctls[] = {	\
	SOC_SINGLE_EXT("RX1 Gain", MIXER_GAIN_CONFIG_RAM_ADDR(0), 0, 0x20000, 0,
		tegra210_mixer_get_gain, tegra210_mixer_put_gain),
	SOC_SINGLE_EXT("RX2 Gain", MIXER_GAIN_CONFIG_RAM_ADDR(1), 0, 0x20000, 0,
		tegra210_mixer_get_gain, tegra210_mixer_put_gain),
	SOC_SINGLE_EXT("RX3 Gain", MIXER_GAIN_CONFIG_RAM_ADDR(2), 0, 0x20000, 0,
		tegra210_mixer_get_gain, tegra210_mixer_put_gain),
	SOC_SINGLE_EXT("RX4 Gain", MIXER_GAIN_CONFIG_RAM_ADDR(3), 0, 0x20000, 0,
		tegra210_mixer_get_gain, tegra210_mixer_put_gain),
	SOC_SINGLE_EXT("RX5 Gain", MIXER_GAIN_CONFIG_RAM_ADDR(4), 0, 0x20000, 0,
		tegra210_mixer_get_gain, tegra210_mixer_put_gain),
	SOC_SINGLE_EXT("RX6 Gain", MIXER_GAIN_CONFIG_RAM_ADDR(5), 0, 0x20000, 0,
		tegra210_mixer_get_gain, tegra210_mixer_put_gain),
	SOC_SINGLE_EXT("RX7 Gain", MIXER_GAIN_CONFIG_RAM_ADDR(6), 0, 0x20000, 0,
		tegra210_mixer_get_gain, tegra210_mixer_put_gain),
	SOC_SINGLE_EXT("RX8 Gain", MIXER_GAIN_CONFIG_RAM_ADDR(7), 0, 0x20000, 0,
		tegra210_mixer_get_gain, tegra210_mixer_put_gain),
	SOC_SINGLE_EXT("RX9 Gain", MIXER_GAIN_CONFIG_RAM_ADDR(8), 0, 0x20000, 0,
		tegra210_mixer_get_gain, tegra210_mixer_put_gain),
	SOC_SINGLE_EXT("RX10 Gain", MIXER_GAIN_CONFIG_RAM_ADDR(9), 0, 0x20000, 0,
		tegra210_mixer_get_gain, tegra210_mixer_put_gain),
	SOC_SINGLE_EXT("RX1 Gain Instant", MIXER_GAIN_CONFIG_RAM_ADDR(0), 0,
		0x20000, 0, tegra210_mixer_get_gain, tegra210_mixer_put_gain),
	SOC_SINGLE_EXT("RX2 Gain Instant", MIXER_GAIN_CONFIG_RAM_ADDR(1), 0,
		0x20000, 0, tegra210_mixer_get_gain, tegra210_mixer_put_gain),
	SOC_SINGLE_EXT("RX3 Gain Instant", MIXER_GAIN_CONFIG_RAM_ADDR(2), 0,
		0x20000, 0, tegra210_mixer_get_gain, tegra210_mixer_put_gain),
	SOC_SINGLE_EXT("RX4 Gain Instant", MIXER_GAIN_CONFIG_RAM_ADDR(3), 0,
		0x20000, 0, tegra210_mixer_get_gain, tegra210_mixer_put_gain),
	SOC_SINGLE_EXT("RX5 Gain Instant", MIXER_GAIN_CONFIG_RAM_ADDR(4), 0,
		0x20000, 0, tegra210_mixer_get_gain, tegra210_mixer_put_gain),
	SOC_SINGLE_EXT("RX6 Gain Instant", MIXER_GAIN_CONFIG_RAM_ADDR(5), 0,
		0x20000, 0, tegra210_mixer_get_gain, tegra210_mixer_put_gain),
	SOC_SINGLE_EXT("RX7 Gain Instant", MIXER_GAIN_CONFIG_RAM_ADDR(6), 0,
		0x20000, 0, tegra210_mixer_get_gain, tegra210_mixer_put_gain),
	SOC_SINGLE_EXT("RX8 Gain Instant", MIXER_GAIN_CONFIG_RAM_ADDR(7), 0,
		0x20000, 0, tegra210_mixer_get_gain, tegra210_mixer_put_gain),
	SOC_SINGLE_EXT("RX9 Gain Instant", MIXER_GAIN_CONFIG_RAM_ADDR(8), 0,
		0x20000, 0, tegra210_mixer_get_gain, tegra210_mixer_put_gain),
	SOC_SINGLE_EXT("RX10 Gain Instant", MIXER_GAIN_CONFIG_RAM_ADDR(9), 0,
		0x20000, 0, tegra210_mixer_get_gain, tegra210_mixer_put_gain),
	SOC_SINGLE("Mixer Enable", TEGRA210_MIXER_ENABLE, 0, 1, 0),
};

static const struct snd_soc_dapm_widget tegra210_mixer_widgets[] = {
	SND_SOC_DAPM_AIF_IN("RX1", NULL, 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("RX2", NULL, 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("RX3", NULL, 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("RX4", NULL, 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("RX5", NULL, 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("RX6", NULL, 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("RX7", NULL, 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("RX8", NULL, 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("RX9", NULL, 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("RX10", NULL, 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("TX1", NULL, 0,
		TEGRA210_MIXER_AXBAR_TX1_ENABLE, 0, 0),
	SND_SOC_DAPM_AIF_OUT("TX2", NULL, 0,
		TEGRA210_MIXER_AXBAR_TX2_ENABLE, 0, 0),
	SND_SOC_DAPM_AIF_OUT("TX3", NULL, 0,
		TEGRA210_MIXER_AXBAR_TX3_ENABLE, 0, 0),
	SND_SOC_DAPM_AIF_OUT("TX4", NULL, 0,
		TEGRA210_MIXER_AXBAR_TX4_ENABLE, 0, 0),
	SND_SOC_DAPM_AIF_OUT("TX5", NULL, 0,
		TEGRA210_MIXER_AXBAR_TX5_ENABLE, 0, 0),
	SND_SOC_DAPM_MIXER("Adder1", SND_SOC_NOPM, 1, 0,
		adder1, ARRAY_SIZE(adder1)),
	SND_SOC_DAPM_MIXER("Adder2", SND_SOC_NOPM, 1, 0,
		adder2, ARRAY_SIZE(adder2)),
	SND_SOC_DAPM_MIXER("Adder3", SND_SOC_NOPM, 1, 0,
		adder3, ARRAY_SIZE(adder3)),
	SND_SOC_DAPM_MIXER("Adder4", SND_SOC_NOPM, 1, 0,
		adder4, ARRAY_SIZE(adder4)),
	SND_SOC_DAPM_MIXER("Adder5", SND_SOC_NOPM, 1, 0,
		adder5, ARRAY_SIZE(adder5)),
};

#define MIXER_ROUTES(name, id)	\
	{name,	"RX1",	"RX1",},	\
	{name,	"RX2",	"RX2",},	\
	{name,	"RX3",	"RX3",},	\
	{name,	"RX4",	"RX4",},	\
	{name,	"RX5",	"RX5",},	\
	{name,	"RX6",	"RX6",},	\
	{name,	"RX7",	"RX7",},	\
	{name,	"RX8",	"RX8",},	\
	{name,	"RX9",	"RX9",},	\
	{name,	"RX10",	"RX10"},	\
	{"TX"#id,	NULL,	name}

static const struct snd_soc_dapm_route tegra210_mixer_routes[] = {
	{ "RX1",	NULL,	"RX1 Receive" },
	{ "RX2",	NULL,	"RX2 Receive" },
	{ "RX3",	NULL,	"RX3 Receive" },
	{ "RX4",	NULL,	"RX4 Receive" },
	{ "RX5",	NULL,	"RX5 Receive" },
	{ "RX6",	NULL,	"RX6 Receive" },
	{ "RX7",	NULL,	"RX7 Receive" },
	{ "RX8",	NULL,	"RX8 Receive" },
	{ "RX9",	NULL,	"RX9 Receive" },
	{ "RX10",	NULL,	"RX10 Receive" },
	/* route between MIXER RXs and TXs */
	MIXER_ROUTES("Adder1", 1),
	MIXER_ROUTES("Adder2", 2),
	MIXER_ROUTES("Adder3", 3),
	MIXER_ROUTES("Adder4", 4),
	MIXER_ROUTES("Adder5", 5),
	{ "TX1 Transmit",	NULL,	"TX1" },
	{ "TX2 Transmit",	NULL,	"TX2" },
	{ "TX3 Transmit",	NULL,	"TX3" },
	{ "TX4 Transmit",	NULL,	"TX4" },
	{ "TX5 Transmit",	NULL,	"TX5" },
};

static struct snd_soc_codec_driver tegra210_mixer_codec = {
	.probe = tegra210_mixer_codec_probe,
	.dapm_widgets = tegra210_mixer_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tegra210_mixer_widgets),
	.dapm_routes = tegra210_mixer_routes,
	.num_dapm_routes = ARRAY_SIZE(tegra210_mixer_routes),
	.controls = tegra210_mixer_gain_ctls,
	.num_controls = ARRAY_SIZE(tegra210_mixer_gain_ctls),
	.idle_bias_off = 1,
};

static bool tegra210_mixer_wr_reg(struct device *dev,
				unsigned int reg)
{
	if (reg < TEGRA210_MIXER_AXBAR_RX_LIMIT)
		reg %= TEGRA210_MIXER_AXBAR_RX_STRIDE;
	else if (reg < TEGRA210_MIXER_AXBAR_TX_LIMIT)
		reg = (reg % TEGRA210_MIXER_AXBAR_TX_STRIDE) +
			TEGRA210_MIXER_AXBAR_TX1_ENABLE;

	switch (reg) {
	case TEGRA210_MIXER_AXBAR_RX1_SOFT_RESET:
	case TEGRA210_MIXER_AXBAR_RX1_STATUS:
	case TEGRA210_MIXER_AXBAR_RX1_CIF_CTRL:
	case TEGRA210_MIXER_AXBAR_RX1_CTRL:
	case TEGRA210_MIXER_AXBAR_RX1_PEAK_CTRL:

	case TEGRA210_MIXER_AXBAR_TX1_ENABLE:
	case TEGRA210_MIXER_AXBAR_TX1_SOFT_RESET:
	case TEGRA210_MIXER_AXBAR_TX1_INT_MASK:
	case TEGRA210_MIXER_AXBAR_TX1_INT_SET:
	case TEGRA210_MIXER_AXBAR_TX1_INT_CLEAR:
	case TEGRA210_MIXER_AXBAR_TX1_CIF_CTRL:
	case TEGRA210_MIXER_AXBAR_TX1_ADDER_CONFIG:

	case TEGRA210_MIXER_ENABLE:
	case TEGRA210_MIXER_SOFT_RESET:
	case TEGRA210_MIXER_CG:
	case TEGRA210_MIXER_AHUBRAMCTL_GAIN_CONFIG_RAM_CTRL:
	case TEGRA210_MIXER_AHUBRAMCTL_GAIN_CONFIG_RAM_DATA:
	case TEGRA210_MIXER_AHUBRAMCTL_PEAKM_RAM_CTRL:
	case TEGRA210_MIXER_AHUBRAMCTL_PEAKM_RAM_DATA:
	case TEGRA210_MIXER_CTRL:
		return true;
	default:
		return false;
	};
}

static bool tegra210_mixer_rd_reg(struct device *dev,
				unsigned int reg)
{
	if (reg < TEGRA210_MIXER_AXBAR_RX_LIMIT)
		reg %= TEGRA210_MIXER_AXBAR_RX_STRIDE;
	else if (reg < TEGRA210_MIXER_AXBAR_TX_LIMIT)
		reg = (reg % TEGRA210_MIXER_AXBAR_TX_STRIDE) +
			TEGRA210_MIXER_AXBAR_TX1_ENABLE;

	switch (reg) {
	case TEGRA210_MIXER_AXBAR_RX1_SOFT_RESET:
	case TEGRA210_MIXER_AXBAR_RX1_STATUS:
	case TEGRA210_MIXER_AXBAR_RX1_CIF_CTRL:
	case TEGRA210_MIXER_AXBAR_RX1_CTRL:
	case TEGRA210_MIXER_AXBAR_RX1_PEAK_CTRL:
	case TEGRA210_MIXER_AXBAR_RX1_SAMPLE_COUNT:

	case TEGRA210_MIXER_AXBAR_TX1_ENABLE:
	case TEGRA210_MIXER_AXBAR_TX1_SOFT_RESET:
	case TEGRA210_MIXER_AXBAR_TX1_STATUS:
	case TEGRA210_MIXER_AXBAR_TX1_INT_STATUS:
	case TEGRA210_MIXER_AXBAR_TX1_INT_MASK:
	case TEGRA210_MIXER_AXBAR_TX1_INT_SET:
	case TEGRA210_MIXER_AXBAR_TX1_INT_CLEAR:
	case TEGRA210_MIXER_AXBAR_TX1_CIF_CTRL:
	case TEGRA210_MIXER_AXBAR_TX1_ADDER_CONFIG:

	case TEGRA210_MIXER_ENABLE:
	case TEGRA210_MIXER_SOFT_RESET:
	case TEGRA210_MIXER_CG:
	case TEGRA210_MIXER_STATUS:
	case TEGRA210_MIXER_INT_STATUS:
	case TEGRA210_MIXER_AHUBRAMCTL_GAIN_CONFIG_RAM_CTRL:
	case TEGRA210_MIXER_AHUBRAMCTL_GAIN_CONFIG_RAM_DATA:
	case TEGRA210_MIXER_AHUBRAMCTL_PEAKM_RAM_CTRL:
	case TEGRA210_MIXER_AHUBRAMCTL_PEAKM_RAM_DATA:
	case TEGRA210_MIXER_CTRL:
		return true;
	default:
		return false;
	};
}

static bool tegra210_mixer_volatile_reg(struct device *dev,
				unsigned int reg)
{
	if (reg < TEGRA210_MIXER_AXBAR_RX_LIMIT)
		reg %= TEGRA210_MIXER_AXBAR_RX_STRIDE;
	else if (reg < TEGRA210_MIXER_AXBAR_TX_LIMIT)
		reg = (reg % TEGRA210_MIXER_AXBAR_TX_STRIDE) +
			TEGRA210_MIXER_AXBAR_TX1_ENABLE;

	switch (reg) {
	case TEGRA210_MIXER_AXBAR_RX1_SOFT_RESET:
	case TEGRA210_MIXER_AXBAR_RX1_STATUS:

	case TEGRA210_MIXER_AXBAR_TX1_SOFT_RESET:
	case TEGRA210_MIXER_AXBAR_TX1_STATUS:
	case TEGRA210_MIXER_AXBAR_TX1_INT_STATUS:
	case TEGRA210_MIXER_AXBAR_TX1_INT_SET:

	case TEGRA210_MIXER_SOFT_RESET:
	case TEGRA210_MIXER_STATUS:
	case TEGRA210_MIXER_INT_STATUS:
	case TEGRA210_MIXER_AHUBRAMCTL_GAIN_CONFIG_RAM_CTRL:
	case TEGRA210_MIXER_AHUBRAMCTL_GAIN_CONFIG_RAM_DATA:
	case TEGRA210_MIXER_AHUBRAMCTL_PEAKM_RAM_CTRL:
	case TEGRA210_MIXER_AHUBRAMCTL_PEAKM_RAM_DATA:
		return true;
	default:
		return false;
	};
}

static bool tegra210_mixer_precious_reg(struct device *dev,
				unsigned int reg)
{
	switch (reg) {
	case TEGRA210_MIXER_AHUBRAMCTL_GAIN_CONFIG_RAM_DATA:
	case TEGRA210_MIXER_AHUBRAMCTL_PEAKM_RAM_DATA:
		return true;
	default:
		return false;
	};
}

static const struct regmap_config tegra210_mixer_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = TEGRA210_MIXER_CTRL,
	.writeable_reg = tegra210_mixer_wr_reg,
	.readable_reg = tegra210_mixer_rd_reg,
	.volatile_reg = tegra210_mixer_volatile_reg,
	.precious_reg = tegra210_mixer_precious_reg,
	.reg_defaults = tegra210_mixer_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(tegra210_mixer_reg_defaults),
	.cache_type = REGCACHE_FLAT,
};

static const struct tegra210_mixer_soc_data soc_data_tegra210 = {
	.set_audio_cif = tegra210_xbar_set_cif
};

static const struct of_device_id tegra210_mixer_of_match[] = {
	{ .compatible = "nvidia,tegra210-amixer", .data = &soc_data_tegra210 },
	{},
};

static int tegra210_mixer_platform_probe(struct platform_device *pdev)
{
	struct tegra210_mixer *mixer;
	struct resource *mem, *memregion;
	void __iomem *regs;
	int ret, i;
	const struct of_device_id *match;
	struct tegra210_mixer_soc_data *soc_data;

	match = of_match_device(tegra210_mixer_of_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "Error: No device match found\n");
		ret = -ENODEV;
		goto err;
	}
	soc_data = (struct tegra210_mixer_soc_data *)match->data;

	mixer = devm_kzalloc(&pdev->dev,
		sizeof(struct tegra210_mixer), GFP_KERNEL);
	if (!mixer) {
		dev_err(&pdev->dev, "Can't allocate tegra210_mixer\n");
		ret = -ENOMEM;
		goto err;
	}

	mixer->soc_data = soc_data;
	mixer->gain_coeff[0] = 0;
	mixer->gain_coeff[1] = 0;
	mixer->gain_coeff[2] = 0;
	mixer->gain_coeff[3] = 0;
	mixer->gain_coeff[4] = 0;
	mixer->gain_coeff[5] = 0;
	mixer->gain_coeff[6] = 0;
	mixer->gain_coeff[7] = 0x1000000;
	mixer->gain_coeff[8] = 0;
	mixer->gain_coeff[9] = 0x10000;
	mixer->gain_coeff[10] = 0;
	mixer->gain_coeff[11] = 0;
	mixer->gain_coeff[12] = 0x400;
	mixer->gain_coeff[13] = 0x8000000;

	for (i = 0; i < TEGRA210_MIXER_AXBAR_RX_MAX; i++)
		mixer->gain_value[i] = 0x10000;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "No memory resource\n");
		ret = -ENODEV;
		goto err;
	}

	memregion = devm_request_mem_region(&pdev->dev, mem->start,
					    resource_size(mem), DRV_NAME);
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

	mixer->regmap = devm_regmap_init_mmio(&pdev->dev, regs,
					    &tegra210_mixer_regmap_config);
	if (IS_ERR(mixer->regmap)) {
		dev_err(&pdev->dev, "regmap init failed\n");
		ret = PTR_ERR(mixer->regmap);
		goto err;
	}
	regcache_cache_only(mixer->regmap, true);

	if (of_property_read_u32(pdev->dev.of_node,
				"nvidia,ahub-amixer-id",
				&pdev->dev.id) < 0) {
		dev_err(&pdev->dev,
			"Missing property nvidia,ahub-amixer-id\n");
		ret = -ENODEV;
		goto err;
	}

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		ret = tegra210_mixer_runtime_resume(&pdev->dev);
		if (ret)
			goto err_pm_disable;
	}

	ret = snd_soc_register_codec(&pdev->dev, &tegra210_mixer_codec,
				     tegra210_mixer_dais,
				     ARRAY_SIZE(tegra210_mixer_dais));
	if (ret != 0) {
		dev_err(&pdev->dev, "Could not register CODEC: %d\n", ret);
		goto err_suspend;
	}

	dev_set_drvdata(&pdev->dev, mixer);

	return 0;

err_suspend:
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra210_mixer_runtime_suspend(&pdev->dev);
err_pm_disable:
	pm_runtime_disable(&pdev->dev);
err:
	return ret;
}

static int tegra210_mixer_platform_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra210_mixer_runtime_suspend(&pdev->dev);

	return 0;
}

static const struct dev_pm_ops tegra210_mixer_pm_ops = {
	SET_RUNTIME_PM_OPS(tegra210_mixer_runtime_suspend,
			   tegra210_mixer_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(tegra210_mixer_suspend, NULL)
};

static struct platform_driver tegra210_mixer_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = tegra210_mixer_of_match,
		.pm = &tegra210_mixer_pm_ops,
	},
	.probe = tegra210_mixer_platform_probe,
	.remove = tegra210_mixer_platform_remove,
};
module_platform_driver(tegra210_mixer_driver);

MODULE_AUTHOR("Arun Shamanna Lakshmi <aruns@nvidia.com>");
MODULE_DESCRIPTION("Tegra210 MIXER ASoC driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, tegra210_mixer_of_match);
