/*
 * Copyright (c) 2013-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>

#include <media/soc_camera.h>
#include <media/soc_mediabus.h>
#include <media/videobuf2-dma-contig.h>
#include <media/tegra_v4l2_camera.h>

#include "common.h"

static const struct soc_mbus_pixelfmt tegra_camera_yuv_formats[] = {
	{
		.fourcc			= V4L2_PIX_FMT_UYVY,
		.name			= "YUV422 (UYVY) packed",
		.bits_per_sample	= 16,
		.packing		= SOC_MBUS_PACKING_NONE,
		.order			= SOC_MBUS_ORDER_LE,
	},
	{
		.fourcc			= V4L2_PIX_FMT_VYUY,
		.name			= "YUV422 (VYUY) packed",
		.bits_per_sample	= 16,
		.packing		= SOC_MBUS_PACKING_NONE,
		.order			= SOC_MBUS_ORDER_LE,
	},
	{
		.fourcc			= V4L2_PIX_FMT_YUYV,
		.name			= "YUV422 (YUYV) packed",
		.bits_per_sample	= 16,
		.packing		= SOC_MBUS_PACKING_NONE,
		.order			= SOC_MBUS_ORDER_LE,
	},
	{
		.fourcc			= V4L2_PIX_FMT_YVYU,
		.name			= "YUV422 (YVYU) packed",
		.bits_per_sample	= 16,
		.packing		= SOC_MBUS_PACKING_NONE,
		.order			= SOC_MBUS_ORDER_LE,
	},
	{
		.fourcc			= V4L2_PIX_FMT_YUV420,
		.name			= "YUV420 (YU12) planar",
		.bits_per_sample	= 12,
		.packing		= SOC_MBUS_PACKING_NONE,
		.order			= SOC_MBUS_ORDER_LE,
	},
	{
		.fourcc			= V4L2_PIX_FMT_YVU420,
		.name			= "YVU420 (YV12) planar",
		.bits_per_sample	= 12,
		.packing		= SOC_MBUS_PACKING_NONE,
		.order			= SOC_MBUS_ORDER_LE,
	},
};

struct tegra_camera_format_xlate {
	enum v4l2_mbus_pixelcode code;
	const struct soc_mbus_pixelfmt host_fmt;
};

static const struct tegra_camera_format_xlate tegra_camera_bayer_formats[] = {
	{
		.code				= V4L2_MBUS_FMT_SBGGR8_1X8,
		.host_fmt = {
			.fourcc			= V4L2_PIX_FMT_SBGGR8,
			.name			= "Bayer 8 BGBG.. GRGR..",
			.bits_per_sample	= 8,
			.packing		= SOC_MBUS_PACKING_NONE,
			.order			= SOC_MBUS_ORDER_LE,
		},
	},
	{
		.code				= V4L2_MBUS_FMT_SGBRG8_1X8,
		.host_fmt = {
			.fourcc			= V4L2_PIX_FMT_SGBRG8,
			.name			= "Bayer 8 GBGB.. RGRG..",
			.bits_per_sample	= 8,
			.packing		= SOC_MBUS_PACKING_NONE,
			.order			= SOC_MBUS_ORDER_LE,
		},
	},
	{
		.code				= V4L2_MBUS_FMT_SGRBG8_1X8,
		.host_fmt = {
			.fourcc			= V4L2_PIX_FMT_SGRBG8,
			.name			= "Bayer 8 GRGR.. BGBG..",
			.bits_per_sample	= 8,
			.packing		= SOC_MBUS_PACKING_NONE,
			.order			= SOC_MBUS_ORDER_LE,
		},
	},
	{
		.code				= V4L2_MBUS_FMT_SRGGB8_1X8,
		.host_fmt = {
			.fourcc			= V4L2_PIX_FMT_SRGGB8,
			.name			= "Bayer 8 RGRG.. GBGB..",
			.bits_per_sample	= 8,
			.packing		= SOC_MBUS_PACKING_NONE,
			.order			= SOC_MBUS_ORDER_LE,
		},
	},
	{
		.code				= V4L2_MBUS_FMT_SBGGR10_1X10,
		.host_fmt = {
			.fourcc			= V4L2_PIX_FMT_SBGGR10,
			.name			= "Bayer 10 BGBG.. GRGR..",
			.bits_per_sample	= 10,
			.packing		= SOC_MBUS_PACKING_EXTEND16,
			.order			= SOC_MBUS_ORDER_LE,
		},
	},
	{
		.code				= V4L2_MBUS_FMT_SGBRG10_1X10,
		.host_fmt = {
			.fourcc			= V4L2_PIX_FMT_SGBRG10,
			.name			= "Bayer 10 GBGB.. RGRG..",
			.bits_per_sample	= 10,
			.packing		= SOC_MBUS_PACKING_EXTEND16,
			.order			= SOC_MBUS_ORDER_LE,
		},
	},
	{
		.code				= V4L2_MBUS_FMT_SGRBG10_1X10,
		.host_fmt = {
			.fourcc			= V4L2_PIX_FMT_SGRBG10,
			.name			= "Bayer 10 GRGR.. BGBG..",
			.bits_per_sample	= 10,
			.packing		= SOC_MBUS_PACKING_EXTEND16,
			.order			= SOC_MBUS_ORDER_LE,
		},
	},
	{
		.code				= V4L2_MBUS_FMT_SRGGB10_1X10,
		.host_fmt = {
			.fourcc			= V4L2_PIX_FMT_SRGGB10,
			.name			= "Bayer 10 RGRG.. GBGB..",
			.bits_per_sample	= 10,
			.packing		= SOC_MBUS_PACKING_EXTEND16,
			.order			= SOC_MBUS_ORDER_LE,
		},
	},
	{
		.code				= V4L2_MBUS_FMT_SRGGB12_1X12,
		.host_fmt = {
			.fourcc			= V4L2_PIX_FMT_SRGGB12,
			.name			= "Bayer 12 RGRG.. GBGB..",
			.bits_per_sample	= 16,
			.packing		= SOC_MBUS_PACKING_EXTEND16,
			.order			= SOC_MBUS_ORDER_LE,
		},
	},
};

static const struct soc_mbus_pixelfmt tegra_camera_rgb_formats[] = {
	{
		.fourcc			= V4L2_PIX_FMT_RGB32,
		.name			= "RGBA 8-8-8-8",
		.bits_per_sample	= 32,
		.packing		= SOC_MBUS_PACKING_NONE,
		.order			= SOC_MBUS_ORDER_LE,
	},
};

static int tegra_camera_init_buffer(struct tegra_camera_buffer *buf)
{
	struct soc_camera_device *icd = buf->icd;
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct tegra_camera *cam = ici->priv;
	int bytes_per_line =
		cam->ops->bytes_per_line(icd->user_width,
					 icd->current_fmt->host_fmt);
	struct soc_camera_subdev_desc *ssdesc = &icd->sdesc->subdev_desc;
	struct tegra_camera_platform_data *pdata = ssdesc->drv_priv;

	switch (icd->current_fmt->host_fmt->fourcc) {
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
	case V4L2_PIX_FMT_SRGGB12:
	case V4L2_PIX_FMT_RGB32:
		buf->buffer_addr = vb2_dma_contig_plane_dma_addr(&buf->vb, 0);
		buf->start_addr = buf->buffer_addr;

		if (pdata->flip_v)
			buf->start_addr += bytes_per_line *
					   (icd->user_height-1);

		if (pdata->flip_h)
			buf->start_addr += bytes_per_line - 1;

		break;

	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		buf->buffer_addr = vb2_dma_contig_plane_dma_addr(&buf->vb, 0);
		buf->buffer_addr_u = buf->buffer_addr +
				     icd->user_width * icd->user_height;
		buf->buffer_addr_v = buf->buffer_addr_u +
				     (icd->user_width * icd->user_height) / 4;

		/* For YVU420, we swap the locations of the U and V planes. */
		if (icd->current_fmt->host_fmt->fourcc == V4L2_PIX_FMT_YVU420) {
			dma_addr_t temp = buf->buffer_addr_u;
			buf->buffer_addr_u = buf->buffer_addr_v;
			buf->buffer_addr_v = temp;
		}

		buf->start_addr = buf->buffer_addr;
		buf->start_addr_u = buf->buffer_addr_u;
		buf->start_addr_v = buf->buffer_addr_v;

		if (pdata->flip_v) {
			buf->start_addr += icd->user_width *
					   (icd->user_height - 1);

			buf->start_addr_u += ((icd->user_width/2) *
					      ((icd->user_height/2) - 1));

			buf->start_addr_v += ((icd->user_width/2) *
					      ((icd->user_height/2) - 1));
		}

		if (pdata->flip_h) {
			buf->start_addr += icd->user_width - 1;

			buf->start_addr_u += (icd->user_width/2) - 1;

			buf->start_addr_v += (icd->user_width/2) - 1;
		}

		break;

	default:
		dev_err(icd->parent, "Wrong host format %d\n",
			icd->current_fmt->host_fmt->fourcc);
		return -EINVAL;
	}

	return 0;
}

