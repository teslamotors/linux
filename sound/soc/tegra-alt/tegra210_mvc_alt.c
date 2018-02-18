/*
 * tegra210_mvc_alt.c - Tegra210 MVC driver
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
#include "tegra210_mvc_alt.h"

#define DRV_NAME "tegra210-mvc"

static const struct reg_default tegra210_mvc_reg_defaults[] = {
	{ TEGRA210_MVC_AXBAR_RX_INT_MASK, 0x00000001},
	{ TEGRA210_MVC_AXBAR_RX_CIF_CTRL, 0x00007700},
	{ TEGRA210_MVC_AXBAR_TX_INT_MASK, 0x00000001},
	{ TEGRA210_MVC_AXBAR_TX_CIF_CTRL, 0x00007700},
	{ TEGRA210_MVC_CG, 0x1},
	{ TEGRA210_MVC_CTRL, 0x40000001},
	{ TEGRA210_MVC_INIT_VOL, 0x00800000},
	{ TEGRA210_MVC_TARGET_VOL, 0x00800000},
	{ TEGRA210_MVC_DURATION, 0x000012c0},
	{ TEGRA210_MVC_DURATION_INV, 0x0006d3a0},
	{ TEGRA210_MVC_POLY_N1, 0x0000007d},
	{ TEGRA210_MVC_POLY_N2, 0x00000271},
	{ TEGRA210_MVC_PEAK_CTRL, 0x000012c0},
	{ TEGRA210_MVC_AHUBRAMCTL_CONFIG_RAM_CTRL, 0x00004000},
};

static int tegra210_mvc_runtime_suspend(struct device *dev)
{
	struct tegra210_mvc *mvc = dev_get_drvdata(dev);

	regcache_cache_only(mvc->regmap, true);

	pm_runtime_put_sync(dev->parent);

	return 0;
}

static int tegra210_mvc_runtime_resume(struct device *dev)
{
	struct tegra210_mvc *mvc = dev_get_drvdata(dev);
	int ret;

	ret = pm_runtime_get_sync(dev->parent);
	if (ret < 0) {
		dev_err(dev, "parent get_sync failed: %d\n", ret);
		return ret;
	}

	regcache_cache_only(mvc->regmap, false);
	regcache_sync(mvc->regmap);

	regmap_update_bits(mvc->regmap, TEGRA210_MVC_CTRL,
		TEGRA210_MVC_CURVE_TYPE_MASK,
		mvc->curve_type << TEGRA210_MVC_CURVE_TYPE_SHIFT);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tegra210_mvc_suspend(struct device *dev)
{
	struct tegra210_mvc *mvc = dev_get_drvdata(dev);

	if (mvc)
		regcache_mark_dirty(mvc->regmap);

	return 0;
}
#endif

static int tegra210_mvc_write_ram(struct tegra210_mvc *mvc,
				unsigned int addr,
				unsigned int val)
{
	unsigned int reg, value, wait = 0xffff;

	/* check if busy */
	do {
		regmap_read(mvc->regmap,
			TEGRA210_MVC_AHUBRAMCTL_CONFIG_RAM_CTRL,
			&value);
		wait--;
		if (!wait)
			return -EINVAL;
	} while (value & 0x80000000);
	value = 0;

	reg = (addr << TEGRA210_MVC_AHUBRAMCTL_CONFIG_RAM_CTRL_RAM_ADDR_SHIFT) &
			TEGRA210_MVC_AHUBRAMCTL_CONFIG_RAM_CTRL_RAM_ADDR_MASK;
	reg |= TEGRA210_MVC_AHUBRAMCTL_CONFIG_RAM_CTRL_ADDR_INIT_EN;
	reg |= TEGRA210_MVC_AHUBRAMCTL_CONFIG_RAM_CTRL_RW_WRITE;
	reg |= TEGRA210_MVC_AHUBRAMCTL_CONFIG_RAM_CTRL_SEQ_ACCESS_EN;

	regmap_write(mvc->regmap,
		TEGRA210_MVC_AHUBRAMCTL_CONFIG_RAM_CTRL, reg);
	regmap_write(mvc->regmap,
		TEGRA210_MVC_AHUBRAMCTL_CONFIG_RAM_DATA, val);

	return 0;
}

