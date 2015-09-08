/**
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
#include "ipu_device_sp2600_fp_properties_impl.h"
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

static const unsigned int ipu_device_spf0_mem_address[IPU_DEVICE_SP2600_FP_NUM_MEMORIES] = {
	SPF0_REGS_CBUS_ADDRESS,
	IPU_DEVICE_INVALID_MEM_ADDRESS, /* no pmem */
	SPF0_DMEM_CBUS_ADDRESS,
	SPF0_DMEM1_CBUS_ADDRESS
};

static const unsigned int ipu_device_isp0_mem_address[IPU_DEVICE_ISP2600_NUM_MEMORIES] = {
	0x001C0000, /* reg addr */
	0x001D0000, /* pmem addr */
	0x001F0000, /* dmem addr */
	0,          /* bamem addr */
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

static const unsigned int ipu_device_spf0_mem_databus_address[IPU_DEVICE_SP2600_FP_NUM_MEMORIES] = {
	IPU_DEVICE_INVALID_MEM_ADDRESS, /* no reg addr */
	IPU_DEVICE_INVALID_MEM_ADDRESS, /* no pmem */
	SPF0_DMEM_DBUS_ADDRESS,
	SPF0_DMEM1_DBUS_ADDRESS
};

static const unsigned int ipu_device_isp0_mem_databus_address[IPU_DEVICE_ISP2600_NUM_MEMORIES] = {
	IPU_DEVICE_INVALID_MEM_ADDRESS, /* no reg addr */
	0x20000000,			/* pmem databus addr */
	0x22000000,			/* dmem databus addr */
	0x24000000,			/* bamem databus addr */
	0x26000000			/* vmem databus addr */
};

static const unsigned int ipu_device_isp1_mem_databus_address[IPU_DEVICE_ISP2600_NUM_MEMORIES] = {
	IPU_DEVICE_INVALID_MEM_ADDRESS, /* no reg addr */
	0x28000000,			/* pmem databus addr */
	0x2A000000,			/* dmem databus addr */
	0x2C000000,			/* bamem databus addr */
	0x2E000000			/* vmem databus addr */
};

static const unsigned int ipu_device_isp2_mem_databus_address[IPU_DEVICE_ISP2600_NUM_MEMORIES] = {
	IPU_DEVICE_INVALID_MEM_ADDRESS, /* no reg addr */
	0x30000000,			/* pmem databus addr */
	0x32000000,			/* dmem databus addr */
	0x34000000,			/* bamem databus addr */
	0x36000000			/* vmem databus addr */
};

static const unsigned int ipu_device_isp3_mem_databus_address[IPU_DEVICE_ISP2600_NUM_MEMORIES] = {
	IPU_DEVICE_INVALID_MEM_ADDRESS, /* no reg addr */
	0x38000000,			/* pmem databus addr */
	0x3A000000,			/* dmem databus addr */
	0x3C000000,			/* bamem databus addr */
	0x3E000000			/* vmem databus addr */
};


static const struct ipu_device_cell_properties_s ipu_device_cell_properties[NUM_CELLS] = {
	{ &ipu_device_sp2600_control_properties, ipu_device_spc0_mem_address, ipu_device_spc0_mem_databus_address },
	{ &ipu_device_sp2600_proxy_properties, ipu_device_spp0_mem_address, ipu_device_spp0_mem_databus_address },
	{ &ipu_device_sp2600_proxy_properties, ipu_device_spp1_mem_address, ipu_device_spp1_mem_databus_address },
	{ &ipu_device_sp2600_fp_properties, ipu_device_spf0_mem_address, ipu_device_spf0_mem_databus_address },
	{ &ipu_device_isp2600_properties, ipu_device_isp0_mem_address, ipu_device_isp0_mem_databus_address },
	{ &ipu_device_isp2600_properties, ipu_device_isp1_mem_address, ipu_device_isp1_mem_databus_address },
	{ &ipu_device_isp2600_properties, ipu_device_isp2_mem_address, ipu_device_isp2_mem_databus_address },
	{ &ipu_device_isp2600_properties, ipu_device_isp3_mem_address, ipu_device_isp3_mem_databus_address }
};

#endif /* _IPU_DEVICE_CELL_PROPERTIES_IMPL_H_ */
