/*
 * drivers/video/tegra/nvmap/nvmap_handle.c
 *
 * Handle allocation and freeing routines for nvmap
 *
 * Copyright (c) 2009-2011, NVIDIA Corporation.
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

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include <asm/cacheflush.h>
#include <asm/outercache.h>
#include <asm/pgtable.h>

#include <mach/iovmm.h>
#include <mach/nvmap.h>

#include <linux/vmstat.h>
#include <linux/swap.h>

#include "nvmap.h"
#include "nvmap_mru.h"
#include "nvmap_common.h"

#define PRINT_CARVEOUT_CONVERSION 0
#if PRINT_CARVEOUT_CONVERSION
#define PR_INFO pr_info
#else
#define PR_INFO(...)
#endif

#define NVMAP_SECURE_HEAPS	(NVMAP_HEAP_CARVEOUT_IRAM | NVMAP_HEAP_IOVMM | \
				 NVMAP_HEAP_CARVEOUT_VPR)
#ifdef CONFIG_NVMAP_HIGHMEM_ONLY
#define GFP_NVMAP		(__GFP_HIGHMEM | __GFP_NOWARN)
#else
#define GFP_NVMAP		(GFP_KERNEL | __GFP_HIGHMEM | __GFP_NOWARN)
#endif
/* handles may be arbitrarily large (16+MiB), and any handle allocated from
 * the kernel (i.e., not a carveout handle) includes its array of pages. to
 * preserve kmalloc space, if the array of pages exceeds PAGELIST_VMALLOC_MIN,
 * the array is allocated using vmalloc. */
#define PAGELIST_VMALLOC_MIN	(PAGE_SIZE * 2)

static inline void *altalloc(size_t len)
{
	if (len >= PAGELIST_VMALLOC_MIN)
		return vmalloc(len);
	else
		return kmalloc(len, GFP_KERNEL);
}

static inline void altfree(void *ptr, size_t len)
{
	if (!ptr)
		return;

	if (len >= PAGELIST_VMALLOC_MIN)
		vfree(ptr);
	else
		kfree(ptr);
}

void _nvmap_handle_free(struct nvmap_handle *h)
{
	struct nvmap_device *dev = h->dev;
	unsigned int i, nr_page;

	if (nvmap_handle_remove(dev, h) != 0)
		return;

	if (!h->alloc)
		goto out;

	if (!h->heap_pgalloc) {
		nvmap_usecount_inc(h);
		nvmap_heap_free(h->carveout);
		goto out;
	}

	nr_page = DIV_ROUND_UP(h->size, PAGE_SIZE);

	BUG_ON(h->size & ~PAGE_MASK);
	BUG_ON(!h->pgalloc.pages);

	nvmap_mru_remove(nvmap_get_share_from_dev(dev), h);

	// Restore page attributes.
	if (h->flags == NVMAP_HANDLE_WRITE_COMBINE ||
	    h->flags == NVMAP_HANDLE_UNCACHEABLE ||
	    h->flags == NVMAP_HANDLE_INNER_CACHEABLE)
		set_pages_array_wb(h->pgalloc.pages, nr_page);

	if (h->pgalloc.area)
		tegra_iovmm_free_vm(h->pgalloc.area);

	for (i = 0; i < nr_page; i++)
		__free_page(h->pgalloc.pages[i]);

	altfree(h->pgalloc.pages, nr_page * sizeof(struct page *));

out:
	kfree(h);
}

extern void __flush_dcache_page(struct address_space *, struct page *);

static struct page *nvmap_alloc_pages_exact(gfp_t gfp, size_t size)
{
	struct page *page, *p, *e;
	unsigned int order;

	size = PAGE_ALIGN(size);
	order = get_order(size);
	page = alloc_pages(gfp, order);

	if (!page)
		return NULL;

	split_page(page, order);
	e = page + (1 << order);
	for (p = page + (size >> PAGE_SHIFT); p < e; p++)
		__free_page(p);

	return page;
}

