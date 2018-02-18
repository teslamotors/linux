/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PAGE_ALLOCATOR_PRIV_H
#define PAGE_ALLOCATOR_PRIV_H

#include <linux/list.h>
#include <linux/rbtree.h>

#include "gk20a_allocator.h"

struct gk20a_allocator;

/*
 * This allocator implements the ability to do SLAB style allocation since the
 * GPU has two page sizes available - 4k and 64k/128k. When the default
 * granularity is the large page size (64k/128k) small allocations become very
 * space inefficient. This is most notable in PDE and PTE blocks which are 4k
 * in size.
 *
 * Thus we need the ability to suballocate in 64k pages. The way we do this for
 * the GPU is as follows. We have several buckets for sub-64K allocations:
 *
 *   B0 - 4k
 *   B1 - 8k
 *   B3 - 16k
 *   B4 - 32k
 *   B5 - 64k (for when large pages are 128k)
 *
 * When an allocation comes in for less than the large page size (from now on
 * assumed to be 64k) the allocation is satisfied by one of the buckets.
 */
struct page_alloc_slab {
	struct list_head empty;
	struct list_head partial;
	struct list_head full;

	int nr_empty;
	int nr_partial;
	int nr_full;

	u32 slab_size;
};

enum slab_page_state {
	SP_EMPTY,
	SP_PARTIAL,
	SP_FULL,
	SP_NONE
};

struct page_alloc_slab_page {
	unsigned long bitmap;
	u64 page_addr;
	u32 slab_size;

	u32 nr_objects;
	u32 nr_objects_alloced;

	enum slab_page_state state;

	struct page_alloc_slab *owner;
	struct list_head list_entry;
};

struct page_alloc_chunk {
	struct list_head list_entry;

	u64 base;
	u64 length;
};

/*
 * Struct to handle internal management of page allocation. It holds a list
 * of the chunks of pages that make up the overall allocation - much like a
 * scatter gather table.
 */
struct gk20a_page_alloc {
	struct list_head alloc_chunks;

	int nr_chunks;
	u64 length;

	/*
	 * Only useful for the RB tree - since the alloc may have discontiguous
	 * pages the base is essentially irrelevant except for the fact that it
	 * is guarenteed to be unique.
	 */
	u64 base;

	struct rb_node tree_entry;

	/*
	 * Set if this is a slab alloc. Points back to the slab page that owns
	 * this particular allocation. nr_chunks will always be 1 if this is
	 * set.
	 */
	struct page_alloc_slab_page *slab_page;
};

struct gk20a_page_allocator {
	struct gk20a_allocator *owner;	/* Owner of this allocator. */

	/*
	 * Use a buddy allocator to manage the allocation of the underlying
	 * pages. This lets us abstract the discontiguous allocation handling
	 * out of the annoyingly complicated buddy allocator.
	 */
	struct gk20a_allocator source_allocator;

	/*
	 * Page params.
	 */
	u64 base;
	u64 length;
	u64 page_size;
	u32 page_shift;

	struct rb_root allocs;		/* Outstanding allocations. */

	struct page_alloc_slab *slabs;
	int nr_slabs;

	u64 flags;

	/*
	 * Stat tracking.
	 */
	u64 nr_allocs;
	u64 nr_frees;
	u64 nr_fixed_allocs;
	u64 nr_fixed_frees;
	u64 nr_slab_allocs;
	u64 nr_slab_frees;
	u64 pages_alloced;
	u64 pages_freed;
};

static inline struct gk20a_page_allocator *page_allocator(
	struct gk20a_allocator *a)
{
	return (struct gk20a_page_allocator *)(a)->priv;
}

static inline struct gk20a_allocator *palloc_owner(
	struct gk20a_page_allocator *a)
{
	return a->owner;
}

#endif
