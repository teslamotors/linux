/*
 * arch/arm/mach-tegra/include/mach/dc.h
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *	Erik Gilling <konkers@google.com>
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

#ifndef __MACH_TEGRA_DC_H
#define __MACH_TEGRA_DC_H

#include <linux/pm.h>
#include <linux/types.h>
#include <linux/fb.h>
#include <linux/platform/tegra/isomgr.h>
#include <drm/drm_fixed.h>

#if defined(CONFIG_TEGRA_NVDISPLAY)
#define TEGRA_MAX_DC		3
#else
#define TEGRA_MAX_DC		2
#endif

#if defined(CONFIG_ARCH_TEGRA_14x_SOC)
#define DC_N_WINDOWS		6
#elif defined(CONFIG_ARCH_TEGRA_12x_SOC) || defined(CONFIG_ARCH_TEGRA_21x_SOC)
#define DC_N_WINDOWS		5
#elif defined(CONFIG_ARCH_TEGRA_11x_SOC)
#define DC_N_WINDOWS		3
#else  /* Max of all chips */
#define DC_N_WINDOWS		6
#endif

#define DSI_NODE		"/host1x/dsi"
#define SOR_NODE		"/host1x/sor"
#define SOR1_NODE		"/host1x/sor1"

#if defined(CONFIG_ARCH_TEGRA_18x_SOC)
#define DPAUX_NODE		"/host1x/dpaux@155c0000"
#define DPAUX1_NODE		"/host1x/dpaux@15040000"
#else
#define DPAUX_NODE		"/host1x/dpaux"
#define DPAUX1_NODE		"/host1x/dpaux1"
#endif

#if defined(CONFIG_ARCH_TEGRA_21x_SOC) || defined(CONFIG_TEGRA_NVDISPLAY)
#define HDMI_NODE		SOR1_NODE
#else
#define HDMI_NODE		"/host1x/hdmi"
#endif

#define DEFAULT_FPGA_FREQ_KHZ	160000

#define TEGRA_DC_EXT_FLIP_MAX_WINDOW 6
#if defined(CONFIG_TEGRA_NVDISPLAY)
#define GAIN_TABLE_MAX_ENTRIES	32
#define ADAPTATION_FACTOR_MAX_LEVELS	51
#endif
extern atomic_t sd_brightness;

extern struct fb_videomode tegra_dc_vga_mode;

extern char dc_or_node_names[][14];

enum {
	TEGRA_HPD_STATE_FORCE_DEASSERT = -1,
	TEGRA_HPD_STATE_NORMAL = 0,
	TEGRA_HPD_STATE_FORCE_ASSERT = 1,
};

/* DSI pixel data format */
enum {
	TEGRA_DSI_PIXEL_FORMAT_16BIT_P,
	TEGRA_DSI_PIXEL_FORMAT_18BIT_P,
	TEGRA_DSI_PIXEL_FORMAT_18BIT_NP,
	TEGRA_DSI_PIXEL_FORMAT_24BIT_P,
	TEGRA_DSI_PIXEL_FORMAT_8BIT_DSC,
	TEGRA_DSI_PIXEL_FORMAT_12BIT_DSC,
	TEGRA_DSI_PIXEL_FORMAT_16BIT_DSC,
};

/* DSI virtual channel number */
enum {
	TEGRA_DSI_VIRTUAL_CHANNEL_0,
	TEGRA_DSI_VIRTUAL_CHANNEL_1,
	TEGRA_DSI_VIRTUAL_CHANNEL_2,
	TEGRA_DSI_VIRTUAL_CHANNEL_3,
};

/* DSI transmit method for video data */
enum {
	TEGRA_DSI_VIDEO_TYPE_VIDEO_MODE,
	TEGRA_DSI_VIDEO_TYPE_COMMAND_MODE,
};

/* DSI HS clock mode */
enum {
	TEGRA_DSI_VIDEO_CLOCK_CONTINUOUS,
	TEGRA_DSI_VIDEO_CLOCK_TX_ONLY,
};

/* DSI burst mode setting in video mode. Each mode is assigned with a
 * fixed value. The rationale behind this is to avoid change of these
 * values, since the calculation of dsi clock depends on them. */
enum {
	TEGRA_DSI_VIDEO_NONE_BURST_MODE = 0,
	TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END = 1,
	TEGRA_DSI_VIDEO_BURST_MODE_LOWEST_SPEED = 2,
	TEGRA_DSI_VIDEO_BURST_MODE_LOW_SPEED = 3,
	TEGRA_DSI_VIDEO_BURST_MODE_MEDIUM_SPEED = 4,
	TEGRA_DSI_VIDEO_BURST_MODE_FAST_SPEED = 5,
	TEGRA_DSI_VIDEO_BURST_MODE_FASTEST_SPEED = 6,
};

enum {
	TEGRA_DSI_GANGED_SYMMETRIC_LEFT_RIGHT = 1,
	TEGRA_DSI_GANGED_SYMMETRIC_EVEN_ODD = 2,
	TEGRA_DSI_GANGED_SYMMETRIC_LEFT_RIGHT_OVERLAP = 3,
};

/* Split Link Type */
enum {
	TEGRA_DSI_SPLIT_LINK_A_B = 1,
	TEGRA_DSI_SPLIT_LINK_C_D = 2,
	TEGRA_DSI_SPLIT_LINK_A_B_C_D = 3,
};

enum {
	TEGRA_DSI_PACKET_CMD,
	TEGRA_DSI_DELAY_MS,
	TEGRA_DSI_GPIO_SET,
	TEGRA_DSI_SEND_FRAME,
	TEGRA_DSI_PACKET_VIDEO_VBLANK_CMD,
};
enum {
	TEGRA_DSI_LINK0,
	TEGRA_DSI_LINK1,
};

struct tegra_dsi_cmd {
	u8	cmd_type;
	u8	data_id;
	union {
		u16 data_len;
		u16 delay_ms;
		unsigned gpio;
		u16 frame_cnt;
		struct {
			u8 data0;
			u8 data1;
		} sp;
	} sp_len_dly;
	u8	*pdata;
	u8   link_id;
	bool	club_cmd;
};

#define CMD_CLUBBED				true
#define CMD_NOT_CLUBBED				false

#define DSI_GENERIC_LONG_WRITE			0x29
#define DSI_DCS_LONG_WRITE			0x39
#define DSI_GENERIC_SHORT_WRITE_1_PARAMS	0x13
#define DSI_GENERIC_SHORT_WRITE_2_PARAMS	0x23
#define DSI_DCS_WRITE_0_PARAM			0x05
#define DSI_DCS_WRITE_1_PARAM			0x15

