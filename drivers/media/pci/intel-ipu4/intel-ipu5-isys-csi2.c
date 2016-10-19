/*
 * Copyright (c) 2013--2016 Intel Corporation.
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

#include "intel-ipu4.h"
#include "intel-ipu4-buttress.h"
#include "intel-ipu4-isys.h"
#include "intel-ipu5-isys-csi2-reg.h"
#include "intel-ipu5-isys-csi2.h"

struct intel_ipu_isys_csi2_ops csi2_funcs_ipu5 = {
	.set_stream = intel_ipu5_isys_csi2_set_stream,
	.csi2_isr = intel_ipu5_isys_csi2_isr,
	.csi2_error = intel_ipu5_isys_csi2_error,
	.get_current_field = intel_ipu5_isys_csi2_get_current_field,
	.skew_cal_required = intel_ipu5_skew_cal_required,
	.set_skew_cal = intel_ipu5_csi_set_skew_cal,
};

static void intel_ipu5_isys_register_errors(struct intel_ipu4_isys_csi2 *csi2)
{
	/*
	* TODO: IPU5 IRQ
	*/
}

void intel_ipu5_isys_csi2_error(struct intel_ipu4_isys_csi2 *csi2)
{
	/*
	* TODO: IPU5 IRQ
	*/
}

int intel_ipu5_isys_csi2_set_stream(struct v4l2_subdev *sd,
	struct intel_ipu4_isys_csi2_timing timing,
	unsigned int nlanes, int enable)
{
	struct intel_ipu4_isys_csi2 *csi2 = to_intel_ipu4_isys_csi2(sd);
	unsigned int i;
	u32 val;

	if (!enable) {
		intel_ipu5_isys_csi2_error(csi2);
		val = readl(csi2->base + INTEL_IPU5_CSI_REG_RX_CTL);
		val &= ~(INTEL_IPU5_CSI_REG_RX_CONFIG_RELEASE_LP11 |
			INTEL_IPU5_CSI_REG_RX_CONFIG_DISABLE_BYTE_CLK_GATING);
		writel(val, csi2->base + INTEL_IPU5_CSI_REG_RX_CTL);
		writel(0, csi2->base + INTEL_IPU5_CSI_REG_RX_ENABLE);

		/*
		* TODO: IPU5 IRQ
		* So far csi2 irq regs not define and not enable on ipu5
		*/
		return 0;
	}

	/* Do not configure timings on FPGA */
	if (csi2->isys->pdata->type !=
		INTEL_IPU4_ISYS_TYPE_INTEL_IPU4_FPGA) {
		writel(timing.ctermen,
			csi2->base +
			INTEL_IPU5_CSI_REG_RX_DLY_CNT_TERMEN_CLANE);
		writel(timing.csettle,
			csi2->base +
			INTEL_IPU5_CSI_REG_RX_DLY_CNT_SETTLE_CLANE);

		for (i = 0; i < nlanes; i++) {
			writel(timing.dtermen,
				csi2->base +
			INTEL_IPU5_CSI_REG_RX_DLY_CNT_TERMEN_DLANE(i));
			writel(timing.dsettle,
				csi2->base +
			INTEL_IPU5_CSI_REG_RX_DLY_CNT_SETTLE_DLANE(i));
		}
	}

	val = readl(csi2->base + INTEL_IPU5_CSI_REG_RX_CTL);
	val |= INTEL_IPU5_CSI_REG_RX_CONFIG_RELEASE_LP11 |
		INTEL_IPU5_CSI_REG_RX_CONFIG_DISABLE_BYTE_CLK_GATING;
	writel(val, csi2->base + INTEL_IPU5_CSI_REG_RX_CTL);

	writel(nlanes, csi2->base + INTEL_IPU5_CSI_REG_RX_NOF_ENABLED_LANES);
	writel(INTEL_IPU5_CSI_REG_RX_ENABLE_ENABLE,
		csi2->base + INTEL_IPU5_CSI_REG_RX_ENABLE);

	/*
	* TODO: IPU5 IRQ
	* So far csi2 irq regs not define and not enable on ipu5
	*/

	return 0;
}

void intel_ipu5_isys_csi2_isr(struct intel_ipu4_isys_csi2 *csi2)
{
	/*
	* TODO: IPU5 IRQ enable
	*/
}

unsigned int intel_ipu5_isys_csi2_get_current_field(
	struct intel_ipu4_isys_pipeline *ip,
	unsigned int *timestamp)
{
	struct intel_ipu4_isys_video *av =
		container_of(ip, struct intel_ipu4_isys_video, ip);
	struct intel_ipu4_isys *isys = av->isys;
	unsigned int field = V4L2_FIELD_TOP;

	struct intel_ipu4_isys_buffer *short_packet_ib =
		list_last_entry(&ip->short_packet_active,
		struct intel_ipu4_isys_buffer, head);
	struct intel_ipu4_isys_private_buffer *pb =
		intel_ipu4_isys_buffer_to_private_buffer(
		short_packet_ib);
	struct intel_ipu4_isys_mipi_packet_header *ph =
		(struct intel_ipu4_isys_mipi_packet_header *)
		pb->buffer;

	/* Check if the first SOF packet is received. */
	if ((ph->dtype & INTEL_IPU5_ISYS_SHORT_PACKET_DTYPE_MASK) != 0)
		dev_warn(&isys->adev->dev,
			"First short packet is not SOF.\n");
	field = (ph->word_count % 2) ?
		V4L2_FIELD_TOP : V4L2_FIELD_BOTTOM;
	dev_dbg(&isys->adev->dev,
		"Interlaced field ready. frame_num = %d field = %d\n",
		ph->word_count, field);

	return field;
}

bool intel_ipu5_skew_cal_required(struct intel_ipu4_isys_csi2 *csi2)
{
	/*
	* TODO for ipu5
	*/
	return false;
}

int intel_ipu5_csi_set_skew_cal(struct intel_ipu4_isys_csi2 *csi2, int enable)
{
	/*
	* TODO for ipu5
	*/
	return 0;
}

