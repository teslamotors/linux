/*
 * Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2016, Intel Corporation.
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

#ifndef _IPU_DEVICE_FF_DEVICES_H_
#define _IPU_DEVICE_FF_DEVICES_H_

enum ipu_device_ff_id {
	/* PSA fixed functions */
	IPU_DEVICE_FF_WBA_WBA = 0,
	IPU_DEVICE_FF_RYNR_SPLITTER,
	IPU_DEVICE_FF_RYNR_COLLECTOR,
	IPU_DEVICE_FF_RYNR_BNLM,
	IPU_DEVICE_FF_DEMOSAIC_DEMOSAIC,
	IPU_DEVICE_FF_ACM_CCM,            /* ACM is called VCA in HW */
	IPU_DEVICE_FF_ACM_ACM,            /* ACM is called VCA in HW */
	IPU_DEVICE_FF_VCA_VCR2,           /* This device is part of ACM */
	IPU_DEVICE_FF_GAMMASTAR,
	IPU_DEVICE_FF_GTC_CSC_CDS,
	IPU_DEVICE_FF_GTC_GTM,
	IPU_DEVICE_FF_YUV1_SPLITTER,
	IPU_DEVICE_FF_YUV1_IEFD,
	IPU_DEVICE_FF_DVS_YBIN,
	IPU_DEVICE_FF_DVS_DVS,
	IPU_DEVICE_FF_VCSC_VCSC,
	IPU_DEVICE_FF_GLTM,
	IPU_DEVICE_FF_XNR_VHF,
	IPU_DEVICE_FF_XNR_HF,
	IPU_DEVICE_FF_XNR_HF_SE,
	IPU_DEVICE_FF_XNR_MF,
	IPU_DEVICE_FF_XNR_MF_SE,
	IPU_DEVICE_FF_XNR_LF,
	IPU_DEVICE_FF_XNR_LF_SE,
	IPU_DEVICE_FF_XNR_LFE,
	IPU_DEVICE_FF_XNR_VLF,
	IPU_DEVICE_FF_XNR_VLF_SE,
	/* ISA fixed functions */
	IPU_DEVICE_FF_SIS_WB,
	IPU_DEVICE_FF_SIS_SIS,
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

#define IPU_DEVICE_FF_LAST_PSA_FF IPU_DEVICE_FF_XNR_VLF
#define IPU_DEVICE_FF_NUM_PSA_FF  (IPU_DEVICE_FF_LAST_PSA_FF + 1)
#define IPU_DEVICE_FF_NUM_ISA_FF \
	(IPU_DEVICE_FF_NUM_FF - IPU_DEVICE_FF_LAST_PSA_FF)

#endif /*  _IPU_DEVICE_FF_DEVICES_H_ */
