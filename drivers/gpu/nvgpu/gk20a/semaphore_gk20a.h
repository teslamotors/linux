/*
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef SEMAPHORE_GK20A_H
#define SEMAPHORE_GK20A_H

#include <linux/kref.h>
#include <linux/list.h>
#include <linux/delay.h>

#include "gk20a.h"
#include "mm_gk20a.h"
#include "channel_gk20a.h"
#include "gk20a_allocator.h"

#define gpu_sema_dbg(fmt, args...)		\
	gk20a_dbg(gpu_dbg_sema, fmt, ##args)
#define gpu_sema_verbose_dbg(fmt, args...)	\
	gk20a_dbg(gpu_dbg_sema_v, fmt, ##args)

/*
 * Max number of channels that can be used is 512. This of course needs to be
 * fixed to be dynamic but still fast.
 */
#define SEMAPHORE_POOL_COUNT		512
#define SEMAPHORE_SIZE			16
#define SEMAPHORE_SEA_GROWTH_RATE	32

struct gk20a_semaphore_sea;

/*
 * Underlying semaphore data structure. This semaphore can be shared amongst
 * other semaphore instances.
 */
struct gk20a_semaphore_int {
	int idx;			/* Semaphore index. */
	u32 offset;			/* Offset into the pool. */
	atomic_t next_value;		/* Next available value. */
	u32 *value;			/* Current value (access w/ readl()). */
	u32 nr_incrs;			/* Number of increments programmed. */
	struct gk20a_semaphore_pool *p;	/* Pool that owns this sema. */
	struct channel_gk20a *ch;	/* Channel that owns this sema. */
	struct list_head hw_sema_list;	/* List of HW semaphores. */
};

/*
 * A semaphore which the rest of the driver actually uses. This consists of a
 * pointer to a real semaphore and a value to wait for. This allows one physical
 * semaphore to be shared among an essentially infinite number of submits.
 */
struct gk20a_semaphore {
	struct gk20a_semaphore_int *hw_sema;

	atomic_t value;
	int incremented;

	struct kref ref;
};

/*
 * A semaphore pool. Each address space will own exactly one of these.
 */
struct gk20a_semaphore_pool {
	struct page *page;			/* This pool's page of memory */
	struct list_head pool_list_entry;	/* Node for list of pools. */
	void *cpu_va;				/* CPU access to the pool. */
	u64 gpu_va;				/* GPU access to the pool. */
	u64 gpu_va_ro;				/* GPU access to the pool. */
	int page_idx;				/* Index into sea bitmap. */

	struct list_head hw_semas;		/* List of HW semas. */
	DECLARE_BITMAP(semas_alloced, PAGE_SIZE / SEMAPHORE_SIZE);

	struct gk20a_semaphore_sea *sema_sea;	/* Sea that owns this pool. */

	struct mutex pool_lock;

	/*
	 * This is the address spaces's personal RW table. Other channels will
	 * ultimately map this page as RO.
	 */
	struct sg_table *rw_sg_table;

	/*
	 * This is to keep track of whether the pool has had its sg_table
	 * updated during sea resizing.
	 */
	struct sg_table *ro_sg_table;

	int mapped;

	/*
	 * Sometimes a channel can be released before other channels are
	 * done waiting on it. This ref count ensures that the pool doesn't
	 * go away until all semaphores using this pool are cleaned up first.
	 */
	struct kref ref;
};

/*
 * A sea of semaphores pools. Each pool is owned by a single VM. Since multiple
 * channels can share a VM each channel gets it's own HW semaphore from the
 * pool. Channels then allocate regular semaphores - basically just a value that
 * signifies when a particular job is done.
 */
struct gk20a_semaphore_sea {
	struct list_head pool_list;	/* List of pools in this sea. */
	struct gk20a *gk20a;

	size_t size;			/* Number of pages available. */
	u64 gpu_va;			/* GPU virtual address of sema sea. */
	u64 map_size;			/* Size of the mapping. */

	/*
	 * TODO:
	 * List of pages that we use to back the pools. The number of pages
	 * can grow dynamically since allocating 512 pages for all channels at
	 * once would be a tremendous waste.
	 */
	int page_count;			/* Pages allocated to pools. */

	struct sg_table *ro_sg_table;
	/*
	struct page *pages[SEMAPHORE_POOL_COUNT];
	*/

	struct mem_desc sea_mem;

	/*
	 * Can't use a regular allocator here since the full range of pools are
	 * not always allocated. Instead just use a bitmap.
	 */
	DECLARE_BITMAP(pools_alloced, SEMAPHORE_POOL_COUNT);

	struct mutex sea_lock;		/* Lock alloc/free calls. */
};

enum gk20a_mem_rw_flag {
	gk20a_mem_flag_none = 0,
	gk20a_mem_flag_read_only = 1,
	gk20a_mem_flag_write_only = 2,
};

/*
 * Semaphore sea functions.
 */
struct gk20a_semaphore_sea *gk20a_semaphore_sea_create(struct gk20a *gk20a);
int gk20a_semaphore_sea_map(struct gk20a_semaphore_pool *sea,
			    struct vm_gk20a *vm);
void gk20a_semaphore_sea_unmap(struct gk20a_semaphore_pool *sea,
			       struct vm_gk20a *vm);
