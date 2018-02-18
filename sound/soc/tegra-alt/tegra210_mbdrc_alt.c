/*
 * tegra210_mbdrc_alt.c - Tegra210 MBDRC driver
 *
 * Copyright (c) 2014 NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/soc.h>

#include "tegra210_xbar_alt.h"
#include "tegra210_ope_alt.h"
#include "tegra210_mbdrc_alt.h"

#define MBDRC_FILTER_REG(reg, id) \
	(reg + (id * TEGRA210_MBDRC_FILTER_PARAM_STRIDE))
#define MBDRC_FILTER_REG_DEFAULTS(id) \
	{ MBDRC_FILTER_REG(TEGRA210_MBDRC_IIR_CONFIG, id), 0x00000005}, \
	{ MBDRC_FILTER_REG(TEGRA210_MBDRC_IN_ATTACK, id), 0x3e48590c}, \
	{ MBDRC_FILTER_REG(TEGRA210_MBDRC_IN_RELEASE, id), 0x08414e9f}, \
	{ MBDRC_FILTER_REG(TEGRA210_MBDRC_FAST_ATTACK, id), 0x7fffffff}, \
	{ MBDRC_FILTER_REG(TEGRA210_MBDRC_IN_THRESHOLD, id), 0x06145082}, \
	{ MBDRC_FILTER_REG(TEGRA210_MBDRC_OUT_THRESHOLD, id), 0x060d379b}, \
	{ MBDRC_FILTER_REG(TEGRA210_MBDRC_RATIO_1ST, id), 0x0000a000}, \
	{ MBDRC_FILTER_REG(TEGRA210_MBDRC_RATIO_2ND, id), 0x00002000}, \
	{ MBDRC_FILTER_REG(TEGRA210_MBDRC_RATIO_3RD, id), 0x00000b33}, \
	{ MBDRC_FILTER_REG(TEGRA210_MBDRC_RATIO_4TH, id), 0x00000800}, \
	{ MBDRC_FILTER_REG(TEGRA210_MBDRC_RATIO_5TH, id), 0x0000019a}, \
	{ MBDRC_FILTER_REG(TEGRA210_MBDRC_MAKEUP_GAIN, id), 0x00000002}, \
	{ MBDRC_FILTER_REG(TEGRA210_MBDRC_INIT_GAIN, id), 0x00066666}, \
	{ MBDRC_FILTER_REG(TEGRA210_MBDRC_GAIN_ATTACK, id), 0x00d9ba0e}, \
	{ MBDRC_FILTER_REG(TEGRA210_MBDRC_GAIN_RELEASE, id), 0x3e48590c}, \
	{ MBDRC_FILTER_REG(TEGRA210_MBDRC_FAST_RELEASE, id), 0x7ffff26a}, \
	{ MBDRC_FILTER_REG(TEGRA210_MBDRC_AHUBRAMCTL_CONFIG_RAM_CTRL, id), 0x4000}

static const struct reg_default tegra210_mbdrc_reg_defaults[] = {
	{ TEGRA210_MBDRC_CONFIG, 0x0030de51},
	{ TEGRA210_MBDRC_CHANNEL_MASK, 0x00000003},
	{ TEGRA210_MBDRC_FAST_FACTOR, 0x30000800},

	MBDRC_FILTER_REG_DEFAULTS(0),
	MBDRC_FILTER_REG_DEFAULTS(1),
	MBDRC_FILTER_REG_DEFAULTS(2)
};

/* Default MBDRC parameters */
static const struct tegra210_mbdrc_config mbdrc_init_config = {
	.mode = 0, /* bypass */
	.rms_off = 48,
	.peak_rms_mode = 1, /* PEAK */
	.fliter_structure = 0, /* All-pass tree */
	.shift_ctrl = 30,
	.frame_size = 32,
	.channel_mask = 0x3,
	.fa_factor = 2048,
	.fr_factor = 14747,
	.band_params[MBDRC_LOW_BAND] = {
		.band = MBDRC_LOW_BAND,
		.iir_stages = 5,
		.in_attack_tc = 1044928780,
		.in_release_tc = 138497695,
		.fast_attack_tc = 2147483647,
		.in_threshold = {130, 80, 20, 6},
		.out_threshold = {155, 55, 13, 6},
		.ratio = {40960, 8192, 2867, 2048, 410},
		.makeup_gain = 4,
		.gain_init = 419430,
		.gain_attack_tc = 14268942,
		.gain_release_tc = 1440547090,
		.fast_release_tc = 2147480170,
		.biquad_params = {
			/* Gains : b0, b1, a0, a1, a2 */
			961046798, -2030431983, 1073741824, 2030431983, -961046798, /* band-0 */
			1030244425, -2099481453, 1073741824, 2099481453, -1030244425, /* band-1 */
			1067169294, -2136327263, 1073741824, 2136327263, -1067169294, /* band-2 */
			434951949, -1306567134, 1073741824, 1306567134, -434951949, /* band-3 */
			780656019, -1605955641, 1073741824, 1605955641, -780656019, /* band-4 */
			1024497031, -1817128152, 1073741824, 1817128152, -1024497031, /* band-5 */
			1073741824, 0, 0, 0, 0, /* band-6 */
			1073741824, 0, 0, 0, 0, /* band-7 */
		}
	},
	.band_params[MBDRC_MID_BAND] = {
		.band = MBDRC_MID_BAND,
		.iir_stages = 5,
		.in_attack_tc = 1581413104,
		.in_release_tc = 35494783,
		.fast_attack_tc = 2147483647,
		.in_threshold = {130, 50, 30, 6},
		.out_threshold = {106, 50, 30, 13},
		.ratio = {40960, 2867, 4096, 2867, 410},
		.makeup_gain = 6,
		.gain_init = 419430,
		.gain_attack_tc = 4766887,
		.gain_release_tc = 1044928780,
		.fast_release_tc = 2147480170,
		.biquad_params = {
			/* Gains : b0, b1, a0, a1, a2 */
			-1005668963, 1073741824, 0, 1005668963, 0, /* band-0 */
			998437058, -2067742187, 1073741824, 2067742187, -998437058, /* band-1 */
			1051963422, -2121153948, 1073741824, 2121153948, -1051963422, /* band-2 */
			434951949, -1306567134, 1073741824, 1306567134, -434951949, /* band-3 */
			780656019, -1605955641, 1073741824, 1605955641, -780656019, /* band-4 */
			1024497031, -1817128152, 1073741824, 1817128152, -1024497031, /* band-5 */
			1073741824, 0, 0, 0, 0, /* band-6 */
			1073741824, 0, 0, 0, 0, /* band-7 */
		}
	},
	.band_params[MBDRC_HIGH_BAND] = {
		.band = MBDRC_HIGH_BAND,
		.iir_stages = 5,
		.in_attack_tc = 2144750688,
		.in_release_tc = 70402888,
		.fast_attack_tc = 2147483647,
		.in_threshold = {130, 50, 30, 6},
		.out_threshold = {106, 50, 30, 13},
		.ratio = {40960, 2867, 4096, 2867, 410},
		.makeup_gain = 6,
		.gain_init = 419430,
		.gain_attack_tc = 4766887,
		.gain_release_tc = 1044928780,
		.fast_release_tc = 2147480170,
		.biquad_params = {
			/* Gains : b0, b1, a0, a1, a2 */
			1073741824, 0, 0, 0, 0, /* band-0 */
			1073741824, 0, 0, 0, 0, /* band-1 */
			1073741824, 0, 0, 0, 0, /* band-2 */
			-619925131, 1073741824, 0, 619925131, 0, /* band-3 */
			606839335, -1455425976, 1073741824, 1455425976, -606839335, /* band-4 */
			917759617, -1724690840, 1073741824, 1724690840, -917759617, /* band-5 */
			1073741824, 0, 0, 0, 0, /* band-6 */
			1073741824, 0, 0, 0, 0, /* band-7 */
		}
	}
};

