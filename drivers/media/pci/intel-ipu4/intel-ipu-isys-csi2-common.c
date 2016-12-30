/*
 * Copyright (c) 2013--2017 Intel Corporation.
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
#include <linux/module.h>

#include <media/intel-ipu4-isys.h>
#include <media/media-entity.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>

#include "intel-ipu4.h"
#include "intel-ipu4-bus.h"
#include "intel-ipu4-buttress.h"
#include "intel-ipu4-isys.h"
#include "intel-ipu4-isys-csi2.h"
#include "intel-ipu5-isys-csi2.h"
#include "intel-ipu4-isys-subdev.h"
#include "intel-ipu4-isys-video.h"

#define CREATE_TRACE_POINTS
#define IPU4_SOF_SEQID_TRACE
#include "intel-ipu4-trace-event.h"

static const uint32_t csi2_supported_codes_pad_sink[] = {
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

static const uint32_t csi2_supported_codes_pad_source[] = {
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

static const uint32_t csi2_supported_codes_pad_meta[] = {
	MEDIA_BUS_FMT_FIXED,
	0,
};

static const uint32_t *csi2_supported_codes[] = {
	csi2_supported_codes_pad_sink,
	csi2_supported_codes_pad_source,
	csi2_supported_codes_pad_source,
	csi2_supported_codes_pad_source,
	csi2_supported_codes_pad_source,
	csi2_supported_codes_pad_meta,
};

static struct v4l2_subdev_internal_ops csi2_sd_internal_ops = {
	.open = intel_ipu4_isys_subdev_open,
	.close = intel_ipu4_isys_subdev_close,
};

int intel_ipu_isys_csi2_get_link_freq(struct intel_ipu4_isys_csi2 *csi2,
		      __s64 *link_freq)
{
	struct intel_ipu4_isys_pipeline *pipe =
		container_of(csi2->asd.sd.entity.pipe,
			     struct intel_ipu4_isys_pipeline, pipe);
	struct v4l2_subdev *ext_sd =
		media_entity_to_v4l2_subdev(pipe->external->entity);
	struct v4l2_ext_control c = { .id = V4L2_CID_LINK_FREQ, };
	struct v4l2_ext_controls cs = { .count = 1,
					.controls = &c, };
	struct v4l2_querymenu qm = { .id = c.id, };
	int rval;

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

static int get_frame_desc_entry_by_dt(struct v4l2_subdev *sd,
				      struct v4l2_mbus_frame_desc_entry *entry,
				      u8 data_type)
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
	struct intel_ipu4_isys_video *av,
	struct ipu_fw_isys_stream_cfg_data_abi *cfg)
{
	struct intel_ipu4_isys_pipeline *ip =
		to_intel_ipu4_isys_pipeline(av->vdev.entity.pipe);
	struct intel_ipu4_isys_queue *aq = &av->aq;
	struct ipu_fw_isys_output_pin_info_abi *pin_info;
	struct v4l2_mbus_frame_desc_entry entry;
	int pin = cfg->nof_output_pins++;
	int inpin = cfg->nof_input_pins++;
	int rval;

	aq->fw_output = pin;
	ip->output_pins[pin].pin_ready = intel_ipu4_isys_queue_buf_ready;
	ip->output_pins[pin].aq = aq;

	pin_info = &cfg->output_pins[pin];
	pin_info->input_pin_id = inpin;
	pin_info->output_res.width = av->mpix.width;
	pin_info->output_res.height = av->mpix.height;
	pin_info->stride = av->mpix.plane_fmt[0].bytesperline;
	pin_info->pt = aq->css_pin_type;
	pin_info->ft = av->pfmt->css_pixelformat;
	pin_info->send_irq = 1;

	rval = get_frame_desc_entry_by_dt(
		media_entity_to_v4l2_subdev(ip->external->entity),
		&entry,
		INTEL_IPU4_ISYS_MIPI_CSI2_TYPE_EMBEDDED8);
	if (!rval) {
		cfg->input_pins[inpin].dt =
			INTEL_IPU4_ISYS_MIPI_CSI2_TYPE_EMBEDDED8;
		cfg->input_pins[inpin].input_res.width =
			entry.size.two_dim.width * entry.bpp / BITS_PER_BYTE;
		cfg->input_pins[inpin].input_res.height =
			entry.size.two_dim.height;
	}
}

static int subscribe_event(struct v4l2_subdev *sd, struct v4l2_fh *fh,
			   struct v4l2_event_subscription *sub)
{
	if (sub->type != V4L2_EVENT_FRAME_SYNC)
		return -EINVAL;

	if (sub->id != 0)
		return -EINVAL;

	return v4l2_event_subscribe(fh, sub, 10, NULL);
}

static const struct v4l2_subdev_core_ops csi2_sd_core_ops = {
	.subscribe_event = subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

struct intel_ipu4_isys_pixelformat csi2_meta_pfmts[] = {
	{ V4L2_FMT_INTEL_IPU4_ISYS_META, 8, 8, 0, MEDIA_BUS_FMT_FIXED, 0 },
	{ },
};

/*
 * The ISP2401 new input system CSI2+ receiver has several
 * parameters affecting the receiver timings. These depend
 * on the MIPI bus frequency F in Hz (sensor transmitter rate)
 * as follows:
 *	register value = (A/1e9 + B * UI) / COUNT_ACC
 * where
 *	UI = 1 / (2 * F) in seconds
 *	COUNT_ACC = counter accuracy in seconds
 *	For ANN and CHV, COUNT_ACC = 0.0625 ns
 *	For BXT,  COUNT_ACC = 0.125 ns
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

static uint32_t calc_timing(int32_t a, int32_t b, int64_t link_freq,
			    int32_t accinv)
{
	return accinv * a + (accinv * b * (500000000 >> DIV_SHIFT)
			     / (int32_t)(link_freq >> DIV_SHIFT));
}

static int intel_ipu_isys_csi2_calc_timing(struct intel_ipu4_isys_csi2 *csi2,
				  struct intel_ipu4_isys_csi2_timing *timing,
				     uint32_t accinv)
{
	__s64 link_freq;
	int rval;

	rval = intel_ipu_isys_csi2_get_link_freq(csi2, &link_freq);
	if (rval)
		return rval;

	timing->ctermen = calc_timing(
		CSI2_CSI_RX_DLY_CNT_TERMEN_CLANE_A,
		CSI2_CSI_RX_DLY_CNT_TERMEN_CLANE_B, link_freq, accinv);
	timing->csettle = calc_timing(
		CSI2_CSI_RX_DLY_CNT_SETTLE_CLANE_A,
		CSI2_CSI_RX_DLY_CNT_SETTLE_CLANE_B, link_freq, accinv);
	dev_dbg(&csi2->isys->adev->dev, "ctermen %u\n", timing->ctermen);
	dev_dbg(&csi2->isys->adev->dev, "csettle %u\n", timing->csettle);

	timing->dtermen = calc_timing(
		CSI2_CSI_RX_DLY_CNT_TERMEN_DLANE_A,
		CSI2_CSI_RX_DLY_CNT_TERMEN_DLANE_B, link_freq, accinv);
	timing->dsettle = calc_timing(
		CSI2_CSI_RX_DLY_CNT_SETTLE_DLANE_A,
		CSI2_CSI_RX_DLY_CNT_SETTLE_DLANE_B, link_freq, accinv);
	dev_dbg(&csi2->isys->adev->dev, "dtermen %u\n", timing->dtermen);
	dev_dbg(&csi2->isys->adev->dev, "dsettle %u\n", timing->dsettle);

	return 0;
}

#define CSI2_ACCINV	8

static int set_stream(struct v4l2_subdev *sd, int enable)
{
	struct intel_ipu4_isys_csi2 *csi2 = to_intel_ipu4_isys_csi2(sd);
	struct intel_ipu4_isys_pipeline *ip =
		container_of(sd->entity.pipe,
			     struct intel_ipu4_isys_pipeline, pipe);
	struct intel_ipu4_isys_csi2_config *cfg =
		v4l2_get_subdev_hostdata(
			media_entity_to_v4l2_subdev(ip->external->entity));
	struct v4l2_subdev *ext_sd =
		media_entity_to_v4l2_subdev(ip->external->entity);
	struct v4l2_control c = { .id = V4L2_CID_MIPI_LANES, };
	struct intel_ipu4_isys_csi2_timing timing;
	unsigned int nlanes;
	int rval;

	dev_dbg(&csi2->isys->adev->dev, "csi2 s_stream %d\n", enable);

	if (!enable) {
		csi2->stream_count--;
		if (csi2->stream_count)
			return 0;

		if (csi2->csi2_ops && csi2->csi2_ops->set_stream)
			csi2->csi2_ops->set_stream(sd, timing, 0, enable);

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
		dev_dbg(&csi2->isys->adev->dev,
			"lane nr %d,legacy lane cfg %d,combo lane cfg %d.\n",
			nlanes, csi2->isys->legacy_port_cfg,
			csi2->isys->combo_port_cfg);
	} else {
		nlanes = cfg->nlanes;
	}

	/*Do not configure timings on FPGA*/
	if (csi2->isys->pdata->type !=
		INTEL_IPU4_ISYS_TYPE_INTEL_IPU4_FPGA) {
		rval = intel_ipu_isys_csi2_calc_timing(csi2,
			&timing, CSI2_ACCINV);
		if (rval)
			return rval;
	}

	if (csi2->csi2_ops && csi2->csi2_ops->set_stream)
		csi2->csi2_ops->set_stream(sd, timing, nlanes, enable);

	csi2->stream_count++;
	return 0;
}

