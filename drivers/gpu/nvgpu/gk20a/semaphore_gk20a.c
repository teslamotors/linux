/*
 * drivers/video/tegra/host/gk20a/semaphore_gk20a.c
 *
 * GK20A Semaphores
 *
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

#define pr_fmt(fmt) "gpu_sema: " fmt

#include <linux/dma-mapping.h>
#include <linux/highmem.h>
#include <linux/slab.h>

#include <asm/pgtable.h>

#include "gk20a.h"
#include "mm_gk20a.h"
#include "semaphore_gk20a.h"

#define __lock_sema_sea(s)						\
	do {								\
		gpu_sema_verbose_dbg("Acquiring sema lock...");		\
		mutex_lock(&s->sea_lock);				\
		gpu_sema_verbose_dbg("Sema lock aquried!");		\
	} while (0)

#define __unlock_sema_sea(s)						\
	do {								\
		mutex_unlock(&s->sea_lock);				\
		gpu_sema_verbose_dbg("Released sema lock");		\
	} while (0)

/*
 * Return the sema_sea pointer.
 */
struct gk20a_semaphore_sea *gk20a_semaphore_get_sea(struct gk20a *g)
{
	return g->sema_sea;
}

static int __gk20a_semaphore_sea_grow(struct gk20a_semaphore_sea *sea)
{
	int ret = 0;
	struct gk20a *gk20a = sea->gk20a;

	__lock_sema_sea(sea);

	ret = gk20a_gmmu_alloc_attr_sys(gk20a, DMA_ATTR_NO_KERNEL_MAPPING,
				    PAGE_SIZE * SEMAPHORE_POOL_COUNT,
				    &sea->sea_mem);
	if (ret)
		goto out;

	sea->ro_sg_table = sea->sea_mem.sgt;
	sea->size = SEMAPHORE_POOL_COUNT;
	sea->map_size = SEMAPHORE_POOL_COUNT * PAGE_SIZE;

out:
	__unlock_sema_sea(sea);
	return ret;
}

/*
 * Create the semaphore sea. Only create it once - subsequent calls to this will
 * return the originally created sea pointer.
 */
struct gk20a_semaphore_sea *gk20a_semaphore_sea_create(struct gk20a *g)
{
	if (g->sema_sea)
		return g->sema_sea;

	g->sema_sea = kzalloc(sizeof(*g->sema_sea), GFP_KERNEL);
	if (!g->sema_sea)
		return NULL;

	g->sema_sea->size = 0;
	g->sema_sea->page_count = 0;
	g->sema_sea->gk20a = g;
	INIT_LIST_HEAD(&g->sema_sea->pool_list);
	mutex_init(&g->sema_sea->sea_lock);

	if (__gk20a_semaphore_sea_grow(g->sema_sea))
		goto cleanup;

	gpu_sema_dbg("Created semaphore sea!");
	return g->sema_sea;

cleanup:
	kfree(g->sema_sea);
	g->sema_sea = NULL;
	gpu_sema_dbg("Failed to creat semaphore sea!");
	return NULL;
}

static int __semaphore_bitmap_alloc(unsigned long *bitmap, unsigned long len)
{
	unsigned long idx = find_first_zero_bit(bitmap, len);

	if (idx == len)
		return -ENOSPC;

	set_bit(idx, bitmap);

	return (int)idx;
}

/*
 * Allocate a pool from the sea.
 */
struct gk20a_semaphore_pool *gk20a_semaphore_pool_alloc(
				struct gk20a_semaphore_sea *sea)
{
	struct gk20a_semaphore_pool *p;
	unsigned long page_idx;
	int err = 0;

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (!p)
		return ERR_PTR(-ENOMEM);

	__lock_sema_sea(sea);

	page_idx = __semaphore_bitmap_alloc(sea->pools_alloced,
					    SEMAPHORE_POOL_COUNT);
	if (page_idx < 0) {
		err = page_idx;
		goto fail;
	}

	p->page = sea->sea_mem.pages[page_idx];
	p->ro_sg_table = sea->ro_sg_table;
	p->page_idx = page_idx;
	p->sema_sea = sea;
	INIT_LIST_HEAD(&p->hw_semas);
	kref_init(&p->ref);
	mutex_init(&p->pool_lock);

	sea->page_count++;
	list_add(&p->pool_list_entry, &sea->pool_list);
	__unlock_sema_sea(sea);

	gpu_sema_dbg("Allocated semaphore pool: page-idx=%d", p->page_idx);

	return p;

fail:
	__unlock_sema_sea(sea);
	kfree(p);
	gpu_sema_dbg("Failed to allocate semaphore pool!");
	return ERR_PTR(err);
}

/*
 * Map a pool into the passed vm's address space. This handles both the fixed
 * global RO mapping and the non-fixed private RW mapping.
 */
int gk20a_semaphore_pool_map(struct gk20a_semaphore_pool *p,
			     struct vm_gk20a *vm)
{
	int ents, err = 0;
	u64 addr;

