/*
 * Copyright(c) 2011-2016 Intel Corporation. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "../i915_drv.h"
#include <gvt.h>
#include <fb_decoder.h>

#define FORMAT_NUM	16
struct pixel_format {
	int	drm_format;	/* Pixel format in DRM definition */
	int	bpp;		/* Bits per pixel, 0 indicates invalid */
	char	*desc;		/* The description */
};

/* non-supported format has bpp default to 0 */
static struct pixel_format primary_pixel_formats[FORMAT_NUM] = {
	[0b0010]  = {DRM_FORMAT_C8, 8, "8-bit Indexed"},
	[0b0101]  = {DRM_FORMAT_RGB565, 16, "16-bit BGRX (5:6:5 MSB-R:G:B)"},
	[0b0110]  = {DRM_FORMAT_XRGB8888, 32, "32-bit BGRX (8:8:8:8 MSB-X:R:G:B)"},
	[0b1000]  = {DRM_FORMAT_XBGR2101010, 32, "32-bit RGBX (2:10:10:10 MSB-X:B:G:R)"},
	[0b1010] = {DRM_FORMAT_XRGB2101010, 32, "32-bit BGRX (2:10:10:10 MSB-X:R:G:B)"},
	[0b1100] = {DRM_FORMAT_XRGB161616_VGT, 64,
		    "64-bit RGBX Floating Point(16:16:16:16 MSB-X:B:G:R)"},
	[0b1110] = {DRM_FORMAT_XBGR8888, 32, "32-bit RGBX (8:8:8:8 MSB-X:B:G:R)"},
};

/* non-supported format has bpp default to 0 */
static struct pixel_format skl_pixel_formats[] = {
	{DRM_FORMAT_YUYV, 16, "16-bit packed YUYV (8:8:8:8 MSB-V:Y2:U:Y1)"},
	{DRM_FORMAT_UYVY, 16, "16-bit packed UYVY (8:8:8:8 MSB-Y2:V:Y1:U)"},
	{DRM_FORMAT_YVYU, 16, "16-bit packed YVYU (8:8:8:8 MSB-U:Y2:V:Y1)"},
	{DRM_FORMAT_VYUY, 16, "16-bit packed VYUY (8:8:8:8 MSB-Y2:U:Y1:V)"},

	{DRM_FORMAT_C8, 8, "8-bit Indexed"},
	{DRM_FORMAT_RGB565, 16, "16-bit BGRX (5:6:5 MSB-R:G:B)"},
	{DRM_FORMAT_ABGR8888, 32, "32-bit RGBA (8:8:8:8 MSB-A:B:G:R)"},
	{DRM_FORMAT_XBGR8888, 32, "32-bit RGBX (8:8:8:8 MSB-X:B:G:R)"},

	{DRM_FORMAT_ARGB8888, 32, "32-bit BGRA (8:8:8:8 MSB-A:R:G:B)"},
	{DRM_FORMAT_XRGB8888, 32, "32-bit BGRX (8:8:8:8 MSB-X:R:G:B)"},
	{DRM_FORMAT_XBGR2101010, 32, "32-bit RGBX (2:10:10:10 MSB-X:B:G:R)"},
	{DRM_FORMAT_XRGB2101010, 32, "32-bit BGRX (2:10:10:10 MSB-X:R:G:B)"},

	{DRM_FORMAT_XRGB161616_VGT, 64, "64-bit XRGB (16:16:16:16 MSB-X:R:G:B)"},
	{DRM_FORMAT_XBGR161616_VGT, 64, "64-bit XBGR (16:16:16:16 MSB-X:B:G:R)"},

	/* non-supported format has bpp default to 0 */
	{0, 0, NULL},
};

