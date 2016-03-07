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

#include <linux/types.h>
#include <linux/videodev2.h>

#include <media/media-entity.h>

#include <uapi/linux/media-bus-format.h>

#include "intel-ipu4-isys.h"
#include "intel-ipu4-isys-video.h"
#include "intel-ipu4-isys-subdev.h"

unsigned int intel_ipu4_isys_mbus_code_to_bpp(u32 code)
{
	switch (code) {
	case MEDIA_BUS_FMT_RGB888_1X24:
		return 24;
	case MEDIA_BUS_FMT_RGB565_1X16:
	case MEDIA_BUS_FMT_UYVY8_1X16:
	case MEDIA_BUS_FMT_YUYV8_1X16:
		return 16;
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
		return 12;
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
		return 10;
	case MEDIA_BUS_FMT_SBGGR8_1X8:
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SRGGB8_1X8:
		return 8;
	default:
		BUG_ON(1);
	}
}

unsigned int intel_ipu4_isys_mbus_code_to_mipi(u32 code)
{
	switch (code) {
	case MEDIA_BUS_FMT_RGB565_1X16:
		return INTEL_IPU4_ISYS_MIPI_CSI2_TYPE_RGB565;
	case MEDIA_BUS_FMT_RGB888_1X24:
		return INTEL_IPU4_ISYS_MIPI_CSI2_TYPE_RGB888;
	case MEDIA_BUS_FMT_UYVY8_1X16:
	case MEDIA_BUS_FMT_YUYV8_1X16:
		return INTEL_IPU4_ISYS_MIPI_CSI2_TYPE_YUV422_8;
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
		return INTEL_IPU4_ISYS_MIPI_CSI2_TYPE_RAW12;
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
		return INTEL_IPU4_ISYS_MIPI_CSI2_TYPE_RAW10;
	case MEDIA_BUS_FMT_SBGGR8_1X8:
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SRGGB8_1X8:
		return INTEL_IPU4_ISYS_MIPI_CSI2_TYPE_RAW8;
	default:
		BUG_ON(1);
	}
}

enum intel_ipu4_isys_subdev_pixelorder
intel_ipu4_isys_subdev_get_pixelorder(u32 code)
{
	switch (code) {
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SBGGR8_1X8:
		return INTEL_IPU4_ISYS_SUBDEV_PIXELORDER_BGGR;
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGBRG8_1X8:
		return INTEL_IPU4_ISYS_SUBDEV_PIXELORDER_GBRG;
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SGRBG8_1X8:
		return INTEL_IPU4_ISYS_SUBDEV_PIXELORDER_GRBG;
	case MEDIA_BUS_FMT_SRGGB12_1X12:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
	case MEDIA_BUS_FMT_SRGGB8_1X8:
		return INTEL_IPU4_ISYS_SUBDEV_PIXELORDER_RGGB;
	default:
		BUG_ON(1);
	}
}

struct v4l2_mbus_framefmt *__intel_ipu4_isys_get_ffmt(
	struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
	struct v4l2_subdev_fh *cfg,
#else
	struct v4l2_subdev_pad_config *cfg,
#endif
	unsigned int pad, unsigned int which)
{
	struct intel_ipu4_isys_subdev *asd = to_intel_ipu4_isys_subdev(sd);

	if (which == V4L2_SUBDEV_FORMAT_ACTIVE)
		return &asd->ffmt[pad];
	else
		return v4l2_subdev_get_try_format(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
			sd,
#endif
			cfg, pad);
}

struct v4l2_rect *__intel_ipu4_isys_get_selection(
	struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
	struct v4l2_subdev_fh *cfg,
#else
	struct v4l2_subdev_pad_config *cfg,
#endif
	unsigned int target, unsigned int pad, unsigned int which)
{
	struct intel_ipu4_isys_subdev *asd = to_intel_ipu4_isys_subdev(sd);

	if (which == V4L2_SUBDEV_FORMAT_ACTIVE) {
		switch (target) {
		case V4L2_SEL_TGT_CROP:
			return &asd->crop[pad];
		case V4L2_SEL_TGT_COMPOSE:
			return &asd->compose[pad];
		}
	} else {
		switch (target) {
		case V4L2_SEL_TGT_CROP:
			return v4l2_subdev_get_try_crop(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
				sd,
#endif
				cfg, pad);
		case V4L2_SEL_TGT_COMPOSE:
			return v4l2_subdev_get_try_compose(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
				sd,
#endif
				cfg, pad);
		}
	}
	BUG();
}

