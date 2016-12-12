/*
 * Copyright (c) 2014--2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>

#include <media/intel-ipu4-isys.h>
#include <media/media-entity.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-dma-contig.h>

#include <uapi/linux/intel-ipu4-isys.h>
#include <uapi/linux/intel-ipu4-isys-isa-fw.h>

#include "intel-ipu4.h"
#include "intel-ipu4-bus.h"
#include "intel-ipu4-isys.h"
#include "intel-ipu4-isys-isa.h"
#include "intel-ipu4-isys-subdev.h"
#include "intel-ipu4-isys-video.h"

static const uint32_t isa_supported_codes_pad_sink[] = {
	MEDIA_BUS_FMT_SBGGR14_1X14,
	MEDIA_BUS_FMT_SGBRG14_1X14,
	MEDIA_BUS_FMT_SGRBG14_1X14,
	MEDIA_BUS_FMT_SRGGB14_1X14,
	MEDIA_BUS_FMT_SBGGR12_1X12,
	MEDIA_BUS_FMT_SGBRG12_1X12,
	MEDIA_BUS_FMT_SGRBG12_1X12,
	MEDIA_BUS_FMT_SRGGB12_1X12,
	MEDIA_BUS_FMT_SBGGR10_1X10,
	MEDIA_BUS_FMT_SGBRG10_1X10,
	MEDIA_BUS_FMT_SGRBG10_1X10,
	MEDIA_BUS_FMT_SRGGB10_1X10,
	MEDIA_BUS_FMT_SBGGR8_1X8,
	MEDIA_BUS_FMT_SGBRG8_1X8,
	MEDIA_BUS_FMT_SGRBG8_1X8,
	MEDIA_BUS_FMT_SRGGB8_1X8,
	0,
};

/* Regardless of the input mode ISA always produces 16 bit output */
static const uint32_t isa_supported_codes_pad_source[] = {
	MEDIA_BUS_FMT_SBGGR12_1X12,
	MEDIA_BUS_FMT_SGBRG12_1X12,
	MEDIA_BUS_FMT_SGRBG12_1X12,
	MEDIA_BUS_FMT_SRGGB12_1X12,
	0,
};

/* ISA configuration */
struct intel_ipu4_isys_pixelformat isa_config_pfmts[] = {
	{ V4L2_FMT_INTEL_IPU4_ISA_CFG, 8, 8, 0, MEDIA_BUS_FMT_FIXED, 0 },
	{ },
};

static const uint32_t isa_supported_codes_pad_cfg[] = {
	MEDIA_BUS_FMT_FIXED,
	0,
};

static const uint32_t isa_supported_codes_pad_3a[] = {
	MEDIA_BUS_FMT_FIXED,
	0,
};

static const uint32_t isa_supported_codes_pad_source_scaled[] = {
	MEDIA_BUS_FMT_SBGGR12_1X12,
	MEDIA_BUS_FMT_SGBRG12_1X12,
	MEDIA_BUS_FMT_SGRBG12_1X12,
	MEDIA_BUS_FMT_SRGGB12_1X12,
	MEDIA_BUS_FMT_YUYV12_1X24,
	0,
};

static const uint32_t *isa_supported_codes[] = {
	isa_supported_codes_pad_sink,
	isa_supported_codes_pad_source,
	isa_supported_codes_pad_cfg,
	isa_supported_codes_pad_3a,
	isa_supported_codes_pad_source_scaled,
};

static struct v4l2_subdev_internal_ops isa_sd_internal_ops = {
	.open = intel_ipu4_isys_subdev_open,
	.close = intel_ipu4_isys_subdev_close,
};

static int isa_config_vidioc_g_fmt_vid_out_mplane(struct file *file, void *fh,
						  struct v4l2_format *fmt)
{
	struct intel_ipu4_isys_video *av = video_drvdata(file);

	fmt->fmt.pix_mp = av->mpix;

	return 0;
}

const struct intel_ipu4_isys_pixelformat *isa_config_try_fmt_vid_out_mplane(
	struct intel_ipu4_isys_video *av, struct v4l2_pix_format_mplane *mpix)
{
	const struct intel_ipu4_isys_pixelformat *pfmt =
		intel_ipu4_isys_get_pixelformat(av, mpix->pixelformat);

	mpix->pixelformat = pfmt->pixelformat;
	mpix->num_planes = ISA_CFG_BUF_PLANES;

	mpix->plane_fmt[ISA_CFG_BUF_PLANE_PG].bytesperline = 0;
	mpix->plane_fmt[ISA_CFG_BUF_PLANE_PG].sizeimage = ALIGN(
		max_t(uint32_t,
		      sizeof(struct ia_css_process_group_light),
		      mpix->plane_fmt[ISA_CFG_BUF_PLANE_PG].sizeimage),
		av->isys->line_align);

	mpix->plane_fmt[ISA_CFG_BUF_PLANE_DATA].bytesperline = 0;
	mpix->plane_fmt[ISA_CFG_BUF_PLANE_DATA].sizeimage = ALIGN(
		max(1U, mpix->plane_fmt[ISA_CFG_BUF_PLANE_DATA].sizeimage),
		av->isys->line_align);

	return pfmt;
}

static int isa_config_vidioc_s_fmt_vid_out_mplane(struct file *file,
						void *fh, struct v4l2_format *f)
{
	struct intel_ipu4_isys_video *av = video_drvdata(file);

	if (av->aq.vbq.streaming)
		return -EBUSY;

	av->pfmt = isa_config_try_fmt_vid_out_mplane(av, &f->fmt.pix_mp);
	av->mpix = f->fmt.pix_mp;

	return 0;
}

