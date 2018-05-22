// SPDX-License_Identifier: GPL-2.0
// Copyright (C) 2013 - 2018 Intel Corporation

#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/init_task.h>
#include <linux/kthread.h>
#include <linux/pm_runtime.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/compat.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
#include <linux/sched.h>
#else
#include <uapi/linux/sched/types.h>
#endif

#include <media/media-entity.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0)
#include <media/v4l2-mc.h>
#endif

#include "ipu.h"
#include "ipu-bus.h"
#include "ipu-cpd.h"
#include "ipu-isys.h"
#include "ipu-isys-video.h"
#include "ipu-platform.h"
#include "ipu-platform-regs.h"
#include "ipu-platform-buttress-regs.h"
#include "ipu-trace.h"
#include "ipu-fw-isys.h"
#include "ipu-fw-com.h"

static unsigned int num_stream_support = IPU_ISYS_NUM_STREAMS;
module_param(num_stream_support, uint, 0660);
MODULE_PARM_DESC(num_stream_support, "IPU project support number of stream");

static bool use_stream_stop;
module_param(use_stream_stop, bool, 0660);
MODULE_PARM_DESC(use_stream_stop, "Use STOP command if running in CSI capture mode");

const struct ipu_isys_pixelformat ipu_isys_pfmts_be_soc[] = {
	{V4L2_PIX_FMT_Y10, 16, 10, 0, MEDIA_BUS_FMT_Y10_1X10,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_UYVY, 16, 16, 0, MEDIA_BUS_FMT_UYVY8_1X16,
	 IPU_FW_ISYS_FRAME_FORMAT_UYVY},
	{V4L2_PIX_FMT_YUYV, 16, 16, 0, MEDIA_BUS_FMT_YUYV8_1X16,
	 IPU_FW_ISYS_FRAME_FORMAT_YUYV},
	{V4L2_PIX_FMT_NV16, 16, 16, 8, MEDIA_BUS_FMT_YUYV8_1X16,
	 IPU_FW_ISYS_FRAME_FORMAT_NV16},
	{V4L2_PIX_FMT_XRGB32, 32, 32, 0, MEDIA_BUS_FMT_RGB565_1X16,
	 IPU_FW_ISYS_FRAME_FORMAT_RGBA888},
	{V4L2_PIX_FMT_XBGR32, 32, 32, 0, MEDIA_BUS_FMT_RGB888_1X24,
	 IPU_FW_ISYS_FRAME_FORMAT_RGBA888},
	/* Raw bayer formats. */
	{V4L2_PIX_FMT_SBGGR14, 16, 14, 0, MEDIA_BUS_FMT_SBGGR14_1X14,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SGBRG14, 16, 14, 0, MEDIA_BUS_FMT_SGBRG14_1X14,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SGRBG14, 16, 14, 0, MEDIA_BUS_FMT_SGRBG14_1X14,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SRGGB14, 16, 14, 0, MEDIA_BUS_FMT_SRGGB14_1X14,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SBGGR12, 16, 12, 0, MEDIA_BUS_FMT_SBGGR12_1X12,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SGBRG12, 16, 12, 0, MEDIA_BUS_FMT_SGBRG12_1X12,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SGRBG12, 16, 12, 0, MEDIA_BUS_FMT_SGRBG12_1X12,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SRGGB12, 16, 12, 0, MEDIA_BUS_FMT_SRGGB12_1X12,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SBGGR10, 16, 10, 0, MEDIA_BUS_FMT_SBGGR10_1X10,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SGBRG10, 16, 10, 0, MEDIA_BUS_FMT_SGBRG10_1X10,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SGRBG10, 16, 10, 0, MEDIA_BUS_FMT_SGRBG10_1X10,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SRGGB10, 16, 10, 0, MEDIA_BUS_FMT_SRGGB10_1X10,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SBGGR8, 8, 8, 0, MEDIA_BUS_FMT_SBGGR8_1X8,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW8},
	{V4L2_PIX_FMT_SGBRG8, 8, 8, 0, MEDIA_BUS_FMT_SGBRG8_1X8,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW8},
	{V4L2_PIX_FMT_SGRBG8, 8, 8, 0, MEDIA_BUS_FMT_SGRBG8_1X8,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW8},
	{V4L2_PIX_FMT_SRGGB8, 8, 8, 0, MEDIA_BUS_FMT_SRGGB8_1X8,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW8},
	{}
};

const struct ipu_isys_pixelformat ipu_isys_pfmts_packed[] = {
	{V4L2_PIX_FMT_Y10, 10, 10, 0, MEDIA_BUS_FMT_Y10_1X10,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW10},
	{V4L2_PIX_FMT_UYVY, 16, 16, 0, MEDIA_BUS_FMT_UYVY8_1X16,
	 IPU_FW_ISYS_FRAME_FORMAT_UYVY},
	{V4L2_PIX_FMT_YUYV, 16, 16, 0, MEDIA_BUS_FMT_YUYV8_1X16,
	 IPU_FW_ISYS_FRAME_FORMAT_YUYV},
	{V4L2_PIX_FMT_RGB565, 16, 16, 0, MEDIA_BUS_FMT_RGB565_1X16,
	 IPU_FW_ISYS_FRAME_FORMAT_RGB565},
	{V4L2_PIX_FMT_BGR24, 24, 24, 0, MEDIA_BUS_FMT_RGB888_1X24,
	 IPU_FW_ISYS_FRAME_FORMAT_RGBA888},
#ifndef V4L2_PIX_FMT_SBGGR12P
	{V4L2_PIX_FMT_SBGGR12, 12, 12, 0, MEDIA_BUS_FMT_SBGGR12_1X12,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW12},
	{V4L2_PIX_FMT_SGBRG12, 12, 12, 0, MEDIA_BUS_FMT_SGBRG12_1X12,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW12},
	{V4L2_PIX_FMT_SGRBG12, 12, 12, 0, MEDIA_BUS_FMT_SGRBG12_1X12,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW12},
	{V4L2_PIX_FMT_SRGGB12, 12, 12, 0, MEDIA_BUS_FMT_SRGGB12_1X12,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW12},
	{V4L2_PIX_FMT_SBGGR14, 14, 14, 0, MEDIA_BUS_FMT_SBGGR14_1X14,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW14},
	{V4L2_PIX_FMT_SGBRG14, 14, 14, 0, MEDIA_BUS_FMT_SGBRG14_1X14,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW14},
	{V4L2_PIX_FMT_SGRBG14, 14, 14, 0, MEDIA_BUS_FMT_SGRBG14_1X14,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW14},
	{V4L2_PIX_FMT_SRGGB14, 14, 14, 0, MEDIA_BUS_FMT_SRGGB14_1X14,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW14},
#else /* V4L2_PIX_FMT_SBGGR12P */
	{V4L2_PIX_FMT_SBGGR12P, 12, 12, 0, MEDIA_BUS_FMT_SBGGR12_1X12,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW12},
	{V4L2_PIX_FMT_SGBRG12P, 12, 12, 0, MEDIA_BUS_FMT_SGBRG12_1X12,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW12},
	{V4L2_PIX_FMT_SGRBG12P, 12, 12, 0, MEDIA_BUS_FMT_SGRBG12_1X12,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW12},
	{V4L2_PIX_FMT_SRGGB12P, 12, 12, 0, MEDIA_BUS_FMT_SRGGB12_1X12,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW12},
	{V4L2_PIX_FMT_SBGGR14P, 14, 14, 0, MEDIA_BUS_FMT_SBGGR14_1X14,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW14},
	{V4L2_PIX_FMT_SGBRG14P, 14, 14, 0, MEDIA_BUS_FMT_SGBRG14_1X14,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW14},
	{V4L2_PIX_FMT_SGRBG14P, 14, 14, 0, MEDIA_BUS_FMT_SGRBG14_1X14,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW14},
	{V4L2_PIX_FMT_SRGGB14P, 14, 14, 0, MEDIA_BUS_FMT_SRGGB14_1X14,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW14},
#endif /* V4L2_PIX_FMT_SBGGR12P */
	{V4L2_PIX_FMT_SBGGR10P, 10, 10, 0, MEDIA_BUS_FMT_SBGGR10_1X10,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW10},
	{V4L2_PIX_FMT_SGBRG10P, 10, 10, 0, MEDIA_BUS_FMT_SGBRG10_1X10,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW10},
	{V4L2_PIX_FMT_SGRBG10P, 10, 10, 0, MEDIA_BUS_FMT_SGRBG10_1X10,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW10},
	{V4L2_PIX_FMT_SRGGB10P, 10, 10, 0, MEDIA_BUS_FMT_SRGGB10_1X10,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW10},
	{V4L2_PIX_FMT_SBGGR8, 8, 8, 0, MEDIA_BUS_FMT_SBGGR8_1X8,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW8},
	{V4L2_PIX_FMT_SGBRG8, 8, 8, 0, MEDIA_BUS_FMT_SGBRG8_1X8,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW8},
	{V4L2_PIX_FMT_SGRBG8, 8, 8, 0, MEDIA_BUS_FMT_SGRBG8_1X8,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW8},
	{V4L2_PIX_FMT_SRGGB8, 8, 8, 0, MEDIA_BUS_FMT_SRGGB8_1X8,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW8},
	{}
};

