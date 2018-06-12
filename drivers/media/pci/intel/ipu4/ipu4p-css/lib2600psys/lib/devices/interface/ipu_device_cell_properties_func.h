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

#ifndef __IPU_DEVICE_CELL_PROPERTIES_FUNC_H
#define __IPU_DEVICE_CELL_PROPERTIES_FUNC_H

/* define properties for all cells uses in ISYS */

#include "ipu_device_cell_properties_impl.h"
#include "ipu_device_cell_devices.h"
#include "assert_support.h"
#include "storage_class.h"

enum {IA_CSS_CELL_MASTER_ADDRESS_WIDTH = 32};

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_num_devices(void)
{
	return NUM_CELLS;
}

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_num_memories(const unsigned int cell_id)
{
	assert(cell_id < NUM_CELLS);
	return ipu_device_cell_properties[cell_id].type_properties->count->
		num_memories;
}

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_memory_size(const unsigned int cell_id,
			    const unsigned int mem_id)
{
	assert(cell_id < NUM_CELLS);
	assert(mem_id < ipu_device_cell_num_memories(cell_id));
	return ipu_device_cell_properties[cell_id].type_properties->
		mem_size[mem_id];
}

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_memory_address(const unsigned int cell_id,
				const unsigned int mem_id)
{
	assert(cell_id < NUM_CELLS);
	assert(mem_id < ipu_device_cell_num_memories(cell_id));
	return ipu_device_cell_properties[cell_id].mem_address[mem_id];
}

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_databus_memory_address(const unsigned int cell_id,
				       const unsigned int mem_id)
{
	assert(cell_id < NUM_CELLS);
	assert(mem_id < ipu_device_cell_num_memories(cell_id));
	assert(mem_id != 0);
	return ipu_device_cell_properties[cell_id].mem_databus_address[mem_id];
}

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_num_masters(const unsigned int cell_id)
{
	assert(cell_id < NUM_CELLS);
	return ipu_device_cell_properties[cell_id].type_properties->count->
		num_master_ports;
}

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_master_segment_bits(const unsigned int cell_id,
				    const unsigned int master_id)
{
	assert(cell_id < NUM_CELLS);
	assert(master_id < ipu_device_cell_num_masters(cell_id));
	return ipu_device_cell_properties[cell_id].type_properties->
		master[master_id].segment_bits;
}

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_master_num_segments(const unsigned int cell_id,
				    const unsigned int master_id)
{
	return 1u << ipu_device_cell_master_segment_bits(cell_id, master_id);
}

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_master_segment_size(const unsigned int cell_id,
				    const unsigned int master_id)
{
	return 1u << (IA_CSS_CELL_MASTER_ADDRESS_WIDTH -
		      ipu_device_cell_master_segment_bits(cell_id, master_id));
}

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_master_stride(const unsigned int cell_id,
			      const unsigned int master_id)
{
	assert(cell_id < NUM_CELLS);
	assert(master_id < ipu_device_cell_num_masters(cell_id));
	return
		ipu_device_cell_properties[cell_id].type_properties->
			master[master_id].stride;
}

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_master_base_reg(const unsigned int cell_id,
				const unsigned int master_id)
{
	assert(cell_id < NUM_CELLS);
	assert(master_id < ipu_device_cell_num_masters(cell_id));
	return
		ipu_device_cell_properties[cell_id].type_properties->
			master[master_id].base_address_register;
}

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_master_info_reg(const unsigned int cell_id,
				const unsigned int master_id)
{
	assert(cell_id < NUM_CELLS);
	assert(master_id < ipu_device_cell_num_masters(cell_id));
	return
		ipu_device_cell_properties[cell_id].type_properties->
			master[master_id].info_bits_register;
}

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_master_info_override_reg(const unsigned int cell_id,
					const unsigned int master_id)
{
	assert(cell_id < NUM_CELLS);
	assert(master_id < ipu_device_cell_num_masters(cell_id));
	return
		ipu_device_cell_properties[cell_id].type_properties->
			master[master_id].info_override_bits_register;
}

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_icache_align(unsigned int cell_id)
{
	assert(cell_id < NUM_CELLS);
	return ipu_device_cell_properties[cell_id].type_properties->count->
		icache_align;
}

#ifdef C_RUN
STORAGE_CLASS_INLINE int
ipu_device_cell_id_crun(int cell_id)
{
	assert(cell_id < NUM_CELLS);
	return ipu_device_map_cell_id_to_crun_proc_id[cell_id];
}
#endif

#endif /* __IPU_DEVICE_CELL_PROPERTIES_FUNC_H */
