/*
 * tegra210_afc_alt.c - Tegra210 AFC driver
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

#include "tegra210_xbar_alt.h"
#include "tegra210_afc_alt.h"
#if defined(CONFIG_ARCH_TEGRA_18x_SOC)
#include <sound/tegra_audio.h>
#endif

#define DRV_NAME "tegra210-afc"

static const struct reg_default tegra210_afc_reg_defaults[] = {
	{ TEGRA210_AFC_AXBAR_RX_CIF_CTRL, 0x00007700},
	{ TEGRA210_AFC_AXBAR_TX_INT_MASK, 0x00000001},
	{ TEGRA210_AFC_AXBAR_TX_CIF_CTRL, 0x00007000},
	{ TEGRA210_AFC_CG, 0x1},
	{ TEGRA210_AFC_INT_MASK, 0x1},
	{ TEGRA210_AFC_DEST_I2S_PARAMS, 0x01190e0c },
	{ TEGRA210_AFC_TXCIF_FIFO_PARAMS, 0x00190e0c },
	{ TEGRA210_AFC_CLK_PPM_DIFF, 0x0000001e},
	{ TEGRA210_AFC_LCOEF_1_4_0, 0x0000002e},
	{ TEGRA210_AFC_LCOEF_1_4_1, 0x0000f9e6},
	{ TEGRA210_AFC_LCOEF_1_4_2, 0x000020ca},
	{ TEGRA210_AFC_LCOEF_1_4_3, 0x00007147},
	{ TEGRA210_AFC_LCOEF_1_4_4, 0x0000f17e},
	{ TEGRA210_AFC_LCOEF_1_4_5, 0x000001e0},
	{ TEGRA210_AFC_LCOEF_2_4_0, 0x00000117},
	{ TEGRA210_AFC_LCOEF_2_4_1, 0x0000f26b},
	{ TEGRA210_AFC_LCOEF_2_4_2, 0x00004c07},
};

static int tegra210_afc_runtime_suspend(struct device *dev)
{
	struct tegra210_afc *afc = dev_get_drvdata(dev);

	regcache_cache_only(afc->regmap, true);
	regcache_mark_dirty(afc->regmap);

	pm_runtime_put_sync(dev->parent);

	return 0;
}

static int tegra210_afc_runtime_resume(struct device *dev)
{
	struct tegra210_afc *afc = dev_get_drvdata(dev);
	int ret;

	ret = pm_runtime_get_sync(dev->parent);
	if (ret < 0) {
		dev_err(dev, "parent get_sync failed: %d\n", ret);
		return ret;
	}

	regcache_cache_only(afc->regmap, false);
	regcache_sync(afc->regmap);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tegra210_afc_suspend(struct device *dev)
{
	return 0;
}
#endif

static void tegra210_afc_set_ppm_diff(struct tegra210_afc *afc,
			unsigned int ppm_diff)
{
	regmap_update_bits(afc->regmap,
		TEGRA210_AFC_CLK_PPM_DIFF, 0xFFFF, ppm_diff);
}

/* returns the id if SFC is connected along the AFC src path */
unsigned int tegra210_afc_get_sfc_id(unsigned int afc_id)
{
	unsigned int reg, val = 0;

	reg = TEGRA210_XBAR_PART0_RX +
		TEGRA210_XBAR_RX_STRIDE *
		(afc_id + XBAR_AFC_REG_OFFSET_DIVIDED_BY_4);

	tegra210_xbar_read_reg(reg, &val);
	val = val >> 24;

	return val;
}
EXPORT_SYMBOL_GPL(tegra210_afc_get_sfc_id);

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
/* returns the destination I2S id connected along the AFC path */
static unsigned int tegra210_afc_get_i2s_id(unsigned int afc_id)
{
	unsigned int i2s_reg, i2s_val, amx_reg, amx_val, i, j;

	for (i = 1; i < MAX_NUM_I2S; i++) {
		i2s_val = 0;
		i2s_reg = TEGRA210_XBAR_PART1_RX +
			TEGRA210_XBAR_RX_STRIDE * (i + 0xF);
		tegra210_xbar_read_reg(i2s_reg, &i2s_val);
		if ((i2s_val >> 24) & (1 << afc_id)) {
			return i;
		} else if (i2s_val & MASK_AMX_TX) {
			for (j = 1; j < 9; j++) { /* AMX1/2 */
				amx_val = 0;
				amx_reg = TEGRA210_XBAR_PART1_RX +
					TEGRA210_XBAR_RX_STRIDE * (j + 0x4F);
				tegra210_xbar_read_reg(amx_reg, &amx_val);
				if ((amx_val >> 24) & (1 << afc_id))
					return i;
			}
		}
	}
	return 0;
}

