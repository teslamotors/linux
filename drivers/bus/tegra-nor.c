/*
 * Driver for Nvidia NOR Flash bus a.k.a SNOR/GMI.
 *
 * Copyright (C) 2016 Host Mobility AB. All rights reserved.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/regmap.h>

#define TEGRA_NOR_TIMING_REG_COUNT	2

#define TEGRA_NOR_CONFIG			0x00
#define TEGRA_NOR_STATUS			0x04
#define TEGRA_NOR_ADDR_PTR			0x08
#define TEGRA_NOR_AHB_ADDR_PTR		0x0c
#define TEGRA_NOR_TIMING0			0x10
#define TEGRA_NOR_TIMING1			0x14
#define TEGRA_NOR_MIO_CONFIG		0x18
#define TEGRA_NOR_MIO_TIMING		0x1c
#define TEGRA_NOR_DMA_CONFIG		0x20
#define TEGRA_NOR_CS_MUX_CONFIG		0x24

#define TEGRA_NOR_CONFIG_GO				BIT(31)

static const struct of_device_id nor_id_table[] = {
	/* Tegra30 */
	{ .compatible = "nvidia,tegra30-nor", .data = NULL, },
	/* Tegra20 */
	{ .compatible = "nvidia,tegra20-nor", .data = NULL, },

	{ }
};
MODULE_DEVICE_TABLE(of, nor_id_table);


static int __init nor_parse_dt(struct platform_device *pdev,
				void __iomem *base)
{
	struct device_node *of_node = pdev->dev.of_node;
	u32 config, timing[TEGRA_NOR_TIMING_REG_COUNT];
	int ret;

	ret = of_property_read_u32_array(of_node, "nvidia,cs-timing",
					 timing, TEGRA_NOR_TIMING_REG_COUNT);
	if (!ret) {
		writel(timing[0], base + TEGRA_NOR_TIMING0);
		writel(timing[1], base + TEGRA_NOR_TIMING1);
	}

	ret = of_property_read_u32(of_node, "nvidia,config", &config);
	if (ret)
		return ret;

	config |= TEGRA_NOR_CONFIG_GO;
	writel(config, base + TEGRA_NOR_CONFIG);

	if (of_get_child_count(of_node))
		ret = of_platform_populate(of_node,
				   of_default_bus_match_table,
				   NULL, &pdev->dev);

	return ret;
}

static int __init nor_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct clk *clk;
	void __iomem *base;
	int ret;

	/* get the resource */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	/* get the clock */
	clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	ret = clk_prepare_enable(clk);
	if (ret)
		return ret;

	/* parse the device node */
	ret = nor_parse_dt(pdev, base);
	if (ret) {
		dev_err(&pdev->dev, "%s fail to create devices.\n",
			pdev->dev.of_node->full_name);
		clk_disable_unprepare(clk);
		return ret;
	}

	dev_info(&pdev->dev, "Driver registered.\n");
	return 0;
}

static struct platform_driver nor_driver = {
	.driver = {
		.name		= "tegra-nor",
		.of_match_table	= nor_id_table,
	},
};
module_platform_driver_probe(nor_driver, nor_probe);

MODULE_AUTHOR("Mirza Krak <mirza.krak@gmail.com");
MODULE_DESCRIPTION("NVIDIA Tegra NOR Bus Driver");
MODULE_LICENSE("GPL");
