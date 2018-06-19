// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
/*
 * Copyright (C) 2018 Intel Corporation
 */

#include "./ici/ici-isys.h"
#ifdef ICI_ENABLED

#include "./ici/ici-isys-subdev.h"
#include "./ici/ici-isys-pipeline.h"

unsigned int ici_isys_format_code_to_bpp(u32 code)
{
	switch (code) {
	case ICI_FORMAT_RGB888:
		return 24;
	case ICI_FORMAT_RGB565:
	case ICI_FORMAT_UYVY:
	case ICI_FORMAT_YUYV:
		return 16;
	case ICI_FORMAT_SBGGR12:
	case ICI_FORMAT_SGBRG12:
	case ICI_FORMAT_SGRBG12:
	case ICI_FORMAT_SRGGB12:
		return 12;
	case ICI_FORMAT_SBGGR10:
	case ICI_FORMAT_SGBRG10:
	case ICI_FORMAT_SGRBG10:
	case ICI_FORMAT_SRGGB10:
		return 10;
	case ICI_FORMAT_SBGGR8:
	case ICI_FORMAT_SGBRG8:
	case ICI_FORMAT_SGRBG8:
	case ICI_FORMAT_SRGGB8:
	case ICI_FORMAT_SBGGR10_DPCM8:
	case ICI_FORMAT_SGBRG10_DPCM8:
	case ICI_FORMAT_SGRBG10_DPCM8:
	case ICI_FORMAT_SRGGB10_DPCM8:
		return 8;
	default:
		BUG_ON(1);
	}
}

unsigned int ici_isys_format_code_to_mipi(u32 code)
{
	switch (code) {
	case ICI_FORMAT_RGB565:
		return ICI_ISYS_MIPI_CSI2_TYPE_RGB565;
	case ICI_FORMAT_RGB888:
		return ICI_ISYS_MIPI_CSI2_TYPE_RGB888;
	case ICI_FORMAT_UYVY:
	case ICI_FORMAT_YUYV:
		return ICI_ISYS_MIPI_CSI2_TYPE_YUV422_8;
	case ICI_FORMAT_SBGGR12:
	case ICI_FORMAT_SGBRG12:
	case ICI_FORMAT_SGRBG12:
	case ICI_FORMAT_SRGGB12:
		return ICI_ISYS_MIPI_CSI2_TYPE_RAW12;
	case ICI_FORMAT_SBGGR10:
	case ICI_FORMAT_SGBRG10:
	case ICI_FORMAT_SGRBG10:
	case ICI_FORMAT_SRGGB10:
		return ICI_ISYS_MIPI_CSI2_TYPE_RAW10;
	case ICI_FORMAT_SBGGR8:
	case ICI_FORMAT_SGBRG8:
	case ICI_FORMAT_SGRBG8:
	case ICI_FORMAT_SRGGB8:
		return ICI_ISYS_MIPI_CSI2_TYPE_RAW8;
	case ICI_FORMAT_SBGGR10_DPCM8:
	case ICI_FORMAT_SGBRG10_DPCM8:
	case ICI_FORMAT_SGRBG10_DPCM8:
	case ICI_FORMAT_SRGGB10_DPCM8:
		return ICI_ISYS_MIPI_CSI2_TYPE_USER_DEF(1);
	default:
		BUG_ON(1);
	}
}

enum ici_isys_subdev_pixelorder
ici_isys_subdev_get_pixelorder(u32 code)
{
	switch (code) {
	case ICI_FORMAT_SBGGR12:
	case ICI_FORMAT_SBGGR10:
	case ICI_FORMAT_SBGGR8:
	case ICI_FORMAT_SBGGR10_DPCM8:
		return ICI_ISYS_SUBDEV_PIXELORDER_BGGR;
	case ICI_FORMAT_SGBRG12:
	case ICI_FORMAT_SGBRG10:
	case ICI_FORMAT_SGBRG8:
	case ICI_FORMAT_SGBRG10_DPCM8:
		return ICI_ISYS_SUBDEV_PIXELORDER_GBRG;
	case ICI_FORMAT_SGRBG12:
	case ICI_FORMAT_SGRBG10:
	case ICI_FORMAT_SGRBG8:
	case ICI_FORMAT_SGRBG10_DPCM8:
		return ICI_ISYS_SUBDEV_PIXELORDER_GRBG;
	case ICI_FORMAT_SRGGB12:
	case ICI_FORMAT_SRGGB10:
	case ICI_FORMAT_SRGGB8:
	case ICI_FORMAT_SRGGB10_DPCM8:
		return ICI_ISYS_SUBDEV_PIXELORDER_RGGB;
	default:
		BUG_ON(1);
	}
}

