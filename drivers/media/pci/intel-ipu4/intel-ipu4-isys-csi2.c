/*
 * Copyright (c) 2013--2015 Intel Corporation.
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
#include "intel-ipu4-isys.h"
#include "intel-ipu4-isys-csi2.h"
#include "intel-ipu4-isys-csi2-reg.h"
#include "intel-ipu4-isys-subdev.h"
#include "intel-ipu4-isys-video.h"
/* for IA_CSS_ISYS_PIN_TYPE_RAW_NS */
#include "isysapi/interface/ia_css_isysapi_fw_types.h"
#include "isysapi/interface/ia_css_isysapi_types.h"

#define CREATE_TRACE_POINTS
#include "intel-ipu4-trace-event.h"


static const uint32_t csi2_supported_codes_pad[] = {
	MEDIA_BUS_FMT_RGB565_1X16,
	MEDIA_BUS_FMT_RGB888_1X24,
	MEDIA_BUS_FMT_UYVY8_1X16,
	MEDIA_BUS_FMT_SBGGR10_1X10,
	MEDIA_BUS_FMT_SGBRG10_1X10,
	MEDIA_BUS_FMT_SGRBG10_1X10,
	MEDIA_BUS_FMT_SRGGB10_1X10,
	MEDIA_BUS_FMT_SBGGR12_1X12,
	MEDIA_BUS_FMT_SGBRG12_1X12,
	MEDIA_BUS_FMT_SGRBG12_1X12,
	MEDIA_BUS_FMT_SRGGB12_1X12,
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
	csi2_supported_codes_pad,
	csi2_supported_codes_pad,
	csi2_supported_codes_pad_meta,
};

static struct v4l2_subdev_internal_ops csi2_sd_internal_ops = {
	.open = intel_ipu4_isys_subdev_open,
	.close = intel_ipu4_isys_subdev_close,
};

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
	struct ia_css_isys_stream_cfg_data *cfg)
{
	struct intel_ipu4_isys_pipeline *ip =
		to_intel_ipu4_isys_pipeline(av->vdev.entity.pipe);
	struct intel_ipu4_isys_queue *aq = &av->aq;
	struct ia_css_isys_output_pin_info *pin_info;
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
	{ V4L2_FMT_INTEL_IPU4_ISYS_META, 8, 8, MEDIA_BUS_FMT_FIXED, 0 },
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

static void intel_ipu4_isys_csi2_error(struct intel_ipu4_isys_csi2 *csi2)
{
	/*
	 * Strings corresponding to CSI-2 receiver errors are here.
	 * Corresponding macros are defined in the header file.
	 */
	static const char *errors[] = {
		"Single packet header error corrected",
		"Multiple packet header errors detected",
		"Payload checksum (CRC) error",
		"FIFO overflow",
		"Reserved short packet data type detected",
		"Reserved long packet data type detected",
		"Incomplete long packet detected",
		"Frame sync error",
		"Line sync error",
		"DPHY recoverable synchronizationerror",
		"DPHY non-recoverable synchronization error",
		"Escape mode error",
		"Escape mode trigger event",
		"Escape mode ultra-low power state for data lane(s)",
		"Escape mode ultra-low power state exit for clock lane",
		"Inter-frame short packet discarded",
		"Inter-frame long packet discarded"
	};
	unsigned int i;
	u32 status = csi2->receiver_errors;
	csi2->receiver_errors = 0;

	for (i = 0; i < ARRAY_SIZE(errors); i++)
		if (status & BIT(i))
			dev_err(&csi2->isys->adev->dev, "csi2-%i error: %s\n",
				csi2->index, errors[i]);
}

static uint32_t calc_timing(int32_t a, int32_t b, int64_t link_freq,
			    int32_t accinv)
{
	return accinv * a + (accinv * b * (500000000 >> DIV_SHIFT)
			     / (int32_t)(link_freq >> DIV_SHIFT));
}

int intel_ipu4_isys_csi2_calc_timing(struct intel_ipu4_isys_csi2 *csi2,
				  struct intel_ipu4_isys_csi2_timing *timing,
				  uint32_t accinv)
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

