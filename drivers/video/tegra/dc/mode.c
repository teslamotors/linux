/*
 * drivers/video/tegra/dc/mode.c
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
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/clk/tegra.h>

#include <drm/drm_mode.h>

#include <mach/dc.h>
#include <trace/events/display.h>

#include <linux/platform/tegra/mc.h>

#include "dc_reg.h"
#include "dc_priv.h"
#include "dsi.h"

/* return non-zero if constraint is violated */
static int calc_h_ref_to_sync(const struct tegra_dc_mode *mode, int *href)
{
	long a, b;

	/* Constraint 5: H_REF_TO_SYNC >= 0 */
	a = 0;

	/* Constraint 6: H_FRONT_PORT >= (H_REF_TO_SYNC + 1) */
	b = mode->h_front_porch - 1;

	/* Constraint 1: H_REF_TO_SYNC + H_SYNC_WIDTH + H_BACK_PORCH > 11 */
	if (a + mode->h_sync_width + mode->h_back_porch <= 11)
		a = 1 + 11 - mode->h_sync_width - mode->h_back_porch;
	/* check Constraint 1 and 6 */
	if (a > b)
		return 1;

	/* Constraint 4: H_SYNC_WIDTH >= 1 */
	if (mode->h_sync_width < 1)
		return 4;

	/* Constraint 7: H_DISP_ACTIVE >= 16 */
	if (mode->h_active < 16)
		return 7;

	if (href) {
		if (b > a && a % 2)
			*href = a + 1; /* use smallest even value */
		else
			*href = a; /* even or only possible value */
	}

	return 0;
}

static int calc_v_ref_to_sync(const struct tegra_dc_mode *mode, int *vref)
{
	long a;

	/* Constraint 5: V_REF_TO_SYNC >= 1 */
	a = mode->v_front_porch - 1;

	/* Constraint 2: V_REF_TO_SYNC + V_SYNC_WIDTH + V_BACK_PORCH > 1 */
	if (a + mode->v_sync_width + mode->v_back_porch <= 1)
		a = 1 + 1 - mode->v_sync_width - mode->v_back_porch;

	/* Constraint 6 */
	if (mode->v_front_porch < a + 1)
		a = mode->v_front_porch - 1;

	/* Constraint 4: V_SYNC_WIDTH >= 1 */
	if (mode->v_sync_width < 1)
		return 4;

	/* Constraint 7: V_DISP_ACTIVE >= 16 */
	if (mode->v_active < 16)
		return 7;

	if (vref)
		*vref = a;
	return 0;
}

static int calc_ref_to_sync(struct tegra_dc_mode *mode)
{
	int ret;
	ret = calc_h_ref_to_sync(mode, &mode->h_ref_to_sync);
	if (ret)
		return ret;
	ret = calc_v_ref_to_sync(mode, &mode->v_ref_to_sync);
	if (ret)
		return ret;

	return 0;
}

