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

/*
 * Basics:
 *
 *    - Lockless memory allocator for fixed-size structures, whose
 *      size is defined up front at init time.
 *    - Memory footprint scales linearly w/ the number of structures in
 *      the pool. It is ~= sizeof(int) * N.
 *    - Memory is pre-allocated by the client. The allocator itself
 *      only computes the addresses for allocations.
 *    - Limit of MAX_INT nodes that the allocator can be responsible for.
 *
 * Implementation details:
 *
 *    The allocator maintains a single list of free nodes. We allocate &
 *    free nodes from the head of the list. We rely on the cmpxchg() operator
 *    to maintain atomicity on the head.
 *
 *    So, both allocs & frees are O(1)!!
 *
 *    -- Definitions --
 *    Block Size - size of a single structure that this allocator will
 *                 allocate.
 *    Node       - one of the elements of size blk_size in the
 *                 client-allocated buffer.
 *    Node Index - zero-based index of a node in the client-allocated
 *                 contiguous buffer.
 *
 *    -- Initial State --
 *    We maintain the following to track the state of the free list:
 *
 *    1) A "head" index to track the index of the first free node in the list
 *    2) A "next" array to track the index of the next free node in the list
 *       for every node. So next[head], will give the index to the 2nd free
 *       element in the list.
 *
 *    So, to begin with, the free list consists of all node indices, and each
 *    position in the next array contains index N + 1:
 *
 *    head = 0
 *    next = [1, 2, 3, 4, -1] : Example for a user-allocated buffer of 5 nodes
 *    free_list = 0->1->2->3->4->-1
 *
 *    -- Allocations --
 *    1) Read the current head (aka acq_head)
 *    2) Read next[acq_head], to get the 2nd free element (aka new_head)
 *    3) cmp_xchg(&head, acq_head, new_head)
 *    4) If it succeeds, compute the address of the node, based on
 *       base address, blk_size, & acq_head.
 *
 *    head = 1;
 *    next = [1, 2, 3, 4, -1] : Example after allocating Node #0
 *    free_list = 1->2->3->4->-1
 *
 *    head = 2;
 *    next = [1, 2, 3, 4, -1] : Example after allocating Node #1
 *    free_list = 2->3->4->-1
 *
 *    -- Frees --
 *    1) Based on the address to be freed, calculate the index of the node
 *       being freed (cur_idx)
 *    2) Read the current head (old_head)
 *    3) So the freed node is going to go at the head of the list, and we
 *       want to put the old_head after it. So next[cur_idx] = old_head
 *    4) cmpxchg(head, old_head, cur_idx)
 *
 *    head = 0
 *    next = [2, 2, 3, 4, -1]
 *    free_list = 0->2->3->4->-1 : Example after freeing Node #0
 *
 *    head = 1
 *    next = [2, 0, 3, 4, -1]
 *    free_list = 1->0->2->3->4->-1 : Example after freeing Node #1
 */

#ifndef LOCKLESS_ALLOCATOR_PRIV_H
#define LOCKLESS_ALLOCATOR_PRIV_H

struct gk20a_allocator;

struct gk20a_lockless_allocator {
	struct gk20a_allocator *owner;

	u64 base;		/* Base address of the space. */
	u64 length;		/* Length of the space. */
	u64 blk_size;		/* Size of the structure being allocated */
	int nr_nodes;		/* Number of nodes available for allocation */

	int *next;		/* An array holding the next indices per node */
	int head;		/* Current node at the top of the stack */

	u64 flags;

	bool inited;

	/* Statistics */
	atomic_t nr_allocs;
};

static inline struct gk20a_lockless_allocator *lockless_allocator(
	struct gk20a_allocator *a)
{
	return (struct gk20a_lockless_allocator *)(a)->priv;
}

#endif