static int isa_config_vidioc_try_fmt_vid_out_mplane(struct file *file,
						void *fh, struct v4l2_format *f)
{
	struct intel_ipu4_isys_video *av = video_drvdata(file);

	isa_config_try_fmt_vid_out_mplane(av, &f->fmt.pix_mp);
	return 0;
}

static const struct v4l2_ioctl_ops isa_config_ioctl_ops = {
	.vidioc_querycap = intel_ipu4_isys_vidioc_querycap,
	.vidioc_enum_fmt_vid_cap = intel_ipu4_isys_vidioc_enum_fmt,
	.vidioc_g_fmt_vid_out_mplane = isa_config_vidioc_g_fmt_vid_out_mplane,
	.vidioc_s_fmt_vid_out_mplane = isa_config_vidioc_s_fmt_vid_out_mplane,
	.vidioc_try_fmt_vid_out_mplane =
		isa_config_vidioc_try_fmt_vid_out_mplane,
	.vidioc_g_fmt_vid_cap_mplane = isa_config_vidioc_g_fmt_vid_out_mplane,
	.vidioc_s_fmt_vid_cap_mplane = isa_config_vidioc_s_fmt_vid_out_mplane,
	.vidioc_try_fmt_vid_cap_mplane =
		isa_config_vidioc_try_fmt_vid_out_mplane,
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
};

static const struct v4l2_subdev_core_ops isa_sd_core_ops = {
};

static int set_stream(struct v4l2_subdev *sd, int enable)
{
	struct intel_ipu4_isys_isa *isa = to_intel_ipu4_isys_isa(sd);
	unsigned int i;

	if (enable)
		return 0;

	for (i = 0; i < ISA_CFG_BUF_PLANES; i++)
		isa->next_param[i] = NULL;

	return 0;
}

static const struct v4l2_subdev_video_ops isa_sd_video_ops = {
	.s_stream = set_stream,
};

static const struct v4l2_subdev_pad_ops isa_sd_pad_ops = {
	.link_validate = intel_ipu4_isys_subdev_link_validate,
	.get_fmt = intel_ipu4_isys_subdev_get_ffmt,
	.set_fmt = intel_ipu4_isys_subdev_set_ffmt,
	.get_selection = intel_ipu4_isys_subdev_get_sel,
	.set_selection = intel_ipu4_isys_subdev_set_sel,
	.enum_mbus_code = intel_ipu4_isys_subdev_enum_mbus_code,
};

static struct v4l2_subdev_ops isa_sd_ops = {
	.core = &isa_sd_core_ops,
	.video = &isa_sd_video_ops,
	.pad = &isa_sd_pad_ops,
};

static int isa_link_validate(struct media_link *link)
{
	struct intel_ipu4_isys_video *av;
	struct intel_ipu4_isys_pipeline *ip;
	struct media_pipeline *pipe;

	/* Non-video node source */
	if (is_media_entity_v4l2_subdev(link->source->entity))
		return v4l2_subdev_link_validate(link);

	av = container_of(link->source, struct intel_ipu4_isys_video, pad);
	pipe = link->sink->entity->pipe;
	ip = to_intel_ipu4_isys_pipeline(pipe);
	ip->nr_queues++;

	return 0;
}

static struct media_entity_operations isa_entity_ops = {
	.link_validate = isa_link_validate,
};

void intel_ipu4_isys_isa_cleanup(struct intel_ipu4_isys_isa *isa)
{
	v4l2_device_unregister_subdev(&isa->asd.sd);
	intel_ipu4_isys_subdev_cleanup(&isa->asd);
	intel_ipu4_isys_video_cleanup(&isa->av_scaled);
	intel_ipu4_isys_video_cleanup(&isa->av_config);
	intel_ipu4_isys_video_cleanup(&isa->av_3a);
	intel_ipu4_isys_video_cleanup(&isa->av);
}

static void isa_set_ffmt(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
			 struct v4l2_subdev_fh *cfg,
#else
			 struct v4l2_subdev_pad_config *cfg,
#endif
			 struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *ffmt =
		__intel_ipu4_isys_get_ffmt(sd, cfg, fmt->pad, fmt->stream,
					   fmt->which);

	switch (fmt->pad) {
	case ISA_PAD_SINK:
		fmt->format.field = V4L2_FIELD_NONE;
		*ffmt = fmt->format;

		intel_ipu4_isys_subdev_fmt_propagate(
			sd, cfg, &fmt->format, NULL,
			INTEL_IPU4_ISYS_SUBDEV_PROP_TGT_SINK_FMT, fmt->pad,
			fmt->which);
		return;
	case ISA_PAD_SOURCE: {
		struct v4l2_mbus_framefmt *sink_ffmt =
			__intel_ipu4_isys_get_ffmt(sd, cfg, ISA_PAD_SINK,
						   fmt->stream, fmt->which);
		struct v4l2_rect *r =
			__intel_ipu4_isys_get_selection(
				sd, cfg, V4L2_SEL_TGT_CROP,
				ISA_PAD_SOURCE, fmt->which);

		ffmt->width = r->width;
		ffmt->height = r->height;
		ffmt->field = sink_ffmt->field;
		ffmt->code = isa_supported_codes_pad_source[
			intel_ipu4_isys_subdev_get_pixelorder(sink_ffmt->code)];
		return;
	}
	case ISA_PAD_CONFIG:
	case ISA_PAD_3A:
		ffmt->code = MEDIA_BUS_FMT_FIXED;
		ffmt->width = ffmt->height = 0;
		fmt->format = *ffmt;
		return;
	case ISA_PAD_SOURCE_SCALED: {
		struct v4l2_mbus_framefmt *sink_ffmt =
			__intel_ipu4_isys_get_ffmt(sd, cfg, ISA_PAD_SINK,
						   fmt->stream, fmt->which);
		struct v4l2_rect *r =
			__intel_ipu4_isys_get_selection(
				sd, cfg, V4L2_SEL_TGT_CROP,
				ISA_PAD_SOURCE_SCALED, fmt->which);

		ffmt->width = r->width;
		ffmt->height = r->height;
		ffmt->field = sink_ffmt->field;
		if (fmt->format.code == MEDIA_BUS_FMT_YUYV12_1X24)
			ffmt->code = MEDIA_BUS_FMT_YUYV12_1X24;
		else
			ffmt->code = isa_supported_codes_pad_source_scaled[
				intel_ipu4_isys_subdev_get_pixelorder(
					sink_ffmt->code)];
		return;
	}
	default:
		BUG_ON(1);
	}
}

