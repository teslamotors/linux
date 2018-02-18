/*
 * drivers/video/tegra/dc/dc_priv.h
 *
 * Copyright (C) 2010 Google, Inc.
 * Author: Erik Gilling <konkers@android.com>
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

#ifndef __DRIVERS_VIDEO_TEGRA_DC_DC_PRIV_H
#define __DRIVERS_VIDEO_TEGRA_DC_DC_PRIV_H

#include "dc_priv_defs.h"
#ifndef CREATE_TRACE_POINTS
# include <trace/events/display.h>
#define WIN_IS_BLOCKLINEAR(win)	((win)->flags & TEGRA_WIN_FLAG_BLOCKLINEAR)
#endif
#include <linux/tegra-powergate.h>
#include <video/tegra_dc_ext.h>
#include <soc/tegra/tegra_bpmp.h>

#if defined(CONFIG_ARCH_TEGRA_18x_SOC) || defined(CONFIG_ARCH_TEGRA_210_SOC)
#include <linux/clk-provider.h>
#endif

#define WIN_IS_TILED(win)	((win)->flags & TEGRA_WIN_FLAG_TILED)
#define WIN_IS_ENABLED(win)	((win)->flags & TEGRA_WIN_FLAG_ENABLED)
#define WIN_IS_FB(win)		((win)->flags & TEGRA_WIN_FLAG_FB)

#define WIN_IS_INTERLACE(win) ((win)->flags & TEGRA_WIN_FLAG_INTERLACE)

#if defined(CONFIG_ARCH_TEGRA_14x_SOC)
#define WIN_ALL_ACT_REQ (WIN_A_ACT_REQ | WIN_B_ACT_REQ | WIN_C_ACT_REQ | \
	WIN_D_ACT_REQ | WIN_H_ACT_REQ)
#else
#define WIN_ALL_ACT_REQ (WIN_A_ACT_REQ | WIN_B_ACT_REQ | WIN_C_ACT_REQ)
#endif

#ifdef CONFIG_TEGRA_NVDISPLAY
int tegra_nvdisp_powergate_partition(int pg_id);
int tegra_nvdisp_unpowergate_partition(int pg_id);
int tegra_nvdisp_set_compclk(struct tegra_dc *dc);
int tegra_nvdisp_is_powered(int pg_id);
#endif

static inline int tegra_dc_io_start(struct tegra_dc *dc)
{
	int ret = 0;
	ret = nvhost_module_busy_ext(dc->ndev);
	if (ret < 0) {
		dev_warn(&dc->ndev->dev,
			"Host1x powerup failed with err=%d\n", ret);
	}
	return ret;
}

static inline void tegra_dc_io_end(struct tegra_dc *dc)
{
	nvhost_module_idle_ext(dc->ndev);
}

static inline int tegra_dc_is_clk_enabled(struct clk *clk)
{
#if defined(CONFIG_ARCH_TEGRA_18x_SOC) || defined(CONFIG_ARCH_TEGRA_210_SOC)
	return __clk_get_enable_count(clk);
#else
	return tegra_is_clk_enabled(clk);
#endif
}

static inline unsigned long tegra_dc_is_accessible(struct tegra_dc *dc)
{
#if !defined(CONFIG_TEGRA_NVDISPLAY)
	if (likely(tegra_platform_is_silicon())) {
		BUG_ON(!nvhost_module_powered_ext(dc->ndev));
		if (WARN(!tegra_dc_is_clk_enabled(dc->clk),
			"DC is clock-gated.\n") ||
			WARN(!tegra_powergate_is_powered(
			dc->powergate_id), "DC is power-gated.\n"))
			return 1;
	}
#endif
	return 0;
}

static inline unsigned long tegra_dc_readl(struct tegra_dc *dc,
					   unsigned long reg)
{
	unsigned long ret;

	if (tegra_dc_is_accessible(dc))
		return 0;

	ret = readl(dc->base + reg * 4);
	trace_display_readl(dc, ret, dc->base + reg * 4);
	return ret;
}

static inline void tegra_dc_writel(struct tegra_dc *dc, unsigned long val,
				   unsigned long reg)
{
	if (tegra_dc_is_accessible(dc))
		return;

	trace_display_writel(dc, val, dc->base + reg * 4);
	writel(val, dc->base + reg * 4);
}

static inline void tegra_dc_power_on(struct tegra_dc *dc)
{
#if !defined(CONFIG_ARCH_TEGRA_18x_SOC)
	tegra_dc_writel(dc, PW0_ENABLE | PW1_ENABLE | PW2_ENABLE | PW3_ENABLE |
					PW4_ENABLE | PM0_ENABLE | PM1_ENABLE,
					DC_CMD_DISPLAY_POWER_CONTROL);
#endif
}

static inline void _tegra_dc_write_table(struct tegra_dc *dc, const u32 *table,
					 unsigned len)
{
	int i;

	for (i = 0; i < len; i++)
		tegra_dc_writel(dc, table[i * 2 + 1], table[i * 2]);
}

#define tegra_dc_write_table(dc, table)		\
	_tegra_dc_write_table(dc, table, ARRAY_SIZE(table) / 2)

static inline void tegra_dc_set_outdata(struct tegra_dc *dc, void *data)
{
	dc->out_data = data;
}

static inline void *tegra_dc_get_outdata(struct tegra_dc *dc)
{
	return dc->out_data;
}

static inline unsigned long tegra_dc_get_default_emc_clk_rate(
	struct tegra_dc *dc)
{
	return dc->pdata->emc_clk_rate ? dc->pdata->emc_clk_rate : ULONG_MAX;
}

/* return the color format field */
static inline int tegra_dc_fmt(int fmt)
{
	return (fmt & TEGRA_DC_EXT_FMT_MASK) >> TEGRA_DC_EXT_FMT_SHIFT;
}

