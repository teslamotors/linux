/*
 * tegra186_arad_alt.c - Tegra186 ARAD driver
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
#include <linux/of_device.h>
#include <linux/tegra186_ahc.h>

#include "tegra186_asrc_alt.h"
#include "tegra210_xbar_alt.h"
#include "tegra186_arad_alt.h"

#define DRV_NAME "tegra186-arad"

static struct device *arad_dev;

#define ARAD_LANE_NUMERATOR_MUX(id)	\
	(TEGRA186_ARAD_LANE1_NUMERATOR_MUX_SEL + id*TEGRA186_ARAD_LANE_STRIDE)
#define ARAD_LANE_DENOMINATOR_MUX(id)	\
	(TEGRA186_ARAD_LANE1_DENOMINATOR_MUX_SEL + id*TEGRA186_ARAD_LANE_STRIDE)

#define ARAD_LANE_REG(reg, id) (reg + (id * TEGRA186_ARAD_LANE_STRIDE))

#define ARAD_LANE_REG_DEFAULTS(id) \
	{ ARAD_LANE_REG(TEGRA186_ARAD_LANE1_NUMERATOR_MUX_SEL, id), 0x0}, \
	{ ARAD_LANE_REG(TEGRA186_ARAD_LANE1_RATIO_INTEGER_PART, id), 0x0}, \
	{ ARAD_LANE_REG(TEGRA186_ARAD_LANE1_RATIO_FRACTIONAL_PART, id), 0x0}, \
	{ ARAD_LANE_REG(TEGRA186_ARAD_LANE1_NUMERATOR_PRESCALAR, id), 0x0}, \
	{ ARAD_LANE_REG(TEGRA186_ARAD_LANE1_DENOMINATOR_MUX_SEL, id), 0x0}, \
	{ ARAD_LANE_REG(TEGRA186_ARAD_LANE1_DENOMINATOR_PRESCALAR, id), 0x0}, \
	{ ARAD_LANE_REG(TEGRA186_ARAD_LANE1_SERVO_LOOP_CONFIG, id), 0xd5e7}, \
	{ ARAD_LANE_REG(TEGRA186_ARAD_LANE1_LOCK_UNLOCK_DETECTOR_CONFIG, id), 0x840500}, \
	{ ARAD_LANE_REG(TEGRA186_ARAD_LANE1_ERROR_LOCK_THRESHOLD, id), 0x400000}, \
	{ ARAD_LANE_REG(TEGRA186_ARAD_LANE1_ERROR_UNLOCK_THRESHOLD, id), 0xa00000}, \
	{ ARAD_LANE_REG(TEGRA186_ARAD_LANE1_RATIO_CALCULATOR_CONFIG, id), 0xf000006}, \
	{ ARAD_LANE_REG(TEGRA186_ARAD_LANE1_CYA, id), 0x0}
static const struct reg_default tegra186_arad_reg_defaults[] = {
	{ TEGRA186_ARAD_LANE_ENABLE, 0x0},
	{ TEGRA186_ARAD_LANE_SOFT_RESET, 0x0},
	{ TEGRA186_ARAD_LANE_INT_MASK, 0x0},
	{ TEGRA186_ARAD_LANE_INT_SET, 0x0},
	{ TEGRA186_ARAD_LANE_INT_CLEAR, 0x0},
	{ TEGRA186_ARAD_LANE_INT_CLEAR, 0x0},
	{ TEGRA186_ARAD_CG, 0x0},
	{ TEGRA186_ARAD_CYA_GLOBAL, 0x0},

	ARAD_LANE_REG_DEFAULTS(0),
	ARAD_LANE_REG_DEFAULTS(1),
	ARAD_LANE_REG_DEFAULTS(2),
	ARAD_LANE_REG_DEFAULTS(3),
	ARAD_LANE_REG_DEFAULTS(4),
	ARAD_LANE_REG_DEFAULTS(5),

	{ TEGRA186_ARAD_TX_CIF_CTRL, 0x115500},
};
static int tegra186_arad_runtime_suspend(struct device *dev)
{
	struct tegra186_arad *arad = dev_get_drvdata(dev);

	regcache_cache_only(arad->regmap, true);
	regcache_mark_dirty(arad->regmap);

	pm_runtime_put_sync(dev->parent);

	return 0;
}

static int tegra186_arad_runtime_resume(struct device *dev)
{
	struct tegra186_arad *arad = dev_get_drvdata(dev);
	int ret;

	ret = pm_runtime_get_sync(dev->parent);
	if (ret < 0) {
		dev_err(dev, "parent get_sync failed: %d\n", ret);
		return ret;
	}

	regcache_cache_only(arad->regmap, false);
	regcache_sync(arad->regmap);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tegra186_arad_suspend(struct device *dev)
{
	return 0;
}
#endif

static int tegra186_arad_codec_probe(struct snd_soc_codec *codec)
{
	struct tegra186_arad *arad = snd_soc_codec_get_drvdata(codec);

	codec->control_data = arad->regmap;

	return 0;
}

static int tegra186_arad_get_lane_lock_status(
	struct tegra186_arad *arad, unsigned int lane_id)
{
	unsigned int val;

	regmap_read(arad->regmap,
		TEGRA186_ARAD_LANE_STATUS, &val);
	val = (val >> (16 + lane_id)) & 0x1;

	return val;
}

#if defined(CONFIG_SND_SOC_TEGRA186_ARAD_WAR) || defined(CONFIG_SND_SOC_TEGRA186_ASRC_WAR)
static int tegra186_arad_get_lane_ratio_change_status(
	struct tegra186_arad *arad, unsigned int lane_id)
{
	unsigned int val;

	regmap_read(arad->regmap,
		TEGRA186_ARAD_LANE_INT_STATUS, &val);
	val = (val >> (16 + lane_id)) & 0x1;

	return val;
}
#endif

static struct snd_soc_dai_ops tegra186_arad_out_dai_ops = {
};

static struct snd_soc_dai_driver tegra186_arad_dais[] = {
	{
		.name = "ARAD OUT",
		.capture = {
			.stream_name = "ARAD Transmit",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops = &tegra186_arad_out_dai_ops,
	},
};

static const int tegra186_arad_mux_value[] = {
	-1, /* None */
	0, 1, 2, 3, 4, 5,	/* I2S1~6 */
	12, 13, 14, 15,	/* DMIC1~4 */
	24, 25,	/* DSPK1~2 */
	26, 27,	/* IQC1~2 */
	28, 29, 30, 31,	/* SPDIF_RX1,2 & SPDIF_TX1,2 */
};

