/*
 * tegra_virt_vcm30t124_slave.c - Tegra VCM30 T124 slave Machine driver
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/soc.h>
#include <linux/of_platform.h>

#define DRV_NAME "tegra124-virt-machine-slave"

#define SLAVE_NAME(i) "SLAVE AUDIO" #i
#define STREAM_NAME "playback"
#define CODEC_NAME "spdif-dit"
#define LINK_CPU_NAME   "tegra124-virt-ahub-slave"
#define CPU_DAI_NAME(i) "SLAVE APBIF" #i
#define CODEC_DAI_NAME "dit-hifi"
#define PLATFORM_NAME LINK_CPU_NAME

static struct snd_soc_pcm_stream default_params = {
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
};

static struct snd_soc_dai_link tegra_vcm30t124_slave_links[] = {
	{
		/* 0 */
		.name = SLAVE_NAME(0),
		.stream_name = STREAM_NAME,
		.codec_name = CODEC_NAME,
		.cpu_name = LINK_CPU_NAME,
		.cpu_dai_name = CPU_DAI_NAME(0),
		.codec_dai_name = CODEC_DAI_NAME,
		.platform_name = PLATFORM_NAME,
	},
	{
		/* 1 */
		.name = SLAVE_NAME(1),
		.stream_name = STREAM_NAME,
		.codec_name = CODEC_NAME,
		.cpu_name = LINK_CPU_NAME,
		.cpu_dai_name = CPU_DAI_NAME(1),
		.codec_dai_name = CODEC_DAI_NAME,
		.platform_name = PLATFORM_NAME,
	},
	{
		/* 2 */
		.name = SLAVE_NAME(2),
		.stream_name = STREAM_NAME,
		.codec_name = CODEC_NAME,
		.cpu_name = LINK_CPU_NAME,
		.cpu_dai_name = CPU_DAI_NAME(2),
		.codec_dai_name = CODEC_DAI_NAME,
		.platform_name = PLATFORM_NAME,
	},
	{
		/* 3 */
		.name = SLAVE_NAME(3),
		.stream_name = STREAM_NAME,
		.codec_name = CODEC_NAME,
		.cpu_name = LINK_CPU_NAME,
		.cpu_dai_name = CPU_DAI_NAME(3),
		.codec_dai_name = CODEC_DAI_NAME,
		.platform_name = PLATFORM_NAME,
	},
	{
		/* 4 */
		.name = SLAVE_NAME(4),
		.stream_name = STREAM_NAME,
		.codec_name = CODEC_NAME,
		.cpu_name = LINK_CPU_NAME,
		.cpu_dai_name = CPU_DAI_NAME(4),
		.codec_dai_name = CODEC_DAI_NAME,
		.platform_name = PLATFORM_NAME,
	},
	{
		/* 5 */
		.name = SLAVE_NAME(5),
		.stream_name = STREAM_NAME,
		.codec_name = CODEC_NAME,
		.cpu_name = LINK_CPU_NAME,
		.cpu_dai_name = CPU_DAI_NAME(5),
		.codec_dai_name = CODEC_DAI_NAME,
		.platform_name = PLATFORM_NAME,
	},
	{
		/* 6 */
		.name = SLAVE_NAME(6),
		.stream_name = STREAM_NAME,
		.codec_name = CODEC_NAME,
		.cpu_name = LINK_CPU_NAME,
		.cpu_dai_name = CPU_DAI_NAME(6),
		.codec_dai_name = CODEC_DAI_NAME,
		.platform_name = PLATFORM_NAME,
	},
	{
		/* 7 */
		.name = SLAVE_NAME(7),
		.stream_name = STREAM_NAME,
		.codec_name = CODEC_NAME,
		.cpu_name = LINK_CPU_NAME,
		.cpu_dai_name = CPU_DAI_NAME(7),
		.codec_dai_name = CODEC_DAI_NAME,
		.platform_name = PLATFORM_NAME,
	},
	{
		/* 8 */
		.name = SLAVE_NAME(8),
		.stream_name = STREAM_NAME,
		.codec_name = CODEC_NAME,
		.cpu_name = LINK_CPU_NAME,
		.cpu_dai_name = CPU_DAI_NAME(8),
		.codec_dai_name = CODEC_DAI_NAME,
		.platform_name = PLATFORM_NAME,
	},
	{
		/* 9 */
		.name = SLAVE_NAME(9),
		.stream_name = STREAM_NAME,
		.codec_name = CODEC_NAME,
		.cpu_name = LINK_CPU_NAME,
		.cpu_dai_name = CPU_DAI_NAME(9),
		.codec_dai_name = CODEC_DAI_NAME,
		.platform_name = PLATFORM_NAME,
	},
};

static const struct of_device_id tegra124_virt_snd_slave_of_match[] = {
	{ .compatible = "nvidia,tegra124-virt-machine-slave", },
	{},
};

static struct snd_soc_card snd_soc_tegra_vcm30t124_slave = {
	.name = "tegra-virt-pcm",
	.owner = THIS_MODULE,
	.dai_link = tegra_vcm30t124_slave_links,
	.num_links = ARRAY_SIZE(tegra_vcm30t124_slave_links),
	.fully_routed = true,
};

static void tegra_vcm30t124_set_dai_params(
		struct snd_soc_dai_link *tegra_vcm_dai_link,
		struct snd_soc_pcm_stream *user_params,
		unsigned int dai_id)
{
	tegra_vcm_dai_link[dai_id].params = user_params;
}

static int tegra_vcm30t124_slave_driver_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_tegra_vcm30t124_slave;
	int ret = 0;
	int i;
	int apbif_group_id = 0;
	unsigned int num_apbif_masked = 6;
	unsigned int start_offset_masked_apbif = 0;

	card->dev = &pdev->dev;

	if (of_property_read_u32(pdev->dev.of_node,
				"apbif_group_id", &apbif_group_id)) {
		dev_err(&pdev->dev, "property apbif_group_id is not present\n");
		return -ENODEV;
	}

	if (1 == apbif_group_id) {
		num_apbif_masked = 6;
		start_offset_masked_apbif = 4;
	} else if (2 == apbif_group_id) {
		num_apbif_masked = 4;
		start_offset_masked_apbif = 0;
	} else {
		dev_err(&pdev->dev, "Invalid apbif_group_id\n");
		return -ENODEV;
	}

	if (of_property_read_string(pdev->dev.of_node,
		"cardname", &card->name))
			dev_warn(&pdev->dev, "Use default card name\n");

	for (i = 0; i < num_apbif_masked; i++)
		tegra_vcm30t124_set_dai_params(tegra_vcm30t124_slave_links,
						&default_params,
						i + start_offset_masked_apbif);

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
	}
	return ret;
}

static int tegra_vcm30t124_slave_driver_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);

	return 0;
}

static struct platform_driver tegra_vcm30t124_slave_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table =
			of_match_ptr(tegra124_virt_snd_slave_of_match),
	},
	.probe = tegra_vcm30t124_slave_driver_probe,
	.remove = tegra_vcm30t124_slave_driver_remove,
};
module_platform_driver(tegra_vcm30t124_slave_driver);

MODULE_AUTHOR("Aniket Bahadarpurkar <aniketb@nvidia.com>");
MODULE_DESCRIPTION("Tegra+VCM30T124 slave machine ASoC driver");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(of, tegra124_virt_snd_slave_of_match);
MODULE_ALIAS("platform:" DRV_NAME);
