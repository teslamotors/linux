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
					/* Names (shortened) as used in */
	/* PSA fixed functions */	/* ipu_device_ff_hrt.txt        */
	IPU_DEVICE_FF_WBA_WBA = 0,	/* WBA_WBA */
	IPU_DEVICE_FF_RYNR_SPLITTER,	/* RYNR_RYNR_SPLITTER */
	IPU_DEVICE_FF_RYNR_COLLECTOR,	/* RYNR_RYNR_COLLECTOR */
	IPU_DEVICE_FF_RYNR_BNLM,	/* RYNR_BNLM */
	IPU_DEVICE_FF_RYNR_VCUD,	/* RYNR_VCUD */
	IPU_DEVICE_FF_DEMOSAIC_DEMOSAIC,/* DEMOSAIC_DEMOSAIC */
	IPU_DEVICE_FF_ACM_CCM,		/* VCA_VCR, name as used in CNLB0 HW */
	IPU_DEVICE_FF_ACM_ACM,		/* VCA_ACM, name as used in CNLB0 HW */
	IPU_DEVICE_FF_VCA_VCR2,		/* VCA_VCR, part of ACM */
	IPU_DEVICE_FF_GTC_CSC_CDS,	/* GTC_CSC_CDS */
	IPU_DEVICE_FF_GTC_GTM,		/* GTC_GTM */
	IPU_DEVICE_FF_YUV1_SPLITTER,	/* YUV1_Processing_YUV_SPLITTER */
	IPU_DEVICE_FF_YUV1_IEFD,	/* YUV1_Processing_IEFD*/
	IPU_DEVICE_FF_YUV1_YDS,		/* YUV1_Processing_YDS */
	IPU_DEVICE_FF_YUV1_TCC,		/* YUV1_Processing_TCC */
	IPU_DEVICE_FF_DVS_YBIN,		/* DVS_YBIN */
	IPU_DEVICE_FF_DVS_DVS,		/* DVS_DVS */
	IPU_DEVICE_FF_LACE_LACE,	/* Lace_Stat_LACE_STAT */
	/* ISA fixed functions */
	IPU_DEVICE_FF_ICA_INL,		/* Input_Corr_INL */
	IPU_DEVICE_FF_ICA_GBL,		/* Input_Corr_GBL */
	IPU_DEVICE_FF_ICA_PCLN,		/* Input_Corr_PCLN */
	IPU_DEVICE_FF_LSC_LSC,		/* Bayer_Lsc_LSC */
	IPU_DEVICE_FF_DPC_DPC,		/* Bayer_Dpc_GDDPC */
	IPU_DEVICE_FF_IDS_SCALER,	/* Bayer_Scaler_SCALER */
	IPU_DEVICE_FF_AWB_AWRG,		/* Stat_AWB_AWRG */
	IPU_DEVICE_FF_AF_AF,		/* Stat_AF_AWB_FR_AF_AWB_FR_GRD */
	IPU_DEVICE_FF_AE_WGHT_HIST,	/* Stat_AE_WGHT_HIST */
	IPU_DEVICE_FF_AE_CCM,		/* Stat_AE_AE_CCM */
	IPU_DEVICE_FF_NUM_FF
};

#define IPU_DEVICE_FF_NUM_PSA_FF (IPU_DEVICE_FF_LACE_LACE + 1)
#define IPU_DEVICE_FF_NUM_ISA_FF \
	(IPU_DEVICE_FF_NUM_FF - IPU_DEVICE_FF_NUM_PSA_FF)

#endif /* __IPU_DEVICE_FF_DEVICES_H */
