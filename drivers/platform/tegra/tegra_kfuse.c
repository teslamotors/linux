/*
 * Copyright (c) 2010-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* The kfuse block stores downstream and upstream HDCP keys for use by HDMI
 * module.
 */

#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <linux/platform/tegra/tegra_kfuse.h>

#include <linux/platform/tegra/clock.h>

#include "tegra_kfuse_priv.h"
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
#include "tegra18_kfuse_priv.h"
#include <linux/platform/tegra/tegra18_kfuse.h>
#endif

/* Public API does not provide kfuse structure or device */
static struct kfuse *global_kfuse;

/* register definition */
#define KFUSE_PD                0x24
#define KFUSE_PD_PU             (0u << 0)
#define KFUSE_PD_PD             (1u << 0)
#define KFUSE_STATE             0x80
#define KFUSE_STATE_DONE        (1u << 16)
#define KFUSE_STATE_CRCPASS     (1u << 17)
#define KFUSE_KEYADDR           0x88
#define KFUSE_KEYADDR_AUTOINC   (1u << 16)
#define KFUSE_KEYS              0x8c

u32 tegra_kfuse_readl(struct kfuse *kfuse, unsigned long offset)
{
	return readl(kfuse->aperture + offset);
}

void tegra_kfuse_writel(struct kfuse *kfuse, u32 value, unsigned long offset)
{
	writel(value, kfuse->aperture + offset);
}

struct kfuse *tegra_kfuse_get(void)
{
	return global_kfuse;
}

static int wait_for_done(struct kfuse *kfuse)
{
	u32 reg;
	int retries = 50;

	do {
		reg = tegra_kfuse_readl(kfuse, KFUSE_STATE);
		if (reg & KFUSE_STATE_DONE)
			return 0;
		msleep(10);
	} while (--retries);
	return -ETIMEDOUT;
}

/* read up to KFUSE_DATA_SZ bytes into dest.
 * always starts at the first kfuse.
 */
int tegra_kfuse_read(void *dest, size_t len)
{
	struct kfuse *kfuse = tegra_kfuse_get();
	int err;
	u32 v;
	unsigned cnt;
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
	int ret;
#endif

	if (!kfuse)
		return -ENODEV;

	if (len > KFUSE_DATA_SZ)
		return -EINVAL;

#ifdef CONFIG_ARCH_TEGRA_18x_SOC
	ret = tegra_kfuse_enable_sensing();
	if (ret)
		return ret;
#endif

	err = clk_prepare_enable(kfuse->clk);
	if (err)
		return err;

	if (IS_ENABLED(CONFIG_ARCH_TEGRA_21x_SOC) ||
	    IS_ENABLED(CONFIG_ARCH_TEGRA_18x_SOC)) {
		tegra_kfuse_writel(kfuse, KFUSE_PD_PU, KFUSE_PD);
		udelay(2);
	}

	tegra_kfuse_writel(kfuse, KFUSE_KEYADDR_AUTOINC, KFUSE_KEYADDR);

	err = wait_for_done(kfuse);
	if (err) {
		pr_err("kfuse: read timeout\n");
		clk_disable_unprepare(kfuse->clk);
		return err;
	}

	if ((tegra_kfuse_readl(kfuse, KFUSE_STATE) &
			       KFUSE_STATE_CRCPASS) == 0) {
		pr_err("kfuse: crc failed\n");
		clk_disable_unprepare(kfuse->clk);
		return -EIO;
	}

	for (cnt = 0; cnt < len; cnt += 4) {
		v = tegra_kfuse_readl(kfuse, KFUSE_KEYS);
		memcpy(dest + cnt, &v, sizeof(v));
	}

	if (IS_ENABLED(CONFIG_ARCH_TEGRA_21x_SOC) ||
	    IS_ENABLED(CONFIG_ARCH_TEGRA_18x_SOC))
		tegra_kfuse_writel(kfuse, KFUSE_PD_PD, KFUSE_PD);

	clk_disable_unprepare(kfuse->clk);
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
	tegra_kfuse_disable_sensing();
#endif

	return 0;
}
EXPORT_SYMBOL(tegra_kfuse_read);

static int kfuse_probe(struct platform_device *pdev)
{
	struct kfuse *kfuse;
	struct resource *resource;
	int err;

	if (global_kfuse)
		return -EBUSY;

	kfuse = devm_kzalloc(&pdev->dev, sizeof(*kfuse), GFP_KERNEL);
	if (!kfuse)
		return -ENOMEM;

	if (IS_ENABLED(CONFIG_COMMON_CLK))
		kfuse->clk = devm_clk_get(&pdev->dev, "kfuse");
	else
		kfuse->clk = tegra_get_clock_by_name("kfuse");

	if (IS_ERR(kfuse->clk)) {
		err = PTR_ERR(kfuse->clk);
		goto err_get_clk;
	}

	resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!resource) {
		err = -EINVAL;
		goto err_get_resource;
	}

	kfuse->aperture = devm_ioremap_resource(&pdev->dev, resource);
	if (IS_ERR(kfuse->aperture)) {
		err = PTR_ERR(kfuse->aperture);
		goto err_ioremap;
	}

	kfuse->pdev = pdev;

#ifdef CONFIG_ARCH_TEGRA_18x_SOC
	err = tegra18_kfuse_init(kfuse);
	if (err)
		goto err_tegra18_init;
#endif

	/* for public API */
	global_kfuse = kfuse;
	platform_set_drvdata(pdev, kfuse);

	dev_info(&pdev->dev, "initialized\n");

	return 0;

#ifdef CONFIG_ARCH_TEGRA_18x_SOC
err_tegra18_init:
#endif

err_ioremap:
err_get_resource:
err_get_clk:
	kfuse = NULL;
	return err;
}

static int __exit kfuse_remove(struct platform_device *pdev)
{
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
	struct kfuse *kfuse = platform_get_drvdata(pdev);
	int err;

	err = tegra18_kfuse_deinit(kfuse);
	if (err)
		return err;
#endif

	global_kfuse = NULL;

	dev_info(&pdev->dev, "removed\n");

	return 0;
}

static struct of_device_id tegra_kfuse_of_match[] = {
	{ .compatible = "nvidia,tegra124-kfuse" },
	{ .compatible = "nvidia,tegra210-kfuse" },
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
	{ .compatible = "nvidia,tegra186-kfuse" },
#endif
	{ },
};

static struct platform_driver kfuse_driver = {
	.probe = kfuse_probe,
	.remove = __exit_p(kfuse_remove),
	.driver = {
		.owner = THIS_MODULE,
		.name = "kfuse",
		.of_match_table = tegra_kfuse_of_match,
	},
};

module_platform_driver(kfuse_driver);