static int target_valid(struct v4l2_subdev *sd, unsigned int target,
			unsigned int pad)
{
	struct intel_ipu4_isys_subdev *asd = to_intel_ipu4_isys_subdev(sd);

	switch (target) {
	case V4L2_SEL_TGT_CROP:
		return asd->valid_tgts[pad].crop;
	case V4L2_SEL_TGT_COMPOSE:
		return asd->valid_tgts[pad].compose;
	default:
		return 0;
	}
}

void intel_ipu4_isys_subdev_fmt_propagate(
	struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
	struct v4l2_subdev_fh *cfg,
#else
	struct v4l2_subdev_pad_config *cfg,
#endif
	struct v4l2_mbus_framefmt *ffmt, struct v4l2_rect *r,
	enum isys_subdev_prop_tgt tgt, unsigned int pad, unsigned int which)
{
	struct intel_ipu4_isys_subdev *asd = to_intel_ipu4_isys_subdev(sd);
	struct v4l2_mbus_framefmt *ffmts[sd->entity.num_pads];
	struct v4l2_rect *crops[sd->entity.num_pads];
	struct v4l2_rect *compose[sd->entity.num_pads];
	unsigned int i;

	if (tgt == INTEL_IPU4_ISYS_SUBDEV_PROP_TGT_NR_OF)
		return;

	if (WARN_ON(pad >= sd->entity.num_pads))
		return;

	for (i = 0; i < sd->entity.num_pads; i++) {
		ffmts[i] = __intel_ipu4_isys_get_ffmt(sd, cfg, i, which);
		crops[i] = __intel_ipu4_isys_get_selection(
			sd, cfg, V4L2_SEL_TGT_CROP, i, which);
		compose[i] = __intel_ipu4_isys_get_selection(
			sd, cfg, V4L2_SEL_TGT_COMPOSE, i, which);
	}

	switch (tgt) {
	case INTEL_IPU4_ISYS_SUBDEV_PROP_TGT_SINK_FMT:
		crops[pad]->left = crops[pad]->top = 0;
		crops[pad]->width = ffmt->width;
		crops[pad]->height = ffmt->height;
		intel_ipu4_isys_subdev_fmt_propagate(sd, cfg, ffmt, crops[pad],
						     tgt + 1, pad, which);
		return;
	case INTEL_IPU4_ISYS_SUBDEV_PROP_TGT_SINK_CROP:
		if (WARN_ON(sd->entity.pads[pad].flags & MEDIA_PAD_FL_SOURCE))
			return;

		compose[pad]->left = compose[pad]->top = 0;
		compose[pad]->width = r->width;
		compose[pad]->height = r->height;
		intel_ipu4_isys_subdev_fmt_propagate(sd, cfg, ffmt,
						     compose[pad], tgt + 1,
						     pad, which);
		return;
	case INTEL_IPU4_ISYS_SUBDEV_PROP_TGT_SINK_COMPOSE:
		if (pad)
			return;

		for (i = 1; i < sd->entity.num_pads; i++) {
			if (!(sd->entity.pads[i].flags & MEDIA_PAD_FL_SOURCE))
				continue;

			compose[i]->left = compose[i]->top = 0;
			compose[i]->width = r->width;
			compose[i]->height = r->height;
			intel_ipu4_isys_subdev_fmt_propagate(
				sd, cfg, ffmt, compose[i], tgt + 1, i, which);
		}
		return;
	case INTEL_IPU4_ISYS_SUBDEV_PROP_TGT_SOURCE_COMPOSE:
		if (WARN_ON(sd->entity.pads[pad].flags & MEDIA_PAD_FL_SINK))
			return;

		crops[pad]->left = crops[pad]->top = 0;
		crops[pad]->width = r->width;
		crops[pad]->height = r->height;
		intel_ipu4_isys_subdev_fmt_propagate(sd, cfg, ffmt,
						     crops[pad], tgt + 1,
						     pad, which);
		return;
	case INTEL_IPU4_ISYS_SUBDEV_PROP_TGT_SOURCE_CROP: {
		struct v4l2_subdev_format fmt = {
			.which = which,
			.pad = pad,
			.format = {
				.width = r->width,
				.height = r->height,
				/*
				 * Either use the code from sink pad
				 * or the current one.
				 */
				.code = ffmt ? ffmt->code : ffmts[pad]->code,
			},
		};

		asd->set_ffmt(sd, cfg, &fmt);
		return;
	}
	}
}