static bool check_ref_to_sync(struct tegra_dc_mode *mode, bool verbose)
{
#define CHK(c, s) do {				\
		if (unlikely(c)) {		\
			if (verbose)		\
				pr_err(s);	\
			return false;		\
		}				\
	} while (0)

	/* Constraint 1: H_REF_TO_SYNC + H_SYNC_WIDTH + H_BACK_PORCH > 11. */
	CHK(mode->h_ref_to_sync + mode->h_sync_width +
	    mode->h_back_porch <= 11,
	    "H_REF_TO_SYNC + H_SYNC_WIDTH + H_BACK_PORCH <= 11\n");

	/* Constraint 2: V_REF_TO_SYNC + V_SYNC_WIDTH + V_BACK_PORCH > 1. */
	CHK(mode->v_ref_to_sync + mode->v_sync_width + mode->v_back_porch <= 1,
	    "V_REF_TO_SYNC + V_SYNC_WIDTH + V_BACK_PORCH <= 1\n");

	/* Constraint 3: V_FRONT_PORCH + V_SYNC_WIDTH + V_BACK_PORCH > 1
	 * (vertical blank). */
	CHK(mode->v_front_porch + mode->v_sync_width + mode->v_back_porch <= 1,
	    "V_FRONT_PORCH + V_SYNC_WIDTH + V_BACK_PORCH <= 1\n");


	/* Constraint 4: V_SYNC_WIDTH >= 1; H_SYNC_WIDTH >= 1. */
	CHK(mode->v_sync_width < 1 || mode->h_sync_width < 1,
	    "V_SYNC_WIDTH >= 1; H_SYNC_WIDTH < 1\n");

	/* Constraint 5: V_REF_TO_SYNC >= 1; H_REF_TO_SYNC >= 0. */
	CHK(mode->v_ref_to_sync < 1 || mode->h_ref_to_sync < 0,
	    "V_REF_TO_SYNC >= 1; H_REF_TO_SYNC < 0\n");

	/* Constraint 6: V_FRONT_PORCH >= (V_REF_TO_SYNC + 1) */
	CHK(mode->v_front_porch < mode->v_ref_to_sync + 1,
	    "V_FRONT_PORCH < (V_REF_TO_SYNC + 1)\n");

	/* Constraint 7: H_FRONT_PORCH >= (H_REF_TO_SYNC + 1) */
	CHK(mode->h_front_porch < mode->h_ref_to_sync + 1,
	    "H_FRONT_PORCH < (H_REF_TO_SYNC + 1)\n");

	/* Constraint 8: H_DISP_ACTIVE >= 16 */
	CHK(mode->h_active < 16, "H_DISP_ACTIVE < 16\n");

	/* Constraint 9: V_DISP_ACTIVE >= 16 */
	CHK(mode->v_active < 16, "V_DISP_ACTIVE < 16\n");

#undef CHK

	return true;
}

static s64 calc_frametime_ns(const struct tegra_dc_mode *m)
{
	long h_total, v_total;
	h_total = m->h_active + m->h_front_porch + m->h_back_porch +
		m->h_sync_width;
	v_total = m->v_active + m->v_front_porch + m->v_back_porch +
		m->v_sync_width;
	return (!m->pclk) ? 0 : (s64)(div_s64(((s64)h_total * v_total *
					1000000000ULL), m->pclk));
}

/*
 * return in 1000ths of a Hertz
 * TODO: Extend to handle other refresh rates and pclk
 */
static inline int _tegra_dc_calc_refresh(long pclk, long h_total, long v_total)
{
	long refresh;

	if (!pclk || !h_total || !v_total || pclk < h_total)
		return 0;

	refresh = pclk / h_total;
	refresh *= 1000;
	refresh /= v_total;

	return refresh;
}

/* return in 1000ths of a Hertz */
int tegra_dc_calc_refresh(const struct tegra_dc_mode *m)
{
	long h_total, v_total;
	long pclk;

	if (m->rated_pclk > 0)
		pclk = m->rated_pclk;
	else
		pclk = m->pclk;

	h_total = m->h_active + m->h_front_porch + m->h_back_porch +
		m->h_sync_width;
	v_total = m->v_active + m->v_front_porch + m->v_back_porch +
		m->v_sync_width;

	return _tegra_dc_calc_refresh(pclk, h_total, v_total);
}

/* return in 1000ths of a Hertz */
int tegra_dc_calc_fb_refresh(const struct fb_videomode *fbmode)
{
	long h_total, v_total;

	h_total = fbmode->xres + fbmode->right_margin +
		fbmode->left_margin + fbmode->hsync_len;
	v_total = fbmode->yres + fbmode->upper_margin +
		fbmode->lower_margin + fbmode->vsync_len;

	return _tegra_dc_calc_refresh(PICOS2KHZ(fbmode->pixclock) * 1000,
					h_total, v_total);
}

static u8 _calc_default_avi_m(unsigned h_size, unsigned v_size)
{
#define is_avi_m( \
	h_size, v_size, \
	h_avi_m, v_avi_m) \
	(((h_size) * (v_avi_m)) > ((v_size) * ((h_avi_m) - 1)) &&  \
	((h_size) * (v_avi_m)) < ((v_size) * ((h_avi_m) + 1))) \

	if (!h_size || !v_size)
		pr_warn("invalid h_size %u or v_size %u\n", h_size, v_size);

	if (is_avi_m(h_size, v_size, 256, 135))
		return TEGRA_DC_MODE_AVI_M_256_135;
	else if (is_avi_m(h_size, v_size, 64, 27))
		return TEGRA_DC_MODE_AVI_M_64_27;
	else if (is_avi_m(h_size, v_size, 16, 9))
		return TEGRA_DC_MODE_AVI_M_16_9;
	else if (is_avi_m(h_size, v_size, 4, 3))
		return TEGRA_DC_MODE_AVI_M_4_3;

	return TEGRA_DC_MODE_AVI_M_NO_DATA;

