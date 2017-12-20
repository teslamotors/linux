/*
 * drivers/video/tegra/dc/dsi.c
 *
 * Copyright (c) 2011, NVIDIA Corporation.
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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/wakelock.h>
#include <linux/switch.h>
#include <linux/workqueue.h>

#include <mach/clk.h>
#include <mach/dc.h>
#include <mach/fb.h>
#include <mach/csi.h>
#include <linux/nvhost.h>
#include <mach/gpio-names.h>

#include "dc_reg.h"
#include "dc_priv.h"
#include "dsi_regs.h"
#include "dsi.h"

#define DSI_USE_SYNC_POINTS 1
#define S_TO_MS(x)		1000

#define DSI_MODULE_NOT_INIT		0x0
#define DSI_MODULE_INIT			0x1

#define DSI_LPHS_NOT_INIT		0x0
#define DSI_LPHS_IN_LP_MODE		0x1
#define DSI_LPHS_IN_HS_MODE		0x2

#define DSI_VIDEO_TYPE_NOT_INIT		0x0
#define DSI_VIDEO_TYPE_VIDEO_MODE	0x1
#define DSI_VIDEO_TYPE_CMD_MODE		0x2

#define DSI_DRIVEN_MODE_NOT_INIT	0x0
#define DSI_DRIVEN_MODE_DC		0x1
#define DSI_DRIVEN_MODE_HOST		0x2

#define DSI_PHYCLK_OUT_DIS		0x0
#define DSI_PHYCLK_OUT_EN		0x1

#define DSI_PHYCLK_NOT_INIT		0x0
#define DSI_PHYCLK_CONTINUOUS		0x1
#define DSI_PHYCLK_TX_ONLY		0x2

#define DSI_CLK_BURST_NOT_INIT		0x0
#define DSI_CLK_BURST_NONE_BURST	0x1
#define DSI_CLK_BURST_BURST_MODE	0x2

#define DSI_DC_STREAM_DISABLE		0x0
#define DSI_DC_STREAM_ENABLE		0x1

#define DSI_LP_OP_NOT_INIT		0x0
#define DSI_LP_OP_WRITE			0x1
#define DSI_LP_OP_READ			0x2

struct dsi_status {
	unsigned	init:2;

	unsigned	lphs:2;

	unsigned	vtype:2;
	unsigned	driven:2;

	unsigned	clk_out:2;
	unsigned	clk_mode:2;
	unsigned	clk_burst:2;

	unsigned	lp_op:2;

	unsigned	dc_stream:1;
};

/* source of video data */
enum{
	TEGRA_DSI_DRIVEN_BY_DC,
	TEGRA_DSI_DRIVEN_BY_HOST,
};

struct tegra_dc_dsi_data {
	struct tegra_dc	*dc;
	void __iomem	*base;
	struct resource	*base_res;

	struct clk		*dc_clk;
	struct clk		*dsi_clk;
	bool			clk_ref;

	struct mutex	lock;

	/* data from board info */
	struct tegra_dsi_out info;

	struct dsi_status	status;

	u8		driven_mode;
	u8		controller_index;

	u8		pixel_scaler_mul;
	u8		pixel_scaler_div;

	u32		default_shift_clk_div;
	u32		default_pixel_clk_khz;
	u32		default_hs_clk_khz;

	u32		shift_clk_div;
	u32		target_hs_clk_khz;
	u32		target_lp_clk_khz;

	u32		syncpt_id;
	u32		syncpt_val;

	u16		current_bit_clk_ns;
	u32		current_dsi_clk_khz;

	u32		dsi_control_val;

	bool		ulpm;
	bool		enabled;
};

const u32 dsi_pkt_seq_reg[NUMOF_PKT_SEQ] = {
	DSI_PKT_SEQ_0_LO,
	DSI_PKT_SEQ_0_HI,
	DSI_PKT_SEQ_1_LO,
	DSI_PKT_SEQ_1_HI,
	DSI_PKT_SEQ_2_LO,
	DSI_PKT_SEQ_2_HI,
	DSI_PKT_SEQ_3_LO,
	DSI_PKT_SEQ_3_HI,
	DSI_PKT_SEQ_4_LO,
	DSI_PKT_SEQ_4_HI,
	DSI_PKT_SEQ_5_LO,
	DSI_PKT_SEQ_5_HI,
};

const u32 dsi_pkt_seq_video_non_burst_syne[NUMOF_PKT_SEQ] = {
	PKT_ID0(CMD_VS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(0) | PKT_LP,
	0,
	PKT_ID0(CMD_VE) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_BLNK) | PKT_LEN1(1) |
		PKT_ID2(CMD_HE) | PKT_LEN2(0),
	PKT_ID3(CMD_BLNK) | PKT_LEN3(2) | PKT_ID4(CMD_RGB) | PKT_LEN4(3) |
		PKT_ID5(CMD_BLNK) | PKT_LEN5(4),
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_BLNK) | PKT_LEN1(1) |
		PKT_ID2(CMD_HE) | PKT_LEN2(0),
	PKT_ID3(CMD_BLNK) | PKT_LEN3(2) | PKT_ID4(CMD_RGB) | PKT_LEN4(3) |
		PKT_ID5(CMD_BLNK) | PKT_LEN5(4),
};

const u32 dsi_pkt_seq_video_non_burst[NUMOF_PKT_SEQ] = {
	PKT_ID0(CMD_VS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_BLNK) | PKT_LEN1(2) |
		PKT_ID2(CMD_RGB) | PKT_LEN2(3),
	PKT_ID3(CMD_BLNK) | PKT_LEN3(4),
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_BLNK) | PKT_LEN1(2) |
		PKT_ID2(CMD_RGB) | PKT_LEN2(3),
	PKT_ID3(CMD_BLNK) | PKT_LEN3(4),
};

static const u32 dsi_pkt_seq_video_burst[NUMOF_PKT_SEQ] = {
	PKT_ID0(CMD_VS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(7) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(7) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(7) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_BLNK) | PKT_LEN1(2)|
	PKT_ID2(CMD_RGB) | PKT_LEN2(3) | PKT_LP,
	PKT_ID0(CMD_EOT) | PKT_LEN0(7),
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(7) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_BLNK) | PKT_LEN1(2)|
	PKT_ID2(CMD_RGB) | PKT_LEN2(3) | PKT_LP,
	PKT_ID0(CMD_EOT) | PKT_LEN0(7),
};

static const u32 dsi_pkt_seq_video_burst_no_eot[NUMOF_PKT_SEQ] = {
	PKT_ID0(CMD_VS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_BLNK) | PKT_LEN1(2)|
	PKT_ID2(CMD_RGB) | PKT_LEN2(3) | PKT_LP,
	PKT_ID0(CMD_EOT) | PKT_LEN0(0),
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_BLNK) | PKT_LEN1(2)|
	PKT_ID2(CMD_RGB) | PKT_LEN2(3) | PKT_LP,
	PKT_ID0(CMD_EOT) | PKT_LEN0(0),
};

/* TODO: verify with hw about this format */
const u32 dsi_pkt_seq_cmd_mode[NUMOF_PKT_SEQ] = {
	0,
	0,
	0,
	0,
	0,
	0,
	PKT_ID0(CMD_LONGW) | PKT_LEN0(3) | PKT_ID1(CMD_EOT) | PKT_LEN1(7),
	0,
	0,
	0,
	PKT_ID0(CMD_LONGW) | PKT_LEN0(3) | PKT_ID1(CMD_EOT) | PKT_LEN1(7),
	0,
};

const u32 init_reg[] = {
	DSI_INT_ENABLE,
	DSI_INT_STATUS,
	DSI_INT_MASK,
	DSI_INIT_SEQ_DATA_0,
	DSI_INIT_SEQ_DATA_1,
	DSI_INIT_SEQ_DATA_2,
	DSI_INIT_SEQ_DATA_3,
	DSI_INIT_SEQ_DATA_4,
	DSI_INIT_SEQ_DATA_5,
	DSI_INIT_SEQ_DATA_6,
	DSI_INIT_SEQ_DATA_7,
	DSI_DCS_CMDS,
	DSI_PKT_SEQ_0_LO,
	DSI_PKT_SEQ_1_LO,
	DSI_PKT_SEQ_2_LO,
	DSI_PKT_SEQ_3_LO,
	DSI_PKT_SEQ_4_LO,
	DSI_PKT_SEQ_5_LO,
	DSI_PKT_SEQ_0_HI,
	DSI_PKT_SEQ_1_HI,
	DSI_PKT_SEQ_2_HI,
	DSI_PKT_SEQ_3_HI,
	DSI_PKT_SEQ_4_HI,
	DSI_PKT_SEQ_5_HI,
	DSI_CONTROL,
	DSI_HOST_DSI_CONTROL,
	DSI_PAD_CONTROL,
	DSI_PAD_CONTROL_CD,
	DSI_SOL_DELAY,
	DSI_MAX_THRESHOLD,
	DSI_TRIGGER,
	DSI_TX_CRC,
	DSI_INIT_SEQ_CONTROL,
	DSI_PKT_LEN_0_1,
	DSI_PKT_LEN_2_3,
	DSI_PKT_LEN_4_5,
	DSI_PKT_LEN_6_7,
};

inline unsigned long tegra_dsi_readl(struct tegra_dc_dsi_data *dsi, u32 reg)
{
	return readl(dsi->base + reg * 4);
}
EXPORT_SYMBOL(tegra_dsi_readl);

inline void tegra_dsi_writel(struct tegra_dc_dsi_data *dsi, u32 val, u32 reg)
{
	writel(val, dsi->base + reg * 4);
}
EXPORT_SYMBOL(tegra_dsi_writel);

static int tegra_dsi_syncpt(struct tegra_dc_dsi_data *dsi)
{
	u32 val;
	int ret;

	ret = 0;

	dsi->syncpt_val = nvhost_syncpt_read(
			&dsi->dc->ndev->host->syncpt, dsi->syncpt_id);

	val = DSI_INCR_SYNCPT_COND(OP_DONE) |
		DSI_INCR_SYNCPT_INDX(dsi->syncpt_id);
	tegra_dsi_writel(dsi, val, DSI_INCR_SYNCPT);

	/* TODO: Use interrupt rather than polling */
	ret = nvhost_syncpt_wait(&dsi->dc->ndev->host->syncpt,
		dsi->syncpt_id, dsi->syncpt_val + 1);
	if (ret < 0) {
		printk(KERN_ERR "DSI sync point failure\n");
		goto fail;
	}

	(dsi->syncpt_val)++;
	return 0;
fail:
	return ret;
}

static u32 tegra_dsi_get_hs_clk_rate(struct tegra_dc_dsi_data *dsi)
{
	u32 dsi_clock_rate_khz;

	switch (dsi->info.video_burst_mode) {
	case TEGRA_DSI_VIDEO_BURST_MODE_LOW_SPEED:
	case TEGRA_DSI_VIDEO_BURST_MODE_MEDIUM_SPEED:
	case TEGRA_DSI_VIDEO_BURST_MODE_FAST_SPEED:
	case TEGRA_DSI_VIDEO_BURST_MODE_FASTEST_SPEED:
		/* Calculate DSI HS clock rate for DSI burst mode */
		dsi_clock_rate_khz = dsi->default_pixel_clk_khz *
							dsi->shift_clk_div;
		break;
	case TEGRA_DSI_VIDEO_NONE_BURST_MODE:
	case TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END:
	case TEGRA_DSI_VIDEO_BURST_MODE_LOWEST_SPEED:
	default:
		/* Clock rate is default DSI clock rate for non-burst mode */
		dsi_clock_rate_khz = dsi->default_hs_clk_khz;
		break;
	}

	return dsi_clock_rate_khz;
}

