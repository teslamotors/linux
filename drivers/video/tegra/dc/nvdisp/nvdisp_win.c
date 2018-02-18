/*
 * drivers/video/tegra/dc/nvdisplay/nvdis_win.c
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


#include <linux/delay.h>
#include <mach/dc.h>

#include "nvdisp.h"
#include "nvdisp_priv.h"
#include "hw_nvdisp_nvdisp.h"

#include "dc_config.h"

#define NVDISP_ODD_VAL(x) ((x) % (2))

/* Num Fractional Bits in 8.24 fixed point phase and phase increment values */
#define NFB 24

/*
 * Generated from gen_vic_filter.py and taking only
 * the data needed for 3 scaling ratios - 1x,2x,4x for 5 taps.
 * 192 entries are needed to generate the 1x,2x and 4x coeffs
 * For supporting the normal scaler will use only the last
 * 10 bits
 */
static unsigned int vic_filter_coeffs[192] = {
	0x00000000,    0x3c70e400,    0x3bb037e4,    0x0c51cc9c,
	0x00100001,    0x3bf0dbfa,    0x3d00f406,    0x3fe003ff,
	0x00300002,    0x3b80cbf5,    0x3da1040d,    0x3fb003fe,
	0x00400002,    0x3b20bff1,    0x3e511015,    0x3f9003fc,
	0x00500002,    0x3ad0b3ed,    0x3f21201d,    0x3f5003fb,
	0x00500003,    0x3aa0a3e9,    0x3ff13026,    0x3f2007f9,
	0x00500403,    0x3a7097e6,    0x00e1402f,    0x3ee007f7,
	0x00500403,    0x3a608be4,    0x01d14c38,    0x3ea00bf6,
	0x00500403,    0x3a507fe2,    0x02e15c42,    0x3e500ff4,
	0x00500402,    0x3a6073e1,    0x03f16c4d,    0x3e000ff2,
	0x00400402,    0x3a706be0,    0x05117858,    0x3db013f0,
	0x00300402,    0x3a905fe0,    0x06318863,    0x3d6017ee,
	0x00300402,    0x3ab057e0,    0x0771986e,    0x3d001beb,
	0x00200001,    0x3af04fe1,    0x08a1a47a,    0x3cb023e9,
	0x00100001,    0x3b2047e2,    0x09e1b485,    0x3c6027e7,
	0x00100000,    0x3b703fe2,    0x0b11c091,    0x3c002fe6,
	0x3f203800,    0x0391103f,    0x3ff0a014,    0x0811606c,
	0x3f2037ff,    0x0351083c,    0x03e11842,    0x3f203c00,
	0x3f302fff,    0x03010439,    0x04311c45,    0x3f104401,
	0x3f302fff,    0x02c0fc35,    0x04812448,    0x3f104802,
	0x3f4027ff,    0x0270f832,    0x04c1284b,    0x3f205003,
	0x3f4023ff,    0x0230f030,    0x0511304e,    0x3f205403,
	0x3f601fff,    0x01f0e82d,    0x05613451,    0x3f205c04,
	0x3f701bfe,    0x01b0e02a,    0x05a13c54,    0x3f306006,
	0x3f7017fe,    0x0170d827,    0x05f14057,    0x3f406807,
	0x3f8017ff,    0x0140d424,    0x0641445a,    0x3f406c08,
	0x3fa013ff,    0x0100cc22,    0x0681485d,    0x3f507409,
	0x3fa00fff,    0x00d0c41f,    0x06d14c60,    0x3f607c0b,
	0x3fc00fff,    0x0090bc1c,    0x07115063,    0x3f80840c,
	0x3fd00bff,    0x0070b41a,    0x07515465,    0x3f908c0e,
	0x3fe007ff,    0x0040b018,    0x07915868,    0x3fb0900f,
	0x3ff00400,    0x0010a816,    0x07d15c6a,    0x3fd09811,
	0x00a04c0e,    0x0460f442,    0x0240a827,    0x05c15859,
	0x0090440d,    0x0440f040,    0x0480fc43,    0x00b05010,
	0x0080400c,    0x0410ec3e,    0x04910044,    0x00d05411,
	0x0070380b,    0x03f0e83d,    0x04b10846,    0x00e05812,
	0x0060340a,    0x03d0e43b,    0x04d10c48,    0x00f06013,
	0x00503009,    0x03b0e039,    0x04e11449,    0x01106415,
	0x00402c08,    0x0390d838,    0x05011c4b,    0x01206c16,
	0x00302807,    0x0370d436,    0x0511204c,    0x01407018,
	0x00302406,    0x0340d034,    0x0531244e,    0x01507419,
	0x00202005,    0x0320cc32,    0x05412c50,    0x01707c1b,
	0x00101c04,    0x0300c431,    0x05613451,    0x0180801d,
	0x00101803,    0x02e0c02f,    0x05713853,    0x01a0881e,
	0x00101002,    0x02b0bc2d,    0x05814054,    0x01c08c20,
	0x00000c02,    0x02a0b82c,    0x05914455,    0x01e09421,
	0x00000801,    0x0280b02a,    0x05a14c57,    0x02009c23,
	0x00000400,    0x0260ac28,    0x05b15458,    0x0220a025,
};


