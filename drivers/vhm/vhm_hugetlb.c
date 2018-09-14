/*
 * virtio and hyperviosr service module (VHM): hugetlb
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright (c) 2018 Intel Corporation. All rights reserved.
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
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
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
 * Jason Chen CJ <jason.cj.chen@intel.com>
 *
 */

#include <linux/kernel.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/slab.h>

#include <linux/vhm/acrn_hv_defs.h>
#include <linux/vhm/acrn_vhm_mm.h>
#include <linux/vhm/vhm_vm_mngt.h>

#define HUGEPAGE_2M_SHIFT	21
#define HUGEPAGE_1G_SHIFT	30

#define HUGEPAGE_1G_HLIST_IDX	(HUGEPAGE_HLIST_ARRAY_SIZE - 1)

struct hugepage_map {
	struct hlist_node hlist;
	u64 vm0_gpa;
	size_t size;
	u64 guest_gpa;
};

static inline struct hlist_head *hlist_2m_hash(struct vhm_vm *vm,
	unsigned long guest_gpa)
{
	return &vm->hugepage_hlist[guest_gpa >> HUGEPAGE_2M_SHIFT &
			(HUGEPAGE_2M_HLIST_ARRAY_SIZE - 1)];
}

static int add_guest_map(struct vhm_vm *vm, unsigned long vm0_gpa,
	unsigned long guest_gpa, unsigned long size)
{
	struct hugepage_map *map;
	int max_gfn;

	map = kzalloc(sizeof(struct hugepage_map), GFP_KERNEL);
	if (map == NULL)
		return -ENOMEM;

	map->vm0_gpa = vm0_gpa;
	map->guest_gpa = guest_gpa;
	map->size = size;

	INIT_HLIST_NODE(&map->hlist);

	max_gfn = (map->guest_gpa + map->size) >> PAGE_SHIFT;
	if (vm->max_gfn < max_gfn)
		vm->max_gfn = max_gfn;

	pr_info("VHM: add hugepage with size=0x%lx, vm0_gpa=0x%llx,"
		" and its guest gpa = 0x%llx, vm max_gfn 0x%x\n",
		map->size, map->vm0_gpa, map->guest_gpa, vm->max_gfn);

	mutex_lock(&vm->hugepage_lock);
	/* 1G hugepage? */
	if (map->size == (1UL << HUGEPAGE_1G_SHIFT))
		hlist_add_head(&map->hlist,
			&vm->hugepage_hlist[HUGEPAGE_1G_HLIST_IDX]);
	else
		hlist_add_head(&map->hlist,
			hlist_2m_hash(vm, map->guest_gpa));
	mutex_unlock(&vm->hugepage_lock);

	return 0;
}

int hugepage_map_guest(struct vhm_vm *vm, struct vm_memmap *memmap)
{
	struct page *page = NULL, *regions_buf_pg = NULL;
	unsigned long len, guest_gpa, vma;
	struct vm_memory_region *region_array;
	struct set_regions regions;
	int max_size = PAGE_SIZE/sizeof(struct vm_memory_region);
	int ret;

	if (vm == NULL || memmap == NULL)
		return -EINVAL;

	len = memmap->len;
	vma = memmap->vma_base;
	guest_gpa = memmap->gpa;

	/* prepare set_memory_regions info */
	regions_buf_pg = alloc_page(GFP_KERNEL);
	if (regions_buf_pg == NULL)
		return -ENOMEM;
	regions.mr_num = 0;
	regions.vmid = vm->vmid;
	regions.regions_gpa = page_to_phys(regions_buf_pg);
	region_array = page_to_virt(regions_buf_pg);

	while (len > 0) {
		unsigned long vm0_gpa, pagesize;

		ret = get_user_pages_fast(vma, 1, 1, &page);
		if (unlikely(ret != 1) || (page == NULL)) {
			pr_err("failed to pin huge page!\n");
			ret = -ENOMEM;
			goto err;
		}

		vm0_gpa = page_to_phys(page);
		pagesize = PAGE_SIZE << compound_order(page);

		ret = add_guest_map(vm, vm0_gpa, guest_gpa, pagesize);
		if (ret < 0) {
			pr_err("failed to add memseg for huge page!\n");
			goto err;
		}

		/* fill each memory region into region_array */
		region_array[regions.mr_num].type = MR_ADD;
		region_array[regions.mr_num].gpa = guest_gpa;
		region_array[regions.mr_num].vm0_gpa = vm0_gpa;
		region_array[regions.mr_num].size = pagesize;
		region_array[regions.mr_num].prot =
				(MEM_TYPE_WB & MEM_TYPE_MASK) |
				(memmap->prot & MEM_ACCESS_RIGHT_MASK);
		regions.mr_num++;
		if (regions.mr_num == max_size) {
			pr_info("region buffer full, set & renew regions!\n");
			ret = set_memory_regions(&regions);
			if (ret < 0) {
				pr_err("failed to set regions,ret=%d!\n", ret);
				goto err;
			}
			regions.mr_num = 0;
		}

		len -= pagesize;
		vma += pagesize;
		guest_gpa += pagesize;
	}

	ret = set_memory_regions(&regions);
	if (ret < 0) {
		pr_err("failed to set regions, ret=%d!\n", ret);
		goto err;
	}

	__free_page(regions_buf_pg);

	return 0;
err:
	if (regions_buf_pg)
		__free_page(regions_buf_pg);
	if (page)
		put_page(page);
	return ret;
}

