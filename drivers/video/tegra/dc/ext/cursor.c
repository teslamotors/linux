/*
 * drivers/video/tegra/dc/ext/cursor.c
 *
 * Copyright (c) 2011-2016, NVIDIA CORPORATION, All rights reserved.
 *
 * Author: Robert Morell <rmorell@nvidia.com>
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

#include <video/tegra_dc_ext.h>
#include "tegra_dc_ext_priv.h"

/* ugh */
#include "../dc_priv.h"
#include "../dc_reg.h"

int tegra_dc_ext_get_cursor(struct tegra_dc_ext_user *user)
{
	struct tegra_dc_ext *ext = user->ext;
	int ret = 0;

	mutex_lock(&ext->cursor.lock);

	if (!ext->cursor.user)
		ext->cursor.user = user;
	else if (ext->cursor.user != user)
		ret = -EBUSY;

	mutex_unlock(&ext->cursor.lock);

	return ret;
}

int tegra_dc_ext_put_cursor(struct tegra_dc_ext_user *user)
{
	struct tegra_dc_ext *ext = user->ext;
	int ret = 0;

	mutex_lock(&ext->cursor.lock);

	if (ext->cursor.user == user)
		ext->cursor.user = NULL;
	else
		ret = -EACCES;

	mutex_unlock(&ext->cursor.lock);

	return ret;
}

int tegra_dc_ext_set_cursor_image(struct tegra_dc_ext_user *user,
				  struct tegra_dc_ext_cursor_image *args)
{
	struct tegra_dc_ext *ext = user->ext;
	struct tegra_dc *dc = ext->dc;
	struct tegra_dc_dmabuf *handle, *old_handle;
	dma_addr_t phys_addr;
	int ret;
	u32 extformat = TEGRA_DC_EXT_CURSOR_FORMAT_FLAGS(args->flags);
	u32 clrformat = TEGRA_DC_EXT_CURSOR_COLORFMT_FLAGS(args->flags);
	u32 fg = CURSOR_COLOR(args->foreground.r,
			      args->foreground.g,
			      args->foreground.b);
	u32 bg = CURSOR_COLOR(args->background.r,
			      args->background.g,
			      args->background.b);
	unsigned extsize = TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE(args->flags);
	enum tegra_dc_cursor_size size;
	enum tegra_dc_cursor_blend_format blendfmt;
	enum CURSOR_COLOR_FORMAT colorfmt;

	switch (extsize) {
	case TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE_32x32:
		size = TEGRA_DC_CURSOR_SIZE_32X32;
		break;
	case TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE_64x64:
		size = TEGRA_DC_CURSOR_SIZE_64X64;
		break;
	case TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE_128x128:
		size = TEGRA_DC_CURSOR_SIZE_128X128;
		break;
	case TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE_256x256:
		size = TEGRA_DC_CURSOR_SIZE_256X256;
		break;
	default:
		return -EINVAL;
	}

	switch (extformat) {
	case TEGRA_DC_EXT_CURSOR_FORMAT_2BIT_LEGACY:
		blendfmt = TEGRA_DC_CURSOR_FORMAT_2BIT_LEGACY;
		break;
	case TEGRA_DC_EXT_CURSOR_FORMAT_RGBA_NON_PREMULT_ALPHA:
		blendfmt = TEGRA_DC_CURSOR_FORMAT_RGBA_NON_PREMULT_ALPHA;
		break;
	case TEGRA_DC_EXT_CURSOR_FORMAT_RGBA_PREMULT_ALPHA:
		blendfmt = TEGRA_DC_CURSOR_FORMAT_RGBA_PREMULT_ALPHA;
		break;
	case TEGRA_DC_EXT_CURSOR_FORMAT_RGBA_XOR:
		blendfmt = TEGRA_DC_CURSOR_FORMAT_RGBA_XOR;
		break;
	default:
		return -EINVAL;
	}
	switch (clrformat) {
	case TEGRA_DC_CURSOR_COLORFMT_LEGACY:
		colorfmt = legacy;
		break;
	case TEGRA_DC_CURSOR_COLORFMT_R8G8B8A8: /* normal */
		colorfmt = r8g8b8a8;
		break;
	case  TEGRA_DC_CURSOR_COLORFMT_A1R5G5B5:
		colorfmt = a1r5g5b5;
		break;
	case TEGRA_DC_CURSOR_COLORFMT_A8R8G8B8:
		colorfmt = a8r8g8b8;
		break;
	default:
		return -EINVAL;
	}

	mutex_lock(&ext->cursor.lock);

	if (ext->cursor.user != user) {
		ret = -EACCES;
		goto unlock;
	}

	if (!ext->enabled) {
		ret = -ENXIO;
		goto unlock;
	}

	old_handle = ext->cursor.cur_handle;

	ret = tegra_dc_ext_pin_window(user, args->buff_id, &handle, &phys_addr);
	if (ret)
		goto unlock;

	ext->cursor.cur_handle = handle;

	ret = tegra_dc_cursor_image(dc, blendfmt, size, fg, bg, phys_addr,
				colorfmt, args->alpha, args->flags);

	mutex_unlock(&ext->cursor.lock);

	if (old_handle) {
		dma_buf_unmap_attachment(old_handle->attach,
			old_handle->sgt, DMA_TO_DEVICE);
		dma_buf_detach(old_handle->buf, old_handle->attach);
		dma_buf_put(old_handle->buf);
		kfree(old_handle);
	}

	return ret;

unlock:
	mutex_unlock(&ext->cursor.lock);

	return ret;
}

int tegra_dc_ext_set_cursor(struct tegra_dc_ext_user *user,
			    struct tegra_dc_ext_cursor *args)
{
	struct tegra_dc_ext *ext = user->ext;
	struct tegra_dc *dc = ext->dc;
	bool enable;
	int ret;

	mutex_lock(&ext->cursor.lock);

	if (ext->cursor.user != user) {
		ret = -EACCES;
		goto unlock;
	}

	if (!ext->enabled) {
		ret = -ENXIO;
		goto unlock;
	}

	enable = !!(args->flags & TEGRA_DC_EXT_CURSOR_FLAGS_VISIBLE);

	ret = tegra_dc_cursor_set(dc, enable, args->x, args->y);

	mutex_unlock(&ext->cursor.lock);

	return ret;

unlock:
	mutex_unlock(&ext->cursor.lock);

	return ret;
}

int tegra_dc_ext_cursor_clip(struct tegra_dc_ext_user *user,
			    int *args)
{
	struct tegra_dc_ext *ext = user->ext;
	struct tegra_dc *dc = ext->dc;
	int ret;

	mutex_lock(&ext->cursor.lock);

	if (ext->cursor.user != user) {
		ret = -EACCES;
		goto unlock;
	}

	if (!ext->enabled) {
		ret = -ENXIO;
		goto unlock;
	}

	ret = tegra_dc_cursor_clip(dc, *args);

	mutex_unlock(&ext->cursor.lock);

	return ret;

unlock:
	mutex_unlock(&ext->cursor.lock);

	return ret;
}