#define DSI_DCS_SET_ADDR_MODE			0x36
#define DSI_DCS_EXIT_SLEEP_MODE			0x11
#define DSI_DCS_ENTER_SLEEP_MODE		0x10
#define DSI_DCS_SET_DISPLAY_ON			0x29
#define DSI_DCS_SET_DISPLAY_OFF			0x28
#define DSI_DCS_SET_TEARING_EFFECT_OFF		0x34
#define DSI_DCS_SET_TEARING_EFFECT_ON		0x35
#define DSI_DCS_NO_OP				0x0
#define DSI_NULL_PKT_NO_DATA			0x9
#define DSI_BLANKING_PKT_NO_DATA		0x19

#define IS_DSI_SHORT_PKT(cmd)	((cmd.data_id == DSI_DCS_WRITE_0_PARAM) ||\
			(cmd.data_id == DSI_DCS_WRITE_1_PARAM) ||\
			(cmd.data_id == DSI_GENERIC_SHORT_WRITE_1_PARAMS) ||\
			(cmd.data_id == DSI_GENERIC_SHORT_WRITE_2_PARAMS))

#define _DSI_CMD_SHORT(di, p0, p1, lnk_id, _cmd_type, club)	{ \
					.cmd_type = _cmd_type, \
					.data_id = di, \
					.sp_len_dly.sp.data0 = p0, \
					.sp_len_dly.sp.data1 = p1, \
					.link_id = lnk_id, \
					.club_cmd = club,\
					}

#define DSI_CMD_VBLANK_SHORT(di, p0, p1, club) \
			_DSI_CMD_SHORT(di, p0, p1, TEGRA_DSI_LINK0,\
				TEGRA_DSI_PACKET_VIDEO_VBLANK_CMD, club)

#define DSI_CMD_SHORT_LINK(di, p0, p1, lnk_id) \
			_DSI_CMD_SHORT(di, p0, p1, lnk_id,\
				TEGRA_DSI_PACKET_CMD, CMD_NOT_CLUBBED)

#define DSI_CMD_SHORT(di, p0, p1)	\
			DSI_CMD_SHORT_LINK(di, p0, p1, TEGRA_DSI_LINK0)

#define DSI_DLY_MS(ms)	{ \
			.cmd_type = TEGRA_DSI_DELAY_MS, \
			.sp_len_dly.delay_ms = ms, \
			}

#define DSI_GPIO_SET(rst_gpio, on)	{ \
					.cmd_type = TEGRA_DSI_GPIO_SET, \
					.data_id = on, \
					.sp_len_dly.gpio = rst_gpio, \
					}

#define _DSI_CMD_LONG(di, ptr, lnk_id, _cmd_type)	{ \
				.cmd_type = _cmd_type, \
				.data_id = di, \
				.sp_len_dly.data_len = ARRAY_SIZE(ptr), \
				.pdata = ptr, \
				.link_id = lnk_id, \
				}

#define DSI_CMD_VBLANK_LONG(di, ptr)	\
		_DSI_CMD_LONG(di, ptr, TEGRA_DSI_LINK0, \
					TEGRA_DSI_PACKET_VIDEO_VBLANK_CMD)

#define DSI_CMD_LONG_LINK(di, ptr, lnk_id)	\
		_DSI_CMD_LONG(di, ptr, lnk_id, TEGRA_DSI_PACKET_CMD)

#define DSI_CMD_LONG(di, ptr)	\
			DSI_CMD_LONG_LINK(di, ptr, TEGRA_DSI_LINK0)

#define _DSI_CMD_LONG_SIZE(di, ptr, size, lnk_id, _cmd_type)	{ \
				.cmd_type = _cmd_type, \
				.data_id = di, \
				.sp_len_dly.data_len = size, \
				.pdata = ptr, \
				.link_id = lnk_id, \
				}

#define DSI_CMD_LONG_SIZE(di, ptr, size)	\
	_DSI_CMD_LONG_SIZE(di, ptr, size, TEGRA_DSI_LINK0, TEGRA_DSI_PACKET_CMD)

#define DSI_SEND_FRAME(cnt)	{ \
			.cmd_type = TEGRA_DSI_SEND_FRAME, \
			.sp_len_dly.frame_cnt = cnt, \
			}

struct dsi_phy_timing_ns {
	u16		t_hsdexit_ns;
	u16		t_hstrail_ns;
	u16		t_datzero_ns;
	u16		t_hsprepare_ns;

	u16		t_clktrail_ns;
	u16		t_clkpost_ns;
	u16		t_clkzero_ns;
	u16		t_tlpx_ns;

	u16		t_clkprepare_ns;
	u16		t_clkpre_ns;
	u16		t_wakeup_ns;

	u16		t_taget_ns;
	u16		t_tasure_ns;
	u16		t_tago_ns;
};

enum {
	CMD_VS		= 0x01,
	CMD_VE		= 0x11,

	CMD_HS		= 0x21,
	CMD_HE		= 0x31,

	CMD_EOT		= 0x08,
	CMD_NULL	= 0x09,
	CMD_SHORTW	= 0x15,
	CMD_BLNK	= 0x19,
	CMD_LONGW	= 0x39,

	CMD_RGB		= 0x00,
	CMD_RGB_16BPP	= 0x0E,
	CMD_RGB_18BPP	= 0x1E,
	CMD_RGB_18BPPNP = 0x2E,
	CMD_RGB_24BPP	= 0x3E,

	CMD_CMPR_PIXEL_STREAM	 = 0x0B,
	CMD_DSC_WRITE_START	= 0x2C,
	CMD_DSC_WRITE_CONT	= 0x3C,
};

enum {
	TEGRA_DSI_DISABLE,
	TEGRA_DSI_ENABLE,
};

#define PKT_ID0(id)	((((id) & 0x3f) << 3) | \
			(((TEGRA_DSI_ENABLE) & 0x1) << 9))
#define PKT_LEN0(len)	(((len) & 0x7) << 0)
#define PKT_ID1(id)	((((id) & 0x3f) << 13) | \
			(((TEGRA_DSI_ENABLE) & 0x1) << 19))
#define PKT_LEN1(len)	(((len) & 0x7) << 10)
#define PKT_ID2(id)	((((id) & 0x3f) << 23) | \
			(((TEGRA_DSI_ENABLE) & 0x1) << 29))
#define PKT_LEN2(len)	(((len) & 0x7) << 20)
#define PKT_ID3(id)	((((id) & 0x3f) << 3) | \
			(((TEGRA_DSI_ENABLE) & 0x1) << 9))
#define PKT_LEN3(len)	(((len) & 0x7) << 0)
#define PKT_ID4(id)	((((id) & 0x3f) << 13) | \
			(((TEGRA_DSI_ENABLE) & 0x1) << 19))