static int tegra210_afc_set_thresholds(struct tegra210_afc *afc,
				unsigned int afc_id)
{
	unsigned int i2s_id, value;

	if (tegra210_afc_get_sfc_id(afc_id)) {
		/* TODO program thresholds using SRC_BURST */
	} else {
		value = 4 << TEGRA210_AFC_FIFO_HIGH_THRESHOLD_SHIFT;
		value |= 3 << TEGRA210_AFC_FIFO_START_THRESHOLD_SHIFT;
		value |= 2;
	}
	regmap_write(afc->regmap, TEGRA210_AFC_TXCIF_FIFO_PARAMS, value);

	i2s_id = tegra210_afc_get_i2s_id(afc_id);
	if (!i2s_id)
		return -EINVAL;

	value |= CONFIG_AFC_DEST_PARAM(0, i2s_id);
	SET_AFC_DEST_PARAM(value);

	return 0;
}
#endif

static int tegra210_afc_set_audio_cif(struct tegra210_afc *afc,
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

	afc->soc_data->set_audio_cif(afc->regmap, reg, &cif_conf);

	return 0;
}

static int tegra210_afc_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct tegra210_afc *afc = snd_soc_dai_get_drvdata(dai);
	int ret;

	/* set RX cif and TX cif */
	ret = tegra210_afc_set_audio_cif(afc, params,
				TEGRA210_AFC_AXBAR_RX_CIF_CTRL);
	if (ret) {
		dev_err(dev, "Can't set AFC RX CIF: %d\n", ret);
		return ret;
	}
	ret = tegra210_afc_set_audio_cif(afc, params,
				TEGRA210_AFC_AXBAR_TX_CIF_CTRL);
	if (ret) {
		dev_err(dev, "Can't set AFC TX CIF: %d\n", ret);
		return ret;
	}

	tegra210_afc_set_ppm_diff(afc, AFC_CLK_PPM_DIFF);

	/* program the thresholds, destn i2s id, PPM values */
	if (tegra210_afc_set_thresholds(afc, dev->id) == -EINVAL)
		dev_err(dev, "Can't set AFC threshold: %d\n", ret);

	return ret;

}

static int tegra210_afc_codec_probe(struct snd_soc_codec *codec)
{
	struct tegra210_afc *afc = snd_soc_codec_get_drvdata(codec);

	codec->control_data = afc->regmap;

	return 0;
}

static struct snd_soc_dai_ops tegra210_afc_dai_ops = {
	.hw_params	= tegra210_afc_hw_params,
};

static struct snd_soc_dai_driver tegra210_afc_dais[] = {
	{
		.name = "AFC IN",
		.playback = {
			.stream_name = "AFC Receive",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_96000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
	{
		.name = "AFC OUT",
		.capture = {
			.stream_name = "AFC Transmit",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_96000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &tegra210_afc_dai_ops,
	}
};

static const struct snd_soc_dapm_widget tegra210_afc_widgets[] = {
	SND_SOC_DAPM_AIF_IN("AFC RX", NULL, 0, SND_SOC_NOPM,
				0, 0),
	SND_SOC_DAPM_AIF_OUT("AFC TX", NULL, 0, TEGRA210_AFC_ENABLE,
				TEGRA210_AFC_EN_SHIFT, 0),
};

static const struct snd_soc_dapm_route tegra210_afc_routes[] = {
	{ "AFC RX",       NULL, "AFC Receive" },
	{ "AFC TX",       NULL, "AFC RX" },
	{ "AFC Transmit", NULL, "AFC TX" },
};

static struct snd_soc_codec_driver tegra210_afc_codec = {
	.probe = tegra210_afc_codec_probe,
	.dapm_widgets = tegra210_afc_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tegra210_afc_widgets),
	.dapm_routes = tegra210_afc_routes,
	.num_dapm_routes = ARRAY_SIZE(tegra210_afc_routes),
	.idle_bias_off = 1,
};

static bool tegra210_afc_wr_rd_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA210_AFC_AXBAR_RX_STATUS:
	case TEGRA210_AFC_AXBAR_RX_CIF_CTRL:
	case TEGRA210_AFC_AXBAR_RX_CYA:
	case TEGRA210_AFC_AXBAR_TX_STATUS:
	case TEGRA210_AFC_AXBAR_TX_INT_STATUS:
	case TEGRA210_AFC_AXBAR_TX_INT_MASK:
	case TEGRA210_AFC_AXBAR_TX_INT_SET:
	case TEGRA210_AFC_AXBAR_TX_INT_CLEAR:
	case TEGRA210_AFC_AXBAR_TX_CIF_CTRL:
	case TEGRA210_AFC_AXBAR_TX_CYA:
	case TEGRA210_AFC_ENABLE:
	case TEGRA210_AFC_SOFT_RESET:
	case TEGRA210_AFC_CG:
	case TEGRA210_AFC_STATUS:
	case TEGRA210_AFC_INT_STATUS:
	case TEGRA210_AFC_INT_MASK:
	case TEGRA210_AFC_INT_SET:
	case TEGRA210_AFC_INT_CLEAR:
	case TEGRA210_AFC_DEST_I2S_PARAMS:
	case TEGRA210_AFC_TXCIF_FIFO_PARAMS:
	case TEGRA210_AFC_CLK_PPM_DIFF:
	case TEGRA210_AFC_DBG_CTRL:
	case TEGRA210_AFC_TOTAL_SAMPLES:
	case TEGRA210_AFC_DECIMATION_SAMPLES:
	case TEGRA210_AFC_INTERPOLATION_SAMPLES:
	case TEGRA210_AFC_DBG_INTERNAL:
	case TEGRA210_AFC_LCOEF_1_4_0:
	case TEGRA210_AFC_LCOEF_1_4_1:
	case TEGRA210_AFC_LCOEF_1_4_2:
	case TEGRA210_AFC_LCOEF_1_4_3:
	case TEGRA210_AFC_LCOEF_1_4_4:
	case TEGRA210_AFC_LCOEF_1_4_5:
	case TEGRA210_AFC_LCOEF_2_4_0:
	case TEGRA210_AFC_LCOEF_2_4_1:
	case TEGRA210_AFC_LCOEF_2_4_2:
	case TEGRA210_AFC_CYA:
		return true;
	default:
		return false;
	};
}

