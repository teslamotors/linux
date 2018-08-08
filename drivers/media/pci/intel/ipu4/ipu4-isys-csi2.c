// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2013 - 2018 Intel Corporation

#include "ipu.h"
#include "ipu-buttress.h"
#include "ipu-isys.h"
#include "ipu-isys-csi2.h"
#include "ipu-platform-isys-csi2-reg.h"
#include "ipu-platform-regs.h"
#include "ipu-trace.h"
#include "ipu-isys-csi2.h"

#define CSE_IPC_CMDPHYWRITEL	   35
#define CSE_IPC_CMDPHYWRITEH	   36
#define CSE_IPC_CMDLEGACYPHYWRITEL 39
#define CSE_IPC_CMDLEGACYPHYWRITEH 40

#define NBR_BULK_MSGS              30	/* Space reservation for IPC messages */

#define CSI2_UPDATE_TIME_TRY_NUM   3
#define CSI2_UPDATE_TIME_MAX_DIFF  20

static u32
build_cse_ipc_commands(struct ipu_ipc_buttress_bulk_msg *target,
		       u32 nbr_msgs, u32 opcodel, u32 reg, u32 data)
{
	struct ipu_ipc_buttress_bulk_msg *msgs = &target[nbr_msgs];
	u32 opcodeh = opcodel == CSE_IPC_CMDPHYWRITEL ?
	    CSE_IPC_CMDPHYWRITEH : CSE_IPC_CMDLEGACYPHYWRITEH;

	/*
	 * Writing of 32 bits consist of 2 16 bit IPC messages to CSE.
	 * Messages must be in low-high order and nothing else between
	 * them.
	 * Register is in bits 8..15 as index (register value divided by 4)
	 */
	msgs->cmd = opcodel | (reg << (8 - 2)) | ((data & 0xffff) << 16);
	msgs->expected_resp = opcodel;
	msgs->require_resp = true;
	msgs->cmd_size = 4;
	msgs++;

	msgs->cmd = opcodeh | (reg << (8 - 2)) | (data & 0xffff0000);
	msgs->expected_resp = opcodeh;
	msgs->require_resp = true;
	msgs->cmd_size = 4;

	nbr_msgs += 2;

	/* Hits only if code change introduces too many new IPC messages */
	WARN_ON(nbr_msgs > NBR_BULK_MSGS);

	return nbr_msgs;
}

