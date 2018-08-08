/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2014 - 2018 Intel Corporation */

#ifndef IPU_ISYS_ISA_H
#define IPU_ISYS_ISA_H

#include <media/media-entity.h>
#include <media/v4l2-device.h>

#include "ipu-isys-queue.h"
#include "ipu-isys-subdev.h"
#include "ipu-isys-video.h"

#define ISA_PAD_SINK			0
#define ISA_PAD_SOURCE			1
#define ISA_PAD_CONFIG			2
#define ISA_PAD_3A			3
#define ISA_PAD_SOURCE_SCALED		4

#define NR_OF_ISA_PADS			5
#define NR_OF_ISA_SINK_PADS		2
#define NR_OF_ISA_SOURCE_PADS		3
#define NR_OF_ISA_STREAMS		1

struct ipu_isys;
struct ia_css_process_group_light;

/*
 * struct ipu_isa_buffer
 *
 * @ivb: Base buffer type which provides inheritance of
 *       isys buffer and vb2 buffer.
 * @pgl: program group light DMA buffer
 * @pgl.pg: process group, copy of the buffer's plane 0
 *	    but not mapped to user space
 * @pgl.common_pg: A combined process group from both video buffers
 * @pgl.iova: IOVA of common_pg
 */
struct ipu_isys_isa_buffer {
	struct ipu_isys_video_buffer ivb;
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
 * struct ipu_isys_isa
 */
struct ipu_isys_isa {
	struct ipu_isys_subdev asd;
	struct ipu_isys_video av;
	struct ipu_isys_video av_config;
	struct ipu_isys_video av_3a;
	struct ipu_isys_video av_scaled;

	void __iomem *base;

	struct v4l2_ctrl *isa_en;

	struct vb2_buffer *next_param[ISA_PARAM_QUEUES]; /* config and 3a */
};

#define to_ipu_isys_isa(sd)					\
	container_of(to_ipu_isys_subdev(sd), \
	struct ipu_isys_isa, asd)

#define vb2_buffer_to_ipu_isys_isa_buffer(__vb)		\
	container_of(vb2_buffer_to_ipu_isys_video_buffer(__vb), \
		struct ipu_isys_isa_buffer, ivb)

int ipu_isys_isa_init(struct ipu_isys_isa *isa,
		      struct ipu_isys *isys, void __iomem *base);
void ipu_isys_isa_cleanup(struct ipu_isys_isa *isa);
void ipu_isys_isa_isr(struct ipu_isys_isa *isa);

#endif /* IPU_ISYS_ISA_H */