static void csi2_capture_done(struct intel_ipu4_isys_pipeline *ip,
			      struct ipu_fw_isys_resp_info_abi *info)
{
	if (ip->interlaced && ip->isys->short_packet_source ==
	    INTEL_IPU_ISYS_SHORT_PACKET_FROM_RECEIVER) {
		struct intel_ipu4_isys_buffer *ib;
		unsigned long flags;

		spin_lock_irqsave(&ip->short_packet_queue_lock, flags);
		if (!list_empty(&ip->short_packet_active)) {
			ib = list_last_entry(&ip->short_packet_active,
				struct intel_ipu4_isys_buffer, head);
			list_move(&ib->head, &ip->short_packet_incoming);
		}
		spin_unlock_irqrestore(&ip->short_packet_queue_lock, flags);
	}
	if (ip->csi2) {
		if (ip->csi2->csi2_ops && ip->csi2->csi2_ops->csi2_error)
			ip->csi2->csi2_ops->csi2_error(ip->csi2);
	}
}

static int csi2_link_validate(struct media_link *link)
{
	struct intel_ipu4_isys_csi2 *csi2 = to_intel_ipu4_isys_csi2(
		media_entity_to_v4l2_subdev(link->sink->entity));
	struct intel_ipu4_isys_pipeline *ip =
		to_intel_ipu4_isys_pipeline(link->sink->entity->pipe);
	struct v4l2_subdev_route r[INTEL_IPU4_ISYS_MAX_STREAMS];
	struct v4l2_subdev_routing routing = {
		.routes = r,
		.num_routes = INTEL_IPU4_ISYS_MAX_STREAMS,
	};
	int i, rval;
	unsigned int active = 0;

	csi2->receiver_errors = 0;
	ip->csi2 = csi2;
	intel_ipu4_isys_video_add_capture_done(
		to_intel_ipu4_isys_pipeline(link->sink->entity->pipe),
		csi2_capture_done);

	rval = v4l2_subdev_link_validate(link);
	if (rval)
		return rval;

	if (!v4l2_ctrl_g_ctrl(csi2->store_csi2_header)) {
		for (i = 0; i < NR_OF_CSI2_SOURCE_PADS; i++) {
			struct media_pad *remote_pad =
				media_entity_remote_pad(
				&csi2->asd.pad[CSI2_PAD_SOURCE(i)]);

			if (remote_pad && (remote_pad->entity->type &
			    MEDIA_ENT_TYPE_MASK) == MEDIA_ENT_T_V4L2_SUBDEV) {
				dev_err(&csi2->isys->adev->dev,
					"CSI2 BE requires CSI2 headers.\n");
				return -EINVAL;
			}
		}
	}

	rval = v4l2_subdev_call(
		media_entity_to_v4l2_subdev(link->source->entity), pad,
		get_routing, &routing);

	if (rval) {
		csi2->remote_streams = 1;
		return 0;
	}

	for (i = 0; i < routing.num_routes; i++) {
		if (routing.routes[i].flags & V4L2_SUBDEV_ROUTE_FL_ACTIVE)
			active++;
	}

	if (active != bitmap_weight(
		csi2->asd.stream[link->sink->index].streams_stat, 32))
		return -EINVAL;

	csi2->remote_streams = active;

	return 0;
}

