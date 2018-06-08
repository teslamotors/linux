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

#ifndef __IA_CSS_CELL_PROGRAM_LOAD_BIN_H
#define __IA_CSS_CELL_PROGRAM_LOAD_BIN_H

#include "ia_css_cell_program_load_prog.h"

#include "ia_css_cell_program_load_storage_class.h"
#include "ia_css_cell_program_struct.h"
#include "ia_css_cell_regs.h"
#include "misc_support.h"
#include "ia_css_fw_load.h"
#include "platform_support.h"
#include "ipu_device_buttress_properties_struct.h"

IA_CSS_CELL_PROGRAM_LOAD_STORAGE_CLASS_C
void
ia_css_cell_program_load_encode_entry_info(
	struct ia_css_cell_program_entry_func_info_s *entry_info,
	const struct ia_css_cell_program_s *prog)
{
	unsigned int i;

	for (i = 0; i < IA_CSS_CELL_PROGRAM_NUM_FUNC_ID; i++)
		entry_info->start[i] = prog->start[i];

	entry_info->regs_addr = prog->regs_addr;
}

IA_CSS_CELL_PROGRAM_LOAD_STORAGE_CLASS_C
void
ia_css_cell_program_load_set_start_pc(
	unsigned int ssid,
	const struct ia_css_cell_program_entry_func_info_s *entry_info,
	enum ia_css_cell_program_entry_func_id func_id)
{
	unsigned int start_pc;

	start_pc = entry_info->start[func_id];
	/* set start address */
	ia_css_cell_regs_set_start_pc(ssid, entry_info->regs_addr, start_pc);
}

IA_CSS_CELL_PROGRAM_LOAD_STORAGE_CLASS_C
int
ia_css_cell_program_load_icache_prog(
	unsigned int ssid,
	unsigned int mmid,
	ia_css_xmem_address_t host_addr,
	unsigned int vied_addr,
	const struct ia_css_cell_program_s *prog)
{
	unsigned int regs_addr;
	struct ia_css_cell_program_entry_func_info_s entry_info;

	NOT_USED(mmid);
	NOT_USED(host_addr);

	if (prog->cell_id == IA_CSS_CELL_ID_UNDEFINED)
		return -1;

	regs_addr = prog->regs_addr;

	/* set icache base address */
	ia_css_cell_regs_set_icache_base_address(ssid, regs_addr,
		vied_addr + prog->blob_offset + prog->icache_source);

	/* set icache info bits */
	ia_css_cell_regs_set_icache_info_bits(
		ssid, regs_addr, IA_CSS_INFO_BITS_M0_DDR);

	/* by default we set to start PC of exec entry function */
	ia_css_cell_program_load_encode_entry_info(&entry_info, prog);
	ia_css_cell_program_load_set_start_pc(
		ssid, &entry_info, IA_CSS_CELL_PROGRAM_EXEC_FUNC_ID);

	return 0;
}

IA_CSS_CELL_PROGRAM_LOAD_STORAGE_CLASS_C
int
ia_css_cell_program_load_entry_prog(
	unsigned int ssid,
	unsigned int mmid,
	enum ia_css_cell_program_entry_func_id entry_func_id,
	const struct ia_css_cell_program_s *prog)
{
	struct ia_css_cell_program_entry_func_info_s entry_info;

	NOT_USED(mmid);

	if (prog->cell_id == IA_CSS_CELL_ID_UNDEFINED)
		return -1;

	ia_css_cell_program_load_encode_entry_info(&entry_info, prog);
	ia_css_cell_program_load_set_start_pc(ssid, &entry_info, entry_func_id);

	return 0;
}

IA_CSS_CELL_PROGRAM_LOAD_STORAGE_CLASS_C int
ia_css_cell_program_load_mem_prog(
	unsigned int ssid,
	unsigned int mmid,
	ia_css_xmem_address_t host_addr,
	unsigned int vied_addr,
	const struct ia_css_cell_program_s *prog)
{
	unsigned int transferred = 0;
	unsigned int pending = 0;
	unsigned int dmem_addr;
	unsigned int pmem_addr;

	NOT_USED(vied_addr);

#ifdef ENABLE_FW_LOAD_DMA
	pmem_addr = prog->cell_pmem_data_bus_addres;
	dmem_addr = prog->cell_dmem_data_bus_addres;
#else
	pmem_addr = prog->cell_pmem_control_bus_addres;
	dmem_addr = prog->cell_dmem_control_bus_addres;
#endif

	/* Copy text section from ddr to pmem. */
	if (prog->pmem_size) {
		transferred = ia_css_fw_copy_begin(mmid,
				ssid,
				host_addr + prog->blob_offset +
				prog->pmem_source,
				pmem_addr + prog->pmem_target,
				prog->pmem_size);

		assert(prog->pmem_size == transferred);
		/* If less bytes are transferred that requested, signal error,
		 * This architecture enforces DMA xfer size > pmem_size.
		 * So, a DMA transfer request should be xferable*/
		if (transferred != prog->pmem_size)
			return 1;
		pending++;
	}

	/* Copy data section from ddr to dmem. */
	if (prog->data_size) {
		transferred = ia_css_fw_copy_begin(mmid,
				ssid,
				host_addr + prog->blob_offset +
				prog->data_source,
				dmem_addr + prog->data_target,
				prog->data_size);
		assert(prog->data_size == transferred);
		/* If less bytes are transferred that requested, signal error,
		 * This architecture enforces DMA xfer size > data_size.
		 * So, a DMA transfer request should be xferable*/
		if (transferred != prog->data_size)
			return 1; /*FALSE*/
		pending++;
	}

	/* Zero bss section in dmem.*/
	if (prog->bss_size) {
		transferred = ia_css_fw_zero_begin(ssid,
					dmem_addr + prog->bss_target,
					prog->bss_size);
		assert(prog->bss_size == transferred);
		/* If less bytes are transferred that requested, signal error,
		 * This architecture enforces DMA xfer size > bss_size.
		 * So, a DMA transfer request should be xferable*/
		if (transferred != prog->bss_size)
			return 1;
		pending++;
	}

	/* Wait for all fw load to complete */
	while (pending) {
		pending -= ia_css_fw_end(pending);
		ia_css_sleep();
	}
	return 0; /*Success*/
}

#endif /* __IA_CSS_CELL_PROGRAM_LOAD_BIN_H */
