/*
 * Copyright (c) 2011-2016, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef GK20A_ALLOCATOR_H
#define GK20A_ALLOCATOR_H

#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/platform_device.h>

/* #define ALLOCATOR_DEBUG */

struct gk20a_allocator;
struct gk20a_alloc_carveout;
struct vm_gk20a;
struct gk20a;

/*
 * Operations for an allocator to implement.
 */
struct gk20a_allocator_ops {
	u64  (*alloc)(struct gk20a_allocator *allocator, u64 len);
	void (*free)(struct gk20a_allocator *allocator, u64 addr);

	/*
	 * Special interface to allocate a memory region with a specific
	 * starting address. Yikes. Note: if free() works for freeing both
	 * regular and fixed allocations then free_fixed() does not need to
	 * be implemented. This behavior exists for legacy reasons and should
	 * not be propagated to new allocators.
	 */
	u64  (*alloc_fixed)(struct gk20a_allocator *allocator,
			     u64 base, u64 len);
	void (*free_fixed)(struct gk20a_allocator *allocator,
			    u64 base, u64 len);

	/*
	 * Allow allocators to reserve space for carveouts.
	 */
	int  (*reserve_carveout)(struct gk20a_allocator *allocator,
				 struct gk20a_alloc_carveout *co);
	void (*release_carveout)(struct gk20a_allocator *allocator,
				 struct gk20a_alloc_carveout *co);

	/*
	 * Returns info about the allocator.
	 */
	u64  (*base)(struct gk20a_allocator *allocator);
	u64  (*length)(struct gk20a_allocator *allocator);
	u64  (*end)(struct gk20a_allocator *allocator);
	int  (*inited)(struct gk20a_allocator *allocator);
	u64  (*space)(struct gk20a_allocator *allocator);

	/* Destructor. */
	void (*fini)(struct gk20a_allocator *allocator);

	/* Debugging. */
	void (*print_stats)(struct gk20a_allocator *allocator,
			    struct seq_file *s, int lock);
};

struct gk20a_allocator {
	char name[32];
	struct mutex lock;

	void *priv;
	const struct gk20a_allocator_ops *ops;

	struct dentry *debugfs_entry;
	bool debug;				/* Control for debug msgs. */
};

struct gk20a_alloc_carveout {
	const char *name;
	u64 base;
	u64 length;

	struct gk20a_allocator *allocator;

	/*
	 * For usage by the allocator implementation.
	 */
	struct list_head co_entry;
};

#define GK20A_CARVEOUT(__name, __base, __length)	\
	{						\
		.name = (__name),			\
		.base = (__base),			\
		.length = (__length)			\
	}

/*
 * These are the available allocator flags.
 *
 *   GPU_ALLOC_GVA_SPACE
 *
 *     This flag makes sense for the buddy allocator only. It specifies that the
 *     allocator will be used for managing a GVA space. When managing GVA spaces
 *     special care has to be taken to ensure that allocations of similar PTE
 *     sizes are placed in the same PDE block. This allows the higher level
 *     code to skip defining both small and large PTE tables for every PDE. That
 *     can save considerable memory for address spaces that have a lot of
 *     allocations.
 *
 *   GPU_ALLOC_NO_ALLOC_PAGE
 *
 *     For any allocator that needs to manage a resource in a latency critical
 *     path this flag specifies that the allocator should not use any kmalloc()
 *     or similar functions during normal operation. Initialization routines
 *     may still use kmalloc(). This prevents the possibility of long waits for
 *     pages when using alloc_page(). Currently only the bitmap allocator
 *     implements this functionality.
 *
 *     Also note that if you accept this flag then you must also define the
 *     free_fixed() function. Since no meta-data is allocated to help free
 *     allocations you need to keep track of the meta-data yourself (in this
 *     case the base and length of the allocation as opposed to just the base
 *     of the allocation).
 *
 *   GPU_ALLOC_4K_VIDMEM_PAGES
 *
 *     We manage vidmem pages at a large page granularity for performance
 *     reasons; however, this can lead to wasting memory. For page allocators
 *     setting this flag will tell the allocator to manage pools of 4K pages
 *     inside internally allocated large pages.
 *
 *     Currently this flag is ignored since the only usage of the page allocator
 *     uses a 4K block size already. However, this flag has been reserved since
 *     it will be necessary in the future.
 *
 *   GPU_ALLOC_FORCE_CONTIG
 *
 *     Force allocations to be contiguous. Currently only relevant for page
 *     allocators since all other allocators are naturally contiguous.
 *
 *   GPU_ALLOC_NO_SCATTER_GATHER
 *
 *     The page allocator normally returns a scatter gather data structure for
 *     allocations (to handle discontiguous pages). However, at times that can
 *     be annoying so this flag forces the page allocator to return a u64
 *     pointing to the allocation base (requires GPU_ALLOC_FORCE_CONTIG to be
 *     set as well).
 */
