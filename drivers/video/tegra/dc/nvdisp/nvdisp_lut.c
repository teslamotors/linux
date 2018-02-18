/*
 * drivers/video/tegra/dc/nvdisp/nvdisp_lut.c
 *
 * Copyright (c) 2010-2015, NVIDIA CORPORATION, All rights reserved.
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

#include <linux/err.h>
#include <linux/types.h>
#include <linux/export.h>
#include <mach/dc.h>

#include "dc_reg.h"
#include "dc_priv.h"
#include "nvdisp_priv.h"
#include "hw_win_nvdisp.h"

void tegra_dc_init_lut_defaults(struct tegra_dc_lut *lut)
{
	u64 i;
	for (i = 0; i < 256; i++)
		lut->rgb[i] = ((i << 40) | (i << 24) | i << 8);
}

static int tegra_nvdisp_loop_lut(struct tegra_dc *dc,
				struct tegra_dc_win *win,
				int(*lambda)(struct tegra_dc_win *win,
						struct tegra_dc_lut *lut))
{
	struct tegra_dc_lut *lut = NULL;

	if (!(win->ppflags & TEGRA_WIN_PPFLAG_CP_FBOVERRIDE))
		lut = &dc->fb_lut;
	else
		lut = &win->lut;

	if (!lambda(win, lut))
		return 0;

	return 1;
}

static int tegra_nvdisp_lut_isdefaults_lambda(struct tegra_dc_win *win,
						struct tegra_dc_lut *lut)
{
	u64 i;
	for (i = 0; i < 256; i++) {
		if (lut->rgb[i] != ((i << 8) | (i << 24) | (i << 40)))
			return 0;
	}

	return 1;
}

static int tegra_nvdisp_set_lut_setreg_lambda(struct tegra_dc_win *win,
						struct tegra_dc_lut *lut)
{
	/* Set the LUT array address */
	nvdisp_win_write(win,
			tegra_dc_reg_l32(lut->phy_addr),
			win_input_lut_base_r());

	nvdisp_win_write(win,
			tegra_dc_reg_h32(lut->phy_addr),
			win_input_lut_base_hi_r());

	return 1;
}

void tegra_dc_set_lut(struct tegra_dc *dc, struct tegra_dc_win *win)
{
	unsigned long val = nvdisp_win_read(win, win_options_r());

	tegra_nvdisp_loop_lut(dc, win, tegra_nvdisp_set_lut_setreg_lambda);

	if (win->ppflags & TEGRA_WIN_PPFLAG_CP_ENABLE)
		val |= win_options_cp_enable_enable_f();
	else
		val &= ~win_options_cp_enable_enable_f();

	nvdisp_win_write(win, val, win_options_r());

	tegra_dc_enable_general_act(dc);
}

static int tegra_dc_update_winlut(struct tegra_dc *dc, int win_idx, int fbovr)
{
	struct tegra_dc_win *win = tegra_dc_get_window(dc, win_idx);

	if (!win)
		return -EINVAL;
	mutex_lock(&dc->lock);
	tegra_dc_get(dc);

	if (!dc->enabled) {
		tegra_dc_put(dc);
		mutex_unlock(&dc->lock);
		return -EFAULT;
	}

	if (fbovr > 0)
		win->ppflags |= TEGRA_WIN_PPFLAG_CP_FBOVERRIDE;
	else if (fbovr == 0)
		win->ppflags &= ~TEGRA_WIN_PPFLAG_CP_FBOVERRIDE;

	if (!tegra_nvdisp_loop_lut(dc, win, tegra_nvdisp_lut_isdefaults_lambda))
		win->ppflags |= TEGRA_WIN_PPFLAG_CP_ENABLE;
	else
		win->ppflags &= ~TEGRA_WIN_PPFLAG_CP_ENABLE;

	tegra_dc_set_lut(dc, win);

	mutex_unlock(&dc->lock);

	tegra_dc_update_windows(&win, 1, NULL, true);
	tegra_dc_sync_windows(&win, 1);

	tegra_dc_put(dc);
	return 0;
}

int tegra_dc_update_lut(struct tegra_dc *dc, int win_idx, int fboveride)
{
	if (win_idx > -1)
		return tegra_dc_update_winlut(dc, win_idx, fboveride);

	for_each_set_bit(win_idx, &dc->valid_windows, DC_N_WINDOWS) {
		int err = tegra_dc_update_winlut(dc, win_idx, fboveride);
		if (err)
			return err;
	}

	return 0;
}
EXPORT_SYMBOL(tegra_dc_update_lut);

