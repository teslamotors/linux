/*
 * Register definition file for Samsung MFC V12.x Interface (FIMV) driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *     http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _REGS_MFC_V12_H
#define _REGS_MFC_V12_H

#include <linux/sizes.h>
#include "regs-mfc-v10.h"

#define D_DECODED_FIRST_PLANE_CRC                0xF668
#define D_DECODED_SECOND_PLANE_CRC               0xF66C

/* MFCv12 Context buffer sizes */
#define MFC_CTX_BUF_SIZE_V12		(30 * SZ_1K)
#define MFC_H264_DEC_CTX_BUF_SIZE_V12   (2 * SZ_1M)
#define MFC_OTHER_DEC_CTX_BUF_SIZE_V12  (30 * SZ_1K)
#define MFC_H264_ENC_CTX_BUF_SIZE_V12   (100 * SZ_1K)
#define MFC_HEVC_ENC_CTX_BUF_SIZE_V12	(100 * SZ_1K)
#define MFC_OTHER_ENC_CTX_BUF_SIZE_V12  (30 * SZ_1K)

/* MFCv10 variant defines */
#define MAX_FW_SIZE_V12          (SZ_1M)
#define MAX_CPB_SIZE_V12         (3 * SZ_1M)
#define MFC_VERSION_V12          0xC0
#define MFC_NUM_PORTS_V12        1
#define MFC_SCRATCH_BUF_SIZE     (7 * SZ_1M)

#define MFC_FW_DMA_MASK		0x10000000
/*Limit as specified in the MFCv12 UM*/
#define MFC_DMA_MASK		0xC0000000

#endif /*_REGS_MFC_V12_H*/