static u32 tegra_dsi_get_lp_clk_rate(struct tegra_dc_dsi_data *dsi, u8 lp_op)
{
	u32 dsi_clock_rate_khz;

	if (dsi->info.enable_hs_clock_on_lp_cmd_mode)
		if (dsi->info.hs_clk_in_lp_cmd_mode_freq_khz)
			dsi_clock_rate_khz =
				dsi->info.hs_clk_in_lp_cmd_mode_freq_khz;
		else
			dsi_clock_rate_khz = tegra_dsi_get_hs_clk_rate(dsi);
	else
		if (lp_op == DSI_LP_OP_READ)
			dsi_clock_rate_khz =
				dsi->info.lp_read_cmd_mode_freq_khz;
		else
			dsi_clock_rate_khz =
				dsi->info.lp_cmd_mode_freq_khz;

	return dsi_clock_rate_khz;
}

static u32 tegra_dsi_get_shift_clk_div(struct tegra_dc_dsi_data *dsi)
{
	u32 shift_clk_div;
	u32 max_shift_clk_div;
	u32 burst_width;
	u32 burst_width_max;

	/* Get the real value of default shift_clk_div. default_shift_clk_div
	 * holds the real value of shift_clk_div. */
	shift_clk_div = dsi->default_shift_clk_div;

	/* Calculate shift_clk_div which can matche the video_burst_mode.*/
	if (dsi->info.video_burst_mode >=
			TEGRA_DSI_VIDEO_BURST_MODE_LOWEST_SPEED) {
		/* The max_shift_clk_div is multiplied by 10 to save the
		 * fraction */
		if (dsi->info.max_panel_freq_khz >= dsi->default_hs_clk_khz)
			max_shift_clk_div = dsi->info.max_panel_freq_khz
				* shift_clk_div * 10 / dsi->default_hs_clk_khz;
		else
			max_shift_clk_div = shift_clk_div * 10;

		burst_width = dsi->info.video_burst_mode
				- TEGRA_DSI_VIDEO_BURST_MODE_LOWEST_SPEED;
		burst_width_max = TEGRA_DSI_VIDEO_BURST_MODE_FASTEST_SPEED
				- TEGRA_DSI_VIDEO_BURST_MODE_LOWEST_SPEED;

		shift_clk_div = (max_shift_clk_div - shift_clk_div * 10) *
			burst_width / (burst_width_max * 10) + shift_clk_div;
	}

	return shift_clk_div;
}

static void tegra_dsi_init_sw(struct tegra_dc *dc,
						struct tegra_dc_dsi_data *dsi)
{
	u32 h_width_pixels;
	u32 v_width_lines;
	u32 pixel_clk_hz;
	u32 byte_clk_hz;
	u32 plld_clk_mhz;

	switch (dsi->info.pixel_format) {
	case TEGRA_DSI_PIXEL_FORMAT_16BIT_P:
		/* 2 bytes per pixel */
		dsi->pixel_scaler_mul = 2;
		dsi->pixel_scaler_div = 1;
		break;
	case TEGRA_DSI_PIXEL_FORMAT_18BIT_P:
		/* 2.25 bytes per pixel */
		dsi->pixel_scaler_mul = 9;
		dsi->pixel_scaler_div = 4;
		break;
	case TEGRA_DSI_PIXEL_FORMAT_18BIT_NP:
	case TEGRA_DSI_PIXEL_FORMAT_24BIT_P:
		/* 3 bytes per pixel */
		dsi->pixel_scaler_mul = 3;
		dsi->pixel_scaler_div = 1;
		break;
	default:
		break;
	}

	dsi->controller_index = dc->ndev->id;
	dsi->ulpm = false;
	dsi->enabled = false;
	dsi->clk_ref = false;

	dsi->dsi_control_val =
			DSI_CONTROL_VIRTUAL_CHANNEL(dsi->info.virtual_channel) |
			DSI_CONTROL_NUM_DATA_LANES(dsi->info.n_data_lanes - 1) |
			DSI_CONTROL_VID_SOURCE(dsi->controller_index) |
			DSI_CONTROL_DATA_FORMAT(dsi->info.pixel_format);

	/* Below we are going to calculate dsi and dc clock rate.
	 * Calcuate the horizontal and vertical width. */
	h_width_pixels = dc->mode.h_back_porch + dc->mode.h_front_porch +
			dc->mode.h_sync_width + dc->mode.h_active;
	v_width_lines = dc->mode.v_back_porch + dc->mode.v_front_porch +
			dc->mode.v_sync_width + dc->mode.v_active;

	/* Calculate minimum required pixel rate. */
	pixel_clk_hz = h_width_pixels * v_width_lines * dsi->info.refresh_rate;

	dc->pixel_clk = pixel_clk_hz;

	/* Calculate minimum byte rate on DSI interface. */
	byte_clk_hz = (pixel_clk_hz * dsi->pixel_scaler_mul) /
			(dsi->pixel_scaler_div * dsi->info.n_data_lanes);

	/* Round up to multiple of mega hz. */
	plld_clk_mhz = DIV_ROUND_UP((byte_clk_hz * NUMOF_BIT_PER_BYTE),
								1000000);

	/* Calculate default real shift_clk_div. */
	dsi->default_shift_clk_div = (NUMOF_BIT_PER_BYTE / 2) *
		dsi->pixel_scaler_mul / (dsi->pixel_scaler_div *
		dsi->info.n_data_lanes);
	/* Calculate default DSI hs clock. DSI interface is double data rate.
	 * Data is transferred on both rising and falling edge of clk, div by 2
	 * to get the actual clock rate.
	 * */
	dsi->default_hs_clk_khz = plld_clk_mhz * 1000 / 2;
	dsi->default_pixel_clk_khz = plld_clk_mhz * 1000 / 2
						/ dsi->default_shift_clk_div;

	/* Get the actual shift_clk_div and clock rates. */
	dsi->shift_clk_div = tegra_dsi_get_shift_clk_div(dsi);
	dsi->target_lp_clk_khz = tegra_dsi_get_lp_clk_rate(dsi, DSI_LP_OP_WRITE);
	dsi->target_hs_clk_khz = tegra_dsi_get_hs_clk_rate(dsi);

	dev_info(&dc->ndev->dev, "DSI: HS clock rate is %d\n",
							dsi->target_hs_clk_khz);

	dsi->controller_index = dc->ndev->id;

#if DSI_USE_SYNC_POINTS
	dsi->syncpt_id = NVSYNCPT_DSI;
#endif

	/*
	 * Force video clock to be continuous mode if
	 * enable_hs_clock_on_lp_cmd_mode is set
	 */
	if (dsi->info.enable_hs_clock_on_lp_cmd_mode) {
		if (dsi->info.video_clock_mode !=
					TEGRA_DSI_VIDEO_CLOCK_CONTINUOUS)
			printk("Force to clock continuous mode\n");

		dsi->info.video_clock_mode = TEGRA_DSI_VIDEO_CLOCK_CONTINUOUS;
	}

}

static void tegra_dsi_get_phy_timing(struct tegra_dc_dsi_data *dsi,
				struct dsi_phy_timing_inclk *phy_timing_clk,
				u32 clk_ns)
{

	phy_timing_clk->t_hsdexit = dsi->info.phy_timing.t_hsdexit_ns ?
			(dsi->info.phy_timing.t_hsdexit_ns / clk_ns) :
			(T_HSEXIT_DEFAULT(clk_ns));

	phy_timing_clk->t_hstrail = dsi->info.phy_timing.t_hstrail_ns ?
			(dsi->info.phy_timing.t_hstrail_ns / clk_ns) :
			(T_HSTRAIL_DEFAULT(clk_ns));

	phy_timing_clk->t_datzero = dsi->info.phy_timing.t_datzero_ns ?
			(dsi->info.phy_timing.t_datzero_ns / clk_ns) :
			(T_DATZERO_DEFAULT(clk_ns));

	phy_timing_clk->t_hsprepr = dsi->info.phy_timing.t_hsprepr_ns ?
			(dsi->info.phy_timing.t_hsprepr_ns / clk_ns) :
			(T_HSPREPR_DEFAULT(clk_ns));

	phy_timing_clk->t_clktrail = dsi->info.phy_timing.t_clktrail_ns ?
				(dsi->info.phy_timing.t_clktrail_ns / clk_ns) :
				(T_CLKTRAIL_DEFAULT(clk_ns));

	phy_timing_clk->t_clkpost = dsi->info.phy_timing.t_clkpost_ns ?
				(dsi->info.phy_timing.t_clkpost_ns / clk_ns) :
				(T_CLKPOST_DEFAULT(clk_ns));

	phy_timing_clk->t_clkzero = dsi->info.phy_timing.t_clkzero_ns ?
				(dsi->info.phy_timing.t_clkzero_ns / clk_ns) :
				(T_CLKZERO_DEFAULT(clk_ns));

	phy_timing_clk->t_tlpx = dsi->info.phy_timing.t_tlpx_ns ?
				(dsi->info.phy_timing.t_tlpx_ns / clk_ns) :
				(T_TLPX_DEFAULT(clk_ns));

	phy_timing_clk->t_clkpre = T_CLKPRE_DEFAULT(clk_ns);
	phy_timing_clk->t_clkprepare = T_CLKPREPARE_DEFAULT(clk_ns);
	phy_timing_clk->t_wakeup = T_WAKEUP_DEFAULT(clk_ns);

	phy_timing_clk->t_taget = 5 * phy_timing_clk->t_tlpx;
	phy_timing_clk->t_tasure = 2 * phy_timing_clk->t_tlpx;
	phy_timing_clk->t_tago = 4 * phy_timing_clk->t_tlpx;
}

static void tegra_dsi_set_phy_timing(struct tegra_dc_dsi_data *dsi)
{
	u32 val;
	struct dsi_phy_timing_inclk	phy_timing;

	tegra_dsi_get_phy_timing(dsi, &phy_timing, dsi->current_bit_clk_ns);

	val = DSI_PHY_TIMING_0_THSDEXIT(phy_timing.t_hsdexit) |
			DSI_PHY_TIMING_0_THSTRAIL(phy_timing.t_hstrail) |
			DSI_PHY_TIMING_0_TDATZERO(phy_timing.t_datzero) |
			DSI_PHY_TIMING_0_THSPREPR(phy_timing.t_hsprepr);
	tegra_dsi_writel(dsi, val, DSI_PHY_TIMING_0);

	val = DSI_PHY_TIMING_1_TCLKTRAIL(phy_timing.t_clktrail) |
			DSI_PHY_TIMING_1_TCLKPOST(phy_timing.t_clkpost) |
			DSI_PHY_TIMING_1_TCLKZERO(phy_timing.t_clkzero) |
			DSI_PHY_TIMING_1_TTLPX(phy_timing.t_tlpx);
	tegra_dsi_writel(dsi, val, DSI_PHY_TIMING_1);

	val = DSI_PHY_TIMING_2_TCLKPREPARE(phy_timing.t_clkprepare) |
		DSI_PHY_TIMING_2_TCLKPRE(phy_timing.t_clkpre) |
			DSI_PHY_TIMING_2_TWAKEUP(phy_timing.t_wakeup);
	tegra_dsi_writel(dsi, val, DSI_PHY_TIMING_2);

	val = DSI_BTA_TIMING_TTAGET(phy_timing.t_taget) |
			DSI_BTA_TIMING_TTASURE(phy_timing.t_tasure) |
			DSI_BTA_TIMING_TTAGO(phy_timing.t_tago);
	tegra_dsi_writel(dsi, val, DSI_BTA_TIMING);
}

static u32 tegra_dsi_sol_delay_burst(struct tegra_dc *dc,
						struct tegra_dc_dsi_data *dsi)
{
	u32 dsi_to_pixel_clk_ratio;
	u32 temp;
	u32 temp1;
	u32 mipi_clk_adj_kHz;
	u32 sol_delay;
	struct tegra_dc_mode *dc_modes = &dc->mode;

	/* Get Fdsi/Fpixel ration (note: Fdsi si in bit format) */
	dsi_to_pixel_clk_ratio = (dsi->current_dsi_clk_khz * 2 +
		dsi->default_pixel_clk_khz - 1) / dsi->default_pixel_clk_khz;

