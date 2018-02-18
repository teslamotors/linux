/*
 * GM20B THERMAL
 *
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
 */

#include "gk20a/gk20a.h"
#include "hw_therm_gm20b.h"
#include "therm_gm20b.h"

static int gm20b_init_therm_setup_hw(struct gk20a *g)
{
	u32 v;

	gk20a_dbg_fn("");

	/* program NV_THERM registers */
	gk20a_writel(g, therm_use_a_r(), therm_use_a_ext_therm_0_enable_f() |
			therm_use_a_ext_therm_1_enable_f()  |
			therm_use_a_ext_therm_2_enable_f());
	gk20a_writel(g, therm_evt_ext_therm_0_r(),
			therm_evt_ext_therm_0_slow_factor_f(0x2));
	gk20a_writel(g, therm_evt_ext_therm_1_r(),
			therm_evt_ext_therm_1_slow_factor_f(0x6));
	gk20a_writel(g, therm_evt_ext_therm_2_r(),
			therm_evt_ext_therm_2_slow_factor_f(0xe));

	gk20a_writel(g, therm_grad_stepping_table_r(0),
		therm_grad_stepping_table_slowdown_factor0_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by1p5_f()) |
		therm_grad_stepping_table_slowdown_factor1_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by2_f()) |
		therm_grad_stepping_table_slowdown_factor2_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by4_f()) |
		therm_grad_stepping_table_slowdown_factor3_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by8_f()) |
		therm_grad_stepping_table_slowdown_factor4_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by8_f()));
	gk20a_writel(g, therm_grad_stepping_table_r(1),
		therm_grad_stepping_table_slowdown_factor0_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by8_f()) |
		therm_grad_stepping_table_slowdown_factor1_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by8_f()) |
		therm_grad_stepping_table_slowdown_factor2_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by8_f()) |
		therm_grad_stepping_table_slowdown_factor3_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by8_f()) |
		therm_grad_stepping_table_slowdown_factor4_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by8_f()));

	v = gk20a_readl(g, therm_clk_timing_r(0));
	v |= therm_clk_timing_grad_slowdown_enabled_f();
	gk20a_writel(g, therm_clk_timing_r(0), v);

	v = gk20a_readl(g, therm_config2_r());
	v |= therm_config2_grad_enable_f(1);
	v |= therm_config2_slowdown_factor_extended_f(1);
	gk20a_writel(g, therm_config2_r(), v);

	gk20a_writel(g, therm_grad_stepping1_r(),
			therm_grad_stepping1_pdiv_duration_f(32));

	v = gk20a_readl(g, therm_grad_stepping0_r());
	v |= therm_grad_stepping0_feature_enable_f();
	gk20a_writel(g, therm_grad_stepping0_r(), v);

	return 0;
}

void gm20b_init_therm_ops(struct gpu_ops *gops)
{
	gops->therm.init_therm_setup_hw = gm20b_init_therm_setup_hw;
	gops->therm.elcg_init_idle_filters = gk20a_elcg_init_idle_filters;
}
