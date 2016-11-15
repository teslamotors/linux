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
#include <media/intel-ipu4-isys.h>
#include "intel-ipu4.h"
#include "intel-ipu4-buttress.h"
#include "intel-ipu4-isys.h"
#include "intel-ipu5-isys-csi2-reg.h"
#include "intel-ipu5-isys-csi2.h"

struct intel_ipu5_csi2_error {
	const char *error_string;
	bool is_info_only;
};

/*
* just for rx_a/b and cphy_rx_0/1 addr map
* dphy: dphy port? if true, FRAME_LONG_PACKET_DISCARDED in irq_ctrl2,
* and use it to choose vc field
* irq_error_mask: used when checking the error happening
* irq_num: numbers of valid irq
*/
struct intel_ipu5_csi_irq_info_map {
	bool dphy;
	u32 irq_error_mask;
	u32 irq_num;
	unsigned int irq_base;
	unsigned int irq_base_ctrl2;
	struct intel_ipu5_csi2_error *errors;
};

/*
 * Strings corresponding to CSI-2 receiver errors are here.
 * Corresponding macros are defined in the header file.
 */
static struct intel_ipu5_csi2_error dphy_rx_errors[] = {
	{ "Single packet header error corrected", true },
	{ "Multiple packet header errors detected", true },
	{ "Payload checksum (CRC) error", true },
	{ "FIFO overflow", false },
	{ "Reserved short packet data type detected", true },
	{ "Reserved long packet data type detected", true },
	{ "Incomplete long packet detected", false },
	{ "Frame sync error", false },
	{ "Line sync error", false },
	{ "DPHY recoverable synchronization error", true },
	{ "DPHY non-recoverable synchronization error", false },
	{ "Escape mode error", true },
	{ "Escape mode trigger event", true },
	{ "Escape mode ultra-low power state for data lane(s)", true },
	{ "Escape mode ultra-low power state exit for clock lane", true },
	{ "Inter-frame short packet discarded", true },
	{ "Inter-frame long packet discarded", true },
};

static struct intel_ipu5_csi2_error cphy_rx_errors[] = {
	{ "Packet header error corrected", true },
	{ "Payload checksum (CRC) error", true },
	{ "FIFO overflow", false },
	{ "Reserved short packet data type detected", true },
	{ "Reserved long packet data type detected", true },
	{ "Incomplete long packet detected", false },
	{ "Frame sync error", false },
	{ "Line sync error", false },
	{ "SOT missing synchronization0 error", true },
	{ "SOT missing synchronization1 error", false },
	{ NULL, false },
	{ NULL, false },
	{ NULL, false },
	{ NULL, false },
	{ NULL, false },
	{ NULL, false },
	{ NULL, false },
	{ NULL, false },
	{ NULL, false },
	{ NULL, false },
	{ NULL, false },
	{ NULL, false },
	{ NULL, false },
	{ NULL, false },
	{ NULL, false },
	{ NULL, false },
	{ "Inter-frame short packet discarded", true },
	{ "Inter-frame long packet discarded", true },
};

