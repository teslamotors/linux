/*
 * tegra186_dspk_alt.c - Tegra186 DSPK driver
 *
 * Copyright (c) 2015-2016 NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/debugfs.h>
#include <linux/tegra-soc.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/pinconf-tegra.h>

#include "tegra210_xbar_alt.h"
#include "tegra186_xbar_alt.h"
#include "tegra186_dspk_alt.h"
#include "ahub_unit_fpga_clock.h"
#include "ahub_unit_fpga_clock_t18x.h"

#define DRV_NAME "tegra186-dspk"

static const struct reg_default tegra186_dspk_reg_defaults[] = {
	{ TEGRA186_DSPK_AXBAR_RX_INT_MASK, 0x00000007},
	{ TEGRA186_DSPK_AXBAR_RX_CIF_CTRL, 0x00007700},
	{ TEGRA186_DSPK_CG,                0x00000001},
	{ TEGRA186_DSPK_CORE_CTRL,         0x00000310},
	{ TEGRA186_DSPK_CODEC_CTRL,        0x03000000},
	{ TEGRA186_DSPK_SDM_COEF_A_2,      0x000013bb},
	{ TEGRA186_DSPK_SDM_COEF_A_3,      0x00001cbf},
	{ TEGRA186_DSPK_SDM_COEF_A_4,      0x000029d7},
	{ TEGRA186_DSPK_SDM_COEF_A_5,      0x00003782},
	{ TEGRA186_DSPK_SDM_COEF_C_1,      0x000000a6},
	{ TEGRA186_DSPK_SDM_COEF_C_2,      0x00001959},
	{ TEGRA186_DSPK_SDM_COEF_C_3,      0x00002b9f},
	{ TEGRA186_DSPK_SDM_COEF_C_4,      0x00004218},
	{ TEGRA186_DSPK_SDM_COEF_G_1,      0x00000074},
	{ TEGRA186_DSPK_SDM_COEF_G_2,      0x0000007d},
};

static int tegra186_dspk_runtime_suspend(struct device *dev)
{
	struct tegra186_dspk *dspk = dev_get_drvdata(dev);
	int ret;

	regcache_cache_only(dspk->regmap, true);
	regcache_mark_dirty(dspk->regmap);

	if (!(tegra_platform_is_unit_fpga() || tegra_platform_is_fpga())) {
		if (!IS_ERR(dspk->pin_idle_state) && dspk->is_pinctrl) {
			ret = pinctrl_select_state(
				dspk->pinctrl, dspk->pin_idle_state);
			if (ret < 0)
				dev_err(dev,
				"setting dap pinctrl idle state failed\n");
		}
		clk_disable_unprepare(dspk->clk_dspk);
	}

	pm_runtime_put_sync(dev->parent);
	return 0;
}

static int tegra186_dspk_runtime_resume(struct device *dev)
{
	struct tegra186_dspk *dspk = dev_get_drvdata(dev);
	int ret;

	ret = pm_runtime_get_sync(dev->parent);
	if (ret < 0) {
		dev_err(dev, "parent get_sync failed: %d\n", ret);
		return ret;
	}

	if (!(tegra_platform_is_unit_fpga() || tegra_platform_is_fpga())) {
		if (!IS_ERR(dspk->pin_active_state) && dspk->is_pinctrl) {
			ret = pinctrl_select_state(dspk->pinctrl,
						dspk->pin_active_state);
			if (ret < 0)
				dev_err(dev,
				"setting dap pinctrl active state failed\n");
		}

		ret = clk_prepare_enable(dspk->clk_dspk);
		if (ret) {
			dev_err(dev, "clk_enable failed: %d\n", ret);
			return ret;
		}
	}

	regcache_cache_only(dspk->regmap, false);
	regcache_sync(dspk->regmap);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tegra186_dspk_suspend(struct device *dev)
{
	return 0;
}
#endif

static int tegra186_dspk_set_audio_cif(struct tegra186_dspk *dspk,
		struct snd_pcm_hw_params *params,
		unsigned int reg, struct snd_soc_dai *dai)
{
	int channels;
	struct tegra210_xbar_cif_conf cif_conf;
	struct device *dev = dai->dev;

	channels = params_channels(params);
	memset(&cif_conf, 0, sizeof(struct tegra210_xbar_cif_conf));
	cif_conf.audio_channels = channels;
	cif_conf.client_channels = channels;
	cif_conf.client_bits = TEGRA210_AUDIOCIF_BITS_24;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		cif_conf.audio_bits = TEGRA210_AUDIOCIF_BITS_16;
		cif_conf.client_bits = TEGRA210_AUDIOCIF_BITS_16;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		cif_conf.audio_bits = TEGRA210_AUDIOCIF_BITS_32;
		break;
	default:
		dev_err(dev, "Wrong format!\n");
		return -EINVAL;
	}

	dspk->soc_data->set_audio_cif(dspk->regmap,
			TEGRA186_DSPK_AXBAR_RX_CIF_CTRL,
			&cif_conf);
	return 0;
}

static int tegra186_dspk_set_dai_bclk_ratio(struct snd_soc_dai *dai,
		unsigned int ratio)
{
	return 0;
}

static int tegra186_dspk_hw_params(struct snd_pcm_substream *substream,
		    struct snd_pcm_hw_params *params,
		    struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct tegra186_dspk *dspk = snd_soc_dai_get_drvdata(dai);
	int channels, srate, ret, dspk_clk;
	int osr = TEGRA186_DSPK_OSR_64;
	int interface_clk_ratio = 4; /* dspk interface clock should be fsout*4 */

	channels = params_channels(params);
	srate = params_rate(params);
	dspk_clk = (1 << (5+osr)) * srate * interface_clk_ratio;

	if ((tegra_platform_is_unit_fpga() || tegra_platform_is_fpga())) {
		program_dspk_clk(dspk_clk);
	} else {
		ret = clk_set_rate(dspk->clk_dspk, dspk_clk);
		if (ret) {
			dev_err(dev, "Can't set dspk clock rate: %d\n", ret);
			return ret;
		}
	}

	regmap_update_bits(dspk->regmap,
			TEGRA186_DSPK_CORE_CTRL,
			TEGRA186_DSPK_OSR_MASK,
			osr << TEGRA186_DSPK_OSR_SHIFT);

	regmap_update_bits(dspk->regmap,
			TEGRA186_DSPK_CORE_CTRL,
			TEGRA186_DSPK_CHANNEL_SELECT_MASK,
			((1 << channels) - 1) <<
			TEGRA186_DSPK_CHANNEL_SELECT_SHIFT);

	/* program cif control register */
	ret = tegra186_dspk_set_audio_cif(dspk, params,
				TEGRA186_DSPK_AXBAR_RX_CIF_CTRL,
				dai);

	if (ret)
		dev_err(dev, "Can't set dspk RX CIF: %d\n", ret);
	return ret;
}