static int tegra210_mvc_get_vol(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_mvc *mvc = snd_soc_codec_get_drvdata(codec);
	unsigned int reg = mc->reg;

	pm_runtime_get_sync(codec->dev);
	if (reg == TEGRA210_MVC_TARGET_VOL) {
		s32 val;

		regmap_read(mvc->regmap, reg, &val);
		if (mvc->curve_type == CURVE_POLY)
			val >>= 24;
		else {
			val >>= 8;
			val += 120;
		}
		ucontrol->value.integer.value[0] = val;
	} else {
		u32 val;

		regmap_read(mvc->regmap, reg, &val);
		ucontrol->value.integer.value[0] =
			((val & TEGRA210_MVC_MUTE_MASK) != 0);
	}
	pm_runtime_put(codec->dev);
	return 0;
}

static int tegra210_mvc_put_vol(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_mvc *mvc = snd_soc_codec_get_drvdata(codec);
	unsigned int reg = mc->reg;
	unsigned int value, wait = 0xffff;
	int ret = 0;
	s32 val;

	pm_runtime_get_sync(codec->dev);
	/* check if VOLUME_SWITCH is triggered*/
	do {
		regmap_read(mvc->regmap,
				TEGRA210_MVC_SWITCH, &value);
		wait--;
		if (!wait) {
			ret = -EINVAL;
			goto end;
		}
	} while (value & TEGRA210_MVC_VOLUME_SWITCH_MASK);

	if (reg == TEGRA210_MVC_TARGET_VOL) {
		if (mvc->curve_type == CURVE_POLY) {
			val = ucontrol->value.integer.value[0];
			if (val > 100)
				val = 100;
			mvc->volume = val * (1<<24);
		} else {
			val = ucontrol->value.integer.value[0] - 120;
			val <<= 8;
			mvc->volume = val;
		}
		ret = regmap_write(mvc->regmap, reg, mvc->volume);
	} else {
		ret = regmap_update_bits(mvc->regmap, reg,
				TEGRA210_MVC_MUTE_MASK,
				(ucontrol->value.integer.value[0] ?
				TEGRA210_MVC_MUTE_EN : 0));
	}
	ret |= regmap_update_bits(mvc->regmap, TEGRA210_MVC_SWITCH,
			TEGRA210_MVC_VOLUME_SWITCH_MASK,
			TEGRA210_MVC_VOLUME_SWITCH_TRIGGER);

end:
	pm_runtime_put(codec->dev);

	return ret;
}

static int tegra210_mvc_get_curve_type(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_mvc *mvc = snd_soc_codec_get_drvdata(codec);
	unsigned int reg = TEGRA210_MVC_CTRL;
	u32 val;

	pm_runtime_get_sync(codec->dev);
	regmap_read(mvc->regmap, reg, &val);
	ucontrol->value.integer.value[0] =
		(val & TEGRA210_MVC_CURVE_TYPE_MASK) >>
		TEGRA210_MVC_CURVE_TYPE_SHIFT;
	pm_runtime_put(codec->dev);

	return 0;
}

static int tegra210_mvc_put_curve_type(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_mvc *mvc = snd_soc_codec_get_drvdata(codec);
	unsigned int reg = TEGRA210_MVC_CTRL;
	int dcnt = 10;
	int ret = 0;
	u32 value;

	/* if no change in curve type, do nothing */
	if (mvc->curve_type == ucontrol->value.integer.value[0])
		return ret;

	pm_runtime_get_sync(codec->dev);

	/* change curve type */
	ret |= regmap_update_bits(mvc->regmap, reg,
			TEGRA210_MVC_CURVE_TYPE_MASK,
			ucontrol->value.integer.value[0] <<
			TEGRA210_MVC_CURVE_TYPE_SHIFT);
	mvc->curve_type = ucontrol->value.integer.value[0];

	/* issue soft reset */
	regmap_write(mvc->regmap, TEGRA210_MVC_SOFT_RESET, 1);
	/* wait for soft reset bit to clear */
	do {
		udelay(10);
		ret = regmap_read(mvc->regmap, TEGRA210_MVC_SOFT_RESET, &value);
		dcnt--;
		if (dcnt < 0) {
			ret = -EINVAL;
			goto end;
		}
	} while (value);

	/* change volume to default init for new curve type */
	if (ucontrol->value.integer.value[0] == CURVE_POLY)
		mvc->volume = TEGRA210_MVC_INIT_VOL_DEFAULT_POLY;
	else
		mvc->volume = TEGRA210_MVC_INIT_VOL_DEFAULT_LINEAR;

end:
	pm_runtime_put(codec->dev);

	return ret;
}