static int csi2_ev_correction_params(struct ipu_isys_csi2 *csi2,
				     unsigned int lanes)
{
	struct ipu_device *isp = csi2->isys->adev->isp;
	struct ipu_ipc_buttress_bulk_msg *messages;
	const struct ipu_receiver_electrical_params *ev_params;
	const struct ipu_isys_internal_csi2_pdata *csi2_pdata;

	__s64 link_freq;
	unsigned int i;
	u32 val;
	u32 nbr_msgs = 0;
	int rval;
	bool conf_set0;
	bool conf_set1;
	bool conf_combined = false;

	csi2_pdata = &csi2->isys->pdata->ipdata->csi2;
	ev_params = csi2_pdata->evparams;
	if (!ev_params)
		return 0;

	if (csi2->isys->csi2_cse_ipc_not_supported)
		return 0;

	rval = ipu_isys_csi2_get_link_freq(csi2, &link_freq);
	if (rval)
		return rval;

	i = 0;
	while (ev_params[i].device) {
		if (ev_params[i].device == isp->pdev->device &&
		    ev_params[i].revision == isp->pdev->revision &&
		    ev_params[i].min_freq < link_freq &&
		    ev_params[i].max_freq >= link_freq)
			break;
		i++;
	}

	if (!ev_params[i].device) {
		dev_info(&csi2->isys->adev->dev,
			 "No rcomp value override for this HW revision\n");
		return 0;
	}

	messages = kcalloc(NBR_BULK_MSGS, sizeof(*messages), GFP_KERNEL);
	if (!messages)
		return -ENOMEM;

	conf_set0 = csi2_pdata->evsetmask0 & (1 << csi2->index);
	conf_set1 = csi2_pdata->evsetmask1 & (1 << csi2->index);
	if (csi2_pdata->evlanecombine[csi2->index]) {
		conf_combined =
		    lanes > csi2_pdata->evlanecombine[csi2->index] ? 1 : 0;
	}
	conf_set1 |= conf_combined;

	/*
	 * Note: There is no way to make R-M-W to these. Possible non-zero reset
	 * default is OR'd with the values
	 */
	val = 1 << CSI2_SB_CSI_RCOMP_CONTROL_LEGACY_OVR_ENABLE_PORT1_SHIFT |
	    1 << CSI2_SB_CSI_RCOMP_CONTROL_LEGACY_OVR_ENABLE_PORT2_SHIFT |
	    1 << CSI2_SB_CSI_RCOMP_CONTROL_LEGACY_OVR_ENABLE_PORT3_SHIFT |
	    1 << CSI2_SB_CSI_RCOMP_CONTROL_LEGACY_OVR_ENABLE_PORT4_SHIFT |
	    1 << CSI2_SB_CSI_RCOMP_CONTROL_LEGACY_OVR_ENABLE_SHIFT |
	    ev_params[i].rcomp_val_legacy <<
	    CSI2_SB_CSI_RCOMP_CONTROL_LEGACY_OVR_CODE_SHIFT;

	nbr_msgs = build_cse_ipc_commands(messages, nbr_msgs,
					  CSE_IPC_CMDLEGACYPHYWRITEL,
					  CSI2_SB_CSI_RCOMP_CONTROL_LEGACY,
					  val);

	val = 2 << CSI2_SB_CSI_RCOMP_UPDATE_MODE_SHIFT |
	    1 << CSI2_SB_CSI_RCOMP_OVR_ENABLE_SHIFT |
	    ev_params[i].rcomp_val_combo << CSI2_SB_CSI_RCOMP_OVR_CODE_SHIFT;

	nbr_msgs = build_cse_ipc_commands(messages, nbr_msgs,
					  CSE_IPC_CMDPHYWRITEL,
					  CSI2_SB_CSI_RCOMP_CONTROL_COMBO, val);

	if (conf_set0) {
		val = 0x380078 | ev_params[i].ports[0].ctle_val <<
		    CSI2_SB_CPHY0_RX_CONTROL1_EQ_LANE0_SHIFT;
		nbr_msgs = build_cse_ipc_commands(messages, nbr_msgs,
						  CSE_IPC_CMDPHYWRITEL,
						  CSI2_SB_CPHY0_RX_CONTROL1,
						  val);
		val = 0x10000;
		if (ev_params[i].ports[0].crc_val != IPU_EV_AUTO)
			val |= ev_params[i].ports[0].crc_val <<
			    CSI2_SB_CPHY0_DLL_OVRD_CRCDC_FSM_DLANE0_SHIFT |
			    CSI2_SB_CPHY0_DLL_OVRD_LDEN_CRCDC_FSM_DLANE0;

		nbr_msgs = build_cse_ipc_commands(messages, nbr_msgs,
						  CSE_IPC_CMDPHYWRITEL,
						  CSI2_SB_CPHY0_DLL_OVRD, val);
	}

	if (conf_set1) {
		val = 0x380078 | ev_params[i].ports[1].ctle_val <<
		    CSI2_SB_CPHY2_RX_CONTROL1_EQ_LANE1_SHIFT;
		nbr_msgs = build_cse_ipc_commands(messages, nbr_msgs,
						  CSE_IPC_CMDPHYWRITEL,
						  CSI2_SB_CPHY2_RX_CONTROL1,
						  val);

		val = 0x10000;
		if (ev_params[i].ports[1].crc_val != IPU_EV_AUTO)
			val |= ev_params[i].ports[1].crc_val <<
			    CSI2_SB_CPHY2_DLL_OVRD_CRCDC_FSM_DLANE1_SHIFT |
			    CSI2_SB_CPHY2_DLL_OVRD_LDEN_CRCDC_FSM_DLANE1;

		nbr_msgs = build_cse_ipc_commands(messages, nbr_msgs,
						  CSE_IPC_CMDPHYWRITEL,
						  CSI2_SB_CPHY2_DLL_OVRD, val);
	}

	mutex_lock(&csi2->isys->mutex);
	/* This register is shared between two receivers */
	val = csi2->isys->csi2_rx_ctrl_cached;
	if (conf_set0) {
		val &= ~CSI2_SB_DPHY0_RX_CNTRL_SKEWCAL_CR_SEL_DLANE01_MASK;
		if (ev_params[i].ports[0].drc_val != IPU_EV_AUTO)
			val |=
			    CSI2_SB_DPHY0_RX_CNTRL_SKEWCAL_CR_SEL_DLANE01_MASK;
	}

	if (conf_set1) {
		val &= ~CSI2_SB_DPHY0_RX_CNTRL_SKEWCAL_CR_SEL_DLANE23_MASK;
		if (ev_params[i].ports[1].drc_val != IPU_EV_AUTO)
			val |=
			    CSI2_SB_DPHY0_RX_CNTRL_SKEWCAL_CR_SEL_DLANE23_MASK;
	}
	csi2->isys->csi2_rx_ctrl_cached = val;

	nbr_msgs = build_cse_ipc_commands(messages, nbr_msgs,
					  CSE_IPC_CMDPHYWRITEL,
					  CSI2_SB_DPHY0_RX_CNTRL, val);
	mutex_unlock(&csi2->isys->mutex);

	if (conf_set0 && ev_params[i].ports[0].drc_val != IPU_EV_AUTO) {
		/* Write value with FSM disabled */
		val = (conf_combined ?
		       ev_params[i].ports[0].drc_val_combined :
		       ev_params[i].ports[0].drc_val) <<
		    CSI2_SB_DPHY0_DLL_OVRD_DRC_FSM_OVRD_SHIFT;

		nbr_msgs = build_cse_ipc_commands(messages, nbr_msgs,
						  CSE_IPC_CMDPHYWRITEL,
						  CSI2_SB_DPHY0_DLL_OVRD, val);

		/* Write value with FSM enabled */
		val |= 1 << CSI2_SB_DPHY1_DLL_OVRD_LDEN_DRC_FSM_SHIFT;
		nbr_msgs = build_cse_ipc_commands(messages, nbr_msgs,
						  CSE_IPC_CMDPHYWRITEL,
						  CSI2_SB_DPHY0_DLL_OVRD, val);
	} else if (conf_set0 && ev_params[i].ports[0].drc_val == IPU_EV_AUTO) {
		nbr_msgs = build_cse_ipc_commands(messages, nbr_msgs,
						  CSE_IPC_CMDPHYWRITEL,
						  CSI2_SB_DPHY0_DLL_OVRD, 0);
	}

	if (conf_set1 && ev_params[i].ports[1].drc_val != IPU_EV_AUTO) {
		val = (conf_combined ?
		       ev_params[i].ports[1].drc_val_combined :
		       ev_params[i].ports[1].drc_val) <<
		    CSI2_SB_DPHY0_DLL_OVRD_DRC_FSM_OVRD_SHIFT;
		nbr_msgs = build_cse_ipc_commands(messages, nbr_msgs,
						  CSE_IPC_CMDPHYWRITEL,
						  CSI2_SB_DPHY1_DLL_OVRD, val);

		val |= 1 << CSI2_SB_DPHY1_DLL_OVRD_LDEN_DRC_FSM_SHIFT;
		nbr_msgs = build_cse_ipc_commands(messages, nbr_msgs,
						  CSE_IPC_CMDPHYWRITEL,
						  CSI2_SB_DPHY1_DLL_OVRD, val);
	} else if (conf_set1 && ev_params[i].ports[1].drc_val == IPU_EV_AUTO) {
		nbr_msgs = build_cse_ipc_commands(messages, nbr_msgs,
						  CSE_IPC_CMDPHYWRITEL,
						  CSI2_SB_DPHY1_DLL_OVRD, 0);
	}

	rval = ipu_buttress_ipc_send_bulk(isp,
					  IPU_BUTTRESS_IPC_CSE,
					  messages, nbr_msgs);

	if (rval == -ENODEV)
		csi2->isys->csi2_cse_ipc_not_supported = true;

	kfree(messages);
	return 0;
}

