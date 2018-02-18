/*
 * Copyright (C) 2013 Google, Inc.
 * Copyright (c) 2014, NVIDIA CORPORATION, All rights reserved.
 *
 * modified from drivers/video/tegra/dc/{mode.c,ext/dev.c}
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

#include <linux/memblock.h>
#include <linux/gfp.h>
#include <media/videobuf2-dma-contig.h>
#include <video/adf.h>
#include <video/adf_client.h>
#include <video/adf_fbdev.h>
#include <video/adf_format.h>
#include <video/adf_memblock.h>

#include "dc/dc_config.h"
#include "dc/dc_priv.h"
#include "tegra_adf.h"

struct tegra_adf_info {
	struct adf_device		base;
	struct adf_interface		intf;
	struct adf_overlay_engine	eng;
#if IS_ENABLED(CONFIG_ADF_TEGRA_FBDEV)
	struct adf_fbdev		fbdev;
#endif
	struct tegra_dc			*dc;
	struct tegra_fb_data		*fb_data;
	void				*vb2_dma_conf;
};

struct tegra_adf_flip_data {
	u32 syncpt_max[DC_N_WINDOWS];
	__u16 dirty_rect[4];
	bool dirty_rect_valid;
};

#define adf_dev_to_tegra(p) \
	container_of(p, struct tegra_adf_info, base)

#define adf_intf_to_tegra(p) \
	container_of(p, struct tegra_adf_info, intf)

const u8 tegra_adf_fourcc_to_dc_fmt(u32 fourcc)
{
	switch (fourcc) {
	case TEGRA_ADF_FORMAT_P1:
		return TEGRA_WIN_FMT_P1;
	case TEGRA_ADF_FORMAT_P2:
		return TEGRA_WIN_FMT_P2;
	case TEGRA_ADF_FORMAT_P4:
		return TEGRA_WIN_FMT_P4;
	case TEGRA_ADF_FORMAT_P8:
		return TEGRA_WIN_FMT_P8;
	case DRM_FORMAT_BGRA4444:
		return TEGRA_WIN_FMT_B4G4R4A4;
	case DRM_FORMAT_BGRA5551:
		return TEGRA_WIN_FMT_B5G5R5A;
	case DRM_FORMAT_BGR565:
		return TEGRA_WIN_FMT_B5G6R5;
	case DRM_FORMAT_BGRA8888:
	case DRM_FORMAT_BGRX8888:
		return TEGRA_WIN_FMT_B8G8R8A8;
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_RGBX8888:
		return TEGRA_WIN_FMT_R8G8B8A8;
	case TEGRA_ADF_FORMAT_B6x2G6x2R6x2A8:
		return TEGRA_WIN_FMT_B6x2G6x2R6x2A8;
	case TEGRA_ADF_FORMAT_R6x2G6x2B6x2A8:
		return TEGRA_WIN_FMT_R6x2G6x2B6x2A8;
	case DRM_FORMAT_YUV420:
		return TEGRA_WIN_FMT_YCbCr420P;
	case DRM_FORMAT_YUV422:
		return TEGRA_WIN_FMT_YCbCr422P;
	case TEGRA_ADF_FORMAT_YCbCr422R:
		return TEGRA_WIN_FMT_YCbCr422R;
	case DRM_FORMAT_UYVY:
		return TEGRA_WIN_FMT_YCbCr422;
	case DRM_FORMAT_NV12:
		return TEGRA_WIN_FMT_YCbCr420SP;
	case DRM_FORMAT_NV21:
		return TEGRA_WIN_FMT_YCrCb420SP;
	case DRM_FORMAT_NV16:
		return TEGRA_WIN_FMT_YCbCr422SP;
	default:
		BUG();
	}
}

static int tegra_dc_to_drm_modeinfo(struct drm_mode_modeinfo *dmode,
	const struct tegra_dc_mode *mode)
{
	long mode_pclk;

	if (!dmode || !mode || !mode->pclk)
		return -EINVAL;
	if (mode->rated_pclk >= 1000) /* handle DSI one-shot modes */
		mode_pclk = mode->rated_pclk;
	else if (mode->pclk >= 1000) /* normal continous modes */
		mode_pclk = mode->pclk;
	else
		mode_pclk = 0;
	memset(dmode, 0, sizeof(*dmode));
	dmode->hdisplay = mode->h_active;
	dmode->vdisplay = mode->v_active;
	dmode->hsync_start = dmode->hdisplay + mode->h_front_porch;
	dmode->vsync_start = dmode->vdisplay + mode->v_front_porch;
	dmode->hsync_end = dmode->hsync_start + mode->h_sync_width;
	dmode->vsync_end = dmode->vsync_start + mode->v_sync_width;
	dmode->htotal = dmode->hsync_end + mode->h_back_porch;
	dmode->vtotal = dmode->vsync_end + mode->v_back_porch;
#if 0
	if (mode->stereo_mode) {
#ifndef CONFIG_TEGRA_HDMI_74MHZ_LIMIT
		/* Double the pixel clock and update v_active only for
		 * frame packed mode */
		mode_pclk /= 2;
		/* total v_active = yres*2 + activespace */
		fbmode->vdisplay = (mode->v_active - mode->v_sync_width -
			mode->v_back_porch - mode->v_front_porch) / 2;
		fbmode->vmode |= FB_VMODE_STEREO_FRAME_PACK;
#else
		fbmode->vmode |= FB_VMODE_STEREO_LEFT_RIGHT;
#endif
	}
#endif

	if (mode->flags & FB_VMODE_INTERLACED)
		dmode->flags |= DRM_MODE_FLAG_INTERLACE;

	if ((mode->flags & TEGRA_DC_MODE_FLAG_NEG_H_SYNC))
		dmode->flags |= DRM_MODE_FLAG_NHSYNC;
	else
		dmode->flags |= DRM_MODE_FLAG_PHSYNC;
	if ((mode->flags & TEGRA_DC_MODE_FLAG_NEG_V_SYNC))
		dmode->flags |= DRM_MODE_FLAG_NVSYNC;
	else
		dmode->flags |= DRM_MODE_FLAG_PVSYNC;
