// SPDX-License_Identifier: GPL-2.0
//  Copyright (C) 2018 Intel Corporation

#include "ipu.h"
#include "ipu-buttress.h"
#include "ipu-isys.h"
#include "ipu-isys-csi2.h"
#include "ipu-platform-isys-csi2-reg.h"
#include "ipu-platform-regs.h"
#include "ipu-trace.h"
#include "ipu-isys-csi2.h"

#define CSI2_UPDATE_TIME_TRY_NUM   3
#define CSI2_UPDATE_TIME_MAX_DIFF  20

static int ipu4p_csi2_ev_correction_params(struct ipu_isys_csi2
					   *csi2, unsigned int lanes)
{
	/*
	 * TBD: add implementation for ipu4p
	 * probably re-use ipu4 implementation
	 */
	return 0;
}

static void ipu4p_isys_register_errors(struct ipu_isys_csi2 *csi2)
{
	u32 status;
	unsigned int index;
	struct ipu_isys *isys = csi2->isys;
	void __iomem *isys_base = isys->pdata->base;

	index = csi2->index;
	status = readl(isys_base +
			   IPU_REG_ISYS_CSI_IRQ_CTRL0_BASE(index) + 0x8);
	writel(status, isys_base +
		   IPU_REG_ISYS_CSI_IRQ_CTRL0_BASE(index) + 0xc);

	status &= 0xffff;
	dev_dbg(&isys->adev->dev, "csi %d rxsync status 0x%x", index, status);
	csi2->receiver_errors |= status;
}

