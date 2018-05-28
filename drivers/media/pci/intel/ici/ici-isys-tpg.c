// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
/*
 * Copyright (C) 2018 Intel Corporation
 */

#include "./ici/ici-isys.h"
#ifdef ICI_ENABLED
#include <media/ici.h>
#include "./ici/ici-isys-subdev.h"
#include "./ici/ici-isys-stream.h"
#include "./ici/ici-isys-tpg.h"
#include "ipu-isys-tpg.h"
#include "isysapi/interface/ia_css_isysapi_fw_types.h"

#define MIPI_GEN_PPC		4

#define ici_asd_to_tpg(__asd) \
	container_of(__asd, struct ici_isys_tpg, asd)

static const uint32_t ici_tpg_supported_codes_pad[] = {
	ICI_FORMAT_SBGGR8,
	ICI_FORMAT_SGBRG8,
	ICI_FORMAT_SGRBG8,
	ICI_FORMAT_SRGGB8,
	ICI_FORMAT_SBGGR10,
	ICI_FORMAT_SGBRG10,
	ICI_FORMAT_SGRBG10,
	ICI_FORMAT_SRGGB10,
	0,
};

static const uint32_t *ici_tpg_supported_codes[] = {
	ici_tpg_supported_codes_pad,
};

static int set_stream(
	struct ici_isys_node *node,
	void* ip,
	int enable)
{
	struct ici_isys_subdev *asd = node->sd;
	struct ici_isys_tpg *tpg =
		to_ici_isys_tpg(asd);
	unsigned int bpp =
		ici_isys_format_code_to_bpp(tpg->asd.
						    ffmt[TPG_PAD_SOURCE].
						    pixelformat);
	/*
	 * In B0 MIPI_GEN block is CSI2 FB. Need to enable/disable TPG selection
	 * register to control the TPG streaming.
	 */
	writel(enable ? 1 : 0, tpg->sel);

	if (!enable) {
		writel(0, tpg->base + MIPI_GEN_REG_COM_ENABLE);
		return 0;
	}

	writel(MIPI_GEN_COM_DTYPE_RAW(bpp),
		tpg->base + MIPI_GEN_REG_COM_DTYPE);
	writel(ici_isys_format_code_to_mipi
		(tpg->asd.ffmt[TPG_PAD_SOURCE].pixelformat),
		tpg->base + MIPI_GEN_REG_COM_VTYPE);

	writel(0, tpg->base + MIPI_GEN_REG_COM_VCHAN);
	writel(DIV_ROUND_UP
		(tpg->asd.ffmt[TPG_PAD_SOURCE].width * bpp, BITS_PER_BYTE),
		tpg->base + MIPI_GEN_REG_COM_WCOUNT);

	writel(0, tpg->base + MIPI_GEN_REG_SYNG_NOF_FRAMES);

	writel(DIV_ROUND_UP(tpg->asd.ffmt[TPG_PAD_SOURCE].width, MIPI_GEN_PPC),
		tpg->base + MIPI_GEN_REG_SYNG_NOF_PIXELS);
	writel(tpg->asd.ffmt[TPG_PAD_SOURCE].height,
		tpg->base + MIPI_GEN_REG_SYNG_NOF_LINES);

	writel(0, tpg->base + MIPI_GEN_REG_TPG_MODE);

	writel(-1, tpg->base + MIPI_GEN_REG_TPG_HCNT_MASK);
	writel(-1, tpg->base + MIPI_GEN_REG_TPG_VCNT_MASK);
	writel(-1, tpg->base + MIPI_GEN_REG_TPG_XYCNT_MASK);
	writel(0, tpg->base + MIPI_GEN_REG_TPG_HCNT_DELTA);
	writel(0, tpg->base + MIPI_GEN_REG_TPG_VCNT_DELTA);

	writel(2, tpg->base + MIPI_GEN_REG_COM_ENABLE);
	return 0;
}

static const char *const tpg_mode_items[] = {
	"Ramp",
	"Checkerboard",		/* Does not work, disabled. */
	"Frame Based Colour",
	NULL,
};

void ici_tpg_set_ffmt(struct ici_isys_subdev *asd,
			unsigned pad,
			struct ici_framefmt *ffmt)
{
	struct ici_framefmt *cur_ffmt =
		__ici_isys_subdev_get_ffmt(asd, pad);