	/* Convert Fdsi to byte format */
	dsi_to_pixel_clk_ratio *= 1000/8;

	/* Multiplying by 1000 so that we don't loose the fraction part */
	temp = dc_modes->h_active * 1000;
	temp1 = dc_modes->h_active + dc_modes->h_back_porch +
			dc_modes->h_sync_width;

	sol_delay = temp1 * dsi_to_pixel_clk_ratio -
			temp * dsi->pixel_scaler_mul /
			(dsi->pixel_scaler_div * dsi->info.n_data_lanes);

	/* Do rounding on sol delay */
	sol_delay = (sol_delay + 1000 - 1)/1000;

	/* TODO:
	 * 1. find out the correct sol fifo depth to use
	 * 2. verify with hw about the clamping function
	 */
	if (sol_delay > (480 * 4)) {
		sol_delay = (480 * 4);
		mipi_clk_adj_kHz = sol_delay +
			(dc_modes->h_active * dsi->pixel_scaler_mul) /
			(dsi->info.n_data_lanes * dsi->pixel_scaler_div);

		mipi_clk_adj_kHz *= (dsi->default_pixel_clk_khz / temp1);

		mipi_clk_adj_kHz *= 4;
	}

	dsi->target_hs_clk_khz = mipi_clk_adj_kHz;

	return sol_delay;
}

static void tegra_dsi_set_sol_delay(struct tegra_dc *dc,
						struct tegra_dc_dsi_data *dsi)
{
	u32 sol_delay;

	if (dsi->info.video_burst_mode == TEGRA_DSI_VIDEO_NONE_BURST_MODE ||
		dsi->info.video_burst_mode ==
				TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END) {
		sol_delay = NUMOF_BIT_PER_BYTE * dsi->pixel_scaler_mul /
			(dsi->pixel_scaler_div * dsi->info.n_data_lanes);
		dsi->status.clk_burst = DSI_CLK_BURST_NONE_BURST;
	} else {
		sol_delay = tegra_dsi_sol_delay_burst(dc, dsi);
		dsi->status.clk_burst = DSI_CLK_BURST_BURST_MODE;
	}

	tegra_dsi_writel(dsi, DSI_SOL_DELAY_SOL_DELAY(sol_delay),
								DSI_SOL_DELAY);
}

static void tegra_dsi_set_timeout(struct tegra_dc_dsi_data *dsi)
{
	u32 val;
	u32 bytes_per_frame;
	u32 timeout = 0;

	/* TODO: verify the following eq */
	bytes_per_frame = dsi->current_dsi_clk_khz * 1000 * 2 /
						(dsi->info.refresh_rate * 8);
	timeout = bytes_per_frame / DSI_CYCLE_COUNTER_VALUE;
	timeout = (timeout + DSI_HTX_TO_MARGIN) & 0xffff;

	val = DSI_TIMEOUT_0_LRXH_TO(DSI_LRXH_TO_VALUE) |
			DSI_TIMEOUT_0_HTX_TO(timeout);
	tegra_dsi_writel(dsi, val, DSI_TIMEOUT_0);

	if (dsi->info.panel_reset_timeout_msec)
		timeout = (dsi->info.panel_reset_timeout_msec * 1000*1000)
					/ dsi->current_bit_clk_ns;
	else
		timeout = DSI_PR_TO_VALUE;

	val = DSI_TIMEOUT_1_PR_TO(timeout) |
		DSI_TIMEOUT_1_TA_TO(DSI_TA_TO_VALUE);
	tegra_dsi_writel(dsi, val, DSI_TIMEOUT_1);

	val = DSI_TO_TALLY_P_RESET_STATUS(IN_RESET) |
		DSI_TO_TALLY_TA_TALLY(DSI_TA_TALLY_VALUE)|
		DSI_TO_TALLY_LRXH_TALLY(DSI_LRXH_TALLY_VALUE)|
		DSI_TO_TALLY_HTX_TALLY(DSI_HTX_TALLY_VALUE);
	tegra_dsi_writel(dsi, val, DSI_TO_TALLY);
}

static void tegra_dsi_setup_video_mode_pkt_length(struct tegra_dc *dc,
						struct tegra_dc_dsi_data *dsi)
{
	u32 val;
	u32 hact_pkt_len;
	u32 hsa_pkt_len;
	u32 hbp_pkt_len;
	u32 hfp_pkt_len;

	hact_pkt_len = dc->mode.h_active * dsi->pixel_scaler_mul /
							dsi->pixel_scaler_div;
	hsa_pkt_len = dc->mode.h_sync_width * dsi->pixel_scaler_mul /
							dsi->pixel_scaler_div;
	hbp_pkt_len = dc->mode.h_back_porch * dsi->pixel_scaler_mul /
							dsi->pixel_scaler_div;
	hfp_pkt_len = dc->mode.h_front_porch * dsi->pixel_scaler_mul /
							dsi->pixel_scaler_div;

	if (dsi->info.video_burst_mode !=
				TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END)
		hbp_pkt_len += hsa_pkt_len;

	hsa_pkt_len -= DSI_HSYNC_BLNK_PKT_OVERHEAD;
	hbp_pkt_len -= DSI_HBACK_PORCH_PKT_OVERHEAD;
	hfp_pkt_len -= DSI_HFRONT_PORCH_PKT_OVERHEAD;

	val = DSI_PKT_LEN_0_1_LENGTH_0(0) |
			DSI_PKT_LEN_0_1_LENGTH_1(hsa_pkt_len);
	tegra_dsi_writel(dsi, val, DSI_PKT_LEN_0_1);

	val = DSI_PKT_LEN_2_3_LENGTH_2(hbp_pkt_len) |
			DSI_PKT_LEN_2_3_LENGTH_3(hact_pkt_len);
	tegra_dsi_writel(dsi, val, DSI_PKT_LEN_2_3);

	val = DSI_PKT_LEN_4_5_LENGTH_4(hfp_pkt_len) |
			DSI_PKT_LEN_4_5_LENGTH_5(0);
	tegra_dsi_writel(dsi, val, DSI_PKT_LEN_4_5);

	val = DSI_PKT_LEN_6_7_LENGTH_6(0) | DSI_PKT_LEN_6_7_LENGTH_7(0);
	tegra_dsi_writel(dsi, val, DSI_PKT_LEN_6_7);
}

static void tegra_dsi_setup_cmd_mode_pkt_length(struct tegra_dc *dc,
						struct tegra_dc_dsi_data *dsi)
{
	unsigned long	val;
	unsigned long	act_bytes;

	act_bytes = dc->mode.h_active * dsi->pixel_scaler_mul /
			dsi->pixel_scaler_div + 1;

	val = DSI_PKT_LEN_0_1_LENGTH_0(0) | DSI_PKT_LEN_0_1_LENGTH_1(0);
	tegra_dsi_writel(dsi, val, DSI_PKT_LEN_0_1);

	val = DSI_PKT_LEN_2_3_LENGTH_2(0) | DSI_PKT_LEN_2_3_LENGTH_3(act_bytes);
	tegra_dsi_writel(dsi, val, DSI_PKT_LEN_2_3);

	val = DSI_PKT_LEN_4_5_LENGTH_4(0) | DSI_PKT_LEN_4_5_LENGTH_5(act_bytes);
	tegra_dsi_writel(dsi, val, DSI_PKT_LEN_4_5);

	val = DSI_PKT_LEN_6_7_LENGTH_6(0) | DSI_PKT_LEN_6_7_LENGTH_7(0x0f0f);
	tegra_dsi_writel(dsi, val, DSI_PKT_LEN_6_7);
}

static void tegra_dsi_set_pkt_length(struct tegra_dc *dc,
						struct tegra_dc_dsi_data *dsi)
{
	if (dsi->driven_mode == TEGRA_DSI_DRIVEN_BY_HOST)
		return;

	if (dsi->info.video_data_type == TEGRA_DSI_VIDEO_TYPE_VIDEO_MODE)
		tegra_dsi_setup_video_mode_pkt_length(dc, dsi);
	else
		tegra_dsi_setup_cmd_mode_pkt_length(dc, dsi);
}

static void tegra_dsi_set_pkt_seq(struct tegra_dc *dc,
						struct tegra_dc_dsi_data *dsi)
{
	const u32 *pkt_seq;
	u32 rgb_info;
	u32 pkt_seq_3_5_rgb_lo;
	u32 pkt_seq_3_5_rgb_hi;
	u32	val;
	u32 reg;
	u8  i;

	if (dsi->driven_mode == TEGRA_DSI_DRIVEN_BY_HOST)
		return;

	switch (dsi->info.pixel_format) {
	case TEGRA_DSI_PIXEL_FORMAT_16BIT_P:
		rgb_info = CMD_RGB_16BPP;
		break;
	case TEGRA_DSI_PIXEL_FORMAT_18BIT_P:
		rgb_info = CMD_RGB_18BPP;
		break;
	case TEGRA_DSI_PIXEL_FORMAT_18BIT_NP:
		rgb_info = CMD_RGB_18BPPNP;
		break;
	case TEGRA_DSI_PIXEL_FORMAT_24BIT_P:
	default:
		rgb_info = CMD_RGB_24BPP;
		break;
	}

	pkt_seq_3_5_rgb_lo = 0;
	pkt_seq_3_5_rgb_hi = 0;
	if (dsi->info.video_data_type == TEGRA_DSI_VIDEO_TYPE_COMMAND_MODE)
		pkt_seq = dsi_pkt_seq_cmd_mode;
	else {
		switch (dsi->info.video_burst_mode) {
		case TEGRA_DSI_VIDEO_BURST_MODE_LOWEST_SPEED:
		case TEGRA_DSI_VIDEO_BURST_MODE_LOW_SPEED:
		case TEGRA_DSI_VIDEO_BURST_MODE_MEDIUM_SPEED:
		case TEGRA_DSI_VIDEO_BURST_MODE_FAST_SPEED:
		case TEGRA_DSI_VIDEO_BURST_MODE_FASTEST_SPEED:
			pkt_seq_3_5_rgb_lo =
					DSI_PKT_SEQ_3_LO_PKT_32_ID(rgb_info);
			if (!dsi->info.no_pkt_seq_eot)
				pkt_seq = dsi_pkt_seq_video_burst;
			else
				pkt_seq = dsi_pkt_seq_video_burst_no_eot;
			break;
		case TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END:
			pkt_seq_3_5_rgb_hi =
					DSI_PKT_SEQ_3_HI_PKT_34_ID(rgb_info);
			pkt_seq = dsi_pkt_seq_video_non_burst_syne;
			break;
		case TEGRA_DSI_VIDEO_NONE_BURST_MODE:
		default:
			pkt_seq_3_5_rgb_lo =
					DSI_PKT_SEQ_3_LO_PKT_32_ID(rgb_info);
			pkt_seq = dsi_pkt_seq_video_non_burst;
			break;
		}
	}

	for (i = 0; i < NUMOF_PKT_SEQ; i++) {
		val = pkt_seq[i];
		reg = dsi_pkt_seq_reg[i];
		if ((reg == DSI_PKT_SEQ_3_LO) || (reg == DSI_PKT_SEQ_5_LO))
			val |= pkt_seq_3_5_rgb_lo;
		if ((reg == DSI_PKT_SEQ_3_HI) || (reg == DSI_PKT_SEQ_5_HI))
			val |= pkt_seq_3_5_rgb_hi;
		tegra_dsi_writel(dsi, val, reg);
	}
}

static void tegra_dsi_stop_dc_stream(struct tegra_dc *dc,
						struct tegra_dc_dsi_data *dsi)
{
	tegra_dc_writel(dc, 0, DC_DISP_DISP_WIN_OPTIONS);
	tegra_dc_writel(dc, GENERAL_ACT_REQ << 8, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);