bool csi2_has_route(struct media_entity *entity, unsigned int pad0,
		    unsigned int pad1)
{
	/* TODO: need to remove this when meta data node is removed */
	if (pad0 == CSI2_PAD_META || pad1 == CSI2_PAD_META)
		return true;

	return intel_ipu4_isys_subdev_has_route(entity, pad0, pad1);
}

static const struct v4l2_subdev_video_ops csi2_sd_video_ops = {
	.s_stream = set_stream,
};

static int get_metadata_fmt(struct v4l2_subdev *sd,
			    struct v4l2_subdev_pad_config *cfg,
			    struct v4l2_subdev_format *fmt)
{
	struct media_pad *pad = media_entity_remote_pad(
		&sd->entity.pads[CSI2_PAD_SINK]);
	struct v4l2_mbus_frame_desc_entry entry;
	int rval;

	if (!pad)
		return -EINVAL;

	rval = get_frame_desc_entry_by_dt(
		media_entity_to_v4l2_subdev(pad->entity),
		&entry,
		INTEL_IPU4_ISYS_MIPI_CSI2_TYPE_EMBEDDED8);

	if (!rval) {
		fmt->format.width =
			entry.size.two_dim.width * entry.bpp / BITS_PER_BYTE;
		fmt->format.height = entry.size.two_dim.height;
		fmt->format.code = entry.pixelcode;
		fmt->format.field = V4L2_FIELD_NONE;
	}
	return rval;
}

