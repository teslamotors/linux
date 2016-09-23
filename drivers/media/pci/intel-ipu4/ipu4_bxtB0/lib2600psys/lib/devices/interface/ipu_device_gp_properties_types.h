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

#ifndef __IPU_DEVICE_GP_PROPERTIES_TYPES_H__
#define __IPU_DEVICE_GP_PROPERTIES_TYPES_H__

enum ipu_device_gp_isa_value {
	/* ISA_MUX_SEL options */
	IPU_DEVICE_GP_ISA_MUX_SEL_ICA = 0, /* Enable output after FF ICA */
	IPU_DEVICE_GP_ISA_MUX_SEL_LSC = 1, /* Enable output after FF LSC */
	IPU_DEVICE_GP_ISA_MUX_SEL_DPC = 2, /* Enable output after FF DPC */
	/* ICA stream block options */
	/* UNBLOCK signal received from ICA */
	IPU_DEVICE_GP_ISA_ICA_UNBLOCK = 0,
	/* BLOCK signal received from ICA */
	IPU_DEVICE_GP_ISA_ICA_BLOCK = 1,
	/* LSC stream block options */
	/* UNBLOCK signal received from LSC */
	IPU_DEVICE_GP_ISA_LSC_UNBLOCK = 0,
	/* BLOCK signal received from LSC */
	IPU_DEVICE_GP_ISA_LSC_BLOCK = 1,
	/* DPC stream block options */
	/* UNBLOCK signal received from DPC */
	IPU_DEVICE_GP_ISA_DPC_UNBLOCK = 0,
	/* BLOCK signal received from DPC */
	IPU_DEVICE_GP_ISA_DPC_BLOCK = 1,
	/* Defines needed only for bxtB0 */
	/* ISA_AWB_MUX_SEL options */
	/* Input Correction input */
	IPU_DEVICE_GP_ISA_AWB_MUX_SEL_ICA = 0,
	/* DPC input */
	IPU_DEVICE_GP_ISA_AWB_MUX_SEL_DPC = 1,
	/* ISA_AWB_MUX_SEL options */
	/* UNBLOCK DPC input */
	IPU_DEVICE_GP_ISA_AWB_MUX_ICA_UNBLOCK = 0,
	/* BLOCK DPC input */
	IPU_DEVICE_GP_ISA_AWB_MUX_ICA_BLOCK = 1,
	/* ISA_AWB_MUX_SEL options */
	/* UNBLOCK Input Correction input */
	IPU_DEVICE_GP_ISA_AWB_MUX_DPC_UNBLOCK = 0,
	/* BLOCK Input Correction input */
	IPU_DEVICE_GP_ISA_AWB_MUX_DPC_BLOCK = 1,

	/* PAF STRM options */
	/* Disable streaming to PAF FF*/
	IPU_DEVICE_GP_ISA_PAF_DISABLE_STREAM = 0,
	/* Enable stream0 to PAF FF*/
	IPU_DEVICE_GP_ISA_PAF_ENABLE_STREAM0 = 1,
	/* Enable stream1 to PAF FF*/
	IPU_DEVICE_GP_ISA_PAF_ENABLE_STREAM1 = 2,
	/* PAF SRC SEL options */
	/* External channel input */
	IPU_DEVICE_GP_ISA_PAF_SRC_SEL0 = 0,
	/* DPC extracted input */
	IPU_DEVICE_GP_ISA_PAF_SRC_SEL1 = 1,
	/* PAF_GDDPC_BLK options */
	IPU_DEVICE_GP_ISA_PAF_GDDPC_PORT_BLK0 = 0,
	IPU_DEVICE_GP_ISA_PAF_GDDPC_PORT_BLK1 = 1,
	/* PAF ISA STR_PORT options */
	IPU_DEVICE_GP_ISA_PAF_STR_PORT0 = 0,
	IPU_DEVICE_GP_ISA_PAF_STR_PORT1 = 1,

	/* Needed only for IPU5 */
	/* scaler port block options */
	IPU_DEVICE_GP_ISA_SCALER_PORT_UNBLOCK = 0,
	IPU_DEVICE_GP_ISA_SCALER_PORT_BLOCK = 1,
	/* sis port block options */
	IPU_DEVICE_GP_ISA_SIS_PORT_UNBLOCK = 0,
	IPU_DEVICE_GP_ISA_SIS_PORT_BLOCK = 1,
	IPU_DEVICE_GP_ISA_CONF_INVALID = 0xFF
};