static int skl_format_to_drm(int format, bool rgb_order, bool alpha, int yuv_order)
{
	int skl_pixel_formats_index = 14;

	switch (format) {
	case PLANE_CTL_FORMAT_INDEXED:
		skl_pixel_formats_index = 4;
		break;
	case PLANE_CTL_FORMAT_RGB_565:
		skl_pixel_formats_index = 5;
		break;
	case PLANE_CTL_FORMAT_XRGB_8888:
		if (rgb_order)
			skl_pixel_formats_index = alpha ? 6 : 7;
		else
			skl_pixel_formats_index = alpha ? 8 : 9;
		break;
	case PLANE_CTL_FORMAT_XRGB_2101010:
		skl_pixel_formats_index = rgb_order ? 10 : 11;
		break;

	case PLANE_CTL_FORMAT_XRGB_16161616F:
		skl_pixel_formats_index = rgb_order ? 12 : 13;
		break;

	case PLANE_CTL_FORMAT_YUV422:
		skl_pixel_formats_index = yuv_order >> 16;
		if (skl_pixel_formats_index > 3)
			return -EINVAL;
		break;

	default:
		break;
	}

	return skl_pixel_formats_index;
}

static u32 gvt_get_stride(struct intel_vgpu *vgpu, int pipe, u32 tiled,
			int stride_mask, int bpp)
{
        struct drm_i915_private *dev_priv = vgpu->gvt->dev_priv;

	u32 stride_reg = vgpu_vreg(vgpu, DSPSTRIDE(pipe)) & stride_mask;
	u32 stride = stride_reg;

	if (IS_SKYLAKE(dev_priv) || IS_BROXTON(dev_priv) ||
	    IS_KABYLAKE(dev_priv)) {
		switch (tiled) {
		case PLANE_CTL_TILED_LINEAR:
			stride = stride_reg * 64;
			break;
		case PLANE_CTL_TILED_X:
			stride = stride_reg * 512;
			break;
		case PLANE_CTL_TILED_Y:
			stride = stride_reg * 128;
			break;
		case PLANE_CTL_TILED_YF:
			if (bpp == 8)
				stride = stride_reg * 64;
			else if (bpp == 16 || bpp == 32 || bpp == 64)
				stride = stride_reg * 128;
			else
				DRM_DEBUG_KMS("skl: unsupported bpp:%d\n", bpp);
			break;
		default:
			DRM_DEBUG_KMS("skl: unsupported tile format:%x\n", tiled);
		}
	}

        return stride;
}

static int gvt_decode_primary_plane_format(struct intel_vgpu *vgpu,
	int pipe, struct gvt_primary_plane_format *plane)
{
	u32	val, fmt;
	struct drm_i915_private *dev_priv = vgpu->gvt->dev_priv;

	val = vgpu_vreg(vgpu, DSPCNTR(pipe));
	plane->enabled = !!(val & DISPLAY_PLANE_ENABLE);
	if (!plane->enabled)
		return 0;

	if (IS_SKYLAKE(dev_priv) || IS_BROXTON(dev_priv) ||
	    IS_KABYLAKE(dev_priv)) {
		plane->tiled = (val & PLANE_CTL_TILED_MASK) >> _PLANE_CTL_TILED_SHIFT;
		fmt = skl_format_to_drm(
		val & PLANE_CTL_FORMAT_MASK,
		val & PLANE_CTL_ORDER_RGBX,
		val & PLANE_CTL_ALPHA_MASK,
		val & PLANE_CTL_YUV422_ORDER_MASK);
		plane->bpp = skl_pixel_formats[fmt].bpp;
		plane->drm_format = skl_pixel_formats[fmt].drm_format;
	} else {
		plane->tiled = !!(val & DISPPLANE_TILED);
		fmt = (val & DISPPLANE_PIXFORMAT_MASK) >> _PRI_PLANE_FMT_SHIFT;
		plane->bpp = primary_pixel_formats[fmt].bpp;
		plane->drm_format = primary_pixel_formats[fmt].drm_format;
	}

	if (((IS_SKYLAKE(dev_priv) || IS_BROXTON(dev_priv) ||
	      IS_KABYLAKE(dev_priv)) && !skl_pixel_formats[fmt].bpp) ||
	   (!(IS_SKYLAKE(dev_priv) || IS_BROXTON(dev_priv) ||
	      IS_KABYLAKE(dev_priv)) && !primary_pixel_formats[fmt].bpp)) {
		gvt_err("Non-supported pixel format (0x%x)\n", fmt);
		return -EINVAL;
	}

	plane->hw_format = fmt;

	plane->base = vgpu_vreg(vgpu, DSPSURF(pipe)) & GTT_PAGE_MASK;

	plane->stride = gvt_get_stride(vgpu, pipe, (plane->tiled << 10),
			(IS_SKYLAKE(dev_priv) || IS_BROXTON(dev_priv) ||
			 IS_KABYLAKE(dev_priv)) ?
				(_PRI_PLANE_STRIDE_MASK >> 6)
				: _PRI_PLANE_STRIDE_MASK, plane->bpp);

	plane->width = vgpu_vreg(vgpu, PLANE_SIZE(pipe, PLANE_PRIMARY))&
					_PLANE_SIZE_WIDTH_MASK;
	plane->width += 1;
	plane->height = (vgpu_vreg(vgpu, PLANE_SIZE(pipe, PLANE_PRIMARY)) &
			 _PLANE_SIZE_HEIGHT_MASK) >> _PLANE_SIZE_HEIGHT_SHIFT;
	plane->height += 1;	/* raw height is one minus the real value */

	val = vgpu_vreg(vgpu, DSPTILEOFF(pipe));
	plane->x_offset = (val & _PRI_PLANE_X_OFF_MASK) >>
		_PRI_PLANE_X_OFF_SHIFT;
	plane->y_offset = (val & _PRI_PLANE_Y_OFF_MASK) >>
		_PRI_PLANE_Y_OFF_SHIFT;

	return 0;
}

