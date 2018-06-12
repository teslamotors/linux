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

#ifndef __IA_CSS_CELL_PROGRAM_LOAD_H
#define __IA_CSS_CELL_PROGRAM_LOAD_H

#include "ia_css_cell_program_load_storage_class.h"
#include "ia_css_cell_program_struct.h"
#include "ia_css_xmem.h"

/* Perform full program load:
 * - load program header
 * - initialize icache and start PC of exec entry function
 * - initialize PMEM and DMEM
 * Return 0 on success, -1 on incorrect magic number,
 *			-2 on incorrect release tag
 */

IA_CSS_CELL_PROGRAM_LOAD_STORAGE_CLASS_H
int
ia_css_cell_program_load(
	unsigned int ssid,
	unsigned int mmid,
	/* program address as seen from caller */
	ia_css_xmem_address_t program_addr,
	/* program address as seen from cell's icache */
	unsigned int program_addr_icache
);

/* Perform full program load:
 * - load program header
 * - initialize icache and start PC of exec entry function
 * - initialize info of all entry function
 * - initialize PMEM and DMEM
 * Return 0 on success, -1 on incorrect magic number,
 *			-2 on incorrect release tag
 */

IA_CSS_CELL_PROGRAM_LOAD_STORAGE_CLASS_H
int
ia_css_cell_program_load_multi_entry(
	unsigned int ssid,
	unsigned int mmid,
	/* program address as seen from caller */
	ia_css_xmem_address_t program_addr,
	/* program address as seen from cell's icache */
	unsigned int program_addr_icache,
	struct ia_css_cell_program_entry_func_info_s *entry_info
);

/* Load program header, and initialize icache and start PC.
 * After this, the cell may be started, but the entry function may not yet use
 * global data, nor may code from PMEM be executed.
 * Before accessing global data or executing code from PMEM
 * the function ia_css_cell_load_program_mem must be executed.
 */

IA_CSS_CELL_PROGRAM_LOAD_STORAGE_CLASS_H
int
ia_css_cell_program_load_icache(
	unsigned int ssid,
	unsigned int mmid,
	ia_css_xmem_address_t program_addr,
	unsigned int   program_addr_icache);

/* Load program header and finish the program load by
 * initializing PMEM and DMEM.
 * After this any code from the program may be be executed on the cell.
 */
IA_CSS_CELL_PROGRAM_LOAD_STORAGE_CLASS_H
int
ia_css_cell_program_load_mem(
	unsigned int ssid,
	unsigned int mmid,
	ia_css_xmem_address_t program_addr,
	unsigned int   program_addr_icache);

/* set cell start PC to program init entry function */
IA_CSS_CELL_PROGRAM_LOAD_STORAGE_CLASS_H
void
ia_css_cell_program_load_set_init_start_pc(
	unsigned int ssid,
	const struct ia_css_cell_program_entry_func_info_s *entry_info);

/* set cell start PC to program exec entry function */
IA_CSS_CELL_PROGRAM_LOAD_STORAGE_CLASS_H
void
ia_css_cell_program_load_set_exec_start_pc(
	unsigned int ssid,
	const struct ia_css_cell_program_entry_func_info_s *entry_info);

/* set cell start PC to program done entry function */
IA_CSS_CELL_PROGRAM_LOAD_STORAGE_CLASS_H
void
ia_css_cell_program_load_set_done_start_pc(
	unsigned int ssid,
	const struct ia_css_cell_program_entry_func_info_s *entry_info);

#ifdef __INLINE_IA_CSS_CELL_PROGRAM_LOAD__
#include "ia_css_cell_program_load_impl.h"
#endif

#endif /* __IA_CSS_CELL_PROGRAM_LOAD_H */