#if 0
	if (mode->avi_m == TEGRA_DC_MODE_AVI_M_16_9)
		fbmode->flag |= FB_FLAG_RATIO_16_9;
	else if (mode->avi_m == TEGRA_DC_MODE_AVI_M_4_3)
		fbmode->flag |= FB_FLAG_RATIO_4_3;
#endif

	dmode->clock = mode_pclk / 1000;
	dmode->vrefresh = tegra_dc_calc_refresh(mode) / 1000;

	adf_modeinfo_set_name(dmode);

	return 0;
}

static int tegra_adf_convert_monspecs(struct tegra_adf_info *adf_info,
		struct fb_monspecs *specs, struct drm_mode_modeinfo **modelist,
		size_t *n_modes)
{
	struct tegra_dc *dc = adf_info->dc;
	struct drm_mode_modeinfo *modes;
	size_t n = 0;
	u32 i;

	modes = kmalloc(specs->modedb_len * sizeof(modes[0]), GFP_KERNEL);
	if (!modes)
		return -ENOMEM;

	for (i = 0; i < specs->modedb_len; i++) {
		struct fb_videomode *fb_mode = &specs->modedb[i];
		if (dc->out_ops->mode_filter &&
				!dc->out_ops->mode_filter(dc, fb_mode))
			continue;
		adf_modeinfo_from_fb_videomode(fb_mode, &modes[n]);
		n++;
	}

	*modelist = modes;
	*n_modes = n;
	return 0;
}

static int tegra_adf_convert_builtin_modes(struct tegra_adf_info *adf_info,
		struct drm_mode_modeinfo **modelist, size_t *n_modes)
{
	struct tegra_dc *dc = adf_info->dc;
	struct drm_mode_modeinfo *modes;
	u32 i;

	modes = kmalloc(dc->out->n_modes * sizeof(modes[0]), GFP_KERNEL);
	if (!modes)
		return -ENOMEM;

	for (i = 0; i < dc->out->n_modes; i++) {
		int err = tegra_dc_to_drm_modeinfo(&modes[i],
				&dc->out->modes[i]);
		if (err < 0)
			return err;
	}

	*modelist = modes;
	*n_modes = dc->out->n_modes;
	return 0;
}

static int tegra_adf_do_hotplug(struct tegra_adf_info *adf_info,
		struct drm_mode_modeinfo *modelist, size_t n_modes)
{
	int err = tegra_dc_set_drm_mode(adf_info->dc, modelist, false);
	if (err < 0)
		return err;
	memcpy(&adf_info->intf.current_mode, &modelist[0], sizeof(modelist[0]));

	return adf_hotplug_notify_connected(&adf_info->intf, modelist, n_modes);
}

int tegra_adf_process_hotplug_connected(struct tegra_adf_info *adf_info,
		struct fb_monspecs *specs)
{
	struct tegra_dc_out *out = adf_info->dc->out;
	struct drm_mode_modeinfo *modes;
	size_t n_modes;
	int err;

	if (!specs && !out->modes) {
		struct drm_mode_modeinfo reset_mode = {0};
		return tegra_adf_do_hotplug(adf_info, &reset_mode, 1);
	}

	if (specs)
		err = tegra_adf_convert_monspecs(adf_info, specs, &modes,
				&n_modes);
	else
		err = tegra_adf_convert_builtin_modes(adf_info, &modes,
				&n_modes);

	if (err < 0)
		return err;

	err = tegra_adf_do_hotplug(adf_info, modes, n_modes);
	kfree(modes);
	return err;
}

void tegra_adf_process_hotplug_disconnected(struct tegra_adf_info *adf_info)
{
	adf_interface_blank(&adf_info->intf, DRM_MODE_DPMS_OFF);
	adf_hotplug_notify_disconnected(&adf_info->intf);
}

static int tegra_adf_dev_custom_data(struct adf_obj *obj, void *data,
		size_t *size)
{
	struct tegra_adf_capabilities *caps = data;

	caps->caps = TEGRA_ADF_CAPABILITIES_CURSOR_MODE |
			TEGRA_ADF_CAPABILITIES_BLOCKLINEAR;
	*size = sizeof(*caps);
	return 0;
}

static int tegra_adf_dev_validate_custom_format(struct adf_device *dev,
		struct adf_buffer *buf)
{
	u8 dc_fmt = tegra_adf_fourcc_to_dc_fmt(buf->format);

	if (tegra_dc_is_yuv(dc_fmt)) {
		u8 cpp[3] = { 1, 1, 1 };
		return adf_format_validate_yuv(dev, buf, ARRAY_SIZE(cpp), 1, 2,
				cpp);
	} else {
		u8 cpp = tegra_dc_fmt_bpp(dc_fmt) / 8;
		return adf_format_validate_rgb(dev, buf, cpp);
	}

	return 0;
}

const u32 tegra_adf_formats[] = {
	TEGRA_ADF_FORMAT_P1,
	TEGRA_ADF_FORMAT_P2,
	TEGRA_ADF_FORMAT_P4,
	TEGRA_ADF_FORMAT_P8,
	DRM_FORMAT_BGRA4444,
	DRM_FORMAT_BGRA5551,
	DRM_FORMAT_BGR565,
	DRM_FORMAT_ABGR1555,
	DRM_FORMAT_BGRA8888,
	DRM_FORMAT_BGRX8888,
	DRM_FORMAT_RGBA8888,
	DRM_FORMAT_RGBX8888,
	TEGRA_ADF_FORMAT_B6x2G6x2R6x2A8,
	TEGRA_ADF_FORMAT_R6x2G6x2B6x2A8,
	TEGRA_ADF_FORMAT_R6x2G6x2B6x2A8,
	DRM_FORMAT_YUV420,
	DRM_FORMAT_YUV422,
	TEGRA_ADF_FORMAT_YCbCr422R,
	DRM_FORMAT_UYVY,
	DRM_FORMAT_NV12,
	DRM_FORMAT_NV21,
	DRM_FORMAT_NV16,
};

static inline int test_bit_u32(int nr, const u32 *addr)
{
	return 1UL & (addr[nr / 32] >> (nr & 31));
}

