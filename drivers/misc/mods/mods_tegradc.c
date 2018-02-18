/*
 * mods_tegradc.c - This file is part of NVIDIA MODS kernel driver.
 *
 * Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA MODS kernel driver is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * NVIDIA MODS kernel driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NVIDIA MODS kernel driver.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/uaccess.h>
#include <mach/dc.h>
#include <../drivers/video/tegra/dc/dc_priv.h>
#if defined(CONFIG_TEGRA_NVSD)
#include <../drivers/video/tegra/dc/nvsd.h>
#endif
#include <../arch/arm/mach-tegra/include/mach/dc.h>
#include "mods_internal.h"
#include <linux/platform/tegra/mc.h>

static void mods_tegra_dc_set_windowattr_basic(struct tegra_dc_win *win,
		       const struct MODS_TEGRA_DC_WINDOW *mods_win)
{
	win->global_alpha = 0;
	win->z            = 0;
	win->stride       = 0;
	win->stride_uv    = 0;

	win->flags = TEGRA_WIN_FLAG_ENABLED;
	if (mods_win->flags & MODS_TEGRA_DC_WINDOW_FLAG_TILED)
		win->flags |= TEGRA_WIN_FLAG_TILED;
#if defined(CONFIG_TEGRA_DC_SCAN_COLUMN)
	if (mods_win->flags & MODS_TEGRA_DC_WINDOW_FLAG_SCAN_COL)
		win->flags |= TEGRA_WIN_FLAG_SCAN_COLUMN;
#endif

	win->fmt = mods_win->pixformat;
	win->x.full = mods_win->x;
	win->y.full = mods_win->y;
	win->w.full = mods_win->w;
	win->h.full = mods_win->h;
	/* XXX verify that this doesn't go outside display's active region */
	win->out_x = mods_win->out_x;
	win->out_y = mods_win->out_y;
	win->out_w = mods_win->out_w;
	win->out_h = mods_win->out_h;

	mods_debug_printk(DEBUG_TEGRADC,
		"mods_tegra_dc_set_windowattr_basic window %u:\n"
		"\tflags : 0x%08x\n"
		"\tfmt   : %u\n"
		"\tinput : (%u, %u, %u, %u)\n"
		"\toutput: (%u, %u, %u, %u)\n",
		win->idx, win->flags, win->fmt, dfixed_trunc(win->x),
		dfixed_trunc(win->y), dfixed_trunc(win->w),
		dfixed_trunc(win->h), win->out_x, win->out_y, win->out_w,
		win->out_h);
}

int esc_mods_tegra_dc_config_possible(struct file *fp,
				struct MODS_TEGRA_DC_CONFIG_POSSIBLE *args)
{
	int i;
	struct tegra_dc *dc = tegra_dc_get_dc(args->head);
	struct tegra_dc_win *dc_wins[DC_N_WINDOWS];
#ifndef CONFIG_TEGRA_ISOMGR
	struct clk *emc_clk = 0;
	unsigned long max_bandwidth = 0;
	unsigned long current_emc_freq = 0;
	unsigned long max_available_bandwidth = 0;
#else
	int ret = -EINVAL;
#endif

	LOG_ENT();

	BUG_ON(args->win_num > DC_N_WINDOWS);

	if (!dc) {
		LOG_EXT();
		return -EINVAL;
	}

	for (i = 0; i < args->win_num; i++) {
		int idx = args->windows[i].index;

		if (args->windows[i].flags &
			MODS_TEGRA_DC_WINDOW_FLAG_ENABLED) {
			mods_tegra_dc_set_windowattr_basic(&dc->tmp_wins[idx],
							  &args->windows[i]);
		} else {
			dc->tmp_wins[idx].flags = 0;
		}
		dc_wins[i] = &dc->tmp_wins[idx];
		mods_debug_printk(DEBUG_TEGRADC,
			"esc_mods_tegra_dc_config_possible head %u, "
			"using index %d for window %d\n",
			args->head, i, idx);
	}