	dsi->status.dc_stream = DSI_DC_STREAM_DISABLE;
}

void tegra_dsi_stop_dc_stream_at_frame_end(struct tegra_dc *dc,
						struct tegra_dc_dsi_data *dsi)
{
	int val;
	long timeout;
	u32 frame_period = DIV_ROUND_UP(S_TO_MS(1), dsi->info.refresh_rate);

	/* stop dc */
	tegra_dsi_stop_dc_stream(dc, dsi);

	/* enable frame end interrupt */
	val = tegra_dc_readl(dc, DC_CMD_INT_MASK);
	val |= FRAME_END_INT;
	tegra_dc_writel(dc, val, DC_CMD_INT_MASK);

	/* wait for frame_end completion */
	timeout = wait_for_completion_interruptible_timeout(
			&dc->frame_end_complete,
			msecs_to_jiffies(frame_period));

	/* disable frame end interrupt */
	val = tegra_dc_readl(dc, DC_CMD_INT_MASK);
	val &= ~FRAME_END_INT;
	tegra_dc_writel(dc, val, DC_CMD_INT_MASK);
}

static void tegra_dsi_start_dc_stream(struct tegra_dc *dc,
						struct tegra_dc_dsi_data *dsi)
{
	u32 val;

	tegra_dc_writel(dc, DSI_ENABLE, DC_DISP_DISP_WIN_OPTIONS);

	/* TODO: clean up */
	tegra_dc_writel(dc, PW0_ENABLE | PW1_ENABLE | PW2_ENABLE | PW3_ENABLE |
			PW4_ENABLE | PM0_ENABLE | PM1_ENABLE,
			DC_CMD_DISPLAY_POWER_CONTROL);

	/* Configure one-shot mode or continuous mode */
	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE) {
		/* disable LSPI/LCD_DE output */
		val = PIN_OUTPUT_LSPI_OUTPUT_DIS;
		tegra_dc_writel(dc, val, DC_COM_PIN_OUTPUT_ENABLE3);

		/* enable MSF & set MSF polarity */
		val = MSF_ENABLE | MSF_LSPI;
		if (!dsi->info.te_polarity_low)
			val |= MSF_POLARITY_HIGH;
		else
			val |= MSF_POLARITY_LOW;
		tegra_dc_writel(dc, val, DC_CMD_DISPLAY_COMMAND_OPTION0);

		/* set non-continuous mode */
		tegra_dc_writel(dc, DISP_CTRL_MODE_NC_DISPLAY,
						DC_CMD_DISPLAY_COMMAND);
		tegra_dc_writel(dc, GENERAL_UPDATE, DC_CMD_STATE_CONTROL);
		tegra_dc_writel(dc, GENERAL_ACT_REQ | NC_HOST_TRIG,
						DC_CMD_STATE_CONTROL);
	} else {
		/* set continuous mode */
		tegra_dc_writel(dc, DISP_CTRL_MODE_C_DISPLAY,
						DC_CMD_DISPLAY_COMMAND);
		tegra_dc_writel(dc, GENERAL_UPDATE, DC_CMD_STATE_CONTROL);
		tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);
	}

	dsi->status.dc_stream = DSI_DC_STREAM_ENABLE;
}

static void tegra_dsi_set_dc_clk(struct tegra_dc *dc,
						struct tegra_dc_dsi_data *dsi)
{
	u32 shift_clk_div_register;
	u32 val;

	/* Get the corresponding register value of shift_clk_div. */
	shift_clk_div_register = dsi->shift_clk_div * 2 - 2;

#ifndef CONFIG_TEGRA_SILICON_PLATFORM
	shift_clk_div_register = 1;
#endif

	/* TODO: find out if PCD3 option is required */
	val = PIXEL_CLK_DIVIDER_PCD1 |
				SHIFT_CLK_DIVIDER(shift_clk_div_register);
	tegra_dc_writel(dc, val, DC_DISP_DISP_CLOCK_CONTROL);
}

static void tegra_dsi_set_dsi_clk(struct tegra_dc *dc,
					struct tegra_dc_dsi_data *dsi, u32 clk)
{
	u32 rm;

	rm = clk % 1000;
	if (rm != 0)
		clk -= rm;

	clk *= 2;	/*
			 * Value for PLLD routine is required to be twice as
			 * the desired clock rate
			 */

	dc->mode.pclk = clk*1000;
	tegra_dc_setup_clk(dc, dsi->dsi_clk);
	if (dsi->clk_ref == true)
		clk_disable(dsi->dsi_clk);
	else
		dsi->clk_ref = true;
	clk_enable(dsi->dsi_clk);
	tegra_periph_reset_deassert(dsi->dsi_clk);

	dsi->current_dsi_clk_khz = clk_get_rate(dsi->dsi_clk) / 1000;

	dsi->current_bit_clk_ns =  1000*1000 / (dsi->current_dsi_clk_khz * 2);
}

static void tegra_dsi_hs_clk_out_enable(struct tegra_dc_dsi_data *dsi)
{
	u32 val;

	val = tegra_dsi_readl(dsi, DSI_CONTROL);
	val &= ~DSI_CONTROL_HS_CLK_CTRL(1);

	if (dsi->info.video_clock_mode == TEGRA_DSI_VIDEO_CLOCK_CONTINUOUS) {
		val |= DSI_CONTROL_HS_CLK_CTRL(CONTINUOUS);
		dsi->status.clk_mode = DSI_PHYCLK_CONTINUOUS;
	} else {
		val |= DSI_CONTROL_HS_CLK_CTRL(TX_ONLY);
		dsi->status.clk_mode = DSI_PHYCLK_TX_ONLY;
	}
	tegra_dsi_writel(dsi, val, DSI_CONTROL);

	val = tegra_dsi_readl(dsi, DSI_HOST_DSI_CONTROL);
	val &= ~DSI_HOST_DSI_CONTROL_HIGH_SPEED_TRANS(1);
	val |= DSI_HOST_DSI_CONTROL_HIGH_SPEED_TRANS(TEGRA_DSI_HIGH);
	tegra_dsi_writel(dsi, val, DSI_HOST_DSI_CONTROL);

	dsi->status.clk_out = DSI_PHYCLK_OUT_EN;
}

static void tegra_dsi_hs_clk_out_enable_in_lp(struct tegra_dc_dsi_data *dsi)
{
	u32 val;
	tegra_dsi_hs_clk_out_enable(dsi);

	val = tegra_dsi_readl(dsi, DSI_HOST_DSI_CONTROL);
	val &= ~DSI_HOST_DSI_CONTROL_HIGH_SPEED_TRANS(1);
	val |= DSI_HOST_DSI_CONTROL_HIGH_SPEED_TRANS(TEGRA_DSI_LOW);
	tegra_dsi_writel(dsi, val, DSI_HOST_DSI_CONTROL);
}

static void tegra_dsi_hs_clk_out_disable(struct tegra_dc *dc,
						struct tegra_dc_dsi_data *dsi)
{
	u32 val;

	if (dsi->status.dc_stream == DSI_DC_STREAM_ENABLE)
		tegra_dsi_stop_dc_stream(dc, dsi);

	val = tegra_dsi_readl(dsi, DSI_CONTROL);
	val &= ~DSI_CONTROL_HS_CLK_CTRL(1);
	val |= DSI_CONTROL_HS_CLK_CTRL(TX_ONLY);
	tegra_dsi_writel(dsi, val, DSI_CONTROL);

	/* TODO: issue a cmd */

	val = tegra_dsi_readl(dsi, DSI_HOST_DSI_CONTROL);
	val &= ~DSI_HOST_DSI_CONTROL_HIGH_SPEED_TRANS(1);
	val |= DSI_HOST_DSI_CONTROL_HIGH_SPEED_TRANS(TEGRA_DSI_LOW);
	tegra_dsi_writel(dsi, val, DSI_HOST_DSI_CONTROL);

	dsi->status.clk_mode = DSI_PHYCLK_NOT_INIT;
	dsi->status.clk_out = DSI_PHYCLK_OUT_DIS;
}

static void tegra_dsi_set_control_reg_lp(struct tegra_dc_dsi_data *dsi)
{
	u32 dsi_control;
	u32 host_dsi_control;
	u32 max_threshold;

	dsi_control = dsi->dsi_control_val | DSI_CTRL_HOST_DRIVEN;
	host_dsi_control = HOST_DSI_CTRL_COMMON |
			HOST_DSI_CTRL_HOST_DRIVEN |
			DSI_HOST_DSI_CONTROL_HIGH_SPEED_TRANS(TEGRA_DSI_LOW);
	max_threshold = DSI_MAX_THRESHOLD_MAX_THRESHOLD(DSI_HOST_FIFO_DEPTH);

	tegra_dsi_writel(dsi, max_threshold, DSI_MAX_THRESHOLD);
	tegra_dsi_writel(dsi, dsi_control, DSI_CONTROL);
	tegra_dsi_writel(dsi, host_dsi_control, DSI_HOST_DSI_CONTROL);

	dsi->status.driven = DSI_DRIVEN_MODE_HOST;
	dsi->status.clk_burst = DSI_CLK_BURST_NOT_INIT;
	dsi->status.vtype = DSI_VIDEO_TYPE_NOT_INIT;
}

static void tegra_dsi_set_control_reg_hs(struct tegra_dc_dsi_data *dsi)
{
	u32 dsi_control;
	u32 host_dsi_control;
	u32 max_threshold;
	u32 dcs_cmd;

	dsi_control = dsi->dsi_control_val;
	host_dsi_control = HOST_DSI_CTRL_COMMON;
	max_threshold = 0;
	dcs_cmd = 0;

	if (dsi->driven_mode == TEGRA_DSI_DRIVEN_BY_HOST) {
		dsi_control |= DSI_CTRL_HOST_DRIVEN;
		host_dsi_control |= HOST_DSI_CTRL_HOST_DRIVEN;
		max_threshold =
			DSI_MAX_THRESHOLD_MAX_THRESHOLD(DSI_HOST_FIFO_DEPTH);
		dsi->status.driven = DSI_DRIVEN_MODE_HOST;
	} else {
		dsi_control |= DSI_CTRL_DC_DRIVEN;
		host_dsi_control |= HOST_DSI_CTRL_DC_DRIVEN;
		max_threshold =
			DSI_MAX_THRESHOLD_MAX_THRESHOLD(DSI_VIDEO_FIFO_DEPTH);
		dsi->status.driven = DSI_DRIVEN_MODE_DC;
	}

	if (dsi->info.video_data_type == TEGRA_DSI_VIDEO_TYPE_COMMAND_MODE) {
		dsi_control |= DSI_CTRL_CMD_MODE;
		dcs_cmd = DSI_DCS_CMDS_LT5_DCS_CMD(DSI_WRITE_MEMORY_START)|
			DSI_DCS_CMDS_LT3_DCS_CMD(DSI_WRITE_MEMORY_CONTINUE);
		dsi->status.vtype = DSI_VIDEO_TYPE_CMD_MODE;

	} else {
		dsi_control |= DSI_CTRL_VIDEO_MODE;
		dsi->status.vtype = DSI_VIDEO_TYPE_VIDEO_MODE;
	}

	tegra_dsi_writel(dsi, max_threshold, DSI_MAX_THRESHOLD);
	tegra_dsi_writel(dsi, dcs_cmd, DSI_DCS_CMDS);
	tegra_dsi_writel(dsi, dsi_control, DSI_CONTROL);
	tegra_dsi_writel(dsi, host_dsi_control, DSI_HOST_DSI_CONTROL);
}