#define PKT_LEN4(len)	(((len) & 0x7) << 10)
#define PKT_ID5(id)	((((id) & 0x3f) << 23) | \
			(((TEGRA_DSI_ENABLE) & 0x1) << 29))
#define PKT_LEN5(len)	(((len) & 0x7) << 20)
#define PKT_LP		(((TEGRA_DSI_ENABLE) & 0x1) << 30)
#define NUMOF_PKT_SEQ	12

enum {
	DSI_VS_0 = 0x0,
	DSI_VS_1 = 0x1,
};

enum {
#ifndef CONFIG_TEGRA_NVDISPLAY
	DSI_INSTANCE_0,
	DSI_INSTANCE_1,
#else
/* T186 has 4 controllers. DSI-A and DSI-C are the main controllers
   needed for ganged mode */
	DSI_INSTANCE_0,
	DSI_INSTANCE_1 = 2,
#endif
};

/* Aggressiveness level of DSI suspend. The higher, the more aggressive. */
#define DSI_NO_SUSPEND			0
#define DSI_HOST_SUSPEND_LV0		1
#define DSI_HOST_SUSPEND_LV1		2
#define DSI_HOST_SUSPEND_LV2		3

/*
 * DPD (deep power down) mode for dsi pads.
 *  - Available for DSI_VS1 and later.
 *  - Available for Non-DSI_GANGED.
 * Usually, DSIC/DSID pads' DPD for DSI_INSTANCE_0 and
   DSI/DSIB pads' DPD for DSI_INSTANCE_1 can be enabled, respectively,
 * but in SW, sometimes pins from one pad can be used by
 * more than one module, so it may be dependent on board design.
 */
#define DSI_DPD_EN		(1 << 0)
#define DSIB_DPD_EN		(1 << 1)
#define DSIC_DPD_EN		(1 << 2)
#define DSID_DPD_EN		(1 << 3)

struct tegra_dsi_board_info {
	u32 platform_boardid;
	u32 platform_boardversion;
	u32 display_boardid;
	u32 display_boardversion;
};
struct tegra_dsi_out {
	u8		n_data_lanes;			/* required */
	u8		pixel_format;			/* required */
	u8		refresh_rate;			/* required */
	u8		rated_refresh_rate;
	u8		panel_reset;			/* required */
	u8		virtual_channel;		/* required */
	u8		dsi_instance;
	u16		dsi_panel_rst_gpio;
	u16		dsi_panel_bl_en_gpio;
	u16		dsi_panel_bl_pwm_gpio;
	u16		even_odd_split_width;
	u8		controller_vs;

	bool		panel_has_frame_buffer;	/* required*/

	/* Deprecated. Use DSI_SEND_FRAME panel command instead. */
	bool		panel_send_dc_frames;

	struct tegra_dsi_cmd	*dsi_init_cmd;		/* required */
	u16		n_init_cmd;			/* required */

	struct tegra_dsi_cmd	*dsi_early_suspend_cmd;
	u16		n_early_suspend_cmd;

	struct tegra_dsi_cmd	*dsi_late_resume_cmd;
	u16		n_late_resume_cmd;

	struct tegra_dsi_cmd	*dsi_suspend_cmd;	/* required */
	u16		n_suspend_cmd;			/* required */

	u8		video_data_type;		/* required */
	u8		video_clock_mode;
	u8		video_burst_mode;
	u8		ganged_type;
	u16		ganged_overlap;
	bool		ganged_swap_links;
	bool		ganged_write_to_all_links;
	u8		split_link_type;

	u8		suspend_aggr;

	u16		panel_buffer_size_byte;
	u16		panel_reset_timeout_msec;

	bool		hs_cmd_mode_supported;
	bool		hs_cmd_mode_on_blank_supported;
	bool		enable_hs_clock_on_lp_cmd_mode;
	bool		no_pkt_seq_eot; /* 1st generation panel may not
					 * support eot. Don't set it for
					 * most panels. */
	bool		skip_dsi_pkt_header;
	bool		te_polarity_low;
	bool		power_saving_suspend;
	bool		suspend_stop_stream_late;
	bool		dsi2lvds_bridge_enable;
	bool		dsi2edp_bridge_enable;

	u32		max_panel_freq_khz;
	u32		lp_cmd_mode_freq_khz;
	u32		lp_read_cmd_mode_freq_khz;
	u32		hs_clk_in_lp_cmd_mode_freq_khz;
	u32		burst_mode_freq_khz;
	u32		fpga_freq_khz;

	u32		te_gpio;

	u32		dpd_dsi_pads;

	const u32		*pkt_seq;

	struct dsi_phy_timing_ns phy_timing;

	u8		*bl_name;

	bool		lp00_pre_panel_wakeup;
	bool		ulpm_not_supported;
	struct tegra_dsi_board_info	boardinfo;
	bool		use_video_host_fifo_for_cmd;
	bool		dsi_csi_loopback;
};

enum {
	TEGRA_DC_STEREO_MODE_2D,
	TEGRA_DC_STEREO_MODE_3D
};

enum {
	TEGRA_DC_STEREO_LANDSCAPE,
	TEGRA_DC_STEREO_PORTRAIT
};

struct tegra_stereo_out {
	int  mode_2d_3d;
	int  orientation;

	void (*set_mode)(int mode);
	void (*set_orientation)(int orientation);
};

struct tegra_dc_mode {
	int	pclk;
	int	rated_pclk;
	int	h_ref_to_sync;
	int	v_ref_to_sync;
	int	h_sync_width;
	int	v_sync_width;
	int	h_back_porch;
	int	v_back_porch;
	int	h_active;
	int	v_active;
	int	h_front_porch;
	int	v_front_porch;
	int	stereo_mode;
	u32	flags;
	u8	avi_m;
	u32	vmode;
};

#define TEGRA_DC_MODE_FLAG_NEG_V_SYNC	(1 << 0)
#define TEGRA_DC_MODE_FLAG_NEG_H_SYNC	(1 << 1)
#define TEGRA_DC_MODE_FLAG_NEG_DE		(1 << 2)

#define TEGRA_DC_MODE_AVI_M_NO_DATA	0x0
#define TEGRA_DC_MODE_AVI_M_4_3	0x1
#define TEGRA_DC_MODE_AVI_M_16_9	0x2
#define TEGRA_DC_MODE_AVI_M_64_27	0x3	/* dummy, no avi m support */
#define TEGRA_DC_MODE_AVI_M_256_135	0x4	/* dummy, no avi m support */

