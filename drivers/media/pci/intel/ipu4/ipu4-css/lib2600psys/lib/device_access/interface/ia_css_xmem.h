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

#ifndef __IA_CSS_XMEM_H
#define __IA_CSS_XMEM_H

#include "type_support.h"
#include "storage_class.h"

#ifdef __VIED_CELL
typedef unsigned int ia_css_xmem_address_t;
#else
#include <vied/shared_memory_access.h>
typedef host_virtual_address_t ia_css_xmem_address_t;
#endif

STORAGE_CLASS_INLINE uint8_t
ia_css_xmem_load_8(unsigned int mmid, ia_css_xmem_address_t address);

STORAGE_CLASS_INLINE uint16_t
ia_css_xmem_load_16(unsigned int mmid, ia_css_xmem_address_t address);

STORAGE_CLASS_INLINE uint32_t
ia_css_xmem_load_32(unsigned int mmid, ia_css_xmem_address_t address);

STORAGE_CLASS_INLINE void
ia_css_xmem_load(unsigned int mmid, ia_css_xmem_address_t address, void *data,
		 unsigned int size);

STORAGE_CLASS_INLINE void
ia_css_xmem_store_8(unsigned int mmid, ia_css_xmem_address_t address,
		    uint8_t value);

STORAGE_CLASS_INLINE void
ia_css_xmem_store_16(unsigned int mmid, ia_css_xmem_address_t address,
		     uint16_t value);

STORAGE_CLASS_INLINE void
ia_css_xmem_store_32(unsigned int mmid, ia_css_xmem_address_t address,
		     uint32_t value);

STORAGE_CLASS_INLINE void
ia_css_xmem_store(unsigned int mmid, ia_css_xmem_address_t address,
		  const void *data, unsigned int bytes);

/* Include inline implementation */

#ifdef __VIED_CELL
#include "ia_css_xmem_cell.h"
#else
#include "ia_css_xmem_host.h"
#endif

#endif /* __IA_CSS_XMEM_H */
