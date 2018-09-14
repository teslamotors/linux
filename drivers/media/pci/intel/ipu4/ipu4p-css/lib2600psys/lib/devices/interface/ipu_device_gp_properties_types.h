/**
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

#ifndef __IPU_DEVICE_GP_PROPERTIES_TYPES_H
#define __IPU_DEVICE_GP_PROPERTIES_TYPES_H

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
};

enum ipu_device_gp_isl_value {
	/* choose and route pixel stream to CSI BE */
	IPU_DEVICE_GP_ISL_CSI_BE_IN_USE = 0,
	/* choose and route pixel stream bypass CSI BE */
	IPU_DEVICE_GP_ISL_CSI_BE_BYPASS
};

#endif /* __IPU_DEVICE_GP_PROPERTIES_TYPES_H */