static void tegra_dsi_pad_caliberation(struct tegra_dc_dsi_data *dsi)
{
	u32 val;

	val =	DSI_PAD_CONTROL_PAD_LPUPADJ(0x1) |
		DSI_PAD_CONTROL_PAD_LPDNADJ(0x1) |
		DSI_PAD_CONTROL_PAD_PREEMP_EN(0x1) |
		DSI_PAD_CONTROL_PAD_SLEWDNADJ(0x6) |
		DSI_PAD_CONTROL_PAD_SLEWUPADJ(0x6);
	if (!dsi->ulpm) {
		val |=	DSI_PAD_CONTROL_PAD_PDIO(0) |
			DSI_PAD_CONTROL_PAD_PDIO_CLK(0) |
			DSI_PAD_CONTROL_PAD_PULLDN_ENAB(TEGRA_DSI_DISABLE);
	} else {
		val |=	DSI_PAD_CONTROL_PAD_PDIO(0x3) |
			DSI_PAD_CONTROL_PAD_PDIO_CLK(0x1) |
			DSI_PAD_CONTROL_PAD_PULLDN_ENAB(TEGRA_DSI_ENABLE);
	}
	tegra_dsi_writel(dsi, val, DSI_PAD_CONTROL);

	val = MIPI_CAL_TERMOSA(0x4);
	tegra_vi_csi_writel(val, CSI_CILA_MIPI_CAL_CONFIG_0);

	val = MIPI_CAL_TERMOSB(0x4);
	tegra_vi_csi_writel(val, CSI_CILB_MIPI_CAL_CONFIG_0);

	val = MIPI_CAL_HSPUOSD(0x3) | MIPI_CAL_HSPDOSD(0x4);
	tegra_vi_csi_writel(val, CSI_DSI_MIPI_CAL_CONFIG);

	val = PAD_DRIV_DN_REF(0x5) | PAD_DRIV_UP_REF(0x7);
	tegra_vi_csi_writel(val, CSI_MIPIBIAS_PAD_CONFIG);
}

static int tegra_dsi_init_hw(struct tegra_dc *dc,
						struct tegra_dc_dsi_data *dsi)
{
	u32 val;
	u32 i;

	val = DSI_POWER_CONTROL_LEG_DSI_ENABLE(TEGRA_DSI_DISABLE);
	tegra_dsi_writel(dsi, val, DSI_POWER_CONTROL);

	tegra_dsi_set_dsi_clk(dc, dsi, dsi->target_lp_clk_khz);
	if (dsi->info.dsi_instance) {
		/* TODO:Set the misc register*/
	}

	/* TODO: only need to change the timing for bta */
	tegra_dsi_set_phy_timing(dsi);

	if (dsi->status.dc_stream == DSI_DC_STREAM_ENABLE)
		tegra_dsi_stop_dc_stream(dc, dsi);

	/* Initializing DSI registers */
	for (i = 0; i < ARRAY_SIZE(init_reg); i++)
		tegra_dsi_writel(dsi, 0, init_reg[i]);

	tegra_dsi_writel(dsi, dsi->dsi_control_val, DSI_CONTROL);

	tegra_dsi_pad_caliberation(dsi);

	val = DSI_POWER_CONTROL_LEG_DSI_ENABLE(TEGRA_DSI_ENABLE);
	tegra_dsi_writel(dsi, val, DSI_POWER_CONTROL);

	while (tegra_dsi_readl(dsi, DSI_POWER_CONTROL) != val)
		tegra_dsi_writel(dsi, val, DSI_POWER_CONTROL);

	dsi->status.init = DSI_MODULE_INIT;
	dsi->status.lphs = DSI_LPHS_NOT_INIT;
	dsi->status.vtype = DSI_VIDEO_TYPE_NOT_INIT;
	dsi->status.driven = DSI_DRIVEN_MODE_NOT_INIT;
	dsi->status.clk_out = DSI_PHYCLK_OUT_DIS;
	dsi->status.clk_mode = DSI_PHYCLK_NOT_INIT;
	dsi->status.clk_burst = DSI_CLK_BURST_NOT_INIT;
	dsi->status.dc_stream = DSI_DC_STREAM_DISABLE;
	dsi->status.lp_op = DSI_LP_OP_NOT_INIT;

	return 0;
}

static int tegra_dsi_set_to_lp_mode(struct tegra_dc *dc,
						struct tegra_dc_dsi_data *dsi,
						u8 lp_op)
{
	int	err;

	if (dsi->status.init != DSI_MODULE_INIT) {
		err = -EPERM;
		goto fail;
	}

	if (dsi->status.lphs == DSI_LPHS_IN_LP_MODE &&
			dsi->status.lp_op == lp_op)
		goto success;

	if (dsi->status.dc_stream == DSI_DC_STREAM_ENABLE)
		tegra_dsi_stop_dc_stream_at_frame_end(dc, dsi);

	/* disable/enable hs clk according to enable_hs_clock_on_lp_cmd_mode */
	if ((dsi->status.clk_out == DSI_PHYCLK_OUT_EN) &&
		(!dsi->info.enable_hs_clock_on_lp_cmd_mode))
		tegra_dsi_hs_clk_out_disable(dc, dsi);

	dsi->target_lp_clk_khz = tegra_dsi_get_lp_clk_rate(dsi, lp_op);
	if (dsi->current_dsi_clk_khz != dsi->target_lp_clk_khz) {
		tegra_dsi_set_dsi_clk(dc, dsi, dsi->target_lp_clk_khz);
		tegra_dsi_set_timeout(dsi);
	}

	tegra_dsi_set_control_reg_lp(dsi);

	if ((dsi->status.clk_out == DSI_PHYCLK_OUT_DIS) &&
		(dsi->info.enable_hs_clock_on_lp_cmd_mode))
		tegra_dsi_hs_clk_out_enable_in_lp(dsi);

success:
	dsi->status.lphs = DSI_LPHS_IN_LP_MODE;
	dsi->status.lp_op = lp_op;
	err = 0;
fail:
	return err;
}

static int tegra_dsi_set_to_hs_mode(struct tegra_dc *dc,
						struct tegra_dc_dsi_data *dsi)
{
	int err;

	if (dsi->status.init != DSI_MODULE_INIT) {
		err = -EPERM;
		goto fail;
	}

	if (dsi->status.dc_stream == DSI_DC_STREAM_ENABLE)
		tegra_dsi_stop_dc_stream_at_frame_end(dc, dsi);

	if ((dsi->status.clk_out == DSI_PHYCLK_OUT_EN) &&
		(!dsi->info.enable_hs_clock_on_lp_cmd_mode))
		tegra_dsi_hs_clk_out_disable(dc, dsi);

	if (dsi->current_dsi_clk_khz != dsi->target_hs_clk_khz) {
		tegra_dsi_set_dsi_clk(dc, dsi, dsi->target_hs_clk_khz);
		tegra_dsi_set_timeout(dsi);
	}

	tegra_dsi_set_phy_timing(dsi);

	if (dsi->driven_mode == TEGRA_DSI_DRIVEN_BY_DC) {
		tegra_dsi_set_pkt_seq(dc, dsi);
		tegra_dsi_set_pkt_length(dc, dsi);
		tegra_dsi_set_sol_delay(dc, dsi);
		tegra_dsi_set_dc_clk(dc, dsi);
	}

	tegra_dsi_set_control_reg_hs(dsi);

	if (dsi->status.clk_out == DSI_PHYCLK_OUT_DIS)
		tegra_dsi_hs_clk_out_enable(dsi);

	dsi->status.lphs = DSI_LPHS_IN_HS_MODE;
	err = 0;
fail:
	return err;
}

static bool tegra_dsi_is_controller_idle(struct tegra_dc_dsi_data *dsi)
{
	u32 timeout = 0;
	bool retVal;

	retVal = false;
	while (timeout <= DSI_MAX_COMMAND_DELAY_USEC) {
		if (!tegra_dsi_readl(dsi, DSI_TRIGGER)) {
			retVal = true;
			break;
		}
		udelay(DSI_COMMAND_DELAY_STEPS_USEC);
		timeout += DSI_COMMAND_DELAY_STEPS_USEC;
	}

	return retVal;
}

static int tegra_dsi_host_trigger(struct tegra_dc_dsi_data *dsi)
{
	int status = 0;

	if (tegra_dsi_readl(dsi, DSI_TRIGGER)) {
		status = -EBUSY;
		goto fail;
	}

	tegra_dsi_writel(dsi,
		DSI_TRIGGER_HOST_TRIGGER(TEGRA_DSI_ENABLE), DSI_TRIGGER);

#if DSI_USE_SYNC_POINTS
	status = tegra_dsi_syncpt(dsi);
	if (status < 0) {
		printk(KERN_ERR "DSI syncpt for host trigger failed\n");
		goto fail;
	}
#else
	if (!tegra_dsi_is_controller_idle(dsi))
		status = -EIO;
#endif

fail:
	return status;
}

static int _tegra_dsi_write_data(struct tegra_dc_dsi_data *dsi,
					u8 *pdata, u8 data_id, u16 data_len)
{
	u8 virtual_channel;
	u8 *pval;
	u32 val;
	int err;

	err = 0;

	virtual_channel = dsi->info.virtual_channel <<
						DSI_VIR_CHANNEL_BIT_POSITION;

	/* always use hw for ecc */
	val = (virtual_channel | data_id) << 0 |
			data_len << 8;
	tegra_dsi_writel(dsi, val, DSI_WR_DATA);

	/* if pdata != NULL, pkt type is long pkt */
	if (pdata != NULL) {
		while (data_len) {
			if (data_len >= 4) {
				val = ((u32 *) pdata)[0];
				data_len -= 4;
				pdata += 4;
			} else {
				val = 0;
				pval = (u8 *) &val;
				do
					*pval++ = *pdata++;
				while (--data_len);
			}
			tegra_dsi_writel(dsi, val, DSI_WR_DATA);
		}
	}

	err = tegra_dsi_host_trigger(dsi);
	if (err < 0)
		printk(KERN_ERR "DSI host trigger failed\n");

	return err;
}

int tegra_dsi_write_data(struct tegra_dc *dc,
					struct tegra_dc_dsi_data *dsi,
					u8 *pdata, u8 data_id, u16 data_len)
{
	bool switch_back_to_hs_mode;
	bool switch_back_to_dc_mode;
	int err;

	err = 0;
	switch_back_to_hs_mode = false;
	switch_back_to_dc_mode = false;

	if ((dsi->status.init != DSI_MODULE_INIT) ||
		(dsi->status.lphs == DSI_LPHS_NOT_INIT) ||
		(dsi->status.lp_op == DSI_LP_OP_NOT_INIT)) {
		err = -EPERM;
		goto fail;
	}

	if (!tegra_dsi_is_controller_idle(dsi)) {
		err = -EBUSY;
		goto fail;
	}

	err = 0;

	if (dsi->status.lphs == DSI_LPHS_IN_HS_MODE) {
		if (dsi->info.hs_cmd_mode_supported) {
			if (dsi->status.driven == DSI_DRIVEN_MODE_DC) {
				dsi->driven_mode = TEGRA_DSI_DRIVEN_BY_HOST;
				tegra_dsi_set_to_hs_mode(dc, dsi);
				switch_back_to_dc_mode = true;
			}
		} else {
			tegra_dsi_set_to_lp_mode(dc, dsi, DSI_LP_OP_WRITE);
			switch_back_to_hs_mode = true;
		}
	}

	err = _tegra_dsi_write_data(dsi, pdata, data_id, data_len);


	if (switch_back_to_dc_mode)
		dsi->driven_mode = TEGRA_DSI_DRIVEN_BY_DC;
	if (switch_back_to_dc_mode || switch_back_to_hs_mode)
		tegra_dsi_set_to_hs_mode(dc, dsi);

fail:
	return err;
}
EXPORT_SYMBOL(tegra_dsi_write_data);