unsigned int ici_isys_get_compression_scheme(u32 code)
{
	switch (code) {
	case ICI_FORMAT_SBGGR10_DPCM8:
	case ICI_FORMAT_SGBRG10_DPCM8:
	case ICI_FORMAT_SGRBG10_DPCM8:
	case ICI_FORMAT_SRGGB10_DPCM8:
		return 3;
	default:
		return 0;
	}
}

u32 ici_isys_subdev_code_to_uncompressed(u32 sink_code)
{
	switch (sink_code) {
	case ICI_FORMAT_SBGGR10_DPCM8:
		return ICI_FORMAT_SBGGR10;
	case ICI_FORMAT_SGBRG10_DPCM8:
		return ICI_FORMAT_SGBRG10;
	case ICI_FORMAT_SGRBG10_DPCM8:
		return ICI_FORMAT_SGRBG10;
	case ICI_FORMAT_SRGGB10_DPCM8:
		return ICI_FORMAT_SRGGB10;
	default:
		return sink_code;
	}
}

struct ici_framefmt *__ici_isys_subdev_get_ffmt(
				struct ici_isys_subdev *asd,
				unsigned pad)
{
	if (pad >= asd->num_pads)
		return NULL;

	return &asd->ffmt[pad];
}

int ici_isys_subdev_get_ffmt(
	struct ici_isys_node* node,
	struct ici_pad_framefmt* pff)
{
	int ret = 0;
	struct ici_framefmt *format_out;
	struct ici_isys_subdev *asd = node->sd;

	mutex_lock(&asd->mutex);
	format_out = __ici_isys_subdev_get_ffmt(asd,
		pff->pad.pad_idx);
	if (format_out)
		pff->ffmt = *format_out;
	else
		ret = -EINVAL;
	mutex_unlock(&asd->mutex);
	return ret;
}

static int __subdev_set_ffmt(struct ici_isys_subdev *asd,
			struct ici_pad_framefmt *pff)
{
	unsigned int i;
	unsigned pad = pff->pad.pad_idx;
	unsigned pixelformat;
	BUG_ON(!mutex_is_locked(&asd->mutex));

	if (pad >= asd->num_pads)
		return -EINVAL;

	pff->ffmt.width = clamp(pff->ffmt.width,
		IPU_ISYS_MIN_WIDTH,
		IPU_ISYS_MAX_WIDTH);
	pff->ffmt.height = clamp(pff->ffmt.height,
		IPU_ISYS_MIN_HEIGHT,
		IPU_ISYS_MAX_HEIGHT);

	pixelformat = asd->supported_codes[pad][0];
	for (i = 0; asd->supported_codes[pad][i]; i++) {
		if (asd->supported_codes[pad][i] ==
			pff->ffmt.pixelformat) {

			pixelformat = asd->supported_codes[pad][i];
			break;
		}
	}
	pff->ffmt.pixelformat = pixelformat;
	asd->set_ffmt_internal(asd, pad, &pff->ffmt);
	asd->ffmt[pad] = pff->ffmt;
	return 0;
}

int ici_isys_subdev_set_ffmt(
	struct ici_isys_node* node,
	struct ici_pad_framefmt* pff)
{
	int res;
	struct ici_isys_subdev *asd = node->sd;

	mutex_lock(&asd->mutex);
	res = __subdev_set_ffmt(asd, pff);
	mutex_unlock(&asd->mutex);
	return res;
}

