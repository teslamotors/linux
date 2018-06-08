// SPDX-License_Identifier: GPL-2.0
// Copyright (C) 2013 - 2018 Intel Corporation

#include <linux/device.h>
#include <linux/module.h>

#include <media/ipu-isys.h>
#include <media/media-entity.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>

#include "ipu.h"
#include "ipu-bus.h"
#include "ipu-buttress.h"
#include "ipu-isys.h"
#include "ipu-isys-subdev.h"
#include "ipu-isys-video.h"
#include "ipu-platform-regs.h"

#define CREATE_TRACE_POINTS
#define IPU_SOF_SEQID_TRACE
#define IPU_EOF_SEQID_TRACE
#include "ipu-trace-event.h"

static const u32 csi2_supported_codes_pad_sink[] = {
	MEDIA_BUS_FMT_Y10_1X10,
	MEDIA_BUS_FMT_RGB565_1X16,
	MEDIA_BUS_FMT_RGB888_1X24,
	MEDIA_BUS_FMT_UYVY8_1X16,
	MEDIA_BUS_FMT_YUYV8_1X16,
	MEDIA_BUS_FMT_SBGGR10_1X10,
	MEDIA_BUS_FMT_SGBRG10_1X10,
	MEDIA_BUS_FMT_SGRBG10_1X10,
	MEDIA_BUS_FMT_SRGGB10_1X10,
	MEDIA_BUS_FMT_SBGGR10_DPCM8_1X8,
	MEDIA_BUS_FMT_SGBRG10_DPCM8_1X8,
	MEDIA_BUS_FMT_SGRBG10_DPCM8_1X8,
	MEDIA_BUS_FMT_SRGGB10_DPCM8_1X8,
	MEDIA_BUS_FMT_SBGGR12_1X12,
	MEDIA_BUS_FMT_SGBRG12_1X12,
	MEDIA_BUS_FMT_SGRBG12_1X12,
	MEDIA_BUS_FMT_SRGGB12_1X12,
	MEDIA_BUS_FMT_SBGGR14_1X14,
	MEDIA_BUS_FMT_SGBRG14_1X14,
	MEDIA_BUS_FMT_SGRBG14_1X14,
	MEDIA_BUS_FMT_SRGGB14_1X14,
	MEDIA_BUS_FMT_SBGGR8_1X8,
	MEDIA_BUS_FMT_SGBRG8_1X8,
	MEDIA_BUS_FMT_SGRBG8_1X8,
	MEDIA_BUS_FMT_SRGGB8_1X8,
	0,
};

static const u32 csi2_supported_codes_pad_source[] = {
	MEDIA_BUS_FMT_Y10_1X10,
	MEDIA_BUS_FMT_RGB565_1X16,
	MEDIA_BUS_FMT_RGB888_1X24,
	MEDIA_BUS_FMT_UYVY8_1X16,
	MEDIA_BUS_FMT_YUYV8_1X16,
	MEDIA_BUS_FMT_SBGGR10_1X10,
	MEDIA_BUS_FMT_SGBRG10_1X10,
	MEDIA_BUS_FMT_SGRBG10_1X10,
	MEDIA_BUS_FMT_SRGGB10_1X10,
	MEDIA_BUS_FMT_SBGGR12_1X12,
	MEDIA_BUS_FMT_SGBRG12_1X12,
	MEDIA_BUS_FMT_SGRBG12_1X12,
	MEDIA_BUS_FMT_SRGGB12_1X12,
	MEDIA_BUS_FMT_SBGGR14_1X14,
	MEDIA_BUS_FMT_SGBRG14_1X14,
	MEDIA_BUS_FMT_SGRBG14_1X14,
	MEDIA_BUS_FMT_SRGGB14_1X14,
	MEDIA_BUS_FMT_SBGGR8_1X8,
	MEDIA_BUS_FMT_SGBRG8_1X8,
	MEDIA_BUS_FMT_SGRBG8_1X8,
	MEDIA_BUS_FMT_SRGGB8_1X8,
	0,
};

#ifdef IPU_META_DATA_SUPPORT
static const u32 csi2_supported_codes_pad_meta[] = {
	MEDIA_BUS_FMT_FIXED,
	0,
};
#endif

static const u32 *csi2_supported_codes[NR_OF_CSI2_PADS];

static struct v4l2_subdev_internal_ops csi2_sd_internal_ops = {
	.open = ipu_isys_subdev_open,
	.close = ipu_isys_subdev_close,
};