#define CURSOR_MODE_NUM	(1 << 6)
struct cursor_mode_format {
	int	drm_format;	/* Pixel format in DRM definition */
	u8	bpp;		/* Bits per pixel; 0 indicates invalid */
	u32	width;		/* In pixel */
	u32	height;		/* In lines */
	char	*desc;		/* The description */
};

/* non-supported format has bpp default to 0 */
static struct cursor_mode_format cursor_pixel_formats[CURSOR_MODE_NUM] = {
	[0b100010]  = {DRM_FORMAT_ARGB8888, 32, 128, 128,"128x128 32bpp ARGB"},
	[0b100011]  = {DRM_FORMAT_ARGB8888, 32, 256, 256, "256x256 32bpp ARGB"},
	[0b100111]  = {DRM_FORMAT_ARGB8888, 32, 64, 64, "64x64 32bpp ARGB"},
	[0b000111]  = {DRM_FORMAT_ARGB8888, 32, 64, 64, "64x64 32bpp ARGB"},//actually inverted... figure this out later
};

static int gvt_decode_cursor_plane_format(struct intel_vgpu *vgpu,
	int pipe, struct gvt_cursor_plane_format *plane)
{
	u32 val, mode;
	u32 alpha_plane, alpha_force;
	struct drm_i915_private *dev_priv = vgpu->gvt->dev_priv;

	val = vgpu_vreg(vgpu, CURCNTR(pipe));
	mode = val & CURSOR_MODE;
	plane->enabled = (mode != CURSOR_MODE_DISABLE);
	if (!plane->enabled)
		return 0;

	if (!cursor_pixel_formats[mode].bpp) {
		gvt_err("Non-supported cursor mode (0x%x)\n", mode);
		return -EINVAL;
	}
	plane->mode = mode;
	plane->bpp = cursor_pixel_formats[mode].bpp;
	plane->drm_format = cursor_pixel_formats[mode].drm_format;
	plane->width = cursor_pixel_formats[mode].width;
	plane->height = cursor_pixel_formats[mode].height;

	alpha_plane = (val & _CURSOR_ALPHA_PLANE_MASK) >>
				_CURSOR_ALPHA_PLANE_SHIFT;
	alpha_force = (val & _CURSOR_ALPHA_FORCE_MASK) >>
				_CURSOR_ALPHA_FORCE_SHIFT;
	if (alpha_plane || alpha_force)
		gvt_dbg_core("alpha_plane=0x%x, alpha_force=0x%x\n",
			alpha_plane, alpha_force);

	plane->base = vgpu_vreg(vgpu, CURBASE(pipe)) & GTT_PAGE_MASK;

	val = vgpu_vreg(vgpu, CURPOS(pipe));
	plane->x_pos = (val & _CURSOR_POS_X_MASK) >> _CURSOR_POS_X_SHIFT;
	plane->x_sign = (val & _CURSOR_SIGN_X_MASK) >> _CURSOR_SIGN_X_SHIFT;
	plane->y_pos = (val & _CURSOR_POS_Y_MASK) >> _CURSOR_POS_Y_SHIFT;
	plane->y_sign = (val & _CURSOR_SIGN_Y_MASK) >> _CURSOR_SIGN_Y_SHIFT;

	return 0;
}

