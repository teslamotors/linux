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
#ifndef _HRT_VIED_TYPES_H
#define _HRT_VIED_TYPES_H

/** Types shared by VIED interfaces */

#include <type_support.h>

/** \brief An address within a VIED subsystem
 *
 * This will eventually replace teh vied_memory_address_t and  vied_subsystem_address_t
 */
typedef uint32_t vied_address_t;

/** \brief Memory address type
 *
 * A memory address is an offset within a memory.
 */
typedef uint32_t   vied_memory_address_t;

/** \brief Master port id */
typedef int   vied_master_port_id_t;

/**
 * \brief Require the existence of a certain type
 *
 * This macro can be used in interface header files to ensure that
 * an implementation define type with a specified name exists.
 */
#define _VIED_REQUIRE_TYPE(T) enum { _VIED_SIZEOF_##T = sizeof(T) }


#endif /*  _HRT_VIED_TYPES_H */
