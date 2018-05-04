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

#ifndef __IPU_DEVICE_CELL_PROPERTIES_STRUCT_H
#define __IPU_DEVICE_CELL_PROPERTIES_STRUCT_H

/* definitions for all cell types */

struct ipu_device_cell_count_s {
	unsigned int num_memories;
	unsigned int num_master_ports;
	unsigned int num_stall_bits;
	unsigned int icache_align;
};

struct ipu_device_cell_master_properties_s {
	unsigned int segment_bits;
	unsigned int stride; /* offset to register of next segment */
	unsigned int base_address_register; /* address of first base address
					       register */
	unsigned int info_bits_register;
	unsigned int info_override_bits_register;
};

struct ipu_device_cell_type_properties_s {
	const struct ipu_device_cell_count_s *count;
	const struct ipu_device_cell_master_properties_s *master;
	const unsigned int *reg_offset; /* offsets of registers, some depend
					   on cell type */
	const unsigned int *mem_size;
};

struct ipu_device_cell_properties_s {
	const struct ipu_device_cell_type_properties_s *type_properties;
	const unsigned int *mem_address;
	const unsigned int *mem_databus_address;
	/* const cell_master_port_properties_s* master_port_properties; */
};

#endif /* __IPU_DEVICE_CELL_PROPERTIES_STRUCT_H */