int ipu_isys_csi2_get_link_freq(struct ipu_isys_csi2 *csi2, __s64 *link_freq)
{
	struct ipu_isys_pipeline *pipe = container_of(csi2->asd.sd.entity.pipe,
						      struct ipu_isys_pipeline,
						      pipe);
	struct v4l2_subdev *ext_sd =
	    media_entity_to_v4l2_subdev(pipe->external->entity);
	struct v4l2_ext_control c = {.id = V4L2_CID_LINK_FREQ, };
	struct v4l2_ext_controls cs = {.count = 1,
		.controls = &c,
	};
	struct v4l2_querymenu qm = {.id = c.id, };
	int rval;

	if (!ext_sd) {
		WARN_ON(1);
		return -ENODEV;
	}
	rval = v4l2_g_ext_ctrls(ext_sd->ctrl_handler, &cs);
	if (rval) {
		dev_info(&csi2->isys->adev->dev, "can't get link frequency\n");
		return rval;
	}

	qm.index = c.value;

	rval = v4l2_querymenu(ext_sd->ctrl_handler, &qm);
	if (rval) {
		dev_info(&csi2->isys->adev->dev, "can't get menu item\n");
		return rval;
	}

	dev_dbg(&csi2->isys->adev->dev, "%s: link frequency %lld\n", __func__,
		qm.value);

	if (!qm.value)
		return -EINVAL;
	*link_freq = qm.value;
	return 0;
}

#ifdef IPU_META_DATA_SUPPORT
static int ipu_get_frame_desc_entry_by_dt(struct v4l2_subdev *sd,
					  struct v4l2_mbus_frame_desc_entry
					  *entry, u8 data_type)
{
	struct v4l2_mbus_frame_desc desc;
	int rval, i;

	rval = v4l2_subdev_call(sd, pad, get_frame_desc, 0, &desc);
	if (rval)
		return rval;

	for (i = 0; i < desc.num_entries; i++) {
		if (desc.entry[i].bus.csi2.data_type != data_type)
			continue;
		*entry = desc.entry[i];
		return 0;
	}

	return -EINVAL;
}

static void csi2_meta_prepare_firmware_stream_cfg_default(
			struct ipu_isys_video *av,
			struct ipu_fw_isys_stream_cfg_data_abi *cfg)
{
	struct ipu_isys_pipeline *ip =
	    to_ipu_isys_pipeline(av->vdev.entity.pipe);
	struct ipu_isys_queue *aq = &av->aq;
	struct ipu_fw_isys_output_pin_info_abi *pin_info;
	struct v4l2_mbus_frame_desc_entry entry;
	int pin = cfg->nof_output_pins++;
	int inpin = cfg->nof_input_pins++;
	int rval;

	aq->fw_output = pin;
	ip->output_pins[pin].pin_ready = ipu_isys_queue_buf_ready;
	ip->output_pins[pin].aq = aq;

	pin_info = &cfg->output_pins[pin];
	pin_info->input_pin_id = inpin;
	pin_info->output_res.width = av->mpix.width;
	pin_info->output_res.height = av->mpix.height;
	pin_info->stride = av->mpix.plane_fmt[0].bytesperline;
	pin_info->pt = aq->css_pin_type;
	pin_info->ft = av->pfmt->css_pixelformat;
	pin_info->send_irq = 1;

	rval =
	    ipu_get_frame_desc_entry_by_dt(media_entity_to_v4l2_subdev
					   (ip->external->entity), &entry,
					   IPU_ISYS_MIPI_CSI2_TYPE_EMBEDDED8);
	if (!rval) {
		cfg->input_pins[inpin].dt = IPU_ISYS_MIPI_CSI2_TYPE_EMBEDDED8;
		cfg->input_pins[inpin].input_res.width =
		    entry.size.two_dim.width * entry.bpp / BITS_PER_BYTE;
		cfg->input_pins[inpin].input_res.height =
		    entry.size.two_dim.height;
	}
}
#endif

static int subscribe_event(struct v4l2_subdev *sd, struct v4l2_fh *fh,
			   struct v4l2_event_subscription *sub)
{
	struct ipu_isys_csi2 *csi2 = to_ipu_isys_csi2(sd);

	dev_dbg(&csi2->isys->adev->dev, "subscribe event(type %u id %u)\n",
		sub->type, sub->id);

	switch (sub->type) {
	case V4L2_EVENT_FRAME_SYNC:
		return v4l2_event_subscribe(fh, sub, 10, NULL);
	case V4L2_EVENT_CTRL:
		return v4l2_ctrl_subscribe_event(fh, sub);
	default:
		return -EINVAL;
	}
}

