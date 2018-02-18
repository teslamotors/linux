/*
 * drivers/video/tegra/dc/cursor.c
 *
 * Copyright (c) 2011-2016, NVIDIA CORPORATION, All rights reserved.
 *
 * Author:
 *  Robert Morell <rmorell@nvidia.com>
 *  Jon Mayo <jmayo@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>

#include "dc_priv.h"
#include "dc_reg.h"

/* modify val with cursor field set for a given size.
 * ignore val if it is NULL.
 * return non-zero on error, and clear val. */
static inline int cursor_size_value(enum tegra_dc_cursor_size size, u32 *val)
{
	u32 scratch = 0;

	if (!val)
		val = &scratch;
	switch (size) {
	case TEGRA_DC_CURSOR_SIZE_32X32:
		*val |= CURSOR_SIZE_32;
		return 0;
	case TEGRA_DC_CURSOR_SIZE_64X64:
		*val |= CURSOR_SIZE_64;
		return 0;
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && !defined(CONFIG_ARCH_TEGRA_3x_SOC)
	case TEGRA_DC_CURSOR_SIZE_128X128:
		*val |= CURSOR_SIZE_128;
		return 0;
	case TEGRA_DC_CURSOR_SIZE_256X256:
		*val |= CURSOR_SIZE_256;
		return 0;
#endif
	}
	*val = 0;
	return -EINVAL;
}

/* modify val with cursor blend format.
 * ignore val if it is NULL.
 * return non-zero on error, and clear val. */
static inline u32 cursor_blendfmt_value
	(enum tegra_dc_cursor_blend_format blendfmt, u32 *val)
{
	u32 scratch = 0;

	if (!val)
		val = &scratch;
	switch (blendfmt) {
	case TEGRA_DC_CURSOR_FORMAT_2BIT_LEGACY:
		/* MODE_SELECT_LEGACY */
		*val |= CURSOR_MODE_SELECT(0);
		return 0;
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_3x_SOC)
	case TEGRA_DC_CURSOR_FORMAT_RGBA_NON_PREMULT_ALPHA:
# if !defined(CONFIG_TEGRA_NVDISPLAY)
		/* MODE_SELECT_NORMAL */
		*val |= CURSOR_MODE_SELECT(1);
#endif
# if !defined(CONFIG_ARCH_TEGRA_11x_SOC)
		/* K1_TIMES_SRC, NEG_K1_TIMES_SRC */
		*val |= CURSOR_DST_BLEND_FACTOR_SELECT(2);
		*val |= CURSOR_SRC_BLEND_FACTOR_SELECT(1);
# endif
		return 0;
#endif
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_3x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_11x_SOC)
	case TEGRA_DC_CURSOR_FORMAT_RGBA_PREMULT_ALPHA:
		*val |= CURSOR_MODE_SELECT(1);
		*val |= CURSOR_DST_BLEND_FACTOR_SELECT(2);
		*val |= CURSOR_SRC_BLEND_FACTOR_SELECT(0);
		return 0;
#endif
#if defined(CONFIG_TEGRA_NVDISPLAY)
	case TEGRA_DC_CURSOR_FORMAT_RGBA_XOR:
		/* MODE_SELECT_NORMAL */
		*val |= CURSOR_COMP_MODE(1);
		/* K1, NEG_K1_TIMES_SRC */
		*val |= CURSOR_DST_BLEND_FACTOR_SELECT(1);
		*val |= CURSOR_SRC_BLEND_FACTOR_SELECT(1);
		return 0;
#endif
	default:
		pr_err("%s: invalid blend format 0x%08x\n", __func__, blendfmt);
		break;
	}
	*val = 0;
	return -EINVAL;
}

static inline u32 cursor_alpha_value(struct tegra_dc *dc, u32 *val)
{
	u32 retval = 0;
	u32 data;

	if (!val | !dc)
		return -EINVAL;

	data = *val & (~TEGRA_DC_EXT_CURSOR_FORMAT_ALPHA_MSK);

	if (dc->cursor.alpha > TEGRA_DC_EXT_CURSOR_FORMAT_ALPHA_MAX)
		return -EINVAL;

# if defined(CONFIG_TEGRA_NVDISPLAY)
	data |= dc->cursor.alpha;
# else  /* CONFIG_TEGRA_NVDISPLAY */
	data |= CURSOR_ALPHA(TEGRA_DC_EXT_CURSOR_FORMAT_ALPHA_MAX);
# endif /* CONFIG_TEGRA_NVDISPLAY */

	*val = data;
	return retval;
}


static unsigned int set_cursor_start_addr(struct tegra_dc *dc,
	enum tegra_dc_cursor_size size, dma_addr_t phys_addr)
{
	u32 val = 0;
	int clip_win;

	BUG_ON(phys_addr & ~CURSOR_START_ADDR_MASK);

	dc->cursor.phys_addr = phys_addr;
	dc->cursor.size = size;
	/* this should not fail, as tegra_dc_cursor_image() checks the size */
	if (WARN(cursor_size_value(size, &val), "invalid cursor size."))
		return 0;