static int handle_page_alloc(struct nvmap_client *client,
			     struct nvmap_handle *h, bool contiguous)
{
	size_t size = PAGE_ALIGN(h->size);
	unsigned int nr_page = size >> PAGE_SHIFT;
	pgprot_t prot;
	unsigned int i = 0;
	struct page **pages;

	pages = altalloc(nr_page * sizeof(*pages));
	if (!pages)
		return -ENOMEM;

	prot = nvmap_pgprot(h, pgprot_kernel);

	h->pgalloc.area = NULL;
	if (contiguous) {
		struct page *page;
		page = nvmap_alloc_pages_exact(GFP_NVMAP, size);
		if (!page)
			goto fail;

		for (i = 0; i < nr_page; i++)
			pages[i] = nth_page(page, i);

	} else {
		for (i = 0; i < nr_page; i++) {
			pages[i] = nvmap_alloc_pages_exact(GFP_NVMAP,
				PAGE_SIZE);
			if (!pages[i])
				goto fail;
		}

#ifndef CONFIG_NVMAP_RECLAIM_UNPINNED_VM
		h->pgalloc.area = tegra_iovmm_create_vm(client->share->iovmm,
					NULL, size, h->align, prot,
					h->pgalloc.iovm_addr);
		if (!h->pgalloc.area)
			goto fail;

		h->pgalloc.dirty = true;
#endif
	}

	// Update the pages mapping in kernel page table.
	if (h->flags == NVMAP_HANDLE_WRITE_COMBINE)
		set_pages_array_wc(pages, nr_page);
	else if (h->flags == NVMAP_HANDLE_UNCACHEABLE)
		set_pages_array_uc(pages, nr_page);
	else if (h->flags == NVMAP_HANDLE_INNER_CACHEABLE)
		set_pages_array_iwb(pages, nr_page);

	h->size = size;
	h->pgalloc.pages = pages;
	h->pgalloc.contig = contiguous;
	INIT_LIST_HEAD(&h->pgalloc.mru_list);
	return 0;

fail:
	while (i--)
		__free_page(pages[i]);
	altfree(pages, nr_page * sizeof(*pages));
	wmb();
	return -ENOMEM;
}

static void alloc_handle(struct nvmap_client *client,
			 struct nvmap_handle *h, unsigned int type)
{
	BUG_ON(type & (type - 1));

#ifdef CONFIG_NVMAP_CONVERT_CARVEOUT_TO_IOVMM
#define __NVMAP_HEAP_CARVEOUT	(NVMAP_HEAP_CARVEOUT_IRAM | NVMAP_HEAP_CARVEOUT_VPR)
#define __NVMAP_HEAP_IOVMM	(NVMAP_HEAP_IOVMM | NVMAP_HEAP_CARVEOUT_GENERIC)
	if (type & NVMAP_HEAP_CARVEOUT_GENERIC) {
#ifdef CONFIG_NVMAP_ALLOW_SYSMEM
		if (h->size <= PAGE_SIZE) {
			PR_INFO("###CARVEOUT CONVERTED TO SYSMEM "
				"0x%x bytes %s(%d)###\n",
				h->size, current->comm, current->pid);
			goto sysheap;
		}
#endif
		PR_INFO("###CARVEOUT CONVERTED TO IOVM "
			"0x%x bytes %s(%d)###\n",
			h->size, current->comm, current->pid);
	}
#else
#define __NVMAP_HEAP_CARVEOUT	NVMAP_HEAP_CARVEOUT_MASK
#define __NVMAP_HEAP_IOVMM	NVMAP_HEAP_IOVMM
#endif