static const struct v4l2_subdev_core_ops csi2_sd_core_ops = {
	.subscribe_event = subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

#ifdef IPU_META_DATA_SUPPORT
static struct ipu_isys_pixelformat csi2_meta_pfmts[] = {
	{V4L2_FMT_IPU_ISYS_META, 8, 8, 0, MEDIA_BUS_FMT_FIXED, 0},
	{},
};
#endif
/*
 * The input system CSI2+ receiver has several
 * parameters affecting the receiver timings. These depend
 * on the MIPI bus frequency F in Hz (sensor transmitter rate)
 * as follows:
 *	register value = (A/1e9 + B * UI) / COUNT_ACC
 * where
 *	UI = 1 / (2 * F) in seconds
 *	COUNT_ACC = counter accuracy in seconds
 *	For IPU4,  COUNT_ACC = 0.125 ns
 *
 * A and B are coefficients from the table below,
 * depending whether the register minimum or maximum value is
 * calculated.
 *				       Minimum     Maximum
 * Clock lane			       A     B     A     B
 * reg_rx_csi_dly_cnt_termen_clane     0     0    38     0
 * reg_rx_csi_dly_cnt_settle_clane    95    -8   300   -16
 * Data lanes
 * reg_rx_csi_dly_cnt_termen_dlane0    0     0    35     4
 * reg_rx_csi_dly_cnt_settle_dlane0   85    -2   145    -6
 * reg_rx_csi_dly_cnt_termen_dlane1    0     0    35     4
 * reg_rx_csi_dly_cnt_settle_dlane1   85    -2   145    -6
 * reg_rx_csi_dly_cnt_termen_dlane2    0     0    35     4
 * reg_rx_csi_dly_cnt_settle_dlane2   85    -2   145    -6
 * reg_rx_csi_dly_cnt_termen_dlane3    0     0    35     4
 * reg_rx_csi_dly_cnt_settle_dlane3   85    -2   145    -6
 *
 * We use the minimum values of both A and B.
 */

#define DIV_SHIFT	8

static uint32_t calc_timing(s32 a, int32_t b, int64_t link_freq, int32_t accinv)
{
	return accinv * a + (accinv * b * (500000000 >> DIV_SHIFT)
			     / (int32_t)(link_freq >> DIV_SHIFT));
}

static int
ipu_isys_csi2_calc_timing(struct ipu_isys_csi2 *csi2,
			  struct ipu_isys_csi2_timing *timing, uint32_t accinv)
{
	__s64 link_freq;
	int rval;

	rval = ipu_isys_csi2_get_link_freq(csi2, &link_freq);
	if (rval)
		return rval;

	timing->ctermen = calc_timing(CSI2_CSI_RX_DLY_CNT_TERMEN_CLANE_A,
				      CSI2_CSI_RX_DLY_CNT_TERMEN_CLANE_B,
				      link_freq, accinv);
	timing->csettle = calc_timing(CSI2_CSI_RX_DLY_CNT_SETTLE_CLANE_A,
				      CSI2_CSI_RX_DLY_CNT_SETTLE_CLANE_B,
				      link_freq, accinv);
	dev_dbg(&csi2->isys->adev->dev, "ctermen %u\n", timing->ctermen);
	dev_dbg(&csi2->isys->adev->dev, "csettle %u\n", timing->csettle);

	timing->dtermen = calc_timing(CSI2_CSI_RX_DLY_CNT_TERMEN_DLANE_A,
				      CSI2_CSI_RX_DLY_CNT_TERMEN_DLANE_B,
				      link_freq, accinv);
	timing->dsettle = calc_timing(CSI2_CSI_RX_DLY_CNT_SETTLE_DLANE_A,
				      CSI2_CSI_RX_DLY_CNT_SETTLE_DLANE_B,
				      link_freq, accinv);
	dev_dbg(&csi2->isys->adev->dev, "dtermen %u\n", timing->dtermen);
	dev_dbg(&csi2->isys->adev->dev, "dsettle %u\n", timing->dsettle);

	return 0;
}

#define CSI2_ACCINV	8

static int set_stream(struct v4l2_subdev *sd, int enable)
{
	struct ipu_isys_csi2 *csi2 = to_ipu_isys_csi2(sd);
	struct ipu_isys_pipeline *ip = container_of(sd->entity.pipe,
						    struct ipu_isys_pipeline,
						    pipe);
	struct ipu_isys_csi2_config *cfg;
	struct v4l2_subdev *ext_sd;
	struct v4l2_control c = {.id = V4L2_CID_MIPI_LANES, };
	struct ipu_isys_csi2_timing timing;
	unsigned int nlanes;
	int rval;

	dev_dbg(&csi2->isys->adev->dev, "csi2 s_stream %d\n", enable);

	if (!ip->external->entity) {
		WARN_ON(1);
		return -ENODEV;
	}
	ext_sd = media_entity_to_v4l2_subdev(ip->external->entity);
	cfg = v4l2_get_subdev_hostdata(ext_sd);

	if (!enable) {
		csi2->stream_count--;
		if (csi2->stream_count)
			return 0;

		ipu_isys_csi2_set_stream(sd, timing, 0, enable);
		return 0;
	}

	ip->has_sof = true;

	if (csi2->stream_count) {
		csi2->stream_count++;
		return 0;
	}

	rval = v4l2_g_ctrl(ext_sd->ctrl_handler, &c);
	if (!rval && c.value > 0 && cfg->nlanes > c.value) {
		nlanes = c.value;
		dev_dbg(&csi2->isys->adev->dev, "lane nr %d.\n", nlanes);
	} else {
		nlanes = cfg->nlanes;
	}

	rval = ipu_isys_csi2_calc_timing(csi2, &timing, CSI2_ACCINV);
	if (rval)
		return rval;

	ipu_isys_csi2_set_stream(sd, timing, nlanes, enable);
	csi2->stream_count++;

	return 0;
}

static void csi2_capture_done(struct ipu_isys_pipeline *ip,
			      struct ipu_fw_isys_resp_info_abi *info)
{
	if (ip->interlaced && ip->isys->short_packet_source ==
	    IPU_ISYS_SHORT_PACKET_FROM_RECEIVER) {
		struct ipu_isys_buffer *ib;
		unsigned long flags;

		spin_lock_irqsave(&ip->short_packet_queue_lock, flags);
		if (!list_empty(&ip->short_packet_active)) {
			ib = list_last_entry(&ip->short_packet_active,
					     struct ipu_isys_buffer, head);
			list_move(&ib->head, &ip->short_packet_incoming);
		}
		spin_unlock_irqrestore(&ip->short_packet_queue_lock, flags);
	}
	if (ip->csi2) {
		ipu_isys_csi2_error(ip->csi2);
	}
}

static int csi2_link_validate(struct media_link *link)
{
	struct ipu_isys_csi2 *csi2;
	struct ipu_isys_pipeline *ip;
	struct v4l2_subdev_route r[IPU_ISYS_MAX_STREAMS];
	struct v4l2_subdev_routing routing = {
		.routes = r,
		.num_routes = IPU_ISYS_MAX_STREAMS,
	};
	unsigned int active = 0;
	int i;
	int rval;

	if (!link->sink->entity ||
	    !link->sink->entity->pipe || !link->source->entity)
		return -EINVAL;
	csi2 =
	    to_ipu_isys_csi2(media_entity_to_v4l2_subdev(link->sink->entity));
	ip = to_ipu_isys_pipeline(link->sink->entity->pipe);
	csi2->receiver_errors = 0;
	ip->csi2 = csi2;
	ipu_isys_video_add_capture_done(to_ipu_isys_pipeline
					(link->sink->entity->pipe),
					csi2_capture_done);

	rval = v4l2_subdev_link_validate(link);
	if (rval)
		return rval;

	if (!v4l2_ctrl_g_ctrl(csi2->store_csi2_header)) {
		for (i = 0; i < NR_OF_CSI2_SOURCE_PADS; i++) {
			struct media_pad *remote_pad =
			    media_entity_remote_pad(&csi2->asd.
						    pad[CSI2_PAD_SOURCE(i)]);

			if (remote_pad &&
			    is_media_entity_v4l2_subdev(remote_pad->entity)) {
				dev_err(&csi2->isys->adev->dev,
					"CSI2 BE requires CSI2 headers.\n");
				return -EINVAL;
			}
		}
	}

	rval =
	    v4l2_subdev_call(media_entity_to_v4l2_subdev(link->source->entity),
			     pad, get_routing, &routing);

	if (rval) {
		csi2->remote_streams = 1;
		return 0;
	}

	for (i = 0; i < routing.num_routes; i++) {
		if (routing.routes[i].flags & V4L2_SUBDEV_ROUTE_FL_ACTIVE)
			active++;
	}

	if (active !=
	    bitmap_weight(csi2->asd.stream[link->sink->index].streams_stat, 32))
		return -EINVAL;

	csi2->remote_streams = active;

	return 0;
}

static bool csi2_has_route(struct media_entity *entity, unsigned int pad0,
			   unsigned int pad1, int *stream)
{
#ifdef IPU_META_DATA_SUPPORT
	if (pad0 == CSI2_PAD_META || pad1 == CSI2_PAD_META)
		return true;
#endif
	return ipu_isys_subdev_has_route(entity, pad0, pad1, stream);
}

static const struct v4l2_subdev_video_ops csi2_sd_video_ops = {
	.s_stream = set_stream,
};

#ifdef IPU_META_DATA_SUPPORT
static int get_metadata_fmt(struct v4l2_subdev *sd,
			    struct v4l2_subdev_pad_config *cfg,
			    struct v4l2_subdev_format *fmt)
{
	struct media_pad *pad =
	    media_entity_remote_pad(&sd->entity.pads[CSI2_PAD_SINK]);
	struct v4l2_mbus_frame_desc_entry entry;
	int rval;

	if (!pad)
		return -EINVAL;

	rval =
	    ipu_get_frame_desc_entry_by_dt(media_entity_to_v4l2_subdev
					   (pad->entity), &entry,
					   IPU_ISYS_MIPI_CSI2_TYPE_EMBEDDED8);

	if (!rval) {
		fmt->format.width =
		    entry.size.two_dim.width * entry.bpp / BITS_PER_BYTE;
		fmt->format.height = entry.size.two_dim.height;
		fmt->format.code = entry.pixelcode;
		fmt->format.field = V4L2_FIELD_NONE;
	}
	return rval;
}
#endif

static int ipu_isys_csi2_get_fmt(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_format *fmt)
{
#ifdef IPU_META_DATA_SUPPORT
	if (fmt->pad == CSI2_PAD_META)
		return get_metadata_fmt(sd, cfg, fmt);
#endif
	return ipu_isys_subdev_get_ffmt(sd, cfg, fmt);
}

static int ipu_isys_csi2_set_fmt(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_format *fmt)
{
#ifdef IPU_META_DATA_SUPPORT
	if (fmt->pad == CSI2_PAD_META)
		return get_metadata_fmt(sd, cfg, fmt);
#endif
	return ipu_isys_subdev_set_ffmt(sd, cfg, fmt);
}

static int __subdev_link_validate(struct v4l2_subdev *sd,
				  struct media_link *link,
				  struct v4l2_subdev_format *source_fmt,
				  struct v4l2_subdev_format *sink_fmt)
{
	struct ipu_isys_pipeline *ip = container_of(sd->entity.pipe,
						    struct ipu_isys_pipeline,
						    pipe);

	if (source_fmt->format.field == V4L2_FIELD_ALTERNATE)
		ip->interlaced = true;

	return ipu_isys_subdev_link_validate(sd, link, source_fmt, sink_fmt);
}

static const struct v4l2_subdev_pad_ops csi2_sd_pad_ops = {
	.link_validate = __subdev_link_validate,
	.get_fmt = ipu_isys_csi2_get_fmt,
	.set_fmt = ipu_isys_csi2_set_fmt,
	.enum_mbus_code = ipu_isys_subdev_enum_mbus_code,
	.set_routing = ipu_isys_subdev_set_routing,
	.get_routing = ipu_isys_subdev_get_routing,
};

static struct v4l2_subdev_ops csi2_sd_ops = {
	.core = &csi2_sd_core_ops,
	.video = &csi2_sd_video_ops,
	.pad = &csi2_sd_pad_ops,
};

static struct media_entity_operations csi2_entity_ops = {
	.link_validate = csi2_link_validate,
	.has_route = csi2_has_route,
};

static void csi2_set_ffmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *ffmt =
		__ipu_isys_get_ffmt(sd, cfg, fmt->pad,
				    fmt->stream,
				    fmt->which);

	if (fmt->format.field != V4L2_FIELD_ALTERNATE)
		fmt->format.field = V4L2_FIELD_NONE;

	if (fmt->pad == CSI2_PAD_SINK) {
		*ffmt = fmt->format;
		if (fmt->stream)
			return;
		ipu_isys_subdev_fmt_propagate(
			sd, cfg, &fmt->format, NULL,
			IPU_ISYS_SUBDEV_PROP_TGT_SINK_FMT,
			fmt->pad, fmt->which);
		return;
	}

#ifdef IPU_META_DATA_SUPPORT
	if (fmt->pad == CSI2_PAD_META) {
		struct v4l2_mbus_framefmt *ffmt =
			__ipu_isys_get_ffmt(sd, cfg, fmt->pad,
					    fmt->stream,
					    fmt->which);
		struct media_pad *pad = media_entity_remote_pad(
			&sd->entity.pads[CSI2_PAD_SINK]);
		struct v4l2_mbus_frame_desc_entry entry;
		int rval;

		if (!pad) {
			ffmt->width = 0;
			ffmt->height = 0;
			ffmt->code = 0;
			return;
		}

		rval = ipu_get_frame_desc_entry_by_dt(
				media_entity_to_v4l2_subdev(pad->entity),
				&entry,
				IPU_ISYS_MIPI_CSI2_TYPE_EMBEDDED8);

		if (!rval) {
			ffmt->width = entry.size.two_dim.width * entry.bpp
			    / BITS_PER_BYTE;
			ffmt->height = entry.size.two_dim.height;
			ffmt->code = entry.pixelcode;
			ffmt->field = V4L2_FIELD_NONE;
		}

		return;
	}
#endif
	if (sd->entity.pads[fmt->pad].flags & MEDIA_PAD_FL_SOURCE) {
		ffmt->width = fmt->format.width;
		ffmt->height = fmt->format.height;
		ffmt->field = fmt->format.field;
		ffmt->code =
		    ipu_isys_subdev_code_to_uncompressed(fmt->format.code);
		return;
	}

	WARN_ON(1);
}