#undef is_avi_m
}

static u8 calc_default_avi_m(struct tegra_dc *dc)
{
#define EDID_AVI_M_256_135 91
#define EDID_AVI_M_64_27 138
#define EDID_AVI_M_16_9 79
#define EDID_AVI_M_4_3 34

	if (dc->out) {
		unsigned h_size = tegra_dc_get_out_width(dc);
		unsigned v_size = tegra_dc_get_out_height(dc);

		/* extract picture aspect ratio from real screen sizes */
		if (h_size && v_size)
			return _calc_default_avi_m(h_size, v_size);

		/* assign edid data */
		h_size = dc->out->h_size;
		v_size = dc->out->v_size;

		if (!h_size && !v_size)
			return TEGRA_DC_MODE_AVI_M_NO_DATA;

		/* edid has picture aspect ratio stored */
		if (!h_size || !v_size) {
			unsigned temp = h_size ? : v_size;

			switch (temp) {
			case EDID_AVI_M_256_135:
				return TEGRA_DC_MODE_AVI_M_256_135;
			case EDID_AVI_M_64_27:
				return TEGRA_DC_MODE_AVI_M_64_27;
			case EDID_AVI_M_16_9:
				return TEGRA_DC_MODE_AVI_M_16_9;
			case EDID_AVI_M_4_3:
				return TEGRA_DC_MODE_AVI_M_4_3;
			default:
				/* unsupported picture aspect ratio */
				return TEGRA_DC_MODE_AVI_M_NO_DATA;
			};
		}
	}

	return 0;

#undef EDID_AVI_M_4_3
#undef EDID_AVI_M_16_9
#undef EDID_AVI_M_64_27
#undef EDID_AVI_M_256_135
}

u32 tegra_dc_get_aspect_ratio(struct tegra_dc *dc)
{
	u32 aspect_ratio = 0;

	if (!dc)
		return 0;

	switch (calc_default_avi_m(dc)) {
	case TEGRA_DC_MODE_AVI_M_256_135:
		aspect_ratio = FB_FLAG_RATIO_256_135;
		break;
	case TEGRA_DC_MODE_AVI_M_64_27:
		aspect_ratio = FB_FLAG_RATIO_64_27;
		break;
	case TEGRA_DC_MODE_AVI_M_16_9:
		aspect_ratio = FB_FLAG_RATIO_16_9;
		break;
	case TEGRA_DC_MODE_AVI_M_4_3:
		aspect_ratio = FB_FLAG_RATIO_4_3;
		break;
	default:
		aspect_ratio = 0;
	};

	return aspect_ratio;
}

static bool check_mode_timings(const struct tegra_dc *dc,
			       struct tegra_dc_mode *mode, bool verbose)
{
#if defined(CONFIG_TEGRA_HDMI2_0)
	if ((mode->vmode & FB_VMODE_Y420) ||
	    (mode->vmode & FB_VMODE_Y420_ONLY)) {
		mode->v_ref_to_sync = 1;
	} else {
		calc_ref_to_sync(mode);
	}
	mode->h_ref_to_sync = 1;
#else
	if (dc->out->type == TEGRA_DC_OUT_HDMI) {
			/* HDMI controller requires h_ref=1, v_ref=1 */
		mode->h_ref_to_sync = 1;
		mode->v_ref_to_sync = 1;
	} else {
		calc_ref_to_sync(mode);
	}
#endif

	if (dc->out->type == TEGRA_DC_OUT_DSI && dc->out->vrr) {
		mode->h_ref_to_sync =
			dc->out->modes[dc->out->n_modes-1].h_ref_to_sync;
		mode->v_ref_to_sync =
			dc->out->modes[dc->out->n_modes-1].v_ref_to_sync;
	}

