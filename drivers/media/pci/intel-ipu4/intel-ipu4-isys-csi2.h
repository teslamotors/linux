/*
 * Copyright (c) 2013--2015 Intel Corporation.
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

#ifndef INTEL_IPU4_ISYS_CSI2_H
#define INTEL_IPU4_ISYS_CSI2_H

#include <media/media-entity.h>
#include <media/v4l2-device.h>

#include "intel-ipu4-isys-queue.h"
#include "intel-ipu4-isys-subdev.h"
#include "intel-ipu4-isys-video.h"

struct intel_ipu4_isys_csi2_pdata;
struct intel_ipu4_isys;

#define CSI2_PAD_SINK			0
#define CSI2_PAD_SOURCE			1
#define CSI2_PAD_META                   2
#define NR_OF_CSI2_PADS			3

#define INTEL_IPU4_ISYS_CSI2_SENSOR_CFG_LANE_CLOCK	0
#define INTEL_IPU4_ISYS_CSI2_SENSOR_CFG_LANE_DATA(n)	((n) + 1)

/*
 * struct intel_ipu4_isys_csi2
 *
 * @nlanes: number of lanes in the receiver
 */
struct intel_ipu4_isys_csi2 {
	struct intel_ipu4_isys_csi2_pdata *pdata;
	struct intel_ipu4_isys *isys;
	struct intel_ipu4_isys_subdev asd;
	struct intel_ipu4_isys_video av;
	struct intel_ipu4_isys_video av_meta;

	void __iomem *base;
	u32 receiver_errors;
	unsigned int nlanes;
	unsigned int index;
	atomic_t sof_sequence;
};

struct intel_ipu4_isys_csi2_timing {
	uint32_t ctermen;
	uint32_t csettle;
	uint32_t dtermen;
	uint32_t dsettle;
};

#define to_intel_ipu4_isys_csi2(sd)					\
	container_of(to_intel_ipu4_isys_subdev(sd), struct intel_ipu4_isys_csi2, asd)

int intel_ipu4_isys_csi2_calc_timing(struct intel_ipu4_isys_csi2 *csi2,
				  struct intel_ipu4_isys_csi2_timing *timing,
				  uint32_t accinv);
int intel_ipu4_isys_csi2_init(struct intel_ipu4_isys_csi2 *csi2, struct intel_ipu4_isys *isys,
		      void __iomem *base, unsigned int index);
void intel_ipu4_isys_csi2_cleanup(struct intel_ipu4_isys_csi2 *csi2);
void intel_ipu4_isys_csi2_isr(struct intel_ipu4_isys_csi2 *csi2);

#endif /* INTEL_IPU4_ISYS_CSI2_H */
