/**
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

#ifndef _IPU_DEVICE_GP_DEVICES_H_
#define _IPU_DEVICE_GP_DEVICES_H_
#include "math_support.h"
#include "type_support.h"

enum ipu_device_gp_id {
	IPU_DEVICE_GP_PSA = 0,
	IPU_DEVICE_GP_ISA_STATIC,
	IPU_DEVICE_GP_ISA_RUNTIME,
	IPU_DEVICE_GP_ISL,
	IPU_DEVICE_GP_NUM_GP
};

/* The PSA_* naming in the enum members refers to the internal separation of
 * the PSA. Using it this way should make this change transparent
 * to higher layers.
 *
 * For details on the values and usage of the muxes see Figures 3-1
 * in section 2.1.3 and Figure 3-3 in section 2.2.2 of the IPU-5 General
 * Fixed Function MAS. Additionally, refer to the respective description
 * fields in the RDL files on the SDK (either psa_ip_top_system.rdl
 * or isa_ps_system.rdl).
 */
enum ipu_device_gp_psa_mux_id {
	/* Mux/demuxes */
	/* 0 - BNLM output pixel stream; 1 - BNLM output delta stream */
	IPU_DEVICE_GP_PSA_1_NOISE_MUX = 0,
	/* 0 - To XNR; 1 - WB/DM */
	IPU_DEVICE_GP_PSA_2_STILLS_MODE_MUX,
	/* 0 - To GTC. 1 - To ACM; 2 - To VCSC; 3 - To Gamma_Star */
	IPU_DEVICE_GP_PSA_2_V2S_RGB_4_DEMUX,
	/* 0 - VCSC output is chosen; 1 - VCSC is bypassed */
	IPU_DEVICE_GP_PSA_3_VCSC_BP_MUX,
	/* 0 - XNR output is selected; 1 - XNR bypass is selected */
	IPU_DEVICE_GP_PSA_3_XNR_BP_MUX,
	/* 0 - v2s_1 output; 1 - ISA downscaled stream;
	 * 2 - ISA original sized stream
	 */
	IPU_DEVICE_GP_PSA_1_BNLM_IN_MUX,
	/* Device blockers */
	IPU_DEVICE_GP_PSA_1_BNLM_PIXEL_STRM_BLK,
	IPU_DEVICE_GP_PSA_1_BNLM_DELTA_STRM_BLK,
	IPU_DEVICE_GP_PSA_2_VCSC_IN_BLK,
	IPU_DEVICE_GP_PSA_2_XNR5_BP_BLK,
	IPU_DEVICE_GP_PSA_MUX_NUM_MUX
};

/* Proper description needs to be added to these muxes. The rdl is being
 * updated by the HW team. More detailed description will be added
 * once that is done.
 */
enum ipu_device_gp_isa_static_mux_id {
	/* 0 - From input correction	1 - From LSC	2 - From DPC/GD/AF */
	IPU_DEVICE_GP_ISA_STATIC_MUX_SEL = 0,
	/* 0 - to ISL.S2V; 1 - to PSA */
	IPU_DEVICE_GP_ISA_STATIC_ORIG_OUT_DEMUX_SEL,
	/* 0 - to ISL.S2V; 1 - to PSA */
	IPU_DEVICE_GP_ISA_STATIC_SCALED_OUT_DEMUX_SEL,
	IPU_DEVICE_GP_ISA_STATIC_PORTA_BLK,
	IPU_DEVICE_GP_ISA_STATIC_PORTB_BLK,
	IPU_DEVICE_GP_ISA_STATIC_PORTC_BLK,
	IPU_DEVICE_GP_ISA_STATIC_SPA_SCALER_CTRL,
	IPU_DEVICE_GP_ISA_STATIC_SPA_FULL_RES_CTRL,
	IPU_DEVICE_GP_ISA_STATIC_SPA_SIS_CTRL,
	IPU_DEVICE_GP_ISA_STATIC_SCALED_OUT_YUV_MODE,
	/* 0 - Input Correction input	1 - DPC input */
	IPU_DEVICE_GP_ISA_STATIC_AWB_MUX_SEL,
	IPU_DEVICE_GP_ISA_STATIC_AWB_MUX_INPUT_CORR_PORT_BLK,
	IPU_DEVICE_GP_ISA_STATIC_AWB_MUX_DPC_PORT_BLK,
	IPU_DEVICE_GP_ISA_STATIC_PAF_STREAM_SYNC_CFG,
	IPU_DEVICE_GP_ISA_STATIC_PAF_SRC_SEL,
	IPU_DEVICE_GP_ISA_STATIC_PAF_GDDPC_BLK,
	IPU_DEVICE_GP_ISA_STATIC_PAF_ISA_PS_STREAM_PORT_BLK,
	IPU_DEVICE_GP_ISA_STATIC_SCALER_PORT_BLK,
	IPU_DEVICE_GP_ISA_STATIC_SIS_PORT_BLK,
	IPU_DEVICE_GP_ISA_STATIC_MUX_NUM_MUX
};

enum ipu_device_gp_isa_runtime_mux_id {
	IPU_DEVICE_GP_ISA_RUNTIME_FRAME_SIZE = 0,
	IPU_DEVICE_GP_ISA_RUNTIME_SCALED_FRAME_SIZE,
	IPU_DEVICE_GP_ISA_RUNTIME_MUX_NUM_MUX
};

enum ipu_device_gp_isl_mux_id {
	IPU_DEVICE_GP_ISL_MIPI_BE_MUX = 0,
	IPU_DEVICE_GP_ISL_MUX_NUM_MUX
};

/* The value below is the largest *MUX_NUM_MUX of the mux enums. */
#define IPU_DEVICE_GP_MAX_NUM ((uint32_t)(IPU_DEVICE_GP_ISA_STATIC_MUX_NUM_MUX))

#endif /* _IPU_DEVICE_GP_DEVICES_H_ */
