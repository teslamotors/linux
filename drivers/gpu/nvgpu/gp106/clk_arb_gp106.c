/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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

#include "gk20a/gk20a.h"

#include "clk/clk_arb.h"
#include "clk_arb_gp106.h"

static u32 gp106_get_arbiter_clk_domains(struct gk20a *g)
{
	(void)g;
	return (CTRL_CLK_DOMAIN_MCLK|CTRL_CLK_DOMAIN_GPC2CLK);
}

static int gp106_get_arbiter_clk_range(struct gk20a *g, u32 api_domain,
		u16 *min_mhz, u16 *max_mhz)
{
	enum nv_pmu_clk_clkwhich clkwhich;
	struct clk_set_info *p0_info;
	struct clk_set_info *p5_info;
	struct avfsfllobjs *pfllobjs =  &(g->clk_pmu.avfs_fllobjs);

	u16 limit_min_mhz;

	switch (api_domain) {
	case CTRL_CLK_DOMAIN_MCLK:
		clkwhich = clkwhich_mclk;
		break;

	case CTRL_CLK_DOMAIN_GPC2CLK:
		clkwhich = clkwhich_gpc2clk;
		break;

	default:
		return -EINVAL;
	}

	p5_info = pstate_get_clk_set_info(g,
			CTRL_PERF_PSTATE_P5, clkwhich);
	if (!p5_info)
		return -EINVAL;

	p0_info = pstate_get_clk_set_info(g,
			CTRL_PERF_PSTATE_P0, clkwhich);
	if (!p0_info)
		return -EINVAL;

	limit_min_mhz = p5_info->min_mhz;
	/* WAR for DVCO min */
	if (api_domain == CTRL_CLK_DOMAIN_GPC2CLK)
		if ((pfllobjs->max_min_freq_mhz) &&
		(pfllobjs->max_min_freq_mhz > limit_min_mhz))
			limit_min_mhz = pfllobjs->max_min_freq_mhz;

	*min_mhz = limit_min_mhz;
	*max_mhz = p0_info->max_mhz;

	return 0;
}

static int gp106_get_arbiter_clk_default(struct gk20a *g, u32 api_domain,
		u16 *default_mhz)
{
	enum nv_pmu_clk_clkwhich clkwhich;
	struct clk_set_info *p0_info;

	switch (api_domain) {
	case CTRL_CLK_DOMAIN_MCLK:
		clkwhich = clkwhich_mclk;
		break;

	case CTRL_CLK_DOMAIN_GPC2CLK:
		clkwhich = clkwhich_gpc2clk;
		break;

	default:
		return -EINVAL;
	}

	p0_info = pstate_get_clk_set_info(g,
			CTRL_PERF_PSTATE_P0, clkwhich);
	if (!p0_info)
		return -EINVAL;

	*default_mhz = p0_info->max_mhz;

	return 0;
}

void gp106_init_clk_arb_ops(struct gpu_ops *gops)
{
	gops->clk_arb.get_arbiter_clk_domains = gp106_get_arbiter_clk_domains;
	gops->clk_arb.get_arbiter_clk_range = gp106_get_arbiter_clk_range;
	gops->clk_arb.get_arbiter_clk_default = gp106_get_arbiter_clk_default;
}
