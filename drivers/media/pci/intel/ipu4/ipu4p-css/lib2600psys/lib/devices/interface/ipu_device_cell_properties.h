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

#ifndef __IPU_DEVICE_CELL_PROPERTIES_H
#define __IPU_DEVICE_CELL_PROPERTIES_H

#include "storage_class.h"
#include "ipu_device_cell_type_properties.h"

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_num_devices(void);

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_num_memories(const unsigned int cell_id);

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_memory_size(const unsigned int cell_id,
			    const unsigned int mem_id);

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_memory_address(const unsigned int cell_id,
			       const unsigned int mem_id);

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_databus_memory_address(const unsigned int cell_id,
				       const unsigned int mem_id);

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_num_masters(const unsigned int cell_id);

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_master_segment_bits(const unsigned int cell_id,
				    const unsigned int master_id);

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_master_num_segments(const unsigned int cell_id,
				    const unsigned int master_id);

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_master_segment_size(const unsigned int cell_id,
				    const unsigned int master_id);

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_master_stride(const unsigned int cell_id,
			      const unsigned int master_id);

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_master_base_reg(const unsigned int cell_id,
				const unsigned int master_id);

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_master_info_reg(const unsigned int cell_id,
				const unsigned int master_id);

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_icache_align(unsigned int cell_id);

#ifdef C_RUN
STORAGE_CLASS_INLINE int
ipu_device_cell_id_crun(int cell_id);
#endif

#include "ipu_device_cell_properties_func.h"

#endif /* __IPU_DEVICE_CELL_PROPERTIES_H */