void intel_ipu4_isys_subdev_set_ffmt_default(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
					     struct v4l2_subdev_fh *cfg,
#else
					     struct v4l2_subdev_pad_config *cfg,
#endif
					     struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *ffmt =
		__intel_ipu4_isys_get_ffmt(sd, cfg, fmt->pad, fmt->which);

	/* No propagation for non-zero pads. */
	if (fmt->pad) {
		struct v4l2_mbus_framefmt *sink_ffmt =
			__intel_ipu4_isys_get_ffmt(sd, cfg, 0, fmt->which);

		ffmt->width = sink_ffmt->width;
		ffmt->height = sink_ffmt->height;
		ffmt->code = sink_ffmt->code;
		ffmt->field = sink_ffmt->field;
		return;
	}

	ffmt->width = fmt->format.width;
	ffmt->height = fmt->format.height;
	ffmt->code = fmt->format.code;
	ffmt->field = fmt->format.field;

	intel_ipu4_isys_subdev_fmt_propagate(
		sd, cfg, &fmt->format, NULL,
		INTEL_IPU4_ISYS_SUBDEV_PROP_TGT_SINK_FMT, fmt->pad, fmt->which);
}

int __intel_ipu4_isys_subdev_set_ffmt(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
				 struct v4l2_subdev_fh *cfg,
#else
				 struct v4l2_subdev_pad_config *cfg,
#endif
				 struct v4l2_subdev_format *fmt)
{
	struct intel_ipu4_isys_subdev *asd = to_intel_ipu4_isys_subdev(sd);
	struct v4l2_mbus_framefmt *ffmt =
		__intel_ipu4_isys_get_ffmt(sd, cfg, fmt->pad, fmt->which);
	uint32_t code = asd->supported_codes[fmt->pad][0];
	unsigned int i;

	BUG_ON(!mutex_is_locked(&asd->mutex));

	fmt->format.width = clamp(fmt->format.width, INTEL_IPU4_ISYS_MIN_WIDTH,
				  INTEL_IPU4_ISYS_MAX_WIDTH);
	fmt->format.height = clamp(fmt->format.height,
				   INTEL_IPU4_ISYS_MIN_HEIGHT,
				   INTEL_IPU4_ISYS_MAX_HEIGHT);

	for (i = 0; asd->supported_codes[fmt->pad][i]; i++) {
		if (asd->supported_codes[fmt->pad][i] == fmt->format.code) {
			code = asd->supported_codes[fmt->pad][i];
			break;
		}
	}

	fmt->format.code = code;

	asd->set_ffmt(sd, cfg, fmt);

	fmt->format = *ffmt;

	return 0;
}

int intel_ipu4_isys_subdev_set_ffmt(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
				    struct v4l2_subdev_fh *cfg,
#else
				    struct v4l2_subdev_pad_config *cfg,
#endif
				 struct v4l2_subdev_format *fmt)
{
	struct intel_ipu4_isys_subdev *asd = to_intel_ipu4_isys_subdev(sd);
	int rval;

	mutex_lock(&asd->mutex);
	rval = __intel_ipu4_isys_subdev_set_ffmt(sd, cfg, fmt);
	mutex_unlock(&asd->mutex);

	return rval;
}

int intel_ipu4_isys_subdev_get_ffmt(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
				    struct v4l2_subdev_fh *cfg,
#else
				    struct v4l2_subdev_pad_config *cfg,
#endif
				 struct v4l2_subdev_format *fmt)
{
	struct intel_ipu4_isys_subdev *asd = to_intel_ipu4_isys_subdev(sd);

	mutex_lock(&asd->mutex);
	fmt->format = *__intel_ipu4_isys_get_ffmt(sd, cfg, fmt->pad, fmt->which);
	mutex_unlock(&asd->mutex);

	return 0;
}

int intel_ipu4_isys_subdev_set_sel(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
				   struct v4l2_subdev_fh *cfg,
#else
				   struct v4l2_subdev_pad_config *cfg,
#endif
				struct v4l2_subdev_selection *sel)
{
	struct intel_ipu4_isys_subdev *asd = to_intel_ipu4_isys_subdev(sd);
	struct media_pad *pad = &asd->sd.entity.pads[sel->pad];
	struct v4l2_rect *r, __r = { 0 };
	unsigned int tgt;

