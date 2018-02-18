/*
 * drivers/video/tegra/nvmap/nvmap_pp.c
 *
 * Manage page pools to speed up page allocation.
 *
 * Copyright (c) 2009-2016, NVIDIA CORPORATION. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/moduleparam.h>
#include <linux/nodemask.h>
#include <linux/shrinker.h>
#include <linux/kthread.h>
#include <linux/debugfs.h>
#include <linux/freezer.h>
#include <linux/highmem.h>

#include "nvmap_priv.h"

#define NVMAP_TEST_PAGE_POOL_SHRINKER     1
#define PENDING_PAGES_SIZE                (SZ_1M / PAGE_SIZE)

static bool enable_pp = 1;
static int pool_size;

static struct task_struct *background_allocator;
static DECLARE_WAIT_QUEUE_HEAD(nvmap_bg_wait);

#ifdef CONFIG_NVMAP_PAGE_POOL_DEBUG
static inline void __pp_dbg_var_add(u64 *dbg_var, u32 nr)
{
	*dbg_var += nr;
}
#else
#define __pp_dbg_var_add(dbg_var, nr)
#endif

#define pp_alloc_add(pool, nr) __pp_dbg_var_add(&(pool)->allocs, nr)
#define pp_fill_add(pool, nr)  __pp_dbg_var_add(&(pool)->fills, nr)
#define pp_hit_add(pool, nr)   __pp_dbg_var_add(&(pool)->hits, nr)
#define pp_miss_add(pool, nr)  __pp_dbg_var_add(&(pool)->misses, nr)

static int __nvmap_page_pool_fill_lots_locked(struct nvmap_page_pool *pool,
				       struct page **pages, u32 nr);

/*
 * Make sure any data in the caches is cleaned out before
 * passing these pages to userspace. otherwise, It can lead to
 * corruption in pages that get mapped as something
 * other than WB in userspace and leaked kernel data.
 *
 * Must be called with pool->lock held.
 */
static void pp_clean_cache(struct nvmap_page_pool *pool)
{
	struct page *page;
	u32 dirty_pages = pool->dirty_pages;

	if (!dirty_pages)
		return;
	if (nvmap_cache_maint_by_set_ways &&
		(dirty_pages >= (cache_maint_inner_threshold >> PAGE_SHIFT))) {
		inner_clean_cache_all();
		outer_clean_all();
	} else {
		list_for_each_entry_reverse(page, &pool->page_list, lru) {
			nvmap_clean_cache_page(page);
			dirty_pages--;
			if (!dirty_pages)
				break;
		}
		BUG_ON(dirty_pages);
	}
	pool->dirty_pages = 0;
}

static inline struct page *get_zero_list_page(struct nvmap_page_pool *pool)
{
	struct page *page;

	if (list_empty(&pool->zero_list))
		return NULL;

	page = list_first_entry(&pool->zero_list, struct page, lru);
	list_del(&page->lru);

	pool->to_zero--;

	return page;
}

static inline struct page *get_page_list_page(struct nvmap_page_pool *pool)
{
	struct page *page;

	if (list_empty(&pool->page_list))
		return NULL;

	page = list_first_entry(&pool->page_list, struct page, lru);
	list_del(&page->lru);

	pool->count--;

	return page;
}

static inline bool nvmap_bg_should_run(struct nvmap_page_pool *pool)
{
	bool ret;

	mutex_lock(&pool->lock);
	ret = (pool->to_zero > 0);
	mutex_unlock(&pool->lock);

	return ret;
}

static int nvmap_pp_zero_pages(struct page **pages, int nr)
{
	int i;

	for (i = 0; i < nr; i++)
		clear_highpage(pages[i]);

	return 0;
}