enum {
	TEGRA_DC_OUT_RGB,
	TEGRA_DC_OUT_HDMI,
	TEGRA_DC_OUT_DSI,
	TEGRA_DC_OUT_DP,
	TEGRA_DC_OUT_LVDS,
	TEGRA_DC_OUT_NVSR_DP,
	TEGRA_DC_OUT_FAKE_DP,
	TEGRA_DC_OUT_FAKE_DSIA,
	TEGRA_DC_OUT_FAKE_DSIB,
	TEGRA_DC_OUT_FAKE_DSI_GANGED,
	TEGRA_DC_OUT_NULL,
	TEGRA_DC_OUT_MAX /* Keep this always as last enum */
};

struct tegra_dc_out_pin {
	int	name;
	int	pol;
};

enum {
	TEGRA_DC_OUT_PIN_DATA_ENABLE,
	TEGRA_DC_OUT_PIN_H_SYNC,
	TEGRA_DC_OUT_PIN_V_SYNC,
	TEGRA_DC_OUT_PIN_PIXEL_CLOCK,
};

enum {
	TEGRA_DC_OUT_PIN_POL_LOW,
	TEGRA_DC_OUT_PIN_POL_HIGH,
};

enum {
	TEGRA_DC_UNDEFINED_DITHER = 0,
	TEGRA_DC_DISABLE_DITHER,
	TEGRA_DC_ORDERED_DITHER,
	TEGRA_DC_ERRDIFF_DITHER,
	TEGRA_DC_TEMPORAL_DITHER,
	TEGRA_DC_ERRACC_DITHER,
};

typedef u8 tegra_dc_bl_output[256];
typedef u8 *p_tegra_dc_bl_output;

struct tegra_dc_sd_blp {
	u16 time_constant;
	u8 step;
};

struct tegra_dc_sd_fc {
	u8 time_limit;
	u8 threshold;
};

struct tegra_dc_sd_rgb {
	u8 r;
	u8 g;
	u8 b;
};

struct tegra_dc_sd_agg_priorities {
	u8 pri_lvl;
	u8 agg[4];
};

struct tegra_dc_sd_window {
	u16 h_position;
	u16 v_position;
	u16 h_size;
	u16 v_size;
};

struct tegra_dc_sd_settings {
#ifdef CONFIG_TEGRA_NVDISPLAY
	bool update_sd;
	unsigned upper_bound;
	unsigned lower_bound;
	unsigned num_over_saturated_pixels;
	unsigned over_saturated_bin;
	unsigned last_over_saturated_bin;
	unsigned *gain_table;
	unsigned gain_luts_parsed;
	unsigned pixel_gain_tables[ADAPTATION_FACTOR_MAX_LEVELS]
		[GAIN_TABLE_MAX_ENTRIES];
	unsigned backlight_table[ADAPTATION_FACTOR_MAX_LEVELS];
	unsigned *current_gain_table;
	unsigned *phase_backlight_table;
	unsigned new_backlight;
	unsigned old_backlight;
	unsigned last_phase_step;
	unsigned phase_in_steps;
	int backlight_adjust_steps;
	u8 sw_update_delay;
	int frame_runner;
#endif
	unsigned enable;
	u8 turn_off_brightness;
	u8 turn_on_brightness;
	unsigned enable_int;
	bool use_auto_pwm;
	u8 hw_update_delay;
	u8 aggressiveness;
	short bin_width;
	u8 phase_in_settings;
	u8 phase_in_adjustments;
	u8 cmd;
	u8 final_agg;
	u16 cur_agg_step;
	u16 phase_settings_step;
	u16 phase_adj_step;
	u16 num_phase_in_steps;

	struct tegra_dc_sd_agg_priorities agg_priorities;

	bool use_vid_luma;
	struct tegra_dc_sd_rgb coeff;

	bool k_limit_enable;
	u16 k_limit;

	bool sd_window_enable;
	struct tegra_dc_sd_window sd_window;

	bool soft_clipping_enable;
	u8 soft_clipping_threshold;

	bool smooth_k_enable;
	u16 smooth_k_incr;

	bool sd_proc_control;
	bool soft_clipping_correction;
	bool use_vpulse2;

	struct tegra_dc_sd_fc fc;
	struct tegra_dc_sd_blp blp;
	u8 bltf[4][4][4];
	struct tegra_dc_sd_rgb lut[4][9];

	atomic_t *sd_brightness;
	char *bl_device_name;
	struct backlight_device *bl_device;

	u8 bias0;
};

enum {
	NO_CMD = 0x0,
	ENABLE = 0x1,
	DISABLE = 0x2,
	PHASE_IN = 0x4,
	AGG_CHG = 0x8,
};

enum {
	TEGRA_PIN_OUT_CONFIG_SEL_LHP0_LD21,
	TEGRA_PIN_OUT_CONFIG_SEL_LHP1_LD18,
	TEGRA_PIN_OUT_CONFIG_SEL_LHP2_LD19,
	TEGRA_PIN_OUT_CONFIG_SEL_LVP0_LVP0_Out,
	TEGRA_PIN_OUT_CONFIG_SEL_LVP1_LD20,

	TEGRA_PIN_OUT_CONFIG_SEL_LM1_M1,
	TEGRA_PIN_OUT_CONFIG_SEL_LM1_LD21,
	TEGRA_PIN_OUT_CONFIG_SEL_LM1_PM1,

	TEGRA_PIN_OUT_CONFIG_SEL_LDI_LD22,
	TEGRA_PIN_OUT_CONFIG_SEL_LPP_LD23,
	TEGRA_PIN_OUT_CONFIG_SEL_LDC_SDC,
	TEGRA_PIN_OUT_CONFIG_SEL_LSPI_DE,
};

/* this is the old name. provided for compatibility with old board files. */
#define dcc_bus ddc_bus

struct tegra_vrr {
	s32	capability;
	s32	enable;
	s32	lastenable;
	s32	flip;
	s32	pclk;
	s32	vrr_min_fps;
	s32	vrr_max_fps;
	s32	v_front_porch_max;
	s32	v_front_porch_min;
	s32	vfp_extend;
	s32	vfp_shrink;
	s32	v_front_porch;
	s32	v_back_porch;

	s64	curr_flip_us;
	s64	last_flip_us;
	s32	flip_count;
	s32	flip_interval_us;
	s32	frame_len_max;
	s32	frame_len_min;
	s32	frame_len_fluct;
	s32	line_width;
	s32	lines_per_frame_common;

	s32	frame_type;
	s32	frame_count;
	s32	v_count;
	s32	last_v_cnt;
	s32	curr_v_cnt;
	s64     last_frame_us;
	s64     curr_frame_us;
	s64     fe_time_us;
	s32	frame_delta_us;
	s32	frame_interval_us;
	s32	even_frame_us;
	s32	odd_frame_us;

	s32	max_adj_pct;
	s32	max_flip_pct;
	s32	max_dcb;
	s32	max_inc_pct;

