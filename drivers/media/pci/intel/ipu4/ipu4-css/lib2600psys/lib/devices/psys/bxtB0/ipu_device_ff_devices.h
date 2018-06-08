/*
 * Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2018, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef __IPU_DEVICE_FF_DEVICES_H
#define __IPU_DEVICE_FF_DEVICES_H

enum ipu_device_ff_id {
	/* PSA fixed functions */
	IPU_DEVICE_FF_WBA_WBA           = 0,
	IPU_DEVICE_FF_RYNR_SPLITTER,
	IPU_DEVICE_FF_RYNR_COLLECTOR,
	IPU_DEVICE_FF_RYNR_BNLM,
	IPU_DEVICE_FF_RYNR_VCUD,
	IPU_DEVICE_FF_DEMOSAIC_DEMOSAIC,
	IPU_DEVICE_FF_ACM_CCM,
	IPU_DEVICE_FF_ACM_ACM,
	IPU_DEVICE_FF_GTC_CSC_CDS,
	IPU_DEVICE_FF_GTC_GTM,
	IPU_DEVICE_FF_YUV1_SPLITTER,
	IPU_DEVICE_FF_YUV1_IEFD,
	IPU_DEVICE_FF_YUV1_YDS,
	IPU_DEVICE_FF_YUV1_TCC,
	IPU_DEVICE_FF_DVS_YBIN,
	IPU_DEVICE_FF_DVS_DVS,
	IPU_DEVICE_FF_LACE_LACE,
	/* ISA fixed functions */
	IPU_DEVICE_FF_ICA_INL,
	IPU_DEVICE_FF_ICA_GBL,
	IPU_DEVICE_FF_ICA_PCLN,
	IPU_DEVICE_FF_LSC_LSC,
	IPU_DEVICE_FF_DPC_DPC,
	IPU_DEVICE_FF_IDS_SCALER,
	IPU_DEVICE_FF_AWB_AWRG,
	IPU_DEVICE_FF_AF_AF,
	IPU_DEVICE_FF_AE_WGHT_HIST,
	IPU_DEVICE_FF_AE_CCM,
	IPU_DEVICE_FF_NUM_FF
};

#define IPU_DEVICE_FF_NUM_PSA_FF (IPU_DEVICE_FF_LACE_LACE + 1)
#define IPU_DEVICE_FF_NUM_ISA_FF \
	(IPU_DEVICE_FF_NUM_FF - IPU_DEVICE_FF_NUM_PSA_FF)

#endif /* __IPU_DEVICE_FF_DEVICES_H */