/*
 *  Videobuf operations
 */
static int tegra_camera_videobuf_setup(struct vb2_queue *vq,
				       const struct v4l2_format *fmt,
				       unsigned int *num_buffers,
				       unsigned int *num_planes,
				       unsigned int sizes[],
				       void *alloc_ctxs[])
{
	struct soc_camera_device *icd = container_of(vq,
						     struct soc_camera_device,
						     vb2_vidq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct tegra_camera *cam = ici->priv;
	int bytes_per_line =
		cam->ops->bytes_per_line(icd->user_width,
					icd->current_fmt->host_fmt);

	if (bytes_per_line < 0)
		return bytes_per_line;

	*num_planes = 1;

	sizes[0] = bytes_per_line * icd->user_height;
	alloc_ctxs[0] = cam->alloc_ctx;

	if (!*num_buffers)
		*num_buffers = 2;

	return 0;
}

static int tegra_camera_videobuf_init(struct vb2_buffer *vb)
{
	/* This is for locking debugging only */
	INIT_LIST_HEAD(&to_tegra_vb(vb)->queue);

	return 0;
}

static int tegra_camera_videobuf_prepare(struct vb2_buffer *vb)
{
	struct soc_camera_device *icd = container_of(vb->vb2_queue,
						     struct soc_camera_device,
						     vb2_vidq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct tegra_camera *cam = ici->priv;
	struct tegra_camera_buffer *buf = to_tegra_vb(vb);
	int bytes_per_line = 0;
	unsigned long size = 0;
	struct tegra_camera_platform_data *pdata = icd_to_pdata(icd);
	int port = 0;

	buf->icd = icd;

	if (!icd->current_fmt) {
		dev_err(icd->parent, "%s NULL format point\n", __func__);
		return -EINVAL;
	}

	bytes_per_line =
		cam->ops->bytes_per_line(icd->user_width,
					icd->current_fmt->host_fmt);
	if (bytes_per_line < 0)
		return bytes_per_line;

	if (!pdata) {
		dev_err(icd->parent, "No platform data for this device!\n");
		return -EINVAL;
	}

	port = pdata->port;

	if (!cam->ops->port_is_valid(port)) {
		dev_err(icd->parent,
			"Invalid camera port %d in platform data\n",
			port);
		return -EINVAL;
	}

#ifdef PREFILL_BUFFER
	dev_info(icd->parent, "%s (vb=0x%p) 0x%p %lu\n", __func__,
		vb, vb2_plane_vaddr(vb, 0), vb2_plane_size(vb, 0));

	/*
	 * This can be useful if you want to see if we actually fill
	 * the buffer with something
	 */
	if (vb2_plane_vaddr(vb, 0))
		memset(vb2_plane_vaddr(vb, 0), 0xbd, vb2_plane_size(vb, 0));
#endif

	size = icd->user_height * bytes_per_line;

	if (vb2_plane_size(vb, 0) < size) {
		dev_err(icd->parent, "Buffer too small (%lu < %lu)\n",
			vb2_plane_size(vb, 0), size);
		return -ENOBUFS;
	}

	vb2_set_plane_payload(vb, 0, size);

	return tegra_camera_init_buffer(buf);
}

static void tegra_camera_videobuf_release(struct vb2_buffer *vb)
{
	struct soc_camera_device *icd = container_of(vb->vb2_queue,
						     struct soc_camera_device,
						     vb2_vidq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct tegra_camera_buffer *buf = to_tegra_vb(vb);
	struct tegra_camera *cam = ici->priv;

	if (cam->ops->videobuf_release)
		cam->ops->videobuf_release(cam, icd, buf);
}

static int tegra_camera_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct soc_camera_device *icd = container_of(q,
						     struct soc_camera_device,
						     vb2_vidq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct tegra_camera *cam = ici->priv;

	if (cam->ops->start_streaming)
		return cam->ops->start_streaming(cam, icd, count);

	return 0;
}

static int tegra_camera_stop_streaming(struct vb2_queue *q)
{
	struct soc_camera_device *icd = container_of(q,
						     struct soc_camera_device,
						     vb2_vidq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct tegra_camera *cam = ici->priv;

	if (cam->ops->stop_streaming)
		return cam->ops->stop_streaming(cam, icd);

	return 0;
}

static void tegra_camera_videobuf_queue(struct vb2_buffer *vb)
{
	struct soc_camera_device *icd = container_of(vb->vb2_queue,
						     struct soc_camera_device,
						     vb2_vidq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct tegra_camera *cam = ici->priv;
	struct tegra_camera_buffer *buf = to_tegra_vb(vb);

	if (cam->ops->videobuf_queue)
		cam->ops->videobuf_queue(cam, icd, buf);
}

static struct vb2_ops tegra_camera_videobuf_ops = {
	.queue_setup	= tegra_camera_videobuf_setup,
	.wait_prepare	= soc_camera_unlock,
	.wait_finish	= soc_camera_lock,
	.buf_init	= tegra_camera_videobuf_init,
	.buf_prepare	= tegra_camera_videobuf_prepare,
	.buf_cleanup	= tegra_camera_videobuf_release,
	.start_streaming = tegra_camera_start_streaming,
	.stop_streaming	= tegra_camera_stop_streaming,
	.buf_queue	= tegra_camera_videobuf_queue,
};

/*
 *  SOC camera host operations
 */

/*
 * Called with .video_lock held
 */
static int tegra_camera_add_device(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct tegra_camera *cam = ici->priv;

	if (cam->ops->add_device)
		return cam->ops->add_device(icd);

	return 0;
}

/* Called with .video_lock held */
static void tegra_camera_remove_device(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct tegra_camera *cam = ici->priv;

	if (cam->ops->remove_device)
		cam->ops->remove_device(icd);
}

static int soc_camera_clock_start(struct soc_camera_host *ici)
{
	struct tegra_camera *cam = ici->priv;

	if (cam->ops->clock_start)
		return cam->ops->clock_start(ici);

	return 0;
}

static void soc_camera_clock_stop(struct soc_camera_host *ici)
{
	struct tegra_camera *cam = ici->priv;

	if (cam->ops->clock_stop)
		cam->ops->clock_stop(ici);
}

static int tegra_camera_get_formats(struct soc_camera_device *icd,
				    unsigned int idx,
				    struct soc_camera_format_xlate *xlate)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct device *dev = icd->parent;
	struct soc_camera_host *ici = to_soc_camera_host(dev);
	struct tegra_camera *cam = ici->priv;
	int num_formats;
	const struct soc_mbus_pixelfmt *formats;
	int ret;
	enum v4l2_mbus_pixelcode code;
	int k;

	if (cam->ops->get_formats) {
		ret = cam->ops->get_formats(icd, idx, xlate);
		if (ret != -EAGAIN)
			return ret;
	}

	ret = v4l2_subdev_call(sd, video, enum_mbus_fmt, idx, &code);
	if (ret != 0)
		/* No more formats */
		return 0;

	switch (code) {
	/*
	 * The various YUV formats can have their color components swizzled
	 * by the camera host, so for a given color component ordering in the
	 * input, we can support any color component ordering in the output.
	 */
	case V4L2_MBUS_FMT_UYVY8_2X8:
	case V4L2_MBUS_FMT_VYUY8_2X8:
	case V4L2_MBUS_FMT_YUYV8_2X8:
	case V4L2_MBUS_FMT_YVYU8_2X8:
		formats = tegra_camera_yuv_formats;
		num_formats = ARRAY_SIZE(tegra_camera_yuv_formats);
		break;

	/*
	 * Bayer color components are not able to be swizzled, so we report
	 * one-to-one support for a given bayer color ordering.
	 */
	case V4L2_MBUS_FMT_SBGGR8_1X8:
	case V4L2_MBUS_FMT_SGBRG8_1X8:
	case V4L2_MBUS_FMT_SGRBG8_1X8:
	case V4L2_MBUS_FMT_SRGGB8_1X8:
	case V4L2_MBUS_FMT_SBGGR10_1X10:
	case V4L2_MBUS_FMT_SGBRG10_1X10:
	case V4L2_MBUS_FMT_SGRBG10_1X10:
	case V4L2_MBUS_FMT_SRGGB10_1X10:
	case V4L2_MBUS_FMT_SRGGB12_1X12:
		for (k = 0; k < ARRAY_SIZE(tegra_camera_bayer_formats); k++)
			if (tegra_camera_bayer_formats[k].code == code)
				break;

		if (k < ARRAY_SIZE(tegra_camera_bayer_formats)) {
			formats = &tegra_camera_bayer_formats[k].host_fmt;
			num_formats = 1;
		} else {
			dev_err(dev,
			"Running Out of supporting bayer format code 0x%04x\n", code);
			formats = NULL;
			num_formats = 0;
		}
		break;

	case V4L2_MBUS_FMT_RGBA8888_4X8_LE:
		formats = tegra_camera_rgb_formats;
		num_formats = ARRAY_SIZE(tegra_camera_rgb_formats);
		break;

	default:
		dev_dbg(dev, "Not supporting mbus format code 0x%04x\n", code);
		formats = NULL;
		num_formats = 0;
	}

	for (k = 0; xlate && (k < num_formats); k++) {
		xlate->host_fmt	= &formats[k];
		xlate->code	= code;
		xlate++;

		dev_dbg(dev, "Supporting mbus format code 0x%04x using %s\n",
			code, formats[k].name);
	}

	return num_formats;
}

static void tegra_camera_put_formats(struct soc_camera_device *icd)
{
	kfree(icd->host_priv);
	icd->host_priv = NULL;
}

static int tegra_camera_set_fmt(struct soc_camera_device *icd,
			      struct v4l2_format *f)
{
	struct device *dev = icd->parent;
	struct soc_camera_host *ici = to_soc_camera_host(dev);
	struct tegra_camera *cam = ici->priv;
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	const struct soc_camera_format_xlate *xlate = NULL;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct v4l2_mbus_framefmt mf;
	int ret = 0;

	xlate = soc_camera_xlate_by_fourcc(icd, pix->pixelformat);
	if (!xlate) {
		dev_err(dev, "Format %x not found\n", pix->pixelformat);
		return -EINVAL;
	}

	mf.width	= pix->width;
	mf.height	= pix->height;
	mf.field	= pix->field;
	mf.colorspace	= pix->colorspace;
	mf.code		= xlate->code;

	if (!cam->ops->ignore_subdev_fmt || !cam->ops->ignore_subdev_fmt(cam)) {
		ret = v4l2_subdev_call(sd, video, s_mbus_fmt, &mf);
		if (IS_ERR_VALUE(ret)) {
			dev_err(dev, "Failed to configure for format %x\n",
				pix->pixelformat);
			return ret;
		}

		if (cam->ops->s_mbus_fmt) {
			ret = cam->ops->s_mbus_fmt(sd, &mf);
			if (ret < 0) {
				dev_err(dev, "Camera host failed to configure "
					"for format %x\n", pix->pixelformat);
				return ret;
			}
		}

		if (mf.code != xlate->code) {
			dev_err(dev,
				"mf.code = 0x%04x, xlate->code = 0x%04x, "
				"mismatch\n", mf.code, xlate->code);
			return -EINVAL;
		}
	}

	icd->user_width		= mf.width;
	icd->user_height	= mf.height;
	icd->current_fmt	= xlate;

	return ret;
}

static int tegra_camera_try_fmt(struct soc_camera_device *icd,
				struct v4l2_format *f)
{
	struct device *dev = icd->parent;
	struct soc_camera_host *ici = to_soc_camera_host(dev);
	struct tegra_camera *cam = ici->priv;
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	const struct soc_camera_format_xlate *xlate;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct v4l2_mbus_framefmt mf;
	__u32 pixfmt = pix->pixelformat;
	int ret = 0;
	int bytesperline = 0;

	xlate = soc_camera_xlate_by_fourcc(icd, pixfmt);
	if (!xlate) {
		dev_warn(icd->parent, "Format %x not found\n", pixfmt);
		return -EINVAL;
	}

	bytesperline = cam->ops->bytes_per_line(pix->width,
					xlate->host_fmt);
	if (bytesperline < 0)
		return bytesperline;
	else
		pix->bytesperline = bytesperline;

	pix->sizeimage = pix->height * pix->bytesperline;

	/* limit to sensor capabilities */
	mf.width	= pix->width;
	mf.height	= pix->height;
	mf.field	= pix->field;
	mf.colorspace	= pix->colorspace;
	mf.code		= xlate->code;

	if (!cam->ops->ignore_subdev_fmt || !cam->ops->ignore_subdev_fmt(cam)) {
		ret = v4l2_subdev_call(sd, video, try_mbus_fmt, &mf);
		if (IS_ERR_VALUE(ret))
			return ret;
	}

	if (cam->ops->try_mbus_fmt) {
		ret = cam->ops->try_mbus_fmt(sd, &mf);
		if (ret < 0) {
			dev_warn(icd->parent, "Camera host rejected configure "
				 "for format %x\n", pix->pixelformat);
			return ret;
		}
	}

	pix->width	= mf.width;
	pix->height	= mf.height;
	pix->colorspace	= mf.colorspace;
	/*
	 * width and height could have been changed, therefore update the
	 * bytesperline and sizeimage here.
	 */
	pix->bytesperline = cam->ops->bytes_per_line(pix->width,
						    xlate->host_fmt);
	pix->sizeimage = pix->height * pix->bytesperline;

	switch (mf.field) {
	case V4L2_FIELD_ANY:
	case V4L2_FIELD_NONE:
		pix->field	= V4L2_FIELD_NONE;
		break;
	default:
		/* TODO: support interlaced at least in pass-through mode */
		dev_err(icd->parent, "Field type %d unsupported.\n",
			mf.field);
		return -EINVAL;
	}

	return ret;
}

static int tegra_camera_init_videobuf(struct vb2_queue *q,
				      struct soc_camera_device *icd)
{
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	q->io_modes = VB2_MMAP | VB2_USERPTR;
	q->drv_priv = icd;
	q->ops = &tegra_camera_videobuf_ops;
	q->mem_ops = &vb2_dma_contig_memops;
	q->buf_struct_size = sizeof(struct tegra_camera_buffer);
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;

	return vb2_queue_init(q);
}

static int tegra_camera_reqbufs(struct soc_camera_device *icd,
				struct v4l2_requestbuffers *p)
{
	return 0;
}

static int tegra_camera_querycap(struct soc_camera_host *ici,
				 struct v4l2_capability *cap)
{
	struct tegra_camera *cam = ici->priv;

	strlcpy(cap->card, cam->card, sizeof(cap->card));
	cap->version = cam->version;
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;

	return 0;
}

static int tegra_camera_set_bus_param(struct soc_camera_device *icd)
{
	return 0;
}

static unsigned int tegra_camera_poll(struct file *file, poll_table *pt)
{
	struct soc_camera_device *icd = file->private_data;

	return vb2_poll(&icd->vb2_vidq, file, pt);
}

static struct soc_camera_host_ops tegra_soc_camera_host_ops = {
	.owner		= THIS_MODULE,
	.add		= tegra_camera_add_device,
	.remove		= tegra_camera_remove_device,
	.clock_start	= soc_camera_clock_start,
	.clock_stop	= soc_camera_clock_stop,
	.get_formats	= tegra_camera_get_formats,
	.put_formats	= tegra_camera_put_formats,
	.set_fmt	= tegra_camera_set_fmt,
	.try_fmt	= tegra_camera_try_fmt,
	.init_videobuf2	= tegra_camera_init_videobuf,
	.reqbufs	= tegra_camera_reqbufs,
	.querycap	= tegra_camera_querycap,
	.set_bus_param	= tegra_camera_set_bus_param,
	.poll		= tegra_camera_poll,
};

int tegra_camera_init(struct platform_device *pdev, struct tegra_camera *cam)
{
	int err;

	cam->ici.priv = cam;
	cam->ici.v4l2_dev.dev = &pdev->dev;

	/* This is a WAR that by passes of_node check
	 * in V4L2 framework. There is plan to move vi
	 * driver out of soc_camera and directly adding it
	 * to v4l2 subdev, so this WAR will be removed then
	 */
	cam->ici.v4l2_dev.dev->of_node = NULL;

	cam->ici.nr = pdev->id;
	cam->ici.drv_name = dev_name(&pdev->dev);
	cam->ici.ops = &tegra_soc_camera_host_ops;

	cam->alloc_ctx = vb2_dma_contig_init_ctx(&pdev->dev);
	if (IS_ERR(cam->alloc_ctx))
		return PTR_ERR(cam->alloc_ctx);

	err = soc_camera_host_register(&cam->ici);
	if (IS_ERR_VALUE(err))
		goto exit_cleanup_alloc_ctx;

	return 0;

exit_cleanup_alloc_ctx:
	vb2_dma_contig_cleanup_ctx(cam->alloc_ctx);
	return err;
}
EXPORT_SYMBOL(tegra_camera_init);

void tegra_camera_deinit(struct platform_device *pdev, struct tegra_camera *cam)
{
	soc_camera_host_unregister(&cam->ici);

	vb2_dma_contig_cleanup_ctx(cam->alloc_ctx);
}
EXPORT_SYMBOL(tegra_camera_deinit);