static struct intel_ipu5_csi_irq_info_map irq_info_map[] = {
	{
		.dphy = true,
		.irq_error_mask = CSI_RX_ERROR_IRQ_MASK,
		.irq_num = CSI_RX_NUM_IRQ,
		.irq_base = INTEL_IPU5_CSI_REG_IRQ_CTRL0_A_EDGE,
		.irq_base_ctrl2 = INTEL_IPU5_CSI_REG_IRQ_CTRL2_A_EDGE,
		.errors = dphy_rx_errors,
	},
	{
		.dphy = true,
		.irq_error_mask = CSI_RX_ERROR_IRQ_MASK,
		.irq_num = CSI_RX_NUM_IRQ,
		.irq_base = INTEL_IPU5_CSI_REG_IRQ_CTRL0_B_EDGE,
		.irq_base_ctrl2 = INTEL_IPU5_CSI_REG_IRQ_CTRL2_B_EDGE,
		.errors = dphy_rx_errors,
	},
	{
		.irq_error_mask = CSI_CPHY_RX_ERROR_IRQ_MASK,
		.irq_num = CSI_CPHY_RX_NUM_IRQ,
		.irq_base = INTEL_IPU5_CSI_REG_IRQ_CTRL1_A_EDGE,
		.errors = cphy_rx_errors,
	},
	{
		.irq_error_mask = CSI_CPHY_RX_ERROR_IRQ_MASK,
		.irq_num = CSI_CPHY_RX_NUM_IRQ,
		.irq_base = INTEL_IPU5_CSI_REG_IRQ_CTRL1_B_EDGE,
		.errors = cphy_rx_errors,
	},
	{
		.dphy = true,
		.irq_error_mask = CSI_RX_ERROR_IRQ_MASK,
		.irq_num = CSI_RX_NUM_IRQ,
		.irq_base = INTEL_IPU5_CSI_REG_IRQ_CTRL0_A_EDGE,
		.irq_base_ctrl2 = INTEL_IPU5_CSI_REG_IRQ_CTRL2_A_EDGE,
		.errors = dphy_rx_errors,
	},
	{
		.dphy = true,
		.irq_error_mask = CSI_RX_ERROR_IRQ_MASK,
		.irq_num = CSI_RX_NUM_IRQ,
		.irq_base = INTEL_IPU5_CSI_REG_IRQ_CTRL0_B_EDGE,
		.irq_base_ctrl2 = INTEL_IPU5_CSI_REG_IRQ_CTRL2_B_EDGE,
		.errors = dphy_rx_errors,
	},
	{
		.irq_error_mask = CSI_CPHY_RX_ERROR_IRQ_MASK,
		.irq_num = CSI_CPHY_RX_NUM_IRQ,
		.irq_base = INTEL_IPU5_CSI_REG_IRQ_CTRL1_A_EDGE,
		.errors = cphy_rx_errors,
	},
	{
		.irq_error_mask = CSI_CPHY_RX_ERROR_IRQ_MASK,
		.irq_num = CSI_CPHY_RX_NUM_IRQ,
		.irq_base = INTEL_IPU5_CSI_REG_IRQ_CTRL1_B_EDGE,
		.errors = cphy_rx_errors,
	},
	{
		.dphy = true,
		.irq_error_mask = CSI_RX_ERROR_IRQ_MASK,
		.irq_num = CSI_RX_NUM_IRQ,
		.irq_base = INTEL_IPU5_CSI_REG_IRQ_CTRL0_A_EDGE,
		.irq_base_ctrl2 = INTEL_IPU5_CSI_REG_IRQ_CTRL2_A_EDGE,
		.errors = dphy_rx_errors,
	},
	{
		.dphy = true,
		.irq_error_mask = CSI_RX_ERROR_IRQ_MASK,
		.irq_num = CSI_RX_NUM_IRQ,
		.irq_base = INTEL_IPU5_CSI_REG_IRQ_CTRL0_B_EDGE,
		.irq_base_ctrl2 = INTEL_IPU5_CSI_REG_IRQ_CTRL2_B_EDGE,
		.errors = dphy_rx_errors,
	},
	{
		.irq_error_mask = CSI_CPHY_RX_ERROR_IRQ_MASK,
		.irq_num = CSI_CPHY_RX_NUM_IRQ,
		.irq_base = INTEL_IPU5_CSI_REG_IRQ_CTRL1_A_EDGE,
		.errors = cphy_rx_errors,
	},
	{
		.irq_error_mask = CSI_CPHY_RX_ERROR_IRQ_MASK,
		.irq_num = CSI_CPHY_RX_NUM_IRQ,
		.irq_base = INTEL_IPU5_CSI_REG_IRQ_CTRL1_B_EDGE,
		.errors = cphy_rx_errors,
	},
};

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
	struct intel_ipu4_isys_pipeline *pipe =
		container_of(csi2->asd.sd.entity.pipe,
			     struct intel_ipu4_isys_pipeline, pipe);
	struct intel_ipu4_isys_csi2_config *cfg =
		v4l2_get_subdev_hostdata(
			media_entity_to_v4l2_subdev(pipe->external->entity));
	u32 irq;

	irq = readl(csi2->base + irq_info_map[cfg->port].irq_base +
		INTEL_IPU5_CSI_REG_IRQ_STATUS_OFFSET);

	if (irq & irq_info_map[cfg->port].irq_error_mask) {
		writel(irq & irq_info_map[cfg->port].irq_error_mask,
			csi2->base + irq_info_map[cfg->port].irq_base +
			INTEL_IPU5_CSI_REG_IRQ_CLEAR_OFFSET);
		csi2->receiver_errors |=
			irq & irq_info_map[cfg->port].irq_error_mask;
	}

	/*
	* special handler for dphy FRAME_LONG_PACKET_DISCARDED
	* because this error bit is located in ctrl2 register, not in
	* ctrl0 dphy irq register; when this error occur,
	* we put it to highest bit
	*/
	if (irq_info_map[cfg->port].dphy) {
		u32 irq_ctrl2;

		irq_ctrl2 = readl(csi2->base +
			irq_info_map[cfg->port].irq_base_ctrl2 +
			INTEL_IPU5_CSI_REG_IRQ_STATUS_OFFSET);
		if (irq_ctrl2 & CSI_RX_INTER_FRAME_LONG_PACKET_DISCARDED) {
			writel(irq_ctrl2 &
				CSI_RX_INTER_FRAME_LONG_PACKET_DISCARDED,
				csi2->base +
				irq_info_map[cfg->port].irq_base_ctrl2 +
				INTEL_IPU5_CSI_REG_IRQ_CLEAR_OFFSET);
			csi2->receiver_errors |=
				(irq_info_map[cfg->port].irq_error_mask + 1);
		}
	}
}

