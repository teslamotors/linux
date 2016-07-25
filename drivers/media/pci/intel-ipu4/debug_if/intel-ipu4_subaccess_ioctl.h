/*
 * Copyright (c) 2014--2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*************************************************************************
* IOCTL definations for vied subaccess layer.
* Author- Biswajit Dash, Intel Corp, 2013
* Rev- 1.0 01/20/2013
**************************************************************************/
#ifndef _INTEL_IPU4_SUBACESSS_IOCTL_H_
#define _INTEL_IPU4_SUBACESSS_IOCTL_H_

#include <linux/types.h>
#include <linux/ioctl.h>
#include "vied_memory_access_types.h"
#include "vied_subsystem_access_types.h"
#include "shared_memory_access.h"


#define VIED_SUBACCESS_IOC_MAGIC 'l'
#define VIED_SUBACCESS_IOC_SUBSYSTEM_LOAD	_IO(VIED_SUBACCESS_IOC_MAGIC, 1)
#define VIED_SUBACCESS_IOC_SUBSYSTEM_STORE	_IO(VIED_SUBACCESS_IOC_MAGIC, 2)
#define VIED_SUBACCESS_IOC_SHARED_MEM_LOAD	_IO(VIED_SUBACCESS_IOC_MAGIC, 3)
#define VIED_SUBACCESS_IOC_SHARED_MEM_STORE	_IO(VIED_SUBACCESS_IOC_MAGIC, 4)
#define VIED_SUBACCESS_IOC_MEMORY_MAP_INITIALIZE \
	_IO(VIED_SUBACCESS_IOC_MAGIC, 5)
#define VIED_SUBACCESS_IOC_MEMORY_MAP	_IO(VIED_SUBACCESS_IOC_MAGIC, 6)
#define VIED_SUBACCESS_IOC_MEMORY_ALLOC_INITIALIZE \
	_IO(VIED_SUBACCESS_IOC_MAGIC, 7)
#define VIED_SUBACCESS_IOC_MEMORY_ALLOC	_IO(VIED_SUBACCESS_IOC_MAGIC, 8)
#define VIED_SUBACCESS_IOC_MEMORY_FREE		_IO(VIED_SUBACCESS_IOC_MAGIC, 9)
#define VIED_SUBACCESS_IOC_MEMORY_ALLOC_UNINITIALIZE \
	_IO(VIED_SUBACCESS_IOC_MAGIC, 10)


struct vied_shared_memory_map_req {
	__u32 id;
	__u32 idm;
	size_t mmu_ps;
	size_t mmu_pnrs;
	vied_physical_address_t ddr_addr;
	unsigned long host_va; /*to kernel*/
	__u32 vied_va; /*from kernel for map*/
	__s32 retval;/*from kernel for map initialisation*/
};

struct vied_subsystem_access_req {
	vied_subsystem_t dev;
	vied_subsystem_address_t reg_off;
	void *user_buffer;
	__u32 size;
};

struct vied_shared_memory_access_req {
	vied_memory_t idm;
	host_virtual_address_t va;
	void *user_buffer;
	__u32 size;
};

struct vied_shared_memory_alloc_req {
	vied_memory_t idm;
	vied_physical_address_t host_ddr_addr;
	host_virtual_address_t host_va;
	size_t memory_size;
	size_t ps;
	size_t alloc_size;
	__s32 retval;
};

#endif /*_VIED_SUBACESSS_IOCTL_H_ */