static int tegra_nvdisp_blend(struct tegra_dc_win *win)
{
	bool update_blend_seq = false;
	int idx = win->idx;
	u32 blend_ctrl = 0;
	struct tegra_dc *dc = win->dc;
	struct tegra_dc_blend *blend = &dc->blend;

	/* Update blender */
	if ((win->z != blend->z[idx]) ||
			((win->flags & TEGRA_WIN_BLEND_FLAGS_MASK) !=
					blend->flags[idx])) {
		blend->z[win->idx] = win->z;
		blend->flags[win->idx] =
			win->flags & TEGRA_WIN_BLEND_FLAGS_MASK;
		if (tegra_dc_feature_is_gen2_blender(dc, idx))
			update_blend_seq = true;
	}

	/*
	* For gen2 blending, change in the global alpha needs rewrite
	* to blending regs.
	*/
	if ((blend->alpha[idx] != win->global_alpha) &&
		(tegra_dc_feature_is_gen2_blender(dc, idx)))
		update_blend_seq = true;

	/* Cache the global alpha of each window here. It is necessary
	 * for in-order blending settings. */
	dc->blend.alpha[win->idx] = win->global_alpha;

#if defined(CONFIG_TEGRA_DC_BLENDER_DEPTH)
	blend_ctrl = win_blend_layer_control_depth_f(blend->z[idx]);
#endif

	if (update_blend_seq) {
		if (blend->flags[idx] & TEGRA_WIN_FLAG_BLEND_COVERAGE) {

			blend_ctrl |=
			(win_blend_layer_control_k2_f(0xff) |
			win_blend_layer_control_k1_f(blend->alpha[idx]) |
			win_blend_layer_control_blend_enable_enable_f());

			nvdisp_win_write(win,
			WIN_BLEND_FACT_SRC_COLOR_MATCH_SEL_K1_TIMES_SRC |
			WIN_BLEND_FACT_DST_COLOR_MATCH_SEL_NEG_K1_TIMES_SRC |
			WIN_BLEND_FACT_SRC_ALPHA_MATCH_SEL_K2 |
			WIN_BLEND_FACT_DST_ALPHA_MATCH_SEL_ZERO,
			win_blend_match_select_r());

			nvdisp_win_write(win,
			WIN_BLEND_FACT_SRC_COLOR_NOMATCH_SEL_K1_TIMES_SRC |
			WIN_BLEND_FACT_DST_COLOR_NOMATCH_SEL_NEG_K1_TIMES_SRC |
			WIN_BLEND_FACT_SRC_ALPHA_NOMATCH_SEL_K2 |
			WIN_BLEND_FACT_DST_ALPHA_NOMATCH_SEL_ZERO,
			win_blend_nomatch_select_r());

		} else if (blend->flags[idx] & TEGRA_WIN_FLAG_BLEND_PREMULT) {

			blend_ctrl |=
			(win_blend_layer_control_k2_f(0xff) |
			win_blend_layer_control_k1_f(blend->alpha[idx]) |
			win_blend_layer_control_blend_enable_enable_f());

			nvdisp_win_write(win,
			WIN_BLEND_FACT_SRC_COLOR_MATCH_SEL_K1 |
			WIN_BLEND_FACT_DST_COLOR_MATCH_SEL_NEG_K1_TIMES_SRC |
			WIN_BLEND_FACT_SRC_ALPHA_MATCH_SEL_K2 |
			WIN_BLEND_FACT_DST_ALPHA_MATCH_SEL_ZERO,
			win_blend_match_select_r());

			nvdisp_win_write(win,
			WIN_BLEND_FACT_SRC_COLOR_NOMATCH_SEL_NEG_K1_TIMES_DST |
			WIN_BLEND_FACT_DST_COLOR_NOMATCH_SEL_K1 |
			WIN_BLEND_FACT_SRC_ALPHA_NOMATCH_SEL_K2 |
			WIN_BLEND_FACT_DST_ALPHA_NOMATCH_SEL_ZERO,
			win_blend_nomatch_select_r());

		} else {
			blend_ctrl |=
			win_blend_layer_control_blend_enable_bypass_f();
		}

		nvdisp_win_write(win, blend_ctrl,
			win_blend_layer_control_r());
	}
	return 0;
}

static void tegra_nvdisp_set_scaler_coeff(struct tegra_dc_win *win)
{
	unsigned int ratio, row, col, index, coeff;
	index = 0;

	for (ratio = 0; ratio <= 2; ratio++) {
		for (row = 0; row <= 15; row++) {
			for (col = 0; col <= 3; col++) {

				index = (ratio << 6) + (row << 2) + (col);

				/* coeff value is bit 9:0 of each entry in
					vic_filer_coeffs array */
				/* index field is from 22:15 */
				coeff = win_scaler_set_coeff_index_f(index) |
					win_scaler_set_coeff_data_f(
						vic_filter_coeffs[index]);
				nvdisp_win_write(win, coeff,
					 win_scaler_set_coeff_r());
			}
		}
	}
}

