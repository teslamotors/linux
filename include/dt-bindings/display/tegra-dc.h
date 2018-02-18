/*
 * include/dt-bindings/display/tegra-dc.h
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __TEGRA_DC_H
#define __TEGRA_DC_H

/* tegra_dc_out.type */
#define TEGRA_DC_OUT_RGB		0
#define TEGRA_DC_OUT_HDMI		1
#define TEGRA_DC_OUT_DSI		2
#define TEGRA_DC_OUT_DP			3
#define TEGRA_DC_OUT_LVDS		4
#define TEGRA_DC_OUT_NVSR_DP		5
#define TEGRA_DC_OUT_FAKE_DP		6
#define TEGRA_DC_OUT_FAKE_DSIA		7
#define TEGRA_DC_OUT_FAKE_DSIB		8
#define TEGRA_DC_OUT_FAKE_DSI_GANGED	9
#define TEGRA_DC_OUT_NULL		10
/* Keep as the last dc_out_define*/
#define TEGRA_DC_OUT_MAX		11

/* tegra_dc_out.dither */
#define TEGRA_DC_UNDEFINED_DITHER	0
#define TEGRA_DC_DISABLE_DITHER		1
#define TEGRA_DC_ORDERED_DITHER		2
#define TEGRA_DC_ERRDIFF_DITHER		3
#define TEGRA_DC_TEMPORAL_DITHER	4

/* bits for tegra_dc_out.flags */
#define TEGRA_DC_OUT_HOTPLUG_HIGH		(0 << 1)
#define TEGRA_DC_OUT_HOTPLUG_LOW		(1 << 1)
#define TEGRA_DC_OUT_NVHDCP_POLICY_ALWAYS_ON	(0 << 2)
#define TEGRA_DC_OUT_NVHDCP_POLICY_ON_DEMAND	(1 << 2)
#define TEGRA_DC_OUT_CONTINUOUS_MODE		(0 << 3)
#define TEGRA_DC_OUT_ONE_SHOT_MODE		(1 << 3)
#define TEGRA_DC_OUT_N_SHOT_MODE		(1 << 4)
#define TEGRA_DC_OUT_ONE_SHOT_LP_MODE		(1 << 5)
#define TEGRA_DC_OUT_INITIALIZED_MODE		(1 << 6)
/* Makes hotplug GPIO a LP0 wakeup source */
#define TEGRA_DC_OUT_HOTPLUG_WAKE_LP0		(1 << 7)

/* bots for tegra_dc_out.hdcp_policy */
#define TEGRA_DC_HDCP_POLICY_ALWAYS_ON	0
#define TEGRA_DC_HDCP_POLICY_ON_DEMAND	1
#define TEGRA_DC_HDCP_POLICY_ALWAYS_OFF	2

/* tegra_dc_out.align */
#define TEGRA_DC_ALIGN_MSB		0
#define TEGRA_DC_ALIGN_LSB		1

/* tegra_dc_out.order */
#define TEGRA_DC_ORDER_RED_BLUE		0
#define TEGRA_DC_ORDER_BLUE_RED		1

/* tegra_fb_data.flags */
#define TEGRA_FB_FLIP_ON_PROBE		(1 << 0)

/* tegra_dc_platform_data.flags */
#define TEGRA_DC_FLAG_ENABLED		(1 << 0)
#define TEGRA_DC_FLAG_SET_EARLY_MODE		(1 << 1)

/* tegra_dc_out_pin.name */
#define TEGRA_DC_OUT_PIN_DATA_ENABLE	0
#define TEGRA_DC_OUT_PIN_H_SYNC		1
#define TEGRA_DC_OUT_PIN_V_SYNC		2
#define TEGRA_DC_OUT_PIN_PIXEL_CLOCK	3

/* tegra_dc_out_pin.pol */
#define TEGRA_DC_OUT_PIN_POL_LOW	0
#define TEGRA_DC_OUT_PIN_POL_HIGH	1

#endif /* __TEGRA_DC_H */

