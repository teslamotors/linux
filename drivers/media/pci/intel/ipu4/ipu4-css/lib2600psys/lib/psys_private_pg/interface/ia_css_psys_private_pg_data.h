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

#ifndef __IA_CSS_PSYS_PRIVATE_PG_DATA_H
#define __IA_CSS_PSYS_PRIVATE_PG_DATA_H

#include "ipu_device_acb_devices.h"
#include "ipu_device_gp_devices.h"
#include "type_support.h"
#include "vied_nci_acb_route_type.h"

#define PRIV_CONF_INVALID	0xFF

struct ia_css_psys_pg_buffer_information_s {
	unsigned int buffer_base_addr;
	unsigned int bpe;
	unsigned int buffer_width;
	unsigned int buffer_height;
	unsigned int num_of_buffers;
	unsigned int dfm_port_addr;
};

typedef struct ia_css_psys_pg_buffer_information_s ia_css_psys_pg_buffer_information_t;

struct ia_css_psys_private_pg_data {
	nci_acb_route_t acb_route[IPU_DEVICE_ACB_NUM_ACB];
	uint8_t psa_mux_conf[IPU_DEVICE_GP_PSA_MUX_NUM_MUX];
	uint8_t isa_mux_conf[IPU_DEVICE_GP_ISA_STATIC_MUX_NUM_MUX];
	ia_css_psys_pg_buffer_information_t input_buffer_info;
};

#endif /* __IA_CSS_PSYS_PRIVATE_PG_DATA_H */