/* Returns phase_incr in fixed20.12 format */
static inline u32 compute_phase_incr(fixed20_12 in, unsigned out_int)
{
	u32 phase_incr = 0;
	u64 tmp  = (u64) dfixed_trunc(in);
	u64 tmp2 = (u64) out_int;
	u64 tmp1 = (tmp << NFB) + (tmp2 >> 1);
	tmp1 = tmp1 / tmp2;

	phase_incr =  lower_32_bits(tmp1);
	return phase_incr;
}

static int tegra_nvdisp_scaling(struct tegra_dc_win *win)
{
	u32 hbypass, vbypass, hphase_incr, h_init_phase;
	u32 vphase_incr, v_init_phase;
	bool is_scalar_column = win->flags & TEGRA_WIN_FLAG_SCAN_COLUMN;
	fixed20_12 hscalar = win->w;
	fixed20_12 vscalar = win->h;
	u32 min_width, win_capc, win_cape;

	if (is_scalar_column) {
		hscalar = win->h;
		vscalar = win->w;
	}

	hbypass = (dfixed_trunc(hscalar) == win->out_w);
	vbypass = (dfixed_trunc(vscalar) == win->out_h);

	/* Select 2 taps vs 5 taps */
	min_width = (dfixed_trunc(win->w) < win->out_w) ?
		dfixed_trunc(win->w) : win->out_w;

	win_capc = nvdisp_win_read(win, win_precomp_wgrp_capc_r());
	win_cape = nvdisp_win_read(win, win_precomp_wgrp_cape_r());

	if (min_width < win_precomp_wgrp_capc_max_pixels_5tap444_v(win_capc)) {
		nvdisp_win_write(win, win_scaler_input_h_taps_5_f() |
			win_scaler_input_v_taps_5_f(),
			win_scaler_input_r());
	} else if (min_width <
			win_precomp_wgrp_cape_max_pixels_2tap444_v(win_cape)) {
		nvdisp_win_write(win, win_scaler_input_h_taps_2_f() |
			win_scaler_input_v_taps_2_f(),
			win_scaler_input_r());
	} else {
		dev_err(&win->dc->ndev->dev, "Scaler can't be used\n");
		return -EINVAL;
	}

	nvdisp_win_write(win, win_scaler_usage_hbypass_f(hbypass) |
		win_scaler_usage_vbypass_f(vbypass) |
		win_scaler_usage_use422_disable_f(), win_scaler_usage_r());

	/* scaling is needed in horizontal direction  */
	if (!hbypass) {
		/* Convert phase_incr from fixed20.12 to fixed 8.24 */
		hphase_incr = compute_phase_incr(hscalar, win->out_w) & ~0x1;
		dev_dbg(&win->dc->ndev->dev,
			"hphase_incr: 0x%x.\n", hphase_incr);

		h_init_phase = (1 << (NFB - 1)) + (hphase_incr >> 1);
		dev_dbg(&win->dc->ndev->dev,
			"h_init_phase: 0x%x.\n", h_init_phase);

		nvdisp_win_write(win,
			win_scaler_hphase_incr_incr_f(hphase_incr),
			win_scaler_hphase_incr_r());

		nvdisp_win_write(win,
			win_scaler_hstart_phase_phase_f(h_init_phase),
			win_scaler_hstart_phase_r());
	}

	/* scaling is needed in vertical direction */
	if (!vbypass) {
		/* Convert phase_incr from fixed20.12 to fixed8.24 */
		vphase_incr = compute_phase_incr(vscalar, win->out_h)& ~0x1;
		dev_dbg(&win->dc->ndev->dev,
			"vphase_incr: 0x%x.\n", vphase_incr);

		v_init_phase = (1 << (NFB - 1)) + (vphase_incr >> 1);
		dev_dbg(&win->dc->ndev->dev,
			"v_init_phase: 0x%x.\n", v_init_phase);

		nvdisp_win_write(win,
			win_scaler_vphase_incr_incr_f(vphase_incr),
			win_scaler_vphase_incr_r());

		nvdisp_win_write(win,
			win_scaler_vstart_phase_phase_f(v_init_phase),
			win_scaler_vstart_phase_r());
	}

	return 0;
}

static int tegra_nvdisp_enable_cde(struct tegra_dc_win *win)
{
#if defined(CONFIG_TEGRA_DC_CDE)
	if (win->cde.cde_addr) {
		nvdisp_win_write(win, tegra_dc_reg_l32(win->cde.cde_addr),
			win_cde_base_r());
		nvdisp_win_write(win, tegra_dc_reg_h32(win->cde.cde_addr),
			win_cde_base_hi_r());
		nvdisp_win_write(win,
			win_cde_zbc_color_f(win->cde.zbc_color),
			win_cde_zbc_r());
		nvdisp_win_write(win,
			win_cde_ctb_entry_f(win->cde.ctb_entry),
			win_cde_ctb_r());
		nvdisp_win_write(win,
			win_cde_ctrl_surface_enable_f(),
			win_cde_ctrl_r());

	} else
		nvdisp_win_write(win,
			0,
			win_cde_ctrl_r());
#endif

	return 0;
}