static const struct ipu_isys_pixelformat *
csi2_try_fmt(struct ipu_isys_video *av,
	     struct v4l2_pix_format_mplane *mpix)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
	struct v4l2_subdev *sd =
	    media_entity_to_v4l2_subdev(av->vdev.entity.links[0].source->
					entity);
#else
	struct media_link *link = list_first_entry(&av->vdev.entity.links,
						   struct media_link, list);
	struct v4l2_subdev *sd =
	    media_entity_to_v4l2_subdev(link->source->entity);
#endif
	struct ipu_isys_csi2 *csi2;

	if (!sd)
		return NULL;

	csi2 = to_ipu_isys_csi2(sd);

	return ipu_isys_video_try_fmt_vid_mplane(av, mpix,
				v4l2_ctrl_g_ctrl(csi2->store_csi2_header));
}

void ipu_isys_csi2_cleanup(struct ipu_isys_csi2 *csi2)
{
	int i;

	if (!csi2->isys)
		return;

	v4l2_device_unregister_subdev(&csi2->asd.sd);
	ipu_isys_subdev_cleanup(&csi2->asd);
	for (i = 0; i < NR_OF_CSI2_SOURCE_PADS; i++)
		ipu_isys_video_cleanup(&csi2->av[i]);
	
#ifdef IPU_META_DATA_SUPPORT
	ipu_isys_video_cleanup(&csi2->av_meta);
#endif
	csi2->isys = NULL;
}

