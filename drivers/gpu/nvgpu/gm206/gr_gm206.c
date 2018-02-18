/*
 * gm206 GR
 *
 * Copyright (c) 2011-2016, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/types.h>
#include <linux/delay.h>	/* for mdelay */
#include <linux/io.h>
#include <linux/tegra-fuse.h>
#include <linux/vmalloc.h>

#include "gk20a/gk20a.h"

#include "gm20b/gr_gm20b.h"
#include "gr_gm206.h"
#include "hw_fb_gm206.h"
#include "hw_gr_gm206.h"

static void gr_gm206_init_gpc_mmu(struct gk20a *g)
{
	u32 temp;

	gk20a_dbg_info("initialize gpc mmu");

	temp = gk20a_readl(g, fb_mmu_ctrl_r());
	temp &= gr_gpcs_pri_mmu_ctrl_vm_pg_size_m() |
		gr_gpcs_pri_mmu_ctrl_use_pdb_big_page_size_m() |
		gr_gpcs_pri_mmu_ctrl_use_full_comp_tag_line_m() |
		gr_gpcs_pri_mmu_ctrl_vol_fault_m() |
		gr_gpcs_pri_mmu_ctrl_comp_fault_m() |
		gr_gpcs_pri_mmu_ctrl_miss_gran_m() |
		gr_gpcs_pri_mmu_ctrl_cache_mode_m() |
		gr_gpcs_pri_mmu_ctrl_mmu_aperture_m() |
		gr_gpcs_pri_mmu_ctrl_mmu_vol_m() |
		gr_gpcs_pri_mmu_ctrl_mmu_disable_m();
	gk20a_writel(g, gr_gpcs_pri_mmu_ctrl_r(), temp);
	gk20a_writel(g, gr_gpcs_pri_mmu_pm_unit_mask_r(), 0);
	gk20a_writel(g, gr_gpcs_pri_mmu_pm_req_mask_r(), 0);

	gk20a_writel(g, gr_gpcs_pri_mmu_debug_ctrl_r(),
			gk20a_readl(g, fb_mmu_debug_ctrl_r()));
	gk20a_writel(g, gr_gpcs_pri_mmu_debug_wr_r(),
			gk20a_readl(g, fb_mmu_debug_wr_r()));
	gk20a_writel(g, gr_gpcs_pri_mmu_debug_rd_r(),
			gk20a_readl(g, fb_mmu_debug_rd_r()));

	gk20a_writel(g, gr_gpcs_mmu_num_active_ltcs_r(),
		gk20a_readl(g, fb_fbhub_num_active_ltcs_r()));
	/*  TODO: num_active_ltcs2! */
	gk20a_writel(g, 0x50833c, gk20a_readl(g, 0x100804));
}

static void gr_gm206_bundle_cb_defaults(struct gk20a *g)
{
	struct gr_gk20a *gr = &g->gr;

	gr->bundle_cb_default_size =
		gr_scc_bundle_cb_size_div_256b__prod_v();
	gr->min_gpm_fifo_depth =
		gr_pd_ab_dist_cfg2_state_limit_min_gpm_fifo_depths_v();
	gr->bundle_cb_token_limit =
		gr_pd_ab_dist_cfg2_token_limit_init_v();
}

static void gr_gm206_cb_size_default(struct gk20a *g)
{
	struct gr_gk20a *gr = &g->gr;

	if (!gr->attrib_cb_default_size)
		gr->attrib_cb_default_size =
			gr_gpc0_ppc0_cbm_beta_cb_size_v_default_v();
	gr->alpha_cb_default_size =
		gr_gpc0_ppc0_cbm_alpha_cb_size_v_default_v();
}

void gm206_init_gr(struct gpu_ops *gops)
{
	gm20b_init_gr(gops);
	gops->gr.init_gpc_mmu = gr_gm206_init_gpc_mmu;
	gops->gr.bundle_cb_defaults = gr_gm206_bundle_cb_defaults;
	gops->gr.cb_size_default = gr_gm206_cb_size_default;
}