#define GPU_ALLOC_GVA_SPACE		0x1
#define GPU_ALLOC_NO_ALLOC_PAGE		0x2
#define GPU_ALLOC_4K_VIDMEM_PAGES	0x4
#define GPU_ALLOC_FORCE_CONTIG		0x8
#define GPU_ALLOC_NO_SCATTER_GATHER	0x10

static inline void alloc_lock(struct gk20a_allocator *a)
{
	mutex_lock(&a->lock);
}

static inline void alloc_unlock(struct gk20a_allocator *a)
{
	mutex_unlock(&a->lock);
}

/*
 * Buddy allocator specific initializers.
 */
int  __gk20a_buddy_allocator_init(struct gk20a *g, struct gk20a_allocator *a,
				  struct vm_gk20a *vm, const char *name,
				  u64 base, u64 size, u64 blk_size,
				  u64 max_order, u64 flags);
int  gk20a_buddy_allocator_init(struct gk20a *g, struct gk20a_allocator *a,
				const char *name, u64 base, u64 size,
				u64 blk_size, u64 flags);

/*
 * Bitmap initializers.
 */
int gk20a_bitmap_allocator_init(struct gk20a *g, struct gk20a_allocator *a,
				const char *name, u64 base, u64 length,
				u64 blk_size, u64 flags);

/*
 * Page allocator initializers.
 */
int gk20a_page_allocator_init(struct gk20a *g, struct gk20a_allocator *a,
			      const char *name, u64 base, u64 length,
			      u64 blk_size, u64 flags);

/*
 * Lockless allocatior initializers.
 * Note: This allocator can only allocate fixed-size structures of a
 * pre-defined size.
 */
int gk20a_lockless_allocator_init(struct gk20a *g, struct gk20a_allocator *a,
				  const char *name, u64 base, u64 length,
				  u64 struct_size, u64 flags);

#define GPU_BALLOC_MAX_ORDER		31

/*
 * Allocator APIs.
 */
u64  gk20a_alloc(struct gk20a_allocator *allocator, u64 len);
void gk20a_free(struct gk20a_allocator *allocator, u64 addr);

u64  gk20a_alloc_fixed(struct gk20a_allocator *allocator, u64 base, u64 len);
void gk20a_free_fixed(struct gk20a_allocator *allocator, u64 base, u64 len);

int  gk20a_alloc_reserve_carveout(struct gk20a_allocator *a,
				  struct gk20a_alloc_carveout *co);
void gk20a_alloc_release_carveout(struct gk20a_allocator *a,
				  struct gk20a_alloc_carveout *co);

u64  gk20a_alloc_base(struct gk20a_allocator *a);
u64  gk20a_alloc_length(struct gk20a_allocator *a);
u64  gk20a_alloc_end(struct gk20a_allocator *a);
u64  gk20a_alloc_initialized(struct gk20a_allocator *a);
u64  gk20a_alloc_space(struct gk20a_allocator *a);

void gk20a_alloc_destroy(struct gk20a_allocator *allocator);

void gk20a_alloc_print_stats(struct gk20a_allocator *a,
			     struct seq_file *s, int lock);

/*
 * Common functionality for the internals of the allocators.
 */
void gk20a_init_alloc_debug(struct gk20a *g, struct gk20a_allocator *a);
void gk20a_fini_alloc_debug(struct gk20a_allocator *a);

int  __gk20a_alloc_common_init(struct gk20a_allocator *a,
			       const char *name, void *priv, bool dbg,
			       const struct gk20a_allocator_ops *ops);

static inline void gk20a_alloc_enable_dbg(struct gk20a_allocator *a)
{
	a->debug = true;
}

static inline void gk20a_alloc_disable_dbg(struct gk20a_allocator *a)
{
	a->debug = false;
}

/*
 * Debug stuff.
 */
extern u32 gk20a_alloc_tracing_on;

void gk20a_alloc_debugfs_init(struct device *dev);

#define gk20a_alloc_trace_func()			\
	do {						\
		if (gk20a_alloc_tracing_on)		\
			trace_printk("%s\n", __func__);	\
	} while (0)

#define gk20a_alloc_trace_func_done()				\
	do {							\
		if (gk20a_alloc_tracing_on)			\
			trace_printk("%s_done\n", __func__);	\
	} while (0)

#define __alloc_pstat(seq, allocator, fmt, arg...)		\
	do {							\
		if (s)						\
			seq_printf(seq, fmt, ##arg);		\
		else						\
			alloc_dbg(allocator, fmt, ##arg);	\
	} while (0)

#define __alloc_dbg(a, fmt, arg...)					\
	pr_info("%-25s %25s() " fmt, (a)->name, __func__, ##arg)

#if defined(ALLOCATOR_DEBUG)
/*
 * Always print the debug messages...
 */
#define alloc_dbg(a, fmt, arg...) __alloc_dbg(a, fmt, ##arg)
#else
/*
 * Only print debug messages if debug is enabled for a given allocator.
 */
#define alloc_dbg(a, fmt, arg...)			\
	do {						\
		if ((a)->debug)				\
			__alloc_dbg((a), fmt, ##arg);	\
	} while (0)

#endif

#endif /* GK20A_ALLOCATOR_H */