static int intel_ipu_isys_csi2_get_fmt(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *cfg,
				  struct v4l2_subdev_format *fmt)
{
	if (fmt->pad == CSI2_PAD_META)
		return get_metadata_fmt(sd, cfg, fmt);

	return intel_ipu4_isys_subdev_get_ffmt(sd, cfg, fmt);
}

static int intel_ipu_isys_csi2_set_fmt(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *cfg,
				  struct v4l2_subdev_format *fmt)
{
	if (fmt->pad == CSI2_PAD_META)
		return get_metadata_fmt(sd, cfg, fmt);

	return intel_ipu4_isys_subdev_set_ffmt(sd, cfg, fmt);
}

static int __subdev_link_validate(
	struct v4l2_subdev *sd, struct media_link *link,
	struct v4l2_subdev_format *source_fmt,
	struct v4l2_subdev_format *sink_fmt)
{
	struct intel_ipu4_isys_pipeline *ip =
		container_of(sd->entity.pipe,
		struct intel_ipu4_isys_pipeline, pipe);

	if (source_fmt->format.field == V4L2_FIELD_ALTERNATE)
		ip->interlaced = true;

	return intel_ipu4_isys_subdev_link_validate(
		sd, link, source_fmt, sink_fmt);
}

static const struct v4l2_subdev_pad_ops csi2_sd_pad_ops = {
	.link_validate = __subdev_link_validate,
	.get_fmt = intel_ipu_isys_csi2_get_fmt,
	.set_fmt = intel_ipu_isys_csi2_set_fmt,
	.enum_mbus_code = intel_ipu4_isys_subdev_enum_mbus_code,
	.set_routing = intel_ipu4_isys_subdev_set_routing,
	.get_routing = intel_ipu4_isys_subdev_get_routing,
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
		__intel_ipu4_isys_get_ffmt(sd, cfg, fmt->pad, fmt->stream,
					   fmt->which);

	if (fmt->format.field != V4L2_FIELD_ALTERNATE)
		fmt->format.field = V4L2_FIELD_NONE;

	if (fmt->pad == CSI2_PAD_SINK) {
		*ffmt = fmt->format;
		if (fmt->stream == 0)
			intel_ipu4_isys_subdev_fmt_propagate(
				sd, cfg, &fmt->format, NULL,
				INTEL_IPU4_ISYS_SUBDEV_PROP_TGT_SINK_FMT,
				fmt->pad, fmt->which);
		return;
	}

	if (fmt->pad == CSI2_PAD_META) {
		struct v4l2_mbus_framefmt *ffmt =
			__intel_ipu4_isys_get_ffmt(
				sd, cfg, fmt->pad, fmt->stream, fmt->which);
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

		rval = get_frame_desc_entry_by_dt(
			media_entity_to_v4l2_subdev(pad->entity),
			&entry,
			INTEL_IPU4_ISYS_MIPI_CSI2_TYPE_EMBEDDED8);

		if (!rval) {
			ffmt->width = entry.size.two_dim.width * entry.bpp
				/ BITS_PER_BYTE;
			ffmt->height = entry.size.two_dim.height;
			ffmt->code = entry.pixelcode;
			ffmt->field = V4L2_FIELD_NONE;
		}

		return;
	}

	if (sd->entity.pads[fmt->pad].flags & MEDIA_PAD_FL_SOURCE) {
		ffmt->width = fmt->format.width;
		ffmt->height = fmt->format.height;
		ffmt->field = fmt->format.field;
		ffmt->code =
			intel_ipu4_isys_subdev_code_to_uncompressed(
				fmt->format.code);
		return;
	}

	BUG_ON(1);
}

