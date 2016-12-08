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

#ifndef UAPI_LINUX_CRLMODULE_H
#define UAPI_LINUX_CRLMODULE_H

#define V4L2_CID_CRLMODULE_BASE		(V4L2_CID_USER_BASE + 0x2050)

#define V4L2_CID_FRAME_LENGTH_LINES (V4L2_CID_CRLMODULE_BASE + 1)
#define V4L2_CID_LINE_LENGTH_PIXELS (V4L2_CID_CRLMODULE_BASE + 2)
#define CRL_CID_SENSOR_THERMAL_DATA (V4L2_CID_CRLMODULE_BASE + 3)

/*
 * Select sensor mode directly, driver programs media pad
 * formats as in configuration file
 */
#define CRL_CID_SENSOR_MODE (V4L2_CID_CRLMODULE_BASE + 4)

/* IMX230 HDR specific controls */
#define CRL_CID_IMX230_HDR_MODE		(V4L2_CID_CRLMODULE_BASE + 5)
#define CRL_CID_IMX230_HDR_ET_RATIO	(V4L2_CID_CRLMODULE_BASE + 6)

/* Set multi-exposure frame in HDR with different exposure value */
#define CRL_CID_EXPOSURE_SHS1		(V4L2_CID_CRLMODULE_BASE + 8)
#define CRL_CID_EXPOSURE_SHS2		(V4L2_CID_CRLMODULE_BASE + 9)
#define CRL_CID_EXPOSURE_SHS3		(V4L2_CID_CRLMODULE_BASE + 10)
#define CRL_CID_EXPOSURE_RHS1		(V4L2_CID_CRLMODULE_BASE + 11)
#define CRL_CID_EXPOSURE_RHS2		(V4L2_CID_CRLMODULE_BASE + 12)

#endif /* UAPI_LINUX_CRLMODULE_H */
