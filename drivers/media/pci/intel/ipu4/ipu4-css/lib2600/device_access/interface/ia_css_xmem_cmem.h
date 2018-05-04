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

#ifndef __IA_CSS_XMEM_CMEM_H
#define __IA_CSS_XMEM_CMEM_H

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
	unsigned int size);

/* include inline implementation */
#include "ia_css_xmem_cmem_impl.h"

#endif /* __IA_CSS_XMEM_CMEM_H */