static int tegra210_mvc_get_audio_bits(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_mvc *mvc = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = (mvc->audio_bits + 1) * 4;

	return 0;
}

static int tegra210_mvc_put_audio_bits(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_mvc *mvc = snd_soc_codec_get_drvdata(codec);
	unsigned int val;

	val = ucontrol->value.integer.value[0];
	if ((val >= 8) && (val <= 32) && (val%4 == 0))
		mvc->audio_bits = val/4 - 1;
	else
		return -EINVAL;

	return 0;
}

static int tegra210_mvc_get_channels(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_mvc *mvc = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = mvc->cif_channels;

	return 0;
}

static int tegra210_mvc_put_channels(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_mvc *mvc = snd_soc_codec_get_drvdata(codec);
	unsigned int val;

	val = ucontrol->value.integer.value[0];
	if ((val > 0) && (val << 16))
		mvc->cif_channels = val;
	else
		return -EINVAL;

	return 0;
}

static int tegra210_mvc_set_audio_cif(struct tegra210_mvc *mvc,
				struct snd_pcm_hw_params *params,
				unsigned int reg)
{
	int channels, audio_bits;
	struct tegra210_xbar_cif_conf cif_conf;

	memset(&cif_conf, 0, sizeof(struct tegra210_xbar_cif_conf));

	channels = params_channels(params);
	if (mvc->cif_channels > 0)
		channels = mvc->cif_channels;

	if (channels > 8)
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

	if (mvc->audio_bits > 0)
		audio_bits = mvc->audio_bits;

	cif_conf.audio_channels = channels;
	cif_conf.client_channels = channels;
	cif_conf.audio_bits = audio_bits;
	cif_conf.client_bits = audio_bits;

	mvc->soc_data->set_audio_cif(mvc->regmap, reg, &cif_conf);

	return 0;
}

static int tegra210_mvc_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct tegra210_mvc *mvc = snd_soc_dai_get_drvdata(dai);
	int i, ret;

	/* set RX cif and TX cif */
	ret = tegra210_mvc_set_audio_cif(mvc, params,
				TEGRA210_MVC_AXBAR_RX_CIF_CTRL);
	if (ret) {
		dev_err(dev, "Can't set MVC RX CIF: %d\n", ret);
		return ret;
	}
	ret = tegra210_mvc_set_audio_cif(mvc, params,
				TEGRA210_MVC_AXBAR_TX_CIF_CTRL);
	if (ret) {
		dev_err(dev, "Can't set MVC TX CIF: %d\n", ret);
		return ret;
	}

	/* disable per_channel_cntrl_en */
	regmap_update_bits(mvc->regmap, TEGRA210_MVC_CTRL,
		TEGRA210_MVC_PER_CHAN_CTRL_EN_MASK,
		~(TEGRA210_MVC_PER_CHAN_CTRL_EN_MASK));

	/* init the default volume=1 for MVC */
	regmap_write(mvc->regmap, TEGRA210_MVC_INIT_VOL,
		(mvc->curve_type == CURVE_POLY) ?
		TEGRA210_MVC_INIT_VOL_DEFAULT_POLY :
		TEGRA210_MVC_INIT_VOL_DEFAULT_LINEAR);

	regmap_write(mvc->regmap, TEGRA210_MVC_TARGET_VOL, mvc->volume);
	/* trigger volume switch */
	ret |= regmap_update_bits(mvc->regmap, TEGRA210_MVC_SWITCH,
			TEGRA210_MVC_VOLUME_SWITCH_MASK,
			TEGRA210_MVC_VOLUME_SWITCH_TRIGGER);


	/* program the poly coefficients */
	for (i = 0; i < 9; i++)
		tegra210_mvc_write_ram(mvc, i, mvc->poly_coeff[i]);


	/* program poly_n1, poly_n2, duration */
	regmap_write(mvc->regmap, TEGRA210_MVC_POLY_N1, mvc->poly_n1);
	regmap_write(mvc->regmap, TEGRA210_MVC_POLY_N2, mvc->poly_n2);
	regmap_write(mvc->regmap, TEGRA210_MVC_DURATION, mvc->duration);

	/* program duration_inv */
	regmap_write(mvc->regmap, TEGRA210_MVC_DURATION_INV, mvc->duration_inv);

	return ret;
}