#define FORMAT_NUM_SRRITE	(1 << 3)

static struct pixel_format sprite_pixel_formats[FORMAT_NUM_SRRITE] = {
	[0b000]  = {DRM_FORMAT_YUV422, 16, "YUV 16-bit 4:2:2 packed"},
	[0b001]  = {DRM_FORMAT_XRGB2101010, 32, "RGB 32-bit 2:10:10:10"},
	[0b010]  = {DRM_FORMAT_XRGB8888, 32, "RGB 32-bit 8:8:8:8"},
	[0b011]  = {DRM_FORMAT_XRGB161616_VGT, 64,
		    "RGB 64-bit 16:16:16:16 Floating Point"},
	[0b100] = {DRM_FORMAT_AYUV, 32, "YUV 32-bit 4:4:4 packed (8:8:8:8 MSB-X:Y:U:V)"},
};

static int gvt_decode_sprite_plane_format(struct intel_vgpu *vgpu,
	int pipe, struct gvt_sprite_plane_format *plane)
{
	u32 val, fmt;
	u32 width;
	u32 color_order, yuv_order;
	int drm_format;

	val = vgpu_vreg(vgpu, SPRCTL(pipe));
	plane->enabled = !!(val & SPRITE_ENABLE);
	if (!plane->enabled)
		return 0;

	plane->tiled = !!(val & SPRITE_TILED);
	color_order = !!(val & SPRITE_RGB_ORDER_RGBX);
	yuv_order = (val & SPRITE_YUV_BYTE_ORDER_MASK) >>
				_SPRITE_YUV_ORDER_SHIFT;

	fmt = (val & SPRITE_PIXFORMAT_MASK) >> _SPRITE_FMT_SHIFT;
	if (!sprite_pixel_formats[fmt].bpp) {
		gvt_err("Non-supported pixel format (0x%x)\n", fmt);
		return -EINVAL;
	}
	plane->hw_format = fmt;
	plane->bpp = sprite_pixel_formats[fmt].bpp;
	drm_format = sprite_pixel_formats[fmt].drm_format;

	/* Order of RGB values in an RGBxxx buffer may be ordered RGB or
	 * BGR depending on the state of the color_order field
	 */
	if (!color_order) {
		if (drm_format == DRM_FORMAT_XRGB2101010)
			drm_format = DRM_FORMAT_XBGR2101010;
		else if (drm_format == DRM_FORMAT_XRGB8888)
			drm_format = DRM_FORMAT_XBGR8888;
	}

	if (drm_format == DRM_FORMAT_YUV422) {
		switch (yuv_order){
		case	0:
			drm_format = DRM_FORMAT_YUYV;
			break;
		case	1:
			drm_format = DRM_FORMAT_UYVY;
			break;
		case	2:
			drm_format = DRM_FORMAT_YVYU;
			break;
		case	3:
			drm_format = DRM_FORMAT_VYUY;
			break;
		default:
			/* yuv_order has only 2 bits */
			BUG();
			break;
		}
	}

	plane->drm_format = drm_format;

	plane->base = vgpu_vreg(vgpu, SPRSURF(pipe)) & GTT_PAGE_MASK;
	plane->width = vgpu_vreg(vgpu, SPRSTRIDE(pipe)) &
				_SPRITE_STRIDE_MASK;
	plane->width /= plane->bpp / 8;	/* raw width in bytes */

	val = vgpu_vreg(vgpu, SPRSIZE(pipe));
	plane->height = (val & _SPRITE_SIZE_HEIGHT_MASK) >>
		_SPRITE_SIZE_HEIGHT_SHIFT;
	width = (val & _SPRITE_SIZE_WIDTH_MASK) >> _SPRITE_SIZE_WIDTH_SHIFT;
	plane->height += 1;	/* raw height is one minus the real value */
	width += 1;		/* raw width is one minus the real value */
	if (plane->width != width)
		gvt_dbg_core("sprite_plane: plane->width=%d, width=%d\n",
			plane->width, width);

	val = vgpu_vreg(vgpu, SPRPOS(pipe));
	plane->x_pos = (val & _SPRITE_POS_X_MASK) >> _SPRITE_POS_X_SHIFT;
	plane->y_pos = (val & _SPRITE_POS_Y_MASK) >> _SPRITE_POS_Y_SHIFT;

	val = vgpu_vreg(vgpu, SPROFFSET(pipe));
	plane->x_offset = (val & _SPRITE_OFFSET_START_X_MASK) >>
			   _SPRITE_OFFSET_START_X_SHIFT;
	plane->y_offset = (val & _SPRITE_OFFSET_START_Y_MASK) >>
			   _SPRITE_OFFSET_START_Y_SHIFT;
	return 0;
}

