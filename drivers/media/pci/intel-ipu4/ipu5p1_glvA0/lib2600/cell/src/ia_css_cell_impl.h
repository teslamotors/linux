/**
* INTEL CONFIDENTIAL
*
* Copyright (C) 2015 - 2016 Intel Corporation.
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

#ifndef _IA_CSS_CELL_IMPL_H_
#define _IA_CSS_CELL_IMPL_H_

#include "ia_css_cell.h"

#include "ia_css_cmem.h"
#include "ipu_device_cell_properties.h"
#include "storage_class.h"
#include "assert_support.h"
#include "platform_support.h"
#include "misc_support.h"

STORAGE_CLASS_INLINE unsigned int
ia_css_cell_regs_addr(unsigned int cell_id)
{
	return ipu_device_cell_memory_address(cell_id, 0); /* mem_id 0 is for registers */
}

STORAGE_CLASS_INLINE unsigned int
ia_css_cell_dmem_addr(unsigned int cell_id)
{
	return ipu_device_cell_memory_address(cell_id, 1); /* mem_id 1 is for DMEM */
}

STORAGE_CLASS_INLINE void
ia_css_cell_mem_store_32(unsigned int ssid, unsigned int cell_id,
	unsigned int mem_id, unsigned int addr, unsigned int value)
{
	ia_css_cmem_store_32(ssid, ipu_device_cell_memory_address(cell_id, mem_id) + addr, value);
}

STORAGE_CLASS_INLINE unsigned int
ia_css_cell_mem_load_32(unsigned int ssid, unsigned int cell_id, unsigned int mem_id, unsigned int addr)
{
	return ia_css_cmem_load_32(ssid, ipu_device_cell_memory_address(cell_id, mem_id) + addr);
}

STORAGE_CLASS_INLINE unsigned int
ia_css_cell_get_stat_ctrl(unsigned int ssid, unsigned int cell_id)
{
	return ia_css_cmem_load_32(ssid, ia_css_cell_regs_addr(cell_id) + IPU_DEVICE_CELL_STAT_CTRL_REG_ADDRESS);
}

STORAGE_CLASS_INLINE void
ia_css_cell_set_stat_ctrl(unsigned int ssid, unsigned int cell_id, unsigned int value)
{
	ia_css_cmem_store_32(ssid, ia_css_cell_regs_addr(cell_id) + IPU_DEVICE_CELL_STAT_CTRL_REG_ADDRESS, value);
}

STORAGE_CLASS_INLINE unsigned int
ia_css_cell_is_ready(unsigned int ssid, unsigned int cell_id)
{
	unsigned int reg;

	reg = ia_css_cell_get_stat_ctrl(ssid, cell_id);
	return (reg & (1 << IPU_DEVICE_CELL_STAT_CTRL_READY_BIT)) &&	/* READY must be 1 */
		((~reg) & (1 << IPU_DEVICE_CELL_STAT_CTRL_START_BIT));	/* START must be 0 */
}

STORAGE_CLASS_INLINE void
ia_css_cell_set_start_pc(unsigned int ssid, unsigned int cell_id, unsigned int pc)
{
	/* set start PC */
	ia_css_cmem_store_32(ssid, ia_css_cell_regs_addr(cell_id) + IPU_DEVICE_CELL_START_PC_REG_ADDRESS, pc);
}

STORAGE_CLASS_INLINE void
ia_css_cell_set_start_bit(unsigned int ssid, unsigned int cell_id)
{
	unsigned int reg;

	reg = 1 << IPU_DEVICE_CELL_STAT_CTRL_START_BIT;
	ia_css_cell_set_stat_ctrl(ssid, cell_id, reg);
}

STORAGE_CLASS_INLINE void
ia_css_cell_set_run_bit(unsigned int ssid, unsigned int cell_id, unsigned int value)
{
	unsigned int reg;

	reg = value << IPU_DEVICE_CELL_STAT_CTRL_RUN_BIT;
	ia_css_cell_set_stat_ctrl(ssid, cell_id, reg);
}

STORAGE_CLASS_INLINE void
ia_css_cell_start(unsigned int ssid, unsigned int cell_id)
{
	ia_css_cell_start_prefetch(ssid, cell_id, 0);
}

STORAGE_CLASS_INLINE void
ia_css_cell_start_prefetch(unsigned int ssid, unsigned int cell_id, bool prefetch)
{
	unsigned int reg = 0;

	/* Set run bit and start bit */
	reg |= (1 << IPU_DEVICE_CELL_STAT_CTRL_START_BIT);
	reg |= (1 << IPU_DEVICE_CELL_STAT_CTRL_RUN_BIT);
	/* Invalidate the icache */
	reg |= (1 << IPU_DEVICE_CELL_STAT_CTRL_INVALIDATE_ICACHE_BIT);
	/* Optionally enable prefetching */
	reg |= ((prefetch == 1) ? (1 << IPU_DEVICE_CELL_STAT_CTRL_ICACHE_ENABLE_PREFETCH_BIT) : 0);

	/* store into register */
	ia_css_cell_set_stat_ctrl(ssid, cell_id, reg);
}