static void csi_ctrl_init(struct v4l2_subdev *sd)
{
	struct ipu_isys_csi2 *csi2 = to_ipu_isys_csi2(sd);

	static const struct v4l2_ctrl_config cfg = {
		.id = V4L2_CID_IPU_STORE_CSI2_HEADER,
		.name = "Store CSI-2 Headers",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.min = 0,
		.max = 1,
		.step = 1,
		.def = 1,
	};

	csi2->store_csi2_header = v4l2_ctrl_new_custom(&csi2->asd.ctrl_handler,
						       &cfg, NULL);
}

int ipu_isys_csi2_init(struct ipu_isys_csi2 *csi2,
		       struct ipu_isys *isys,
		       void __iomem *base, unsigned int index)
{
	struct v4l2_subdev_format fmt = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
		.pad = CSI2_PAD_SINK,
		.format = {
			   .width = 4096,
			   .height = 3072,
			  },
	};
#ifdef IPU_META_DATA_SUPPORT
	struct v4l2_subdev_format fmt_meta = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
		.pad = CSI2_PAD_META,
	};
#endif
	int i, rval, src;

	csi2->isys = isys;
	csi2->base = base;
	csi2->index = index;

	csi2->asd.sd.entity.ops = &csi2_entity_ops;
	csi2->asd.ctrl_init = csi_ctrl_init;
	csi2->asd.isys = isys;
	init_completion(&csi2->eof_completion);
	csi2->remote_streams = 1;
	csi2->stream_count = 0;

	rval = ipu_isys_subdev_init(&csi2->asd, &csi2_sd_ops, 0,
				    NR_OF_CSI2_PADS,
				    NR_OF_CSI2_STREAMS,
				    NR_OF_CSI2_SOURCE_PADS,
				    NR_OF_CSI2_SINK_PADS,
				    V4L2_SUBDEV_FL_HAS_SUBSTREAMS);
	if (rval)
		goto fail;

	csi2->asd.pad[CSI2_PAD_SINK].flags = MEDIA_PAD_FL_SINK
	    | MEDIA_PAD_FL_MUST_CONNECT | MEDIA_PAD_FL_MULTIPLEX;
	for (i = CSI2_PAD_SOURCE(0);
	     i < (NR_OF_CSI2_SOURCE_PADS + CSI2_PAD_SOURCE(0)); i++)
		csi2->asd.pad[i].flags = MEDIA_PAD_FL_SOURCE;

