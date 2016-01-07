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
#ifndef _HRT_VIED_DEVICE_ACCESS_TYPES_H
#define _HRT_VIED_DEVICE_ACCESS_TYPES_H

#include "vied_types.h"
#include "vied_subsystem_access_types.h"

typedef unsigned int                  vied_device_id_t;
typedef struct vied_internal_route_s  vied_internal_route_t;

typedef unsigned int   vied_device_port_id_t;
typedef unsigned int   vied_device_regbank_id_t;
typedef unsigned int   vied_device_reg_id_t;
typedef unsigned int   vied_device_memory_id_t;

#include "vied_device_access_types_impl.h"

#endif /* _HRT_VIED_DEVICE_ACCESS_TYPES_H */
