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

#ifndef __IA_CSS_XMEM_CMEM_IMPL_H
#define __IA_CSS_XMEM_CMEM_IMPL_H

#include "ia_css_xmem_cmem.h"

#include "ia_css_cmem.h"
#include "ia_css_xmem.h"

/* Copy data from xmem to cmem, e.g., from a program in DDR to a cell's DMEM */
/* This may also be implemented using DMA */

STORAGE_CLASS_INLINE void
ia_css_xmem_to_cmem_copy(
	unsigned int mmid,
	unsigned int ssid,
	ia_css_xmem_address_t src,
	ia_css_cmem_address_t dst,
	unsigned int size)
{
	/* copy from ddr to subsystem, e.g., cell dmem */
	ia_css_cmem_address_t end = dst + size;

	assert(size % 4 == 0);
	assert((uintptr_t) dst % 4 == 0);
	assert((uintptr_t) src % 4 == 0);

	while (dst != end) {
		uint32_t data;

		data = ia_css_xmem_load_32(mmid, src);
		ia_css_cmem_store_32(ssid, dst, data);
		dst += 4;
		src += 4;
	}
}

/* Copy data from cmem to xmem */

STORAGE_CLASS_INLINE void
ia_css_cmem_to_xmem_copy(
	unsigned int mmid,
	unsigned int ssid,
	ia_css_cmem_address_t src,
	ia_css_xmem_address_t dst,
	unsigned int size)
{
	/* copy from ddr to subsystem, e.g., cell dmem */
	ia_css_xmem_address_t end = dst + size;

	assert(size % 4 == 0);
	assert((uintptr_t) dst % 4 == 0);
	assert((uintptr_t) src % 4 == 0);

	while (dst != end) {
		uint32_t data;

		data = ia_css_cmem_load_32(mmid, src);
		ia_css_xmem_store_32(ssid, dst, data);
		dst += 4;
		src += 4;
	}
}


#endif /* __IA_CSS_XMEM_CMEM_IMPL_H */
