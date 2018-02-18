/*
 * tegra124_afc_alt.c - Tegra124 AFC driver
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

#include "tegra30_xbar_alt.h"
#include "tegra124_afc_alt.h"

#define DRV_NAME "tegra124-afc"

static const struct reg_default tegra124_afc_reg_defaults[] = {
	{TEGRA_AFC_CTRL, 0x80000000},
	{TEGRA_AFC_THRESHOLDS_I2S, 0x00000000},
	{TEGRA_AFC_THRESHOLDS_AFC, 0x00000000},
	{TEGRA_AFC_FLOW_STEP_SIZE, 0x00000004},
	{TEGRA_AFC_AUDIOCIF_AFCTX_CTRL, 0x00001100},
	{TEGRA_AFC_AUDIOCIF_AFCRX_CTRL, 0x00001100},
	{TEGRA_AFC_FLOW_STATUS, 0x00000000},
	{TEGRA_AFC_LCOEF_1_4_0, 0x0000002e},
	{TEGRA_AFC_LCOEF_1_4_1, 0x0000f9e6},
	{TEGRA_AFC_LCOEF_1_4_2, 0x000020ca},
	{TEGRA_AFC_LCOEF_1_4_3, 0x00007147},
	{TEGRA_AFC_LCOEF_1_4_4, 0x0000f17e},
	{TEGRA_AFC_LCOEF_1_4_5, 0x000001e0},
	{TEGRA_AFC_LCOEF_2_4_0, 0x00000117},
	{TEGRA_AFC_LCOEF_2_4_1, 0x0000f26b},
	{TEGRA_AFC_LCOEF_2_4_2, 0x00004c07},
};

static int tegra124_afc_runtime_suspend(struct device *dev)
{
	struct tegra124_afc *afc = dev_get_drvdata(dev);

	regcache_cache_only(afc->regmap, true);
	clk_disable_unprepare(afc->clk_afc);

	return 0;
}

static int tegra124_afc_runtime_resume(struct device *dev)
{
	struct tegra124_afc *afc = dev_get_drvdata(dev);
	int ret;

	ret = clk_prepare_enable(afc->clk_afc);
	if (ret) {
		dev_err(dev, "clk_enable failed: %d\n", ret);
		return ret;
	}

	regcache_cache_only(afc->regmap, false);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tegra124_afc_suspend(struct device *dev)
{
	struct tegra124_afc *afc = dev_get_drvdata(dev);

	regcache_mark_dirty(afc->regmap);

	return 0;
}
#endif

/* returns the destination I2S id connected along the AFC path */
static unsigned int tegra124_afc_get_i2s_id(unsigned int afc_id)
{
	unsigned int i2s_reg[2], i2s_val[2], amx_reg, amx_val, i, j;

	for (i = 0; i < 5; i++) {
		i2s_val[0] = i2s_val[1] = 0;
		i2s_reg[0] = (TEGRA_AHUB_AUDIO_RX_STRIDE * (i + 0x4));
		i2s_reg[1] = (TEGRA_AHUB_AUDIO_RX1 +
			TEGRA_AHUB_AUDIO_RX_STRIDE * (i + 0x4));
		tegra30_xbar_read_reg(i2s_reg[0], &i2s_val[0]);
		tegra30_xbar_read_reg(i2s_reg[1], &i2s_val[1]);

		/* check if AFC is connected to I2S directly */
		if ((i2s_val[1] >> 5) & (1 << afc_id)) {
			return i;
		} else if (i2s_val[0] & BIT(20)) {
			/* if AFC is connected to AMX0 */
			for (j = 0; j < 4; j++) {
				amx_val = 0;
				amx_reg = (TEGRA_AHUB_AUDIO_RX1 +
					TEGRA_AHUB_AUDIO_RX_STRIDE * (j + 0x17));
				tegra30_xbar_read_reg(amx_reg, &amx_val);
				if ((amx_val >> 5) & (1 << afc_id))
					return i;
			}
		} else if (i2s_val[1] & BIT(0)) {
			/* if AFC is connected to AMX1 */
			for (j = 0; j < 4; j++) {
				amx_val = 0;
				amx_reg = (TEGRA_AHUB_AUDIO_RX1 +
					TEGRA_AHUB_AUDIO_RX_STRIDE * (j + 0x1E));
				tegra30_xbar_read_reg(amx_reg, &amx_val);
				if ((amx_val >> 5) & (1 << afc_id))
					return i;
			}
		}
	}
	return 0;
}

