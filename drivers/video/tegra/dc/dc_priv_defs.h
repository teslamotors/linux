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

#ifndef __DRIVERS_VIDEO_TEGRA_DC_DC_PRIV_DEFS_H
#define __DRIVERS_VIDEO_TEGRA_DC_DC_PRIV_DEFS_H
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/fb.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/switch.h>
#include <linux/nvhost.h>
#include <linux/types.h>
#include <linux/clk/tegra.h>
#include <linux/tegra-soc.h>
#include <linux/reset.h>

#include <mach/dc.h>

#include <mach/tegra_dc_ext.h>
#include <linux/platform/tegra/isomgr.h>

#include "dc_reg.h"

#define NEED_UPDATE_EMC_ON_EVERY_FRAME (windows_idle_detection_time == 0)

/* 28 bit offset for window clip number */
#define CURSOR_CLIP_SHIFT_BITS(win)	(win << 28)
#define CURSOR_CLIP_GET_WINDOW(reg)	((reg >> 28) & 3)

#define BLANK_ALL	(~0)

static inline u32 ALL_UF_INT(void)
{
	if (tegra_platform_is_fpga())
		return 0;
#if defined(CONFIG_ARCH_TEGRA_2x_SOC) || \
	defined(CONFIG_ARCH_TEGRA_3x_SOC) || \
	defined(CONFIG_ARCH_TEGRA_11x_SOC)
	return WIN_A_UF_INT | WIN_B_UF_INT | WIN_C_UF_INT;
#elif defined(CONFIG_TEGRA_NVDISPLAY)
	return NVDISP_UF_INT;
#else
	return WIN_A_UF_INT | WIN_B_UF_INT | WIN_C_UF_INT | HC_UF_INT |
		WIN_D_UF_INT | WIN_T_UF_INT;
#endif
}

#if defined(CONFIG_TEGRA_EMC_TO_DDR_CLOCK)
#define EMC_BW_TO_FREQ(bw) (DDR_BW_TO_FREQ(bw) * CONFIG_TEGRA_EMC_TO_DDR_CLOCK)
#else
#define EMC_BW_TO_FREQ(bw) (DDR_BW_TO_FREQ(bw) * 2)
#endif

struct tegra_dc;

struct tegra_dc_blend {
	unsigned z[DC_N_WINDOWS];
	unsigned flags[DC_N_WINDOWS];
	u8 alpha[DC_N_WINDOWS];
};

struct tegra_dc_out_ops {
	/* initialize output.  dc clocks are not on at this point */
	int (*init)(struct tegra_dc *dc);
	/* destroy output.  dc clocks are not on at this point */
	void (*destroy)(struct tegra_dc *dc);
	/* shutdown output.  dc clocks are on at this point */
	void (*shutdown)(struct tegra_dc *dc);
	/* detect connected display.  can sleep.*/
	bool (*detect)(struct tegra_dc *dc);
	/* enable output.  dc clocks are on at this point */
	void (*enable)(struct tegra_dc *dc);
	/* enable dc client.  Panel is enable at this point */
	void (*postpoweron)(struct tegra_dc *dc);
	/* disable output.  dc clocks are on at this point */
	void (*disable)(struct tegra_dc *dc);
	/* dc client is disabled.  dc clocks are on at this point */
	void (*postpoweroff) (struct tegra_dc *dc);
	/* hold output.  keeps dc clocks on. */
	void (*hold)(struct tegra_dc *dc);
	/* release output.  dc clocks may turn off after this. */
	void (*release)(struct tegra_dc *dc);
	/* idle routine of output.  dc clocks may turn off after this. */
	void (*idle)(struct tegra_dc *dc);
	/* suspend output.  dc clocks are on at this point */
	void (*suspend)(struct tegra_dc *dc);
	/* resume output.  dc clocks are on at this point */
	void (*resume)(struct tegra_dc *dc);
	/* mode filter. to provide a list of supported modes*/
	bool (*mode_filter)(const struct tegra_dc *dc,
			struct fb_videomode *mode);
	/* setup pixel clock and parent clock programming */
	long (*setup_clk)(struct tegra_dc *dc, struct clk *clk);
	/*
	 * return true if display client is suspended during OSidle.
	 * If true, dc will not wait on any display client event
	 * during OSidle.
	 */
	bool (*osidle)(struct tegra_dc *dc);
	/* callback before new mode is programmed.
	 * dc clocks are on at this point */
	void (*modeset_notifier)(struct tegra_dc *dc);
	/* Set up interface and sink for partial frame update.
	 */
	int (*partial_update) (struct tegra_dc *dc, unsigned int *xoff,
		unsigned int *yoff, unsigned int *width, unsigned int *height);
	/* refcounted enable of pads and clocks before performing DDC/I2C. */
	int (*ddc_enable)(struct tegra_dc *dc);
	/* refcounted disable of pads and clocks after performing DDC/I2C. */
	int (*ddc_disable)(struct tegra_dc *dc);
	/* Enable/disable VRR */
	void (*vrr_enable)(struct tegra_dc *dc, bool enable);
	/* Mark VRR-compatible modes in fb_info->info->modelist */
	void (*vrr_update_monspecs)(struct tegra_dc *dc,
		struct list_head *head);
	/* return if hpd asserted or deasserted */
	bool (*hpd_state)(struct tegra_dc *dc);
	/* Configure controller to receive hotplug events */
	int (*hotplug_init)(struct tegra_dc *dc);
	int (*set_hdr)(struct tegra_dc *dc);
	/* shutdown the serial interface */
	void (*shutdown_interface)(struct tegra_dc *dc);
};

