/*
 * drivers/video/tegra/dc/dc.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Author: Erik Gilling <konkers@android.com>
 *
 * Copyright (C) 2010-2011 NVIDIA Corporation
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/workqueue.h>
#include <linux/ktime.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/backlight.h>
#include <linux/firmware.h>
#include <linux/vmalloc.h>
#include <drm/drm_fixed.h>
#ifdef CONFIG_SWITCH
#include <linux/switch.h>
#endif


#include <mach/clk.h>
#include <mach/dc.h>
#include <mach/fb.h>
#include <mach/mc.h>
#include <linux/nvhost.h>
#include <linux/clk.h>
#include <mach/latency_allowance.h>

#include "dc_reg.h"
#include "dc_priv.h"
#include "overlay.h"
#include "nvsd.h"
#include "edid.h"

#define TEGRA_CRC_LATCHED_DELAY		34

#ifdef CONFIG_TEGRA_SILICON_PLATFORM
#define ALL_UF_INT (WIN_A_UF_INT | WIN_B_UF_INT | WIN_C_UF_INT)
#else
/* ignore underflows when on simulation and fpga platform */
#define ALL_UF_INT (0)
#endif

/* defined as extern in dc_priv.h, so it cannot be static */
int no_vsync;

module_param_named(no_vsync, no_vsync, int, S_IRUGO | S_IWUSR);

static int __initdata nomodeset = 0;
static int use_dynamic_emc = 1;

module_param_named(use_dynamic_emc, use_dynamic_emc, int, S_IRUGO | S_IWUSR);

struct tegra_dc *tegra_dcs[TEGRA_MAX_DC];

DEFINE_SPINLOCK(tegra_dc_lock);
DEFINE_MUTEX(shared_lock);

static const struct {
	bool h;
	bool v;
} can_filter[] = {
	/* Window A has no filtering */
	{ false, false },
	/* Window B has both H and V filtering */
	{ true,  true  },
	/* Window C has only H filtering */
	{ false, true  },
};
static inline bool win_use_v_filter(const struct tegra_dc_win *win)
{
	return can_filter[win->idx].v &&
		win->h.full != dfixed_const(win->out_h);
}
static inline bool win_use_h_filter(const struct tegra_dc_win *win)
{
	return can_filter[win->idx].h &&
		win->w.full != dfixed_const(win->out_w);
}

static inline int tegra_dc_fmt_bpp(int fmt)
{
	switch (fmt) {
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
		return 16;

	case TEGRA_WIN_FMT_B8G8R8A8:
	case TEGRA_WIN_FMT_R8G8B8A8:
	case TEGRA_WIN_FMT_B6x2G6x2R6x2A8:
	case TEGRA_WIN_FMT_R6x2G6x2B6x2A8:
		return 32;

	/* for planar formats, size of the Y plane, 8bit */
	case TEGRA_WIN_FMT_YCbCr420P:
	case TEGRA_WIN_FMT_YUV420P:
	case TEGRA_WIN_FMT_YCbCr422P:
	case TEGRA_WIN_FMT_YUV422P:
		return 8;

	case TEGRA_WIN_FMT_YCbCr422:
	case TEGRA_WIN_FMT_YUV422:
	case TEGRA_WIN_FMT_YCbCr422R:
	case TEGRA_WIN_FMT_YUV422R:
	case TEGRA_WIN_FMT_YCbCr422RA:
	case TEGRA_WIN_FMT_YUV422RA:
		/* FIXME: need to know the bpp of these formats */
		return 0;
	}
	return 0;
}

static inline bool tegra_dc_is_yuv_planar(int fmt)
{
	switch (fmt) {
	case TEGRA_WIN_FMT_YUV420P:
	case TEGRA_WIN_FMT_YCbCr420P:
	case TEGRA_WIN_FMT_YCbCr422P:
	case TEGRA_WIN_FMT_YUV422P:
		return true;
	}
	return false;
}

#define DUMP_REG(a) do {			\
	snprintf(buff, sizeof(buff), "%-32s\t%03x\t%08lx\n", \
		 #a, a, tegra_dc_readl(dc, a));		      \
	print(data, buff);				      \
	} while (0)

static void _dump_regs(struct tegra_dc *dc, void *data,
		       void (* print)(void *data, const char *str))
{
	int i;
	char buff[256];

	tegra_dc_io_start(dc);
	clk_enable(dc->clk);

	DUMP_REG(DC_CMD_DISPLAY_COMMAND_OPTION0);
	DUMP_REG(DC_CMD_DISPLAY_COMMAND);
	DUMP_REG(DC_CMD_SIGNAL_RAISE);
	DUMP_REG(DC_CMD_INT_STATUS);
	DUMP_REG(DC_CMD_INT_MASK);
	DUMP_REG(DC_CMD_INT_ENABLE);
	DUMP_REG(DC_CMD_INT_TYPE);
	DUMP_REG(DC_CMD_INT_POLARITY);
	DUMP_REG(DC_CMD_SIGNAL_RAISE1);
	DUMP_REG(DC_CMD_SIGNAL_RAISE2);
	DUMP_REG(DC_CMD_SIGNAL_RAISE3);
	DUMP_REG(DC_CMD_STATE_ACCESS);
	DUMP_REG(DC_CMD_STATE_CONTROL);
	DUMP_REG(DC_CMD_DISPLAY_WINDOW_HEADER);
	DUMP_REG(DC_CMD_REG_ACT_CONTROL);

	DUMP_REG(DC_DISP_DISP_SIGNAL_OPTIONS0);
	DUMP_REG(DC_DISP_DISP_SIGNAL_OPTIONS1);
	DUMP_REG(DC_DISP_DISP_WIN_OPTIONS);
	DUMP_REG(DC_DISP_MEM_HIGH_PRIORITY);
	DUMP_REG(DC_DISP_MEM_HIGH_PRIORITY_TIMER);
	DUMP_REG(DC_DISP_DISP_TIMING_OPTIONS);
	DUMP_REG(DC_DISP_REF_TO_SYNC);
	DUMP_REG(DC_DISP_SYNC_WIDTH);
	DUMP_REG(DC_DISP_BACK_PORCH);
	DUMP_REG(DC_DISP_DISP_ACTIVE);
	DUMP_REG(DC_DISP_FRONT_PORCH);
	DUMP_REG(DC_DISP_H_PULSE0_CONTROL);
	DUMP_REG(DC_DISP_H_PULSE0_POSITION_A);
	DUMP_REG(DC_DISP_H_PULSE0_POSITION_B);
	DUMP_REG(DC_DISP_H_PULSE0_POSITION_C);
	DUMP_REG(DC_DISP_H_PULSE0_POSITION_D);
	DUMP_REG(DC_DISP_H_PULSE1_CONTROL);
	DUMP_REG(DC_DISP_H_PULSE1_POSITION_A);
	DUMP_REG(DC_DISP_H_PULSE1_POSITION_B);
	DUMP_REG(DC_DISP_H_PULSE1_POSITION_C);
	DUMP_REG(DC_DISP_H_PULSE1_POSITION_D);
	DUMP_REG(DC_DISP_H_PULSE2_CONTROL);
	DUMP_REG(DC_DISP_H_PULSE2_POSITION_A);
	DUMP_REG(DC_DISP_H_PULSE2_POSITION_B);
	DUMP_REG(DC_DISP_H_PULSE2_POSITION_C);
	DUMP_REG(DC_DISP_H_PULSE2_POSITION_D);
	DUMP_REG(DC_DISP_V_PULSE0_CONTROL);
	DUMP_REG(DC_DISP_V_PULSE0_POSITION_A);
	DUMP_REG(DC_DISP_V_PULSE0_POSITION_B);
	DUMP_REG(DC_DISP_V_PULSE0_POSITION_C);
	DUMP_REG(DC_DISP_V_PULSE1_CONTROL);
	DUMP_REG(DC_DISP_V_PULSE1_POSITION_A);
	DUMP_REG(DC_DISP_V_PULSE1_POSITION_B);
	DUMP_REG(DC_DISP_V_PULSE1_POSITION_C);
	DUMP_REG(DC_DISP_V_PULSE2_CONTROL);
	DUMP_REG(DC_DISP_V_PULSE2_POSITION_A);
	DUMP_REG(DC_DISP_V_PULSE3_CONTROL);
	DUMP_REG(DC_DISP_V_PULSE3_POSITION_A);
	DUMP_REG(DC_DISP_M0_CONTROL);
	DUMP_REG(DC_DISP_M1_CONTROL);
	DUMP_REG(DC_DISP_DI_CONTROL);
	DUMP_REG(DC_DISP_PP_CONTROL);
	DUMP_REG(DC_DISP_PP_SELECT_A);
	DUMP_REG(DC_DISP_PP_SELECT_B);
	DUMP_REG(DC_DISP_PP_SELECT_C);
	DUMP_REG(DC_DISP_PP_SELECT_D);
	DUMP_REG(DC_DISP_DISP_CLOCK_CONTROL);
	DUMP_REG(DC_DISP_DISP_INTERFACE_CONTROL);
	DUMP_REG(DC_DISP_DISP_COLOR_CONTROL);
	DUMP_REG(DC_DISP_SHIFT_CLOCK_OPTIONS);
	DUMP_REG(DC_DISP_DATA_ENABLE_OPTIONS);
	DUMP_REG(DC_DISP_SERIAL_INTERFACE_OPTIONS);
	DUMP_REG(DC_DISP_LCD_SPI_OPTIONS);
	DUMP_REG(DC_DISP_BORDER_COLOR);
	DUMP_REG(DC_DISP_COLOR_KEY0_LOWER);
	DUMP_REG(DC_DISP_COLOR_KEY0_UPPER);
	DUMP_REG(DC_DISP_COLOR_KEY1_LOWER);
	DUMP_REG(DC_DISP_COLOR_KEY1_UPPER);
	DUMP_REG(DC_DISP_CURSOR_FOREGROUND);
	DUMP_REG(DC_DISP_CURSOR_BACKGROUND);
	DUMP_REG(DC_DISP_CURSOR_START_ADDR);
	DUMP_REG(DC_DISP_CURSOR_START_ADDR_NS);
	DUMP_REG(DC_DISP_CURSOR_POSITION);
	DUMP_REG(DC_DISP_CURSOR_POSITION_NS);
	DUMP_REG(DC_DISP_INIT_SEQ_CONTROL);
	DUMP_REG(DC_DISP_SPI_INIT_SEQ_DATA_A);
	DUMP_REG(DC_DISP_SPI_INIT_SEQ_DATA_B);
	DUMP_REG(DC_DISP_SPI_INIT_SEQ_DATA_C);
	DUMP_REG(DC_DISP_SPI_INIT_SEQ_DATA_D);
	DUMP_REG(DC_DISP_DC_MCCIF_FIFOCTRL);
	DUMP_REG(DC_DISP_MCCIF_DISPLAY0A_HYST);
	DUMP_REG(DC_DISP_MCCIF_DISPLAY0B_HYST);
	DUMP_REG(DC_DISP_MCCIF_DISPLAY0C_HYST);
	DUMP_REG(DC_DISP_MCCIF_DISPLAY1B_HYST);
	DUMP_REG(DC_DISP_DAC_CRT_CTRL);
	DUMP_REG(DC_DISP_DISP_MISC_CONTROL);


	for (i = 0; i < 3; i++) {
		print(data, "\n");
		snprintf(buff, sizeof(buff), "WINDOW %c:\n", 'A' + i);
		print(data, buff);

		tegra_dc_writel(dc, WINDOW_A_SELECT << i,
				DC_CMD_DISPLAY_WINDOW_HEADER);
		DUMP_REG(DC_CMD_DISPLAY_WINDOW_HEADER);
		DUMP_REG(DC_WIN_WIN_OPTIONS);
		DUMP_REG(DC_WIN_BYTE_SWAP);
		DUMP_REG(DC_WIN_BUFFER_CONTROL);
		DUMP_REG(DC_WIN_COLOR_DEPTH);
		DUMP_REG(DC_WIN_POSITION);
		DUMP_REG(DC_WIN_SIZE);
		DUMP_REG(DC_WIN_PRESCALED_SIZE);
		DUMP_REG(DC_WIN_H_INITIAL_DDA);
		DUMP_REG(DC_WIN_V_INITIAL_DDA);
		DUMP_REG(DC_WIN_DDA_INCREMENT);
		DUMP_REG(DC_WIN_LINE_STRIDE);
		DUMP_REG(DC_WIN_BUF_STRIDE);
		DUMP_REG(DC_WIN_UV_BUF_STRIDE);
		DUMP_REG(DC_WIN_BLEND_NOKEY);
		DUMP_REG(DC_WIN_BLEND_1WIN);
		DUMP_REG(DC_WIN_BLEND_2WIN_X);
		DUMP_REG(DC_WIN_BLEND_2WIN_Y);
		DUMP_REG(DC_WIN_BLEND_3WIN_XY);
		DUMP_REG(DC_WINBUF_START_ADDR);
		DUMP_REG(DC_WINBUF_START_ADDR_U);
		DUMP_REG(DC_WINBUF_START_ADDR_V);
		DUMP_REG(DC_WINBUF_ADDR_H_OFFSET);
		DUMP_REG(DC_WINBUF_ADDR_V_OFFSET);
		DUMP_REG(DC_WINBUF_UFLOW_STATUS);
		DUMP_REG(DC_WIN_CSC_YOF);
		DUMP_REG(DC_WIN_CSC_KYRGB);
		DUMP_REG(DC_WIN_CSC_KUR);
		DUMP_REG(DC_WIN_CSC_KVR);
		DUMP_REG(DC_WIN_CSC_KUG);
		DUMP_REG(DC_WIN_CSC_KVG);
		DUMP_REG(DC_WIN_CSC_KUB);
		DUMP_REG(DC_WIN_CSC_KVB);
	}

	DUMP_REG(DC_CMD_DISPLAY_POWER_CONTROL);
	DUMP_REG(DC_COM_PIN_OUTPUT_ENABLE2);
	DUMP_REG(DC_COM_PIN_OUTPUT_POLARITY2);
	DUMP_REG(DC_COM_PIN_OUTPUT_DATA2);
	DUMP_REG(DC_COM_PIN_INPUT_ENABLE2);
	DUMP_REG(DC_COM_PIN_OUTPUT_SELECT5);
	DUMP_REG(DC_DISP_DISP_SIGNAL_OPTIONS0);
	DUMP_REG(DC_DISP_M1_CONTROL);
	DUMP_REG(DC_COM_PM1_CONTROL);
	DUMP_REG(DC_COM_PM1_DUTY_CYCLE);
	DUMP_REG(DC_DISP_SD_CONTROL);

	clk_disable(dc->clk);
	tegra_dc_io_end(dc);
}