static bool tegra210_afc_volatile_reg(struct device *dev, unsigned int reg)
{
	return false;
}

static const struct regmap_config tegra210_afc_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = TEGRA210_AFC_CYA,
	.writeable_reg = tegra210_afc_wr_rd_reg,
	.readable_reg = tegra210_afc_wr_rd_reg,
	.volatile_reg = tegra210_afc_volatile_reg,
	.reg_defaults = tegra210_afc_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(tegra210_afc_reg_defaults),
	.cache_type = REGCACHE_FLAT,
};

static const struct tegra210_afc_soc_data soc_data_tegra210 = {
	.set_audio_cif = tegra210_xbar_set_cif,
};

static const struct of_device_id tegra210_afc_of_match[] = {
	{ .compatible = "nvidia,tegra210-afc", .data = &soc_data_tegra210 },
	{},
};

static int tegra210_afc_platform_probe(struct platform_device *pdev)
{
	struct tegra210_afc *afc;
	struct resource *mem, *memregion;
	void __iomem *regs;
	int ret = 0;
	const struct of_device_id *match;
	struct tegra210_afc_soc_data *soc_data;

	match = of_match_device(tegra210_afc_of_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "Error: No device match found\n");
		ret = -ENODEV;
		goto err;
	}
	soc_data = (struct tegra210_afc_soc_data *)match->data;

	afc = devm_kzalloc(&pdev->dev, sizeof(struct tegra210_afc), GFP_KERNEL);
	if (!afc) {
		dev_err(&pdev->dev, "Can't allocate afc\n");
		ret = -ENOMEM;
		goto err;
	}

	afc->soc_data = soc_data;

	/* initialize default destination I2S */
	afc->destination_i2s = 1;

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

	afc->regmap = devm_regmap_init_mmio(&pdev->dev, regs,
					    &tegra210_afc_regmap_config);
	if (IS_ERR(afc->regmap)) {
		dev_err(&pdev->dev, "regmap init failed\n");
		ret = PTR_ERR(afc->regmap);
		goto err;
	}
	regcache_cache_only(afc->regmap, true);

	if (of_property_read_u32(pdev->dev.of_node,
				"nvidia,ahub-afc-id",
				&pdev->dev.id) < 0) {
		dev_err(&pdev->dev,
			"Missing property nvidia,ahub-afc-id\n");
		ret = -ENODEV;
		goto err;
	}

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		ret = tegra210_afc_runtime_resume(&pdev->dev);
		if (ret)
			goto err_pm_disable;
	}

	/* Disable SLGC */
	regmap_write(afc->regmap, TEGRA210_AFC_CG, 0);

	ret = snd_soc_register_codec(&pdev->dev, &tegra210_afc_codec,
				     tegra210_afc_dais,
				     ARRAY_SIZE(tegra210_afc_dais));
	if (ret != 0) {
		dev_err(&pdev->dev, "Could not register CODEC: %d\n", ret);
		goto err_suspend;
	}

	dev_set_drvdata(&pdev->dev, afc);

	return 0;

err_suspend:
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra210_afc_runtime_suspend(&pdev->dev);
err_pm_disable:
	pm_runtime_disable(&pdev->dev);
err:
	return ret;
}

static int tegra210_afc_platform_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra210_afc_runtime_suspend(&pdev->dev);

	return 0;
}

static const struct dev_pm_ops tegra210_afc_pm_ops = {
	SET_RUNTIME_PM_OPS(tegra210_afc_runtime_suspend,
			   tegra210_afc_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(tegra210_afc_suspend, NULL)
};

static struct platform_driver tegra210_afc_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = tegra210_afc_of_match,
		.pm = &tegra210_afc_pm_ops,
	},
	.probe = tegra210_afc_platform_probe,
	.remove = tegra210_afc_platform_remove,
};
module_platform_driver(tegra210_afc_driver)

MODULE_AUTHOR("Arun Shamanna Lakshmi <aruns@nvidia.com>");
MODULE_DESCRIPTION("Tegra210 AFC ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, tegra210_afc_of_match);
