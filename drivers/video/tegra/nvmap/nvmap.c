/*
 * drivers/video/tegra/nvmap/nvmap.c
 *
 * Memory manager for Tegra GPU
 *
 * Copyright (c) 2009-2017, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#define pr_fmt(fmt)	"nvmap: %s() " fmt, __func__

#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/io.h>
#include <linux/rbtree.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/export.h>

#include <asm/pgtable.h>
#include <asm/tlbflush.h>

#include <linux/nvmap.h>
#include <trace/events/nvmap.h>

#include "nvmap_priv.h"

static phys_addr_t handle_phys(struct nvmap_handle *h)
{
	if (h->heap_pgalloc)
		BUG();
	return h->carveout->base;
}

void *__nvmap_kmap(struct nvmap_handle *h, unsigned int pagenum)
{
	phys_addr_t paddr;
	unsigned long kaddr;
	pgprot_t prot;
	struct vm_struct *area = NULL;

	if (!virt_addr_valid(h))
		return NULL;

	h = nvmap_handle_get(h);
	if (!h)
		return NULL;

	if (!h->alloc)
		goto put_handle;

	if (!(h->heap_type & nvmap_dev->cpu_access_mask))
		goto put_handle;

	nvmap_kmaps_inc(h);
	if (pagenum >= h->size >> PAGE_SHIFT)
		goto out;
	prot = nvmap_pgprot(h, PG_PROT_KERNEL);
	area = alloc_vm_area(PAGE_SIZE, NULL);
	if (!area)
		goto out;
	kaddr = (ulong)area->addr;

	if (h->heap_pgalloc)
		paddr = page_to_phys(nvmap_to_page(h->pgalloc.pages[pagenum]));
	else
		paddr = h->carveout->base + pagenum * PAGE_SIZE;

	ioremap_page_range(kaddr, kaddr + PAGE_SIZE, paddr, prot);
	return (void *)kaddr;
out:
	nvmap_kmaps_dec(h);
put_handle:
	nvmap_handle_put(h);
	return NULL;
}

void __nvmap_kunmap(struct nvmap_handle *h, unsigned int pagenum,
		  void *addr)
{
	phys_addr_t paddr;
	struct vm_struct *area = NULL;

	if (!h || !h->alloc ||
	    WARN_ON(!virt_addr_valid(h)) ||
	    WARN_ON(!addr) ||
	    !(h->heap_type & nvmap_dev->cpu_access_mask))
		return;

	if (WARN_ON(pagenum >= h->size >> PAGE_SHIFT))
		return;

	if (h->heap_pgalloc)
		paddr = page_to_phys(nvmap_to_page(h->pgalloc.pages[pagenum]));
	else
		paddr = h->carveout->base + pagenum * PAGE_SIZE;

	if (h->flags != NVMAP_HANDLE_UNCACHEABLE &&
	    h->flags != NVMAP_HANDLE_WRITE_COMBINE) {
		__dma_flush_range(addr, addr + PAGE_SIZE);
		outer_flush_range(paddr, paddr + PAGE_SIZE); /* FIXME */
	}

	area = find_vm_area(addr);
	if (area)
		free_vm_area(area);
	else
		WARN(1, "Invalid address passed");
	nvmap_kmaps_dec(h);
	nvmap_handle_put(h);
}

