/*
 * drivers/video/tegra/dc/nvdisp/nvdisp_csc.c
 *
 * Copyright (c) 2014-2016, NVIDIA CORPORATION, All rights reserved.
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
#include "hw_nvdisp_nvdisp.h"
#include "hw_win_nvdisp.h"

#include "dc_priv.h"

static struct tegra_dc_csc_v2 unity_matrix = {
	.r2r = 0x10000,
	.g2r = 0x00000,
	.b2r = 0x00000,
	.const2r = 0x00000,
	.r2g = 0x00000,
	.g2g = 0x10000,
	.b2g = 0x00000,
	.const2g = 0x00000,
	.r2b = 0x00000,
	.g2b = 0x00000,
	.b2b = 0x10000,
	.const2b = 0x00000,
};

void tegra_nvdisp_init_csc_defaults(struct tegra_dc_csc_v2 *csc)
{

	memcpy(csc, &unity_matrix, sizeof(*csc) - sizeof(csc->csc_enable));

	return;
}
EXPORT_SYMBOL(tegra_nvdisp_init_csc_defaults);

int tegra_nvdisp_set_csc(struct tegra_dc_win *win, struct tegra_dc_csc_v2 *csc)
{
	u32 csc_enable = win_window_set_control_csc_disable_f();

	if (WIN_IS_ENABLED(win) && csc->csc_enable) {
		nvdisp_win_write(win, win_r2r_coeff_f(csc->r2r), win_r2r_r());
		nvdisp_win_write(win, win_g2r_coeff_f(csc->g2r), win_g2r_r());
		nvdisp_win_write(win, win_b2r_coeff_f(csc->b2r), win_b2r_r());
		nvdisp_win_write(win,
			win_const2r_coeff_f(csc->const2r), win_const2r_r());

		nvdisp_win_write(win, win_r2g_coeff_f(csc->r2g), win_r2g_r());
		nvdisp_win_write(win, win_g2g_coeff_f(csc->g2g), win_g2g_r());
		nvdisp_win_write(win, win_b2g_coeff_f(csc->b2g), win_b2g_r());
		nvdisp_win_write(win,
			win_const2g_coeff_f(csc->const2g), win_const2g_r());

		nvdisp_win_write(win, win_r2b_coeff_f(csc->r2b), win_r2b_r());
		nvdisp_win_write(win, win_g2b_coeff_f(csc->g2b), win_g2b_r());
		nvdisp_win_write(win, win_b2b_coeff_f(csc->b2b), win_b2b_r());
		nvdisp_win_write(win,
			win_const2b_coeff_f(csc->const2b), win_const2b_r());

		csc_enable = win_window_set_control_csc_enable_f();
	}

	/* Enable the CSC */
	nvdisp_win_write(win, csc_enable, win_window_set_control_r());

	return 0;
}

int tegra_nvdisp_update_csc(struct tegra_dc *dc, int win_idx)
{
	struct tegra_dc_win *win = tegra_dc_get_window(dc, win_idx);

	if (!win)
		return -EFAULT;
	mutex_lock(&dc->lock);

	if (!dc->enabled) {
		mutex_unlock(&dc->lock);
		return -EFAULT;
	}

	win->csc_dirty = true;

	mutex_unlock(&dc->lock);

	return 0;
}
EXPORT_SYMBOL(tegra_nvdisp_update_csc);