	ffmt->field = ICI_FIELD_NONE;
	ffmt->colorspace = 0;
	memset(ffmt->reserved, 0, sizeof(ffmt->reserved));
	if (cur_ffmt) {
            *cur_ffmt = *ffmt;
	     dev_dbg(&asd->isys->adev->dev, "%s: TPG ici stream set format\n"
	        "width: %u, height: %u, pixelformat: %u, colorspace: %u field: %u\n",
		__func__,
		cur_ffmt->width,
		cur_ffmt->height,
		cur_ffmt->pixelformat, cur_ffmt->colorspace, cur_ffmt->field);
	}
}

static int ici_tpg_pipeline_validate(
	struct node_pipeline *inp,
	struct ici_isys_node *node)
{
	struct ici_isys_subdev* asd = node->sd;
	struct ici_isys_tpg *tpg =
		ici_asd_to_tpg(asd);
	struct ici_isys_pipeline *ip =
		ici_nodepipe_to_pipeline(inp);

	ip->asd_source = &tpg->asd;
	ip->asd_source_pad_id = TPG_PAD_SOURCE;
	return 0;
}

int ici_isys_tpg_init(struct ici_isys_tpg *tpg,
				struct ici_isys *isys,
				void __iomem *base, void __iomem *sel,
				unsigned int index)
{
	struct ici_pad_framefmt fmt = {
		.pad.pad_idx = TPG_PAD_SOURCE,
		.ffmt = {
				.width = 4096,
				.height = 3072,
			},
	};

	int rval;
	char name[ICI_MAX_NODE_NAME];

	dev_dbg(&isys->adev->dev, "ici_isys_tpg_init\n");

	tpg->isys = isys;
	tpg->base = base;
	tpg->index = index;
	tpg->sel = sel;
	tpg->asd.isys = isys;

	snprintf(name, sizeof(name),
			 IPU_ISYS_ENTITY_PREFIX " TPG %u", index);
	rval = ici_isys_subdev_init(&tpg->asd,
		name, NR_OF_TPG_PADS, 0);
	if (rval)
		goto fail;

	tpg->asd.pads[TPG_PAD_SOURCE].flags = ICI_PAD_FLAGS_SOURCE;

	tpg->asd.source = IA_CSS_ISYS_STREAM_SRC_MIPIGEN_PORT0 + index;
	tpg->asd.supported_codes = ici_tpg_supported_codes;
	tpg->asd.set_ffmt_internal = ici_tpg_set_ffmt;
	tpg->asd.node.node_set_streaming = set_stream;
	tpg->asd.node.node_pipeline_validate =
		ici_tpg_pipeline_validate;
	tpg->asd.node.node_set_pad_ffmt(&tpg->asd.node, &fmt);
	tpg->as.isys = isys;
	tpg->as.try_fmt_vid_mplane =
		ici_isys_video_try_fmt_vid_mplane_default;
	tpg->as.prepare_firmware_stream_cfg =
		ici_isys_prepare_firmware_stream_cfg_default;
	tpg->as.pfmts = ici_isys_pfmts_packed;
	tpg->as.packed = true;
	tpg->as.buf_list.css_pin_type = IA_CSS_ISYS_PIN_TYPE_MIPI;
	tpg->as.line_header_length =
		INTEL_IPU4_ISYS_CSI2_LONG_PACKET_HEADER_SIZE;
	tpg->as.line_footer_length =
		INTEL_IPU4_ISYS_CSI2_LONG_PACKET_FOOTER_SIZE;

	/*TODO:*/
	/*
	 * Buffer queue management call backs to be added.
	 */

	rval = ici_isys_stream_init(&tpg->as, &tpg->asd,
			&tpg->asd.node, TPG_PAD_SOURCE,
			ICI_PAD_FLAGS_SINK);
	if (rval) {
		dev_err(&isys->adev->dev, "can't init stream node\n");
		goto fail;
	}

	return 0;

fail:
	ici_isys_tpg_cleanup(tpg);

	return 1;
}
EXPORT_SYMBOL(ici_isys_tpg_init);

void ici_isys_tpg_cleanup(struct ici_isys_tpg *tpg)
{
	ici_isys_subdev_cleanup(&tpg->asd);
	ici_isys_stream_cleanup(&tpg->as);
}
EXPORT_SYMBOL(ici_isys_tpg_cleanup);

#endif /*ICI_ENABLED*/