static inline u32 tegra_nvdisp_win_swap_uv(struct tegra_dc_win *win)
{

	u32 swap_uv = 0;

	switch (tegra_dc_fmt(win->fmt)) {
	case TEGRA_WIN_FMT_YCbCr420SP:
		swap_uv = 1;
		break;

	case TEGRA_WIN_FMT_YCrCb420SP:
		win->fmt = TEGRA_WIN_FMT_YCbCr420SP;
		break;

	case TEGRA_WIN_FMT_YCrCb422SP:
		swap_uv = 1;
		break;

	case TEGRA_WIN_FMT_YCbCr422SP:
		win->fmt = TEGRA_WIN_FMT_YCrCb422SP;
		break;

	case TEGRA_WIN_FMT_YCrCb422RSP:
		swap_uv = 1;
		break;

	case TEGRA_WIN_FMT_YCbCr422RSP:
		win->fmt = TEGRA_WIN_FMT_YCrCb422RSP;
		break;

	case TEGRA_WIN_FMT_YCrCb444SP:
		swap_uv = 1;
		break;

	case TEGRA_WIN_FMT_YCbCr444SP:
		win->fmt = TEGRA_WIN_FMT_YCrCb444SP;
		break;
	}

	return swap_uv;
}

int tegra_nvdisp_get_degamma_config(struct tegra_dc *dc,
	struct tegra_dc_win *win)
{
	int ret = 0;

	if (!dc->cmu_enabled)
		return win_win_set_params_degamma_range_none_f();

	if (tegra_dc_is_yuv(win->fmt)) {
		/* yuv8_10 for rec601/709/2020-10 and
		 * yuv12 for rec2020-12 yuv inputs
		 */
		if (tegra_dc_is_yuv_12bpc(win->fmt))
			ret |=
				win_win_set_params_degamma_range_yuv12_f();
		else
			ret |=
				win_win_set_params_degamma_range_yuv8_10_f();
	} else {
		/* srgb for rgb, none for I8 */
		if (win->fmt == TEGRA_WIN_FMT_P8)
			ret |=
				win_win_set_params_degamma_range_none_f();
		else
			ret |=
				win_win_set_params_degamma_range_srgb_f();
	}

	return ret;
}

