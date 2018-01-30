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

#ifndef __IPU_DEVICE_GP_PROPERTIES_H
#define __IPU_DEVICE_GP_PROPERTIES_H

#include "storage_class.h"
#include "ipu_device_gp_properties_types.h"

STORAGE_CLASS_INLINE unsigned int
ipu_device_gp_mux_addr(const unsigned int device_id, const unsigned int mux_id);

#include "ipu_device_gp_properties_func.h"

#endif /* __IPU_DEVICE_GP_PROPERTIES_H */
