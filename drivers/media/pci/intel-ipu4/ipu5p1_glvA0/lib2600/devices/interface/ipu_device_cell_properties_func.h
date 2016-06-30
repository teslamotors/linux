/**
* INTEL CONFIDENTIAL
*
* Copyright (C) 2014 - 2016 Intel Corporation.
* All Rights Reserved.
*
* The source code contained or described herein and all documents
* related to the source code ("Material") are owned by Intel Corporation
* or licensors. Title to the Material remains with Intel
* Corporation or its licensors. The Material contains trade
* secrets and proprietary and confidential information of Intel or its
* licensors. The Material is protected by worldwide copyright
* and trade secret laws and treaty provisions. No part of the Material may
* be used, copied, reproduced, modified, published, uploaded, posted,
* transmitted, distributed, or disclosed in any way without Intel's prior
* express written permission.
*
* No License under any patent, copyright, trade secret or other intellectual
* property right is granted to or conferred upon you by disclosure or
* delivery of the Materials, either expressly, by implication, inducement,
* estoppel or otherwise. Any license under such intellectual property rights
* must be express and approved by Intel in writing.
*/

#ifndef _IPU_DEVICE_CELL_PROPERTIES_FUNC_H_
#define _IPU_DEVICE_CELL_PROPERTIES_FUNC_H_

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
	return ipu_device_cell_properties[cell_id].type_properties->count->num_memories;
}

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_memory_size(const unsigned int cell_id, const unsigned int mem_id)
{
	assert(cell_id < NUM_CELLS);
	assert(mem_id < ipu_device_cell_num_memories(cell_id));
	return ipu_device_cell_properties[cell_id].type_properties->mem_size[mem_id];
}

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_memory_address(const unsigned int cell_id, const unsigned int mem_id)
{
	assert(cell_id < NUM_CELLS);
	assert(mem_id < ipu_device_cell_num_memories(cell_id));
	return ipu_device_cell_properties[cell_id].mem_address[mem_id];
}

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_databus_memory_address(const unsigned int cell_id, const unsigned int mem_id)
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
	return ipu_device_cell_properties[cell_id].type_properties->count->num_master_ports;
}

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_master_segment_bits(const unsigned int cell_id, const unsigned int master_id)
{
	assert(cell_id < NUM_CELLS);
	assert(master_id < ipu_device_cell_num_masters(cell_id));
	return ipu_device_cell_properties[cell_id].type_properties->master[master_id].segment_bits;
}

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_master_num_segments(const unsigned int cell_id, const unsigned int master_id)
{
	return 1u << ipu_device_cell_master_segment_bits(cell_id, master_id);
}

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_master_segment_size(const unsigned int cell_id, const unsigned int master_id)
{
	return 1u << (IA_CSS_CELL_MASTER_ADDRESS_WIDTH - ipu_device_cell_master_segment_bits(cell_id, master_id));
}

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_master_stride(const unsigned int cell_id, const unsigned int master_id)
{
	assert(cell_id < NUM_CELLS);
	assert(master_id < ipu_device_cell_num_masters(cell_id));
	return
		ipu_device_cell_properties[cell_id].type_properties->master[master_id].stride;
}

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_master_base_reg(const unsigned int cell_id, const unsigned int master_id)
{
	assert(cell_id < NUM_CELLS);
	assert(master_id < ipu_device_cell_num_masters(cell_id));
	return
		ipu_device_cell_properties[cell_id].type_properties->master[master_id].base_address_register;
}

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_master_info_reg(const unsigned int cell_id, const unsigned int master_id)
{
	assert(cell_id < NUM_CELLS);
	assert(master_id < ipu_device_cell_num_masters(cell_id));
	return
		ipu_device_cell_properties[cell_id].type_properties->master[master_id].info_bits_register;
}

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_master_info_override_reg(const unsigned int cell_id, const unsigned int master_id)
{
	assert(cell_id < NUM_CELLS);
	assert(master_id < ipu_device_cell_num_masters(cell_id));
	return
		ipu_device_cell_properties[cell_id].type_properties->master[master_id].info_override_bits_register;
}

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_icache_align(unsigned int cell_id)
{
	assert(cell_id < NUM_CELLS);
	return ipu_device_cell_properties[cell_id].type_properties->count->icache_align;
}

#ifdef C_RUN
STORAGE_CLASS_INLINE int
ipu_device_cell_id_crun(int cell_id)
{
	assert(cell_id < NUM_CELLS);
	return ipu_device_map_cell_id_to_crun_proc_id[cell_id];
}
#endif

#endif /* _IPU_DEVICE_CELL_PROPERTIES_FUNC_H_ */