static int tegra210_mbdrc_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_ope *ope = snd_soc_codec_get_drvdata(codec);
	unsigned int mask = (1 << fls(mc->max)) - 1;
	unsigned int val;

	regmap_read(ope->mbdrc_regmap, mc->reg, &val);
	ucontrol->value.integer.value[0] = (val >> mc->shift) & mask;
	if (mc->invert)
		ucontrol->value.integer.value[0] =
			mc->max - ucontrol->value.integer.value[0];

	return 0;
}

static int tegra210_mbdrc_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_ope *ope = snd_soc_codec_get_drvdata(codec);
	unsigned int mask = (1 << fls(mc->max)) - 1;
	unsigned int val;

	val = (ucontrol->value.integer.value[0] & mask);
	if (mc->invert)
		val = mc->max - val;
	val = val << mc->shift;

	return regmap_update_bits(ope->mbdrc_regmap, mc->reg,
				(mask << mc->shift), val);
}

static int tegra210_mbdrc_get_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_ope *ope = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int val;

	regmap_read(ope->mbdrc_regmap, e->reg, &val);
	ucontrol->value.enumerated.item[0] = (val >> e->shift_l) & e->mask;

	return 0;
}

static int tegra210_mbdrc_put_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_ope *ope = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int val;
	unsigned int mask;

	if (ucontrol->value.enumerated.item[0] > e->items - 1)
		return -EINVAL;

	val = ucontrol->value.enumerated.item[0] << e->shift_l;
	mask = e->mask << e->shift_l;

	return regmap_update_bits(ope->mbdrc_regmap, e->reg, mask, val);
}

static int tegra210_mbdrc_band_params_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tegra_soc_bytes *params = (void *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_ope *ope = snd_soc_codec_get_drvdata(codec);
	u32 *data = (u32 *)ucontrol->value.bytes.data;
	u32 regs = params->soc.base;
	u32 mask = params->soc.mask;
	u32 shift = params->shift;
	int i;

	for (i = 0; i < params->soc.num_regs; i++,
			regs += codec->component.val_bytes) {
		regmap_read(ope->mbdrc_regmap, regs, &data[i]);
		data[i] = ((data[i] & mask) >> shift);
	}

	return 0;
}

static int tegra210_mbdrc_band_params_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tegra_soc_bytes *params = (void *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_ope *ope = snd_soc_codec_get_drvdata(codec);
	u32 *data = (u32 *)ucontrol->value.bytes.data;
	u32 regs = params->soc.base;
	u32 mask = params->soc.mask;
	u32 shift = params->shift;
	int i;

	for (i = 0; i < params->soc.num_regs; i++,
			regs += codec->component.val_bytes)
		regmap_update_bits(ope->mbdrc_regmap, regs, mask,
				   data[i] << shift);

	return 0;
}

