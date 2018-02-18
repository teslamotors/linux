/*
 * drivers/video/tegra/dc/window.c
 *
 * Copyright (C) 2010 Google, Inc.
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
#include <linux/err.h>
#include <linux/types.h>
#include <linux/moduleparam.h>
#include <linux/export.h>
#include <linux/delay.h>
#if defined(CONFIG_ARCH_TEGRA_210_SOC)
#include <soc/tegra/fuse.h>
#endif
#include <mach/dc.h>
#include <mach/hardware.h>
#include <trace/events/display.h>
#include <linux/fb.h>
#include "dc_reg.h"
#include "dc_config.h"
#include "dc_priv.h"

int no_vsync;

module_param_named(no_vsync, no_vsync, int, S_IRUGO | S_IWUSR);

static bool tegra_dc_windows_are_clean(struct tegra_dc_win *windows[],
					     int n)
{
	int i;

	for (i = 0; i < n; i++) {
		if (windows[i]->dirty) {
			return false;
		}
	}

	return true;
}

#ifndef CONFIG_TEGRA_NVDISPLAY
static int get_topmost_window(u32 *depths, unsigned long *wins, int win_num)
{
	int idx, best = -1;

	for_each_set_bit(idx, wins, win_num) {
		if (best == -1 || depths[idx] < depths[best])
			best = idx;
	}
	clear_bit(best, wins);
	return best;
}

static u32 blend_topwin(u32 flags)
{
	if (flags & TEGRA_WIN_FLAG_BLEND_COVERAGE)
		return BLEND(NOKEY, ALPHA, 0xff, 0xff);
	else if (flags & TEGRA_WIN_FLAG_BLEND_PREMULT)
		return BLEND(NOKEY, PREMULT, 0xff, 0xff);
	else
		return BLEND(NOKEY, FIX, 0xff, 0xff);
}

static u32 blend_2win(int idx, unsigned long behind_mask,
						u32 *flags, int xy, int win_num)
{
	int other;

	for (other = 0; other < win_num; other++) {
		if (other != idx && (xy-- == 0))
			break;
	}
	if (BIT(other) & behind_mask)
		return blend_topwin(flags[idx]);
	else if (flags[other])
		return BLEND(NOKEY, DEPENDANT, 0x00, 0x00);
	else
		return BLEND(NOKEY, FIX, 0x00, 0x00);
}

static u32 blend_3win(int idx, unsigned long behind_mask,
						u32 *flags, int win_num)
{
	unsigned long infront_mask;
	int first, second;

	infront_mask = ~(behind_mask | BIT(idx));
	infront_mask &= (BIT(win_num) - 1);
	first = ffs(infront_mask) - 1;

	if (win_num != 3) {
		if (!infront_mask)
			return blend_topwin(flags[idx]);
		else if (behind_mask && first != -1 && flags[first])
			return BLEND(NOKEY, DEPENDANT, 0x00, 0x00);
		else
			return BLEND(NOKEY, FIX, 0x0, 0x0);
	} else {
		if (!infront_mask) {
			return blend_topwin(flags[idx]);
		} else if (behind_mask) {
			/* If the first (top) window is not opaque, check if the
			 * current (middle) window is not opaque. If yes then we need
			 * to enable appropriate blending mode otherwise current window
			 * is dependent on the first window. If the first window is
			 * opaque then the current window has no contribution and we
			 * set its weight to 0.
			 */

			if (flags[first]) {
				if (flags[idx])
					return blend_topwin(flags[idx]);
				else
					return BLEND(NOKEY, DEPENDANT, 0x00, 0x00);
			} else {
				return BLEND(NOKEY, FIX, 0x00, 0x00);
			}
		} else {
			infront_mask &= ~(BIT(first));
			second = ffs(infront_mask) - 1;

			/* If both windows above the bottom window are not opaque then
			 * the bottom window is dependent otherwise the bottom window
			 * has no contribution and we set its weight to 0.
			 */

			if (flags[first] && flags[second])
				return BLEND(NOKEY, DEPENDANT, 0x00, 0x00);
			else
				return BLEND(NOKEY, FIX, 0x00, 0x00);
		}
	}
}

static void tegra_dc_blend_parallel(struct tegra_dc *dc,
				struct tegra_dc_blend *blend)
{
	int win_num = dc->gen1_blend_num;
	unsigned long mask = BIT(win_num) - 1;

	tegra_dc_io_start(dc);
	while (mask) {
		int idx = get_topmost_window(blend->z, &mask, win_num);

		tegra_dc_writel(dc, WINDOW_A_SELECT << idx,
				DC_CMD_DISPLAY_WINDOW_HEADER);
		tegra_dc_writel(dc, BLEND(NOKEY, FIX, 0xff, 0xff),
				DC_WIN_BLEND_NOKEY);
		tegra_dc_writel(dc, blend_2win(idx, mask, blend->flags, 0,
				win_num), DC_WIN_BLEND_2WIN_X);
		tegra_dc_writel(dc, blend_2win(idx, mask, blend->flags, 1,
				win_num), DC_WIN_BLEND_2WIN_Y);
		tegra_dc_writel(dc, blend_3win(idx, mask, blend->flags,
				win_num), DC_WIN_BLEND_3WIN_XY);
	}
	tegra_dc_io_end(dc);
}

static void tegra_dc_blend_sequential(struct tegra_dc *dc,
				struct tegra_dc_blend *blend)
{
	int idx;
	unsigned long mask = dc->valid_windows;

