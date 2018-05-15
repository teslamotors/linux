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

#ifndef __IPU_DEVICE_GP_DEVICES_H
#define __IPU_DEVICE_GP_DEVICES_H
#include "math_support.h"
#include "type_support.h"

enum ipu_device_gp_id {
	IPU_DEVICE_GP_PSA = 0,	/* PSA */
	IPU_DEVICE_GP_ISA_STATIC,		/* ISA Static */
	IPU_DEVICE_GP_ISA_RUNTIME,		/* ISA Runtime */
	IPU_DEVICE_GP_ISL,		/* ISL */
	IPU_DEVICE_GP_NUM_GP
};

enum ipu_device_gp_psa_mux_id {
	/* Post RYNR/CCN: 0-To ACM (Video), 1-To Demosaic (Stills)*/
	IPU_DEVICE_GP_PSA_STILLS_MODE_MUX = 0,
	/* Post Vec2Str 4: 0-To GTC, 1-To ACM  */
	IPU_DEVICE_GP_PSA_V2S_RGB_4_DEMUX,
	/* Post DM and pre ACM	0-CCM/ACM: 1-DM Component Splitter */
	IPU_DEVICE_GP_PSA_S2V_RGB_F_MUX,
	/* Pre ACM/CCM: 0-To CCM/ACM, 1-To str2vec id_f */
	IPU_DEVICE_GP_PSA_ACM_DEMUX,
	IPU_DEVICE_GP_PSA_MUX_NUM_MUX
};

enum ipu_device_gp_isa_static_mux_id {
	IPU_DEVICE_GP_ISA_STATIC_MUX_SEL = 0,
	IPU_DEVICE_GP_ISA_STATIC_PORTA_BLK,
	IPU_DEVICE_GP_ISA_STATIC_PORTB_BLK,
	IPU_DEVICE_GP_ISA_STATIC_PORTC_BLK,
	IPU_DEVICE_GP_ISA_STATIC_AWB_MUX_SEL,
	IPU_DEVICE_GP_ISA_STATIC_AWB_MUX_INPUT_CORR_PORT_BLK,
	IPU_DEVICE_GP_ISA_STATIC_AWB_MUX_DPC_PORT_BLK,
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

#define IPU_DEVICE_GP_MAX_NUM MAX4((uint32_t)IPU_DEVICE_GP_PSA_MUX_NUM_MUX, \
	(uint32_t)IPU_DEVICE_GP_ISA_STATIC_MUX_NUM_MUX,                     \
	(uint32_t)IPU_DEVICE_GP_ISA_RUNTIME_MUX_NUM_MUX,                    \
	(uint32_t)IPU_DEVICE_GP_ISL_MUX_NUM_MUX)

#endif /* __IPU_DEVICE_GP_DEVICES_H */