static int tegra_nvdisp_win_attribute(struct tegra_dc_win *win,
				      bool wait_for_vblank)
{
	u32 win_options, win_params, swap_uv;
	fixed20_12 h_offset, v_offset;
	int err = 0;

	bool yuv = tegra_dc_is_yuv(win->fmt);
	bool yuvp = tegra_dc_is_yuv_planar(win->fmt);
	bool yuvsp = tegra_dc_is_yuv_semi_planar(win->fmt);

	struct tegra_dc *dc = win->dc;

	/* Check the cropped size width and height are even for YUV formats
	 * Fail the call if not even
	 */
	if (yuv && (NVDISP_ODD_VAL(dfixed_trunc(win->h)) ||
			NVDISP_ODD_VAL(dfixed_trunc(win->w)))) {
		dev_err(&dc->ndev->dev,
		"Window HEIGHT and WIDTH must be EVEN for YUV formats\n");
		err = -EINVAL;
		goto attr_fail;
	}

	h_offset.full = dfixed_floor(win->x);
	v_offset      = win->y;

	/* For YUV format X & Y must be even */
	if (yuv && (NVDISP_ODD_VAL(dfixed_trunc(h_offset))
		 || NVDISP_ODD_VAL(dfixed_trunc(v_offset)))) {
		dev_err(&dc->ndev->dev,
		"X and Y offsets must be EVEN for YUV formats\n");
		err = -EINVAL;
		goto attr_fail;
	}

	swap_uv = tegra_nvdisp_win_swap_uv(win);
	nvdisp_win_write(win, tegra_dc_fmt(win->fmt), win_color_depth_r());
	nvdisp_win_write(win, win_wgrp_params_swap_uv_f(swap_uv),
		 win_wgrp_params_r());

	nvdisp_win_write(win,
		win_position_v_position_f(win->out_y) |
		win_position_h_position_f(win->out_x),
		win_position_r());

	if (tegra_dc_feature_has_interlace(dc, win->idx) &&
		(dc->mode.vmode == FB_VMODE_INTERLACED)) {
		nvdisp_win_write(win,
			win_size_v_size_f((win->out_h) >> 1) |
			win_size_h_size_f(win->out_w),
			win_size_r());
	} else {
		nvdisp_win_write(win, win_size_v_size_f(win->out_h) |
			win_size_h_size_f(win->out_w),
			win_size_r());
	}

	win_options = win_options_win_enable_enable_f();
	if (win->flags & TEGRA_WIN_FLAG_SCAN_COLUMN)
		win_options |= win_options_scan_column_enable_f();
	if (win->flags & TEGRA_WIN_FLAG_INVERT_H)
		win_options |= win_options_h_direction_decrement_f();
	if (win->flags & TEGRA_WIN_FLAG_INVERT_V)
		win_options |= win_options_v_direction_decrement_f();
	if (tegra_dc_fmt_bpp(win->fmt) < 24)
		win_options |= win_options_color_expand_enable_f();
	if (win->ppflags & TEGRA_WIN_PPFLAG_CP_ENABLE)
		win_options |= win_options_cp_enable_enable_f();

	if (dc->yuv_bypass) {
		win_options &= ~win_options_cp_enable_enable_f();
	}

	if (win_options & win_options_cp_enable_enable_f())
		tegra_dc_set_lut(dc, win);

	nvdisp_win_write(win, win_options, win_options_r());

	nvdisp_win_write(win,
		win_set_cropped_size_in_height_f(dfixed_trunc(win->h)) |
		win_set_cropped_size_in_width_f(dfixed_trunc(win->w)),
		win_set_cropped_size_in_r());

	nvdisp_win_write(win, tegra_dc_reg_l32(win->phys_addr),
		win_start_addr_r());
	nvdisp_win_write(win, tegra_dc_reg_h32(win->phys_addr),
		win_start_addr_hi_r());

	/*	pitch is in 64B chunks	*/
	nvdisp_win_write(win, (win->stride>>6),
		win_set_planar_storage_r());

	if (yuvp) {
		nvdisp_win_write(win, tegra_dc_reg_l32(win->phys_addr_u),
			win_start_addr_u_r());
		nvdisp_win_write(win, tegra_dc_reg_h32(win->phys_addr_u),
			win_start_addr_hi_u_r());
		nvdisp_win_write(win, tegra_dc_reg_l32(win->phys_addr_v),
			win_start_addr_v_r());
		nvdisp_win_write(win, tegra_dc_reg_h32(win->phys_addr_v),
			win_start_addr_hi_v_r());

		nvdisp_win_write(win,
			win_set_planar_storage_uv_uv0_f(win->stride_uv>>6) |
			win_set_planar_storage_uv_uv1_f(win->stride_uv>>6),
			win_set_planar_storage_uv_r());
	} else if (yuvsp) {
		nvdisp_win_write(win, tegra_dc_reg_l32(win->phys_addr_u),
			win_start_addr_u_r());
		nvdisp_win_write(win, tegra_dc_reg_h32(win->phys_addr_u),
			win_start_addr_hi_u_r());

		nvdisp_win_write(win,
			win_set_planar_storage_uv_uv0_f(win->stride_uv>>6),
			win_set_planar_storage_uv_r());
	}

	/* setting full range and clamp before blend as default */
	win_params = win_win_set_params_in_range_full_f() |
		win_win_set_params_clamp_before_blend_enable_f();

	/* setting input-range */
	if (win->flags & TEGRA_WIN_FLAG_INPUT_RANGE_BYPASS)
		win_params = win_win_set_params_in_range_bypass_f();
	else if (win->flags & TEGRA_WIN_FLAG_INPUT_RANGE_LIMITED)
		win_params = win_win_set_params_in_range_limited_f();

	win_params |= tegra_nvdisp_get_degamma_config(dc, win);

	/* color_space settings */
	if (yuv) {
		if (win->flags & TEGRA_WIN_FLAG_CS_REC601)
			win_params |= win_win_set_params_cs_range_yuv_601_f();
		else if (win->flags & TEGRA_WIN_FLAG_CS_REC2020)
			win_params |= win_win_set_params_cs_range_yuv_2020_f();
		else
			win_params |= win_win_set_params_cs_range_yuv_709_f();

	} else {
		win_params |= win_win_set_params_cs_range_rgb_f();
	}

	/* over-ride the win params for yuv bypass */
	if (dc->yuv_bypass) {
		win_params = win_win_set_params_in_range_bypass_f();
		win_params |=
			win_win_set_params_degamma_range_none_f();
	}

	nvdisp_win_write(win, win_params, win_win_set_params_r());

	nvdisp_win_write(win,
			win_cropped_point_h_offset_f(dfixed_trunc(h_offset))|
			win_cropped_point_v_offset_f(dfixed_trunc(v_offset)),
			win_cropped_point_r());

#if defined(CONFIG_TEGRA_DC_INTERLACE)
	if ((dc->mode.vmode == FB_VMODE_INTERLACED) && WIN_IS_FB(win)) {
		if (!WIN_IS_INTERLACE(win))
			win->phys_addr2 = win->phys_addr;
	}

	if (tegra_dc_feature_has_interlace(dc, win->idx) &&
		(dc->mode.vmode == FB_VMODE_INTERLACED)) {
			nvdisp_win_write(win,
				tegra_dc_reg_l32(win->phys_addr2),
				win_start_addr_fld2_r());
			nvdisp_win_write(win,
				tegra_dc_reg_h32(win->phys_addr2),
				win_start_addr_fld2_hi_r());
		if (yuvp) {
			nvdisp_win_write(win,
				tegra_dc_reg_l32(win->phys_addr_u2),
				win_start_addr_fld2_u_r());
			nvdisp_win_write(win,
				tegra_dc_reg_h32(win->phys_addr_u2),
				win_start_addr_fld2_hi_u_r());
			nvdisp_win_write(win,
				tegra_dc_reg_l32(win->phys_addr_v2),
				win_start_addr_fld2_v_r());
			nvdisp_win_write(win,
				tegra_dc_reg_h32(win->phys_addr_v2),
				win_start_addr_fld2_hi_v_r());
		} else if (yuvsp) {
			nvdisp_win_write(win,
				tegra_dc_reg_l32(win->phys_addr_u2),
				win_start_addr_fld2_u_r());
			nvdisp_win_write(win,
				tegra_dc_reg_h32(win->phys_addr_u2),
				win_start_addr_fld2_hi_u_r());
		}
		nvdisp_win_write(win,
			win_cropped_point_fld2_h_f(dfixed_trunc(h_offset)),
			win_cropped_point_fld2_r());

		if (WIN_IS_INTERLACE(win)) {
			nvdisp_win_write(win,
				win_cropped_point_fld2_v_f(
						dfixed_trunc(v_offset)),
				win_cropped_point_fld2_r());
		} else {
			v_offset.full += dfixed_const(1);
			nvdisp_win_write(win,
				win_cropped_point_fld2_v_f(
						dfixed_trunc(v_offset)),
				win_cropped_point_fld2_r());
		}
	}
#endif

	if (WIN_IS_BLOCKLINEAR(win)) {
		nvdisp_win_write(win, win_surface_kind_kind_bl_f() |
			win_surface_kind_block_height_f(win->block_height_log2),
			win_surface_kind_r());
	} else if (WIN_IS_TILED(win)) {
		nvdisp_win_write(win, win_surface_kind_kind_tiled_f(),
			win_surface_kind_r());
	} else {
		nvdisp_win_write(win, win_surface_kind_kind_pitch_f(),
			win_surface_kind_r());
	}

attr_fail:
	return err;
}