static void nvmap_pp_do_background_zero_pages(struct nvmap_page_pool *pool)
{
	int i;
	struct page *page;
	int ret;

	/*
	 * Statically declared array of pages to be zeroed in a batch,
	 * local to this thread but too big for the stack.
	 */
	static struct page *pending_zero_pages[PENDING_PAGES_SIZE];

	mutex_lock(&pool->lock);
	for (i = 0; i < PENDING_PAGES_SIZE; i++) {
		page = get_zero_list_page(pool);
		if (page == NULL)
			break;
		pending_zero_pages[i] = page;
	}
	mutex_unlock(&pool->lock);

	ret = nvmap_pp_zero_pages(pending_zero_pages, i);
	if (ret < 0) {
		ret = 0;
		goto out;
	}

	mutex_lock(&pool->lock);
	ret = __nvmap_page_pool_fill_lots_locked(pool, pending_zero_pages, i);
	mutex_unlock(&pool->lock);

out:
	for (; ret < i; ret++)
		__free_page(pending_zero_pages[ret]);
}

/*
 * This thread fills the page pools with zeroed pages. We avoid releasing the
 * pages directly back into the page pools since we would then have to zero
 * them ourselves. Instead it is easier to just reallocate zeroed pages. This
 * happens in the background so that the overhead of allocating zeroed pages is
 * not directly seen by userspace. Of course if the page pools are empty user
 * space will suffer.
 */
static int nvmap_background_zero_thread(void *arg)
{
	struct nvmap_page_pool *pool = &nvmap_dev->pool;
	struct sched_param param = { .sched_priority = 0 };

	pr_info("PP zeroing thread starting.\n");

	set_freezable();
	sched_setscheduler(current, SCHED_IDLE, &param);

	while (!kthread_should_stop()) {
		while (nvmap_bg_should_run(pool))
			nvmap_pp_do_background_zero_pages(pool);

		/* clean cache in the background so that allocations immediately
		 * after fill don't suffer the cache clean overhead.
		 */
		if (pool->dirty_pages >=
		    (cache_maint_inner_threshold >> PAGE_SHIFT)) {
			mutex_lock(&pool->lock);
			pp_clean_cache(pool);
			mutex_unlock(&pool->lock);
		}
		wait_event_freezable(nvmap_bg_wait,
				nvmap_bg_should_run(pool) ||
				kthread_should_stop());
	}

	return 0;
}

/*
 * This removes a page from the page pool. If ignore_disable is set, then
 * the enable_pp flag is ignored.
 */
static struct page *nvmap_page_pool_alloc_locked(struct nvmap_page_pool *pool,
						 int force_alloc)
{
	struct page *page;

	if (!force_alloc && !enable_pp)
		return NULL;

	if (list_empty(&pool->page_list)) {
		pp_miss_add(pool, 1);
		return NULL;
	}

	if (IS_ENABLED(CONFIG_NVMAP_PAGE_POOL_DEBUG))
		BUG_ON(pool->count == 0);

	pp_clean_cache(pool);
	page = get_page_list_page(pool);
	if (!page)
		return NULL;

	/* Sanity check. */
	if (IS_ENABLED(CONFIG_NVMAP_PAGE_POOL_DEBUG)) {
		atomic_dec(&page->_count);
		BUG_ON(atomic_read(&page->_count) != 1);
	}

	pp_alloc_add(pool, 1);
	pp_hit_add(pool, 1);

	return page;
}

/*
 * Alloc a bunch of pages from the page pool. This will alloc as many as it can
 * and return the number of pages allocated. Pages are placed into the passed
 * array in a linear fashion starting from index 0.
 */
int nvmap_page_pool_alloc_lots(struct nvmap_page_pool *pool,
				struct page **pages, u32 nr)
{
	u32 real_nr;
	u32 ind = 0;

	if (!enable_pp)
		return 0;

	mutex_lock(&pool->lock);
	pp_clean_cache(pool);

	real_nr = min_t(u32, nr, pool->count);

	while (real_nr--) {
		struct page *page;
		if (IS_ENABLED(CONFIG_NVMAP_PAGE_POOL_DEBUG))
			BUG_ON(list_empty(&pool->page_list));
		page = get_page_list_page(pool);
		pages[ind++] = page;
		if (IS_ENABLED(CONFIG_NVMAP_PAGE_POOL_DEBUG)) {
			atomic_dec(&page->_count);
			BUG_ON(atomic_read(&page->_count) != 1);
		}
	}
	mutex_unlock(&pool->lock);

