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

#ifndef _IA_CSS_CELL_PROGRAM_GROUP_LOAD_H_
#define _IA_CSS_CELL_PROGRAM_GROUP_LOAD_H_

#include "ia_css_cell_program_load_storage_class.h"
#include "ia_css_xmem.h"

/*  Load all programs in program group
    Return 0 on success, -1 on incorrect magic number, -2 on incorrect release tag
 */

IA_CSS_CELL_PROGRAM_LOAD_STORAGE_CLASS_H
int
ia_css_cell_program_group_load(
	unsigned int ssid,
	unsigned int mmid,
	ia_css_xmem_address_t program_addr, /* program address as seen from caller */
	unsigned int program_addr_icache    /* program address as seen from cell's icache */
);

/*  Load all programs in program group
    each group may have mutiple entry function. This function will return the info
    of each entry function to allow user start any of them
    Return 0 on success, -1 on incorrect magic number, -2 on incorrect release tag
 */

IA_CSS_CELL_PROGRAM_LOAD_STORAGE_CLASS_H
int
ia_css_cell_program_group_load_multi_entry(
	unsigned int ssid,
	unsigned int mmid,
	ia_css_xmem_address_t program_addr, /* program address as seen from caller */
	unsigned int program_addr_icache,    /* program address as seen from cell's icache */
	struct ia_css_cell_program_entry_func_info_s *entry_info,
	unsigned int num_entry_info
);

/*  Load all programs in program group, except icache of first program
 */

IA_CSS_CELL_PROGRAM_LOAD_STORAGE_CLASS_H
int
ia_css_cell_program_group_load_mem(
	unsigned int ssid,
	unsigned int mmid,
	ia_css_xmem_address_t program_addr, /* program address as seen from caller */
	unsigned int program_addr_icache    /* program address as seen from cell's icache */
);

#ifdef __INLINE_IA_CSS_CELL_PROGRAM_LOAD__
#include "ia_css_cell_program_group_load_impl.h"
#endif

#endif /* _IA_CSS_CELL_PROGRAM_GROUP_LOAD_H_ */
