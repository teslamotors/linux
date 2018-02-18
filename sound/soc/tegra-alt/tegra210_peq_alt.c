/*
 * tegra210_peq_alt.c - Tegra210 PEQ driver
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

#include "tegra210_xbar_alt.h"
#include "tegra210_ope_alt.h"
#include "tegra210_peq_alt.h"

static const struct reg_default tegra210_peq_reg_defaults[] = {
	{ TEGRA210_PEQ_CONFIG, 0x00000013},
	{ TEGRA210_PEQ_AHUBRAMCTL_CONFIG_RAM_CTRL, 0x00004000},
	{ TEGRA210_PEQ_AHUBRAMCTL_CONFIG_RAM_SHIFT_CTRL, 0x00004000},
};

/* Default PEQ filter parameters for a 5-stage biquad*/
static const int biquad_init_stage = 5;
static const u32 biquad_init_gains[TEGRA210_PEQ_GAIN_PARAM_SIZE_PER_CH] = {
	1495012349, /* pre-gain */
	/* Gains : b0, b1, a0, a1, a2 */
	536870912, -1073741824, 536870912, 2143508246, -1069773768, /* band-0 */
	134217728, -265414508, 131766272, 2140402222, -1071252997, /* band-1 */
	268435456, -233515765, -33935948, 1839817267, -773826124, /* band-2 */
	536870912, -672537913, 139851540, 1886437554, -824433167, /* band-3 */
	268435456, -114439279, 173723964, 205743566, 278809729, /* band-4 */
	1, 0, 0, 0, 0, /* band-5 */
	1, 0, 0, 0, 0, /* band-6 */
	1, 0, 0, 0, 0, /* band-7 */
	1, 0, 0, 0, 0, /* band-8 */
	1, 0, 0, 0, 0, /* band-9 */
	1, 0, 0, 0, 0, /* band-10 */
	1, 0, 0, 0, 0, /* band-11 */
	963423114, /* post-gain */
};

static const u32 biquad_init_shifts[TEGRA210_PEQ_SHIFT_PARAM_SIZE_PER_CH] = {
	23, /* pre-shift */
	30, 30, 30, 30, 30, 0, 0, 0, 0, 0, 0, 0, /* shift for bands */
	28, /* post-shift */
};

static int tegra210_peq_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_ope *ope = snd_soc_codec_get_drvdata(codec);
	unsigned int mask = (1 << fls(mc->max)) - 1;
	unsigned int val;

	regmap_read(ope->peq_regmap, mc->reg, &val);
	ucontrol->value.integer.value[0] = (val >> mc->shift) & mask;
	if (mc->invert)
		ucontrol->value.integer.value[0] =
			mc->max - ucontrol->value.integer.value[0];

	return 0;
}

static int tegra210_peq_put(struct snd_kcontrol *kcontrol,
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

	return regmap_update_bits(ope->peq_regmap, mc->reg,
				(mask << mc->shift), val);
}

static int tegra210_peq_ahub_ram_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tegra_soc_bytes *params = (void *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	u32 *data = (u32 *)ucontrol->value.bytes.data;

	memset(data, 0, params->soc.num_regs * codec->component.val_bytes);
	return 0;
}

static int tegra210_peq_ahub_ram_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tegra_soc_bytes *params = (void *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra210_ope *ope = snd_soc_codec_get_drvdata(codec);
	u32 reg_ctrl = params->soc.base;
	u32 reg_data = reg_ctrl + codec->component.val_bytes;
	u32 *data = (u32 *)ucontrol->value.bytes.data;

	tegra210_xbar_write_ahubram(ope->peq_regmap, reg_ctrl, reg_data,
				    params->shift, data, params->soc.num_regs);
	return 0;
}

