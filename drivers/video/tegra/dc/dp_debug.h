/*
 * drivers/video/tegra/dc/dp_debug.h
 *
 * Copyright (c) 2015 NVIDIA CORPORATION, All rights reserved.
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

#ifndef __DRIVER_VIDEO_TEGRA_DC_DP_DEBUG_H__
#define __DRIVER_VIDEO_TEGRA_DC_DP_DEBUG_H__

#include "sor_regs.h"

struct tegra_dp_test_settings {
	u8	drive_strength;
	u8	preemphasis;
	u8	lanes;
	u8	bitrate;
	u8	tpg;
	u8	dynrange_val;
	char	*bitrate_name;
	char	*patt;
	char	*dynrange;
	bool	disable_ssc;
	bool	disable_tx_pu;
};

#endif
