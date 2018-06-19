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

#ifndef __IA_CSS_CMEM_H
#define __IA_CSS_CMEM_H

#include "type_support.h"
#include "storage_class.h"

#ifdef __VIED_CELL
typedef unsigned int ia_css_cmem_address_t;
#else
#include <vied/vied_subsystem_access.h>
typedef vied_subsystem_address_t ia_css_cmem_address_t;
#endif

STORAGE_CLASS_INLINE uint32_t
ia_css_cmem_load_32(unsigned int ssid, ia_css_cmem_address_t address);

STORAGE_CLASS_INLINE void
ia_css_cmem_store_32(unsigned int ssid, ia_css_cmem_address_t address,
		     uint32_t value);

STORAGE_CLASS_INLINE void
ia_css_cmem_load(unsigned int ssid, ia_css_cmem_address_t address, void *data,
		 unsigned int size);

STORAGE_CLASS_INLINE void
ia_css_cmem_store(unsigned int ssid, ia_css_cmem_address_t address,
		  const void *data, unsigned int size);

STORAGE_CLASS_INLINE void
ia_css_cmem_zero(unsigned int ssid, ia_css_cmem_address_t address,
		 unsigned int size);

STORAGE_CLASS_INLINE ia_css_cmem_address_t
ia_css_cmem_get_cmem_addr_from_dmem(unsigned int base_addr, void *p);

/* Include inline implementation */

#ifdef __VIED_CELL
#include "ia_css_cmem_cell.h"
#else
#include "ia_css_cmem_host.h"
#endif

#endif /* __IA_CSS_CMEM_H */