static int tegra186_dspk_codec_probe(struct snd_soc_codec *codec)
{
	struct tegra186_dspk *dspk = snd_soc_codec_get_drvdata(codec);

	codec->control_data = dspk->regmap;

	return 0;
}

static struct snd_soc_dai_ops tegra186_dspk_dai_ops = {
	.hw_params	= tegra186_dspk_hw_params,
	.set_bclk_ratio	= tegra186_dspk_set_dai_bclk_ratio,
};

static struct snd_soc_dai_driver tegra186_dspk_dais[] = {
	/* for left channel audio */
	{
	    .name = "DAP Left",
	    .capture = {
		.stream_name = "DSPK Left Transmit",
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	    },
	    .ops = &tegra186_dspk_dai_ops,
	    .symmetric_rates = 1,
	},
	{
	    .name = "CIF Right",
	    .playback = {
		.stream_name = "DSPK Receive Right",
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	    },
	    .ops = &tegra186_dspk_dai_ops,
	    .symmetric_rates = 1,
	},

	/* for right channel audio */
	{
	    .name = "DAP Right",
	    .capture = {
		.stream_name = "DSPK Right Transmit",
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	    },
	    .ops = &tegra186_dspk_dai_ops,
	    .symmetric_rates = 1,
	},
	{
	    .name = "CIF Left",
	    .playback = {
		.stream_name = "DSPK Receive Left",
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	    },
	    .ops = &tegra186_dspk_dai_ops,
	    .symmetric_rates = 1,
	}
};

static const struct snd_soc_dapm_widget tegra186_dspk_widgets[] = {
	SND_SOC_DAPM_AIF_IN("DSPK RX", NULL, 0, TEGRA186_DSPK_ENABLE, 0, 0),
	SND_SOC_DAPM_AIF_OUT("DSPK TX", NULL, 0, SND_SOC_NOPM, 0, 0),
};

static const struct snd_soc_dapm_route tegra186_dspk_routes[] = {
	{ "DSPK RX",	   NULL, "DSPK Receive Right" },
	{ "DSPK TX",	   NULL, "DSPK RX" },
	{ "DSPK Right Transmit", NULL, "DSPK TX" },
	{ "DSPK Left Transmit", NULL, "DSPK TX" },
};

