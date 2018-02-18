/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
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
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/tachometer.h>
#include <linux/slab.h>
#include <linux/reset.h>
#include <linux/of_device.h>
#include <asm/io.h>

/* Since oscillator clock (38.4MHz) serves as a clock source for
 * the tach input controller, 1.0105263MHz (i.e. 38.4/38) has to be
 * used as a clock value in the RPM calculations
 */
#define counter_clock	1010526

struct tegra_tachometer_device {
	struct tachometer_dev *tach_dev;
	struct clk *clk;
	unsigned int clock;
	void __iomem *mmio_base;
	struct reset_control *rstc;
};

static inline u32 tachometer_readl(struct tegra_tachometer_device *tach,
		int reg)
{
	return readl(tach->mmio_base + reg);
}

static inline void tachometer_writeb(struct tegra_tachometer_device *tach,
		u8 val, int reg)
{
	writeb(val, tach->mmio_base + reg);
}

static inline u8 tachometer_readb(struct tegra_tachometer_device *tach,
		int reg)
{
	return readb(tach->mmio_base + reg);
}

static unsigned long tegra_tachometer_read_winlen(struct tachometer_dev *tach)
{
	return 0;
}

static int tegra_tachometer_set_winlen(struct tachometer_dev *tach, u8 win_len)
{
	struct tegra_tachometer_device *tegra_tach;
	u8 tach0, wlen;

	if (hweight8(win_len) != 1) {
		pr_err("Tachometer: Select winlen from {1, 2, 4 or 8} only\n");
		return -1;
	} else if (tach->pulse_per_rev > win_len) {
		pr_err("Tachometer: winlen should be >= PPR value for accurate RPM value\n");
		pr_err("Tachometer: PPR (pulse per revolution) value is: %d\n",
				tach->pulse_per_rev);
		return -1;
	}

	tegra_tach = (struct tegra_tachometer_device *)tachometer_get_drvdata(tach);

	wlen = ffs(win_len) - 1;
	tach0 = tachometer_readb(tegra_tach, TACH_FAN_TACH0_OVERFLOW);
	tach0 &= ~TACH_FAN_TACH0_OVERFLOW_WIN_LEN_MASK;
	tach0 |= (wlen << 1);
	tachometer_writeb(tegra_tach, tach0, TACH_FAN_TACH0_OVERFLOW);
	tach->win_len = win_len;
	return 0;
}

static unsigned long tegra_tachometer_read_rpm(struct tachometer_dev *tach)
{
	struct tegra_tachometer_device *tegra_tach;
	u32 tach0;
	unsigned long period, rpm, denominator, numerator;

	tegra_tach = (struct tegra_tachometer_device *)tachometer_get_drvdata(tach);

	tach0 = tachometer_readl(tegra_tach, TACH_FAN_TACH0);
	if (tach0 & TACH_FAN_TACH0_OVERFLOW_MASK) {
		/* Fan is stalled, clear overflow state */
		pr_info("Tachometer: Overflow is detected\n");
		tachometer_writeb(tegra_tach, (u8)(tach0 >> 24),
				TACH_FAN_TACH0_OVERFLOW);
		return 0;
	}

	period = (tach0 & TACH_FAN_TACH0_PERIOD_MASK) + 1; /* Bug 200046190 */

	numerator = 60 * counter_clock * tach->win_len;
	denominator = period * tach->pulse_per_rev;

	rpm = numerator / denominator;

	return rpm;
}

struct tachometer_ops tegra186_tachometer_ops = {
	.read_rpm = tegra_tachometer_read_rpm,
	.read_winlen = tegra_tachometer_read_winlen,
	.set_winlen = tegra_tachometer_set_winlen,
};

struct tachometer_desc tegra_tachometer_desc = {
	.name = "tegra-tachometer",
	.ops = &tegra186_tachometer_ops,
};