void hugepage_free_guest(struct vhm_vm *vm)
{
	struct hlist_node *htmp;
	struct hugepage_map *map;
	int i;

	mutex_lock(&vm->hugepage_lock);
	for (i = 0; i < HUGEPAGE_HLIST_ARRAY_SIZE; i++) {
		if (!hlist_empty(&vm->hugepage_hlist[i])) {
			hlist_for_each_entry_safe(map, htmp,
					&vm->hugepage_hlist[i], hlist) {
				hlist_del(&map->hlist);
				/* put_page to unpin huge page */
				put_page(pfn_to_page(
					map->vm0_gpa >> PAGE_SHIFT));
				kfree(map);
			}
		}
	}
	mutex_unlock(&vm->hugepage_lock);
}

void *hugepage_map_guest_phys(struct vhm_vm *vm, u64 guest_phys, size_t size)
{
	struct hlist_node *htmp;
	struct hugepage_map *map;

	mutex_lock(&vm->hugepage_lock);
	/* check 1G hlist first */
	if (!hlist_empty(&vm->hugepage_hlist[HUGEPAGE_1G_HLIST_IDX])) {
		hlist_for_each_entry_safe(map, htmp,
			&vm->hugepage_hlist[HUGEPAGE_1G_HLIST_IDX], hlist) {
			if (map->guest_gpa >= guest_phys + size ||
				guest_phys >= map->guest_gpa + map->size)
				continue;

			if (guest_phys + size > map->guest_gpa + map->size ||
					guest_phys < map->guest_gpa)
				goto err;

			mutex_unlock(&vm->hugepage_lock);
			return phys_to_virt(map->vm0_gpa +
					guest_phys - map->guest_gpa);
		}
	}

	/* check 2m hlist */
	hlist_for_each_entry_safe(map, htmp,
			hlist_2m_hash(vm, guest_phys), hlist) {
		if (map->guest_gpa >= guest_phys + size ||
				guest_phys >= map->guest_gpa + map->size)
			continue;

		if (guest_phys + size > map->guest_gpa + map->size ||
				guest_phys < map->guest_gpa)
			goto err;

		mutex_unlock(&vm->hugepage_lock);
		return phys_to_virt(map->vm0_gpa +
				guest_phys - map->guest_gpa);
	}

err:
	mutex_unlock(&vm->hugepage_lock);
	printk(KERN_WARNING "cannot find correct mem map, please check the "
		"input's range or alignment");
	return NULL;
}

int hugepage_unmap_guest_phys(struct vhm_vm *vm, u64 guest_phys)
{
	struct hlist_node *htmp;
	struct hugepage_map *map;

	mutex_lock(&vm->hugepage_lock);
	/* check 1G hlist first */
	if (!hlist_empty(&vm->hugepage_hlist[HUGEPAGE_1G_HLIST_IDX])) {
		hlist_for_each_entry_safe(map, htmp,
			&vm->hugepage_hlist[HUGEPAGE_1G_HLIST_IDX], hlist) {
			if (map->guest_gpa <= guest_phys &&
				guest_phys < map->guest_gpa + map->size) {
				mutex_unlock(&vm->hugepage_lock);
				return 0;
			}
		}
	}
	/* check 2m hlist */
	hlist_for_each_entry_safe(map, htmp,
			hlist_2m_hash(vm, guest_phys), hlist) {
			if (map->guest_gpa <= guest_phys &&
				guest_phys < map->guest_gpa + map->size) {
				mutex_unlock(&vm->hugepage_lock);
				return 0;
			}
	}
	mutex_unlock(&vm->hugepage_lock);
	return -ESRCH;
}
