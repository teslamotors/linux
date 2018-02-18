/*
 * NVIDIA Tegra Video Input Device
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Bryan Wu <pengw@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/atomic.h>
#include <linux/bitmap.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/nvhost.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/lcm.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-contig.h>
#include <media/camera_common.h>

#include <mach/clk.h>
#include <mach/io_dpd.h>

#include "mc_common.h"

void tegra_channel_fmts_bitmap_init(struct tegra_channel *chan,
				    struct tegra_vi_graph_entity *entity)
{
	int ret, index;
	struct v4l2_subdev *subdev = entity->subdev;
	struct v4l2_subdev_mbus_code_enum code = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
	};

	bitmap_zero(chan->fmts_bitmap, MAX_FORMAT_NUM);

	while (1) {
		ret = v4l2_subdev_call(subdev, pad, enum_mbus_code,
				       NULL, &code);
		if (ret < 0)
			/* no more formats */
			break;

		index = tegra_core_get_idx_by_code(code.code);
		if (index >= 0)
			bitmap_set(chan->fmts_bitmap, index, 1);

		code.index++;
	}

	if (0 == bitmap_weight(chan->fmts_bitmap, MAX_FORMAT_NUM)) {
		index = tegra_core_get_idx_by_code(V4L2_MBUS_FMT_SRGGB10_1X10);
		if (index >= 0)
			bitmap_set(chan->fmts_bitmap, index, 1);
	}
}

/* -----------------------------------------------------------------------------
 * videobuf2 queue operations
 */

static int
tegra_channel_queue_setup(struct vb2_queue *vq, const struct v4l2_format *fmt,
		     unsigned int *nbuffers, unsigned int *nplanes,
		     unsigned int sizes[], void *alloc_ctxs[])
{
	struct tegra_channel *chan = vb2_get_drv_priv(vq);

	/* Make sure the image size is large enough. */
	if (fmt && fmt->fmt.pix.sizeimage < chan->format.sizeimage)
		return -EINVAL;

	*nplanes = 1;

	sizes[0] = fmt ? fmt->fmt.pix.sizeimage : chan->format.sizeimage;
	alloc_ctxs[0] = chan->alloc_ctx;

	if (!*nbuffers)
		*nbuffers = 2;

	return 0;
}

static int tegra_channel_buffer_prepare(struct vb2_buffer *vb)
{
	struct tegra_channel *chan = vb2_get_drv_priv(vb->vb2_queue);
	struct tegra_channel_buffer *buf = to_tegra_channel_buffer(vb);

	buf->chan = chan;
	vb2_set_plane_payload(vb, 0, chan->format.sizeimage);
#if defined(CONFIG_VIDEOBUF2_DMA_CONTIG)
	buf->addr = vb2_dma_contig_plane_dma_addr(vb, 0);
#endif

	return 0;
}

static void tegra_channel_buffer_queue(struct vb2_buffer *vb)
{
	/* for bypass mode - do nothing */
}

static int tegra_channel_set_stream(struct tegra_channel *chan, bool on)
{
	int num_sd = 0;
	int ret = 0;
	struct v4l2_subdev *subdev = chan->subdev[num_sd];

	while (subdev != NULL) {
		ret = v4l2_subdev_call(subdev, video, s_stream, on);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return ret;

		num_sd++;
		if (num_sd >= chan->num_subdevs)
			break;

		subdev = chan->subdev[num_sd];
	}

	return 0;
}

static int tegra_channel_set_power(struct tegra_channel *chan, bool on)
{
	int num_sd = 0;
	int ret = 0;
	struct v4l2_subdev *subdev = chan->subdev[num_sd];

	while (subdev != NULL) {
		ret = v4l2_subdev_call(subdev, core, s_power, on);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return ret;

		num_sd++;
		if (num_sd >= chan->num_subdevs)
			break;

		subdev = chan->subdev[num_sd];
	}

	return 0;
}


static int tegra_channel_start_streaming(struct vb2_queue *vq, u32 count)
{
	struct tegra_channel *chan = vb2_get_drv_priv(vq);
	struct media_pipeline *pipe = chan->video.entity.pipe;
	int ret = 0;

	ret = media_entity_pipeline_start(&chan->video.entity, pipe);
	if (ret < 0)
		goto error_pipeline_start;

	/* Start the pipeline. */
	ret = tegra_channel_set_stream(chan, true);
	if (ret < 0)
		goto error_set_stream;

	return 0;
error_set_stream:
	media_entity_pipeline_stop(&chan->video.entity);
error_pipeline_start:
	vq->start_streaming_called = 0;

	return ret;
}

