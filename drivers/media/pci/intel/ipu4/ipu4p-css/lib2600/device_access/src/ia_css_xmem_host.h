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

#ifndef __IA_CSS_XMEM_HOST_H
#define __IA_CSS_XMEM_HOST_H

#include "ia_css_xmem.h"
#include <vied/shared_memory_access.h>
#include "assert_support.h"
#include <type_support.h>

STORAGE_CLASS_INLINE uint8_t
ia_css_xmem_load_8(unsigned int mmid, ia_css_xmem_address_t address)
{
	return shared_memory_load_8(mmid, address);
}

STORAGE_CLASS_INLINE uint16_t
ia_css_xmem_load_16(unsigned int mmid, ia_css_xmem_address_t address)
{
	/* Address has to be half-word aligned */
	assert(0 == (uintptr_t) address % 2);
	return shared_memory_load_16(mmid, address);
}

STORAGE_CLASS_INLINE uint32_t
ia_css_xmem_load_32(unsigned int mmid, ia_css_xmem_address_t address)
{
	/* Address has to be word aligned */
	assert(0 == (uintptr_t) address % 4);
	return shared_memory_load_32(mmid, address);
}

STORAGE_CLASS_INLINE void
ia_css_xmem_load(unsigned int mmid, ia_css_xmem_address_t address, void *data,
		 unsigned int size)
{
	shared_memory_load(mmid, address, data, size);
}

STORAGE_CLASS_INLINE void
ia_css_xmem_store_8(unsigned int mmid, ia_css_xmem_address_t address,
		    uint8_t value)
{
	shared_memory_store_8(mmid, address, value);
}

STORAGE_CLASS_INLINE void
ia_css_xmem_store_16(unsigned int mmid, ia_css_xmem_address_t address,
		     uint16_t value)
{
	/* Address has to be half-word aligned */
	assert(0 == (uintptr_t) address % 2);
	shared_memory_store_16(mmid, address, value);
}

STORAGE_CLASS_INLINE void
ia_css_xmem_store_32(unsigned int mmid, ia_css_xmem_address_t address,
		     uint32_t value)
{
	/* Address has to be word aligned */
	assert(0 == (uintptr_t) address % 4);
	shared_memory_store_32(mmid, address, value);
}

STORAGE_CLASS_INLINE void
ia_css_xmem_store(unsigned int mmid, ia_css_xmem_address_t address,
		  const void *data, unsigned int bytes)
{
	shared_memory_store(mmid, address, data, bytes);
}

#endif /* __IA_CSS_XMEM_HOST_H */