	tegra_dc_io_start(dc);
	for_each_set_bit(idx, &mask, DC_N_WINDOWS) {
		if (!tegra_dc_feature_is_gen2_blender(dc, idx))
			continue;

		tegra_dc_writel(dc, WINDOW_A_SELECT << idx,
				DC_CMD_DISPLAY_WINDOW_HEADER);

		if (blend->flags[idx] & TEGRA_WIN_FLAG_BLEND_COVERAGE) {
#if defined(CONFIG_TEGRA_DC_BLENDER_DEPTH)
			tegra_dc_writel(dc,
					WIN_K1(blend->alpha[idx]) |
					WIN_K2(0xff) |
					WIN_BLEND_ENABLE |
					WIN_DEPTH(dc->blend.z[idx]),
					DC_WINBUF_BLEND_LAYER_CONTROL);
#else
			tegra_dc_writel(dc,
					WIN_K1(blend->alpha[idx]) |
					WIN_K2(0xff) |
					WIN_BLEND_ENABLE,
					DC_WINBUF_BLEND_LAYER_CONTROL);
#endif

			tegra_dc_writel(dc,
			WIN_BLEND_FACT_SRC_COLOR_MATCH_SEL_K1_TIMES_SRC |
			WIN_BLEND_FACT_DST_COLOR_MATCH_SEL_NEG_K1_TIMES_SRC |
			WIN_BLEND_FACT_SRC_ALPHA_MATCH_SEL_K2 |
			WIN_BLEND_FACT_DST_ALPHA_MATCH_SEL_ZERO,
			DC_WINBUF_BLEND_MATCH_SELECT);

			tegra_dc_writel(dc,
			WIN_BLEND_FACT_SRC_COLOR_NOMATCH_SEL_K1_TIMES_SRC |
			WIN_BLEND_FACT_DST_COLOR_NOMATCH_SEL_NEG_K1_TIMES_SRC |
			WIN_BLEND_FACT_SRC_ALPHA_NOMATCH_SEL_K2 |
			WIN_BLEND_FACT_DST_ALPHA_NOMATCH_SEL_ZERO,
			DC_WINBUF_BLEND_NOMATCH_SELECT);

			tegra_dc_writel(dc,
					WIN_ALPHA_1BIT_WEIGHT0(0) |
					WIN_ALPHA_1BIT_WEIGHT1(0xff),
					DC_WINBUF_BLEND_ALPHA_1BIT);
		} else if (blend->flags[idx] & TEGRA_WIN_FLAG_BLEND_PREMULT) {
#if defined(CONFIG_TEGRA_DC_BLENDER_DEPTH)
			tegra_dc_writel(dc,
					WIN_K1(blend->alpha[idx]) |
					WIN_K2(0xff) |
					WIN_BLEND_ENABLE |
					WIN_DEPTH(dc->blend.z[idx]),
					DC_WINBUF_BLEND_LAYER_CONTROL);
#else
			tegra_dc_writel(dc,
					WIN_K1(blend->alpha[idx]) |
					WIN_K2(0xff) |
					WIN_BLEND_ENABLE,
					DC_WINBUF_BLEND_LAYER_CONTROL);
#endif

			tegra_dc_writel(dc,
			WIN_BLEND_FACT_SRC_COLOR_MATCH_SEL_K1 |
			WIN_BLEND_FACT_DST_COLOR_MATCH_SEL_NEG_K1_TIMES_SRC |
			WIN_BLEND_FACT_SRC_ALPHA_MATCH_SEL_K2 |
			WIN_BLEND_FACT_DST_ALPHA_MATCH_SEL_ZERO,
			DC_WINBUF_BLEND_MATCH_SELECT);

			tegra_dc_writel(dc,
			WIN_BLEND_FACT_SRC_COLOR_NOMATCH_SEL_NEG_K1_TIMES_DST |
			WIN_BLEND_FACT_DST_COLOR_NOMATCH_SEL_K1 |
			WIN_BLEND_FACT_SRC_ALPHA_NOMATCH_SEL_K2 |
			WIN_BLEND_FACT_DST_ALPHA_NOMATCH_SEL_ZERO,
			DC_WINBUF_BLEND_NOMATCH_SELECT);

			tegra_dc_writel(dc,
					WIN_ALPHA_1BIT_WEIGHT0(0) |
					WIN_ALPHA_1BIT_WEIGHT1(0xff),
					DC_WINBUF_BLEND_ALPHA_1BIT);
		} else {
#if defined(CONFIG_TEGRA_DC_BLENDER_DEPTH)
			tegra_dc_writel(dc,
					WIN_BLEND_BYPASS |
					WIN_DEPTH(dc->blend.z[idx]),
					DC_WINBUF_BLEND_LAYER_CONTROL);
#else
			tegra_dc_writel(dc,
					WIN_BLEND_BYPASS,
					DC_WINBUF_BLEND_LAYER_CONTROL);
#endif
		}
	}
	tegra_dc_io_end(dc);
}
#endif	/* TEGRA_NVDISPLAY */

/* does not support syncing windows on multiple dcs in one call */
int tegra_dc_sync_windows(struct tegra_dc_win *windows[], int n)
{
	int ret = 0;
	struct tegra_dc *dc = windows[0]->dc;

	if (dc == NULL)
		return -EFAULT;

	if (n < 1 || n > DC_N_WINDOWS)
		return -EINVAL;

	if (!dc->enabled)
		return -EFAULT;

	trace_sync_windows(dc);
	mutex_lock(&dc->lock);
	if (tegra_platform_is_linsim()) {
		/* Don't want to timeout on simulator */
		ret = ___wait_event(dc->wq, tegra_dc_windows_are_clean(windows, n),
			TASK_INTERRUPTIBLE, 0, 0,
			mutex_unlock(&dc->lock); schedule(); mutex_lock(&dc->lock));
	} else {
		/*
		 * Putting the task state as TASK_UINTERRUPTIBLE makes
		 * task wait till windows status promoted or timeout occurred
		 * and wont be interrupted by signal or any other reason.
		 */
		ret = ___wait_event(dc->wq,
			___wait_cond_timeout(tegra_dc_windows_are_clean(windows, n)),
			TASK_UNINTERRUPTIBLE, 0, HZ,
			mutex_unlock(&dc->lock); __ret = schedule_timeout(__ret); mutex_lock(&dc->lock));
	}
	mutex_unlock(&dc->lock);

	if (dc->out_ops && dc->out_ops->release)
		dc->out_ops->release(dc);

	return ret;
}
EXPORT_SYMBOL(tegra_dc_sync_windows);

static inline u32 compute_dda_inc(fixed20_12 in, unsigned out_int,
				  bool v, unsigned Bpp)
{
	/*
	 * min(round((prescaled_size_in_pixels - 1) * 0x1000 /
	 *	     (post_scaled_size_in_pixels - 1)), MAX)
	 * Where the value of MAX is as follows:
	 * For V_DDA_INCREMENT: 15.0 (0xF000)
	 * For H_DDA_INCREMENT:  4.0 (0x4000) for 4 Bytes/pix formats.
	 *			 8.0 (0x8000) for 2 Bytes/pix formats.
	 */

	fixed20_12 out = dfixed_init(out_int);
	u32 dda_inc;
	int max;

	if (v) {
		max = 15;
	} else {
		switch (Bpp) {
		default:
			WARN_ON_ONCE(1);
			/* fallthrough */
		case 4:
			max = 4;
			break;
		case 2:
			max = 8;
			break;
		}
	}

	out.full = max_t(u32, out.full - dfixed_const(1), dfixed_const(1));
	in.full -= dfixed_const(1);

	dda_inc = dfixed_div(in, out);

	dda_inc = min_t(u32, dda_inc, dfixed_const(max));

	return dda_inc;
}