static void tegra_channel_stop_streaming(struct vb2_queue *vq)
{
	struct tegra_channel *chan = vb2_get_drv_priv(vq);

	media_entity_pipeline_stop(&chan->video.entity);

	tegra_channel_set_stream(chan, false);
}

static const struct vb2_ops tegra_channel_queue_qops = {
	.queue_setup = tegra_channel_queue_setup,
	.buf_prepare = tegra_channel_buffer_prepare,
	.buf_queue = tegra_channel_buffer_queue,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.start_streaming = tegra_channel_start_streaming,
	.stop_streaming = tegra_channel_stop_streaming,
};

/* -----------------------------------------------------------------------------
 * V4L2 ioctls
 */

static int
tegra_channel_querycap(struct file *file, void *fh, struct v4l2_capability *cap)
{
	struct v4l2_fh *vfh = file->private_data;
	struct tegra_channel *chan = to_tegra_channel(vfh->vdev);

	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	strlcpy(cap->driver, "tegra-video", sizeof(cap->driver));
	strlcpy(cap->card, chan->video.name, sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s:%u",
		 dev_name(chan->vi->dev), chan->port);

	return 0;
}

static int
tegra_channel_enum_format(struct file *file, void *fh, struct v4l2_fmtdesc *f)
{
	struct v4l2_fh *vfh = file->private_data;
	struct tegra_channel *chan = to_tegra_channel(vfh->vdev);
	unsigned int index = 0, i;
	unsigned long *fmts_bitmap = NULL;

	if (chan->vi->pg_mode)
		fmts_bitmap = chan->vi->tpg_fmts_bitmap;
	else
		fmts_bitmap = chan->fmts_bitmap;

	if (f->index >= bitmap_weight(fmts_bitmap, MAX_FORMAT_NUM))
		return -EINVAL;

	for (i = 0; i < f->index + 1; i++, index++)
		index = find_next_bit(fmts_bitmap, MAX_FORMAT_NUM, index);

	index -= 1;
	f->pixelformat = tegra_core_get_fourcc_by_idx(index);

	return 0;
}

static void tegra_channel_fmt_align(struct v4l2_pix_format *pix,
			unsigned int channel_align, unsigned int bpp)
{
	unsigned int min_width;
	unsigned int max_width;
	unsigned int min_bpl;
	unsigned int max_bpl;
	unsigned int width;
	unsigned int align;
	unsigned int bpl;

	/* The transfer alignment requirements are expressed in bytes. Compute
	 * the minimum and maximum values, clamp the requested width and convert
	 * it back to pixels.
	 */
	align = lcm(channel_align, bpp);
	min_width = roundup(TEGRA_MIN_WIDTH, align);
	max_width = rounddown(TEGRA_MAX_WIDTH, align);
	width = roundup(pix->width * bpp, align);

	pix->width = clamp(width, min_width, max_width) / bpp;
	pix->height = clamp(pix->height, TEGRA_MIN_HEIGHT, TEGRA_MAX_HEIGHT);

	/* Clamp the requested bytes per line value. If the maximum bytes per
	 * line value is zero, the module doesn't support user configurable line
	 * sizes. Override the requested value with the minimum in that case.
	 */
	min_bpl = pix->width * bpp;
	max_bpl = rounddown(TEGRA_MAX_WIDTH, channel_align);
	bpl = roundup(pix->bytesperline, channel_align);

	pix->bytesperline = clamp(bpl, min_bpl, max_bpl);
	pix->sizeimage = pix->bytesperline * pix->height;
}

