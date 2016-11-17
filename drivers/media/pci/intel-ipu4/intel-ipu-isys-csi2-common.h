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

#ifndef INTEL_IPU_ISYS_CSI2_COMMON_H
#define INTEL_IPU_ISYS_CSI2_COMMON_H

#include <media/media-entity.h>
#include <media/v4l2-device.h>

#include "intel-ipu4-isys-queue.h"
#include "intel-ipu4-isys-subdev.h"
#include "intel-ipu4-isys-video.h"

struct intel_ipu4_isys_csi2_timing;
struct intel_ipu4_isys_csi2_pdata;
struct intel_ipu4_isys;

#define CSI2_PAD_SINK			0
#define CSI2_PAD_SOURCE(n)	\
	((n) >= NR_OF_CSI2_SOURCE_PADS ? \
		(NR_OF_CSI2_PADS - 2) : \
		((n) + NR_OF_CSI2_SINK_PADS))

#define CSI2_PAD_META                   5

#define NR_OF_CSI2_PADS			6
#define NR_OF_CSI2_SINK_PADS		1
#define NR_OF_CSI2_SOURCE_PADS		(NR_OF_CSI2_PADS - 2)
#define NR_OF_CSI2_STREAMS		4
#define NR_OF_CSI2_VC			4

#define INTEL_IPU_ISYS_SHORT_PACKET_BUFFER_NUM	VIDEO_MAX_FRAME
#define INTEL_IPU_ISYS_SHORT_PACKET_WIDTH	32
#define INTEL_IPU_ISYS_SHORT_PACKET_FRAME_PACKETS	2
#define INTEL_IPU_ISYS_SHORT_PACKET_EXTRA_PACKETS	64
#define INTEL_IPU_ISYS_SHORT_PACKET_UNITSIZE	8
#define INTEL_IPU_ISYS_SHORT_PACKET_GENERAL_DT	0
#define INTEL_IPU_ISYS_SHORT_PACKET_PT		0
#define INTEL_IPU_ISYS_SHORT_PACKET_FT		0

#define INTEL_IPU_ISYS_SHORT_PACKET_STRIDE \
	(INTEL_IPU_ISYS_SHORT_PACKET_WIDTH * \
	INTEL_IPU_ISYS_SHORT_PACKET_UNITSIZE)
#define INTEL_IPU_ISYS_SHORT_PACKET_NUM(num_lines) \
	((num_lines) * 2 + INTEL_IPU_ISYS_SHORT_PACKET_FRAME_PACKETS + \
	INTEL_IPU_ISYS_SHORT_PACKET_EXTRA_PACKETS)
#define INTEL_IPU_ISYS_SHORT_PACKET_PKT_LINES(num_lines) \
	DIV_ROUND_UP(INTEL_IPU_ISYS_SHORT_PACKET_NUM(num_lines) * \
	INTEL_IPU_ISYS_SHORT_PACKET_UNITSIZE, \
	INTEL_IPU_ISYS_SHORT_PACKET_STRIDE)
#define INTEL_IPU_ISYS_SHORT_PACKET_BUF_SIZE(num_lines) \
	(INTEL_IPU_ISYS_SHORT_PACKET_WIDTH * \
	INTEL_IPU_ISYS_SHORT_PACKET_PKT_LINES(num_lines) * \
	INTEL_IPU_ISYS_SHORT_PACKET_UNITSIZE)

#define INTEL_IPU_ISYS_SHORT_PACKET_TRACE_MSG_NUMBER	256
#define INTEL_IPU_ISYS_SHORT_PACKET_TRACE_MSG_SIZE	16
#define INTEL_IPU_ISYS_SHORT_PACKET_TRACE_BUFFER_SIZE \
	(INTEL_IPU_ISYS_SHORT_PACKET_TRACE_MSG_NUMBER * \
	INTEL_IPU_ISYS_SHORT_PACKET_TRACE_MSG_SIZE)

#define INTEL_IPU_ISYS_SHORT_PACKET_FROM_RECEIVER	0
#define INTEL_IPU_ISYS_SHORT_PACKET_FROM_TUNIT		1

#define CSI2_CSI_RX_DLY_CNT_TERMEN_CLANE_A		0
#define CSI2_CSI_RX_DLY_CNT_TERMEN_CLANE_B		0
#define CSI2_CSI_RX_DLY_CNT_SETTLE_CLANE_A		95
#define CSI2_CSI_RX_DLY_CNT_SETTLE_CLANE_B		-8

