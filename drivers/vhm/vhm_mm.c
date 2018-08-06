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

struct guest_memseg {
	struct list_head list;
	u64 vm0_gpa;
	size_t len;
	u64 gpa;
	long vma_count;
};

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

static int add_guest_memseg(struct vhm_vm *vm, unsigned long vm0_gpa,
	unsigned long guest_gpa, unsigned long len)
{
	struct guest_memseg *seg;
	int max_gfn;

	seg = kzalloc(sizeof(struct guest_memseg), GFP_KERNEL);
	if (seg == NULL)
		return -ENOMEM;

	seg->vm0_gpa = vm0_gpa;
	seg->gpa = guest_gpa;
	seg->len = len;

	max_gfn = (seg->gpa + seg->len) >> PAGE_SHIFT;
	if (vm->max_gfn < max_gfn)
		vm->max_gfn = max_gfn;

	pr_info("VHM: add memseg with len=0x%lx, vm0_gpa=0x%llx,"
		" and its guest gpa = 0x%llx, vm max_gfn 0x%x\n",
		seg->len, seg->vm0_gpa, seg->gpa, vm->max_gfn);

	seg->vma_count = 0;
	mutex_lock(&vm->seg_lock);
	list_add(&seg->list, &vm->memseg_list);
	mutex_unlock(&vm->seg_lock);

	return 0;
}

int alloc_guest_memseg(struct vhm_vm *vm, struct vm_memseg *memseg)
{
	unsigned long vm0_gpa;
	int ret;

	vm0_gpa = _alloc_memblk(vm->dev, memseg->len);
	if (vm0_gpa == 0ULL)
		return -ENOMEM;

	ret = add_guest_memseg(vm, vm0_gpa, memseg->gpa, memseg->len);
	if (ret < 0)
		_free_memblk(vm->dev, vm0_gpa, memseg->len);

	return ret;
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
	struct guest_memseg *seg = NULL;
	unsigned int type;
	unsigned int mem_type, mem_access_right;
	unsigned long guest_gpa, host_gpa;

	/* hugetlb use vma to do the mapping */
	if (memmap->type == VM_MEMMAP_SYSMEM && memmap->using_vma)
		return hugepage_map_guest(vm, memmap);

	mutex_lock(&vm->seg_lock);

	/* cma or mmio */
	if (memmap->type == VM_MEMMAP_SYSMEM) {
		list_for_each_entry(seg, &vm->memseg_list, list) {
			if (seg->gpa == memmap->gpa
				&& seg->len == memmap->len)
				break;
		}
		if (&seg->list == &vm->memseg_list) {
			mutex_unlock(&vm->seg_lock);
			return -EINVAL;
		}
		guest_gpa = seg->gpa;
		host_gpa = seg->vm0_gpa;
		mem_type = MEM_TYPE_WB;
		mem_access_right = (memmap->prot & MEM_ACCESS_RIGHT_MASK);
		type = MAP_MEM;
	} else {
		guest_gpa = memmap->gpa;
		host_gpa = acrn_hpa2gpa(memmap->hpa);
		mem_type = MEM_TYPE_UC;
		mem_access_right = (memmap->prot & MEM_ACCESS_RIGHT_MASK);
		type = MAP_MMIO;
	}

	if (_mem_set_memmap(vm->vmid, guest_gpa, host_gpa, memmap->len,
		mem_type, mem_access_right, type) < 0) {
		pr_err("vhm: failed to set memmap %ld!\n", vm->vmid);
		mutex_unlock(&vm->seg_lock);
		return -EFAULT;
	}

	mutex_unlock(&vm->seg_lock);

	return 0;
}

void free_guest_mem(struct vhm_vm *vm)
{
	struct guest_memseg *seg;

	if (vm->hugetlb_enabled)
		return hugepage_free_guest(vm);

	mutex_lock(&vm->seg_lock);
	while (!list_empty(&vm->memseg_list)) {
		seg = list_first_entry(&vm->memseg_list,
				struct guest_memseg, list);
		if (!_free_memblk(vm->dev, seg->vm0_gpa, seg->len))
			pr_warn("failed to free memblk\n");
		list_del(&seg->list);
		kfree(seg);
	}
	mutex_unlock(&vm->seg_lock);
}