	clip_win = dc->cursor.clip_win;
	val |= CURSOR_CLIP_SHIFT_BITS(clip_win);
#if defined(CONFIG_ARCH_TEGRA_2x_SOC) || defined(CONFIG_ARCH_TEGRA_3x_SOC) || \
	defined(CONFIG_ARCH_TEGRA_11x_SOC) || defined(CONFIG_ARCH_TEGRA_14x_SOC)
	tegra_dc_writel(dc, val | CURSOR_START_ADDR(((unsigned long)phys_addr)),
			DC_DISP_CURSOR_START_ADDR);
#else
	/* TODO: check calculation with HW */
	tegra_dc_writel(dc, (u32)(CURSOR_START_ADDR_HI(phys_addr)),
			DC_DISP_CURSOR_START_ADDR_HI);
	tegra_dc_writel(dc, (u32)(val | CURSOR_START_ADDR_LOW(phys_addr)),
			DC_DISP_CURSOR_START_ADDR);
#endif

#if defined(CONFIG_TEGRA_NVDISPLAY)
	WARN_ON((phys_addr & 0x3FF) != 0);
#endif

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_3x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_11x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_14x_SOC)
	tegra_dc_writel(dc, CURSOR_UPDATE, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, CURSOR_ACT_REQ, DC_CMD_STATE_CONTROL);
	return 0;
#else
	return 1;
#endif
}

static int set_cursor_position(struct tegra_dc *dc, s16 x, s16 y)
{
#if defined(CONFIG_TEGRA_NVDISPLAY)
	nvdisp_set_cursor_position(dc, x, y);
#else
	tegra_dc_writel(dc, CURSOR_POSITION(x, y), DC_DISP_CURSOR_POSITION);
#endif

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_3x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_11x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_14x_SOC)
	tegra_dc_writel(dc, CURSOR_UPDATE, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, CURSOR_ACT_REQ, DC_CMD_STATE_CONTROL);
	return 0;
#else
	return 1;
#endif
}

static int set_cursor_activation_control(struct tegra_dc *dc)
{
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_3x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_11x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_14x_SOC) && \
	!defined(CONFIG_TEGRA_NVDISPLAY)
	u32 reg = tegra_dc_readl(dc, DC_CMD_REG_ACT_CONTROL);

	if ((reg & (1 << CURSOR_ACT_CNTR_SEL)) ==
	    (CURSOR_ACT_CNTR_SEL_V << CURSOR_ACT_CNTR_SEL)) {
		reg &= ~(1 << CURSOR_ACT_CNTR_SEL);
		reg |= (CURSOR_ACT_CNTR_SEL_V << CURSOR_ACT_CNTR_SEL);
		tegra_dc_writel(dc, reg, DC_CMD_REG_ACT_CONTROL);
		return 1;
	}
#endif
	return 0;
}

static int set_cursor_enable(struct tegra_dc *dc, bool enable)
{
	u32 val = tegra_dc_readl(dc, DC_DISP_DISP_WIN_OPTIONS);

	if (!!(val & CURSOR_ENABLE) != enable) {
		val &= ~CURSOR_ENABLE;
		if (enable)
			val |= CURSOR_ENABLE;
		tegra_dc_writel(dc, val, DC_DISP_DISP_WIN_OPTIONS);
		return 1;
	}
	dc->cursor.enabled = enable;
	return 0;
}

static int set_cursor_blend(struct tegra_dc *dc, u32 blendfmt)
{
	u32 val = tegra_dc_readl(dc, DC_DISP_BLEND_CURSOR_CONTROL);
	u32 newval = WINH_CURS_SELECT(0);
	int ret = 0;

	/* this should not fail, as tegra_dc_cursor_image() checks the format */
	if (WARN(cursor_blendfmt_value(blendfmt, &newval),
		"invalid cursor blend format"))
		return 0;
	if (WARN(cursor_alpha_value(dc, &newval),
		"invalid cursor alpha"))
		return 0;

	if (val != newval) {
		tegra_dc_writel(dc, newval, DC_DISP_BLEND_CURSOR_CONTROL);
		ret = 1;
	}
	dc->cursor.blendfmt = blendfmt;

#if defined(CONFIG_TEGRA_NVDISPLAY)
	nvdisp_set_cursor_colorfmt(dc); /* color fmt */
#endif
	return ret;
}

static int set_cursor_fg_bg(struct tegra_dc *dc, u32 fg, u32 bg)
{
	int general_update_needed = 0;

#if !defined(CONFIG_TEGRA_NVDISPLAY)
	/* TODO: check fg/bg against data structure, don't read the HW */
	if (fg != tegra_dc_readl(dc, DC_DISP_CURSOR_FOREGROUND)) {
		tegra_dc_writel(dc, fg, DC_DISP_CURSOR_FOREGROUND);
		general_update_needed |= 1;
	}

	if (bg != tegra_dc_readl(dc, DC_DISP_CURSOR_BACKGROUND)) {
		tegra_dc_writel(dc, bg, DC_DISP_CURSOR_BACKGROUND);
		general_update_needed |= 1;
	}
	dc->cursor.fg = fg;
	dc->cursor.bg = bg;
#endif

	return general_update_needed;
}