static int tegra_dsi_send_panel_cmd(struct tegra_dc *dc,
						struct tegra_dc_dsi_data *dsi,
						struct tegra_dsi_cmd *cmd,
						u32 n_cmd)
{
	u32 i;
	int err;

	err = 0;
	for (i = 0; i < n_cmd; i++) {
		struct tegra_dsi_cmd *cur_cmd;
		cur_cmd = &cmd[i];

		if (cur_cmd->cmd_type == TEGRA_DSI_DELAY_MS)
			mdelay(cur_cmd->sp_len_dly.delay_ms);
		else {
			err = tegra_dsi_write_data(dc, dsi,
						cur_cmd->pdata,
						cur_cmd->data_id,
						cur_cmd->sp_len_dly.data_len);
			if (err < 0)
				break;
		}
	}
	return err;
}

static u8 get8BitECC(u32 header)
{
	char ecc_parity[24] = { 0x07, 0x0b, 0x0d, 0x0e, 0x13, 0x15, 0x16, 0x19,
				0x1a, 0x1c, 0x23, 0x25, 0x26, 0x29, 0x2a, 0x2c,
				0x31, 0x32, 0x34, 0x38, 0x1f, 0x2f, 0x37, 0x3b};
	u8 ecc_byte;
	int i;

	ecc_byte = 0;
	for (i = 0; i < 24; i++)
		ecc_byte ^= ((header >> i) & 1) ? ecc_parity[i] : 0x00;

	return ecc_byte;
}

/* This function is written to send DCS short write (1 parameter) only
 * This means the cmd will contain only 1 byte of index and 1 byte of value
 * The data type ID is fixed at 0x15 and the ECC is calculated
 * based on the data in pdata
 * The command will be sent by hardware every frame
 * pdata should contain both the index + value for each cmd
 * data_len will be the total number of bytes in pdata
 */

int tegra_dsi_send_panel_short_cmd(struct tegra_dc *dc,
						u8 *pdata,
						u8 data_len)
{
	u8 ecc8Bits = 0, data_len_orig = 0;
	u32 val = 0, packetHeader = 0;
	int err = 0, count = 0;
	struct tegra_dc_dsi_data *dsi = tegra_dc_get_outdata(dc);

	data_len_orig = data_len;
	if (pdata != NULL) {
		while (data_len) {
			if (data_len >= 2) {
				packetHeader = (CMD_SHORTW |
					(((u16 *) pdata)[0]) << 8 | 0x00 << 24);
				ecc8Bits = get8BitECC(packetHeader);
				val = (packetHeader | (ecc8Bits << 24));
				data_len -= 2;
				pdata += 2;
				count++;
			}
			switch (count) {
			case 1:
				tegra_dsi_writel(dsi, val, DSI_INIT_SEQ_DATA_0);
				break;
			case 2:
				tegra_dsi_writel(dsi, val, DSI_INIT_SEQ_DATA_1);
				break;
			case 3:
				tegra_dsi_writel(dsi, val, DSI_INIT_SEQ_DATA_2);
				break;
			case 4:
				tegra_dsi_writel(dsi, val, DSI_INIT_SEQ_DATA_3);
				break;
			case 5:
				tegra_dsi_writel(dsi, val, DSI_INIT_SEQ_DATA_4);
				break;
			case 6:
				tegra_dsi_writel(dsi, val, DSI_INIT_SEQ_DATA_5);
				break;
			case 7:
				tegra_dsi_writel(dsi, val, DSI_INIT_SEQ_DATA_6);
				break;
			case 8:
				tegra_dsi_writel(dsi, val, DSI_INIT_SEQ_DATA_7);
				break;
			default:
				err = 1;
				break;
			}
		}
	}

	val = DSI_INIT_SEQ_CONTROL_DSI_FRAME_INIT_BYTE_COUNT(data_len_orig*2) |
		DSI_INIT_SEQ_CONTROL_DSI_SEND_INIT_SEQUENCE(1);
	tegra_dsi_writel(dsi, val, DSI_INIT_SEQ_CONTROL);

	return err;
}
EXPORT_SYMBOL(tegra_dsi_send_panel_short_cmd);

static int tegra_dsi_bta(struct tegra_dc_dsi_data *dsi)
{
	u32 val;
	u32 poll_time;
	int err;

	poll_time = 0;
	err = 0;

	val = tegra_dsi_readl(dsi, DSI_HOST_DSI_CONTROL);
	val |= DSI_HOST_DSI_CONTROL_IMM_BTA(TEGRA_DSI_ENABLE);
	tegra_dsi_writel(dsi, val, DSI_HOST_DSI_CONTROL);

#if DSI_USE_SYNC_POINTS
	/* FIXME: Workaround for nvhost_syncpt_read */
	dsi->syncpt_val = nvhost_syncpt_update_min(
			&dsi->dc->ndev->host->syncpt, dsi->syncpt_id);

	val = DSI_INCR_SYNCPT_COND(OP_DONE) |
		DSI_INCR_SYNCPT_INDX(dsi->syncpt_id);
	tegra_dsi_writel(dsi, val, DSI_INCR_SYNCPT);

	/* TODO: Use interrupt rather than polling */
	err = nvhost_syncpt_wait(&dsi->dc->ndev->host->syncpt,
		dsi->syncpt_id, dsi->syncpt_val + 1);
	if (err < 0)
		printk(KERN_ERR "DSI sync point failure\n");
	else
		(dsi->syncpt_val)++;
#else
	while (poll_time <  DSI_STATUS_POLLING_DURATION_USEC) {
		val = tegra_dsi_readl(dsi, DSI_HOST_DSI_CONTROL);
		val &= DSI_HOST_DSI_CONTROL_IMM_BTA(TEGRA_DSI_ENABLE);
		if (!val)
			break;
		udelay(DSI_STATUS_POLLING_DELAY_USEC);
		poll_time += DSI_STATUS_POLLING_DELAY_USEC;
	}
	if (poll_time > DSI_STATUS_POLLING_DURATION_USEC)
		err = -EBUSY;
#endif

	return err;
}

static void tegra_dsi_read_fifo(struct tegra_dc *dc,
			struct tegra_dc_dsi_data *dsi,
			u32 rd_fifo_cnt,
			u8 *read_fifo)
{
	u32 val;
	u32 i;

	/* Read data from FIFO */
	for (i = 0; i < rd_fifo_cnt; i++) {
		val = tegra_dsi_readl(dsi, DSI_RD_DATA);
		printk(KERN_INFO "Read data[%d]: 0x%x\n", i, val);
		memcpy(read_fifo, &val, 4);
		read_fifo += 4;
	}

	/* Make sure all the data is read from the FIFO */
	val = tegra_dsi_readl(dsi, DSI_STATUS);
	val &= DSI_STATUS_RD_FIFO_COUNT(0x1f);
	if (val)
		dev_err(&dc->ndev->dev, "DSI FIFO_RD_CNT not zero"
		" even after reading FIFO_RD_CNT words from read fifo\n");
}

static int tegra_dsi_parse_read_response(struct tegra_dc *dc,
				u32 rd_fifo_cnt, u8 *read_fifo)
{
	int err;
	u32 payload_size;

	payload_size = 0;
	err = 0;

	printk(KERN_INFO "escape sequence[0x%x]\n", read_fifo[0]);
	switch (read_fifo[4] & 0xff) {
	case GEN_LONG_RD_RES:
		/* Fall through */
	case DCS_LONG_RD_RES:
		payload_size = (read_fifo[5] |
				(read_fifo[6] << 8)) & 0xFFFF;
		printk(KERN_INFO "Long read response Packet\n"
				"payload_size[0x%x]\n", payload_size);
		break;
	case GEN_1_BYTE_SHORT_RD_RES:
		/* Fall through */
	case DCS_1_BYTE_SHORT_RD_RES:
		payload_size = 1;
		printk(KERN_INFO "Short read response Packet\n"
			"payload_size[0x%x]\n", payload_size);
		break;
	case GEN_2_BYTE_SHORT_RD_RES:
		/* Fall through */
	case DCS_2_BYTE_SHORT_RD_RES:
		payload_size = 2;
		printk(KERN_INFO "Short read response Packet\n"
			"payload_size[0x%x]\n", payload_size);
		break;
	case ACK_ERR_RES:
		payload_size = 2;
		printk(KERN_INFO "Acknowledge error report response\n"
			"Packet payload_size[0x%x]\n", payload_size);
		break;
	default:
		/*reading from RD_FIFO_COUNT*/
		printk(KERN_INFO "Invalid read response payload_size\n");
		err = -EINVAL;
		break;
	}
	return err;
}

int tegra_dsi_read_data(struct tegra_dc *dc,
				struct tegra_dc_dsi_data *dsi,
				u32 max_ret_payload_size,
				u32 panel_reg_addr, u8 *read_data)
{
	u32 val;
	int err;
	u32 poll_time;
	u32 rd_fifo_cnt;
	bool switch_back_to_hs_mode;
	bool restart_dc_stream;
	bool switch_back_to_dc_mode;

	err = 0;
	switch_back_to_hs_mode = false;
	restart_dc_stream = false;
	switch_back_to_dc_mode = false;

	if ((dsi->status.init != DSI_MODULE_INIT) ||
		(dsi->status.lphs == DSI_LPHS_NOT_INIT) ||
		(dsi->status.driven == DSI_DRIVEN_MODE_NOT_INIT)||
		(dsi->status.lp_op == DSI_LP_OP_NOT_INIT)) {
		err = -EPERM;
		goto fail;
	}

	tegra_dc_io_start(dc);

	val = tegra_dsi_readl(dsi, DSI_STATUS);
	val &= DSI_STATUS_RD_FIFO_COUNT(0x1f);
	if (val) {
		err = -EBUSY;
		dev_err(&dc->ndev->dev, "DSI fifo count not zero\n");
		goto fail;
	}

	if (!tegra_dsi_is_controller_idle(dsi)) {
		err = -EBUSY;
		dev_err(&dc->ndev->dev, "DSI trigger bit is already set\n");
		goto fail;
	}

	val = tegra_dsi_readl(dsi, DSI_STATUS);
	val &= (DSI_STATUS_LB_OVERFLOW(0x1) | DSI_STATUS_LB_UNDERFLOW(0x1));
	if (val) {
		dev_warn(&dc->ndev->dev, "Reset overflow/underflow\n");
		val = tegra_dsi_readl(dsi, DSI_HOST_DSI_CONTROL);
		val |= DSI_HOST_CONTROL_FIFO_STAT_RESET(0x1);
		tegra_dsi_writel(dsi, val, DSI_HOST_DSI_CONTROL);
		ndelay(200);
	}

	if (dsi->status.lphs == DSI_LPHS_IN_HS_MODE) {
		if (dsi->status.driven == DSI_DRIVEN_MODE_DC) {
			if (dsi->status.dc_stream == DSI_DC_STREAM_ENABLE) {
				tegra_dsi_stop_dc_stream_at_frame_end(dc, dsi);
				restart_dc_stream = true;
			}
			dsi->driven_mode = TEGRA_DSI_DRIVEN_BY_HOST;
			switch_back_to_dc_mode = true;
			if (dsi->info.hs_cmd_mode_supported) {
				err = tegra_dsi_set_to_hs_mode(dc, dsi);
				if (err < 0) {
					dev_err(&dc->ndev->dev,
					"DSI failed to go to HS host driven mode\n");
					goto fail;
				}
			}
		}
		if (!dsi->info.hs_cmd_mode_supported) {
			err = tegra_dsi_set_to_lp_mode(dc, dsi, DSI_LP_OP_WRITE);
			if (err < 0) {
				dev_err(&dc->ndev->dev,
				"DSI failed to go to LP mode\n");
				goto fail;
			}
			switch_back_to_hs_mode = true;
		}
	}

	/* Set max return payload size in words */
	err = _tegra_dsi_write_data(dsi, NULL,
		dsi_command_max_return_pkt_size,
		max_ret_payload_size);
	if (err < 0) {
		dev_err(&dc->ndev->dev,
				"DSI write failed\n");
		goto fail;
	}

	/* DCS to read given panel register */
	err = _tegra_dsi_write_data(dsi, NULL,
		dsi_command_dcs_read_with_no_params,
		panel_reg_addr);
	if (err < 0) {
		dev_err(&dc->ndev->dev,
				"DSI write failed\n");
		goto fail;
	}

	if (switch_back_to_hs_mode) {
		err = tegra_dsi_set_to_lp_mode(dc, dsi, DSI_LP_OP_READ);
		if (err < 0) {
			dev_err(&dc->ndev->dev,
			"DSI failed to go to LP mode\n");
			goto fail;
		}
	}

	err = tegra_dsi_bta(dsi);
	if (err < 0) {
		dev_err(&dc->ndev->dev,
			"DSI IMM BTA timeout\n");
		goto fail;
	}

	poll_time = 0;
	while (poll_time <  DSI_DELAY_FOR_READ_FIFO) {
		mdelay(1);
		val = tegra_dsi_readl(dsi, DSI_STATUS);
		rd_fifo_cnt = val & DSI_STATUS_RD_FIFO_COUNT(0x1f);
		if ((rd_fifo_cnt << 2) > DSI_READ_FIFO_DEPTH) {
			dev_err(&dc->ndev->dev, "DSI RD_FIFO_CNT is greater "
					"than RD_FIFO_DEPTH\n");
			break;
		}
		poll_time++;
	}

	if (rd_fifo_cnt == 0) {
		dev_info(&dc->ndev->dev,
			"DSI RD_FIFO_CNT is zero\n");
		err = -EINVAL;
		goto fail;
	}

	if (val & DSI_STATUS_LB_UNDERFLOW(0x1) ||
		val & DSI_STATUS_LB_OVERFLOW(0x1)) {
		dev_err(&dc->ndev->dev,
			"DSI overflow/underflow error\n");
		err = -EINVAL;
		goto fail;
	}

	tegra_dsi_read_fifo(dc, dsi, rd_fifo_cnt, read_data);

	err = tegra_dsi_parse_read_response(dc, rd_fifo_cnt, read_data);
fail:
	if (switch_back_to_dc_mode)
		dsi->driven_mode = TEGRA_DSI_DRIVEN_BY_DC;
	if (switch_back_to_dc_mode || switch_back_to_hs_mode)
		tegra_dsi_set_to_hs_mode(dc, dsi);
	if (restart_dc_stream)
		tegra_dsi_start_dc_stream(dc, dsi);

	tegra_dc_io_end(dc);

	return err;
}
EXPORT_SYMBOL(tegra_dsi_read_data);

