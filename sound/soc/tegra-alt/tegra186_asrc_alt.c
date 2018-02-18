/*
 * tegra186_asrc_alt.c - Tegra186 ASRC driver
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/delay.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_device.h>
#include <linux/tegra186_ahc.h>

#include "tegra210_xbar_alt.h"
#include "tegra186_asrc_alt.h"
#include "tegra186_arad_alt.h"

#define DRV_NAME "tegra186-asrc"
#define ASRC_ARAM_START_ADDR 0x3F800000
#define RATIO_ARAD	0
#define RATIO_SW	1

#define ASRC_STREAM_SOURCE_SELECT(id)	\
	(TEGRA186_ASRC_STREAM1_CONFIG + id*TEGRA186_ASRC_STREAM_STRIDE)

#define ASRC_STREAM_REG(reg, id) (reg + (id * TEGRA186_ASRC_STREAM_STRIDE))

#define ASRC_STREAM_REG_DEFAULTS(id) \
	{ ASRC_STREAM_REG(TEGRA186_ASRC_STREAM1_CONFIG, id), ((id+1)<<4)}, \
	{ ASRC_STREAM_REG(TEGRA186_ASRC_STREAM1_RATIO_INTEGER_PART, id), 0x1}, \
	{ ASRC_STREAM_REG(TEGRA186_ASRC_STREAM1_RATIO_FRAC_PART, id), 0x0}, \
	{ ASRC_STREAM_REG(TEGRA186_ASRC_STREAM1_MUTE_UNMUTE_DURATION, id), 0x400}, \
	{ ASRC_STREAM_REG(TEGRA186_ASRC_STREAM1_RX_CIF_CTRL, id), 0x7700}, \
	{ ASRC_STREAM_REG(TEGRA186_ASRC_STREAM1_TX_CIF_CTRL, id), 0x7700}, \
	{ ASRC_STREAM_REG(TEGRA186_ASRC_STREAM1_ENABLE, id), 0x0}, \
	{ ASRC_STREAM_REG(TEGRA186_ASRC_STREAM1_SOFT_RESET, id), 0x0}, \
	{ ASRC_STREAM_REG(TEGRA186_ASRC_STREAM1_STATEBUF_ADDR, id), 0x0}, \
	{ ASRC_STREAM_REG(TEGRA186_ASRC_STREAM1_STATEBUF_CONFIG, id), 0x445}, \
	{ ASRC_STREAM_REG(TEGRA186_ASRC_STREAM1_INSAMPLEBUF_ADDR, id), 0x0}, \
	{ ASRC_STREAM_REG(TEGRA186_ASRC_STREAM1_INSAMPLEBUF_CONFIG, id), 0x64}, \
	{ ASRC_STREAM_REG(TEGRA186_ASRC_STREAM1_OUTSAMPLEBUF_ADDR, id), 0x4b0}, \
	{ ASRC_STREAM_REG(TEGRA186_ASRC_STREAM1_OUTSAMPLEBUF_CONFIG, id), 0x64}
static struct device *asrc_dev;

static const struct reg_default tegra186_asrc_reg_defaults[] = {
	ASRC_STREAM_REG_DEFAULTS(0),
	ASRC_STREAM_REG_DEFAULTS(1),
	ASRC_STREAM_REG_DEFAULTS(2),
	ASRC_STREAM_REG_DEFAULTS(3),
	ASRC_STREAM_REG_DEFAULTS(4),
	ASRC_STREAM_REG_DEFAULTS(5),

	{ TEGRA186_ASRC_GLOBAL_ENB, 0},
	{ TEGRA186_ASRC_GLOBAL_SOFT_RESET, 0},
	{ TEGRA186_ASRC_GLOBAL_CG, 0},
	{ TEGRA186_ASRC_GLOBAL_SCRATCH_ADDR, 0},
	{ TEGRA186_ASRC_GLOBAL_SCRATCH_CONFIG, 0x0c207980},
	{ TEGRA186_ASRC_RATIO_UPD_RX_CIF_CTRL, 0x00115500},
	{ TEGRA186_ASRC_RATIO_UPD_RX_STATUS, 0},
	{ TEGRA186_ASRC_GLOBAL_STATUS, 0},
	{ TEGRA186_ASRC_GLOBAL_STREAM_ENABLE_STATUS, 0},
	{ TEGRA186_ASRC_GLOBAL_INT_MASK, 0x0},
	{ TEGRA186_ASRC_GLOBAL_INT_SET, 0x0},
	{ TEGRA186_ASRC_GLOBAL_INT_CLEAR, 0x0},
	{ TEGRA186_ASRC_GLOBAL_APR_CTRL, 0x0},
	{ TEGRA186_ASRC_GLOBAL_APR_CTRL_ACCESS_CTRL, 0x0},
	{ TEGRA186_ASRC_GLOBAL_DISARM_APR, 0x0},
	{ TEGRA186_ASRC_GLOBAL_DISARM_APR_ACCESS_CTRL, 0x0},
	{ TEGRA186_ASRC_GLOBAL_RATIO_WR_ACCESS, 0x0},
	{ TEGRA186_ASRC_GLOBAL_RATIO_WR_ACCESS_CTRL, 0x0},
	{ TEGRA186_ASRC_CYA, 0x0},
};


static int tegra186_asrc_get_stream_enable_status(struct tegra186_asrc *asrc,
				unsigned int lane_id)
{
	int val;

	regmap_read(asrc->regmap,
		ASRC_STREAM_REG(
			TEGRA186_ASRC_STREAM1_STATUS, lane_id),
		&val);

	return val & 0x01;

}

static int tegra186_asrc_get_ratio_lock_status(struct tegra186_asrc *asrc,
				unsigned int lane_id)
{
	int val;

	regmap_read(asrc->regmap,
		ASRC_STREAM_REG(
			TEGRA186_ASRC_STREAM1_RATIO_LOCK_STATUS, lane_id),
		&val);

	return val & 0x1;
}

static void tegra186_asrc_set_ratio_lock_status(struct tegra186_asrc *asrc,
				unsigned int lane_id)
{
	regmap_write(asrc->regmap,
		ASRC_STREAM_REG(
			TEGRA186_ASRC_STREAM1_RATIO_LOCK_STATUS, lane_id), 1);
}

int tegra186_asrc_set_source(int id, int source)
{
	struct tegra186_asrc *asrc = dev_get_drvdata(asrc_dev);

	regmap_update_bits(asrc->regmap,
		ASRC_STREAM_REG(TEGRA186_ASRC_STREAM1_CONFIG, id),
		TEGRA186_ASRC_STREAM_RATIO_TYPE_MASK, (source & 1));

	return 0;
}

int tegra186_asrc_update_ratio(int id, int inte, int frac)
{
	struct tegra186_asrc *asrc = dev_get_drvdata(asrc_dev);

	regmap_write(asrc->regmap, ASRC_STREAM_REG(
		TEGRA186_ASRC_STREAM1_RATIO_INTEGER_PART, id),
		inte);
	regmap_write(asrc->regmap, ASRC_STREAM_REG(
		TEGRA186_ASRC_STREAM1_RATIO_FRAC_PART, id),
		frac);

	tegra186_asrc_set_ratio_lock_status(asrc, (unsigned int)id);

	return 0;
}

static int tegra186_asrc_runtime_suspend(struct device *dev)
{
	struct tegra186_asrc *asrc = dev_get_drvdata(dev);

#ifdef CONFIG_TEGRA186_AHC
	regmap_write(asrc->regmap, TEGRA186_ASRC_GLOBAL_INT_MASK, 0x1);
#endif
	regcache_cache_only(asrc->regmap, true);
	regcache_mark_dirty(asrc->regmap);

	pm_runtime_put_sync(dev->parent);

	return 0;
}

static int tegra186_asrc_runtime_resume(struct device *dev)
{
	struct tegra186_asrc *asrc = dev_get_drvdata(dev);
	int ret, lane_id;
	ret = pm_runtime_get_sync(dev->parent);
	if (ret < 0) {
		dev_err(dev, "parent get_sync failed: %d\n", ret);
		return ret;
	}

	regcache_cache_only(asrc->regmap, false);
	regcache_sync(asrc->regmap);

	/* HW needs sw reset to make sure previous
		transaction was clean */
	regmap_write(asrc->regmap,
		TEGRA186_ASRC_GLOBAL_SOFT_RESET, 0x1);
	/* Set global starting address of the buffer in ARAM */
	regmap_write(asrc->regmap,
		TEGRA186_ASRC_GLOBAL_SCRATCH_ADDR,
		ASRC_ARAM_START_ADDR);

	regmap_write(asrc->regmap, TEGRA186_ASRC_GLOBAL_INT_MASK,
		0x01);
	/* set global enable */
	regmap_write(asrc->regmap,
		TEGRA186_ASRC_GLOBAL_ENB, TEGRA186_ASRC_GLOBAL_EN);

	regmap_write(asrc->regmap, TEGRA186_ASRC_GLOBAL_INT_CLEAR,
		0x01);
	for (lane_id = 0; lane_id < 6; lane_id++) {
		if (asrc->lane[lane_id].ratio_source == RATIO_SW) {
			regmap_write(asrc->regmap,
				ASRC_STREAM_REG(
				TEGRA186_ASRC_STREAM1_RATIO_INTEGER_PART,
				lane_id),
				asrc->lane[lane_id].int_part);
			regmap_write(asrc->regmap,
				ASRC_STREAM_REG(
					TEGRA186_ASRC_STREAM1_RATIO_FRAC_PART,
					lane_id),
				asrc->lane[lane_id].frac_part);
			tegra186_asrc_set_ratio_lock_status(asrc, lane_id);
		}
	}
