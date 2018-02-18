/*
 * Virtualized GPU L2
 *
 * Copyright (c) 2014-2016 NVIDIA CORPORATION.  All rights reserved.
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

#include "vgpu/vgpu.h"

static int vgpu_determine_L2_size_bytes(struct gk20a *g)
{
	struct gk20a_platform *platform = gk20a_get_platform(g->dev);
	u32 cache_size = 0;

	gk20a_dbg_fn("");

	if (vgpu_get_attribute(platform->virt_handle,
			TEGRA_VGPU_ATTRIB_L2_SIZE, &cache_size))
		dev_err(dev_from_gk20a(g), "unable to get L2 size\n");

	return cache_size;
}

static int vgpu_ltc_init_comptags(struct gk20a *g, struct gr_gk20a *gr)
{
	struct gk20a_platform *platform = gk20a_get_platform(g->dev);
	u32 max_comptag_lines = 0;
	int err;

	gk20a_dbg_fn("");

	err = vgpu_get_attribute(platform->virt_handle,
			TEGRA_VGPU_ATTRIB_CACHELINE_SIZE,
			&gr->cacheline_size);
	err |= vgpu_get_attribute(platform->virt_handle,
			TEGRA_VGPU_ATTRIB_COMPTAGS_PER_CACHELINE,
			&gr->comptags_per_cacheline);
	err |= vgpu_get_attribute(platform->virt_handle,
			TEGRA_VGPU_ATTRIB_SLICES_PER_LTC,
			&gr->slices_per_ltc);
	err |= vgpu_get_attribute(platform->virt_handle,
			TEGRA_VGPU_ATTRIB_COMPTAG_LINES, &max_comptag_lines);
	if (err) {
		dev_err(dev_from_gk20a(g), "failed to get ctags atributes\n");
		return -ENXIO;
	}

	if (max_comptag_lines < 2)
		return -ENXIO;

	err = gk20a_comptag_allocator_init(&gr->comp_tags, max_comptag_lines);
	if (err)
		return err;

	return 0;
}

static void vgpu_ltc_init_fs_state(struct gk20a *g)
{
	struct gk20a_platform *platform = gk20a_get_platform(g->dev);
	u32 ltc_count = 0;
	int err;

	gk20a_dbg_fn("");

	err = vgpu_get_attribute(platform->virt_handle,
			TEGRA_VGPU_ATTRIB_LTC_COUNT, &ltc_count);
	WARN_ON(err);
	g->ltc_count = ltc_count;
}

void vgpu_init_ltc_ops(struct gpu_ops *gops)
{
	gops->ltc.determine_L2_size_bytes = vgpu_determine_L2_size_bytes;
	gops->ltc.init_comptags = vgpu_ltc_init_comptags;
	gops->ltc.init_fs_state = vgpu_ltc_init_fs_state;
	gops->ltc.cbc_ctrl = NULL;
}