	if (dc->out->type == TEGRA_DC_OUT_DP) {
		mode->h_ref_to_sync = 1;
		mode->v_ref_to_sync = 1;
	}

	if (!check_ref_to_sync(mode, verbose)) {
		if (verbose)
			dev_err(&dc->ndev->dev,
				"Display timing doesn't meet restrictions.\n");
		return false;
	}
	if (verbose)
		dev_dbg(&dc->ndev->dev,
			"Using mode %dx%d pclk=%d href=%d vref=%d\n",
			mode->h_active, mode->v_active, mode->pclk,
			mode->h_ref_to_sync, mode->v_ref_to_sync);

	return true;
}

bool check_fb_videomode_timings(const struct tegra_dc *dc,
				const struct fb_videomode *fbmode)
{
	struct tegra_dc_mode mode;

	if (!dc || !fbmode)
		return false;

	/* Only copy the relevant info */
	mode.h_front_porch = fbmode->right_margin;
	mode.v_front_porch = fbmode->lower_margin;
	mode.h_sync_width = fbmode->hsync_len;
	mode.v_sync_width = fbmode->vsync_len;
	mode.h_back_porch = fbmode->left_margin;
	mode.v_back_porch = fbmode->upper_margin;
	mode.h_active = fbmode->xres;
	mode.v_active = fbmode->yres;
	mode.vmode = fbmode->vmode;

	return check_mode_timings(dc, &mode, false);
}

#ifdef DEBUG
static void print_mode(struct tegra_dc *dc,
			const struct tegra_dc_mode *mode, const char *note)
{
	if (mode) {
		int refresh = tegra_dc_calc_refresh(mode);
		dev_info(&dc->ndev->dev, "%s():MODE:%dx%d@%d.%03uHz pclk=%d\n",
			note ? note : "",
			mode->h_active, mode->v_active,
			refresh / 1000, refresh % 1000,
			mode->pclk);
	}
}
#else /* !DEBUG */
static inline void print_mode(struct tegra_dc *dc,
			const struct tegra_dc_mode *mode, const char *note) { }
#endif /* DEBUG */