static int tegra_adf_check_windowattr(struct tegra_adf_info *adf_info,
		const struct tegra_adf_flip_windowattr *attr, u32 fourcc)
{
	u32 *addr;
	struct tegra_dc *dc = adf_info->dc;
	struct device *dev = &adf_info->base.base.dev;
	u8 fmt = tegra_adf_fourcc_to_dc_fmt(fourcc);

	addr = tegra_dc_parse_feature(dc, attr->win_index, GET_WIN_FORMATS);
	/* Check if the window exists */
	if (!addr) {
		dev_err(dev, "window %d feature is not found.\n",
				attr->win_index);
		goto fail;
	}
	/* Check the window format */
	if (!test_bit_u32(fmt, addr)) {
		dev_err(dev,
			"Color format of window %d is invalid.\n",
			attr->win_index);
		goto fail;
	}

	/* Check window size */
	addr = tegra_dc_parse_feature(dc, attr->win_index, GET_WIN_SIZE);
	if (CHECK_SIZE(attr->out_w, addr[MIN_WIDTH], addr[MAX_WIDTH]) ||
		CHECK_SIZE(attr->out_h, addr[MIN_HEIGHT], addr[MAX_HEIGHT])) {
		dev_err(dev,
			"Size of window %d is invalid with %d wide %d high.\n",
			attr->win_index, attr->out_w, attr->out_h);
		goto fail;
	}

	if (attr->flags & TEGRA_ADF_FLIP_FLAG_BLOCKLINEAR) {
		if (attr->flags & TEGRA_ADF_FLIP_FLAG_TILED) {
			dev_err(&dc->ndev->dev, "Layout cannot be both blocklinear and tile for window %d.\n",
				attr->win_index);
			goto fail;
		}

		/* TODO: also check current window blocklinear support */
	}

	if ((attr->flags & TEGRA_ADF_FLIP_FLAG_SCAN_COLUMN) &&
			!tegra_dc_feature_has_scan_column(dc,
					attr->win_index)) {
		dev_err(&dc->ndev->dev, "rotation not supported for window %d.\n",
				attr->win_index);
		goto fail;
	}

	return 0;
fail:
	return -EINVAL;
}

static int tegra_adf_sanitize_flip_args(struct tegra_adf_info *adf_info,
		struct adf_post *cfg,
		struct tegra_adf_flip_windowattr *win, int win_num,
		__u16 *dirty_rect[4])
{
	struct device *dev = &adf_info->base.base.dev;
	struct tegra_dc *dc = adf_info->dc;
	int i, used_windows = 0;

	if (win_num > DC_N_WINDOWS) {
		dev_err(dev, "too many windows (%u > %u)\n", win_num,
				DC_N_WINDOWS);
		return -EINVAL;
	}

	for (i = 0; i < win_num; i++) {
		int index = win[i].win_index;
		int buf_index = win[i].buf_index;
		int err;

		if (index < 0)
			continue;

		if (index >= DC_N_WINDOWS ||
				!test_bit(index, &dc->valid_windows)) {
			dev_err(dev, "invalid window index %u\n", index);
			return -EINVAL;
		}

		if (used_windows & BIT(index)) {
			dev_err(dev, "window index %u already used\n", index);
			return -EINVAL;
		}

		if (buf_index >= 0) {
			if (buf_index >= cfg->n_bufs) {
				dev_err(dev, "invalid buffer index %d (n_bufs = %zu)\n",
						buf_index, cfg->n_bufs);
				return -EINVAL;
			}

			err = tegra_adf_check_windowattr(adf_info, &win[i],
					cfg->bufs[buf_index].format);
			if (err < 0)
				return err;
		}

		used_windows |= BIT(index);
	}

	if (!used_windows) {
		dev_err(dev, "no windows used\n");
		return -EINVAL;
	}

	if (*dirty_rect) {
		unsigned int xoff = (*dirty_rect)[0];
		unsigned int yoff = (*dirty_rect)[1];
		unsigned int width = (*dirty_rect)[2];
		unsigned int height = (*dirty_rect)[3];
		struct tegra_dc *dc = adf_info->dc;

		if ((!width && !height) || dc->mode.vmode == FB_VMODE_INTERLACED
				|| !dc->out_ops || !dc->out_ops->partial_update
				|| (!xoff && !yoff
						&& (width == dc->mode.h_active)
						&& (height == dc->mode.v_active))) {
			/* Partial update undesired, unsupported,
			 * or dirty_rect covers entire frame. */
			*dirty_rect = 0;
		} else {
			if (!width || !height
					|| (xoff + width) > dc->mode.h_active
					|| (yoff + height) > dc->mode.v_active)
				return -EINVAL;

			/* Constraint 7: H/V_DISP_ACTIVE >= 16.
			 * Make sure the minimal size of dirty region is 16*16.
			 * If not, extend the dirty region. */
			if (width < 16) {
				width = (*dirty_rect)[2] = 16;
				if (xoff + width > dc->mode.h_active)
					(*dirty_rect)[0] = dc->mode.h_active
							- width;
			}
			if (height < 16) {
				height = (*dirty_rect)[3] = 16;
				if (yoff + height > dc->mode.v_active)
					(*dirty_rect)[1] = dc->mode.v_active
							- height;
			}
		}
	}

	return 0;
}