#ifdef CONFIG_TEGRA186_AHC
	regmap_write(asrc->regmap, TEGRA186_ASRC_GLOBAL_INT_MASK, 0x0);
#endif

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tegra186_asrc_suspend(struct device *dev)
{
	return 0;
}
#endif

static int tegra186_asrc_set_audio_cif(struct tegra186_asrc *asrc,
				struct snd_pcm_hw_params *params,
				unsigned int reg)
{
	int channels, audio_bits;
	struct tegra210_xbar_cif_conf cif_conf;

	memset(&cif_conf, 0, sizeof(struct tegra210_xbar_cif_conf));

	channels = params_channels(params);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		audio_bits = TEGRA210_AUDIOCIF_BITS_16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		audio_bits = TEGRA210_AUDIOCIF_BITS_24;
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
	asrc->soc_data->set_audio_cif(asrc->regmap, reg, &cif_conf);

	return 0;
}

static int tegra186_asrc_in_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct tegra186_asrc *asrc = snd_soc_dai_get_drvdata(dai);
	int ret, lane_id = dai->id;

	/* set threshold */
	regmap_write(asrc->regmap,
		ASRC_STREAM_REG(TEGRA186_ASRC_STREAM1_RX_THRESHOLD, dai->id),
		asrc->lane[lane_id].input_thresh);

	ret = tegra186_asrc_set_audio_cif(asrc, params,
		ASRC_STREAM_REG(TEGRA186_ASRC_STREAM1_RX_CIF_CTRL, dai->id));
	if (ret) {
		dev_err(dev, "Can't set ASRC RX%d CIF: %d\n", dai->id, ret);
		return ret;
	}

	return ret;
}

static int tegra186_asrc_out_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct tegra186_asrc *asrc = snd_soc_dai_get_drvdata(dai);
	int ret, lane_id = dai->id - 7, dcnt = 10;

	regmap_update_bits(asrc->regmap,
		ASRC_STREAM_REG(TEGRA186_ASRC_STREAM1_CONFIG, lane_id),
		1, asrc->lane[lane_id].ratio_source);

	ret = tegra186_asrc_set_audio_cif(asrc, params,
		ASRC_STREAM_REG(TEGRA186_ASRC_STREAM1_TX_CIF_CTRL, lane_id));
	if (ret) {
		dev_err(dev, "Can't set ASRC TX%d CIF: %d\n", lane_id, ret);
		return ret;
	}

	/* set ENABLE_HW_RATIO_COMP */
	if (asrc->lane[lane_id].hwcomp_disable) {
		regmap_update_bits(asrc->regmap,
			ASRC_STREAM_REG(TEGRA186_ASRC_STREAM1_CONFIG, lane_id),
			TEGRA186_ASRC_STREAM_ENABLE_HW_RATIO_COMP_MASK,
			TEGRA186_ASRC_STREAM_ENABLE_HW_RATIO_COMP_DISABLE);
	} else {
		regmap_update_bits(asrc->regmap,
			ASRC_STREAM_REG(TEGRA186_ASRC_STREAM1_CONFIG, lane_id),
			TEGRA186_ASRC_STREAM_ENABLE_HW_RATIO_COMP_MASK,
			TEGRA186_ASRC_STREAM_ENABLE_HW_RATIO_COMP_ENABLE);

		regmap_write(asrc->regmap,
			ASRC_STREAM_REG(
			TEGRA186_ASRC_STREAM1_RATIO_COMP, lane_id),
			TEGRA186_ASRC_STREAM_DEFAULT_HW_COMP_BIAS_VALUE);
	}

	/* set lock */
	if (asrc->lane[lane_id].ratio_source == RATIO_SW) {
		regmap_write(asrc->regmap,
			ASRC_STREAM_REG(
				TEGRA186_ASRC_STREAM1_RATIO_INTEGER_PART,
				lane_id),
			asrc->lane[lane_id].int_part);
		regmap_write(asrc->regmap,
			ASRC_STREAM_REG(
				TEGRA186_ASRC_STREAM1_RATIO_FRAC_PART, lane_id),
			asrc->lane[lane_id].frac_part);
		tegra186_asrc_set_ratio_lock_status(asrc, lane_id);
	} else
		while (!tegra186_asrc_get_ratio_lock_status(asrc, lane_id) &&
			dcnt--)
			udelay(100);

	 /* set threshold */
	regmap_write(asrc->regmap,
		ASRC_STREAM_REG(TEGRA186_ASRC_STREAM1_TX_THRESHOLD, lane_id),
		asrc->lane[lane_id].output_thresh);

	return ret;
}

