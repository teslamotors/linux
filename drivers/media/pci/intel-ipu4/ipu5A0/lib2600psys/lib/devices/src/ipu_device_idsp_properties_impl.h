/**
* Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2016, Intel Corporation.
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

#ifndef _IPU_DEVICE_IDSP_PROPERTIES_IMPL_H_
#define _IPU_DEVICE_IDSP_PROPERTIES_IMPL_H_

/* idsp definition */

#include "ipu_device_cell_type_properties.h"
#include "ipu_device_cell_properties_struct.h"

enum ipu_device_idsp_registers {
	/* control registers */
	IPU_DEVICE_IDSP_STAT_CTRL      = 0x0,
	IPU_DEVICE_IDSP_START_PC       = 0x4, /* TODO: Verified from idsp_manual. Find the right define */

	/* master port registers */
	IPU_DEVICE_IDSP_ICACHE_BASE    = 0x10,
	IPU_DEVICE_IDSP_ICACHE_INFO    = 0x14,
	IPU_DEVICE_IDSP_ICACHE_INFO_OVERRIDE    = 0x18,

	IPU_DEVICE_IDSP_QMEM_BASE      = 0x1C,

	IPU_DEVICE_IDSP_CMEM_BASE      = 0x28,

	IPU_DEVICE_IDSP_XMEM_BASE      = 0x88,
	IPU_DEVICE_IDSP_XMEM_INFO      = 0x8C,
	IPU_DEVICE_IDSP_XMEM_INFO_OVERRIDE      = 0x90,

	IPU_DEVICE_IDSP_XVMEM_BASE     = 0xB8,

	/* debug registers */
	IPU_DEVICE_IDSP_DEBUG_PC       = 0x130,
	IPU_DEVICE_IDSP_STALL          = 0x134
};

#define IPU_DEVICE_IDSP_NUM_MEMORIES IPU_DEVICE_CELL_NUM_MEMORIES

static const unsigned int
ipu_device_idsp_mem_size[IPU_DEVICE_IDSP_NUM_MEMORIES] = {
	0x00140,
	0x14000,
	0x04000,
	0x20000,
	0x20000
};

#define IPU_DEVICE_IDSP_NUM_MASTERS IPU_DEVICE_CELL_MASTER_NUM_MASTERS
/* The stride field is the offset from base address of one master to the base
 * address of the next master
 */
static const struct ipu_device_cell_master_properties_s
ipu_device_idsp_masters[IPU_DEVICE_IDSP_NUM_MASTERS] = {
	{0, 0xC, IPU_DEVICE_IDSP_ICACHE_BASE, IPU_DEVICE_IDSP_ICACHE_INFO, IPU_DEVICE_IDSP_ICACHE_INFO_OVERRIDE},
	{0, 0xC, IPU_DEVICE_IDSP_QMEM_BASE,   0xFFFFFFFF, 0xFFFFFFFF},
	{3, 0xC, IPU_DEVICE_IDSP_CMEM_BASE,   0xFFFFFFFF, 0xFFFFFFFF},
	{2, 0xC, IPU_DEVICE_IDSP_XMEM_BASE,   IPU_DEVICE_IDSP_XMEM_INFO, IPU_DEVICE_IDSP_XMEM_INFO_OVERRIDE},
	{3, 0xC, IPU_DEVICE_IDSP_XVMEM_BASE,   0xFFFFFFFF, 0xFFFFFFFF}
};

/* TODO: Share this structure across devices. It is tricky because these values
 * are used as bitmasks
 */
enum ipu_device_idsp_stall_bits {
	IPU_DEVICE_IDSP_STALL_ICACHE0,
	IPU_DEVICE_IDSP_STALL_ICACHE1,
	IPU_DEVICE_IDSP_STALL_DMEM,
	IPU_DEVICE_IDSP_STALL_QMEM,
	IPU_DEVICE_IDSP_STALL_CMEM,
	IPU_DEVICE_IDSP_STALL_XMEM,
	IPU_DEVICE_IDSP_STALL_VMEM,
	IPU_DEVICE_IDSP_STALL_BAMEM,
	IPU_DEVICE_IDSP_STALL_XVMEM,
	IPU_DEVICE_IDSP_NUM_STALL_BITS
};

#define IPU_DEVICE_IDSP_ICACHE_WORD_SIZE 64 /* 512 bits per instruction */
#define IPU_DEVICE_IDSP_ICACHE_BURST_SIZE 8 /* 8 instructions per burst */

static const struct ipu_device_cell_count_s ipu_device_idsp_count = {
	IPU_DEVICE_IDSP_NUM_MEMORIES,
	IPU_DEVICE_IDSP_NUM_MASTERS,
	IPU_DEVICE_IDSP_NUM_STALL_BITS,
	IPU_DEVICE_IDSP_ICACHE_WORD_SIZE * IPU_DEVICE_IDSP_ICACHE_BURST_SIZE
};

static const unsigned int ipu_device_idsp_reg_offset[/* CELL_NUM_REGS */] = {
	IPU_DEVICE_IDSP_STAT_CTRL, IPU_DEVICE_IDSP_START_PC, IPU_DEVICE_IDSP_ICACHE_BASE, IPU_DEVICE_IDSP_DEBUG_PC, IPU_DEVICE_IDSP_STALL
};

static const struct ipu_device_cell_type_properties_s ipu_device_idsp_properties = {
	&ipu_device_idsp_count,
	ipu_device_idsp_masters,
	ipu_device_idsp_reg_offset,
	ipu_device_idsp_mem_size
};

#endif /* _IPU_DEVICE_IDSP_PROPERTIES_IMPL_H_ */
