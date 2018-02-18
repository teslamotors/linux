/*
 * tegra210_xbar_alt.c - Tegra210 XBAR driver
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
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/tegra_pm_domains.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/tegra-soc.h>
#include <sound/soc.h>
#include <mach/clk.h>

#include "tegra210_xbar_alt.h"
/* TODO: remove DRV_NAME_T18X after registering properly */
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
#define DRV_NAME "tegra210-axbar"
#else
#include <sound/tegra_audio.h>
#define DRV_NAME DRV_NAME_T18X
#endif

static struct tegra210_xbar *xbar;

static bool tegra210_xbar_volatile_reg(struct device *dev, unsigned int reg)
{
	return false;
}

static const struct regmap_config tegra210_xbar_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = MAX_REGISTER_ADDR,
	.volatile_reg = tegra210_xbar_volatile_reg,
	.cache_type = REGCACHE_FLAT,
};

static int tegra210_xbar_runtime_suspend(struct device *dev)
{
	regcache_cache_only(xbar->regmap, true);
	regcache_mark_dirty(xbar->regmap);

	if (!(tegra_platform_is_unit_fpga() || tegra_platform_is_fpga())) {
		clk_disable_unprepare(xbar->clk);
		clk_disable_unprepare(xbar->clk_ape);
#if defined(CONFIG_ARCH_TEGRA_18x_SOC)
		clk_disable_unprepare(xbar->clk_apb2ape);
#endif
	}

	return 0;
}