static const struct of_device_id tegra_tachometer_of_match[] = {
	{
		.compatible = "nvidia,tegra186-tachometer"
	},
	{}
};
MODULE_DEVICE_TABLE(of, tegra_tachometer_of_match);

static int tegra_tachometer_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct tegra_tachometer_device *tegra_tach;
	struct tachometer_dev *tach_dev;
	struct tachometer_config config = { };
	struct resource *r;
	int ret;

	tegra_tach = devm_kzalloc(&pdev->dev, sizeof(*tegra_tach), GFP_KERNEL);
	if (!tegra_tach) {
		dev_err(dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r) {
		dev_err(dev, "No memory resource defined\n");
		return -ENODEV;
	}

	tegra_tach->mmio_base = devm_ioremap_resource(dev, r);
	if (IS_ERR_OR_NULL(tegra_tach->mmio_base)) {
		dev_err(dev, "Tachometer io mapping failed");
		return PTR_ERR(tegra_tach->mmio_base);
	}

	tegra_tach->rstc = devm_reset_control_get(dev, "tach");
	if (IS_ERR(tegra_tach->rstc)) {
		dev_err(dev, "Reset control is not found, err: %ld\n",
				PTR_ERR(tegra_tach->rstc));
		return PTR_ERR(tegra_tach->rstc);
	}

	tegra_tach->clk = devm_clk_get(&pdev->dev, "tach");
	if (IS_ERR(tegra_tach->clk)) {
		dev_err(&pdev->dev, "clock error\n");
		tegra_tach->clk = NULL;
		return PTR_ERR(tegra_tach->clk);
	}
	ret = clk_prepare_enable(tegra_tach->clk);
	clk_set_rate(tegra_tach->clk, counter_clock);

	reset_control_reset(tegra_tach->rstc);

	config.of_node = (dev && dev->of_node) ? dev->of_node : np;
	tach_dev = devm_tachometer_register(dev,
			&tegra_tachometer_desc, &config);
	if (IS_ERR(tach_dev)) {
		ret = PTR_ERR(tach_dev);
		pr_err("T186 Tachometer driver init failed, err: %d\n", ret);
		return ret;
	}
	tachometer_set_drvdata(tach_dev, tegra_tach);

	/* As per spec, the WIN_LENGTH value should be greater than or equal to
	 * Pulse Per Revolution Value to measure the accurate time period values
	 */
	if (tach_dev->pulse_per_rev > tach_dev->win_len)
		tach_dev->win_len = tach_dev->pulse_per_rev;
	ret = tegra_tachometer_set_winlen(tach_dev, tach_dev->win_len);
	if (ret)
		dev_info(&pdev->dev, "win len setting is failed\n");

	pr_info("Tachometer driver initialized with pulse_per_rev: %d and win_len: %d\n",
			tach_dev->pulse_per_rev, tach_dev->win_len);


	return 0;
}

static int tegra_tachometer_remove(struct platform_device *pdev)
{
	struct tachometer_dev *tach = platform_get_drvdata(pdev);
	struct tegra_tachometer_device *tegra_tach;
	struct resource *iomem = platform_get_resource(pdev,
			IORESOURCE_MEM, 0);

	if (!tach)
		return 0;

	tegra_tach = (struct tegra_tachometer_device *)tachometer_get_drvdata(tach);
	iounmap(tegra_tach->mmio_base);
	if (iomem)
		release_mem_region(iomem->start, resource_size(iomem));
	kfree(tach);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct platform_driver tegra_tachometer_driver = {
	.driver = {
		.name = "tegra-tachometer",
		.of_match_table = tegra_tachometer_of_match,
	},
	.probe = tegra_tachometer_probe,
	.remove = tegra_tachometer_remove,
};

module_platform_driver(tegra_tachometer_driver);

MODULE_DESCRIPTION("NVIDIA Tegra Tachometer driver");
MODULE_AUTHOR("R Raj Kumar <rrajk@nvidia.com>");
MODULE_LICENSE("GPL v2");
