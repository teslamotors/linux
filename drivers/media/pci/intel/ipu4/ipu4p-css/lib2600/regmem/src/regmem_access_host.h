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

#ifndef __REGMEM_ACCESS_HOST_H
#define __REGMEM_ACCESS_HOST_H

#include "regmem_access.h" /* implemented interface */

#include "storage_class.h"
#include "regmem_const.h"
#include <vied/vied_subsystem_access.h>
#include "ia_css_cmem.h"

STORAGE_CLASS_INLINE unsigned int
regmem_load_32(unsigned int mem_addr, unsigned int reg, unsigned int ssid)
{
	/* No need to add REGMEM_OFFSET, it is already included in mem_addr. */
	return ia_css_cmem_load_32(ssid, mem_addr + (REGMEM_WORD_BYTES*reg));
}

STORAGE_CLASS_INLINE void
regmem_store_32(unsigned int mem_addr, unsigned int reg,
	unsigned int value, unsigned int ssid)
{
	/* No need to add REGMEM_OFFSET, it is already included in mem_addr. */
	ia_css_cmem_store_32(ssid, mem_addr + (REGMEM_WORD_BYTES*reg),
		value);
}

#endif /* __REGMEM_ACCESS_HOST_H */