	s32	dcb;
	s32	frame_avg_pct;
	s32	fluct_avg_pct;

	s32	fe_intr_req;
	s32	db_tolerance;
	s32	frame2flip_us;
	s32	adjust_vfp;
	s32	adjust_db;
	u32	db_correct_cap;
	u32	db_hist_cap;
	s32	vfp;
	s32	insert_frame;
	s32	g_vrr_session_id;
	s32	nvdisp_direct_drive;

	/* Must be kept in order */
	u8	keynum;
	u8	serial[9];
	u8	challenge[32];
	u8	digest[32];
	u8	challenge_src;
};

struct tegra_dc_out {
	int				type;
	unsigned			flags;
	unsigned			hdcp_policy;

	/* size in mm */
	unsigned			h_size;
	unsigned			v_size;

	int				ddc_bus;
	int				hotplug_gpio;
	int				hotplug_state; /* 0 normal 1 force on */
	const char			*parent_clk;
	const char			*parent_clk_backup;

	unsigned			max_pixclock;
	unsigned			order;
	unsigned			align;
	unsigned			depth;
	unsigned			dither;

	struct tegra_dc_mode		*modes;
	int				n_modes;

	struct tegra_dsi_out		*dsi;
	struct tegra_hdmi_out		*hdmi_out;
	struct tegra_dp_out		*dp_out;
	struct tegra_stereo_out		*stereo;
	struct tegra_vrr		*vrr;

	unsigned			height; /* mm */
	unsigned			width; /* mm */
	unsigned			rotation; /* degrees */

	struct tegra_dc_out_pin		*out_pins;
	unsigned			n_out_pins;

	struct tegra_dc_sd_settings	*sd_settings;

	/* DSI link compression parameters */
	u32		slice_height;
	u32		slice_width;
	u8		num_of_slices;
	u8		dsc_bpp;
	bool		en_block_pred;
	bool		dsc_en;

	u8			*out_sel_configs;
	unsigned		n_out_sel_configs;
	int			user_needs_vblank;
	struct completion	user_vblank_comp;

	bool				is_ext_dp_panel;

	int	(*enable)(struct device *);
	int	(*postpoweron)(struct device *);
	int	(*prepoweroff)(void);
	int	(*disable)(struct device *);

	int	(*hotplug_init)(struct device *);
	int	(*postsuspend)(void);
	void	(*hotplug_report)(bool);
};

/* bits for tegra_dc_out.flags */
#define TEGRA_DC_OUT_HOTPLUG_HIGH		(0 << 1)
#define TEGRA_DC_OUT_HOTPLUG_LOW		(1 << 1)
#define TEGRA_DC_OUT_HOTPLUG_MASK		(1 << 1)
#define TEGRA_DC_OUT_NVHDCP_POLICY_ALWAYS_ON	(0 << 2)
#define TEGRA_DC_OUT_NVHDCP_POLICY_ON_DEMAND	(1 << 2)
#define TEGRA_DC_OUT_NVHDCP_POLICY_MASK		(1 << 2)
#define TEGRA_DC_OUT_CONTINUOUS_MODE		(0 << 3)
#define TEGRA_DC_OUT_ONE_SHOT_MODE		(1 << 3)
#define TEGRA_DC_OUT_N_SHOT_MODE		(1 << 4)
#define TEGRA_DC_OUT_ONE_SHOT_LP_MODE		(1 << 5)
#define TEGRA_DC_OUT_INITIALIZED_MODE		(1 << 6)
/* Makes hotplug GPIO a LP0 wakeup source */
#define TEGRA_DC_OUT_HOTPLUG_WAKE_LP0		(1 << 7)
#define TEGRA_DC_OUT_NVSR_MODE			(1 << 8)

#define TEGRA_DC_HDCP_POLICY_ALWAYS_ON	0
#define TEGRA_DC_HDCP_POLICY_ON_DEMAND	1
#define TEGRA_DC_HDCP_POLICY_ALWAYS_OFF	2

#define TEGRA_DC_ALIGN_MSB		0
#define TEGRA_DC_ALIGN_LSB		1

#define TEGRA_DC_ORDER_RED_BLUE		0
#define TEGRA_DC_ORDER_BLUE_RED		1

/* Errands use the interrupts */
#define V_BLANK_FLIP		0
#define V_BLANK_NVSD		1
#define V_BLANK_USER		2

#define V_PULSE2_FLIP		0
#define V_PULSE2_NVSD		1

struct tegra_dc;
struct nvmap_handle_ref;

#if defined(CONFIG_TEGRA_CSC_V2)
struct tegra_dc_csc_v2 {
	u32 r2r;
	u32 g2r;
	u32 b2r;
	u32 const2r;
	u32 r2g;
	u32 g2g;
	u32 b2g;
	u32 const2g;
	u32 r2b;
	u32 g2b;
	u32 b2b;
	u32 const2b;
	u32 csc_enable;
};
#endif

struct tegra_dc_csc {
	unsigned short yof;
	unsigned short kyrgb;
	unsigned short kur;
	unsigned short kvr;
	unsigned short kug;
	unsigned short kvg;
	unsigned short kub;
	unsigned short kvb;
};

#if defined(CONFIG_TEGRA_LUT)
/* palette lookup table */
struct tegra_dc_lut {
	u8 r[256];
	u8 g[256];
	u8 b[256];
};
#endif

#if defined(CONFIG_TEGRA_LUT_V2)
/* palette lookup table */
struct tegra_dc_lut {
	u64 *rgb;
	dma_addr_t phy_addr;
	size_t size;
};
#endif

struct tegra_dc_cmu_csc {
	u16 krr;
	u16 kgr;
	u16 kbr;
	u16 krg;
	u16 kgg;
	u16 kbg;
	u16 krb;
	u16 kgb;
	u16 kbb;
};

#if defined(CONFIG_TEGRA_DC_CMU_V2)
struct tegra_dc_cmu {
	u64			rgb[1025];
#if defined(CONFIG_TEGRA_CSC_V2)
	struct tegra_dc_csc_v2	panel_csc;
#endif
};
#else
struct tegra_dc_cmu {
	u16 lut1[256];
	struct tegra_dc_cmu_csc csc;
	u8 lut2[960];
};
#endif

struct tegra_dc_hdr {
	bool		enabled;
	u32		eotf;
	u32		static_metadata_id;
	u8		static_metadata[24];
};

