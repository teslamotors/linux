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
 *
 * Jason Zeng <jason.zeng@intel.com>
 * Jason Chen CJ <jason.cj.chen@intel.com>
 *
 */

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#include <linux/vhm/vhm_ioctl_defs.h>
#include <linux/vhm/acrn_hv_defs.h>
#include <linux/vhm/acrn_vhm_mm.h>
#include <linux/vhm/vhm_vm_mngt.h>
#include <linux/vhm/vhm_hypercall.h>

static u64 _alloc_memblk(struct device *dev, size_t len)
{
	unsigned int count;
	struct page *page;

	if (!PAGE_ALIGNED(len)) {
		pr_warn("alloc size of memblk must be page aligned\n");
		return 0ULL;
	}

	count = PAGE_ALIGN(len) >> PAGE_SHIFT;
	page = dma_alloc_from_contiguous(dev, count, 1, GFP_KERNEL);
	if (page)
		return page_to_phys(page);
	else
		return 0ULL;
}

static bool _free_memblk(struct device *dev, u64 vm0_gpa, size_t len)
{
	unsigned int count = PAGE_ALIGN(len) >> PAGE_SHIFT;
	struct page *page = pfn_to_page(vm0_gpa >> PAGE_SHIFT);

	return dma_release_from_contiguous(dev, page, count);
}

int _mem_set_memmap(unsigned long vmid, unsigned long guest_gpa,
	unsigned long host_gpa, unsigned long len,
	unsigned int mem_type, unsigned int mem_access_right,
	unsigned int type)
{
	struct vm_set_memmap set_memmap;

	set_memmap.type = type;
	set_memmap.remote_gpa = guest_gpa;
	set_memmap.vm0_gpa = host_gpa;
	set_memmap.length = len;
	set_memmap.prot = set_memmap.prot_2 = ((mem_type & MEM_TYPE_MASK) |
			(mem_access_right & MEM_ACCESS_RIGHT_MASK));

	/* hypercall to notify hv the guest EPT setting*/
	if (hcall_set_memmap(vmid,
			virt_to_phys(&set_memmap)) < 0) {
		pr_err("vhm: failed to set memmap %ld!\n", vmid);
		return -EFAULT;
	}

	pr_debug("VHM: set ept for mem map[type=0x%x, host_gpa=0x%lx,"
		"guest_gpa=0x%lx,len=0x%lx, prot=0x%x]\n",
		type, host_gpa, guest_gpa, len, set_memmap.prot);

	return 0;
}

int set_mmio_map(unsigned long vmid, unsigned long guest_gpa,
	unsigned long host_gpa, unsigned long len,
	unsigned int mem_type, unsigned mem_access_right)
{
	return _mem_set_memmap(vmid, guest_gpa, host_gpa, len,
		mem_type, mem_access_right, MAP_MMIO);
}

int unset_mmio_map(unsigned long vmid, unsigned long guest_gpa,
	unsigned long host_gpa, unsigned long len)
{
	return _mem_set_memmap(vmid, guest_gpa, host_gpa, len,
		0, 0,  MAP_UNMAP);
}

int set_memmaps(struct set_memmaps *memmaps)
{
	if (memmaps == NULL)
		return -EINVAL;
	if (memmaps->memmaps_num > 0) {
		if (hcall_set_memmaps(virt_to_phys(memmaps)) < 0) {
			pr_err("vhm: failed to set memmaps!\n");
			return -EFAULT;
		}
	}

	return 0;
}

int update_memmap_attr(unsigned long vmid, unsigned long guest_gpa,
	unsigned long host_gpa, unsigned long len,
	unsigned int mem_type, unsigned int mem_access_right)
{
	return _mem_set_memmap(vmid, guest_gpa, host_gpa, len,
		mem_type, mem_access_right, MAP_MEM);
}

int map_guest_memseg(struct vhm_vm *vm, struct vm_memmap *memmap)
{
	unsigned int type;
	unsigned int mem_type, mem_access_right;
	unsigned long guest_gpa, host_gpa;

	/* hugetlb use vma to do the mapping */
	if (memmap->type == VM_MEMMAP_SYSMEM && memmap->using_vma)
		return hugepage_map_guest(vm, memmap);

	/* mmio */
	if (memmap->type != VM_MEMMAP_MMIO) {
		pr_err("vhm: %s invalid memmap type: %d\n",
			__func__, memmap->type);
		return -EINVAL;
	}
	guest_gpa = memmap->gpa;
	host_gpa = acrn_hpa2gpa(memmap->hpa);
	mem_type = MEM_TYPE_UC;
	mem_access_right = (memmap->prot & MEM_ACCESS_RIGHT_MASK);
	type = MAP_MMIO;

	if (_mem_set_memmap(vm->vmid, guest_gpa, host_gpa, memmap->len,
		mem_type, mem_access_right, type) < 0) {
		pr_err("vhm: failed to set memmap %ld!\n", vm->vmid);
		return -EFAULT;
	}

	return 0;
}

void free_guest_mem(struct vhm_vm *vm)
{
	if (vm->hugetlb_enabled)
		return hugepage_free_guest(vm);
}

#define TRUSTY_MEM_GPA_BASE (511UL * 1024UL * 1024UL * 1024UL)
#define TRUSTY_MEM_SIZE    (0x01000000)
int init_trusty(struct vhm_vm *vm)
{
	unsigned long host_gpa, guest_gpa = TRUSTY_MEM_GPA_BASE;
	unsigned long len = TRUSTY_MEM_SIZE;

	host_gpa = _alloc_memblk(vm->dev, TRUSTY_MEM_SIZE);
	if (host_gpa == 0ULL)
		return -ENOMEM;

	vm->trusty_host_gpa = host_gpa;

	pr_info("VHM: set ept for trusty memory [host_gpa=0x%lx, "
		"guest_gpa=0x%lx, len=0x%lx]", host_gpa, guest_gpa, len);
	return _mem_set_memmap(vm->vmid, guest_gpa, host_gpa, len,
		MEM_TYPE_WB, MEM_ACCESS_RWX, MAP_MEM);
}

void deinit_trusty(struct vhm_vm *vm)
{
	_free_memblk(vm->dev, vm->trusty_host_gpa, TRUSTY_MEM_SIZE);
	vm->trusty_host_gpa = 0;
}

void *map_guest_phys(unsigned long vmid, u64 guest_phys, size_t size)
{
	struct vhm_vm *vm;
	void *ret = NULL;

	vm = find_get_vm(vmid);
	if (vm == NULL)
		return ret;

	if (vm->hugetlb_enabled)
		ret = hugepage_map_guest_phys(vm, guest_phys, size);

	put_vm(vm);

	return ret;
}
EXPORT_SYMBOL(map_guest_phys);

int unmap_guest_phys(unsigned long vmid, u64 guest_phys)
{
	struct vhm_vm *vm;
	int ret = -ESRCH;

	vm = find_get_vm(vmid);
	if (vm == NULL) {
		pr_warn("vm_list corrupted\n");
		return ret;
	}

	if (vm->hugetlb_enabled)
		ret = hugepage_unmap_guest_phys(vm, guest_phys);

	put_vm(vm);
	return ret;
}
EXPORT_SYMBOL(unmap_guest_phys);