static int tegra_dsi_enter_ulpm(struct tegra_dc_dsi_data *dsi)
{
	u32 val;
	int ret;

	ret = 0;

	val = tegra_dsi_readl(dsi, DSI_HOST_DSI_CONTROL);
	val &= ~DSI_HOST_DSI_CONTROL_ULTRA_LOW_POWER(3);
	val |= DSI_HOST_DSI_CONTROL_ULTRA_LOW_POWER(ENTER_ULPM);
	tegra_dsi_writel(dsi, val, DSI_HOST_DSI_CONTROL);

#if DSI_USE_SYNC_POINTS
	ret = tegra_dsi_syncpt(dsi);
	if (ret < 0) {
		printk(KERN_ERR "DSI syncpt for ulpm enter failed\n");
		goto fail;
	}
#else
	/* TODO: Find exact delay required */
	mdelay(10);
#endif
	dsi->ulpm = true;
fail:
	return ret;
}

static int tegra_dsi_exit_ulpm(struct tegra_dc_dsi_data *dsi)
{
	u32 val;
	int ret;

	ret = 0;

	val = tegra_dsi_readl(dsi, DSI_HOST_DSI_CONTROL);
	val &= ~DSI_HOST_DSI_CONTROL_ULTRA_LOW_POWER(3);
	val |= DSI_HOST_DSI_CONTROL_ULTRA_LOW_POWER(EXIT_ULPM);
	tegra_dsi_writel(dsi, val, DSI_HOST_DSI_CONTROL);

#if DSI_USE_SYNC_POINTS
	ret = tegra_dsi_syncpt(dsi);
	if (ret < 0) {
		printk(KERN_ERR "DSI syncpt for ulpm exit failed\n");
		goto fail;
	}
#else
	/* TODO: Find exact delay required */
	mdelay(10);
#endif
	dsi->ulpm = false;

	val = tegra_dsi_readl(dsi, DSI_HOST_DSI_CONTROL);
	val &= ~DSI_HOST_DSI_CONTROL_ULTRA_LOW_POWER(0x3);
	val |= DSI_HOST_DSI_CONTROL_ULTRA_LOW_POWER(NORMAL);
	tegra_dsi_writel(dsi, val, DSI_HOST_DSI_CONTROL);
fail:
	return ret;

}

static void tegra_dc_dsi_enable(struct tegra_dc *dc)
{
	struct tegra_dc_dsi_data *dsi = tegra_dc_get_outdata(dc);
	int err;
	u32 val;

	tegra_dc_io_start(dc);
	mutex_lock(&dsi->lock);

	/* Stop DC stream before configuring DSI registers
	 * to avoid visible glitches on panel during transition
	 * from bootloader to kernel driver
	 */
	tegra_dsi_stop_dc_stream_at_frame_end(dc, dsi);

	if (dsi->enabled) {
		if (dsi->ulpm) {
			if (tegra_dsi_exit_ulpm(dsi) < 0) {
				printk(KERN_ERR "DSI failed to exit ulpm\n");
				goto fail;
			}
		}

		if (dsi->info.panel_reset) {
			err = tegra_dsi_send_panel_cmd(dc, dsi,
							dsi->info.dsi_init_cmd,
							dsi->info.n_init_cmd);
			if (err < 0) {
				dev_err(&dc->ndev->dev,
				"dsi: error sending dsi init cmd\n");
				goto fail;
			}
		} else if (dsi->info.dsi_late_resume_cmd) {
			err = tegra_dsi_send_panel_cmd(dc, dsi,
						dsi->info.dsi_late_resume_cmd,
						dsi->info.n_late_resume_cmd);
			if (err < 0) {
				dev_err(&dc->ndev->dev,
				"dsi: error sending late resume cmd\n");
				goto fail;
			}
		}
	} else {
		err = tegra_dsi_init_hw(dc, dsi);
		if (err < 0) {
			dev_err(&dc->ndev->dev,
				"dsi: not able to init dsi hardware\n");
			goto fail;
		}

		if (dsi->ulpm) {
			if (tegra_dsi_enter_ulpm(dsi) < 0) {
				printk(KERN_ERR "DSI failed to enter ulpm\n");
				goto fail;
			}
			val = DSI_PAD_CONTROL_PAD_PDIO(0) |
				DSI_PAD_CONTROL_PAD_PDIO_CLK(0) |
				DSI_PAD_CONTROL_PAD_PULLDN_ENAB(TEGRA_DSI_DISABLE);
			tegra_dsi_writel(dsi, val, DSI_PAD_CONTROL);
			if (tegra_dsi_exit_ulpm(dsi) < 0) {
				printk(KERN_ERR "DSI failed to exit ulpm\n");
				goto fail;
			}
		}

		err = tegra_dsi_set_to_lp_mode(dc, dsi, DSI_LP_OP_WRITE);
		if (err < 0) {
			dev_err(&dc->ndev->dev,
				"dsi: not able to set to lp mode\n");
			goto fail;
		}

		err = tegra_dsi_send_panel_cmd(dc, dsi, dsi->info.dsi_init_cmd,
						dsi->info.n_init_cmd);
		if (err < 0) {
			dev_err(&dc->ndev->dev,
				"dsi: error while sending dsi init cmd\n");
			goto fail;
		}

		err = tegra_dsi_set_to_hs_mode(dc, dsi);
		if (err < 0) {
			dev_err(&dc->ndev->dev,
				"dsi: not able to set to hs mode\n");
			goto fail;
		}

		dsi->enabled = true;
	}

	if (dsi->status.driven == DSI_DRIVEN_MODE_DC)
		tegra_dsi_start_dc_stream(dc, dsi);
fail:
	mutex_unlock(&dsi->lock);
	tegra_dc_io_end(dc);
}

static void _tegra_dc_dsi_init(struct tegra_dc *dc)
{
	struct tegra_dc_dsi_data *dsi = tegra_dc_get_outdata(dc);

	tegra_dsi_init_sw(dc, dsi);
	/* TODO: Configure the CSI pad configuration */
}

static int tegra_dc_dsi_cp_p_cmd(struct tegra_dsi_cmd *src,
					struct tegra_dsi_cmd *dst, u16 n_cmd)
{
	u16 i;
	u16 len;

	memcpy(dst, src, sizeof(*dst) * n_cmd);

	for (i = 0; i < n_cmd; i++)
		if (src[i].pdata) {
			len = sizeof(*src[i].pdata) *
					src[i].sp_len_dly.data_len;
			dst[i].pdata = kzalloc(len, GFP_KERNEL);
			if (!dst[i].pdata)
				goto free_cmd_pdata;
			memcpy(dst[i].pdata, src[i].pdata, len);
		}

	return 0;

free_cmd_pdata:
	for (i = 0; i < n_cmd; i++)
		if (dst[i].pdata)
			kfree(dst[i].pdata);
	return -ENOMEM;
}

static int tegra_dc_dsi_cp_info(struct tegra_dc_dsi_data *dsi,
						struct tegra_dsi_out *p_dsi)
{
	struct tegra_dsi_cmd *p_init_cmd;
	struct tegra_dsi_cmd *p_early_suspend_cmd;
	struct tegra_dsi_cmd *p_late_resume_cmd;
	struct tegra_dsi_cmd *p_suspend_cmd;
	int err;

	if (p_dsi->n_data_lanes > MAX_DSI_DATA_LANES)
		return -EINVAL;

	p_init_cmd = kzalloc(sizeof(*p_init_cmd) *
				p_dsi->n_init_cmd, GFP_KERNEL);
	if (!p_init_cmd)
		return -ENOMEM;

	if (p_dsi->dsi_early_suspend_cmd) {
		p_early_suspend_cmd = kzalloc(sizeof(*p_early_suspend_cmd) *
					p_dsi->n_early_suspend_cmd,
					GFP_KERNEL);
		if (!p_early_suspend_cmd) {
			err = -ENOMEM;
			goto err_free_init_cmd;
		}
	}

	if (p_dsi->dsi_late_resume_cmd) {
		p_late_resume_cmd = kzalloc(sizeof(*p_late_resume_cmd) *
					p_dsi->n_late_resume_cmd,
					GFP_KERNEL);
		if (!p_late_resume_cmd) {
			err = -ENOMEM;
			goto err_free_p_early_suspend_cmd;
		}
	}

	p_suspend_cmd = kzalloc(sizeof(*p_suspend_cmd) * p_dsi->n_suspend_cmd,
				GFP_KERNEL);
	if (!p_suspend_cmd) {
		err = -ENOMEM;
		goto err_free_p_late_resume_cmd;
	}

	memcpy(&dsi->info, p_dsi, sizeof(dsi->info));

	/* Copy panel init cmd */
	err = tegra_dc_dsi_cp_p_cmd(p_dsi->dsi_init_cmd,
						p_init_cmd, p_dsi->n_init_cmd);
	if (err < 0)
		goto err_free;
	dsi->info.dsi_init_cmd = p_init_cmd;

	/* Copy panel early suspend cmd */
	if (p_dsi->dsi_early_suspend_cmd) {
		err = tegra_dc_dsi_cp_p_cmd(p_dsi->dsi_early_suspend_cmd,
					p_early_suspend_cmd,
					p_dsi->n_early_suspend_cmd);
		if (err < 0)
			goto err_free;
		dsi->info.dsi_early_suspend_cmd = p_early_suspend_cmd;
	}

	/* Copy panel late resume cmd */
	if (p_dsi->dsi_late_resume_cmd) {
		err = tegra_dc_dsi_cp_p_cmd(p_dsi->dsi_late_resume_cmd,
						p_late_resume_cmd,
						p_dsi->n_late_resume_cmd);
		if (err < 0)
			goto err_free;
		dsi->info.dsi_late_resume_cmd = p_late_resume_cmd;
	}