struct tegra_dc_imp_head_results {
	u32	num_windows;
	u8	cursor_active;
	u32	win_ids[DC_N_WINDOWS];
	u32	thread_group_win[DC_N_WINDOWS];
	u32	metering_slots_value_win[DC_N_WINDOWS];
	u32	thresh_lwm_dvfs_win[DC_N_WINDOWS];
	u32	pipe_meter_value_win[DC_N_WINDOWS];
	u32	pool_config_entries_win[DC_N_WINDOWS];
	u32	metering_slots_value_cursor;
	u32	pipe_meter_value_cursor;
	u32	pool_config_entries_cursor;
	u64	total_latency;
	u64     hubclk;
	u32	window_slots_value;
	u32	cursor_slots_value;
	u64     required_total_bw_kbps;
};

struct tegra_dc_win {
	u8			idx;
	u8			ppflags; /* see TEGRA_WIN_PPFLAG* */
	u8			global_alpha;
	u32			fmt;
	u32			flags;

	void			*virt_addr;
	dma_addr_t		phys_addr;
	dma_addr_t		phys_addr_u;
	dma_addr_t		phys_addr_v;
#if defined(CONFIG_TEGRA_DC_INTERLACE)
	/* field 2 starting address */
	dma_addr_t		phys_addr2;
	dma_addr_t		phys_addr_u2;
	dma_addr_t		phys_addr_v2;
#endif
	unsigned		stride;
	unsigned		stride_uv;
	fixed20_12		x;
	fixed20_12		y;
	fixed20_12		w;
	fixed20_12		h;
	unsigned		out_x;
	unsigned		out_y;
	unsigned		out_w;
	unsigned		out_h;
	unsigned		z;

#if defined(CONFIG_TEGRA_CSC_V2)
	struct tegra_dc_csc_v2	csc;
#else
	struct tegra_dc_csc	csc;
#endif
	bool			csc_dirty;

	int			dirty;
	int			underflows;
	struct tegra_dc		*dc;

	struct nvmap_handle_ref	*cur_handle;
	unsigned		bandwidth;
	unsigned		new_bandwidth;
	struct tegra_dc_lut	lut;
#if defined(CONFIG_TEGRA_DC_BLOCK_LINEAR)
	u8	block_height_log2;
#endif
#if defined(CONFIG_TEGRA_DC_CDE)
	struct {
		dma_addr_t cde_addr;
		unsigned offset_x;
		unsigned offset_y;
		u32 zbc_color;
		unsigned ctb_entry;
	} cde;
#endif
	struct {
		u32			id;
		u32			min;
		u32			max;
	} syncpt;

	bool		is_scaler_coeff_set;
};

#define TEGRA_WIN_PPFLAG_CP_ENABLE	(1 << 0) /* enable RGB color lut */
#define TEGRA_WIN_PPFLAG_CP_FBOVERRIDE	(1 << 1) /* override fbdev color lut */

#define TEGRA_WIN_FLAG_ENABLED		(1 << 0)
#define TEGRA_WIN_FLAG_BLEND_PREMULT	(1 << 1)
#define TEGRA_WIN_FLAG_BLEND_COVERAGE	(1 << 2)
#define TEGRA_WIN_FLAG_INVERT_H		(1 << 3)
#define TEGRA_WIN_FLAG_INVERT_V		(1 << 4)
#define TEGRA_WIN_FLAG_TILED		(1 << 5)
#define TEGRA_WIN_FLAG_H_FILTER		(1 << 6)
#define TEGRA_WIN_FLAG_V_FILTER		(1 << 7)
#define TEGRA_WIN_FLAG_BLOCKLINEAR	(1 << 8)
#define TEGRA_WIN_FLAG_SCAN_COLUMN	(1 << 9)
#define TEGRA_WIN_FLAG_INTERLACE	(1 << 10)
#define TEGRA_WIN_FLAG_FB		(1 << 11)
#define TEGRA_WIN_FLAG_INPUT_RANGE_MASK	(3 << 12)
#define TEGRA_WIN_FLAG_INPUT_RANGE_FULL	(0 << 12)
#define TEGRA_WIN_FLAG_INPUT_RANGE_LIMITED	(1 << 12)
#define TEGRA_WIN_FLAG_INPUT_RANGE_BYPASS	(2 << 12)
#define TEGRA_WIN_FLAG_CS_MASK		(7 << 14)
#define TEGRA_WIN_FLAG_CS_DEFAULT	(0 << 14)
#define TEGRA_WIN_FLAG_CS_REC601	(1 << 14)
#define TEGRA_WIN_FLAG_CS_REC709	(2 << 14)
#define TEGRA_WIN_FLAG_CS_REC2020	(4 << 14)
#define TEGRA_WIN_FLAG_INVALID		(1 << 31) /* window does not exist. */

#define TEGRA_WIN_BLEND_FLAGS_MASK \
	(TEGRA_WIN_FLAG_BLEND_PREMULT | TEGRA_WIN_FLAG_BLEND_COVERAGE)

/* Note: These are the actual values written to the DC_WIN_COLOR_DEPTH register
 * and may change in new tegra architectures.
 */