#ifdef IPU_META_DATA_SUPPORT
	csi2->asd.pad[CSI2_PAD_META].flags = MEDIA_PAD_FL_SOURCE;
#endif
	src = index;
#ifdef CONFIG_VIDEO_INTEL_IPU4P
	src = index ? (index + 5) : (index + 3);
#endif
	csi2->asd.source = IPU_FW_ISYS_STREAM_SRC_CSI2_PORT0 + src;
	csi2_supported_codes[CSI2_PAD_SINK] = csi2_supported_codes_pad_sink;

	for (i = 0; i < NR_OF_CSI2_SOURCE_PADS; i++)
		csi2_supported_codes[i + 1] = csi2_supported_codes_pad_source;
#ifdef IPU_META_DATA_SUPPORT
	csi2_supported_codes[CSI2_PAD_META] = csi2_supported_codes_pad_meta;
#endif
	csi2->asd.supported_codes = csi2_supported_codes;
	csi2->asd.set_ffmt = csi2_set_ffmt;

	csi2->asd.sd.flags |= V4L2_SUBDEV_FL_HAS_EVENTS;
	csi2->asd.sd.internal_ops = &csi2_sd_internal_ops;
	snprintf(csi2->asd.sd.name, sizeof(csi2->asd.sd.name),
		 IPU_ISYS_ENTITY_PREFIX " CSI-2 %u", index);
	v4l2_set_subdevdata(&csi2->asd.sd, &csi2->asd);

	mutex_lock(&csi2->asd.mutex);
	rval = v4l2_device_register_subdev(&isys->v4l2_dev, &csi2->asd.sd);
	if (rval) {
		mutex_unlock(&csi2->asd.mutex);
		dev_info(&isys->adev->dev, "can't register v4l2 subdev\n");
		goto fail;
	}

	__ipu_isys_subdev_set_ffmt(&csi2->asd.sd, NULL, &fmt);