int tegra_adf_dev_validate(struct adf_device *dev, struct adf_post *cfg,
		void **driver_state)
{
	struct tegra_adf_flip *args = cfg->custom_data;
	struct tegra_adf_info *adf_info = adf_dev_to_tegra(dev);
	struct tegra_adf_flip_windowattr *win;
	struct tegra_adf_flip_data *data;
	unsigned int win_num;
	size_t custom_data_size = sizeof(*args);
	__u16 *dirty_rect;
	int ret = 0;

	if (cfg->custom_data_size < custom_data_size) {
		dev_err(dev->dev, "custom data size too small (%zu < %zu)\n",
				cfg->custom_data_size, custom_data_size);
		return -EINVAL;
	}

	win = args->win;
	win_num = args->win_num;

	custom_data_size += win_num * sizeof(win[0]);
	if (cfg->custom_data_size != custom_data_size) {
		dev_err(dev->dev, "expected %zu bytes of custom data for %u windows, received %zu\n",
				custom_data_size, args->win_num,
				cfg->custom_data_size);
		return -EINVAL;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		dev_err(dev->dev, "failed to allocate driver state\n");
		return -ENOMEM;
	}

	dirty_rect = args->dirty_rect;
	ret = tegra_adf_sanitize_flip_args(adf_info, cfg, win, win_num,
			&dirty_rect);
	if (ret < 0)
		goto done;

	if (dirty_rect) {
		memcpy(data->dirty_rect, dirty_rect, sizeof(data->dirty_rect));
		data->dirty_rect_valid = true;
	}

	BUG_ON(win_num > DC_N_WINDOWS);

done:
	if (ret < 0)
		kfree(data);
	else
		*driver_state = data;
	return ret;
}

static inline dma_addr_t tegra_adf_phys_addr(struct adf_buffer *buf,
		struct adf_buffer_mapping *mapping,
		size_t plane)
{
	struct scatterlist *sgl = buf->dma_bufs[plane] ?
			mapping->sg_tables[plane]->sgl :
			mapping->sg_tables[TEGRA_DC_Y]->sgl;

	dma_addr_t addr = sg_dma_address(sgl);
	if (!addr)
		addr = sg_phys(sgl);
	addr += buf->offset[plane];
	return addr;
}

static void tegra_adf_set_windowattr_basic(struct tegra_dc_win *win,
		const struct tegra_adf_flip_windowattr *attr,
		u32 format)
{
	win->flags = TEGRA_WIN_FLAG_ENABLED;
	if (attr->blend == TEGRA_ADF_BLEND_PREMULT)
		win->flags |= TEGRA_WIN_FLAG_BLEND_PREMULT;
	else if (attr->blend == TEGRA_ADF_BLEND_COVERAGE)
		win->flags |= TEGRA_WIN_FLAG_BLEND_COVERAGE;
	if (attr->flags & TEGRA_ADF_FLIP_FLAG_TILED)
		win->flags |= TEGRA_WIN_FLAG_TILED;
	if (attr->flags & TEGRA_ADF_FLIP_FLAG_INVERT_H)
		win->flags |= TEGRA_WIN_FLAG_INVERT_H;
	if (attr->flags & TEGRA_ADF_FLIP_FLAG_INVERT_V)
		win->flags |= TEGRA_WIN_FLAG_INVERT_V;
	if (attr->flags & TEGRA_ADF_FLIP_FLAG_GLOBAL_ALPHA)
		win->global_alpha = attr->global_alpha;
	else
		win->global_alpha = 255;
#if defined(CONFIG_TEGRA_DC_SCAN_COLUMN)
	if (attr->flags & TEGRA_ADF_FLIP_FLAG_SCAN_COLUMN)
		win->flags |= TEGRA_WIN_FLAG_SCAN_COLUMN;
#endif
#if defined(CONFIG_TEGRA_DC_BLOCK_LINEAR)
	if (attr->flags & TEGRA_ADF_FLIP_FLAG_BLOCKLINEAR) {
		win->flags |= TEGRA_WIN_FLAG_BLOCKLINEAR;
		win->block_height_log2 = attr->block_height_log2;
	}
#endif
#if defined(CONFIG_TEGRA_DC_CDE)
	if (attr->flags & TEGRA_ADF_FLIP_FLAG_COMPRESSED) {
		win->cde.offset_x = attr->cde.offset_x;
		win->cde.offset_y = attr->cde.offset_y;
		win->cde.zbc_color = attr->cde.zbc_color;
		win->cde.ctb_entry = 0x02;
	}
#endif
#if defined(CONFIG_TEGRA_DC_INTERLACE)
	if (attr->flags & TEGRA_ADF_FLIP_FLAG_INTERLACE)
		win->flags |= TEGRA_WIN_FLAG_INTERLACE;
#endif

	win->fmt = tegra_adf_fourcc_to_dc_fmt(format);
	win->x.full = attr->x;
	win->y.full = attr->y;
	win->w.full = attr->w;
	win->h.full = attr->h;
	/* XXX verify that this doesn't go outside display's active region */
	win->out_x = attr->out_x;
	win->out_y = attr->out_y;
	win->out_w = attr->out_w;
	win->out_h = attr->out_h;
	win->z = attr->z;
}

static void tegra_adf_set_windowattr(struct tegra_adf_info *adf_info,
		struct tegra_dc_win *win,
		const struct tegra_adf_flip_windowattr *attr,
		struct adf_buffer *buf, struct adf_buffer_mapping *mapping)
{
	if (!buf) {
		win->flags = 0;
		return;
	}

	tegra_adf_set_windowattr_basic(win, attr, buf->format);

	win->stride = buf->pitch[0];
	win->stride_uv = buf->pitch[1];

	/* XXX verify that this won't read outside of the surface */
	win->phys_addr = tegra_adf_phys_addr(buf, mapping, TEGRA_DC_Y);
	win->phys_addr_u = tegra_adf_phys_addr(buf, mapping, TEGRA_DC_U);
	win->phys_addr_v = tegra_adf_phys_addr(buf, mapping, TEGRA_DC_V);

#if defined(CONFIG_TEGRA_DC_INTERLACE)
	if (adf_info->dc->mode.vmode == FB_VMODE_INTERLACED) {
		if (attr->flags & TEGRA_ADF_FLIP_FLAG_INTERLACE) {
			win->phys_addr2 = win->phys_addr + attr->offset2;
			win->phys_addr_u2 = win->phys_addr_u + attr->offset_u2;
			win->phys_addr_v2 = win->phys_addr_v + attr->offset_v2;
		} else {
			win->phys_addr2 = win->phys_addr;
			win->phys_addr_u2 = win->phys_addr_u;
			win->phys_addr_v2 = win->phys_addr_v;
		}
	}
#endif

#if defined(CONFIG_TEGRA_DC_CDE)
	if (attr->flags & TEGRA_DC_EXT_FLIP_FLAG_COMPRESSED) {
		win->cde.cde_addr = win->phys_addr + attr->cde.offset;
	} else {
		win->cde.cde_addr = 0;
	}
#endif

