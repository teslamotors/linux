/*
 * Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2015, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 * */

#ifndef _IPU_DEVICE_FF_DEVICES_H_
#define _IPU_DEVICE_FF_DEVICES_H_

enum ipu_device_ff_id {
	/* PSA fixed functions */
	IPU_DEVICE_FF_WBA_WBA           = 0,
	IPU_DEVICE_FF_WBA_BC            ,
	IPU_DEVICE_FF_ANR_SEARCH        ,
	IPU_DEVICE_FF_ANR_TRANSFORM     ,
	IPU_DEVICE_FF_ANR_STITCH        ,
	IPU_DEVICE_FF_ANR_TILE          ,
	IPU_DEVICE_FF_DEMOSAIC_DEMOSAIC ,
	IPU_DEVICE_FF_CCM_CCM           ,
	IPU_DEVICE_FF_CCM_BDC           ,
	IPU_DEVICE_FF_GTC_CSC_CDS       ,
	IPU_DEVICE_FF_GTC_GTM           ,
	IPU_DEVICE_FF_YUV1_SPLITTER     ,
	IPU_DEVICE_FF_YUV1_IEFD        ,
	IPU_DEVICE_FF_YUV1_YDS          ,
	IPU_DEVICE_FF_YUV1_TCC          ,
	IPU_DEVICE_FF_DVS_YBIN          ,
	IPU_DEVICE_FF_DVS_DVS           ,
	IPU_DEVICE_FF_LACE_LACE         ,
	/* ISA fixed functions */
	IPU_DEVICE_FF_ICA_INL           ,
	IPU_DEVICE_FF_ICA_GBL           ,
	IPU_DEVICE_FF_ICA_PCLN          ,
	IPU_DEVICE_FF_LSC_LSC           ,
	IPU_DEVICE_FF_DPC_DPC           ,
	IPU_DEVICE_FF_AWB_AWRG          ,
	IPU_DEVICE_FF_AF_AF             ,
	IPU_DEVICE_FF_AE_WGHT_HIST      ,
	IPU_DEVICE_FF_AE_CCM            ,
	IPU_DEVICE_FF_NUM_FF
};

#endif /*  _IPU_DEVICE_FF_DEVICES_H_ */