static int tegra210_mvc_codec_probe(struct snd_soc_codec *codec)
{
	struct tegra210_mvc *mvc = snd_soc_codec_get_drvdata(codec);

	codec->control_data = mvc->regmap;

	return 0;
}

static struct snd_soc_dai_ops tegra210_mvc_dai_ops = {
	.hw_params	= tegra210_mvc_hw_params,
};

static const unsigned int tegra210_mvc_curve_type_values[] = {
	CURVE_POLY,
	CURVE_LINEAR,
};

static const char * const tegra210_mvc_curve_type_text[] = {
	"Poly",
	"Linear",
};

static const struct soc_enum tegra210_mvc_curve_type_ctrl =
	SOC_ENUM_SINGLE_EXT(2, tegra210_mvc_curve_type_text);

static const struct snd_kcontrol_new tegra210_mvc_vol_ctrl[] = {
	SOC_SINGLE_EXT("Vol", TEGRA210_MVC_TARGET_VOL, 0, 160, 0,
		tegra210_mvc_get_vol, tegra210_mvc_put_vol),
	SOC_SINGLE_EXT("Mute", TEGRA210_MVC_CTRL, 0, 1, 0,
		tegra210_mvc_get_vol, tegra210_mvc_put_vol),
	SOC_ENUM_EXT("Curve Type", tegra210_mvc_curve_type_ctrl,
		tegra210_mvc_get_curve_type, tegra210_mvc_put_curve_type),
	SOC_SINGLE_EXT("Bits", 0, 0, 32, 0,
		tegra210_mvc_get_audio_bits, tegra210_mvc_put_audio_bits),
	SOC_SINGLE_EXT("Channels", 0, 0, 16, 0,
		tegra210_mvc_get_channels, tegra210_mvc_put_channels),
};

static struct snd_soc_dai_driver tegra210_mvc_dais[] = {
	{
		.name = "MVC IN",
		.playback = {
			.stream_name = "MVC Receive",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S8 |
				SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_S32_LE,
		},
	},
	{
		.name = "MVC OUT",
		.capture = {
			.stream_name = "MVC Transmit",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S8 |
				SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &tegra210_mvc_dai_ops,
	}
};

static const struct snd_soc_dapm_widget tegra210_mvc_widgets[] = {
	SND_SOC_DAPM_AIF_IN("MVC RX", NULL, 0, SND_SOC_NOPM,
				0, 0),
	SND_SOC_DAPM_AIF_OUT("MVC TX", NULL, 0, TEGRA210_MVC_ENABLE,
				TEGRA210_MVC_EN_SHIFT, 0),
};

static const struct snd_soc_dapm_route tegra210_mvc_routes[] = {
	{ "MVC RX",       NULL, "MVC Receive" },
	{ "MVC TX",       NULL, "MVC RX" },
	{ "MVC Transmit", NULL, "MVC TX" },
};

static struct snd_soc_codec_driver tegra210_mvc_codec = {
	.probe = tegra210_mvc_codec_probe,
	.dapm_widgets = tegra210_mvc_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tegra210_mvc_widgets),
	.dapm_routes = tegra210_mvc_routes,
	.num_dapm_routes = ARRAY_SIZE(tegra210_mvc_routes),
	.controls = tegra210_mvc_vol_ctrl,
	.num_controls = ARRAY_SIZE(tegra210_mvc_vol_ctrl),
	.idle_bias_off = 1,
};

