/*
 * Clock driver for Maxim Max77620 device.
 *
 * Copyright (c) 2014, NVIDIA Corporation.
 *
 * Author: Chaitanya Bandi <bandik@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/mfd/max77620.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

struct max77620_clks {
	struct device *dev;
	struct max77620_chip *max77620;
	bool enabled_at_boot;
};

static int max77620_clks_prepare(struct max77620_clks *max77620_clks)
{
	int ret;

	ret = max77620_reg_update(max77620_clks->max77620->dev, MAX77620_PWR_SLAVE,
			MAX77620_REG_CNFG1_32K,	MAX77620_CNFG1_32K_OUT0_EN,
			MAX77620_CNFG1_32K_OUT0_EN);
	if (ret < 0)
		dev_err(max77620_clks->dev, "RTC_CONTROL_REG update failed, %d\n",
			ret);

	return ret;
}

static void max77620_clks_unprepare(struct max77620_clks *max77620_clks)
{
	int ret;

	ret = max77620_reg_update(max77620_clks->max77620->dev, MAX77620_PWR_SLAVE,
			MAX77620_REG_CNFG1_32K, MAX77620_CNFG1_32K_OUT0_EN, 0);
	if (ret < 0)
		dev_err(max77620_clks->dev, "RTC_CONTROL_REG update failed, %d\n",
			ret);
}

static int max77620_clks_probe(struct platform_device *pdev)
{
	struct max77620_clks *max77620_clks;
	struct max77620_chip *max77620 = dev_get_drvdata(pdev->dev.parent);
	struct device_node *np = pdev->dev.parent->of_node;
	bool clk32k_enable = false;
	int ret;

	if (np)
		clk32k_enable = of_property_read_bool(np,
					"maxim,enable-clock32k-out");

	max77620_clks = devm_kzalloc(&pdev->dev, sizeof(*max77620_clks),
				GFP_KERNEL);
	if (!max77620_clks)
		return -ENOMEM;

	platform_set_drvdata(pdev, max77620_clks);

	max77620_clks->dev = &pdev->dev;
	max77620_clks->max77620 = max77620;
	max77620_clks->enabled_at_boot = clk32k_enable;

	if (clk32k_enable) {
		ret = max77620_clks_prepare(max77620_clks);
		if (ret < 0)
			dev_err(&pdev->dev,
				"Fail to enable clk32k out %d\n", ret);
		return ret;
	} else {
		max77620_clks_unprepare(max77620_clks);
	}

	return 0;
}

static struct platform_driver max77620_clks_driver = {
	.driver = {
		.name = "max77620-clk",
		.owner = THIS_MODULE,
	},
	.probe = max77620_clks_probe,
};

static int __init max77620_clk_init(void)
{
	return platform_driver_register(&max77620_clks_driver);
}
subsys_initcall(max77620_clk_init);

static void __exit max77620_clk_exit(void)
{
	platform_driver_unregister(&max77620_clks_driver);
}
module_exit(max77620_clk_exit);

MODULE_DESCRIPTION("Clock driver for Maxim max77620 PMIC Device");
MODULE_ALIAS("platform:max77620-clk");
MODULE_AUTHOR("Chaitanya Bandi <bandik@nvidia.com>");
MODULE_LICENSE("GPL v2");