	timing->ctermen = calc_timing(
		CSI2_CSI_RX_DLY_CNT_TERMEN_CLANE_A,
		CSI2_CSI_RX_DLY_CNT_TERMEN_CLANE_B, qm.value, accinv);
	timing->csettle = calc_timing(
		CSI2_CSI_RX_DLY_CNT_SETTLE_CLANE_A,
		CSI2_CSI_RX_DLY_CNT_SETTLE_CLANE_B, qm.value, accinv);
	dev_dbg(&csi2->isys->adev->dev, "ctermen %u\n", timing->ctermen);
	dev_dbg(&csi2->isys->adev->dev, "csettle %u\n", timing->csettle);

	timing->dtermen = calc_timing(
		CSI2_CSI_RX_DLY_CNT_TERMEN_DLANE_A,
		CSI2_CSI_RX_DLY_CNT_TERMEN_DLANE_B, qm.value, accinv);
	timing->dsettle = calc_timing(
		CSI2_CSI_RX_DLY_CNT_SETTLE_DLANE_A,
		CSI2_CSI_RX_DLY_CNT_SETTLE_DLANE_B, qm.value, accinv);
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
	unsigned int i, nlanes;
	int rval;
	u32 csi2csirx, csi2part;

	dev_dbg(&csi2->isys->adev->dev, "csi2 s_stream %d\n", enable);

	if (!enable) {
		intel_ipu4_isys_csi2_error(csi2);
		writel(0, csi2->base + CSI2_REG_CSI_RX_CONFIG);
		writel(0, csi2->base + CSI2_REG_CSI_RX_ENABLE);

		/* Disable interrupts */
		writel(0, csi2->base + CSI2_REG_CSI2S2M_IRQ_MASK);
		writel(0, csi2->base + CSI2_REG_CSI2S2M_IRQ_ENABLE);
		writel(0, csi2->base + CSI2_REG_CSI2PART_IRQ_MASK);
		writel(0, csi2->base + CSI2_REG_CSI2PART_IRQ_ENABLE);
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
	if (csi2->isys->pdata->type != INTEL_IPU4_ISYS_TYPE_INTEL_IPU4_FPGA) {
		rval = intel_ipu4_isys_csi2_calc_timing(csi2, &timing, CSI2_ACCINV);
		if (rval)
			return rval;

		writel(timing.ctermen,
			csi2->base + CSI2_REG_CSI_RX_DLY_CNT_TERMEN_CLANE);
		writel(timing.csettle,
			csi2->base + CSI2_REG_CSI_RX_DLY_CNT_SETTLE_CLANE);

		for (i = 0; i < nlanes; i++) {
			writel(timing.dtermen,
				csi2->base + CSI2_REG_CSI_RX_DLY_CNT_TERMEN_DLANE(i));
			writel(timing.dsettle,
				csi2->base + CSI2_REG_CSI_RX_DLY_CNT_SETTLE_DLANE(i));
		}
	}
	writel(CSI2_CSI_RX_CONFIG_DISABLE_BYTE_CLK_GATING |
		CSI2_CSI_RX_CONFIG_RELEASE_LP11,
		csi2->base + CSI2_REG_CSI_RX_CONFIG);

	writel(nlanes, csi2->base + CSI2_REG_CSI_RX_NOF_ENABLED_LANES);

	writel(CSI2_CSI_RX_ENABLE_ENABLE, csi2->base + CSI2_REG_CSI_RX_ENABLE);

	if (is_intel_ipu4_hw_bxt_a0(csi2->isys->adev->isp)) {
		/* Enable frame start (SOF) irq for virtual channel zero */
		u32 csi2s2m = CSI2_IRQ_FS_VC(0);
		/* Enable irqs only from S2M source */
		csi2part = CSI2_CSI2PART_IRQ_CSI2S2M_A0 |
			   CSI2_CSI2PART_IRQ_CSIRX_A0;

		writel(csi2s2m, csi2->base + CSI2_REG_CSI2S2M_IRQ_EDGE);
		writel(0, csi2->base + CSI2_REG_CSI2S2M_IRQ_LEVEL_NOT_PULSE);
		writel(csi2s2m, csi2->base + CSI2_REG_CSI2S2M_IRQ_CLEAR);
		writel(csi2s2m, csi2->base + CSI2_REG_CSI2S2M_IRQ_MASK);
		writel(csi2s2m, csi2->base + CSI2_REG_CSI2S2M_IRQ_ENABLE);
	} else {
		/* SOF enabled from CSI2PART register in B0 */
		csi2part = CSI2_IRQ_FS_VC(0) |
			   CSI2_CSI2PART_IRQ_CSIRX_B0;
	}

	/* Enable csi2 receiver error interrupts */
	csi2csirx = BIT(CSI2_CSIRX_NUM_ERRORS) - 1;
	writel(csi2csirx, csi2->base + CSI2_REG_CSIRX_IRQ_EDGE);
	writel(0, csi2->base + CSI2_REG_CSIRX_IRQ_LEVEL_NOT_PULSE);
	writel(csi2csirx, csi2->base + CSI2_REG_CSIRX_IRQ_CLEAR);
	writel(csi2csirx, csi2->base + CSI2_REG_CSIRX_IRQ_MASK);
	writel(csi2csirx, csi2->base + CSI2_REG_CSIRX_IRQ_ENABLE);

	/* Enable csi2 error and SOF-related irqs */
	writel(csi2part, csi2->base + CSI2_REG_CSI2PART_IRQ_EDGE);
	writel(0, csi2->base + CSI2_REG_CSI2PART_IRQ_LEVEL_NOT_PULSE);
	writel(csi2part, csi2->base + CSI2_REG_CSI2PART_IRQ_CLEAR);
	writel(csi2part, csi2->base + CSI2_REG_CSI2PART_IRQ_MASK);
	writel(csi2part, csi2->base + CSI2_REG_CSI2PART_IRQ_ENABLE);

	ip->has_sof = true;

	return 0;
}

static void csi2_capture_done(struct intel_ipu4_isys_pipeline *ip,
			      struct ia_css_isys_resp_info *info)
{
	if (ip->csi2)
		intel_ipu4_isys_csi2_error(ip->csi2);
}

static int csi2_link_validate(struct media_link *link)
{
	struct intel_ipu4_isys_csi2 *csi2 = to_intel_ipu4_isys_csi2(
		media_entity_to_v4l2_subdev(link->sink->entity));
	struct intel_ipu4_isys_pipeline *ip =
		to_intel_ipu4_isys_pipeline(link->sink->entity->pipe);

	csi2->receiver_errors = 0;
	ip->csi2 = csi2;
	intel_ipu4_isys_video_add_capture_done(
		to_intel_ipu4_isys_pipeline(link->sink->entity->pipe),
		csi2_capture_done);

	return v4l2_subdev_link_validate(link);
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
	}
	return rval;
}