static const struct intel_ipu4_isys_pixelformat *csi2_try_fmt(
	struct intel_ipu4_isys_video *av, struct v4l2_pix_format_mplane *mpix)
{
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(
				 av->vdev.entity.links[0].source->entity);
	struct intel_ipu4_isys_csi2 *csi2 = to_intel_ipu4_isys_csi2(sd);

	return intel_ipu4_isys_video_try_fmt_vid_mplane(av, mpix,
		v4l2_ctrl_g_ctrl(csi2->store_csi2_header));
}

void intel_ipu_isys_csi2_cleanup(struct intel_ipu4_isys_csi2 *csi2)
{
	int i;

	if (!csi2->isys)
		return;

	v4l2_device_unregister_subdev(&csi2->asd.sd);
	intel_ipu4_isys_subdev_cleanup(&csi2->asd);
	for (i = 0; i < NR_OF_CSI2_SOURCE_PADS; i++)
		intel_ipu4_isys_video_cleanup(&csi2->av[i]);
	intel_ipu4_isys_video_cleanup(&csi2->av_meta);
	csi2->isys = NULL;
}

static void csi_ctrl_init(struct v4l2_subdev *sd)
{
	struct intel_ipu4_isys_csi2 *csi2 = to_intel_ipu4_isys_csi2(sd);

	static const struct v4l2_ctrl_config cfg = {
		.id = V4L2_CID_INTEL_IPU4_STORE_CSI2_HEADER,
		.name = "Store CSI-2 Headers",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.min = 0,
		.max = 1,
		.step = 1,
		.def = 1,
	};

	csi2->store_csi2_header = v4l2_ctrl_new_custom(
		&csi2->asd.ctrl_handler, &cfg, NULL);
}