	if (attr->flags & TEGRA_ADF_FLIP_FLAG_UPDATE_CSC) {
		win->csc.yof = attr->csc.yof;
		win->csc.kyrgb = attr->csc.kyrgb;
		win->csc.kur = attr->csc.kur;
		win->csc.kug = attr->csc.kug;
		win->csc.kub = attr->csc.kub;
		win->csc.kvr = attr->csc.kvr;
		win->csc.kvg = attr->csc.kvg;
		win->csc.kvb = attr->csc.kvb;
		win->csc_dirty = true;
	}

	if (tegra_platform_is_silicon()) {
		dev_WARN_ONCE(&adf_info->base.base.dev, attr->timestamp_ns,
				"timestamping not implemented\n");
		/* TODO: implement timestamping */
#if 0
		if (timestamp_ns) {
			/* XXX: Should timestamping be overridden by "no_vsync"
			 * flag */
			tegra_dc_config_frame_end_intr(win->dc, true);
			err = wait_event_interruptible(win->dc->timestamp_wq,
				tegra_dc_is_within_n_vsync(win->dc,
						timestamp_ns));
			tegra_dc_config_frame_end_intr(win->dc, false);
		}
#endif
	}
}

static void tegra_adf_dev_post(struct adf_device *dev, struct adf_post *cfg,
		void *driver_state)
{
	struct tegra_adf_info *adf_info = adf_dev_to_tegra(dev);
	struct tegra_adf_flip *args = cfg->custom_data;
	struct tegra_adf_flip_data *data = driver_state;
	int win_num = args->win_num;
	struct tegra_dc_win *wins[DC_N_WINDOWS];
	int i, nr_win = 0;
	bool skip_flip = false;

	BUG_ON(win_num > DC_N_WINDOWS);
	for (i = 0; i < win_num; i++) {
		struct tegra_adf_flip_windowattr *attr = &args->win[i];
		int index = attr->win_index;
		struct adf_buffer *buf;
		struct adf_buffer_mapping *mapping;
		struct tegra_dc_win *win;

		if (index < 0)
			continue;

		if (attr->buf_index < 0) {
			buf = NULL;
			mapping = NULL;
		} else {
			buf = &cfg->bufs[attr->buf_index];
			mapping = &cfg->mappings[attr->buf_index];
		}

		win = tegra_dc_get_window(adf_info->dc, index);

#if 0
		if (flip_win->flags & TEGRA_ADF_FLIP_FLAG_CURSOR)
			skip_flip = true;

		mutex_lock(&ext_win->queue_lock);
		list_for_each_entry(temp, &ext_win->timestamp_queue,
				timestamp_node) {
			if (!tegra_platform_is_silicon())
				continue;
			if (j == 0) {
				if (unlikely(temp != data))
					dev_err(&win->dc->ndev->dev,
							"work queue did NOT dequeue head!!!");
				else
					head_timestamp =
						timespec_to_ns(&flip_win->attr.timestamp);
			} else {
				s64 timestamp =
					timespec_to_ns(&temp->win[i].attr.timestamp);

				skip_flip = !tegra_dc_does_vsync_separate(ext->dc,
						timestamp, head_timestamp);
				/* Look ahead only one flip */
				break;
			}
			j++;
		}
		if (!list_empty(&ext_win->timestamp_queue))
			list_del(&data->timestamp_node);
		mutex_unlock(&ext_win->queue_lock);

		if (skip_flip)
			old_handle = flip_win->handle[TEGRA_DC_Y];
		else
			old_handle = ext_win->cur_handle[TEGRA_DC_Y];

		if (old_handle) {
			int j;
			for (j = 0; j < TEGRA_DC_NUM_PLANES; j++) {
				if (skip_flip)
					old_handle = flip_win->handle[j];
				else
					old_handle = ext_win->cur_handle[j];

				if (!old_handle)
					continue;

				unpin_handles[nr_unpin++] = old_handle;
			}
		}

		if (!skip_flip)
#endif
			tegra_adf_set_windowattr(adf_info, win, attr, buf,
					mapping);

		wins[nr_win++] = win;
	}

	if (!skip_flip) {
		tegra_dc_update_windows(wins, nr_win,
			data->dirty_rect_valid ? data->dirty_rect : NULL, true);
		/* TODO: implement swapinterval here */
		tegra_dc_sync_windows(wins, nr_win);
		tegra_dc_program_bandwidth(adf_info->dc, true);
		if (!tegra_dc_has_multiple_dc())
			tegra_dc_call_flip_callback();
	}
}

static struct sync_fence *tegra_adf_dev_complete_fence(struct adf_device *dev,
		struct adf_post *cfg, void *driver_state)
{
	struct tegra_adf_info *adf_info = adf_dev_to_tegra(dev);
	struct tegra_adf_flip *args = cfg->custom_data;
	struct tegra_adf_flip_windowattr *win = args->win;
	struct tegra_adf_flip_data *data = driver_state;
	u32 syncpt_val;
	int work_index = -1;
	unsigned int win_num = args->win_num, i;

	for (i = 0; i < win_num; i++) {
		int index = win[i].win_index;

		if (index < 0)
			continue;

		data->syncpt_max[i] = tegra_dc_incr_syncpt_max(adf_info->dc,
				index);

		/*
		 * Any of these windows' syncpoints should be equivalent for
		 * the client, so we just send back an arbitrary one of them
		 */
		syncpt_val = data->syncpt_max[i];
		work_index = index;
	}
	if (work_index < 0)
		return ERR_PTR(-EINVAL);

	return tegra_dc_create_fence(adf_info->dc, work_index, syncpt_val + 1);
}

static void tegra_adf_dev_advance_timeline(struct adf_device *dev,
		struct adf_post *cfg, void *driver_state)
{
	struct tegra_adf_info *adf_info = adf_dev_to_tegra(dev);
	struct tegra_adf_flip *args = cfg->custom_data;
	struct tegra_adf_flip_windowattr *win = args->win;
	u32 *syncpt_max = driver_state;
	unsigned int win_num = args->win_num, i;