enum ipu_device_gp_psa_value {
	/* Defines needed for bxtB0 */
	/* PSA_STILLS_MODE_MUX */
	IPU_DEVICE_GP_PSA_MUX_POST_RYNR_ROUTE_WO_DM  = 0,
	IPU_DEVICE_GP_PSA_MUX_POST_RYNR_ROUTE_W_DM = 1,
	/* PSA_ACM_DEMUX */
	IPU_DEVICE_GP_PSA_DEMUX_PRE_ACM_ROUTE_TO_ACM = 0,
	IPU_DEVICE_GP_PSA_DEMUX_PRE_ACM_ROUTE_TO_S2V = 1,
	/* PSA_S2V_RGB_F_MUX */
	IPU_DEVICE_GP_PSA_MUX_PRE_S2V_RGB_F_FROM_ACM = 0,
	IPU_DEVICE_GP_PSA_MUX_PRE_S2V_RGB_F_FROM_DM_OR_SPLITTER = 1,
	/* PSA_V2S_RGB_4_DEMUX */
	IPU_DEVICE_GP_PSA_DEMUX_POST_V2S_RGB_4_TO_GTM = 0,
	IPU_DEVICE_GP_PSA_DEMUX_POST_V2S_RGB_4_TO_ACM = 1,
	/* Defines needed for IPU5.
	 * For details see diagram in section 2.2.2 of IPU5 general
	 * fixed function MAS. Choose between pixel stream and
	 * delta stream as BNLM output (gpreg 1)
	 */
	IPU_DEVICE_GP_PSA_1_NOISE_MUX_BNLM_PIXELS = 0,
	IPU_DEVICE_GP_PSA_1_NOISE_MUX_DELTA_STREAM = 1,
	/* enable/disable BNLM Pixel Block (gpreg 2) */
	IPU_DEVICE_GP_PSA_1_BNLM_PIXEL_STREAM_BLOCK_DISABLE = 0,
	IPU_DEVICE_GP_PSA_1_BNLM_PIXEL_STREAM_BLOCK_ENABLE = 1,
	/* enable/disable BNLM delta stream (gpreg 3) */
	IPU_DEVICE_GP_PSA_1_BNLM_DELTA_STREAM_BLOCK_DISABLE = 0,
	IPU_DEVICE_GP_PSA_1_BNLM_DELTA_STREAM_BLOCK_ENABLE = 1,
	/* choose BNLM output to XNR or to WB/DM (gpreg 0) */
	IPU_DEVICE_GP_PSA_2_BNLM_TO_XNR = 0,
	IPU_DEVICE_GP_PSA_2_BNLM_TO_WB_DM = 1,
	/* choose direction of output from vec2str 4 (gpreg 4) */
	IPU_DEVICE_GP_PSA_2_V2S_RGB_4_TO_GTC = 0,
	IPU_DEVICE_GP_PSA_2_V2S_RGB_4_TO_ACM = 1,
	IPU_DEVICE_GP_PSA_2_V2S_RGB_4_TO_VCSC = 2,
	IPU_DEVICE_GP_PSA_2_V2S_RGB_4_TO_GSTAR = 3,
	/* enable/disable VCSC input block (gpreg 7) */
	IPU_DEVICE_GP_PSA_2_VCSC_INPUT_BLOCK_DISABLE = 0,
	IPU_DEVICE_GP_PSA_2_VCSC_INPUT_BLOCK_ENABLE = 1,
	/* enable/disable XNR5 bypass block (gpreg 8) */
	IPU_DEVICE_GP_PSA_2_XNR5_BP_BLOCK_DISABLE = 0,
	IPU_DEVICE_GP_PSA_2_XNR5_BP_BLOCK_ENABLE = 1,
	/* choose to use VCSC or bypass it (gpreg 5) */
	IPU_DEVICE_GP_PSA_3_MUX_USE_VCSC = 0,
	IPU_DEVICE_GP_PSA_3_MUX_BP_VCSC = 1,
	/* choose to use XNR5 or bypass it (gpreg 6) */
	IPU_DEVICE_GP_PSA_3_MUX_USE_XNR5 = 0,
	IPU_DEVICE_GP_PSA_3_MUX_BP_XNR5 = 1,
	/* choose which input to use for the BNLM acc */
	IPU_DEVICE_GP_PSA_1_BNLM_IN_MUX_V2S = 0,
	IPU_DEVICE_GP_PSA_1_BNLM_IN_MUX_ISA_DOWNSCALED = 1,
	IPU_DEVICE_GP_PSA_1_BNLM_IN_MUX_ISA_UNSCALED = 2,
	IPU_DEVICE_GP_PSA_CONF_INVALID = 0xFF
};

enum ipu_device_gp_isl_value {
	/* choose and route pixel stream to CSI BE */
	IPU_DEVICE_GP_ISL_CSI_BE_IN_USE = 0,
	/* choose and route pixel stream bypass CSI BE */
	IPU_DEVICE_GP_ISL_CSI_BE_BYPASS
};

#endif /* __IPU_DEVICE_GP_PROPERTIES_TYPES_H__ */