	mods_debug_printk(DEBUG_TEGRADC,
		"esc_mods_tegra_dc_config_possible head %u, "
		"dc->mode.pclk %u\n",
		args->head, dc->mode.pclk);

#ifndef CONFIG_TEGRA_ISOMGR
	max_bandwidth = tegra_dc_get_bandwidth(dc_wins, args->win_num);

	emc_clk = clk_get_sys("tegra_emc", "emc");
	if (IS_ERR(emc_clk)) {
		mods_debug_printk(DEBUG_TEGRADC,
		"esc_mods_tegra_dc_config_possible "
		"invalid clock specified when fetching EMC clock\n");
	} else {
		current_emc_freq = clk_get_rate(emc_clk);
		current_emc_freq /= 1000;
		max_available_bandwidth =
			8 * tegra_emc_freq_req_to_bw(current_emc_freq);
		max_available_bandwidth = (max_available_bandwidth / 100) * 50;
	}

	mods_debug_printk(DEBUG_TEGRADC,
		"esc_mods_tegra_dc_config_possible bandwidth needed = %lu,"
		" bandwidth available = %lu\n",
		max_bandwidth, max_available_bandwidth);

	args->possible = (max_bandwidth <= max_available_bandwidth);
#else
	ret = tegra_dc_bandwidth_negotiate_bw(dc, dc_wins, args->win_num);
	args->possible = (ret == 0);
#endif
	for (i = 0; i < args->win_num; i++) {
		args->windows[i].bandwidth = dc_wins[i]->new_bandwidth;
		mods_debug_printk(DEBUG_TEGRADC,
			"esc_mods_tegra_dc_config_possible head %u, "
			"window %d bandwidth %d\n",
			args->head, dc_wins[i]->idx, dc_wins[i]->new_bandwidth);
	}

	LOG_EXT();
	return 0;
}

#ifdef CONFIG_TEGRA_NVSD
static struct tegra_dc_sd_settings mods_sd_settings[TEGRA_MAX_DC] = {
	{
		.enable = 0,
		.sd_brightness = NULL,
		.bl_device_name = NULL,
		.bl_device = NULL,
	},
	{
		.enable = 0,
		.sd_brightness = NULL,
		.bl_device_name = NULL,
		.bl_device = NULL,
	}
};
static struct tegra_dc_sd_settings *tegra_dc_saved_sd_settings[TEGRA_MAX_DC];
#endif

#define SD_KINIT_BIAS(x)  (((x) & 0x3) << 29)

int esc_mods_tegra_dc_setup_sd(struct file *fp,
	struct MODS_TEGRA_DC_SETUP_SD *args)
{
#if !defined(CONFIG_TEGRA_NVDISPLAY)
	int i;
	struct tegra_dc *dc = tegra_dc_get_dc(args->head);
	struct tegra_dc_sd_settings *sd_settings = dc->out->sd_settings;
#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
	u32 val;
#endif
	u32 bw_idx;
	LOG_ENT();

	BUG_ON(args->head > TEGRA_MAX_DC);

	sd_settings->enable = args->enable ? 1 : 0;
	sd_settings->use_auto_pwm = false;
	sd_settings->hw_update_delay = 0;

	sd_settings->aggressiveness = args->aggressiveness;
	sd_settings->bin_width = (1 << args->bin_width_log2);

	sd_settings->phase_in_settings = 0;
	sd_settings->phase_in_adjustments = 0;
	sd_settings->cmd = 0;
	sd_settings->final_agg = args->aggressiveness;
	sd_settings->cur_agg_step = 0;
	sd_settings->phase_settings_step = 0;
	sd_settings->phase_adj_step = 0;
	sd_settings->num_phase_in_steps = 0;

	sd_settings->agg_priorities.agg[0] = args->aggressiveness;

	sd_settings->use_vid_luma = args->use_vid_luma;
	sd_settings->coeff.r = args->csc_r;
	sd_settings->coeff.g = args->csc_g;
	sd_settings->coeff.b = args->csc_b;