	for (i = 0; i < win_num; i++) {
		int index = win[i].win_index;

		if (index < 0)
			continue;

		tegra_dc_incr_syncpt_min(adf_info->dc, index, syncpt_max[i]);
	}
}

static void tegra_adf_dev_state_free(struct adf_device *dev, void *driver_state)
{
	kfree(driver_state);
}

static int tegra_adf_negotiate_bw(struct tegra_adf_info *adf_info,
			struct tegra_adf_proposed_bw *bw)
{
#ifdef CONFIG_TEGRA_ISOMGR
	struct tegra_dc_win *dc_wins[DC_N_WINDOWS];
	struct tegra_dc *dc = adf_info->dc;
	struct adf_overlay_engine *eng = &adf_info->eng;
	u8 i;

	/* If display has been disconnected return with error. */
	if (!dc->connected)
		return -1;

	for (i = 0; i < bw->win_num; i++) {
		struct tegra_adf_flip_windowattr *attr = &bw->win[i].attr;
		s32 idx = attr->win_index;

		if (attr->buf_index >= 0) {
			u32 fourcc = bw->win[i].format;
			if (!adf_overlay_engine_supports_format(eng, fourcc)) {
				char format_str[ADF_FORMAT_STR_SIZE];
				adf_format_str(fourcc, format_str);
				dev_err(&eng->base.dev, "%s: unsupported format %s\n",
						__func__, format_str);
				return -EINVAL;
			}

			tegra_adf_set_windowattr_basic(&dc->tmp_wins[idx],
					attr, fourcc);
		} else {
			dc->tmp_wins[i].flags = 0;
		}

		dc_wins[i] = &dc->tmp_wins[idx];
	}

	return tegra_dc_bandwidth_negotiate_bw(dc, dc_wins, bw->win_num);
#else
	return -EINVAL;
#endif
}

static int tegra_adf_set_proposed_bw(struct tegra_adf_info *adf_info,
			struct tegra_adf_proposed_bw __user *arg)
{
	u8 win_num;
	size_t bw_size;
	struct tegra_adf_proposed_bw *bw;
	int ret;

	if (get_user(win_num, &arg->win_num))
		return -EFAULT;

	bw_size = sizeof(*bw) + sizeof(bw->win[0]) * win_num;
	bw = kmalloc(bw_size, GFP_KERNEL);
	if (!bw)
		return -ENOMEM;

	if (copy_from_user(bw, arg, bw_size)) {
		ret = -EFAULT;
		goto done;
	}

	ret = tegra_adf_negotiate_bw(adf_info, bw);
done:
	kfree(bw);
	return ret;
}

static long tegra_adf_dev_ioctl(struct adf_obj *obj, unsigned int cmd,
		unsigned long arg)
{
	struct adf_device *dev = adf_obj_to_device(obj);
	struct tegra_adf_info *adf_info = adf_dev_to_tegra(dev);

	switch (cmd) {
	case TEGRA_ADF_SET_PROPOSED_BW:
		return tegra_adf_set_proposed_bw(adf_info,
				(struct tegra_adf_proposed_bw __user *)arg);

	default:
		return -ENOTTY;
	}
}


void tegra_adf_process_vblank(struct tegra_adf_info *adf_info,
		ktime_t timestamp)
{
	if (unlikely(!adf_info))
		pr_debug("%s: suppressing vblank event since ADF is not finished probing\n",
				__func__);
	else
		adf_vsync_notify(&adf_info->intf, timestamp);
}

static bool tegra_adf_intf_supports_event(struct adf_obj *obj,
		enum adf_event_type type)
{
	struct adf_interface *intf = adf_obj_to_interface(obj);
	struct tegra_adf_info *tegra_adf = adf_intf_to_tegra(intf);

	switch (type) {
	case ADF_EVENT_VSYNC:
		return true;
	case ADF_EVENT_HOTPLUG:
		return tegra_dc_get_out(tegra_adf->dc) == TEGRA_DC_OUT_HDMI;
	default:
		return false;
	}
}

static void tegra_adf_set_vsync(struct tegra_adf_info *tegra_adf, bool enabled)
{
	if (enabled) {
		tegra_dc_hold_dc_out(tegra_adf->dc);
		tegra_dc_vsync_enable(tegra_adf->dc);
	} else {
		tegra_dc_vsync_disable(tegra_adf->dc);
		tegra_dc_release_dc_out(tegra_adf->dc);
	}
}

static void tegra_adf_intf_set_event(struct adf_obj *obj,
		enum adf_event_type type, bool enabled)
{
	struct adf_interface *intf = adf_obj_to_interface(obj);
	struct tegra_adf_info *tegra_adf = adf_intf_to_tegra(intf);

	switch (type) {
	case ADF_EVENT_VSYNC:
		tegra_adf_set_vsync(tegra_adf, enabled);
		return;

	case ADF_EVENT_HOTPLUG:
		return;

	default:
		BUG();
	}
}

static enum adf_interface_type tegra_adf_interface_type(struct tegra_dc *dc)
{
	/* TODO: can RGB and LVDS be mapped to existing ADF_INTF types?
	 * Should they be added to ADF's list? */
	switch (tegra_dc_get_out(dc)) {
	case TEGRA_DC_OUT_RGB:
		return TEGRA_ADF_INTF_RGB;
	case TEGRA_DC_OUT_HDMI:
		return ADF_INTF_HDMI;
	case TEGRA_DC_OUT_DSI:
		return ADF_INTF_DSI;
	case TEGRA_DC_OUT_DP:
		return ADF_INTF_eDP;
	case TEGRA_DC_OUT_LVDS:
		return TEGRA_ADF_INTF_LVDS;
	default:
		BUG();
	}
}

static const char *tegra_adf_intf_type_str(struct adf_interface *intf)
{
	switch ((enum tegra_adf_interface_type)intf->type) {
	case TEGRA_ADF_INTF_RGB:
		return "RGB";
	case TEGRA_ADF_INTF_LVDS:
		return "LVDS";
	default:
		BUG();
	}
}