#define TEGRA210_PEQ_GAIN_PARAMS_CTRL(chan) \
	TEGRA_SOC_BYTES_EXT("peq channel" #chan " biquad gain params", \
		TEGRA210_PEQ_AHUBRAMCTL_CONFIG_RAM_CTRL, \
		TEGRA210_PEQ_GAIN_PARAM_SIZE_PER_CH, \
		(TEGRA210_PEQ_GAIN_PARAM_SIZE_PER_CH * chan), 0xffffffff, \
		tegra210_peq_ahub_ram_get, tegra210_peq_ahub_ram_put)

#define TEGRA210_PEQ_SHIFT_PARAMS_CTRL(chan) \
	TEGRA_SOC_BYTES_EXT("peq channel" #chan " biquad shift params", \
		TEGRA210_PEQ_AHUBRAMCTL_CONFIG_RAM_SHIFT_CTRL, \
		TEGRA210_PEQ_SHIFT_PARAM_SIZE_PER_CH, \
		(TEGRA210_PEQ_SHIFT_PARAM_SIZE_PER_CH * chan), 0x1f, \
		tegra210_peq_ahub_ram_get, tegra210_peq_ahub_ram_put)

static const struct snd_kcontrol_new tegra210_peq_controls[] = {
	SOC_SINGLE_EXT("peq active", TEGRA210_PEQ_CONFIG,
		TEGRA210_PEQ_CONFIG_MODE_SHIFT, 1, 0,
		tegra210_peq_get, tegra210_peq_put),
	SOC_SINGLE_EXT("peq biquad stages", TEGRA210_PEQ_CONFIG,
		TEGRA210_PEQ_CONFIG_BIQUAD_STAGES_SHIFT,
		TEGRA210_PEQ_MAX_BIQUAD_STAGES - 1, 0,
		tegra210_peq_get, tegra210_peq_put),

	TEGRA210_PEQ_GAIN_PARAMS_CTRL(0),
	TEGRA210_PEQ_GAIN_PARAMS_CTRL(1),

	TEGRA210_PEQ_SHIFT_PARAMS_CTRL(0),
	TEGRA210_PEQ_SHIFT_PARAMS_CTRL(1),
};

static bool tegra210_peq_wr_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA210_PEQ_SOFT_RESET:
	case TEGRA210_PEQ_CG:
	case TEGRA210_PEQ_CONFIG:
	case TEGRA210_PEQ_AHUBRAMCTL_CONFIG_RAM_CTRL:
	case TEGRA210_PEQ_AHUBRAMCTL_CONFIG_RAM_DATA:
	case TEGRA210_PEQ_AHUBRAMCTL_CONFIG_RAM_SHIFT_CTRL:
	case TEGRA210_PEQ_AHUBRAMCTL_CONFIG_RAM_SHIFT_DATA:
		return true;
	default:
		return false;
	};
}

static bool tegra210_peq_rd_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA210_PEQ_SOFT_RESET:
	case TEGRA210_PEQ_CG:
	case TEGRA210_PEQ_STATUS:
	case TEGRA210_PEQ_CONFIG:
	case TEGRA210_PEQ_AHUBRAMCTL_CONFIG_RAM_CTRL:
	case TEGRA210_PEQ_AHUBRAMCTL_CONFIG_RAM_DATA:
	case TEGRA210_PEQ_AHUBRAMCTL_CONFIG_RAM_SHIFT_CTRL:
	case TEGRA210_PEQ_AHUBRAMCTL_CONFIG_RAM_SHIFT_DATA:
		return true;
	default:
		return false;
	};
}

static bool tegra210_peq_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA210_PEQ_SOFT_RESET:
	case TEGRA210_PEQ_STATUS:
	case TEGRA210_PEQ_AHUBRAMCTL_CONFIG_RAM_CTRL:
	case TEGRA210_PEQ_AHUBRAMCTL_CONFIG_RAM_DATA:
	case TEGRA210_PEQ_AHUBRAMCTL_CONFIG_RAM_SHIFT_CTRL:
	case TEGRA210_PEQ_AHUBRAMCTL_CONFIG_RAM_SHIFT_DATA:
		return true;
	default:
		return false;
	};
}