static int tegra210_mbdrc_threshold_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tegra_soc_bytes *params = (void *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_ope *ope = snd_soc_codec_get_drvdata(codec);
	u32 *data = (u32 *)ucontrol->value.bytes.data;
	u32 regs = params->soc.base;
	u32 num_regs = params->soc.num_regs;
	u32 val;
	int i;

	for (i = 0; i < num_regs; i += 4, regs += codec->component.val_bytes) {
		regmap_read(ope->mbdrc_regmap, regs, &val);

		data[i] = (val & TEGRA210_MBDRC_THRESH_1ST_MASK) >>
					TEGRA210_MBDRC_THRESH_1ST_SHIFT;
		data[i + 1] = (val & TEGRA210_MBDRC_THRESH_2ND_MASK) >>
					TEGRA210_MBDRC_THRESH_2ND_SHIFT;
		data[i + 2] = (val & TEGRA210_MBDRC_THRESH_3RD_MASK) >>
					TEGRA210_MBDRC_THRESH_3RD_SHIFT;
		data[i + 3] = (val & TEGRA210_MBDRC_THRESH_4TH_MASK) >>
					TEGRA210_MBDRC_THRESH_4TH_SHIFT;
	}

	return 0;
}

static int tegra210_mbdrc_threshold_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tegra_soc_bytes *params = (void *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_ope *ope = snd_soc_codec_get_drvdata(codec);
	u32 *data = (u32 *)ucontrol->value.bytes.data;
	u32 regs = params->soc.base;
	u32 num_regs = params->soc.num_regs;
	int i;

	for (i = 0; i < num_regs; i += 4, regs += codec->component.val_bytes) {
		data[i] = (((data[i] >> TEGRA210_MBDRC_THRESH_1ST_SHIFT) &
					TEGRA210_MBDRC_THRESH_1ST_MASK) |
			((data[i + 1] >> TEGRA210_MBDRC_THRESH_2ND_SHIFT) &
					TEGRA210_MBDRC_THRESH_2ND_MASK) |
			((data[i + 2] >> TEGRA210_MBDRC_THRESH_3RD_SHIFT) &
					TEGRA210_MBDRC_THRESH_3RD_MASK) |
			((data[i + 3] >> TEGRA210_MBDRC_THRESH_4TH_SHIFT) &
					TEGRA210_MBDRC_THRESH_4TH_MASK));
		regmap_update_bits(ope->mbdrc_regmap, regs,
			0xffffffff, data[i]);
	}

	return 0;
}

static int tegra210_mbdrc_biquad_coeffs_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tegra_soc_bytes *params = (void *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	u32 *data = (u32 *)ucontrol->value.bytes.data;

	memset(data, 0, params->soc.num_regs * codec->component.val_bytes);
	return 0;
}

static int tegra210_mbdrc_biquad_coeffs_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tegra_soc_bytes *params = (void *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_ope *ope = snd_soc_codec_get_drvdata(codec);
	u32 reg_ctrl = params->soc.base;
	u32 reg_data = reg_ctrl + codec->component.val_bytes;
	u32 *data = (u32 *)ucontrol->value.bytes.data;

	tegra210_xbar_write_ahubram(ope->mbdrc_regmap, reg_ctrl, reg_data,
				    params->shift, data, params->soc.num_regs);

	return 0;
}

static const char * const tegra210_mbdrc_mode_text[] = {
	"bypass", "fullband", "dualband", "multiband"
};

static const struct soc_enum tegra210_mbdrc_mode_enum =
	SOC_ENUM_SINGLE(TEGRA210_MBDRC_CONFIG,
		TEGRA210_MBDRC_CONFIG_MBDRC_MODE_SHIFT, 4,
		tegra210_mbdrc_mode_text);

static const char * const tegra210_mbdrc_peak_rms_text[] = {
	"peak", "rms"
};

static const struct soc_enum tegra210_mbdrc_peak_rms_enum =
	SOC_ENUM_SINGLE(TEGRA210_MBDRC_CONFIG,
		TEGRA210_MBDRC_CONFIG_PEAK_RMS_SHIFT, 2,
		tegra210_mbdrc_peak_rms_text);

static const char * const tegra210_mbdrc_filter_structure_text[] = {
	"all-pass-tree", "flexible"
};

static const struct soc_enum tegra210_mbdrc_filter_structure_enum =
	SOC_ENUM_SINGLE(TEGRA210_MBDRC_CONFIG,
		TEGRA210_MBDRC_CONFIG_FILTER_STRUCTURE_SHIFT, 2,
		tegra210_mbdrc_filter_structure_text);

static const char * const tegra210_mbdrc_frame_size_text[] = {
	"N1", "N2", "N4", "N8", "N16", "N32", "N64"
};

static const struct soc_enum tegra210_mbdrc_frame_size_enum =
	SOC_ENUM_SINGLE(TEGRA210_MBDRC_CONFIG,
		TEGRA210_MBDRC_CONFIG_FRAME_SIZE_SHIFT, 7,
		tegra210_mbdrc_frame_size_text);

#define TEGRA_MBDRC_BYTES_EXT(xname, xbase, xregs, xshift, xmask) \
	TEGRA_SOC_BYTES_EXT(xname, xbase, xregs, xshift, xmask, \
	    tegra210_mbdrc_band_params_get, tegra210_mbdrc_band_params_put)

