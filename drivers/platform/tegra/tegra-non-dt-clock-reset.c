/*
 * tegra-non-dt-clock-reset: Non DT clock reset driver from reset framework
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/clk/tegra.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/reset-controller.h>

#define MAX_TEGRA_RESET_CLK_TABLE	1024

static DEFINE_MUTEX(tegra_reset_clk_mutex);

static struct clk *tegra_reset_clk[MAX_TEGRA_RESET_CLK_TABLE];
static unsigned long tegra_allocated_clocks;

static int tegra_clk_reset(struct reset_controller_dev *rcdev,
	unsigned long id)
{
	struct clk *clk;

	if (id >= tegra_allocated_clocks)
		return -EINVAL;

	clk = tegra_reset_clk[id];
	if (clk) {
		tegra_periph_reset_assert(clk);
		udelay(2);
		tegra_periph_reset_deassert(clk);
	} else {
		pr_err("tegra-non-dt-clock-reset: Clock not found\n");
		return -EINVAL;
	}
	return 0;
}

static int tegra_clk_assert(struct reset_controller_dev *rcdev,
	unsigned long id)
{
	struct clk *clk;

	if (id >= tegra_allocated_clocks)
		return -EINVAL;

	clk = tegra_reset_clk[id];
	if (!clk) {
		pr_err("tegra-non-dt-clock-reset: Clock not found\n");
		return -EINVAL;
	}

	tegra_periph_reset_assert(clk);
	return 0;
}

static int tegra_clk_deassert(struct reset_controller_dev *rcdev,
	unsigned long id)
{
	struct clk *clk;

	if (id >= tegra_allocated_clocks)
		return -EINVAL;

	clk = tegra_reset_clk[id];
	if (!clk) {
		pr_err("tegra-non-dt-clock-reset: Clock not found\n");
		return -EINVAL;
	}

	tegra_periph_reset_deassert(clk);
	return 0;
}

static int tegra_clk_consumer_map(struct reset_controller_dev *rcdev,
	struct device *dev, const char *conn)
{
	const char *dev_id = dev ? dev_name(dev) : NULL;
	int id;
	struct clk *clk;

	clk = clk_get_sys(dev_id, conn);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	mutex_lock(&tegra_reset_clk_mutex);
	for (id = 0; id < tegra_allocated_clocks; ++id) {
		if (tegra_reset_clk[id] == clk)
			goto done;
	}

	if (tegra_allocated_clocks >= MAX_TEGRA_RESET_CLK_TABLE) {
		id = -EINVAL;
		goto done;
	}

	id = tegra_allocated_clocks;
	tegra_reset_clk[id] = clk;
	tegra_allocated_clocks++;
done:
	mutex_unlock(&tegra_reset_clk_mutex);
	return id;
}

static struct reset_control_ops tegra_reset_clk_ops  = {
	.reset = tegra_clk_reset,
	.assert = tegra_clk_assert,
	.deassert = tegra_clk_deassert,
};

static struct reset_controller_dev tegra_rst_dev = {
	.ops = &tegra_reset_clk_ops,
	.owner = THIS_MODULE,
	.nr_resets = MAX_TEGRA_RESET_CLK_TABLE,
	.consumer_map = tegra_clk_consumer_map,
};

int tegra_non_dt_clock_reset_init(void)
{
	int ret;

	ret = reset_controller_register(&tegra_rst_dev);
	if (ret < 0) {
		pr_err("Tegra reset control registration failed: %d\n", ret);
		return ret;
	}
	pr_info("Tegra reset control registration success\n");
	return 0;
}
EXPORT_SYMBOL(tegra_non_dt_clock_reset_init);