#if IS_ENABLED(CONFIG_ADF_TEGRA_FBDEV)
static int tegra_adf_dpms_to_fb_blank(u8 dpms_state)
{
	switch (dpms_state) {
	case DRM_MODE_DPMS_ON:
		return FB_BLANK_UNBLANK;
	case DRM_MODE_DPMS_STANDBY:
		return FB_BLANK_HSYNC_SUSPEND;
	case DRM_MODE_DPMS_SUSPEND:
		return FB_BLANK_VSYNC_SUSPEND;
	case DRM_MODE_DPMS_OFF:
		return FB_BLANK_POWERDOWN;
	default:
		BUG();
	}
}
#endif

static int tegra_adf_intf_blank(struct adf_interface *intf, u8 state)
{
	struct tegra_adf_info *adf_info = adf_intf_to_tegra(intf);

	switch (state) {
	case DRM_MODE_DPMS_ON:
		tegra_dc_enable(adf_info->dc);
		break;

	case DRM_MODE_DPMS_STANDBY:
		tegra_dc_blank(adf_info->dc, BLANK_ALL);
		break;

	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_OFF:
		tegra_dc_disable(adf_info->dc);
		break;

	default:
		return -ENOTTY;
	}

#if IS_ENABLED(CONFIG_ADF_TEGRA_FBDEV)
	if (intf->flags & ADF_INTF_FLAG_PRIMARY) {
		struct fb_event event;
		int fb_state = tegra_adf_dpms_to_fb_blank(state);

		event.info = adf_info->fbdev.info;
		event.data = &fb_state;
		fb_notifier_call_chain(FB_EVENT_BLANK, &event);
	}
#endif

	return 0;
}

static int tegra_adf_intf_alloc_simple_buffer(struct adf_interface *intf,
		u16 w, u16 h, u32 format,
		struct dma_buf **dma_buf, u32 *offset, u32 *pitch)
{
	size_t i;
	struct tegra_adf_info *adf_info = adf_intf_to_tegra(intf);
	const struct vb2_mem_ops *mem_ops = &vb2_dma_contig_memops;
	void *vb2_buf;
	bool format_valid = false;

	for (i = 0; i < ARRAY_SIZE(tegra_adf_formats); i++) {
		if (tegra_adf_formats[i] == format) {
			format_valid = true;
			break;
		}
	}

	if (!format_valid)
		return -EINVAL;

	*offset = 0;
	*pitch = ALIGN(w * adf_format_bpp(format) / 8, 64);

	vb2_buf = mem_ops->alloc(adf_info->vb2_dma_conf,
				 h * *pitch, __GFP_HIGHMEM);
	if (IS_ERR(vb2_buf))
		return PTR_ERR(vb2_buf);

	*dma_buf = mem_ops->get_dmabuf(vb2_buf);
	mem_ops->put(vb2_buf);
	if (!*dma_buf)
		return -ENOMEM;

	return 0;
}

static int tegra_adf_intf_describe_simple_post(struct adf_interface *intf,
		struct adf_buffer *fb, void *data, size_t *size)
{
	struct tegra_adf_info *adf_info = adf_intf_to_tegra(intf);
	struct tegra_adf_flip *args = data;
	int i;

	args->win_num = 0;
	for_each_set_bit(i, &adf_info->dc->valid_windows, DC_N_WINDOWS) {
		struct tegra_adf_flip_windowattr *win =
				&args->win[args->win_num];
		win->win_index = i;
		if (i == adf_info->fb_data->win) {
			win->buf_index = 0;
			win->out_w = intf->current_mode.hdisplay;
			win->out_h = intf->current_mode.vdisplay;
		} else {
			win->buf_index = -1;
		}
		args->win_num++;
	}

	*size = sizeof(*args) + args->win_num * sizeof(args->win[0]);
	return 0;
}

static int tegra_adf_intf_modeset(struct adf_interface *intf,
		struct drm_mode_modeinfo *mode)
{
	struct tegra_adf_info *adf_info = adf_intf_to_tegra(intf);
	return tegra_dc_set_drm_mode(adf_info->dc, mode, false);
}

static int tegra_adf_intf_screen_size(struct adf_interface *intf, u16 *width_mm,
		u16 *height_mm)
{
	struct tegra_adf_info *adf_info = adf_intf_to_tegra(intf);
	struct tegra_dc_out *out = adf_info->dc->out;

	if (!out->height)
		return -EINVAL;

	*width_mm = out->width;
	*height_mm = out->height;
	return 0;
}

struct adf_device_ops tegra_adf_dev_ops = {
	.owner = THIS_MODULE,
	.base = {
		.custom_data = tegra_adf_dev_custom_data,
		.ioctl = tegra_adf_dev_ioctl,
	},
	.validate_custom_format = tegra_adf_dev_validate_custom_format,
	.validate = tegra_adf_dev_validate,
	.complete_fence = tegra_adf_dev_complete_fence,
	.post = tegra_adf_dev_post,
	.advance_timeline = tegra_adf_dev_advance_timeline,
	.state_free = tegra_adf_dev_state_free,
};

struct adf_interface_ops tegra_adf_intf_ops = {
	.base = {
		.supports_event = tegra_adf_intf_supports_event,
		.set_event = tegra_adf_intf_set_event,
	},
	.blank = tegra_adf_intf_blank,
	.alloc_simple_buffer = tegra_adf_intf_alloc_simple_buffer,
	.describe_simple_post = tegra_adf_intf_describe_simple_post,
	.modeset = tegra_adf_intf_modeset,
	.screen_size = tegra_adf_intf_screen_size,
	.type_str = tegra_adf_intf_type_str,
};

struct adf_overlay_engine_ops tegra_adf_eng_ops = {
	.supported_formats = tegra_adf_formats,
	.n_supported_formats = ARRAY_SIZE(tegra_adf_formats),
};

int tegra_adf_process_bandwidth_renegotiate(struct tegra_adf_info *adf_info,
						struct tegra_dc_bw_data *bw)
{
	struct tegra_adf_event_bandwidth event;
	event.base.type = TEGRA_ADF_EVENT_BANDWIDTH_RENEGOTIATE;
	event.base.length = sizeof(event);
	event.total_bw = bw->total_bw;
	event.avail_bw = bw->avail_bw;
	event.resvd_bw = bw->resvd_bw;
	return adf_event_notify(&adf_info->base.base, &event.base);
}

