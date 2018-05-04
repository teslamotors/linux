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

#ifndef __IPU_DEVICE_ISP2600_PROPERTIES_IMPL_H
#define __IPU_DEVICE_ISP2600_PROPERTIES_IMPL_H

/* isp2600 definition */

#include "ipu_device_cell_properties_struct.h"

enum ipu_device_isp2600_registers {
	/* control registers */
	IPU_DEVICE_ISP2600_STAT_CTRL      = 0x0,
	IPU_DEVICE_ISP2600_START_PC       = 0x4,

	/* master port registers */
	IPU_DEVICE_ISP2600_ICACHE_BASE    = 0x10,
	IPU_DEVICE_ISP2600_ICACHE_INFO    = 0x14,
	IPU_DEVICE_ISP2600_ICACHE_INFO_OVERRIDE    = 0x18,

	IPU_DEVICE_ISP2600_QMEM_BASE      = 0x1C,

	IPU_DEVICE_ISP2600_CMEM_BASE      = 0x28,

	IPU_DEVICE_ISP2600_XMEM_BASE      = 0x88,
	IPU_DEVICE_ISP2600_XMEM_INFO      = 0x8C,
	IPU_DEVICE_ISP2600_XMEM_INFO_OVERRIDE      = 0x90,

	IPU_DEVICE_ISP2600_XVMEM_BASE     = 0xB8,

	/* debug registers */
	IPU_DEVICE_ISP2600_DEBUG_PC       = 0x130,
	IPU_DEVICE_ISP2600_STALL          = 0x134
};


enum ipu_device_isp2600_memories {
	IPU_DEVICE_ISP2600_REGS,
	IPU_DEVICE_ISP2600_PMEM,
	IPU_DEVICE_ISP2600_DMEM,
	IPU_DEVICE_ISP2600_BAMEM,
	IPU_DEVICE_ISP2600_VMEM,
	IPU_DEVICE_ISP2600_NUM_MEMORIES
};

static const unsigned int
ipu_device_isp2600_mem_size[IPU_DEVICE_ISP2600_NUM_MEMORIES] = {
	0x00140,
	0x14000,
	0x04000,
	0x20000,
	0x20000
};


enum ipu_device_isp2600_masters {
	IPU_DEVICE_ISP2600_ICACHE,
	IPU_DEVICE_ISP2600_QMEM,
	IPU_DEVICE_ISP2600_CMEM,
	IPU_DEVICE_ISP2600_XMEM,
	IPU_DEVICE_ISP2600_XVMEM,
	IPU_DEVICE_ISP2600_NUM_MASTERS
};

static const struct ipu_device_cell_master_properties_s
ipu_device_isp2600_masters[IPU_DEVICE_ISP2600_NUM_MASTERS] = {
	{
		0,
		0xC,
		IPU_DEVICE_ISP2600_ICACHE_BASE,
		IPU_DEVICE_ISP2600_ICACHE_INFO,
		IPU_DEVICE_ISP2600_ICACHE_INFO_OVERRIDE
	},
	{
		0,
		0xC,
		IPU_DEVICE_ISP2600_QMEM_BASE,
		0xFFFFFFFF,
		0xFFFFFFFF
	},
	{
		3,
		0xC,
		IPU_DEVICE_ISP2600_CMEM_BASE,
		0xFFFFFFFF,
		0xFFFFFFFF
	},
	{
		2,
		0xC,
		IPU_DEVICE_ISP2600_XMEM_BASE,
		IPU_DEVICE_ISP2600_XMEM_INFO,
		IPU_DEVICE_ISP2600_XMEM_INFO_OVERRIDE
	},
	{
		3,
		0xC,
		IPU_DEVICE_ISP2600_XVMEM_BASE,
		0xFFFFFFFF,
		0xFFFFFFFF
	}
};

enum ipu_device_isp2600_stall_bits {
	IPU_DEVICE_ISP2600_STALL_ICACHE0,
	IPU_DEVICE_ISP2600_STALL_ICACHE1,
	IPU_DEVICE_ISP2600_STALL_DMEM,
	IPU_DEVICE_ISP2600_STALL_QMEM,
	IPU_DEVICE_ISP2600_STALL_CMEM,
	IPU_DEVICE_ISP2600_STALL_XMEM,
	IPU_DEVICE_ISP2600_STALL_BAMEM,
	IPU_DEVICE_ISP2600_STALL_VMEM,
	IPU_DEVICE_ISP2600_STALL_XVMEM,
	IPU_DEVICE_ISP2600_NUM_STALL_BITS
};

#define IPU_DEVICE_ISP2600_ICACHE_WORD_SIZE 64 /* 512 bits per instruction */
#define IPU_DEVICE_ISP2600_ICACHE_BURST_SIZE 8 /* 8 instructions per burst */

static const struct ipu_device_cell_count_s ipu_device_isp2600_count = {
	IPU_DEVICE_ISP2600_NUM_MEMORIES,
	IPU_DEVICE_ISP2600_NUM_MASTERS,
	IPU_DEVICE_ISP2600_NUM_STALL_BITS,
	IPU_DEVICE_ISP2600_ICACHE_WORD_SIZE *
	IPU_DEVICE_ISP2600_ICACHE_BURST_SIZE
};

static const unsigned int ipu_device_isp2600_reg_offset[/* CELL_NUM_REGS */] = {
	0x0, 0x4, 0x10, 0x130, 0x134
};

static const struct ipu_device_cell_type_properties_s
ipu_device_isp2600_properties = {
	&ipu_device_isp2600_count,
	ipu_device_isp2600_masters,
	ipu_device_isp2600_reg_offset,
	ipu_device_isp2600_mem_size
};

#endif /* __IPU_DEVICE_ISP2600_PROPERTIES_IMPL_H */