void intel_ipu5_isys_csi2_error(struct intel_ipu4_isys_csi2 *csi2)
{
	struct intel_ipu4_isys_pipeline *pipe =
		container_of(csi2->asd.sd.entity.pipe,
			     struct intel_ipu4_isys_pipeline, pipe);
	struct intel_ipu4_isys_csi2_config *cfg =
		v4l2_get_subdev_hostdata(
			media_entity_to_v4l2_subdev(pipe->external->entity));
	struct intel_ipu5_csi2_error *errors;
	u32 status;
	unsigned int i;

	/* Register errors once more in case of error interrupts are disabled */
	intel_ipu5_isys_register_errors(csi2);
	status = csi2->receiver_errors;
	csi2->receiver_errors = 0;
	errors = irq_info_map[cfg->port].errors;

	for (i = 0; i < irq_info_map[cfg->port].irq_num; i++) {
		if (status & BIT(i) & irq_info_map[cfg->port].irq_error_mask) {
			if (errors[i].is_info_only)
				dev_dbg(&csi2->isys->adev->dev,
					"csi2-%i info: %s\n",
					csi2->index, errors[i].error_string);
			else
				dev_err_ratelimited(&csi2->isys->adev->dev,
					"csi2-%i error: %s\n",
					csi2->index, errors[i].error_string);
		}
	}

	if (irq_info_map[cfg->port].dphy) {
		if (status & (irq_info_map[cfg->port].irq_error_mask + 1)) {
			if (errors[CSI_RX_NUM_ERRORS_IN_CTRL0].is_info_only)
				dev_dbg(&csi2->isys->adev->dev,
				"csi2-%i info: %s\n", csi2->index,
			errors[CSI_RX_NUM_ERRORS_IN_CTRL0].error_string);
			else
				dev_err_ratelimited(&csi2->isys->adev->dev,
				"csi2-%i error: %s\n", csi2->index,
			errors[CSI_RX_NUM_ERRORS_IN_CTRL0].error_string);
		}
	}
}