static int intel_ipu4_isys_csi2_get_fmt(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *cfg,
				  struct v4l2_subdev_format *fmt)
{
	if (fmt->pad == CSI2_PAD_META)
		return get_metadata_fmt(sd, cfg, fmt);

	return intel_ipu4_isys_subdev_get_ffmt(sd, cfg, fmt);
}

static int intel_ipu4_isys_csi2_set_fmt(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *cfg,
				  struct v4l2_subdev_format *fmt)
{
	if (fmt->pad == CSI2_PAD_META)
		return get_metadata_fmt(sd, cfg, fmt);

	return intel_ipu4_isys_subdev_set_ffmt(sd, cfg, fmt);
}

static const struct v4l2_subdev_pad_ops csi2_sd_pad_ops = {
	.link_validate = intel_ipu4_isys_subdev_link_validate,
	.get_fmt = intel_ipu4_isys_csi2_get_fmt,
	.set_fmt = intel_ipu4_isys_csi2_set_fmt,
	.enum_mbus_code = intel_ipu4_isys_subdev_enum_mbus_code,
};

static struct v4l2_subdev_ops csi2_sd_ops = {
	.core = &csi2_sd_core_ops,
	.video = &csi2_sd_video_ops,
	.pad = &csi2_sd_pad_ops,
};

static struct media_entity_operations csi2_entity_ops = {
	.link_validate = csi2_link_validate,
};

static void csi2_set_ffmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	if (fmt->format.field != V4L2_FIELD_ALTERNATE)
		fmt->format.field = V4L2_FIELD_NONE;

	if (fmt->pad == CSI2_PAD_META) {
		struct v4l2_mbus_framefmt *ffmt =
			__intel_ipu4_isys_get_ffmt(
				sd, cfg, fmt->pad, fmt->which);
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
		}
	} else {
		intel_ipu4_isys_subdev_set_ffmt_default(sd, cfg, fmt);
	}
}