int tegra_dc_program_mode(struct tegra_dc *dc, struct tegra_dc_mode *mode)
{
	unsigned long val;
	unsigned long rate;
	unsigned long div;
	unsigned long pclk;

	unsigned long v_back_porch;
	unsigned long v_front_porch;
	unsigned long v_sync_width;
	unsigned long v_active;

	tegra_dc_get(dc);

	if (dc->out_ops && dc->out_ops->modeset_notifier)
		dc->out_ops->modeset_notifier(dc);

	v_back_porch = mode->v_back_porch;
	v_front_porch = mode->v_front_porch;
	v_sync_width = mode->v_sync_width;
	v_active = mode->v_active;

	if (mode->vmode & FB_VMODE_INTERLACED) {
		v_back_porch /= 2;
		v_front_porch /= 2;
		v_sync_width /= 2;
		v_active /= 2;
	}

	print_mode(dc, mode, __func__);

	/* use default EMC rate when switching modes */
#ifdef CONFIG_TEGRA_ISOMGR
	dc->new_bw_kbps = tegra_dc_calc_min_bandwidth(dc);
#else
	dc->new_bw_kbps = tegra_emc_freq_req_to_bw(
		tegra_dc_get_default_emc_clk_rate(dc) / 1000);
#endif
	tegra_dc_program_bandwidth(dc, true);

	tegra_dc_writel(dc, 0x0, DC_DISP_DISP_TIMING_OPTIONS);
	tegra_dc_writel(dc, mode->h_ref_to_sync | (mode->v_ref_to_sync << 16),
			DC_DISP_REF_TO_SYNC);
	tegra_dc_writel(dc, mode->h_sync_width | (v_sync_width << 16),
			DC_DISP_SYNC_WIDTH);
	if ((dc->out->type == TEGRA_DC_OUT_DP) ||
		(dc->out->type == TEGRA_DC_OUT_FAKE_DP) ||
		(dc->out->type == TEGRA_DC_OUT_NVSR_DP) ||
		(dc->out->type == TEGRA_DC_OUT_NULL) ||
		(dc->out->type == TEGRA_DC_OUT_LVDS)) {
		tegra_dc_writel(dc, mode->h_back_porch |
			((v_back_porch - mode->v_ref_to_sync) << 16),
			DC_DISP_BACK_PORCH);
		tegra_dc_writel(dc, mode->h_front_porch |
			((v_front_porch + mode->v_ref_to_sync) << 16),
			DC_DISP_FRONT_PORCH);
	} else {
		tegra_dc_writel(dc, mode->h_back_porch |
			(v_back_porch << 16),
			DC_DISP_BACK_PORCH);
		tegra_dc_writel(dc, mode->h_front_porch |
			(v_front_porch << 16),
			DC_DISP_FRONT_PORCH);
	}
	tegra_dc_writel(dc, mode->h_active | (v_active << 16),
			DC_DISP_DISP_ACTIVE);

#if defined(CONFIG_TEGRA_DC_INTERLACE)
	if (mode->vmode == FB_VMODE_INTERLACED)
		tegra_dc_writel(dc, INTERLACE_MODE_ENABLE |
			INTERLACE_START_FIELD_1
			| INTERLACE_STATUS_FIELD_1,
			DC_DISP_INTERLACE_CONTROL);
	else
		tegra_dc_writel(dc, INTERLACE_MODE_DISABLE,
			DC_DISP_INTERLACE_CONTROL);

	if (mode->vmode == FB_VMODE_INTERLACED) {
		tegra_dc_writel(dc, (mode->h_ref_to_sync |
			((mode->h_sync_width + mode->h_back_porch +
			mode->h_active + mode->h_front_porch) >> 1)
			<< 16), DC_DISP_INTERLACE_FIELD2_REF_TO_SYNC);
		tegra_dc_writel(dc, mode->h_sync_width |
			(v_sync_width << 16),
			DC_DISP_INTERLACE_FIELD2_SYNC_WIDTH);
		tegra_dc_writel(dc, mode->h_back_porch |
			((v_back_porch + 1) << 16),
			DC_DISP_INTERLACE_FIELD2_BACK_PORCH);
		tegra_dc_writel(dc, mode->h_active |
			(v_active << 16),
			DC_DISP_INTERLACE_FIELD2_DISP_ACTIVE);
		tegra_dc_writel(dc, mode->h_front_porch |
			(v_front_porch << 16),
			DC_DISP_INTERLACE_FIELD2_FRONT_PORCH);
	}
#endif

	tegra_dc_writel(dc, DE_SELECT_ACTIVE | DE_CONTROL_NORMAL,
			DC_DISP_DATA_ENABLE_OPTIONS);

	/* TODO: MIPI/CRT/HDMI clock cals */
	val = 0;
	if (!(dc->out->type == TEGRA_DC_OUT_DSI ||
		dc->out->type == TEGRA_DC_OUT_FAKE_DSIA ||
		dc->out->type == TEGRA_DC_OUT_FAKE_DSIB ||
		dc->out->type == TEGRA_DC_OUT_FAKE_DSI_GANGED ||
		dc->out->type == TEGRA_DC_OUT_HDMI)) {
		val = DISP_DATA_FORMAT_DF1P1C;

		if (dc->out->align == TEGRA_DC_ALIGN_MSB)
			val |= DISP_DATA_ALIGNMENT_MSB;
		else
			val |= DISP_DATA_ALIGNMENT_LSB;

		if (dc->out->order == TEGRA_DC_ORDER_RED_BLUE)
			val |= DISP_DATA_ORDER_RED_BLUE;
		else
			val |= DISP_DATA_ORDER_BLUE_RED;
	}
	tegra_dc_writel(dc, val, DC_DISP_DISP_INTERFACE_CONTROL);

	rate = tegra_dc_clk_get_rate(dc);
	pclk = tegra_dc_pclk_round_rate(dc, mode->pclk);
	div = (rate * 2 / pclk) - 2;
	dev_info(&dc->ndev->dev,
		"nominal-pclk:%d parent:%lu div:%lu.%lu pclk:%lu %d~%d\n",
		mode->pclk, rate, (div + 2) / 2, ((div + 2) % 2) * 5, pclk,
		mode->pclk / 100 * 99, mode->pclk / 100 * 109);

	/* skip pclk range check for TEGRA_DC_OUT_NULL */
	if (dc->out->type != TEGRA_DC_OUT_NULL) {
		if (!pclk || pclk < (mode->pclk / 100 * 99) ||
			pclk > (mode->pclk / 100 * 109)) {
			dev_err(&dc->ndev->dev, "pclk out of range!\n");
			return -EINVAL;
		}
	}

	/* SW WAR for bug 1045373. To make the shift clk dividor effect under
	 * all circumstances, write N+2 to SHIFT_CLK_DIVIDER and activate it.
	 * After 2us delay, write the target values to it. */
#if defined(CONFIG_ARCH_TEGRA_14x_SOC)
	tegra_dc_writel(dc, PIXEL_CLK_DIVIDER_PCD1 | SHIFT_CLK_DIVIDER(div + 2),
			DC_DISP_DISP_CLOCK_CONTROL);
	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);

	udelay(2);