STORAGE_CLASS_INLINE void
ia_css_cell_wait(unsigned int ssid, unsigned int cell_id)
{
	do {
		ia_css_sleep();
	} while (!ia_css_cell_is_ready(ssid, cell_id));
};

STORAGE_CLASS_INLINE void
ia_css_cell_set_icache_base_address(unsigned int ssid, unsigned int cell_id, unsigned int value)
{
	ia_css_cmem_store_32(ssid, ia_css_cell_regs_addr(cell_id) + IPU_DEVICE_CELL_ICACHE_BASE_REG_ADDRESS, value);
}

/* master port configuration */


STORAGE_CLASS_INLINE void
ia_css_cell_set_master_segment_info_bits(unsigned int ssid, unsigned int cell,
	unsigned int master, unsigned int segment, unsigned int value)
{
	unsigned int addr;

	assert(cell < ipu_device_cell_num_devices());
	assert(master < ipu_device_cell_num_masters(cell));
	assert(segment < ipu_device_cell_master_num_segments(cell, master));

	addr = ipu_device_cell_memory_address(cell, 0);
	addr += ipu_device_cell_master_info_reg(cell, master);
	addr += segment * ipu_device_cell_master_stride(cell, master);
	ia_css_cmem_store_32(ssid, addr, value);
}

STORAGE_CLASS_INLINE void
ia_css_cell_set_master_segment_info_override_bits(unsigned int ssid, unsigned int cell,
	unsigned int master, unsigned int segment, unsigned int value)
{
	unsigned int addr;

	assert(cell < ipu_device_cell_num_devices());
	assert(master < ipu_device_cell_num_masters(cell));
	assert(segment < ipu_device_cell_master_num_segments(cell, master));

	addr = ipu_device_cell_memory_address(cell, 0);
	addr += ipu_device_cell_master_info_override_reg(cell, master);
	addr += segment * ipu_device_cell_master_stride(cell, master);
	ia_css_cmem_store_32(ssid, addr, value);
}

STORAGE_CLASS_INLINE void
ia_css_cell_set_master_segment_base_address(unsigned int ssid, unsigned int cell,
	unsigned int master, unsigned int segment, unsigned int value)

{
	unsigned int addr;

	assert(cell < ipu_device_cell_num_devices());
	assert(master < ipu_device_cell_num_masters(cell));
	assert(segment < ipu_device_cell_master_num_segments(cell, master));

	addr = ipu_device_cell_memory_address(cell, 0);
	addr += ipu_device_cell_master_base_reg(cell, master);
	addr += segment * ipu_device_cell_master_stride(cell, master);
	ia_css_cmem_store_32(ssid, addr, value);
}

STORAGE_CLASS_INLINE void
ia_css_cell_set_master_info_bits(unsigned int ssid, unsigned int cell,
	unsigned int master, unsigned int value)
{
	unsigned int addr, s, stride, num_segments;

	assert(cell < ipu_device_cell_num_devices());
	assert(master < ipu_device_cell_num_masters(cell));

	addr = ipu_device_cell_memory_address(cell, 0);
	addr += ipu_device_cell_master_info_reg(cell, master);
	stride = ipu_device_cell_master_stride(cell, master);
	num_segments = ipu_device_cell_master_num_segments(cell, master);
	for (s = 0; s < num_segments; s++) {
		ia_css_cmem_store_32(ssid, addr, value);
		addr += stride;
	}
}

STORAGE_CLASS_INLINE void
ia_css_cell_set_master_info_override_bits(unsigned int ssid, unsigned int cell,
	unsigned int master, unsigned int value)
{
	unsigned int addr, s, stride, num_segments;

	assert(cell < ipu_device_cell_num_devices());
	assert(master < ipu_device_cell_num_masters(cell));

	addr = ipu_device_cell_memory_address(cell, 0);
	addr += ipu_device_cell_master_info_override_reg(cell, master);
	stride = ipu_device_cell_master_stride(cell, master);
	num_segments = ipu_device_cell_master_num_segments(cell, master);
	for (s = 0; s < num_segments; s++) {
		ia_css_cmem_store_32(ssid, addr, value);
		addr += stride;
	}
}

STORAGE_CLASS_INLINE void
ia_css_cell_set_master_base_address(unsigned int ssid, unsigned int cell,
	unsigned int master, unsigned int value)
{
	unsigned int addr, s, stride, num_segments, segment_size;

	assert(cell < ipu_device_cell_num_devices());
	assert(master < ipu_device_cell_num_masters(cell));

	addr  = ipu_device_cell_memory_address(cell, 0);
	addr += ipu_device_cell_master_base_reg(cell, master);
	stride = ipu_device_cell_master_stride(cell, master);
	num_segments = ipu_device_cell_master_num_segments(cell, master);
	segment_size = ipu_device_cell_master_segment_size(cell, master);

	for (s = 0; s < num_segments; s++) {
		ia_css_cmem_store_32(ssid, addr, value);
		addr += stride;
		value += segment_size;
	}
}

#endif
