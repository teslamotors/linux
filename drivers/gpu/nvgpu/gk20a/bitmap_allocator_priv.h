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

#ifndef BITMAP_ALLOCATOR_PRIV_H
#define BITMAP_ALLOCATOR_PRIV_H

#include <linux/rbtree.h>

struct gk20a_allocator;

struct gk20a_bitmap_allocator {
	struct gk20a_allocator *owner;

	u64 base;			/* Base address of the space. */
	u64 length;			/* Length of the space. */
	u64 blk_size;			/* Size that corresponds to 1 bit. */
	u64 blk_shift;			/* Bit shift to divide by blk_size. */
	u64 num_bits;			/* Number of allocatable bits. */
	u64 bit_offs;			/* Offset of bitmap. */

	/*
	 * Optimization for making repeated allocations faster. Keep track of
	 * the next bit after the most recent allocation. This is where the next
	 * search will start from. This should make allocation faster in cases
	 * where lots of allocations get made one after another. It shouldn't
	 * have a negative impact on the case where the allocator is fragmented.
	 */
	u64 next_blk;

	unsigned long *bitmap;		/* The actual bitmap! */
	struct rb_root allocs;		/* Tree of outstanding allocations. */

	u64 flags;

	bool inited;

	/* Statistics */
	u64 nr_allocs;
	u64 nr_fixed_allocs;
	u64 bytes_alloced;
	u64 bytes_freed;
};

struct gk20a_bitmap_alloc {
	u64 base;
	u64 length;
	struct rb_node alloc_entry;	/* RB tree of allocations. */
};

static inline struct gk20a_bitmap_allocator *bitmap_allocator(
	struct gk20a_allocator *a)
{
	return (struct gk20a_bitmap_allocator *)(a)->priv;
}


#endif
