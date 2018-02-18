/*
 * tegra210_iqc.c - Tegra210 IQC driver
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
#include <linux/debugfs.h>

#include "tegra210_xbar_alt.h"
#include "tegra210_iqc_alt.h"

#define DRV_NAME "tegra210-iqc"

static const struct reg_default tegra210_iqc_reg_defaultss[] = {
	{ TEGRA210_IQC_AXBAR_TX_INT_MASK, 0x0000000f},
	{ TEGRA210_IQC_AXBAR_TX_CIF_CTRL, 0x00007700},
	{ TEGRA210_IQC_CG, 0x1},
	{ TEGRA210_IQC_CTRL, 0x80000020},
};

static int tegra210_iqc_runtime_suspend(struct device *dev)
{
	struct tegra210_iqc *iqc = dev_get_drvdata(dev);

	regcache_cache_only(iqc->regmap, true);
	regcache_mark_dirty(iqc->regmap);

#ifndef CONFIG_MACH_GRENADA
	clk_disable_unprepare(iqc->clk_iqc);
#endif
	pm_runtime_put_sync(dev->parent);

	return 0;
}

static int tegra210_iqc_runtime_resume(struct device *dev)
{
	struct tegra210_iqc *iqc = dev_get_drvdata(dev);
	int ret;

	ret = pm_runtime_get_sync(dev->parent);
	if (ret < 0) {
		dev_err(dev, "parent get_sync failed: %d\n", ret);
		return ret;
	}

#ifndef CONFIG_MACH_GRENADA
	ret = clk_prepare_enable(iqc->clk_iqc);
	if (ret) {
		dev_err(dev, "clk_enable failed: %d\n", ret);
		return ret;
	}
#endif

	regcache_cache_only(iqc->regmap, false);
	regcache_sync(iqc->regmap);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tegra210_iqc_suspend(struct device *dev)
{
	return 0;
}
#endif

static int tegra210_iqc_set_audio_cif(struct tegra210_iqc *iqc,
				struct snd_pcm_hw_params *params,
				unsigned int reg)
{
	int channels, audio_bits;
	struct tegra210_xbar_cif_conf cif_conf;

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

	memset(cif_conf, 0, sizeof(struct tegra210_xbar_cif_conf));
	cif_conf.audio_channels = channels;
	cif_conf.client_channels = channels;
	cif_conf.audio_bits = audio_bits;
	cif_conf.client_bits = audio_bits;

	iqc->soc_data->set_audio_cif(iqc->regmap, reg, &cif_conf);

	return 0;
}

static int tegra210_iqc_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct tegra210_iqc *iqc = snd_soc_dai_get_drvdata(dai);
	int ret;

	/* set IQC tx cif */
	ret = tegra210_iqc_set_audio_cif(iqc, params,
				TEGRA210_IQC_AXBAR_TX_CIF_CTRL +
				(dai->id * TEGRA210_IQC_AXBAR_TX_STRIDE));
	if (ret) {
		dev_err(dev, "Can't set IQC TX CIF: %d\n", ret);
		return ret;
	}

	/* disable timestamp */
	if (!iqc->timestamp_enable)
		regmap_update_bits(iqc->regmap, TEGRA210_IQC_CTRL,
			TEGRA210_IQC_TIMESTAMP_MASK,
			~(TEGRA210_IQC_TIMESTAMP_EN));

	/* set the IQC data offset */
	if (iqc->data_offset)
		regmap_update_bits(iqc->regmap, TEGRA210_IQC_CTRL,
			TEGRA210_IQC_DATA_OFFSET_MASK,
			iqc->data_offset);

	return ret;
}


static int tegra210_iqc_codec_probe(struct snd_soc_codec *codec)
{
	struct tegra210_iqc *iqc = snd_soc_codec_get_drvdata(codec);

	codec->control_data = iqc->regmap;

	return 0;
}

static struct snd_soc_dai_ops tegra210_iqc_dai_ops = {
	.hw_params	= tegra210_iqc_hw_params,
};

#define IN_DAI(id)						\
	{							\
		.name = "DAP",					\
		.playback = {					\
			.stream_name = "DAP" #id " Receive",	\
			.channels_min = 1,			\
			.channels_max = 16,			\
			.rates = SNDRV_PCM_RATE_8000_96000,	\
			.formats = SNDRV_PCM_FMTBIT_S16_LE,	\
		},						\
	}

#define OUT_DAI(id)						\
	{							\
		.name = "CIF",					\
		.capture = {					\
			.stream_name = "CIF" #id " Transmit",	\
			.channels_min = 1,			\
			.channels_max = 16,			\
			.rates = SNDRV_PCM_RATE_8000_96000,	\
			.formats = SNDRV_PCM_FMTBIT_S16_LE,	\
		},						\
		.ops = &tegra210_iqc_dai_ops,			\
	}