#define CSI2_CSI_RX_DLY_CNT_TERMEN_DLANE_A		0
#define CSI2_CSI_RX_DLY_CNT_TERMEN_DLANE_B		0
#define CSI2_CSI_RX_DLY_CNT_SETTLE_DLANE_A		85
#define CSI2_CSI_RX_DLY_CNT_SETTLE_DLANE_B		-2

#define INTEL_IPU_EOF_TIMEOUT 300
#define INTEL_IPU_EOF_TIMEOUT_JIFFIES msecs_to_jiffies(INTEL_IPU_EOF_TIMEOUT)

struct intel_ipu_isys_csi2_ops {
	int (*set_stream)(struct v4l2_subdev  *sd,
		struct intel_ipu4_isys_csi2_timing timing,
		unsigned int nlanes, int enable);
	void (*csi2_isr)(struct intel_ipu4_isys_csi2 *csi2);
	void (*csi2_error)(struct intel_ipu4_isys_csi2 *csi2);
	unsigned int (*get_current_field)(struct intel_ipu4_isys_pipeline *ip,
		unsigned int *timestamp);
	bool (*skew_cal_required)(struct intel_ipu4_isys_csi2 *csi2);
	int (*set_skew_cal)(struct intel_ipu4_isys_csi2 *csi2, int enable);
};

/*
 * struct intel_ipu4_isys_csi2
 *
 * @nlanes: number of lanes in the receiver
 */
struct intel_ipu4_isys_csi2 {
	struct intel_ipu4_isys_csi2_pdata *pdata;
	struct intel_ipu4_isys *isys;
	struct intel_ipu4_isys_subdev asd;
	struct intel_ipu4_isys_video av[NR_OF_CSI2_SOURCE_PADS];
	struct intel_ipu4_isys_video av_meta;
	struct completion eof_completion;

	void __iomem *base;
	u32 receiver_errors;
	unsigned int nlanes;
	unsigned int index;
	atomic_t sof_sequence;
	bool in_frame[NR_OF_CSI2_VC];
	bool wait_for_sync[NR_OF_CSI2_VC];

	unsigned int remote_streams;
	unsigned int stream_count;

	struct v4l2_ctrl *store_csi2_header;
	struct intel_ipu_isys_csi2_ops *csi2_ops;
};

struct intel_ipu4_isys_csi2_timing {
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
__packed struct intel_ipu4_isys_mipi_packet_header {
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
__packed struct intel_ipu4_isys_csi2_monitor_message {
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

#define to_intel_ipu4_isys_csi2(sd)					\
	container_of(to_intel_ipu4_isys_subdev(sd), \
	struct intel_ipu4_isys_csi2, asd)

int intel_ipu_isys_csi2_get_link_freq(struct intel_ipu4_isys_csi2 *csi2,
		      __s64 *link_freq);
int intel_ipu_isys_csi2_init(struct intel_ipu4_isys_csi2 *csi2,
	struct intel_ipu4_isys *isys,
	void __iomem *base, unsigned int index);
void intel_ipu_isys_csi2_cleanup(struct intel_ipu4_isys_csi2 *csi2);
void intel_ipu_isys_csi2_isr(struct intel_ipu4_isys_csi2 *csi2);
struct intel_ipu4_isys_buffer *intel_ipu_isys_csi2_get_short_packet_buffer(
	struct intel_ipu4_isys_pipeline *ip);
unsigned int intel_ipu_isys_csi2_get_current_field(
	struct intel_ipu4_isys_pipeline *ip,
	struct ipu_fw_isys_resp_info_abi *info);
void intel_ipu_isys_csi2_sof_event(struct intel_ipu4_isys_csi2 *csi2,
					   unsigned int vc);
void intel_ipu_isys_csi2_eof_event(struct intel_ipu4_isys_csi2 *csi2,
					   unsigned int vc);
void intel_ipu_isys_csi2_wait_last_eof(struct intel_ipu4_isys_csi2 *csi2);
bool intel_ipu_skew_cal_required(struct intel_ipu4_isys_csi2 *csi2);
int intel_ipu_csi_set_skew_cal(struct intel_ipu4_isys_csi2 *csi2, int enable);

#endif /* INTEL_IPU_ISYS_CSI2_COMMON_H */