static int isa_s_ctrl(struct v4l2_ctrl *ctrl)
{
	return 0;
}

static struct v4l2_ctrl_ops isa_ctrl_ops = {
	.s_ctrl = isa_s_ctrl,
};

static void isa_capture_done(struct intel_ipu4_isys_pipeline *ip,
			     struct ipu_fw_isys_resp_info_abi *info)
{
	struct intel_ipu4_isys_isa *isa = &ip->isys->isa;
	struct intel_ipu4_isys_queue *aq = &isa->av_config.aq;
	struct intel_ipu4_isys_buffer *ib;
	unsigned long flags;

	if (WARN_ON_ONCE(list_empty(&aq->active)))
		return;

	spin_lock_irqsave(&aq->lock, flags);
	ib = list_last_entry(&aq->active, struct intel_ipu4_isys_buffer, head);
	list_del(&ib->head);
	dev_dbg(&ip->isys->adev->dev, "isa cfg: dequeued buffer %p", ib);
	spin_unlock_irqrestore(&aq->lock, flags);

	intel_ipu4_isys_buf_calc_sequence_time(ib, info);
	intel_ipu4_isys_queue_buf_done(ib);

	aq = &isa->av_3a.aq;

	if (isa->av_3a.vdev.entity.pipe != isa->av_config.vdev.entity.pipe) {
		dev_dbg(&ip->isys->adev->dev, "3a disabled\n");
		return;
	}

	if (WARN_ON_ONCE(list_empty(&aq->active)))
		return;

	spin_lock_irqsave(&aq->lock, flags);
	ib = list_last_entry(&aq->active, struct intel_ipu4_isys_buffer, head);
	list_del(&ib->head);
	dev_dbg(&ip->isys->adev->dev, "isa 3a: dequeued buffer %p", ib);
	spin_unlock_irqrestore(&aq->lock, flags);

	intel_ipu4_isys_buf_calc_sequence_time(ib, info);
	intel_ipu4_isys_queue_buf_done(ib);
}

/* Maximum size of the buffer-specific process group. */
#define PGL_SIZE	PAGE_SIZE

static int isa_3a_buf_init(struct vb2_buffer *vb)
{
	struct intel_ipu4_isys_isa_buffer *isa_buf =
		vb2_buffer_to_intel_ipu4_isys_isa_buffer(vb);

	isa_buf->pgl.pg = kzalloc(PGL_SIZE, GFP_KERNEL);
	if (!isa_buf->pgl.pg)
		return -ENOMEM;

	return 0;
}

static void isa_3a_buf_cleanup(struct vb2_buffer *vb)
{
	struct intel_ipu4_isys_isa_buffer *isa_buf =
		vb2_buffer_to_intel_ipu4_isys_isa_buffer(vb);

	kfree(isa_buf->pgl.pg);
}

static int isa_config_buf_init(struct vb2_buffer *vb)
{
	struct intel_ipu4_isys_queue *aq =
		vb2_queue_to_intel_ipu4_isys_queue(vb->vb2_queue);
	struct intel_ipu4_isys_video *av = intel_ipu4_isys_queue_to_video(aq);
	struct intel_ipu4_isys_isa_buffer *isa_buf =
		vb2_buffer_to_intel_ipu4_isys_isa_buffer(vb);
	struct dma_attrs attrs;
	int rval;

	rval = isa_3a_buf_init(vb);
	if (rval)
		return rval;

	init_dma_attrs(&attrs);
	dma_set_attr(DMA_ATTR_NON_CONSISTENT, &attrs);

	isa_buf->pgl.common_pg =
		dma_alloc_attrs(&av->isys->adev->dev, PGL_SIZE << 1,
				&isa_buf->pgl.iova, GFP_KERNEL | __GFP_ZERO,
				&attrs);

	dev_dbg(&av->isys->adev->dev,
		"buf_init: index %u, cpu addr %p, dma addr %pad\n",
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
		vb->v4l2_buf.index,
#else
		vb->index,
#endif
		isa_buf->pgl.common_pg, &isa_buf->pgl.iova);

	if (!isa_buf->pgl.common_pg) {
		isa_3a_buf_cleanup(vb);
		return -ENOMEM;
	}

	return 0;
}

