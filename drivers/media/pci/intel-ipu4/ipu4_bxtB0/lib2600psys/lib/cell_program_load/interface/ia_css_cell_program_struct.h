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

#ifndef _IA_CSS_CELL_PROGRAM_STRUCT_
#define _IA_CSS_CELL_PROGRAM_STRUCT_

#define IA_CSS_CELL_ID_UNDEFINED		0xFFFFFFFF
#define IA_CSS_CELL_PROGRAM_MAGIC_NUMBER	0xF1A30002
#define IA_CSS_FW_RELEASE_SIZE			64

struct ia_css_cell_program_s {

	char release[IA_CSS_FW_RELEASE_SIZE];
	unsigned int magic_number;      /* must be equal to IA_CSS_CELL_PROGRAM_MAGIC_NUMBER */

	unsigned int blob_offset;	/* offset of blob relative to start of this struct */
	unsigned int blob_size;		/* size of the blob, not used */

	unsigned int start;		/* start PC value */

	unsigned int icache_source;	/* offset of icache section in blob */
	unsigned int icache_target;	/* offset in the instruction space, not used */
	unsigned int icache_size;	/* icache section size, not used */

	unsigned int pmem_source;	/* offset of pmem section in blob */
	unsigned int pmem_target;	/* offset in the pmem, typically 0 */
	unsigned int pmem_size;		/* pmem section size, 0 if not used */

	unsigned int data_source;	/* offset of data section in blob */
	unsigned int data_target;	/* offset of data section in dmem */
	unsigned int data_size;		/* size of dmem data section */

	unsigned int bss_target;	/* offset of bss section in dmem, to be zeroed */
	unsigned int bss_size;		/* size of bss section in dmem */

	unsigned int cell_id;		/* for checking */
	unsigned int regs_addr;		/* base address for cell's registers */

	unsigned int cell_pmem_data_bus_addres;		/* pmem data bus address */
	unsigned int cell_dmem_data_bus_addres;		/* dmem data bus address */
	unsigned int cell_pmem_control_bus_addres;	/* pmem config bus address */
	unsigned int cell_dmem_control_bus_addres;	/* dmem config bus address */

	unsigned int next;		/* offset to header of next program */
	unsigned int dummy[2];		/* Temporary workaround for a dma bug where it fails to trasfer
								data with size which is not multiple of 64 bytes */
};

#endif /* _IA_CSS_CELL_PROGRAM_STRUCT_H_ */
