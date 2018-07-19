// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
/*
 * Copyright (C) 2018 Intel Corporation
 */

#include "./ici/ici-isys.h"
#ifdef ICI_ENABLED
#include <media/ici.h>
#include "./ici/ici-isys-subdev.h"
#include "./ici/ici-isys-stream.h"
#include "./ici/ici-isys-csi2.h"
#include "isysapi/interface/ia_css_isysapi_fw_types.h"
#include "ipu-platform-isys-csi2-reg.h"
//#include "intel-ipu-isys-csi2-common.h"

#define CSI2_ACCINV 8

#define ici_asd_to_csi2(__asd, index) \
	container_of(__asd, struct ici_isys_csi2, asd[index])

static const uint32_t ici_csi2_supported_codes_pad_sink[] = {
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
	ICI_FORMAT_SBGGR10_DPCM8,
	ICI_FORMAT_SGBRG10_DPCM8,
	ICI_FORMAT_SGRBG10_DPCM8,
	ICI_FORMAT_SRGGB10_DPCM8,
	0,
};

static const uint32_t ici_csi2_supported_codes_pad_source[] = {
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

static const uint32_t *ici_csi2_supported_codes[] = {
	ici_csi2_supported_codes_pad_sink,
	ici_csi2_supported_codes_pad_source,
};

void ici_csi2_set_ffmt(struct ici_isys_subdev *asd,
			unsigned pad,
			struct ici_framefmt *ffmt)
{
	struct ici_framefmt *cur_ffmt =
		__ici_isys_subdev_get_ffmt(asd, pad);

	if (ffmt->field != ICI_FIELD_ALTERNATE)
		ffmt->field = ICI_FIELD_NONE;
	ffmt->colorspace = 0;
	memset(ffmt->reserved, 0, sizeof(ffmt->reserved));

	switch (pad) {
	case CSI2_ICI_PAD_SINK:
		if (cur_ffmt)
			*cur_ffmt = *ffmt;
		ici_isys_subdev_fmt_propagate(asd, pad, NULL,
				ICI_ISYS_SUBDEV_PROP_TGT_SINK_FMT,
				ffmt);
		break;
	case CSI2_ICI_PAD_SOURCE:{
			struct ici_framefmt *sink_ffmt =
				__ici_isys_subdev_get_ffmt(asd,
						CSI2_ICI_PAD_SINK);
			if (sink_ffmt) {
			    *cur_ffmt = *sink_ffmt;
			    cur_ffmt->pixelformat =
			        ici_isys_subdev_code_to_uncompressed
				(sink_ffmt->pixelformat);
			    *ffmt = *cur_ffmt;
			}
			break;
		}
	default:
		BUG_ON(1);
	}
}

static void ici_isys_csi2_error(struct ici_isys_csi2
						*csi2)
{
	/*
	 * Strings corresponding to CSI-2 receiver errors are here.
	 * Corresponding macros are defined in the header file.
	 */
	static const struct ici_isys_csi2_error {
		const char *error_string;
		bool is_info_only;
	} errors[] = {
		{
		"Single packet header error corrected", true}, {
		"Multiple packet header errors detected", true}, {
		"Payload checksum (CRC) error", true}, {
		"FIFO overflow", false}, {
		"Reserved short packet data type detected", true}, {
		"Reserved long packet data type detected", true}, {
		"Incomplete long packet detected", false}, {
		"Frame sync error", false}, {
		"Line sync error", false}, {
		"DPHY recoverable synchronization error", true}, {
		"DPHY non-recoverable synchronization error", false}, {
		"Escape mode error", true}, {
		"Escape mode trigger event", true}, {
		"Escape mode ultra-low power state for data lane(s)", true},
		{
		"Escape mode ultra-low power state exit for clock lane",
				true}, {
		"Inter-frame short packet discarded", true}, {
	"Inter-frame long packet discarded", true},};
	u32 status = csi2->receiver_errors;
	unsigned int i;

	csi2->receiver_errors = 0;

	for (i = 0; i < ARRAY_SIZE(errors); i++) {
		if (status & BIT(i)) {
			if (errors[i].is_info_only)
				dev_dbg(&csi2->isys->adev->dev,
					"csi2-%i info: %s\n",
					csi2->index, errors[i].error_string);
			else
				dev_err_ratelimited(&csi2->isys->adev->dev,
							"csi2-%i error: %s\n",
							csi2->index,
							errors[i].error_string);
		}
	}
}

#define DIV_SHIFT   8

static uint32_t calc_timing(int32_t a, int32_t b, int64_t link_freq,
			    int32_t accinv)
{
	return accinv * a + (accinv * b * (500000000 >> DIV_SHIFT)
			     / (int32_t) (link_freq >> DIV_SHIFT));
}

int ici_isys_csi2_calc_timing(struct ici_isys_csi2
						*csi2, struct
						ici_isys_csi2_timing
						*timing, uint32_t accinv)
{
	int64_t link_frequency = 0;

	int idx, rval;

	struct ici_ext_subdev *sd =
		(struct ici_ext_subdev*)csi2->ext_sd;

	struct ici_ext_sd_param param = {
		.sd = sd,
		.id = ICI_EXT_SD_PARAM_ID_LINK_FREQ,
		.type = ICI_EXT_SD_PARAM_TYPE_INT32,
	};

	if (!sd || !sd->get_param) {
		dev_err(&csi2->isys->adev->dev,
			"External device not available\n");
		return -ENODEV;
	}
	rval = sd->get_param(&param);
	if (rval) {
		dev_info(&csi2->isys->adev->dev, "can't get link frequency\n");
		return rval;
	}

	idx = param.val;
	param.type = ICI_EXT_SD_PARAM_TYPE_INT64;

	rval = sd->get_menu_item(&param, idx);
	if (rval) {
		dev_info(&csi2->isys->adev->dev, "can't get menu item\n");
		return rval;
	}

	link_frequency = param.val;
	dev_dbg(&csi2->isys->adev->dev, "%s: link frequency %lld\n", __func__,
		link_frequency);

	if (!link_frequency)
		return -EINVAL;

	timing->ctermen = calc_timing(CSI2_CSI_RX_DLY_CNT_TERMEN_CLANE_A,
					CSI2_CSI_RX_DLY_CNT_TERMEN_CLANE_B,
					link_frequency, accinv);
	timing->csettle =
		calc_timing(CSI2_CSI_RX_DLY_CNT_SETTLE_CLANE_A,
			CSI2_CSI_RX_DLY_CNT_SETTLE_CLANE_B, link_frequency,
			accinv);
	dev_dbg(&csi2->isys->adev->dev, "ctermen %u\n", timing->ctermen);
	dev_dbg(&csi2->isys->adev->dev, "csettle %u\n", timing->csettle);

	timing->dtermen = calc_timing(CSI2_CSI_RX_DLY_CNT_TERMEN_DLANE_A,
					CSI2_CSI_RX_DLY_CNT_TERMEN_DLANE_B,
					link_frequency, accinv);
	timing->dsettle =
	    calc_timing(CSI2_CSI_RX_DLY_CNT_SETTLE_DLANE_A,
			CSI2_CSI_RX_DLY_CNT_SETTLE_DLANE_B, link_frequency,
			accinv);
	dev_dbg(&csi2->isys->adev->dev, "dtermen %u\n", timing->dtermen);
	dev_dbg(&csi2->isys->adev->dev, "dsettle %u\n", timing->dsettle);

	return 0;
}

static void ici_isys_register_errors(struct
						 ici_isys_csi2
						 *csi2)
{
	u32 status = readl(csi2->base + CSI2_REG_CSIRX_IRQ_STATUS);

	dev_dbg(&csi2->isys->adev->dev,
		"ici_isys_register_errors\n");
	writel(status, csi2->base + CSI2_REG_CSIRX_IRQ_CLEAR);
	csi2->receiver_errors |= status;
}

static void ici_isys_csi2_sof_event(struct ici_isys_csi2
						*csi2, unsigned int vc)
{
	unsigned long flags;

	spin_lock_irqsave(&csi2->isys->lock, flags);
	csi2->in_frame = true;
	spin_unlock_irqrestore(&csi2->isys->lock, flags);
}

static void ici_isys_csi2_eof_event(struct ici_isys_csi2
						*csi2, unsigned int vc)
{
	unsigned long flags;

	spin_lock_irqsave(&csi2->isys->lock, flags);
	csi2->in_frame = false;
	if (csi2->wait_for_sync)
		complete(&csi2->eof_completion);
	spin_unlock_irqrestore(&csi2->isys->lock, flags);
}

void ici_isys_csi2_isr(struct ici_isys_csi2 *csi2)
{
	u32 status = readl(csi2->base + CSI2_REG_CSI2PART_IRQ_STATUS);
	unsigned int i;

	writel(status, csi2->base + CSI2_REG_CSI2PART_IRQ_CLEAR);

	if (status & CSI2_CSI2PART_IRQ_CSIRX)
		ici_isys_register_errors(csi2);

	for (i = 0; i < NR_OF_CSI2_ICI_VC; i++) {
		if ((status & CSI2_IRQ_FS_VC(i)))
			ici_isys_csi2_sof_event(csi2, i);

		if ((status & CSI2_IRQ_FE_VC(i)))
			ici_isys_csi2_eof_event(csi2, i);
	}

}
EXPORT_SYMBOL(ici_isys_csi2_isr);

void ici_isys_csi2_wait_last_eof(struct ici_isys_csi2
						*csi2)
{
	unsigned long flags;
	int tout;

	spin_lock_irqsave(&csi2->isys->lock, flags);
	if (!csi2->in_frame) {
		spin_unlock_irqrestore(&csi2->isys->lock, flags);
		return;
	}

	reinit_completion(&csi2->eof_completion);
	csi2->wait_for_sync = true;
	spin_unlock_irqrestore(&csi2->isys->lock, flags);
	tout = wait_for_completion_timeout(&csi2->eof_completion,
						ICI_EOF_TIMEOUT_JIFFIES);
	if (!tout) {
		dev_err(&csi2->isys->adev->dev,
			"csi2-%d: timeout at sync to eof\n", csi2->index);
	}
	csi2->wait_for_sync = false;
}

static void csi2_capture_done(struct ici_isys_pipeline *ip,
			      struct ia_css_isys_resp_info *info)
{
	ici_isys_frame_buf_capture_done(ip, info);
	if (ip->csi2)
		ici_isys_csi2_error(ip->csi2);
}

int ici_csi2_set_stream(
	struct ici_isys_node *node,
	void* ip,
	int state)
{
	struct ici_isys_subdev* asd = node->sd;
	struct ici_isys_csi2 *csi2 =
	    ici_asd_to_csi2(asd, asd->index);
	struct ici_isys_csi2_timing timing = { 0 };
	unsigned int i, nlanes;
	int rval;
	u32 csi2csirx = 0, csi2part = 0;

	dev_dbg(&csi2->isys->adev->dev, "csi2 s_stream %d\n", state);

	if (!state) {
		ici_isys_csi2_error(csi2);
		writel(0, csi2->base + CSI2_REG_CSI_RX_CONFIG);
		writel(0, csi2->base + CSI2_REG_CSI_RX_ENABLE);

		/* Disable interrupts */
		writel(0, csi2->base + CSI2_REG_CSI2S2M_IRQ_MASK);
		writel(0, csi2->base + CSI2_REG_CSI2S2M_IRQ_ENABLE);
		writel(0, csi2->base + CSI2_REG_CSI2PART_IRQ_MASK);
		writel(0, csi2->base + CSI2_REG_CSI2PART_IRQ_ENABLE);
		return 0;
	}

	ici_isys_stream_add_capture_done(ip, csi2_capture_done);

	nlanes = csi2->nlanes;

	rval = ici_isys_csi2_calc_timing(csi2,
							&timing,
							CSI2_ACCINV);
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
	writel(CSI2_CSI_RX_CONFIG_DISABLE_BYTE_CLK_GATING |
	CSI2_CSI_RX_CONFIG_RELEASE_LP11,
		csi2->base + CSI2_REG_CSI_RX_CONFIG);

	writel(nlanes, csi2->base + CSI2_REG_CSI_RX_NOF_ENABLED_LANES);

	writel(CSI2_CSI_RX_ENABLE_ENABLE, csi2->base + CSI2_REG_CSI_RX_ENABLE);

	/* SOF enabled from CSI2PART register in B0 */
	for (i = 0; i < NR_OF_CSI2_ICI_VC; i++)
		csi2part |= CSI2_IRQ_FS_VC(i) | CSI2_IRQ_FE_VC(i);

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

	return 0;
}

unsigned int ici_isys_csi2_get_current_field(
	struct device* dev,
	struct ici_isys_mipi_packet_header *ph)
{
	unsigned int field;

	/* Check if the first SOF packet is received. */
	if ((ph->dtype & ICI_ISYS_SHORT_PACKET_DTYPE_MASK) != 0)
		dev_warn(dev,
			"First short packet is not SOF.\n");
	field = (ph->word_count % 2) ? ICI_FIELD_TOP :
		ICI_FIELD_BOTTOM;
	dev_dbg(dev,
		"Interlaced field ready. frame_num = %d field = %d\n",
		ph->word_count, field);

	return field;
}

static int ici_csi2_pipeline_validate(
	struct node_pipeline *inp,
	struct ici_isys_node *node)
{
	struct ici_isys_subdev* asd = node->sd;
	struct ici_isys_csi2 *csi2 =
		ici_asd_to_csi2(asd, asd->index);
	struct ici_isys_pipeline *ip =
		ici_nodepipe_to_pipeline(inp);

	if (ip->csi2) {
		dev_err(&csi2->isys->adev->dev,
			"Pipeline does not support > 1 CSI2 node\n");
		return -EINVAL;
	}
	node->pipe = inp;
	ip->csi2 = csi2;
	ip->asd_source = asd;
	ip->vc = asd - csi2->asd; // index of asd element in csi2->asd array
	ip->asd_source_pad_id = CSI2_ICI_PAD_SINK;
	return 0;
}

int ici_isys_csi2_init(struct ici_isys_csi2 *csi2,
					struct ici_isys *isys,
					void __iomem *base, unsigned int index)
{
	struct ici_pad_framefmt fmt = {
		.pad.pad_idx = CSI2_ICI_PAD_SINK,
		.ffmt = {
				.width = 4096,
				.height = 3072,
			},
	};

	int rval;
	char name[ICI_MAX_NODE_NAME];
	unsigned int i;

	csi2->isys = isys;
	csi2->base = base;
	csi2->index = index;

	for(i=0; i<NR_OF_CSI2_VC; i++)
	{
		snprintf(name, sizeof(name),
			 IPU_ISYS_ENTITY_PREFIX " CSI-2 %u VC %u", index, i);

		csi2->asd[i].isys = isys;
		rval = ici_isys_subdev_init(&csi2->asd[i],
						name,
						NR_OF_CSI2_ICI_PADS,
						i);
		if (rval)
			goto fail;

		csi2->asd[i].pads[CSI2_ICI_PAD_SINK].flags = ICI_PAD_FLAGS_SINK;
        	csi2->asd[i].pads[CSI2_ICI_PAD_SOURCE].flags = ICI_PAD_FLAGS_SOURCE;

		csi2->asd[i].source = IA_CSS_ISYS_STREAM_SRC_CSI2_PORT0 + index;
		csi2->asd[i].supported_codes = ici_csi2_supported_codes;
		csi2->asd[i].set_ffmt_internal = ici_csi2_set_ffmt;
		csi2->asd[i].node.node_set_streaming =
			ici_csi2_set_stream;
		csi2->asd[i].node.node_pipeline_validate =
			ici_csi2_pipeline_validate;

		csi2->asd[i].node.node_set_pad_ffmt(&csi2->asd[i].node, &fmt);

		snprintf(csi2->as[i].node.name, sizeof(csi2->as[i].node.name),
			IPU_ISYS_ENTITY_PREFIX " CSI-2 %u VC %u capture", index, i);
		csi2->as[i].isys = isys;
		csi2->as[i].try_fmt_vid_mplane =
			ici_isys_video_try_fmt_vid_mplane_default;
		csi2->as[i].prepare_firmware_stream_cfg =
			ici_isys_prepare_firmware_stream_cfg_default;
		csi2->as[i].packed = true;
		csi2->as[i].buf_list.css_pin_type = IA_CSS_ISYS_PIN_TYPE_MIPI;
		csi2->as[i].pfmts = ici_isys_pfmts_packed;
		csi2->as[i].line_header_length =
			INTEL_IPU4_ISYS_CSI2_LONG_PACKET_HEADER_SIZE;
		csi2->as[i].line_footer_length =
			INTEL_IPU4_ISYS_CSI2_LONG_PACKET_FOOTER_SIZE;
		init_completion(&csi2->eof_completion);

		rval = ici_isys_stream_init(&csi2->as[i], &csi2->asd[i],
				&csi2->asd[i].node, CSI2_ICI_PAD_SOURCE,
				ICI_PAD_FLAGS_SINK);
		if (rval) {
			dev_err(&isys->adev->dev, "can't init stream node\n");
			goto fail;
		}
	}
	init_completion(&csi2->eof_completion);

	return 0;

fail:
	ici_isys_csi2_cleanup(csi2);

	return rval;
}
EXPORT_SYMBOL(ici_isys_csi2_init);

void ici_isys_csi2_cleanup(struct ici_isys_csi2 *csi2)
{
    ici_isys_subdev_cleanup(&csi2->asd[0]);
	ici_isys_stream_cleanup(&csi2->as[0]);
}
EXPORT_SYMBOL(ici_isys_csi2_cleanup);

#endif /* ICI_ENABLED */