static void isa_config_buf_cleanup(struct vb2_buffer *vb)
{
	struct intel_ipu4_isys_queue *aq =
		vb2_queue_to_intel_ipu4_isys_queue(vb->vb2_queue);
	struct intel_ipu4_isys_video *av = intel_ipu4_isys_queue_to_video(aq);
	struct intel_ipu4_isys_isa_buffer *isa_buf =
		vb2_buffer_to_intel_ipu4_isys_isa_buffer(vb);
	struct dma_attrs attrs;

	dev_dbg(&av->isys->adev->dev,
		"buf_cleanup: index %u, cpu addr %p, dma addr %pad\n",
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
		vb->v4l2_buf.index,
#else
		vb->index,
#endif
		isa_buf->pgl.pg, &isa_buf->pgl.iova);
	if (!isa_buf->pgl.pg)
		return;

	init_dma_attrs(&attrs);
	dma_set_attr(DMA_ATTR_NON_CONSISTENT, &attrs);

	dma_free_attrs(&av->isys->adev->dev, PGL_SIZE << 1,
		       isa_buf->pgl.common_pg, isa_buf->pgl.iova, &attrs);

	isa_3a_buf_cleanup(vb);
}

static void isa_prepare_firmware_stream_cfg(
	struct intel_ipu4_isys_video *av,
	struct ipu_fw_isys_stream_cfg_data_abi *cfg)
{
#ifdef IPU_STEP_BXTB0
	struct v4l2_rect *r;
	unsigned int pad, cropping_location, res_info;

	if (av == &av->isys->isa.av) {
		pad = ISA_PAD_SOURCE;
		cropping_location =
			IPU_FW_ISYS_CROPPING_LOCATION_POST_ISA_NONSCALED;
		res_info = IPU_FW_ISYS_RESOLUTION_INFO_POST_ISA_NONSCALED;
	} else if (av == &av->isys->isa.av_scaled) {
		pad = ISA_PAD_SOURCE_SCALED;
		cropping_location =
			IPU_FW_ISYS_CROPPING_LOCATION_POST_ISA_SCALED;
		res_info = IPU_FW_ISYS_RESOLUTION_INFO_POST_ISA_SCALED;
	} else {
		BUG();
	}

	r = __intel_ipu4_isys_get_selection(&av->isys->isa.asd.sd, NULL,
					    V4L2_SEL_TGT_CROP, pad,
					    V4L2_SUBDEV_FORMAT_ACTIVE);

	cfg->crop[cropping_location].top_offset = r->top;
	cfg->crop[cropping_location].left_offset = r->left;
	cfg->crop[cropping_location].bottom_offset = r->top + r->height;
	cfg->crop[cropping_location].right_offset = r->left + r->width;

	r = __intel_ipu4_isys_get_selection(&av->isys->isa.asd.sd, NULL,
					    V4L2_SEL_TGT_COMPOSE, pad,
					    V4L2_SUBDEV_FORMAT_ACTIVE);

	cfg->isa_cfg.isa_res[res_info].height = r->height;
	cfg->isa_cfg.isa_res[res_info].width = r->width;
#endif
	intel_ipu4_isys_prepare_firmware_stream_cfg_default(av, cfg);
}

static void isa_prepare_firmware_stream_cfg_param(
	struct intel_ipu4_isys_video *av,
	struct ipu_fw_isys_stream_cfg_data_abi *cfg)
{
	struct intel_ipu4_isys_isa *isa = &av->isys->isa;
	struct intel_ipu4_isys_pipeline *ip =
		to_intel_ipu4_isys_pipeline(av->vdev.entity.pipe);

	cfg->isa_cfg.cfg.blc =
		!!(isa->isa_en->val & V4L2_INTEL_IPU4_ISA_EN_BLC);
	cfg->isa_cfg.cfg.lsc =
		!!(isa->isa_en->val & V4L2_INTEL_IPU4_ISA_EN_LSC);
	cfg->isa_cfg.cfg.dpc =
		!!(isa->isa_en->val & V4L2_INTEL_IPU4_ISA_EN_DPC);
	cfg->isa_cfg.cfg.downscaler =
		!!(isa->isa_en->val & V4L2_INTEL_IPU4_ISA_EN_SCALER);
	cfg->isa_cfg.cfg.awb =
		!!(isa->isa_en->val & V4L2_INTEL_IPU4_ISA_EN_AWB);
	cfg->isa_cfg.cfg.af =
		!!(isa->isa_en->val & V4L2_INTEL_IPU4_ISA_EN_AF);
	cfg->isa_cfg.cfg.ae =
		!!(isa->isa_en->val & V4L2_INTEL_IPU4_ISA_EN_AE);

	cfg->isa_cfg.cfg.send_irq_stats_ready = 1;
	cfg->isa_cfg.cfg.send_resp_stats_ready = 1;

	intel_ipu4_isys_video_add_capture_done(ip, isa_capture_done);
}

static bool is_capture_terminal(struct ia_css_terminal *t)
{
	switch (t->terminal_type) {
	case IPU_FW_TERMINAL_TYPE_PARAM_CACHED_OUT:
	case IPU_FW_TERMINAL_TYPE_PARAM_SPATIAL_OUT:
	case IPU_FW_TERMINAL_TYPE_PARAM_SLICED_OUT:
		return true;
	default:
		return false;
	}
}