/**
 * gvt_decode_fb_format - Decode framebuffer information from raw vMMIO
 * @gvt: GVT device
 * @vmid: guest domain ID
 * @fb: frame buffer infomation of guest.
 * This function is called for query frame buffer format, so that gl can
 * display guest fb in Dom0
 *
 * Returns:
 * Zero on success, negative error code if failed.
 */
int gvt_decode_fb_format(struct intel_gvt *gvt, int id, struct gvt_fb_format *fb)
{
	int i;
	struct intel_vgpu *vgpu = NULL;
	int ret = 0;
	struct drm_i915_private *dev_priv = gvt->dev_priv;

	if (!fb)
		return -EINVAL;

	/* TODO: use fine-grained refcnt later */
	mutex_lock(&gvt->lock);

	for_each_active_vgpu(gvt, vgpu, i)
		if (vgpu->id == id)
			break;

	if (!vgpu) {
		gvt_err("Invalid vgpu ID (%d)\n", id);
		mutex_unlock(&gvt->lock);
		return -ENODEV;
	}

	for (i = 0; i < I915_MAX_PIPES; i++) {
		struct gvt_pipe_format *pipe = &fb->pipes[i];
		u32 ddi_func_ctl = vgpu_vreg(vgpu, TRANS_DDI_FUNC_CTL(i));

		if (!(ddi_func_ctl & TRANS_DDI_FUNC_ENABLE)) {
			pipe->ddi_port = DDI_PORT_NONE;
		} else {
			u32 port = (ddi_func_ctl & TRANS_DDI_PORT_MASK) >>
						TRANS_DDI_PORT_SHIFT;
			if (port <= DDI_PORT_E)
				pipe->ddi_port = port;
			else
				pipe->ddi_port = DDI_PORT_NONE;
		}

		ret |= gvt_decode_primary_plane_format(vgpu, i, &pipe->primary);
		ret |= gvt_decode_sprite_plane_format(vgpu, i, &pipe->sprite);
		ret |= gvt_decode_cursor_plane_format(vgpu, i, &pipe->cursor);

		if (ret) {
			gvt_err("Decode format error for pipe(%d)\n", i);
			ret = -EINVAL;
			break;
		}
	}

	mutex_unlock(&gvt->lock);

	return ret;
}
