/*
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
#ifndef _HRT_VIED_MEMORY_ACCESS_TYPES_H
#define _HRT_VIED_MEMORY_ACCESS_TYPES_H

/** Types for the VIED memory access interface */

#include "vied_types.h"

/**
 * \brief An identifier for a system memory.
 *
 * This identifier must be a compile-time constant. It is used in
 * access to system memory.
 */
typedef unsigned int    vied_memory_t;

#ifndef __HIVECC
/**
 * \brief The type for a physical address
 */
typedef unsigned long long    vied_physical_address_t;
#endif

#endif /* _HRT_VIED_MEMORY_ACCESS_TYPES_H */