/* returns the id if DAM is connected along the AFC src path */
static unsigned int tegra124_afc_get_dam_id(unsigned int afc_id)
{
	unsigned int reg, val = 0, mask;

	/* check AFC_RX register in xbar */
	reg = TEGRA_AHUB_AUDIO_RX_STRIDE * (afc_id + 0x23);
	mask = BIT(9) | BIT(10) | BIT(11);
	tegra30_xbar_read_reg(reg, &val);

	return (val & mask) >> 9;
}

static int tegra124_afc_set_thresholds(struct tegra124_afc *afc,
				unsigned int afc_id)
{
	unsigned int i2s_id, value;

	if (tegra124_afc_get_dam_id(afc_id)) {
		/* TODO program thresholds using SRC_BURST */
	} else {
		value = 4 << TEGRA_AFC_FIFO_HIGH_THRESHOLD_SHIFT;
		value |= 3 << TEGRA_AFC_FIFO_START_THRESHOLD_SHIFT;
		value |= 2;
	}
	regmap_write(afc->regmap, TEGRA_AFC_THRESHOLDS_AFC, value);

	i2s_id = tegra124_afc_get_i2s_id(afc_id);

	value |= i2s_id << TEGRA_AFC_DEST_I2S_ID_SHIFT;

	regmap_write(afc->regmap, TEGRA_AFC_THRESHOLDS_I2S, value);

	return 0;
}

static int tegra124_afc_set_audio_cif(struct tegra124_afc *afc,
				struct snd_pcm_hw_params *params,
				unsigned int reg)
{
	int channels, audio_bits;
	struct tegra30_xbar_cif_conf cif_conf;

	channels = params_channels(params);
	if (channels < 2)
		return -EINVAL;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		audio_bits = TEGRA30_AUDIOCIF_BITS_16;
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
	cif_conf.client_bits = audio_bits;
	cif_conf.expand = 0;
	cif_conf.stereo_conv = 0;
	cif_conf.replicate = 0;
	cif_conf.truncate = 0;
	cif_conf.direction = 0;
	cif_conf.mono_conv = 0;

	afc->soc_data->set_audio_cif(afc->regmap, reg, &cif_conf);

	return 0;
}

static int tegra124_afc_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct tegra124_afc *afc = snd_soc_dai_get_drvdata(dai);
	int ret;

	/* set RX cif and TX cif */
	ret = tegra124_afc_set_audio_cif(afc, params,
				TEGRA_AFC_AUDIOCIF_AFCRX_CTRL);
	if (ret) {
		dev_err(dev, "Can't set AFC RX CIF: %d\n", ret);
		return ret;
	}
	ret = tegra124_afc_set_audio_cif(afc, params,
				TEGRA_AFC_AUDIOCIF_AFCTX_CTRL);
	if (ret) {
		dev_err(dev, "Can't set AFC TX CIF: %d\n", ret);
		return ret;
	}

	/* set AFC thresholds */
	tegra124_afc_set_thresholds(afc, dev->id);

	/* set AFC step size */
	regmap_write(afc->regmap, TEGRA_AFC_FLOW_STEP_SIZE, 5);

	return ret;

}

static int tegra124_afc_codec_probe(struct snd_soc_codec *codec)
{
	struct tegra124_afc *afc = snd_soc_codec_get_drvdata(codec);

	codec->control_data = afc->regmap;

	return 0;
}

static struct snd_soc_dai_ops tegra124_afc_dai_ops = {
	.hw_params	= tegra124_afc_hw_params,
};