	if (type & __NVMAP_HEAP_CARVEOUT) {
		struct nvmap_heap_block *b;
#ifdef CONFIG_NVMAP_CONVERT_CARVEOUT_TO_IOVMM
		PR_INFO("###IRAM REQUEST RETAINED "
			"0x%x bytes %s(%d)###\n",
			h->size, current->comm, current->pid);
#endif
		/* Protect handle from relocation */
		nvmap_usecount_inc(h);

		b = nvmap_carveout_alloc(client, h, type);
		if (b) {
			h->heap_pgalloc = false;
			h->alloc = true;
			nvmap_carveout_commit_add(client,
				nvmap_heap_to_arg(nvmap_block_to_heap(b)),
				h->size);
		}
		nvmap_usecount_dec(h);

	} else if (type & __NVMAP_HEAP_IOVMM) {
		size_t reserved = PAGE_ALIGN(h->size);
		int commit = 0;
		int ret;

		/* increment the committed IOVM space prior to allocation
		 * to avoid race conditions with other threads simultaneously
		 * allocating. */
		commit = atomic_add_return(reserved,
					    &client->iovm_commit);

		if (commit < client->iovm_limit)
			ret = handle_page_alloc(client, h, false);
		else
			ret = -ENOMEM;

		if (!ret) {
			h->heap_pgalloc = true;
			h->alloc = true;
		} else {
			atomic_sub(reserved, &client->iovm_commit);
		}

	} else if (type & NVMAP_HEAP_SYSMEM) {
#if defined(CONFIG_NVMAP_CONVERT_CARVEOUT_TO_IOVMM) && \
	defined(CONFIG_NVMAP_ALLOW_SYSMEM)
sysheap:
#endif
		if (handle_page_alloc(client, h, true) == 0) {
			BUG_ON(!h->pgalloc.contig);
			h->heap_pgalloc = true;
			h->alloc = true;
		}
	}
}

/* small allocations will try to allocate from generic OS memory before
 * any of the limited heaps, to increase the effective memory for graphics
 * allocations, and to reduce fragmentation of the graphics heaps with
 * sub-page splinters */
static const unsigned int heap_policy_small[] = {
	NVMAP_HEAP_CARVEOUT_VPR,
	NVMAP_HEAP_CARVEOUT_IRAM,
#ifdef CONFIG_NVMAP_ALLOW_SYSMEM
	NVMAP_HEAP_SYSMEM,
#endif
	NVMAP_HEAP_CARVEOUT_MASK,
	NVMAP_HEAP_IOVMM,
	0,
};

static const unsigned int heap_policy_large[] = {
	NVMAP_HEAP_CARVEOUT_VPR,
	NVMAP_HEAP_CARVEOUT_IRAM,
	NVMAP_HEAP_IOVMM,
	NVMAP_HEAP_CARVEOUT_MASK,
#ifdef CONFIG_NVMAP_ALLOW_SYSMEM
	NVMAP_HEAP_SYSMEM,
#endif
	0,
};

/* Do not override single page policy if there is not much space to
avoid invoking system oom killer. */
#define NVMAP_SMALL_POLICY_SYSMEM_THRESHOLD 50000000

