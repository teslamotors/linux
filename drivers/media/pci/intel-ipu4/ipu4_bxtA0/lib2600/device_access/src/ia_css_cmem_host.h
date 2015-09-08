/**
* Support for Intel Camera Imaging ISP subsystem.
* Copyright (c) 2010 - 2015, Intel Corporation.
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

#ifndef _IA_CSS_CMEM_HOST_H_
#define _IA_CSS_CMEM_HOST_H_

#include "ia_css_cmem.h"

#include <vied/vied_subsystem_access.h>
#include "assert_support.h"
#include "type_support.h"

STORAGE_CLASS_INLINE uint8_t
ia_css_cmem_load_8(unsigned int ssid, ia_css_cmem_address_t address)
{
	return vied_subsystem_load_8(ssid, address);
}

STORAGE_CLASS_INLINE uint16_t
ia_css_cmem_load_16(unsigned int ssid, ia_css_cmem_address_t address)
{
	return vied_subsystem_load_16(ssid, address);
}

STORAGE_CLASS_INLINE uint32_t
ia_css_cmem_load_32(unsigned int ssid, ia_css_cmem_address_t address)
{
	return vied_subsystem_load_32(ssid, address);
}

STORAGE_CLASS_INLINE void
ia_css_cmem_store_8(unsigned int ssid, ia_css_cmem_address_t address, uint8_t data)
{
	vied_subsystem_store_8(ssid, address, data);
}

STORAGE_CLASS_INLINE void
ia_css_cmem_store_16(unsigned int ssid, ia_css_cmem_address_t address, uint16_t data)
{
	vied_subsystem_store_16(ssid, address, data);
}

STORAGE_CLASS_INLINE void
ia_css_cmem_store_32(unsigned int ssid, ia_css_cmem_address_t address, uint32_t data)
{
	vied_subsystem_store_32(ssid, address, data);
}

STORAGE_CLASS_INLINE void
ia_css_cmem_load(unsigned int ssid, ia_css_cmem_address_t address, void *data, unsigned int size)
{
	vied_subsystem_load(ssid, address, data, size);
}

STORAGE_CLASS_INLINE void
ia_css_cmem_store(unsigned int ssid, ia_css_cmem_address_t address, const void *data, unsigned int size)
{
	uint32_t *data32 = (uint32_t*)data;
	unsigned int end = address + size;

	assert(size % 4 == 0);
	assert(address % 4 == 0);
	assert((long)data % 4 == 0);

	while (address != end) {
		ia_css_cmem_store_32(ssid, address, *data32);
		address += 4;
		data32 += 1;
	}
}

STORAGE_CLASS_INLINE void
ia_css_cmem_zero(unsigned int ssid, ia_css_cmem_address_t address, unsigned int size)
{
	unsigned int end = address + size;

	assert(size % 4 == 0);
	assert(address % 4 == 0);

	while (address != end) {
		ia_css_cmem_store_32(ssid, address, 0);
		address += 4;
	}
}

#endif