/* Return the pointer to the terminal payload's IOVA. */
static int isa_terminal_get_iova(struct device *dev, struct ia_css_terminal *t,
				 uint32_t **iova)
{
	switch (t->terminal_type) {
	case IPU_FW_TERMINAL_TYPE_PARAM_CACHED_IN:
	case IPU_FW_TERMINAL_TYPE_PARAM_CACHED_OUT: {
		struct ia_css_param_terminal *tpterm = (void *)t;

		*iova = &tpterm->param_payload.buffer;
		break;
	}
	case IPU_FW_TERMINAL_TYPE_PARAM_SPATIAL_IN:
	case IPU_FW_TERMINAL_TYPE_PARAM_SPATIAL_OUT: {
		struct ia_css_spatial_param_terminal *tpterm = (void *)t;

		*iova = &tpterm->param_payload.buffer;
		break;
	}
	case IPU_FW_TERMINAL_TYPE_PARAM_SLICED_IN:
	case IPU_FW_TERMINAL_TYPE_PARAM_SLICED_OUT: {
		struct ia_css_sliced_param_terminal *tpterm = (void *)t;

		*iova = &tpterm->param_payload.buffer;
		break;
	}
	case IPU_FW_TERMINAL_TYPE_PROGRAM: {
		struct ia_css_program_terminal *tpterm = (void *)t;

		*iova = &tpterm->param_payload.buffer;
		break;
	}
	default:
		dev_dbg(dev, "unhandled terminal type %u\n", t->terminal_type);
		return -EINVAL;
	}

	return 0;
}

/*
 * Validate a process group, and add the IOVA of the data plane to the
 * offsets related to the start of the data plane.
 */
static int isa_import_pg(struct vb2_buffer *vb)
{
	void *__pg = vb2_plane_vaddr(vb, ISA_CFG_BUF_PLANE_PG);
	struct intel_ipu4_isys_queue *aq =
		vb2_queue_to_intel_ipu4_isys_queue(vb->vb2_queue);
	struct intel_ipu4_isys_video *av = intel_ipu4_isys_queue_to_video(aq);
	struct intel_ipu4_isys_isa_buffer *isa_buf =
		vb2_buffer_to_intel_ipu4_isys_isa_buffer(vb);
	struct ia_css_process_group_light *pg = isa_buf->pgl.pg;
	bool capture = aq->vbq.type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	uint32_t addr = vb2_dma_contig_plane_dma_addr(vb,
						      ISA_CFG_BUF_PLANE_DATA);
	unsigned int i;

	if (!__pg) {
		dev_warn(&av->isys->adev->dev,
			 "virtual mapping of the buffer failed\n");
		return -EINVAL;
	}

	if (vb2_plane_size(vb, ISA_CFG_BUF_PLANE_PG) > PGL_SIZE) {
		dev_dbg(&av->isys->adev->dev,
			"too large process group, max %lu\n",
			PGL_SIZE);
		return -EINVAL;
	}

	/*
	 * Copy the light process group to a kernel buffer so that it
	 * cannot be modified by the user space.
	 */
	memcpy(pg, __pg, vb2_plane_size(vb, ISA_CFG_BUF_PLANE_PG));

	if (pg->size > vb2_plane_size(vb, ISA_CFG_BUF_PLANE_PG)) {
		dev_dbg(&av->isys->adev->dev,
			"process group size too large (%u bytes, %lu bytes available)\n",
			pg->size, vb2_plane_size(vb, ISA_CFG_BUF_PLANE_PG));
		return -EINVAL;
	}

	if (!pg->terminal_count) {
		dev_dbg(&av->isys->adev->dev, "no terminals defined\n");
		return -EINVAL;
	}

	if ((void *)(ia_css_terminal_offsets(pg) +
		     pg->terminal_count * sizeof(uint16_t)) - (void *)pg
	    > pg->size) {
		dev_dbg(&av->isys->adev->dev,
			"terminal offsets do not fit in the buffer\n");
		return -EINVAL;
	}

	for (i = 0; i < pg->terminal_count; i++) {
		struct ia_css_terminal *t = to_ia_css_terminal(pg, i);
		uint32_t *iova;
		int rval;

		if ((void *)t + sizeof(*t) - (void *)pg > pg->size) {
			dev_dbg(&av->isys->adev->dev,
				"terminal %u does not fit in the buffer\n", i);
			return -EINVAL;
		}

		dev_dbg(&av->isys->adev->dev,
			"terminal: parsing terminal %u, \
			size %u, capture %u / %u\n",
			i, t->size, capture, is_capture_terminal(t));

		if (capture != is_capture_terminal(t))
			continue;

		dev_dbg(&av->isys->adev->dev,
			       "terminal: %u offset %u\n", i,
			       ia_css_terminal_offsets(pg)[i]);

		rval = isa_terminal_get_iova(&av->isys->adev->dev, t, &iova);
		if (rval)
			return rval;

		dev_dbg(&av->isys->adev->dev,
			"terminal: offset 0x%x, address 0x%8.8x\n",
			*iova, (uint32_t)addr + *iova);

		if (addr + *iova < addr) {
			dev_dbg(&av->isys->adev->dev,
				"address space overflow\n");
			return -EINVAL;
		}

		if (*iova > vb2_plane_size(vb, ISA_CFG_BUF_PLANE_DATA)) {
			dev_dbg(&av->isys->adev->dev,
				"offset outside the buffer\n");
			return -EINVAL;
		}

		/*
		 * Add the IOVA of the data plane to the terminal
		 * payload's offset.
		 */
		*iova += addr;
	}

	return 0;
}

static int isa_terminal_buf_prepare(struct vb2_buffer *vb)
{
	struct intel_ipu4_isys_queue *aq =
		vb2_queue_to_intel_ipu4_isys_queue(vb->vb2_queue);
	struct intel_ipu4_isys_video *av = intel_ipu4_isys_queue_to_video(aq);
	unsigned int i;

	for (i = 0; i < ISA_CFG_BUF_PLANES; i++) {
		vb2_set_plane_payload(vb, i, av->mpix.plane_fmt[i].sizeimage);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
		vb->v4l2_planes[i].data_offset = 0;
#else
		vb->planes[i].data_offset = 0;
#endif
	}

	return isa_import_pg(vb);
}

