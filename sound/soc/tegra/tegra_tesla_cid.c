/*
 * tegra_tesla_cid.c - Tegra machine ASoC driver for Tesla CID.
 *
 * Author: Edward Cragg <edward.cragg@codethink.co.uk
 * Copyright (C) 2016 Codethink Ltd.
 *
 * Based on sound/soc/tegra/tegra_wm8903.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/clk.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include <soc/tegra/tesla.h>

#include "tegra_asoc_utils.h"

struct tegra_cid {
	struct tegra_asoc_utils_data util_data;
	struct clk *clk_sync;
};

static int tegra_cid_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params)
{
	/* currently we don't need to set anything here */
	return 0;
}

static struct snd_soc_ops tegra_cid_ops = {
	.hw_params = tegra_cid_hw_params,
};

static int tegra_cid_link_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret;

	/* set DAI slots, 4 channels, all channels are enabled */
	ret = snd_soc_dai_set_tdm_slot(cpu_dai, 0x0f, 0x0f, 4, 32);
	if (ret < 0)
		return ret;

	return 0;
}

static struct snd_soc_dai_link tegra_cid_dai[] = {
	{
		.name = "DIT Codec",
		.stream_name = "I2S-Digital",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.dai_fmt = SND_SOC_DAIFMT_DSP_B |
			   SND_SOC_DAIFMT_NB_NF |
			   SND_SOC_DAIFMT_CBM_CFM,
		.init = tegra_cid_link_init,
		.ops = &tegra_cid_ops,
	},
};

static struct snd_soc_card snd_soc_tegra_cid = {
	.name = "tegra-generic",
	.owner = THIS_MODULE,
	.dai_link = tegra_cid_dai,
	.num_links = ARRAY_SIZE(tegra_cid_dai),
};

static int tegra_cid_driver_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct snd_soc_card *card = &snd_soc_tegra_cid;
	struct device *dev = &pdev->dev;
	struct tegra_cid *data;
	int i, ret;

	data = devm_kzalloc(dev, sizeof(struct tegra_cid), GFP_KERNEL);
	if (!data) {
		dev_err(&pdev->dev, "Can't allocate tegra_cid struct\n");
		return -ENOMEM;
	}

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, data);

	ret = snd_soc_of_parse_card_name(card, "nvidia,model");
	if (ret)
		goto err;

	for (i = 0; i < ARRAY_SIZE(tegra_cid_dai); i++) {
		tegra_cid_dai[i].cpu_of_node = of_parse_phandle(np,
				"nvidia,i2s-controller", i);
		if (!tegra_cid_dai[i].cpu_of_node) {
			dev_err(dev,
				"Property 'nvidia,i2s-controller' missing or invalid\n");
			ret = -EINVAL;
			goto err;
		}

		tegra_cid_dai[i].platform_of_node =
			tegra_cid_dai[i].cpu_of_node;
	}

	data->clk_sync = devm_clk_get(dev, "i2s_sync");
	if (IS_ERR(data->clk_sync)) {
		dev_err(dev, "failed to get i2s_sync clock\n");
		ret = PTR_ERR(data->clk_sync);
		goto err;
	}

	ret = clk_set_rate(data->clk_sync, tegra_audio_tdm_rate);
	if (ret) {
		dev_err(dev, "failed to set rate of i2s_sync clock\n");
		goto err;
	}

	ret = tegra_asoc_utils_init(&data->util_data, dev);
	if (ret) {
		dev_err(dev, "tegra_asoc_utils_init failed (%d)\n", ret);
		goto err;
	}

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(dev, "snd_soc_register_card failed (%d)\n", ret);
		goto err_fini_utils;
	}

	return 0;

err_fini_utils:
	tegra_asoc_utils_fini(&data->util_data);
err:
	return ret;
}

static int tegra_cid_driver_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct tegra_cid *data = snd_soc_card_get_drvdata(card);

	snd_soc_unregister_card(card);

	tegra_asoc_utils_fini(&data->util_data);

	return 0;
}

static const struct of_device_id tegra_cid_of_match[] = {
	{ .compatible = "nvidia,tegra-audio-tesla-cid", },
	{},
};

static struct platform_driver tegra_cid_driver = {
	.driver = {
		.name = "tegra-tesla-cid",
		.pm = &snd_soc_pm_ops,
		.of_match_table = tegra_cid_of_match,
	},
	.probe = tegra_cid_driver_probe,
	.remove = tegra_cid_driver_remove,
};

module_platform_driver(tegra_cid_driver);

MODULE_AUTHOR("Edward Cragg <edward.cragg@codethink.co.uk");
MODULE_AUTHOR("Ben Dooks <ben.dooks@codethink.co.uk");
MODULE_DESCRIPTION("Tesla CID Tegra machine ASoC driver");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(of, tegra_cid_of_match);