struct fb_ops tegra_adf_fb_ops = {
	.owner = THIS_MODULE,
	.fb_open = adf_fbdev_open,
	.fb_release = adf_fbdev_release,
	.fb_check_var = adf_fbdev_check_var,
	.fb_set_par = adf_fbdev_set_par,
	.fb_blank = adf_fbdev_blank,
	.fb_pan_display = adf_fbdev_pan_display,
	.fb_mmap = adf_fbdev_mmap,
};

static void tegra_adf_save_bootloader_logo(struct tegra_adf_info *adf_info,
            struct resource *fb_mem)
{
	struct device *dev = adf_info->base.dev;
	struct adf_buffer logo;
	struct sync_fence *fence;

	memset(&logo, 0, sizeof(logo));
	logo.dma_bufs[0] = adf_memblock_export(fb_mem->start,
			resource_size(fb_mem), 0);
	if (IS_ERR(logo.dma_bufs[0])) {
		dev_warn(dev, "failed to export bootloader logo: %ld\n",
				PTR_ERR(logo.dma_bufs[0]));
		return;
	}

	logo.overlay_engine = &adf_info->eng;
	logo.w = adf_info->fb_data->xres;
	logo.h = adf_info->fb_data->yres;
	logo.format = adf_info->fb_data->bits_per_pixel == 16 ?
			DRM_FORMAT_RGB565 :
			DRM_FORMAT_RGBA8888;
	logo.pitch[0] = logo.w * adf_info->fb_data->bits_per_pixel / 8;
	logo.n_planes = 1;

	fence = adf_interface_simple_post(&adf_info->intf, &logo);
	if (IS_ERR(fence))
		dev_warn(dev, "failed to post bootloader logo: %ld\n",
				PTR_ERR(fence));
	else
		sync_fence_put(fence);

	dma_buf_put(logo.dma_bufs[0]);
}

struct tegra_adf_info *tegra_adf_init(struct platform_device *ndev,
		struct tegra_dc *dc,
		struct tegra_fb_data *fb_data,
		struct resource *fb_mem)
{
	struct tegra_adf_info *adf_info;
	int err;
	enum adf_interface_type intf_type;
	u32 intf_flags = 0;
#if IS_ENABLED(CONFIG_ADF_TEGRA_FBDEV)
	u32 fb_format;
#endif

	adf_info = kzalloc(sizeof(*adf_info), GFP_KERNEL);
	if (!adf_info)
		return ERR_PTR(-ENOMEM);

	adf_info->dc = dc;
	adf_info->fb_data = fb_data;

	err = adf_device_init(&adf_info->base, &ndev->dev,
			&tegra_adf_dev_ops, "%s", dev_name(&ndev->dev));
	if (err < 0)
		goto err_dev_init;

	intf_type = tegra_adf_interface_type(dc);

	if (ndev->id == 0)
		intf_flags |= ADF_INTF_FLAG_PRIMARY;

	if (intf_type == ADF_INTF_HDMI)
		intf_flags |= ADF_INTF_FLAG_EXTERNAL;

	err = adf_interface_init(&adf_info->intf, &adf_info->base,
			intf_type, 0, intf_flags,
			&tegra_adf_intf_ops, "%s", dev_name(&ndev->dev));
	if (err < 0)
		goto err_intf_init;

	err = adf_overlay_engine_init(&adf_info->eng, &adf_info->base,
			&tegra_adf_eng_ops, "%s", dev_name(&ndev->dev));
	if (err < 0)
		goto err_eng_init;

#if IS_ENABLED(CONFIG_ADF_TEGRA_FBDEV)
	fb_format = fb_data->bits_per_pixel == 16 ? DRM_FORMAT_RGB565 :
			DRM_FORMAT_RGBA8888;
	err = adf_fbdev_init(&adf_info->fbdev, &adf_info->intf,
		&adf_info->eng, fb_data->xres, fb_data->yres * 2, fb_format,
		&tegra_adf_fb_ops, "%s", dev_name(&ndev->dev));
	if (err < 0)
		goto err_fbdev;
#endif

	err = adf_attachment_allow(&adf_info->base, &adf_info->eng,
			&adf_info->intf);
	if (err < 0)
		goto err_attach;

	adf_info->vb2_dma_conf = vb2_dma_contig_init_ctx(&ndev->dev);
	if ((err = IS_ERR(adf_info->vb2_dma_conf)))
		goto err_attach;

	if (dc->out->n_modes) {
		err = tegra_adf_process_hotplug_connected(adf_info, NULL);
		if (err < 0)
			goto err_attach;
	}

	if (dc->enabled)
		adf_info->intf.dpms_state = DRM_MODE_DPMS_ON;

	if (fb_data->flags & TEGRA_FB_FLIP_ON_PROBE)
		tegra_adf_save_bootloader_logo(adf_info, fb_mem);
	else
		memblock_free(fb_mem->start, resource_size(fb_mem));

	dev_info(&ndev->dev, "ADF initialized\n");

	return adf_info;

err_attach:
#if IS_ENABLED(CONFIG_ADF_TEGRA_FBDEV)
	adf_fbdev_destroy(&adf_info->fbdev);

err_fbdev:
#endif
	adf_overlay_engine_destroy(&adf_info->eng);

err_eng_init:
	adf_interface_destroy(&adf_info->intf);

err_intf_init:
	adf_device_destroy(&adf_info->base);

err_dev_init:
	kfree(adf_info);
	return ERR_PTR(err);
}

void tegra_adf_unregister(struct tegra_adf_info *adf_info)
{
#if IS_ENABLED(CONFIG_ADF_TEGRA_FBDEV)
	adf_fbdev_destroy(&adf_info->fbdev);
#endif
	adf_overlay_engine_destroy(&adf_info->eng);
	adf_interface_destroy(&adf_info->intf);
	adf_device_destroy(&adf_info->base);
	kfree(adf_info);
}
