/**
* Support for Intel Camera Imaging ISP subsystem.
* Copyright (c) 2010 - 2016, Intel Corporation.
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

#ifdef C_RUN
#error C_RUN not supported
#endif

#include "storage_class.h"
#include "regmem_const.h"
#include <vied/vied_subsystem_access.h>

STORAGE_CLASS_INLINE unsigned int
regmem_load_32(unsigned int mem_addr, unsigned int reg, unsigned int ssid)
{
	return vied_subsystem_load_32(ssid, mem_addr + REGMEM_OFFSET + (4*reg));
}

STORAGE_CLASS_INLINE void
regmem_store_32(unsigned int mem_addr, unsigned int reg,
	unsigned int value, unsigned int ssid)
{
	vied_subsystem_store_32(ssid, mem_addr + REGMEM_OFFSET + (4*reg),
		value);
}

#endif /* __REGMEM_ACCESS_HOST_H */