static int tegra186_asrc_get_ratio_source(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *asrc_private =
		(struct soc_enum  *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra186_asrc *asrc = snd_soc_codec_get_drvdata(codec);
	unsigned int id = asrc_private->reg / TEGRA186_ASRC_STREAM_STRIDE;

	/* get the source of a lane in asrc */
	ucontrol->value.integer.value[0] = asrc->lane[id].ratio_source;

	return 0;
}

static int tegra186_asrc_put_ratio_source(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *asrc_private =
		(struct soc_enum  *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra186_asrc *asrc = snd_soc_codec_get_drvdata(codec);
	unsigned int id = asrc_private->reg / TEGRA186_ASRC_STREAM_STRIDE;

	/* update the source of the lane */
	asrc->lane[id].ratio_source = ucontrol->value.integer.value[0];
	regmap_update_bits(asrc->regmap, asrc_private->reg,
		TEGRA186_ASRC_STREAM_RATIO_TYPE_MASK,
		asrc->lane[id].ratio_source);

	return 0;
}

static int tegra186_asrc_get_enable_stream(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *asrc_private =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra186_asrc *asrc = snd_soc_codec_get_drvdata(codec);
	unsigned int enable;

	regmap_read(asrc->regmap, asrc_private->reg, &enable);
	ucontrol->value.integer.value[0] = enable;

	return 0;
}

static int tegra186_asrc_put_enable_stream(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *asrc_private =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra186_asrc *asrc = snd_soc_codec_get_drvdata(codec);
	unsigned int enable = 0;

	enable = ucontrol->value.integer.value[0];
	regmap_write(asrc->regmap, asrc_private->reg, enable);

	return 0;
}

static int tegra186_asrc_get_ratio_int(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *asrc_private =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra186_asrc *asrc = snd_soc_codec_get_drvdata(codec);
	unsigned int id = asrc_private->reg / TEGRA186_ASRC_STREAM_STRIDE;

	regmap_read(asrc->regmap,
		ASRC_STREAM_REG(TEGRA186_ASRC_STREAM1_RATIO_INTEGER_PART, id),
		&asrc->lane[id].int_part);
	ucontrol->value.integer.value[0] = asrc->lane[id].int_part;

	return 0;
}

static int tegra186_asrc_put_ratio_int(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *asrc_private =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra186_asrc *asrc = snd_soc_codec_get_drvdata(codec);
	unsigned int id = asrc_private->reg / TEGRA186_ASRC_STREAM_STRIDE;

	asrc->lane[id].int_part = ucontrol->value.integer.value[0];
	regmap_write(asrc->regmap,
		ASRC_STREAM_REG(TEGRA186_ASRC_STREAM1_RATIO_INTEGER_PART, id),
		asrc->lane[id].int_part);
	tegra186_asrc_set_ratio_lock_status(asrc, id);

	return 0;
}

static int tegra186_asrc_get_ratio_frac(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mreg_control *asrc_private =
		(struct soc_mreg_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra186_asrc *asrc = snd_soc_codec_get_drvdata(codec);
	unsigned int id = asrc_private->regbase / TEGRA186_ASRC_STREAM_STRIDE;

	regmap_read(asrc->regmap,
		ASRC_STREAM_REG(TEGRA186_ASRC_STREAM1_RATIO_FRAC_PART, id),
		&asrc->lane[id].frac_part);
	ucontrol->value.integer.value[0] = asrc->lane[id].frac_part;

	return 0;
}

static int tegra186_asrc_put_ratio_frac(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mreg_control *asrc_private =
		(struct soc_mreg_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra186_asrc *asrc = snd_soc_codec_get_drvdata(codec);
	unsigned int id = asrc_private->regbase / TEGRA186_ASRC_STREAM_STRIDE;

	asrc->lane[id].frac_part = ucontrol->value.integer.value[0];
	regmap_write(asrc->regmap,
		ASRC_STREAM_REG(TEGRA186_ASRC_STREAM1_RATIO_FRAC_PART, id),
		asrc->lane[id].frac_part);
	tegra186_asrc_set_ratio_lock_status(asrc, id);

	return 0;
}

static int tegra186_asrc_get_hwcomp_disable(struct snd_kcontrol *kcontrol,
    struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *asrc_private =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra186_asrc *asrc = snd_soc_codec_get_drvdata(codec);
	unsigned int id = asrc_private->reg / TEGRA186_ASRC_STREAM_STRIDE;

	ucontrol->value.integer.value[0] = asrc->lane[id].hwcomp_disable;

	return 0;
}

static int tegra186_asrc_put_hwcomp_disable(struct snd_kcontrol *kcontrol,
    struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *asrc_private =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra186_asrc *asrc = snd_soc_codec_get_drvdata(codec);
	unsigned int id = asrc_private->reg / TEGRA186_ASRC_STREAM_STRIDE;

	asrc->lane[id].hwcomp_disable = ucontrol->value.integer.value[0];

	return 0;
}

static int tegra186_asrc_get_input_threshold(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *asrc_private =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra186_asrc *asrc = snd_soc_codec_get_drvdata(codec);
	unsigned int id = asrc_private->reg / TEGRA186_ASRC_STREAM_STRIDE;

	ucontrol->value.integer.value[0] = (asrc->lane[id].input_thresh & 0x3);

	return 0;
}

static int tegra186_asrc_put_input_threshold(struct snd_kcontrol *kcontrol,
    struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *asrc_private =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra186_asrc *asrc = snd_soc_codec_get_drvdata(codec);
	unsigned int id = asrc_private->reg / TEGRA186_ASRC_STREAM_STRIDE;

	asrc->lane[id].input_thresh = (asrc->lane[id].input_thresh & ~(0x3))
				| ucontrol->value.integer.value[0];
    return 0;
}

static int tegra186_asrc_get_output_threshold(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *asrc_private =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra186_asrc *asrc = snd_soc_codec_get_drvdata(codec);
	unsigned int id = asrc_private->reg / TEGRA186_ASRC_STREAM_STRIDE;

	ucontrol->value.integer.value[0] = (asrc->lane[id].output_thresh & 0x3);

	return 0;
}

static int tegra186_asrc_put_output_threshold(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *asrc_private =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra186_asrc *asrc = snd_soc_codec_get_drvdata(codec);
	unsigned int id = asrc_private->reg / TEGRA186_ASRC_STREAM_STRIDE;

	asrc->lane[id].output_thresh = (asrc->lane[id].output_thresh & ~(0x3))
				| ucontrol->value.integer.value[0];

	return 0;
}
static int tegra186_asrc_req_arad_ratio(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct device *dev = codec->dev;
	struct tegra186_asrc *asrc = dev_get_drvdata(dev);
	int ret = 0;
	unsigned int lane_id = 0;
	lane_id = (w->reg - TEGRA186_ASRC_STREAM1_ENABLE) /
			TEGRA186_ASRC_STREAM_STRIDE;

	if (event == SND_SOC_DAPM_POST_PMD) {
		regmap_write(asrc->regmap, ASRC_STREAM_REG
			(TEGRA186_ASRC_STREAM1_SOFT_RESET, lane_id),
			0x1);
		return ret;
	}
	if (asrc->lane[lane_id].ratio_source == RATIO_ARAD)
		tegra186_arad_send_ratio();

	return ret;
}

static int tegra186_asrc_codec_probe(struct snd_soc_codec *codec)
{
	struct tegra186_asrc *asrc = snd_soc_codec_get_drvdata(codec);

	codec->control_data = asrc->regmap;

	return 0;
}

static struct snd_soc_dai_ops tegra186_asrc_in_dai_ops = {
	.hw_params	= tegra186_asrc_in_hw_params,
};

static struct snd_soc_dai_ops tegra186_asrc_out_dai_ops = {
	.hw_params	= tegra186_asrc_out_hw_params,
};

#define IN_DAI(sname, id, dai_ops)	\
	{							\
		.name = #sname #id,					\
		.playback = {					\
			.stream_name = #sname #id " Receive",	\
			.channels_min = 1,			\
			.channels_max = 12,		\
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
			.channels_max = 12,			\
			.rates = SNDRV_PCM_RATE_8000_192000,		\
			.formats = SNDRV_PCM_FMTBIT_S8 |		\
				SNDRV_PCM_FMTBIT_S16_LE |		\
				SNDRV_PCM_FMTBIT_S24_LE |		\
				SNDRV_PCM_FMTBIT_S32_LE,		\
		},						\
		.ops = dai_ops,		\
	}

static struct snd_soc_dai_driver tegra186_asrc_dais[] = {
	IN_DAI(RX, 1, &tegra186_asrc_in_dai_ops),
	IN_DAI(RX, 2, &tegra186_asrc_in_dai_ops),
	IN_DAI(RX, 3, &tegra186_asrc_in_dai_ops),
	IN_DAI(RX, 4, &tegra186_asrc_in_dai_ops),
	IN_DAI(RX, 5, &tegra186_asrc_in_dai_ops),
	IN_DAI(RX, 6, &tegra186_asrc_in_dai_ops),
	IN_DAI(RX, 7, &tegra186_asrc_in_dai_ops),
	OUT_DAI(TX, 1, &tegra186_asrc_out_dai_ops),
	OUT_DAI(TX, 2, &tegra186_asrc_out_dai_ops),
	OUT_DAI(TX, 3, &tegra186_asrc_out_dai_ops),
	OUT_DAI(TX, 4, &tegra186_asrc_out_dai_ops),
	OUT_DAI(TX, 5, &tegra186_asrc_out_dai_ops),
	OUT_DAI(TX, 6, &tegra186_asrc_out_dai_ops),
};

#define SND_SOC_DAPM_IN(wname, wevent) \
{	.id = snd_soc_dapm_spk, .name = wname, .kcontrol_news = NULL, \
	.num_kcontrols = 0, .reg = SND_SOC_NOPM, .event = wevent, \
	.event_flags = SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD}

static const struct snd_soc_dapm_widget tegra186_asrc_widgets[] = {
	SND_SOC_DAPM_AIF_IN("RX1", NULL, 0, SND_SOC_NOPM,
				0, 0),
	SND_SOC_DAPM_AIF_IN("RX2", NULL, 0, SND_SOC_NOPM,
				0, 0),
	SND_SOC_DAPM_AIF_IN("RX3", NULL, 0, SND_SOC_NOPM,
				0, 0),
	SND_SOC_DAPM_AIF_IN("RX4", NULL, 0, SND_SOC_NOPM,
				0, 0),
	SND_SOC_DAPM_AIF_IN("RX5", NULL, 0, SND_SOC_NOPM,
				0, 0),
	SND_SOC_DAPM_AIF_IN("RX6", NULL, 0, SND_SOC_NOPM,
				0, 0),
	SND_SOC_DAPM_AIF_OUT_E("TX1", NULL, 0, TEGRA186_ASRC_STREAM1_ENABLE,
				TEGRA186_ASRC_STREAM_EN_SHIFT, 0,
				tegra186_asrc_req_arad_ratio,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_OUT_E("TX2", NULL, 0, TEGRA186_ASRC_STREAM2_ENABLE,
				TEGRA186_ASRC_STREAM_EN_SHIFT, 0,
				tegra186_asrc_req_arad_ratio,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_OUT_E("TX3", NULL, 0, TEGRA186_ASRC_STREAM3_ENABLE,
				TEGRA186_ASRC_STREAM_EN_SHIFT, 0,
				tegra186_asrc_req_arad_ratio,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_OUT_E("TX4", NULL, 0, TEGRA186_ASRC_STREAM4_ENABLE,
				TEGRA186_ASRC_STREAM_EN_SHIFT, 0,
				tegra186_asrc_req_arad_ratio,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_OUT_E("TX5", NULL, 0, TEGRA186_ASRC_STREAM5_ENABLE,
				TEGRA186_ASRC_STREAM_EN_SHIFT, 0,
				tegra186_asrc_req_arad_ratio,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_OUT_E("TX6", NULL, 0, TEGRA186_ASRC_STREAM6_ENABLE,
				TEGRA186_ASRC_STREAM_EN_SHIFT, 0,
				tegra186_asrc_req_arad_ratio,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_IN("RX7", NULL),
};

static const struct snd_soc_dapm_route tegra186_asrc_routes[] = {
	{ "RX1",       NULL, "RX1 Receive" },
	{ "TX1",       NULL, "RX1" },
	{ "TX1 Transmit", NULL, "TX1" },
	{ "RX2",       NULL, "RX2 Receive" },
	{ "TX2",       NULL, "RX2" },
	{ "TX2 Transmit", NULL, "TX2" },
	{ "RX3",       NULL, "RX3 Receive" },
	{ "TX3",       NULL, "RX3" },
	{ "TX3 Transmit", NULL, "TX3" },
	{ "RX4",       NULL, "RX4 Receive" },
	{ "TX4",       NULL, "RX4" },
	{ "TX4 Transmit", NULL, "TX4" },
	{ "RX5",       NULL, "RX5 Receive" },
	{ "TX5",       NULL, "RX5" },
	{ "TX5 Transmit", NULL, "TX5" },
	{ "RX6",       NULL, "RX6 Receive" },
	{ "TX6",       NULL, "RX6" },
	{ "TX6 Transmit", NULL, "TX6" },
	{ "RX7",       NULL, "RX7 Receive" },
};

static const char * const tegra186_asrc_ratio_source_text[] = {
	"ARAD",
	"SW",
};

#define ASRC_SOURCE_DECL(name, id) \
	static const struct soc_enum name = \
		SOC_ENUM_SINGLE(ASRC_STREAM_SOURCE_SELECT(id), \
			0, 2, tegra186_asrc_ratio_source_text)

ASRC_SOURCE_DECL(src_select1, 0);
ASRC_SOURCE_DECL(src_select2, 1);
ASRC_SOURCE_DECL(src_select3, 2);
ASRC_SOURCE_DECL(src_select4, 3);
ASRC_SOURCE_DECL(src_select5, 4);
ASRC_SOURCE_DECL(src_select6, 5);

#define SOC_SINGLE_EXT_FRAC(xname, xregbase, \
	xmax, xget, xput) \
{       .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = (xname), \
	.info = snd_soc_info_xr_sx, .get = xget, \
	.put = xput, \
	.private_value = (unsigned long)&(struct soc_mreg_control) \
		{.regbase = xregbase, .regcount = 1, .nbits = 32, \
		.invert = 0, .min = 0, .max = xmax} }

static const struct snd_kcontrol_new tegra186_asrc_controls[] = {
	SOC_SINGLE_EXT("Ratio1 Int", TEGRA186_ASRC_STREAM1_RATIO_INTEGER_PART,
		0, TEGRA186_ASRC_STREAM_RATIO_INTEGER_PART_MASK, 0,
		tegra186_asrc_get_ratio_int, tegra186_asrc_put_ratio_int),
	SOC_SINGLE_EXT_FRAC("Ratio1 Frac",
		TEGRA186_ASRC_STREAM1_RATIO_FRAC_PART,
		TEGRA186_ASRC_STREAM_RATIO_FRAC_PART_MASK,
		tegra186_asrc_get_ratio_frac, tegra186_asrc_put_ratio_frac),
	SOC_SINGLE_EXT("Ratio2 Int", TEGRA186_ASRC_STREAM2_RATIO_INTEGER_PART,
		0, TEGRA186_ASRC_STREAM_RATIO_INTEGER_PART_MASK, 0,
		tegra186_asrc_get_ratio_int, tegra186_asrc_put_ratio_int),
	SOC_SINGLE_EXT_FRAC("Ratio2 Frac",
		TEGRA186_ASRC_STREAM2_RATIO_FRAC_PART,
		TEGRA186_ASRC_STREAM_RATIO_FRAC_PART_MASK,
		tegra186_asrc_get_ratio_frac, tegra186_asrc_put_ratio_frac),
	SOC_SINGLE_EXT("Ratio3 Int", TEGRA186_ASRC_STREAM3_RATIO_INTEGER_PART,
		0, TEGRA186_ASRC_STREAM_RATIO_INTEGER_PART_MASK, 0,
		tegra186_asrc_get_ratio_int, tegra186_asrc_put_ratio_int),
	SOC_SINGLE_EXT_FRAC("Ratio3 Frac",
		TEGRA186_ASRC_STREAM3_RATIO_FRAC_PART,
		TEGRA186_ASRC_STREAM_RATIO_FRAC_PART_MASK,
		tegra186_asrc_get_ratio_frac, tegra186_asrc_put_ratio_frac),
	SOC_SINGLE_EXT("Ratio4 Int", TEGRA186_ASRC_STREAM4_RATIO_INTEGER_PART,
		0, TEGRA186_ASRC_STREAM_RATIO_INTEGER_PART_MASK, 0,
		tegra186_asrc_get_ratio_int, tegra186_asrc_put_ratio_int),
	SOC_SINGLE_EXT_FRAC("Ratio4 Frac",
		TEGRA186_ASRC_STREAM4_RATIO_FRAC_PART,
		TEGRA186_ASRC_STREAM_RATIO_FRAC_PART_MASK,
		tegra186_asrc_get_ratio_frac, tegra186_asrc_put_ratio_frac),
	SOC_SINGLE_EXT("Ratio5 Int", TEGRA186_ASRC_STREAM5_RATIO_INTEGER_PART,
		0, TEGRA186_ASRC_STREAM_RATIO_INTEGER_PART_MASK, 0,
		tegra186_asrc_get_ratio_int, tegra186_asrc_put_ratio_int),
	SOC_SINGLE_EXT_FRAC("Ratio5 Frac",
		TEGRA186_ASRC_STREAM5_RATIO_FRAC_PART,
		TEGRA186_ASRC_STREAM_RATIO_FRAC_PART_MASK,
		tegra186_asrc_get_ratio_frac, tegra186_asrc_put_ratio_frac),
	SOC_SINGLE_EXT("Ratio6 Int", TEGRA186_ASRC_STREAM6_RATIO_INTEGER_PART,
		0, TEGRA186_ASRC_STREAM_RATIO_INTEGER_PART_MASK, 0,
		tegra186_asrc_get_ratio_int, tegra186_asrc_put_ratio_int),
	SOC_SINGLE_EXT_FRAC("Ratio6 Frac",
		TEGRA186_ASRC_STREAM6_RATIO_FRAC_PART,
		TEGRA186_ASRC_STREAM_RATIO_FRAC_PART_MASK,
		tegra186_asrc_get_ratio_frac, tegra186_asrc_put_ratio_frac),

	SOC_ENUM_EXT("Ratio1 SRC", src_select1,
		tegra186_asrc_get_ratio_source, tegra186_asrc_put_ratio_source),
	SOC_ENUM_EXT("Ratio2 SRC", src_select2,
		tegra186_asrc_get_ratio_source, tegra186_asrc_put_ratio_source),
	SOC_ENUM_EXT("Ratio3 SRC", src_select3,
		tegra186_asrc_get_ratio_source, tegra186_asrc_put_ratio_source),
	SOC_ENUM_EXT("Ratio4 SRC", src_select4,
		tegra186_asrc_get_ratio_source, tegra186_asrc_put_ratio_source),
	SOC_ENUM_EXT("Ratio5 SRC", src_select5,
		tegra186_asrc_get_ratio_source, tegra186_asrc_put_ratio_source),
	SOC_ENUM_EXT("Ratio6 SRC", src_select6,
		tegra186_asrc_get_ratio_source, tegra186_asrc_put_ratio_source),

	SOC_SINGLE_EXT("Stream1 Enable",
		TEGRA186_ASRC_STREAM1_ENABLE, 0, 1, 0,
		tegra186_asrc_get_enable_stream, tegra186_asrc_put_enable_stream),
	SOC_SINGLE_EXT("Stream2 Enable",
		TEGRA186_ASRC_STREAM2_ENABLE, 0, 1, 0,
		tegra186_asrc_get_enable_stream, tegra186_asrc_put_enable_stream),
	SOC_SINGLE_EXT("Stream3 Enable",
		TEGRA186_ASRC_STREAM3_ENABLE, 0, 1, 0,
		tegra186_asrc_get_enable_stream, tegra186_asrc_put_enable_stream),
	SOC_SINGLE_EXT("Stream4 Enable",
		TEGRA186_ASRC_STREAM4_ENABLE, 0, 1, 0,
		tegra186_asrc_get_enable_stream, tegra186_asrc_put_enable_stream),
	SOC_SINGLE_EXT("Stream5 Enable",
		TEGRA186_ASRC_STREAM5_ENABLE, 0, 1, 0,
		tegra186_asrc_get_enable_stream, tegra186_asrc_put_enable_stream),
	SOC_SINGLE_EXT("Stream6 Enable",
		TEGRA186_ASRC_STREAM6_ENABLE, 0, 1, 0,
		tegra186_asrc_get_enable_stream, tegra186_asrc_put_enable_stream),
	SOC_SINGLE_EXT("Stream1 Hwcomp Disable",
		TEGRA186_ASRC_STREAM1_CONFIG,
		0, 1, 0,
		tegra186_asrc_get_hwcomp_disable, tegra186_asrc_put_hwcomp_disable),
	SOC_SINGLE_EXT("Stream2 Hwcomp Disable",
		TEGRA186_ASRC_STREAM2_CONFIG,
		0, 1, 0,
		tegra186_asrc_get_hwcomp_disable, tegra186_asrc_put_hwcomp_disable),
	SOC_SINGLE_EXT("Stream3 Hwcomp Disable",
		TEGRA186_ASRC_STREAM3_CONFIG,
		0, 1, 0,
		tegra186_asrc_get_hwcomp_disable, tegra186_asrc_put_hwcomp_disable),
	SOC_SINGLE_EXT("Stream4 Hwcomp Disable",
		TEGRA186_ASRC_STREAM4_CONFIG,
		0, 1, 0,
		tegra186_asrc_get_hwcomp_disable, tegra186_asrc_put_hwcomp_disable),
	SOC_SINGLE_EXT("Stream5 Hwcomp Disable",
		TEGRA186_ASRC_STREAM5_CONFIG,
		0, 1, 0,
		tegra186_asrc_get_hwcomp_disable, tegra186_asrc_put_hwcomp_disable),
	SOC_SINGLE_EXT("Stream6 Hwcomp Disable",
		TEGRA186_ASRC_STREAM6_CONFIG,
		0, 1, 0,
		tegra186_asrc_get_hwcomp_disable, tegra186_asrc_put_hwcomp_disable),
	SOC_SINGLE_EXT("Stream1 Input Thresh",
		TEGRA186_ASRC_STREAM1_RX_THRESHOLD,
		0, 3, 0,
		tegra186_asrc_get_input_threshold, tegra186_asrc_put_input_threshold),
	SOC_SINGLE_EXT("Stream2 Input Thresh",
		TEGRA186_ASRC_STREAM2_RX_THRESHOLD,
		0, 3, 0,
		tegra186_asrc_get_input_threshold, tegra186_asrc_put_input_threshold),
	SOC_SINGLE_EXT("Stream3 Input Thresh",
		TEGRA186_ASRC_STREAM3_RX_THRESHOLD,
		0, 3, 0,
		tegra186_asrc_get_input_threshold, tegra186_asrc_put_input_threshold),
	SOC_SINGLE_EXT("Stream4 Input Thresh",
		TEGRA186_ASRC_STREAM4_RX_THRESHOLD,
		0, 3, 0,
		tegra186_asrc_get_input_threshold, tegra186_asrc_put_input_threshold),
	SOC_SINGLE_EXT("Stream5 Input Thresh",
		TEGRA186_ASRC_STREAM5_RX_THRESHOLD,
		0, 3, 0,
		tegra186_asrc_get_input_threshold, tegra186_asrc_put_input_threshold),
	SOC_SINGLE_EXT("Stream6 Input Thresh",
		TEGRA186_ASRC_STREAM6_RX_THRESHOLD,
		0, 3, 0,
		tegra186_asrc_get_input_threshold, tegra186_asrc_put_input_threshold),
	SOC_SINGLE_EXT("Stream1 Output Thresh",
		TEGRA186_ASRC_STREAM1_TX_THRESHOLD,
		0, 3, 0,
		tegra186_asrc_get_output_threshold, tegra186_asrc_put_output_threshold),
	SOC_SINGLE_EXT("Stream2 Output Thresh",
		TEGRA186_ASRC_STREAM2_TX_THRESHOLD,
		0, 3, 0,
		tegra186_asrc_get_output_threshold, tegra186_asrc_put_output_threshold),
	SOC_SINGLE_EXT("Stream3 Output Thresh",
		TEGRA186_ASRC_STREAM3_TX_THRESHOLD,
		0, 3, 0,
		tegra186_asrc_get_output_threshold, tegra186_asrc_put_output_threshold),
	SOC_SINGLE_EXT("Stream4 Output Thresh",
		TEGRA186_ASRC_STREAM4_TX_THRESHOLD,
		0, 3, 0,
		tegra186_asrc_get_output_threshold, tegra186_asrc_put_output_threshold),
	SOC_SINGLE_EXT("Stream5 Output Thresh",
		TEGRA186_ASRC_STREAM5_TX_THRESHOLD,
		0, 3, 0,
		tegra186_asrc_get_output_threshold, tegra186_asrc_put_output_threshold),
	SOC_SINGLE_EXT("Stream6 Output Thresh",
		TEGRA186_ASRC_STREAM6_TX_THRESHOLD,
		0, 3, 0,
		tegra186_asrc_get_output_threshold, tegra186_asrc_put_output_threshold),
};

static struct snd_soc_codec_driver tegra186_asrc_codec = {
	.probe = tegra186_asrc_codec_probe,
	.dapm_widgets = tegra186_asrc_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tegra186_asrc_widgets),
	.dapm_routes = tegra186_asrc_routes,
	.num_dapm_routes = ARRAY_SIZE(tegra186_asrc_routes),
	.controls = tegra186_asrc_controls,
	.num_controls = ARRAY_SIZE(tegra186_asrc_controls),
	.idle_bias_off = 1,
};

static bool tegra186_asrc_wr_reg(struct device *dev, unsigned int reg)
{
	if (reg < TEGRA186_ASRC_STREAM_LIMIT)
		reg %= TEGRA186_ASRC_STREAM_STRIDE;

	switch (reg) {
	case TEGRA186_ASRC_STREAM1_CONFIG:
	case TEGRA186_ASRC_STREAM1_RATIO_INTEGER_PART:
	case TEGRA186_ASRC_STREAM1_RATIO_FRAC_PART:
	case TEGRA186_ASRC_STREAM1_RX_THRESHOLD:
	case TEGRA186_ASRC_STREAM1_TX_THRESHOLD:
	case TEGRA186_ASRC_STREAM1_RATIO_LOCK_STATUS:
	case TEGRA186_ASRC_STREAM1_MUTE_UNMUTE_DURATION:
	case TEGRA186_ASRC_STREAM1_RATIO_COMP:
	case TEGRA186_ASRC_STREAM1_RX_CIF_CTRL:
	case TEGRA186_ASRC_STREAM1_TX_CIF_CTRL:
	case TEGRA186_ASRC_STREAM1_ENABLE:
	case TEGRA186_ASRC_STREAM1_SOFT_RESET:
	case TEGRA186_ASRC_STREAM1_STATEBUF_ADDR:
	case TEGRA186_ASRC_STREAM1_STATEBUF_CONFIG:
	case TEGRA186_ASRC_STREAM1_INSAMPLEBUF_ADDR:
	case TEGRA186_ASRC_STREAM1_INSAMPLEBUF_CONFIG:
	case TEGRA186_ASRC_STREAM1_OUTSAMPLEBUF_ADDR:
	case TEGRA186_ASRC_STREAM1_OUTSAMPLEBUF_CONFIG:

	case TEGRA186_ASRC_RATIO_UPD_RX_CIF_CTRL:

	case TEGRA186_ASRC_GLOBAL_ENB:
	case TEGRA186_ASRC_GLOBAL_SOFT_RESET:
	case TEGRA186_ASRC_GLOBAL_CG:
	case TEGRA186_ASRC_GLOBAL_SCRATCH_ADDR:
	case TEGRA186_ASRC_GLOBAL_SCRATCH_CONFIG:

	case TEGRA186_ASRC_GLOBAL_INT_MASK:
	case TEGRA186_ASRC_GLOBAL_INT_SET:
	case TEGRA186_ASRC_GLOBAL_INT_CLEAR:
	case TEGRA186_ASRC_GLOBAL_APR_CTRL:
	case TEGRA186_ASRC_GLOBAL_APR_CTRL_ACCESS_CTRL:
	case TEGRA186_ASRC_GLOBAL_DISARM_APR:
	case TEGRA186_ASRC_GLOBAL_DISARM_APR_ACCESS_CTRL:
	case TEGRA186_ASRC_GLOBAL_RATIO_WR_ACCESS:
	case TEGRA186_ASRC_GLOBAL_RATIO_WR_ACCESS_CTRL:
	case TEGRA186_ASRC_CYA:
		return true;
	default:
		return false;
	};
}

static bool tegra186_asrc_rd_reg(struct device *dev, unsigned int reg)
{
	if (reg < TEGRA186_ASRC_STREAM_LIMIT)
		reg %= TEGRA186_ASRC_STREAM_STRIDE;

	switch (reg) {
	case TEGRA186_ASRC_STREAM1_CONFIG:
	case TEGRA186_ASRC_STREAM1_RATIO_INTEGER_PART:
	case TEGRA186_ASRC_STREAM1_RATIO_FRAC_PART:
	case TEGRA186_ASRC_STREAM1_RX_THRESHOLD:
	case TEGRA186_ASRC_STREAM1_TX_THRESHOLD:
	case TEGRA186_ASRC_STREAM1_RATIO_LOCK_STATUS:
	case TEGRA186_ASRC_STREAM1_MUTE_UNMUTE_DURATION:
	case TEGRA186_ASRC_STREAM1_RATIO_COMP:
	case TEGRA186_ASRC_STREAM1_RX_STATUS:
	case TEGRA186_ASRC_STREAM1_RX_CIF_CTRL:
	case TEGRA186_ASRC_STREAM1_TX_STATUS:
	case TEGRA186_ASRC_STREAM1_TX_CIF_CTRL:
	case TEGRA186_ASRC_STREAM1_ENABLE:
	case TEGRA186_ASRC_STREAM1_SOFT_RESET:
	case TEGRA186_ASRC_STREAM1_STATUS:
	case TEGRA186_ASRC_STREAM1_BUFFER_STATUS:
	case TEGRA186_ASRC_STREAM1_CONFIG_ERR_TYPE:
	case TEGRA186_ASRC_STREAM1_STATEBUF_ADDR:
	case TEGRA186_ASRC_STREAM1_STATEBUF_CONFIG:
	case TEGRA186_ASRC_STREAM1_INSAMPLEBUF_ADDR:
	case TEGRA186_ASRC_STREAM1_INSAMPLEBUF_CONFIG:
	case TEGRA186_ASRC_STREAM1_OUTSAMPLEBUF_ADDR:
	case TEGRA186_ASRC_STREAM1_OUTSAMPLEBUF_CONFIG:

	case TEGRA186_ASRC_RATIO_UPD_RX_CIF_CTRL:
	case TEGRA186_ASRC_RATIO_UPD_RX_STATUS:

	case TEGRA186_ASRC_GLOBAL_ENB:
	case TEGRA186_ASRC_GLOBAL_SOFT_RESET:
	case TEGRA186_ASRC_GLOBAL_CG:
	case TEGRA186_ASRC_GLOBAL_SCRATCH_ADDR:
	case TEGRA186_ASRC_GLOBAL_SCRATCH_CONFIG:

	case TEGRA186_ASRC_GLOBAL_STATUS:
	case TEGRA186_ASRC_GLOBAL_STREAM_ENABLE_STATUS:
	case TEGRA186_ASRC_GLOBAL_INT_STATUS:
	case TEGRA186_ASRC_GLOBAL_INT_MASK:
	case TEGRA186_ASRC_GLOBAL_INT_SET:
	case TEGRA186_ASRC_GLOBAL_INT_CLEAR:
	case TEGRA186_ASRC_GLOBAL_TRANSFER_ERROR_LOG:
	case TEGRA186_ASRC_GLOBAL_APR_CTRL:
	case TEGRA186_ASRC_GLOBAL_APR_CTRL_ACCESS_CTRL:
	case TEGRA186_ASRC_GLOBAL_DISARM_APR:
	case TEGRA186_ASRC_GLOBAL_DISARM_APR_ACCESS_CTRL:
	case TEGRA186_ASRC_GLOBAL_RATIO_WR_ACCESS:
	case TEGRA186_ASRC_GLOBAL_RATIO_WR_ACCESS_CTRL:
	case TEGRA186_ASRC_CYA:
		return true;
	default:
		return false;
	};
}

static bool tegra186_asrc_volatile_reg(struct device *dev, unsigned int reg)
{
	if (reg < TEGRA186_ASRC_STREAM_LIMIT)
		reg %= TEGRA186_ASRC_STREAM_STRIDE;

	switch (reg) {
	case TEGRA186_ASRC_STREAM1_RX_STATUS:
	case TEGRA186_ASRC_STREAM1_TX_STATUS:
	case TEGRA186_ASRC_STREAM1_SOFT_RESET:
	case TEGRA186_ASRC_STREAM1_RATIO_INTEGER_PART:
	case TEGRA186_ASRC_STREAM1_RATIO_FRAC_PART:
	case TEGRA186_ASRC_STREAM1_STATUS:
	case TEGRA186_ASRC_STREAM1_BUFFER_STATUS:
	case TEGRA186_ASRC_STREAM1_CONFIG_ERR_TYPE:
	case TEGRA186_ASRC_STREAM1_RATIO_LOCK_STATUS:
	case TEGRA186_ASRC_RATIO_UPD_RX_STATUS:
	case TEGRA186_ASRC_GLOBAL_SOFT_RESET:
	case TEGRA186_ASRC_GLOBAL_STATUS:
	case TEGRA186_ASRC_GLOBAL_STREAM_ENABLE_STATUS:
	case TEGRA186_ASRC_GLOBAL_INT_STATUS:
	case TEGRA186_ASRC_GLOBAL_TRANSFER_ERROR_LOG:
		return true;
	default:
		return false;
	};
}

void tegra186_asrc_handle_arad_unlock(int stream_id, int action)
{
	struct tegra186_asrc *asrc = dev_get_drvdata(asrc_dev);
	int dcnt = 10;

	regmap_write(asrc->regmap, ASRC_STREAM_REG
			(TEGRA186_ASRC_STREAM1_ENABLE, stream_id), action);
	if (!action)
		udelay(2000);

	while ((tegra186_asrc_get_stream_enable_status(asrc,
					stream_id) != action) && dcnt--)
		udelay(100);
}

static const struct regmap_config tegra186_asrc_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = TEGRA186_ASRC_CYA,
	.writeable_reg = tegra186_asrc_wr_reg,
	.readable_reg = tegra186_asrc_rd_reg,
	.volatile_reg = tegra186_asrc_volatile_reg,
	.reg_defaults = tegra186_asrc_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(tegra186_asrc_reg_defaults),
	.cache_type = REGCACHE_FLAT,
};

#ifdef CONFIG_TEGRA186_AHC
static void tegra186_asrc_ahc_cb(void *data)
{
	struct device *dev = (struct device *)data;
	struct tegra186_asrc *asrc = dev_get_drvdata(dev);

	regcache_cache_bypass(asrc->regmap, true);
	regmap_write(asrc->regmap, TEGRA186_ASRC_GLOBAL_INT_CLEAR, 0x1);
	regmap_write(asrc->regmap, TEGRA186_ASRC_GLOBAL_ENB, 0x0);
	udelay(100);
	regmap_write(asrc->regmap, TEGRA186_ASRC_GLOBAL_ENB, 0x1);
	regcache_cache_bypass(asrc->regmap, false);
}
#endif

static const struct tegra186_asrc_soc_data soc_data_tegra186 = {
	.set_audio_cif = tegra210_xbar_set_cif,
};

static const struct of_device_id tegra186_asrc_of_match[] = {
	{ .compatible = "nvidia,tegra186-asrc", .data = &soc_data_tegra186 },
	{},
};
static int tegra186_asrc_platform_probe(struct platform_device *pdev)
{
	struct tegra186_asrc *asrc;
	struct resource *mem, *memregion;
	void __iomem *regs;
	int ret = 0;
	const struct of_device_id *match;
	struct tegra186_asrc_soc_data *soc_data;
	unsigned int i = 0;

	match = of_match_device(tegra186_asrc_of_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "Error: No device match found\n");
		ret = -ENODEV;
		goto err;
	}
	soc_data = (struct tegra186_asrc_soc_data *)match->data;
	asrc_dev = &pdev->dev;
	asrc = devm_kzalloc(&pdev->dev,
		sizeof(struct tegra186_asrc), GFP_KERNEL);
	if (!asrc) {
		dev_err(&pdev->dev, "Can't allocate asrc\n");
		ret = -ENOMEM;
		goto err;
	}

	asrc->soc_data = soc_data;

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

	asrc->regmap = devm_regmap_init_mmio(&pdev->dev, regs,
					    &tegra186_asrc_regmap_config);
	if (IS_ERR(asrc->regmap)) {
		dev_err(&pdev->dev, "regmap init failed\n");
		ret = PTR_ERR(asrc->regmap);
		goto err;
	}
	regcache_cache_only(asrc->regmap, true);

	if (of_property_read_u32(pdev->dev.of_node,
				"nvidia,ahub-asrc-id",
				&pdev->dev.id) < 0) {
		dev_err(&pdev->dev,
			"Missing property nvidia,ahub-asrc-id\n");
		ret = -ENODEV;
		goto err;
	}

#ifdef CONFIG_TEGRA186_AHC
	tegra186_ahc_register_cb(tegra186_asrc_ahc_cb,
			TEGRA186_AHC_ASRC1_CB, &pdev->dev);
#endif

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		ret = tegra186_asrc_runtime_resume(&pdev->dev);
		if (ret)
			goto err_pm_disable;
	}

	regmap_write(asrc->regmap, TEGRA186_ASRC_GLOBAL_CONFIG,
		TEGRA186_ASRC_GLOBAL_CONFIG_FRAC_32BIT_PRECISION);

	/* initialize default output srate */
	for (i = 0; i < 6; i++) {
		asrc->lane[i].int_part = 1;
		asrc->lane[i].frac_part = 0;
		asrc->lane[i].ratio_source = RATIO_SW;
		asrc->lane[i].hwcomp_disable = 0;
		asrc->lane[i].input_thresh =
			TEGRA186_ASRC_STREAM_DEFAULT_INPUT_HW_COMP_THRESH_CONFIG;
		asrc->lane[i].output_thresh =
			TEGRA186_ASRC_STREAM_DEFAULT_OUTPUT_HW_COMP_THRESH_CONFIG;
		regmap_update_bits(asrc->regmap,
			ASRC_STREAM_REG(TEGRA186_ASRC_STREAM1_CONFIG, i), 1, 1);
	}

	ret = snd_soc_register_codec(&pdev->dev, &tegra186_asrc_codec,
				     tegra186_asrc_dais,
				     ARRAY_SIZE(tegra186_asrc_dais));
	if (ret != 0) {
		dev_err(&pdev->dev, "Could not register CODEC: %d\n", ret);
		goto err_suspend;
	}

	dev_set_drvdata(&pdev->dev, asrc);

	return 0;

err_suspend:
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra186_asrc_runtime_suspend(&pdev->dev);
err_pm_disable:
	pm_runtime_disable(&pdev->dev);
err:
	return ret;
}

static int tegra186_asrc_platform_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra186_asrc_runtime_suspend(&pdev->dev);

	return 0;
}

static const struct dev_pm_ops tegra186_asrc_pm_ops = {
	SET_RUNTIME_PM_OPS(tegra186_asrc_runtime_suspend,
			   tegra186_asrc_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(tegra186_asrc_suspend, NULL)
};

static struct platform_driver tegra186_asrc_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = tegra186_asrc_of_match,
		.pm = &tegra186_asrc_pm_ops,
	},
	.probe = tegra186_asrc_platform_probe,
	.remove = tegra186_asrc_platform_remove,
};
module_platform_driver(tegra186_asrc_driver)

MODULE_AUTHOR("Junghyun Kim <juskim@nvidia.com>");
MODULE_DESCRIPTION("Tegra186 ASRC ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, tegra186_asrc_of_match);
