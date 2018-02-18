/*
 * drivers/video/tegra/dc/vrr.h
 *
 * Copyright (c) 2015, NVIDIA CORPORATION, All rights reserved.
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

#ifndef __DRIVERS_VIDEO_TEGRA_DC_VRR_H
#define __DRIVERS_VIDEO_TEGRA_DC_VRR_H

int  vrr_create_sysfs(struct device *dev);
void vrr_remove_sysfs(struct device *dev);

#endif
