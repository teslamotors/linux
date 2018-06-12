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

#ifndef __IA_CSS_BASE_TYPES_H
#define __IA_CSS_BASE_TYPES_H

#include "type_support.h"

#define VIED_VADDRESS_BITS				32
typedef uint32_t vied_vaddress_t;

#define DEVICE_DESCRIPTOR_ID_BITS			32
typedef struct {
	uint8_t device_id;
	uint8_t instance_id;
	uint8_t channel_id;
	uint8_t section_id;
} device_descriptor_fields_t;

typedef union {
	device_descriptor_fields_t fields;
	uint32_t data;
} device_descriptor_id_t;

typedef uint16_t ia_css_process_id_t;

#endif /* __IA_CSS_BASE_TYPES_H */