static struct snd_soc_dai_driver tegra210_iqc_dais[] = {
	OUT_DAI(1),
	OUT_DAI(2),
	IN_DAI(1),
	IN_DAI(2),
};

static const struct snd_kcontrol_new tegra210_iqc_controls[] = {
	SOC_SINGLE("IQC Enable", TEGRA210_IQC_ENABLE, 0, 1, 0),
};

static const struct snd_soc_dapm_widget tegra210_iqc_widgets[] = {
	SND_SOC_DAPM_AIF_IN("IQC RX1", NULL, 0, SND_SOC_NOPM,
				0, 0),
	SND_SOC_DAPM_AIF_IN("IQC RX2", NULL, 0, SND_SOC_NOPM,
				0, 0),
	SND_SOC_DAPM_AIF_OUT("IQC TX1", NULL, 0, SND_SOC_NOPM,
				0, 0),
	SND_SOC_DAPM_AIF_OUT("IQC TX2", NULL, 0, SND_SOC_NOPM,
				0, 0),
};

static const struct snd_soc_dapm_route tegra210_iqc_routes[] = {
	{ "IQC RX1",       NULL, "DAP1 Receive" },
	{ "IQC TX1",       NULL, "IQC RX1" },
	{ "CIF1 Transmit", NULL, "IQC TX1" },

	{ "IQC RX2",       NULL, "DAP2 Receive" },
	{ "IQC TX2",       NULL, "IQC RX2" },
	{ "CIF2 Transmit", NULL, "IQC TX2" },
};

static struct snd_soc_codec_driver tegra210_iqc_codec = {
	.probe = tegra210_iqc_codec_probe,
	.dapm_widgets = tegra210_iqc_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tegra210_iqc_widgets),
	.dapm_routes = tegra210_iqc_routes,
	.num_dapm_routes = ARRAY_SIZE(tegra210_iqc_routes),
	.controls = tegra210_iqc_controls,
	.num_controls = ARRAY_SIZE(tegra210_iqc_controls),
	.idle_bias_off = 1,
};

static bool tegra210_iqc_wr_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA210_IQC_AXBAR_TX_INT_MASK:
	case TEGRA210_IQC_AXBAR_TX_INT_SET:
	case TEGRA210_IQC_AXBAR_TX_INT_CLEAR:
	case TEGRA210_IQC_AXBAR_TX_CIF_CTRL:
	case TEGRA210_IQC_ENABLE:
	case TEGRA210_IQC_SOFT_RESET:
	case TEGRA210_IQC_CG:
	case TEGRA210_IQC_CTRL:
	case TEGRA210_IQC_CYA:
		return true;
	default:
		return false;
	};
}

static bool tegra210_iqc_rd_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA210_IQC_AXBAR_TX_STATUS:
	case TEGRA210_IQC_AXBAR_TX_INT_STATUS:
	case TEGRA210_IQC_AXBAR_TX_INT_MASK:
	case TEGRA210_IQC_AXBAR_TX_INT_SET:
	case TEGRA210_IQC_AXBAR_TX_INT_CLEAR:
	case TEGRA210_IQC_AXBAR_TX_CIF_CTRL:
	case TEGRA210_IQC_ENABLE:
	case TEGRA210_IQC_SOFT_RESET:
	case TEGRA210_IQC_CG:
	case TEGRA210_IQC_STATUS:
	case TEGRA210_IQC_INT_STATUS:
	case TEGRA210_IQC_CTRL:
	case TEGRA210_IQC_TIME_STAMP_STATUS_0:
	case TEGRA210_IQC_TIME_STAMP_STATUS_1:
	case TEGRA210_IQC_WS_EDGE_STATUS:
	case TEGRA210_IQC_CYA:
		return true;
	default:
		return false;
	};
}

static bool tegra210_iqc_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA210_IQC_AXBAR_TX_CIF_CTRL:
	case TEGRA210_IQC_ENABLE:
	case TEGRA210_IQC_CTRL:
		return true;
	default:
		return false;
	};
}

static const struct regmap_config tegra210_iqc_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = TEGRA210_IQC_CYA,
	.writeable_reg = tegra210_iqc_wr_reg,
	.readable_reg = tegra210_iqc_rd_reg,
	.volatile_reg = tegra210_iqc_volatile_reg,
	.reg_defaults = tegra210_iqc_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(tegra210_iqc_reg_defaults),
	.cache_type = REGCACHE_FLAT,
};