	if (!target_valid(sd, sel->target, sel->pad))
		return -EINVAL;

	switch (sel->target) {
	case V4L2_SEL_TGT_CROP:
		if (pad->flags & MEDIA_PAD_FL_SINK) {
			struct v4l2_mbus_framefmt *ffmt =
				__intel_ipu4_isys_get_ffmt(sd, cfg, sel->pad,
							   sel->which);

			__r.width = ffmt->width;
			__r.height = ffmt->height;
			r = &__r;
			tgt = INTEL_IPU4_ISYS_SUBDEV_PROP_TGT_SINK_CROP;
		} else {
			/* 0 is the sink pad. */
			r = __intel_ipu4_isys_get_selection(
				sd, cfg, sel->target, 0, sel->which);
			tgt = INTEL_IPU4_ISYS_SUBDEV_PROP_TGT_SOURCE_CROP;
		}

		break;
	case V4L2_SEL_TGT_COMPOSE:
		if (pad->flags & MEDIA_PAD_FL_SINK) {
			r = __intel_ipu4_isys_get_selection(
				sd, cfg, V4L2_SEL_TGT_CROP, sel->pad, sel->which);
			tgt = INTEL_IPU4_ISYS_SUBDEV_PROP_TGT_SINK_COMPOSE;
		} else {
			r = __intel_ipu4_isys_get_selection(
				sd, cfg, V4L2_SEL_TGT_COMPOSE, 0, sel->which);
			tgt = INTEL_IPU4_ISYS_SUBDEV_PROP_TGT_SOURCE_COMPOSE;
		}
		break;
	default:
		BUG();
	}

	sel->r.width = clamp(sel->r.width, INTEL_IPU4_ISYS_MIN_WIDTH,
			     r->width);
	sel->r.height = clamp(sel->r.height, INTEL_IPU4_ISYS_MIN_HEIGHT,
			      r->height);
	*__intel_ipu4_isys_get_selection(sd, cfg, sel->target, sel->pad,
					 sel->which) = sel->r;
	intel_ipu4_isys_subdev_fmt_propagate(sd, cfg, NULL, &sel->r, tgt,
					     sel->pad, sel->which);

	return 0;
}

int intel_ipu4_isys_subdev_get_sel(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
				   struct v4l2_subdev_fh *cfg,
#else
				   struct v4l2_subdev_pad_config *cfg,
#endif
				struct v4l2_subdev_selection *sel)
{
	if (!target_valid(sd, sel->target, sel->pad))
		return -EINVAL;

	sel->r = *__intel_ipu4_isys_get_selection(sd, cfg, sel->target,
						  sel->pad, sel->which);

	return 0;
}

int intel_ipu4_isys_subdev_enum_mbus_code(
	struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
	struct v4l2_subdev_fh *cfg,
#else
	struct v4l2_subdev_pad_config *cfg,
#endif
	struct v4l2_subdev_mbus_code_enum *code)
{
	struct intel_ipu4_isys_subdev *asd = to_intel_ipu4_isys_subdev(sd);
	const uint32_t *supported_codes = asd->supported_codes[code->pad];
	uint32_t index;

	for (index = 0; supported_codes[index]; index++) {
		if (index == code->index) {
			code->code = supported_codes[index];
			return 0;
		}
	}

	return -EINVAL;
}

/*
 * Besides validating the link, figure out the external pad and the
 * ISYS library source.
 */
int intel_ipu4_isys_subdev_link_validate(
	struct v4l2_subdev *sd, struct media_link *link,
	struct v4l2_subdev_format *source_fmt,
	struct v4l2_subdev_format *sink_fmt)
{
	struct v4l2_subdev *source_sd =
		media_entity_to_v4l2_subdev(link->source->entity);
	struct intel_ipu4_isys_pipeline *ip =
		container_of(sd->entity.pipe,
			     struct intel_ipu4_isys_pipeline, pipe);
	struct intel_ipu4_isys_subdev *asd = to_intel_ipu4_isys_subdev(sd);