int ici_isys_subdev_get_supported_format(
	struct ici_isys_node* node,
	struct ici_pad_supported_format_desc* psfd)
{
	struct ici_isys_subdev *asd = node->sd;
	int pad = psfd->pad.pad_idx;
	int idx = psfd->idx;
	int i;
	int rval = 0;

	mutex_lock(&asd->mutex);
	if (!asd->supported_code_counts[pad]) {
		for (i = 0; asd->supported_codes[pad][i]; i++) {}
		asd->supported_code_counts[pad] = i;
	}

	if (idx < asd->supported_code_counts[pad]) {
		psfd->color_format = asd->supported_codes[pad][idx];
		psfd->min_width = IPU_ISYS_MIN_WIDTH;
		psfd->max_width = IPU_ISYS_MAX_WIDTH;
		psfd->min_height = IPU_ISYS_MIN_HEIGHT;
		psfd->max_height = IPU_ISYS_MAX_HEIGHT;
	} else {
		rval = -EINVAL;
	}

	mutex_unlock(&asd->mutex);
	return rval;
}


int intel_ipu4_isys_subdev_set_crop_rect(struct ici_isys_subdev
					*asd, unsigned pad,
					struct ici_rect *r)
{
	struct node_pad *np;
	struct ici_rect rmax = { 0 };
	struct ici_rect *rcrop;
	unsigned int tgt;
	struct ici_framefmt *ffmt =
		__ici_isys_subdev_get_ffmt(asd, pad);

	if (!ffmt)
		return -EINVAL;
	if (!asd->valid_tgts[pad].crop)
		return -EINVAL;
	np = &asd->pads[pad];
	rcrop = &asd->crop[pad];

	if (np->flags & ICI_PAD_FLAGS_SINK) {
		rmax.width = ffmt->width;
		rmax.height = ffmt->height;
		tgt = ICI_ISYS_SUBDEV_PROP_TGT_SINK_CROP;
	} else {
		/* 0 is the sink pad. */
		rmax = asd->crop[0];
		tgt = ICI_ISYS_SUBDEV_PROP_TGT_SOURCE_CROP;
	}
	rcrop->width = clamp(r->width, IPU_ISYS_MIN_WIDTH, rmax.width);
	rcrop->height = clamp(r->height, IPU_ISYS_MIN_HEIGHT,
				rmax.height);
	ici_isys_subdev_fmt_propagate(asd, pad, rcrop, tgt, NULL);
	return 0;
}

int intel_ipu4_isys_subdev_set_compose_rect(struct ici_isys_subdev
						*asd, unsigned pad,
						struct ici_rect *r)
{
	struct node_pad *np;
	struct ici_rect rmax = { 0 };
	struct ici_rect *rcompose;
	unsigned int tgt;
	struct ici_framefmt *ffmt =
		__ici_isys_subdev_get_ffmt(asd, pad);

	if (!ffmt)
		return -EINVAL;
	if (!asd->valid_tgts[pad].compose)
		return -EINVAL;
	np = &asd->pads[pad];
	rcompose = &asd->compose[pad];

	if (np->flags & ICI_PAD_FLAGS_SINK) {
		rmax = asd->crop[pad];
		tgt = ICI_ISYS_SUBDEV_PROP_TGT_SINK_COMPOSE;
	} else {
		/* 0 is the sink pad. */
		rmax = asd->compose[0];
		tgt = ICI_ISYS_SUBDEV_PROP_TGT_SOURCE_COMPOSE;
	}
	rcompose->width = clamp(r->width, IPU_ISYS_MIN_WIDTH,
				rmax.width);
	rcompose->height = clamp(r->height, IPU_ISYS_MIN_HEIGHT,
				 rmax.height);
	ici_isys_subdev_fmt_propagate(asd, pad, rcompose, tgt,
						  NULL);
	return 0;
}

int ici_isys_subdev_set_sel(
	struct ici_isys_node* node,
	struct ici_pad_selection* ps)
{
	struct ici_isys_subdev *asd = node->sd;
	int rval = 0;

	if (WARN_ON(ps->pad.pad_idx >= asd->num_pads))
		return -EINVAL;

