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

#ifndef _REGMEM_ACCESS_H_
#define _REGMEM_ACCESS_H_

#include "storage_class.h"

enum regmem_id {
	/* pass pkg_dir address to SPC in non-secure mode */
	PKG_DIR_ADDR_REG	= 0,
	/* pass syscom configuration to SPC */
	SYSCOM_CONFIG_REG	= 1,
	/* syscom state - modified by SP */
	SYSCOM_STATE_REG	= 2,
	/* syscom commands - modified by the host */
	SYSCOM_COMMAND_REG	= 3,
	/* first syscom queue pointer register */
	SYSCOM_QPR_BASE_REG	= 4
};

STORAGE_CLASS_INLINE unsigned int
regmem_load_32(unsigned int mem_address, unsigned int reg, unsigned int ssid);

STORAGE_CLASS_INLINE void
regmem_store_32(unsigned int mem_address, unsigned int reg, unsigned int value,
		unsigned int ssid);

#ifdef __VIED_CELL
#include "regmem_access_cell.h"
#else
#include "regmem_access_host.h"
#endif

#endif /*_REGMEM_ACCESS_H_*/