static int tegra_channel_get_subdevices(struct tegra_channel *chan)
{
	struct media_entity *entity;
	struct media_pad *pad;
	int index = 0;
	int num_sd = 0;

	/* set_stream of CSI */
	entity = &chan->video.entity;
	pad = media_entity_remote_pad(&chan->pad);
	if (!pad)
		return -ENODEV;

	entity = pad->entity;
	chan->subdev[num_sd++] = media_entity_to_v4l2_subdev(entity);

	index = pad->index - 1;
	while (index >= 0) {
		pad = &entity->pads[index];
		if (!(pad->flags & MEDIA_PAD_FL_SINK))
			break;

		pad = media_entity_remote_pad(pad);
		if (pad == NULL ||
		    media_entity_type(pad->entity) != MEDIA_ENT_T_V4L2_SUBDEV)
			break;

		if (num_sd >= MAX_SUBDEVICES)
			break;

		entity = pad->entity;
		chan->subdev[num_sd++] = media_entity_to_v4l2_subdev(entity);

		index = pad->index - 1;
	}
	chan->num_subdevs = num_sd;

	return 0;
}

static int
tegra_channel_get_format(struct file *file, void *fh,
			struct v4l2_format *format)
{
	struct v4l2_fh *vfh = file->private_data;
	struct tegra_channel *chan = to_tegra_channel(vfh->vdev);
	struct v4l2_mbus_framefmt mf;
	struct v4l2_pix_format *pix = &format->fmt.pix;
	int ret = 0;
	int num_sd = 0;
	struct v4l2_subdev *sd = chan->subdev[num_sd];

	while (sd != NULL) {
		ret = v4l2_subdev_call(sd, video, g_mbus_fmt, &mf);
		if (IS_ERR_VALUE(ret)) {
			if (ret == -ENOIOCTLCMD) {
				num_sd++;
				if (num_sd < chan->num_subdevs) {
					sd = chan->subdev[num_sd];
					continue;
				} else
					break;
			} else
				return ret;
		}

		pix->width = mf.width;
		pix->height = mf.height;
		pix->field = mf.field;
		pix->colorspace = mf.colorspace;

		return ret;
	}

	return -ENODEV;
}

static int
__tegra_channel_try_format(struct tegra_channel *chan,
			struct v4l2_pix_format *pix,
			const struct tegra_video_format **fmtinfo)
{
	const struct tegra_video_format *info;
	struct v4l2_mbus_framefmt mf;
	int ret = 0;
	int num_sd = 0;
	struct v4l2_subdev *sd = chan->subdev[num_sd];

	/* Retrieve format information and select the default format if the
	 * requested format isn't supported.
	 */
	info = tegra_core_get_format_by_fourcc(pix->pixelformat);
	if (!info)
		info = tegra_core_get_format_by_code(TEGRA_VF_DEF);

	pix->pixelformat = info->fourcc;
	/* Change this when start adding interlace format support */
	pix->field = V4L2_FIELD_NONE;
	pix->colorspace = chan->format.colorspace;

	tegra_channel_fmt_align(pix, chan->align, info->bpp);

	/* verify with subdevice the format supported */
	mf.width = pix->width;
	mf.height = pix->height;
	mf.field = pix->field;
	mf.colorspace = pix->colorspace;
	mf.code = info->code;

	while (sd != NULL) {
		ret = v4l2_subdev_call(sd, video, try_mbus_fmt, &mf);
		if (IS_ERR_VALUE(ret)) {
			if (ret == -ENOIOCTLCMD) {
				num_sd++;
				if (num_sd < chan->num_subdevs) {
					sd = chan->subdev[num_sd];
					continue;
				} else
					break;
			} else
				return ret;
		}

		pix->width = mf.width;
		pix->height = mf.height;
		pix->colorspace = mf.colorspace;

		tegra_channel_fmt_align(pix, chan->align, info->bpp);
		if (fmtinfo) {
			*fmtinfo = info;
			ret = v4l2_subdev_call(sd, video, s_mbus_fmt, &mf);
		}

		return ret;
	}

	return -ENODEV;
}

static int
tegra_channel_try_format(struct file *file, void *fh,
			struct v4l2_format *format)
{
	struct v4l2_fh *vfh = file->private_data;
	struct tegra_channel *chan = to_tegra_channel(vfh->vdev);
	int ret = 0;

	ret = __tegra_channel_try_format(chan, &format->fmt.pix, NULL);

	return ret;
}

static int
tegra_channel_set_format(struct file *file, void *fh,
			struct v4l2_format *format)
{
	struct v4l2_fh *vfh = file->private_data;
	struct tegra_channel *chan = to_tegra_channel(vfh->vdev);
	const struct tegra_video_format *info = NULL;
	int ret = 0;