#endif

#if defined(CONFIG_ARCH_TEGRA_2x_SOC) || defined(CONFIG_ARCH_TEGRA_3x_SOC)
	/* Deprecated on t11x and t14x. */
	tegra_dc_writel(dc, 0x00010001,
			DC_DISP_SHIFT_CLOCK_OPTIONS);
#endif

	tegra_dc_writel(dc, PIXEL_CLK_DIVIDER_PCD1 | SHIFT_CLK_DIVIDER(div),
			DC_DISP_DISP_CLOCK_CONTROL);

#ifdef CONFIG_SWITCH
	if (dc->switchdev_registered)
		switch_set_state(&dc->modeset_switch,
			 (mode->h_active << 16) | mode->v_active);
#endif

	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);

	tegra_dc_put(dc);

	dc->mode_dirty = false;

	trace_display_mode(dc, &dc->mode);
	return 0;
}

static int panel_sync_rate;

int tegra_dc_get_panel_sync_rate(void)
{
	return panel_sync_rate;
}
EXPORT_SYMBOL(tegra_dc_get_panel_sync_rate);

int _tegra_dc_set_mode(struct tegra_dc *dc,
				const struct tegra_dc_mode *mode)
{
	struct tegra_dc_mode new_mode = *mode;
	int yuv_flag = new_mode.vmode & FB_VMODE_YUV_MASK;

	if (yuv_flag == (FB_VMODE_Y420      | FB_VMODE_Y24) ||
	    yuv_flag == (FB_VMODE_Y420_ONLY | FB_VMODE_Y24)) {
		new_mode.h_back_porch /= 2;
		new_mode.h_front_porch /= 2;
		new_mode.h_sync_width /= 2;
		new_mode.h_active /= 2;
		new_mode.pclk /= 2;
	} else if (yuv_flag == (FB_VMODE_Y420 | FB_VMODE_Y30)) {
		new_mode.h_back_porch = (new_mode.h_back_porch * 5) / 8;
		new_mode.h_front_porch = (new_mode.h_front_porch * 5) / 8;
		new_mode.h_sync_width = (new_mode.h_sync_width * 5) / 8;
		new_mode.h_active = (new_mode.h_active * 5) / 8;
		new_mode.pclk = (new_mode.pclk / 8) * 5;
	}

	if (memcmp(&dc->mode, &new_mode, sizeof(dc->mode)) == 0) {
		/* mode is unchanged, just return */
		return 0;
	}

	memcpy(&dc->mode, &new_mode, sizeof(dc->mode));
	dc->mode_dirty = true;

	if (dc->out->type == TEGRA_DC_OUT_RGB)
		panel_sync_rate = tegra_dc_calc_refresh(&new_mode);
	else if (dc->out->type == TEGRA_DC_OUT_DSI)
		panel_sync_rate = dc->out->dsi->rated_refresh_rate * 1000;

	if (dc->out->type == TEGRA_DC_OUT_FAKE_DSIA ||
		dc->out->type == TEGRA_DC_OUT_FAKE_DSIB ||
		dc->out->type == TEGRA_DC_OUT_FAKE_DSI_GANGED) {
		tegra_dsi_init_clock_param(dc);
	}

	print_mode(dc, &new_mode, __func__);
	dc->frametime_ns = calc_frametime_ns(&new_mode);

	return 0;
}