	if (source_sd->owner != THIS_MODULE) {
		/*
		 * source_sd isn't ours --- sd must be the external
		 * sub-device.
		 */
		ip->external = link->source;
		ip->source = to_intel_ipu4_isys_subdev(sd)->source;
		dev_dbg(&asd->isys->adev->dev, "%s: using source %d\n",
			sd->entity.name, ip->source);
	} else if (source_sd->entity.num_pads == 1) {
		/* All internal sources have a single pad. */
		ip->external = link->source;
		ip->source = to_intel_ipu4_isys_subdev(source_sd)->source;

		dev_dbg(&asd->isys->adev->dev, "%s: using source %d\n",
			sd->entity.name, ip->source);
	}

	if (asd->isl_mode != INTEL_IPU4_ISL_OFF)
		ip->isl_mode = asd->isl_mode;

	return v4l2_subdev_link_validate_default(sd, link, source_fmt,
						 sink_fmt);
}

int intel_ipu4_isys_subdev_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct intel_ipu4_isys_subdev *asd = to_intel_ipu4_isys_subdev(sd);
	unsigned int i;

	mutex_lock(&asd->mutex);

	for (i = 0; i < asd->sd.entity.num_pads; i++) {
		struct v4l2_mbus_framefmt *try_fmt =
			v4l2_subdev_get_try_format(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
			sd, fh->pad,
#else
			fh,
#endif
			i);
		struct v4l2_rect *try_crop =
			v4l2_subdev_get_try_crop(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
				sd, fh->pad,
#else
				fh,
#endif
				i);
		struct v4l2_rect *try_compose =
			v4l2_subdev_get_try_compose(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
				sd, fh->pad,
#else
				fh,
#endif
				i);

		*try_fmt = asd->ffmt[i];
		*try_crop = asd->crop[i];
		*try_compose = asd->compose[i];
	}

	mutex_unlock(&asd->mutex);

	return 0;
}

int intel_ipu4_isys_subdev_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	return 0;
}

int intel_ipu4_isys_subdev_init(struct intel_ipu4_isys_subdev *asd,
			     struct v4l2_subdev_ops *ops, unsigned int nr_ctrls,
			     unsigned int num_pads)
{
	int rval = -EINVAL;

	mutex_init(&asd->mutex);

	v4l2_subdev_init(&asd->sd, ops);

	asd->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	asd->sd.owner = THIS_MODULE;

	asd->pad = devm_kcalloc(&asd->isys->adev->dev, num_pads,
				sizeof(*asd->pad), GFP_KERNEL);

	asd->ffmt = devm_kcalloc(&asd->isys->adev->dev, num_pads,
				 sizeof(*asd->ffmt), GFP_KERNEL);

	asd->crop = devm_kcalloc(&asd->isys->adev->dev, num_pads,
				 sizeof(*asd->crop), GFP_KERNEL);

	asd->compose = devm_kcalloc(&asd->isys->adev->dev, num_pads,
				 sizeof(*asd->compose), GFP_KERNEL);

	asd->valid_tgts = devm_kcalloc(&asd->isys->adev->dev, num_pads,
				       sizeof(*asd->valid_tgts), GFP_KERNEL);

	if (!asd->pad || !asd->ffmt || !asd->crop || !asd->compose ||
	    !asd->valid_tgts)
		return -ENOMEM;

	rval = media_entity_init(&asd->sd.entity, num_pads, asd->pad, 0);
	if (rval)
		goto out_mutex_destroy;

	if (asd->ctrl_init) {
		rval = v4l2_ctrl_handler_init(&asd->ctrl_handler, nr_ctrls);
		if (rval)
			goto out_media_entity_cleanup;

		asd->ctrl_init(&asd->sd);
		if (asd->ctrl_handler.error) {
			rval = asd->ctrl_handler.error;
			goto out_v4l2_ctrl_handler_free;
		}

		asd->sd.ctrl_handler = &asd->ctrl_handler;
	}

	asd->source = -1;

	return 0;

out_v4l2_ctrl_handler_free:
	v4l2_ctrl_handler_free(&asd->ctrl_handler);

out_media_entity_cleanup:
	media_entity_cleanup(&asd->sd.entity);

out_mutex_destroy:
	mutex_destroy(&asd->mutex);

	return rval;
}

void intel_ipu4_isys_subdev_cleanup(struct intel_ipu4_isys_subdev *asd)
{
	media_entity_cleanup(&asd->sd.entity);
	v4l2_ctrl_handler_free(&asd->ctrl_handler);
	mutex_destroy(&asd->mutex);
}