static bool tegra210_peq_precious_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA210_PEQ_AHUBRAMCTL_CONFIG_RAM_DATA:
	case TEGRA210_PEQ_AHUBRAMCTL_CONFIG_RAM_SHIFT_DATA:
		return true;
	default:
		return false;
	};
}

static const struct regmap_config tegra210_peq_regmap_config = {
	.name = "peq",
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = TEGRA210_PEQ_AHUBRAMCTL_CONFIG_RAM_SHIFT_DATA,
	.writeable_reg = tegra210_peq_wr_reg,
	.readable_reg = tegra210_peq_rd_reg,
	.volatile_reg = tegra210_peq_volatile_reg,
	.precious_reg = tegra210_peq_precious_reg,
	.reg_defaults = tegra210_peq_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(tegra210_peq_reg_defaults),
	.cache_type = REGCACHE_FLAT,
};

int tegra210_peq_codec_init(struct snd_soc_codec *codec)
{
	struct tegra210_ope *ope = snd_soc_codec_get_drvdata(codec);
	int i = 0;

	pm_runtime_get_sync(codec->dev);
	regmap_update_bits(ope->peq_regmap, TEGRA210_PEQ_CONFIG,
		TEGRA210_PEQ_CONFIG_MODE_MASK,
		0 << TEGRA210_PEQ_CONFIG_MODE_SHIFT);
	regmap_update_bits(ope->peq_regmap, TEGRA210_PEQ_CONFIG,
		TEGRA210_PEQ_CONfIG_BIQUAD_STAGES_MASK,
		(biquad_init_stage - 1) <<
		TEGRA210_PEQ_CONFIG_BIQUAD_STAGES_SHIFT);

	/* Initialize PEQ AHUB RAM with default params */
	for (i = 0; i < TEGRA210_PEQ_MAX_CHANNELS; i++) {
		/* Set default gain params */
		tegra210_xbar_write_ahubram(ope->peq_regmap,
			TEGRA210_PEQ_AHUBRAMCTL_CONFIG_RAM_CTRL,
			TEGRA210_PEQ_AHUBRAMCTL_CONFIG_RAM_DATA,
			(i * TEGRA210_PEQ_GAIN_PARAM_SIZE_PER_CH),
			(u32 *)&biquad_init_gains,
			TEGRA210_PEQ_GAIN_PARAM_SIZE_PER_CH);

		/* Set default shift params */
		tegra210_xbar_write_ahubram(ope->peq_regmap,
			TEGRA210_PEQ_AHUBRAMCTL_CONFIG_RAM_SHIFT_CTRL,
			TEGRA210_PEQ_AHUBRAMCTL_CONFIG_RAM_SHIFT_DATA,
			(i * TEGRA210_PEQ_SHIFT_PARAM_SIZE_PER_CH),
			(u32 *)&biquad_init_shifts,
			TEGRA210_PEQ_SHIFT_PARAM_SIZE_PER_CH);
	}
	pm_runtime_put_sync(codec->dev);

	snd_soc_add_codec_controls(codec, tegra210_peq_controls,
		ARRAY_SIZE(tegra210_peq_controls));
	return 0;
}
EXPORT_SYMBOL_GPL(tegra210_peq_codec_init);

int tegra210_peq_init(struct platform_device *pdev, int id)
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

	ope->peq_regmap = devm_regmap_init_mmio(&pdev->dev, regs,
					    &tegra210_peq_regmap_config);
	if (IS_ERR(ope->peq_regmap)) {
		dev_err(&pdev->dev, "regmap init failed\n");
		ret = PTR_ERR(ope->peq_regmap);
		goto err;
	}

	return 0;
err:
	return ret;
}
EXPORT_SYMBOL_GPL(tegra210_peq_init);

MODULE_AUTHOR("Sumit Bhattacharya <sumitb@nvidia.com>");
MODULE_DESCRIPTION("Tegra210 PEQ module");
MODULE_LICENSE("GPL");
