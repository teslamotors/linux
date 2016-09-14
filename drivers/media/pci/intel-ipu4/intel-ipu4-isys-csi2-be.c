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
#include <linux/module.h>

#include <media/intel-ipu4-isys.h>
#include <media/media-entity.h>
#include <media/v4l2-device.h>

#include "intel-ipu4.h"
#include "intel-ipu4-bus.h"
#include "intel-ipu4-isys.h"
#include "intel-ipu4-isys-csi2-be.h"
#include "intel-ipu4-isys-subdev.h"
#include "intel-ipu4-isys-video.h"

/*
 * Raw bayer format pixel order MUST BE MAINTAINED in groups of four codes.
 * Otherwise pixel order calculation below WILL BREAK!
 */
static const uint32_t csi2_be_supported_codes_pad[] = {
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

static const uint32_t *csi2_be_supported_codes[] = {
	csi2_be_supported_codes_pad,
	csi2_be_supported_codes_pad,
};

static struct v4l2_subdev_internal_ops csi2_be_sd_internal_ops = {
	.open = intel_ipu4_isys_subdev_open,
	.close = intel_ipu4_isys_subdev_close,
};

static const struct v4l2_subdev_core_ops csi2_be_sd_core_ops = {
};

static int set_stream(struct v4l2_subdev *sd, int enable)
{
	return 0;
}

static const struct v4l2_subdev_video_ops csi2_be_sd_video_ops = {
	.s_stream = set_stream,
};

static int __subdev_link_validate(
	struct v4l2_subdev *sd, struct media_link *link,
	struct v4l2_subdev_format *source_fmt,
	struct v4l2_subdev_format *sink_fmt)
{
	struct intel_ipu4_isys_pipeline *ip =
		container_of(sd->entity.pipe,
			struct intel_ipu4_isys_pipeline, pipe);

	ip->csi2_be = to_intel_ipu4_isys_csi2_be(sd);
	return intel_ipu4_isys_subdev_link_validate(sd, link,
						source_fmt, sink_fmt);
}

static int get_supported_code_index(uint32_t code)
{
	int i;

	for (i = 0; csi2_be_supported_codes_pad[i]; i++) {
		if (csi2_be_supported_codes_pad[i] == code)
			return i;
	}
	return -EINVAL;
}

static int ipu4_isys_csi2_be_set_sel(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_selection *sel)
{
	struct intel_ipu4_isys_subdev *asd = to_intel_ipu4_isys_subdev(sd);
	struct media_pad *pad = &asd->sd.entity.pads[sel->pad];

	if (sel->target == V4L2_SEL_TGT_CROP &&
	    pad->flags & MEDIA_PAD_FL_SOURCE &&
	    asd->valid_tgts[CSI2_BE_PAD_SOURCE].crop) {
		struct v4l2_mbus_framefmt *ffmt =
			__intel_ipu4_isys_get_ffmt(sd, cfg, sel->pad, 0,
						sel->which);
		struct v4l2_rect *r = __intel_ipu4_isys_get_selection(
			sd, cfg, sel->target, CSI2_BE_PAD_SINK, sel->which);

		if (get_supported_code_index(ffmt->code) < 0) {
			/* Non-bayer formats can't be single line cropped */
			sel->r.left &= ~1;
			sel->r.top &= ~1;

			/* Non-bayer formats can't pe padded at all */
			sel->r.width = clamp(sel->r.width,
					INTEL_IPU4_ISYS_MIN_WIDTH,
					r->width);
		} else {
			sel->r.width = clamp(sel->r.width,
					INTEL_IPU4_ISYS_MIN_WIDTH,
					INTEL_IPU4_ISYS_MAX_WIDTH);
		}

		/*
		 * ISAPF can pad only horizontally, height is
		 * restricted by sink pad resolution.
		 */
		sel->r.height = clamp(sel->r.height, INTEL_IPU4_ISYS_MIN_HEIGHT,
					r->height);
		*__intel_ipu4_isys_get_selection(sd, cfg, sel->target, sel->pad,
						sel->which) = sel->r;
		intel_ipu4_isys_subdev_fmt_propagate(
			sd, cfg, NULL, &sel->r,
			INTEL_IPU4_ISYS_SUBDEV_PROP_TGT_SOURCE_CROP,
			sel->pad, sel->which);
		return 0;
	}
	return intel_ipu4_isys_subdev_set_sel(sd, cfg, sel);
}

static const struct v4l2_subdev_pad_ops csi2_be_sd_pad_ops = {
	.link_validate = __subdev_link_validate,
	.get_fmt = intel_ipu4_isys_subdev_get_ffmt,
	.set_fmt = intel_ipu4_isys_subdev_set_ffmt,
	.get_selection = intel_ipu4_isys_subdev_get_sel,
	.set_selection = ipu4_isys_csi2_be_set_sel,
	.enum_mbus_code = intel_ipu4_isys_subdev_enum_mbus_code,
};

static struct v4l2_subdev_ops csi2_be_sd_ops = {
	.core = &csi2_be_sd_core_ops,
	.video = &csi2_be_sd_video_ops,
	.pad = &csi2_be_sd_pad_ops,
};

static struct media_entity_operations csi2_be_entity_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static void csi2_be_set_ffmt(struct v4l2_subdev *sd,
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
	case CSI2_BE_PAD_SINK:
		if (fmt->format.field != V4L2_FIELD_ALTERNATE)
			fmt->format.field = V4L2_FIELD_NONE;
		*ffmt = fmt->format;

		intel_ipu4_isys_subdev_fmt_propagate(
			sd, cfg, &fmt->format, NULL,
			INTEL_IPU4_ISYS_SUBDEV_PROP_TGT_SINK_FMT, fmt->pad,
			fmt->which);
		return;
	case CSI2_BE_PAD_SOURCE: {
		struct v4l2_mbus_framefmt *sink_ffmt =
			__intel_ipu4_isys_get_ffmt(sd, cfg,
						CSI2_BE_PAD_SINK,
						fmt->stream, fmt->which);
		struct v4l2_rect *r =
			__intel_ipu4_isys_get_selection(
				sd, cfg, V4L2_SEL_TGT_CROP,
				CSI2_BE_PAD_SOURCE, fmt->which);
		struct intel_ipu4_isys_subdev *asd =
			to_intel_ipu4_isys_subdev(sd);
		u32 code = sink_ffmt->code;
		int idx = get_supported_code_index(code);

		if (asd->valid_tgts[CSI2_BE_PAD_SOURCE].crop && idx >= 0) {
			int crop_info = 0;

			if (r->top & 1)
				crop_info |= CSI2_BE_CROP_VER;
			if (r->left & 1)
				crop_info |= CSI2_BE_CROP_HOR;
			code = csi2_be_supported_codes_pad[
				((idx & CSI2_BE_CROP_MASK) ^ crop_info)
				+ (idx & ~CSI2_BE_CROP_MASK)];
		}

		ffmt->width = r->width;
		ffmt->height = r->height;
		ffmt->code = code;
		ffmt->field = sink_ffmt->field;
		return;
	}
	default:
		BUG_ON(1);
	}
}