static inline u32 compute_initial_dda(fixed20_12 in)
{
	return dfixed_frac(in);
}

static inline void __maybe_unused tegra_dc_update_scaling(
				struct tegra_dc *dc,
				struct tegra_dc_win *win, unsigned Bpp,
				unsigned Bpp_bw, bool scan_column)
{
	u32 h_dda, h_dda_init;
	u32 v_dda, v_dda_init;

	h_dda_init = compute_initial_dda(win->x);
	v_dda_init = compute_initial_dda(win->y);

	if (scan_column) {
		if (tegra_dc_feature_has_interlace(dc, win->idx)
			&& (dc->mode.vmode == FB_VMODE_INTERLACED)) {
			if (WIN_IS_INTERLACE(win))
				tegra_dc_writel(dc,
					V_PRESCALED_SIZE(dfixed_trunc(win->w)) |
					H_PRESCALED_SIZE(dfixed_trunc(win->h)
					* Bpp),
					DC_WIN_PRESCALED_SIZE);
			else
				tegra_dc_writel(dc,
					V_PRESCALED_SIZE(dfixed_trunc(win->w))
					| H_PRESCALED_SIZE(dfixed_trunc(win->h)
					* Bpp), DC_WIN_PRESCALED_SIZE);
		} else {
			tegra_dc_writel(dc,
				V_PRESCALED_SIZE(dfixed_trunc(win->w)) |
				H_PRESCALED_SIZE(dfixed_trunc(win->h) * Bpp),
				DC_WIN_PRESCALED_SIZE);
		}
		tegra_dc_writel(dc, v_dda_init, DC_WIN_H_INITIAL_DDA);
		tegra_dc_writel(dc, h_dda_init, DC_WIN_V_INITIAL_DDA);
		h_dda = compute_dda_inc(win->h, win->out_w,
				false, Bpp_bw);
		v_dda = compute_dda_inc(win->w, win->out_h,
				true, Bpp_bw);
	} else {
		if (tegra_dc_feature_has_interlace(dc, win->idx)
			&& (dc->mode.vmode == FB_VMODE_INTERLACED)) {
			if (WIN_IS_INTERLACE(win))
				tegra_dc_writel(dc,
					V_PRESCALED_SIZE(dfixed_trunc(win->h)
					>> 1) |
					H_PRESCALED_SIZE(dfixed_trunc(win->w)
					* Bpp), DC_WIN_PRESCALED_SIZE);
			else
				tegra_dc_writel(dc,
					V_PRESCALED_SIZE(dfixed_trunc(win->h)) |
					H_PRESCALED_SIZE(dfixed_trunc(win->w)
					* Bpp),
					DC_WIN_PRESCALED_SIZE);
		} else {
			tegra_dc_writel(dc,
				V_PRESCALED_SIZE(dfixed_trunc(win->h)) |
				H_PRESCALED_SIZE(dfixed_trunc(win->w) * Bpp),
				DC_WIN_PRESCALED_SIZE);
		}
		tegra_dc_writel(dc, h_dda_init, DC_WIN_H_INITIAL_DDA);
		tegra_dc_writel(dc, v_dda_init, DC_WIN_V_INITIAL_DDA);
		h_dda = compute_dda_inc(win->w, win->out_w,
				false, Bpp_bw);
		v_dda = compute_dda_inc(win->h, win->out_h,
				true, Bpp_bw);
	}

	if (tegra_dc_feature_has_interlace(dc, win->idx) &&
		(dc->mode.vmode == FB_VMODE_INTERLACED)) {
		if (WIN_IS_INTERLACE(win))
			tegra_dc_writel(dc, V_DDA_INC(v_dda) |
				H_DDA_INC(h_dda), DC_WIN_DDA_INCREMENT);
		else
			tegra_dc_writel(dc, V_DDA_INC(v_dda << 1) |
				H_DDA_INC(h_dda), DC_WIN_DDA_INCREMENT);

	} else {
		tegra_dc_writel(dc, V_DDA_INC(v_dda) |
			H_DDA_INC(h_dda), DC_WIN_DDA_INCREMENT);
	}
}


/*
 * The immediate flip with hsync can change only few registers, so this call
 * should filter out changes from previous flip that may cause a change to
 * these registers.
 */
bool  update_is_hsync_safe(struct tegra_dc_win *cur_win,
			   struct tegra_dc_win *new_win)
{
	return ((cur_win->fmt == new_win->fmt) &&
		(cur_win->flags == new_win->flags) &&
		(dfixed_trunc(cur_win->x) == dfixed_trunc(new_win->x)) &&
		(dfixed_trunc(cur_win->y) == dfixed_trunc(new_win->y)) &&
		(dfixed_trunc(cur_win->w) == dfixed_trunc(new_win->w)) &&
		(dfixed_trunc(cur_win->h) == dfixed_trunc(new_win->h)) &&
		(cur_win->out_x == new_win->out_x) &&
		(cur_win->out_y == new_win->out_y) &&
		(cur_win->out_w == new_win->out_w) &&
		(cur_win->out_h == new_win->out_h) &&
		(cur_win->z == new_win->z) &&
		(cur_win->dc == new_win->dc) &&
		(!memcmp(&cur_win->csc, &new_win->csc, sizeof(cur_win->csc)))
		);
}