#ifdef IPU_META_DATA_SUPPORT
	__ipu_isys_subdev_set_ffmt(&csi2->asd.sd, NULL, &fmt_meta);
#endif

	/* create default route information */
	for (i = 0; i < NR_OF_CSI2_STREAMS; i++) {
		csi2->asd.route[i].sink = CSI2_PAD_SINK;
		csi2->asd.route[i].source = CSI2_PAD_SOURCE(i);
		csi2->asd.route[i].flags = 0;
	}

	for (i = 0; i < NR_OF_CSI2_SOURCE_PADS; i++) {
		csi2->asd.stream[CSI2_PAD_SINK].stream_id[i] = i;
		csi2->asd.stream[CSI2_PAD_SOURCE(i)].stream_id[CSI2_PAD_SINK]
		    = i;
	}
	csi2->asd.route[0].flags = V4L2_SUBDEV_ROUTE_FL_ACTIVE |
	    V4L2_SUBDEV_ROUTE_FL_IMMUTABLE;
	bitmap_set(csi2->asd.stream[CSI2_PAD_SINK].streams_stat, 0, 1);
	bitmap_set(csi2->asd.stream[CSI2_PAD_SOURCE(0)].streams_stat, 0, 1);

	mutex_unlock(&csi2->asd.mutex);

	for (i = 0; i < NR_OF_CSI2_SOURCE_PADS; i++) {
		snprintf(csi2->av[i].vdev.name, sizeof(csi2->av[i].vdev.name),
			 IPU_ISYS_ENTITY_PREFIX " CSI-2 %u capture %d",
			 index, i);
		csi2->av[i].isys = isys;
		csi2->av[i].aq.css_pin_type = IPU_FW_ISYS_PIN_TYPE_MIPI;
		csi2->av[i].pfmts = ipu_isys_pfmts_packed;
		csi2->av[i].try_fmt_vid_mplane = csi2_try_fmt;
		csi2->av[i].prepare_firmware_stream_cfg =
		    ipu_isys_prepare_firmware_stream_cfg_default;
		csi2->av[i].packed = true;
		csi2->av[i].line_header_length =
		    IPU_ISYS_CSI2_LONG_PACKET_HEADER_SIZE;
		csi2->av[i].line_footer_length =
		    IPU_ISYS_CSI2_LONG_PACKET_FOOTER_SIZE;
		csi2->av[i].aq.buf_prepare = ipu_isys_buf_prepare;
		csi2->av[i].aq.fill_frame_buff_set_pin =
		    ipu_isys_buffer_list_to_ipu_fw_isys_frame_buff_set_pin;
		csi2->av[i].aq.link_fmt_validate = ipu_isys_link_fmt_validate;
		csi2->av[i].aq.vbq.buf_struct_size =
		    sizeof(struct ipu_isys_video_buffer);

		rval = ipu_isys_video_init(&csi2->av[i],
					   &csi2->asd.sd.entity,
					   CSI2_PAD_SOURCE(i),
					   MEDIA_PAD_FL_SINK, 0);
		if (rval) {
			dev_info(&isys->adev->dev, "can't init video node\n");
			goto fail;
		}
	}

#ifdef IPU_META_DATA_SUPPORT
	snprintf(csi2->av_meta.vdev.name, sizeof(csi2->av_meta.vdev.name),
		 IPU_ISYS_ENTITY_PREFIX " CSI-2 %u meta", index);
	csi2->av_meta.isys = isys;
	csi2->av_meta.aq.css_pin_type = IPU_FW_ISYS_PIN_TYPE_MIPI;
	csi2->av_meta.pfmts = csi2_meta_pfmts;
	csi2->av_meta.try_fmt_vid_mplane = csi2_try_fmt;
	csi2->av_meta.prepare_firmware_stream_cfg =
	    csi2_meta_prepare_firmware_stream_cfg_default;
	csi2->av_meta.packed = true;
	csi2->av_meta.line_header_length =
	    IPU_ISYS_CSI2_LONG_PACKET_HEADER_SIZE;
	csi2->av_meta.line_footer_length =
	    IPU_ISYS_CSI2_LONG_PACKET_FOOTER_SIZE;
	csi2->av_meta.aq.buf_prepare = ipu_isys_buf_prepare;
	csi2->av_meta.aq.fill_frame_buff_set_pin =
	    ipu_isys_buffer_list_to_ipu_fw_isys_frame_buff_set_pin;
	csi2->av_meta.aq.link_fmt_validate = ipu_isys_link_fmt_validate;
	csi2->av_meta.aq.vbq.buf_struct_size =
	    sizeof(struct ipu_isys_video_buffer);

	rval = ipu_isys_video_init(&csi2->av_meta, &csi2->asd.sd.entity,
				   CSI2_PAD_META, MEDIA_PAD_FL_SINK, 0);
	if (rval) {
		dev_info(&isys->adev->dev, "can't init metadata node\n");
		goto fail;
	}
