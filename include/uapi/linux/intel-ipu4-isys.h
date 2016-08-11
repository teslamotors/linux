/*
 * Copyright (c) 2016 Intel Corporation.
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

#ifndef UAPI_LINUX_INTEL_IPU4_ISYS_H
#define UAPI_LINUX_INTEL_IPU4_ISYS_H

#define V4L2_CID_INTEL_IPU4_BASE	(V4L2_CID_USER_BASE + 0x1080)

#define V4L2_CID_INTEL_IPU4_ISA_EN	(V4L2_CID_INTEL_IPU4_BASE + 1)
#define V4L2_CID_INTEL_IPU4_STORE_CSI2_HEADER	(V4L2_CID_INTEL_IPU4_BASE + 2)

#define V4L2_INTEL_IPU4_ISA_EN_BLC	(1 << 0)
#define V4L2_INTEL_IPU4_ISA_EN_LSC	(1 << 1)
#define V4L2_INTEL_IPU4_ISA_EN_DPC	(1 << 2)
#define V4L2_INTEL_IPU4_ISA_EN_SCALER	(1 << 3)
#define V4L2_INTEL_IPU4_ISA_EN_AWB	(1 << 4)
#define V4L2_INTEL_IPU4_ISA_EN_AF	(1 << 5)
#define V4L2_INTEL_IPU4_ISA_EN_AE	(1 << 6)
#define NR_OF_INTEL_IPU4_ISA_CFG	7

#define V4L2_FMT_INTEL_IPU4_ISA_CFG	v4l2_fourcc('i', 'p', '4', 'c')
#define V4L2_FMT_INTEL_IPU4_ISYS_META	v4l2_fourcc('i', 'p', '4', 'm')

#endif /* UAPI_LINUX_INTEL_IPU4_ISYS_H */