void tegra_dc_win_partial_update(struct tegra_dc *dc, struct tegra_dc_win *win,
	unsigned int xoff, unsigned int yoff, unsigned int width,
	unsigned int height)
{
	if (!win->out_w || !win->out_h ||
		(win->out_x >= (xoff + width)) ||
		(win->out_y >= (yoff + height)) ||
		(xoff >= (win->out_x + win->out_w)) ||
		(yoff >= (win->out_y + win->out_h))) {
		tegra_dc_writel(dc, 0, DC_WIN_WIN_OPTIONS);
		return;
	} else {
		u64 tmp_64;
		fixed20_12 fixed_tmp;
		unsigned int xoff_2;
		unsigned int yoff_2;
		unsigned int width_2;
		unsigned int height_2;

		xoff_2 = (win->out_x < xoff) ? xoff : win->out_x;
		yoff_2 = (win->out_y < yoff) ? yoff : win->out_y;
		width_2 = ((win->out_x + win->out_w) > (xoff + width)) ?
			(xoff + width - xoff_2) :
			(win->out_x + win->out_w - xoff_2);
		height_2 = ((win->out_y + win->out_h) > (yoff + height)) ?
			(yoff + height - yoff_2) :
			(win->out_y + win->out_h - yoff_2);

		tmp_64 = (u64)(xoff_2 - win->out_x) * win->w.full;
		do_div(tmp_64, win->out_w);
		fixed_tmp.full = (u32)tmp_64;
		win->x.full += dfixed_floor(fixed_tmp);

		tmp_64 = (u64)(yoff_2 - win->out_y) * win->h.full;
		do_div(tmp_64, win->out_h);
		fixed_tmp.full = (u32)tmp_64;
		win->y.full += dfixed_floor(fixed_tmp);

		tmp_64 = (u64)(width_2) * win->w.full;
		do_div(tmp_64, win->out_w);
		fixed_tmp.full = (u32)tmp_64;
		win->w.full = dfixed_floor(fixed_tmp);

		tmp_64 = (u64)(height_2) * win->h.full;
		do_div(tmp_64, win->out_h);
		fixed_tmp.full = (u32)tmp_64;
		win->h.full = dfixed_floor(fixed_tmp);

		/* Move the partial region to the up-left corner
		 * so dc can only scan out this region. */
		win->out_x = xoff_2 - xoff;
		win->out_y = yoff_2 - yoff;
		win->out_w = width_2;
		win->out_h = height_2;

		/* Update shadow registers */
		memcpy(&dc->shadow_windows[win->idx], win,
			sizeof(struct tegra_dc_win));
	}
}

static void tegra_dc_vrr_flip_time(struct tegra_dc *dc)
{
	struct timespec time_now;
	struct tegra_vrr *vrr  = dc->out->vrr;

	if (!vrr || !vrr->capability)
		return;

	if (vrr->enable) {
		vrr->lastenable = 1;
		getnstimeofday(&time_now);
		vrr->curr_flip_us = (s64)time_now.tv_sec * 1000000 +
				time_now.tv_nsec / 1000;
		vrr->flip = 1;
	} else {
		vrr->curr_flip_us = 0;
		vrr->last_flip_us = 0;
	}
}

static void tegra_dc_vrr_cancel_vfp(struct tegra_dc *dc)
{
	struct tegra_vrr *vrr  = dc->out->vrr;

	if (!vrr || !vrr->capability)
		return;

	if (vrr->enable) {
		if (dc->out->type == TEGRA_DC_OUT_DSI)
			tegra_dc_set_act_vfp(dc, vrr->vfp_shrink);
		else
			tegra_dc_set_act_vfp(dc, dc->mode.v_front_porch);
	} else {
		if (dc->out->type == TEGRA_DC_OUT_DSI) {
			if (vrr->lastenable && vrr->dcb <= vrr->db_tolerance) {
				tegra_dc_set_act_vfp(dc,
						dc->mode.v_front_porch);
				vrr->lastenable = 0;
				vrr->frame_type = 0;
				vrr->last_frame_us = 0;
				vrr->flip_interval_us = 0;
				vrr->frame_count = 0;
				vrr->flip_count = 0;
				vrr->vfp_shrink = vrr->v_front_porch_min;
				vrr->vfp_extend = vrr->v_front_porch_max;
			}
		} else
			tegra_dc_set_act_vfp(dc, dc->mode.v_front_porch);
	}
}


#if !defined(CONFIG_TEGRA_NVDISPLAY)
/* Program registers for each window. struct tegra_dc_win --> Assembly registers
 */
