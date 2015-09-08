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

#ifndef _IA_CSS_CELL_PROGRAM_LOAD_IMPL_H_
#define _IA_CSS_CELL_PROGRAM_LOAD_IMPL_H_

#include "ia_css_cell_program_load.h"

#include "ia_css_cell_program_load_storage_class.h"
#include "ia_css_cell_program_load_prog.h"
#include "ia_css_cell_program_struct.h"

IA_CSS_CELL_PROGRAM_LOAD_STORAGE_CLASS_C
int
ia_css_cell_program_load(
	unsigned int ssid,
	unsigned int mmid,
	ia_css_xmem_address_t host_addr,
	unsigned int vied_addr)
{
	struct ia_css_cell_program_s prog;
	int status;

	status = ia_css_cell_program_load_header(mmid, host_addr, &prog);
	if (status)
		return status;

	status = ia_css_cell_program_load_icache_prog(ssid, mmid, host_addr, vied_addr, &prog);
	if (status)
		return status;

	status = ia_css_cell_program_load_mem_prog(ssid, mmid, host_addr, vied_addr, &prog);
	if (status)
		return status;

	return status;
}

IA_CSS_CELL_PROGRAM_LOAD_STORAGE_CLASS_C
int
ia_css_cell_program_load_icache(
	unsigned int ssid,
	unsigned int mmid,
	ia_css_xmem_address_t host_addr,
	unsigned int vied_addr)
{
	struct ia_css_cell_program_s prog;
	int status;

	status = ia_css_cell_program_load_header(mmid, host_addr, &prog);
	if (status)
		return status;

	status = ia_css_cell_program_load_icache_prog(ssid, mmid, host_addr, vied_addr, &prog);
	return status;
}

IA_CSS_CELL_PROGRAM_LOAD_STORAGE_CLASS_C
int
ia_css_cell_program_load_mem(
	unsigned int ssid,
	unsigned int mmid,
	ia_css_xmem_address_t host_addr,
	unsigned int vied_addr)
{
	struct ia_css_cell_program_s prog;
	int status;

	status = ia_css_cell_program_load_header(mmid, host_addr, &prog);
	if (status)
		return status;

	status = ia_css_cell_program_load_mem_prog(ssid, mmid, host_addr, vied_addr, &prog);
	return status;
}

#endif /* _IA_CSS_CELL_PROGRAM_LOAD_IMPL_H_ */