int intel_ipu_isys_csi2_init(struct intel_ipu4_isys_csi2 *csi2,
				struct intel_ipu4_isys *isys,
				void __iomem *base,
				unsigned int index)
{
	struct v4l2_subdev_format fmt = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
		.pad = CSI2_PAD_SINK,
		.format = {
			.width = 4096,
			.height = 3072,
		},
	};
	struct v4l2_subdev_format fmt_meta = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
		.pad = CSI2_PAD_META,
	};
	int i, rval;

	csi2->isys = isys;
	csi2->base = base;
	csi2->index = index;

	csi2->asd.sd.entity.ops = &csi2_entity_ops;
	csi2->asd.ctrl_init = csi_ctrl_init;
	csi2->asd.isys = isys;
	init_completion(&csi2->eof_completion);
	csi2->remote_streams = 1;
	csi2->stream_count = 0;

	rval = intel_ipu4_isys_subdev_init(&csi2->asd, &csi2_sd_ops, 0,
				NR_OF_CSI2_PADS, NR_OF_CSI2_STREAMS,
				NR_OF_CSI2_SOURCE_PADS, NR_OF_CSI2_SINK_PADS,
				V4L2_SUBDEV_FL_HAS_SUBSTREAMS);
	if (rval)
		goto fail;

	csi2->asd.pad[CSI2_PAD_SINK].flags = MEDIA_PAD_FL_SINK
		| MEDIA_PAD_FL_MUST_CONNECT | MEDIA_PAD_FL_MULTIPLEX;
	for (i = CSI2_PAD_SOURCE(0);
		i < (NR_OF_CSI2_SOURCE_PADS + CSI2_PAD_SOURCE(0)); i++)
		csi2->asd.pad[i].flags = MEDIA_PAD_FL_SOURCE;

	csi2->asd.pad[CSI2_PAD_META].flags = MEDIA_PAD_FL_SOURCE;
	csi2->asd.source = IPU_FW_ISYS_STREAM_SRC_CSI2_PORT0 + index;
	csi2->asd.supported_codes = csi2_supported_codes;
	csi2->asd.set_ffmt = csi2_set_ffmt;

	csi2->asd.sd.flags |= V4L2_SUBDEV_FL_HAS_EVENTS;
	csi2->asd.sd.internal_ops = &csi2_sd_internal_ops;
	snprintf(csi2->asd.sd.name, sizeof(csi2->asd.sd.name),
		 INTEL_IPU4_ISYS_ENTITY_PREFIX " CSI-2 %u", index);
	v4l2_set_subdevdata(&csi2->asd.sd, &csi2->asd);

	mutex_lock(&csi2->asd.mutex);
	rval = v4l2_device_register_subdev(&isys->v4l2_dev, &csi2->asd.sd);
	if (rval) {
		mutex_unlock(&csi2->asd.mutex);
		dev_info(&isys->adev->dev, "can't register v4l2 subdev\n");
		goto fail;
	}

	__intel_ipu4_isys_subdev_set_ffmt(&csi2->asd.sd, NULL, &fmt);
	__intel_ipu4_isys_subdev_set_ffmt(&csi2->asd.sd, NULL, &fmt_meta);

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
			 INTEL_IPU4_ISYS_ENTITY_PREFIX " CSI-2 %u capture %d",
			 index, i);
		csi2->av[i].isys = isys;
		csi2->av[i].aq.css_pin_type = IPU_FW_ISYS_PIN_TYPE_MIPI;
		csi2->av[i].pfmts = intel_ipu4_isys_pfmts_packed;
		csi2->av[i].try_fmt_vid_mplane = csi2_try_fmt;
		csi2->av[i].prepare_firmware_stream_cfg =
			intel_ipu4_isys_prepare_firmware_stream_cfg_default;
		csi2->av[i].packed = true;
		csi2->av[i].line_header_length =
			INTEL_IPU4_ISYS_CSI2_LONG_PACKET_HEADER_SIZE;
		csi2->av[i].line_footer_length =
			INTEL_IPU4_ISYS_CSI2_LONG_PACKET_FOOTER_SIZE;
		csi2->av[i].aq.buf_prepare = intel_ipu4_isys_buf_prepare;
		csi2->av[i].aq.fill_frame_buff_set_pin =
		intel_ipu4_isys_buffer_list_to_ipu_fw_isys_frame_buff_set_pin;
		csi2->av[i].aq.link_fmt_validate =
			intel_ipu4_isys_link_fmt_validate;
		csi2->av[i].aq.vbq.buf_struct_size =
			sizeof(struct intel_ipu4_isys_video_buffer);

		rval = intel_ipu4_isys_video_init(
			&csi2->av[i], &csi2->asd.sd.entity, CSI2_PAD_SOURCE(i),
			MEDIA_PAD_FL_SINK, 0);
		if (rval) {
			dev_info(&isys->adev->dev, "can't init video node\n");
			goto fail;
		}
	}

	/* TODO: remove meta data pad */
	snprintf(csi2->av_meta.vdev.name, sizeof(csi2->av_meta.vdev.name),
		 INTEL_IPU4_ISYS_ENTITY_PREFIX " CSI-2 %u meta", index);
	csi2->av_meta.isys = isys;
	csi2->av_meta.aq.css_pin_type = IPU_FW_ISYS_PIN_TYPE_MIPI;
	csi2->av_meta.pfmts = csi2_meta_pfmts;
	csi2->av_meta.try_fmt_vid_mplane = csi2_try_fmt;
	csi2->av_meta.prepare_firmware_stream_cfg =
		csi2_meta_prepare_firmware_stream_cfg_default;
	csi2->av_meta.packed = true;
	csi2->av_meta.line_header_length =
		INTEL_IPU4_ISYS_CSI2_LONG_PACKET_HEADER_SIZE;
	csi2->av_meta.line_footer_length =
		INTEL_IPU4_ISYS_CSI2_LONG_PACKET_FOOTER_SIZE;
	csi2->av_meta.aq.buf_prepare = intel_ipu4_isys_buf_prepare;
	csi2->av_meta.aq.fill_frame_buff_set_pin =
		intel_ipu4_isys_buffer_list_to_ipu_fw_isys_frame_buff_set_pin;
	csi2->av_meta.aq.link_fmt_validate = intel_ipu4_isys_link_fmt_validate;

	rval = intel_ipu4_isys_video_init(
		&csi2->av_meta, &csi2->asd.sd.entity, CSI2_PAD_META,
		MEDIA_PAD_FL_SINK, 0);
	if (rval) {
		dev_info(&isys->adev->dev, "can't init metadata node\n");
		goto fail;
	}

	if (is_intel_ipu4_hw_bxt_b0(isys->adev->isp))
		csi2->csi2_ops = &csi2_funcs_ipu4;
	else
		csi2->csi2_ops = &csi2_funcs_ipu5;

	return 0;