	sd_settings->k_limit_enable = (args->klimit != 0);
	sd_settings->k_limit = args->klimit;
	sd_settings->sd_window_enable = true;

	sd_settings->sd_window.h_position = args->win_x;
	sd_settings->sd_window.v_position = args->win_y;
	sd_settings->sd_window.h_size     = args->win_w;
	sd_settings->sd_window.v_size     = args->win_h;

	sd_settings->soft_clipping_enable    = true;
	sd_settings->soft_clipping_threshold = args->soft_clipping_threshold;


	sd_settings->smooth_k_enable = (args->smooth_k_inc != 0);
	sd_settings->smooth_k_incr   = args->smooth_k_inc;

	sd_settings->sd_proc_control = false;
	sd_settings->soft_clipping_correction = false;
	sd_settings->use_vpulse2 = false;

	sd_settings->fc.time_limit = 0;
	sd_settings->fc.threshold  = 0;

	sd_settings->blp.time_constant = 1024;
	sd_settings->blp.step          = 0;


#ifdef CONFIG_TEGRA_SD_GEN2
	bw_idx = 0;
#else
	bw_idx = args->bin_width_log2;
#endif
	for (i = 0; i < MODS_TEGRA_DC_SETUP_BLTF_SIZE; i++) {
		sd_settings->bltf[bw_idx][i/4][i%4] =
			args->bltf[i];
	}

	for (i = 0; i < MODS_TEGRA_DC_SETUP_SD_LUT_SIZE; i++) {
		sd_settings->lut[bw_idx][i].r =
			args->lut[i] & 0xff;
		sd_settings->lut[bw_idx][i].g =
			(args->lut[i] >> 8) & 0xff;
		sd_settings->lut[bw_idx][i].b =
			(args->lut[i] >> 16) & 0xff;
	}

#if defined(CONFIG_TEGRA_NVSD)
	nvsd_init(dc, sd_settings);
#endif
#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
	tegra_dc_io_start(dc);
	val = tegra_dc_readl(dc, DC_DISP_SD_CONTROL);
	val &= ~SD_KINIT_BIAS(0);
	val &= ~SD_CORRECTION_MODE_MAN;
	tegra_dc_writel(dc, val | SD_KINIT_BIAS(args->k_init_bias),
		DC_DISP_SD_CONTROL);
	tegra_dc_io_end(dc);
#endif

	if (dc->enabled) {
		mutex_lock(&dc->lock);
		tegra_dc_get(dc);
		tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);
		tegra_dc_put(dc);
		mutex_unlock(&dc->lock);
	}

	LOG_EXT();
#endif
	return 0;
}

int mods_init_tegradc(void)
{
#if defined(CONFIG_TEGRA_NVSD)
	int i;
	int ret = 0;
	LOG_ENT();
	for (i = 0; i < TEGRA_MAX_DC; i++) {
		struct tegra_dc *dc = tegra_dc_get_dc(i);
		if (!dc)
			continue;

		tegra_dc_saved_sd_settings[i] = dc->out->sd_settings;
		dc->out->sd_settings = &mods_sd_settings[i];

		if (dc->enabled)
			nvsd_init(dc, dc->out->sd_settings);

		if (!tegra_dc_saved_sd_settings[i])
			ret = nvsd_create_sysfs(&dc->ndev->dev);
	}
	LOG_EXT();
	return ret;
#else
	return 0;
#endif
}

void mods_exit_tegradc(void)
{
#if defined(CONFIG_TEGRA_NVSD)
	int i;
	LOG_ENT();
	for (i = 0; i < TEGRA_MAX_DC; i++) {
		struct tegra_dc *dc = tegra_dc_get_dc(i);
		if (!dc)
			continue;
		if (!tegra_dc_saved_sd_settings[i])
			nvsd_remove_sysfs(&dc->ndev->dev);
		dc->out->sd_settings = tegra_dc_saved_sd_settings[i];
		if (dc->enabled)
			nvsd_init(dc, dc->out->sd_settings);
	}
#endif
}