void intel_ipu4_isys_csi2_cleanup(struct intel_ipu4_isys_csi2 *csi2)
{
	v4l2_device_unregister_subdev(&csi2->asd.sd);
	intel_ipu4_isys_subdev_cleanup(&csi2->asd);
	intel_ipu4_isys_video_cleanup(&csi2->av);
	intel_ipu4_isys_video_cleanup(&csi2->av_meta);
}

int intel_ipu4_isys_csi2_init(struct intel_ipu4_isys_csi2 *csi2, struct intel_ipu4_isys *isys,
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
	struct v4l2_subdev_format fmt_meta = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
		.pad = CSI2_PAD_META,
	};
	int rval;

	csi2->isys = isys;
	csi2->base = base;
	csi2->index = index;

	csi2->asd.sd.entity.ops = &csi2_entity_ops;
	csi2->asd.isys = isys;

	rval = intel_ipu4_isys_subdev_init(&csi2->asd, &csi2_sd_ops, 0,
					NR_OF_CSI2_PADS);
	if (rval)
		goto fail;

	csi2->asd.pad[CSI2_PAD_SINK].flags = MEDIA_PAD_FL_SINK
		| MEDIA_PAD_FL_MUST_CONNECT;
	csi2->asd.pad[CSI2_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	csi2->asd.pad[CSI2_PAD_META].flags = MEDIA_PAD_FL_SOURCE;
	csi2->asd.source = IA_CSS_ISYS_STREAM_SRC_CSI2_PORT0 + index;
	csi2->asd.supported_codes = csi2_supported_codes;
	csi2->asd.set_ffmt = csi2_set_ffmt;
	intel_ipu4_isys_subdev_set_ffmt(&csi2->asd.sd, NULL, &fmt);
	intel_ipu4_isys_subdev_set_ffmt(&csi2->asd.sd, NULL, &fmt_meta);

	csi2->asd.sd.flags |= V4L2_SUBDEV_FL_HAS_EVENTS;
	csi2->asd.sd.internal_ops = &csi2_sd_internal_ops;
	snprintf(csi2->asd.sd.name, sizeof(csi2->asd.sd.name),
		 INTEL_IPU4_ISYS_ENTITY_PREFIX " CSI-2 %u", index);
	v4l2_set_subdevdata(&csi2->asd.sd, &csi2->asd);
	rval = v4l2_device_register_subdev(&isys->v4l2_dev, &csi2->asd.sd);
	if (rval) {
		dev_info(&isys->adev->dev, "can't register v4l2 subdev\n");
		goto fail;
	}

	snprintf(csi2->av.vdev.name, sizeof(csi2->av.vdev.name),
		 INTEL_IPU4_ISYS_ENTITY_PREFIX " CSI-2 %u capture", index);
	csi2->av.isys = isys;
	csi2->av.aq.css_pin_type = IA_CSS_ISYS_PIN_TYPE_MIPI;
	csi2->av.pfmts = intel_ipu4_isys_pfmts_packed;
	csi2->av.try_fmt_vid_mplane =
		intel_ipu4_isys_video_try_fmt_vid_mplane_default;
	csi2->av.prepare_firmware_stream_cfg =
		intel_ipu4_isys_prepare_firmware_stream_cfg_default;
	csi2->av.packed = true;
	csi2->av.line_header_length =
		INTEL_IPU4_ISYS_CSI2_LONG_PACKET_HEADER_SIZE;
	csi2->av.line_footer_length =
		INTEL_IPU4_ISYS_CSI2_LONG_PACKET_FOOTER_SIZE;
	csi2->av.aq.buf_prepare = intel_ipu4_isys_buf_prepare;
	csi2->av.aq.fill_frame_buff_set_pin =
		intel_ipu4_isys_buffer_list_to_ia_css_isys_frame_buff_set_pin;
	csi2->av.aq.link_fmt_validate = intel_ipu4_isys_link_fmt_validate;
	csi2->av.aq.vbq.buf_struct_size =
		sizeof(struct intel_ipu4_isys_video_buffer);

	rval = intel_ipu4_isys_video_init(
		&csi2->av, &csi2->asd.sd.entity, CSI2_PAD_SOURCE,
		MEDIA_PAD_FL_SINK, 0);
	if (rval) {
		dev_info(&isys->adev->dev, "can't init video node\n");
		goto fail;
	}

	snprintf(csi2->av_meta.vdev.name, sizeof(csi2->av_meta.vdev.name),
		 INTEL_IPU4_ISYS_ENTITY_PREFIX " CSI-2 %u meta", index);
	csi2->av_meta.isys = isys;
	csi2->av_meta.aq.css_pin_type = IA_CSS_ISYS_PIN_TYPE_MIPI;
	csi2->av_meta.pfmts = csi2_meta_pfmts;
	csi2->av_meta.try_fmt_vid_mplane =
		intel_ipu4_isys_video_try_fmt_vid_mplane_default;
	csi2->av_meta.prepare_firmware_stream_cfg =
		csi2_meta_prepare_firmware_stream_cfg_default;
	csi2->av_meta.packed = true;
	csi2->av_meta.line_header_length =
		INTEL_IPU4_ISYS_CSI2_LONG_PACKET_HEADER_SIZE;
	csi2->av_meta.line_footer_length =
		INTEL_IPU4_ISYS_CSI2_LONG_PACKET_FOOTER_SIZE;
	csi2->av_meta.aq.buf_prepare = intel_ipu4_isys_buf_prepare;
	csi2->av_meta.aq.fill_frame_buff_set_pin =
		intel_ipu4_isys_buffer_list_to_ia_css_isys_frame_buff_set_pin;
	csi2->av_meta.aq.link_fmt_validate = intel_ipu4_isys_link_fmt_validate;

	rval = intel_ipu4_isys_video_init(
		&csi2->av_meta, &csi2->asd.sd.entity, CSI2_PAD_META,
		MEDIA_PAD_FL_SINK, 0);
	if (rval) {
		dev_info(&isys->adev->dev, "can't init metadata node\n");
		goto fail;
	}

	return 0;

fail:
	intel_ipu4_isys_csi2_cleanup(csi2);

	return rval;
}