/*
 * Count relevant terminals in a light process group and add the
 * number of found to the common light process group.
 */
static void isa_config_count_valid_terminals(
	struct device *dev, struct ia_css_process_group_light *cpg,
	struct ia_css_process_group_light *pg, bool capture)
{
	unsigned int i;

	for (i = 0; i < pg->terminal_count; i++)
		if (capture == is_capture_terminal(to_ia_css_terminal(pg, i)))
			cpg->terminal_count++;
}

static void isa_config_prepare_frame_buff_set_one(
	struct device *dev,
	struct ia_css_process_group_light *cpg,
	struct ia_css_process_group_light *pg, dma_addr_t addr, bool capture,
	unsigned int *terminal_count)
{
	unsigned int i;

	dev_dbg(dev, "terminal: size %u, count %u, offset %u\n",
		pg->size, pg->terminal_count, pg->terminals_offset_offset);

	dev_dbg(dev, "terminal: copying %u terminal offsets to %p from %p\n",
		pg->terminal_count, ia_css_terminal_offsets(cpg),
		ia_css_terminal_offsets(pg));

	for (i = 0; i < pg->terminal_count; i++) {
		struct ia_css_terminal *t = to_ia_css_terminal(pg, i), *ct;

		dev_dbg(dev,
			"terminal: parsing %u, size %u, capture %u / %u\n",
			i, t->size, capture, is_capture_terminal(t));

		if (capture != is_capture_terminal(t))
			continue;

		ia_css_terminal_offsets(cpg)[*terminal_count] =
			ia_css_terminal_offset(cpg, *terminal_count);

		dev_dbg(dev, "terminal: %u offset %u\n", *terminal_count,
			ia_css_terminal_offsets(cpg)[*terminal_count]);

		ct = to_ia_css_terminal(cpg, *terminal_count);

		dev_dbg(dev,
			"terminal: copying terminal %p to %p (%u bytes)\n",
			t, ct, t->size);
		memcpy(ct, t, t->size);

		(*terminal_count)++;
	}
}

/*
 * Move the terminals from a read-only or write-only light process
 * group to a common process group.
 */
static void isa_config_prepare_frame_buff_set(struct vb2_buffer *__vb)
{
	struct intel_ipu4_isys_queue *aq =
		vb2_queue_to_intel_ipu4_isys_queue(__vb->vb2_queue);
	struct intel_ipu4_isys_video *av = intel_ipu4_isys_queue_to_video(aq);
	struct intel_ipu4_isys_isa *isa = &av->isys->isa;
	struct vb2_buffer *vb[ISA_PARAM_QUEUES];
	struct ia_css_process_group_light *pg[ISA_PARAM_QUEUES];
	dma_addr_t addr[ISA_PARAM_QUEUES];
	struct ia_css_process_group_light *cpg;
	struct intel_ipu4_isys_isa_buffer *__isa_buf;
	unsigned int terminal_count = 0, i;
	bool capture = &av->isys->isa.av_3a.aq == aq;

	dev_dbg(&av->isys->adev->dev, "%s: capture %u\n", av->vdev.name,
		capture);

	isa->next_param[capture] = __vb;

	/* Proceed only when both cfg and stats buffers are available. */
	if (!isa->next_param[!capture])
		return;

	/* Obtain common process group light buffer from config buffer */
	__isa_buf = vb2_buffer_to_intel_ipu4_isys_isa_buffer(
		isa->next_param[ISA_CFG_BUF_PLANE_PG]);

	for (i = 0; i < ISA_PARAM_QUEUES; i++) {
		struct intel_ipu4_isys_isa_buffer *isa_buf;

		vb[i] = isa->next_param[i];
		isa_buf = vb2_buffer_to_intel_ipu4_isys_isa_buffer(vb[i]);
		pg[i] = isa_buf->pgl.pg;
		addr[i] = vb2_dma_contig_plane_dma_addr(vb[i],
						ISA_CFG_BUF_PLANE_DATA);

		dma_sync_single_for_device(&av->isys->adev->dev,
					   addr[i],
					   vb2_plane_size(vb[i],
						ISA_CFG_BUF_PLANE_DATA),
					   DMA_TO_DEVICE);

		dev_dbg(&av->isys->adev->dev,
			"terminal: queue %u, plane 0: vaddr %p, \
			dma_addr %pad program group size %u \
			program group terminals %u\n",
			       i, pg[i], &addr[i],
			       pg[i]->size, pg[i]->terminal_count);
	}

	cpg = __isa_buf->pgl.common_pg;
	cpg->terminal_count = 0;
	cpg->terminals_offset_offset = sizeof(*cpg);

	if (cpg->size > PGL_SIZE << 1) {
		dev_err(&av->isys->adev->dev,
			"not enough room for terms, %lu found, %u needed\n",
			PGL_SIZE << 1, cpg->size);
		return;
	}

	for (i = 0; i < ISA_PARAM_QUEUES; i++)
		isa_config_count_valid_terminals(&av->isys->adev->dev,
						 cpg, pg[i], i);

	for (i = 0; i < ISA_PARAM_QUEUES; i++) {
		isa_config_prepare_frame_buff_set_one(
			&av->isys->adev->dev, cpg, pg[i], addr[i], i,
			&terminal_count);

		isa->next_param[i] = NULL;
	}

	cpg->size = ia_css_terminal_offset(cpg, cpg->terminal_count);

	dev_dbg(&av->isys->adev->dev, "common pg size 0x%x count %d\n",
		       cpg->size, cpg->terminal_count);

	dma_sync_single_for_device(&av->isys->adev->dev, __isa_buf->pgl.iova,
				   PGL_SIZE << 1, DMA_TO_DEVICE);
}