static void ipu_isys_register_errors(struct ipu_isys_csi2 *csi2)
{
	u32 status = readl(csi2->base + CSI2_REG_CSIRX_IRQ_STATUS);

	writel(status, csi2->base + CSI2_REG_CSIRX_IRQ_CLEAR);
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
	ipu_isys_register_errors(csi2);
	status = csi2->receiver_errors;
	csi2->receiver_errors = 0;

	for (i = 0; i < ARRAY_SIZE(errors); i++) {
		if (!(status & BIT(i)))
			continue;

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

static u64 tunit_time_to_us(struct ipu_isys *isys, u64 time)
{
	struct ipu_bus_device *adev = to_ipu_bus_device(isys->adev->iommu);
	u64 isys_clk = IS_FREQ_SOURCE / adev->ctrl->divisor / 1000000;

	do_div(time, isys_clk);

	return time;
}

static int update_timer_base(struct ipu_isys *isys)
{
	int rval, i;
	u64 time;

	for (i = 0; i < CSI2_UPDATE_TIME_TRY_NUM; i++) {
		rval = ipu_trace_get_timer(&isys->adev->dev, &time);
		if (rval) {
			dev_err(&isys->adev->dev,
				"Failed to read Tunit timer.\n");
			return rval;
		}
		rval = ipu_buttress_tsc_read(isys->adev->isp,
					     &isys->tsc_timer_base);
		if (rval) {
			dev_err(&isys->adev->dev,
				"Failed to read TSC timer.\n");
			return rval;
		}
		rval = ipu_trace_get_timer(&isys->adev->dev,
					   &isys->tunit_timer_base);
		if (rval) {
			dev_err(&isys->adev->dev,
				"Failed to read Tunit timer.\n");
			return rval;
		}
		if (tunit_time_to_us(isys, isys->tunit_timer_base - time) <
		    CSI2_UPDATE_TIME_MAX_DIFF)
			return 0;
	}
	dev_dbg(&isys->adev->dev, "Timer base values may not be accurate.\n");
	return 0;
}

static int
ipu_isys_csi2_configure_tunit(struct ipu_isys_csi2 *csi2, bool enable)
{
	struct ipu_isys *isys = csi2->isys;
	void __iomem *isys_base = isys->pdata->base;
	void __iomem *tunit_base = isys_base + TRACE_REG_IS_TRACE_UNIT_BASE;
	int i, ret = 0;

	mutex_lock(&isys->short_packet_tracing_mutex);
	if (!enable) {
		isys->short_packet_tracing_count--;
		if (isys->short_packet_tracing_count == 0)
			writel(0, tunit_base + TRACE_REG_TUN_DDR_ENABLE);
		goto out_release_mutex;
	}

	isys->short_packet_tracing_count++;
	if (isys->short_packet_tracing_count > 1)
		goto out_release_mutex;

	memset(isys->short_packet_trace_buffer, 0,
	       IPU_ISYS_SHORT_PACKET_TRACE_BUFFER_SIZE);
	dma_sync_single_for_device(&isys->adev->dev,
				   isys->short_packet_trace_buffer_dma_addr,
				   IPU_ISYS_SHORT_PACKET_TRACE_BUFFER_SIZE,
				   DMA_BIDIRECTIONAL);

	/* ring buffer base */
	writel(isys->short_packet_trace_buffer_dma_addr,
		   tunit_base + TRACE_REG_TUN_DRAM_BASE_ADDR);

	/* ring buffer end */
	writel(isys->short_packet_trace_buffer_dma_addr +
		   IPU_ISYS_SHORT_PACKET_TRACE_BUFFER_SIZE -
		   IPU_ISYS_SHORT_PACKET_TRACE_MSG_SIZE,
		   tunit_base + TRACE_REG_TUN_DRAM_END_ADDR);

	/* Infobits for ddr trace */
	writel(IPU_INFO_REQUEST_DESTINATION_PRIMARY,
		   tunit_base + TRACE_REG_TUN_DDR_INFO_VAL);

	/* Remove reset from trace timers */
	writel(TRACE_REG_GPREG_TRACE_TIMER_RST_OFF,
		   isys_base + TRACE_REG_IS_GPREG_TRACE_TIMER_RST_N);

	/* Reset CSI2 monitors */
	writel(1, isys->pdata->base + TRACE_REG_CSI2_TM_BASE +
		   TRACE_REG_CSI2_TM_RESET_REG_IDX);
	writel(1, isys->pdata->base + TRACE_REG_CSI2_3PH_TM_BASE +
		   TRACE_REG_CSI2_TM_RESET_REG_IDX);

	/* Set trace address register. */
	writel(TRACE_REG_CSI2_TM_TRACE_ADDRESS_VAL,
		   isys->pdata->base + TRACE_REG_CSI2_TM_BASE +
		   TRACE_REG_CSI2_TM_TRACE_ADDRESS_REG_IDX);
	writel(TRACE_REG_CSI2_TM_TRACE_HEADER_VAL,
		   isys->pdata->base + TRACE_REG_CSI2_TM_BASE +
		   TRACE_REG_CSI2_TM_TRACE_HEADER_REG_IDX);
	writel(TRACE_REG_CSI2_3PH_TM_TRACE_ADDRESS_VAL,
		   isys->pdata->base + TRACE_REG_CSI2_3PH_TM_BASE +
		   TRACE_REG_CSI2_TM_TRACE_ADDRESS_REG_IDX);
	writel(TRACE_REG_CSI2_TM_TRACE_HEADER_VAL,
		   isys->pdata->base + TRACE_REG_CSI2_3PH_TM_BASE +
		   TRACE_REG_CSI2_TM_TRACE_HEADER_REG_IDX);

	/* Enable DDR trace. */
	writel(1, tunit_base + TRACE_REG_TUN_DDR_ENABLE);

	/* Enable trace for CSI2 port. */
	for (i = 0; i < IPU_ISYS_MAX_CSI2_LEGACY_PORTS +
	     IPU_ISYS_MAX_CSI2_COMBO_PORTS; i++) {
		void __iomem *event_mask_reg =
		    (i < IPU_ISYS_MAX_CSI2_LEGACY_PORTS) ?
		    isys->pdata->base + TRACE_REG_CSI2_TM_BASE +
		    TRACE_REG_CSI2_TM_TRACE_DDR_EN_REG_IDX_P(i) :
		    isys->pdata->base + TRACE_REG_CSI2_3PH_TM_BASE +
		    TRACE_REG_CSI2_3PH_TM_TRACE_DDR_EN_REG_IDX_P(i);

		writel(IPU_ISYS_SHORT_PACKET_TRACE_EVENT_MASK,
			   event_mask_reg);
	}

	/* Enable CSI2 receiver monitor */
	writel(1, isys->pdata->base + TRACE_REG_CSI2_TM_BASE +
		   TRACE_REG_CSI2_TM_OVERALL_ENABLE_REG_IDX);
	writel(1, isys->pdata->base + TRACE_REG_CSI2_3PH_TM_BASE +
		   TRACE_REG_CSI2_TM_OVERALL_ENABLE_REG_IDX);

	ret = update_timer_base(isys);

out_release_mutex:
	mutex_unlock(&isys->short_packet_tracing_mutex);

	return ret;
}

int ipu_isys_csi2_set_stream(struct v4l2_subdev *sd,
			     struct ipu_isys_csi2_timing timing,
			     unsigned int nlanes, int enable)
{
	struct ipu_isys_csi2 *csi2 = to_ipu_isys_csi2(sd);
	struct ipu_isys_pipeline *ip = container_of(sd->entity.pipe,
						    struct ipu_isys_pipeline,
						    pipe);
	unsigned int i;
	int rval;
	u32 val, csi2part = 0, csi2csirx;

	dev_dbg(&csi2->isys->adev->dev, "csi2 s_stream %d\n", enable);

	if (!enable) {
		ipu_isys_csi2_error(csi2);

		val = readl(csi2->base + CSI2_REG_CSI_RX_CONFIG);
		val &= ~(CSI2_CSI_RX_CONFIG_DISABLE_BYTE_CLK_GATING |
			 CSI2_CSI_RX_CONFIG_RELEASE_LP11);
		writel(val, csi2->base + CSI2_REG_CSI_RX_CONFIG);

		writel(0, csi2->base + CSI2_REG_CSI_RX_ENABLE);

		/* Disable interrupts */
		writel(0, csi2->base + CSI2_REG_CSI2S2M_IRQ_MASK);
		writel(0, csi2->base + CSI2_REG_CSI2S2M_IRQ_ENABLE);
		writel(0, csi2->base + CSI2_REG_CSI2PART_IRQ_MASK);
		writel(0, csi2->base + CSI2_REG_CSI2PART_IRQ_ENABLE);
		if (ip->interlaced)
			ipu_isys_csi2_configure_tunit(csi2, 0);
		return 0;
	}

	csi2_ev_correction_params(csi2, nlanes);

	writel(timing.ctermen,
		   csi2->base + CSI2_REG_CSI_RX_DLY_CNT_TERMEN_CLANE);
	writel(timing.csettle,
		   csi2->base + CSI2_REG_CSI_RX_DLY_CNT_SETTLE_CLANE);

	for (i = 0; i < nlanes; i++) {
		writel(timing.dtermen,
			   csi2->base +
			   CSI2_REG_CSI_RX_DLY_CNT_TERMEN_DLANE(i));
		writel(timing.dsettle,
			   csi2->base +
			   CSI2_REG_CSI_RX_DLY_CNT_SETTLE_DLANE(i));
	}

	val = readl(csi2->base + CSI2_REG_CSI_RX_CONFIG);
	val |= CSI2_CSI_RX_CONFIG_DISABLE_BYTE_CLK_GATING |
	    CSI2_CSI_RX_CONFIG_RELEASE_LP11;
	writel(val, csi2->base + CSI2_REG_CSI_RX_CONFIG);

	writel(nlanes, csi2->base + CSI2_REG_CSI_RX_NOF_ENABLED_LANES);
	writel(CSI2_CSI_RX_ENABLE_ENABLE,
		   csi2->base + CSI2_REG_CSI_RX_ENABLE);

	/* SOF/EOF of VC0-VC3 enabled from CSI2PART register in B0 */
	for (i = 0; i < NR_OF_CSI2_VC; i++)
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
	if (ip->interlaced) {
		writel(CSI2_RX_SYNC_COUNTER_EXTERNAL,
			   csi2->base + CSI2_REG_CSI_RX_SYNC_COUNTER_SEL);
		rval = ipu_isys_csi2_configure_tunit(csi2, 1);
		if (rval)
			return rval;
	}

	return 0;
}

void ipu_isys_csi2_isr(struct ipu_isys_csi2 *csi2)
{
	u32 status = readl(csi2->base + CSI2_REG_CSI2PART_IRQ_STATUS);
	unsigned int i;

	writel(status, csi2->base + CSI2_REG_CSI2PART_IRQ_CLEAR);

	if (status & CSI2_CSI2PART_IRQ_CSIRX)
		ipu_isys_register_errors(csi2);

	for (i = 0; i < NR_OF_CSI2_VC; i++) {
		if ((status & CSI2_IRQ_FS_VC(i)))
			ipu_isys_csi2_sof_event(csi2, i);

		if ((status & CSI2_IRQ_FE_VC(i)))
			ipu_isys_csi2_eof_event(csi2, i);
	}
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
static u64
extract_time_from_short_packet_msg(struct ipu_isys_csi2_monitor_message *msg)
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

unsigned int
ipu_isys_csi2_get_current_field(struct ipu_isys_pipeline *ip,
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