int check_guest_mem(struct vhm_vm *vm)
{
	struct guest_memseg *seg;

	mutex_lock(&vm->seg_lock);
	list_for_each_entry(seg, &vm->memseg_list, list) {
		if (seg->vma_count == 0)
			continue;

		mutex_unlock(&vm->seg_lock);
		return -EAGAIN;
	}
	mutex_unlock(&vm->seg_lock);
	return 0;
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

static void guest_vm_open(struct vm_area_struct *vma)
{
	struct vhm_vm *vm = vma->vm_file->private_data;
	struct guest_memseg *seg = vma->vm_private_data;

	mutex_lock(&vm->seg_lock);
	seg->vma_count++;
	mutex_unlock(&vm->seg_lock);
}

static void guest_vm_close(struct vm_area_struct *vma)
{
	struct vhm_vm *vm = vma->vm_file->private_data;
	struct guest_memseg *seg = vma->vm_private_data;

	mutex_lock(&vm->seg_lock);
	seg->vma_count--;
	BUG_ON(seg->vma_count < 0);
	mutex_unlock(&vm->seg_lock);
}

static const struct vm_operations_struct guest_vm_ops = {
	.open = guest_vm_open,
	.close = guest_vm_close,
};

static int do_mmap_guest(struct file *file,
		struct vm_area_struct *vma, struct guest_memseg *seg)
{
	struct page *page;
	size_t size = seg->len;
	unsigned long pfn;
	unsigned long start_addr;

	vma->vm_flags |= VM_MIXEDMAP | VM_DONTEXPAND | VM_DONTCOPY;
	pfn = seg->vm0_gpa >> PAGE_SHIFT;
	start_addr = vma->vm_start;
	while (size > 0) {
		page = pfn_to_page(pfn);
		if (vm_insert_page(vma, start_addr, page))
			return -EINVAL;
		size -= PAGE_SIZE;
		start_addr += PAGE_SIZE;
		pfn++;
	}
	seg->vma_count++;
	vma->vm_ops = &guest_vm_ops;
	vma->vm_private_data = (void *)seg;

	pr_info("VHM: mmap for memseg [seg vm0_gpa=0x%llx, gpa=0x%llx] "
		"to start addr 0x%lx\n",
		seg->vm0_gpa, seg->gpa, start_addr);

	return 0;
}

int vhm_dev_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct vhm_vm *vm = file->private_data;
	struct guest_memseg *seg;
	u64 offset = vma->vm_pgoff << PAGE_SHIFT;
	size_t len = vma->vm_end - vma->vm_start;
	int ret;

	if (vm->hugetlb_enabled)
		return -EINVAL;

	mutex_lock(&vm->seg_lock);
	list_for_each_entry(seg, &vm->memseg_list, list) {
		if (seg->gpa != offset || seg->len != len)
			continue;

		ret = do_mmap_guest(file, vma, seg);
		mutex_unlock(&vm->seg_lock);
		return ret;
	}
	mutex_unlock(&vm->seg_lock);
	return -EINVAL;
}

static void *do_map_guest_phys(struct vhm_vm *vm, u64 guest_phys, size_t size)
{
	struct guest_memseg *seg;

	mutex_lock(&vm->seg_lock);
	list_for_each_entry(seg, &vm->memseg_list, list) {
		if (seg->gpa > guest_phys ||
		    guest_phys >= seg->gpa + seg->len)
			continue;

		if (guest_phys + size > seg->gpa + seg->len) {
			mutex_unlock(&vm->seg_lock);
			return NULL;
		}

		mutex_unlock(&vm->seg_lock);
		return phys_to_virt(seg->vm0_gpa + guest_phys - seg->gpa);
	}
	mutex_unlock(&vm->seg_lock);
	return NULL;
}

void *map_guest_phys(unsigned long vmid, u64 guest_phys, size_t size)
{
	struct vhm_vm *vm;
	void *ret;

	vm = find_get_vm(vmid);
	if (vm == NULL)
		return NULL;

	if (vm->hugetlb_enabled)
		ret = hugepage_map_guest_phys(vm, guest_phys, size);
	else
		ret = do_map_guest_phys(vm, guest_phys, size);

	put_vm(vm);

	return ret;
}
EXPORT_SYMBOL(map_guest_phys);

static int do_unmap_guest_phys(struct vhm_vm *vm, u64 guest_phys)
{
	struct guest_memseg *seg;

	mutex_lock(&vm->seg_lock);
	list_for_each_entry(seg, &vm->memseg_list, list) {
		if (seg->gpa <= guest_phys &&
			guest_phys < seg->gpa + seg->len) {
			mutex_unlock(&vm->seg_lock);
			return 0;
		}
	}
	mutex_unlock(&vm->seg_lock);

	return -ESRCH;
}

int unmap_guest_phys(unsigned long vmid, u64 guest_phys)
{
	struct vhm_vm *vm;
	int ret;

	vm = find_get_vm(vmid);
	if (vm == NULL) {
		pr_warn("vm_list corrupted\n");
		return -ESRCH;
	}

	if (vm->hugetlb_enabled)
		ret = hugepage_unmap_guest_phys(vm, guest_phys);
	else
		ret = do_unmap_guest_phys(vm, guest_phys);

	put_vm(vm);
	return ret;
}
EXPORT_SYMBOL(unmap_guest_phys);