int tegra_nvdisp_get_linestride(struct tegra_dc *dc, int win)
{
	return nvdisp_win_read(tegra_dc_get_window(dc, win),
		win_set_planar_storage_r()) << 6;
}


int tegra_nvdisp_update_windows(struct tegra_dc *dc,
	struct tegra_dc_win *windows[], int n,
	u16 *dirty_rect, bool wait_for_vblank)
{
	int i;
	u32 update_mask = nvdisp_cmd_state_ctrl_general_update_enable_f();
	u32 act_req_mask = nvdisp_cmd_state_ctrl_general_act_req_enable_f();
	u32 act_control = 0;

	mutex_lock(&tegra_nvdisp_lock);

	/* If any of the window updates requires vsync to program the window
	   update safely, vsync all windows in this flip.  Safety overrides both
	   the requested wait_for_vblank, and also the no_vsync global.
	   The HSync Flip has some restrictions on changes from previous frame.
	   The update_is_hsync_safe() call is used to filter out flips to force
	   the VSync instead. */
	for (i = 0; i < n; i++) {
		struct tegra_dc_win *win = windows[i];

		if ((!wait_for_vblank
			&& !update_is_hsync_safe(
				&dc->shadow_windows[win->idx], win))
			/*|| do_partial_update*/)
			wait_for_vblank = true;

		memcpy(&dc->shadow_windows[win->idx], win, sizeof(*win));
	}

	/*
	 * If this HEAD has already reserved exclusive use of the COMMON
	 * channel, go ahead ahd program the ihub registers (assembly)
	 * if they haven't been programmed yet.
	 */
	if (dc->common_channel_reserved && dc->new_imp_results_needed) {
		tegra_nvdisp_program_imp_results(dc);

		/*
		 * The ihub registers live on the COMMON channel. Update the
		 * masks accordingly so that the COMMON channel state is
		 * promoted along with the WINDOW channel state.
		 */
		update_mask |=
			nvdisp_cmd_state_ctrl_common_act_update_enable_f();
		act_req_mask |= nvdisp_cmd_state_ctrl_common_act_req_enable_f();

		dc->new_imp_results_needed = false;
		dc->common_channel_pending = true;
	}

	for (i = 0; i < n; i++) {
		struct tegra_dc_win *win = windows[i];
		struct tegra_dc_win *dc_win;

		if (!win) {
			dev_err(&dc->ndev->dev,
				"Invalid window %d to update\n", i);
			mutex_unlock(&tegra_nvdisp_lock);

			return -EINVAL;
		}

		dc_win = tegra_dc_get_window(dc, win->idx);

		if (!dc_win) {
			dev_err(&dc->ndev->dev,
				"Cannot get window %d to update\n", i);
			mutex_unlock(&tegra_nvdisp_lock);

			return -EINVAL;
		}

		if (!WIN_IS_ENABLED(win)) {
			u32 win_options;
			/* Support for YUV bypass */
			#define RGB_TO_YUV420_8BPC_BLACK_PIX 0x00801010
			#define RGB_TO_YUV422_10BPC_BLACK_PIX 0x00001080

			if (dc->yuv_bypass) {
				if (dc->mode.vmode &
					(FB_VMODE_Y420 | FB_VMODE_Y420_ONLY |
					 FB_VMODE_Y24))
					tegra_dc_writel(dc,
					RGB_TO_YUV420_8BPC_BLACK_PIX,
					nvdisp_background_color_r());
				else if (dc->mode.vmode &
					(FB_VMODE_Y422 | FB_VMODE_Y30))
					tegra_dc_writel(dc,
					RGB_TO_YUV422_10BPC_BLACK_PIX,
					nvdisp_background_color_r());
			}

			#undef RGB_TO_YUV422_10BPC_BLACK_PIX
			#undef RGB_TO_YUV420_8BPC_BLACK_PIX

			update_mask |=
				nvdisp_cmd_state_ctrl_win_a_update_enable_f()
				<< win->idx;
			act_req_mask |=
				nvdisp_cmd_state_ctrl_a_act_req_enable_f()
				<< win->idx;

			/* detach the window from the head
			 * this must be written before other window regs */
			nvdisp_win_write(win, win_set_control_owner_none_f(),
					win_set_control_r());

			/* disable scaler (bypass mode) */
			nvdisp_win_write(win, win_scaler_usage_hbypass_f(1) |
				win_scaler_usage_vbypass_f(1) |
				win_scaler_usage_use422_disable_f(),
				win_scaler_usage_r());

			/* disable csc */
			tegra_nvdisp_set_csc(win, &win->csc);
			/*
			 * mark csc_dirty so that next time when window is
			 * enabled, CSC can be programmed.
			 */
			win->csc_dirty = true;

			/* disable cde */
			nvdisp_win_write(win, 0, win_cde_ctrl_r());

			/* disable window */
			win_options = win_options_win_enable_disable_f();
			nvdisp_win_write(win, win_options, win_options_r());

			/* disable HSync Flip */
			wait_for_vblank = true;
			nvdisp_win_write(win,
				win_act_control_ctrl_sel_vcounter_f(),
				win_act_control_r());

			dc_win->dirty = no_vsync ? 0 : 1;
		} else {
			/* attach window to the head */
			nvdisp_win_write(win, dc->ctrl_num,
					win_set_control_r());

			update_mask |= nvdisp_cmd_state_ctrl_win_a_update_enable_f()
				<< win->idx;
			act_req_mask |=  nvdisp_cmd_state_ctrl_a_act_req_enable_f()
				<< win->idx;

			if (wait_for_vblank)
				act_control = win_act_control_ctrl_sel_vcounter_f();
			else
				act_control = win_act_control_ctrl_sel_hcounter_f();

			nvdisp_win_write(win, act_control, win_act_control_r());

			tegra_nvdisp_blend(win);
			tegra_nvdisp_scaling(win);
			tegra_nvdisp_enable_cde(win);

			/* if (do_partial_update) { */
				/* /\* calculate the xoff, yoff etc values *\/ */
				/* tegra_dc_win_partial_update(dc, win, xoff, yoff, */
				/* 	width, height); */
			/* } */

			if (tegra_nvdisp_win_attribute(win, wait_for_vblank)) {
				dev_err(&dc->ndev->dev,
					"win_attribute settings failed\n");
				mutex_unlock(&tegra_nvdisp_lock);

				return -EINVAL;
			}

			/* skip updating csc if cmu is disabled */
			if (dc->cmu_enabled && dc_win->csc_dirty) {
				tegra_nvdisp_set_csc(win, &dc_win->csc);
				dc_win->csc_dirty = false;
			}

			dc_win->dirty = 1;
			win->dirty = 1;
		}

		trace_window_update(dc, win);
	}

	if (tegra_cpu_is_asim())
		tegra_dc_writel(dc,
			(nvdisp_cmd_int_status_frame_end_f(1) |
			nvdisp_cmd_int_status_v_blank_f(1)),
			nvdisp_cmd_int_status_r());

	if (wait_for_vblank) {
		/* Use the interrupt handler.  ISR will clear the dirty flags
		   when the flip is completed */
		set_bit(V_BLANK_FLIP, &dc->vblank_ref_count);
		tegra_dc_unmask_interrupt(dc,
			(nvdisp_cmd_int_status_frame_end_f(1) |
			nvdisp_cmd_int_status_v_blank_f(1) |
			nvdisp_cmd_int_status_uf_f(1)));
	}

	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE) {
		schedule_delayed_work(&dc->one_shot_work,
				msecs_to_jiffies(dc->one_shot_delay_ms));
	}
	dc->crc_pending = true;

	tegra_dc_program_bandwidth(dc, false);

	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE)
		act_req_mask |= nvdisp_cmd_state_ctrl_host_trig_enable_f();

	/* cannot set fields related to UPDATE and ACT_REQ in the same write */
	tegra_dc_writel(dc, update_mask, nvdisp_cmd_state_ctrl_r());
	tegra_dc_readl(dc, nvdisp_cmd_state_ctrl_r()); /* flush */
	if (act_req_mask) {
		tegra_dc_writel(dc, act_req_mask, nvdisp_cmd_state_ctrl_r());
		tegra_dc_readl(dc, nvdisp_cmd_state_ctrl_r()); /* flush */
	}

	if (!wait_for_vblank) {
		/* Don't use a interrupt handler for the update, but leave
		   vblank interrupts unmasked since they could be used by other
		   windows.  One window could flip on HBLANK while others flip
		   on VBLANK.  Poll HW until this window update is completed
		   which could take up to 16 scan lines for T18x or time out
		   about a frame period. */
		unsigned int  winmask = act_req_mask &
			((0x3f & dc->pdata->win_mask) << 1)/*WIN_ALL_ACT_REQ*/;
		int  i = 17000;  /* 60Hz frame period in uSec */

		while (tegra_dc_windows_are_dirty(dc, winmask) && --i)
			udelay(1);

		if (i) {
			for (i = 0; i < n; i++)
				windows[i]->dirty = 0;
		} else {  /* time out */
			dev_warn(&dc->ndev->dev, "winmask:0x%x: HSync timeout\n",
				winmask);
		}
	}

	mutex_unlock(&tegra_nvdisp_lock);

	return 0;
}


