// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
/*
 * Copyright (C) 2018 Intel Corporation
 */

#include "./ici/ici-isys.h"
#ifdef ICI_ENABLED

#ifndef IPU4_DEBUG
#define IPU4_DEBUG 1
#endif

#include "./ici/ici-isys-csi2-be.h"
#include "isysapi/interface/ia_css_isysapi_fw_types.h"

#define ici_asd_to_csi2_be(__asd) \
	container_of(__asd, struct ici_isys_csi2_be, asd)

static const uint32_t ici_csi2_be_supported_codes_pad[] = {
	ICI_FORMAT_SBGGR12,
	ICI_FORMAT_SGBRG12,
	ICI_FORMAT_SGRBG12,
	ICI_FORMAT_SRGGB12,
	ICI_FORMAT_SBGGR10,
	ICI_FORMAT_SGBRG10,
	ICI_FORMAT_SGRBG10,
	ICI_FORMAT_SRGGB10,
	ICI_FORMAT_SBGGR8,
	ICI_FORMAT_SGBRG8,
	ICI_FORMAT_SGRBG8,
	ICI_FORMAT_SRGGB8,
	0,
};

static const uint32_t ici_csi2_be_soc_supported_codes_pad[] = {
	ICI_FORMAT_RGB888,
	ICI_FORMAT_RGB565,
	ICI_FORMAT_UYVY,
	ICI_FORMAT_YUYV,
	ICI_FORMAT_SBGGR12,
	ICI_FORMAT_SGBRG12,
	ICI_FORMAT_SGRBG12,
	ICI_FORMAT_SRGGB12,
	ICI_FORMAT_SBGGR10,
	ICI_FORMAT_SGBRG10,
	ICI_FORMAT_SGRBG10,
	ICI_FORMAT_SRGGB10,
	ICI_FORMAT_SBGGR8,
	ICI_FORMAT_SGBRG8,
	ICI_FORMAT_SGRBG8,
	ICI_FORMAT_SRGGB8,
	0,
};

static const uint32_t *ici_csi2_be_supported_codes[] = {
	ici_csi2_be_supported_codes_pad,
	ici_csi2_be_supported_codes_pad,
};

static const uint32_t *ici_csi2_be_soc_supported_codes[] = {
	ici_csi2_be_soc_supported_codes_pad,
	ici_csi2_be_soc_supported_codes_pad,
};

static int get_supported_code_index(uint32_t code)
{
	int i;

	for (i = 0; ici_csi2_be_supported_codes_pad[i]; i++) {
		if (ici_csi2_be_supported_codes_pad[i] == code)
			return i;
	}
	return -EINVAL;
}

void ici_csi2_be_set_ffmt(struct ici_isys_subdev *asd,
			unsigned pad,
			struct ici_framefmt *ffmt)
{
	struct ici_framefmt *cur_ffmt =
		__ici_isys_subdev_get_ffmt(asd, pad);
	int idx=0;
	if (!cur_ffmt)
	    return;

	ffmt->colorspace = 0;
	memset(ffmt->reserved, 0, sizeof(ffmt->reserved));
	switch (pad) {
	case CSI2_BE_ICI_PAD_SINK:
		DEBUGK("%s: sink pad %u\n", __func__, pad);
		if (ffmt->field != ICI_FIELD_ALTERNATE)
			ffmt->field = ICI_FIELD_NONE;
		*cur_ffmt = *ffmt;

		ici_isys_subdev_fmt_propagate(asd, pad, NULL,
				ICI_ISYS_SUBDEV_PROP_TGT_SINK_FMT,
							  ffmt);
		break;
	case CSI2_BE_ICI_PAD_SOURCE: {
		struct ici_framefmt *sink_ffmt =
			__ici_isys_subdev_get_ffmt(asd,
					CSI2_BE_ICI_PAD_SINK);

		struct ici_rect *r =
			&asd->crop[CSI2_BE_ICI_PAD_SOURCE];

		u32 code = 0;
		if (sink_ffmt)
		    code = sink_ffmt->pixelformat;

		idx = get_supported_code_index(code);

		DEBUGK("%s: source pad %u\n", __func__, pad);

		if (asd->valid_tgts[CSI2_BE_ICI_PAD_SOURCE].crop
			&& idx >= 0) {
			int crop_info = 0;

			DEBUGK("%s: setting CROP, pad %u\n", __func__,
				pad);

			if (r->top & 1)
				crop_info |= CSI2_BE_ICI_CROP_VER;
			if (r->left & 1)
				crop_info |= CSI2_BE_ICI_CROP_HOR;
			code = ici_csi2_be_supported_codes_pad[((
					idx &
					CSI2_BE_ICI_CROP_MASK)
					^
					crop_info)
					+
					(idx &
					~CSI2_BE_ICI_CROP_MASK)];
		}

		DEBUGK("%s: setting to w:%u,h:%u,pf:%u,field:%u\n",
			__func__, r->width,
			r->height, code, sink_ffmt->field);
		cur_ffmt->width = r->width;
		cur_ffmt->height = r->height;
		cur_ffmt->pixelformat = code;
		cur_ffmt->field = sink_ffmt->field;
		*ffmt = *cur_ffmt;
		break;
		}
	default:
		BUG_ON(1);
	}
}

