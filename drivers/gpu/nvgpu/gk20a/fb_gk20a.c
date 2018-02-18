/*
 * GK20A memory interface
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
 */

#include <linux/types.h>

#include "gk20a.h"
#include "kind_gk20a.h"
#include "hw_mc_gk20a.h"
#include "hw_fb_gk20a.h"
#include "fb_gk20a.h"

void fb_gk20a_reset(struct gk20a *g)
{
	u32 val;

	gk20a_dbg_info("reset gk20a fb");

	gk20a_reset(g, mc_enable_pfb_enabled_f()
			| mc_enable_l2_enabled_f()
			| mc_enable_xbar_enabled_f()
			| mc_enable_hub_enabled_f());

	val = gk20a_readl(g, mc_elpg_enable_r());
	val |= mc_elpg_enable_xbar_enabled_f()
		| mc_elpg_enable_pfb_enabled_f()
		| mc_elpg_enable_hub_enabled_f();
	gk20a_writel(g, mc_elpg_enable_r(), val);
}

static void gk20a_fb_set_mmu_page_size(struct gk20a *g)
{
	/* set large page size in fb */
	u32 fb_mmu_ctrl = gk20a_readl(g, fb_mmu_ctrl_r());

	fb_mmu_ctrl = (fb_mmu_ctrl &
		       ~fb_mmu_ctrl_vm_pg_size_f(~0x0)) |
		fb_mmu_ctrl_vm_pg_size_128kb_f();

	gk20a_writel(g, fb_mmu_ctrl_r(), fb_mmu_ctrl);
}

static int gk20a_fb_compression_page_size(struct gk20a *g)
{
	return SZ_128K;
}

static int gk20a_fb_compressible_page_size(struct gk20a *g)
{
	return SZ_64K;
}

void gk20a_init_fb(struct gpu_ops *gops)
{
	gops->fb.reset = fb_gk20a_reset;
	gops->fb.set_mmu_page_size = gk20a_fb_set_mmu_page_size;
	gops->fb.compression_page_size = gk20a_fb_compression_page_size;
	gops->fb.compressible_page_size = gk20a_fb_compressible_page_size;
	gk20a_init_uncompressed_kind_map();
	gk20a_init_kind_attr();
}