	/* Copy panel suspend cmd */
	err = tegra_dc_dsi_cp_p_cmd(p_dsi->dsi_suspend_cmd, p_suspend_cmd,
					p_dsi->n_suspend_cmd);
	if (err < 0)
		goto err_free;
	dsi->info.dsi_suspend_cmd = p_suspend_cmd;

	if (!dsi->info.panel_reset_timeout_msec)
		dsi->info.panel_reset_timeout_msec =
						DEFAULT_PANEL_RESET_TIMEOUT;

	if (!dsi->info.panel_buffer_size_byte)
		dsi->info.panel_buffer_size_byte = DEFAULT_PANEL_BUFFER_BYTE;

	if (!dsi->info.max_panel_freq_khz) {
		dsi->info.max_panel_freq_khz = DEFAULT_MAX_DSI_PHY_CLK_KHZ;

		if (dsi->info.video_burst_mode >
				TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END){
			dev_err(&dsi->dc->ndev->dev, "DSI: max_panel_freq_khz"
					"is not set for DSI burst mode.\n");
			dsi->info.video_burst_mode =
				TEGRA_DSI_VIDEO_BURST_MODE_LOWEST_SPEED;
		}
	}

	if (!dsi->info.lp_cmd_mode_freq_khz)
		dsi->info.lp_cmd_mode_freq_khz = DEFAULT_LP_CMD_MODE_CLK_KHZ;

	if (!dsi->info.chip_id || !dsi->info.chip_rev)
		printk(KERN_WARNING "DSI: Failed to get chip info\n");

	if (!dsi->info.lp_read_cmd_mode_freq_khz)
		dsi->info.lp_read_cmd_mode_freq_khz =
			dsi->info.lp_cmd_mode_freq_khz;

	/* host mode is for testing only*/
	dsi->driven_mode = TEGRA_DSI_DRIVEN_BY_DC;
	return 0;

err_free:
	kfree(p_suspend_cmd);
err_free_p_late_resume_cmd:
	kfree(p_late_resume_cmd);
err_free_p_early_suspend_cmd:
	kfree(p_early_suspend_cmd);
err_free_init_cmd:
	kfree(p_init_cmd);
	return err;
}

static int tegra_dc_dsi_init(struct tegra_dc *dc)
{
	struct tegra_dc_dsi_data *dsi;
	struct resource *res;
	struct resource *base_res;
	void __iomem *base;
	struct clk *dc_clk = NULL;
	struct clk *dsi_clk = NULL;
	struct tegra_dsi_out *dsi_pdata;
	int err;

	err = 0;

	dsi = kzalloc(sizeof(*dsi), GFP_KERNEL);
	if (!dsi)
		return -ENOMEM;

	res = nvhost_get_resource_byname(dc->ndev, IORESOURCE_MEM,
					"dsi_regs");
	if (!res) {
		dev_err(&dc->ndev->dev, "dsi: no mem resource\n");
		err = -ENOENT;
		goto err_free_dsi;
	}

	base_res = request_mem_region(res->start, resource_size(res),
				dc->ndev->name);
	if (!base_res) {
		dev_err(&dc->ndev->dev, "dsi: request_mem_region failed\n");
		err = -EBUSY;
		goto err_free_dsi;
	}

	base = ioremap(res->start, resource_size(res));
	if (!base) {
		dev_err(&dc->ndev->dev, "dsi: registers can't be mapped\n");
		err = -EBUSY;
		goto err_release_regs;
	}

	dsi_pdata = dc->pdata->default_out->dsi;
	if (!dsi_pdata) {
		dev_err(&dc->ndev->dev, "dsi: dsi data not available\n");
		goto err_release_regs;
	}

	if (dsi_pdata->dsi_instance)
		dsi_clk = clk_get(&dc->ndev->dev, "dsib");
	else
		dsi_clk = clk_get(&dc->ndev->dev, "dsia");

	if (IS_ERR_OR_NULL(dsi_clk)) {
		dev_err(&dc->ndev->dev, "dsi: can't get clock\n");
		err = -EBUSY;
		goto err_release_regs;
	}

	dc_clk = clk_get_sys(dev_name(&dc->ndev->dev), NULL);
	if (IS_ERR_OR_NULL(dc_clk)) {
		dev_err(&dc->ndev->dev, "dsi: dc clock %s unavailable\n",
			dev_name(&dc->ndev->dev));
		err = -EBUSY;
		goto err_clk_put;
	}

	mutex_init(&dsi->lock);
	dsi->dc = dc;
	dsi->base = base;
	dsi->base_res = base_res;
	dsi->dc_clk = dc_clk;
	dsi->dsi_clk = dsi_clk;

	err = tegra_dc_dsi_cp_info(dsi, dsi_pdata);
	if (err < 0)
		goto err_dsi_data;

	tegra_dc_set_outdata(dc, dsi);
	_tegra_dc_dsi_init(dc);

	return 0;

err_dsi_data:
err_clk_put:
	clk_put(dsi_clk);
err_release_regs:
	release_resource(base_res);
err_free_dsi:
	kfree(dsi);

	return err;
}

static void tegra_dc_dsi_destroy(struct tegra_dc *dc)
{
	struct tegra_dc_dsi_data *dsi = tegra_dc_get_outdata(dc);
	u16 i;
	u32 val;

	mutex_lock(&dsi->lock);

	/* free up the pdata*/
	for (i = 0; i < dsi->info.n_init_cmd; i++) {
		if (dsi->info.dsi_init_cmd[i].pdata)
			kfree(dsi->info.dsi_init_cmd[i].pdata);
	}
	kfree(dsi->info.dsi_init_cmd);

	/* Disable dc stream*/
	if (dsi->status.dc_stream == DSI_DC_STREAM_ENABLE)
		tegra_dsi_stop_dc_stream(dc, dsi);

	/* Disable dsi phy clock*/
	if (dsi->status.clk_out == DSI_PHYCLK_OUT_EN)
		tegra_dsi_hs_clk_out_disable(dc, dsi);

	val = DSI_POWER_CONTROL_LEG_DSI_ENABLE(TEGRA_DSI_DISABLE);
	tegra_dsi_writel(dsi, val, DSI_POWER_CONTROL);

	iounmap(dsi->base);
	release_resource(dsi->base_res);

	clk_put(dsi->dc_clk);
	clk_put(dsi->dsi_clk);

	mutex_unlock(&dsi->lock);

	mutex_destroy(&dsi->lock);
	kfree(dsi);
}

static int tegra_dsi_deep_sleep(struct tegra_dc *dc,
				struct tegra_dc_dsi_data *dsi)
{
	int err = 0;
	int val;
	struct clk *parent_clk = NULL;
	struct clk *base_clk = NULL;

	if (!dsi->enabled) {
		err = -EPERM;
		goto fail;
	}

	err = tegra_dsi_set_to_lp_mode(dc, dsi, DSI_LP_OP_WRITE);
	if (err < 0) {
		dev_err(&dc->ndev->dev,
		"DSI failed to go to LP mode\n");
		goto fail;
	}

	/* Suspend panel */
	err = tegra_dsi_send_panel_cmd(dc, dsi,
			dsi->info.dsi_suspend_cmd,
			dsi->info.n_suspend_cmd);
	if (err < 0) {
		dev_err(&dc->ndev->dev,
			"dsi: Error sending suspend cmd\n");
		goto fail;
	}

	if (!dsi->ulpm) {
		err = tegra_dsi_enter_ulpm(dsi);
		if (err < 0) {
			dev_err(&dc->ndev->dev,
				"DSI failed to enter ulpm\n");
			goto fail;
		}
	}

	/* Suspend pad */
	val = tegra_dsi_readl(dsi, DSI_PAD_CONTROL);
	val = DSI_PAD_CONTROL_PAD_PDIO(0x3) |
		DSI_PAD_CONTROL_PAD_PDIO_CLK(0x1) |
		DSI_PAD_CONTROL_PAD_PULLDN_ENAB(TEGRA_DSI_ENABLE);
	tegra_dsi_writel(dsi, val, DSI_PAD_CONTROL);

	/* Suspend core-logic */
	val = DSI_POWER_CONTROL_LEG_DSI_ENABLE(TEGRA_DSI_DISABLE);
	tegra_dsi_writel(dsi, val, DSI_POWER_CONTROL);

	/* Disable dsi fast and slow clock */
	parent_clk = clk_get_parent(dsi->dsi_clk);
	base_clk = clk_get_parent(parent_clk);
	if (dsi->info.dsi_instance)
		tegra_clk_cfg_ex(base_clk,
				TEGRA_CLK_PLLD_CSI_OUT_ENB,
				0);
	else
		tegra_clk_cfg_ex(base_clk,
				TEGRA_CLK_PLLD_DSI_OUT_ENB,
				0);

	/* Disable dsi source clock */
	clk_disable(dsi->dsi_clk);

	dsi->clk_ref = false;
	dsi->enabled = false;

	return 0;
fail:
	return err;
}

static void tegra_dc_dsi_disable(struct tegra_dc *dc)
{
	int err;
	struct tegra_dc_dsi_data *dsi = tegra_dc_get_outdata(dc);

	tegra_dc_io_start(dc);
	mutex_lock(&dsi->lock);

	if (dsi->status.dc_stream == DSI_DC_STREAM_ENABLE)
		tegra_dsi_stop_dc_stream_at_frame_end(dc, dsi);

	if (dsi->info.power_saving_suspend) {
		if (tegra_dsi_deep_sleep(dc, dsi) < 0) {
			printk(KERN_ERR "DSI failed to enter deep sleep\n");
			goto fail;
		}
	} else {
		if (dsi->info.dsi_early_suspend_cmd) {
			err = tegra_dsi_send_panel_cmd(dc, dsi,
				dsi->info.dsi_early_suspend_cmd,
				dsi->info.n_early_suspend_cmd);
			if (err < 0) {
				dev_err(&dc->ndev->dev,
				"dsi: Error sending early suspend cmd\n");
				goto fail;
			}
		}

		if (!dsi->ulpm) {
			if (tegra_dsi_enter_ulpm(dsi) < 0) {
				printk(KERN_ERR "DSI failed to enter ulpm\n");
				goto fail;
			}
		}
	}

fail:
	mutex_unlock(&dsi->lock);
	tegra_dc_io_end(dc);
}

#ifdef CONFIG_PM
static void tegra_dc_dsi_suspend(struct tegra_dc *dc)
{
	struct tegra_dc_dsi_data *dsi;

	dsi = tegra_dc_get_outdata(dc);

	tegra_dc_io_start(dc);
	mutex_lock(&dsi->lock);

	if (!dsi->enabled)
		goto fail;

	if (!dsi->info.power_saving_suspend) {
		if (dsi->ulpm) {
			if (tegra_dsi_exit_ulpm(dsi) < 0) {
				printk(KERN_ERR "DSI failed to exit ulpm");
				goto fail;
			}
		}

		if (tegra_dsi_deep_sleep(dc, dsi) < 0) {
			printk(KERN_ERR "DSI failed to enter deep sleep\n");
			goto fail;
		}
	}
fail:
	mutex_unlock(&dsi->lock);
	tegra_dc_io_end(dc);
}

static void tegra_dc_dsi_resume(struct tegra_dc *dc)
{
	/* Not required since tegra_dc_dsi_enable
	 * will reconfigure the controller from scratch
	 */
}
#endif

struct tegra_dc_out_ops tegra_dc_dsi_ops = {
	.init = tegra_dc_dsi_init,
	.destroy = tegra_dc_dsi_destroy,
	.enable = tegra_dc_dsi_enable,
	.disable = tegra_dc_dsi_disable,
#ifdef CONFIG_PM
	.suspend = tegra_dc_dsi_suspend,
	.resume = tegra_dc_dsi_resume,
#endif
};
