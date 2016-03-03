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

#ifndef _IA_CSS_CELL_PROGRAM_LOAD_PROG_IMPL_H_
#define _IA_CSS_CELL_PROGRAM_LOAD_PROG_IMPL_H_

#include "ia_css_cell_program_load_prog.h"
#include "ia_css_fw_load.h"

IA_CSS_CELL_PROGRAM_LOAD_STORAGE_CLASS_C
int
ia_css_cell_program_load_header(
	unsigned int mmid,
	ia_css_xmem_address_t host_addr,
	struct ia_css_cell_program_s *prog)
{

	/* read the program header from DDR */
	ia_css_fw_load(mmid,
		host_addr,
		prog,
		sizeof(struct ia_css_cell_program_s));

	/* check magic number */
	if (prog->magic_number != IA_CSS_CELL_PROGRAM_MAGIC_NUMBER)
		return -1;

	return 0;
}

#if defined(C_RUN) || defined(HRT_UNSCHED) || defined(HRT_CSIM)
#include "ia_css_cell_program_load_csim.h"
#else
#include "ia_css_cell_program_load_bin.h"
#endif

#endif /* _IA_CSS_CELL_PROGRAM_LOAD_PROG_IMPL_H_ */