void ipu_isys_csi2_error(struct ipu_isys_csi2 *csi2)
{
	/*
	 * Strings corresponding to CSI-2 receiver errors are here.
	 * Corresponding macros are defined in the header file.
	 */
	static const struct ipu_isys_csi2_error {
		const char *error_string;
		bool is_info_only;
	} errors[] = {
		{"Single packet header error corrected", true},
		{"Multiple packet header errors detected", true},
		{"Payload checksum (CRC) error", true},
		{"FIFO overflow", false},
		{"Reserved short packet data type detected", true},
		{"Reserved long packet data type detected", true},
		{"Incomplete long packet detected", false},
		{"Frame sync error", false},
		{"Line sync error", false},
		{"DPHY recoverable synchronization error", true},
		{"DPHY non-recoverable synchronization error", false},
		{"Escape mode error", true},
		{"Escape mode trigger event", true},
		{"Escape mode ultra-low power state for data lane(s)", true},
		{"Escape mode ultra-low power state exit for clock lane", true},
		{"Inter-frame short packet discarded", true},
		{"Inter-frame long packet discarded", true},
	};
	u32 status;
	unsigned int i;

	/* Register errors once more in case of error interrupts are disabled */
	ipu4p_isys_register_errors(csi2);
	status = csi2->receiver_errors;
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

int ipu_isys_csi2_set_stream(struct v4l2_subdev *sd,
			     struct ipu_isys_csi2_timing timing,
			     unsigned int nlanes, int enable)
{
	struct ipu_isys_csi2 *csi2 = to_ipu_isys_csi2(sd);
	struct ipu_isys *isys = csi2->isys;
	void __iomem *isys_base = isys->pdata->base;
	unsigned int i;
	u32 val, csi2part = 0;

	dev_dbg(&csi2->isys->adev->dev, "csi2 s_stream %d\n", enable);
	if (!enable) {
		ipu_isys_csi2_error(csi2);

		val = readl(csi2->base + CSI2_REG_CSI_RX_CONFIG);
		val &= ~(CSI2_CSI_RX_CONFIG_DISABLE_BYTE_CLK_GATING |
			 CSI2_CSI_RX_CONFIG_RELEASE_LP11);
		writel(val, csi2->base + CSI2_REG_CSI_RX_CONFIG);

		writel(0, csi2->base + CSI2_REG_CSI_RX_ENABLE);

		writel(0, isys_base +
			   IPU_REG_ISYS_CSI_IRQ_CTRL_BASE(csi2->index) + 0x4);
		writel(0, isys_base +
			   IPU_REG_ISYS_CSI_IRQ_CTRL_BASE(csi2->index) +
			   0x10);
		writel
		    (0, isys_base +
		     IPU_REG_ISYS_CSI_IRQ_CTRL0_BASE(csi2->index) + 0x4);
		writel
		    (0, isys_base +
		     IPU_REG_ISYS_CSI_IRQ_CTRL0_BASE(csi2->index) + 0x10);
		return 0;
	}

	ipu4p_csi2_ev_correction_params(csi2, nlanes);

	writel(timing.ctermen,
		   csi2->base + CSI2_REG_CSI_RX_DLY_CNT_TERMEN_CLANE);
	writel(timing.csettle,
		   csi2->base + CSI2_REG_CSI_RX_DLY_CNT_SETTLE_CLANE);

	for (i = 0; i < nlanes; i++) {
		writel
		    (timing.dtermen,
		     csi2->base + CSI2_REG_CSI_RX_DLY_CNT_TERMEN_DLANE(i));
		writel
		    (timing.dsettle,
		     csi2->base + CSI2_REG_CSI_RX_DLY_CNT_SETTLE_DLANE(i));
	}

	val = readl(csi2->base + CSI2_REG_CSI_RX_CONFIG);
	val |= CSI2_CSI_RX_CONFIG_DISABLE_BYTE_CLK_GATING |
	    CSI2_CSI_RX_CONFIG_RELEASE_LP11;
	writel(val, csi2->base + CSI2_REG_CSI_RX_CONFIG);

	writel(nlanes, csi2->base + CSI2_REG_CSI_RX_NOF_ENABLED_LANES);
	writel(CSI2_CSI_RX_ENABLE_ENABLE,
		   csi2->base + CSI2_REG_CSI_RX_ENABLE);

	/* SOF of VC0-VC3 enabled from CSI2PART register in B0 */
	for (i = 0; i < NR_OF_CSI2_VC; i++)
		csi2part |= CSI2_IRQ_FS_VC(i) | CSI2_IRQ_FE_VC(i);

	/* Enable csi2 receiver error interrupts */
	writel(1, isys_base +
		   IPU_REG_ISYS_CSI_IRQ_CTRL_BASE(csi2->index));
	writel(0, isys_base +
		   IPU_REG_ISYS_CSI_IRQ_CTRL_BASE(csi2->index) + 0x14);
	writel(0xffffffff, isys_base +
		   IPU_REG_ISYS_CSI_IRQ_CTRL_BASE(csi2->index) + 0xc);
	writel(1, isys_base +
		   IPU_REG_ISYS_CSI_IRQ_CTRL_BASE(csi2->index) + 0x4);
	writel(1, isys_base +
		   IPU_REG_ISYS_CSI_IRQ_CTRL_BASE(csi2->index) + 0x10);

	csi2part |= 0xffff;
	writel(csi2part, isys_base +
		   IPU_REG_ISYS_CSI_IRQ_CTRL0_BASE(csi2->index));
	writel(0, isys_base +
		   IPU_REG_ISYS_CSI_IRQ_CTRL0_BASE(csi2->index) + 0x14);
	writel(0xffffffff, isys_base +
		   IPU_REG_ISYS_CSI_IRQ_CTRL0_BASE(csi2->index) + 0xc);
	writel(csi2part, isys_base +
		   IPU_REG_ISYS_CSI_IRQ_CTRL0_BASE(csi2->index) + 0x4);
	writel(csi2part, isys_base +
		   IPU_REG_ISYS_CSI_IRQ_CTRL0_BASE(csi2->index) + 0x10);

	return 0;
}

void ipu_isys_csi2_isr(struct ipu_isys_csi2 *csi2)
{
	u32 status = 0;
	unsigned int i, bus;
	struct ipu_isys *isys = csi2->isys;
	void __iomem *isys_base = isys->pdata->base;

	bus = csi2->index;
	/* handle ctrl and ctrl0 irq */
	status = readl(isys_base +
			   IPU_REG_ISYS_CSI_IRQ_CTRL_BASE(bus) + 0x8);
	writel(status, isys_base +
		   IPU_REG_ISYS_CSI_IRQ_CTRL_BASE(bus) + 0xc);
	dev_dbg(&isys->adev->dev, "csi %d irq_ctrl status 0x%x", bus, status);

	if (!(status & BIT(0)))
		return;

	status = readl(isys_base +
			   IPU_REG_ISYS_CSI_IRQ_CTRL0_BASE(bus) + 0x8);
	writel(status, isys_base +
		   IPU_REG_ISYS_CSI_IRQ_CTRL0_BASE(bus) + 0xc);
	dev_dbg(&isys->adev->dev, "csi %d irq_ctrl0 status 0x%x", bus, status);
	/* register the csi sync error */
	csi2->receiver_errors |= status & 0xffff;
	/* handle sof and eof event */
	for (i = 0; i < NR_OF_CSI2_VC; i++) {
		if (status & CSI2_IRQ_FS_VC(i))
			ipu_isys_csi2_sof_event(csi2, i);

		if (status & CSI2_IRQ_FE_VC(i))
			ipu_isys_csi2_eof_event(csi2, i);
	}
}

static u64 tunit_time_to_us(struct ipu_isys *isys, u64 time)
{
	struct ipu_bus_device *adev = to_ipu_bus_device(isys->adev->iommu);
	u64 isys_clk = IS_FREQ_SOURCE / adev->ctrl->divisor / 1000000;

	do_div(time, isys_clk);

	return time;
}

static u64 tsc_time_to_tunit_time(struct ipu_isys *isys,
				  u64 tsc_base, u64 tunit_base, u64 tsc_time)
{
	struct ipu_bus_device *adev = to_ipu_bus_device(isys->adev->iommu);
	u64 isys_clk = IS_FREQ_SOURCE / adev->ctrl->divisor / 100000;
	u64 tsc_clk = IPU_BUTTRESS_TSC_CLK / 100000;
	u64 tunit_time;

	tunit_time = (tsc_time - tsc_base) * isys_clk;
	do_div(tunit_time, tsc_clk);

	return tunit_time + tunit_base;
}

/* Extract the timestamp from trace message.
 * The timestamp in the traces message contains two parts.
 * The lower part contains bit0 ~ 15 of the total 64bit timestamp.
 * The higher part contains bit14 ~ 63 of the 64bit timestamp.
 * These two parts are sampled at different time.
 * Two overlaped bits are used to identify if there's roll overs
 * in the lower part during the two samples.
 * If the two overlapped bits do not match, a fix is needed to
 * handle the roll over.
 */
static u64 extract_time_from_short_packet_msg(struct
					      ipu_isys_csi2_monitor_message
					      *msg)
{
	u64 time_h = msg->timestamp_h << 14;
	u64 time_l = msg->timestamp_l;
	u64 time_h_ovl = time_h & 0xc000;
	u64 time_h_h = time_h & (~0xffff);

	/* Fix possible roll overs. */
	if (time_h_ovl >= (time_l & 0xc000))
		return time_h_h | time_l;
	else
		return (time_h_h - 0x10000) | time_l;
}

unsigned int ipu_isys_csi2_get_current_field(struct ipu_isys_pipeline *ip,
					     unsigned int *timestamp)
{
	struct ipu_isys_video *av = container_of(ip, struct ipu_isys_video, ip);
	struct ipu_isys *isys = av->isys;
	unsigned int field = V4L2_FIELD_TOP;

	/*
	 * Find the nearest message that has matched msg type,
	 * port id, virtual channel and packet type.
	 */
	unsigned int i = ip->short_packet_trace_index;
	bool msg_matched = false;
	unsigned int monitor_id;

	if (ip->csi2->index >= IPU_ISYS_MAX_CSI2_LEGACY_PORTS)
		monitor_id = TRACE_REG_CSI2_3PH_TM_MONITOR_ID;
	else
		monitor_id = TRACE_REG_CSI2_TM_MONITOR_ID;

	dma_sync_single_for_cpu(&isys->adev->dev,
				isys->short_packet_trace_buffer_dma_addr,
				IPU_ISYS_SHORT_PACKET_TRACE_BUFFER_SIZE,
				DMA_BIDIRECTIONAL);

	do {
		struct ipu_isys_csi2_monitor_message msg =
		    isys->short_packet_trace_buffer[i];
		u64 sof_time = tsc_time_to_tunit_time(isys,
						      isys->tsc_timer_base,
						      isys->tunit_timer_base,
						      (((u64) timestamp[1]) <<
						       32) | timestamp[0]);
		u64 trace_time = extract_time_from_short_packet_msg(&msg);
		u64 delta_time_us = tunit_time_to_us(isys,
						     (sof_time > trace_time) ?
						     sof_time - trace_time :
						     trace_time - sof_time);

		i = (i + 1) % IPU_ISYS_SHORT_PACKET_TRACE_MSG_NUMBER;

		if (msg.cmd == TRACE_REG_CMD_TYPE_D64MTS &&
		    msg.monitor_id == monitor_id &&
		    msg.fs == 1 &&
		    msg.port == ip->csi2->index &&
		    msg.vc == ip->vc &&
		    delta_time_us < IPU_ISYS_SHORT_PACKET_TRACE_MAX_TIMESHIFT) {
			field = (msg.sequence % 2) ?
			    V4L2_FIELD_TOP : V4L2_FIELD_BOTTOM;
			ip->short_packet_trace_index = i;
			msg_matched = true;
			dev_dbg(&isys->adev->dev,
				"Interlaced field ready. field = %d\n", field);
			break;
		}
	} while (i != ip->short_packet_trace_index);
	if (!msg_matched)
		/* We have walked through the whole buffer. */
		dev_dbg(&isys->adev->dev, "No matched trace message found.\n");

	return field;
}

bool ipu_isys_csi2_skew_cal_required(struct ipu_isys_csi2 *csi2)
{
	__s64 link_freq;
	int rval;

	if (!csi2)
		return false;

	/* Not yet ? */
	if (csi2->remote_streams != csi2->stream_count)
		return false;

	rval = ipu_isys_csi2_get_link_freq(csi2, &link_freq);
	if (rval)
		return false;

	if (link_freq <= IPU_SKEW_CAL_LIMIT_HZ)
		return false;

	return true;
}

int ipu_isys_csi2_set_skew_cal(struct ipu_isys_csi2 *csi2, int enable)
{
	u32 val;

	val = readl(csi2->base + CSI2_REG_CSI_RX_CONFIG);

	if (enable)
		val |= CSI2_CSI_RX_CONFIG_SKEWCAL_ENABLE;
	else
		val &= ~CSI2_CSI_RX_CONFIG_SKEWCAL_ENABLE;

	writel(val, csi2->base + CSI2_REG_CSI_RX_CONFIG);

	return 0;
}