static int _tegra_dc_program_windows(struct tegra_dc *dc,
	struct tegra_dc_win *windows[], int n, u16 *dirty_rect,
	bool wait_for_vblank)
{
	unsigned long update_mask = GENERAL_ACT_REQ;
	unsigned long act_control = 0;
	unsigned long win_options;
	bool update_blend_par = false;
	bool update_blend_seq = false;
	int i;
	bool do_partial_update = false;
	unsigned int xoff;
	unsigned int yoff;
	unsigned int width;
	unsigned int height;

	if (dirty_rect) {
		xoff = dirty_rect[0];
		yoff = dirty_rect[1];
		width = dirty_rect[2];
		height = dirty_rect[3];
		do_partial_update = !dc->out_ops->partial_update(dc,
			&xoff, &yoff, &width, &height);

		if (do_partial_update) {
			tegra_dc_writel(dc, width | (height << 16),
				DC_DISP_DISP_ACTIVE);

			dc->disp_active_dirty = true;
		}
	}

	/* If any of the window updates requires vsync to program the window
	   update safely, vsync all windows in this flip.  Safety overrides both
	   the requested wait_for_vblank, and also the no_vsync global. */
	for (i = 0; i < n; i++) {
		struct tegra_dc_win *win = windows[i];

		if ((!wait_for_vblank &&
		    !update_is_hsync_safe(&dc->shadow_windows[win->idx],
					  win)) || do_partial_update)
			wait_for_vblank = 1;

		memcpy(&dc->shadow_windows[win->idx], win,
		       sizeof(struct tegra_dc_win));
	}

	for (i = 0; i < n; i++) {
		struct tegra_dc_win *win = windows[i];
		struct tegra_dc_win *dc_win = tegra_dc_get_window(dc, win->idx);
		bool scan_column = 0;
		fixed20_12 h_offset, v_offset;
		bool invert_h = (win->flags & TEGRA_WIN_FLAG_INVERT_H) != 0;
		bool invert_v = (win->flags & TEGRA_WIN_FLAG_INVERT_V) != 0;
		bool yuv = tegra_dc_is_yuv(win->fmt);
		bool yuvp = tegra_dc_is_yuv_planar(win->fmt);
		bool yuvsp = tegra_dc_is_yuv_semi_planar(win->fmt);
		unsigned Bpp = tegra_dc_fmt_bpp(win->fmt) / 8;
		/* Bytes per pixel of bandwidth, used for dda_inc calculation */
		unsigned Bpp_bw = Bpp * ((yuvp || yuvsp) ? 2 : 1);
		bool filter_h;
		bool filter_v;
#if defined(CONFIG_TEGRA_DC_SCAN_COLUMN)
		scan_column = (win->flags & TEGRA_WIN_FLAG_SCAN_COLUMN);
#endif

		tegra_dc_writel(dc, WINDOW_A_SELECT << win->idx,
				DC_CMD_DISPLAY_WINDOW_HEADER);

		if (!no_vsync)
			update_mask |= WIN_A_ACT_REQ << win->idx;

		if (!WIN_IS_ENABLED(win)) {
		#define RGB_TO_YUV420_8BPC_BLACK_PIX 0x00801010
		#define RGB_TO_YUV422_10BPC_BLACK_PIX 0x00001080

			dc_win->dirty = no_vsync ? 0 : 1;
			tegra_dc_writel(dc, 0, DC_WIN_WIN_OPTIONS);
			if (dc->yuv_bypass) {
				if (dc->mode.vmode &
					(FB_VMODE_Y420 | FB_VMODE_Y420_ONLY |
					 FB_VMODE_Y24))
					tegra_dc_writel(dc,
						RGB_TO_YUV420_8BPC_BLACK_PIX,
						DC_DISP_BLEND_BACKGROUND_COLOR);
				else if (dc->mode.vmode &
					(FB_VMODE_Y422 | FB_VMODE_Y30))
					tegra_dc_writel(dc,
						RGB_TO_YUV422_10BPC_BLACK_PIX,
						DC_DISP_BLEND_BACKGROUND_COLOR);
			}
			continue;

		#undef RGB_TO_YUV422_10BPC_BLACK_PIX
		#undef RGB_TO_YUV420_8BPC_BLACK_PIX
		}

		filter_h = win_use_h_filter(dc, win);
		filter_v = win_use_v_filter(dc, win);

		/* Update blender */
		if ((win->z != dc->blend.z[win->idx]) ||
				((win->flags & TEGRA_WIN_BLEND_FLAGS_MASK) !=
						dc->blend.flags[win->idx])) {
			dc->blend.z[win->idx] = win->z;
			dc->blend.flags[win->idx] =
					win->flags & TEGRA_WIN_BLEND_FLAGS_MASK;
			if (tegra_dc_feature_is_gen2_blender(dc, win->idx))
				update_blend_seq = true;
			else
				update_blend_par = true;
		}

		tegra_dc_writel(dc, WINDOW_A_SELECT << win->idx,
				DC_CMD_DISPLAY_WINDOW_HEADER);

		update_mask |= WIN_A_ACT_REQ << win->idx;

		if (wait_for_vblank)
			act_control &= ~WIN_ACT_CNTR_SEL_HCOUNTER(win->idx);
		else
			act_control |= WIN_ACT_CNTR_SEL_HCOUNTER(win->idx);

#if defined(CONFIG_TEGRA_DC_CDE)
		if (win->cde.cde_addr) {
			tegra_dc_writel(dc, ENABLESURFACE0,
				DC_WINBUF_CDE_CONTROL);
			tegra_dc_writel(dc, tegra_dc_reg_l32(win->cde.cde_addr),
				DC_WINBUF_CDE_COMPTAG_BASE_0);
			tegra_dc_writel(dc, tegra_dc_reg_h32(win->cde.cde_addr),
				DC_WINBUF_CDE_COMPTAG_BASEHI_0);

			tegra_dc_writel(dc, win->cde.zbc_color,
				DC_WINBUF_CDE_ZBC_COLOR_0);

			tegra_dc_writel(dc, win->cde.offset_x |
				((u32)win->cde.offset_y << 16),
				DC_WINBUF_CDE_SURFACE_OFFSET_0);
			tegra_dc_writel(dc, win->cde.ctb_entry,
				DC_WINBUF_CDE_CTB_ENTRY_0);
#if defined(CONFIG_ARCH_TEGRA_210_SOC)
			if (tegra_get_chip_id() == TEGRA210
#else
			if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA21
#endif
				&& ((tegra_revision == TEGRA_REVISION_A01) ||
					(tegra_revision == TEGRA_REVISION_A01q)))
				tegra_dc_writel(dc, 0, DC_WINBUF_CDE_CG_SW_OVR);
		} else {
			tegra_dc_writel(dc, 0, DC_WINBUF_CDE_CONTROL);
			tegra_dc_writel(dc, 0x00000001,
				DC_WINBUF_CDE_CG_SW_OVR);
		}
#endif
		tegra_dc_writel(dc, tegra_dc_fmt(win->fmt),
			DC_WIN_COLOR_DEPTH);
		tegra_dc_writel(dc, tegra_dc_fmt_byteorder(win->fmt),
			DC_WIN_BYTE_SWAP);


		if (do_partial_update)
			tegra_dc_win_partial_update(dc, win, xoff, yoff,
				width, height);

		tegra_dc_writel(dc,
			V_POSITION(win->out_y) | H_POSITION(win->out_x),
			DC_WIN_POSITION);

		if (tegra_dc_feature_has_interlace(dc, win->idx) &&
			(dc->mode.vmode == FB_VMODE_INTERLACED)) {
				tegra_dc_writel(dc,
					V_SIZE((win->out_h) >> 1) |
					H_SIZE(win->out_w),
					DC_WIN_SIZE);
		} else {
			tegra_dc_writel(dc,
				V_SIZE(win->out_h) | H_SIZE(win->out_w),
				DC_WIN_SIZE);
		}

		win_options = WIN_ENABLE;
		if (scan_column)
			win_options |= WIN_SCAN_COLUMN;

		win_options |= H_FILTER_ENABLE(filter_h);
		win_options |= V_FILTER_ENABLE(filter_v);

		/* Update scaling registers if window supports scaling. */
		if (likely(tegra_dc_feature_has_scaling(dc, win->idx)))
			tegra_dc_update_scaling(dc, win, Bpp, Bpp_bw,
								scan_column);

#if defined(CONFIG_ARCH_TEGRA_2x_SOC) || defined(CONFIG_ARCH_TEGRA_3x_SOC)
		tegra_dc_writel(dc, 0, DC_WIN_BUF_STRIDE);
		tegra_dc_writel(dc, 0, DC_WIN_UV_BUF_STRIDE);
#endif

#if defined(CONFIG_ARCH_TEGRA_2x_SOC) || defined(CONFIG_ARCH_TEGRA_3x_SOC) || \
	defined(CONFIG_ARCH_TEGRA_11x_SOC) || defined(CONFIG_ARCH_TEGRA_14x_SOC)
		tegra_dc_writel(dc, win->phys_addr, DC_WINBUF_START_ADDR);
#else
		tegra_dc_writel(dc, tegra_dc_reg_l32(win->phys_addr),
			DC_WINBUF_START_ADDR);
		tegra_dc_writel(dc, tegra_dc_reg_h32(win->phys_addr),
			DC_WINBUF_START_ADDR_HI);
#endif
		if (!yuvp && !yuvsp) {
			tegra_dc_writel(dc, win->stride, DC_WIN_LINE_STRIDE);
		} else if (yuvp) {
#if defined(CONFIG_ARCH_TEGRA_2x_SOC) || defined(CONFIG_ARCH_TEGRA_3x_SOC) || \
	defined(CONFIG_ARCH_TEGRA_11x_SOC) || defined(CONFIG_ARCH_TEGRA_14x_SOC)
			tegra_dc_writel(dc, win->phys_addr_u,
				DC_WINBUF_START_ADDR_U);
			tegra_dc_writel(dc, win->phys_addr_v,
				DC_WINBUF_START_ADDR_V);
#else
			tegra_dc_writel(dc, tegra_dc_reg_l32(win->phys_addr_u),
				DC_WINBUF_START_ADDR_U);
			tegra_dc_writel(dc, tegra_dc_reg_h32(win->phys_addr_u),
				DC_WINBUF_START_ADDR_HI_U);
			tegra_dc_writel(dc, tegra_dc_reg_l32(win->phys_addr_v),
				DC_WINBUF_START_ADDR_V);
			tegra_dc_writel(dc, tegra_dc_reg_h32(win->phys_addr_v),
				DC_WINBUF_START_ADDR_HI_V);
#endif
			tegra_dc_writel(dc,
				LINE_STRIDE(win->stride) |
				UV_LINE_STRIDE(win->stride_uv),
				DC_WIN_LINE_STRIDE);
		} else {
#if defined(CONFIG_ARCH_TEGRA_2x_SOC) || defined(CONFIG_ARCH_TEGRA_3x_SOC) || \
	defined(CONFIG_ARCH_TEGRA_11x_SOC) || defined(CONFIG_ARCH_TEGRA_14x_SOC)
			tegra_dc_writel(dc, win->phys_addr_u,
					DC_WINBUF_START_ADDR_U);
#else
			tegra_dc_writel(dc, tegra_dc_reg_l32(win->phys_addr_u),
				DC_WINBUF_START_ADDR_U);
			tegra_dc_writel(dc, tegra_dc_reg_h32(win->phys_addr_u),
				DC_WINBUF_START_ADDR_HI_U);
#endif
			tegra_dc_writel(dc,
					LINE_STRIDE(win->stride) |
					UV_LINE_STRIDE(win->stride_uv),
					DC_WIN_LINE_STRIDE);
		}

		if (invert_h) {
			h_offset.full = win->x.full + win->w.full;
			h_offset.full = dfixed_floor(h_offset) * Bpp;
			h_offset.full -= dfixed_const(1);
		} else {
			h_offset.full = dfixed_floor(win->x) * Bpp;
		}

		v_offset = win->y;
		if (invert_v)
			v_offset.full += win->h.full - dfixed_const(1);

		tegra_dc_writel(dc, dfixed_trunc(h_offset),
				DC_WINBUF_ADDR_H_OFFSET);
		tegra_dc_writel(dc, dfixed_trunc(v_offset),
				DC_WINBUF_ADDR_V_OFFSET);

#if defined(CONFIG_TEGRA_DC_INTERLACE)
	if ((dc->mode.vmode == FB_VMODE_INTERLACED) && WIN_IS_FB(win)) {
		if (!WIN_IS_INTERLACE(win))
			win->phys_addr2 = win->phys_addr;
	}

	if (tegra_dc_feature_has_interlace(dc, win->idx) &&
		(dc->mode.vmode == FB_VMODE_INTERLACED)) {

#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
			tegra_dc_writel(dc, tegra_dc_reg_l32
				(win->phys_addr2),
				DC_WINBUF_START_ADDR_FIELD2);
			tegra_dc_writel(dc, tegra_dc_reg_h32
				(win->phys_addr2),
				DC_WINBUF_START_ADDR_FIELD2_HI);
#else
			tegra_dc_writel(dc,
				win->phys_addr2,
				DC_WINBUF_START_ADDR_FIELD2);
#endif
		if (yuvp) {
#if defined(CONFIG_ARCH_TEGRA_2x_SOC) || defined(CONFIG_ARCH_TEGRA_3x_SOC) || \
	defined(CONFIG_ARCH_TEGRA_11x_SOC) || defined(CONFIG_ARCH_TEGRA_14x_SOC)
			tegra_dc_writel(dc, win->phys_addr_u2,
				DC_WINBUF_START_ADDR_FIELD2_U);

			tegra_dc_writel(dc, win->phys_addr_v2,
				DC_WINBUF_START_ADDR_FIELD2_V);
#else
			tegra_dc_writel(dc, tegra_dc_reg_l32(win->phys_addr_u2),
				DC_WINBUF_START_ADDR_FIELD2_U);
			tegra_dc_writel(dc, tegra_dc_reg_h32(win->phys_addr_u2),
				DC_WINBUF_START_ADDR_FIELD2_HI_U);

			tegra_dc_writel(dc, tegra_dc_reg_l32(win->phys_addr_v2),
				DC_WINBUF_START_ADDR_FIELD2_V);
			tegra_dc_writel(dc, tegra_dc_reg_h32(win->phys_addr_v2),
				DC_WINBUF_START_ADDR_FIELD2_HI_V);
#endif
		} else if (yuvsp) {
#if defined(CONFIG_ARCH_TEGRA_2x_SOC) || defined(CONFIG_ARCH_TEGRA_3x_SOC) || \
	defined(CONFIG_ARCH_TEGRA_11x_SOC) || defined(CONFIG_ARCH_TEGRA_14x_SOC)
			tegra_dc_writel(dc, win->phys_addr_u2,
				DC_WINBUF_START_ADDR_FIELD2_U);
#else
			tegra_dc_writel(dc, tegra_dc_reg_l32(win->phys_addr_u2),
				DC_WINBUF_START_ADDR_FIELD2_U);
			tegra_dc_writel(dc, tegra_dc_reg_h32(win->phys_addr_u2),
				DC_WINBUF_START_ADDR_FIELD2_HI_U);
#endif
		}
		tegra_dc_writel(dc, dfixed_trunc(h_offset),
			DC_WINBUF_ADDR_H_OFFSET_FIELD2);

		if (WIN_IS_INTERLACE(win)) {
			tegra_dc_writel(dc, dfixed_trunc(v_offset),
					DC_WINBUF_ADDR_V_OFFSET_FIELD2);
		} else {
			v_offset.full += dfixed_const(1);
			tegra_dc_writel(dc, dfixed_trunc(v_offset),
					DC_WINBUF_ADDR_V_OFFSET_FIELD2);
		}
	}
#endif

		if (tegra_dc_feature_has_tiling(dc, win->idx)) {
			if (WIN_IS_TILED(win))
				tegra_dc_writel(dc,
					DC_WIN_BUFFER_ADDR_MODE_TILE |
					DC_WIN_BUFFER_ADDR_MODE_TILE_UV,
					DC_WIN_BUFFER_ADDR_MODE);
			else
				tegra_dc_writel(dc,
					DC_WIN_BUFFER_ADDR_MODE_LINEAR |
					DC_WIN_BUFFER_ADDR_MODE_LINEAR_UV,
					DC_WIN_BUFFER_ADDR_MODE);
		}

#if defined(CONFIG_TEGRA_DC_BLOCK_LINEAR)
		if (tegra_dc_feature_has_blocklinear(dc, win->idx) ||
			tegra_dc_feature_has_tiling(dc, win->idx)) {
				if (WIN_IS_BLOCKLINEAR(win)) {
					tegra_dc_writel(dc,
						DC_WIN_BUFFER_SURFACE_BL_16B2 |
						(win->block_height_log2
							<< BLOCK_HEIGHT_SHIFT),
						DC_WIN_BUFFER_SURFACE_KIND);
				} else if (WIN_IS_TILED(win)) {
					tegra_dc_writel(dc,
						DC_WIN_BUFFER_SURFACE_TILED,
						DC_WIN_BUFFER_SURFACE_KIND);
				} else {
					tegra_dc_writel(dc,
						DC_WIN_BUFFER_SURFACE_PITCH,
						DC_WIN_BUFFER_SURFACE_KIND);
				}
		}

#endif
		if (yuv)
			win_options |= CSC_ENABLE;
		else if (tegra_dc_fmt_bpp(win->fmt) < 24)
			win_options |= COLOR_EXPAND;

		/*
		 * For gen2 blending, change in the global alpha needs rewrite
		 * to blending regs.
		 */
		if ((dc->blend.alpha[win->idx] != win->global_alpha) &&
		    (tegra_dc_feature_is_gen2_blender(dc, win->idx)))
			update_blend_seq = true;

		/* Cache the global alpha of each window here. It is necessary
		 * for in-order blending settings. */
		dc->blend.alpha[win->idx] = win->global_alpha;
		if (!tegra_dc_feature_is_gen2_blender(dc, win->idx)) {
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
			/* Update global alpha if blender is gen1. */
			if (win->global_alpha == 255) {
				tegra_dc_writel(dc, 0, DC_WIN_GLOBAL_ALPHA);
			} else {
				tegra_dc_writel(dc, GLOBAL_ALPHA_ENABLE |
					win->global_alpha, DC_WIN_GLOBAL_ALPHA);
				win_options |= CP_ENABLE;
			}
#endif

#if !defined(CONFIG_TEGRA_DC_BLENDER_GEN2)
			if (win->flags &
					TEGRA_WIN_FLAG_BLEND_COVERAGE) {
				tegra_dc_writel(dc,
					BLEND(NOKEY, ALPHA, 0xff, 0xff),
					DC_WIN_BLEND_1WIN);
			} else {
				/* global_alpha is 255 if not in use */
				tegra_dc_writel(dc,
					BLEND(NOKEY, FIX, win->global_alpha,
						win->global_alpha),
					DC_WIN_BLEND_1WIN);
			}
#endif
		}

		if (win->ppflags & TEGRA_WIN_PPFLAG_CP_ENABLE)
			win_options |= CP_ENABLE;

		win_options |= H_DIRECTION_DECREMENT(invert_h);
		win_options |= V_DIRECTION_DECREMENT(invert_v);
#if defined(CONFIG_TEGRA_DC_INTERLACE)
		if (tegra_dc_feature_has_interlace(dc, win->idx)) {
			if (dc->mode.vmode == FB_VMODE_INTERLACED)
				win_options |= INTERLACE_ENABLE;
		}
#endif
		if (dc_win->csc_dirty) {
			tegra_dc_set_csc(dc, &dc_win->csc);
			dc_win->csc_dirty = false;
		}

		if (dc->yuv_bypass)
			win_options &= ~CP_ENABLE;

		tegra_dc_writel(dc, win_options, DC_WIN_WIN_OPTIONS);

		dc_win->dirty = 1;

		trace_window_update(dc, win);
	}

	if (update_blend_par || update_blend_seq) {
		if (update_blend_par)
			tegra_dc_blend_parallel(dc, &dc->blend);
		if (update_blend_seq)
			tegra_dc_blend_sequential(dc, &dc->blend);

		for_each_set_bit(i, &dc->valid_windows, DC_N_WINDOWS) {
			struct tegra_dc_win *win = tegra_dc_get_window(dc, i);

			win->dirty = 1;
			update_mask |= WIN_A_ACT_REQ << i;
		}
	}

	tegra_dc_set_dynamic_emc(dc);

#if defined(CONFIG_ARCH_TEGRA_11x_SOC) || defined(CONFIG_ARCH_TEGRA_12x_SOC) \
	|| defined(CONFIG_ARCH_TEGRA_14x_SOC) \
	|| defined(CONFIG_ARCH_TEGRA_21x_SOC)
	/* prevent FIFO from taking in stale data after a reset */
	tegra_dc_writel(dc, MEMFETCH_RESET, DC_WINBUF_MEMFETCH_CONTROL);
#endif

	/* WIN_x_UPDATE is the same as WIN_x_ACT_REQ << 8 */
	tegra_dc_writel(dc, update_mask << 8, DC_CMD_STATE_CONTROL);

	if (tegra_cpu_is_asim())
		tegra_dc_writel(dc, FRAME_END_INT | V_BLANK_INT,
						DC_CMD_INT_STATUS);

	tegra_dc_writel(dc, act_control, DC_CMD_REG_ACT_CONTROL);

	if (wait_for_vblank) {
		/* Use the interrupt handler.  ISR will clear the dirty flags
		   when the flip is completed */
		set_bit(V_BLANK_FLIP, &dc->vblank_ref_count);
		tegra_dc_unmask_interrupt(dc,
			FRAME_END_INT | V_BLANK_INT | ALL_UF_INT());
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && !defined(CONFIG_ARCH_TEGRA_3x_SOC)
		set_bit(V_PULSE2_FLIP, &dc->vpulse2_ref_count);
		tegra_dc_unmask_interrupt(dc, V_PULSE2_INT);
#endif
	}

	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE) {
		schedule_delayed_work(&dc->one_shot_work,
				msecs_to_jiffies(dc->one_shot_delay_ms));
	}
	dc->crc_pending = true;

	/* update EMC clock if calculated bandwidth has changed */
	tegra_dc_program_bandwidth(dc, false);

	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE)
		update_mask |= NC_HOST_TRIG;

	tegra_dc_writel(dc, update_mask, DC_CMD_STATE_CONTROL);

	if (!wait_for_vblank) {
		/* Don't use a interrupt handler for the update, but leave
		   vblank interrupts unmasked since they could be used by other
		   windows.  One window could flip on HBLANK while others flip
		   on VBLANK.  Poll HW until this window update is completed
		   which could block for as long as it takes to display one
		   scanline. */

		unsigned int winmask = update_mask & WIN_ALL_ACT_REQ;

		while (tegra_dc_windows_are_dirty(dc, winmask))
			udelay(1);

		for_each_set_bit(i, &dc->valid_windows, n)
			tegra_dc_get_window(dc, windows[i]->idx)->dirty = 0;
	}
	return 0;
}
#endif

