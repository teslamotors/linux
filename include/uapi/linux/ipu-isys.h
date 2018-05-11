/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2016 - 2018 Intel Corporation */

#ifndef UAPI_LINUX_IPU_ISYS_H
#define UAPI_LINUX_IPU_ISYS_H

#define V4L2_CID_IPU_BASE	(V4L2_CID_USER_BASE + 0x1080)

#define V4L2_CID_IPU_ISA_EN	(V4L2_CID_IPU_BASE + 1)
#define V4L2_CID_IPU_STORE_CSI2_HEADER	(V4L2_CID_IPU_BASE + 2)

#define V4L2_IPU_ISA_EN_BLC	(1 << 0)
#define V4L2_IPU_ISA_EN_LSC	(1 << 1)
#define V4L2_IPU_ISA_EN_DPC	(1 << 2)
#define V4L2_IPU_ISA_EN_SCALER	(1 << 3)
#define V4L2_IPU_ISA_EN_AWB	(1 << 4)
#define V4L2_IPU_ISA_EN_AF	(1 << 5)
#define V4L2_IPU_ISA_EN_AE	(1 << 6)
#define NR_OF_IPU_ISA_CFG	7

#define V4L2_FMT_IPU_ISA_CFG	v4l2_fourcc('i', 'p', '4', 'c')
#define V4L2_FMT_IPU_ISYS_META	v4l2_fourcc('i', 'p', '4', 'm')

#endif /* UAPI_LINUX_IPU_ISYS_H */