static int tegra210_xbar_runtime_resume(struct device *dev)
{
	int ret;

	if (!(tegra_platform_is_unit_fpga() || tegra_platform_is_fpga())) {
		ret = clk_prepare_enable(xbar->clk_ape);
		if (ret) {
			dev_err(dev, "clk_prepare_enable failed: %d\n", ret);
			return ret;
		}

#if defined(CONFIG_ARCH_TEGRA_18x_SOC)
		ret = clk_prepare_enable(xbar->clk_apb2ape);
		if (ret) {
			dev_err(dev, "clk_prepare_enable failed: %d\n", ret);
			return ret;
		}
#endif
		ret = clk_prepare_enable(xbar->clk);
		if (ret) {
			dev_err(dev, "clk_prepare_enable failed: %d\n", ret);
			return ret;
		}
	}

	regcache_cache_only(xbar->regmap, false);
	regcache_sync(xbar->regmap);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tegra210_xbar_child_suspend(struct device *dev, void *data)
{
	struct device_driver *drv = dev->driver;
	int ret = 0;

	if (!drv)
		return 0;

	if (drv->pm)
		if (drv->pm->suspend)
			ret = drv->pm->suspend(dev);

	return ret;
}

static int tegra210_xbar_suspend(struct device *dev)
{
	device_for_each_child(dev, NULL, tegra210_xbar_child_suspend);

	return 0;
}
#endif

int tegra210_xbar_codec_probe(struct snd_soc_codec *codec)
{
	codec->control_data = xbar->regmap;

	return 0;
}
EXPORT_SYMBOL_GPL(tegra210_xbar_codec_probe);

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
#define DAI(sname)						\
	{							\
		.name = #sname,					\
		.playback = {					\
			.stream_name = #sname " Receive",	\
			.channels_min = 1,			\
			.channels_max = 16,			\
			.rates = SNDRV_PCM_RATE_8000_192000,		\
			.formats = SNDRV_PCM_FMTBIT_S8 |		\
				SNDRV_PCM_FMTBIT_S16_LE |		\
				SNDRV_PCM_FMTBIT_S24_LE |		\
				SNDRV_PCM_FMTBIT_S32_LE,		\
		},						\
		.capture = {					\
			.stream_name = #sname " Transmit",	\
			.channels_min = 1,			\
			.channels_max = 16,			\
			.rates = SNDRV_PCM_RATE_8000_192000,		\
			.formats = SNDRV_PCM_FMTBIT_S8 |		\
				SNDRV_PCM_FMTBIT_S16_LE |		\
				SNDRV_PCM_FMTBIT_S24_LE |		\
				SNDRV_PCM_FMTBIT_S32_LE,		\
		},						\
	}

static struct snd_soc_dai_driver tegra210_xbar_dais[] = {
	DAI(ADMAIF1),
	DAI(ADMAIF2),
	DAI(ADMAIF3),
	DAI(ADMAIF4),
	DAI(ADMAIF5),
	DAI(ADMAIF6),
	DAI(ADMAIF7),
	DAI(ADMAIF8),
	DAI(ADMAIF9),
	DAI(ADMAIF10),
	DAI(I2S1),
	DAI(I2S2),
	DAI(I2S3),
	DAI(I2S4),
	DAI(I2S5),
	DAI(SFC1),
	DAI(SFC2),
	DAI(SFC3),
	DAI(SFC4),
	DAI(MIXER1-1),
	DAI(MIXER1-2),
	DAI(MIXER1-3),
	DAI(MIXER1-4),
	DAI(MIXER1-5),
	DAI(MIXER1-6),
	DAI(MIXER1-7),
	DAI(MIXER1-8),
	DAI(MIXER1-9),
	DAI(MIXER1-10),
	DAI(SPDIF1-1),
	DAI(SPDIF1-2),
	DAI(AFC1),
	DAI(AFC2),
	DAI(AFC3),
	DAI(AFC4),
	DAI(AFC5),
	DAI(AFC6),
	DAI(OPE1),
	DAI(OPE2),
	DAI(SPKPROT1),
	DAI(MVC1),
	DAI(MVC2),
	DAI(IQC1-1),
	DAI(IQC1-2),
	DAI(IQC2-1),
	DAI(IQC2-2),
	DAI(DMIC1),
	DAI(DMIC2),
	DAI(DMIC3),
	DAI(AMX1),
	DAI(AMX1-1),
	DAI(AMX1-2),
	DAI(AMX1-3),
	DAI(AMX1-4),
	DAI(AMX2),
	DAI(AMX2-1),
	DAI(AMX2-2),
	DAI(AMX2-3),
	DAI(AMX2-4),
	DAI(ADX1-1),
	DAI(ADX1-2),
	DAI(ADX1-3),
	DAI(ADX1-4),
	DAI(ADX1),
	DAI(ADX2-1),
	DAI(ADX2-2),
	DAI(ADX2-3),
	DAI(ADX2-4),
	DAI(ADX2),
};
#endif

static const char * const tegra210_xbar_mux_texts[] = {
	"None",
	"ADMAIF1",
	"ADMAIF2",
	"ADMAIF3",
	"ADMAIF4",
	"ADMAIF5",
	"ADMAIF6",
	"ADMAIF7",
	"ADMAIF8",
	"ADMAIF9",
	"ADMAIF10",
	"I2S1",
	"I2S2",
	"I2S3",
	"I2S4",
	"I2S5",
	"SFC1",
	"SFC2",
	"SFC3",
	"SFC4",
	/* index 0..19 above are inputs of PART0 Mux */
	"MIXER1-1",
	"MIXER1-2",
	"MIXER1-3",
	"MIXER1-4",
	"MIXER1-5",
	"AMX1",
	"AMX2",
	"SPDIF1-1",
	"SPDIF1-2",
	"AFC1",
	"AFC2",
	"AFC3",
	"AFC4",
	"AFC5",
	"AFC6",
	/* index 20..34 above are inputs of PART1 Mux */
	"OPE1",
	"OPE2",
	"SPKPROT1",
	"MVC1",
	"MVC2",
	"IQC1-1",
	"IQC1-2",
	"IQC2-1",
	"IQC2-2",
	"DMIC1",
	"DMIC2",
	"DMIC3",
	"ADX1-1",
	"ADX1-2",
	"ADX1-3",
	"ADX1-4",
	"ADX2-1",
	"ADX2-2",
	"ADX2-3",
	"ADX2-4",
	/* index 35..53 above are inputs of PART2 Mux */
};

#define MUX_VALUE(npart, nbit) (1 + nbit + npart * 32)
static const int tegra210_xbar_mux_values[] = {
	/* Mux0 input,	Mux1 input, Mux2 input */
	0,
	MUX_VALUE(0, 0),
	MUX_VALUE(0, 1),
	MUX_VALUE(0, 2),
	MUX_VALUE(0, 3),
	MUX_VALUE(0, 4),
	MUX_VALUE(0, 5),
	MUX_VALUE(0, 6),
	MUX_VALUE(0, 7),
	MUX_VALUE(0, 8),
	MUX_VALUE(0, 9),
	MUX_VALUE(0, 16),
	MUX_VALUE(0, 17),
	MUX_VALUE(0, 18),
	MUX_VALUE(0, 19),
	MUX_VALUE(0, 20),
	MUX_VALUE(0, 24),
	MUX_VALUE(0, 25),
	MUX_VALUE(0, 26),
	MUX_VALUE(0, 27),
	/* index 0..19 above are inputs of PART0 Mux */
	MUX_VALUE(1, 0),
	MUX_VALUE(1, 1),
	MUX_VALUE(1, 2),
	MUX_VALUE(1, 3),
	MUX_VALUE(1, 4),
	MUX_VALUE(1, 8),
	MUX_VALUE(1, 9),
	MUX_VALUE(1, 20),
	MUX_VALUE(1, 21),
	MUX_VALUE(1, 24),
	MUX_VALUE(1, 25),
	MUX_VALUE(1, 26),
	MUX_VALUE(1, 27),
	MUX_VALUE(1, 28),
	MUX_VALUE(1, 29),
	/* index 20..34 above are inputs of PART1 Mux */
	MUX_VALUE(2, 0),
	MUX_VALUE(2, 1),
	MUX_VALUE(2, 4),
	MUX_VALUE(2, 8),
	MUX_VALUE(2, 9),
	MUX_VALUE(2, 12),
	MUX_VALUE(2, 13),
	MUX_VALUE(2, 14),
	MUX_VALUE(2, 15),
	MUX_VALUE(2, 18),
	MUX_VALUE(2, 19),
	MUX_VALUE(2, 20),
	MUX_VALUE(2, 24),
	MUX_VALUE(2, 25),
	MUX_VALUE(2, 26),
	MUX_VALUE(2, 27),
	MUX_VALUE(2, 28),
	MUX_VALUE(2, 29),
	MUX_VALUE(2, 30),
	MUX_VALUE(2, 31),
	/* index 35..53 above are inputs of PART2 Mux */
};

int tegra210_xbar_get_value_enum(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_dapm_kcontrol_codec(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int reg_count, reg_val, val, bit_pos = 0, i;
	unsigned int reg[TEGRA210_XBAR_UPDATE_MAX_REG];

	reg_count = xbar->soc_data->reg_count;

	if (reg_count > TEGRA210_XBAR_UPDATE_MAX_REG)
		return -EINVAL;

	for (i = 0; i < reg_count; i++) {
		reg[i] = (e->reg +
			xbar->soc_data->reg_offset * i);
		reg_val = snd_soc_read(codec, reg[i]);
		val = reg_val & xbar->soc_data->mask[i];
		if (val != 0) {
			bit_pos = ffs(val) +
					(8*codec->component.val_bytes * i);
			break;
		}
	}

	for (i = 0; i < e->items; i++) {
		if (bit_pos == e->values[i]) {
			ucontrol->value.enumerated.item[0] = i;
			break;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(tegra210_xbar_get_value_enum);

int tegra210_xbar_put_value_enum(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_dapm_kcontrol_codec(kcontrol);
	struct snd_soc_dapm_context *dapm =
				snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int *item = ucontrol->value.enumerated.item;
	unsigned int change = 0, reg_idx = 0, value, *mask, bit_pos = 0;
	unsigned int i, reg_count, reg_val = 0, update_idx = 0;
	unsigned int reg[TEGRA210_XBAR_UPDATE_MAX_REG];
	struct snd_soc_dapm_update update[TEGRA210_XBAR_UPDATE_MAX_REG];

	/* initialize the reg_count and mask from soc_data */
	reg_count = xbar->soc_data->reg_count;
	mask = (unsigned int *)xbar->soc_data->mask;

	if (item[0] >= e->items || reg_count > TEGRA210_XBAR_UPDATE_MAX_REG)
		return -EINVAL;

	value = e->values[item[0]];

	if (value) {
		/* get the register index and value to set */
		reg_idx = (value - 1) / (8 * codec->component.val_bytes);
		bit_pos = (value - 1) % (8 * codec->component.val_bytes);
		reg_val = BIT(bit_pos);
	}

	for (i = 0; i < reg_count; i++) {
		reg[i] = e->reg + xbar->soc_data->reg_offset * i;
		if (i == reg_idx) {
			change |= snd_soc_test_bits(codec, reg[i],
							mask[i], reg_val);
			/* set the selected register */
			update[reg_count - 1].reg = reg[reg_idx];
			update[reg_count - 1].mask = mask[reg_idx];
			update[reg_count - 1].val = reg_val;
		} else {
			/* accumulate the change to update the DAPM path
			    when none is selected */
			change |= snd_soc_test_bits(codec, reg[i],
							mask[i], 0);

			/* clear the register when not selected */
			update[update_idx].reg = reg[i];
			update[update_idx].mask = mask[i];
			update[update_idx++].val = 0;
		}
	}

	/* power the widgets */
	if (change) {
		for (i = 0; i < reg_count; i++) {
			update[i].kcontrol = kcontrol;
			snd_soc_dapm_mux_update_power(dapm,
				kcontrol, item[0], e, &update[i]);
		}
	}

	return change;
}
EXPORT_SYMBOL_GPL(tegra210_xbar_put_value_enum);

#define MUX_REG(id) (TEGRA210_XBAR_RX_STRIDE * (id))

#define SOC_VALUE_ENUM_WIDE(xreg, shift, xmax, xtexts, xvalues) \
{	.reg = xreg, .shift_l = shift, .shift_r = shift, \
	.items = xmax, .texts = xtexts, .values = xvalues, \
	.mask = xmax ? roundup_pow_of_two(xmax) - 1 : 0}

#define SOC_VALUE_ENUM_WIDE_DECL(name, xreg, shift, \
		xtexts, xvalues) \
	static struct soc_enum name = SOC_VALUE_ENUM_WIDE(xreg, shift, \
					ARRAY_SIZE(xtexts), xtexts, xvalues)

#define MUX_ENUM_CTRL_DECL(ename, id) \
	SOC_VALUE_ENUM_WIDE_DECL(ename##_enum, MUX_REG(id), 0,	\
			tegra210_xbar_mux_texts, tegra210_xbar_mux_values); \
	static const struct snd_kcontrol_new ename##_control = \
		SOC_DAPM_ENUM_EXT("Route", ename##_enum,\
				tegra210_xbar_get_value_enum,\
				tegra210_xbar_put_value_enum)

MUX_ENUM_CTRL_DECL(admaif1_tx, 0x00);
MUX_ENUM_CTRL_DECL(admaif2_tx, 0x01);
MUX_ENUM_CTRL_DECL(admaif3_tx, 0x02);
MUX_ENUM_CTRL_DECL(admaif4_tx, 0x03);
MUX_ENUM_CTRL_DECL(admaif5_tx, 0x04);
MUX_ENUM_CTRL_DECL(admaif6_tx, 0x05);
MUX_ENUM_CTRL_DECL(admaif7_tx, 0x06);
MUX_ENUM_CTRL_DECL(admaif8_tx, 0x07);
MUX_ENUM_CTRL_DECL(admaif9_tx, 0x08);
MUX_ENUM_CTRL_DECL(admaif10_tx, 0x09);
MUX_ENUM_CTRL_DECL(i2s1_tx, 0x10);
MUX_ENUM_CTRL_DECL(i2s2_tx, 0x11);
MUX_ENUM_CTRL_DECL(i2s3_tx, 0x12);
MUX_ENUM_CTRL_DECL(i2s4_tx, 0x13);
MUX_ENUM_CTRL_DECL(i2s5_tx, 0x14);
MUX_ENUM_CTRL_DECL(sfc1_tx, 0x18);
MUX_ENUM_CTRL_DECL(sfc2_tx, 0x19);
MUX_ENUM_CTRL_DECL(sfc3_tx, 0x1a);
MUX_ENUM_CTRL_DECL(sfc4_tx, 0x1b);
MUX_ENUM_CTRL_DECL(mixer11_tx, 0x20);
MUX_ENUM_CTRL_DECL(mixer12_tx, 0x21);
MUX_ENUM_CTRL_DECL(mixer13_tx, 0x22);
MUX_ENUM_CTRL_DECL(mixer14_tx, 0x23);
MUX_ENUM_CTRL_DECL(mixer15_tx, 0x24);
MUX_ENUM_CTRL_DECL(mixer16_tx, 0x25);
MUX_ENUM_CTRL_DECL(mixer17_tx, 0x26);
MUX_ENUM_CTRL_DECL(mixer18_tx, 0x27);
MUX_ENUM_CTRL_DECL(mixer19_tx, 0x28);
MUX_ENUM_CTRL_DECL(mixer110_tx, 0x29);
MUX_ENUM_CTRL_DECL(spdif11_tx, 0x30);
MUX_ENUM_CTRL_DECL(spdif12_tx, 0x31);
MUX_ENUM_CTRL_DECL(afc1_tx, 0x34);
MUX_ENUM_CTRL_DECL(afc2_tx, 0x35);
MUX_ENUM_CTRL_DECL(afc3_tx, 0x36);
MUX_ENUM_CTRL_DECL(afc4_tx, 0x37);
MUX_ENUM_CTRL_DECL(afc5_tx, 0x38);
MUX_ENUM_CTRL_DECL(afc6_tx, 0x39);
MUX_ENUM_CTRL_DECL(ope1_tx, 0x40);
MUX_ENUM_CTRL_DECL(ope2_tx, 0x41);
MUX_ENUM_CTRL_DECL(spkprot_tx, 0x44);
MUX_ENUM_CTRL_DECL(mvc1_tx, 0x48);
MUX_ENUM_CTRL_DECL(mvc2_tx, 0x49);
MUX_ENUM_CTRL_DECL(amx11_tx, 0x50);
MUX_ENUM_CTRL_DECL(amx12_tx, 0x51);
MUX_ENUM_CTRL_DECL(amx13_tx, 0x52);
MUX_ENUM_CTRL_DECL(amx14_tx, 0x53);
MUX_ENUM_CTRL_DECL(amx21_tx, 0x54);
MUX_ENUM_CTRL_DECL(amx22_tx, 0x55);
MUX_ENUM_CTRL_DECL(amx23_tx, 0x56);
MUX_ENUM_CTRL_DECL(amx24_tx, 0x57);
MUX_ENUM_CTRL_DECL(adx1_tx, 0x58);
MUX_ENUM_CTRL_DECL(adx2_tx, 0x59);

#define WIDGETS(sname, ename) \
	SND_SOC_DAPM_AIF_IN(sname " RX", NULL, 0, SND_SOC_NOPM, 0, 0), \
	SND_SOC_DAPM_AIF_OUT(sname " TX", NULL, 0, SND_SOC_NOPM, 0, 0), \
	SND_SOC_DAPM_MUX(sname " Mux", SND_SOC_NOPM, 0, 0, &ename##_control)

#define TX_WIDGETS(sname) \
	SND_SOC_DAPM_AIF_IN(sname " RX", NULL, 0, SND_SOC_NOPM, 0, 0), \
	SND_SOC_DAPM_AIF_OUT(sname " TX", NULL, 0, SND_SOC_NOPM, 0, 0)

/*
 * The number of entries in, and order of, this array is closely tied to the
 * calculation of tegra210_xbar_codec.num_dapm_widgets near the end of
 * tegra210_xbar_probe()
 */
static const struct snd_soc_dapm_widget tegra210_xbar_widgets[] = {
	WIDGETS("ADMAIF1", admaif1_tx),
	WIDGETS("ADMAIF2", admaif2_tx),
	WIDGETS("ADMAIF3", admaif3_tx),
	WIDGETS("ADMAIF4", admaif4_tx),
	WIDGETS("ADMAIF5", admaif5_tx),
	WIDGETS("ADMAIF6", admaif6_tx),
	WIDGETS("ADMAIF7", admaif7_tx),
	WIDGETS("ADMAIF8", admaif8_tx),
	WIDGETS("ADMAIF9", admaif9_tx),
	WIDGETS("ADMAIF10", admaif10_tx),
	WIDGETS("I2S1", i2s1_tx),
	WIDGETS("I2S2", i2s2_tx),
	WIDGETS("I2S3", i2s3_tx),
	WIDGETS("I2S4", i2s4_tx),
	WIDGETS("I2S5", i2s5_tx),
	WIDGETS("SFC1", sfc1_tx),
	WIDGETS("SFC2", sfc2_tx),
	WIDGETS("SFC3", sfc3_tx),
	WIDGETS("SFC4", sfc4_tx),
	WIDGETS("MIXER1-1", mixer11_tx),
	WIDGETS("MIXER1-2", mixer12_tx),
	WIDGETS("MIXER1-3", mixer13_tx),
	WIDGETS("MIXER1-4", mixer14_tx),
	WIDGETS("MIXER1-5", mixer15_tx),
	WIDGETS("MIXER1-6", mixer16_tx),
	WIDGETS("MIXER1-7", mixer17_tx),
	WIDGETS("MIXER1-8", mixer18_tx),
	WIDGETS("MIXER1-9", mixer19_tx),
	WIDGETS("MIXER1-10", mixer110_tx),
	WIDGETS("SPDIF1-1", spdif11_tx),
	WIDGETS("SPDIF1-2", spdif12_tx),
	WIDGETS("AFC1", afc1_tx),
	WIDGETS("AFC2", afc2_tx),
	WIDGETS("AFC3", afc3_tx),
	WIDGETS("AFC4", afc4_tx),
	WIDGETS("AFC5", afc5_tx),
	WIDGETS("AFC6", afc6_tx),
	WIDGETS("OPE1", ope1_tx),
	WIDGETS("OPE2", ope2_tx),
	WIDGETS("SPKPROT1", spkprot_tx),
	WIDGETS("MVC1", mvc1_tx),
	WIDGETS("MVC2", mvc2_tx),
	WIDGETS("AMX1-1", amx11_tx),
	WIDGETS("AMX1-2", amx12_tx),
	WIDGETS("AMX1-3", amx13_tx),
	WIDGETS("AMX1-4", amx14_tx),
	WIDGETS("AMX2-1", amx21_tx),
	WIDGETS("AMX2-2", amx22_tx),
	WIDGETS("AMX2-3", amx23_tx),
	WIDGETS("AMX2-4", amx24_tx),
	WIDGETS("ADX1", adx1_tx),
	WIDGETS("ADX2", adx2_tx),
	TX_WIDGETS("IQC1-1"),
	TX_WIDGETS("IQC1-2"),
	TX_WIDGETS("IQC2-1"),
	TX_WIDGETS("IQC2-2"),
	TX_WIDGETS("DMIC1"),
	TX_WIDGETS("DMIC2"),
	TX_WIDGETS("DMIC3"),
	TX_WIDGETS("AMX1"),
	TX_WIDGETS("ADX1-1"),
	TX_WIDGETS("ADX1-2"),
	TX_WIDGETS("ADX1-3"),
	TX_WIDGETS("ADX1-4"),
	TX_WIDGETS("AMX2"),
	TX_WIDGETS("ADX2-1"),
	TX_WIDGETS("ADX2-2"),
	TX_WIDGETS("ADX2-3"),
	TX_WIDGETS("ADX2-4"),
};

#define TEGRA210_ROUTES(name)					\
	{ name " RX",       NULL,		name " Receive"},	\
	{ name " Transmit", NULL,		name " TX"},		\
	{ name " TX",       NULL,		name " Mux" },		\
	{ name " Mux",      "ADMAIF1",		"ADMAIF1 RX" },		\
	{ name " Mux",      "ADMAIF2",		"ADMAIF2 RX" },		\
	{ name " Mux",      "ADMAIF3",		"ADMAIF3 RX" },		\
	{ name " Mux",      "ADMAIF4",		"ADMAIF4 RX" },		\
	{ name " Mux",      "ADMAIF5",		"ADMAIF5 RX" },		\
	{ name " Mux",      "ADMAIF6",		"ADMAIF6 RX" },		\
	{ name " Mux",      "ADMAIF7",		"ADMAIF7 RX" },		\
	{ name " Mux",      "ADMAIF8",		"ADMAIF8 RX" },		\
	{ name " Mux",      "ADMAIF9",		"ADMAIF9 RX" },		\
	{ name " Mux",      "ADMAIF10",		"ADMAIF10 RX" },	\
	{ name " Mux",      "I2S1",		"I2S1 RX" },		\
	{ name " Mux",      "I2S2",		"I2S2 RX" },		\
	{ name " Mux",      "I2S3",		"I2S3 RX" },		\
	{ name " Mux",      "I2S4",		"I2S4 RX" },		\
	{ name " Mux",      "I2S5",		"I2S5 RX" },		\
	{ name " Mux",      "SFC1",		"SFC1 RX" },		\
	{ name " Mux",      "SFC2",		"SFC2 RX" },		\
	{ name " Mux",      "SFC3",		"SFC3 RX" },		\
	{ name " Mux",      "SFC4",		"SFC4 RX" },		\
	{ name " Mux",      "MIXER1-1",		"MIXER1-1 RX" },	\
	{ name " Mux",      "MIXER1-2",		"MIXER1-2 RX" },	\
	{ name " Mux",      "MIXER1-3",		"MIXER1-3 RX" },	\
	{ name " Mux",      "MIXER1-4",		"MIXER1-4 RX" },	\
	{ name " Mux",      "MIXER1-5",		"MIXER1-5 RX" },	\
	{ name " Mux",      "SPDIF1-1",		"SPDIF1-1 RX" },	\
	{ name " Mux",      "SPDIF1-2",		"SPDIF1-2 RX" },	\
	{ name " Mux",      "AFC1",		"AFC1 RX" },		\
	{ name " Mux",      "AFC2",		"AFC2 RX" },		\
	{ name " Mux",      "AFC3",		"AFC3 RX" },		\
	{ name " Mux",      "AFC4",		"AFC4 RX" },		\
	{ name " Mux",      "AFC5",		"AFC5 RX" },		\
	{ name " Mux",      "AFC6",		"AFC6 RX" },		\
	{ name " Mux",      "OPE1",		"OPE1 RX" },		\
	{ name " Mux",      "OPE2",		"OPE2 RX" },		\
	{ name " Mux",      "MVC1",		"MVC1 RX" },		\
	{ name " Mux",      "MVC2",		"MVC2 RX" },		\
	{ name " Mux",      "IQC1-1",		"IQC1-1 RX" },		\
	{ name " Mux",      "IQC1-2",		"IQC1-2 RX" },		\
	{ name " Mux",      "IQC2-1",		"IQC2-1 RX" },		\
	{ name " Mux",      "IQC2-2",		"IQC2-2 RX" },		\
	{ name " Mux",      "DMIC1",		"DMIC1 RX" },		\
	{ name " Mux",      "DMIC2",		"DMIC2 RX" },		\
	{ name " Mux",      "DMIC3",		"DMIC3 RX" },		\
	{ name " Mux",      "AMX1",		"AMX1 RX" },		\
	{ name " Mux",      "ADX1-1",		"ADX1-1 RX" },		\
	{ name " Mux",      "ADX1-2",		"ADX1-2 RX" },		\
	{ name " Mux",      "ADX1-3",		"ADX1-3 RX" },		\
	{ name " Mux",      "ADX1-4",		"ADX1-4 RX" },		\
	{ name " Mux",      "AMX2",		"AMX2 RX" },		\
	{ name " Mux",      "ADX2-1",		"ADX2-1 RX" },		\
	{ name " Mux",      "ADX2-2",		"ADX2-2 RX" },		\
	{ name " Mux",      "ADX2-3",		"ADX2-3 RX" },		\
	{ name " Mux",      "ADX2-4",		"ADX2-4 RX" },

#define IN_OUT_ROUTES(name)				\
	{ name " RX",       NULL,	name " Receive" },	\
	{ name " Transmit", NULL,	name " TX" },

/*
 * The number of entries in, and order of, this array is closely tied to the
 * calculation of tegra210_xbar_codec.num_dapm_routes near the end of
 * tegra210_xbar_probe()
 */
static const struct snd_soc_dapm_route tegra210_xbar_routes[] = {
	TEGRA210_ROUTES("ADMAIF1")
	TEGRA210_ROUTES("ADMAIF2")
	TEGRA210_ROUTES("ADMAIF3")
	TEGRA210_ROUTES("ADMAIF4")
	TEGRA210_ROUTES("ADMAIF5")
	TEGRA210_ROUTES("ADMAIF6")
	TEGRA210_ROUTES("ADMAIF7")
	TEGRA210_ROUTES("ADMAIF8")
	TEGRA210_ROUTES("ADMAIF9")
	TEGRA210_ROUTES("ADMAIF10")
	TEGRA210_ROUTES("I2S1")
	TEGRA210_ROUTES("I2S2")
	TEGRA210_ROUTES("I2S3")
	TEGRA210_ROUTES("I2S4")
	TEGRA210_ROUTES("I2S5")
	TEGRA210_ROUTES("SFC1")
	TEGRA210_ROUTES("SFC2")
	TEGRA210_ROUTES("SFC3")
	TEGRA210_ROUTES("SFC4")
	TEGRA210_ROUTES("MIXER1-1")
	TEGRA210_ROUTES("MIXER1-2")
	TEGRA210_ROUTES("MIXER1-3")
	TEGRA210_ROUTES("MIXER1-4")
	TEGRA210_ROUTES("MIXER1-5")
	TEGRA210_ROUTES("MIXER1-6")
	TEGRA210_ROUTES("MIXER1-7")
	TEGRA210_ROUTES("MIXER1-8")
	TEGRA210_ROUTES("MIXER1-9")
	TEGRA210_ROUTES("MIXER1-10")
	TEGRA210_ROUTES("SPDIF1-1")
	TEGRA210_ROUTES("SPDIF1-2")
	TEGRA210_ROUTES("AFC1")
	TEGRA210_ROUTES("AFC2")
	TEGRA210_ROUTES("AFC3")
	TEGRA210_ROUTES("AFC4")
	TEGRA210_ROUTES("AFC5")
	TEGRA210_ROUTES("AFC6")
	TEGRA210_ROUTES("OPE1")
	TEGRA210_ROUTES("OPE2")
	TEGRA210_ROUTES("SPKPROT1")
	TEGRA210_ROUTES("MVC1")
	TEGRA210_ROUTES("MVC2")
	TEGRA210_ROUTES("AMX1-1")
	TEGRA210_ROUTES("AMX1-2")
	TEGRA210_ROUTES("AMX1-3")
	TEGRA210_ROUTES("AMX1-4")
	TEGRA210_ROUTES("AMX2-1")
	TEGRA210_ROUTES("AMX2-2")
	TEGRA210_ROUTES("AMX2-3")
	TEGRA210_ROUTES("AMX2-4")
	TEGRA210_ROUTES("ADX1")
	TEGRA210_ROUTES("ADX2")
	IN_OUT_ROUTES("IQC1-1")
	IN_OUT_ROUTES("IQC1-2")
	IN_OUT_ROUTES("IQC2-1")
	IN_OUT_ROUTES("IQC2-1")
	IN_OUT_ROUTES("DMIC1")
	IN_OUT_ROUTES("DMIC2")
	IN_OUT_ROUTES("DMIC3")
	IN_OUT_ROUTES("AMX1")
	IN_OUT_ROUTES("AMX2")
	IN_OUT_ROUTES("ADX1-1")
	IN_OUT_ROUTES("ADX1-2")
	IN_OUT_ROUTES("ADX1-3")
	IN_OUT_ROUTES("ADX1-4")
	IN_OUT_ROUTES("ADX2-1")
	IN_OUT_ROUTES("ADX2-2")
	IN_OUT_ROUTES("ADX2-3")
	IN_OUT_ROUTES("ADX2-4")
};

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
static struct snd_soc_codec_driver tegra210_xbar_codec = {
	.probe = tegra210_xbar_codec_probe,
	.dapm_widgets = tegra210_xbar_widgets,
	.dapm_routes = tegra210_xbar_routes,
	.num_dapm_widgets = ARRAY_SIZE(tegra210_xbar_widgets),
	.num_dapm_routes = ARRAY_SIZE(tegra210_xbar_routes),
	.idle_bias_off = 1,
};
#endif
static const struct tegra210_xbar_soc_data soc_data_tegra210 = {
	.regmap_config = &tegra210_xbar_regmap_config,
	.mask[0] = TEGRA210_XBAR_REG_MASK_0,
	.mask[1] = TEGRA210_XBAR_REG_MASK_1,
	.mask[2] = TEGRA210_XBAR_REG_MASK_2,
	.mask[3] = TEGRA210_XBAR_REG_MASK_3,
	.reg_count = TEGRA210_XBAR_UPDATE_MAX_REG,
	.reg_offset = TEGRA210_XBAR_PART1_RX,
};

static const struct of_device_id tegra210_xbar_of_match[] = {
	{ .compatible = "nvidia,tegra210-axbar", .data = &soc_data_tegra210 },
	{},
};

static const struct {
	const char *clk_name;
} configlink_clocks[] = {
};

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
static struct of_dev_auxdata tegra210_xbar_auxdata[] = {
	OF_DEV_AUXDATA("nvidia,tegra210-admaif", ADMAIF_BASE_ADDR, "tegra210-admaif", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-i2s", I2S1_BASE_ADDR, "tegra210-i2s.0", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-i2s", I2S2_BASE_ADDR, "tegra210-i2s.1", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-i2s", I2S3_BASE_ADDR, "tegra210-i2s.2", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-i2s", I2S4_BASE_ADDR, "tegra210-i2s.3", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-i2s", I2S5_BASE_ADDR, "tegra210-i2s.4", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-amx", AMX1_BASE_ADDR, "tegra210-amx.0", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-amx", AMX2_BASE_ADDR, "tegra210-amx.1", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-adx", ADX1_BASE_ADDR, "tegra210-adx.0", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-adx", ADX2_BASE_ADDR, "tegra210-adx.1", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-afc", AFC1_BASE_ADDR, "tegra210-afc.0", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-afc", AFC2_BASE_ADDR, "tegra210-afc.1", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-afc", AFC3_BASE_ADDR, "tegra210-afc.2", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-afc", AFC4_BASE_ADDR, "tegra210-afc.3", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-afc", AFC5_BASE_ADDR, "tegra210-afc.4", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-afc", AFC6_BASE_ADDR, "tegra210-afc.5", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-sfc", SFC1_BASE_ADDR, "tegra210-sfc.0", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-sfc", SFC2_BASE_ADDR, "tegra210-sfc.1", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-sfc", SFC3_BASE_ADDR, "tegra210-sfc.2", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-sfc", SFC4_BASE_ADDR, "tegra210-sfc.3", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-mvc", MVC1_BASE_ADDR, "tegra210-mvc.0", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-mvc", MVC2_BASE_ADDR, "tegra210-mvc.1", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-iqc", IQC1_BASE_ADDR, "tegra210-iqc.0", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-iqc", IQC2_BASE_ADDR, "tegra210-iqc.1", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-dmic", DMIC1_BASE_ADDR, "tegra210-dmic.0", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-dmic", DMIC2_BASE_ADDR, "tegra210-dmic.1", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-dmic", DMIC3_BASE_ADDR, "tegra210-dmic.2", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-ope", OPE1_BASE_ADDR, "tegra210-ope.0", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-ope", OPE2_BASE_ADDR, "tegra210-ope.1", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-amixer", AMIXER1_BASE_ADDR, "tegra210-mixer", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-spdif", SPDIF1_BASE_ADDR, "tegra210-spdif", NULL),
	{}
};
#endif

int tegra210_xbar_set_clock(unsigned long rate)
{
	int ret = 0;

	ret = clk_set_rate(xbar->clk_parent, rate);
	if (ret)
		pr_info("Failed to set clock rate of pll_a_out0\n");

	ret = clk_set_rate(xbar->clk, rate);
	if (ret)
		pr_info("Failed to set clock rate of ahub\n");

	return 0;
}
EXPORT_SYMBOL_GPL(tegra210_xbar_set_clock);

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
static int tegra210_xbar_registration(struct platform_device *pdev)
{
	int ret;

	tegra210_xbar_codec.num_dapm_widgets = (NUM_MUX_WIDGETS * 3) +
				(NUM_DAIS - NUM_MUX_WIDGETS) * 2;
	tegra210_xbar_codec.num_dapm_routes =
		(NUM_DAIS - NUM_MUX_WIDGETS) * 2 +
		(NUM_MUX_WIDGETS * NUM_MUX_INPUT);

	ret = snd_soc_register_codec(&pdev->dev, &tegra210_xbar_codec,
				tegra210_xbar_dais, NUM_DAIS);

	if (ret != 0) {
		dev_err(&pdev->dev, "Could not register CODEC: %d\n", ret);
		return -EBUSY;
	}

	of_platform_populate(pdev->dev.of_node, NULL, tegra210_xbar_auxdata,
			     &pdev->dev);

	return 0;
}
#endif

static int tegra210_xbar_probe(struct platform_device *pdev)
{
	struct clk *clk;
	void __iomem *regs;
	int ret, i;
	const struct of_device_id *match;
	struct tegra210_xbar_soc_data *soc_data;
	struct resource *res;
	struct clk *parent_clk;

	match = of_match_device(tegra210_xbar_of_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "Error: No device match found\n");
		ret = -ENODEV;
		goto err;
	}
	soc_data = (struct tegra210_xbar_soc_data *)match->data;

	/*
	 * The TEGRA APE XBAR client a register bus: the "configlink".
	 * For this to operate correctly, all devices on this bus must
	 * be out of reset.
	 * Ensure that here.
	 */
	for (i = 0; i < ARRAY_SIZE(configlink_clocks); i++) {
		clk = devm_clk_get(&pdev->dev, configlink_clocks[i].clk_name);
		if (IS_ERR(clk)) {
			dev_err(&pdev->dev, "Can't get clock %s\n",
				configlink_clocks[i].clk_name);
			ret = PTR_ERR(clk);
			goto err;
		}
		tegra_periph_reset_deassert(clk);
		devm_clk_put(&pdev->dev, clk);
	}

	xbar = devm_kzalloc(&pdev->dev, sizeof(*xbar), GFP_KERNEL);
	if (!xbar) {
		dev_err(&pdev->dev, "Can't allocate xbar\n");
		ret = -ENOMEM;
		goto err;
	}

	xbar->soc_data = soc_data;

	if (!(tegra_platform_is_unit_fpga() || tegra_platform_is_fpga())) {
		xbar->clk = devm_clk_get(&pdev->dev, "ahub");
		if (IS_ERR(xbar->clk)) {
			dev_err(&pdev->dev, "Can't retrieve ahub clock\n");
			ret = PTR_ERR(xbar->clk);
			goto err;
		}

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
		xbar->clk_parent = clk_get_sys(NULL, "pll_a_out0");
#else
		xbar->clk_parent = devm_clk_get(&pdev->dev, "pll_a_out0");
#endif
		if (IS_ERR(xbar->clk_parent)) {
			dev_err(&pdev->dev, "Can't retrieve pll_a_out0 clock\n");
			ret = PTR_ERR(xbar->clk_parent);
			goto err_clk_put;
		}

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
		xbar->clk_ape = clk_get_sys(NULL, "xbar.ape");
		if (IS_ERR(xbar->clk_ape)) {
			dev_err(&pdev->dev, "Can't retrieve ape clock\n");
			ret = PTR_ERR(xbar->clk_ape);
			goto err_clk_put_parent;
		}
#else
		xbar->clk_apb2ape = devm_clk_get(&pdev->dev, "apb2ape");
		if (IS_ERR(xbar->clk_apb2ape)) {
			dev_err(&pdev->dev, "Can't retrieve apb2ape clock\n");
			ret = PTR_ERR(xbar->clk_apb2ape);
			goto err_clk_put_parent;
		}

		xbar->clk_ape = devm_clk_get(&pdev->dev, "xbar.ape");
		if (IS_ERR(xbar->clk_ape)) {
			dev_err(&pdev->dev, "Can't retrieve ape clock\n");
			ret = PTR_ERR(xbar->clk_ape);
			goto err_clk_put_apb2ape;
		}
#endif
	}

	parent_clk = clk_get_parent(xbar->clk);
	if (IS_ERR(parent_clk)) {
		dev_err(&pdev->dev, "Can't get parent clock for xbar\n");
		ret = PTR_ERR(parent_clk);
		goto err_clk_put_ape;
	}

	if (!(tegra_platform_is_unit_fpga() || tegra_platform_is_fpga())) {
		ret = clk_set_parent(xbar->clk, xbar->clk_parent);
		if (ret) {
			dev_err(&pdev->dev, "Failed to set parent clock with pll_a_out0\n");
			goto err_clk_put_ape;
		}
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "No memory resource for admaif\n");
		ret = -ENODEV;
		goto err;
	}

	regs = devm_ioremap_resource(&pdev->dev, res);
	if (!regs) {
		dev_err(&pdev->dev, "request/iomap region failed\n");
		ret = -ENODEV;
		goto err_clk_set_parent;
	}

	xbar->regmap = devm_regmap_init_mmio(&pdev->dev, regs,
					     soc_data->regmap_config);
	if (IS_ERR(xbar->regmap)) {
		dev_err(&pdev->dev, "regmap init failed\n");
		ret = PTR_ERR(xbar->regmap);
		goto err_clk_set_parent;
	}
	regcache_cache_only(xbar->regmap, true);

	tegra_pd_add_device(&pdev->dev);

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		ret = tegra210_xbar_runtime_resume(&pdev->dev);
		if (ret)
			goto err_pm_disable;
	}

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	ret = tegra210_xbar_registration(pdev);
#else
	ret = tegra186_xbar_registration(pdev);
#endif
	if (ret == -EBUSY)
		goto err_suspend;

	return 0;

err_suspend:
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra210_xbar_runtime_suspend(&pdev->dev);
err_pm_disable:
	pm_runtime_disable(&pdev->dev);
	tegra_pd_remove_device(&pdev->dev);
err_clk_set_parent:
	clk_set_parent(xbar->clk, parent_clk);
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
err_clk_put_ape:
	clk_put(xbar->clk_ape);
err_clk_put_parent:
	if (!(tegra_platform_is_unit_fpga() || tegra_platform_is_fpga()))
		clk_put(xbar->clk_parent);
#else
err_clk_put_ape:
	devm_clk_put(&pdev->dev, xbar->clk_ape);
err_clk_put_apb2ape:
	devm_clk_put(&pdev->dev, xbar->clk_apb2ape);
err_clk_put_parent:
	if (!(tegra_platform_is_unit_fpga() || tegra_platform_is_fpga()))
		devm_clk_put(&pdev->dev, xbar->clk_parent);
#endif
err_clk_put:
	devm_clk_put(&pdev->dev, xbar->clk);
err:
	return ret;
}

static int tegra210_xbar_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra210_xbar_runtime_suspend(&pdev->dev);

	tegra_pd_remove_device(&pdev->dev);

	devm_clk_put(&pdev->dev, xbar->clk);
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	clk_put(xbar->clk_parent);
	clk_put(xbar->clk_ape);
#else
	devm_clk_put(&pdev->dev, xbar->clk_parent);
	devm_clk_put(&pdev->dev, xbar->clk_ape);
	devm_clk_put(&pdev->dev, xbar->clk_apb2ape);
#endif

	return 0;
}

static const struct dev_pm_ops tegra210_xbar_pm_ops = {
	SET_RUNTIME_PM_OPS(tegra210_xbar_runtime_suspend,
			   tegra210_xbar_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(tegra210_xbar_suspend, NULL)
};

static struct platform_driver tegra210_xbar_driver = {
	.probe = tegra210_xbar_probe,
	.remove = tegra210_xbar_remove,
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = tegra210_xbar_of_match,
		.pm = &tegra210_xbar_pm_ops,
	},
};
module_platform_driver(tegra210_xbar_driver);

void tegra210_xbar_set_cif(struct regmap *regmap, unsigned int reg,
			  struct tegra210_xbar_cif_conf *conf)
{
	unsigned int value;

	value = (conf->threshold <<
			TEGRA210_AUDIOCIF_CTRL_FIFO_THRESHOLD_SHIFT) |
		((conf->audio_channels - 1) <<
			TEGRA210_AUDIOCIF_CTRL_AUDIO_CHANNELS_SHIFT) |
		((conf->client_channels - 1) <<
			TEGRA210_AUDIOCIF_CTRL_CLIENT_CHANNELS_SHIFT) |
		(conf->audio_bits <<
			TEGRA210_AUDIOCIF_CTRL_AUDIO_BITS_SHIFT) |
		(conf->client_bits <<
			TEGRA210_AUDIOCIF_CTRL_CLIENT_BITS_SHIFT) |
		(conf->expand <<
			TEGRA210_AUDIOCIF_CTRL_EXPAND_SHIFT) |
		(conf->stereo_conv <<
			TEGRA210_AUDIOCIF_CTRL_STEREO_CONV_SHIFT) |
		(conf->replicate <<
			TEGRA210_AUDIOCIF_CTRL_REPLICATE_SHIFT) |
		(conf->truncate <<
			TEGRA210_AUDIOCIF_CTRL_TRUNCATE_SHIFT) |
		(conf->mono_conv <<
			TEGRA210_AUDIOCIF_CTRL_MONO_CONV_SHIFT);

	regmap_update_bits(regmap, reg, 0x3fffffff, value);
}
EXPORT_SYMBOL_GPL(tegra210_xbar_set_cif);

void tegra210_xbar_write_ahubram(struct regmap *regmap, unsigned int reg_ctrl,
				unsigned int reg_data, unsigned int ram_offset,
				unsigned int *data, size_t size)
{
	unsigned int val = 0;
	int i = 0;

	val = (ram_offset << TEGRA210_AHUBRAMCTL_CTRL_RAM_ADDR_SHIFT) &
		TEGRA210_AHUBRAMCTL_CTRL_RAM_ADDR_MASK;
	val |= TEGRA210_AHUBRAMCTL_CTRL_ADDR_INIT_EN;
	val |= TEGRA210_AHUBRAMCTL_CTRL_SEQ_ACCESS_EN;
	val |= TEGRA210_AHUBRAMCTL_CTRL_RW_WRITE;

	regmap_write(regmap, reg_ctrl, val);
	for (i = 0; i < size; i++)
		regmap_write(regmap, reg_data, data[i]);

	return;
}
EXPORT_SYMBOL_GPL(tegra210_xbar_write_ahubram);

void tegra210_xbar_read_ahubram(struct regmap *regmap, unsigned int reg_ctrl,
				unsigned int reg_data, unsigned int ram_offset,
				unsigned int *data, size_t size)
{
	unsigned int val = 0;
	int i = 0;

	val = (ram_offset << TEGRA210_AHUBRAMCTL_CTRL_RAM_ADDR_SHIFT) &
		TEGRA210_AHUBRAMCTL_CTRL_RAM_ADDR_MASK;
	val |= TEGRA210_AHUBRAMCTL_CTRL_ADDR_INIT_EN;
	val |= TEGRA210_AHUBRAMCTL_CTRL_SEQ_ACCESS_EN;
	val |= TEGRA210_AHUBRAMCTL_CTRL_RW_READ;

	regmap_write(regmap, reg_ctrl, val);
	/* Since all ahub non-io modules work under same ahub clock it is not
	   necessary to check ahub read busy bit after every read */
	for (i = 0; i < size; i++)
		regmap_read(regmap, reg_data, &data[i]);

	return;
}
EXPORT_SYMBOL_GPL(tegra210_xbar_read_ahubram);

int tegra210_xbar_read_reg (unsigned int reg, unsigned int *val)
{
	int ret;

	ret = regmap_read (xbar->regmap, reg, val);
	return ret;
}
EXPORT_SYMBOL_GPL(tegra210_xbar_read_reg);

MODULE_AUTHOR("Stephen Warren <swarren@nvidia.com>");
MODULE_DESCRIPTION("Tegra210 XBAR driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
