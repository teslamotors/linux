/*
 * GP10B FB
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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
#include "gm20b/fb_gm20b.h"
#include "gk20a/kind_gk20a.h"

#include "hw_gmmu_gp10b.h"

static void gp10b_init_uncompressed_kind_map(void)
{
	gm20b_init_uncompressed_kind_map();

	gk20a_uc_kind_map[gmmu_pte_kind_z16_2cz_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_z16_ms2_2cz_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_z16_ms4_2cz_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_z16_ms8_2cz_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_z16_ms16_2cz_v()] =
		gmmu_pte_kind_z16_v();

	gk20a_uc_kind_map[gmmu_pte_kind_c32_ms4_4cbra_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c64_ms4_4cbra_v()] =
		gmmu_pte_kind_generic_16bx2_v();
}

static bool gp10b_kind_supported(u8 k)
{
	return (k >= gmmu_pte_kind_z16_2cz_v() &&
		k <= gmmu_pte_kind_z16_ms8_2cz_v())
		|| k == gmmu_pte_kind_z16_ms16_2cz_v()
		|| k == gmmu_pte_kind_c32_ms4_4cbra_v()
		|| k == gmmu_pte_kind_c64_ms4_4cbra_v();
}

static bool gp10b_kind_z(u8 k)
{
	return (k >= gmmu_pte_kind_z16_2cz_v() &&
		 k <= gmmu_pte_kind_z16_ms8_2cz_v()) ||
		k == gmmu_pte_kind_z16_ms16_2cz_v();
}

static bool gp10b_kind_compressible(u8 k)
{
	return (k >= gmmu_pte_kind_z16_2cz_v() &&
		 k <= gmmu_pte_kind_z16_ms8_2cz_v()) ||
		k == gmmu_pte_kind_z16_ms16_2cz_v() ||
	       (k >= gmmu_pte_kind_z16_4cz_v() &&
		 k <= gmmu_pte_kind_z16_ms16_4cz_v()) ||
		k == gmmu_pte_kind_c32_ms4_4cbra_v() ||
		k == gmmu_pte_kind_c64_ms4_4cbra_v();
}

static bool gp10b_kind_zbc(u8 k)
{
	return (k >= gmmu_pte_kind_z16_2cz_v() &&
		 k <= gmmu_pte_kind_z16_ms8_2cz_v()) ||
		k == gmmu_pte_kind_z16_ms16_2cz_v() ||
		k == gmmu_pte_kind_c32_ms4_4cbra_v() ||
		k == gmmu_pte_kind_c64_ms4_4cbra_v();
}

static void gp10b_init_kind_attr(void)
{
	u16 k;

	gm20b_init_kind_attr();

	for (k = 0; k < 256; k++) {
		if (gp10b_kind_supported((u8)k))
			gk20a_kind_attr[k] |= GK20A_KIND_ATTR_SUPPORTED;
		if (gp10b_kind_compressible((u8)k))
			gk20a_kind_attr[k] |= GK20A_KIND_ATTR_COMPRESSIBLE;
		if (gp10b_kind_z((u8)k))
			gk20a_kind_attr[k] |= GK20A_KIND_ATTR_Z;
		if (gp10b_kind_zbc((u8)k))
			gk20a_kind_attr[k] |= GK20A_KIND_ATTR_ZBC;
	}
}

static int gp10b_fb_compression_page_size(struct gk20a *g)
{
	return SZ_64K;
}

static int gp10b_fb_compressible_page_size(struct gk20a *g)
{
	return SZ_4K;
}

void gp10b_init_fb(struct gpu_ops *gops)
{
	gm20b_init_fb(gops);
	gops->fb.compression_page_size = gp10b_fb_compression_page_size;
	gops->fb.compressible_page_size = gp10b_fb_compressible_page_size;

	gp10b_init_uncompressed_kind_map();
	gp10b_init_kind_attr();
}