/* Does not support updating windows on multiple dcs in one call.
 * Requires a matching sync_windows to avoid leaking ref-count on clocks.
 */
int tegra_dc_update_windows(struct tegra_dc_win *windows[], int n,
	u16 *dirty_rect, bool wait_for_vblank)
{
	struct tegra_dc *dc;
	int i;
	int e = 0;

	dc = windows[0]->dc;
	trace_update_windows(dc);

	if (dc == NULL)
		return -EINVAL;

	/* check that window arguments are valid */
	for (i = 0; i < n; i++) {
		struct tegra_dc_win *win = windows[i];
		struct tegra_dc_win *dc_win =
			win ? tegra_dc_get_window(dc, win->idx) : NULL;
		if (WARN_ONCE(!dc_win, "ignoring invalid windows in request"))
			return -EINVAL;
	}

	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE) {
		/* Acquire one_shot_lock to avoid race condition between
		 * cancellation of old delayed work and schedule of new
		 * delayed work. */
		mutex_lock(&dc->one_shot_lock);
		cancel_delayed_work_sync(&dc->one_shot_work);
		tegra_dc_get(dc);
	}
	mutex_lock(&dc->lock);

	if (!dc->enabled) {
		e = -EFAULT;
		goto done;
	}

	tegra_dc_io_start(dc);
	if (dc->out_ops && dc->out_ops->hold)
		dc->out_ops->hold(dc);

	if (no_vsync)
		wait_for_vblank = 0;

	BUG_ON(!wait_for_vblank && dirty_rect);