fail:
	intel_ipu_isys_csi2_cleanup(csi2);

	return rval;
}

void intel_ipu_isys_csi2_sof_event(struct intel_ipu4_isys_csi2 *csi2,
					   unsigned int vc)
{
	struct intel_ipu4_isys_pipeline *ip = NULL;
	struct v4l2_event ev = {
		.type = V4L2_EVENT_FRAME_SYNC,
	};
	struct video_device *vdev = csi2->asd.sd.devnode;
	unsigned long flags;
	unsigned int i;

	spin_lock_irqsave(&csi2->isys->lock, flags);
	csi2->in_frame[vc] = true;

	for (i = 0; i < INTEL_IPU4_ISYS_MAX_STREAMS; i++) {
		if (csi2->isys->pipes[i] && csi2->isys->pipes[i]->csi2 == csi2
		    && csi2->isys->pipes[i]->vc == vc) {
			ip = csi2->isys->pipes[i];
			break;
		}
	}

	/* Pipe already vanished */
	if (!ip) {
		spin_unlock_irqrestore(&csi2->isys->lock, flags);
		return;
	}

	ev.u.frame_sync.frame_sequence =
		atomic_inc_return(&ip->sequence) - 1;
	ev.id = ip->stream_id;
	spin_unlock_irqrestore(&csi2->isys->lock, flags);

	trace_ipu4_sof_seqid(ev.u.frame_sync.frame_sequence, csi2->index, vc);
	v4l2_event_queue(vdev, &ev);

	dev_dbg(&csi2->isys->adev->dev,
		"csi2-%i sequence: %i, vc: %d, stream_id: %d\n",
		csi2->index, ev.u.frame_sync.frame_sequence, vc, ip->stream_id);
}