#define TEGRA_MBDRC_BAND_BYTES_EXT(xname, xbase, xshift, xmask) \
	TEGRA_MBDRC_BYTES_EXT(xname, xbase, TEGRA210_MBDRC_FILTER_COUNT, \
		xshift, xmask)

static const struct snd_kcontrol_new tegra210_mbdrc_controls[] = {
	SOC_ENUM_EXT("mbdrc peak-rms mode", tegra210_mbdrc_peak_rms_enum,
		tegra210_mbdrc_get_enum, tegra210_mbdrc_put_enum),
	SOC_ENUM_EXT("mbdrc filter structure",
		tegra210_mbdrc_filter_structure_enum,
		tegra210_mbdrc_get_enum, tegra210_mbdrc_put_enum),
	SOC_ENUM_EXT("mbdrc frame size", tegra210_mbdrc_frame_size_enum,
		tegra210_mbdrc_get_enum, tegra210_mbdrc_put_enum),
	SOC_ENUM_EXT("mbdrc mode", tegra210_mbdrc_mode_enum,
		tegra210_mbdrc_get_enum, tegra210_mbdrc_put_enum),

	SOC_SINGLE_EXT("mbdrc rms offset", TEGRA210_MBDRC_CONFIG,
		TEGRA210_MBDRC_CONFIG_RMS_OFFSET_SHIFT, 0x1ff, 0,
		tegra210_mbdrc_get, tegra210_mbdrc_put),
	SOC_SINGLE_EXT("mbdrc shift control", TEGRA210_MBDRC_CONFIG,
		TEGRA210_MBDRC_CONFIG_SHIFT_CTRL_SHIFT, 0x1f, 0,
		tegra210_mbdrc_get, tegra210_mbdrc_put),
	SOC_SINGLE_EXT("mbdrc master volume", TEGRA210_MBDRC_MASTER_VOLUME,
		TEGRA210_MBDRC_MASTER_VOLUME_SHIFT, 0xffffffff, 0,
		tegra210_mbdrc_get, tegra210_mbdrc_put),
	SOC_SINGLE_EXT("mbdrc fast attack factor", TEGRA210_MBDRC_FAST_FACTOR,
		TEGRA210_MBDRC_FAST_FACTOR_ATTACK_SHIFT, 0xffff, 0,
		tegra210_mbdrc_get, tegra210_mbdrc_put),
	SOC_SINGLE_EXT("mbdrc fast release factor", TEGRA210_MBDRC_FAST_FACTOR,
		TEGRA210_MBDRC_FAST_FACTOR_RELEASE_SHIFT, 0xffff, 0,
		tegra210_mbdrc_get, tegra210_mbdrc_put),

	TEGRA_SOC_BYTES_EXT("mbdrc iir stages", TEGRA210_MBDRC_IIR_CONFIG,
		TEGRA210_MBDRC_FILTER_COUNT,
		TEGRA210_MBDRC_IIR_CONFIG_NUM_STAGES_SHIFT,
		TEGRA210_MBDRC_IIR_CONFIG_NUM_STAGES_MASK,
		tegra210_mbdrc_band_params_get, tegra210_mbdrc_band_params_put),
	TEGRA_SOC_BYTES_EXT("mbdrc in attack tc", TEGRA210_MBDRC_IN_ATTACK,
		TEGRA210_MBDRC_FILTER_COUNT,
		TEGRA210_MBDRC_IN_ATTACK_TC_SHIFT,
		TEGRA210_MBDRC_IN_ATTACK_TC_MASK,
		tegra210_mbdrc_band_params_get, tegra210_mbdrc_band_params_put),
	TEGRA_SOC_BYTES_EXT("mbdrc in release tc", TEGRA210_MBDRC_IN_RELEASE,
		TEGRA210_MBDRC_FILTER_COUNT,
		TEGRA210_MBDRC_IN_RELEASE_TC_SHIFT,
		TEGRA210_MBDRC_IN_RELEASE_TC_MASK,
		tegra210_mbdrc_band_params_get, tegra210_mbdrc_band_params_put),
	TEGRA_SOC_BYTES_EXT("mbdrc fast attack tc", TEGRA210_MBDRC_FAST_ATTACK,
		TEGRA210_MBDRC_FILTER_COUNT,
		TEGRA210_MBDRC_FAST_ATTACK_TC_SHIFT,
		TEGRA210_MBDRC_FAST_ATTACK_TC_MASK,
		tegra210_mbdrc_band_params_get, tegra210_mbdrc_band_params_put),

	TEGRA_SOC_BYTES_EXT("mbdrc in threshold", TEGRA210_MBDRC_IN_THRESHOLD,
		TEGRA210_MBDRC_FILTER_COUNT * 4, 0, 0xffffffff,
		tegra210_mbdrc_threshold_get, tegra210_mbdrc_threshold_put),
	TEGRA_SOC_BYTES_EXT("mbdrc out threshold", TEGRA210_MBDRC_OUT_THRESHOLD,
		TEGRA210_MBDRC_FILTER_COUNT * 4, 0, 0xffffffff,
		tegra210_mbdrc_threshold_get, tegra210_mbdrc_threshold_put),

	TEGRA_SOC_BYTES_EXT("mbdrc ratio", TEGRA210_MBDRC_RATIO_1ST,
		TEGRA210_MBDRC_FILTER_COUNT * 5,
		TEGRA210_MBDRC_RATIO_1ST_SHIFT, TEGRA210_MBDRC_RATIO_1ST_MASK,
		tegra210_mbdrc_band_params_get, tegra210_mbdrc_band_params_put),

	TEGRA_SOC_BYTES_EXT("mbdrc makeup gain", TEGRA210_MBDRC_MAKEUP_GAIN,
		TEGRA210_MBDRC_FILTER_COUNT,
		TEGRA210_MBDRC_MAKEUP_GAIN_SHIFT,
		TEGRA210_MBDRC_MAKEUP_GAIN_MASK,
		tegra210_mbdrc_band_params_get, tegra210_mbdrc_band_params_put),
	TEGRA_SOC_BYTES_EXT("mbdrc init gain", TEGRA210_MBDRC_INIT_GAIN,
		TEGRA210_MBDRC_FILTER_COUNT,
		TEGRA210_MBDRC_INIT_GAIN_SHIFT,
		TEGRA210_MBDRC_INIT_GAIN_MASK,
		tegra210_mbdrc_band_params_get, tegra210_mbdrc_band_params_put),
	TEGRA_SOC_BYTES_EXT("mbdrc attack gain", TEGRA210_MBDRC_GAIN_ATTACK,
		TEGRA210_MBDRC_FILTER_COUNT,
		TEGRA210_MBDRC_GAIN_ATTACK_SHIFT,
		TEGRA210_MBDRC_GAIN_ATTACK_MASK,
		tegra210_mbdrc_band_params_get, tegra210_mbdrc_band_params_put),
	TEGRA_SOC_BYTES_EXT("mbdrc release gain", TEGRA210_MBDRC_GAIN_RELEASE,
		TEGRA210_MBDRC_FILTER_COUNT,
		TEGRA210_MBDRC_GAIN_RELEASE_SHIFT,
		TEGRA210_MBDRC_GAIN_RELEASE_MASK,
		tegra210_mbdrc_band_params_get, tegra210_mbdrc_band_params_put),
	TEGRA_SOC_BYTES_EXT("mbdrc fast release gain",
		TEGRA210_MBDRC_FAST_RELEASE,
		TEGRA210_MBDRC_FILTER_COUNT,
		TEGRA210_MBDRC_FAST_RELEASE_SHIFT,
		TEGRA210_MBDRC_FAST_RELEASE_MASK,
		tegra210_mbdrc_band_params_get, tegra210_mbdrc_band_params_put),

	TEGRA_SOC_BYTES_EXT("mbdrc low band biquad coeffs",
		TEGRA210_MBDRC_AHUBRAMCTL_CONFIG_RAM_CTRL,
		TEGRA210_MBDRC_MAX_BIQUAD_STAGES * 5, 0, 0xffffffff,
		tegra210_mbdrc_biquad_coeffs_get,
		tegra210_mbdrc_biquad_coeffs_put),
	TEGRA_SOC_BYTES_EXT("mbdrc mid band biquad coeffs",
		TEGRA210_MBDRC_AHUBRAMCTL_CONFIG_RAM_CTRL +
		TEGRA210_MBDRC_FILTER_PARAM_STRIDE,
		TEGRA210_MBDRC_MAX_BIQUAD_STAGES * 5, 0, 0xffffffff,
		tegra210_mbdrc_biquad_coeffs_get,
		tegra210_mbdrc_biquad_coeffs_put),
	TEGRA_SOC_BYTES_EXT("mbdrc high band biquad coeffs",
		TEGRA210_MBDRC_AHUBRAMCTL_CONFIG_RAM_CTRL +
		(TEGRA210_MBDRC_FILTER_PARAM_STRIDE * 2),
		TEGRA210_MBDRC_MAX_BIQUAD_STAGES * 5, 0, 0xffffffff,
		tegra210_mbdrc_biquad_coeffs_get,
		tegra210_mbdrc_biquad_coeffs_put),
};

