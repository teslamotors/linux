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

#ifndef INTEL_IPU4_ISYS_CSI2_H
#define INTEL_IPU4_ISYS_CSI2_H

struct intel_ipu4_isys_csi2_timing;
struct intel_ipu4_isys_csi2;
struct intel_ipu4_isys_pipeline;
struct v4l2_subdev;

#define INTEL_IPU4_ISYS_SHORT_PACKET_TRACE_MAX_TIMESHIFT 100
#define INTEL_IPU4_ISYS_SHORT_PACKET_TRACE_EVENT_MASK	0x2082
#define INTEL_IPU4_SKEW_CAL_LIMIT_HZ (1500000000ul / 2)

extern struct intel_ipu_isys_csi2_ops csi2_funcs_ipu4;

int intel_ipu4_isys_csi2_set_stream(struct v4l2_subdev *sd,
	struct intel_ipu4_isys_csi2_timing timing,
	unsigned int nlanes, int enable);
void intel_ipu4_isys_csi2_isr(struct intel_ipu4_isys_csi2 *csi2);
void intel_ipu4_isys_csi2_error(struct intel_ipu4_isys_csi2 *csi2);
unsigned int intel_ipu4_isys_csi2_get_current_field(
	struct intel_ipu4_isys_pipeline *ip,
	unsigned int *timestamp);
bool intel_ipu4_skew_cal_required(struct intel_ipu4_isys_csi2 *csi2);
int intel_ipu4_csi_set_skew_cal(struct intel_ipu4_isys_csi2 *csi2, int enable);
void intel_ipu4_isys_csi2_trigger_error_all(struct intel_ipu4_isys *isys);

#endif /* INTEL_IPU4_ISYS_CSI2_H */
