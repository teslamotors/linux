/*
 * GM20B GPC MMU
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

#include "gk20a/gk20a.h"
#include "gk20a/kind_gk20a.h"
#include "gk20a/fb_gk20a.h"

#include "hw_fb_gm20b.h"
#include "hw_top_gm20b.h"
#include "hw_gmmu_gm20b.h"

static void fb_gm20b_init_fs_state(struct gk20a *g)
{
	gk20a_dbg_info("initialize gm20b fb");

	gk20a_writel(g, fb_fbhub_num_active_ltcs_r(),
			g->ltc_count);
}

void gm20b_init_uncompressed_kind_map(void)
{
	gk20a_init_uncompressed_kind_map();

	gk20a_uc_kind_map[gmmu_pte_kind_s8_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_s8_2s_v()] =
		gmmu_pte_kind_s8_v();
}

static bool gm20b_kind_supported(u8 k)
{
	return k == gmmu_pte_kind_smsked_message_v()
		|| (k >= gmmu_pte_kind_s8_v() &&
		    k <= gmmu_pte_kind_s8_2s_v());
}

static bool gm20b_kind_z(u8 k)
{
	return (k >= gmmu_pte_kind_s8_v() &&
		 k <= gmmu_pte_kind_s8_2s_v());
}

static bool gm20b_kind_compressible(u8 k)
{
	return (k >= gmmu_pte_kind_s8_v() &&
		 k <= gmmu_pte_kind_s8_2s_v());
}

static bool gm20b_kind_zbc(u8 k)
{
	return (k >= gmmu_pte_kind_s8_v() &&
		 k <= gmmu_pte_kind_s8_2s_v());
}

void gm20b_init_kind_attr(void)
{
	u16 k;

	gk20a_init_kind_attr();

	for (k = 0; k < 256; k++) {
		if (gm20b_kind_supported((u8)k))
			gk20a_kind_attr[k] |= GK20A_KIND_ATTR_SUPPORTED;
		if (gm20b_kind_compressible((u8)k))
			gk20a_kind_attr[k] |= GK20A_KIND_ATTR_COMPRESSIBLE;
		if (gm20b_kind_z((u8)k))
			gk20a_kind_attr[k] |= GK20A_KIND_ATTR_Z;
		if (gm20b_kind_zbc((u8)k))
			gk20a_kind_attr[k] |= GK20A_KIND_ATTR_ZBC;
	}
}

static void gm20b_fb_set_mmu_page_size(struct gk20a *g)
{
	/* set large page size in fb */
	u32 fb_mmu_ctrl = gk20a_readl(g, fb_mmu_ctrl_r());
	fb_mmu_ctrl |= fb_mmu_ctrl_use_pdb_big_page_size_true_f();
	gk20a_writel(g, fb_mmu_ctrl_r(), fb_mmu_ctrl);
}

static bool gm20b_fb_set_use_full_comp_tag_line(struct gk20a *g)
{
	/* set large page size in fb */
	u32 fb_mmu_ctrl = gk20a_readl(g, fb_mmu_ctrl_r());
	fb_mmu_ctrl |= fb_mmu_ctrl_use_full_comp_tag_line_true_f();
	gk20a_writel(g, fb_mmu_ctrl_r(), fb_mmu_ctrl);

	return true;
}

static int gm20b_fb_compression_page_size(struct gk20a *g)
{
	return SZ_128K;
}

static int gm20b_fb_compressible_page_size(struct gk20a *g)
{
	return SZ_64K;
}

static void gm20b_fb_dump_vpr_wpr_info(struct gk20a *g)
{
	u32 val;

	/* print vpr and wpr info */
	val = gk20a_readl(g, fb_mmu_vpr_info_r());
	val &= ~0x3;
	val |= fb_mmu_vpr_info_index_addr_lo_v();
	gk20a_writel(g, fb_mmu_vpr_info_r(), val);
	gk20a_err(dev_from_gk20a(g), "VPR: %08x %08x %08x %08x",
		gk20a_readl(g, fb_mmu_vpr_info_r()),
		gk20a_readl(g, fb_mmu_vpr_info_r()),
		gk20a_readl(g, fb_mmu_vpr_info_r()),
		gk20a_readl(g, fb_mmu_vpr_info_r()));

	val = gk20a_readl(g, fb_mmu_wpr_info_r());
	val &= ~0xf;
	val |= (fb_mmu_wpr_info_index_allow_read_v());
	gk20a_writel(g, fb_mmu_wpr_info_r(), val);
	gk20a_err(dev_from_gk20a(g), "WPR: %08x %08x %08x %08x %08x %08x",
		gk20a_readl(g, fb_mmu_wpr_info_r()),
		gk20a_readl(g, fb_mmu_wpr_info_r()),
		gk20a_readl(g, fb_mmu_wpr_info_r()),
		gk20a_readl(g, fb_mmu_wpr_info_r()),
		gk20a_readl(g, fb_mmu_wpr_info_r()),
		gk20a_readl(g, fb_mmu_wpr_info_r()));

}

void gm20b_init_fb(struct gpu_ops *gops)
{
	gops->fb.reset = fb_gk20a_reset;
	gops->fb.init_fs_state = fb_gm20b_init_fs_state;
	gops->fb.set_mmu_page_size = gm20b_fb_set_mmu_page_size;
	gops->fb.set_use_full_comp_tag_line = gm20b_fb_set_use_full_comp_tag_line;
	gops->fb.compression_page_size = gm20b_fb_compression_page_size;
	gops->fb.compressible_page_size = gm20b_fb_compressible_page_size;
	gops->fb.dump_vpr_wpr_info = gm20b_fb_dump_vpr_wpr_info;
	gm20b_init_uncompressed_kind_map();
	gm20b_init_kind_attr();
}