#undef DUMP_REG

#ifdef DEBUG
static void dump_regs_print(void *data, const char *str)
{
	struct tegra_dc *dc = data;
	dev_dbg(&dc->ndev->dev, "%s", str);
}

static void dump_regs(struct tegra_dc *dc)
{
	_dump_regs(dc, dc, dump_regs_print);
}
#else /* !DEBUG */

static void dump_regs(struct tegra_dc *dc) {}

#endif /* DEBUG */

#ifdef CONFIG_DEBUG_FS

static void dbg_regs_print(void *data, const char *str)
{
	struct seq_file *s = data;

	seq_printf(s, "%s", str);
}

#undef DUMP_REG

static int dbg_dc_show(struct seq_file *s, void *unused)
{
	struct tegra_dc *dc = s->private;

	_dump_regs(dc, s, dbg_regs_print);

	return 0;
}


static int dbg_dc_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_dc_show, inode->i_private);
}

static const struct file_operations regs_fops = {
	.open		= dbg_dc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int dbg_dc_mode_show(struct seq_file *s, void *unused)
{
	struct tegra_dc *dc = s->private;
	struct tegra_dc_mode *m;

	mutex_lock(&dc->lock);
	m = &dc->mode;
	seq_printf(s,
		"pclk: %d\n"
		"h_ref_to_sync: %d\n"
		"v_ref_to_sync: %d\n"
		"h_sync_width: %d\n"
		"v_sync_width: %d\n"
		"h_back_porch: %d\n"
		"v_back_porch: %d\n"
		"h_active: %d\n"
		"v_active: %d\n"
		"h_front_porch: %d\n"
		"v_front_porch: %d\n"
		"stereo_mode: %d\n",
		m->pclk, m->h_ref_to_sync, m->v_ref_to_sync,
		m->h_sync_width, m->v_sync_width,
		m->h_back_porch, m->v_back_porch,
		m->h_active, m->v_active,
		m->h_front_porch, m->v_front_porch,
		m->stereo_mode);
	mutex_unlock(&dc->lock);
	return 0;
}

static int dbg_dc_mode_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_dc_mode_show, inode->i_private);
}

static const struct file_operations mode_fops = {
	.open		= dbg_dc_mode_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int dbg_dc_stats_show(struct seq_file *s, void *unused)
{
	struct tegra_dc *dc = s->private;

	mutex_lock(&dc->lock);
	seq_printf(s,
		"underflows: %llu\n"
		"underflows_a: %llu\n"
		"underflows_b: %llu\n"
		"underflows_c: %llu\n",
		dc->stats.underflows,
		dc->stats.underflows_a,
		dc->stats.underflows_b,
		dc->stats.underflows_c);
	mutex_unlock(&dc->lock);

	return 0;
}

static int dbg_dc_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_dc_stats_show, inode->i_private);
}