static void isa_config_fill_frame_buff_set_pin(
	struct vb2_buffer *vb, struct ipu_fw_isys_frame_buff_set_abi *set)
{
	struct intel_ipu4_isys_isa_buffer *isa_buf =
		vb2_buffer_to_intel_ipu4_isys_isa_buffer(vb);

	set->process_group_light.addr = isa_buf->pgl.iova;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
	set->process_group_light.param_buf_id = vb->v4l2_buf.index + 1;
#else
	set->process_group_light.param_buf_id = vb->index + 1;
#endif
}

static void isa_ctrl_init(struct v4l2_subdev *sd)
{
	struct intel_ipu4_isys_isa *isa = to_intel_ipu4_isys_isa(sd);
	static const struct v4l2_ctrl_config cfg = {
		.ops = &isa_ctrl_ops,
		.id = V4L2_CID_INTEL_IPU4_ISA_EN,
		.name = "ISA enable",
		.type = V4L2_CTRL_TYPE_BITMASK,
		.max = V4L2_INTEL_IPU4_ISA_EN_BLC
		| V4L2_INTEL_IPU4_ISA_EN_LSC
		| V4L2_INTEL_IPU4_ISA_EN_DPC
		| V4L2_INTEL_IPU4_ISA_EN_SCALER
		| V4L2_INTEL_IPU4_ISA_EN_AWB
		| V4L2_INTEL_IPU4_ISA_EN_AF
		| V4L2_INTEL_IPU4_ISA_EN_AE,
	};

	isa->isa_en = v4l2_ctrl_new_custom(&isa->asd.ctrl_handler, &cfg, NULL);
}

