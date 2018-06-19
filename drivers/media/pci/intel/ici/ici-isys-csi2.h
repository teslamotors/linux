/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/*
 * Copyright (C) 2018 Intel Corporation
 */

#ifndef ICI_ISYS_CSI2_H
#define ICI_ISYS_CSI2_H

#include "ici-isys-frame-buf.h"
#include "ici-isys-subdev.h"
#include "ici-isys-stream.h"

struct ici_isys_csi2_pdata;

#define CSI2_ICI_PAD_SINK				0
#define CSI2_ICI_PAD_SOURCE			1
#define NR_OF_CSI2_ICI_PADS			2
#define NR_OF_CSI2_ICI_VC				4

#define ICI_ISYS_SHORT_PACKET_BUFFER_NUM	32
#define ICI_ISYS_SHORT_PACKET_WIDTH	32
#define ICI_ISYS_SHORT_PACKET_FRAME_PACKETS	2
#define ICI_ISYS_SHORT_PACKET_EXTRA_PACKETS	64
#define ICI_ISYS_SHORT_PACKET_UNITSIZE	8
#define ICI_ISYS_SHORT_PACKET_GENERAL_DT	0
#define ICI_ISYS_SHORT_PACKET_PT		0
#define ICI_ISYS_SHORT_PACKET_FT		0
#define ICI_ISYS_SHORT_PACKET_DTYPE_MASK	0x3f
#define ICI_ISYS_SHORT_PACKET_STRIDE \
	(ICI_ISYS_SHORT_PACKET_WIDTH * \
	ICI_ISYS_SHORT_PACKET_UNITSIZE)
#define ICI_ISYS_SHORT_PACKET_NUM(num_lines) \
	((num_lines) * 2 + ICI_ISYS_SHORT_PACKET_FRAME_PACKETS + \
	ICI_ISYS_SHORT_PACKET_EXTRA_PACKETS)
#define ICI_ISYS_SHORT_PACKET_PKT_LINES(num_lines) \
	DIV_ROUND_UP(ICI_ISYS_SHORT_PACKET_NUM(num_lines) * \
	ICI_ISYS_SHORT_PACKET_UNITSIZE, \
	ICI_ISYS_SHORT_PACKET_STRIDE)
#define ICI_ISYS_SHORT_PACKET_BUF_SIZE(num_lines) \
	(ICI_ISYS_SHORT_PACKET_WIDTH * \
	ICI_ISYS_SHORT_PACKET_PKT_LINES(num_lines) * \
	ICI_ISYS_SHORT_PACKET_UNITSIZE)
#define IPU_ISYS_SHORT_PACKET_TRACE_MSG_NUMBER	256
#define IPU_ISYS_SHORT_PACKET_TRACE_MSG_SIZE	16
#define IPU_ISYS_SHORT_PACKET_TRACE_BUFFER_SIZE \
	(IPU_ISYS_SHORT_PACKET_TRACE_MSG_NUMBER * \
	IPU_ISYS_SHORT_PACKET_TRACE_MSG_SIZE)
#define IPU_ISYS_SHORT_PACKET_TRACE_MAX_TIMESHIFT 100
#define IPU_ISYS_SHORT_PACKET_FROM_RECEIVER	0
#define IPU_ISYS_SHORT_PACKET_FROM_TUNIT		1

#define ICI_EOF_TIMEOUT 1000
#define ICI_EOF_TIMEOUT_JIFFIES msecs_to_jiffies(ICI_EOF_TIMEOUT)

#define IPU_ISYS_SHORT_PACKET_TRACE_MAX_TIMESHIFT 100
#define IPU_ISYS_SHORT_PACKET_TRACE_EVENT_MASK	0x2082
#define IPU_SKEW_CAL_LIMIT_HZ (1500000000ul / 2)

#define CSI2_CSI_RX_DLY_CNT_TERMEN_CLANE_A		0
#define CSI2_CSI_RX_DLY_CNT_TERMEN_CLANE_B		0
#define CSI2_CSI_RX_DLY_CNT_SETTLE_CLANE_A		95
#define CSI2_CSI_RX_DLY_CNT_SETTLE_CLANE_B		-8

#define CSI2_CSI_RX_DLY_CNT_TERMEN_DLANE_A		0
#define CSI2_CSI_RX_DLY_CNT_TERMEN_DLANE_B		0
#define CSI2_CSI_RX_DLY_CNT_SETTLE_DLANE_A		85
#define CSI2_CSI_RX_DLY_CNT_SETTLE_DLANE_B		-2

/*
 * struct ici_isys_csi2
 *
 */
struct ici_isys_csi2 {
	struct ici_isys_csi2_pdata *pdata;
	struct ici_isys *isys;
	struct ici_isys_subdev asd[NR_OF_CSI2_ICI_VC];
	struct ici_isys_stream as[NR_OF_CSI2_ICI_VC];
	void *ext_sd;

	void __iomem *base;
	u32 receiver_errors;
	unsigned int nlanes;
	unsigned int index;
	atomic_t sof_sequence;

	bool wait_for_sync;
	bool in_frame;
	struct completion eof_completion;
};

struct ici_isys_csi2_timing {
	uint32_t ctermen;
	uint32_t csettle;
	uint32_t dtermen;
	uint32_t dsettle;
};

/*
 * This structure defines the MIPI packet header output
 * from IPU4 MIPI receiver. Due to hardware conversion,
 * this structure is not the same as defined in CSI-2 spec.
 */
__packed struct ici_isys_mipi_packet_header {
	uint32_t word_count : 16,
		 dtype : 13,
		 sync : 2,
		 stype : 1;
	uint32_t sid : 4,
		 port_id : 4,
		 reserved : 23,
		 odd_even : 1;
};

/*
 * This structure defines the trace message content
 * for CSI2 receiver monitor messages.
 */
__packed struct ici_isys_csi2_monitor_message {
	uint64_t fe : 1,
		 fs : 1,
		 pe : 1,
		 ps : 1,
		 le : 1,
		 ls : 1,
		 reserved1 : 2,
		 sequence : 2,
		 reserved2 : 2,
		 flash_shutter : 4,
		 error_cause : 12,
		 fifo_overrun : 1,
		 crc_error : 2,
		 reserved3 : 1,
		 timestamp_l : 16,
		 port : 4,
		 vc : 2,
		 reserved4 : 2,
		 frame_sync : 4,
		 reserved5 : 4;
	uint64_t reserved6 : 3,
		 cmd : 2,
		 reserved7 : 1,
		 monitor_id : 7,
		 reserved8 : 1,
		 timestamp_h : 50;
};

int ici_isys_csi2_init(struct ici_isys_csi2 *csi2,
			struct ici_isys *isys,
			void __iomem *base, unsigned int index);
void ici_isys_csi2_cleanup(struct ici_isys_csi2 *csi2);
void ici_isys_csi2_wait_last_eof(struct ici_isys_csi2 *csi2);
void ici_isys_csi2_isr(struct ici_isys_csi2 *csi2);
unsigned int ici_isys_csi2_get_current_field(
	struct device* dev, struct ici_isys_mipi_packet_header *ph);
	
#endif /* ICI_ISYS_CSI2_H */
