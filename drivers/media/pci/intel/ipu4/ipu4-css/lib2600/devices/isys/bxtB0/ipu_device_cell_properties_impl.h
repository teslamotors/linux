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

#ifndef __IPU_DEVICE_CELL_PROPERTIES_IMPL_H
#define __IPU_DEVICE_CELL_PROPERTIES_IMPL_H

/* define properties for all cells uses in ISYS */

#include "ipu_device_sp2600_control_properties_impl.h"
#include "ipu_device_cell_properties_defs.h"
#include "ipu_device_cell_devices.h"
#include "ipu_device_cell_type_properties.h"/* IPU_DEVICE_INVALID_MEM_ADDRESS */

static const unsigned int
ipu_device_spc0_mem_address[IPU_DEVICE_SP2600_CONTROL_NUM_MEMORIES] = {
	SPC0_REGS_CBUS_ADDRESS,
	IPU_DEVICE_INVALID_MEM_ADDRESS,	/* no pmem */
	SPC0_DMEM_CBUS_ADDRESS
};

static const unsigned int
ipu_device_spc0_databus_mem_address[IPU_DEVICE_SP2600_CONTROL_NUM_MEMORIES] = {
	IPU_DEVICE_INVALID_MEM_ADDRESS, /* regs not accessible from DBUS */
	IPU_DEVICE_INVALID_MEM_ADDRESS,	/* no pmem */
	SPC0_DMEM_DBUS_ADDRESS
};

static const struct
ipu_device_cell_properties_s ipu_device_cell_properties[NUM_CELLS] = {
	{
		&ipu_device_sp2600_control_properties,
		ipu_device_spc0_mem_address,
		ipu_device_spc0_databus_mem_address
	}
};

#ifdef C_RUN

/* Mapping between hrt_hive_processors enum and cell_id's used in FW */
static const int ipu_device_map_cell_id_to_crun_proc_id[NUM_CELLS] = {
	0  /* SPC0 */
};

#endif

#endif /* __IPU_DEVICE_CELL_PROPERTIES_IMPL_H */