#if defined(CONFIG_TEGRA_NVDISPLAY)
	e = tegra_nvdisp_update_windows(dc, windows, n, dirty_rect,
		wait_for_vblank);
#else
	e = _tegra_dc_program_windows(dc, windows, n, dirty_rect,
		wait_for_vblank);
#endif
	/*
	 * tegra_dc_put() called at frame end, for one shot.
	 * out_ops->release called in tegra_dc_sync_windows.
	 */
	tegra_dc_io_end(dc);

	BUG_ON(e);
	if (WARN_ONCE(e, "horrible failure")) /* horrible failure */
		goto done;

	if (dc->out->type == TEGRA_DC_OUT_DSI)
		tegra_dc_vrr_flip_time(dc);

	tegra_dc_vrr_cancel_vfp(dc);
done:
	mutex_unlock(&dc->lock);
	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE)
		mutex_unlock(&dc->one_shot_lock);

	return 0;
}
EXPORT_SYMBOL(tegra_dc_update_windows);

void tegra_dc_trigger_windows(struct tegra_dc *dc)
{
	u32 val, i;
	u32 completed = 0;
	u32 dirty = 0;
	bool interlace_done = true;

#if defined(CONFIG_TEGRA_DC_INTERLACE)
	if (dc->mode.vmode == FB_VMODE_INTERLACED) {
		val = tegra_dc_readl(dc, DC_DISP_INTERLACE_CONTROL);
		interlace_done = (val & INTERLACE_MODE_ENABLE) &&
			(val & INTERLACE_STATUS_FIELD_2);
	}
#endif

	val = tegra_dc_readl(dc, DC_CMD_STATE_CONTROL);
	for_each_set_bit(i, &dc->valid_windows, DC_N_WINDOWS) {
		struct tegra_dc_win *win = tegra_dc_get_window(dc, i);

		if (tegra_platform_is_linsim()) {
			/* FIXME: this is not needed when
			   the simulator clears WIN_x_UPDATE
			   bits as in HW */
			if (interlace_done) {
				win->dirty = 0;
				completed = 1;
			}
		} else {
			if (!(val & (WIN_A_ACT_REQ << i)) && interlace_done) {
				win->dirty = 0;
				completed = 1;
			} else {
				dirty = 1;
			}
		}
	}

	if (!dirty) {
		if (!(dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE)
			&& !atomic_read(&dc->frame_end_ref))
			tegra_dc_mask_interrupt(dc, FRAME_END_INT);
	}

	if (completed)
		wake_up(&dc->wq);
}

/* Dynamically attach window idx to head dc */
int tegra_dc_attach_win(struct tegra_dc *dc, unsigned idx)
{
#if defined(CONFIG_TEGRA_NVDISPLAY)
	return tegra_nvdisp_assign_win(dc, idx);
#else
	if (dc)
		dev_warn(&dc->ndev->dev, "%s: unsupported\n", __func__);
	return -EINVAL;
#endif
}
EXPORT_SYMBOL(tegra_dc_attach_win);

/* Dynamically dettach window idx from head dc */
int tegra_dc_dettach_win(struct tegra_dc *dc, unsigned idx)
{
#if defined(CONFIG_TEGRA_NVDISPLAY)
	return tegra_nvdisp_detach_win(dc, idx);
#else
	if (dc)
		dev_warn(&dc->ndev->dev, "%s: unsupported\n", __func__);
	return -EINVAL;
#endif
}
EXPORT_SYMBOL(tegra_dc_dettach_win);