int tegra_dc_set_mode(struct tegra_dc *dc, const struct tegra_dc_mode *mode)
{
	mutex_lock(&dc->lock);
	_tegra_dc_set_mode(dc, mode);
	mutex_unlock(&dc->lock);

	return 0;
}
EXPORT_SYMBOL(tegra_dc_set_mode);

int tegra_dc_to_fb_videomode(struct fb_videomode *fbmode,
	const struct tegra_dc_mode *mode)
{
	long mode_pclk;

	if (!fbmode || !mode || !mode->pclk) {
		if (fbmode)
			memset(fbmode, 0, sizeof(*fbmode));
		return -EINVAL;
	}
	if (mode->rated_pclk >= 1000) /* handle DSI one-shot modes */
		mode_pclk = mode->rated_pclk;
	else if (mode->pclk >= 1000) /* normal continous modes */
		mode_pclk = mode->pclk;
	else
		mode_pclk = 0;
	memset(fbmode, 0, sizeof(*fbmode));
	fbmode->right_margin = mode->h_front_porch;
	fbmode->lower_margin = mode->v_front_porch;
	fbmode->hsync_len = mode->h_sync_width;
	fbmode->vsync_len = mode->v_sync_width;
	fbmode->left_margin = mode->h_back_porch;
	fbmode->upper_margin = mode->v_back_porch;
	fbmode->xres = mode->h_active;
	fbmode->yres = mode->v_active;
	fbmode->vmode = mode->vmode;
	if (mode->stereo_mode) {
#ifndef CONFIG_TEGRA_HDMI_74MHZ_LIMIT
		/* Double the pixel clock and update v_active only for
		 * frame packed mode */
		mode_pclk /= 2;
		/* total v_active = yres*2 + activespace */
		fbmode->yres = (mode->v_active - mode->v_sync_width -
			mode->v_back_porch - mode->v_front_porch) / 2;
		fbmode->vmode |= FB_VMODE_STEREO_FRAME_PACK;
#else
		fbmode->vmode |= FB_VMODE_STEREO_LEFT_RIGHT;
#endif
	}

	if (!(mode->flags & TEGRA_DC_MODE_FLAG_NEG_H_SYNC))
		fbmode->sync |=  FB_SYNC_HOR_HIGH_ACT;
	if (!(mode->flags & TEGRA_DC_MODE_FLAG_NEG_V_SYNC))
		fbmode->sync |= FB_SYNC_VERT_HIGH_ACT;

	if (mode->avi_m == TEGRA_DC_MODE_AVI_M_256_135)
		fbmode->flag |= FB_FLAG_RATIO_256_135;
	else if (mode->avi_m == TEGRA_DC_MODE_AVI_M_64_27)
		fbmode->flag |= FB_FLAG_RATIO_64_27;
	else if (mode->avi_m == TEGRA_DC_MODE_AVI_M_16_9)
		fbmode->flag |= FB_FLAG_RATIO_16_9;
	else if (mode->avi_m == TEGRA_DC_MODE_AVI_M_4_3)
		fbmode->flag |= FB_FLAG_RATIO_4_3;

	if (mode_pclk >= 1000) /* else 0 */
		fbmode->pixclock = KHZ2PICOS(mode_pclk / 1000);
	fbmode->refresh = tegra_dc_calc_refresh(mode) / 1000;

	return 0;
}

int tegra_dc_update_mode(struct tegra_dc *dc)
{
	if (dc->mode_dirty)
#ifdef CONFIG_TEGRA_NVDISPLAY
		return tegra_nvdisp_program_mode(dc, &dc->mode);
#else
		return tegra_dc_program_mode(dc, &dc->mode);
#endif
	return 0;
}

