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
#ifndef _HRT_VIED_SUBSYSTEM_ACCESS_TYPES_H
#define _HRT_VIED_SUBSYSTEM_ACCESS_TYPES_H

/** Types for the VIED subsystem access interface */
#include <type_support.h>

/** \brief An identifier for a VIED subsystem.
 *
 * This identifier must be a compile-time constant. It is used in
 * access to a VIED subsystem.
 */
typedef  unsigned int   vied_subsystem_t;


/** \brief An address within a VIED subsystem */
typedef  uint32_t    vied_subsystem_address_t;

/** \brief A base address of a VIED subsystem seen from the host */
typedef  unsigned long long   vied_subsystem_base_address_t;

#endif /* _HRT_VIED_SUBSYSTEM_ACCESS_TYPES_H */