static int ici_csi2_be_set_stream(
	struct ici_isys_node *node,
	void* ip,
	int state)
{
	return 0;
}

static int ici_csi2_be_pipeline_validate(
	struct node_pipeline *inp,
	struct ici_isys_node *node)
{
	struct ici_isys_subdev* asd = node->sd;
	struct ici_isys_csi2_be *csi2_be =
		ici_asd_to_csi2_be(asd);
	struct ici_isys_pipeline *ip =
		ici_nodepipe_to_pipeline(inp);

        ip->csi2_be = csi2_be;
        return 0;
}

int ici_isys_csi2_be_init(struct ici_isys_csi2_be
					*csi2_be,
					struct ici_isys *isys,
					unsigned int type)
{
	struct ici_pad_framefmt pff = {
		.pad.pad_idx = CSI2_BE_ICI_PAD_SINK,
		.ffmt = {
			.width = 4096,
			.height = 3072,
		},
	};
	int rval;
	char name[ICI_MAX_NODE_NAME];

	dev_info(&isys->adev->dev, "ici_isys_csi2_be_init\n");

	csi2_be->asd.isys = isys;
	if (type == ICI_BE_RAW) {
		csi2_be->as.buf_list.css_pin_type =
			IA_CSS_ISYS_PIN_TYPE_RAW_NS;
		snprintf(name, sizeof(name),
			IPU_ISYS_ENTITY_PREFIX " CSI2 BE");
	} else if (type >= ICI_BE_SOC) {
		csi2_be->as.buf_list.css_pin_type =
			IA_CSS_ISYS_PIN_TYPE_RAW_SOC;
		snprintf(name, sizeof(name),
			IPU_ISYS_ENTITY_PREFIX " CSI2 BE SOC %u", type-1);
	} else {
		return -EINVAL;
	}

	rval = ici_isys_subdev_init(&csi2_be->asd,
						name,
						NR_OF_CSI2_BE_ICI_PADS,
						0);
	if (rval) {
		dev_err(&isys->adev->dev, "can't init subdevice\n");
		goto fail_subdev;
	}

	csi2_be->asd.pads[CSI2_BE_ICI_PAD_SINK].flags = ICI_PAD_FLAGS_SINK
		| ICI_PAD_FLAGS_MUST_CONNECT;
	csi2_be->asd.pads[CSI2_BE_ICI_PAD_SOURCE].flags =
		ICI_PAD_FLAGS_SOURCE;

	if (type == ICI_BE_RAW)
		csi2_be->asd.valid_tgts[CSI2_BE_ICI_PAD_SOURCE].crop = true;
	else
		csi2_be->asd.valid_tgts[CSI2_BE_ICI_PAD_SOURCE].crop = false;

	csi2_be->asd.set_ffmt_internal = ici_csi2_be_set_ffmt;

	if (type == ICI_BE_RAW) {
		csi2_be->asd.supported_codes = ici_csi2_be_supported_codes;
		csi2_be->asd.be_mode = ICI_BE_RAW;
		csi2_be->asd.isl_mode = ICI_ISL_CSI2_BE;
	} else {
		csi2_be->asd.supported_codes = ici_csi2_be_soc_supported_codes;
		csi2_be->asd.be_mode = ICI_BE_SOC;
		csi2_be->asd.isl_mode = ICI_ISL_OFF;
	}

	csi2_be->asd.node.node_set_pad_ffmt(&csi2_be->asd.node, &pff);
	/* ipu4_isys_csi2_be2_set_sel(&csi2_be->asd.sd, NULL, &sel); */
	/* csi2_be->asd.sd.internal_ops = &csi2_be_sd_internal_ops; */
	csi2_be->asd.node.node_set_streaming =
		ici_csi2_be_set_stream;
	csi2_be->asd.node.node_pipeline_validate =
		ici_csi2_be_pipeline_validate;


	csi2_be->as.isys = isys;
	if (type == ICI_BE_RAW)
		csi2_be->as.pfmts = ici_isys_pfmts;
	else
		csi2_be->as.pfmts = ici_isys_pfmts_be_soc;

	csi2_be->as.try_fmt_vid_mplane =
		ici_isys_video_try_fmt_vid_mplane_default;
	csi2_be->as.prepare_firmware_stream_cfg =
		ici_isys_prepare_firmware_stream_cfg_default;

	rval = ici_isys_stream_init(&csi2_be->as, &csi2_be->asd,
				&csi2_be->asd.node, CSI2_BE_ICI_PAD_SOURCE,
				ICI_PAD_FLAGS_SINK);
	if (rval) {
		dev_err(&isys->adev->dev, "can't init stream node\n");
		goto fail_stream;
	}
	return 0;

fail_stream:
	ici_isys_subdev_cleanup(&csi2_be->asd);
fail_subdev:
	return rval;
}
EXPORT_SYMBOL(ici_isys_csi2_be_init);

void ici_isys_csi2_be_cleanup(struct ici_isys_csi2_be
					  *csi2_be)
{
	ici_isys_subdev_cleanup(&csi2_be->asd);
	ici_isys_stream_cleanup(&csi2_be->as);
}
EXPORT_SYMBOL(ici_isys_csi2_be_cleanup);

#endif /* ICI_ENABLED */