	gpu_sema_dbg("Mapping sempahore pool! (idx=%d)", p->page_idx);

	p->cpu_va = vmap(&p->page, 1, 0,
			 pgprot_writecombine(PAGE_KERNEL));

	gpu_sema_dbg("  %d: CPU VA = 0x%p!", p->page_idx, p->cpu_va);

	/* First do the RW mapping. */
	p->rw_sg_table = kzalloc(sizeof(*p->rw_sg_table), GFP_KERNEL);
	if (!p->rw_sg_table)
		return -ENOMEM;

	err = sg_alloc_table_from_pages(p->rw_sg_table, &p->page, 1, 0,
					PAGE_SIZE, GFP_KERNEL);
	if (err) {
		err = -ENOMEM;
		goto fail;
	}

	/* Add IOMMU mapping... */
	ents = dma_map_sg(dev_from_vm(vm), p->rw_sg_table->sgl, 1,
			  DMA_BIDIRECTIONAL);
	if (ents != 1) {
		err = -ENOMEM;
		goto fail_free_sgt;
	}

	gpu_sema_dbg("  %d: DMA addr = 0x%pad", p->page_idx,
		     &sg_dma_address(p->rw_sg_table->sgl));

	/* Map into the GPU... Doesn't need to be fixed. */
	p->gpu_va = gk20a_gmmu_map(vm, &p->rw_sg_table, PAGE_SIZE,
				   0, gk20a_mem_flag_none, false,
				   APERTURE_SYSMEM);
	if (!p->gpu_va) {
		err = -ENOMEM;
		goto fail_unmap_sgt;
	}

	gpu_sema_dbg("  %d: GPU read-write VA = 0x%llx", p->page_idx,
		     p->gpu_va);

	/*
	 * And now the global mapping. Take the sea lock so that we don't race
	 * with a concurrent remap.
	 */
	__lock_sema_sea(p->sema_sea);

	BUG_ON(p->mapped);
	addr = gk20a_gmmu_fixed_map(vm, &p->sema_sea->ro_sg_table,
				    p->sema_sea->gpu_va, p->sema_sea->map_size,
				    0,
				    gk20a_mem_flag_read_only,
				    false,
				    APERTURE_SYSMEM);
	if (!addr) {
		err = -ENOMEM;
		BUG();
		goto fail_unlock;
	}
	p->gpu_va_ro = addr;
	p->mapped = 1;

	gpu_sema_dbg("  %d: GPU read-only  VA = 0x%llx", p->page_idx,
		     p->gpu_va_ro);

	__unlock_sema_sea(p->sema_sea);

	return 0;

fail_unlock:
	__unlock_sema_sea(p->sema_sea);
fail_unmap_sgt:
	dma_unmap_sg(dev_from_vm(vm), p->rw_sg_table->sgl, 1,
		     DMA_BIDIRECTIONAL);
fail_free_sgt:
	sg_free_table(p->rw_sg_table);
fail:
	kfree(p->rw_sg_table);
	p->rw_sg_table = NULL;
	gpu_sema_dbg("  %d: Failed to map semaphore pool!", p->page_idx);
	return err;
}

/*
 * Unmap a semaphore_pool.
 */
void gk20a_semaphore_pool_unmap(struct gk20a_semaphore_pool *p,
				struct vm_gk20a *vm)
{
	struct gk20a_semaphore_int *hw_sema;

	kunmap(p->cpu_va);

	/* First the global RO mapping... */
	__lock_sema_sea(p->sema_sea);
	gk20a_gmmu_unmap(vm, p->gpu_va_ro,
			 p->sema_sea->map_size, gk20a_mem_flag_none);
	p->ro_sg_table = NULL;
	__unlock_sema_sea(p->sema_sea);

	/* And now the private RW mapping. */
	gk20a_gmmu_unmap(vm, p->gpu_va, PAGE_SIZE, gk20a_mem_flag_none);
	p->gpu_va = 0;

	dma_unmap_sg(dev_from_vm(vm), p->rw_sg_table->sgl, 1,
		     DMA_BIDIRECTIONAL);

	sg_free_table(p->rw_sg_table);
	kfree(p->rw_sg_table);
	p->rw_sg_table = NULL;

	list_for_each_entry(hw_sema, &p->hw_semas, hw_sema_list)
		/*
		 * Make sure the mem addresses are all NULL so if this gets
		 * reused we will fault.
		 */
		hw_sema->value = NULL;

	gpu_sema_dbg("Unmapped semaphore pool! (idx=%d)", p->page_idx);
}

/*
 * Completely free a sempahore_pool. You should make sure this pool is not
 * mapped otherwise there's going to be a memory leak.
 */