static const struct file_operations stats_fops = {
	.open		= dbg_dc_stats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void __devexit tegra_dc_remove_debugfs(struct tegra_dc *dc)
{
	if (dc->debugdir)
		debugfs_remove_recursive(dc->debugdir);
	dc->debugdir = NULL;
}

static void tegra_dc_create_debugfs(struct tegra_dc *dc)
{
	struct dentry *retval;

	dc->debugdir = debugfs_create_dir(dev_name(&dc->ndev->dev), NULL);
	if (!dc->debugdir)
		goto remove_out;

	retval = debugfs_create_file("regs", S_IRUGO, dc->debugdir, dc,
		&regs_fops);
	if (!retval)
		goto remove_out;

	retval = debugfs_create_file("mode", S_IRUGO, dc->debugdir, dc,
		&mode_fops);
	if (!retval)
		goto remove_out;

	retval = debugfs_create_file("stats", S_IRUGO, dc->debugdir, dc,
		&stats_fops);
	if (!retval)
		goto remove_out;

	return;
remove_out:
	dev_err(&dc->ndev->dev, "could not create debugfs\n");
	tegra_dc_remove_debugfs(dc);
}

#else /* !CONFIG_DEBUGFS */
static inline void tegra_dc_create_debugfs(struct tegra_dc *dc) { };
static inline void __devexit tegra_dc_remove_debugfs(struct tegra_dc *dc) { };
#endif /* CONFIG_DEBUGFS */

static int tegra_dc_set(struct tegra_dc *dc, int index)
{
	int ret = 0;

	spin_lock(&tegra_dc_lock);
	if (index >= TEGRA_MAX_DC) {
		ret = -EINVAL;
		goto out;
	}

	if (dc != NULL && tegra_dcs[index] != NULL) {
		ret = -EBUSY;
		goto out;
	}

	tegra_dcs[index] = dc;

out:
	spin_unlock(&tegra_dc_lock);

	return ret;
}

unsigned int tegra_dc_has_multiple_dc(void)
{
	unsigned int idx;
	unsigned int cnt = 0;
	struct tegra_dc *dc;

	spin_lock(&tegra_dc_lock);
	for (idx = 0; idx < TEGRA_MAX_DC; idx++)
		cnt += ((dc = tegra_dcs[idx]) != NULL && dc->enabled) ? 1 : 0;
	spin_unlock(&tegra_dc_lock);

	return (cnt > 1);
}

struct tegra_dc *tegra_dc_get_dc(unsigned idx)
{
	if (idx < TEGRA_MAX_DC)
		return tegra_dcs[idx];
	else
		return NULL;
}
EXPORT_SYMBOL(tegra_dc_get_dc);

struct tegra_dc_win *tegra_dc_get_window(struct tegra_dc *dc, unsigned win)
{
	if (win >= dc->n_windows)
		return NULL;

	return &dc->windows[win];
}
EXPORT_SYMBOL(tegra_dc_get_window);

static int get_topmost_window(u32 *depths, unsigned long *wins)
{
	int idx, best = -1;

	for_each_set_bit(idx, wins, DC_N_WINDOWS) {
		if (best == -1 || depths[idx] < depths[best])
			best = idx;
	}
	clear_bit(best, wins);
	return best;
}

bool tegra_dc_get_connected(struct tegra_dc *dc)
{
	return dc->connected;
}
EXPORT_SYMBOL(tegra_dc_get_connected);

static u32 blend_topwin(u32 flags)
{
	if (flags & TEGRA_WIN_FLAG_BLEND_COVERAGE)
		return BLEND(NOKEY, ALPHA, 0xff, 0xff);
	else if (flags & TEGRA_WIN_FLAG_BLEND_PREMULT)
		return BLEND(NOKEY, PREMULT, 0xff, 0xff);
	else
		return BLEND(NOKEY, FIX, 0xff, 0xff);
}

static u32 blend_2win(int idx, unsigned long behind_mask, u32* flags, int xy)
{
	int other;

	for (other = 0; other < DC_N_WINDOWS; other++) {
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

static u32 blend_3win(int idx, unsigned long behind_mask, u32* flags)
{
	unsigned long infront_mask;
	int first;

	infront_mask = ~(behind_mask | BIT(idx));
	infront_mask &= (BIT(DC_N_WINDOWS) - 1);
	first = ffs(infront_mask) - 1;

	if (!infront_mask)
		return blend_topwin(flags[idx]);
	else if (behind_mask && first != -1 && flags[first])
		return BLEND(NOKEY, DEPENDANT, 0x00, 0x00);
	else
		return BLEND(NOKEY, FIX, 0x0, 0x0);
}

static void tegra_dc_set_blending(struct tegra_dc *dc, struct tegra_dc_blend *blend)
{
	unsigned long mask = BIT(DC_N_WINDOWS) - 1;

	while (mask) {
		int idx = get_topmost_window(blend->z, &mask);

		tegra_dc_writel(dc, WINDOW_A_SELECT << idx,
				DC_CMD_DISPLAY_WINDOW_HEADER);
		tegra_dc_writel(dc, BLEND(NOKEY, FIX, 0xff, 0xff),
				DC_WIN_BLEND_NOKEY);
		tegra_dc_writel(dc, BLEND(NOKEY, FIX, 0xff, 0xff),
				DC_WIN_BLEND_1WIN);
		tegra_dc_writel(dc, blend_2win(idx, mask, blend->flags, 0),
				DC_WIN_BLEND_2WIN_X);
		tegra_dc_writel(dc, blend_2win(idx, mask, blend->flags, 1),
				DC_WIN_BLEND_2WIN_Y);
		tegra_dc_writel(dc, blend_3win(idx, mask, blend->flags),
				DC_WIN_BLEND_3WIN_XY);
	}
}

static void tegra_dc_init_csc_defaults(struct tegra_dc_csc *csc)
{
	csc->yof   = 0x00f0;
	csc->kyrgb = 0x012a;
	csc->kur   = 0x0000;
	csc->kvr   = 0x0198;
	csc->kug   = 0x039b;
	csc->kvg   = 0x032f;
	csc->kub   = 0x0204;
	csc->kvb   = 0x0000;
}

static void tegra_dc_set_csc(struct tegra_dc *dc, struct tegra_dc_csc *csc)
{
	tegra_dc_writel(dc, csc->yof,	DC_WIN_CSC_YOF);
	tegra_dc_writel(dc, csc->kyrgb,	DC_WIN_CSC_KYRGB);
	tegra_dc_writel(dc, csc->kur,	DC_WIN_CSC_KUR);
	tegra_dc_writel(dc, csc->kvr,	DC_WIN_CSC_KVR);
	tegra_dc_writel(dc, csc->kug,	DC_WIN_CSC_KUG);
	tegra_dc_writel(dc, csc->kvg,	DC_WIN_CSC_KVG);
	tegra_dc_writel(dc, csc->kub,	DC_WIN_CSC_KUB);
	tegra_dc_writel(dc, csc->kvb,	DC_WIN_CSC_KVB);
}

int tegra_dc_update_csc(struct tegra_dc *dc, int win_idx)
{
	mutex_lock(&dc->lock);

	if (!dc->enabled) {
		mutex_unlock(&dc->lock);
		return -EFAULT;
	}

	tegra_dc_writel(dc, WINDOW_A_SELECT << win_idx,
			DC_CMD_DISPLAY_WINDOW_HEADER);

	tegra_dc_set_csc(dc, &dc->windows[win_idx].csc);

	mutex_unlock(&dc->lock);

	return 0;
}
EXPORT_SYMBOL(tegra_dc_update_csc);

static void tegra_dc_init_lut_defaults(struct tegra_dc_lut *lut)
{
	int i;
	for (i = 0; i < 256; i++)
		lut->r[i] = lut->g[i] = lut->b[i] = (u8)i;
}

static void tegra_dc_set_lut(struct tegra_dc *dc,
			     struct tegra_dc_lut *lut,
			     int start,
			     int len)
{
	int i;
	for (i = start, len += start; i < len; i++) {
		u32 rgb = ((u32)lut->r[i]) |
			  ((u32)lut->g[i]<<8) |
			  ((u32)lut->b[i]<<16);
		tegra_dc_writel(dc, rgb, DC_WIN_COLOR_PALETTE(i));
	}
}

int tegra_dc_update_lut(struct tegra_dc *dc, int win_idx, int start, int len)
{
	mutex_lock(&dc->lock);

	if (!dc->enabled) {
		mutex_unlock(&dc->lock);
		return -EFAULT;
	}

	tegra_dc_writel(dc, WINDOW_A_SELECT << win_idx,
			DC_CMD_DISPLAY_WINDOW_HEADER);

	tegra_dc_set_lut(dc, &dc->windows[win_idx].lut, start, len);

	mutex_unlock(&dc->lock);

	return 0;
}
EXPORT_SYMBOL(tegra_dc_update_lut);

static void tegra_dc_set_scaling_filter(struct tegra_dc *dc)
{
	unsigned i;
	unsigned v0 = 128;
	unsigned v1 = 0;
	/* linear horizontal and vertical filters */
	for (i = 0; i < 16; i++) {
		tegra_dc_writel(dc, (v1 << 16) | (v0 << 8),
				DC_WIN_H_FILTER_P(i));

		tegra_dc_writel(dc, v0,
				DC_WIN_V_FILTER_P(i));
		v0 -= 8;
		v1 += 8;
	}
}

static void tegra_dc_set_latency_allowance(struct tegra_dc *dc,
	struct tegra_dc_win *w)
{
	/* windows A, B, C for first and second display */
	static const enum tegra_la_id la_id_tab[2][3] = {
		/* first display */
		{ TEGRA_LA_DISPLAY_0A, TEGRA_LA_DISPLAY_0B,
			TEGRA_LA_DISPLAY_0C },
		/* second display */
		{ TEGRA_LA_DISPLAY_0AB, TEGRA_LA_DISPLAY_0BB,
			TEGRA_LA_DISPLAY_0CB },
	};
	/* window B V-filter tap for first and second display. */
	static const enum tegra_la_id vfilter_tab[2] = {
		TEGRA_LA_DISPLAY_1B, TEGRA_LA_DISPLAY_1BB,
	};
	unsigned long bw;

	BUG_ON(dc->ndev->id >= ARRAY_SIZE(la_id_tab));
	BUG_ON(dc->ndev->id >= ARRAY_SIZE(vfilter_tab));
	BUG_ON(w->idx >= ARRAY_SIZE(*la_id_tab));

	bw = w->new_bandwidth;

	/* tegra_dc_get_bandwidth() treats V filter windows as double
	 * bandwidth, but LA has a seperate client for V filter */
	if (w->idx == 1 && win_use_v_filter(w))
		bw /= 2;

	/* our bandwidth is in bytes/sec, but LA takes MBps.
	 * round up bandwidth to 1MBps */
	bw = bw / 1000000 + 1;

	tegra_set_latency_allowance(la_id_tab[dc->ndev->id][w->idx], bw);

	/* if window B, also set the 1B client for the 2-tap V filter. */
	if (w->idx == 1)
		tegra_set_latency_allowance(vfilter_tab[dc->ndev->id], bw);

	w->bandwidth = w->new_bandwidth;
}

static unsigned int tegra_dc_windows_is_overlapped(struct tegra_dc_win *a,
						   struct tegra_dc_win *b)
{
	if (!WIN_IS_ENABLED(a) || !WIN_IS_ENABLED(b))
		return 0;

	/* because memory access to load the fifo can overlap, only care
	 * if windows overlap vertically */
	return ((a->out_y + a->out_h > b->out_y) && (a->out_y <= b->out_y)) ||
		((b->out_y + b->out_h > a->out_y) && (b->out_y <= a->out_y));
}

static unsigned long tegra_dc_find_max_bandwidth(struct tegra_dc_win *wins[],
						 int n)
{
	unsigned i;
	unsigned j;
	unsigned overlap_count;
	unsigned max_bw = 0;

	WARN_ONCE(n > 3, "Code assumes at most 3 windows, bandwidth is likely"
			 "inaccurate.\n");

	/* If we had a large number of windows, we would compute adjacency
	 * graph representing 2 window overlaps, find all cliques in the graph,
	 * assign bandwidth to each clique, and then select the clique with
	 * maximum bandwidth. But because we have at most 3 windows,
	 * implementing proper Bron-Kerbosh algorithm would be an overkill,
	 * brute force will suffice.
	 *
	 * Thus: find maximum bandwidth for either single or a pair of windows
	 * and count number of window pair overlaps. If there are three
	 * pairs, all 3 window overlap.
	 */

	overlap_count = 0;
	for (i = 0; i < n; i++) {
		unsigned int bw1;

		if (wins[i] == NULL)
			continue;
		bw1 = wins[i]->new_bandwidth;
		if (bw1 > max_bw)
			/* Single window */
			max_bw = bw1;

		for (j = i + 1; j < n; j++) {
			if (wins[j] == NULL)
				continue;
			if (tegra_dc_windows_is_overlapped(wins[i], wins[j])) {
				unsigned int bw2 = wins[j]->new_bandwidth;
				if (bw1 + bw2 > max_bw)
					/* Window pair overlaps */
					max_bw = bw1 + bw2;
				overlap_count++;
			}
		}
	}

	if (overlap_count == 3)
		/* All three windows overlap */
		max_bw = wins[0]->new_bandwidth + wins[1]->new_bandwidth +
			 wins[2]->new_bandwidth;

	return max_bw;
}

/*
 * Calculate peak EMC bandwidth for each enabled window =
 * pixel_clock * win_bpp * (use_v_filter ? 2 : 1)) * H_scale_factor *
 * (windows_tiling ? 2 : 1)
 *
 *
 * note:
 * (*) We use 2 tap V filter, so need double BW if use V filter
 * (*) Tiling mode on T30 and DDR3 requires double BW
 */
static unsigned long tegra_dc_calc_win_bandwidth(struct tegra_dc *dc,
	struct tegra_dc_win *w)
{
	unsigned long ret;
	int tiled_windows_bw_multiplier;
	unsigned long bpp;

	if (!WIN_IS_ENABLED(w))
		return 0;

	if (dfixed_trunc(w->w) == 0 || dfixed_trunc(w->h) == 0 ||
	    w->out_w == 0 || w->out_h == 0)
		return 0;

	tiled_windows_bw_multiplier =
		tegra_mc_get_tiled_memory_bandwidth_multiplier();

	/* all of tegra's YUV formats(420 and 422) fetch 2 bytes per pixel,
	 * but the size reported by tegra_dc_fmt_bpp for the planar version
	 * is of the luma plane's size only. */
	bpp = tegra_dc_is_yuv_planar(w->fmt) ?
		2 * tegra_dc_fmt_bpp(w->fmt) : tegra_dc_fmt_bpp(w->fmt);
	/* perform calculations with most significant bits of pixel clock
	 * to prevent overflow of long. */
	ret = (unsigned long)(dc->pixel_clk >> 16) *
		bpp / 8 *
		(win_use_v_filter(w) ? 2 : 1) * dfixed_trunc(w->w) / w->out_w *
		(WIN_IS_TILED(w) ? tiled_windows_bw_multiplier : 1);

/*
 * Assuming 48% efficiency: i.e. if we calculate we need 70MBps, we
 * will request 147MBps from EMC.
 */
	ret = ret * 2 + ret / 10;

	/* if overflowed */
	if (ret > (1UL << 31))
		return ULONG_MAX;

	return ret << 16; /* restore the scaling we did above */
}

unsigned long tegra_dc_get_bandwidth(struct tegra_dc_win *windows[], int n)
{
	int i;

	BUG_ON(n > DC_N_WINDOWS);

	/* emc rate and latency allowance both need to know per window
	 * bandwidths */
	for (i = 0; i < n; i++) {
		struct tegra_dc_win *w = windows[i];
		if (w)
			w->new_bandwidth = tegra_dc_calc_win_bandwidth(w->dc, w);
	}

	return tegra_dc_find_max_bandwidth(windows, n);
}

static void tegra_dc_program_bandwidth(struct tegra_dc *dc)
{
	unsigned i;

	if (dc->emc_clk_rate != dc->new_emc_clk_rate) {
		dc->emc_clk_rate = dc->new_emc_clk_rate;
		clk_set_rate(dc->emc_clk, dc->emc_clk_rate);
	}

	for (i = 0; i < DC_N_WINDOWS; i++) {
		struct tegra_dc_win *w = &dc->windows[i];
		if (w->bandwidth != w->new_bandwidth && w->new_bandwidth != 0)
			tegra_dc_set_latency_allowance(dc, w);
	}
}

static int tegra_dc_set_dynamic_emc(struct tegra_dc_win *windows[], int n)
{
	unsigned long new_rate;
	struct tegra_dc *dc;

	if (!use_dynamic_emc)
		return 0;

	dc = windows[0]->dc;

	/* calculate the new rate based on this POST */
	new_rate = tegra_dc_get_bandwidth(windows, n);
	new_rate = EMC_BW_TO_FREQ(new_rate);

	if (tegra_dc_has_multiple_dc())
		new_rate = ULONG_MAX;

	dc->new_emc_clk_rate = new_rate;

	return 0;
}

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

/* does not support updating windows on multiple dcs in one call */
int tegra_dc_update_windows(struct tegra_dc_win *windows[], int n,
			    bool wait_for_vblank)
{
	struct tegra_dc *dc;
	unsigned long update_mask = GENERAL_ACT_REQ;
	unsigned long act_control = 0;
	unsigned long val;
	unsigned long flags;
	bool update_blend = false;
	int i;

	if (!n)
		return 0;

	dc = windows[0]->dc;

	if (!dc->enabled) {
		return -EFAULT;
	}

	mutex_lock(&dc->lock);
	spin_lock_irqsave(&dc->update_spinlock, flags);

	if (no_vsync)
		wait_for_vblank = 0;

	for (i = 0; i < n; i++) {
		struct tegra_dc_win *win = windows[i];
		unsigned h_dda;
		unsigned v_dda;
		fixed20_12 h_offset, v_offset;
		bool invert_h = (win->flags & TEGRA_WIN_FLAG_INVERT_H) != 0;
		bool invert_v = (win->flags & TEGRA_WIN_FLAG_INVERT_V) != 0;
		bool yuvp = tegra_dc_is_yuv_planar(win->fmt);
		unsigned Bpp = tegra_dc_fmt_bpp(win->fmt) / 8;
		/* Bytes per pixel of bandwidth, used for dda_inc calculation */
		unsigned Bpp_bw = Bpp * (yuvp ? 2 : 1);
		const bool filter_h = win_use_h_filter(win);
		const bool filter_v = win_use_v_filter(win);

		if (win->z != dc->blend.z[win->idx]) {
			dc->blend.z[win->idx] = win->z;
			update_blend = true;
		}
		if ((win->flags & TEGRA_WIN_BLEND_FLAGS_MASK) !=
			dc->blend.flags[win->idx]) {
			dc->blend.flags[win->idx] =
				win->flags & TEGRA_WIN_BLEND_FLAGS_MASK;
			update_blend = true;
		}

		tegra_dc_writel(dc, WINDOW_A_SELECT << win->idx,
				DC_CMD_DISPLAY_WINDOW_HEADER);

		update_mask |= WIN_A_ACT_REQ << win->idx;

		if (wait_for_vblank)
			act_control &= ~WIN_ACT_CNTR_SEL_HCOUNTER(win->idx);
		else
			act_control |= WIN_ACT_CNTR_SEL_HCOUNTER(win->idx);

		if (!WIN_IS_ENABLED(win)) {
			dc->windows[i].dirty = 1;
			tegra_dc_writel(dc, 0, DC_WIN_WIN_OPTIONS);
			continue;
		}

		tegra_dc_writel(dc, win->fmt, DC_WIN_COLOR_DEPTH);
		tegra_dc_writel(dc, 0, DC_WIN_BYTE_SWAP);

		tegra_dc_writel(dc,
				V_POSITION(win->out_y) | H_POSITION(win->out_x),
				DC_WIN_POSITION);
		tegra_dc_writel(dc,
				V_SIZE(win->out_h) | H_SIZE(win->out_w),
				DC_WIN_SIZE);
		tegra_dc_writel(dc,
				V_PRESCALED_SIZE(dfixed_trunc(win->h)) |
				H_PRESCALED_SIZE(dfixed_trunc(win->w) * Bpp),
				DC_WIN_PRESCALED_SIZE);

		h_dda = compute_dda_inc(win->w, win->out_w, false, Bpp_bw);
		v_dda = compute_dda_inc(win->h, win->out_h, true, Bpp_bw);
		tegra_dc_writel(dc, V_DDA_INC(v_dda) | H_DDA_INC(h_dda),
				DC_WIN_DDA_INCREMENT);
		h_dda = compute_initial_dda(win->x);
		v_dda = compute_initial_dda(win->y);
		tegra_dc_writel(dc, h_dda, DC_WIN_H_INITIAL_DDA);
		tegra_dc_writel(dc, v_dda, DC_WIN_V_INITIAL_DDA);

		tegra_dc_writel(dc, 0, DC_WIN_BUF_STRIDE);
		tegra_dc_writel(dc, 0, DC_WIN_UV_BUF_STRIDE);
		tegra_dc_writel(dc,
				(unsigned long)win->phys_addr,
				DC_WINBUF_START_ADDR);

		if (!yuvp) {
			tegra_dc_writel(dc, win->stride, DC_WIN_LINE_STRIDE);
		} else {
			tegra_dc_writel(dc,
					(unsigned long)win->phys_addr_u,
					DC_WINBUF_START_ADDR_U);
			tegra_dc_writel(dc,
					(unsigned long)win->phys_addr_v,
					DC_WINBUF_START_ADDR_V);
			tegra_dc_writel(dc,
					LINE_STRIDE(win->stride) |
					UV_LINE_STRIDE(win->stride_uv),
					DC_WIN_LINE_STRIDE);
		}

		h_offset = win->x;
		if (invert_h) {
			h_offset.full += win->w.full - dfixed_const(1);
		}

		v_offset = win->y;
		if (invert_v) {
			v_offset.full += win->h.full - dfixed_const(1);
		}

		tegra_dc_writel(dc, dfixed_trunc(h_offset) * Bpp,
				DC_WINBUF_ADDR_H_OFFSET);
		tegra_dc_writel(dc, dfixed_trunc(v_offset),
				DC_WINBUF_ADDR_V_OFFSET);

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

		val = WIN_ENABLE | CP_ENABLE;
		if (yuvp)
			val |= CSC_ENABLE;
		else if (tegra_dc_fmt_bpp(win->fmt) < 24)
			val |= COLOR_EXPAND;

		if (filter_h)
			val |= H_FILTER_ENABLE;
		if (filter_v)
			val |= V_FILTER_ENABLE;

		if (invert_h)
			val |= H_DIRECTION_DECREMENT;
		if (invert_v)
			val |= V_DIRECTION_DECREMENT;

		tegra_dc_writel(dc, val, DC_WIN_WIN_OPTIONS);

		win->dirty = 1;
		win->flip_wait_for_vblank = wait_for_vblank;

		dev_dbg(&dc->ndev->dev, "%s():idx=%d z=%d x=%d y=%d w=%d h=%d "
			"out_x=%u out_y=%u out_w=%u out_h=%u "
			"fmt=%d yuvp=%d Bpp=%u filter_h=%d filter_v=%d",
			__func__, win->idx, win->z,
			dfixed_trunc(win->x), dfixed_trunc(win->y),
			dfixed_trunc(win->w), dfixed_trunc(win->h),
			win->out_x, win->out_y, win->out_w, win->out_h,
			win->fmt, yuvp, Bpp, filter_h, filter_v);
	}

	if (update_blend) {
		tegra_dc_set_blending(dc, &dc->blend);
		for (i = 0; i < DC_N_WINDOWS; i++) {
			dc->windows[i].dirty = 1;
			update_mask |= WIN_A_ACT_REQ << i;
		}
	}

	tegra_dc_set_dynamic_emc(windows, n);

	tegra_dc_writel(dc, act_control, DC_CMD_REG_ACT_CONTROL);

	tegra_dc_writel(dc, update_mask << 8, DC_CMD_STATE_CONTROL);

	tegra_dc_writel(dc, FRAME_END_INT | V_BLANK_INT | H_BLANK_INT,
			DC_CMD_INT_STATUS);
	val = tegra_dc_readl(dc, DC_CMD_INT_MASK);
	val |= (V_BLANK_INT | ALL_UF_INT);
	if (wait_for_vblank)
		val |= FRAME_END_INT;
	else
		val |= H_BLANK_INT;

	tegra_dc_writel(dc, val, DC_CMD_INT_MASK);

	tegra_dc_writel(dc, update_mask, DC_CMD_STATE_CONTROL);

	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE)
		tegra_dc_writel(dc, NC_HOST_TRIG, DC_CMD_STATE_CONTROL);

	mutex_unlock(&dc->lock);

	spin_unlock_irqrestore(&dc->update_spinlock, flags);

	return 0;
}
EXPORT_SYMBOL(tegra_dc_update_windows);

u32 tegra_dc_get_syncpt_id(const struct tegra_dc *dc, int i)
{
	return dc->syncpt[i].id;
}
EXPORT_SYMBOL(tegra_dc_get_syncpt_id);

u32 tegra_dc_incr_syncpt_max(struct tegra_dc *dc, int i)
{
	u32 max;

	mutex_lock(&dc->lock);
	max = nvhost_syncpt_incr_max(&dc->ndev->host->syncpt,
		dc->syncpt[i].id, ((dc->enabled) ? 1 : 0));
	dc->syncpt[i].max = max;
	mutex_unlock(&dc->lock);

	return max;
}

void tegra_dc_incr_syncpt_min(struct tegra_dc *dc, int i, u32 val)
{
	mutex_lock(&dc->lock);
	if ( dc->enabled )
		while (dc->syncpt[i].min < val) {
			dc->syncpt[i].min++;
			nvhost_syncpt_cpu_incr(&dc->ndev->host->syncpt,
					dc->syncpt[i].id);
		}
	mutex_unlock(&dc->lock);
}

static bool tegra_dc_windows_are_clean(struct tegra_dc_win *windows[],
					     int n)
{
	int i;

	for (i = 0; i < n; i++) {
		if (windows[i]->dirty)
			return false;
	}

	return true;
}

/* does not support syncing windows on multiple dcs in one call */
int tegra_dc_sync_windows(struct tegra_dc_win *windows[], int n)
{
	if (n < 1 || n > DC_N_WINDOWS)
		return -EINVAL;

	if (!windows[0]->dc->enabled)
		return -EFAULT;

	return wait_event_interruptible_timeout(windows[0]->dc->wq,
					 tegra_dc_windows_are_clean(windows, n),
					 HZ);
}
EXPORT_SYMBOL(tegra_dc_sync_windows);

static unsigned long tegra_dc_clk_get_rate(struct tegra_dc *dc)
{
#ifdef CONFIG_TEGRA_SILICON_PLATFORM
	return clk_get_rate(dc->clk);
#else
	return 27000000;
#endif
}

static unsigned long tegra_dc_pclk_round_rate(struct tegra_dc *dc, int pclk)
{
	unsigned long rate;
	unsigned long div;

	rate = tegra_dc_clk_get_rate(dc);

	div = DIV_ROUND_CLOSEST(rate * 2, pclk);

	if (div < 2)
		return 0;

	return rate * 2 / div;
}

void tegra_dc_setup_clk(struct tegra_dc *dc, struct clk *clk)
{
	int pclk;

	if (dc->out->type == TEGRA_DC_OUT_RGB) {
		unsigned long rate;
		struct clk *parent_clk =
			clk_get_sys(NULL, dc->out->parent_clk ? : "pll_p");

		if (clk_get_parent(clk) != parent_clk)
			clk_set_parent(clk, parent_clk);

		if (parent_clk != clk_get_sys(NULL, "pll_p")) {
			struct clk *base_clk = clk_get_parent(parent_clk);

			/* Assuming either pll_d or pll_d2 is used */
			rate = dc->mode.pclk * 2;

			if (rate != clk_get_rate(base_clk))
				clk_set_rate(base_clk, rate);
		}
	}

	if (dc->out->type == TEGRA_DC_OUT_HDMI) {
		unsigned long rate;
		struct clk *parent_clk =
			clk_get_sys(NULL, dc->out->parent_clk ? : "pll_d_out0");
		struct clk *base_clk = clk_get_parent(parent_clk);

		/* needs to match tegra_dc_hdmi_supported_modes[]
		and tegra_pll_d_freq_table[] */
		if (dc->mode.pclk > 70000000)
			rate = 594000000;
		else if (dc->mode.pclk > 25200000)
			rate = 216000000;
		else
			rate = 504000000;

		if (rate != clk_get_rate(base_clk))
			clk_set_rate(base_clk, rate);

		if (clk_get_parent(clk) != parent_clk)
			clk_set_parent(clk, parent_clk);
	}

	if (dc->out->type == TEGRA_DC_OUT_DSI) {
		unsigned long rate;
		struct clk *parent_clk;
		struct clk *base_clk;

		if (clk == dc->clk) {
			parent_clk = clk_get_sys(NULL,
					dc->out->parent_clk ? : "pll_d_out0");
			base_clk = clk_get_parent(parent_clk);
			tegra_clk_cfg_ex(base_clk,
					TEGRA_CLK_PLLD_DSI_OUT_ENB, 1);
		} else {
			if (dc->pdata->default_out->dsi->dsi_instance) {
				parent_clk = clk_get_sys(NULL,
					dc->out->parent_clk ? : "pll_d2_out0");
				base_clk = clk_get_parent(parent_clk);
				tegra_clk_cfg_ex(base_clk,
						TEGRA_CLK_PLLD_CSI_OUT_ENB, 1);
			} else {
				parent_clk = clk_get_sys(NULL,
					dc->out->parent_clk ? : "pll_d_out0");
				base_clk = clk_get_parent(parent_clk);
				tegra_clk_cfg_ex(base_clk,
						TEGRA_CLK_PLLD_DSI_OUT_ENB, 1);
			}
		}

		rate = dc->mode.pclk;
		if (rate != clk_get_rate(base_clk))
			clk_set_rate(base_clk, rate);

		if (clk_get_parent(clk) != parent_clk)
			clk_set_parent(clk, parent_clk);
	}

	pclk = tegra_dc_pclk_round_rate(dc, dc->mode.pclk);
	tegra_dvfs_set_rate(clk, pclk);
}

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
	a = 1; /* Constraint 5: V_REF_TO_SYNC >= 1 */

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

static bool check_ref_to_sync(struct tegra_dc_mode *mode)
{
	/* Constraint 1: H_REF_TO_SYNC + H_SYNC_WIDTH + H_BACK_PORCH > 11. */
	if (mode->h_ref_to_sync + mode->h_sync_width + mode->h_back_porch <= 11)
		return false;

	/* Constraint 2: V_REF_TO_SYNC + V_SYNC_WIDTH + V_BACK_PORCH > 1. */
	if (mode->v_ref_to_sync + mode->v_sync_width + mode->v_back_porch <= 1)
		return false;

	/* Constraint 3: V_FRONT_PORCH + V_SYNC_WIDTH + V_BACK_PORCH > 1
	 * (vertical blank). */
	if (mode->v_front_porch + mode->v_sync_width + mode->v_back_porch <= 1)
		return false;

	/* Constraint 4: V_SYNC_WIDTH >= 1; H_SYNC_WIDTH >= 1. */
	if (mode->v_sync_width < 1 || mode->h_sync_width < 1)
		return false;

	/* Constraint 5: V_REF_TO_SYNC >= 1; H_REF_TO_SYNC >= 0. */
	if (mode->v_ref_to_sync < 1 || mode->h_ref_to_sync < 0)
		return false;

	/* Constraint 6: V_FRONT_PORT >= (V_REF_TO_SYNC + 1);
	 * H_FRONT_PORT >= (H_REF_TO_SYNC + 1). */
	if (mode->v_front_porch < mode->v_ref_to_sync + 1 ||
		mode->h_front_porch < mode->h_ref_to_sync + 1)
		return false;

	/* Constraint 7: H_DISP_ACTIVE >= 16; V_DISP_ACTIVE >= 16. */
	if (mode->h_active < 16 || mode->v_active < 16)
		return false;

	return true;
}

#ifdef DEBUG
/* return in 1000ths of a Hertz */
static int calc_refresh(struct tegra_dc *dc, const struct tegra_dc_mode *m)
{
	long h_total, v_total, refresh;
	h_total = m->h_active + m->h_front_porch + m->h_back_porch +
		m->h_sync_width;
	v_total = m->v_active + m->v_front_porch + m->v_back_porch +
		m->v_sync_width;
	refresh = dc->pixel_clk / h_total;
	refresh *= 1000;
	refresh /= v_total;
	return refresh;
}

static void print_mode(struct tegra_dc *dc,
			const struct tegra_dc_mode *mode, const char *note)
{
	if (mode) {
		int refresh = calc_refresh(dc, mode);
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

static inline void enable_dc_irq(unsigned int irq)
{
#ifdef CONFIG_TEGRA_SILICON_PLATFORM
	enable_irq(irq);
#else
	/* Always disable DC interrupts on FPGA. */
	disable_irq(irq);
#endif
}

static inline void disable_dc_irq(unsigned int irq)
{
	disable_irq(irq);
}

static int tegra_dc_program_mode(struct tegra_dc *dc, struct tegra_dc_mode *mode)
{
	unsigned long val;
	unsigned long rate;
	unsigned long div;
	unsigned long pclk;

	print_mode(dc, mode, __func__);

	/* use default EMC rate when switching modes */
	dc->new_emc_clk_rate = tegra_dc_get_default_emc_clk_rate(dc);
	tegra_dc_program_bandwidth(dc);

	tegra_dc_writel(dc, 0x0, DC_DISP_DISP_TIMING_OPTIONS);
	tegra_dc_writel(dc, mode->h_ref_to_sync | (mode->v_ref_to_sync << 16),
			DC_DISP_REF_TO_SYNC);
	tegra_dc_writel(dc, mode->h_sync_width | (mode->v_sync_width << 16),
			DC_DISP_SYNC_WIDTH);
	tegra_dc_writel(dc, mode->h_back_porch | (mode->v_back_porch << 16),
			DC_DISP_BACK_PORCH);
	tegra_dc_writel(dc, mode->h_active | (mode->v_active << 16),
			DC_DISP_DISP_ACTIVE);
	tegra_dc_writel(dc, mode->h_front_porch | (mode->v_front_porch << 16),
			DC_DISP_FRONT_PORCH);

	tegra_dc_writel(dc, DE_SELECT_ACTIVE | DE_CONTROL_NORMAL,
			DC_DISP_DATA_ENABLE_OPTIONS);

	val = tegra_dc_readl(dc, DC_COM_PIN_OUTPUT_POLARITY1);
	if (mode->flags & TEGRA_DC_MODE_FLAG_NEG_V_SYNC)
		val |= PIN1_LVS_OUTPUT;
	else
		val &= ~PIN1_LVS_OUTPUT;

	if (mode->flags & TEGRA_DC_MODE_FLAG_NEG_H_SYNC)
		val |= PIN1_LHS_OUTPUT;
	else
		val &= ~PIN1_LHS_OUTPUT;
	tegra_dc_writel(dc, val, DC_COM_PIN_OUTPUT_POLARITY1);

	/* TODO: MIPI/CRT/HDMI clock cals */

	val = DISP_DATA_FORMAT_DF1P1C;

	if (dc->out->align == TEGRA_DC_ALIGN_MSB)
		val |= DISP_DATA_ALIGNMENT_MSB;
	else
		val |= DISP_DATA_ALIGNMENT_LSB;

	if (dc->out->order == TEGRA_DC_ORDER_RED_BLUE)
		val |= DISP_DATA_ORDER_RED_BLUE;
	else
		val |= DISP_DATA_ORDER_BLUE_RED;

	tegra_dc_writel(dc, val, DC_DISP_DISP_INTERFACE_CONTROL);

	rate = tegra_dc_clk_get_rate(dc);

	pclk = tegra_dc_pclk_round_rate(dc, mode->pclk);
	if (pclk < (mode->pclk / 100 * 99) ||
	    pclk > (mode->pclk / 100 * 109)) {
		dev_err(&dc->ndev->dev,
			"can't divide %ld clock to %d -1/+9%% %ld %d %d\n",
			rate, mode->pclk,
			pclk, (mode->pclk / 100 * 99),
			(mode->pclk / 100 * 109));
		return -EINVAL;
	}

	div = (rate * 2 / pclk) - 2;

	tegra_dc_writel(dc, 0x00010001,
			DC_DISP_SHIFT_CLOCK_OPTIONS);
	tegra_dc_writel(dc, PIXEL_CLK_DIVIDER_PCD1 | SHIFT_CLK_DIVIDER(div),
			DC_DISP_DISP_CLOCK_CONTROL);

#ifdef CONFIG_SWITCH
	switch_set_state(&dc->modeset_switch,
			 (mode->h_active << 16) | mode->v_active);
#endif

	dc->pixel_clk = dc->mode.pclk;

	return 0;
}


int tegra_dc_set_mode(struct tegra_dc *dc, const struct tegra_dc_mode *mode)
{
	memcpy(&dc->mode, mode, sizeof(dc->mode));

	print_mode(dc, mode, __func__);

	return 0;
}
EXPORT_SYMBOL(tegra_dc_set_mode);

int tegra_dc_set_fb_mode(struct tegra_dc *dc,
		const struct fb_videomode *fbmode, bool stereo_mode)
{
	struct tegra_dc_mode mode;

	if (!fbmode->pixclock)
		return -EINVAL;

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
	if (dc->out->type == TEGRA_DC_OUT_HDMI) {
		/* HDMI controller requires h_ref=1, v_ref=1 */
		mode.h_ref_to_sync = 1;
		mode.v_ref_to_sync = 1;
	} else {
		calc_ref_to_sync(&mode);
	}
	if (!check_ref_to_sync(&mode)) {
		dev_err(&dc->ndev->dev,
				"Display timing doesn't meet restrictions.\n");
		return -EINVAL;
	}
	dev_info(&dc->ndev->dev, "Using mode %dx%d pclk=%d href=%d vref=%d\n",
		mode.h_active, mode.v_active, mode.pclk,
		mode.h_ref_to_sync, mode.v_ref_to_sync
	);

	if (mode.stereo_mode) {
		mode.pclk *= 2;
		/* total v_active = yres*2 + activespace */
		mode.v_active = fbmode->yres*2 +
				fbmode->vsync_len +
				fbmode->upper_margin +
				fbmode->lower_margin;
	}

	mode.flags = 0;

	if (!(fbmode->sync & FB_SYNC_HOR_HIGH_ACT))
		mode.flags |= TEGRA_DC_MODE_FLAG_NEG_H_SYNC;

	if (!(fbmode->sync & FB_SYNC_VERT_HIGH_ACT))
		mode.flags |= TEGRA_DC_MODE_FLAG_NEG_V_SYNC;

	return tegra_dc_set_mode(dc, &mode);
}
EXPORT_SYMBOL(tegra_dc_set_fb_mode);

void
tegra_dc_config_pwm(struct tegra_dc *dc, struct tegra_dc_pwm_params *cfg)
{
	unsigned int ctrl;
	unsigned long out_sel;
	unsigned long cmd_state;

	mutex_lock(&dc->lock);
	if (!dc->enabled) {
		mutex_unlock(&dc->lock);
		return;
	}

	ctrl = ((cfg->period << PM_PERIOD_SHIFT) |
		(cfg->clk_div << PM_CLK_DIVIDER_SHIFT) |
		cfg->clk_select);

	/* The new value should be effected immediately */
	cmd_state = tegra_dc_readl(dc, DC_CMD_STATE_ACCESS);
	tegra_dc_writel(dc, (cmd_state | (1 << 2)), DC_CMD_STATE_ACCESS);

	if (cfg->switch_to_sfio && cfg->gpio_conf_to_sfio)
		cfg->switch_to_sfio(cfg->gpio_conf_to_sfio);
	else
		dev_err(&dc->ndev->dev, "Error: Need gpio_conf_to_sfio\n");

	switch (cfg->which_pwm) {
	case TEGRA_PWM_PM0:
		/* Select the LM0 on PM0 */
		out_sel = tegra_dc_readl(dc, DC_COM_PIN_OUTPUT_SELECT5);
		out_sel &= ~(7 << 0);
		out_sel |= (3 << 0);
		tegra_dc_writel(dc, out_sel, DC_COM_PIN_OUTPUT_SELECT5);
		tegra_dc_writel(dc, ctrl, DC_COM_PM0_CONTROL);
		tegra_dc_writel(dc, cfg->duty_cycle, DC_COM_PM0_DUTY_CYCLE);
		break;
	case TEGRA_PWM_PM1:
		/* Select the LM1 on PM1 */
		out_sel = tegra_dc_readl(dc, DC_COM_PIN_OUTPUT_SELECT5);
		out_sel &= ~(7 << 4);
		out_sel |= (3 << 4);
		tegra_dc_writel(dc, out_sel, DC_COM_PIN_OUTPUT_SELECT5);
		tegra_dc_writel(dc, ctrl, DC_COM_PM1_CONTROL);
		tegra_dc_writel(dc, cfg->duty_cycle, DC_COM_PM1_DUTY_CYCLE);
		break;
	default:
		dev_err(&dc->ndev->dev, "Error: Need which_pwm\n");
		break;
	}
	tegra_dc_writel(dc, cmd_state, DC_CMD_STATE_ACCESS);
	mutex_unlock(&dc->lock);
}
EXPORT_SYMBOL(tegra_dc_config_pwm);

static void tegra_dc_set_out_pin_polars(struct tegra_dc *dc,
				const struct tegra_dc_out_pin *pins,
				const unsigned int n_pins)
{
	unsigned int i;

	int name;
	int pol;

	u32 pol1, pol3;

	u32 set1, unset1;
	u32 set3, unset3;

	set1 = set3 = unset1 = unset3 = 0;

	for (i = 0; i < n_pins; i++) {
		name = (pins + i)->name;
		pol  = (pins + i)->pol;

		/* set polarity by name */
		switch (name) {
		case TEGRA_DC_OUT_PIN_DATA_ENABLE:
			if (pol == TEGRA_DC_OUT_PIN_POL_LOW)
				set3 |= LSPI_OUTPUT_POLARITY_LOW;
			else
				unset3 |= LSPI_OUTPUT_POLARITY_LOW;
			break;
		case TEGRA_DC_OUT_PIN_H_SYNC:
			if (pol == TEGRA_DC_OUT_PIN_POL_LOW)
				set1 |= LHS_OUTPUT_POLARITY_LOW;
			else
				unset1 |= LHS_OUTPUT_POLARITY_LOW;
			break;
		case TEGRA_DC_OUT_PIN_V_SYNC:
			if (pol == TEGRA_DC_OUT_PIN_POL_LOW)
				set1 |= LVS_OUTPUT_POLARITY_LOW;
			else
				unset1 |= LVS_OUTPUT_POLARITY_LOW;
			break;
		case TEGRA_DC_OUT_PIN_PIXEL_CLOCK:
			if (pol == TEGRA_DC_OUT_PIN_POL_LOW)
				set1 |= LSC0_OUTPUT_POLARITY_LOW;
			else
				unset1 |= LSC0_OUTPUT_POLARITY_LOW;
			break;
		default:
			printk("Invalid argument in function %s\n",
			       __FUNCTION__);
			break;
		}
	}

	pol1 = tegra_dc_readl(dc, DC_COM_PIN_OUTPUT_POLARITY1);
	pol3 = tegra_dc_readl(dc, DC_COM_PIN_OUTPUT_POLARITY3);

	pol1 |= set1;
	pol1 &= ~unset1;

	pol3 |= set3;
	pol3 &= ~unset3;

	tegra_dc_writel(dc, pol1, DC_COM_PIN_OUTPUT_POLARITY1);
	tegra_dc_writel(dc, pol3, DC_COM_PIN_OUTPUT_POLARITY3);
}

static void tegra_dc_set_out(struct tegra_dc *dc, struct tegra_dc_out *out)
{
	dc->out = out;

	if (out->n_modes > 0)
		tegra_dc_set_mode(dc, &dc->out->modes[0]);

	switch (out->type) {
	case TEGRA_DC_OUT_RGB:
		dc->out_ops = &tegra_dc_rgb_ops;
		break;

	case TEGRA_DC_OUT_HDMI:
		dc->out_ops = &tegra_dc_hdmi_ops;
		break;

	case TEGRA_DC_OUT_DSI:
		dc->out_ops = &tegra_dc_dsi_ops;
		break;

	default:
		dc->out_ops = NULL;
		break;
	}

	if (dc->out_ops && dc->out_ops->init)
		dc->out_ops->init(dc);

}

unsigned tegra_dc_get_out_height(const struct tegra_dc *dc)
{
	if (dc->out)
		return dc->out->height;
	else
		return 0;
}
EXPORT_SYMBOL(tegra_dc_get_out_height);

unsigned tegra_dc_get_out_width(const struct tegra_dc *dc)
{
	if (dc->out)
		return dc->out->width;
	else
		return 0;
}
EXPORT_SYMBOL(tegra_dc_get_out_width);

unsigned tegra_dc_get_out_max_pixclock(const struct tegra_dc *dc)
{
	if (dc->out && dc->out->max_pixclock)
		return dc->out->max_pixclock;
	else
		return 0;
}
EXPORT_SYMBOL(tegra_dc_get_out_max_pixclock);

void tegra_dc_enable_crc(struct tegra_dc *dc)
{
	u32 val;
	tegra_dc_io_start(dc);

	val = CRC_ALWAYS_ENABLE | CRC_INPUT_DATA_ACTIVE_DATA |
		CRC_ENABLE_ENABLE;
	tegra_dc_writel(dc, val, DC_COM_CRC_CONTROL);
	tegra_dc_writel(dc, GENERAL_UPDATE, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);
}

void tegra_dc_disable_crc(struct tegra_dc *dc)
{
	tegra_dc_writel(dc, 0x0, DC_COM_CRC_CONTROL);
	tegra_dc_writel(dc, GENERAL_UPDATE, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);

	tegra_dc_io_end(dc);
}

u32 tegra_dc_read_checksum_latched(struct tegra_dc *dc)
{
	int crc = 0;

	if(!dc) {
		pr_err("%s Failed to get dc.\n", __func__);
		goto crc_error;
	}

	/* TODO: Replace mdelay with code to sync VBlANK, since
	 * DC_COM_CRC_CHECKSUM_LATCHED is available after VBLANK */
	mdelay(TEGRA_CRC_LATCHED_DELAY);

	crc = tegra_dc_readl(dc, DC_COM_CRC_CHECKSUM_LATCHED);
crc_error:
	return crc;
}

static void tegra_dc_vblank(struct work_struct *work)
{
	struct tegra_dc *dc = container_of(work, struct tegra_dc, vblank_work);
	bool nvsd_updated = false;

	mutex_lock(&dc->lock);

	/* update EMC clock if calculated bandwidth has changed */
	tegra_dc_program_bandwidth(dc);

	/* Update the SD brightness */
	if (dc->enabled && dc->out->sd_settings)
		nvsd_updated = nvsd_update_brightness(dc);

	mutex_unlock(&dc->lock);

	/* Do the actual brightness update outside of the mutex */
	if (nvsd_updated && dc->out->sd_settings &&
	    dc->out->sd_settings->bl_device) {

		struct platform_device *pdev = dc->out->sd_settings->bl_device;
		struct backlight_device *bl = platform_get_drvdata(pdev);
		if (bl)
			backlight_update_status(bl);
	}
}

#ifdef CONFIG_TEGRA_SILICON_PLATFORM
static void tegra_dc_underflow_handler(struct tegra_dc *dc)
{
	u32 val, i;

	/* Check for any underflow reset conditions */
	for (i = 0; i < DC_N_WINDOWS; i++) {
		if (dc->underflow_mask & (WIN_A_UF_INT << i)) {
			dc->windows[i].underflows++;

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
			if (dc->windows[i].underflows > 4)
				schedule_work(&dc->reset_work);
#endif
		} else {
			dc->windows[i].underflows = 0;
		}
	}

	if (!dc->underflow_mask) {
		/* If we have no underflow to check, go ahead
		   and disable the interrupt */
		val = tegra_dc_readl(dc, DC_CMD_INT_MASK);
		if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE)
			val &= ~FRAME_END_INT;
		else
			val &= ~V_BLANK_INT;
		tegra_dc_writel(dc, val, DC_CMD_INT_MASK);
	}

	/* Clear the underflow mask now that we've checked it. */
	dc->underflow_mask = 0;
}

static void tegra_dc_trigger_windows(struct tegra_dc *dc)
{
	u32 val, i;
	u32 completed = 0;
	u32 vdirty = 0, hdirty = 0;

	val = tegra_dc_readl(dc, DC_CMD_STATE_CONTROL);
	for (i = 0; i < DC_N_WINDOWS; i++) {
		if (!(val & (WIN_A_ACT_REQ << i))) {
			dc->windows[i].dirty = 0;
			dc->windows[i].last_flip_vblank_syncpt_val =
				nvhost_syncpt_update_min(
					&dc->ndev->host->syncpt,
					dc->vblank_syncpt);
			dc->windows[i].has_flipped_once_already = true;
			completed = 1;
		} else {
			if (dc->windows[i].flip_wait_for_vblank)
				vdirty = 1;
			else
				hdirty = 1;
		}
	}

	if (!vdirty || !hdirty) {
		u32 orig = tegra_dc_readl(dc, DC_CMD_INT_MASK);
		val = orig;

		if (!vdirty) {
			if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE)
				val &= ~V_BLANK_INT;
			else
				val &= ~FRAME_END_INT;
		}

		if (!hdirty)
			val &= ~H_BLANK_INT;

		if (completed && !vdirty && !hdirty) {
			/* With the last completed window, go ahead
			   and enable the vblank interrupt for nvsd. */
			val |= V_BLANK_INT;
		}

		if (orig != val)
			tegra_dc_writel(dc, val, DC_CMD_INT_MASK);
	}

	if (completed)
		wake_up(&dc->wq);
}

static void tegra_dc_one_shot_irq(struct tegra_dc *dc, unsigned long status)
{
	spin_lock(&dc->update_spinlock);

	if (status & (V_BLANK_INT | H_BLANK_INT)) {
		/* Sync up windows. */
		tegra_dc_trigger_windows(dc);
	}

	if (status & V_BLANK_INT) {
		/* Schedule any additional bottom-half vblank actvities. */
		schedule_work(&dc->vblank_work);
	}

	/* Check underflow at frame end */
	if (status & FRAME_END_INT) {
		/* Check underflow */
		tegra_dc_underflow_handler(dc);

		/* Mark the frame_end as complete. */
		if (completion_done(&dc->frame_end_complete))
			complete(&dc->frame_end_complete);
	}

	spin_unlock(&dc->update_spinlock);
}

static void tegra_dc_continuous_irq(struct tegra_dc *dc, unsigned long status)
{
	spin_lock(&dc->update_spinlock);

	if (status & V_BLANK_INT) {
		/* Check underflow */
		tegra_dc_underflow_handler(dc);

		/* Schedule any additional bottom-half vblank actvities. */
		schedule_work(&dc->vblank_work);
	}

	if (status & (FRAME_END_INT | H_BLANK_INT)) {
		/* Sync up windows. */
		tegra_dc_trigger_windows(dc);
	}

	spin_unlock(&dc->update_spinlock);
}
#endif

/* return an arbitrarily large number if count overflow occurs.
 * make it a nice base-10 number to show up in stats output */
static u64 tegra_dc_underflow_count(struct tegra_dc *dc, unsigned reg)
{
	unsigned count = tegra_dc_readl(dc, reg);
	tegra_dc_writel(dc, 0, reg);
	return ((count & 0x80000000) == 0) ? count : 10000000000ll;
}

static irqreturn_t tegra_dc_irq(int irq, void *ptr)
{
#ifdef CONFIG_TEGRA_SILICON_PLATFORM
	struct tegra_dc *dc = ptr;
	unsigned long status;
	unsigned long val;
	unsigned long underflow_mask;

	status = tegra_dc_readl(dc, DC_CMD_INT_STATUS);
	tegra_dc_writel(dc, status, DC_CMD_INT_STATUS);

	/*
	 * Overlays can get thier internal state corrupted during and underflow
	 * condition.  The only way to fix this state is to reset the DC.
	 * if we get 4 consecutive frames with underflows, assume we're
	 * hosed and reset.
	 */
	underflow_mask = status & ALL_UF_INT;

	if (underflow_mask) {
		val = tegra_dc_readl(dc, DC_CMD_INT_MASK);
		val |= V_BLANK_INT;
		tegra_dc_writel(dc, val, DC_CMD_INT_MASK);
		dc->underflow_mask |= underflow_mask;
		dc->stats.underflows++;
		if (status & WIN_A_UF_INT)
			dc->stats.underflows_a += tegra_dc_underflow_count(dc,
				DC_WINBUF_AD_UFLOW_STATUS);
		if (status & WIN_B_UF_INT)
			dc->stats.underflows_b += tegra_dc_underflow_count(dc,
				DC_WINBUF_BD_UFLOW_STATUS);
		if (status & WIN_C_UF_INT)
			dc->stats.underflows_c += tegra_dc_underflow_count(dc,
				DC_WINBUF_CD_UFLOW_STATUS);
	}

	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE)
		tegra_dc_one_shot_irq(dc, status);
	else
		tegra_dc_continuous_irq(dc, status);

	return IRQ_HANDLED;
#else /* CONFIG_TEGRA_SILICON_PLATFORM */
	return IRQ_NONE;
#endif /* !CONFIG_TEGRA_SILICON_PLATFORM */
}

static void tegra_dc_set_color_control(struct tegra_dc *dc)
{
	u32 color_control;

	switch (dc->out->depth) {
	case 3:
		color_control = BASE_COLOR_SIZE111;
		break;

	case 6:
		color_control = BASE_COLOR_SIZE222;
		break;

	case 8:
		color_control = BASE_COLOR_SIZE332;
		break;

	case 9:
		color_control = BASE_COLOR_SIZE333;
		break;

	case 12:
		color_control = BASE_COLOR_SIZE444;
		break;

	case 15:
		color_control = BASE_COLOR_SIZE555;
		break;

	case 16:
		color_control = BASE_COLOR_SIZE565;
		break;

	case 18:
		color_control = BASE_COLOR_SIZE666;
		break;

	default:
		color_control = BASE_COLOR_SIZE888;
		break;
	}

	switch (dc->out->dither) {
	case TEGRA_DC_DISABLE_DITHER:
		color_control |= DITHER_CONTROL_DISABLE;
		break;
	case TEGRA_DC_ORDERED_DITHER:
		color_control |= DITHER_CONTROL_ORDERED;
		break;
	case TEGRA_DC_ERRDIFF_DITHER:
		/* The line buffer for error-diffusion dither is limited
		 * to 1280 pixels per line. This limits the maximum
		 * horizontal active area size to 1280 pixels when error
		 * diffusion is enabled.
		 */
		BUG_ON(dc->mode.h_active > 1280);
		color_control |= DITHER_CONTROL_ERRDIFF;
		break;
	}

	tegra_dc_writel(dc, color_control, DC_DISP_DISP_COLOR_CONTROL);
}

static u32 get_syncpt(struct tegra_dc *dc, int idx)
{
	u32 syncpt_id;

	switch (dc->ndev->id) {
	case 0:
		switch (idx) {
		case 0:
			syncpt_id = NVSYNCPT_DISP0_A;
			break;
		case 1:
			syncpt_id = NVSYNCPT_DISP0_B;
			break;
		case 2:
			syncpt_id = NVSYNCPT_DISP0_C;
			break;
		default:
			BUG();
			break;
		}
		break;
	case 1:
		switch (idx) {
		case 0:
			syncpt_id = NVSYNCPT_DISP1_A;
			break;
		case 1:
			syncpt_id = NVSYNCPT_DISP1_B;
			break;
		case 2:
			syncpt_id = NVSYNCPT_DISP1_C;
			break;
		default:
			BUG();
			break;
		}
		break;
	default:
		BUG();
		break;
	}

	return syncpt_id;
}

static void tegra_dc_init(struct tegra_dc *dc)
{
	int i;

	tegra_dc_writel(dc, 0x00000100, DC_CMD_GENERAL_INCR_SYNCPT_CNTRL);
	if (dc->ndev->id == 0) {
		tegra_mc_set_priority(TEGRA_MC_CLIENT_DISPLAY0A,
				      TEGRA_MC_PRIO_MED);
		tegra_mc_set_priority(TEGRA_MC_CLIENT_DISPLAY0B,
				      TEGRA_MC_PRIO_MED);
		tegra_mc_set_priority(TEGRA_MC_CLIENT_DISPLAY0C,
				      TEGRA_MC_PRIO_MED);
		tegra_mc_set_priority(TEGRA_MC_CLIENT_DISPLAY1B,
				      TEGRA_MC_PRIO_MED);
		tegra_mc_set_priority(TEGRA_MC_CLIENT_DISPLAYHC,
				      TEGRA_MC_PRIO_HIGH);
	} else if (dc->ndev->id == 1) {
		tegra_mc_set_priority(TEGRA_MC_CLIENT_DISPLAY0AB,
				      TEGRA_MC_PRIO_MED);
		tegra_mc_set_priority(TEGRA_MC_CLIENT_DISPLAY0BB,
				      TEGRA_MC_PRIO_MED);
		tegra_mc_set_priority(TEGRA_MC_CLIENT_DISPLAY0CB,
				      TEGRA_MC_PRIO_MED);
		tegra_mc_set_priority(TEGRA_MC_CLIENT_DISPLAY1BB,
				      TEGRA_MC_PRIO_MED);
		tegra_mc_set_priority(TEGRA_MC_CLIENT_DISPLAYHCB,
				      TEGRA_MC_PRIO_HIGH);
	}
	tegra_dc_writel(dc, 0x00000100 | dc->vblank_syncpt,
			DC_CMD_CONT_SYNCPT_VSYNC);
	tegra_dc_writel(dc, 0x00004700, DC_CMD_INT_TYPE);
	tegra_dc_writel(dc, 0x0001c700, DC_CMD_INT_POLARITY);
	tegra_dc_writel(dc, 0x00202020, DC_DISP_MEM_HIGH_PRIORITY);
	tegra_dc_writel(dc, 0x00010101, DC_DISP_MEM_HIGH_PRIORITY_TIMER);

	/*
	 * We toggle frame_end, vblank, and hblank interrupts using the
	 * mask instead of the enable bit so that pending interrupt
	 * clearing still works. We enable underflow interrupts always.
	 */
	tegra_dc_writel(dc, (FRAME_END_INT | V_BLANK_INT | H_BLANK_INT |
			ALL_UF_INT), DC_CMD_INT_ENABLE);
	tegra_dc_writel(dc, ALL_UF_INT, DC_CMD_INT_MASK);

	tegra_dc_writel(dc, WRITE_MUX_ASSEMBLY | READ_MUX_ASSEMBLY,
			DC_CMD_STATE_ACCESS);

	tegra_dc_writel(dc, 0x00000000, DC_DISP_BORDER_COLOR);

	tegra_dc_set_color_control(dc);
	for (i = 0; i < DC_N_WINDOWS; i++) {
		tegra_dc_writel(dc, WINDOW_A_SELECT << i,
				DC_CMD_DISPLAY_WINDOW_HEADER);
		tegra_dc_init_csc_defaults(&dc->windows[i].csc);
		tegra_dc_set_csc(dc, &dc->windows[i].csc);
		tegra_dc_init_lut_defaults(&dc->windows[i].lut);
		tegra_dc_set_lut(dc, &dc->windows[i].lut, 0, 256);
		tegra_dc_set_scaling_filter(dc);
	}


	for (i = 0; i < dc->n_windows; i++) {
		u32 syncpt = get_syncpt(dc, i);

		dc->syncpt[i].id = syncpt;

		dc->syncpt[i].min = dc->syncpt[i].max =
			nvhost_syncpt_read(&dc->ndev->host->syncpt, syncpt);
	}

	print_mode(dc, &dc->mode, __func__);

	if (dc->mode.pclk && !nomodeset)
		tegra_dc_program_mode(dc, &dc->mode);

	/* Initialize SD AFTER the modeset.
	   nvsd_init handles the sd_settings = NULL case. */
	nvsd_init(dc, dc->out->sd_settings);

	spin_lock_init(&dc->update_spinlock);
}

static bool _tegra_dc_controller_enable(struct tegra_dc *dc)
{
	int ret;
	if (dc->out->enable)
		dc->out->enable();

	tegra_dc_setup_clk(dc, dc->clk);

	ret = clk_enable(dc->clk);
	if (ret)
	{
		dev_err(&dc->ndev->dev, "Could not enable dc clock\n");
		goto dc_err;
	}

	ret = clk_enable(dc->emc_clk);
	if (ret)
	{
		dev_err(&dc->ndev->dev, "Could not enable emc clock\n");
		clk_disable(dc->clk);
		goto dc_err;
	}

	enable_dc_irq(dc->irq);

	tegra_dc_init(dc);

	if (dc->out_ops && dc->out_ops->enable)
		dc->out_ops->enable(dc);

	if (dc->out->out_pins)
		tegra_dc_set_out_pin_polars(dc, dc->out->out_pins,
					    dc->out->n_out_pins);

	if (dc->out->postpoweron)
		dc->out->postpoweron();

	/* force a full blending update */
	dc->blend.z[0] = -1;

	tegra_dc_ext_enable(dc->ext);

	return true;

dc_err:
	pr_err("Failed to enable dc controller\n");
	return ret;
}

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
static bool _tegra_dc_controller_reset_enable(struct tegra_dc *dc)
{
	if (dc->out->enable)
		dc->out->enable();

	tegra_dc_setup_clk(dc, dc->clk);
	clk_enable(dc->clk);
	clk_enable(dc->emc_clk);

	if (dc->ndev->id == 0 && tegra_dcs[1] != NULL) {
		mutex_lock(&tegra_dcs[1]->lock);
		disable_irq(tegra_dcs[1]->irq);
	} else if (dc->ndev->id == 1 && tegra_dcs[0] != NULL) {
		mutex_lock(&tegra_dcs[0]->lock);
		disable_irq(tegra_dcs[0]->irq);
	}

	msleep(5);
	tegra_periph_reset_assert(dc->clk);
	msleep(2);
#ifdef CONFIG_TEGRA_SILICON_PLATFORM
	tegra_periph_reset_deassert(dc->clk);
	msleep(1);
#endif

	if (dc->ndev->id == 0 && tegra_dcs[1] != NULL) {
		enable_dc_irq(tegra_dcs[1]->irq);
		mutex_unlock(&tegra_dcs[1]->lock);
	} else if (dc->ndev->id == 1 && tegra_dcs[0] != NULL) {
		enable_dc_irq(tegra_dcs[0]->irq);
		mutex_unlock(&tegra_dcs[0]->lock);
	}

	enable_dc_irq(dc->irq);

	tegra_dc_init(dc);

	if (dc->out_ops && dc->out_ops->enable)
		dc->out_ops->enable(dc);

	if (dc->out->out_pins)
		tegra_dc_set_out_pin_polars(dc, dc->out->out_pins,
					    dc->out->n_out_pins);

	if (dc->out->postpoweron)
		dc->out->postpoweron();

	/* force a full blending update */
	dc->blend.z[0] = -1;

	return true;
}
#endif

static bool _tegra_dc_enable(struct tegra_dc *dc)
{
	if (dc->mode.pclk == 0)
		return false;

	if (!dc->out)
		return false;

	tegra_dc_io_start(dc);

	return _tegra_dc_controller_enable(dc);
}

void tegra_dc_enable(struct tegra_dc *dc)
{
	mutex_lock(&dc->lock);

	if (!dc->enabled)
		dc->enabled = _tegra_dc_enable(dc);

	mutex_unlock(&dc->lock);
}

static void _tegra_dc_controller_disable(struct tegra_dc *dc)
{
	unsigned i;

	tegra_dc_writel(dc, 0, DC_CMD_INT_MASK);
	tegra_dc_writel(dc, 0, DC_CMD_INT_ENABLE);
	disable_irq(dc->irq);

	if (dc->out_ops && dc->out_ops->disable)
		dc->out_ops->disable(dc);

	clk_disable(dc->emc_clk);
	clk_disable(dc->clk);
	tegra_dvfs_set_rate(dc->clk, 0);

	if (dc->out && dc->out->disable)
		dc->out->disable();

	for (i = 0; i < dc->n_windows; i++) {
		struct tegra_dc_win *w = &dc->windows[i];

		/* reset window bandwidth */
		w->bandwidth = 0;
		w->new_bandwidth = 0;

		/* disable windows */
		w->flags &= ~TEGRA_WIN_FLAG_ENABLED;

		/* flush any pending syncpt waits */
		while (dc->syncpt[i].min < dc->syncpt[i].max) {
			dc->syncpt[i].min++;
			nvhost_syncpt_cpu_incr(&dc->ndev->host->syncpt,
				dc->syncpt[i].id);
		}
	}
}

void tegra_dc_stats_enable(struct tegra_dc *dc, bool enable)
{
#if 0 /* underflow interrupt is already enabled by dc reset worker */
	u32 val;
	if (dc->enabled)  {
		val = tegra_dc_readl(dc, DC_CMD_INT_ENABLE);
		if (enable)
			val |= (WIN_A_UF_INT | WIN_B_UF_INT | WIN_C_UF_INT);
		else
			val &= ~(WIN_A_UF_INT | WIN_B_UF_INT | WIN_C_UF_INT);
		tegra_dc_writel(dc, val, DC_CMD_INT_ENABLE);
	}
#endif
}

bool tegra_dc_stats_get(struct tegra_dc *dc)
{
#if 0 /* right now it is always enabled */
	u32 val;
	bool res;

	if (dc->enabled)  {
		val = tegra_dc_readl(dc, DC_CMD_INT_ENABLE);
		res = !!(val & (WIN_A_UF_INT | WIN_B_UF_INT | WIN_C_UF_INT));
	} else {
		res = false;
	}

	return res;
#endif
	return true;
}

/* make the screen blank by disabling all windows */
void tegra_dc_blank(struct tegra_dc *dc)
{
	struct tegra_dc_win *dcwins[DC_N_WINDOWS];
	unsigned i;

	for (i = 0; i < DC_N_WINDOWS; i++) {
		dcwins[i] = tegra_dc_get_window(dc, i);
		dcwins[i]->flags &= ~TEGRA_WIN_FLAG_ENABLED;
	}

	tegra_dc_update_windows(dcwins, DC_N_WINDOWS, 1);
	tegra_dc_sync_windows(dcwins, DC_N_WINDOWS);
}

static void _tegra_dc_disable(struct tegra_dc *dc)
{
	_tegra_dc_controller_disable(dc);
	tegra_dc_io_end(dc);
}

void tegra_dc_disable(struct tegra_dc *dc)
{
	if (dc->overlay)
		tegra_overlay_disable(dc->overlay);

	tegra_dc_ext_disable(dc->ext);

	mutex_lock(&dc->lock);

	if (dc->enabled) {
		dc->enabled = false;

		if (!dc->suspended)
			_tegra_dc_disable(dc);
	}

#ifdef CONFIG_SWITCH
	switch_set_state(&dc->modeset_switch, 0);
#endif

	mutex_unlock(&dc->lock);
}

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
static void tegra_dc_reset_worker(struct work_struct *work)
{
	struct tegra_dc *dc =
		container_of(work, struct tegra_dc, reset_work);

	unsigned long val = 0;

	dev_warn(&dc->ndev->dev, "overlay stuck in underflow state.  resetting.\n");

	tegra_dc_ext_disable(dc->ext);

	mutex_lock(&shared_lock);
	mutex_lock(&dc->lock);

	if (dc->enabled == false)
		goto unlock;

	dc->enabled = false;

	/*
	 * off host read bus
	 */
	val = tegra_dc_readl(dc, DC_CMD_CONT_SYNCPT_VSYNC);
	val &= ~(0x00000100);
	tegra_dc_writel(dc, val, DC_CMD_CONT_SYNCPT_VSYNC);

	/*
	 * set DC to STOP mode
	 */
	tegra_dc_writel(dc, DISP_CTRL_MODE_STOP, DC_CMD_DISPLAY_COMMAND);

	msleep(10);

	_tegra_dc_controller_disable(dc);

	/* _tegra_dc_controller_reset_enable deasserts reset */
	_tegra_dc_controller_reset_enable(dc);

	dc->enabled = true;
unlock:
	mutex_unlock(&dc->lock);
	mutex_unlock(&shared_lock);
}
#endif

#ifdef CONFIG_SWITCH
static ssize_t switch_modeset_print_mode(struct switch_dev *sdev, char *buf)
{
	struct tegra_dc *dc =
		container_of(sdev, struct tegra_dc, modeset_switch);

	if (!sdev->state)
		return sprintf(buf, "offline\n");

	return sprintf(buf, "%dx%d\n", dc->mode.h_active, dc->mode.v_active);
}
#endif

static int __init modeset_disable(char *options)
{
	nomodeset = 1;
	return 1;
}
__setup("nomodeset", modeset_disable);

static void tegra_dc_sw_edid_cont(const struct firmware *sw_edid, void *context)
{
	struct tegra_dc *dc = context;
	struct fb_monspecs specs;

	if (!sw_edid) {
		dev_warn(&dc->ndev->dev,
				"unable to read SW EDID\n");
		return;
	}

	if (dc->out->type == TEGRA_DC_OUT_HDMI) {
		struct tegra_edid *edid = tegra_dc_get_edid_struct(dc);
		struct tegra_dc_edid *edid_data;
		u8 *buf;

		/* XXX Currently, only 256-byte EDID with one extension
		 * block is supported */
		edid_data = vmalloc(SZ_256 + sizeof(struct tegra_dc_edid));
		if (!edid_data) {
			dev_err(&dc->ndev->dev, "could not allocate tegra_dc_edid\n");
			goto out;
		}

		buf = edid_data->buf;
		memset(buf, 0x0, SZ_256);
		memcpy(buf, sw_edid->data,
				(sw_edid->size > SZ_256) ? SZ_256 : sw_edid->size);
		if (tegra_edid_extract_monspecs(edid, edid_data, &specs)) {
			dev_err(&dc->ndev->dev, "error parsing SW EDID\n");
			vfree(edid_data);
			goto out;
		}

		vfree(edid_data);
	} else {
		fb_edid_to_monspecs((unsigned char *)sw_edid->data, &specs);
		if (specs.modedb == NULL) {
			dev_err(&dc->ndev->dev,
					"no modes present in SW EDID\n");
			goto out;
		}
	}

	if (tegra_dc_set_fb_mode(dc, &specs.modedb[0],
			specs.modedb[0].vmode & FB_VMODE_STEREO_MASK)) {
		dev_err(&dc->ndev->dev,
				"no valid modes in SW EDID\n");
		goto out;
	}

	if (tegra_dc_program_mode(dc, &dc->mode))
		dev_err(&dc->ndev->dev,
				"could not program the mode in SW EDID\n");

out:
	release_firmware(sw_edid);
}

static int tegra_dc_probe(struct nvhost_device *ndev)
{
	struct tegra_dc *dc;
	struct clk *clk;
	struct clk *emc_clk;
	struct resource	*res;
	struct resource *base_res;
	struct resource *fb_mem = NULL;
	int ret = 0;
	void __iomem *base;
	int irq;
	int i;

	if (!ndev->dev.platform_data) {
		dev_err(&ndev->dev, "no platform data\n");
		return -ENOENT;
	}

	dc = kzalloc(sizeof(struct tegra_dc), GFP_KERNEL);
	if (!dc) {
		dev_err(&ndev->dev, "can't allocate memory for tegra_dc\n");
		return -ENOMEM;
	}

	irq = nvhost_get_irq_byname(ndev, "irq");
	if (irq <= 0) {
		dev_err(&ndev->dev, "no irq\n");
		ret = -ENOENT;
		goto err_free;
	}

	res = nvhost_get_resource_byname(ndev, IORESOURCE_MEM, "regs");
	if (!res) {
		dev_err(&ndev->dev, "no mem resource\n");
		ret = -ENOENT;
		goto err_free;
	}

	base_res = request_mem_region(res->start, resource_size(res), ndev->name);
	if (!base_res) {
		dev_err(&ndev->dev, "request_mem_region failed\n");
		ret = -EBUSY;
		goto err_free;
	}

	base = ioremap(res->start, resource_size(res));
	if (!base) {
		dev_err(&ndev->dev, "registers can't be mapped\n");
		ret = -EBUSY;
		goto err_release_resource_reg;
	}

	fb_mem = nvhost_get_resource_byname(ndev, IORESOURCE_MEM, "fbmem");

	clk = clk_get(&ndev->dev, NULL);
	if (IS_ERR_OR_NULL(clk)) {
		dev_err(&ndev->dev, "can't get clock\n");
		ret = -ENOENT;
		goto err_iounmap_reg;
	}

	emc_clk = clk_get(&ndev->dev, "emc");
	if (IS_ERR_OR_NULL(emc_clk)) {
		dev_err(&ndev->dev, "can't get emc clock\n");
		ret = -ENOENT;
		goto err_put_clk;
	}

	dc->clk = clk;
	dc->emc_clk = emc_clk;

	dc->base_res = base_res;
	dc->base = base;
	dc->irq = irq;
	dc->ndev = ndev;
	dc->pdata = ndev->dev.platform_data;

	/*
	 * The emc is a shared clock, it will be set based on
	 * the requirements for each user on the bus.
	 */
	dc->emc_clk_rate = tegra_dc_get_default_emc_clk_rate(dc);
	clk_set_rate(emc_clk, dc->emc_clk_rate);

	if (dc->pdata->flags & TEGRA_DC_FLAG_ENABLED)
		dc->enabled = true;

	mutex_init(&dc->lock);
	init_completion(&dc->frame_end_complete);
	init_waitqueue_head(&dc->wq);
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	INIT_WORK(&dc->reset_work, tegra_dc_reset_worker);
#endif
	INIT_WORK(&dc->vblank_work, tegra_dc_vblank);

	dc->n_windows = DC_N_WINDOWS;
	for (i = 0; i < dc->n_windows; i++) {
		dc->windows[i].idx = i;
		dc->windows[i].dc = dc;
	}

	if (request_irq(irq, tegra_dc_irq, IRQF_DISABLED,
			dev_name(&ndev->dev), dc)) {
		dev_err(&ndev->dev, "request_irq %d failed\n", irq);
		ret = -EBUSY;
		goto err_put_emc_clk;
	}

	/* hack to balance enable_irq calls in _tegra_dc_enable() */
	disable_dc_irq(dc->irq);

	ret = tegra_dc_set(dc, ndev->id);
	if (ret < 0) {
		dev_err(&ndev->dev, "can't add dc\n");
		goto err_free_irq;
	}

	nvhost_set_drvdata(ndev, dc);

#ifdef CONFIG_SWITCH
	dc->modeset_switch.name = dev_name(&ndev->dev);
	dc->modeset_switch.state = 0;
	dc->modeset_switch.print_state = switch_modeset_print_mode;
	switch_dev_register(&dc->modeset_switch);
#endif

	if (dc->pdata->default_out)
		tegra_dc_set_out(dc, dc->pdata->default_out);
	else
		dev_err(&ndev->dev, "No default output specified.  Leaving output disabled.\n");

	dc->vblank_syncpt = (dc->ndev->id == 0) ?
		NVSYNCPT_VBLANK0 : NVSYNCPT_VBLANK1;

	dc->ext = tegra_dc_ext_register(ndev, dc);
	if (IS_ERR_OR_NULL(dc->ext)) {
		dev_warn(&ndev->dev, "Failed to enable Tegra DC extensions.\n");
		dc->ext = NULL;
	}

	mutex_lock(&dc->lock);
	if (dc->enabled)
	{
		ret = _tegra_dc_enable(dc);
		if (!ret)
		{
			dev_err(&ndev->dev, "Can't enable dc\n");
			goto err_free_irq;
		}
	}
	mutex_unlock(&dc->lock);

	tegra_dc_create_debugfs(dc);

	dev_info(&ndev->dev, "probed\n");

	if (dc->pdata->fb) {
		if (dc->pdata->fb->bits_per_pixel == -1) {
			unsigned long fmt;
			tegra_dc_writel(dc,
					WINDOW_A_SELECT << dc->pdata->fb->win,
					DC_CMD_DISPLAY_WINDOW_HEADER);

			fmt = tegra_dc_readl(dc, DC_WIN_COLOR_DEPTH);
			dc->pdata->fb->bits_per_pixel =
				tegra_dc_fmt_bpp(fmt);
		}

		dc->fb = tegra_fb_register(ndev, dc, dc->pdata->fb, fb_mem);
		if (IS_ERR_OR_NULL(dc->fb))
			dc->fb = NULL;
	}

	if (dc->fb) {
		dc->overlay = tegra_overlay_register(ndev, dc);
		if (IS_ERR_OR_NULL(dc->overlay))
			dc->overlay = NULL;
	}

	if (dc->out && dc->out->hotplug_init)
		dc->out->hotplug_init();

	if (dc->out_ops && dc->out_ops->detect)
		dc->out_ops->detect(dc);
	else
		dc->connected = true;

	tegra_dc_create_sysfs(&dc->ndev->dev);

	if (request_firmware_nowait(THIS_MODULE, FW_ACTION_NOHOTPLUG, "sw_edid",
				&dc->ndev->dev, GFP_KERNEL, dc, tegra_dc_sw_edid_cont))
			dev_err(&dc->ndev->dev, "request_firmware_nowait failed.\n");

	return 0;

err_free_irq:
	free_irq(irq, dc);
err_put_emc_clk:
	clk_put(emc_clk);
err_put_clk:
	clk_put(clk);
err_iounmap_reg:
	iounmap(base);
	if (fb_mem)
		release_resource(fb_mem);
err_release_resource_reg:
	release_resource(base_res);
err_free:
	kfree(dc);

	return ret;
}

static int tegra_dc_remove(struct nvhost_device *ndev)
{
	struct tegra_dc *dc = nvhost_get_drvdata(ndev);

	tegra_dc_remove_sysfs(&dc->ndev->dev);
	tegra_dc_remove_debugfs(dc);

	if (dc->overlay) {
		tegra_overlay_unregister(dc->overlay);
	}

	if (dc->fb) {
		tegra_fb_unregister(dc->fb);
		if (dc->fb_mem)
			release_resource(dc->fb_mem);
	}

	tegra_dc_ext_disable(dc->ext);

	if (dc->ext)
		tegra_dc_ext_unregister(dc->ext);

	if (dc->enabled)
		_tegra_dc_disable(dc);

#ifdef CONFIG_SWITCH
	switch_dev_unregister(&dc->modeset_switch);
#endif
	free_irq(dc->irq, dc);
	clk_put(dc->emc_clk);
	clk_put(dc->clk);
	iounmap(dc->base);
	if (dc->fb_mem)
		release_resource(dc->base_res);
	kfree(dc);
	tegra_dc_set(NULL, ndev->id);
	return 0;
}

#ifdef CONFIG_PM
static int tegra_dc_suspend(struct nvhost_device *ndev, pm_message_t state)
{
	struct tegra_dc *dc = nvhost_get_drvdata(ndev);

	dev_info(&ndev->dev, "suspend\n");

	if (dc->overlay)
		tegra_overlay_disable(dc->overlay);

	tegra_dc_ext_disable(dc->ext);

	mutex_lock(&dc->lock);

	if (dc->out_ops && dc->out_ops->suspend)
		dc->out_ops->suspend(dc);

	if (dc->enabled) {
		_tegra_dc_disable(dc);

		dc->suspended = true;
	}

	if (dc->out && dc->out->postsuspend) {
		dc->out->postsuspend();
		msleep(100); /* avoid resume event due to voltage falling */
	}

	mutex_unlock(&dc->lock);

	return 0;
}

static int tegra_dc_resume(struct nvhost_device *ndev)
{
	struct tegra_dc *dc = nvhost_get_drvdata(ndev);

	dev_info(&ndev->dev, "resume\n");

	mutex_lock(&dc->lock);
	dc->suspended = false;

	if (dc->enabled)
		_tegra_dc_enable(dc);

	if (dc->out && dc->out->hotplug_init)
		dc->out->hotplug_init();

	if (dc->out_ops && dc->out_ops->resume)
		dc->out_ops->resume(dc);
	mutex_unlock(&dc->lock);

	return 0;
}

#endif /* CONFIG_PM */

int suspend_set(const char *val, struct kernel_param *kp)
{
	if (!strcmp(val, "dump"))
		dump_regs(tegra_dcs[0]);
#ifdef CONFIG_PM
	else if (!strcmp(val, "suspend"))
		tegra_dc_suspend(tegra_dcs[0]->ndev, PMSG_SUSPEND);
	else if (!strcmp(val, "resume"))
		tegra_dc_resume(tegra_dcs[0]->ndev);
#endif

	return 0;
}

int suspend_get(char *buffer, struct kernel_param *kp)
{
	return 0;
}

int suspend;

module_param_call(suspend, suspend_set, suspend_get, &suspend, 0644);

struct nvhost_driver tegra_dc_driver = {
	.driver = {
		.name = "tegradc",
		.owner = THIS_MODULE,
	},
	.probe = tegra_dc_probe,
	.remove = tegra_dc_remove,
#ifdef CONFIG_PM
	.suspend = tegra_dc_suspend,
	.resume = tegra_dc_resume,
#endif
};

static int __init tegra_dc_module_init(void)
{
	int ret = tegra_dc_ext_module_init();
	if (ret)
		return ret;
	return nvhost_driver_register(&tegra_dc_driver);
}

static void __exit tegra_dc_module_exit(void)
{
	nvhost_driver_unregister(&tegra_dc_driver);
	tegra_dc_ext_module_exit();
}

module_exit(tegra_dc_module_exit);
module_init(tegra_dc_module_init);
