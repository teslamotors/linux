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

#ifndef INTEL_IPU4_ISYS_CSI2_BE_H
#define INTEL_IPU4_ISYS_CSI2_BE_H

#include <media/media-entity.h>
#include <media/v4l2-device.h>

#include "intel-ipu4-isys-queue.h"
#include "intel-ipu4-isys-subdev.h"
#include "intel-ipu4-isys-video.h"

struct intel_ipu4_isys_csi2_be_pdata;
struct intel_ipu4_isys;

#define CSI2_BE_PAD_SINK		0
#define CSI2_BE_PAD_SOURCE		1

#define NR_OF_CSI2_BE_PADS		2
#define NR_OF_CSI2_BE_SOURCE_PADS	1
#define NR_OF_CSI2_BE_SINK_PADS	1
#define NR_OF_CSI2_BE_STREAMS	1

#define NR_OF_CSI2_BE_SOC_SOURCE_PADS	8
#define NR_OF_CSI2_BE_SOC_SINK_PADS	8
#define NR_OF_CSI2_BE_SOC_PADS \
	(NR_OF_CSI2_BE_SOC_SOURCE_PADS + NR_OF_CSI2_BE_SOC_SINK_PADS)
#define NR_OF_CSI2_BE_SOC_STREAMS	8

#define CSI2_BE_SOC_PAD_SINK(n)		\
	((n) >= NR_OF_CSI2_BE_SOC_SINK_PADS ? (NR_OF_CSI2_BE_SOC_SINK_PADS) \
	 : (n))
#define CSI2_BE_SOC_PAD_SOURCE(n)	\
	((n) >= NR_OF_CSI2_BE_SOC_SOURCE_PADS ? \
		(NR_OF_CSI2_BE_SOC_PADS - 1) : \
		((n) + NR_OF_CSI2_BE_SOC_SINK_PADS))

#define CSI2_BE_CROP_HOR	(1 << 0)
#define CSI2_BE_CROP_VER	(1 << 1)
#define CSI2_BE_CROP_MASK	(CSI2_BE_CROP_VER | CSI2_BE_CROP_HOR)

/*
 * struct intel_ipu4_isys_csi2_be
 */
struct intel_ipu4_isys_csi2_be {
	struct intel_ipu4_isys_csi2_be_pdata *pdata;
	struct intel_ipu4_isys_subdev asd;
	struct intel_ipu4_isys_video av;
};

struct intel_ipu4_isys_csi2_be_soc {
	struct intel_ipu4_isys_csi2_be_pdata *pdata;
	struct intel_ipu4_isys_subdev asd;
	struct intel_ipu4_isys_video av[NR_OF_CSI2_BE_SOC_SOURCE_PADS];
};

#define to_intel_ipu4_isys_csi2_be(sd)	\
	container_of(to_intel_ipu4_isys_subdev(sd), \
	struct intel_ipu4_isys_csi2_be, asd)

#define to_intel_ipu4_isys_csi2_be_soc(sd)	\
	container_of(to_intel_ipu4_isys_subdev(sd), \
	struct intel_ipu4_isys_csi2_be_soc, asd)

int intel_ipu4_isys_csi2_be_init(
				struct intel_ipu4_isys_csi2_be *csi2_be,
				struct intel_ipu4_isys *isys);
int intel_ipu4_isys_csi2_be_soc_init(
				struct intel_ipu4_isys_csi2_be_soc *csi2_be_soc,
				struct intel_ipu4_isys *isys);
void intel_ipu4_isys_csi2_be_cleanup(struct intel_ipu4_isys_csi2_be *csi2_be);
void intel_ipu4_isys_csi2_be_soc_cleanup(
				struct intel_ipu4_isys_csi2_be_soc *csi2_be);
void intel_ipu4_isys_csi2_be_isr(struct intel_ipu4_isys_csi2_be *csi2_be);
void intel_ipu4_isys_csi2_be_soc_isr(
				struct intel_ipu4_isys_csi2_be_soc *csi2_be);

#endif /* INTEL_IPU4_ISYS_CSI2_BE_H */