static int video_open(struct file *file)
{
	struct ipu_isys_video *av = video_drvdata(file);
	struct ipu_isys *isys = av->isys;
	struct ipu_bus_device *adev = to_ipu_bus_device(&isys->adev->dev);
	struct ipu_device *isp = adev->isp;
	int rval;

	mutex_lock(&isys->mutex);

	if (isys->reset_needed || isp->flr_done) {
		mutex_unlock(&isys->mutex);
		dev_warn(&isys->adev->dev, "isys power cycle required\n");
		return -EIO;
	}
	mutex_unlock(&isys->mutex);

	rval = ipu_buttress_authenticate(isp);
	if (rval) {
		dev_err(&isys->adev->dev, "FW authentication failed\n");
		return rval;
	}

	rval = pm_runtime_get_sync(&isys->adev->dev);
	if (rval < 0) {
		pm_runtime_put_noidle(&isys->adev->dev);
		return rval;
	}

	rval = v4l2_fh_open(file);
	if (rval)
		goto out_power_down;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
	rval = ipu_pipeline_pm_use(&av->vdev.entity, 1);
#else
	rval = v4l2_pipeline_pm_use(&av->vdev.entity, 1);
#endif
	if (rval)
		goto out_v4l2_fh_release;

	mutex_lock(&isys->mutex);

	if (isys->video_opened++) {
		/* Already open */
		mutex_unlock(&isys->mutex);
		return 0;
	}

	ipu_configure_spc(adev->isp,
			  &isys->pdata->ipdata->hw_variant,
			  IPU_CPD_PKG_DIR_ISYS_SERVER_IDX,
			  isys->pdata->base, isys->pkg_dir,
			  isys->pkg_dir_dma_addr);

	/*
	 * Buffers could have been left to wrong queue at last closure.
	 * Move them now back to empty buffer queue.
	 */
	ipu_cleanup_fw_msg_bufs(isys);

	if (isys->fwcom) {
		/*
		 * Something went wrong in previous shutdown. As we are now
		 * restarting isys we can safely delete old context.
		 */
		dev_err(&isys->adev->dev, "Clearing old context\n");
		ipu_fw_isys_cleanup(isys);
	}


	rval = ipu_fw_isys_init(av->isys, num_stream_support);
	if (rval < 0)
		goto out_lib_init;

	mutex_unlock(&isys->mutex);

	return 0;

out_lib_init:
	isys->video_opened--;
	mutex_unlock(&isys->mutex);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
	ipu_pipeline_pm_use(&av->vdev.entity, 0);
#else
	v4l2_pipeline_pm_use(&av->vdev.entity, 0);
#endif

out_v4l2_fh_release:
	v4l2_fh_release(file);
out_power_down:
	pm_runtime_put(&isys->adev->dev);

	return rval;
}

static int video_release(struct file *file)
{
	struct ipu_isys_video *av = video_drvdata(file);
	int ret = 0;

	vb2_fop_release(file);

	mutex_lock(&av->isys->mutex);

	if (!--av->isys->video_opened) {
		ipu_fw_isys_close(av->isys);
		if (av->isys->fwcom) {
			av->isys->reset_needed = true;
			ret = -EIO;
		}
	}

	mutex_unlock(&av->isys->mutex);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
	ipu_pipeline_pm_use(&av->vdev.entity, 0);
#else
	v4l2_pipeline_pm_use(&av->vdev.entity, 0);
#endif

	if (av->isys->reset_needed)
		pm_runtime_put_sync(&av->isys->adev->dev);
	else
		pm_runtime_put(&av->isys->adev->dev);

	return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
static struct media_pad *other_pad(struct media_pad *pad)
{
	struct media_link *link;

	list_for_each_entry(link, &pad->entity->links, list) {
		if ((link->flags & MEDIA_LNK_FL_LINK_TYPE)
		    != MEDIA_LNK_FL_DATA_LINK)
			continue;

		return link->source == pad ? link->sink : link->source;
	}

	WARN_ON(1);
	return NULL;
}
#endif

const struct ipu_isys_pixelformat *ipu_isys_get_pixelformat(
					struct ipu_isys_video *av,
					u32 pixelformat)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
	struct media_pad *pad =
	    av->vdev.entity.pads[0].flags & MEDIA_PAD_FL_SOURCE ?
	    av->vdev.entity.links[0].sink : av->vdev.entity.links[0].source;
#else
	struct media_pad *pad = other_pad(&av->vdev.entity.pads[0]);
#endif
	struct v4l2_subdev *sd;
	const u32 *supported_codes;
	const struct ipu_isys_pixelformat *pfmt;

	if (!pad || !pad->entity) {
		WARN_ON(1);
		return NULL;
	}

	sd = media_entity_to_v4l2_subdev(pad->entity);
	supported_codes = to_ipu_isys_subdev(sd)->supported_codes[pad->index];

	for (pfmt = av->pfmts; pfmt->bpp; pfmt++) {
		unsigned int i;

		if (pfmt->pixelformat != pixelformat)
			continue;

		for (i = 0; supported_codes[i]; i++) {
			if (pfmt->code == supported_codes[i])
				return pfmt;
		}
	}

	/* Not found. Get the default, i.e. the first defined one. */
	for (pfmt = av->pfmts; pfmt->bpp; pfmt++) {
		if (pfmt->code == *supported_codes)
			return pfmt;
	}

	WARN_ON(1);
	return NULL;
}

int ipu_isys_vidioc_querycap(struct file *file, void *fh,
			     struct v4l2_capability *cap)
{
	struct ipu_isys_video *av = video_drvdata(file);

	strlcpy(cap->driver, IPU_ISYS_NAME, sizeof(cap->driver));
	strlcpy(cap->card, av->isys->media_dev.model, sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info), "PCI:%s",
		 av->isys->media_dev.bus_info);

	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE
	    | V4L2_CAP_VIDEO_CAPTURE_MPLANE
	    | V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING
	    | V4L2_CAP_DEVICE_CAPS;

	cap->device_caps = V4L2_CAP_STREAMING;

	switch (av->aq.vbq.type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		cap->device_caps |= V4L2_CAP_VIDEO_CAPTURE;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		cap->device_caps |= V4L2_CAP_VIDEO_CAPTURE_MPLANE;
		break;
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		cap->device_caps |= V4L2_CAP_VIDEO_OUTPUT_MPLANE;
		break;
	default:
		WARN_ON(1);
	}

	return 0;
}

int ipu_isys_vidioc_enum_fmt(struct file *file, void *fh,
			     struct v4l2_fmtdesc *f)
{
	struct ipu_isys_video *av = video_drvdata(file);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
	struct media_pad *pad =
	    av->vdev.entity.pads[0].flags & MEDIA_PAD_FL_SOURCE ?
	    av->vdev.entity.links[0].sink : av->vdev.entity.links[0].source;
#else
	struct media_pad *pad = other_pad(&av->vdev.entity.pads[0]);
#endif
	struct v4l2_subdev *sd;
	const u32 *supported_codes;
	const struct ipu_isys_pixelformat *pfmt;
	u32 index;

	if (!pad || !pad->entity)
		return -EINVAL;
	sd = media_entity_to_v4l2_subdev(pad->entity);
	supported_codes = to_ipu_isys_subdev(sd)->supported_codes[pad->index];