#define TEGRA_WIN_FMT_P1			0
#define TEGRA_WIN_FMT_P2			1
#define TEGRA_WIN_FMT_P4			2
#define TEGRA_WIN_FMT_P8			3
#define TEGRA_WIN_FMT_B4G4R4A4			4
#define TEGRA_WIN_FMT_B5G5R5A			5
#define TEGRA_WIN_FMT_B5G6R5			6
#define TEGRA_WIN_FMT_AB5G5R5			7
#define TEGRA_WIN_FMT_T_R4G4B4A4		8
#define TEGRA_WIN_FMT_B8G8R8A8			12
#define TEGRA_WIN_FMT_R8G8B8A8			13
#define TEGRA_WIN_FMT_B6x2G6x2R6x2A8		14
#define TEGRA_WIN_FMT_R6x2G6x2B6x2A8		15
#define TEGRA_WIN_FMT_YCbCr422			16
#define TEGRA_WIN_FMT_YUV422			17
#define TEGRA_WIN_FMT_YCbCr420P			18
#define TEGRA_WIN_FMT_YUV420P			19
#define TEGRA_WIN_FMT_YCbCr422P			20
#define TEGRA_WIN_FMT_YUV422P			21
#define TEGRA_WIN_FMT_YCbCr422R			22
#define TEGRA_WIN_FMT_YUV422R			23
#define TEGRA_WIN_FMT_YCbCr422RA		24
#define TEGRA_WIN_FMT_YUV422RA			25
#define TEGRA_WIN_FMT_A8R8G8B8			35
#define TEGRA_WIN_FMT_A8B8G8R8			36
#define TEGRA_WIN_FMT_B8G8R8X8			37
#define TEGRA_WIN_FMT_R8G8B8X8			38
#define TEGRA_WIN_FMT_YCbCr444P			41
#define TEGRA_WIN_FMT_YUV444P			52
#define TEGRA_WIN_FMT_YCrCb420SP		42
#define TEGRA_WIN_FMT_YCbCr420SP		43
#define TEGRA_WIN_FMT_YCrCb422SP		44
#define TEGRA_WIN_FMT_YCbCr422SP		45
#define TEGRA_WIN_FMT_YCrCb422RSP		46
#define TEGRA_WIN_FMT_YCbCr422RSP		47
#define TEGRA_WIN_FMT_YCrCb444SP		48
#define TEGRA_WIN_FMT_YCbCr444SP		49
#define TEGRA_WIN_FMT_YVU420SP			53
#define TEGRA_WIN_FMT_YUV420SP			54
#define TEGRA_WIN_FMT_YVU422SP			55
#define TEGRA_WIN_FMT_YUV422SP			56
#define TEGRA_WIN_FMT_YVU444SP			59
#define TEGRA_WIN_FMT_YUV444SP			60
#define TEGRA_WIN_FMT_T_A2R10G10B10		70
#define TEGRA_WIN_FMT_T_A2B10G10R10             71
#define TEGRA_WIN_FMT_T_X2BL10GL10RL10_XRBIAS   72
#define TEGRA_WIN_FMT_T_X2BL10GL10RL10_XVYCC    73
#define TEGRA_WIN_FMT_T_R16_G16_B16_A16         75
#define TEGRA_WIN_FMT_T_Y10___U10___V10_N420    80
#define TEGRA_WIN_FMT_T_Y10___U10___V10_N444    82
#define TEGRA_WIN_FMT_T_Y10___V10U10_N420       83
#define TEGRA_WIN_FMT_T_Y10___U10V10_N422       84
#define TEGRA_WIN_FMT_T_Y10___U10V10_N422R      86
#define TEGRA_WIN_FMT_T_Y10___U10V10_N444       88
#define TEGRA_WIN_FMT_T_Y12___U12___V12_N420    96
#define TEGRA_WIN_FMT_T_Y12___U12___V12_N444    98
#define TEGRA_WIN_FMT_T_Y12___V12U12_N420       99
#define TEGRA_WIN_FMT_T_Y12___U12V12_N422       100
#define TEGRA_WIN_FMT_T_Y12___U12V12_N422R      102
#define TEGRA_WIN_FMT_T_Y12___U12V12_N444	104

/* Note: These values are not intended to be programmed directly into the
 * DC_WIN_COLOR_DEPTH register. They only signal formats possible through
 * additional parameters (mainly through the UV swap control).
 *
 * The values are chosen to prevent collisions with valid register values,
 * while fitting into the features 4 32b words bitmap (max value: 127).
 */
#define TEGRA_WIN_FMT_T_Y10___U10V10_N420	127
#define TEGRA_WIN_FMT_T_Y12___U12V12_N420	126

struct tegra_fb_data {
	int		win;

	int		xres;
	int		yres;
	int		bits_per_pixel; /* -1 means autodetect */

	unsigned long	flags;
};

#define TEGRA_FB_FLIP_ON_PROBE		(1 << 0)

struct tegra_dc_platform_data {
	unsigned long		flags;
	unsigned long		emc_clk_rate;
	struct tegra_dc_out	*default_out;
	struct tegra_fb_data	*fb;
	unsigned long		low_v_win;

#if defined(CONFIG_TEGRA_DC_CMU) || defined(CONFIG_TEGRA_DC_CMU_V2)
	bool			cmu_enable;
	struct tegra_dc_cmu	*cmu;
	struct tegra_dc_cmu	*cmu_adbRGB;
	int			default_clr_space;
#endif
	unsigned long		ctrl_num;
	unsigned long		win_mask;
};

struct tegra_dc_bw_data {
	u32	total_bw;
	u32	avail_bw;
	u32	resvd_bw;
};

#define TEGRA_DC_FLAG_ENABLED		(1 << 0)
#define TEGRA_DC_FLAG_SET_EARLY_MODE		(1 << 1)

struct drm_mode_modeinfo;

int tegra_dc_get_stride(struct tegra_dc *dc, unsigned win);
struct tegra_dc *tegra_dc_get_dc(unsigned idx);
struct tegra_dc_win *tegra_dc_get_window(struct tegra_dc *dc, unsigned win);
bool tegra_dc_get_connected(struct tegra_dc *);
bool tegra_dc_hpd(struct tegra_dc *dc);


bool tegra_dc_has_vsync(struct tegra_dc *dc);
int tegra_dc_vsync_enable(struct tegra_dc *dc);
void tegra_dc_vsync_disable(struct tegra_dc *dc);
int tegra_dc_wait_for_vsync(struct tegra_dc *dc);
void tegra_dc_blank(struct tegra_dc *dc, unsigned windows);
int tegra_dc_restore(struct tegra_dc *dc);

void tegra_dc_enable(struct tegra_dc *dc);
void tegra_dc_disable(struct tegra_dc *dc);
int tegra_dc_set_default_videomode(struct tegra_dc *dc);


u32 tegra_dc_get_syncpt_id(struct tegra_dc *dc, int i);
u32 tegra_dc_incr_syncpt_max(struct tegra_dc *dc, int i);
void tegra_dc_incr_syncpt_min(struct tegra_dc *dc, int i, u32 val);
struct sync_fence *tegra_dc_create_fence(struct tegra_dc *dc, int i, u32 val);

/* tegra_dc_update_windows and tegra_dc_sync_windows do not support windows
 * with differenct dcs in one call
 * dirty_rect is u16[4]: xoff, yoff, width, height
 */
int tegra_dc_update_windows(struct tegra_dc_win *windows[], int n,
	u16 *dirty_rect, bool wait_for_vblank);
int tegra_dc_sync_windows(struct tegra_dc_win *windows[], int n);
void tegra_dc_disable_window(struct tegra_dc *dc, unsigned win);
int tegra_dc_attach_win(struct tegra_dc *dc, unsigned idx);
int tegra_dc_dettach_win(struct tegra_dc *dc, unsigned idx);
int tegra_dc_config_frame_end_intr(struct tegra_dc *dc, bool enable);
bool tegra_dc_is_within_n_vsync(struct tegra_dc *dc, s64 ts);
bool tegra_dc_does_vsync_separate(struct tegra_dc *dc, s64 new_ts, s64 old_ts);

int tegra_dc_set_mode(struct tegra_dc *dc, const struct tegra_dc_mode *mode);
struct fb_videomode;
int tegra_dc_to_fb_videomode(struct fb_videomode *fbmode,
	const struct tegra_dc_mode *mode);
int tegra_dc_set_drm_mode(struct tegra_dc *dc,
	const struct drm_mode_modeinfo *dmode, bool stereo_mode);
int tegra_dc_set_fb_mode(struct tegra_dc *dc, const struct fb_videomode *fbmode,
	bool stereo_mode);