static bool tegra210_mbdrc_wr_reg(struct device *dev, unsigned int reg)
{
	if (reg >= TEGRA210_MBDRC_IIR_CONFIG)
		reg -= ((reg - TEGRA210_MBDRC_IIR_CONFIG) %
			(TEGRA210_MBDRC_FILTER_PARAM_STRIDE *
			 TEGRA210_MBDRC_FILTER_COUNT));

	switch (reg) {
	case TEGRA210_MBDRC_CG:
	case TEGRA210_MBDRC_SOFT_RESET:
	case TEGRA210_MBDRC_CONFIG:
	case TEGRA210_MBDRC_CHANNEL_MASK:
	case TEGRA210_MBDRC_MASTER_VOLUME:
	case TEGRA210_MBDRC_FAST_FACTOR:
	case TEGRA210_MBDRC_IIR_CONFIG:
	case TEGRA210_MBDRC_IN_ATTACK:
	case TEGRA210_MBDRC_IN_RELEASE:
	case TEGRA210_MBDRC_FAST_ATTACK:
	case TEGRA210_MBDRC_IN_THRESHOLD:
	case TEGRA210_MBDRC_OUT_THRESHOLD:
	case TEGRA210_MBDRC_RATIO_1ST:
	case TEGRA210_MBDRC_RATIO_2ND:
	case TEGRA210_MBDRC_RATIO_3RD:
	case TEGRA210_MBDRC_RATIO_4TH:
	case TEGRA210_MBDRC_RATIO_5TH:
	case TEGRA210_MBDRC_MAKEUP_GAIN:
	case TEGRA210_MBDRC_INIT_GAIN:
	case TEGRA210_MBDRC_GAIN_ATTACK:
	case TEGRA210_MBDRC_GAIN_RELEASE:
	case TEGRA210_MBDRC_FAST_RELEASE:
	case TEGRA210_MBDRC_AHUBRAMCTL_CONFIG_RAM_CTRL:
	case TEGRA210_MBDRC_AHUBRAMCTL_CONFIG_RAM_DATA:
		return true;
	default:
		return false;
	};
}