static struct snd_soc_dai_driver tegra124_afc_dais[] = {
	{
		.name = "AFC IN",
		.playback = {
			.stream_name = "AFC Receive",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_96000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
	{
		.name = "AFC OUT",
		.capture = {
			.stream_name = "AFC Transmit",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_96000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &tegra124_afc_dai_ops,
	}
};

static const struct snd_soc_dapm_widget tegra124_afc_widgets[] = {
	SND_SOC_DAPM_AIF_IN("AFC RX", NULL, 0, SND_SOC_NOPM,
				0, 0),
	SND_SOC_DAPM_AIF_OUT("AFC TX", NULL, 0, TEGRA_AFC_CTRL,
				0, 0),
};

static const struct snd_soc_dapm_route tegra124_afc_routes[] = {
	{ "AFC RX",       NULL, "AFC Receive" },
	{ "AFC TX",       NULL, "AFC RX" },
	{ "AFC Transmit", NULL, "AFC TX" },
};

static struct snd_soc_codec_driver tegra124_afc_codec = {
	.probe = tegra124_afc_codec_probe,
	.dapm_widgets = tegra124_afc_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tegra124_afc_widgets),
	.dapm_routes = tegra124_afc_routes,
	.num_dapm_routes = ARRAY_SIZE(tegra124_afc_routes),
	.idle_bias_off = 1,
};

static bool tegra124_afc_wr_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA_AFC_CTRL:
	case TEGRA_AFC_THRESHOLDS_I2S:
	case TEGRA_AFC_THRESHOLDS_AFC:
	case TEGRA_AFC_FLOW_STEP_SIZE:
	case TEGRA_AFC_AUDIOCIF_AFCTX_CTRL:
	case TEGRA_AFC_AUDIOCIF_AFCRX_CTRL:
	case TEGRA_AFC_FLOW_STATUS:
	case TEGRA_AFC_LCOEF_1_4_0:
	case TEGRA_AFC_LCOEF_1_4_1:
	case TEGRA_AFC_LCOEF_1_4_2:
	case TEGRA_AFC_LCOEF_1_4_3:
	case TEGRA_AFC_LCOEF_1_4_4:
	case TEGRA_AFC_LCOEF_1_4_5:
	case TEGRA_AFC_LCOEF_2_4_0:
	case TEGRA_AFC_LCOEF_2_4_1:
	case TEGRA_AFC_LCOEF_2_4_2:
		return true;
	default:
		return false;
	};
}

static bool tegra124_afc_rd_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA_AFC_CTRL:
	case TEGRA_AFC_THRESHOLDS_I2S:
	case TEGRA_AFC_THRESHOLDS_AFC:
	case TEGRA_AFC_FLOW_STEP_SIZE:
	case TEGRA_AFC_AUDIOCIF_AFCTX_CTRL:
	case TEGRA_AFC_AUDIOCIF_AFCRX_CTRL:
	case TEGRA_AFC_FLOW_STATUS:
	case TEGRA_AFC_FLOW_TOTAL:
	case TEGRA_AFC_FLOW_OVER:
	case TEGRA_AFC_FLOW_UNDER:
	case TEGRA_AFC_LCOEF_1_4_0:
	case TEGRA_AFC_LCOEF_1_4_1:
	case TEGRA_AFC_LCOEF_1_4_2:
	case TEGRA_AFC_LCOEF_1_4_3:
	case TEGRA_AFC_LCOEF_1_4_4:
	case TEGRA_AFC_LCOEF_1_4_5:
	case TEGRA_AFC_LCOEF_2_4_0:
	case TEGRA_AFC_LCOEF_2_4_1:
	case TEGRA_AFC_LCOEF_2_4_2:
		return true;
	default:
		return false;
	};
}

static bool tegra124_afc_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA_AFC_FLOW_STATUS:
	case TEGRA_AFC_FLOW_TOTAL:
	case TEGRA_AFC_FLOW_OVER:
	case TEGRA_AFC_FLOW_UNDER:
		return true;
	default:
		return false;
	};

}

static const struct regmap_config tegra124_afc_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = TEGRA_AFC_LCOEF_2_4_2,
	.writeable_reg = tegra124_afc_wr_reg,
	.readable_reg = tegra124_afc_rd_reg,
	.volatile_reg = tegra124_afc_volatile_reg,
	.cache_type = REGCACHE_FLAT,
	.reg_defaults = tegra124_afc_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(tegra124_afc_reg_defaults),
};