static bool tegra210_mvc_wr_rd_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA210_MVC_AXBAR_RX_STATUS:
	case TEGRA210_MVC_AXBAR_RX_INT_STATUS:
	case TEGRA210_MVC_AXBAR_RX_INT_MASK:
	case TEGRA210_MVC_AXBAR_RX_INT_SET:
	case TEGRA210_MVC_AXBAR_RX_INT_CLEAR:
	case TEGRA210_MVC_AXBAR_RX_CIF_CTRL:
	case TEGRA210_MVC_AXBAR_RX_CYA:
	case TEGRA210_MVC_AXBAR_RX_DBG:
	case TEGRA210_MVC_AXBAR_TX_STATUS:
	case TEGRA210_MVC_AXBAR_TX_INT_STATUS:
	case TEGRA210_MVC_AXBAR_TX_INT_MASK:
	case TEGRA210_MVC_AXBAR_TX_INT_SET:
	case TEGRA210_MVC_AXBAR_TX_INT_CLEAR:
	case TEGRA210_MVC_AXBAR_TX_CIF_CTRL:
	case TEGRA210_MVC_AXBAR_TX_CYA:
	case TEGRA210_MVC_AXBAR_TX_DBG:
	case TEGRA210_MVC_ENABLE:
	case TEGRA210_MVC_SOFT_RESET:
	case TEGRA210_MVC_CG:
	case TEGRA210_MVC_STATUS:
	case TEGRA210_MVC_INT_STATUS:
	case TEGRA210_MVC_CTRL:
	case TEGRA210_MVC_SWITCH:
	case TEGRA210_MVC_INIT_VOL:
	case TEGRA210_MVC_TARGET_VOL:
	case TEGRA210_MVC_DURATION:
	case TEGRA210_MVC_DURATION_INV:
	case TEGRA210_MVC_POLY_N1:
	case TEGRA210_MVC_POLY_N2:
	case TEGRA210_MVC_PEAK_CTRL:
	case TEGRA210_MVC_AHUBRAMCTL_CONFIG_RAM_CTRL:
	case TEGRA210_MVC_AHUBRAMCTL_CONFIG_RAM_DATA:
	case TEGRA210_MVC_PEAK_VALUE:
	case TEGRA210_MVC_CONFIG_ERR_TYPE:
	case TEGRA210_MVC_CYA:
	case TEGRA210_MVC_DBG:
		return true;
	default:
		return false;
	};
}

static bool tegra210_mvc_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA210_MVC_AXBAR_RX_STATUS:
	case TEGRA210_MVC_AXBAR_RX_INT_STATUS:
	case TEGRA210_MVC_AXBAR_RX_INT_SET:

	case TEGRA210_MVC_AXBAR_TX_STATUS:
	case TEGRA210_MVC_AXBAR_TX_INT_STATUS:
	case TEGRA210_MVC_AXBAR_TX_INT_SET:

	case TEGRA210_MVC_SOFT_RESET:
	case TEGRA210_MVC_STATUS:
	case TEGRA210_MVC_INT_STATUS:
	case TEGRA210_MVC_SWITCH:
	case TEGRA210_MVC_AHUBRAMCTL_CONFIG_RAM_CTRL:
	case TEGRA210_MVC_AHUBRAMCTL_CONFIG_RAM_DATA:
	case TEGRA210_MVC_PEAK_VALUE:
	case TEGRA210_MVC_CTRL:
		return true;
	default:
		return false;
	};
}

static const struct regmap_config tegra210_mvc_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = TEGRA210_MVC_CYA,
	.writeable_reg = tegra210_mvc_wr_rd_reg,
	.readable_reg = tegra210_mvc_wr_rd_reg,
	.volatile_reg = tegra210_mvc_volatile_reg,
	.reg_defaults = tegra210_mvc_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(tegra210_mvc_reg_defaults),
	.cache_type = REGCACHE_FLAT,
};

static const struct tegra210_mvc_soc_data soc_data_tegra210 = {
	.set_audio_cif = tegra210_xbar_set_cif,
};