	/* Walk the 0-terminated array for the f->index-th code. */
	for (index = f->index; *supported_codes && index;
	     index--, supported_codes++) {
	};

	if (!*supported_codes)
		return -EINVAL;

	f->flags = 0;

	/* Code found */
	for (pfmt = av->pfmts; pfmt->bpp; pfmt++)
		if (pfmt->code == *supported_codes)
			break;

	if (!pfmt->bpp) {
		dev_warn(&av->isys->adev->dev,
			 "Format not found in mapping table.");
		return -EINVAL;
	}

	f->pixelformat = pfmt->pixelformat;

	return 0;
}

static int vidioc_g_fmt_vid_cap_mplane(struct file *file, void *fh,
				       struct v4l2_format *fmt)
{
	struct ipu_isys_video *av = video_drvdata(file);

	fmt->fmt.pix_mp = av->mpix;

	return 0;
}

const struct ipu_isys_pixelformat *
ipu_isys_video_try_fmt_vid_mplane_default(struct ipu_isys_video *av,
					  struct v4l2_pix_format_mplane *mpix)
{
	return ipu_isys_video_try_fmt_vid_mplane(av, mpix, 0);
}

const struct ipu_isys_pixelformat *ipu_isys_video_try_fmt_vid_mplane(
				struct ipu_isys_video *av,
				struct v4l2_pix_format_mplane *mpix,
				int store_csi2_header)
{
	const struct ipu_isys_pixelformat *pfmt =
	    ipu_isys_get_pixelformat(av, mpix->pixelformat);

	if (!pfmt)
		return NULL;
	mpix->pixelformat = pfmt->pixelformat;
	mpix->num_planes = 1;

	mpix->width = clamp(mpix->width, IPU_ISYS_MIN_WIDTH,
			    IPU_ISYS_MAX_WIDTH);
	mpix->height = clamp(mpix->height, IPU_ISYS_MIN_HEIGHT,
			     IPU_ISYS_MAX_HEIGHT);

	if (!av->packed)
		mpix->plane_fmt[0].bytesperline =
		    mpix->width * DIV_ROUND_UP(pfmt->bpp_planar ?
					       pfmt->bpp_planar : pfmt->bpp,
					       BITS_PER_BYTE);
	else if (store_csi2_header)
		mpix->plane_fmt[0].bytesperline =
		    DIV_ROUND_UP(av->line_header_length +
				 av->line_footer_length +
				 (unsigned int)mpix->width * pfmt->bpp,
				 BITS_PER_BYTE);
	else
		mpix->plane_fmt[0].bytesperline =
		    DIV_ROUND_UP((unsigned int)mpix->width * pfmt->bpp,
				 BITS_PER_BYTE);

	mpix->plane_fmt[0].bytesperline = ALIGN(mpix->plane_fmt[0].bytesperline,
						av->isys->line_align);
	if (pfmt->bpp_planar)
		mpix->plane_fmt[0].bytesperline =
		    mpix->plane_fmt[0].bytesperline *
		    pfmt->bpp / pfmt->bpp_planar;
	/*
	 * (height + 1) * bytesperline due to a hardware issue: the DMA unit
	 * is a power of two, and a line should be transferred as few units
	 * as possible. The result is that up to line length more data than
	 * the image size may be transferred to memory after the image.
	 * Another limition is the GDA allocation unit size. For low
	 * resolution it gives a bigger number. Use larger one to avoid
	 * memory corruption.
	 */
	mpix->plane_fmt[0].sizeimage =
	    max(max(mpix->plane_fmt[0].sizeimage,
		    mpix->plane_fmt[0].bytesperline * mpix->height +
		    max(mpix->plane_fmt[0].bytesperline,
			av->isys->pdata->ipdata->isys_dma_overshoot)), 1U);

	memset(mpix->plane_fmt[0].reserved, 0,
	       sizeof(mpix->plane_fmt[0].reserved));

	if (mpix->field == V4L2_FIELD_ANY)
		mpix->field = V4L2_FIELD_NONE;
	/* Use defaults */
	mpix->colorspace = V4L2_COLORSPACE_RAW;
	mpix->ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	mpix->quantization = V4L2_QUANTIZATION_DEFAULT;
	mpix->xfer_func = V4L2_XFER_FUNC_DEFAULT;

	return pfmt;
}

static int vidioc_s_fmt_vid_cap_mplane(struct file *file, void *fh,
				       struct v4l2_format *f)
{
	struct ipu_isys_video *av = video_drvdata(file);

	if (av->aq.vbq.streaming)
		return -EBUSY;

	av->pfmt = av->try_fmt_vid_mplane(av, &f->fmt.pix_mp);
	av->mpix = f->fmt.pix_mp;

	return 0;
}

static int vidioc_try_fmt_vid_cap_mplane(struct file *file, void *fh,
					 struct v4l2_format *f)
{
	struct ipu_isys_video *av = video_drvdata(file);

	av->try_fmt_vid_mplane(av, &f->fmt.pix_mp);

	return 0;
}

static void fmt_sp_to_mp(struct v4l2_pix_format_mplane *mpix,
			 struct v4l2_pix_format *pix)
{
	mpix->width = pix->width;
	mpix->height = pix->height;
	mpix->pixelformat = pix->pixelformat;
	mpix->field = pix->field;
	mpix->num_planes = 1;
	mpix->plane_fmt[0].bytesperline = pix->bytesperline;
	mpix->plane_fmt[0].sizeimage = pix->sizeimage;
	mpix->flags = pix->flags;
}

static void fmt_mp_to_sp(struct v4l2_pix_format *pix,
			 struct v4l2_pix_format_mplane *mpix)
{
	pix->width = mpix->width;
	pix->height = mpix->height;
	pix->pixelformat = mpix->pixelformat;
	pix->field = mpix->field;
	WARN_ON(mpix->num_planes != 1);
	pix->bytesperline = mpix->plane_fmt[0].bytesperline;
	pix->sizeimage = mpix->plane_fmt[0].sizeimage;
	pix->flags = mpix->flags;
	pix->colorspace = mpix->colorspace;
	pix->ycbcr_enc = mpix->ycbcr_enc;
	pix->quantization = mpix->quantization;
	pix->xfer_func = mpix->xfer_func;
}

static int vidioc_g_fmt_vid_cap(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct ipu_isys_video *av = video_drvdata(file);

	fmt_mp_to_sp(&f->fmt.pix, &av->mpix);

	return 0;
}

static int vidioc_s_fmt_vid_cap(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct ipu_isys_video *av = video_drvdata(file);
	struct v4l2_pix_format_mplane mpix = { 0 };

	if (av->aq.vbq.streaming)
		return -EBUSY;

	fmt_sp_to_mp(&mpix, &f->fmt.pix);

	av->pfmt = av->try_fmt_vid_mplane(av, &mpix);
	av->mpix = mpix;

	fmt_mp_to_sp(&f->fmt.pix, &mpix);

	return 0;
}

static int vidioc_try_fmt_vid_cap(struct file *file, void *fh,
				  struct v4l2_format *f)
{
	struct ipu_isys_video *av = video_drvdata(file);
	struct v4l2_pix_format_mplane mpix = { 0 };

	fmt_sp_to_mp(&mpix, &f->fmt.pix);

	av->try_fmt_vid_mplane(av, &mpix);

	fmt_mp_to_sp(&f->fmt.pix, &mpix);

	return 0;
}

static int vidioc_enum_input(struct file *file, void *fh,
			     struct v4l2_input *input)
{
	if (input->index > 0)
		return -EINVAL;
	strlcpy(input->name, "camera", sizeof(input->name));
	input->type = V4L2_INPUT_TYPE_CAMERA;

	return 0;
}

static int vidioc_g_input(struct file *file, void *fh, unsigned int *input)
{
	*input = 0;

	return 0;
}

static int vidioc_s_input(struct file *file, void *fh, unsigned int input)
{
	return input == 0 ? 0 : -EINVAL;
}

/*
 * Return true if an entity directly connected to an Iunit entity is
 * an image source for the ISP. This can be any external directly
 * connected entity or any of the test pattern generators in the
 * Iunit.
 */