int nvmap_alloc_handle_id(struct nvmap_client *client,
			  unsigned long id, unsigned int heap_mask,
			  size_t align, unsigned int flags)
{
	struct nvmap_handle *h = NULL;
	const unsigned int *alloc_policy;
	int nr_page;
	int err = -ENOMEM;

	h = nvmap_get_handle_id(client, id);

	if (!h)
		return -EINVAL;

	if (h->alloc)
		goto out;

	nr_page = ((h->size + PAGE_SIZE - 1) >> PAGE_SHIFT);
	h->secure = !!(flags & NVMAP_HANDLE_SECURE);
	h->flags = (flags & NVMAP_HANDLE_CACHE_FLAG);
	h->align = max_t(size_t, align, L1_CACHE_BYTES);

#ifndef CONFIG_TEGRA_IOVMM
	if (heap_mask & NVMAP_HEAP_IOVMM) {
		heap_mask &= NVMAP_HEAP_IOVMM;
		heap_mask |= NVMAP_HEAP_CARVEOUT_GENERIC;
	}
#endif
#ifndef CONFIG_NVMAP_CONVERT_CARVEOUT_TO_IOVMM
#ifdef CONFIG_NVMAP_ALLOW_SYSMEM
	/* Allow single pages allocations in system memory to save
	 * carveout space and avoid extra iovm mappings */
	if (nr_page == 1) {
		if (heap_mask & NVMAP_HEAP_IOVMM)
			heap_mask |= NVMAP_HEAP_SYSMEM;
		else if (heap_mask & NVMAP_HEAP_CARVEOUT_GENERIC) {
			/* Calculate size of free physical pages
			 * managed by kernel */
			unsigned long freeMem =
				(global_page_state(NR_FREE_PAGES) +
				global_page_state(NR_FILE_PAGES) -
				 total_swapcache_pages()) << PAGE_SHIFT;

			if (freeMem > NVMAP_SMALL_POLICY_SYSMEM_THRESHOLD)
				heap_mask |= NVMAP_HEAP_SYSMEM;
		}
	}
#endif

	/* This restriction is deprecated as alignments greater than
	   PAGE_SIZE are now correctly handled, but it is retained for
	   AP20 compatibility. */
	if (h->align > PAGE_SIZE)
		heap_mask &= NVMAP_HEAP_CARVEOUT_MASK;
#endif
	/* secure allocations can only be served from secure heaps */
	if (h->secure)
		heap_mask &= NVMAP_SECURE_HEAPS;

	if (!heap_mask) {
		err = -EINVAL;
		goto out;
	}

	alloc_policy = (nr_page == 1) ? heap_policy_small : heap_policy_large;

	while (!h->alloc && *alloc_policy) {
		unsigned int heap_type;

		heap_type = *alloc_policy++;
		heap_type &= heap_mask;

		if (!heap_type)
			continue;

		heap_mask &= ~heap_type;

		while (heap_type && !h->alloc) {
			unsigned int heap;

			/* iterate possible heaps MSB-to-LSB, since higher-
			 * priority carveouts will have higher usage masks */
			heap = 1 << __fls(heap_type);
			alloc_handle(client, h, heap);
			heap_type &= ~heap;
		}
	}

out:
	err = (h->alloc) ? 0 : err;
	nvmap_handle_put(h);
	return err;
}

void nvmap_free_handle_id(struct nvmap_client *client, unsigned long id)
{
	struct nvmap_handle_ref *ref;
	struct nvmap_handle *h;
	int pins;

	nvmap_ref_lock(client);

	ref = _nvmap_validate_id_locked(client, id);
	if (!ref) {
		nvmap_ref_unlock(client);
		return;
	}

	BUG_ON(!ref->handle);
	h = ref->handle;

	if (atomic_dec_return(&ref->dupes)) {
		nvmap_ref_unlock(client);
		goto out;
	}

	smp_rmb();
	pins = atomic_read(&ref->pin);
	rb_erase(&ref->node, &client->handle_refs);

	if (h->alloc && h->heap_pgalloc && !h->pgalloc.contig)
		atomic_sub(h->size, &client->iovm_commit);

	if (h->alloc && !h->heap_pgalloc) {
		mutex_lock(&h->lock);
		nvmap_carveout_commit_subtract(client,
			nvmap_heap_to_arg(nvmap_block_to_heap(h->carveout)),
			h->size);
		mutex_unlock(&h->lock);
	}

	nvmap_ref_unlock(client);

	if (pins)
		nvmap_err(client, "%s freeing pinned handle %p\n",
			  current->group_leader->comm, h);

	while (pins--)
		nvmap_unpin_handles(client, &ref->handle, 1);

	if (h->owner == client)
		h->owner = NULL;

	kfree(ref);

out:
	BUG_ON(!atomic_read(&h->ref));
	nvmap_handle_put(h);
}

static void add_handle_ref(struct nvmap_client *client,
			   struct nvmap_handle_ref *ref)
{
	struct rb_node **p, *parent = NULL;