int intel_ipu4_isys_isa_init(struct intel_ipu4_isys_isa *isa,
			  struct intel_ipu4_isys *isys, void __iomem *base)
{
	struct v4l2_subdev_format fmt = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
		.pad = ISA_PAD_SINK,
		.format = {
			.width = 4096,
			.height = 3072,
		},
	};
	struct v4l2_subdev_format fmt_config = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
		.pad = ISA_PAD_CONFIG,
	};
	struct v4l2_subdev_format fmt_3a = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
		.pad = ISA_PAD_3A,
	};
	int rval;

	isa->base = base;

	isa->asd.sd.entity.ops = &isa_entity_ops;
	isa->asd.ctrl_init = isa_ctrl_init;
	isa->asd.isys = isys;

	rval = intel_ipu4_isys_subdev_init(&isa->asd, &isa_sd_ops, 1,
				NR_OF_ISA_PADS, NR_OF_ISA_STREAMS,
				NR_OF_ISA_SOURCE_PADS, NR_OF_ISA_SINK_PADS,
				0);
	if (rval)
		goto fail;

	isa->asd.pad[ISA_PAD_SINK].flags = MEDIA_PAD_FL_SINK
		| MEDIA_PAD_FL_MUST_CONNECT;
	isa->asd.pad[ISA_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	isa->asd.valid_tgts[ISA_PAD_SOURCE].crop = true;
	isa->asd.pad[ISA_PAD_CONFIG].flags = MEDIA_PAD_FL_SINK
		| MEDIA_PAD_FL_MUST_CONNECT;
	isa->asd.pad[ISA_PAD_3A].flags = MEDIA_PAD_FL_SOURCE;
	isa->asd.pad[ISA_PAD_SOURCE_SCALED].flags = MEDIA_PAD_FL_SOURCE;
	isa->asd.valid_tgts[ISA_PAD_SOURCE_SCALED].compose = true;
	isa->asd.valid_tgts[ISA_PAD_SOURCE_SCALED].crop = true;

	isa->asd.isl_mode = INTEL_IPU4_ISL_ISA;
	isa->asd.supported_codes = isa_supported_codes;
	isa->asd.set_ffmt = isa_set_ffmt;
	intel_ipu4_isys_subdev_set_ffmt(&isa->asd.sd, NULL, &fmt);
	intel_ipu4_isys_subdev_set_ffmt(&isa->asd.sd, NULL, &fmt_config);
	intel_ipu4_isys_subdev_set_ffmt(&isa->asd.sd, NULL, &fmt_3a);

	isa->asd.sd.internal_ops = &isa_sd_internal_ops;
	snprintf(isa->asd.sd.name, sizeof(isa->asd.sd.name),
		 INTEL_IPU4_ISYS_ENTITY_PREFIX " ISA");
	v4l2_set_subdevdata(&isa->asd.sd, &isa->asd);
	rval = v4l2_device_register_subdev(&isys->v4l2_dev, &isa->asd.sd);
	if (rval) {
		dev_info(&isys->adev->dev, "can't register v4l2 subdev\n");
		goto fail;
	}

	snprintf(isa->av.vdev.name, sizeof(isa->av.vdev.name),
		 INTEL_IPU4_ISYS_ENTITY_PREFIX " ISA capture");
	isa->av.isys = isys;
	isa->av.aq.css_pin_type = IPU_FW_ISYS_PIN_TYPE_RAW_NS;
	isa->av.pfmts = intel_ipu4_isys_pfmts;
	isa->av.try_fmt_vid_mplane =
		intel_ipu4_isys_video_try_fmt_vid_mplane_default;
	isa->av.prepare_firmware_stream_cfg =
		isa_prepare_firmware_stream_cfg;
	isa->av.aq.buf_prepare = intel_ipu4_isys_buf_prepare;
	isa->av.aq.fill_frame_buff_set_pin =
		intel_ipu4_isys_buffer_list_to_ipu_fw_isys_frame_buff_set_pin;
	isa->av.aq.link_fmt_validate = intel_ipu4_isys_link_fmt_validate;
	isa->av.aq.vbq.buf_struct_size =
		sizeof(struct intel_ipu4_isys_video_buffer);

	rval = intel_ipu4_isys_video_init(
		&isa->av, &isa->asd.sd.entity, ISA_PAD_SOURCE,
		MEDIA_PAD_FL_SINK, 0);
	if (rval) {
		dev_info(&isys->adev->dev, "can't init video node\n");
		goto fail;
	}

	snprintf(isa->av_config.vdev.name, sizeof(isa->av_config.vdev.name),
		 INTEL_IPU4_ISYS_ENTITY_PREFIX " ISA config");
	isa->av_config.isys = isys;
	isa->av_config.pfmts = isa_config_pfmts;
	isa->av_config.try_fmt_vid_mplane = isa_config_try_fmt_vid_out_mplane;
	isa->av_config.prepare_firmware_stream_cfg =
		isa_prepare_firmware_stream_cfg_param;
	isa->av_config.vdev.ioctl_ops = &isa_config_ioctl_ops;
	isa->av_config.aq.buf_init = isa_config_buf_init;
	isa->av_config.aq.buf_cleanup = isa_config_buf_cleanup;
	isa->av_config.aq.buf_prepare = isa_terminal_buf_prepare;
	isa->av_config.aq.prepare_frame_buff_set =
		isa_config_prepare_frame_buff_set;
	isa->av_config.aq.fill_frame_buff_set_pin =
		isa_config_fill_frame_buff_set_pin;
	isa->av_config.aq.link_fmt_validate =
		intel_ipu4_isys_link_fmt_validate;
	isa->av_config.aq.vbq.io_modes = VB2_MMAP;
	isa->av_config.aq.vbq.buf_struct_size =
		sizeof(struct intel_ipu4_isys_isa_buffer);

	rval = intel_ipu4_isys_video_init(
		&isa->av_config, &isa->asd.sd.entity, ISA_PAD_CONFIG,
		MEDIA_PAD_FL_SOURCE, 0);
	if (rval) {
		dev_info(&isys->adev->dev, "can't init video node\n");
		goto fail;
	}

	snprintf(isa->av_3a.vdev.name, sizeof(isa->av_3a.vdev.name),
		 INTEL_IPU4_ISYS_ENTITY_PREFIX " ISA 3A stats");
	isa->av_3a.isys = isys;
	isa->av_3a.pfmts = isa_config_pfmts;
	isa->av_3a.try_fmt_vid_mplane = isa_config_try_fmt_vid_out_mplane;
	isa->av_3a.prepare_firmware_stream_cfg =
		isa_prepare_firmware_stream_cfg_param;
	isa->av_3a.vdev.ioctl_ops = &isa_config_ioctl_ops;
	isa->av_3a.aq.buf_init = isa_3a_buf_init;
	isa->av_3a.aq.buf_cleanup = isa_3a_buf_cleanup;
	isa->av_3a.aq.buf_prepare = isa_terminal_buf_prepare;
	isa->av_3a.aq.prepare_frame_buff_set =
		isa_config_prepare_frame_buff_set;
	isa->av_3a.aq.link_fmt_validate = intel_ipu4_isys_link_fmt_validate;
	isa->av_3a.aq.vbq.io_modes = VB2_MMAP;
	isa->av_3a.aq.vbq.buf_struct_size =
		sizeof(struct intel_ipu4_isys_isa_buffer);
	isa->av_3a.line_header_length = 4; /* Set to non-zero to force mplane*/

	rval = intel_ipu4_isys_video_init(
		&isa->av_3a, &isa->asd.sd.entity, ISA_PAD_3A,
		MEDIA_PAD_FL_SINK, 0);
	if (rval) {
		dev_info(&isys->adev->dev, "can't init video node\n");
		goto fail;
	}

	snprintf(isa->av_scaled.vdev.name, sizeof(isa->av_scaled.vdev.name),
		 INTEL_IPU4_ISYS_ENTITY_PREFIX " ISA scaled capture");
	isa->av_scaled.isys = isys;
	isa->av_scaled.aq.css_pin_type = IPU_FW_ISYS_PIN_TYPE_RAW_S;
	isa->av_scaled.pfmts = isa->av.pfmts;
	isa->av_scaled.try_fmt_vid_mplane =
		intel_ipu4_isys_video_try_fmt_vid_mplane_default;
	isa->av_scaled.prepare_firmware_stream_cfg =
		isa_prepare_firmware_stream_cfg;
	isa->av_scaled.aq.buf_prepare = intel_ipu4_isys_buf_prepare;
	isa->av_scaled.aq.fill_frame_buff_set_pin =
		intel_ipu4_isys_buffer_list_to_ipu_fw_isys_frame_buff_set_pin;
	isa->av_scaled.aq.link_fmt_validate = intel_ipu4_isys_link_fmt_validate;
	isa->av_scaled.aq.vbq.buf_struct_size =
		sizeof(struct intel_ipu4_isys_video_buffer);

	rval = intel_ipu4_isys_video_init(
		&isa->av_scaled, &isa->asd.sd.entity, ISA_PAD_SOURCE_SCALED,
		MEDIA_PAD_FL_SINK, 0);
	if (rval) {
		dev_info(&isys->adev->dev, "can't init video node\n");
		goto fail;
	}

	return 0;

fail:
	intel_ipu4_isys_isa_cleanup(isa);

	return rval;
}
