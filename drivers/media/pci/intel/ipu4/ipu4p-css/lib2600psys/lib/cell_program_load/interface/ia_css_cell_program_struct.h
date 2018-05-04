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

#ifndef __IA_CSS_CELL_PROGRAM_STRUCT_H
#define __IA_CSS_CELL_PROGRAM_STRUCT_H

#define IA_CSS_CELL_ID_UNDEFINED		0xFFFFFFFF
#define IA_CSS_CELL_PROGRAM_MAGIC_NUMBER	0xF1A30002

#define CSIM_PROGRAM_NAME_SIZE 64

enum ia_css_cell_program_entry_func_id {
	IA_CSS_CELL_PROGRAM_INIT_FUNC_ID,
	IA_CSS_CELL_PROGRAM_EXEC_FUNC_ID,
	IA_CSS_CELL_PROGRAM_DONE_FUNC_ID,
	IA_CSS_CELL_PROGRAM_NUM_FUNC_ID,
};

struct ia_css_cell_program_entry_func_info_s {
	/* start PC value of program entry functions */
	unsigned int start[IA_CSS_CELL_PROGRAM_NUM_FUNC_ID];

#if defined(C_RUN)
	/* entry function names */
	char func_name[IA_CSS_CELL_PROGRAM_NUM_FUNC_ID][CSIM_PROGRAM_NAME_SIZE];
	/* for crun use only */
	unsigned int cell_id;
#endif
	/* base address for cell's registers */
	unsigned int regs_addr;

};

struct ia_css_cell_program_s {
	/* must be equal to IA_CSS_CELL_PROGRAM_MAGIC_NUMBER */
	unsigned int magic_number;

	/* offset of blob relative to start of this struct */
	unsigned int blob_offset;
	/* size of the blob, not used */
	unsigned int blob_size;

	/* start PC value of program entry functions */
	unsigned int start[IA_CSS_CELL_PROGRAM_NUM_FUNC_ID];

#if defined(C_RUN) || defined(HRT_UNSCHED) || defined(HRT_SCHED)
	/* program name */
	char prog_name[CSIM_PROGRAM_NAME_SIZE];
#if defined(C_RUN)
	/* entry function names */
	char func_name[IA_CSS_CELL_PROGRAM_NUM_FUNC_ID][CSIM_PROGRAM_NAME_SIZE];
#endif
#endif

	/* offset of icache section in blob */
	unsigned int icache_source;
	/* offset in the instruction space, not used */
	unsigned int icache_target;
	/* icache section size, not used */
	unsigned int icache_size;

	/* offset of pmem section in blob */
	unsigned int pmem_source;
	/* offset in the pmem, typically 0 */
	unsigned int pmem_target;
	/* pmem section size, 0 if not used */
	unsigned int pmem_size;

	/* offset of data section in blob */
	unsigned int data_source;
	/* offset of data section in dmem */
	unsigned int data_target;
	/* size of dmem data section */
	unsigned int data_size;

	/* offset of bss section in dmem, to be zeroed */
	unsigned int bss_target;
	/* size of bss section in dmem */
	unsigned int bss_size;

	/* for checking */
	unsigned int cell_id;
	/* base address for cell's registers */
	unsigned int regs_addr;

	/* pmem data bus address */
	unsigned int cell_pmem_data_bus_addres;
	/* dmem data bus address */
	unsigned int cell_dmem_data_bus_addres;
	/* pmem config bus address */
	unsigned int cell_pmem_control_bus_addres;
	/* dmem config bus address */
	unsigned int cell_dmem_control_bus_addres;

	/* offset to header of next program */
	unsigned int next;
	/* Temporary workaround for a dma bug where it fails to trasfer
	 * data with size which is not multiple of 64 bytes
	 */
	unsigned int dummy[2];
};

#endif /* __IA_CSS_CELL_PROGRAM_STRUCT_H */