void intel_ipu_isys_csi2_eof_event(struct intel_ipu4_isys_csi2 *csi2,
					   unsigned int vc)
{
	unsigned long flags;

	spin_lock_irqsave(&csi2->isys->lock, flags);
	csi2->in_frame[vc] = false;
	if (csi2->wait_for_sync[vc])
		complete(&csi2->eof_completion);
	spin_unlock_irqrestore(&csi2->isys->lock, flags);
}

/* Call this function only _after_ the sensor has been stopped */
void intel_ipu_isys_csi2_wait_last_eof(struct intel_ipu4_isys_csi2 *csi2)
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
			INTEL_IPU_EOF_TIMEOUT_JIFFIES);
		if (!tout)
			dev_err(&csi2->isys->adev->dev,
				"csi2-%d: timeout at sync to eof of vc %d\n",
				csi2->index, i);
		csi2->wait_for_sync[i] = false;
	}
}

void intel_ipu_isys_csi2_isr(struct intel_ipu4_isys_csi2 *csi2)
{
	if (csi2->csi2_ops && csi2->csi2_ops->csi2_isr)
		csi2->csi2_ops->csi2_isr(csi2);
}

struct intel_ipu4_isys_buffer *
intel_ipu_isys_csi2_get_short_packet_buffer(
	struct intel_ipu4_isys_pipeline *ip)
{
	struct intel_ipu4_isys_buffer *ib;
	struct intel_ipu4_isys_private_buffer *pb;
	struct intel_ipu4_isys_mipi_packet_header *ph;

	if (list_empty(&ip->short_packet_incoming))
		return NULL;
	ib = list_last_entry(&ip->short_packet_incoming,
			     struct intel_ipu4_isys_buffer, head);
	pb = intel_ipu4_isys_buffer_to_private_buffer(ib);
	ph = (struct intel_ipu4_isys_mipi_packet_header *) pb->buffer;

	/* Fill the packet header with magic number. */
	ph->word_count = 0xffff;
	ph->dtype = 0xff;

	dma_sync_single_for_cpu(&ip->isys->adev->dev, pb->dma_addr,
		sizeof(*ph), DMA_BIDIRECTIONAL);
	return ib;
}

unsigned int intel_ipu_isys_csi2_get_current_field(
	struct intel_ipu4_isys_pipeline *ip,
	struct ipu_fw_isys_resp_info_abi *info)
{
	unsigned int field = V4L2_FIELD_TOP;

	if (ip->csi2 && ip->csi2->csi2_ops &&
		ip->csi2->csi2_ops->get_current_field)
		field = ip->csi2->csi2_ops->get_current_field(ip,
			info->timestamp);
	return field;
}

bool intel_ipu_skew_cal_required(struct intel_ipu4_isys_csi2 *csi2)
{
	if (csi2 && csi2->csi2_ops && csi2->csi2_ops->skew_cal_required)
		return csi2->csi2_ops->skew_cal_required(csi2);
	else
		return false;
}

int intel_ipu_csi_set_skew_cal(struct intel_ipu4_isys_csi2 *csi2, int enable)
{
	if (csi2 && csi2->csi2_ops && csi2->csi2_ops->set_skew_cal)
		csi2->csi2_ops->set_skew_cal(csi2, enable);

	return 0;
}