static const char * const tegra186_arad_mux_text[] = {
	"None",
	"I2S1",
	"I2S2",
	"I2S3",
	"I2S4",
	"I2S5",
	"I2S6",
	"DMIC1",
	"DMIC2",
	"DMIC3",
	"DMIC4",
	"DSPK1",
	"DSPK2",
	"IQC1",
	"IQC2",
	"SPDIF1_RX1",
	"SPDIF1_RX2",
	"SPDIF1_TX1",
	"SPDIF1_TX2",
};

static int tegra186_arad_mux_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *arad_private =
		(struct soc_enum  *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra186_arad *arad = snd_soc_codec_get_drvdata(codec);
	unsigned int val, loop = 0;

	regmap_read(arad->regmap, arad_private->reg, &val);

	if (val) {
		for (loop = 1; loop < 19; loop++)
			if (val & (1<<arad_private->values[loop])) {
				val = loop;
				break;
			}
	}

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int tegra186_arad_mux_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{

	struct soc_enum *arad_private =
		(struct soc_enum  *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra186_arad *arad = snd_soc_codec_get_drvdata(codec);
	unsigned int val = ucontrol->value.integer.value[0];
	if (!val)
		regmap_write(arad->regmap, arad_private->reg, 0);
	else
		regmap_write(arad->regmap, arad_private->reg,
			1 << arad_private->values[val]);

	return 0;
}

static int tegra186_arad_get_ratio_int(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *arad_private =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra186_arad *arad = snd_soc_codec_get_drvdata(codec);
	unsigned int val;

	regmap_read(arad->regmap, arad_private->reg, &val);
	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int tegra186_arad_get_ratio_frac(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *arad_private =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra186_arad *arad = snd_soc_codec_get_drvdata(codec);
	unsigned int val;

	regmap_read(arad->regmap, arad_private->reg, &val);
	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int tegra186_arad_get_enable_lane(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *arad_private =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra186_arad *arad = snd_soc_codec_get_drvdata(codec);
	unsigned int enable;

	regmap_read(arad->regmap, arad_private->reg, &enable);
	enable = (enable >> arad_private->shift) & arad_private->max;
	ucontrol->value.integer.value[0] = enable;

	return 0;
}

static int tegra186_arad_put_enable_lane(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *arad_private =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct device *dev = codec->dev;
	struct tegra186_arad *arad = snd_soc_codec_get_drvdata(codec);
	unsigned int enable = 0, lane_id = arad_private->shift, state;
	int dcnt = 10;

	pm_runtime_get_sync(dev);

	regmap_read(arad->regmap,
		TEGRA186_ARAD_LANE_STATUS, &state);
	enable = ucontrol->value.integer.value[0];

	/* Return, if current state and requested state of lane is disable */
	if (!enable && !((state >> lane_id) & 1)) {
		pm_runtime_put_sync(dev);
		return 0;
	}

	regmap_update_bits(arad->regmap,
		arad_private->reg, 1<<arad_private->shift,
		enable<<arad_private->shift);

	if (enable)
		while (!tegra186_arad_get_lane_lock_status(arad, lane_id) &&
			dcnt--)
			udelay(10000);
	else {
		regmap_update_bits(arad->regmap,
			TEGRA186_ARAD_LANE_SOFT_RESET, 1<<arad_private->shift,
			1<<arad_private->shift);
		while (tegra186_arad_get_lane_lock_status(arad, lane_id) &&
			dcnt--)
			udelay(100);
	}
	pm_runtime_put_sync(dev);

	if (dcnt < 0) {
		if (enable)
			pr_err("ARAD Lane %d can't be locked\n", lane_id+1);
		else
			pr_err("ARAD Lane %d can't be unlocked\n", lane_id+1);
		return -ETIMEDOUT;
	} else
		return 0;
}
static int tegra186_arad_tx_stop(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct device *dev = codec->dev;
	struct tegra186_arad *arad = dev_get_drvdata(dev);

	regmap_write(arad->regmap, TEGRA186_ARAD_LANE_ENABLE, 0);
	regmap_write(arad->regmap, TEGRA186_ARAD_GLOBAL_SOFT_RESET, 1);
	return 0;
}
static const struct snd_soc_dapm_widget tegra186_arad_widgets[] = {
	SND_SOC_DAPM_SIGGEN("Lane1 SIG"),
	SND_SOC_DAPM_SIGGEN("Lane2 SIG"),
	SND_SOC_DAPM_SIGGEN("Lane3 SIG"),
	SND_SOC_DAPM_SIGGEN("Lane4 SIG"),
	SND_SOC_DAPM_SIGGEN("Lane5 SIG"),
	SND_SOC_DAPM_SIGGEN("Lane6 SIG"),

	SND_SOC_DAPM_AIF_OUT_E("Packetizer", NULL, 0, SND_SOC_NOPM, 0, 0,
		tegra186_arad_tx_stop, SND_SOC_DAPM_PRE_PMD),
};

static const struct snd_soc_dapm_route tegra186_arad_routes[] = {

	{ "Packetizer",    NULL, "Lane1 SIG"},
	{ "Packetizer",    NULL, "Lane2 SIG"},
	{ "Packetizer",    NULL, "Lane3 SIG"},
	{ "Packetizer",    NULL, "Lane4 SIG"},
	{ "Packetizer",    NULL, "Lane5 SIG"},
	{ "Packetizer",    NULL, "Lane6 SIG"},
	{ "ARAD Transmit", NULL, "Packetizer" },
};

static int tegra186_arad_get_prescalar(struct snd_kcontrol *kcontrol,
		    struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *arad_private =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra186_arad *arad = snd_soc_codec_get_drvdata(codec);
	unsigned int reg = arad_private->reg;
	unsigned int val = 0;

	regmap_read(arad->regmap, reg, &val);
	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int tegra186_arad_put_prescalar(struct snd_kcontrol *kcontrol,
		    struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *arad_private =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra186_arad *arad = snd_soc_codec_get_drvdata(codec);
	unsigned int reg = arad_private->reg;

	regmap_write(arad->regmap, reg, ucontrol->value.integer.value[0]);

	return 0;
}

#define SOC_VALUE_ENUM_WIDE(xreg, shift, xmax, xtexts, xvalues) \
{	.reg = xreg, .shift_l = shift, .shift_r = shift, \
	.items = xmax, .texts = xtexts, .values = xvalues, \
	.mask = xmax ? roundup_pow_of_two(xmax) - 1 : 0}

#define SOC_VALUE_ENUM_WIDE_DECL(name, xreg, shift, \
		xtexts, xvalues) \
	struct soc_enum name = SOC_VALUE_ENUM_WIDE(xreg, shift, \
					ARRAY_SIZE(xtexts), xtexts, xvalues)

#define ARAD_MUX_ENUM_CTRL_DECL(ename, reg)             \
	SOC_VALUE_ENUM_WIDE_DECL(ename##_enum, reg, 0,  \
		tegra186_arad_mux_text, tegra186_arad_mux_value);

static ARAD_MUX_ENUM_CTRL_DECL(numerator1,
		TEGRA186_ARAD_LANE1_NUMERATOR_MUX_SEL);
static ARAD_MUX_ENUM_CTRL_DECL(numerator2,
		TEGRA186_ARAD_LANE2_NUMERATOR_MUX_SEL);
static ARAD_MUX_ENUM_CTRL_DECL(numerator3,
		TEGRA186_ARAD_LANE3_NUMERATOR_MUX_SEL);
static ARAD_MUX_ENUM_CTRL_DECL(numerator4,
		TEGRA186_ARAD_LANE4_NUMERATOR_MUX_SEL);
static ARAD_MUX_ENUM_CTRL_DECL(numerator5,
		TEGRA186_ARAD_LANE5_NUMERATOR_MUX_SEL);
static ARAD_MUX_ENUM_CTRL_DECL(numerator6,
		TEGRA186_ARAD_LANE6_NUMERATOR_MUX_SEL);

static ARAD_MUX_ENUM_CTRL_DECL(denominator1,
		TEGRA186_ARAD_LANE1_DENOMINATOR_MUX_SEL);
static ARAD_MUX_ENUM_CTRL_DECL(denominator2,
		TEGRA186_ARAD_LANE2_DENOMINATOR_MUX_SEL);
static ARAD_MUX_ENUM_CTRL_DECL(denominator3,
		TEGRA186_ARAD_LANE3_DENOMINATOR_MUX_SEL);
static ARAD_MUX_ENUM_CTRL_DECL(denominator4,
		TEGRA186_ARAD_LANE4_DENOMINATOR_MUX_SEL);
static ARAD_MUX_ENUM_CTRL_DECL(denominator5,
		TEGRA186_ARAD_LANE5_DENOMINATOR_MUX_SEL);
static ARAD_MUX_ENUM_CTRL_DECL(denominator6,
		TEGRA186_ARAD_LANE6_DENOMINATOR_MUX_SEL);

#define ARAD_NUMERATOR_PRESCALAR(id) \
	SOC_SINGLE_EXT("Numerator"#id" Prescalar", \
		TEGRA186_ARAD_LANE1_NUMERATOR_PRESCALAR + (id-1)*TEGRA186_ARAD_LANE_STRIDE, \
		0, 0xffff, 0, tegra186_arad_get_prescalar, \
		tegra186_arad_put_prescalar)

#define ARAD_DENOMINATOR_PRESCALAR(id) \
	SOC_SINGLE_EXT("Denominator"#id" Prescalar", \
		TEGRA186_ARAD_LANE1_DENOMINATOR_PRESCALAR + (id-1)*TEGRA186_ARAD_LANE_STRIDE, \
		0, 0xffff, 0, tegra186_arad_get_prescalar, \
		tegra186_arad_put_prescalar)

static const struct snd_kcontrol_new tegra186_arad_controls[] = {
	SOC_SINGLE_EXT("Lane1 enable", TEGRA186_ARAD_LANE_ENABLE, 0, 1, 0,
		tegra186_arad_get_enable_lane, tegra186_arad_put_enable_lane),
	SOC_SINGLE_EXT("Lane2 enable", TEGRA186_ARAD_LANE_ENABLE, 1, 1, 0,
		tegra186_arad_get_enable_lane, tegra186_arad_put_enable_lane),
	SOC_SINGLE_EXT("Lane3 enable", TEGRA186_ARAD_LANE_ENABLE, 2, 1, 0,
		tegra186_arad_get_enable_lane, tegra186_arad_put_enable_lane),
	SOC_SINGLE_EXT("Lane4 enable", TEGRA186_ARAD_LANE_ENABLE, 3, 1, 0,
		tegra186_arad_get_enable_lane, tegra186_arad_put_enable_lane),
	SOC_SINGLE_EXT("Lane5 enable", TEGRA186_ARAD_LANE_ENABLE, 4, 1, 0,
		tegra186_arad_get_enable_lane, tegra186_arad_put_enable_lane),
	SOC_SINGLE_EXT("Lane6 enable", TEGRA186_ARAD_LANE_ENABLE, 5, 1, 0,
		tegra186_arad_get_enable_lane, tegra186_arad_put_enable_lane),

	SOC_ENUM_EXT("Numerator1 Mux", numerator1_enum,
		tegra186_arad_mux_get, tegra186_arad_mux_put),
	SOC_ENUM_EXT("Numerator2 Mux", numerator2_enum,
		tegra186_arad_mux_get, tegra186_arad_mux_put),
	SOC_ENUM_EXT("Numerator3 Mux", numerator3_enum,
		tegra186_arad_mux_get, tegra186_arad_mux_put),
	SOC_ENUM_EXT("Numerator4 Mux", numerator4_enum,
		tegra186_arad_mux_get, tegra186_arad_mux_put),
	SOC_ENUM_EXT("Numerator5 Mux", numerator5_enum,
		tegra186_arad_mux_get, tegra186_arad_mux_put),
	SOC_ENUM_EXT("Numerator6 Mux", numerator6_enum,
		tegra186_arad_mux_get, tegra186_arad_mux_put),


	SOC_ENUM_EXT("Denominator1 Mux", denominator1_enum,
		tegra186_arad_mux_get, tegra186_arad_mux_put),
	SOC_ENUM_EXT("Denominator2 Mux", denominator2_enum,
		tegra186_arad_mux_get, tegra186_arad_mux_put),
	SOC_ENUM_EXT("Denominator3 Mux", denominator3_enum,
		tegra186_arad_mux_get, tegra186_arad_mux_put),
	SOC_ENUM_EXT("Denominator4 Mux", denominator4_enum,
		tegra186_arad_mux_get, tegra186_arad_mux_put),
	SOC_ENUM_EXT("Denominator5 Mux", denominator5_enum,
		tegra186_arad_mux_get, tegra186_arad_mux_put),
	SOC_ENUM_EXT("Denominator6 Mux", denominator6_enum,
		tegra186_arad_mux_get, tegra186_arad_mux_put),

	SOC_SINGLE_EXT("Lane1 Ratio Int",
		TEGRA186_ARAD_LANE1_RATIO_INTEGER_PART,
		0, TEGRA186_ARAD_LANE_RATIO_INTEGER_PART_MASK, 0,
		tegra186_arad_get_ratio_int, NULL),
	SOC_SINGLE_EXT("Lane1 Ratio Frac",
		TEGRA186_ARAD_LANE1_RATIO_FRACTIONAL_PART,
		0, TEGRA186_ARAD_LANE_RATIO_FRAC_PART_MASK, 0,
		tegra186_arad_get_ratio_frac, NULL),
	SOC_SINGLE_EXT("Lane2 Ratio Int",
		TEGRA186_ARAD_LANE2_RATIO_INTEGER_PART,
		0, TEGRA186_ARAD_LANE_RATIO_INTEGER_PART_MASK, 0,
		tegra186_arad_get_ratio_int, NULL),
	SOC_SINGLE_EXT("Lane2 Ratio Frac",
		TEGRA186_ARAD_LANE2_RATIO_FRACTIONAL_PART,
		0, TEGRA186_ARAD_LANE_RATIO_FRAC_PART_MASK, 0,
		tegra186_arad_get_ratio_frac, NULL),
	SOC_SINGLE_EXT("Lane3 Ratio Int",
		TEGRA186_ARAD_LANE3_RATIO_INTEGER_PART,
		0, TEGRA186_ARAD_LANE_RATIO_INTEGER_PART_MASK, 0,
		tegra186_arad_get_ratio_int, NULL),
	SOC_SINGLE_EXT("Lane3 Ratio Frac",
		TEGRA186_ARAD_LANE3_RATIO_FRACTIONAL_PART,
		0, TEGRA186_ARAD_LANE_RATIO_FRAC_PART_MASK, 0,
		tegra186_arad_get_ratio_frac, NULL),
	SOC_SINGLE_EXT("Lane4 Ratio Int",
		TEGRA186_ARAD_LANE4_RATIO_INTEGER_PART,
		0, TEGRA186_ARAD_LANE_RATIO_INTEGER_PART_MASK, 0,
		tegra186_arad_get_ratio_int, NULL),
	SOC_SINGLE_EXT("Lane4 Ratio Frac",
		TEGRA186_ARAD_LANE4_RATIO_FRACTIONAL_PART,
		0, TEGRA186_ARAD_LANE_RATIO_FRAC_PART_MASK, 0,
		tegra186_arad_get_ratio_frac, NULL),
	SOC_SINGLE_EXT("Lane5 Ratio Int",
		TEGRA186_ARAD_LANE5_RATIO_INTEGER_PART,
		0, TEGRA186_ARAD_LANE_RATIO_INTEGER_PART_MASK, 0,
		tegra186_arad_get_ratio_int, NULL),
	SOC_SINGLE_EXT("Lane5 Ratio Frac",
		TEGRA186_ARAD_LANE5_RATIO_FRACTIONAL_PART,
		0, TEGRA186_ARAD_LANE_RATIO_FRAC_PART_MASK, 0,
		tegra186_arad_get_ratio_frac, NULL),
	SOC_SINGLE_EXT("Lane6 Ratio Int",
		TEGRA186_ARAD_LANE6_RATIO_INTEGER_PART,
		0, TEGRA186_ARAD_LANE_RATIO_INTEGER_PART_MASK, 0,
		tegra186_arad_get_ratio_int, NULL),
	SOC_SINGLE_EXT("Lane6 Ratio Frac",
		TEGRA186_ARAD_LANE6_RATIO_FRACTIONAL_PART,
		0, TEGRA186_ARAD_LANE_RATIO_FRAC_PART_MASK, 0,
		tegra186_arad_get_ratio_frac, NULL),

	ARAD_NUMERATOR_PRESCALAR(1),
	ARAD_NUMERATOR_PRESCALAR(2),
	ARAD_NUMERATOR_PRESCALAR(3),
	ARAD_NUMERATOR_PRESCALAR(4),
	ARAD_NUMERATOR_PRESCALAR(5),
	ARAD_NUMERATOR_PRESCALAR(6),

	ARAD_DENOMINATOR_PRESCALAR(1),
	ARAD_DENOMINATOR_PRESCALAR(2),
	ARAD_DENOMINATOR_PRESCALAR(3),
	ARAD_DENOMINATOR_PRESCALAR(4),
	ARAD_DENOMINATOR_PRESCALAR(5),
	ARAD_DENOMINATOR_PRESCALAR(6),
};

void tegra186_arad_send_ratio(void)
{
	struct tegra186_arad *arad = dev_get_drvdata(arad_dev);

	if (!arad)
		return;
	pm_runtime_get_sync(arad_dev);
	regmap_write(arad->regmap, TEGRA186_ARAD_SEND_RATIO, 0x1);
	pm_runtime_put(arad_dev);
}
EXPORT_SYMBOL(tegra186_arad_send_ratio);

static struct snd_soc_codec_driver tegra186_arad_codec = {
	.probe = tegra186_arad_codec_probe,
	.dapm_widgets = tegra186_arad_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tegra186_arad_widgets),
	.dapm_routes = tegra186_arad_routes,
	.num_dapm_routes = ARRAY_SIZE(tegra186_arad_routes),
	.controls = tegra186_arad_controls,
	.num_controls = ARRAY_SIZE(tegra186_arad_controls),
	.idle_bias_off = 1,
};

static bool tegra186_arad_wr_reg(struct device *dev, unsigned int reg)
{
	if ((reg >= TEGRA186_ARAD_LANE_START) &&
		(reg <= TEGRA186_ARAD_LANE_LIMIT)) {
		reg -= TEGRA186_ARAD_LANE_START;
		reg %= TEGRA186_ARAD_LANE_STRIDE;
		reg += TEGRA186_ARAD_LANE_START;
	}

	switch (reg) {
	case TEGRA186_ARAD_LANE_ENABLE:
	case TEGRA186_ARAD_LANE_SOFT_RESET:
	case TEGRA186_ARAD_LANE_INT_MASK:
	case TEGRA186_ARAD_LANE_INT_SET:
	case TEGRA186_ARAD_LANE_INT_CLEAR:
	case TEGRA186_ARAD_GLOBAL_SOFT_RESET:
	case TEGRA186_ARAD_SEND_RATIO:
	case TEGRA186_ARAD_CG:
	case TEGRA186_ARAD_CYA_GLOBAL:

	case TEGRA186_ARAD_LANE1_NUMERATOR_MUX_SEL:
	case TEGRA186_ARAD_LANE1_NUMERATOR_PRESCALAR:
	case TEGRA186_ARAD_LANE1_DENOMINATOR_MUX_SEL:
	case TEGRA186_ARAD_LANE1_DENOMINATOR_PRESCALAR:
	case TEGRA186_ARAD_LANE1_SERVO_LOOP_CONFIG:
	case TEGRA186_ARAD_LANE1_LOCK_UNLOCK_DETECTOR_CONFIG:
	case TEGRA186_ARAD_LANE1_ERROR_LOCK_THRESHOLD:
	case TEGRA186_ARAD_LANE1_ERROR_UNLOCK_THRESHOLD:
	case TEGRA186_ARAD_LANE1_RATIO_CALCULATOR_CONFIG:
	case TEGRA186_ARAD_LANE1_CYA:
		return true;
	default:
		return false;
	};
}

static bool tegra186_arad_rd_reg(struct device *dev, unsigned int reg)
{
	if ((reg >= TEGRA186_ARAD_LANE_START) &&
		(reg <= TEGRA186_ARAD_LANE_LIMIT)) {
		reg -= TEGRA186_ARAD_LANE_START;
		reg %= TEGRA186_ARAD_LANE_STRIDE;
		reg += TEGRA186_ARAD_LANE_START;
	}

	switch (reg) {
	case TEGRA186_ARAD_LANE_ENABLE:
	case TEGRA186_ARAD_LANE_STATUS:
	case TEGRA186_ARAD_LANE_SOFT_RESET:
	case TEGRA186_ARAD_LANE_INT_STATUS:
	case TEGRA186_ARAD_LANE_INT_MASK:
	case TEGRA186_ARAD_LANE_INT_SET:
	case TEGRA186_ARAD_LANE_INT_CLEAR:
	case TEGRA186_ARAD_GLOBAL_SOFT_RESET:
	case TEGRA186_ARAD_CG:
	case TEGRA186_ARAD_STATUS:
	case TEGRA186_ARAD_CYA_GLOBAL:

	case TEGRA186_ARAD_LANE1_NUMERATOR_MUX_SEL:
	case TEGRA186_ARAD_LANE1_NUMERATOR_PRESCALAR:
	case TEGRA186_ARAD_LANE1_DENOMINATOR_MUX_SEL:
	case TEGRA186_ARAD_LANE1_DENOMINATOR_PRESCALAR:
	case TEGRA186_ARAD_LANE1_RATIO_INTEGER_PART:
	case TEGRA186_ARAD_LANE1_RATIO_FRACTIONAL_PART:
	case TEGRA186_ARAD_LANE1_PERIOD_COUNT:
	case TEGRA186_ARAD_LANE1_SERVO_LOOP_CONFIG:
	case TEGRA186_ARAD_LANE1_LOCK_UNLOCK_DETECTOR_CONFIG:
	case TEGRA186_ARAD_LANE1_ERROR_LOCK_THRESHOLD:
	case TEGRA186_ARAD_LANE1_ERROR_UNLOCK_THRESHOLD:
	case TEGRA186_ARAD_LANE1_RATIO_CALCULATOR_CONFIG:
	case TEGRA186_ARAD_LANE1_CYA:
		return true;
	default:
		return false;
	};
}

static bool tegra186_arad_volatile_reg(struct device *dev, unsigned int reg)
{
	if ((reg >= TEGRA186_ARAD_LANE_START) &&
		(reg <= TEGRA186_ARAD_LANE_LIMIT)) {
		reg -= TEGRA186_ARAD_LANE_START;
		reg %= TEGRA186_ARAD_LANE_STRIDE;
		reg += TEGRA186_ARAD_LANE_START;
	}

	switch (reg) {
	case TEGRA186_ARAD_LANE_ENABLE:
	case TEGRA186_ARAD_LANE_STATUS:
	case TEGRA186_ARAD_LANE_INT_STATUS:
	case TEGRA186_ARAD_STATUS:

	case TEGRA186_ARAD_LANE1_RATIO_INTEGER_PART:
	case TEGRA186_ARAD_LANE1_RATIO_FRACTIONAL_PART:
	case TEGRA186_ARAD_LANE1_PERIOD_COUNT:
		return true;
	default:
		return false;
	};
}

static const struct regmap_config tegra186_arad_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = TEGRA186_ARAD_TX_CIF_CTRL,
	.writeable_reg = tegra186_arad_wr_reg,
	.readable_reg = tegra186_arad_rd_reg,
	.volatile_reg = tegra186_arad_volatile_reg,
	.reg_defaults = tegra186_arad_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(tegra186_arad_reg_defaults),
	.cache_type = REGCACHE_FLAT,
};

static const struct tegra186_arad_soc_data soc_data_tegra186 = {
	.set_audio_cif = tegra210_xbar_set_cif,
};

static const struct of_device_id tegra186_arad_of_match[] = {
	{ .compatible = "nvidia,tegra186-arad", .data = &soc_data_tegra186 },
	{},
};

#ifdef CONFIG_SND_SOC_TEGRA186_ARAD_WAR
static void tegra186_arad_ahc_cb(void *data)
{
	int i = 0;
	unsigned int val = 0, status;
	struct tegra186_arad *arad = dev_get_drvdata(
					(struct device *) data);
	/* WAR for Bug 200102368 */
	regmap_read(arad->regmap,
		TEGRA186_ARAD_LANE_INT_STATUS, &status);
	for (i = 0; i < TEGRA186_ARAD_LANE_MAX; i++) {
		if ((1 << i) & status) {
			/*Its a case where interrupt is because the lane
			is unlocked */
			regmap_read(arad->regmap,
				TEGRA186_ARAD_LANE_ENABLE, &val);
			val |= 1<<i;
			regmap_write(arad->regmap,
				TEGRA186_ARAD_LANE_INT_CLEAR, 1<<i);
			regmap_write(arad->regmap,
				TEGRA186_ARAD_LANE_SOFT_RESET, 1<<i);
			regmap_write(arad->regmap,
				TEGRA186_ARAD_LANE_ENABLE, val);
			val = 0;
		} else if (tegra186_arad_get_lane_ratio_change_status(arad, i)) {
			/*Its a case where interrupt is because of
			ratio change, clear the interrupt*/
			regmap_write(arad->regmap,
				TEGRA186_ARAD_LANE_INT_CLEAR,
				1<<(TEGRA186_ARAD_LANE_INT_RATIO_CHANGE_SHIFT+i));
		}
	}
	spin_lock(&arad->status_lock);
	arad->int_status = status;
	spin_unlock(&arad->status_lock);
}

#ifdef CONFIG_SND_SOC_TEGRA186_ASRC_WAR
void tegra186_arad_ahc_deferred_cb(void *data)
{
	struct tegra186_arad *arad = dev_get_drvdata(
									(struct device *)data);
	int i = 0, inte = 0, frac = 0;
	unsigned int status;

	spin_lock(&arad->status_lock);
	status = &arad->int_status;
	spin_unlock(&arad->status_lock);

	for (i = 0; i < TEGRA186_ARAD_LANE_MAX; i++) {
		if ((1 << i) & status)
			tegra186_asrc_handle_arad_unlock(i, 0);
		else if (tegra186_arad_get_lane_lock_status(arad, i) &&
				tegra186_arad_get_lane_ratio_change_status(arad, i)) {
			regmap_read(arad->regmap, ARAD_LANE_REG(
				TEGRA186_ARAD_LANE1_RATIO_INTEGER_PART,
				i), &inte);
			regmap_read(arad->regmap, ARAD_LANE_REG(
				TEGRA186_ARAD_LANE1_RATIO_FRACTIONAL_PART,
				i), &frac);

			/* source SW:1 and ARAD:0 */
			tegra186_asrc_set_source(i, 1);
			tegra186_asrc_update_ratio(i, inte, frac);
			tegra186_asrc_set_source(i, 0);
			tegra186_asrc_handle_arad_unlock(i, 1);
		}
	}
}
#endif
#endif

static int tegra186_arad_platform_probe(struct platform_device *pdev)
{
	struct tegra186_arad *arad;
	struct resource *mem, *memregion;
	void __iomem *regs;
	int ret = 0;
	const struct of_device_id *match;
	struct tegra186_arad_soc_data *soc_data;

	match = of_match_device(tegra186_arad_of_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "Error: No device match found\n");
		ret = -ENODEV;
		goto err;
	}
	soc_data = (struct tegra186_arad_soc_data *)match->data;

	arad = devm_kzalloc(&pdev->dev,
		sizeof(struct tegra186_arad), GFP_KERNEL);
	if (!arad) {
		dev_err(&pdev->dev, "Can't allocate tegra210_arad\n");
		ret = -ENOMEM;
		goto err;
	}
	arad_dev = &pdev->dev;

	arad->soc_data = soc_data;

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

	arad->regmap = devm_regmap_init_mmio(&pdev->dev, regs,
					    &tegra186_arad_regmap_config);
	if (IS_ERR(arad->regmap)) {
		dev_err(&pdev->dev, "regmap init failed\n");
		ret = PTR_ERR(arad->regmap);
		goto err;
	}
	regcache_cache_only(arad->regmap, true);

	if (of_property_read_u32(pdev->dev.of_node,
				"nvidia,ahub-arad-id",
				&pdev->dev.id) < 0) {
		dev_err(&pdev->dev,
			"Missing property nvidia,ahub-arad-id\n");
		ret = -ENODEV;
		goto err;
	}

	pm_runtime_enable(&pdev->dev);

	ret = snd_soc_register_codec(&pdev->dev, &tegra186_arad_codec,
				     tegra186_arad_dais,
				     ARRAY_SIZE(tegra186_arad_dais));
	if (ret != 0) {
		dev_err(&pdev->dev, "Could not register CODEC: %d\n", ret);
		goto err_suspend;
	}

#ifdef CONFIG_SND_SOC_TEGRA186_ARAD_WAR
	spin_lock_init(&arad->status_lock);
	tegra186_ahc_register_cb(tegra186_arad_ahc_cb,
			TEGRA186_AHC_ARAD1_CB, &pdev->dev);
#ifdef CONFIG_SND_SOC_TEGRA186_ASRC_WAR
	tegra186_ahc_register_deferred_cb(tegra186_arad_ahc_deferred_cb,
			TEGRA186_AHC_ARAD1_CB, &pdev->dev);
#endif
#endif
	dev_set_drvdata(&pdev->dev, arad);

	return 0;

err_suspend:
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra186_arad_runtime_suspend(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
err:
	return ret;
}

static int tegra186_arad_platform_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra186_arad_runtime_suspend(&pdev->dev);

	return 0;
}

static const struct dev_pm_ops tegra186_arad_pm_ops = {
	SET_RUNTIME_PM_OPS(tegra186_arad_runtime_suspend,
			   tegra186_arad_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(tegra186_arad_suspend, NULL)
};

static struct platform_driver tegra186_arad_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = tegra186_arad_of_match,
		.pm = &tegra186_arad_pm_ops,
	},
	.probe = tegra186_arad_platform_probe,
	.remove = tegra186_arad_platform_remove,
};
module_platform_driver(tegra186_arad_driver)

MODULE_AUTHOR("Junghyun Kim <juskim@nvidia.com>");
MODULE_DESCRIPTION("Tegra186 ARAD ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, tegra186_arad_of_match);