static bool is_external(struct ipu_isys_video *av, struct media_entity *entity)
{
	struct v4l2_subdev *sd;
	unsigned int i;

	/* All video nodes are ours. */
	if (!is_media_entity_v4l2_subdev(entity))
		return false;

	sd = media_entity_to_v4l2_subdev(entity);
	if (strncmp(sd->name, IPU_ISYS_ENTITY_PREFIX,
		strlen(IPU_ISYS_ENTITY_PREFIX)) != 0)
		return true;

	for (i = 0; i < av->isys->pdata->ipdata->tpg.ntpgs &&
	     av->isys->tpg[i].isys; i++)
		if (entity == &av->isys->tpg[i].asd.sd.entity)
			return true;

	return false;
}

static int link_validate(struct media_link *link)
{
	struct ipu_isys_video *av =
	    container_of(link->sink, struct ipu_isys_video, pad);
	/* All sub-devices connected to a video node are ours. */
	struct ipu_isys_pipeline *ip =
		to_ipu_isys_pipeline(av->vdev.entity.pipe);
	struct v4l2_subdev_route r[IPU_ISYS_MAX_STREAMS];
	struct v4l2_subdev_routing routing = {
		.routes = r,
		.num_routes = IPU_ISYS_MAX_STREAMS,
	};
	int i, rval, active = 0;
	struct v4l2_subdev *sd;

	if (!link->source->entity)
		return -EINVAL;
	sd = media_entity_to_v4l2_subdev(link->source->entity);
	if (is_external(av, link->source->entity)) {
		ip->external = media_entity_remote_pad(av->vdev.entity.pads);
		ip->source = to_ipu_isys_subdev(sd)->source;
	}

	rval = v4l2_subdev_call(sd, pad, get_routing, &routing);
	if (rval)
		goto err_subdev;

	for (i = 0; i < routing.num_routes; i++) {
		if (!(routing.routes[i].flags & V4L2_SUBDEV_ROUTE_FL_ACTIVE))
			continue;

		if (routing.routes[i].source_pad == link->source->index)
			ip->stream_id = routing.routes[i].sink_stream;

		active++;
	}

	if (ip->external) {
		struct v4l2_mbus_frame_desc desc = {
			.num_entries = IPU_ISYS_MAX_STREAMS,
		};

		sd = media_entity_to_v4l2_subdev(ip->external->entity);
		rval = ipu_isys_subdev_get_frame_desc(sd, &desc);
		if (!rval && ip->stream_id < desc.num_entries)
			ip->vc = desc.entry[ip->stream_id].bus.csi2.channel;
	}

err_subdev:
	ip->nr_queues++;

	return 0;
}

static void get_stream_opened(struct ipu_isys_video *av)
{
	unsigned long flags;

	spin_lock_irqsave(&av->isys->lock, flags);
	av->isys->stream_opened++;
	spin_unlock_irqrestore(&av->isys->lock, flags);
}

static void put_stream_opened(struct ipu_isys_video *av)
{
	unsigned long flags;

	spin_lock_irqsave(&av->isys->lock, flags);
	av->isys->stream_opened--;
	spin_unlock_irqrestore(&av->isys->lock, flags);
}

static int get_stream_handle(struct ipu_isys_video *av)
{
	struct ipu_isys_pipeline *ip =
	    to_ipu_isys_pipeline(av->vdev.entity.pipe);
	unsigned int stream_handle;
	unsigned long flags;

	spin_lock_irqsave(&av->isys->lock, flags);
	for (stream_handle = 0;
	     stream_handle < IPU_ISYS_MAX_STREAMS; stream_handle++)
		if (!av->isys->pipes[stream_handle])
			break;
	if (stream_handle == IPU_ISYS_MAX_STREAMS) {
		spin_unlock_irqrestore(&av->isys->lock, flags);
		return -EBUSY;
	}
	av->isys->pipes[stream_handle] = ip;
	ip->stream_handle = stream_handle;
	spin_unlock_irqrestore(&av->isys->lock, flags);
	return 0;
}

static void put_stream_handle(struct ipu_isys_video *av)
{
	struct ipu_isys_pipeline *ip =
	    to_ipu_isys_pipeline(av->vdev.entity.pipe);
	unsigned long flags;

	spin_lock_irqsave(&av->isys->lock, flags);
	av->isys->pipes[ip->stream_handle] = NULL;
	ip->stream_handle = -1;
	spin_unlock_irqrestore(&av->isys->lock, flags);
}

static int get_external_facing_format(struct ipu_isys_pipeline *ip,
				      struct v4l2_subdev_format *format)
{
	struct ipu_isys_video *av = container_of(ip, struct ipu_isys_video, ip);
	struct v4l2_subdev *sd;
	struct media_pad *external_facing;

	if (!ip->external->entity) {
		WARN_ON(1);
		return -ENODEV;
	}
	sd = media_entity_to_v4l2_subdev(ip->external->entity);
	external_facing = (strncmp(sd->name, IPU_ISYS_ENTITY_PREFIX,
		strlen(IPU_ISYS_ENTITY_PREFIX)) == 0) ?
	    ip->external : media_entity_remote_pad(ip->external);
	if (WARN_ON(!external_facing)) {
		dev_warn(&av->isys->adev->dev,
			 "no external facing pad --- driver bug?\n");
		return -EINVAL;
	}

	format->which = V4L2_SUBDEV_FORMAT_ACTIVE;
	format->pad = 0;
	format->stream = ip->stream_id;
	sd = media_entity_to_v4l2_subdev(external_facing->entity);

	return v4l2_subdev_call(sd, pad, get_fmt, NULL, format);
}

static void short_packet_queue_destroy(struct ipu_isys_pipeline *ip)
{
	struct ipu_isys_video *av = container_of(ip, struct ipu_isys_video, ip);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
	struct dma_attrs attrs;
#else
	unsigned long attrs;
#endif
	unsigned int i;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
	init_dma_attrs(&attrs);
	dma_set_attr(DMA_ATTR_NON_CONSISTENT, &attrs);
#else
	attrs = DMA_ATTR_NON_CONSISTENT;
#endif
	if (!ip->short_packet_bufs)
		return;
	for (i = 0; i < IPU_ISYS_SHORT_PACKET_BUFFER_NUM; i++) {
		if (ip->short_packet_bufs[i].buffer)
			dma_free_attrs(&av->isys->adev->dev,
				       ip->short_packet_buffer_size,
				       ip->short_packet_bufs[i].buffer,
				       ip->short_packet_bufs[i].dma_addr,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
				       &attrs
#else
				       attrs
#endif
			    );
	}
	kfree(ip->short_packet_bufs);
	ip->short_packet_bufs = NULL;
}

static int short_packet_queue_setup(struct ipu_isys_pipeline *ip)
{
	struct ipu_isys_video *av = container_of(ip, struct ipu_isys_video, ip);
	struct v4l2_subdev_format source_fmt = { 0 };
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
	struct dma_attrs attrs;
#else
	unsigned long attrs;
#endif
	unsigned int i;
	int rval;
	size_t buf_size;

	INIT_LIST_HEAD(&ip->pending_interlaced_bufs);
	ip->cur_field = V4L2_FIELD_TOP;

	if (ip->isys->short_packet_source == IPU_ISYS_SHORT_PACKET_FROM_TUNIT) {
		ip->short_packet_trace_index = 0;
		return 0;
	}

	rval = get_external_facing_format(ip, &source_fmt);
	if (rval)
		return rval;
	buf_size = IPU_ISYS_SHORT_PACKET_BUF_SIZE(source_fmt.format.height);
	ip->short_packet_buffer_size = buf_size;
	ip->num_short_packet_lines =
	    IPU_ISYS_SHORT_PACKET_PKT_LINES(source_fmt.format.height);

	/* Initialize short packet queue. */
	INIT_LIST_HEAD(&ip->short_packet_incoming);
	INIT_LIST_HEAD(&ip->short_packet_active);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
	init_dma_attrs(&attrs);
	dma_set_attr(DMA_ATTR_NON_CONSISTENT, &attrs);
#else
	attrs = DMA_ATTR_NON_CONSISTENT;
#endif

	ip->short_packet_bufs =
	    kzalloc(sizeof(struct ipu_isys_private_buffer) *
		    IPU_ISYS_SHORT_PACKET_BUFFER_NUM, GFP_KERNEL);
	if (!ip->short_packet_bufs)
		return -ENOMEM;

	for (i = 0; i < IPU_ISYS_SHORT_PACKET_BUFFER_NUM; i++) {
		struct ipu_isys_private_buffer *buf = &ip->short_packet_bufs[i];

		buf->index = (unsigned int)i;
		buf->ip = ip;
		buf->ib.type = IPU_ISYS_SHORT_PACKET_BUFFER;
		buf->bytesused = buf_size;
		buf->buffer = dma_alloc_attrs(&av->isys->adev->dev, buf_size,
					      &buf->dma_addr, GFP_KERNEL,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
					      &attrs
#else
					      attrs
#endif
		    );
		if (!buf->buffer) {
			short_packet_queue_destroy(ip);
			return -ENOMEM;
		}
		list_add(&buf->ib.head, &ip->short_packet_incoming);
	}

	return 0;
}