void *__nvmap_mmap(struct nvmap_handle *h)
{
	pgprot_t prot;
	void *vaddr;
	unsigned long adj_size;
	struct vm_struct *v;
	struct page **pages;

	if (!virt_addr_valid(h))
		return NULL;

	h = nvmap_handle_get(h);
	if (!h)
		return NULL;

	if (!h->alloc)
		goto put_handle;

	if (!(h->heap_type & nvmap_dev->cpu_access_mask))
		goto put_handle;

	if (h->vaddr)
		return h->vaddr;

	nvmap_kmaps_inc(h);
	prot = nvmap_pgprot(h, PG_PROT_KERNEL);

	if (h->heap_pgalloc) {
		pages = nvmap_pages(h->pgalloc.pages, h->size >> PAGE_SHIFT);
		if (!pages)
			goto out;

		vaddr = vm_map_ram(pages, h->size >> PAGE_SHIFT, -1, prot);
		nvmap_altfree(pages, (h->size >> PAGE_SHIFT) * sizeof(*pages));
		if (!vaddr && !h->vaddr)
			goto out;

		if (vaddr && atomic_long_cmpxchg(&h->vaddr, 0, (long)vaddr)) {
			nvmap_kmaps_dec(h);
			vm_unmap_ram(vaddr, h->size >> PAGE_SHIFT);
		}
		return h->vaddr;
	}

	/* carveout - explicitly map the pfns into a vmalloc area */
	adj_size = h->carveout->base & ~PAGE_MASK;
	adj_size += h->size;
	adj_size = PAGE_ALIGN(adj_size);

	v = alloc_vm_area(adj_size, NULL);
	if (!v)
		goto out;

	vaddr = v->addr + (h->carveout->base & ~PAGE_MASK);
	ioremap_page_range((ulong)v->addr, (ulong)v->addr + adj_size,
		h->carveout->base & PAGE_MASK, prot);

	if (vaddr && atomic_long_cmpxchg(&h->vaddr, 0, (long)vaddr)) {
		struct vm_struct *vm;

		vaddr -= (h->carveout->base & ~PAGE_MASK);
		vm = find_vm_area(vaddr);
		BUG_ON(!vm);
		free_vm_area(vm);
		nvmap_kmaps_dec(h);
	}

	/* leave the handle ref count incremented by 1, so that
	 * the handle will not be freed while the kernel mapping exists.
	 * nvmap_handle_put will be called by unmapping this address */
	return h->vaddr;
out:
	nvmap_kmaps_dec(h);
put_handle:
	nvmap_handle_put(h);
	return NULL;
}

void __nvmap_munmap(struct nvmap_handle *h, void *addr)
{
	if (!h || !h->alloc ||
	    WARN_ON(!virt_addr_valid(h)) ||
	    WARN_ON(!addr) ||
	    !(h->heap_type & nvmap_dev->cpu_access_mask))
		return;

	nvmap_handle_put(h);
}

void nvmap_handle_put(struct nvmap_handle *h)
{
	int cnt;

	if (WARN_ON(!virt_addr_valid(h)))
		return;
	cnt = atomic_dec_return(&h->ref);

	if (WARN_ON(cnt < 0)) {
		pr_err("%s: %s put to negative references\n",
			__func__, current->comm);
	} else if (cnt == 0)
		_nvmap_handle_free(h);
}

struct sg_table *__nvmap_sg_table(struct nvmap_client *client,
		struct nvmap_handle *h)
{
	struct sg_table *sgt = NULL;
	int err, npages;
	struct page **pages;

	if (!virt_addr_valid(h))
		return ERR_PTR(-EINVAL);

	h = nvmap_handle_get(h);
	if (!h)
		return ERR_PTR(-EINVAL);

	if (!h->alloc) {
		err = -EINVAL;
		goto put_handle;
	}

	npages = PAGE_ALIGN(h->size) >> PAGE_SHIFT;
	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt) {
		err = -ENOMEM;
		goto err;
	}

	if (!h->heap_pgalloc) {
		err = sg_alloc_table(sgt, 1, GFP_KERNEL);
		if (err)
			goto err;
		sg_set_buf(sgt->sgl, phys_to_virt(handle_phys(h)), h->size);
	} else {
		pages = nvmap_pages(h->pgalloc.pages, npages);
		if (!pages) {
			err = -ENOMEM;
			goto err;
		}
		err = sg_alloc_table_from_pages(sgt, pages,
				npages, 0, h->size, GFP_KERNEL);
		nvmap_altfree(pages, npages * sizeof(*pages));
		if (err)
			goto err;
	}
	nvmap_handle_put(h);
	return sgt;

err:
	kfree(sgt);
put_handle:
	nvmap_handle_put(h);
	return ERR_PTR(err);
}

void __nvmap_free_sg_table(struct nvmap_client *client,
		struct nvmap_handle *h, struct sg_table *sgt)
{
	sg_free_table(sgt);
	kfree(sgt);
}