	pp_alloc_add(pool, ind);
	pp_hit_add(pool, ind);
	pp_miss_add(pool, nr - ind);

	return ind;
}

/*
 * Fill a bunch of pages into the page pool. This will fill as many as it can
 * and return the number of pages filled. Pages are used from the start of the
 * passed page pointer array in a linear fashion.
 *
 * You must lock the page pool before using this.
 */
static int __nvmap_page_pool_fill_lots_locked(struct nvmap_page_pool *pool,
				       struct page **pages, u32 nr)
{
	u32 real_nr;
	u32 ind = 0;

	if (!enable_pp)
		return 0;

	real_nr = min_t(u32, pool->max - pool->count, nr);
	if (real_nr == 0)
		return 0;

	while (real_nr--) {
		if (IS_ENABLED(CONFIG_NVMAP_PAGE_POOL_DEBUG)) {
			atomic_inc(&pages[ind]->_count);
			BUG_ON(atomic_read(&pages[ind]->_count) != 2);
		}
		list_add_tail(&pages[ind++]->lru, &pool->page_list);
	}

	pool->dirty_pages += ind;
	pool->count += ind;
	pp_fill_add(pool, ind);

	return ind;
}

int nvmap_page_pool_fill_lots(struct nvmap_page_pool *pool,
				       struct page **pages, u32 nr)
{
	int ret = 0;

	mutex_lock(&pool->lock);
	if (zero_memory) {
		int i;

		nr = min(nr, pool->max - pool->count - pool->to_zero);

		for (i = 0; i < nr; i++) {
			/* If page has additonal referecnces, Don't add it into
			 * page pool. get_user_pages() on mmap'ed nvmap handle can
			 * hold a refcount on the page. These pages can't be
			 * reused till the additional refs are dropped.
			 */
			if (atomic_read(&pages[i]->_count) > 1) {
				__free_page(pages[i]);
			} else {
				list_add_tail(&pages[i]->lru, &pool->zero_list);
				pool->to_zero++;
			}
		}

		if (i)
			wake_up_interruptible(&nvmap_bg_wait);
		ret = i;
	} else {
		int i;
		bool add_to_pp = true;

		/* This path is only for debug purposes.
		 * Need not be efficient.
		 */
		for (i = 0; i < nr; i++) {
			if (atomic_read(&pages[i]->_count) > 1) {
				add_to_pp = false;
				break;
			}
		}

		if (add_to_pp)
			ret = __nvmap_page_pool_fill_lots_locked(pool, pages, nr);
	}
	mutex_unlock(&pool->lock);

	return ret;
}

/*
 * Free the passed number of pages from the page pool. This happen irregardless
 * of whether ther page pools are enabled. This lets one disable the page pools
 * and then free all the memory therein.
 */
static int nvmap_page_pool_free(struct nvmap_page_pool *pool, int nr_free)
{
	int i = nr_free;
	struct page *page;

	if (!nr_free)
		return nr_free;

	mutex_lock(&pool->lock);
	while (i) {
		page = get_zero_list_page(pool);
		if (!page)
			page = nvmap_page_pool_alloc_locked(pool, 1);
		if (!page)
			break;
		__free_page(page);
		i--;
	}
	mutex_unlock(&pool->lock);

	return i;
}

ulong nvmap_page_pool_get_unused_pages(void)
{
	int total = 0;

	if (!nvmap_dev)
		return 0;

	total = nvmap_dev->pool.count + nvmap_dev->pool.to_zero;

	return total;
}

/*
 * Remove and free to the system all the pages currently in the page
 * pool. This operation will happen even if the page pools are disabled.
 */