static void tegra_dc_cursor_do_update(struct tegra_dc *dc,
	bool need_general_update)
{
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_3x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_11x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_14x_SOC)
	tegra_dc_writel(dc, CURSOR_UPDATE, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, CURSOR_ACT_REQ, DC_CMD_STATE_CONTROL);
#else
	need_general_update = true;
#endif
	if (need_general_update) {
		tegra_dc_writel(dc, GENERAL_UPDATE, DC_CMD_STATE_CONTROL);
		tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);
	}
}

static int tegra_dc_cursor_program(struct tegra_dc *dc)
{
	bool need_general_update = false;

	if (!dc->enabled)
		return 0;
	/* these checks are redundant */
	if (cursor_size_value(dc->cursor.size, NULL))
		return -EINVAL;
	if (cursor_blendfmt_value(dc->cursor.blendfmt, NULL))
		return -EINVAL;

	mutex_lock(&dc->lock);
	if (!dc->cursor.dirty) {
		mutex_unlock(&dc->lock);
		return 0;
	}
	tegra_dc_get(dc);

	need_general_update |= set_cursor_start_addr(dc, dc->cursor.size,
		dc->cursor.phys_addr);

	need_general_update |= set_cursor_fg_bg(dc,
		dc->cursor.fg, dc->cursor.bg);

	need_general_update |= set_cursor_blend(dc, dc->cursor.blendfmt);

	need_general_update |= set_cursor_enable(dc, dc->cursor.enabled);

	need_general_update |= set_cursor_position(dc,
		dc->cursor.x, dc->cursor.y);

	need_general_update |= set_cursor_activation_control(dc);

	tegra_dc_cursor_do_update(dc, need_general_update);

	tegra_dc_put(dc);
	dc->cursor.dirty = false;
	mutex_unlock(&dc->lock);

	return 0;
}

int tegra_dc_cursor_image(struct tegra_dc *dc,
	enum tegra_dc_cursor_blend_format blendfmt,
	enum tegra_dc_cursor_size size,
	u32 fg, u32 bg, dma_addr_t phys_addr,
	enum tegra_dc_cursor_color_format colorfmt, u32 alpha, u32 flags)
{
	if (cursor_size_value(size, NULL))
		return -EINVAL;
	if (cursor_blendfmt_value(blendfmt, NULL))
		return -EINVAL;

	mutex_lock(&dc->lock);
	dc->cursor.fg = fg;
	dc->cursor.bg = bg;
	dc->cursor.size = size;
	dc->cursor.blendfmt = blendfmt;
	dc->cursor.colorfmt = colorfmt;
	dc->cursor.phys_addr = phys_addr;
	dc->cursor.dirty = true;

	if (TEGRA_DC_EXT_CURSOR_COLORFMT_FLAGS(flags) ==
		TEGRA_DC_EXT_CURSOR_COLORFMT_LEGACY) {
		alpha    = TEGRA_DC_EXT_CURSOR_FORMAT_ALPHA_MAX;
#if defined(CONFIG_TEGRA_NVDISPLAY)
		dc->cursor.colorfmt = TEGRA_DC_EXT_CURSOR_COLORFMT_A8R8G8B8;
#else  /*CONFIG_TEGRA_NVDISPLAY*/
		dc->cursor.colorfmt = TEGRA_DC_EXT_CURSOR_COLORFMT_R8G8B8A8;
#endif /*CONFIG_TEGRA_NVDISPLAY*/
	}
	dc->cursor.alpha = alpha;
	mutex_unlock(&dc->lock);

	return tegra_dc_cursor_program(dc);
}

int tegra_dc_cursor_set(struct tegra_dc *dc, bool enable, int x, int y)
{
	mutex_lock(&dc->lock);
	dc->cursor.x = x;
	dc->cursor.y = y;
	dc->cursor.enabled = enable;
	dc->cursor.dirty = true;
	mutex_unlock(&dc->lock);

	return tegra_dc_cursor_program(dc);
}

/* clip:
 * 0 - display
 * 1 - window A
 * 2 - window B
 * 3 - window C
 */
int tegra_dc_cursor_clip(struct tegra_dc *dc, unsigned clip)
{
	mutex_lock(&dc->lock);
	dc->cursor.clip_win = clip;
	dc->cursor.dirty = true;
	mutex_unlock(&dc->lock);

	return tegra_dc_cursor_program(dc);
}

/* disable the cursor on suspend. but leave the state unmodified */
int tegra_dc_cursor_suspend(struct tegra_dc *dc)
{
	if (!dc->enabled)
		return 0;
	mutex_lock(&dc->lock);
	tegra_dc_get(dc);
	set_cursor_enable(dc, false);
	tegra_dc_cursor_do_update(dc, true);
	dc->cursor.dirty = true;
	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);
	return 0;
}

/* restore the state */
int tegra_dc_cursor_resume(struct tegra_dc *dc)
{
	return tegra_dc_cursor_program(dc);
}
