/*
 * Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/clk/tegra.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/tegra-soc.h>

#include "clk-t18x.h"

/* Needed for a nvdisp linsim clock hack */
#define CLK_RST_CONTROLLER_RST_DEV_NVDISPLAY0_CLR_0 0x800008
#define CLK_RST_CONTROLLER_CLK_OUT_ENB_NVDISPLAY0_SET_0 0x801004

static void __init tegra186_clock_init(struct device_node *np)
{
	int err;

	printk(KERN_INFO "Registering Tegra186 clocks (this may take a while)...");
	err = PTR_RET(tegra_bpmp_clk_init(np));
	printk("done\n");

	if (err)
		pr_err("Failed to initialize Tegra186 clocks. err: %d\n", err);

	/* Nvdisp linsim clock hack */
	if (tegra_platform_is_linsim() || tegra_platform_is_fpga()) {
		void __iomem *base;
		base = of_iomap(np, 0);
		if (!base) {
			pr_err("ioremap Tegra186 CAR failed\n");
			return;
		}

		writel(0x3ff, base + CLK_RST_CONTROLLER_RST_DEV_NVDISPLAY0_CLR_0);
		writel(0xf, base + CLK_RST_CONTROLLER_CLK_OUT_ENB_NVDISPLAY0_SET_0);
	}
}

static const struct of_device_id tegra186_clock_ids[] __initconst = {
	{ .compatible = "nvidia,tegra18x-car",  .data = tegra186_clock_init},
	{},
};

static int __init tegra186_of_clk_init(void)
{
	of_clk_init(tegra186_clock_ids);

	return 0;
}

arch_initcall(tegra186_of_clk_init);