static const struct tegra124_afc_soc_data soc_data_tegra124 = {
	.set_audio_cif = tegra124_xbar_set_cif,
};

static const struct of_device_id tegra124_afc_of_match[] = {
	{ .compatible = "nvidia,tegra124-afc", .data = &soc_data_tegra124 },
	{},
};

static int tegra124_afc_platform_probe(struct platform_device *pdev)
{
	struct tegra124_afc *afc;
	struct resource *mem, *memregion;
	void __iomem *regs;
	int ret = 0;
	const struct of_device_id *match;
	struct tegra124_afc_soc_data *soc_data;

	match = of_match_device(tegra124_afc_of_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "Error: No device match found\n");
		ret = -ENODEV;
		goto err;
	}
	soc_data = (struct tegra124_afc_soc_data *)match->data;

	afc = devm_kzalloc(&pdev->dev, sizeof(struct tegra124_afc), GFP_KERNEL);
	if (!afc) {
		dev_err(&pdev->dev, "Can't allocate afc\n");
		ret = -ENOMEM;
		goto err;
	}
	dev_set_drvdata(&pdev->dev, afc);

	afc->soc_data = soc_data;

	/* initialize default destination I2S */
	afc->destination_i2s = 0;

	afc->clk_afc = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(afc->clk_afc)) {
		dev_err(&pdev->dev, "Can't retrieve afc clock\n");
		ret = PTR_ERR(afc->clk_afc);
		goto err;
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "No memory resource\n");
		ret = -ENODEV;
		goto err_clk_put;
	}

	memregion = devm_request_mem_region(&pdev->dev, mem->start,
					    resource_size(mem), pdev->name);
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

	afc->regmap = devm_regmap_init_mmio(&pdev->dev, regs,
					    &tegra124_afc_regmap_config);
	if (IS_ERR(afc->regmap)) {
		dev_err(&pdev->dev, "regmap init failed\n");
		ret = PTR_ERR(afc->regmap);
		goto err_clk_put;
	}
	regcache_cache_only(afc->regmap, true);

	if (of_property_read_u32(pdev->dev.of_node,
				"nvidia,ahub-afc-id",
				&pdev->dev.id) < 0) {
		dev_err(&pdev->dev,
			"Missing property nvidia,ahub-afc-id\n");
		ret = -ENODEV;
		goto err_clk_put;
	}

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		ret = tegra124_afc_runtime_resume(&pdev->dev);
		if (ret)
			goto err_pm_disable;
	}

	ret = snd_soc_register_codec(&pdev->dev, &tegra124_afc_codec,
				     tegra124_afc_dais,
				     ARRAY_SIZE(tegra124_afc_dais));
	if (ret != 0) {
		dev_err(&pdev->dev, "Could not register CODEC: %d\n", ret);
		goto err_suspend;
	}

	return 0;

err_suspend:
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra124_afc_runtime_suspend(&pdev->dev);
err_pm_disable:
	pm_runtime_disable(&pdev->dev);
err_clk_put:
	devm_clk_put(&pdev->dev, afc->clk_afc);
err:
	return ret;
}

static int tegra124_afc_platform_remove(struct platform_device *pdev)
{
	struct tegra124_afc *afc = dev_get_drvdata(&pdev->dev);

	snd_soc_unregister_codec(&pdev->dev);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra124_afc_runtime_suspend(&pdev->dev);

	devm_clk_put(&pdev->dev, afc->clk_afc);
	return 0;
}

static const struct dev_pm_ops tegra124_afc_pm_ops = {
	SET_RUNTIME_PM_OPS(tegra124_afc_runtime_suspend,
			   tegra124_afc_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(tegra124_afc_suspend,
			   NULL)
};

static struct platform_driver tegra124_afc_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = tegra124_afc_of_match,
		.pm = &tegra124_afc_pm_ops,
	},
	.probe = tegra124_afc_platform_probe,
	.remove = tegra124_afc_platform_remove,
};
module_platform_driver(tegra124_afc_driver)

MODULE_AUTHOR("Arun S L <aruns@nvidia.com>");
MODULE_DESCRIPTION("Tegra124 AFC ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, tegra124_afc_of_match);