static const struct tegra210_iqc_soc_data soc_data_tegra210 = {
	.set_audio_cif = tegra210_xbar_set_cif,
};

static const struct of_device_id tegra210_iqc_of_match[] = {
	{ .compatible = "nvidia,tegra210-iqc", .data = &soc_data_tegra210 },
	{},
};

static int tegra210_iqc_platform_probe(struct platform_device *pdev)
{
	struct tegra210_iqc *iqc;
	struct resource *mem, *memregion;
	void __iomem *regs;
	int ret = 0;
	const struct of_device_id *match;
	struct tegra210_iqc_soc_data *soc_data;

	match = of_match_device(tegra210_iqc_of_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "Error: No device match found\n");
		ret = -ENODEV;
		goto err;
	}
	soc_data = (struct tegra210_iqc_soc_data *)match->data;

	iqc = devm_kzalloc(&pdev->dev, sizeof(struct tegra210_iqc), GFP_KERNEL);
	if (!iqc) {
		dev_err(&pdev->dev, "Can't allocate iqc\n");
		ret = -ENOMEM;
		goto err;
	}

	iqc->soc_data = soc_data;

	if (!(tegra_platform_is_unit_fpga() || tegra_platform_is_fpga())) {
		iqc->clk_iqc = devm_clk_get(&pdev->dev, NULL);
		if (IS_ERR(iqc->clk_iqc)) {
			dev_err(&pdev->dev, "Can't retrieve iqc clock\n");
			ret = PTR_ERR(iqc->clk_iqc);
			goto err;
		}
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

	iqc->regmap = devm_regmap_init_mmio(&pdev->dev, regs,
					    &tegra210_iqc_regmap_config);
	if (IS_ERR(iqc->regmap)) {
		dev_err(&pdev->dev, "regmap init failed\n");
		ret = PTR_ERR(iqc->regmap);
		goto err_clk_put;
	}
	regcache_cache_only(iqc->regmap, true);

	if (of_property_read_u32(pdev->dev.of_node,
				"nvidia,ahub-iqc-id",
				&pdev->dev.id) < 0) {
		dev_err(&pdev->dev,
			"Missing property nvidia,ahub-iqc-id\n");
		ret = -ENODEV;
		goto err_clk_put;
	}

	if (of_property_read_u32(pdev->dev.of_node,
				"timestamp-enable",
				&iqc->timestamp_enable) < 0) {
		dev_err(&pdev->dev,
			"Missing property timestamp-enable for IQC\n");
		iqc->timestamp_enable = 1;
	}

	if (of_property_read_u32(pdev->dev.of_node,
				"data-offset",
				&iqc->data_offset) < 0) {
		dev_err(&pdev->dev,
			"Missing property data-offset for IQC\n");
		iqc->data_offset = 0;
	}

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		ret = tegra210_iqc_runtime_resume(&pdev->dev);
		if (ret)
			goto err_pm_disable;
	}

	ret = snd_soc_register_codec(&pdev->dev, &tegra210_iqc_codec,
				     tegra210_iqc_dais,
				     ARRAY_SIZE(tegra210_iqc_dais));
	if (ret != 0) {
		dev_err(&pdev->dev, "Could not register CODEC: %d\n", ret);
		goto err_suspend;
	}

	dev_set_drvdata(&pdev->dev, iqc);

	return 0;

err_suspend:
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra210_iqc_runtime_suspend(&pdev->dev);
err_pm_disable:
	pm_runtime_disable(&pdev->dev);
err_clk_put:
	devm_clk_put(&pdev->dev, iqc->clk_iqc);
err:
	return ret;
}

static int tegra210_iqc_platform_remove(struct platform_device *pdev)
{
	struct tegra210_iqc *iqc = dev_get_drvdata(&pdev->dev);

	snd_soc_unregister_codec(&pdev->dev);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra210_iqc_runtime_suspend(&pdev->dev);

	devm_clk_put(&pdev->dev, iqc->clk_iqc);

	return 0;
}

static const struct dev_pm_ops tegra210_iqc_pm_ops = {
	SET_RUNTIME_PM_OPS(tegra210_iqc_runtime_suspend,
			   tegra210_iqc_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(tegra210_iqc_suspend, NULL)
};

static struct platform_driver tegra210_iqc_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = tegra210_iqc_of_match,
		.pm = &tegra210_iqc_pm_ops,
	},
	.probe = tegra210_iqc_platform_probe,
	.remove = tegra210_iqc_platform_remove,
};
module_platform_driver(tegra210_iqc_driver)

MODULE_AUTHOR("Arun S L <aruns@nvidia.com>");
MODULE_DESCRIPTION("Tegra210 IQC ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, tegra210_iqc_of_match);