void csi_short_packet_prepare_firmware_stream_cfg(
				struct ipu_isys_pipeline *ip,
				struct ipu_fw_isys_stream_cfg_data_abi *cfg)
{
	int input_pin = cfg->nof_input_pins++;
	int output_pin = cfg->nof_output_pins++;
	struct ipu_fw_isys_input_pin_info_abi *input_info =
	    &cfg->input_pins[input_pin];
	struct ipu_fw_isys_output_pin_info_abi *output_info =
	    &cfg->output_pins[output_pin];

	/*
	 * Setting dt as IPU_ISYS_SHORT_PACKET_GENERAL_DT will cause
	 * MIPI receiver to receive all MIPI short packets.
	 */
	input_info->dt = IPU_ISYS_SHORT_PACKET_GENERAL_DT;
	input_info->input_res.width = IPU_ISYS_SHORT_PACKET_WIDTH;
	input_info->input_res.height = ip->num_short_packet_lines;

	ip->output_pins[output_pin].pin_ready =
	    ipu_isys_queue_short_packet_ready;
	ip->output_pins[output_pin].aq = NULL;
	ip->short_packet_output_pin = output_pin;

	output_info->input_pin_id = input_pin;
	output_info->output_res.width = IPU_ISYS_SHORT_PACKET_WIDTH;
	output_info->output_res.height = ip->num_short_packet_lines;
	output_info->stride = IPU_ISYS_SHORT_PACKET_WIDTH *
	    IPU_ISYS_SHORT_PACKET_UNITSIZE;
	output_info->pt = IPU_ISYS_SHORT_PACKET_PT;
	output_info->ft = IPU_ISYS_SHORT_PACKET_FT;
	output_info->send_irq = 1;
}

void ipu_isys_prepare_firmware_stream_cfg_default(
			struct ipu_isys_video *av,
			struct ipu_fw_isys_stream_cfg_data_abi *cfg)
{
	struct ipu_isys_pipeline *ip =
	    to_ipu_isys_pipeline(av->vdev.entity.pipe);

	struct ipu_isys_queue *aq = &av->aq;
	struct ipu_fw_isys_output_pin_info_abi *pin_info;
	int pin = cfg->nof_output_pins++;

	aq->fw_output = pin;
	ip->output_pins[pin].pin_ready = ipu_isys_queue_buf_ready;
	ip->output_pins[pin].aq = aq;

	pin_info = &cfg->output_pins[pin];
	pin_info->input_pin_id = 0;
	pin_info->output_res.width = av->mpix.width;
	pin_info->output_res.height = av->mpix.height;

	if (!av->pfmt->bpp_planar)
		pin_info->stride = av->mpix.plane_fmt[0].bytesperline;
	else
		pin_info->stride = ALIGN(DIV_ROUND_UP(av->mpix.width *
						      av->pfmt->bpp_planar,
						      BITS_PER_BYTE),
					 av->isys->line_align);

	pin_info->pt = aq->css_pin_type;
	pin_info->ft = av->pfmt->css_pixelformat;
	pin_info->send_irq = 1;
	cfg->vc = ip->vc;
}

static unsigned int ipu_isys_get_compression_scheme(u32 code)
{
	switch (code) {
	case MEDIA_BUS_FMT_SBGGR10_DPCM8_1X8:
	case MEDIA_BUS_FMT_SGBRG10_DPCM8_1X8:
	case MEDIA_BUS_FMT_SGRBG10_DPCM8_1X8:
	case MEDIA_BUS_FMT_SRGGB10_DPCM8_1X8:
		return 3;
	default:
		return 0;
	}
}

static unsigned int get_comp_format(u32 code)
{
	unsigned int predictor = 0;	/* currently hard coded */
	unsigned int udt = ipu_isys_mbus_code_to_mipi(code);
	unsigned int scheme = ipu_isys_get_compression_scheme(code);

	/* if data type is not user defined return here */
	if (udt < IPU_ISYS_MIPI_CSI2_TYPE_USER_DEF(1) ||
	    udt > IPU_ISYS_MIPI_CSI2_TYPE_USER_DEF(8))
		return 0;

	/*
	 * For each user defined type (1..8) there is configuration bitfield for
	 * decompression.
	 *
	 * | bit 3     | bits 2:0 |
	 * | predictor | scheme   |
	 * compression schemes:
	 * 000 = no compression
	 * 001 = 10 - 6 - 10
	 * 010 = 10 - 7 - 10
	 * 011 = 10 - 8 - 10
	 * 100 = 12 - 6 - 12
	 * 101 = 12 - 7 - 12
	 * 110 = 12 - 8 - 12
	 */

	return ((predictor << 3) | scheme) <<
	    ((udt - IPU_ISYS_MIPI_CSI2_TYPE_USER_DEF(1)) * 4);
}

