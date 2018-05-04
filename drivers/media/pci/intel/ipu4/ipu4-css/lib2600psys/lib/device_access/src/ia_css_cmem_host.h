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

#ifndef __IA_CSS_CMEM_HOST_H
#define __IA_CSS_CMEM_HOST_H

/* This file is an inline implementation for the interface ia_css_cmem.h
 * and should only be included there. */

#include "assert_support.h"
#include "misc_support.h"

STORAGE_CLASS_INLINE uint32_t
ia_css_cmem_load_32(unsigned int ssid, ia_css_cmem_address_t address)
{
	/* Address has to be word aligned */
	assert(0 == address % 4);
	return vied_subsystem_load_32(ssid, address);
}

STORAGE_CLASS_INLINE uint32_t
ia_css_cond_cmem_load_32(bool cond, unsigned int ssid,
			 ia_css_cmem_address_t address)
{
	/* Address has to be word aligned */
	assert(0 == address % 4);
	if (cond)
		return vied_subsystem_load_32(ssid, address);
	else
		return 0;
}

STORAGE_CLASS_INLINE void
ia_css_cmem_store_32(unsigned int ssid, ia_css_cmem_address_t address,
		     uint32_t data)
{
	/* Address has to be word aligned */
	assert(0 == address % 4);
	vied_subsystem_store_32(ssid, address, data);
}

STORAGE_CLASS_INLINE void
ia_css_cond_cmem_store_32(bool cond, unsigned int ssid,
			  ia_css_cmem_address_t address, uint32_t data)
{
	/* Address has to be word aligned */
	assert(0 == address % 4);
	if (cond)
		vied_subsystem_store_32(ssid, address, data);
}

STORAGE_CLASS_INLINE void
ia_css_cmem_load(unsigned int ssid, ia_css_cmem_address_t address, void *data,
		 unsigned int size)
{
	uint32_t *data32 = (uint32_t *)data;
	uint32_t end = address + size;

	assert(size % 4 == 0);
	assert(address % 4 == 0);
	assert((long)data % 4 == 0);

	while (address != end) {
		*data32 = ia_css_cmem_load_32(ssid, address);
		address += 4;
		data32 += 1;
	}
}

STORAGE_CLASS_INLINE void
ia_css_cmem_store(unsigned int ssid, ia_css_cmem_address_t address,
		  const void *data, unsigned int size)
{
	uint32_t *data32 = (uint32_t *)data;
	uint32_t end = address + size;

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
ia_css_cmem_zero(unsigned int ssid, ia_css_cmem_address_t address,
		 unsigned int size)
{
	uint32_t end = address + size;

	assert(size % 4 == 0);
	assert(address % 4 == 0);

	while (address != end) {
		ia_css_cmem_store_32(ssid, address, 0);
		address += 4;
	}
}

STORAGE_CLASS_INLINE ia_css_cmem_address_t
ia_css_cmem_get_cmem_addr_from_dmem(unsigned int base_addr, void *p)
{
	NOT_USED(base_addr);
	return (ia_css_cmem_address_t)(uintptr_t)p;
}

#endif /* __IA_CSS_CMEM_HOST_H */