struct tegra_dc_shift_clk_div {
	unsigned long mul; /* numerator */
	unsigned long div; /* denominator */
};

struct tegra_dc_nvsr_data;

enum tegra_dc_cursor_size {
	TEGRA_DC_CURSOR_SIZE_32X32 = 0,
	TEGRA_DC_CURSOR_SIZE_64X64 = 1,
	TEGRA_DC_CURSOR_SIZE_128X128 = 2,
	TEGRA_DC_CURSOR_SIZE_256X256 = 3,
};

enum tegra_dc_cursor_blend_format {
	TEGRA_DC_CURSOR_FORMAT_2BIT_LEGACY = 0,
	TEGRA_DC_CURSOR_FORMAT_RGBA_NON_PREMULT_ALPHA = 1,
	TEGRA_DC_CURSOR_FORMAT_RGBA_PREMULT_ALPHA = 3,
	TEGRA_DC_CURSOR_FORMAT_RGBA_XOR = 4,
};

enum tegra_dc_cursor_color_format {
	TEGRA_DC_CURSOR_COLORFMT_LEGACY,   /* 2bpp   */
	TEGRA_DC_CURSOR_COLORFMT_R8G8B8A8, /* normal */
	TEGRA_DC_CURSOR_COLORFMT_A1R5G5B5,
	TEGRA_DC_CURSOR_COLORFMT_A8R8G8B8,
};

#ifdef CONFIG_TEGRA_NVDISPLAY
struct tegra_nvdisp_tg_req {
	int	dc_idx;
	u32	num_wins;
	u32	win_ids[DC_N_WINDOWS];
	u32	tgs[DC_N_WINDOWS];
};
#endif

struct tegra_dc {
	struct platform_device		*ndev;
	struct tegra_dc_platform_data	*pdata;

	struct resource			*base_res;
	void __iomem			*base;
	int				irq;

	struct clk			*clk;
	struct reset_control		*rst;

#if defined(CONFIG_TEGRA_NVDISPLAY) && defined(CONFIG_TEGRA_ISOMGR)
	/* Reference to a single instance */
	struct nvdisp_isoclient_bw_info	*ihub_bw_info;
	bool				la_dirty;
#elif defined(CONFIG_TEGRA_ISOMGR)
	tegra_isomgr_handle		isomgr_handle;
	u32				reserved_bw;
	u32				available_bw;
#else
	struct clk			*emc_clk;
#endif
	struct clk			*emc_la_clk;
	long				bw_kbps; /* bandwidth in KBps */
	long				new_bw_kbps;
	struct tegra_dc_shift_clk_div	shift_clk_div;

	u32				powergate_id;
	int				sor_instance;

	bool				connected;
	bool				enabled;
	bool				suspended;
	bool				blanked;
	bool				shutdown;

	/* Some of the setup code could reset display even if
	 * DC is already by bootloader.  This one-time mark is
	 * used to suppress such code blocks during system boot,
	 * a.k.a the call stack above tegra_dc_probe().
	 */
	bool				initialized;
#ifdef CONFIG_TEGRA_HDMI2FPD
	struct tegra_dc_hdmi2fpd_data   *fpddata;
#endif
	struct tegra_dc_out		*out;
	struct tegra_dc_out_ops		*out_ops;
	void				*out_data;

	struct tegra_dc_mode		mode;
	struct tegra_dc_mode		cached_mode;
	bool				use_cached_mode;
	s64				frametime_ns;

#ifndef CONFIG_TEGRA_NVDISPLAY
	struct tegra_dc_win		windows[DC_N_WINDOWS];
#endif
	struct tegra_dc_win		shadow_windows[DC_N_WINDOWS];

