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

#ifndef INTEL_IPU4_ISYS_TPG_H
#define INTEL_IPU4_ISYS_TPG_H

#include <media/media-entity.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>

#include "intel-ipu4-isys-subdev.h"
#include "intel-ipu4-isys-video.h"
#include "intel-ipu4-isys-queue.h"

struct intel_ipu4_isys_tpg_pdata;
struct intel_ipu4_isys;

#define TPG_PAD_SOURCE			0
#define NR_OF_TPG_PADS			1
#define NR_OF_TPG_SOURCE_PADS		1
#define NR_OF_TPG_SINK_PADS		0
#define NR_OF_TPG_STREAMS		1

/*
 * struct intel_ipu4_isys_tpg
 *
 * @nlanes: number of lanes in the receiver
 */
struct intel_ipu4_isys_tpg {
	struct intel_ipu4_isys_tpg_pdata *pdata;
	struct intel_ipu4_isys *isys;
	struct intel_ipu4_isys_subdev asd;
	struct intel_ipu4_isys_video av;

	void __iomem *base;
	void __iomem *sel;
	unsigned int index;
	int streaming;

	struct v4l2_ctrl *hblank;
	struct v4l2_ctrl *vblank;
	struct v4l2_ctrl *llp;
	struct v4l2_ctrl *fll;
	struct v4l2_ctrl *pixel_rate;
};

#define to_intel_ipu4_isys_tpg(sd)		\
	container_of(to_intel_ipu4_isys_subdev(sd), \
	struct intel_ipu4_isys_tpg, asd)

int intel_ipu4_isys_tpg_init(struct intel_ipu4_isys_tpg *tpg,
					struct intel_ipu4_isys *isys,
					void __iomem *base, void __iomem *sel,
					unsigned int index);
void intel_ipu4_isys_tpg_cleanup(struct intel_ipu4_isys_tpg *tpg);
void intel_ipu4_isys_tpg_isr(struct intel_ipu4_isys_tpg *tpg);

#endif /* INTEL_IPU4_ISYS_TPG_H */
