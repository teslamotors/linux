/*
 * drivers/gpu/nvgpu/gk20a/platform_gk20a_generic.c
 *
 * GK20A Generic Platform Interface
 *
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

#include "platform_gk20a.h"
#include "hal_gk20a.h"
#include "gk20a.h"

/*
 * gk20a_generic_get_clocks()
 *
 * This function finds clocks in tegra platform and populates
 * the clock information to gk20a platform data.
 */

static int gk20a_generic_get_clocks(struct device *pdev)
{
	struct gk20a_platform *platform = dev_get_drvdata(pdev);

	platform->clk[0] = clk_get_sys("tegra_gk20a.0",	"PLLG_ref");
	platform->clk[1] = clk_get_sys("tegra_gk20a.0", "pwr");
	platform->clk[2] = clk_get_sys("tegra_gk20a.0", "emc");
	platform->num_clks = 3;

	if (IS_ERR(platform->clk[0]) ||
	    IS_ERR(platform->clk[1]) ||
	    IS_ERR(platform->clk[2]))
		goto err_get_clock;

	clk_set_rate(platform->clk[0], UINT_MAX);
	clk_set_rate(platform->clk[1], 204000000);
	clk_set_rate(platform->clk[2], UINT_MAX);

	return 0;

err_get_clock:
	if (!IS_ERR_OR_NULL(platform->clk[0]))
		clk_put(platform->clk[0]);
	if (!IS_ERR_OR_NULL(platform->clk[1]))
		clk_put(platform->clk[1]);
	if (!IS_ERR_OR_NULL(platform->clk[2]))
		clk_put(platform->clk[2]);

	platform->clk[0] = NULL;
	platform->clk[1] = NULL;
	platform->clk[2] = NULL;
	return -ENODEV;
}

static int gk20a_generic_probe(struct device *dev)
{
	gk20a_generic_get_clocks(dev);

	return 0;
}

static int gk20a_generic_late_probe(struct device *dev)
{
	return 0;
}

static int gk20a_generic_remove(struct device *dev)
{
	return 0;
}

struct gk20a_platform gk20a_generic_platform = {
	.probe = gk20a_generic_probe,
	.late_probe = gk20a_generic_late_probe,
	.remove = gk20a_generic_remove,
	.default_big_page_size	= SZ_128K,
};
