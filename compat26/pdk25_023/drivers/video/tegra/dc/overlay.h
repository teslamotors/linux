/*
 * drivers/video/tegra/dc/overlay.h
 *
 * Copyright (c) 2010-2011, NVIDIA Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __DRIVERS_VIDEO_TEGRA_OVERLAY_H
#define __DRIVERS_VIDEO_TEGRA_OVERLAY_H

struct tegra_overlay_info;

#ifdef CONFIG_TEGRA_OVERLAY
struct tegra_overlay_info *tegra_overlay_register(struct nvhost_device *ndev,
						  struct tegra_dc *dc);
void tegra_overlay_unregister(struct tegra_overlay_info *overlay_info);
void tegra_overlay_disable(struct tegra_overlay_info *overlay_info);
#else
static inline struct tegra_overlay_info *tegra_overlay_register(struct nvhost_device *ndev,
								struct tegra_dc *dc)
{
	return NULL;
}

static inline void tegra_overlay_unregister(struct tegra_overlay_info *overlay_info)
{
}

static inline void tegra_overlay_disable(struct tegra_overlay_info *overlay_info)
{
}
#endif

#endif