unsigned tegra_dc_get_out_height(const struct tegra_dc *dc);
unsigned tegra_dc_get_out_width(const struct tegra_dc *dc);
unsigned tegra_dc_get_out_max_pixclock(const struct tegra_dc *dc);
#ifdef CONFIG_TEGRA_NVDISPLAY
void tegra_sd_check_prism_thresh(struct device *dev, int brightness);
void tegra_sd_enbl_dsbl_prism(struct device *dev, bool status);
#else
void nvsd_check_prism_thresh(struct device *dev, int brightness);
void nvsd_enbl_dsbl_prism(struct device *dev, bool status);
#endif

/* PM0 and PM1 signal control */
#define TEGRA_PWM_PM0 0
#define TEGRA_PWM_PM1 1

struct tegra_dc_pwm_params {
	int which_pwm;
	int gpio_conf_to_sfio;
	unsigned int period;
	unsigned int clk_div;
	unsigned int clk_select;
	unsigned int duty_cycle;
};

void tegra_dc_config_pwm(struct tegra_dc *dc, struct tegra_dc_pwm_params *cfg);

int tegra_dsi_send_panel_short_cmd(struct tegra_dc *dc, u8 *pdata, u8 data_len);

#if defined(CONFIG_TEGRA_CSC_V2)
int tegra_nvdisp_update_csc(struct tegra_dc *dc, int win_index);
#else
int tegra_dc_update_csc(struct tegra_dc *dc, int win_index);
#endif

int tegra_dc_update_lut(struct tegra_dc *dc, int win_index, int fboveride);

/*
 * In order to get a dc's current EDID, first call tegra_dc_get_edid() from an
 * interruptible context.  The returned value (if non-NULL) points to a
 * snapshot of the current state; after copying data from it, call
 * tegra_dc_put_edid() on that pointer.  Do not dereference anything through
 * that pointer after calling tegra_dc_put_edid().
 */
struct tegra_dc_edid {
	size_t		len;
	u8		buf[0];
};
struct tegra_dc_edid *tegra_dc_get_edid(struct tegra_dc *dc);
void tegra_dc_put_edid(struct tegra_dc_edid *edid);

int tegra_dc_set_flip_callback(void (*callback)(void));
int tegra_dc_unset_flip_callback(void);
int tegra_dc_get_panel_sync_rate(void);

int tegra_dc_get_head(const struct tegra_dc *dc);
int tegra_dc_get_out(const struct tegra_dc *dc);

struct device_node *tegra_primary_panel_get_dt_node(
				struct tegra_dc_platform_data *pdata);
struct device_node *tegra_secondary_panel_get_dt_node(
				struct tegra_dc_platform_data *pdata);
#if defined(CONFIG_TEGRA_NVDISPLAY)
struct device_node *tegra_tertiary_panel_get_dt_node(
				struct tegra_dc_platform_data *pdata);
#else
static inline struct device_node *tegra_tertiary_panel_get_dt_node(
				struct tegra_dc_platform_data *pdata)
{
	return NULL;
}
#endif
bool tegra_is_bl_display_initialized(int instance);

#ifdef CONFIG_TEGRA_COMMON
void find_dc_node(struct device_node **dc1_node,
				struct device_node **dc2_node);
#else
/* stub for panel files that rely on this function
 * in kernel/.../drivers/platform/tegra/common.c,
 * which isn't compiled for t186.
 */
static inline void find_dc_node(struct device_node **dc1_node,
	struct device_node **dc2_node)
{
	pr_err("%s: function is unimplemented\n", __func__);
}
#endif

void tegra_get_fb_resource(struct resource *fb_res);
void tegra_get_fb2_resource(struct resource *fb2_res);
unsigned tegra_dc_out_flags_from_dev(struct device *dev);
bool tegra_dc_initialized(struct device *dev);
bool tegra_dc_is_ext_dp_panel(const struct tegra_dc *dc);

/* table of electrical settings, must be in acending order. */
struct tmds_config {
	u32 version;	/* MAJOR, MINOR */
	int pclk;
	u32 pll0;
	u32 pll1;
	u32 pe_current; /* pre-emphasis */
	u32 drive_current;
	u32 peak_current; /* for TEGRA_11x_SOC */
	u32 pad_ctls0_mask; /* register AND mask */
	u32 pad_ctls0_setting; /* register OR mask */
};

struct tegra_hdmi_out {
	struct tmds_config *tmds_config;
	int n_tmds_config;
	bool hdmi2fpd_bridge_enable;
	bool hdmi2gmsl_bridge_enable;
};

enum {
	DRIVE_CURRENT_L0 = 0,
	DRIVE_CURRENT_L1 = 1,
	DRIVE_CURRENT_L2 = 2,
	DRIVE_CURRENT_L3 = 3,
};

enum {
	PRE_EMPHASIS_L0 = 0,
	PRE_EMPHASIS_L1 = 1,
	PRE_EMPHASIS_L2 = 2,
	PRE_EMPHASIS_L3 = 3,
};

enum {
	POST_CURSOR2_L0 = 0,
	POST_CURSOR2_L1 = 1,
	POST_CURSOR2_L2 = 2,
	POST_CURSOR2_L3 = 3,
};

enum {
	SOR_LINK_SPEED_G1_62 = 6,
	SOR_LINK_SPEED_G2_7 = 10,
	SOR_LINK_SPEED_G5_4 = 20,
	SOR_LINK_SPEED_LVDS = 7,
};

struct tegra_dc_dp_lt_settings {
	u32 drive_current[4]; /* Entry for each lane */
	u32 lane_preemphasis[4]; /* Entry for each lane */
	u32 post_cursor[4]; /* Entry for each lane */
	u32 tx_pu;
	u32 load_adj;
};

enum {
	DP_VS = 0,
	DP_PE = 1,
	DP_PC = 2,
	DP_TX_PU = 3,
};

struct tegra_dc_lt_data {
	u32 data[4][4][4];
	const char *name;
};

struct tegra_dp_out {
	struct tegra_dc_dp_lt_settings *lt_settings;
	int n_lt_settings;
	bool tx_pu_disable;
	int lanes;
	u8 link_bw;
	bool hdmi2fpd_bridge_enable;
	struct tegra_dc_lt_data *lt_data;
	int n_lt_data;
};

/*
 * On T18x, isohub is the only memclient that's relevant to display.
 * This struct keeps track of its isoclient info.
 */
#if defined(CONFIG_TEGRA_NVDISPLAY) && defined(CONFIG_TEGRA_ISOMGR)
struct nvdisp_isoclient_bw_info {
	tegra_isomgr_handle	isomgr_handle;
	u32			available_bw;
	u32			reserved_bw_kbps;
	u32			new_bw_kbps;
};
#endif
#endif