	mutex_lock(&asd->mutex);
	switch (ps->sel_type)
	{
		case ICI_EXT_SEL_TYPE_COMPOSE:
			rval = intel_ipu4_isys_subdev_set_compose_rect(
				asd, ps->pad.pad_idx, &ps->rect);
			break;
		case ICI_EXT_SEL_TYPE_CROP:
			rval = intel_ipu4_isys_subdev_set_crop_rect(
				asd, ps->pad.pad_idx, &ps->rect);
			break;
		default:
			rval = -EINVAL;
	}
	mutex_unlock(&asd->mutex);
	return rval;
}

int ici_isys_subdev_get_sel(
	struct ici_isys_node* node,
	struct ici_pad_selection* ps)
{
	struct ici_isys_subdev *asd = node->sd;
	int rval = 0;

	if (WARN_ON(ps->pad.pad_idx >= asd->num_pads))
		return -EINVAL;

	mutex_lock(&asd->mutex);
	switch (ps->sel_type)
	{
		case ICI_EXT_SEL_TYPE_COMPOSE:
			ps->rect = asd->compose[ps->pad.pad_idx];
			break;
		case ICI_EXT_SEL_TYPE_CROP:
			ps->rect = asd->crop[ps->pad.pad_idx];
			break;
		default:
			rval = -EINVAL;
	}
	mutex_unlock(&asd->mutex);
	return rval;
}

void ici_isys_subdev_fmt_propagate(
				struct ici_isys_subdev *asd,
				unsigned pad,
				struct ici_rect *r,
				enum ici_isys_subdev_prop_tgt tgt,
				struct ici_framefmt *ffmt)
{
	unsigned i;
	struct ici_framefmt *ffmts[asd->num_pads];
	struct ici_rect *crops[asd->num_pads];
	struct ici_rect *compose[asd->num_pads];

	if (WARN_ON(pad >= asd->num_pads))
		return;

	for (i = 0; i < asd->num_pads; i++) {
		ffmts[i] = __ici_isys_subdev_get_ffmt(asd, pad);
		crops[i] = &asd->crop[i];
		compose[i] = &asd->compose[i];
	}

	switch (tgt) {
	case ICI_ISYS_SUBDEV_PROP_TGT_SINK_FMT:
		crops[pad]->left = crops[pad]->top = 0;
		crops[pad]->width = ffmt->width;
		crops[pad]->height = ffmt->height;
		ici_isys_subdev_fmt_propagate(asd, pad,
							crops[pad], tgt + 1,
							ffmt);
		return;
	case ICI_ISYS_SUBDEV_PROP_TGT_SINK_CROP:
		if (WARN_ON(asd->pads[pad].flags & ICI_PAD_FLAGS_SOURCE))
			return;
		compose[pad]->left = compose[pad]->top = 0;
		compose[pad]->width = r->width;
		compose[pad]->height = r->height;
		ici_isys_subdev_fmt_propagate(asd, pad,
							compose[pad], tgt + 1,
							ffmt);
		return;
	case ICI_ISYS_SUBDEV_PROP_TGT_SINK_COMPOSE:
		if (!(asd->pads[pad].flags & ICI_PAD_FLAGS_SINK))
			return;

		for (i = 1; i < asd->num_pads; i++) {
			if (!(asd->pads[i].flags & ICI_PAD_FLAGS_SOURCE))
				continue;

			compose[i]->left = compose[i]->top = 0;
			compose[i]->width = r->width;
			compose[i]->height = r->height;
			ici_isys_subdev_fmt_propagate(asd, i,
								compose[i],
								tgt + 1,
								ffmt);
		}
		return;
	case ICI_ISYS_SUBDEV_PROP_TGT_SOURCE_COMPOSE:
		if (WARN_ON(asd->pads[pad].flags & ICI_PAD_FLAGS_SINK))
			return;

		crops[pad]->left = crops[pad]->top = 0;
		crops[pad]->width = r->width;
		crops[pad]->height = r->height;
		ici_isys_subdev_fmt_propagate(asd, pad,
							crops[pad], tgt + 1,
							ffmt);
		return;
	case ICI_ISYS_SUBDEV_PROP_TGT_SOURCE_CROP:{
			struct ici_framefmt fmt = {
				.width = r->width,
				.height = r->height,
				/*
				 * Either use the code from sink pad
				 * or the current one.
				 */
				.pixelformat = (ffmt ? ffmt->pixelformat :
						ffmts[pad]->pixelformat),
			};

			asd->set_ffmt_internal(asd, pad, &fmt);
			return;
		}
	}
}