static struct snd_soc_codec_driver tegra186_dspk_codec = {
	.probe = tegra186_dspk_codec_probe,
	.dapm_widgets = tegra186_dspk_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tegra186_dspk_widgets),
	.dapm_routes = tegra186_dspk_routes,
	.num_dapm_routes = ARRAY_SIZE(tegra186_dspk_routes),
	.idle_bias_off = 1,
};

/* Regmap callback functions */
static bool tegra186_dspk_wr_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA186_DSPK_AXBAR_RX_INT_MASK:
	case TEGRA186_DSPK_AXBAR_RX_INT_SET:
	case TEGRA186_DSPK_AXBAR_RX_INT_CLEAR:
	case TEGRA186_DSPK_AXBAR_RX_CIF_CTRL:
	case TEGRA186_DSPK_AXBAR_RX_CYA:
	case TEGRA186_DSPK_ENABLE:
	case TEGRA186_DSPK_SOFT_RESET:
	case TEGRA186_DSPK_CG:
		return true;
	default:
		if (((reg % 4) == 0) && (reg >= TEGRA186_DSPK_CORE_CTRL) &&
		    (reg <= TEGRA186_DSPK_SDM_COEF_G_2))
			return true;
		else
			return false;
	};
}

static bool tegra186_dspk_rd_reg(struct device *dev, unsigned int reg)
{
	if (tegra186_dspk_wr_reg(dev, reg))
		return true;

	switch (reg) {
	case TEGRA186_DSPK_AXBAR_RX_STATUS:
	case TEGRA186_DSPK_AXBAR_RX_INT_STATUS:
	case TEGRA186_DSPK_AXBAR_RX_CIF_FIFO_STATUS:
	case TEGRA186_DSPK_STATUS:
	case TEGRA186_DSPK_INT_STATUS:
		return true;
	default:
		if (((reg % 4) == 0) && (reg >= TEGRA186_DSPK_DEBUG_STATUS) &&
		    (reg <= TEGRA186_DSPK_DEBUG_STAGE4_CNTR))
			return true;
		else
			return false;
	};
}

static bool tegra186_dspk_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA186_DSPK_AXBAR_RX_STATUS:
	case TEGRA186_DSPK_AXBAR_RX_INT_STATUS:
	case TEGRA186_DSPK_AXBAR_RX_CIF_FIFO_STATUS:
	case TEGRA186_DSPK_STATUS:
	case TEGRA186_DSPK_INT_STATUS:
		return true;
	default:
		return false;
	};
}

static const struct regmap_config tegra186_dspk_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = TEGRA186_DSPK_DEBUG_STAGE4_CNTR,
	.writeable_reg = tegra186_dspk_wr_reg,
	.readable_reg = tegra186_dspk_rd_reg,
	.volatile_reg = tegra186_dspk_volatile_reg,
	.precious_reg = NULL,
	.reg_defaults = tegra186_dspk_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(tegra186_dspk_reg_defaults),
	.cache_type = REGCACHE_FLAT,
};

static const struct tegra186_dspk_soc_data soc_data_tegra186 = {
	.set_audio_cif = tegra210_xbar_set_cif,
};

static const struct of_device_id tegra186_dspk_of_match[] = {
	{ .compatible = "nvidia,tegra186-dspk", .data = &soc_data_tegra186 },
	{},
};