/* Create stream and start it using the CSS FW ABI. */
static int start_stream_firmware(struct ipu_isys_video *av,
				 struct ipu_isys_buffer_list *bl)
{
	struct ipu_isys_pipeline *ip =
	    to_ipu_isys_pipeline(av->vdev.entity.pipe);
	struct device *dev = &av->isys->adev->dev;
	struct v4l2_subdev_selection sel_fmt = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
		.target = V4L2_SEL_TGT_CROP,
		.pad = CSI2_BE_PAD_SOURCE,
	};
	struct ipu_fw_isys_stream_cfg_data_abi *stream_cfg;
	struct isys_fw_msgs *msg = NULL;
	struct ipu_fw_isys_frame_buff_set_abi *buf = NULL;
	struct ipu_isys_queue *aq;
	struct ipu_isys_video *isl_av = NULL;
	struct ipu_isys_request *ireq = NULL;
	struct v4l2_subdev_format source_fmt = { 0 };
	struct v4l2_subdev *be_sd = NULL;
	struct media_pad *source_pad = media_entity_remote_pad(&av->pad);
	int rval, rvalout, tout;

	rval = get_external_facing_format(ip, &source_fmt);
	if (rval)
		return rval;

	msg = ipu_get_fw_msg_buf(ip);
	if (!msg)
		return -ENOMEM;

	stream_cfg = to_stream_cfg_msg_buf(msg);
	stream_cfg->compfmt = get_comp_format(source_fmt.format.code);
	stream_cfg->input_pins[0].input_res.width = source_fmt.format.width;
	stream_cfg->input_pins[0].input_res.height = source_fmt.format.height;
	stream_cfg->input_pins[0].dt =
	    ipu_isys_mbus_code_to_mipi(source_fmt.format.code);
	stream_cfg->input_pins[0].mapped_dt = N_IPU_FW_ISYS_MIPI_DATA_TYPE;

	if (ip->csi2 && !v4l2_ctrl_g_ctrl(ip->csi2->store_csi2_header))
		stream_cfg->input_pins[0].mipi_store_mode =
		    IPU_FW_ISYS_MIPI_STORE_MODE_DISCARD_LONG_HEADER;
	else if (ip->tpg && !v4l2_ctrl_g_ctrl(ip->tpg->store_csi2_header))
		stream_cfg->input_pins[0].mipi_store_mode =
		    IPU_FW_ISYS_MIPI_STORE_MODE_DISCARD_LONG_HEADER;

	stream_cfg->src = ip->source;
	stream_cfg->vc = 0;
	stream_cfg->isl_use = ip->isl_mode;
	stream_cfg->nof_input_pins = 1;

	/*
	 * Only CSI2-BE and SOC BE has the capability to do crop,
	 * so get the crop info from csi2-be or csi2-be-soc.
	 */
	if (ip->csi2_be) {
		be_sd = &ip->csi2_be->asd.sd;
	} else if (ip->csi2_be_soc) {
		be_sd = &ip->csi2_be_soc->asd.sd;
		if (source_pad)
			sel_fmt.pad = source_pad->index;
	}
	if (be_sd &&
	    !v4l2_subdev_call(be_sd, pad, get_selection, NULL, &sel_fmt)) {
		stream_cfg->crop[0].left_offset = sel_fmt.r.left;
		stream_cfg->crop[0].top_offset = sel_fmt.r.top;
		stream_cfg->crop[0].right_offset = sel_fmt.r.left +
		    sel_fmt.r.width;
		stream_cfg->crop[0].bottom_offset = sel_fmt.r.top +
		    sel_fmt.r.height;

	} else {
		stream_cfg->crop[0].right_offset = source_fmt.format.width;
		stream_cfg->crop[0].bottom_offset = source_fmt.format.height;
	}

	/*
	 * If the CSI-2 backend's video node is part of the pipeline
	 * it must be arranged first in the output pin list. This is
	 * the most probably a firmware requirement.
	 */
	if (ip->isl_mode == IPU_ISL_CSI2_BE)
		isl_av = &ip->csi2_be->av;
	else if (ip->isl_mode == IPU_ISL_ISA)
		isl_av = &av->isys->isa.av;

	if (isl_av) {
		struct ipu_isys_queue *safe;

		list_for_each_entry_safe(aq, safe, &ip->queues, node) {
			struct ipu_isys_video *av = ipu_isys_queue_to_video(aq);

			if (av != isl_av)
				continue;

			list_del(&aq->node);
			list_add(&aq->node, &ip->queues);
			break;
		}
	}

	list_for_each_entry(aq, &ip->queues, node) {
		struct ipu_isys_video *__av = ipu_isys_queue_to_video(aq);

		__av->prepare_firmware_stream_cfg(__av, stream_cfg);
	}

	if (ip->interlaced && ip->isys->short_packet_source ==
	    IPU_ISYS_SHORT_PACKET_FROM_RECEIVER)
		csi_short_packet_prepare_firmware_stream_cfg(ip, stream_cfg);

	ipu_fw_isys_dump_stream_cfg(dev, stream_cfg);

	ip->nr_output_pins = stream_cfg->nof_output_pins;

	rval = get_stream_handle(av);
	if (rval) {
		dev_dbg(dev, "Can't get stream_handle\n");
		return rval;
	}

	reinit_completion(&ip->stream_open_completion);

	ipu_fw_isys_set_params(stream_cfg);

	rval = ipu_fw_isys_complex_cmd(av->isys,
				       ip->stream_handle,
				       stream_cfg,
				       to_dma_addr(msg),
				       sizeof(*stream_cfg),
				       IPU_FW_ISYS_SEND_TYPE_STREAM_OPEN);
	ipu_put_fw_mgs_buffer(av->isys, (uintptr_t) stream_cfg);

	if (rval < 0) {
		dev_err(dev, "can't open stream (%d)\n", rval);
		goto out_put_stream_handle;
	}

	get_stream_opened(av);

	tout = wait_for_completion_timeout(&ip->stream_open_completion,
					   IPU_LIB_CALL_TIMEOUT_JIFFIES);
	if (!tout) {
		dev_err(dev, "stream open time out\n");
		rval = -ETIMEDOUT;
		goto out_put_stream_opened;
	}
	if (ip->error) {
		dev_err(dev, "stream open error: %d\n", ip->error);
		rval = -EIO;
		goto out_put_stream_opened;
	}
	dev_dbg(dev, "start stream: open complete\n");

	ireq = ipu_isys_next_queued_request(ip);

	if (bl || ireq) {
		msg = ipu_get_fw_msg_buf(ip);
		if (!msg) {
			rval = -ENOMEM;
			goto out_put_stream_opened;
		}
		buf = to_frame_msg_buf(msg);
	}

	if (bl) {
		ipu_isys_buffer_list_to_ipu_fw_isys_frame_buff_set(buf, ip, bl);
		ipu_isys_buffer_list_queue(bl,
					   IPU_ISYS_BUFFER_LIST_FL_ACTIVE, 0);
	} else if (ireq) {
		rval = ipu_isys_req_prepare(&av->isys->media_dev,
					    ireq, ip, buf);
		if (rval)
			goto out_put_stream_opened;
	}

	reinit_completion(&ip->stream_start_completion);

	if (bl || ireq) {
		ipu_fw_isys_dump_frame_buff_set(dev, buf,
						stream_cfg->nof_output_pins);
		rval = ipu_fw_isys_complex_cmd(av->isys,
				ip->stream_handle,
				buf, to_dma_addr(msg),
				sizeof(*buf),
				IPU_FW_ISYS_SEND_TYPE_STREAM_START_AND_CAPTURE);
		ipu_put_fw_mgs_buffer(av->isys, (uintptr_t) buf);
	} else {
		rval = ipu_fw_isys_simple_cmd(av->isys,
					ip->stream_handle,
					IPU_FW_ISYS_SEND_TYPE_STREAM_START);
	}

	if (rval < 0) {
		dev_err(dev, "can't start streaming (%d)\n", rval);
		goto out_stream_close;
	}

	tout = wait_for_completion_timeout(&ip->stream_start_completion,
					   IPU_LIB_CALL_TIMEOUT_JIFFIES);
	if (!tout) {
		dev_err(dev, "stream start time out\n");
		rval = -ETIMEDOUT;
		goto out_stream_close;
	}
	if (ip->error) {
		dev_err(dev, "stream start error: %d\n", ip->error);
		rval = -EIO;
		goto out_stream_close;
	}
	dev_dbg(dev, "start stream: complete\n");

	return 0;

out_stream_close:
	reinit_completion(&ip->stream_close_completion);

	rvalout = ipu_fw_isys_simple_cmd(av->isys,
					 ip->stream_handle,
					 IPU_FW_ISYS_SEND_TYPE_STREAM_CLOSE);
	if (rvalout < 0) {
		dev_dbg(dev, "can't close stream (%d)\n", rvalout);
		goto out_put_stream_opened;
	}

	tout = wait_for_completion_timeout(&ip->stream_close_completion,
					   IPU_LIB_CALL_TIMEOUT_JIFFIES);
	if (!tout)
		dev_err(dev, "stream close time out\n");
	else if (ip->error)
		dev_err(dev, "stream close error: %d\n", ip->error);
	else
		dev_dbg(dev, "stream close complete\n");

out_put_stream_opened:
	put_stream_opened(av);

out_put_stream_handle:
	put_stream_handle(av);
	return rval;
}

static void stop_streaming_firmware(struct ipu_isys_video *av)
{
	struct ipu_isys_pipeline *ip =
	    to_ipu_isys_pipeline(av->vdev.entity.pipe);
	struct device *dev = &av->isys->adev->dev;
	int rval, tout;
	enum ipu_fw_isys_send_type send_type =
		IPU_FW_ISYS_SEND_TYPE_STREAM_FLUSH;

	reinit_completion(&ip->stream_stop_completion);

	/* Use STOP command if running in CSI capture mode */
	if (use_stream_stop)
		send_type = IPU_FW_ISYS_SEND_TYPE_STREAM_STOP;

	rval = ipu_fw_isys_simple_cmd(av->isys, ip->stream_handle,
				      send_type);

	if (rval < 0) {
		dev_err(dev, "can't stop stream (%d)\n", rval);
		return;
	}

	tout = wait_for_completion_timeout(&ip->stream_stop_completion,
					   IPU_LIB_CALL_TIMEOUT_JIFFIES);
	if (!tout)
		dev_err(dev, "stream stop time out\n");
	else if (ip->error)
		dev_err(dev, "stream stop error: %d\n", ip->error);
	else
		dev_dbg(dev, "stop stream: complete\n");
}

