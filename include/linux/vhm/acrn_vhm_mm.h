/*
 * virtio and hyperviosr service module (VHM): memory map
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright (c) 2017 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * BSD LICENSE
 *
 * Copyright (C) 2017 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Jason Chen CJ <jason.cj.chen@intel.com>
 *
 */

/**
 * DOC: Virtio and Hypervisor Module memory manager APIs
 */

#ifndef __ACRN_VHM_MM_H__
#define __ACRN_VHM_MM_H__

#include <linux/vhm/vhm_ioctl_defs.h>
#include <linux/vhm/vhm_vm_mngt.h>
#include <linux/vhm/acrn_hv_defs.h>

/**
 * acrn_hpa2gpa - physical address conversion
 *
 * convert host physical address (hpa) to guest physical address (gpa)
 * gpa and hpa is 1:1 mapping for service OS
 *
 * @hpa: host physical address
 *
 * Return: guest physical address
 */
static inline unsigned long  acrn_hpa2gpa(unsigned long hpa)
{
	return hpa;
}

/**
 * map_guest_phys - map guest physical address to SOS kernel virtual address
 *
 * @vmid: guest vmid
 * @uos_phys: physical address in guest
 * @size: the memory size mapped
 *
 * Return: SOS kernel virtual address, NULL on error
 */
void *map_guest_phys(unsigned long vmid, u64 uos_phys, size_t size);

/**
 * unmap_guest_phys - unmap guest physical address
 *
 * @vmid: guest vmid
 * @uos_phys: physical address in guest
 *
 * Return: 0 on success, <0 for error.
 */
int unmap_guest_phys(unsigned long vmid, u64 uos_phys);

/**
 * set_mmio_map - map mmio EPT mapping between UOS gpa and SOS gpa
 *
 * @vmid: guest vmid
 * @guest_gpa: gpa of UOS
 * @host_gpa: gpa of SOS
 * @len: memory mapped length
 * @mem_type: memory mapping type. Possible value could be:
 *                    MEM_TYPE_WB
 *                    MEM_TYPE_WT
 *                    MEM_TYPE_UC
 *                    MEM_TYPE_WC
 *                    MEM_TYPE_WP
 * @mem_access_right: memory mapping access. Possible value could be:
 *                    MEM_ACCESS_READ
 *                    MEM_ACCESS_WRITE
 *                    MEM_ACCESS_EXEC
 *                    MEM_ACCESS_RWX
 *
 * Return: 0 on success, <0 for error.
 */
int set_mmio_map(unsigned long vmid, unsigned long guest_gpa,
	unsigned long host_gpa, unsigned long len,
	unsigned int mem_type, unsigned int mem_access_right);

/**
 * unset_mmio_map - unmap mmio mapping between UOS gpa and SOS gpa
 *
 * @vmid: guest vmid
 * @guest_gpa: gpa of UOS
 * @host_gpa: gpa of SOS
 * @len: memory mapped length
 *
 * Return: 0 on success, <0 for error.
 */
int unset_mmio_map(unsigned long vmid, unsigned long guest_gpa,
	unsigned long host_gpa, unsigned long len);

/**
 * update_memmap_attr - update mmio EPT mapping between UOS gpa and SOS gpa
 *
 * @vmid: guest vmid
 * @guest_gpa: gpa of UOS
 * @host_gpa: gpa of SOS
 * @len: memory mapped length
 * @mem_type: memory mapping type. Possible value could be:
 *                    MEM_TYPE_WB
 *                    MEM_TYPE_WT
 *                    MEM_TYPE_UC
 *                    MEM_TYPE_WC
 *                    MEM_TYPE_WP
 * @mem_access_right: memory mapping access. Possible value could be:
 *                    MEM_ACCESS_READ
 *                    MEM_ACCESS_WRITE
 *                    MEM_ACCESS_EXEC
 *                    MEM_ACCESS_RWX
 *
 * Return: 0 on success, <0 for error.
 */
int update_memmap_attr(unsigned long vmid, unsigned long guest_gpa,
	unsigned long host_gpa, unsigned long len,
	unsigned int mem_type, unsigned int mem_access_right);

int vhm_dev_mmap(struct file *file, struct vm_area_struct *vma);

/**
 * free_guest_mem - free memory of guest
 *
 * @vm: pointer to guest vm
 *
 * Return:
 */
void free_guest_mem(struct vhm_vm *vm);

/**
 * map_guest_memseg - set guest mmapping of memory according to
 * pre-defined memory mapping info
 *
 * @vm: pointer to guest vm
 * @memmap: pointer to guest memory mapping info
 *
 * Return:
 */
int map_guest_memseg(struct vhm_vm *vm, struct vm_memmap *memmap);

int init_trusty(struct vhm_vm *vm);
void deinit_trusty(struct vhm_vm *vm);

int _mem_set_memmap(unsigned long vmid, unsigned long guest_gpa,
	unsigned long host_gpa, unsigned long len,
	unsigned int mem_type, unsigned int mem_access_right,
	unsigned int type);
int hugepage_map_guest(struct vhm_vm *vm, struct vm_memmap *memmap);
void hugepage_free_guest(struct vhm_vm *vm);
void *hugepage_map_guest_phys(struct vhm_vm *vm, u64 guest_phys, size_t size);
int hugepage_unmap_guest_phys(struct vhm_vm *vm, u64 guest_phys);

/**
 * set_memmaps - set guest mapping for multi regions
 *
 * @memmaps: pointer to set_memmaps
 *
 * Return: 0 on success, <0 for error.
 */
int set_memmaps(struct set_memmaps *memmaps);
#endif