#endif
	return 0;

fail:
	ipu_isys_csi2_cleanup(csi2);

	return rval;
}

void ipu_isys_csi2_sof_event(struct ipu_isys_csi2 *csi2, unsigned int vc)
{
	struct ipu_isys_pipeline *ip = NULL;
	struct v4l2_event ev = {
		.type = V4L2_EVENT_FRAME_SYNC,
	};
	struct video_device *vdev = csi2->asd.sd.devnode;
	unsigned long flags;
	unsigned int i;

	spin_lock_irqsave(&csi2->isys->lock, flags);
	csi2->in_frame[vc] = true;

	for (i = 0; i < IPU_ISYS_MAX_STREAMS; i++) {
		if (csi2->isys->pipes[i] &&
		    csi2->isys->pipes[i]->vc == vc &&
		    csi2->isys->pipes[i]->csi2 == csi2) {
			ip = csi2->isys->pipes[i];
			break;
		}
	}

	/* Pipe already vanished */
	if (!ip) {
		spin_unlock_irqrestore(&csi2->isys->lock, flags);
		return;
	}

	ev.u.frame_sync.frame_sequence = atomic_inc_return(&ip->sequence) - 1;
	ev.id = ip->stream_id;
	spin_unlock_irqrestore(&csi2->isys->lock, flags);

	trace_ipu_sof_seqid(ev.u.frame_sync.frame_sequence, csi2->index, vc);
	v4l2_event_queue(vdev, &ev);
	dev_dbg(&csi2->isys->adev->dev,
		"sof_event::csi2-%i sequence: %i, vc: %d, stream_id: %d\n",
		csi2->index, ev.u.frame_sync.frame_sequence, vc, ip->stream_id);
}

void ipu_isys_csi2_eof_event(struct ipu_isys_csi2 *csi2, unsigned int vc)
{
	struct ipu_isys_pipeline *ip = NULL;
	unsigned long flags;
	unsigned int i;
	u32 frame_sequence;

	spin_lock_irqsave(&csi2->isys->lock, flags);
	csi2->in_frame[vc] = false;
	if (csi2->wait_for_sync[vc])
		complete(&csi2->eof_completion);
	spin_unlock_irqrestore(&csi2->isys->lock, flags);

	for (i = 0; i < IPU_ISYS_MAX_STREAMS; i++) {
		if (csi2->isys->pipes[i] &&
		    csi2->isys->pipes[i]->vc == vc &&
		    csi2->isys->pipes[i]->csi2 == csi2) {
			ip = csi2->isys->pipes[i];
			break;
		}
	}

	if (ip) {
		frame_sequence = atomic_read(&ip->sequence);

		trace_ipu_eof_seqid(frame_sequence, csi2->index, vc);

		dev_dbg(&csi2->isys->adev->dev,
			"eof_event::csi2-%i sequence: %i, vc: %d, stream_id: %d\n",
			csi2->index, frame_sequence, vc, ip->stream_id);
	}
}

/* Call this function only _after_ the sensor has been stopped */
void ipu_isys_csi2_wait_last_eof(struct ipu_isys_csi2 *csi2)
{
	unsigned long flags, tout;
	unsigned int i;

	for (i = 0; i < NR_OF_CSI2_VC; i++) {
		spin_lock_irqsave(&csi2->isys->lock, flags);

		if (!csi2->in_frame[i]) {
			spin_unlock_irqrestore(&csi2->isys->lock, flags);
			continue;
		}

		reinit_completion(&csi2->eof_completion);
		csi2->wait_for_sync[i] = true;
		spin_unlock_irqrestore(&csi2->isys->lock, flags);
		tout = wait_for_completion_timeout(&csi2->eof_completion,
						   IPU_EOF_TIMEOUT_JIFFIES);
		if (!tout)
			dev_err(&csi2->isys->adev->dev,
				"csi2-%d: timeout at sync to eof of vc %d\n",
				csi2->index, i);
		csi2->wait_for_sync[i] = false;
	}
}

struct ipu_isys_buffer *ipu_isys_csi2_get_short_packet_buffer(struct
							      ipu_isys_pipeline
							      *ip)
{
	struct ipu_isys_buffer *ib;
	struct ipu_isys_private_buffer *pb;
	struct ipu_isys_mipi_packet_header *ph;

	if (list_empty(&ip->short_packet_incoming))
		return NULL;
	ib = list_last_entry(&ip->short_packet_incoming,
			     struct ipu_isys_buffer, head);
	pb = ipu_isys_buffer_to_private_buffer(ib);
	ph = (struct ipu_isys_mipi_packet_header *)pb->buffer;

	/* Fill the packet header with magic number. */
	ph->word_count = 0xffff;
	ph->dtype = 0xff;

	dma_sync_single_for_cpu(&ip->isys->adev->dev, pb->dma_addr,
				sizeof(*ph), DMA_BIDIRECTIONAL);
	return ib;
}