static void close_streaming_firmware(struct ipu_isys_video *av)
{
	struct ipu_isys_pipeline *ip =
	    to_ipu_isys_pipeline(av->vdev.entity.pipe);
	struct device *dev = &av->isys->adev->dev;
	int rval, tout;

	reinit_completion(&ip->stream_close_completion);

	rval = ipu_fw_isys_simple_cmd(av->isys, ip->stream_handle,
				      IPU_FW_ISYS_SEND_TYPE_STREAM_CLOSE);
	if (rval < 0) {
		dev_err(dev, "can't close stream (%d)\n", rval);
		return;
	}

	tout = wait_for_completion_timeout(&ip->stream_close_completion,
					   IPU_LIB_CALL_TIMEOUT_JIFFIES);
	if (!tout)
		dev_err(dev, "stream close time out\n");
	else if (ip->error)
		dev_err(dev, "stream close error: %d\n", ip->error);
	else
		dev_dbg(dev, "close stream: complete\n");

	put_stream_opened(av);
	put_stream_handle(av);
}

void
ipu_isys_video_add_capture_done(struct ipu_isys_pipeline *ip,
				void (*capture_done)
				 (struct ipu_isys_pipeline *ip,
				  struct ipu_fw_isys_resp_info_abi *resp))
{
	unsigned int i;

	/* Different instances may register same function. Add only once */
	for (i = 0; i < IPU_NUM_CAPTURE_DONE; i++)
		if (ip->capture_done[i] == capture_done)
			return;

	for (i = 0; i < IPU_NUM_CAPTURE_DONE; i++) {
		if (!ip->capture_done[i]) {
			ip->capture_done[i] = capture_done;
			return;
		}
	}
	/*
	 * Too many call backs registered. Change to IPU_NUM_CAPTURE_DONE
	 * constant probably required.
	 */
	WARN_ON(1);
}

int ipu_isys_video_prepare_streaming(struct ipu_isys_video *av,
				     unsigned int state)
{
	struct ipu_isys *isys = av->isys;
	struct device *dev = &isys->adev->dev;
	struct ipu_isys_pipeline *ip;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	struct media_graph graph;
#else
	struct media_entity_graph graph;
#endif
	struct media_entity *entity;
	struct media_device *mdev = &av->isys->media_dev;
	int rval;
	unsigned int i;

	dev_dbg(dev, "prepare stream: %d\n", state);

	if (!state) {
		ip = to_ipu_isys_pipeline(av->vdev.entity.pipe);

		if (ip->interlaced && isys->short_packet_source ==
		    IPU_ISYS_SHORT_PACKET_FROM_RECEIVER)
			short_packet_queue_destroy(ip);
		media_pipeline_stop(&av->vdev.entity);
		media_entity_enum_cleanup(&ip->entity_enum);
		return 0;
	}

	ip = &av->ip;

	WARN_ON(ip->nr_streaming);
	ip->has_sof = false;
	ip->nr_queues = 0;
	ip->external = NULL;
	atomic_set(&ip->sequence, 0);
	ip->isl_mode = IPU_ISL_OFF;

	for (i = 0; i < IPU_NUM_CAPTURE_DONE; i++)
		ip->capture_done[i] = NULL;
	ip->csi2_be = NULL;
	ip->csi2_be_soc = NULL;
	ip->csi2 = NULL;
	ip->tpg = NULL;
	ip->seq_index = 0;
	memset(ip->seq, 0, sizeof(ip->seq));

	WARN_ON(!list_empty(&ip->queues));
	ip->interlaced = false;

	rval = media_entity_enum_init(&ip->entity_enum, mdev);
	if (rval)
		return rval;

	rval = media_pipeline_start(&av->vdev.entity, &ip->pipe);
	if (rval < 0) {
		dev_dbg(dev, "pipeline start failed\n");
		goto out_enum_cleanup;
	}

	if (!ip->external) {
		dev_err(dev, "no external entity set! Driver bug?\n");
		rval = -EINVAL;
		goto out_pipeline_stop;
	}

	rval = media_graph_walk_init(&graph, mdev);
	if (rval)
		goto out_pipeline_stop;

	/* Gather all entities in the graph. */
	mutex_lock(&mdev->graph_mutex);
	media_graph_walk_start(&graph, &av->vdev.entity.pads[0]);
	while ((entity = media_graph_walk_next(&graph)))
		media_entity_enum_set(&ip->entity_enum, entity);

	mutex_unlock(&mdev->graph_mutex);

	media_graph_walk_cleanup(&graph);

	if (ip->interlaced) {
		rval = short_packet_queue_setup(ip);
		if (rval) {
			dev_err(&isys->adev->dev,
				"Failed to setup short packet buffer.\n");
			goto out_pipeline_stop;
		}
	}

	dev_dbg(dev, "prepare stream: external entity %s\n",
		ip->external->entity->name);

	return 0;

out_pipeline_stop:
	media_pipeline_stop(&av->vdev.entity);

out_enum_cleanup:
	media_entity_enum_cleanup(&ip->entity_enum);

	return rval;
}

static int perform_skew_cal(struct ipu_isys_pipeline *ip)
{
	struct v4l2_subdev *ext_sd =
	    media_entity_to_v4l2_subdev(ip->external->entity);
	int rval;

	if (!ext_sd) {
		WARN_ON(1);
		return -ENODEV;
	}
	ipu_isys_csi2_set_skew_cal(ip->csi2, true);

	rval = v4l2_subdev_call(ext_sd, video, s_stream, true);
	if (rval)
		goto turn_off_skew_cal;

	/* TODO: do we have a better way available than waiting for a while ? */
	msleep(50);

	rval = v4l2_subdev_call(ext_sd, video, s_stream, false);

turn_off_skew_cal:
	ipu_isys_csi2_set_skew_cal(ip->csi2, false);

	/* TODO: do we have a better way available than waiting for a while ? */
	msleep(50);

	return rval;
}

int ipu_isys_video_set_streaming(struct ipu_isys_video *av,
				 unsigned int state,
				 struct ipu_isys_buffer_list *bl)
{
	struct device *dev = &av->isys->adev->dev;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
	struct media_device *mdev = av->vdev.entity.parent;
	struct media_entity_graph graph;
#else
	struct media_device *mdev = av->vdev.entity.graph_obj.mdev;
#endif
	struct media_entity_enum entities;

	struct media_entity *entity, *entity2;
	struct ipu_isys_pipeline *ip =
	    to_ipu_isys_pipeline(av->vdev.entity.pipe);
	struct v4l2_subdev *sd, *esd;
	int rval = 0;

	dev_dbg(dev, "set stream: %d\n", state);

	if (!ip->external->entity) {
		WARN_ON(1);
		return -ENODEV;
	}
	esd = media_entity_to_v4l2_subdev(ip->external->entity);

	if (state) {
		rval = media_graph_walk_init(&ip->graph, mdev);
		if (rval)
			return rval;
		rval = media_entity_enum_init(&entities, mdev);
		if (rval)
			goto out_media_entity_graph_init;
	}

	if (!state) {
		stop_streaming_firmware(av);

		/* stop external sub-device now. */
		dev_err(dev, "s_stream %s (ext)\n", ip->external->entity->name);

		if (ip->csi2) {
			if (ip->csi2->stream_count == 1) {
				v4l2_subdev_call(esd, video, s_stream, state);
				ipu_isys_csi2_wait_last_eof(ip->csi2);
			}
		} else {
			v4l2_subdev_call(esd, video, s_stream, state);
		}
	}

	mutex_lock(&mdev->graph_mutex);

	media_graph_walk_start(&
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
				      ip->
#endif
				      graph,
				      &av->vdev.entity.pads[0]);

	while ((entity = media_graph_walk_next(&
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
						      ip->
#endif
						      graph))) {
		sd = media_entity_to_v4l2_subdev(entity);

		dev_dbg(dev, "set stream: entity %s\n", entity->name);

		/* Non-subdev nodes can be safely ignored here. */
		if (!is_media_entity_v4l2_subdev(entity))
			continue;

		/* Don't start truly external devices quite yet. */
		if (strncmp(sd->name, IPU_ISYS_ENTITY_PREFIX,
			strlen(IPU_ISYS_ENTITY_PREFIX)) != 0 ||
			ip->external->entity == entity)
			continue;

		dev_dbg(dev, "s_stream %s\n", entity->name);
		rval = v4l2_subdev_call(sd, video, s_stream, state);
		if (!state)
			continue;
		if (rval && rval != -ENOIOCTLCMD) {
			mutex_unlock(&mdev->graph_mutex);
			goto out_media_entity_stop_streaming;
		}

		media_entity_enum_set(&entities, entity);
	}