void intel_ipu4_isys_csi2_be_cleanup(struct intel_ipu4_isys_csi2_be *csi2_be)
{
	v4l2_device_unregister_subdev(&csi2_be->asd.sd);
	intel_ipu4_isys_subdev_cleanup(&csi2_be->asd);
	intel_ipu4_isys_video_cleanup(&csi2_be->av);
}

int intel_ipu4_isys_csi2_be_init(struct intel_ipu4_isys_csi2_be *csi2_be,
				struct intel_ipu4_isys *isys)
{
	struct v4l2_subdev_format fmt = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
		.pad = CSI2_BE_PAD_SINK,
		.format = {
			.width = 4096,
			.height = 3072,
		},
	};
	struct v4l2_subdev_selection sel = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
		.pad = CSI2_BE_PAD_SOURCE,
		.target = V4L2_SEL_TGT_CROP,
		.r = {
			.width = fmt.format.width,
			.height = fmt.format.height,
		},
	};
	int rval;

	csi2_be->asd.sd.entity.ops = &csi2_be_entity_ops;
	csi2_be->asd.isys = isys;

	rval = intel_ipu4_isys_subdev_init(&csi2_be->asd, &csi2_be_sd_ops, 0,
			NR_OF_CSI2_BE_PADS, NR_OF_CSI2_BE_STREAMS,
			NR_OF_CSI2_BE_SOURCE_PADS,
			NR_OF_CSI2_BE_SINK_PADS, 0);
	if (rval)
		goto fail;

	csi2_be->asd.pad[CSI2_BE_PAD_SINK].flags = MEDIA_PAD_FL_SINK
		| MEDIA_PAD_FL_MUST_CONNECT;
	csi2_be->asd.pad[CSI2_BE_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	csi2_be->asd.valid_tgts[CSI2_BE_PAD_SOURCE].crop = true;
	csi2_be->asd.set_ffmt = csi2_be_set_ffmt;
	csi2_be->asd.isys = isys;

	BUILD_BUG_ON(ARRAY_SIZE(csi2_be_supported_codes) !=
						NR_OF_CSI2_BE_PADS);
	csi2_be->asd.supported_codes = csi2_be_supported_codes;
	csi2_be->asd.be_mode = INTEL_IPU4_BE_RAW;
	csi2_be->asd.isl_mode = INTEL_IPU4_ISL_CSI2_BE;

	intel_ipu4_isys_subdev_set_ffmt(&csi2_be->asd.sd, NULL, &fmt);
	ipu4_isys_csi2_be_set_sel(&csi2_be->asd.sd, NULL, &sel);

	csi2_be->asd.sd.internal_ops = &csi2_be_sd_internal_ops;
	snprintf(csi2_be->asd.sd.name, sizeof(csi2_be->asd.sd.name),
		 INTEL_IPU4_ISYS_ENTITY_PREFIX " CSI2 BE");
	snprintf(csi2_be->av.vdev.name, sizeof(csi2_be->av.vdev.name),
		 INTEL_IPU4_ISYS_ENTITY_PREFIX " CSI2 BE capture");
	csi2_be->av.aq.css_pin_type = IPU_FW_ISYS_PIN_TYPE_RAW_NS;
	v4l2_set_subdevdata(&csi2_be->asd.sd, &csi2_be->asd);
	rval = v4l2_device_register_subdev(&isys->v4l2_dev, &csi2_be->asd.sd);
	if (rval) {
		dev_info(&isys->adev->dev, "can't register v4l2 subdev\n");
		goto fail;
	}

	csi2_be->av.isys = isys;

	if (is_intel_ipu5_hw_a0(isys->adev->isp))
		csi2_be->av.pfmts = intel_ipu5_isys_pfmts;
	else
		csi2_be->av.pfmts = intel_ipu4_isys_pfmts;

	csi2_be->av.try_fmt_vid_mplane =
		intel_ipu4_isys_video_try_fmt_vid_mplane_default;
	csi2_be->av.prepare_firmware_stream_cfg =
		intel_ipu4_isys_prepare_firmware_stream_cfg_default;
	csi2_be->av.aq.buf_prepare = intel_ipu4_isys_buf_prepare;
	csi2_be->av.aq.fill_frame_buff_set_pin =
		intel_ipu4_isys_buffer_list_to_ipu_fw_isys_frame_buff_set_pin;
	csi2_be->av.aq.link_fmt_validate = intel_ipu4_isys_link_fmt_validate;
	csi2_be->av.aq.vbq.buf_struct_size =
		sizeof(struct intel_ipu4_isys_video_buffer);

	rval = intel_ipu4_isys_video_init(&csi2_be->av, &csi2_be->asd.sd.entity,
			CSI2_BE_PAD_SOURCE, MEDIA_PAD_FL_SINK, 0);
	if (rval) {
		dev_info(&isys->adev->dev, "can't init video node\n");
		goto fail;
	}

	return 0;

fail:
	intel_ipu4_isys_csi2_be_cleanup(csi2_be);

	return rval;
}

void intel_ipu4_isys_csi2_be_isr(struct intel_ipu4_isys_csi2_be *csi2_be)
{
}
