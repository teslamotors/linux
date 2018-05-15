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

#ifndef __IA_CSS_CELL_PROGRAM_GROUP_LOAD_IMPL_H
#define __IA_CSS_CELL_PROGRAM_GROUP_LOAD_IMPL_H

#include "ia_css_cell_program_group_load.h"

#include "ia_css_cell_program_load_storage_class.h"
#include "ia_css_cell_program_load_prog.h"
#include "ia_css_cell_program_struct.h"

IA_CSS_CELL_PROGRAM_LOAD_STORAGE_CLASS_C
int
ia_css_cell_program_group_load(
	unsigned int ssid,
	unsigned int mmid,
	ia_css_xmem_address_t host_addr,
	unsigned int vied_addr)
{
	struct ia_css_cell_program_s prog;
	unsigned int next;
	int status = 0;

	do {
		status = ia_css_cell_program_load_prog(
				ssid, mmid, host_addr, vied_addr, &prog);
		if (status)
			return status;

		next = prog.next;
		host_addr =
		  (ia_css_xmem_address_t)((unsigned long long)host_addr + next);
		vied_addr += next;
	} while (next);

	return status;
}

IA_CSS_CELL_PROGRAM_LOAD_STORAGE_CLASS_C
int
ia_css_cell_program_group_load_multi_entry(
	unsigned int ssid,
	unsigned int mmid,
	ia_css_xmem_address_t host_addr,
	unsigned int vied_addr,
	struct ia_css_cell_program_entry_func_info_s *entry_info,
	unsigned int num_entry_info)
{
	struct ia_css_cell_program_s prog;
	unsigned int next;
	int status = 0;
	unsigned int i = 0;

	do {
		status = ia_css_cell_program_load_prog(
				ssid, mmid, host_addr, vied_addr, &prog);
		if (status)
			return status;
		if (i >= num_entry_info) {
			/* more program than entry info,
			 * cause access out of bound.
			 */
			return 1;
		}
		ia_css_cell_program_load_encode_entry_info(
			&entry_info[i], &prog);

		next = prog.next;
		host_addr =
		  (ia_css_xmem_address_t)((unsigned long long)host_addr + next);
		vied_addr += next;
		i++;
	} while (next);

	return status;
}

IA_CSS_CELL_PROGRAM_LOAD_STORAGE_CLASS_C
int
ia_css_cell_program_group_load_mem(
	unsigned int ssid,
	unsigned int mmid,
	ia_css_xmem_address_t host_addr,
	unsigned int vied_addr)
{
	struct ia_css_cell_program_s prog;
	unsigned int next;
	int status = 0;

	status = ia_css_cell_program_load_header(mmid, host_addr, &prog);
	if (status)
		return status;

	/* load memories of first program */
	status = ia_css_cell_program_load_mem_prog(
			ssid, mmid, host_addr, vied_addr, &prog);
	if (status)
		return status;

	/* return next from ia_css_cell_program_load_mem_prog? */
	next = prog.next;

	/* load next programs, if any */
	if (next) {
		host_addr =
		  (ia_css_xmem_address_t)((unsigned long long)host_addr + next);
		status = ia_css_cell_program_group_load(
				ssid, mmid, host_addr, vied_addr + next);
		if (status)
			return status;
	}

	return status;
}

#endif /* __IA_CSS_CELL_PROGRAM_GROUP_LOAD_IMPL_H */