	nvmap_ref_lock(client);
	p = &client->handle_refs.rb_node;
	while (*p) {
		struct nvmap_handle_ref *node;
		parent = *p;
		node = rb_entry(parent, struct nvmap_handle_ref, node);
		if (ref->handle > node->handle)
			p = &parent->rb_right;
		else
			p = &parent->rb_left;
	}
	rb_link_node(&ref->node, parent, p);
	rb_insert_color(&ref->node, &client->handle_refs);
	nvmap_ref_unlock(client);
}

struct nvmap_handle_ref *nvmap_create_handle(struct nvmap_client *client,
					     size_t size)
{
	struct nvmap_handle *h;
	struct nvmap_handle_ref *ref = NULL;

	if (!client )
		return ERR_PTR(-EINVAL);

	if (!size)
		return ERR_PTR(-EINVAL);

	h = kzalloc(sizeof(*h), GFP_KERNEL);
	if (!h)
		return ERR_PTR(-ENOMEM);

	ref = kzalloc(sizeof(*ref), GFP_KERNEL);
	if (!ref) {
		kfree(h);
		return ERR_PTR(-ENOMEM);
	}

	atomic_set(&h->ref, 1);
	atomic_set(&h->pin, 0);
	h->owner = client;
	h->dev = client->dev;
	BUG_ON(!h->owner);
	h->size = h->orig_size = size;
	h->flags = NVMAP_HANDLE_WRITE_COMBINE;
	mutex_init(&h->lock);

	nvmap_handle_add(client->dev, h);

	atomic_set(&ref->dupes, 1);
	ref->handle = h;
	atomic_set(&ref->pin, 0);
	add_handle_ref(client, ref);
	return ref;
}

struct nvmap_handle_ref *nvmap_duplicate_handle_id(struct nvmap_client *client,
						   unsigned long id)
{
	struct nvmap_handle_ref *ref = NULL;
	struct nvmap_handle *h = NULL;

	BUG_ON(!client || client->dev != nvmap_dev);
	/* on success, the reference count for the handle should be
	 * incremented, so the success paths will not call nvmap_handle_put */
	h = nvmap_validate_get(client, id);

	if (!h) {
		nvmap_debug(client, "%s duplicate handle failed\n",
			    current->group_leader->comm);
		return ERR_PTR(-EPERM);
	}

	if (!h->alloc) {
		nvmap_err(client, "%s duplicating unallocated handle\n",
			  current->group_leader->comm);
		nvmap_handle_put(h);
		return ERR_PTR(-EINVAL);
	}

	nvmap_ref_lock(client);
	ref = _nvmap_validate_id_locked(client, (unsigned long)h);

	if (ref) {
		/* handle already duplicated in client; just increment
		 * the reference count rather than re-duplicating it */
		atomic_inc(&ref->dupes);
		nvmap_ref_unlock(client);
		return ref;
	}

	nvmap_ref_unlock(client);

	/* verify that adding this handle to the process' access list
	 * won't exceed the IOVM limit */
	if (h->heap_pgalloc && !h->pgalloc.contig) {
		int oc;
		oc = atomic_add_return(h->size, &client->iovm_commit);
		if (oc > client->iovm_limit && !client->super) {
			atomic_sub(h->size, &client->iovm_commit);
			nvmap_handle_put(h);
			nvmap_err(client, "duplicating %p in %s over-commits"
				  " IOVMM space\n", (void *)id,
				  current->group_leader->comm);
			return ERR_PTR(-ENOMEM);
		}
	}

	ref = kzalloc(sizeof(*ref), GFP_KERNEL);
	if (!ref) {
		nvmap_handle_put(h);
		return ERR_PTR(-ENOMEM);
	}

	if (!h->heap_pgalloc) {
		mutex_lock(&h->lock);
		nvmap_carveout_commit_add(client,
			nvmap_heap_to_arg(nvmap_block_to_heap(h->carveout)),
			h->size);
		mutex_unlock(&h->lock);
	}

	atomic_set(&ref->dupes, 1);
	ref->handle = h;
	atomic_set(&ref->pin, 0);
	add_handle_ref(client, ref);
	return ref;
}
