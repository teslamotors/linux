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

#ifndef __BUFFER_TYPE_H
#define __BUFFER_TYPE_H

/* portable access to buffers in DDR */

#ifdef __VIED_CELL
typedef unsigned int buffer_address;
#else
/* workaround needed because shared_memory_access.h uses size_t */
#include "type_support.h"
#include "vied/shared_memory_access.h"
typedef host_virtual_address_t buffer_address;
#endif

#endif /* __BUFFER_TYPE_H */
