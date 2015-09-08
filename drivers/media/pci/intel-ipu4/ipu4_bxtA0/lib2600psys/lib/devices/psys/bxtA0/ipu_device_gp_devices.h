/**
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
*/

#ifndef _IPU_DEVICE_GP_DEVICES_H_
#define _IPU_DEVICE_GP_DEVICES_H_

enum ipu_device_gp_id {
	IPU_DEVICE_GP_PSA = 0,	/* PSA */
	IPU_DEVICE_GP_ISA,		/* ISA */
	IPU_DEVICE_GP_NUM_GP
};

enum ipu_device_gp_psa_mux_id {
	IPU_DEVICE_GP_PSA_RGB_S2V_MUX = 0, /* Currently not used in bxtA0 */
	IPU_DEVICE_GP_PSA_MUX_NUM_MUX
};

enum ipu_device_gp_isa_mux_id {
	IPU_DEVICE_GP_ISA_FRAME_SIZE,
	IPU_DEVICE_GP_ISA_SCALED_FRAME_SIZE,
	IPU_DEVICE_GP_ISA_MUX_SEL,
	IPU_DEVICE_GP_ISA_PORTA_BLK,
	IPU_DEVICE_GP_ISA_PORTB_BLK,
	IPU_DEVICE_GP_ISA_PORTC_BLK,
	IPU_DEVICE_GP_ISA_MUX_NUM_MUX
};


#endif /* _IPU_DEVICE_GP_DEVICES_H_ */