int tegra_dc_set_drm_mode(struct tegra_dc *dc,
		const struct drm_mode_modeinfo *dmode, bool stereo_mode)
{
	struct tegra_dc_mode mode;

	if (!dmode->clock)
		return -EINVAL;

	memset(&mode, 0, sizeof(mode));
	mode.pclk = dmode->clock * 1000;
	mode.h_sync_width = dmode->hsync_end - dmode->hsync_start;
	mode.v_sync_width = dmode->vsync_end - dmode->vsync_start;
	mode.h_back_porch = dmode->htotal - dmode->hsync_end;
	mode.v_back_porch = dmode->vtotal - dmode->vsync_end;
	mode.h_active = dmode->hdisplay;
	mode.v_active = dmode->vdisplay;
	mode.h_front_porch = dmode->hsync_start - dmode->hdisplay;
	mode.v_front_porch = dmode->vsync_start - dmode->vdisplay;
	mode.stereo_mode = stereo_mode;
	if (dmode->flags & DRM_MODE_FLAG_INTERLACE)
		mode.vmode |= FB_VMODE_INTERLACED;
	mode.avi_m = calc_default_avi_m(dc);

	if (!check_mode_timings(dc, &mode, true))
		return -EINVAL;

#ifndef CONFIG_TEGRA_HDMI_74MHZ_LIMIT
	/* Double the pixel clock and update v_active only for
	 * frame packed mode */
	if (mode.stereo_mode) {
		mode.pclk *= 2;
		/* total v_active = yres*2 + activespace */
		mode.v_active = dmode->vtotal + dmode->vdisplay;
	}
#endif

	mode.flags = 0;

	if (!(dmode->flags & DRM_MODE_FLAG_PHSYNC))
		mode.flags |= TEGRA_DC_MODE_FLAG_NEG_H_SYNC;

	if (!(dmode->flags & DRM_MODE_FLAG_PVSYNC))
		mode.flags |= TEGRA_DC_MODE_FLAG_NEG_V_SYNC;

	return tegra_dc_set_mode(dc, &mode);
}
EXPORT_SYMBOL(tegra_dc_set_drm_mode);

int tegra_dc_set_fb_mode(struct tegra_dc *dc,
		const struct fb_videomode *fbmode, bool stereo_mode)
{
	struct tegra_dc_mode mode;

	if (!fbmode->pixclock)
		return -EINVAL;

	memset(&mode, 0, sizeof(mode));
	mode.pclk = PICOS2KHZ(fbmode->pixclock) * 1000;

	mode.h_sync_width = fbmode->hsync_len;
	mode.v_sync_width = fbmode->vsync_len;
	mode.h_back_porch = fbmode->left_margin;
	mode.v_back_porch = fbmode->upper_margin;
	mode.h_active = fbmode->xres;
	mode.v_active = fbmode->yres;
	mode.h_front_porch = fbmode->right_margin;
	mode.v_front_porch = fbmode->lower_margin;
	mode.stereo_mode = stereo_mode;
	mode.vmode = fbmode->vmode;

	if (fbmode->flag & FB_FLAG_RATIO_256_135)
		mode.avi_m = TEGRA_DC_MODE_AVI_M_256_135;
	else if (fbmode->flag & FB_FLAG_RATIO_64_27)
		mode.avi_m = TEGRA_DC_MODE_AVI_M_64_27;
	else if (fbmode->flag & FB_FLAG_RATIO_16_9)
		mode.avi_m = TEGRA_DC_MODE_AVI_M_16_9;
	else if (fbmode->flag & FB_FLAG_RATIO_4_3)
		mode.avi_m = TEGRA_DC_MODE_AVI_M_4_3;
	else
		mode.avi_m = calc_default_avi_m(dc);

	if (!check_mode_timings(dc, &mode, true))
		return -EINVAL;

#ifndef CONFIG_TEGRA_HDMI_74MHZ_LIMIT
	/* Double the pixel clock and update v_active only for
	 * frame packed mode */
	if (mode.stereo_mode) {
		mode.pclk *= 2;
		/* total v_active = yres*2 + activespace */
		mode.v_active = fbmode->yres * 2 +
				fbmode->vsync_len +
				fbmode->upper_margin +
				fbmode->lower_margin;
	}
#endif

	mode.flags = 0;

	if (!(fbmode->sync & FB_SYNC_HOR_HIGH_ACT))
		mode.flags |= TEGRA_DC_MODE_FLAG_NEG_H_SYNC;

	if (!(fbmode->sync & FB_SYNC_VERT_HIGH_ACT))
		mode.flags |= TEGRA_DC_MODE_FLAG_NEG_V_SYNC;

	return _tegra_dc_set_mode(dc, &mode);
}
EXPORT_SYMBOL(tegra_dc_set_fb_mode);