static void gk20a_semaphore_pool_free(struct kref *ref)
{
	struct gk20a_semaphore_pool *p =
		container_of(ref, struct gk20a_semaphore_pool, ref);
	struct gk20a_semaphore_sea *s = p->sema_sea;
	struct gk20a_semaphore_int *hw_sema, *tmp;

	WARN_ON(p->gpu_va || p->rw_sg_table || p->ro_sg_table);

	__lock_sema_sea(s);
	list_del(&p->pool_list_entry);
	clear_bit(p->page_idx, s->pools_alloced);
	s->page_count--;
	__unlock_sema_sea(s);

	list_for_each_entry_safe(hw_sema, tmp, &p->hw_semas, hw_sema_list)
		kfree(hw_sema);

	gpu_sema_dbg("Freed semaphore pool! (idx=%d)", p->page_idx);
	kfree(p);
}

void gk20a_semaphore_pool_get(struct gk20a_semaphore_pool *p)
{
	kref_get(&p->ref);
}

void gk20a_semaphore_pool_put(struct gk20a_semaphore_pool *p)
{
	kref_put(&p->ref, gk20a_semaphore_pool_free);
}

/*
 * Get the address for a semaphore_pool - if global is true then return the
 * global RO address instead of the RW address owned by the semaphore's VM.
 */
u64 __gk20a_semaphore_pool_gpu_va(struct gk20a_semaphore_pool *p, bool global)
{
	if (!global)
		return p->gpu_va;

	return p->gpu_va_ro + (PAGE_SIZE * p->page_idx);
}

static int __gk20a_init_hw_sema(struct channel_gk20a *ch)
{
	int hw_sema_idx;
	int ret = 0;
	struct gk20a_semaphore_int *hw_sema;
	struct gk20a_semaphore_pool *p = ch->vm->sema_pool;

	BUG_ON(!p);

	mutex_lock(&p->pool_lock);

	/* Find an available HW semaphore. */
	hw_sema_idx = __semaphore_bitmap_alloc(p->semas_alloced,
					       PAGE_SIZE / SEMAPHORE_SIZE);
	if (hw_sema_idx < 0) {
		ret = hw_sema_idx;
		goto fail;
	}

	hw_sema = kzalloc(sizeof(struct gk20a_semaphore_int), GFP_KERNEL);
	if (!hw_sema) {
		ret = -ENOMEM;
		goto fail_free_idx;
	}

	ch->hw_sema = hw_sema;
	hw_sema->ch = ch;
	hw_sema->p = p;
	hw_sema->idx = hw_sema_idx;
	hw_sema->offset = SEMAPHORE_SIZE * hw_sema_idx;
	atomic_set(&hw_sema->next_value, 0);
	hw_sema->value = p->cpu_va + hw_sema->offset;
	writel(0, hw_sema->value);

	list_add(&hw_sema->hw_sema_list, &p->hw_semas);

	mutex_unlock(&p->pool_lock);

	return 0;

fail_free_idx:
	clear_bit(hw_sema_idx, p->semas_alloced);
fail:
	mutex_unlock(&p->pool_lock);
	return ret;
}

/*
 * Free the channel used semaphore index
 */
void gk20a_semaphore_free_hw_sema(struct channel_gk20a *ch)
{
	struct gk20a_semaphore_pool *p = ch->vm->sema_pool;

	BUG_ON(!p);

	mutex_lock(&p->pool_lock);

	clear_bit(ch->hw_sema->idx, p->semas_alloced);

	/* Make sure that when the ch is re-opened it will get a new HW sema. */
	list_del(&ch->hw_sema->hw_sema_list);
	kfree(ch->hw_sema);
	ch->hw_sema = NULL;

	mutex_unlock(&p->pool_lock);
}

/*
 * Allocate a semaphore from the passed pool.
 *
 * Since semaphores are ref-counted there's no explicit free for external code
 * to use. When the ref-count hits 0 the internal free will happen.
 */
struct gk20a_semaphore *gk20a_semaphore_alloc(struct channel_gk20a *ch)
{
	struct gk20a_semaphore *s;
	int ret;

	if (!ch->hw_sema) {
		ret = __gk20a_init_hw_sema(ch);
		if (ret)
			return NULL;
	}

	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (!s)
		return NULL;

	kref_init(&s->ref);
	s->hw_sema = ch->hw_sema;
	atomic_set(&s->value, 0);

	/*
	 * Take a ref on the pool so that we can keep this pool alive for
	 * as long as this semaphore is alive.
	 */
	gk20a_semaphore_pool_get(s->hw_sema->p);

	gpu_sema_dbg("Allocated semaphore (c=%d)", ch->hw_chid);

	return s;
}

static void gk20a_semaphore_free(struct kref *ref)
{
	struct gk20a_semaphore *s =
		container_of(ref, struct gk20a_semaphore, ref);

	gk20a_semaphore_pool_put(s->hw_sema->p);

	kfree(s);
}

void gk20a_semaphore_put(struct gk20a_semaphore *s)
{
	kref_put(&s->ref, gk20a_semaphore_free);
}

void gk20a_semaphore_get(struct gk20a_semaphore *s)
{
	kref_get(&s->ref);
}