int nvmap_page_pool_clear(void)
{
	struct page *page;
	struct nvmap_page_pool *pool = &nvmap_dev->pool;

	mutex_lock(&pool->lock);

	while ((page = nvmap_page_pool_alloc_locked(pool, 1)) != NULL)
		__free_page(page);

	while (!list_empty(&pool->zero_list)) {
		page = get_zero_list_page(pool);
		__free_page(page);
	}

	/* For some reason, if an error occured... */
	if (!list_empty(&pool->page_list) || !list_empty(&pool->zero_list)) {
		mutex_unlock(&pool->lock);
		return -ENOMEM;
	}

	mutex_unlock(&pool->lock);

	return 0;
}

/*
 * Resizes the page pool to the passed size. If the passed size is 0 then
 * all associated resources are released back to the system. This operation
 * will only occur if the page pools are enabled.
 */
static void nvmap_page_pool_resize(struct nvmap_page_pool *pool, int size)
{
	mutex_lock(&pool->lock);

	while (pool->count > size)
		__free_page(nvmap_page_pool_alloc_locked(pool, 0));

	pr_debug("page pool resized to %d from %d pages\n", size, pool->max);
	pool->max = size;

	mutex_unlock(&pool->lock);
}

static unsigned long nvmap_page_pool_count_objects(struct shrinker *shrinker,
						   struct shrink_control *sc)
{
	return nvmap_page_pool_get_unused_pages();
}

static unsigned long nvmap_page_pool_scan_objects(struct shrinker *shrinker,
						  struct shrink_control *sc)
{
	unsigned long remaining;

	pr_debug("sh_pages=%lu", sc->nr_to_scan);

	remaining = nvmap_page_pool_free(&nvmap_dev->pool, sc->nr_to_scan);

	return (remaining == sc->nr_to_scan) ? \
			   SHRINK_STOP : (sc->nr_to_scan - remaining);
}

static struct shrinker nvmap_page_pool_shrinker = {
	.count_objects = nvmap_page_pool_count_objects,
	.scan_objects = nvmap_page_pool_scan_objects,
	.seeks = 1,
};

static void shrink_page_pools(int *total_pages, int *available_pages)
{
	struct shrink_control sc;

	if (*total_pages == 0) {
		sc.gfp_mask = GFP_KERNEL;
		sc.nr_to_scan = 0;
		*total_pages = nvmap_page_pool_count_objects(NULL, &sc);
	}
	sc.nr_to_scan = *total_pages;
	nvmap_page_pool_scan_objects(NULL, &sc);
	*available_pages = nvmap_page_pool_count_objects(NULL, &sc);
}

#if NVMAP_TEST_PAGE_POOL_SHRINKER
static int shrink_pp;
static int shrink_set(const char *arg, const struct kernel_param *kp)
{
	int cpu = smp_processor_id();
	unsigned long long t1, t2;
	int total_pages, available_pages;

	param_set_int(arg, kp);

	if (shrink_pp) {
		total_pages = shrink_pp;
		t1 = cpu_clock(cpu);
		shrink_page_pools(&total_pages, &available_pages);
		t2 = cpu_clock(cpu);
		pr_debug("shrink page pools: time=%lldns, "
			"total_pages_released=%d, free_pages_available=%d",
			t2-t1, total_pages, available_pages);
	}
	return 0;
}

static int shrink_get(char *buff, const struct kernel_param *kp)
{
	return param_get_int(buff, kp);
}

static struct kernel_param_ops shrink_ops = {
	.get = shrink_get,
	.set = shrink_set,
};

module_param_cb(shrink_page_pools, &shrink_ops, &shrink_pp, 0644);
#endif

static int enable_pp_set(const char *arg, const struct kernel_param *kp)
{
	int ret;

	ret = param_set_bool(arg, kp);
	if (ret)
		return ret;

	if (!enable_pp)
		nvmap_page_pool_clear();

	return 0;
}

static int enable_pp_get(char *buff, const struct kernel_param *kp)
{
	return param_get_int(buff, kp);
}

static struct kernel_param_ops enable_pp_ops = {
	.get = enable_pp_get,
	.set = enable_pp_set,
};