/* return the byte swap field */
static inline int tegra_dc_fmt_byteorder(int fmt)
{
	return (fmt & TEGRA_DC_EXT_FMT_BYTEORDER_MASK) >>
		TEGRA_DC_EXT_FMT_BYTEORDER_SHIFT;
}

static inline int tegra_dc_which_sor(struct tegra_dc *dc)
{
#ifndef CONFIG_TEGRA_NVDISPLAY
	/* Till support is added to Previous chip */
	if (dc->out->type == TEGRA_DC_OUT_HDMI) {
		return 1;
	}
	return dc->ndev->id;
#else
	return dc->sor_instance;
#endif
}

#define BUF_SIZE	256
static inline char *tegra_dc_update_base_name(struct tegra_dc *dc,
						const char *base_name)
{
	char *buf;
	int head_num;

	if (!dc || !base_name)
		return NULL;

	head_num = dc->ctrl_num;
	buf = kzalloc(BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return NULL;

	snprintf(buf, BUF_SIZE, "%s%d", base_name, head_num);
	return buf;
}
#undef BUF_SIZE

static inline int tegra_dc_fmt_bpp(int fmt)
{
	switch (tegra_dc_fmt(fmt)) {
	case TEGRA_WIN_FMT_P1:
		return 1;

	case TEGRA_WIN_FMT_P2:
		return 2;

	case TEGRA_WIN_FMT_P4:
		return 4;

	case TEGRA_WIN_FMT_P8:
		return 8;

	case TEGRA_WIN_FMT_B4G4R4A4:
	case TEGRA_WIN_FMT_B5G5R5A:
	case TEGRA_WIN_FMT_B5G6R5:
	case TEGRA_WIN_FMT_AB5G5R5:
	case TEGRA_WIN_FMT_T_R4G4B4A4:
		return 16;

	case TEGRA_WIN_FMT_B8G8R8A8:
	case TEGRA_WIN_FMT_R8G8B8A8:
	case TEGRA_WIN_FMT_B6x2G6x2R6x2A8:
	case TEGRA_WIN_FMT_R6x2G6x2B6x2A8:
	case TEGRA_WIN_FMT_T_A2R10G10B10:
	case TEGRA_WIN_FMT_T_A2B10G10R10:
	case TEGRA_WIN_FMT_T_X2BL10GL10RL10_XRBIAS:
	case TEGRA_WIN_FMT_T_X2BL10GL10RL10_XVYCC:
		return 32;

	/* for planar formats, size of the Y plane, 8bit */
	case TEGRA_WIN_FMT_YCbCr420P:
	case TEGRA_WIN_FMT_YUV420P:
	case TEGRA_WIN_FMT_YCbCr422P:
	case TEGRA_WIN_FMT_YUV422P:
	case TEGRA_WIN_FMT_YCbCr422R:
	case TEGRA_WIN_FMT_YUV422R:
	case TEGRA_WIN_FMT_YCbCr422RA:
	case TEGRA_WIN_FMT_YUV422RA:
	case TEGRA_WIN_FMT_YCbCr444P:
	case TEGRA_WIN_FMT_YUV444P:
	case TEGRA_WIN_FMT_YUV422SP:
	case TEGRA_WIN_FMT_YUV420SP:
	case TEGRA_WIN_FMT_YCbCr420SP:
	case TEGRA_WIN_FMT_YCbCr422SP:
		return 8;

	case TEGRA_WIN_FMT_T_Y10___U10___V10_N420:
	case TEGRA_WIN_FMT_T_Y10___U10___V10_N444:
	case TEGRA_WIN_FMT_T_Y10___V10U10_N420:
	case TEGRA_WIN_FMT_T_Y10___U10V10_N422:
	case TEGRA_WIN_FMT_T_Y10___U10V10_N422R:
	case TEGRA_WIN_FMT_T_Y10___U10V10_N444:
	case TEGRA_WIN_FMT_T_Y12___U12___V12_N420:
	case TEGRA_WIN_FMT_T_Y12___U12___V12_N444:
	case TEGRA_WIN_FMT_T_Y12___V12U12_N420:
	case TEGRA_WIN_FMT_T_Y12___U12V12_N422:
	case TEGRA_WIN_FMT_T_Y12___U12V12_N422R:
	case TEGRA_WIN_FMT_T_Y12___U12V12_N444:
		return 16;

	/* YUV packed into 32-bits */
	case TEGRA_WIN_FMT_YCbCr422:
	case TEGRA_WIN_FMT_YUV422:
		return 16;

	/* RGB with 64-bits size */
	case TEGRA_WIN_FMT_T_R16_G16_B16_A16:
		return 64;

	}
	return 0;
}

static inline bool tegra_dc_is_yuv(int fmt)
{
	switch (tegra_dc_fmt(fmt)) {
	case TEGRA_WIN_FMT_YUV420P:
	case TEGRA_WIN_FMT_YCbCr420P:
	case TEGRA_WIN_FMT_YCbCr422P:
	case TEGRA_WIN_FMT_YUV422P:
	case TEGRA_WIN_FMT_YCbCr422:
	case TEGRA_WIN_FMT_YUV422:
	case TEGRA_WIN_FMT_YCbCr422R:
	case TEGRA_WIN_FMT_YUV422R:
	case TEGRA_WIN_FMT_YCbCr422RA:
	case TEGRA_WIN_FMT_YUV422RA:
	case TEGRA_WIN_FMT_YCbCr444P:
	case TEGRA_WIN_FMT_YUV444P:
	case TEGRA_WIN_FMT_YUV422SP:
	case TEGRA_WIN_FMT_YCbCr422SP:
	case TEGRA_WIN_FMT_YCbCr420SP:
	case TEGRA_WIN_FMT_YUV420SP:

	case TEGRA_WIN_FMT_T_Y10___U10___V10_N420:
	case TEGRA_WIN_FMT_T_Y10___U10___V10_N444:
	case TEGRA_WIN_FMT_T_Y10___V10U10_N420:
	case TEGRA_WIN_FMT_T_Y10___U10V10_N422:
	case TEGRA_WIN_FMT_T_Y10___U10V10_N422R:
	case TEGRA_WIN_FMT_T_Y10___U10V10_N444:
	case TEGRA_WIN_FMT_T_Y12___U12___V12_N420:
	case TEGRA_WIN_FMT_T_Y12___U12___V12_N444:
	case TEGRA_WIN_FMT_T_Y12___V12U12_N420:
	case TEGRA_WIN_FMT_T_Y12___U12V12_N422:
	case TEGRA_WIN_FMT_T_Y12___U12V12_N422R:
	case TEGRA_WIN_FMT_T_Y12___U12V12_N444:

		return true;
	}
	return false;
}

static inline bool tegra_dc_is_yuv_planar(int fmt)
{
	switch (tegra_dc_fmt(fmt)) {
	case TEGRA_WIN_FMT_YUV420P:
	case TEGRA_WIN_FMT_YCbCr420P:
	case TEGRA_WIN_FMT_YCbCr422P:
	case TEGRA_WIN_FMT_YUV422P:
	case TEGRA_WIN_FMT_YCbCr422R:
	case TEGRA_WIN_FMT_YUV422R:
	case TEGRA_WIN_FMT_YCbCr422RA:
	case TEGRA_WIN_FMT_YUV422RA:
	case TEGRA_WIN_FMT_YCbCr444P:
	case TEGRA_WIN_FMT_YUV444P:
	case TEGRA_WIN_FMT_T_Y10___U10___V10_N420:
	case TEGRA_WIN_FMT_T_Y10___U10___V10_N444:
	case TEGRA_WIN_FMT_T_Y10___V10U10_N420:
	case TEGRA_WIN_FMT_T_Y10___U10V10_N422:
	case TEGRA_WIN_FMT_T_Y10___U10V10_N422R:
	case TEGRA_WIN_FMT_T_Y10___U10V10_N444:
	case TEGRA_WIN_FMT_T_Y12___U12___V12_N420:
	case TEGRA_WIN_FMT_T_Y12___U12___V12_N444:
	case TEGRA_WIN_FMT_T_Y12___V12U12_N420:
	case TEGRA_WIN_FMT_T_Y12___U12V12_N422:
	case TEGRA_WIN_FMT_T_Y12___U12V12_N422R:
	case TEGRA_WIN_FMT_T_Y12___U12V12_N444:
		return true;
	}
	return false;
}

static inline bool tegra_dc_is_yuv_full_planar(int fmt)
{
	switch (fmt) {
	case TEGRA_WIN_FMT_YCbCr444P:
	case TEGRA_WIN_FMT_YUV444P:
	case TEGRA_WIN_FMT_T_Y10___U10___V10_N420:
	case TEGRA_WIN_FMT_T_Y10___U10___V10_N444:
	case TEGRA_WIN_FMT_T_Y12___U12___V12_N420:
	case TEGRA_WIN_FMT_T_Y12___U12___V12_N444:
		return true;
	}
	return false;
}

static inline bool tegra_dc_is_yuv_semi_planar(int fmt)
{
	switch (fmt) {
	case TEGRA_WIN_FMT_YUV420SP:
	case TEGRA_WIN_FMT_YCbCr420SP:
	case TEGRA_WIN_FMT_YCbCr422SP:
	case TEGRA_WIN_FMT_YUV422SP:
	case TEGRA_WIN_FMT_T_Y10___V10U10_N420:
	case TEGRA_WIN_FMT_T_Y10___U10V10_N422:
	case TEGRA_WIN_FMT_T_Y10___U10V10_N422R:
	case TEGRA_WIN_FMT_T_Y10___U10V10_N444:
	case TEGRA_WIN_FMT_T_Y12___V12U12_N420:
	case TEGRA_WIN_FMT_T_Y12___U12V12_N422:
	case TEGRA_WIN_FMT_T_Y12___U12V12_N422R:
	case TEGRA_WIN_FMT_T_Y12___U12V12_N444:
		return true;
	}
	return false;
}

static inline u32 tegra_dc_unmask_interrupt(struct tegra_dc *dc, u32 int_val)
{
	u32 val;

	val = tegra_dc_readl(dc, DC_CMD_INT_MASK);
	tegra_dc_writel(dc, val | int_val, DC_CMD_INT_MASK);
	return val;
}

static inline u32 tegra_dc_flush_interrupt(struct tegra_dc *dc, u32 val)
{
	unsigned long flag;

	local_irq_save(flag);

	tegra_dc_writel(dc, val, DC_CMD_INT_STATUS);

	local_irq_restore(flag);

	return val;
}

static inline u32 tegra_dc_mask_interrupt(struct tegra_dc *dc, u32 int_val)
{
	u32 val;

	val = tegra_dc_readl(dc, DC_CMD_INT_MASK);
	tegra_dc_writel(dc, val & ~int_val, DC_CMD_INT_MASK);
	return val;
}

static inline void tegra_dc_restore_interrupt(struct tegra_dc *dc, u32 val)
{
	tegra_dc_writel(dc, val, DC_CMD_INT_MASK);
}

static inline int tegra_dc_clk_set_rate(struct tegra_dc *dc, unsigned long rate)
{
#if !defined(CONFIG_ARCH_TEGRA_18x_SOC)
	return 0;
#else
	if (!tegra_platform_is_silicon() || !tegra_bpmp_running())
		return 0;

	if (clk_set_rate(dc->clk, rate)) {
		dev_err(&dc->ndev->dev, "Failed to set dc clk to %ld\n", rate);
		return -EINVAL;
	}

	tegra_nvdisp_set_compclk(dc);
#endif
	return 0;
}

static inline unsigned long tegra_dc_clk_get_rate(struct tegra_dc *dc)
{
#if defined(CONFIG_ARCH_TEGRA_18x_SOC)
	if (!tegra_platform_is_silicon() || !tegra_bpmp_running())
#else
	if (!tegra_platform_is_silicon())
#endif
		return dc->mode.pclk;

	return clk_get_rate(dc->clk);
}

static inline int tegra_disp_clk_prepare_enable(struct clk *clk)
{
#if defined(CONFIG_ARCH_TEGRA_18x_SOC)
	if (tegra_platform_is_silicon() && tegra_bpmp_running())
#else
	if (tegra_platform_is_silicon())
#endif
		return clk_prepare_enable(clk);

	return 0;
}

static inline void tegra_disp_clk_disable_unprepare(struct clk *clk)
{
#if defined(CONFIG_ARCH_TEGRA_18x_SOC)
	if (tegra_platform_is_silicon() && tegra_bpmp_running())
#else
	if (tegra_platform_is_silicon())
#endif
		clk_disable_unprepare(clk);
}

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && !defined(CONFIG_ARCH_TEGRA_3x_SOC) \
	&& IS_ENABLED(CONFIG_PM_GENERIC_DOMAINS)
static inline void tegra_dc_powergate_locked(struct tegra_dc *dc)
{
#if defined(CONFIG_ARCH_TEGRA_18x_SOC)
	tegra_nvdisp_powergate_partition(dc->powergate_id);
#else
	tegra_powergate_partition(dc->powergate_id);
#endif
}

static inline void tegra_dc_unpowergate_locked(struct tegra_dc *dc)
{
	int ret;
#if defined(CONFIG_ARCH_TEGRA_18x_SOC)
	ret = tegra_nvdisp_unpowergate_partition(dc->powergate_id);
#else
	ret = tegra_unpowergate_partition(dc->powergate_id);
#endif
	if (ret < 0)
		dev_err(&dc->ndev->dev, "%s: could not unpowergate %d\n",
							__func__, ret);
}

static inline bool tegra_dc_is_powered(struct tegra_dc *dc)
{
#if defined(CONFIG_TEGRA_NVDISPLAY)
	if (tegra_platform_is_linsim())
		return true;
	return tegra_nvdisp_is_powered(dc->powergate_id);
#endif
	return tegra_powergate_is_powered(dc->powergate_id);
}

void tegra_dc_powergate_locked(struct tegra_dc *dc);
void tegra_dc_unpowergate_locked(struct tegra_dc *dc);
#else
static inline void tegra_dc_powergate_locked(struct tegra_dc *dc) { }
static inline void tegra_dc_unpowergate_locked(struct tegra_dc *dc) { }
static inline bool tegra_dc_is_powered(struct tegra_dc *dc)
{
	return true;
}
#endif

static inline void tegra_dc_set_edid(struct tegra_dc *dc,
	struct tegra_edid *edid)
{
	dc->edid = edid;
}


#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
static inline u32 tegra_dc_reg_l32(dma_addr_t v)
{
	return v & 0xffffffff;
}

static inline u32 tegra_dc_reg_h32(dma_addr_t v)
{
	return v >> 32;
}
#else
static inline u32 tegra_dc_reg_l32(dma_addr_t v)
{
	return v;
}

static inline u32 tegra_dc_reg_h32(dma_addr_t v)
{
	return 0;
}
#endif
extern struct tegra_dc_out_ops tegra_dc_rgb_ops;
extern struct tegra_dc_out_ops tegra_dc_dsi_ops;

#if defined(CONFIG_TEGRA_HDMI2_0)
extern struct tegra_dc_out_ops tegra_dc_hdmi2_0_ops;
#elif defined(CONFIG_TEGRA_HDMI)
extern struct tegra_dc_out_ops tegra_dc_hdmi_ops;
#endif

#ifdef CONFIG_TEGRA_DP
extern struct tegra_dc_out_ops tegra_dc_dp_ops;
#endif
#ifdef CONFIG_TEGRA_LVDS
extern struct tegra_dc_out_ops tegra_dc_lvds_ops;
#endif
#ifdef CONFIG_TEGRA_NVSR
extern struct tegra_dc_out_ops tegra_dc_nvsr_ops;
#endif

extern struct tegra_dc_out_ops tegra_dc_null_ops;

/* defined in dc_sysfs.c, used by dc.c */
void tegra_dc_remove_sysfs(struct device *dev);
void tegra_dc_create_sysfs(struct device *dev);

/* defined in dc.c, used by dc_sysfs.c */
void tegra_dc_stats_enable(struct tegra_dc *dc, bool enable);
bool tegra_dc_stats_get(struct tegra_dc *dc);

/* defined in dc.c, used by dc_sysfs.c */
u32 tegra_dc_read_checksum_latched(struct tegra_dc *dc);
void tegra_dc_enable_crc(struct tegra_dc *dc);
void tegra_dc_disable_crc(struct tegra_dc *dc);

void tegra_dc_set_out_pin_polars(struct tegra_dc *dc,
				const struct tegra_dc_out_pin *pins,
				const unsigned int n_pins);
/* defined in dc.c, used in bandwidth.c and ext/dev.c */
unsigned int tegra_dc_has_multiple_dc(void);

/* defined in dc.c, used in hdmihdcp.c */
int tegra_dc_ddc_enable(struct tegra_dc *dc, bool enabled);

/* defined in dc.c, used in dsi.c */
void tegra_dc_clk_enable(struct tegra_dc *dc);
void tegra_dc_clk_disable(struct tegra_dc *dc);

/* defined in dc.c, used in nvsd.c and dsi.c */
void tegra_dc_get(struct tegra_dc *dc);
void tegra_dc_put(struct tegra_dc *dc);

/* defined in dc.c, used in tegra_adf.c */
void tegra_dc_hold_dc_out(struct tegra_dc *dc);
void tegra_dc_release_dc_out(struct tegra_dc *dc);

/* defined in dc.c, used in ext/dev.c */
void tegra_dc_call_flip_callback(void);

/* defined in dc.c, used in sor.c */
unsigned long tegra_dc_poll_register(struct tegra_dc *dc,
u32 reg, u32 mask, u32 exp_val, u32 poll_interval_us,
u32 timeout_ms);
void tegra_dc_enable_general_act(struct tegra_dc *dc);

/* defined in dc.c, used in dsi.c */
void tegra_dc_dsc_init(struct tegra_dc *dc);
void tegra_dc_en_dis_dsc(struct tegra_dc *dc, bool enable);

/* defined in dc.c, used by ext/dev.c */
extern int no_vsync;

/* defined in dc.c, used in ext/dev.c */
int tegra_dc_config_frame_end_intr(struct tegra_dc *dc, bool enable);

/* defined in dc.c, used in dsi.c */
int _tegra_dc_wait_for_frame_end(struct tegra_dc *dc,
	u32 timeout_ms);

/* defined in bandwidth.c, used in dc.c */
void tegra_dc_clear_bandwidth(struct tegra_dc *dc);
void tegra_dc_program_bandwidth(struct tegra_dc *dc, bool use_new);
int tegra_dc_set_dynamic_emc(struct tegra_dc *dc);
#ifdef CONFIG_TEGRA_ISOMGR
int tegra_dc_bandwidth_negotiate_bw(struct tegra_dc *dc,
			struct tegra_dc_win *windows[], int n);
void tegra_dc_bandwidth_renegotiate(void *p, u32 avail_bw);
#endif
unsigned long tegra_dc_get_bandwidth(struct tegra_dc_win *windows[], int n);
long tegra_dc_calc_min_bandwidth(struct tegra_dc *dc);

/* defined in mode.c, used in dc.c, window.c and hdmi2.0.c */
int tegra_dc_program_mode(struct tegra_dc *dc, struct tegra_dc_mode *mode);
int tegra_dc_calc_refresh(const struct tegra_dc_mode *m);
int tegra_dc_calc_fb_refresh(const struct fb_videomode *fbmode);
int tegra_dc_update_mode(struct tegra_dc *dc);
u32 tegra_dc_get_aspect_ratio(struct tegra_dc *dc);

/* defined in mode.c, used in hdmi.c and hdmi2.0.c */
bool check_fb_videomode_timings(const struct tegra_dc *dc,
				const struct fb_videomode *fbmode);

/* defined in mode.c, used in nvsr.c */
int _tegra_dc_set_mode(struct tegra_dc *dc, const struct tegra_dc_mode *mode);

/* defined in clock.c, used in dc.c, rgb.c, dsi.c and hdmi.c */
void tegra_dc_setup_clk(struct tegra_dc *dc, struct clk *clk);
unsigned long tegra_dc_pclk_round_rate(struct tegra_dc *dc, int pclk);
unsigned long tegra_dc_pclk_predict_rate(
	int out_type, struct clk *parent, int pclk);

/* defined in lut.c, used in dc.c */
void tegra_dc_init_lut_defaults(struct tegra_dc_lut *lut);
void tegra_dc_set_lut(struct tegra_dc *dc, struct tegra_dc_win *win);

/* defined in csc.c, used in dc.c */
void tegra_dc_init_csc_defaults(struct tegra_dc_csc *csc);
void tegra_dc_set_csc(struct tegra_dc *dc, struct tegra_dc_csc *csc);

/* defined in window.c, used in dc.c and nvdisp_win.c */
void tegra_dc_trigger_windows(struct tegra_dc *dc);
bool update_is_hsync_safe(struct tegra_dc_win *cur_win,
	struct tegra_dc_win *new_win);

void tegra_dc_set_color_control(struct tegra_dc *dc);
#if defined(CONFIG_TEGRA_DC_CMU) || defined(CONFIG_TEGRA_DC_CMU_V2)
void tegra_dc_cmu_enable(struct tegra_dc *dc, bool cmu_enable);
#endif
#ifdef CONFIG_TEGRA_DC_CMU
void _tegra_dc_cmu_enable(struct tegra_dc *dc, bool cmu_enable);
#endif

#ifdef CONFIG_TEGRA_DC_CMU
int tegra_dc_update_cmu(struct tegra_dc *dc, struct tegra_dc_cmu *cmu);
int tegra_dc_update_cmu_aligned(struct tegra_dc *dc, struct tegra_dc_cmu *cmu);
#endif

int tegra_dc_set_hdr(struct tegra_dc *dc, struct tegra_dc_hdr *hdr,
					bool cache_dirty);

struct device_node *tegra_get_panel_node_out_type_check
	(struct tegra_dc *dc, u32 out_type);

struct tegra_dc_platform_data
	*of_dc_parse_platform_data(struct platform_device *ndev);

/* defined in dc.c, used in dc.c and dev.c */
void tegra_dc_set_act_vfp(struct tegra_dc *dc, int vfp);

/* defined in dc.c, used in dc.c and window.c */
bool tegra_dc_windows_are_dirty(struct tegra_dc *dc, u32 win_act_req_mask);
int tegra_dc_get_v_count(struct tegra_dc *dc);

/* defined in dc.c, used in vrr.c */
s32 tegra_dc_calc_v_front_porch(struct tegra_dc_mode *mode,
				int desired_fps);

/* defined in cursor.c, used in dc.c and ext/cursor.c */
int tegra_dc_cursor_image(struct tegra_dc *dc,
	enum tegra_dc_cursor_blend_format blendfmt,
	enum tegra_dc_cursor_size size,
	u32 fg, u32 bg, dma_addr_t phys_addr,
	enum tegra_dc_cursor_color_format colorfmt, u32 alpha, u32 flags);
int tegra_dc_cursor_set(struct tegra_dc *dc, bool enable, int x, int y);
int tegra_dc_cursor_clip(struct tegra_dc *dc, unsigned clip);
int tegra_dc_cursor_suspend(struct tegra_dc *dc);
int tegra_dc_cursor_resume(struct tegra_dc *dc);
void tegra_dc_win_partial_update(struct tegra_dc *dc, struct tegra_dc_win *win,
	unsigned int xoff, unsigned int yoff, unsigned int width,
	unsigned int height);
int tegra_dc_slgc_disp0(struct notifier_block *nb, unsigned long unused0,
	void *unused1);

/* defined in dc.c, used in dc_sysfs.c and ext/dev.c */
int tegra_dc_update_winmask(struct tegra_dc *dc, unsigned long winmask);

/* common display clock calls */
struct clk *tegra_disp_clk_get(struct device *dev, const char *id);
void tegra_disp_clk_put(struct device *dev, struct clk *clk);
struct clk *tegra_disp_of_clk_get_by_name(struct device_node *np,
						const char *name);

#ifdef CONFIG_TEGRA_NVDISPLAY
int tegra_nvdisp_init(struct tegra_dc *dc);
int tegra_nvdisp_update_windows(struct tegra_dc *dc,
	struct tegra_dc_win *windows[], int n,
	u16 *dirty_rect, bool wait_for_vblank);
int tegra_nvdisp_assign_win(struct tegra_dc *dc, unsigned idx);
int tegra_nvdisp_detach_win(struct tegra_dc *dc, unsigned idx);
int tegra_nvdisp_head_enable(struct tegra_dc *dc);
int tegra_nvdisp_head_disable(struct tegra_dc *dc);
int tegra_nvdisp_get_linestride(struct tegra_dc *dc, int win);
void tegra_nvdisp_enable_crc(struct tegra_dc *dc);
void tegra_nvdisp_disable_crc(struct tegra_dc *dc);
u32 tegra_nvdisp_read_rg_crc(struct tegra_dc *dc);
int tegra_nvdisp_program_mode(struct tegra_dc *dc,
			struct tegra_dc_mode *mode);
void tegra_nvdisp_underflow_handler(struct tegra_dc *dc);
int tegra_nvdisp_set_compclk(struct tegra_dc *dc);
int tegra_nvdisp_test_and_set_compclk(unsigned long rate,
					struct tegra_dc *dc);
void reg_dump(struct tegra_dc *dc, void *data,
	void (*print)(void *data, const char *str));

u32 tegra_nvdisp_ihub_read(struct tegra_dc *dc, int win_num, int ihub_switch);

struct tegra_fb_info *tegra_nvdisp_fb_register(struct platform_device *ndev,
	struct tegra_dc *dc, struct tegra_fb_data *fb_data,
	struct resource *fb_mem);

void nvdisp_dc_feature_register(struct tegra_dc *dc);
int nvdisp_set_cursor_position(struct tegra_dc *dc, s16 x, s16 y);
int nvdisp_set_cursor_colorfmt(struct tegra_dc *dc);
int tegra_nvdisp_set_output_lut(struct tegra_dc *dc,
					struct tegra_dc_lut *lut);
int tegra_nvdisp_update_cmu(struct tegra_dc *dc, struct tegra_dc_lut *lut);
void tegra_dc_cache_cmu(struct tegra_dc *dc, struct tegra_dc_cmu *src_cmu);
void tegra_nvdisp_get_default_cmu(struct tegra_dc_cmu *default_cmu);
int tegra_nvdisp_process_imp_results(struct tegra_dc *dc,
			struct tegra_dc_ext_flip_user_data *flip_user_data);
void tegra_nvdisp_handle_common_channel_promotion(struct tegra_dc *dc);
void tegra_nvdisp_complete_imp_programming(struct tegra_dc *dc);
void tegra_nvdisp_get_imp_user_info(struct tegra_dc *dc,
				struct tegra_dc_ext_imp_user_info *info);
int nvdisp_register_backlight_notifier(struct tegra_dc *dc);
void tegra_nvdisp_set_vrr_mode(struct tegra_dc *dc);
void tegra_nvdisp_stop_display(struct tegra_dc *dc);
#ifdef CONFIG_TEGRA_ISOMGR
void tegra_nvdisp_isomgr_attach(struct tegra_dc *dc);
int tegra_nvdisp_isomgr_register(enum tegra_iso_client client, u32 udedi_bw);
void tegra_nvdisp_isomgr_unregister(void);
#endif
#if defined(CONFIG_TEGRA_CSC_V2)
void tegra_nvdisp_init_csc_defaults(struct tegra_dc_csc_v2 *csc);
#endif
void tegra_nvdisp_vrr_work(struct work_struct *work);

int tegra_nvdisp_set_chroma_lpf(struct tegra_dc *dc);
int tegra_nvdisp_set_ocsc(struct tegra_dc *dc, struct tegra_dc_mode *mode);
#endif

bool fb_console_mapped(void);

#endif
