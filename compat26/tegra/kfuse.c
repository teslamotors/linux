/* compat26/tegra/kfuse.c
 *
 * Copyright 2016 Codethink Ltd.
 *
 * Parts from:
 * arch/arm/mach-tegra/kfuse.c
 *
 * Copyright (C) 2010-2011 NVIDIA Corporation.
 *
 * Rewrite by Ben Dooks <ben.dooks@codethink.co.uk>
 * Copyright 2016 Codethink Ltd.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>

#include <asm/unaligned.h>

#include "../include/compat26.h"

#define FUSE_GPU_INFO		0x390
#define FUSE_GPU_INFO_MASK	(1<<2)

#define KFUSE_STATE		0x80
#define  KFUSE_STATE_DONE	(1u << 16)
#define  KFUSE_STATE_CRCPASS	(1u << 17)
#define KFUSE_KEYADDR		0x88
#define  KFUSE_KEYADDR_AUTOINC	(1u << 16)
#define KFUSE_KEYS		0x8c

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
static struct clk *clk_kfuse;
/* we have to use apbdma to read the kfuse registers */

static inline u32 tegra_kfuse_readl(u32 reg) { return 0; }
static inline void tegra_kfuse_writel(u32 value, u32 reg) {}
static inline int tegra_kfuse_map_regs(struct platform_device *pdev) { return 0; }
#endif

#ifdef CONFIG_ARCH_TEGRA_3x_SOC
static void __iomem *reg_base;
static struct clk *clk_kfuse;

static inline u32 tegra_kfuse_readl(u32 reg)
{
	BUG_ON(!reg_base);
	return readl(reg_base + reg);
}

static inline void tegra_kfuse_writel(u32 value, u32 reg)
{
	BUG_ON(!reg_base);
	writel(value, reg_base + reg);
}

static inline int tegra_kfuse_map_regs(struct platform_device *pdev)
{
	struct resource *res;
	void __iomem *regs;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR_OR_NULL(regs)) {
		dev_err(&pdev->dev, "failed to remap registers\n");
		return PTR_ERR(regs);
	}

	reg_base = regs;
	return 0;
}

static int wait_for_done(void)
{
	u32 reg;
	int retries = 50;

	do {
		reg = tegra_kfuse_readl(KFUSE_STATE);
		if (reg & KFUSE_STATE_DONE)
			return 0;
		msleep(10);
	} while(--retries);

	return -ETIMEDOUT;
}
#endif

static int tegra_kfuse_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret;

	ret = tegra_kfuse_map_regs(pdev);
	if (ret)
		return ret;

	clk_kfuse = devm_clk_get(dev, NULL);
	if (IS_ERR(clk_kfuse)) {
		dev_err(dev, "failed to get interface clock\n");
		return PTR_ERR(clk_kfuse);
	}

	dev_info(dev, "kfuse device probed\n");
	return 0;
}

static const struct of_device_id tegra_kfuse_match[] = {
        { .compatible = "nvidia,tegra30-kfuse", },
        { .compatible = "nvidia,tegra20-kfuse", },
};

static struct platform_driver tegra_kfuse_driver = {
	.probe	= tegra_kfuse_probe,
	.driver	= {
		.of_match_table	= tegra_kfuse_match,
		.name		= "tegra_kfuse"
	},
};

builtin_platform_driver(tegra_kfuse_driver);

/* note, this is only really needed by the drivers/video/fbdev/tegra/dc/nvhdcp.c
 * driver as current, so we will avoid implementing it until it is really
 * needed.
 *
 * It seems the fuse data is only read to obtain HDCP keys to be written to
 * the HDMI block, and most likely not required for the Tesla hardware.
 *
 * not TEGRA_FUSE_BASE 0x7000F800
 * uses TEGRA_KFUSE_BASE 0x7000FC00  fuse@7000f800
 * 
 * in 4.4:
 * TEGRA30_CLK_KFUSE 40
 */
int tegra_kfuse_read(void *dest, size_t len)
{
#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
	memset(dest, 0x00, len);
	return 0;
#elif defined(CONFIG_ARCH_TEGRA_3x_SOC)
	int ret = -EIO;
	u32 *destptr = (u32 *)dest;
	u32 v;

	if (!reg_base) {
		pr_err("%s: register base not mappedn\n", __func__);
		return -EIO;
	}

	clk_enable(clk_kfuse);

	tegra_kfuse_writel(KFUSE_KEYADDR_AUTOINC, KFUSE_KEYADDR);
	wait_for_done();

	if ((tegra_kfuse_readl(KFUSE_STATE) & KFUSE_STATE_CRCPASS) == 0) {
		pr_err("%s: failed crc\n", __func__);
		goto out;
	}

	for (; len > 0; len -= 4, destptr++) {
		v = tegra_kfuse_readl(KFUSE_KEYS);
		put_unaligned_le32(v, destptr);
	}

	ret = 0;
out:
	clk_disable(clk_kfuse);
	return ret;
#endif
}
EXPORT_SYMBOL(tegra_kfuse_read);

int tegra_gpu_register_sets(void)
{
#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
	return 1;
#elif defined(CONFIG_ARCH_TEGRA_3x_SOC)
	static void __iomem *io_base;
	u32 reg;

	if (!io_base) {
		io_base = ioremap(0x60006000, SZ_16K);
		if (!io_base) {
			pr_err("%s: cannot map CLK_RESET area\n", __func__);
			return -1;
		}
	}

	reg = readl(io_base + FUSE_GPU_INFO);
	return (reg & FUSE_GPU_INFO_MASK) ? 1 : 2;
#else
#error ERROR! Neither 2x or 3x Tegra present
#endif
}
EXPORT_SYMBOL(tegra_gpu_register_sets);