	if (vb2_is_busy(&chan->queue))
		return -EBUSY;

	ret = __tegra_channel_try_format(chan, &format->fmt.pix, &info);

	chan->format = format->fmt.pix;
	chan->fmtinfo = info;

	return ret;
}

static const struct v4l2_ioctl_ops tegra_channel_ioctl_ops = {
	.vidioc_querycap		= tegra_channel_querycap,
	.vidioc_enum_fmt_vid_cap	= tegra_channel_enum_format,
	.vidioc_g_fmt_vid_cap		= tegra_channel_get_format,
	.vidioc_s_fmt_vid_cap		= tegra_channel_set_format,
	.vidioc_try_fmt_vid_cap		= tegra_channel_try_format,
	.vidioc_reqbufs			= vb2_ioctl_reqbufs,
	.vidioc_querybuf		= vb2_ioctl_querybuf,
	.vidioc_qbuf			= vb2_ioctl_qbuf,
	.vidioc_dqbuf			= vb2_ioctl_dqbuf,
	.vidioc_create_bufs		= vb2_ioctl_create_bufs,
	.vidioc_expbuf			= vb2_ioctl_expbuf,
	.vidioc_streamon		= vb2_ioctl_streamon,
	.vidioc_streamoff		= vb2_ioctl_streamoff,
};

static int tegra_channel_open(struct file *fp)
{
	int ret = 0;
	struct video_device *vdev = video_devdata(fp);
	struct tegra_channel *chan = video_get_drvdata(vdev);
	int num_sd = 0;
	struct v4l2_subdev *sd = chan->subdev[num_sd];

	if (sd == NULL) {
		ret = tegra_channel_get_subdevices(chan);
		if (ret < 0)
			return ret;

		/* Initialize the subdev and controls here at first open */
		sd = chan->subdev[num_sd];
		while (sd != NULL) {
			/* Add control handler for the subdevice */
			ret = v4l2_ctrl_add_handler(&chan->ctrl_handler,
						sd->ctrl_handler, NULL);
			if (chan->ctrl_handler.error)
				dev_err(chan->vi->dev,
					"Failed to add controls\n");

			ret = v4l2_ctrl_handler_setup(&chan->ctrl_handler);
			if (ret < 0)
				dev_err(chan->vi->dev,
					"Failed to setup controls\n");

			num_sd++;
			if (num_sd >= chan->num_subdevs)
				break;
			sd = chan->subdev[num_sd];
		}
	}

	/* power on sensors connected in channel */
	ret = tegra_channel_set_power(chan, 1);
	if (ret < 0)
		return ret;

	return v4l2_fh_open(fp);
}

static int tegra_channel_close(struct file *fp)
{
	int ret = 0;
	struct video_device *vdev = video_devdata(fp);
	struct tegra_channel *chan = video_get_drvdata(vdev);

	/* power off sensors connected in channel */
	ret = tegra_channel_set_power(chan, 0);
	if (ret < 0)
		dev_err(chan->vi->dev, "Failed to power off subdevices\n");

	return vb2_fop_release(fp);
}

/* -----------------------------------------------------------------------------
 * V4L2 file operations
 */
static const struct v4l2_file_operations tegra_channel_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= video_ioctl2,
	.open		= tegra_channel_open,
	.release	= tegra_channel_close,
	.read		= vb2_fop_read,
	.poll		= vb2_fop_poll,
	.mmap		= vb2_fop_mmap,
};