static int tegra186_dspk_platform_probe(struct platform_device *pdev)
{
	struct tegra186_dspk *dspk;
	struct device_node *np = pdev->dev.of_node;
	struct resource *mem, *memregion;
	void __iomem *regs;
	int ret = 0;
	const struct of_device_id *match;
	struct tegra186_dspk_soc_data *soc_data;
	const char *prod_name;

	match = of_match_device(tegra186_dspk_of_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "Error: No device match found\n");
		ret = -ENODEV;
		goto err;
	}
	soc_data = (struct tegra186_dspk_soc_data *)match->data;

	dspk = devm_kzalloc(&pdev->dev, sizeof(struct tegra186_dspk),
			GFP_KERNEL);
	if (!dspk) {
		dev_err(&pdev->dev, "Can't allocate dspk\n");
		ret = -ENOMEM;
		goto err;
	}

	dspk->soc_data = soc_data;

	if (!(tegra_platform_is_unit_fpga() || tegra_platform_is_fpga())) {
		dspk->clk_dspk = devm_clk_get(&pdev->dev, NULL);
		if (IS_ERR(dspk->clk_dspk)) {
			dev_err(&pdev->dev, "Can't retrieve dspk clock\n");
			ret = PTR_ERR(dspk->clk_dspk);
			goto err;
		}

		dspk->clk_pll_a_out0 = devm_clk_get(&pdev->dev, "pll_a_out0");
		if (IS_ERR_OR_NULL(dspk->clk_pll_a_out0)) {
			dev_err(&pdev->dev, "Can't retrieve pll_a_out0 clock\n");
			ret = -ENOENT;
		goto err_clk_put;
		}

		ret = clk_set_parent(dspk->clk_dspk, dspk->clk_pll_a_out0);
		if (ret) {
			dev_err(&pdev->dev, "Can't set parent of dspk clock\n");
			goto err_plla_clk_put;
		}
	}

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

	dspk->regmap = devm_regmap_init_mmio(&pdev->dev, regs,
					    &tegra186_dspk_regmap_config);
	if (IS_ERR(dspk->regmap)) {
		dev_err(&pdev->dev, "regmap init failed\n");
		ret = PTR_ERR(dspk->regmap);
		goto err;
	}
	regcache_cache_only(dspk->regmap, true);

	if (of_property_read_u32(np, "nvidia,ahub-dspk-id",
				&pdev->dev.id) < 0) {
		dev_err(&pdev->dev,
			"Missing property nvidia,ahub-dspk-id\n");
		ret = -ENODEV;
		goto err;
	}

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		ret = tegra186_dspk_runtime_resume(&pdev->dev);
		if (ret)
			goto err_pm_disable;
	}

	ret = snd_soc_register_codec(&pdev->dev, &tegra186_dspk_codec,
				     tegra186_dspk_dais,
				     ARRAY_SIZE(tegra186_dspk_dais));
	if (ret != 0) {
		dev_err(&pdev->dev, "Could not register CODEC: %d\n", ret);
		goto err_suspend;
	}

	if (of_property_read_string(np, "prod-name", &prod_name) == 0) {
		ret = tegra_pinctrl_config_prod(&pdev->dev, prod_name);
		if (ret < 0)
			dev_warn(&pdev->dev, "Failed to set %s setting\n",
					prod_name);
	}

	if (of_property_read_u32(np, "nvidia,is-pinctrl",
				&dspk->is_pinctrl) < 0)
		dspk->is_pinctrl = 0;

	if (dspk->is_pinctrl) {
		dspk->pinctrl = devm_pinctrl_get(&pdev->dev);
		if (IS_ERR(dspk->pinctrl)) {
			dev_warn(&pdev->dev, "Missing pinctrl device\n");
			goto err_dap;
		}

		dspk->pin_active_state = pinctrl_lookup_state(dspk->pinctrl,
									"dap_active");
		if (IS_ERR(dspk->pin_active_state)) {
			dev_warn(&pdev->dev, "Missing dap-active state\n");
			goto err_dap;
		}

		dspk->pin_idle_state = pinctrl_lookup_state(dspk->pinctrl,
								"dap_inactive");
		if (IS_ERR(dspk->pin_idle_state)) {
			dev_warn(&pdev->dev, "Missing dap-inactive state\n");
			goto err_dap;
		}

		ret = pinctrl_select_state(dspk->pinctrl, dspk->pin_idle_state);
		if (ret < 0) {
			dev_err(&pdev->dev, "setting state failed\n");
			goto err_dap;
		}
	}

err_dap:
	dev_set_drvdata(&pdev->dev, dspk);

	return 0;

err_suspend:
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra186_dspk_runtime_suspend(&pdev->dev);
err_pm_disable:
	pm_runtime_disable(&pdev->dev);
err_plla_clk_put:
	devm_clk_put(&pdev->dev, dspk->clk_pll_a_out0);
err_clk_put:
	devm_clk_put(&pdev->dev, dspk->clk_dspk);
err:
	return ret;
}



static int tegra186_dspk_platform_remove(struct platform_device *pdev)
{
	struct tegra186_dspk *dspk;

	dspk = dev_get_drvdata(&pdev->dev);
	snd_soc_unregister_codec(&pdev->dev);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra186_dspk_runtime_suspend(&pdev->dev);

	devm_clk_put(&pdev->dev, dspk->clk_pll_a_out0);
	devm_clk_put(&pdev->dev, dspk->clk_dspk);

	return 0;
}

static const struct dev_pm_ops tegra186_dspk_pm_ops = {
	SET_RUNTIME_PM_OPS(tegra186_dspk_runtime_suspend,
				tegra186_dspk_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(tegra186_dspk_suspend, NULL)
};

static struct platform_driver tegra186_dspk_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = tegra186_dspk_of_match,
		.pm = &tegra186_dspk_pm_ops,
	},
	.probe = tegra186_dspk_platform_probe,
	.remove = tegra186_dspk_platform_remove,
};
module_platform_driver(tegra186_dspk_driver);


MODULE_AUTHOR("Mohan Kumar <mkumard@nvidia.com>");
MODULE_DESCRIPTION("Tegra186 DSPK ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, tegra186_dspk_of_match);
