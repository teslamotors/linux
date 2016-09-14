/*
 * Copyright (c) 2014--2016 Intel Corporation.
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

#ifndef INTEL_IPU4_ISYS_ISA_H
#define INTEL_IPU4_ISYS_ISA_H

#include <media/media-entity.h>
#include <media/v4l2-device.h>

#include <uapi/linux/intel-ipu4-isys.h>

#include "intel-ipu4-isys-queue.h"
#include "intel-ipu4-isys-subdev.h"
#include "intel-ipu4-isys-video.h"

struct intel_ipu4_isys;
struct ipu_fw_isys_frame_buff_set_abi;
struct ipu_fw_isys_stream_cfg_data;

#define ISA_PAD_SINK			0
#define ISA_PAD_SOURCE			1
#define ISA_PAD_CONFIG			2
#define ISA_PAD_3A			3
#define ISA_PAD_SOURCE_SCALED		4

#define NR_OF_ISA_PADS			5
#define NR_OF_ISA_SINK_PADS		2
#define NR_OF_ISA_SOURCE_PADS		3
#define NR_OF_ISA_STREAMS		1

struct ia_css_process_group_light;
struct ia_css_terminal;

/*
 * struct intel_ipu4_isa_buffer
 *
 * @ivb: Base buffer type which provides inheritance of
 *       isys buffer and vb2 buffer.
 * @pgl: program group light DMA buffer
 * @pgl.pg: process group, copy of the buffer's plane 0
 *	    but not mapped to user space
 * @pgl.common_pg: A combined process group from both video buffers
 * @pgl.iova: IOVA of common_pg
 */
struct intel_ipu4_isys_isa_buffer {
	struct intel_ipu4_isys_video_buffer ivb;
	struct {
		struct ia_css_process_group_light *pg;
		struct ia_css_process_group_light *common_pg;
		dma_addr_t iova;
	} pgl;
};

/* ISA CFG will use multiplanar buffers */
#define ISA_CFG_BUF_PLANE_PG		0
#define ISA_CFG_BUF_PLANE_DATA		1
#define ISA_CFG_BUF_PLANES		2

#define ISA_PARAM_QUEUES		2

/*
 * struct intel_ipu4_isys_isa
 */
struct intel_ipu4_isys_isa {
	struct intel_ipu4_isys_subdev asd;
	struct intel_ipu4_isys_video av;
	struct intel_ipu4_isys_video av_config;
	struct intel_ipu4_isys_video av_3a;
	struct intel_ipu4_isys_video av_scaled;

	void __iomem *base;

	struct v4l2_ctrl *isa_en;

	struct vb2_buffer *next_param[ISA_PARAM_QUEUES]; /* config and 3a */
};

#define to_intel_ipu4_isys_isa(sd)					\
	container_of(to_intel_ipu4_isys_subdev(sd), \
	struct intel_ipu4_isys_isa, asd)

#define vb2_buffer_to_intel_ipu4_isys_isa_buffer(__vb)		\
	container_of(vb2_buffer_to_intel_ipu4_isys_video_buffer(__vb), \
		struct intel_ipu4_isys_isa_buffer, ivb)

int intel_ipu4_isys_isa_init(struct intel_ipu4_isys_isa *isa,
			struct intel_ipu4_isys *isys, void __iomem *base);
void intel_ipu4_isys_isa_cleanup(struct intel_ipu4_isys_isa *isa);
void intel_ipu4_isys_isa_isr(struct intel_ipu4_isys_isa *isa);

#endif /* INTEL_IPU4_ISYS_ISA_H */