/* detach window idx from current head */
int tegra_nvdisp_detach_win(struct tegra_dc *dc, unsigned idx)
{
	struct tegra_dc_win *win = tegra_dc_get_window(dc, idx);

	if (!win || win->dc != dc) {
		dev_err(&dc->ndev->dev,
			"%s: window %d does not belong to head %d\n",
			__func__, idx, dc->ctrl_num);
		return -EINVAL;
	}

	mutex_lock(&tegra_nvdisp_lock);

	/* detach window idx */
	nvdisp_win_write(win,
		win_set_control_owner_none_f(), win_set_control_r());

	win->dc = NULL;
	win->is_scaler_coeff_set = false;
	mutex_unlock(&tegra_nvdisp_lock);
	return 0;
}


/* Assign window idx to head dc */
int tegra_nvdisp_assign_win(struct tegra_dc *dc, unsigned idx)
{
	struct tegra_dc_win *win = tegra_dc_get_window(dc, idx);
	/* Pulls configuration in from TEGRA_DC_FEATURE_INVERT_TYPE field */
	bool enable_blx4 = tegra_dc_feature_has_scan_column(dc, idx);

	if (win == NULL)
		return -EINVAL;

	if (win->dc == dc) { /* already assigned to current head */
		/* If Reset happens without detach_win call
		 * call attach idx for safety. Can remove this
		 * once make sure detach_win is called before attach
		 */
		nvdisp_win_write(win, dc->ctrl_num, win_set_control_r());
		return 0;
	}

	tegra_nvdisp_reserve_common_channel(dc);

	mutex_lock(&tegra_nvdisp_lock);

	if (win->dc) {	/* window is owned by another head */
		dev_err(&dc->ndev->dev,
			"%s: cannot assign win %d to head %d, it owned by %d\n",
			__func__, idx, dc->ctrl_num, win->dc->ctrl_num);
		dc->valid_windows &= ~(0x1 << idx);
		mutex_unlock(&tegra_nvdisp_lock);
		return -EINVAL;
	}

	win->dc = dc;
	dc->tmp_wins[idx].dc = dc;

	/* attach window idx */
	nvdisp_win_write(win, dc->ctrl_num, win_set_control_r());

	/* configure some IHUB related settings  */
	if (enable_blx4)
		nvdisp_win_write(win,
				win_ihub_linebuf_config_mode_four_lines_f(),
				win_ihub_linebuf_config_r());
	else
		nvdisp_win_write(win,
				win_ihub_linebuf_config_mode_two_lines_f(),
				win_ihub_linebuf_config_r());

	/* assign a default thread group to the window.
	 * WinA=group 0, WinB=group 1, ... */
	nvdisp_win_write(win, win_ihub_thread_group_num_f(idx) |
			win_ihub_thread_group_enable_yes_f(),
			win_ihub_thread_group_r());

	/* TODO: configure the mempool
	nvdisp_win_write(win, win_ihub_pool_config_entries_f(817),
			win_ihub_pool_config_r());
	*/

	/* promote the state */
	tegra_dc_writel(dc, nvdisp_cmd_state_ctrl_common_act_update_enable_f() |
		nvdisp_cmd_state_ctrl_win_a_update_enable_f() << win->idx,
		nvdisp_cmd_state_ctrl_r());
	tegra_dc_readl(dc, nvdisp_cmd_state_ctrl_r()); /* flush */
	tegra_dc_writel(dc, nvdisp_cmd_state_ctrl_common_act_req_enable_f() |
		nvdisp_cmd_state_ctrl_a_act_req_enable_f() << win->idx,
		nvdisp_cmd_state_ctrl_r());
	/* wait for COMMON_ACT_REQ to complete or time out */
	if (tegra_dc_poll_register(dc, nvdisp_cmd_state_ctrl_r(),
			nvdisp_cmd_state_ctrl_common_act_req_enable_f(),
			0, 1, NVDISP_TEGRA_POLL_TIMEOUT_MS))
		dev_err(&dc->ndev->dev,
			"dc timeout waiting for DC to stop\n");

	tegra_nvdisp_release_common_channel(dc);

	/* set the windows scaler coeff value */
	if (!win->is_scaler_coeff_set) {
		tegra_nvdisp_set_scaler_coeff(win);
		win->is_scaler_coeff_set = true;
	}

	win->csc = dc->default_csc;
	win->csc_dirty = true;

	mutex_unlock(&tegra_nvdisp_lock);
	return 0;
}
