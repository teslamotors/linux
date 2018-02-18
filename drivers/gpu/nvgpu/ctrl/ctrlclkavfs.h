/*
 * _NVRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2016 by NVIDIA Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to NVIDIA
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of NVIDIA Corporation is prohibited.
 *
 * _NVRM_COPYRIGHT_END_
 */

#ifndef _ctrlclkavfs_h_
#define _ctrlclkavfs_h_

#include "ctrlboardobj.h"
/*!
 * Valid global VIN ID values
 */
#define	CTRL_CLK_VIN_ID_SYS		0x00000000
#define	CTRL_CLK_VIN_ID_LTC		0x00000001
#define	CTRL_CLK_VIN_ID_XBAR	0x00000002
#define	CTRL_CLK_VIN_ID_GPC0	0x00000003
#define	CTRL_CLK_VIN_ID_GPC1	0x00000004
#define	CTRL_CLK_VIN_ID_GPC2	0x00000005
#define	CTRL_CLK_VIN_ID_GPC3	0x00000006
#define	CTRL_CLK_VIN_ID_GPC4	0x00000007
#define	CTRL_CLK_VIN_ID_GPC5	0x00000008
#define	CTRL_CLK_VIN_ID_GPCS	0x00000009
#define	CTRL_CLK_VIN_ID_SRAM	0x0000000A
#define	CTRL_CLK_VIN_ID_UNDEFINED	0x000000FF

#define	CTRL_CLK_VIN_TYPE_DISABLED 0x00000000

/*!
 * Mask of all GPC VIN IDs supported by RM
 */
#define	CTRL_CLK_VIN_MASK_UNICAST_GPC (BIT(CTRL_CLK_VIN_ID_GPC0) | \
			BIT(CTRL_CLK_VIN_ID_GPC1) | \
			BIT(CTRL_CLK_VIN_ID_GPC2) | \
			BIT(CTRL_CLK_VIN_ID_GPC3) | \
			BIT(CTRL_CLK_VIN_ID_GPC4) | \
			BIT(CTRL_CLK_VIN_ID_GPC5))
#define CTRL_CLK_LUT_NUM_ENTRIES 0x50
#define CTRL_CLK_VIN_STEP_SIZE_UV (10000)
#define CTRL_CLK_LUT_MIN_VOLTAGE_UV (450000)
#define CTRL_CLK_FLL_TYPE_DISABLED 0

#define    CTRL_CLK_FLL_ID_SYS            (0x00000000)
#define    CTRL_CLK_FLL_ID_LTC            (0x00000001)
#define    CTRL_CLK_FLL_ID_XBAR           (0x00000002)
#define    CTRL_CLK_FLL_ID_GPC0           (0x00000003)
#define    CTRL_CLK_FLL_ID_GPC1           (0x00000004)
#define    CTRL_CLK_FLL_ID_GPC2           (0x00000005)
#define    CTRL_CLK_FLL_ID_GPC3           (0x00000006)
#define    CTRL_CLK_FLL_ID_GPC4           (0x00000007)
#define    CTRL_CLK_FLL_ID_GPC5           (0x00000008)
#define    CTRL_CLK_FLL_ID_GPCS           (0x00000009)
#define    CTRL_CLK_FLL_ID_UNDEFINED      (0x000000FF)
#define    CTRL_CLK_FLL_MASK_UNDEFINED    (0x00000000)

/*!
 * Mask of all GPC FLL IDs supported by RM
 */
#define    CTRL_CLK_FLL_MASK_UNICAST_GPC    (BIT(CTRL_CLK_FLL_ID_GPC0) | \
					       BIT(CTRL_CLK_FLL_ID_GPC1) | \
					       BIT(CTRL_CLK_FLL_ID_GPC2) | \
					       BIT(CTRL_CLK_FLL_ID_GPC3) | \
					       BIT(CTRL_CLK_FLL_ID_GPC4) | \
					       BIT(CTRL_CLK_FLL_ID_GPC5))
/*!
 * Mask of all FLL IDs supported by RM
 */
#define    CTRL_CLK_FLL_ID_ALL_MASK         (BIT(CTRL_CLK_FLL_ID_SYS)  | \
					       BIT(CTRL_CLK_FLL_ID_LTC)  | \
					       BIT(CTRL_CLK_FLL_ID_XBAR) | \
					       BIT(CTRL_CLK_FLL_ID_GPC0) | \
					       BIT(CTRL_CLK_FLL_ID_GPC1) | \
					       BIT(CTRL_CLK_FLL_ID_GPC2) | \
					       BIT(CTRL_CLK_FLL_ID_GPC3) | \
					       BIT(CTRL_CLK_FLL_ID_GPC4) | \
					       BIT(CTRL_CLK_FLL_ID_GPC5) | \
					       BIT(CTRL_CLK_FLL_ID_GPCS))

#define CTRL_CLK_FLL_REGIME_ID_INVALID                     (0x00000000)
#define CTRL_CLK_FLL_REGIME_ID_FFR                         (0x00000001)
#define CTRL_CLK_FLL_REGIME_ID_FR                          (0x00000002)

#endif