	mutex_unlock(&mdev->graph_mutex);

	/* Oh crap */
	if (state) {
		if (ipu_isys_csi2_skew_cal_required(ip->csi2) &&
		    ip->csi2->remote_streams == ip->csi2->stream_count)
			perform_skew_cal(ip);

		rval = start_stream_firmware(av, bl);
		if (rval)
			goto out_media_entity_stop_streaming;

		dev_dbg(dev, "set stream: source %d, stream_handle %d\n",
			ip->source, ip->stream_handle);

		/* Start external sub-device now. */
		dev_dbg(dev, "set stream: s_stream %s (ext)\n",
			ip->external->entity->name);

		if (ip->csi2 &&
		    ip->csi2->remote_streams == ip->csi2->stream_count)
			rval = v4l2_subdev_call(esd, video, s_stream, state);
		else if (!ip->csi2)
			rval = v4l2_subdev_call(esd, video, s_stream, state);
		if (rval)
			goto out_media_entity_stop_streaming_firmware;
	} else {
		close_streaming_firmware(av);
		av->ip.stream_id = 0;
		av->ip.vc = 0;
	}

	if (state)
		media_entity_enum_cleanup(&entities);
	else
		media_graph_walk_cleanup(&ip->graph);
	av->streaming = state;

	return 0;

out_media_entity_stop_streaming_firmware:
	stop_streaming_firmware(av);

out_media_entity_stop_streaming:
	mutex_lock(&mdev->graph_mutex);

	media_graph_walk_start(&
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
				      ip->
#endif
				      graph,
				      &av->vdev.entity.pads[0]);

	while (state && (entity2 = media_graph_walk_next(&
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
								ip->
#endif
								graph)) &&
	       entity2 != entity) {
		sd = media_entity_to_v4l2_subdev(entity2);

		if (!media_entity_enum_test(&entities, entity2))
			continue;

		v4l2_subdev_call(sd, video, s_stream, 0);
	}

	mutex_unlock(&mdev->graph_mutex);

	media_entity_enum_cleanup(&entities);

out_media_entity_graph_init:
	media_graph_walk_cleanup(&ip->graph);

	return rval;
}

#ifdef CONFIG_COMPAT
static long ipu_isys_compat_ioctl(struct file *file, unsigned int cmd,
				  unsigned long arg)
{
	long ret = -ENOIOCTLCMD;
	void __user *up = compat_ptr(arg);

	/*
	 * at present, there is not any private IOCTL need to compat handle
	 */
	if (file->f_op->unlocked_ioctl)
		ret = file->f_op->unlocked_ioctl(file, cmd, (unsigned long)up);

	return ret;
}
#endif

static const struct v4l2_ioctl_ops ioctl_ops_splane = {
	.vidioc_querycap = ipu_isys_vidioc_querycap,
	.vidioc_enum_fmt_vid_cap = ipu_isys_vidioc_enum_fmt,
	.vidioc_g_fmt_vid_cap = vidioc_g_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap = vidioc_s_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap = vidioc_try_fmt_vid_cap,
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_enum_input = vidioc_enum_input,
	.vidioc_g_input = vidioc_g_input,
	.vidioc_s_input = vidioc_s_input,
};

static const struct v4l2_ioctl_ops ioctl_ops_mplane = {
	.vidioc_querycap = ipu_isys_vidioc_querycap,
	.vidioc_enum_fmt_vid_cap_mplane = ipu_isys_vidioc_enum_fmt,
	.vidioc_g_fmt_vid_cap_mplane = vidioc_g_fmt_vid_cap_mplane,
	.vidioc_s_fmt_vid_cap_mplane = vidioc_s_fmt_vid_cap_mplane,
	.vidioc_try_fmt_vid_cap_mplane = vidioc_try_fmt_vid_cap_mplane,
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_enum_input = vidioc_enum_input,
	.vidioc_g_input = vidioc_g_input,
	.vidioc_s_input = vidioc_s_input,
};

static const struct media_entity_operations entity_ops = {
	.link_validate = link_validate,
};

static const struct v4l2_file_operations isys_fops = {
	.owner = THIS_MODULE,
	.poll = vb2_fop_poll,
	.unlocked_ioctl = video_ioctl2,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = ipu_isys_compat_ioctl,
#endif
	.mmap = vb2_fop_mmap,
	.open = video_open,
	.release = video_release,
};

/*
 * Do everything that's needed to initialise things related to video
 * buffer queue, video node, and the related media entity. The caller
 * is expected to assign isys field and set the name of the video
 * device.
 */
int ipu_isys_video_init(struct ipu_isys_video *av,
			struct media_entity *entity,
			unsigned int pad, unsigned long pad_flags,
			unsigned int flags)
{
	const struct v4l2_ioctl_ops *ioctl_ops = NULL;
	int rval;

	mutex_init(&av->mutex);
	init_completion(&av->ip.stream_open_completion);
	init_completion(&av->ip.stream_close_completion);
	init_completion(&av->ip.stream_start_completion);
	init_completion(&av->ip.stream_stop_completion);
	init_completion(&av->ip.capture_ack_completion);
	INIT_LIST_HEAD(&av->ip.queues);
	spin_lock_init(&av->ip.short_packet_queue_lock);
	av->ip.isys = av->isys;
	av->ip.stream_id = 0;
	av->ip.vc = 0;

	if (pad_flags & MEDIA_PAD_FL_SINK) {
		/* data_offset is available only for multi-plane buffers */
		if (av->line_header_length) {
			av->aq.vbq.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
			ioctl_ops = &ioctl_ops_mplane;
		} else {
			av->aq.vbq.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			ioctl_ops = &ioctl_ops_splane;
		}
		av->vdev.vfl_dir = VFL_DIR_RX;
	} else {
		av->aq.vbq.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		av->vdev.vfl_dir = VFL_DIR_TX;
	}
	rval = ipu_isys_queue_init(&av->aq);
	if (rval)
		goto out_mutex_destroy;

	av->pad.flags = pad_flags | MEDIA_PAD_FL_MUST_CONNECT;
	rval = media_entity_pads_init(&av->vdev.entity, 1, &av->pad);
	if (rval)
		goto out_ipu_isys_queue_cleanup;

	av->vdev.entity.ops = &entity_ops;
	av->vdev.release = video_device_release_empty;
	av->vdev.fops = &isys_fops;
	av->vdev.v4l2_dev = &av->isys->v4l2_dev;
	if (!av->vdev.ioctl_ops)
		av->vdev.ioctl_ops = ioctl_ops;
	av->vdev.queue = &av->aq.vbq;
	av->vdev.lock = &av->mutex;
	set_bit(V4L2_FL_USES_V4L2_FH, &av->vdev.flags);
	video_set_drvdata(&av->vdev, av);

	mutex_lock(&av->mutex);

	rval = video_register_device(&av->vdev, VFL_TYPE_GRABBER, -1);
	if (rval)
		goto out_media_entity_cleanup;

	if (pad_flags & MEDIA_PAD_FL_SINK)
		rval = media_create_pad_link(entity, pad,
					     &av->vdev.entity, 0, flags);
	else
		rval = media_create_pad_link(&av->vdev.entity, 0, entity,
					     pad, flags);
	if (rval) {
		dev_info(&av->isys->adev->dev, "can't create link\n");
		goto out_media_entity_cleanup;
	}

	av->pfmt = av->try_fmt_vid_mplane(av, &av->mpix);

	mutex_unlock(&av->mutex);

	return rval;

out_media_entity_cleanup:
	video_unregister_device(&av->vdev);
	mutex_unlock(&av->mutex);
	media_entity_cleanup(&av->vdev.entity);

out_ipu_isys_queue_cleanup:
	ipu_isys_queue_cleanup(&av->aq);

out_mutex_destroy:
	mutex_destroy(&av->mutex);

	return rval;
}

void ipu_isys_video_cleanup(struct ipu_isys_video *av)
{
	video_unregister_device(&av->vdev);
	media_entity_cleanup(&av->vdev.entity);
	mutex_destroy(&av->mutex);
	ipu_isys_queue_cleanup(&av->aq);
}