static const struct of_device_id tegra210_mvc_of_match[] = {
	{ .compatible = "nvidia,tegra210-mvc", .data = &soc_data_tegra210 },
	{},
};

static int tegra210_mvc_platform_probe(struct platform_device *pdev)
{
	struct tegra210_mvc *mvc;
	struct resource *mem, *memregion;
	void __iomem *regs;
	int ret = 0;
	const struct of_device_id *match;
	struct tegra210_mvc_soc_data *soc_data;

	match = of_match_device(tegra210_mvc_of_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "Error: No device match found\n");
		ret = -ENODEV;
		goto err;
	}
	soc_data = (struct tegra210_mvc_soc_data *)match->data;

	mvc = devm_kzalloc(&pdev->dev, sizeof(struct tegra210_mvc), GFP_KERNEL);
	if (!mvc) {
		dev_err(&pdev->dev, "Can't allocate mvc\n");
		ret = -ENOMEM;
		goto err;
	}

	mvc->soc_data = soc_data;

	mvc->poly_n1 = 16;
	mvc->poly_n2 = 63;
	mvc->duration = 150;
	mvc->duration_inv = 14316558;
	mvc->poly_coeff[0] = 23738319;
	mvc->poly_coeff[1] = 659403;
	mvc->poly_coeff[2] = -3680;
	mvc->poly_coeff[3] = 15546680;
	mvc->poly_coeff[4] = 2530732;
	mvc->poly_coeff[5] = -120985;
	mvc->poly_coeff[6] = 12048422;
	mvc->poly_coeff[7] = 5527252;
	mvc->poly_coeff[8] = -785042;
	mvc->curve_type = CURVE_LINEAR;
	mvc->volume = TEGRA210_MVC_INIT_VOL_DEFAULT_POLY;

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

	mvc->regmap = devm_regmap_init_mmio(&pdev->dev, regs,
					    &tegra210_mvc_regmap_config);
	if (IS_ERR(mvc->regmap)) {
		dev_err(&pdev->dev, "regmap init failed\n");
		ret = PTR_ERR(mvc->regmap);
		goto err;
	}
	regcache_cache_only(mvc->regmap, true);

	if (of_property_read_u32(pdev->dev.of_node,
				"nvidia,ahub-mvc-id",
				&pdev->dev.id) < 0) {
		dev_err(&pdev->dev,
			"Missing property nvidia,ahub-mvc-id\n");
		ret = -ENODEV;
		goto err;
	}

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		ret = tegra210_mvc_runtime_resume(&pdev->dev);
		if (ret)
			goto err_pm_disable;
	}

	ret = snd_soc_register_codec(&pdev->dev, &tegra210_mvc_codec,
				     tegra210_mvc_dais,
				     ARRAY_SIZE(tegra210_mvc_dais));
	if (ret != 0) {
		dev_err(&pdev->dev, "Could not register CODEC: %d\n", ret);
		goto err_suspend;
	}

	dev_set_drvdata(&pdev->dev, mvc);

	return 0;

err_suspend:
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra210_mvc_runtime_suspend(&pdev->dev);
err_pm_disable:
	pm_runtime_disable(&pdev->dev);
err:
	return ret;
}

static int tegra210_mvc_platform_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra210_mvc_runtime_suspend(&pdev->dev);

	return 0;
}

static const struct dev_pm_ops tegra210_mvc_pm_ops = {
	SET_RUNTIME_PM_OPS(tegra210_mvc_runtime_suspend,
			   tegra210_mvc_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(tegra210_mvc_suspend, NULL)
};

static struct platform_driver tegra210_mvc_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = tegra210_mvc_of_match,
		.pm = &tegra210_mvc_pm_ops,
	},
	.probe = tegra210_mvc_platform_probe,
	.remove = tegra210_mvc_platform_remove,
};
module_platform_driver(tegra210_mvc_driver)

MODULE_AUTHOR("Arun Shamanna Lakshmi <aruns@nvidia.com>");
MODULE_DESCRIPTION("Tegra210 MVC ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, tegra210_mvc_of_match);
