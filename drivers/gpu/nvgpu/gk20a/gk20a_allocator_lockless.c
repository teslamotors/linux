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

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/atomic.h>

#include "gk20a_allocator.h"
#include "lockless_allocator_priv.h"

static u64 gk20a_lockless_alloc_length(struct gk20a_allocator *a)
{
	struct gk20a_lockless_allocator *pa = a->priv;

	return pa->length;
}

static u64 gk20a_lockless_alloc_base(struct gk20a_allocator *a)
{
	struct gk20a_lockless_allocator *pa = a->priv;

	return pa->base;
}

static int gk20a_lockless_alloc_inited(struct gk20a_allocator *a)
{
	struct gk20a_lockless_allocator *pa = a->priv;
	int inited = pa->inited;

	rmb();
	return inited;
}

static u64 gk20a_lockless_alloc_end(struct gk20a_allocator *a)
{
	struct gk20a_lockless_allocator *pa = a->priv;

	return pa->base + pa->length;
}

static u64 gk20a_lockless_alloc(struct gk20a_allocator *a, u64 len)
{
	struct gk20a_lockless_allocator *pa = a->priv;
	int head, new_head, ret;
	u64 addr = 0;

	if (len != pa->blk_size)
		return 0;

	head = ACCESS_ONCE(pa->head);
	while (head >= 0) {
		new_head = ACCESS_ONCE(pa->next[head]);
		ret = cmpxchg(&pa->head, head, new_head);
		if (ret == head) {
			addr = pa->base + head * pa->blk_size;
			atomic_inc(&pa->nr_allocs);
			alloc_dbg(a, "Alloc node # %d @ addr 0x%llx\n", head,
				  addr);
			break;
		}
		head = ACCESS_ONCE(pa->head);
	}
	return addr;
}

static void gk20a_lockless_free(struct gk20a_allocator *a, u64 addr)
{
	struct gk20a_lockless_allocator *pa = a->priv;
	int head, ret;
	int cur_idx = (addr - pa->base) / pa->blk_size;

	while (1) {
		head = ACCESS_ONCE(pa->head);
		ACCESS_ONCE(pa->next[cur_idx]) = head;
		ret = cmpxchg(&pa->head, head, cur_idx);
		if (ret == head) {
			atomic_dec(&pa->nr_allocs);
			alloc_dbg(a, "Free node # %d\n", cur_idx);
			break;
		}
	}
}

static void gk20a_lockless_alloc_destroy(struct gk20a_allocator *a)
{
	struct gk20a_lockless_allocator *pa = a->priv;

	gk20a_fini_alloc_debug(a);

	vfree(pa->next);
	kfree(pa);
}

static void gk20a_lockless_print_stats(struct gk20a_allocator *a,
				   struct seq_file *s, int lock)
{
	struct gk20a_lockless_allocator *pa = a->priv;

	__alloc_pstat(s, a, "Lockless allocator params:\n");
	__alloc_pstat(s, a, "  start = 0x%llx\n", pa->base);
	__alloc_pstat(s, a, "  end   = 0x%llx\n", pa->base + pa->length);

	/* Actual stats. */
	__alloc_pstat(s, a, "Stats:\n");
	__alloc_pstat(s, a, "  Number allocs = %d\n",
		      atomic_read(&pa->nr_allocs));
	__alloc_pstat(s, a, "  Number free   = %d\n",
		      pa->nr_nodes - atomic_read(&pa->nr_allocs));
}

static const struct gk20a_allocator_ops pool_ops = {
	.alloc		= gk20a_lockless_alloc,
	.free		= gk20a_lockless_free,

	.base		= gk20a_lockless_alloc_base,
	.length		= gk20a_lockless_alloc_length,
	.end		= gk20a_lockless_alloc_end,
	.inited		= gk20a_lockless_alloc_inited,

	.fini		= gk20a_lockless_alloc_destroy,

	.print_stats	= gk20a_lockless_print_stats,
};

int gk20a_lockless_allocator_init(struct gk20a *g, struct gk20a_allocator *__a,
			      const char *name, u64 base, u64 length,
			      u64 blk_size, u64 flags)
{
	int i;
	int err;
	int nr_nodes;
	u64 count;
	struct gk20a_lockless_allocator *a;

	if (!blk_size)
		return -EINVAL;

	/*
	 * Ensure we have space for atleast one node & there's no overflow.
	 * In order to control memory footprint, we require count < INT_MAX
	 */
	count = length / blk_size;
	if (!base || !count || count > INT_MAX)
		return -EINVAL;

	a = kzalloc(sizeof(struct gk20a_lockless_allocator), GFP_KERNEL);
	if (!a)
		return -ENOMEM;

	err = __gk20a_alloc_common_init(__a, name, a, false, &pool_ops);
	if (err)
		goto fail;

	a->next = vzalloc(sizeof(*a->next) * count);
	if (!a->next) {
		err = -ENOMEM;
		goto fail;
	}

	/* chain the elements together to form the initial free list  */
	nr_nodes = (int)count;
	for (i = 0; i < nr_nodes; i++)
		a->next[i] = i + 1;
	a->next[nr_nodes - 1] = -1;

	a->base = base;
	a->length = length;
	a->blk_size = blk_size;
	a->nr_nodes = nr_nodes;
	a->flags = flags;
	atomic_set(&a->nr_allocs, 0);

	wmb();
	a->inited = true;

	gk20a_init_alloc_debug(g, __a);
	alloc_dbg(__a, "New allocator: type          lockless\n");
	alloc_dbg(__a, "               base          0x%llx\n", a->base);
	alloc_dbg(__a, "               nodes         %d\n", a->nr_nodes);
	alloc_dbg(__a, "               blk_size      0x%llx\n", a->blk_size);
	alloc_dbg(__a, "               flags         0x%llx\n", a->flags);

	return 0;

fail:
	kfree(a);
	return err;
}