	struct tegra_dc_blend		blend;
	int				n_windows;
	struct tegra_dc_hdr		hdr;

#ifdef CONFIG_TEGRA_NVDISPLAY
	struct tegra_dc_imp_head_results	imp_results[TEGRA_MAX_DC];
	struct tegra_nvdisp_tg_req		tg_reqs[DC_N_WINDOWS];
	bool					common_channel_reserved;
	bool					common_channel_pending;
	bool					new_imp_results_needed;
	bool					need_to_complete_imp;
	bool					new_mempool_needed;
	bool					new_bw_pending;
#endif

#if defined(CONFIG_TEGRA_DC_CMU)
	struct tegra_dc_cmu		cmu;
#elif defined(CONFIG_TEGRA_DC_CMU_V2)
	struct tegra_dc_lut		cmu;
#endif

#if defined(CONFIG_TEGRA_CSC_V2)
	/* either unity or panel specific */
	struct tegra_dc_csc_v2		default_csc;
#endif

#if defined(CONFIG_TEGRA_DC_CMU) || defined(CONFIG_TEGRA_DC_CMU_V2)
	struct tegra_dc_cmu		cmu_shadow;
	bool				cmu_dirty;
	/* Is CMU set by bootloader */
	bool				is_cmu_set_bl;
	bool				cmu_shadow_dirty;
	bool				cmu_shadow_force_update;
	bool				cmu_enabled;
#endif
	wait_queue_head_t		wq;
	wait_queue_head_t		timestamp_wq;

	struct mutex			lp_lock;
	struct mutex			lock;
	struct mutex			one_shot_lock;

	struct resource			*fb_mem;
	struct tegra_fb_info		*fb;
#ifdef CONFIG_ADF_TEGRA
	struct tegra_adf_info		*adf;
#endif

	u32				vblank_syncpt;

	unsigned long int		valid_windows;

	unsigned long			underflow_mask;
	struct work_struct		reset_work;

#ifdef CONFIG_SWITCH
	struct switch_dev		modeset_switch;
#endif

	struct completion		frame_end_complete;
	struct completion		crc_complete;
	bool				crc_pending;

	struct work_struct		vblank_work;
	long				vblank_ref_count;
	struct work_struct		frame_end_work;
	struct work_struct		vpulse2_work;
	long				vpulse2_ref_count;

	struct {
		u64			underflows;
		u64			underflows_a;
		u64			underflows_b;
		u64			underflows_c;
		u64			underflows_d;
		u64			underflows_h;
		u64			underflows_t;
		u64			underflow_frames;
	} stats;

#ifdef CONFIG_TEGRA_DC_EXTENSIONS
	struct tegra_dc_ext		*ext;
#endif

	struct tegra_dc_feature		*feature;
	int				gen1_blend_num;

#ifdef CONFIG_DEBUG_FS
	struct dentry			*debugdir;
#endif
	struct tegra_dc_lut		fb_lut;
	struct delayed_work		underflow_work;
	u32				one_shot_delay_ms;
	struct delayed_work		one_shot_work;
	s64				frame_end_timestamp;
	atomic_t			frame_end_ref;
#ifdef CONFIG_TEGRA_NVDISPLAY
	struct delayed_work		vrr_work;
#endif

	bool				mode_dirty;
	bool				yuv_bypass;
	bool				yuv_bypass_dirty;
	atomic_t			holding;

	struct tegra_dc_win		tmp_wins[DC_N_WINDOWS];

	struct tegra_edid		*edid;

	struct tegra_dc_nvsr_data *nvsr;

	bool	disp_active_dirty;

	struct tegra_dc_cursor {
		bool dirty;
		bool enabled;
		dma_addr_t phys_addr;
		u32 fg;
		u32 bg;
		unsigned clip_win;
		int x;
		int y;
		enum tegra_dc_cursor_size size;
		enum tegra_dc_cursor_blend_format blendfmt;
		enum tegra_dc_cursor_color_format colorfmt;
		u32 alpha;
	} cursor;

	int	ctrl_num;
	bool	switchdev_registered;

	struct notifier_block slgc_notifier;
	bool	vedid;
	u8	*vedid_data;
	atomic_t	enable_count;
	bool	hdr_cache_dirty;
	bool    hotplug_supported;

	/* user call-back for shared ISR */
	int  (*isr_usr_cb)(int dcid, unsigned long irq_sts, void *usr_pdt);
	void  *isr_usr_pdt;

	u32 dbg_fe_count;
};
#endif