static void intel_ipu4_isys_csi2_sof_event(struct intel_ipu4_isys_csi2 *csi2)
{
	struct intel_ipu4_isys_pipeline *ip =
		to_intel_ipu4_isys_pipeline(csi2->asd.sd.entity.pipe);
	struct v4l2_event ev = {
		.type = V4L2_EVENT_FRAME_SYNC,
		.u.frame_sync.frame_sequence =
		atomic_inc_return(&ip->sequence) - 1,
	};
	struct video_device *vdev = csi2->asd.sd.devnode;

	trace_ipu4_sof_seqid(ev.u.frame_sync.frame_sequence);
	dev_dbg(&csi2->isys->adev->dev, "%s sequence %i\n", __func__,
		ev.u.frame_sync.frame_sequence);
	v4l2_event_queue(vdev, &ev);
}

static void intel_ipu4_isys_register_errors(struct intel_ipu4_isys_csi2 *csi2)
{
	u32 status = readl(csi2->base + CSI2_REG_CSIRX_IRQ_STATUS);
	writel(status, csi2->base + CSI2_REG_CSIRX_IRQ_CLEAR);
	csi2->receiver_errors |= status;
}

void intel_ipu4_isys_csi2_isr(struct intel_ipu4_isys_csi2 *csi2)
{
	u32 status = readl(csi2->base + CSI2_REG_CSI2PART_IRQ_STATUS);

	writel(status, csi2->base + CSI2_REG_CSI2PART_IRQ_CLEAR);

	if (is_intel_ipu4_hw_bxt_a0(csi2->isys->adev->isp)) {
		if (status & CSI2_CSI2PART_IRQ_CSIRX_A0)
			intel_ipu4_isys_register_errors(csi2);

		if (!(status & CSI2_CSI2PART_IRQ_CSI2S2M_A0))
			return;

		status = readl(csi2->base + CSI2_REG_CSI2S2M_IRQ_STATUS);
		writel(status, csi2->base + CSI2_REG_CSI2S2M_IRQ_CLEAR);
	}
	if ((is_intel_ipu4_hw_bxt_b0(csi2->isys->adev->isp) &&
	    status & CSI2_CSI2PART_IRQ_CSIRX_B0))
		intel_ipu4_isys_register_errors(csi2);

	if (!(status & CSI2_IRQ_FS_VC(0)))
		return;

	intel_ipu4_isys_csi2_sof_event(csi2);
}
