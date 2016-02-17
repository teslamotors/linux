/*
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

#ifndef _IPU_DEVICE_CELL_PROPERTIES_IMPL_H_
#define _IPU_DEVICE_CELL_PROPERTIES_IMPL_H_

#include "ipu_device_sp2600_control_properties_impl.h"
#include "ipu_device_sp2600_proxy_properties_impl.h"
#include "ipu_device_isp2600_properties_impl.h"
#include "ipu_device_cell_properties_defs.h"
#include "ipu_device_cell_devices.h"
#include "ipu_device_cell_type_properties.h" /* IPU_DEVICE_INVALID_MEM_ADDRESS */

static const unsigned int ipu_device_spc0_mem_address[IPU_DEVICE_SP2600_CONTROL_NUM_MEMORIES] = {
	SPC0_REGS_CBUS_ADDRESS,
	IPU_DEVICE_INVALID_MEM_ADDRESS, /* no pmem */
	SPC0_DMEM_CBUS_ADDRESS
};

static const unsigned int ipu_device_spp0_mem_address[IPU_DEVICE_SP2600_PROXY_NUM_MEMORIES] = {
	SPP0_REGS_CBUS_ADDRESS,
	IPU_DEVICE_INVALID_MEM_ADDRESS, /* no pmem */
	SPP0_DMEM_CBUS_ADDRESS
};

static const unsigned int ipu_device_spp1_mem_address[IPU_DEVICE_SP2600_PROXY_NUM_MEMORIES] = {
	SPP1_REGS_CBUS_ADDRESS,
	IPU_DEVICE_INVALID_MEM_ADDRESS, /* no pmem */
	SPP1_DMEM_CBUS_ADDRESS
};

static const unsigned int ipu_device_isp0_mem_address[IPU_DEVICE_ISP2600_NUM_MEMORIES] = {
	0x001C0000, /* reg addr */
	0x001D0000, /* pmem addr */
	0x001F0000, /* dmem addr */
	0x00200000, /* bamem addr */
	0x00220000  /* vmem addr */
};

static const unsigned int ipu_device_isp1_mem_address[IPU_DEVICE_ISP2600_NUM_MEMORIES] = {
	0x00240000, /* reg addr */
	0x00250000, /* pmem addr */
	0x00270000, /* dmem addr */
	0x00280000, /* bamem addr */
	0x002A0000  /* vmem addr */
};

static const unsigned int ipu_device_isp2_mem_address[IPU_DEVICE_ISP2600_NUM_MEMORIES] = {
	0x002C0000, /* reg addr */
	0x002D0000, /* pmem addr */
	0x002F0000, /* dmem addr */
	0x00300000, /* bamem addr */
	0x00320000  /* vmem addr */
};

static const unsigned int ipu_device_isp3_mem_address[IPU_DEVICE_ISP2600_NUM_MEMORIES] = {
	0x00340000, /* reg addr */
	0x00350000, /* pmem addr */
	0x00370000, /* dmem addr */
	0x00380000, /* bamem addr */
	0x003A0000  /* vmem addr */
};

static const unsigned int ipu_device_spc0_mem_databus_address[IPU_DEVICE_SP2600_CONTROL_NUM_MEMORIES] = {
	IPU_DEVICE_INVALID_MEM_ADDRESS, /* no reg addr */
	IPU_DEVICE_INVALID_MEM_ADDRESS, /* no pmem */
	SPC0_DMEM_DBUS_ADDRESS
};

static const unsigned int ipu_device_spp0_mem_databus_address[IPU_DEVICE_SP2600_PROXY_NUM_MEMORIES] = {
	IPU_DEVICE_INVALID_MEM_ADDRESS, /* no reg addr */
	IPU_DEVICE_INVALID_MEM_ADDRESS, /* no pmem */
	SPP0_DMEM_DBUS_ADDRESS
};

static const unsigned int ipu_device_spp1_mem_databus_address[IPU_DEVICE_SP2600_PROXY_NUM_MEMORIES] = {
	IPU_DEVICE_INVALID_MEM_ADDRESS, /* no reg addr */
	IPU_DEVICE_INVALID_MEM_ADDRESS, /* no pmem */
	SPP1_DMEM_DBUS_ADDRESS
};

static const unsigned int ipu_device_isp0_mem_databus_address[IPU_DEVICE_ISP2600_NUM_MEMORIES] = {
	IPU_DEVICE_INVALID_MEM_ADDRESS, /* no reg addr */
	0x08000000,			/* pmem databus addr */
	0x08400000,			/* dmem databus addr */
	0x09000000,			/* bamem databus addr */
	0x08800000			/* vmem databus addr */
};

static const unsigned int ipu_device_isp1_mem_databus_address[IPU_DEVICE_ISP2600_NUM_MEMORIES] = {
	IPU_DEVICE_INVALID_MEM_ADDRESS, /* no reg addr */
	0x0A000000,			/* pmem databus addr */
	0x0A400000,			/* dmem databus addr */
	0x0B000000,			/* bamem databus addr */
	0x0A800000			/* vmem databus addr */
};

static const unsigned int ipu_device_isp2_mem_databus_address[IPU_DEVICE_ISP2600_NUM_MEMORIES] = {
	IPU_DEVICE_INVALID_MEM_ADDRESS, /* no reg addr */
	0x0C000000,			/* pmem databus addr */
	0x0C400000			,/* dmem databus addr */
	0x0D000000,			/* bamem databus addr */
	0x0C800000			/* vmem databus addr */
};

static const unsigned int ipu_device_isp3_mem_databus_address[IPU_DEVICE_ISP2600_NUM_MEMORIES] = {
	IPU_DEVICE_INVALID_MEM_ADDRESS, /* no reg addr */
	0x0E000000,			/* pmem databus addr */
	0x0E400000,			/* dmem databus addr */
	0x0F000000,			/* bamem databus addr */
	0x0E800000			/* vmem databus addr */
};

static const struct ipu_device_cell_properties_s ipu_device_cell_properties[NUM_CELLS] = {
	{ &ipu_device_sp2600_control_properties, ipu_device_spc0_mem_address, ipu_device_spc0_mem_databus_address },
	{ &ipu_device_sp2600_proxy_properties, ipu_device_spp0_mem_address, ipu_device_spp0_mem_databus_address },
	{ &ipu_device_sp2600_proxy_properties, ipu_device_spp1_mem_address, ipu_device_spp1_mem_databus_address },
	{ &ipu_device_isp2600_properties, ipu_device_isp0_mem_address, ipu_device_isp0_mem_databus_address },
	{ &ipu_device_isp2600_properties, ipu_device_isp1_mem_address, ipu_device_isp1_mem_databus_address },
	{ &ipu_device_isp2600_properties, ipu_device_isp2_mem_address, ipu_device_isp2_mem_databus_address },
	{ &ipu_device_isp2600_properties, ipu_device_isp3_mem_address, ipu_device_isp3_mem_databus_address }
};

#ifdef C_RUN

/* Mapping between hrt_hive_processors enum and cell_id's used in FW */
static const int ipu_device_map_cell_id_to_crun_proc_id[NUM_CELLS] = {
	4, /* SPC0 */
	5, /* SPP0 */
	6, /* SPP1 */
	0, /* ISP0 */
	1, /* ISP1 */
	2, /* ISP2 */
	3  /* ISP3 */
};

#endif

#endif /* _IPU_DEVICE_CELL_PROPERTIES_IMPL_H_ */