int ici_isys_subdev_init(struct ici_isys_subdev *asd,
	const char* name,
	unsigned int num_pads,
	unsigned int index)
{
	int res = 0;

	mutex_init(&asd->mutex);
	asd->num_pads = num_pads;
	asd->pads = devm_kcalloc(&asd->isys->adev->dev, num_pads,
				sizeof(*asd->pads), GFP_KERNEL);

	asd->ffmt = devm_kcalloc(&asd->isys->adev->dev, num_pads,
				sizeof(*asd->ffmt), GFP_KERNEL);

	asd->crop = devm_kcalloc(&asd->isys->adev->dev, num_pads,
				sizeof(*asd->crop), GFP_KERNEL);

	asd->compose = devm_kcalloc(&asd->isys->adev->dev, num_pads,
				sizeof(*asd->compose), GFP_KERNEL);

	asd->valid_tgts = devm_kcalloc(&asd->isys->adev->dev, num_pads,
					sizeof(*asd->valid_tgts),
					GFP_KERNEL);

	asd->supported_code_counts = devm_kcalloc(&asd->isys->adev->dev,
		num_pads, sizeof(*asd->supported_code_counts),
		GFP_KERNEL);

	if (!asd->pads || !asd->ffmt || !asd->crop || !asd->compose ||
	    !asd->valid_tgts || !asd->supported_code_counts) {
		res = -ENOMEM;
		goto cleanup_allocs;
	}

	asd->isl_mode = ICI_ISL_OFF;
	asd->be_mode = ICI_BE_RAW;
	asd->source = -1;
	asd->index = index;

	asd->node.parent = &asd->isys->pipeline_dev;
	asd->node.sd = asd;
	asd->node.external = false;

	res = ici_isys_pipeline_node_init(asd->isys,
		&asd->node, name, asd->num_pads, asd->pads);
	if (res)
		goto cleanup_allocs;

	asd->node.node_set_pad_ffmt =
		ici_isys_subdev_set_ffmt;
	asd->node.node_get_pad_ffmt =
		ici_isys_subdev_get_ffmt;
	asd->node.node_get_pad_supported_format =
		ici_isys_subdev_get_supported_format;
	asd->node.node_set_pad_sel =
		ici_isys_subdev_set_sel;
	asd->node.node_get_pad_sel =
		ici_isys_subdev_get_sel;

	return 0;

cleanup_allocs:
	if (asd->valid_tgts)
		devm_kfree(&asd->isys->adev->dev, asd->valid_tgts);
	if (asd->compose)
		devm_kfree(&asd->isys->adev->dev, asd->compose);
	if (asd->crop)
		devm_kfree(&asd->isys->adev->dev, asd->crop);
	if (asd->ffmt)
		devm_kfree(&asd->isys->adev->dev, asd->ffmt);
	if (asd->pads)
		devm_kfree(&asd->isys->adev->dev, asd->pads);
	mutex_destroy(&asd->mutex);
	return res;
}

void ici_isys_subdev_cleanup(
	struct ici_isys_subdev *asd)
{
	list_del(&asd->node.node_entry);

	if (asd->valid_tgts)
		devm_kfree(&asd->isys->adev->dev, asd->valid_tgts);
	if (asd->compose)
		devm_kfree(&asd->isys->adev->dev, asd->compose);
	if (asd->crop)
		devm_kfree(&asd->isys->adev->dev, asd->crop);
	if (asd->ffmt)
		devm_kfree(&asd->isys->adev->dev, asd->ffmt);
	if (asd->pads)
		devm_kfree(&asd->isys->adev->dev, asd->pads);
	mutex_destroy(&asd->mutex);
}

#endif /*ICI_ENABLED*/