int intel_ipu5_isys_csi2_set_stream(struct v4l2_subdev *sd,
	struct intel_ipu4_isys_csi2_timing timing,
	unsigned int nlanes, int enable)
{
	struct intel_ipu4_isys_csi2 *csi2 = to_intel_ipu4_isys_csi2(sd);
	struct intel_ipu4_isys_pipeline *ip =
		container_of(sd->entity.pipe,
			     struct intel_ipu4_isys_pipeline, pipe);
	struct intel_ipu4_isys_csi2_config *cfg =
		v4l2_get_subdev_hostdata(
			media_entity_to_v4l2_subdev(ip->external->entity));
	unsigned int i, port;
	u32 val, csi_irqs;

	port = cfg->port;
	if (!enable) {
		intel_ipu5_isys_csi2_error(csi2);
		val = readl(csi2->base + INTEL_IPU5_CSI_REG_RX_CTL);
		val &= ~(INTEL_IPU5_CSI_REG_RX_CONFIG_RELEASE_LP11 |
			INTEL_IPU5_CSI_REG_RX_CONFIG_DISABLE_BYTE_CLK_GATING);
		writel(val, csi2->base + INTEL_IPU5_CSI_REG_RX_CTL);
		writel(0, csi2->base + INTEL_IPU5_CSI_REG_RX_ENABLE);

		/* Disable interrupts */
		writel(0, csi2->base + irq_info_map[port].irq_base +
			INTEL_IPU5_CSI_REG_IRQ_MASK_OFFSET);
		writel(0, csi2->base + irq_info_map[port].irq_base +
			INTEL_IPU5_CSI_REG_IRQ_ENABLE_OFFSET);
		if (irq_info_map[port].dphy) {
			writel(0,
				csi2->base + irq_info_map[port].irq_base_ctrl2 +
				INTEL_IPU5_CSI_REG_IRQ_MASK_OFFSET);
			writel(0,
				csi2->base + irq_info_map[port].irq_base_ctrl2 +
				INTEL_IPU5_CSI_REG_IRQ_ENABLE_OFFSET);
		}
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

	/* enable all related irq */
	csi_irqs = BIT(irq_info_map[port].irq_num) - 1;
	writel(csi_irqs, csi2->base + irq_info_map[port].irq_base);
	writel(csi_irqs, csi2->base + irq_info_map[port].irq_base +
		INTEL_IPU5_CSI_REG_IRQ_MASK_OFFSET);
	writel(csi_irqs, csi2->base + irq_info_map[port].irq_base +
		INTEL_IPU5_CSI_REG_IRQ_CLEAR_OFFSET);
	writel(csi_irqs, csi2->base + irq_info_map[port].irq_base +
		INTEL_IPU5_CSI_REG_IRQ_ENABLE_OFFSET);
	writel(csi_irqs, csi2->base + irq_info_map[port].irq_base +
		INTEL_IPU5_CSI_REG_IRQ_PULSE_OFFSET);
	/*
	* enable irq of FRAME_LONG_PACKET_DISCARDED when dphy,
	* because this error bit is located in ctrl2 register
	*/
	if (irq_info_map[port].dphy) {
		csi_irqs = readl(csi2->base +
			irq_info_map[port].irq_base_ctrl2 +
			INTEL_IPU5_CSI_REG_IRQ_ENABLE_OFFSET);
		csi_irqs |= CSI_RX_INTER_FRAME_LONG_PACKET_DISCARDED;
		writel(csi_irqs,
			csi2->base + irq_info_map[port].irq_base_ctrl2);
		writel(csi_irqs,
			csi2->base + irq_info_map[port].irq_base_ctrl2 +
			INTEL_IPU5_CSI_REG_IRQ_MASK_OFFSET);
		writel(csi_irqs,
			csi2->base + irq_info_map[port].irq_base_ctrl2 +
			INTEL_IPU5_CSI_REG_IRQ_CLEAR_OFFSET);
		writel(csi_irqs,
			csi2->base + irq_info_map[port].irq_base_ctrl2 +
			INTEL_IPU5_CSI_REG_IRQ_ENABLE_OFFSET);
		writel(csi_irqs,
			csi2->base + irq_info_map[port].irq_base_ctrl2 +
			INTEL_IPU5_CSI_REG_IRQ_PULSE_OFFSET);
	}

	return 0;
}

void intel_ipu5_isys_csi2_isr(struct intel_ipu4_isys_csi2 *csi2)
{
	struct intel_ipu4_isys_pipeline *pipe =
		container_of(csi2->asd.sd.entity.pipe,
			     struct intel_ipu4_isys_pipeline, pipe);
	struct intel_ipu4_isys_csi2_config *cfg =
		v4l2_get_subdev_hostdata(
			media_entity_to_v4l2_subdev(pipe->external->entity));

	u32 status;
	unsigned int i;

	intel_ipu5_isys_register_errors(csi2);

	status = readl(csi2->base + irq_info_map[cfg->port].irq_base +
			INTEL_IPU5_CSI_REG_IRQ_STATUS_OFFSET);

	writel(status, csi2->base + irq_info_map[cfg->port].irq_base +
		INTEL_IPU5_CSI_REG_IRQ_CLEAR_OFFSET);

	for (i = 0; i < NR_OF_CSI2_VC; i++) {
		if (irq_info_map[cfg->port].dphy) {
			if (status & INTEL_IPU5_CSI_RX_IRQ_FS_VC(i))
				intel_ipu_isys_csi2_sof_event(csi2, i);

			if (status & INTEL_IPU5_CSI_RX_IRQ_FE_VC(i))
				intel_ipu_isys_csi2_eof_event(csi2, i);
		} else {
			if (status & INTEL_IPU5_CSI_CPHY_RX_IRQ_FS_VC(i))
				intel_ipu_isys_csi2_sof_event(csi2, i);

			if (status & INTEL_IPU5_CSI_CPHY_RX_IRQ_FE_VC(i))
				intel_ipu_isys_csi2_eof_event(csi2, i);
		}
	}

	return;

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