static bool tegra210_mbdrc_rd_reg(struct device *dev, unsigned int reg)
{
	if (reg >= TEGRA210_MBDRC_IIR_CONFIG)
		reg -= ((reg - TEGRA210_MBDRC_IIR_CONFIG) %
			(TEGRA210_MBDRC_FILTER_PARAM_STRIDE *
			 TEGRA210_MBDRC_FILTER_COUNT));
	switch (reg) {
	case TEGRA210_MBDRC_CG:
	case TEGRA210_MBDRC_SOFT_RESET:
	case TEGRA210_MBDRC_STATUS:
	case TEGRA210_MBDRC_CONFIG:
	case TEGRA210_MBDRC_CHANNEL_MASK:
	case TEGRA210_MBDRC_MASTER_VOLUME:
	case TEGRA210_MBDRC_FAST_FACTOR:
	case TEGRA210_MBDRC_IIR_CONFIG:
	case TEGRA210_MBDRC_IN_ATTACK:
	case TEGRA210_MBDRC_IN_RELEASE:
	case TEGRA210_MBDRC_FAST_ATTACK:
	case TEGRA210_MBDRC_IN_THRESHOLD:
	case TEGRA210_MBDRC_OUT_THRESHOLD:
	case TEGRA210_MBDRC_RATIO_1ST:
	case TEGRA210_MBDRC_RATIO_2ND:
	case TEGRA210_MBDRC_RATIO_3RD:
	case TEGRA210_MBDRC_RATIO_4TH:
	case TEGRA210_MBDRC_RATIO_5TH:
	case TEGRA210_MBDRC_MAKEUP_GAIN:
	case TEGRA210_MBDRC_INIT_GAIN:
	case TEGRA210_MBDRC_GAIN_ATTACK:
	case TEGRA210_MBDRC_GAIN_RELEASE:
	case TEGRA210_MBDRC_FAST_RELEASE:
	case TEGRA210_MBDRC_AHUBRAMCTL_CONFIG_RAM_CTRL:
	case TEGRA210_MBDRC_AHUBRAMCTL_CONFIG_RAM_DATA:
		return true;
	default:
		return false;
	};
}

static bool tegra210_mbdrc_volatile_reg(struct device *dev, unsigned int reg)
{
	if (reg >= TEGRA210_MBDRC_IIR_CONFIG)
		reg -= ((reg - TEGRA210_MBDRC_IIR_CONFIG) %
			(TEGRA210_MBDRC_FILTER_PARAM_STRIDE *
			 TEGRA210_MBDRC_FILTER_COUNT));

	switch (reg) {
	case TEGRA210_MBDRC_SOFT_RESET:
	case TEGRA210_MBDRC_AHUBRAMCTL_CONFIG_RAM_CTRL:
	case TEGRA210_MBDRC_AHUBRAMCTL_CONFIG_RAM_DATA:
		return true;
	default:
		return false;
	};
}

static bool tegra210_mbdrc_precious_reg(struct device *dev, unsigned int reg)
{
	if (reg >= TEGRA210_MBDRC_IIR_CONFIG)
		reg -= ((reg - TEGRA210_MBDRC_IIR_CONFIG) %
			(TEGRA210_MBDRC_FILTER_PARAM_STRIDE *
			 TEGRA210_MBDRC_FILTER_COUNT));

	switch (reg) {
	case TEGRA210_MBDRC_AHUBRAMCTL_CONFIG_RAM_DATA:
		return true;
	default:
		return false;
	};
}

static const struct regmap_config tegra210_mbdrc_regmap_config = {
	.name = "mbdrc",
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = TEGRA210_MBDRC_MAX_REG,
	.writeable_reg = tegra210_mbdrc_wr_reg,
	.readable_reg = tegra210_mbdrc_rd_reg,
	.volatile_reg = tegra210_mbdrc_volatile_reg,
	.precious_reg = tegra210_mbdrc_precious_reg,
	.reg_defaults = tegra210_mbdrc_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(tegra210_mbdrc_reg_defaults),
	.cache_type = REGCACHE_FLAT,
};

