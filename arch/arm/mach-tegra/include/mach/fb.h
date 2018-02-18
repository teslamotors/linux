/*
 * arch/arm/mach-tegra/include/mach/fb.h
 *
 * Copyright (C) 2010 Google, Inc.
 * Author: Erik Gilling <konkers@google.com>
 *
 * Copyright (c) 2010-2016, NVIDIA CORPORATION, All rights reserved.
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

#ifndef __MACH_TEGRA_FB_H
#define __MACH_TEGRA_FB_H

#include <linux/fb.h>

struct platform_device;
struct tegra_dc;
struct tegra_fb_data;
struct tegra_fb_info;
struct resource;

#ifdef CONFIG_FB_TEGRA
struct tegra_fb_info *tegra_fb_register(struct platform_device *ndev,
					struct tegra_dc *dc,
					struct tegra_fb_data *fb_data,
					struct resource *fb_mem,
					void *virt_addr);
void tegra_fb_unregister(struct tegra_fb_info *fb_info);
void tegra_fb_pan_display_reset(struct tegra_fb_info *fb_info);
void tegra_fb_update_monspecs(struct tegra_fb_info *fb_info,
			      struct fb_monspecs *specs,
			      bool (*mode_filter)(const struct tegra_dc *dc,
						  struct fb_videomode *mode));
void tegra_fb_update_fix(struct tegra_fb_info *fb_info,
				struct fb_monspecs *specs);
struct fb_var_screeninfo *tegra_fb_get_var(struct tegra_fb_info *fb_info);
int tegra_fb_create_sysfs(struct device *dev);
void tegra_fb_remove_sysfs(struct device *dev);
int tegra_fb_update_modelist(struct tegra_dc *dc, int fblistindex);
struct tegra_dc_win *tegra_fb_get_win(struct tegra_fb_info *tegra_fb);
struct tegra_dc_win *tegra_fb_get_blank_win(struct tegra_fb_info *tegra_fb);
int tegra_fb_set_win_index(struct tegra_dc *dc, unsigned long win_mask);
struct fb_videomode *tegra_fb_get_mode(struct tegra_dc *dc);
#else
static inline struct tegra_fb_info *tegra_fb_register(
	struct platform_device *ndev, struct tegra_dc *dc,
	struct tegra_fb_data *fb_data, struct resource *fb_mem)
{
	return NULL;
}

static inline void tegra_fb_unregister(struct tegra_fb_info *fb_info)
{
}

static inline void tegra_fb_pan_display_reset(struct tegra_fb_info *fb_info)
{
}

static inline void tegra_fb_update_monspecs(struct tegra_fb_info *fb_info,
					    struct fb_monspecs *specs,
				bool (*mode_filter)(struct fb_videomode *mode))
{
}

static inline void tegra_fb_update_fix(struct tegra_fb_info *fb_info,
					struct fb_monspecs *specs)
{
}

static struct fb_var_screeninfo *tegra_fb_get_var(struct tegra_fb_info *fb_info)
{
	return NULL;
}

static inline int tegra_fb_create_sysfs(struct device *dev)
{
	return -ENOENT;
}

static inline void tegra_fb_remove_sysfs(struct device *dev)
{
}

static inline struct tegra_dc_win *tegra_fb_get_win(
				struct tegra_fb_info *tegra_fb)
{
	return NULL;
}

static inline struct tegra_dc_win *tegra_fb_get_blank_win(
				struct tegra_fb_info *tegra_fb)
{
	return NULL;

}

static inline struct fb_videomode *tegra_fb_get_mode(struct tegra_dc *dc)
{
	return NULL;
}
static int tegra_fb_set_win_index(struct tegra_dc *dc, unsigned long win_mask)
{
	return 0;
}
#endif
#endif