struct gk20a_semaphore_sea *gk20a_semaphore_get_sea(struct gk20a *g);

/*
 * Semaphore pool functions.
 */
struct gk20a_semaphore_pool *gk20a_semaphore_pool_alloc(
	struct gk20a_semaphore_sea *sea);
int gk20a_semaphore_pool_map(struct gk20a_semaphore_pool *pool,
			     struct vm_gk20a *vm);
void gk20a_semaphore_pool_unmap(struct gk20a_semaphore_pool *pool,
				struct vm_gk20a *vm);
u64 __gk20a_semaphore_pool_gpu_va(struct gk20a_semaphore_pool *p, bool global);
void gk20a_semaphore_pool_get(struct gk20a_semaphore_pool *p);
void gk20a_semaphore_pool_put(struct gk20a_semaphore_pool *p);

/*
 * Semaphore functions.
 */
struct gk20a_semaphore *gk20a_semaphore_alloc(struct channel_gk20a *ch);
void gk20a_semaphore_put(struct gk20a_semaphore *s);
void gk20a_semaphore_get(struct gk20a_semaphore *s);
void gk20a_semaphore_free_hw_sema(struct channel_gk20a *ch);

/*
 * Return the address of a specific semaphore.
 *
 * Don't call this on a semaphore you don't own - the VA returned will make no
 * sense in your specific channel's VM.
 */
static inline u64 gk20a_semaphore_gpu_rw_va(struct gk20a_semaphore *s)
{
	return __gk20a_semaphore_pool_gpu_va(s->hw_sema->p, false) +
		s->hw_sema->offset;
}

/*
 * Get the global RO address for the semaphore. Can be called on any semaphore
 * regardless of whether you own it.
 */
static inline u64 gk20a_semaphore_gpu_ro_va(struct gk20a_semaphore *s)
{
	return __gk20a_semaphore_pool_gpu_va(s->hw_sema->p, true) +
		s->hw_sema->offset;
}

static inline u64 gk20a_hw_sema_addr(struct gk20a_semaphore_int *hw_sema)
{
	return __gk20a_semaphore_pool_gpu_va(hw_sema->p, true) +
		hw_sema->offset;
}

/*
 * TODO: handle wrap around... Hmm, how to do this?
 */
static inline bool gk20a_semaphore_is_released(struct gk20a_semaphore *s)
{
	u32 sema_val = readl(s->hw_sema->value);

	/*
	 * If the underlying semaphore value is greater than or equal to
	 * the value of the semaphore then the semaphore has been signaled
	 * (a.k.a. released).
	 */
	return sema_val >= atomic_read(&s->value);
}

static inline bool gk20a_semaphore_is_acquired(struct gk20a_semaphore *s)
{
	return !gk20a_semaphore_is_released(s);
}

/*
 * Read the underlying value from a semaphore.
 */
static inline u32 gk20a_semaphore_read(struct gk20a_semaphore *s)
{
	return readl(s->hw_sema->value);
}

static inline u32 gk20a_semaphore_get_value(struct gk20a_semaphore *s)
{
	return atomic_read(&s->value);
}

static inline u32 gk20a_semaphore_next_value(struct gk20a_semaphore *s)
{
	return atomic_read(&s->hw_sema->next_value);
}

/*
 * If @force is set then this will not wait for the underlying semaphore to
 * catch up to the passed semaphore.
 */
static inline void __gk20a_semaphore_release(struct gk20a_semaphore *s,
					     bool force)
{
	u32 current_val;
	u32 val = gk20a_semaphore_get_value(s);
	int attempts = 0;

	/*
	 * Wait until the sema value is 1 less than the write value. That
	 * way this function is essentially an increment.
	 *
	 * TODO: tune the wait a little better.
	 */
	while ((current_val = gk20a_semaphore_read(s)) < (val - 1)) {
		if (force)
			break;
		msleep(100);
		attempts += 1;
		if (attempts > 100) {
			WARN(1, "Stall on sema release!");
			return;
		}
	}

	/*
	 * If the semaphore has already passed the value we would write then
	 * this is really just a NO-OP.
	 */
	if (current_val >= val)
		return;

	writel(val, s->hw_sema->value);

	gpu_sema_verbose_dbg("(c=%d) WRITE %u",
			     s->hw_sema->ch->hw_chid, val);
}

static inline void gk20a_semaphore_release(struct gk20a_semaphore *s)
{
	__gk20a_semaphore_release(s, false);
}

/*
 * Configure a software based increment on this semaphore. This is useful for
 * when we want the GPU to wait on a SW event before processing a channel.
 * Another way to describe this is when the GPU needs to wait on a SW pre-fence.
 * The pre-fence signals SW which in turn calls gk20a_semaphore_release() which
 * then allows the GPU to continue.
 *
 * Also used to prep a semaphore for an INCR by the GPU.
 */
static inline void gk20a_semaphore_incr(struct gk20a_semaphore *s)
{
	BUG_ON(s->incremented);

	atomic_set(&s->value, atomic_add_return(1, &s->hw_sema->next_value));
	s->incremented = 1;

	gpu_sema_verbose_dbg("INCR sema for c=%d (%u)",
			     s->hw_sema->ch->hw_chid,
			     gk20a_semaphore_next_value(s));
}
#endif
