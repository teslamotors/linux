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

#ifndef __IA_CSS_CELL_PROGRAM_LOAD_PROG_H
#define __IA_CSS_CELL_PROGRAM_LOAD_PROG_H

/* basic functions needed to implement all program(group) loads */

#include "ia_css_cell_program_load_storage_class.h"
#include "ia_css_cell_program_struct.h"
#include "ia_css_xmem.h"


IA_CSS_CELL_PROGRAM_LOAD_STORAGE_CLASS_H
void
ia_css_cell_program_load_encode_entry_info(
	struct ia_css_cell_program_entry_func_info_s *entry_info,
	const struct ia_css_cell_program_s *prog);

IA_CSS_CELL_PROGRAM_LOAD_STORAGE_CLASS_H
void
ia_css_cell_program_load_set_start_pc(
	unsigned int ssid,
	const struct ia_css_cell_program_entry_func_info_s *entry_info,
	enum ia_css_cell_program_entry_func_id func_id);

IA_CSS_CELL_PROGRAM_LOAD_STORAGE_CLASS_H
int
ia_css_cell_program_load_header(
	unsigned int mmid,
	ia_css_xmem_address_t host_addr,
	struct ia_css_cell_program_s *prog);

IA_CSS_CELL_PROGRAM_LOAD_STORAGE_CLASS_H
int
ia_css_cell_program_load_icache_prog(
	unsigned int ssid,
	unsigned int mmid,
	ia_css_xmem_address_t host_addr,
	unsigned int vied_addr,
	const struct ia_css_cell_program_s *prog);

IA_CSS_CELL_PROGRAM_LOAD_STORAGE_CLASS_H
int
ia_css_cell_program_load_entry_prog(
	unsigned int ssid,
	unsigned int mmid,
	enum ia_css_cell_program_entry_func_id entry_func_id,
	const struct ia_css_cell_program_s *prog);

IA_CSS_CELL_PROGRAM_LOAD_STORAGE_CLASS_H
int
ia_css_cell_program_load_mem_prog(
	unsigned int ssid,
	unsigned int mmid,
	ia_css_xmem_address_t host_addr,
	unsigned int vied_addr,
	const struct ia_css_cell_program_s *prog);

IA_CSS_CELL_PROGRAM_LOAD_STORAGE_CLASS_H
int
ia_css_cell_program_load_prog(
	unsigned int ssid,
	unsigned int mmid,
	ia_css_xmem_address_t host_addr,
	unsigned int vied_addr,
	struct ia_css_cell_program_s *prog);

#ifdef __INLINE_IA_CSS_CELL_PROGRAM_LOAD__
#include "ia_css_cell_program_load_prog_impl.h"
#endif

#endif /* __IA_CSS_CELL_PROGRAM_LOAD_PROG_H */