static int tegra_channel_init(struct tegra_mc_vi *vi, unsigned int port)
{
	int ret;
	struct tegra_channel *chan  = &vi->chans[port];

	chan->vi = vi;
	chan->port = port;
	chan->align = 64;
	chan->num_subdevs = 0;

	/* Init video format */
	chan->fmtinfo = tegra_core_get_format_by_code(TEGRA_VF_DEF);
	chan->format.pixelformat = chan->fmtinfo->fourcc;
	chan->format.colorspace = V4L2_COLORSPACE_SRGB;
	chan->format.field = V4L2_FIELD_NONE;
	chan->format.width = TEGRA_DEF_WIDTH;
	chan->format.height = TEGRA_DEF_HEIGHT;
	chan->format.bytesperline = chan->format.width * chan->fmtinfo->bpp;
	chan->format.sizeimage = chan->format.bytesperline *
				    chan->format.height;

	/* Initialize the media entity... */
	chan->pad.flags = MEDIA_PAD_FL_SINK;

	ret = media_entity_init(&chan->video.entity, 1, &chan->pad, 0);
	if (ret < 0)
		return ret;

	/* init control handler */
	ret = v4l2_ctrl_handler_init(&chan->ctrl_handler, MAX_CID_CONTROLS);
	if (chan->ctrl_handler.error) {
		dev_err(&chan->video.dev, "failed to init control handler\n");
		goto video_register_error;
	}

	/* init video node... */
	chan->video.fops = &tegra_channel_fops;
	chan->video.v4l2_dev = &vi->v4l2_dev;
	chan->video.queue = &chan->queue;
	snprintf(chan->video.name, sizeof(chan->video.name), "%s-%s-%u",
		 dev_name(vi->dev), "output", port);
	chan->video.vfl_type = VFL_TYPE_GRABBER;
	chan->video.vfl_dir = VFL_DIR_RX;
	chan->video.release = video_device_release_empty;
	chan->video.ioctl_ops = &tegra_channel_ioctl_ops;
	chan->video.ctrl_handler = &chan->ctrl_handler;

	video_set_drvdata(&chan->video, chan);

#if defined(CONFIG_VIDEOBUF2_DMA_CONTIG)
	/* get the buffers queue... */
	chan->alloc_ctx = vb2_dma_contig_init_ctx(&chan->video.dev);
	if (IS_ERR(chan->alloc_ctx)) {
		dev_err(chan->vi->dev, "failed to init vb2 buffer\n");
		ret = -ENOMEM;
		goto vb2_init_error;
	}
#endif

	chan->queue.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	chan->queue.io_modes = VB2_MMAP | VB2_DMABUF | VB2_READ | VB2_USERPTR;
	chan->queue.drv_priv = chan;
	chan->queue.buf_struct_size = sizeof(struct tegra_channel_buffer);
	chan->queue.ops = &tegra_channel_queue_qops;
#if defined(CONFIG_VIDEOBUF2_DMA_CONTIG)
	chan->queue.mem_ops = &vb2_dma_contig_memops;
#endif
	chan->queue.timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC
				   | V4L2_BUF_FLAG_TSTAMP_SRC_EOF;
	ret = vb2_queue_init(&chan->queue);
	if (ret < 0) {
		dev_err(chan->vi->dev, "failed to initialize VB2 queue\n");
		goto vb2_queue_error;
	}

	ret = video_register_device(&chan->video, VFL_TYPE_GRABBER, -1);
	if (ret < 0) {
		dev_err(&chan->video.dev, "failed to register video device\n");
		goto video_register_error;
	}

	return 0;

video_register_error:
	vb2_queue_release(&chan->queue);
vb2_queue_error:
#if defined(CONFIG_VIDEOBUF2_DMA_CONTIG)
	vb2_dma_contig_cleanup_ctx(chan->alloc_ctx);
vb2_init_error:
#endif
	media_entity_cleanup(&chan->video.entity);
	return ret;
}

static int tegra_channel_cleanup(struct tegra_channel *chan)
{
	video_unregister_device(&chan->video);

	v4l2_ctrl_handler_free(&chan->ctrl_handler);
	vb2_queue_release(&chan->queue);
#if defined(CONFIG_VIDEOBUF2_DMA_CONTIG)
	vb2_dma_contig_cleanup_ctx(chan->alloc_ctx);
#endif

	media_entity_cleanup(&chan->video.entity);

	return 0;
}

int tegra_vi_channels_init(struct tegra_mc_vi *vi)
{
	unsigned int i;
	int ret;

	for (i = 0; i < vi->num_channels; i++) {
		ret = tegra_channel_init(vi, i);
		if (ret < 0) {
			dev_err(vi->dev, "channel %d init failed\n", i);
			return ret;
		}
	}
	return 0;
}

int tegra_vi_channels_cleanup(struct tegra_mc_vi *vi)
{
	unsigned int i;
	int ret;

	for (i = 0; i < vi->num_channels; i++) {
		ret = tegra_channel_cleanup(&vi->chans[i]);
		if (ret < 0) {
			dev_err(vi->dev, "channel %d cleanup failed\n", i);
			return ret;
		}
	}
	return 0;
}

