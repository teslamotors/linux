/*
 * drivers/video/tegra/dc/null_or.h
 *
 * Copyright (c) 2014, NVIDIA CORPORATION, All rights reserved.
 * Author: Aron Wong <awong@nvidia.com>
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

#ifndef __DRIVERS_VIDEO_TEGRA_DC_NULL_OR_H
#define __DRIVERS_VIDEO_TEGRA_DC_NULL_OR_H

extern struct tegra_dc_out_ops tegra_dc_null_ops;
extern int tegra_dc_init_null_or(struct tegra_dc *dc);

#endif
