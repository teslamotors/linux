/* compat26/tegra/csi.c
 *
 * Copyright 2016 Codethink Ltd.
 */

#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/io.h>

#include <mach/csi.h>

#include "../include/compat26.h"
#include "../include/compat_clk.h"
#include "compat26_internal.h"

static struct clk *vi_clk;
static struct clk *csi_clk;

/* port of arch/arm/mach-tegra/csi.c */

static int tegra_vi_csi_getclocks(void)
{
	struct device *vi_dev;

	if (vi_clk && csi_clk) {
		clk_enable(vi_clk);
		clk_enable(csi_clk);
		return 0;
	}

	vi_dev = tegra26_get_dev(__DEV_VI);
	if (!vi_dev) {
		pr_err("%s: cannot get vi device\n", __func__);
		return -EINVAL;
	}

	if (!vi_clk)
		vi_clk = clk_get(vi_dev, NULL);
	if (!csi_clk)
		csi_clk = clk_get(vi_dev, "csi");

	if (IS_ERR_OR_NULL(vi_clk) || IS_ERR_OR_NULL(csi_clk)) {
		if (IS_ERR_OR_NULL(vi_clk)) {
			vi_clk = NULL;
			pr_err("%s: cannot get vi clock\n", __func__);
		}

		if (IS_ERR_OR_NULL(csi_clk)) {
			csi_clk = NULL;
			pr_err("%s: cannot get csi clock\n", __func__);
		}
		return -EINVAL;
	}

	//clk_prepare_enable(vi_clk);
	//clk_prepare_enable(csi_clk);
	clk_prepare(vi_clk);
	clk_prepare(csi_clk);
	tegra26_compat_clk_enable(vi_clk);
	tegra26_compat_clk_enable(csi_clk);

	return 0;
}

static inline void tegra_vi_csi_clkoff(void)
{
	clk_disable(vi_clk);
	clk_disable(csi_clk);
}

int tegra_vi_csi_writel(u32 val, u32 offset)
{
	void __iomem *vi_base;
	int ret;

	ret = tegra_vi_csi_getclocks();
	if (ret)
		return ret;

	vi_base = tegra26_get_iomap(__DEV_VI);
	if (!vi_base)
		return -EINVAL;

	writel(val, vi_base + offset * 4);
	tegra_vi_csi_clkoff();

	return 0;
}

int tegra_vi_csi_readl(u32 offset, u32 *val)
{
	void __iomem *vi_base;
	int ret;

	ret = tegra_vi_csi_getclocks();
	if (ret)
		return ret;

	vi_base = tegra26_get_iomap(__DEV_VI);
	if (!vi_base)
		return -EINVAL;

	*val = readl(vi_base + offset * 4);
	tegra_vi_csi_clkoff();
	return 0;
}

/* todo
 * - get proper referecnes to the devices for the vi/csi
 * - get clocks once
 * - ensure we remap the vi base from the device
*/
