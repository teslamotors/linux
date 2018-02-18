/*
 * drivers/video/tegra/dc/nvdisp/nvdisp_cursor.c
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


#include <mach/dc.h>

#include "nvdisp.h"
#include "nvdisp_priv.h"

#include "dc_config.h"
#include "dc_priv.h"

/* TODO: these NVDISP register defines are here temporarily
 * to avoid build dependencies between NVDISP and main, and
 * should eventually be moved to dc_reg.h.
 */
#define DC_DISP_PCALC_HEAD_SET_CROPPED_POINT_IN_CURSOR	0x442
#define   CURSOR_POINT_IN(_x, _y)	\
	(((_x) & ((1 << 16) - 1)) |		\
	(((_y) & ((1 << 16) - 1)) << 16))

#define DC_DISP_PCALC_HEAD_SET_CROPPED_SIZE_IN_CURSOR	0x446
#define   CURSOR_CROPPED_SIZE(_width, _height)	\
	(((_width) & ((1 << 16) - 1)) |		\
	(((_height) & ((1 << 16) - 1)) << 16))

/*
cursor SIZE_IN:  height/width of cursor visible in the raster
cursor POINT_IN: h/v offset into cursor surface where scanout begins
*/
int nvdisp_set_cursor_position(struct tegra_dc *dc, s16 x, s16 y)
{
	u16 cursor_width;
	u16 cursor_height;
	u16 cursor_size_in_width;
	u16 cursor_size_in_height;
	u16 point_in_x;
	u16 point_in_y;

	if (!dc) {
		pr_err("%s: Invalid *dc\n", __func__);
		return -EINVAL;
	}
	switch (dc->cursor.size) {
	case TEGRA_DC_CURSOR_SIZE_32X32:
		cursor_width = 32;
		cursor_height = 32;
		break;
	case TEGRA_DC_CURSOR_SIZE_64X64:
		cursor_width = 64;
		cursor_height = 64;
		break;
	case TEGRA_DC_CURSOR_SIZE_128X128:
		cursor_width = 128;
		cursor_height = 128;
		break;
	case TEGRA_DC_CURSOR_SIZE_256X256:
		cursor_width = 256;
		cursor_height = 256;
		break;
	default:
		dev_err(&dc->ndev->dev, "Invalid cursor.size\n");
		return -EINVAL;
	}
	/* crop against right edge or bottom edge */
	if (x > dc->mode.h_active) {
		cursor_size_in_width = 0;
	} else {
		if ((x + cursor_width) > dc->mode.h_active) {
			cursor_size_in_width = cursor_width -
				((x + cursor_width) - dc->mode.h_active);
		} else {
			cursor_size_in_width = cursor_width;
		}
	}
	if (y > dc->mode.v_active) {
		cursor_size_in_height = 0;
	} else {
		if ((y + cursor_height) > dc->mode.v_active) {
			cursor_size_in_height = cursor_height
				- ((y + cursor_height) - dc->mode.v_active);
		} else {
			cursor_size_in_height = cursor_height;
		}
	}
	/* crop against left edge or top */
	if (x >= 0) {
		point_in_x = 0; /* entire cursor visible in active area */
	} else {
		if ((x + cursor_width) > 0) {
			point_in_x = 0 - x;
			cursor_size_in_width -= point_in_x;
		} else {
			point_in_x = 0;
		}
		x = 0;
	}
	if (y >= 0) {
		point_in_y = 0; /* entire cursor visible in active area */
	} else {
		if ((y + cursor_height) > 0) {
			point_in_y = 0 - y;
			cursor_size_in_height -= point_in_y;
		} else {
			point_in_y = 0;
		}
		y = 0;
	}

	tegra_dc_writel(dc,
			CURSOR_CROPPED_SIZE(cursor_size_in_width,
				cursor_size_in_height),
			DC_DISP_PCALC_HEAD_SET_CROPPED_SIZE_IN_CURSOR);
	tegra_dc_writel(dc,
			CURSOR_POINT_IN(point_in_x, point_in_y),
			DC_DISP_PCALC_HEAD_SET_CROPPED_POINT_IN_CURSOR);
	tegra_dc_writel(dc,
			CURSOR_POSITION(x, y),
			DC_DISP_CURSOR_POSITION);

	return 0;
}

int nvdisp_set_cursor_colorfmt(struct tegra_dc *dc)
{
	u32 val, nval;
	u32 newfmt = 0;
	int ret = -EINVAL;

	if (!dc) {
		pr_err("%s: Invalid *dc\n", __func__);
		return ret;
	}

	switch (dc->cursor.colorfmt) {
	case legacy:
		return ret;
	case r8g8b8a8:
		return ret;
	case a1r5g5b5:
		newfmt = CURSOR_FORMAT_T_A1R5G5B5;
		break;
	case a8r8g8b8:
		newfmt = CURSOR_FORMAT_T_A8R8G8B8;
		break;
	default:
		return ret;
	}

	val = tegra_dc_readl(dc, DC_DISP_CORE_HEAD_SET_CONTROL_CURSOR);
	nval = (val & ~SET_CONTROL_CURSOR_FORMAT_MASK) |
		(SET_CONTROL_CURSOR_FORMAT(newfmt));

	if (val != nval) {
		tegra_dc_writel(dc, nval, DC_DISP_CORE_HEAD_SET_CONTROL_CURSOR);
		val = tegra_dc_readl(dc, DC_DISP_CORE_HEAD_SET_CONTROL_CURSOR);
	}

	return 0;
}