int tegra210_mbdrc_codec_init(struct snd_soc_codec *codec)
{
	struct tegra210_ope *ope = snd_soc_codec_get_drvdata(codec);
	const struct tegra210_mbdrc_config *conf = &mbdrc_init_config;
	u32 val;
	int i;

	pm_runtime_get_sync(codec->dev);
	/* Initialize MBDRC registers and ahub-ram with default params */
	regmap_update_bits(ope->mbdrc_regmap, TEGRA210_MBDRC_CONFIG,
		TEGRA210_MBDRC_CONFIG_MBDRC_MODE_MASK,
		conf->mode << TEGRA210_MBDRC_CONFIG_MBDRC_MODE_SHIFT);
	regmap_update_bits(ope->mbdrc_regmap, TEGRA210_MBDRC_CONFIG,
		TEGRA210_MBDRC_CONFIG_RMS_OFFSET_MASK,
		conf->rms_off << TEGRA210_MBDRC_CONFIG_RMS_OFFSET_SHIFT);
	regmap_update_bits(ope->mbdrc_regmap, TEGRA210_MBDRC_CONFIG,
		TEGRA210_MBDRC_CONFIG_PEAK_RMS_MASK,
		conf->peak_rms_mode << TEGRA210_MBDRC_CONFIG_PEAK_RMS_SHIFT);
	regmap_update_bits(ope->mbdrc_regmap, TEGRA210_MBDRC_CONFIG,
		TEGRA210_MBDRC_CONFIG_FILTER_STRUCTURE_MASK,
		conf->fliter_structure <<
		TEGRA210_MBDRC_CONFIG_FILTER_STRUCTURE_SHIFT);
	regmap_update_bits(ope->mbdrc_regmap, TEGRA210_MBDRC_CONFIG,
		TEGRA210_MBDRC_CONFIG_SHIFT_CTRL_MASK,
		conf->shift_ctrl << TEGRA210_MBDRC_CONFIG_SHIFT_CTRL_SHIFT);
	regmap_update_bits(ope->mbdrc_regmap, TEGRA210_MBDRC_CONFIG,
		TEGRA210_MBDRC_CONFIG_FRAME_SIZE_MASK,
		__ffs(conf->frame_size) <<
		TEGRA210_MBDRC_CONFIG_FRAME_SIZE_SHIFT);
	regmap_update_bits(ope->mbdrc_regmap, TEGRA210_MBDRC_CHANNEL_MASK,
		TEGRA210_MBDRC_CHANNEL_MASK_MASK,
		conf->channel_mask << TEGRA210_MBDRC_CHANNEL_MASK_SHIFT);
	regmap_update_bits(ope->mbdrc_regmap, TEGRA210_MBDRC_FAST_FACTOR,
		TEGRA210_MBDRC_FAST_FACTOR_ATTACK_MASK,
		conf->fa_factor << TEGRA210_MBDRC_FAST_FACTOR_ATTACK_SHIFT);
	regmap_update_bits(ope->mbdrc_regmap, TEGRA210_MBDRC_FAST_FACTOR,
		TEGRA210_MBDRC_FAST_FACTOR_ATTACK_MASK,
		conf->fr_factor << TEGRA210_MBDRC_FAST_FACTOR_ATTACK_SHIFT);

	for (i = 0; i < MBDRC_NUM_BAND; i++) {
		const struct tegra210_mbdrc_band_params *params =
						&conf->band_params[i];
		u32 reg_off = i * TEGRA210_MBDRC_FILTER_PARAM_STRIDE;

		regmap_update_bits(ope->mbdrc_regmap,
			reg_off + TEGRA210_MBDRC_IIR_CONFIG,
			TEGRA210_MBDRC_IIR_CONFIG_NUM_STAGES_MASK,
			params->iir_stages <<
			TEGRA210_MBDRC_IIR_CONFIG_NUM_STAGES_SHIFT);
		regmap_update_bits(ope->mbdrc_regmap,
			reg_off + TEGRA210_MBDRC_IN_ATTACK,
			TEGRA210_MBDRC_IN_ATTACK_TC_MASK,
			params->in_attack_tc <<
			TEGRA210_MBDRC_IN_ATTACK_TC_SHIFT);
		regmap_update_bits(ope->mbdrc_regmap,
			reg_off + TEGRA210_MBDRC_IN_RELEASE,
			TEGRA210_MBDRC_IN_RELEASE_TC_MASK,
			params->in_release_tc <<
			TEGRA210_MBDRC_IN_RELEASE_TC_SHIFT);
		regmap_update_bits(ope->mbdrc_regmap,
			reg_off + TEGRA210_MBDRC_FAST_ATTACK,
			TEGRA210_MBDRC_FAST_ATTACK_TC_MASK,
			params->fast_attack_tc <<
			TEGRA210_MBDRC_FAST_ATTACK_TC_SHIFT);

		val = (((params->in_threshold[0] >>
				TEGRA210_MBDRC_THRESH_1ST_SHIFT) &
				TEGRA210_MBDRC_THRESH_1ST_MASK) |
			((params->in_threshold[1] >>
				TEGRA210_MBDRC_THRESH_2ND_SHIFT) &
				TEGRA210_MBDRC_THRESH_2ND_MASK) |
			((params->in_threshold[2] >>
				TEGRA210_MBDRC_THRESH_3RD_SHIFT) &
				TEGRA210_MBDRC_THRESH_3RD_MASK) |
			((params->in_threshold[3] >>
				TEGRA210_MBDRC_THRESH_4TH_SHIFT) &
				TEGRA210_MBDRC_THRESH_4TH_MASK));
		regmap_update_bits(ope->mbdrc_regmap,
			reg_off + TEGRA210_MBDRC_IN_THRESHOLD, 0xffffffff, val);

		val = (((params->out_threshold[0] >>
				TEGRA210_MBDRC_THRESH_1ST_SHIFT) &
				TEGRA210_MBDRC_THRESH_1ST_MASK) |
			((params->out_threshold[1] >>
				TEGRA210_MBDRC_THRESH_2ND_SHIFT) &
				TEGRA210_MBDRC_THRESH_2ND_MASK) |
			((params->out_threshold[2] >>
				TEGRA210_MBDRC_THRESH_3RD_SHIFT) &
				TEGRA210_MBDRC_THRESH_3RD_MASK) |
			((params->out_threshold[3] >>
				TEGRA210_MBDRC_THRESH_4TH_SHIFT) &
				TEGRA210_MBDRC_THRESH_4TH_MASK));
		regmap_update_bits(ope->mbdrc_regmap,
			reg_off + TEGRA210_MBDRC_OUT_THRESHOLD,
			0xffffffff, val);

		regmap_update_bits(ope->mbdrc_regmap,
			reg_off + TEGRA210_MBDRC_RATIO_1ST,
			TEGRA210_MBDRC_RATIO_1ST_MASK,
			params->ratio[0] << TEGRA210_MBDRC_RATIO_1ST_SHIFT);
		regmap_update_bits(ope->mbdrc_regmap,
			reg_off + TEGRA210_MBDRC_RATIO_2ND,
			TEGRA210_MBDRC_RATIO_2ND_MASK,
			params->ratio[1] << TEGRA210_MBDRC_RATIO_2ND_SHIFT);
		regmap_update_bits(ope->mbdrc_regmap,
			reg_off + TEGRA210_MBDRC_RATIO_3RD,
			TEGRA210_MBDRC_RATIO_3RD_MASK,
			params->ratio[2] << TEGRA210_MBDRC_RATIO_3RD_SHIFT);
		regmap_update_bits(ope->mbdrc_regmap,
			reg_off + TEGRA210_MBDRC_RATIO_4TH,
			TEGRA210_MBDRC_RATIO_4TH_MASK,
			params->ratio[3] << TEGRA210_MBDRC_RATIO_4TH_SHIFT);
		regmap_update_bits(ope->mbdrc_regmap,
			reg_off + TEGRA210_MBDRC_RATIO_5TH,
			TEGRA210_MBDRC_RATIO_5TH_MASK,
			params->ratio[4] << TEGRA210_MBDRC_RATIO_5TH_SHIFT);

		regmap_update_bits(ope->mbdrc_regmap,
			reg_off + TEGRA210_MBDRC_MAKEUP_GAIN,
			TEGRA210_MBDRC_MAKEUP_GAIN_MASK,
			params->makeup_gain <<
			TEGRA210_MBDRC_MAKEUP_GAIN_SHIFT);
		regmap_update_bits(ope->mbdrc_regmap,
			reg_off + TEGRA210_MBDRC_INIT_GAIN,
			TEGRA210_MBDRC_INIT_GAIN_MASK,
			params->gain_init <<
			TEGRA210_MBDRC_INIT_GAIN_SHIFT);
		regmap_update_bits(ope->mbdrc_regmap,
			reg_off + TEGRA210_MBDRC_GAIN_ATTACK,
			TEGRA210_MBDRC_GAIN_ATTACK_MASK,
			params->gain_attack_tc <<
			TEGRA210_MBDRC_GAIN_ATTACK_SHIFT);
		regmap_update_bits(ope->mbdrc_regmap,
			reg_off + TEGRA210_MBDRC_GAIN_RELEASE,
			TEGRA210_MBDRC_GAIN_RELEASE_MASK,
			params->gain_release_tc <<
			TEGRA210_MBDRC_GAIN_RELEASE_SHIFT);
		regmap_update_bits(ope->mbdrc_regmap,
			reg_off + TEGRA210_MBDRC_FAST_RELEASE,
			TEGRA210_MBDRC_FAST_RELEASE_MASK,
			params->fast_release_tc <<
			TEGRA210_MBDRC_FAST_RELEASE_SHIFT);

		tegra210_xbar_write_ahubram(ope->mbdrc_regmap,
			reg_off + TEGRA210_MBDRC_AHUBRAMCTL_CONFIG_RAM_CTRL,
			reg_off + TEGRA210_MBDRC_AHUBRAMCTL_CONFIG_RAM_DATA, 0,
			(u32 *)&params->biquad_params[0],
			TEGRA210_MBDRC_MAX_BIQUAD_STAGES * 5);
	}
	pm_runtime_put_sync(codec->dev);

	snd_soc_add_codec_controls(codec, tegra210_mbdrc_controls,
		ARRAY_SIZE(tegra210_mbdrc_controls));
	return 0;
}
EXPORT_SYMBOL_GPL(tegra210_mbdrc_codec_init);

int tegra210_mbdrc_init(struct platform_device *pdev, int id)
{
	struct tegra210_ope *ope = dev_get_drvdata(&pdev->dev);
	struct resource *mem, *memregion;
	void __iomem *regs;
	int ret = 0;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, id);
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

	ope->mbdrc_regmap = devm_regmap_init_mmio(&pdev->dev, regs,
					    &tegra210_mbdrc_regmap_config);
	if (IS_ERR(ope->mbdrc_regmap)) {
		dev_err(&pdev->dev, "regmap init failed\n");
		ret = PTR_ERR(ope->mbdrc_regmap);
		goto err;
	}

	return 0;
err:
	return ret;
}
EXPORT_SYMBOL_GPL(tegra210_mbdrc_init);

MODULE_AUTHOR("Sumit Bhattacharya <sumitb@nvidia.com>");
MODULE_DESCRIPTION("Tegra210 MBDRC module");
MODULE_LICENSE("GPL");
