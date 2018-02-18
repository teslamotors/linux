/*
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

#ifndef __LINUX_TEGRA_DSI_BACKLIGHT_H
#define __LINUX_TEGRA_DSI_BACKLIGHT_H

#include <linux/backlight.h>

struct tegra_dsi_bl_platform_data {
	struct tegra_dsi_cmd	*dsi_backlight_cmd;
	u32		n_backlight_cmd;
	unsigned int dft_brightness;
	int which_dc;
	int (*notify)(struct device *dev, int brightness);
	int (*check_fb)(struct device *dev, struct fb_info *info);
};

#endif