module_param_cb(enable_page_pools, &enable_pp_ops, &enable_pp, 0644);

static int pool_size_set(const char *arg, const struct kernel_param *kp)
{
	param_set_int(arg, kp);

	if (pool_size  < 0) {
		pool_size = nvmap_dev->pool.max;
		pr_err("pool_size can't be set to -ve value!\n");
		return -EINVAL;
	} else if (pool_size != nvmap_dev->pool.max) {
		nvmap_page_pool_resize(&nvmap_dev->pool, pool_size);
	}

	return 0;
}

static int pool_size_get(char *buff, const struct kernel_param *kp)
{
	return param_get_int(buff, kp);
}

static struct kernel_param_ops pool_size_ops = {
	.get = pool_size_get,
	.set = pool_size_set,
};

module_param_cb(pool_size, &pool_size_ops, &pool_size, 0644);

int nvmap_page_pool_debugfs_init(struct dentry *nvmap_root)
{
	struct dentry *pp_root;

	if (!nvmap_root)
		return -ENODEV;

	pp_root = debugfs_create_dir("pagepool", nvmap_root);
	if (!pp_root)
		return -ENODEV;

	debugfs_create_u32("page_pool_available_pages",
			   S_IRUGO, pp_root,
			   &nvmap_dev->pool.count);
	debugfs_create_u32("page_pool_pages_to_zero",
			   S_IRUGO, pp_root,
			   &nvmap_dev->pool.to_zero);
#ifdef CONFIG_NVMAP_PAGE_POOL_DEBUG
	debugfs_create_u64("page_pool_allocs",
			   S_IRUGO, pp_root,
			   &nvmap_dev->pool.allocs);
	debugfs_create_u64("page_pool_fills",
			   S_IRUGO, pp_root,
			   &nvmap_dev->pool.fills);
	debugfs_create_u64("page_pool_hits",
			   S_IRUGO, pp_root,
			   &nvmap_dev->pool.hits);
	debugfs_create_u64("page_pool_misses",
			   S_IRUGO, pp_root,
			   &nvmap_dev->pool.misses);
#endif

	return 0;
}

int nvmap_page_pool_init(struct nvmap_device *dev)
{
	struct sysinfo info;
	struct nvmap_page_pool *pool = &dev->pool;

	memset(pool, 0x0, sizeof(*pool));
	mutex_init(&pool->lock);
	INIT_LIST_HEAD(&pool->page_list);
	INIT_LIST_HEAD(&pool->zero_list);

	si_meminfo(&info);
	pr_info("Total RAM pages: %lu\n", info.totalram);

	if (!CONFIG_NVMAP_PAGE_POOL_SIZE)
		/* The ratio is pool pages per 1K ram pages.
		 * So, the >> 10 */
		pool->max = (info.totalram * NVMAP_PP_POOL_SIZE) >> 10;
	else
		pool->max = CONFIG_NVMAP_PAGE_POOL_SIZE;

	if (pool->max >= info.totalram)
		goto fail;
	pool_size = pool->max;

	pr_info("nvmap page pool size: %u pages (%u MB)\n", pool->max,
		(pool->max * info.mem_unit) >> 20);

	background_allocator = kthread_run(nvmap_background_zero_thread,
					    NULL, "nvmap-bz");
	if (IS_ERR(background_allocator))
		goto fail;

	register_shrinker(&nvmap_page_pool_shrinker);

	return 0;
fail:
	nvmap_page_pool_fini(dev);
	return -ENOMEM;
}

int nvmap_page_pool_fini(struct nvmap_device *dev)
{
	struct nvmap_page_pool *pool = &dev->pool;

	/*
	 * if background allocator is not initialzed or not
	 * properly initialized, then shrinker is also not
	 * registered
	 */
	if (!IS_ERR_OR_NULL(background_allocator)) {
		unregister_shrinker(&nvmap_page_pool_shrinker);
		kthread_stop(background_allocator);
	}

	WARN_ON(!list_empty(&pool->page_list));

	return 0;
}
