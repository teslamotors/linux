/*
 * drivers/video/tegra/nvmap/nvmap_cache.c
 *
 * Copyright (c) 2011-2017, NVIDIA CORPORATION. All rights reserved.
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

#include <linux/highmem.h>
#include <linux/io.h>
#include <linux/debugfs.h>

#include <trace/events/nvmap.h>

#include "nvmap_priv.h"

/* this is basically the L2 cache size but may be tuned as per requirement */
#ifndef CONFIG_NVMAP_CACHE_MAINT_BY_SET_WAYS
size_t cache_maint_inner_threshold = SIZE_MAX;
#elif defined(CONFIG_ARCH_TEGRA_13x_SOC)
size_t cache_maint_inner_threshold = SZ_2M * 8;
#elif defined(CONFIG_ARCH_TEGRA_12x_SOC)
size_t cache_maint_inner_threshold = SZ_1M;
#else
size_t cache_maint_inner_threshold = SZ_2M;
#endif

#ifndef CONFIG_NVMAP_OUTER_CACHE_MAINT_BY_SET_WAYS
static int nvmap_outer_cache_maint_by_set_ways;
static size_t cache_maint_outer_threshold = SIZE_MAX;
#else
static int nvmap_outer_cache_maint_by_set_ways = 1;
static size_t cache_maint_outer_threshold = SZ_1M;
#endif

#ifdef CONFIG_NVMAP_CACHE_MAINT_BY_SET_WAYS
int nvmap_cache_maint_by_set_ways = 1;
#ifdef CONFIG_NVMAP_CACHE_MAINT_BY_SET_WAYS_ON_ONE_CPU
static int nvmap_cache_maint_by_set_ways_on_one_cpu = 1;
#else
static int nvmap_cache_maint_by_set_ways_on_one_cpu;
#endif
#else
int nvmap_cache_maint_by_set_ways;
static int nvmap_cache_maint_by_set_ways_on_one_cpu;
#endif

static struct static_key nvmap_disable_vaddr_for_cache_maint;

inline static void nvmap_flush_dcache_all(void *dummy)
{
#if defined(CONFIG_DENVER_CPU)
	u64 id_afr0;
	u64 midr;

	asm volatile ("mrs %0, MIDR_EL1" : "=r"(midr));
	/* check if current core is a Denver processor */
	if ((midr & 0xFF8FFFF0) == 0x4e0f0000) {
		asm volatile ("mrs %0, ID_AFR0_EL1" : "=r"(id_afr0));
		/* check if complete cache flush through msr is supported */
		if (likely((id_afr0 & 0xf00) == 0x100)) {
			asm volatile ("msr s3_0_c15_c13_0, %0" : : "r" (0));
			asm volatile ("dsb sy");
			return;
		}
	}
#endif
	__flush_dcache_all(NULL);
}

static void nvmap_inner_flush_cache_all(void)
{
#if defined(CONFIG_ARM64)
	if (nvmap_cache_maint_by_set_ways_on_one_cpu)
		nvmap_flush_dcache_all(NULL);
	else
		on_each_cpu(nvmap_flush_dcache_all, NULL, 1);
#else
	if (nvmap_cache_maint_by_set_ways_on_one_cpu)
		on_each_cpu(v7_flush_kern_cache_all, NULL, 1);
	else
		v7_flush_kern_cache_all();
#endif
}
void (*inner_flush_cache_all)(void) = nvmap_inner_flush_cache_all;

extern void __clean_dcache_louis(void *);
extern void v7_clean_kern_cache_louis(void *);
static void nvmap_inner_clean_cache_all(void)
{
#if defined(CONFIG_ARM64)
	if (nvmap_cache_maint_by_set_ways_on_one_cpu) {
		on_each_cpu(__clean_dcache_louis, NULL, 1);
		__clean_dcache_all(NULL);
	} else {
		on_each_cpu(__clean_dcache_all, NULL, 1);
	}
#else
	if (nvmap_cache_maint_by_set_ways_on_one_cpu) {
		on_each_cpu(v7_clean_kern_cache_louis, NULL, 1);
		v7_clean_kern_cache_all(NULL);
	} else {
		on_each_cpu(v7_clean_kern_cache_all, NULL, 1);
	}
#endif
}
void (*inner_clean_cache_all)(void) = nvmap_inner_clean_cache_all;

/*
 * FIXME:
 *
 *   __clean_dcache_page() is only available on ARM64 (well, we haven't
 *   implemented it on ARMv7).
 */
void nvmap_clean_cache_page(struct page *page)
{
#if defined(CONFIG_ARM64)
	__clean_dcache_page(page);
#else
	__flush_dcache_page(page_mapping(page), page);
#endif
	outer_clean_range(page_to_phys(page), page_to_phys(page) + PAGE_SIZE);
}

void nvmap_clean_cache(struct page **pages, int numpages)
{
	int i;

	/* Not technically a flush but that's what nvmap knows about. */
	nvmap_stats_inc(NS_CFLUSH_DONE, numpages << PAGE_SHIFT);
	trace_nvmap_cache_flush(numpages << PAGE_SHIFT,
		nvmap_stats_read(NS_ALLOC),
		nvmap_stats_read(NS_CFLUSH_RQ),
		nvmap_stats_read(NS_CFLUSH_DONE));

	for (i = 0; i < numpages; i++)
		nvmap_clean_cache_page(pages[i]);
}

__weak void nvmap_override_cache_ops(void)
{
}

void inner_cache_maint(unsigned int op, void *vaddr, size_t size)
{
	if (op == NVMAP_CACHE_OP_WB_INV)
		__dma_flush_range(vaddr, vaddr + size);
	else if (op == NVMAP_CACHE_OP_INV)
		__dma_map_area(vaddr, size, DMA_FROM_DEVICE);
	else
		__dma_map_area(vaddr, size, DMA_TO_DEVICE);
}

void outer_cache_maint(unsigned int op, phys_addr_t paddr, size_t size)
{
	if (op == NVMAP_CACHE_OP_WB_INV)
		outer_flush_range(paddr, paddr + size);
	else if (op == NVMAP_CACHE_OP_INV)
		outer_inv_range(paddr, paddr + size);
	else
		outer_clean_range(paddr, paddr + size);
}

static void heap_page_cache_maint(
	struct nvmap_handle *h, unsigned long start, unsigned long end,
	unsigned int op, bool inner, bool outer, bool clean_only_dirty)
{
	if (h->userflags & NVMAP_HANDLE_CACHE_SYNC) {
		/*
		 * zap user VA->PA mappings so that any access to the pages
		 * will result in a fault and can be marked dirty
		 */
		nvmap_handle_mkclean(h, start, end-start);
		nvmap_zap_handle(h, start, end - start);
	}

	if (static_key_false(&nvmap_disable_vaddr_for_cache_maint))
		goto per_page_cache_maint;

	if (inner) {
		if (!h->vaddr) {
			if (__nvmap_mmap(h))
				__nvmap_munmap(h, h->vaddr);
			else
				goto per_page_cache_maint;
		}
		/* Fast inner cache maintenance using single mapping */
		inner_cache_maint(op, h->vaddr + start, end - start);
		if (!outer)
			return;
		/* Skip per-page inner maintenance in loop below */
		inner = false;

	}
per_page_cache_maint:

	while (start < end) {
		struct page *page;
		phys_addr_t paddr;
		unsigned long next;
		unsigned long off;
		size_t size;
		int ret;

		page = nvmap_to_page(h->pgalloc.pages[start >> PAGE_SHIFT]);
		next = min(((start + PAGE_SIZE) & PAGE_MASK), end);
		off = start & ~PAGE_MASK;
		size = next - start;
		paddr = page_to_phys(page) + off;

		ret = nvmap_cache_maint_phys_range(op, paddr, paddr + size,
				inner, outer);
		BUG_ON(ret != 0);
		start = next;
	}
}

static bool fast_cache_maint_outer(unsigned long start,
		unsigned long end, unsigned int op)
{
	if (!nvmap_outer_cache_maint_by_set_ways)
		return false;

	if ((op == NVMAP_CACHE_OP_INV) ||
		((end - start) < cache_maint_outer_threshold))
		return false;

	if (op == NVMAP_CACHE_OP_WB_INV)
		outer_flush_all();
	else if (op == NVMAP_CACHE_OP_WB)
		outer_clean_all();

	return true;
}

static inline bool can_fast_cache_maint(unsigned long start,
			unsigned long end, unsigned int op)
{
	if (!nvmap_cache_maint_by_set_ways)
		return false;

	if ((op == NVMAP_CACHE_OP_INV) ||
		((end - start) < cache_maint_inner_threshold))
		return false;
	return true;
}

static bool fast_cache_maint(struct nvmap_handle *h,
	unsigned long start,
	unsigned long end, unsigned int op,
	bool clean_only_dirty)
{
	if (!can_fast_cache_maint(start, end, op))
		return false;

	if (h->userflags & NVMAP_HANDLE_CACHE_SYNC) {
		nvmap_handle_mkclean(h, 0, h->size);
		nvmap_zap_handle(h, 0, h->size);
	}

	if (op == NVMAP_CACHE_OP_WB_INV)
		inner_flush_cache_all();
	else if (op == NVMAP_CACHE_OP_WB)
		inner_clean_cache_all();

	/* outer maintenance */
	if (h->flags != NVMAP_HANDLE_INNER_CACHEABLE) {
		if(!fast_cache_maint_outer(start, end, op))
		{
			if (h->heap_pgalloc) {
				heap_page_cache_maint(h, start,
					end, op, false, true,
					clean_only_dirty);
			} else  {
				phys_addr_t pstart;

				pstart = start + h->carveout->base;
				outer_cache_maint(op, pstart, end - start);
			}
		}
	}
	return true;
}

struct cache_maint_op {
	phys_addr_t start;
	phys_addr_t end;
	unsigned int op;
	struct nvmap_handle *h;
	bool inner;
	bool outer;
	bool clean_only_dirty;
};

int nvmap_cache_maint_phys_range(unsigned int op, phys_addr_t pstart,
		phys_addr_t pend, int inner, int outer)
{
	unsigned long kaddr;
	struct vm_struct *area = NULL;
	phys_addr_t loop;

	if (!inner)
		goto do_outer;

	if (can_fast_cache_maint((unsigned long)pstart,
				 (unsigned long)pend, op)) {
		if (op == NVMAP_CACHE_OP_WB_INV)
			inner_flush_cache_all();
		else if (op == NVMAP_CACHE_OP_WB)
			inner_clean_cache_all();
		goto do_outer;
	}

	area = alloc_vm_area(PAGE_SIZE, NULL);
	if (!area)
		return -ENOMEM;
	kaddr = (ulong)area->addr;

	loop = pstart;
	while (loop < pend) {
		phys_addr_t next = (loop + PAGE_SIZE) & PAGE_MASK;
		void *base = (void *)kaddr + (loop & ~PAGE_MASK);

		next = min(next, pend);
		ioremap_page_range(kaddr, kaddr + PAGE_SIZE,
			loop, PG_PROT_KERNEL);
		inner_cache_maint(op, base, next - loop);
		loop = next;
		unmap_kernel_range(kaddr, PAGE_SIZE);
	}

	free_vm_area(area);
do_outer:
	if (!outer)
		return 0;

	if (!fast_cache_maint_outer(pstart, pend, op))
		outer_cache_maint(op, pstart, pend - pstart);
	return 0;
}

static int do_cache_maint(struct cache_maint_op *cache_work)
{
	phys_addr_t pstart = cache_work->start;
	phys_addr_t pend = cache_work->end;
	int err = 0;
	struct nvmap_handle *h = cache_work->h;
	unsigned int op = cache_work->op;

	if (!h || !h->alloc)
		return -EFAULT;

	wmb();
	if (h->flags == NVMAP_HANDLE_UNCACHEABLE ||
	    h->flags == NVMAP_HANDLE_WRITE_COMBINE || pstart == pend)
		goto out;

	trace_nvmap_cache_maint(h->owner, h, pstart, pend, op, pend - pstart);
	if (pstart > h->size || pend > h->size) {
		pr_warn("cache maintenance outside handle\n");
		err = -EINVAL;
		goto out;
	}

	if (fast_cache_maint(h, pstart, pend, op, cache_work->clean_only_dirty))
		goto out;

	if (h->heap_pgalloc) {
		heap_page_cache_maint(h, pstart, pend, op, true,
			(h->flags == NVMAP_HANDLE_INNER_CACHEABLE) ?
			false : true, cache_work->clean_only_dirty);
		goto out;
	}

	pstart += h->carveout->base;
	pend += h->carveout->base;

	err = nvmap_cache_maint_phys_range(op, pstart, pend, true,
			h->flags != NVMAP_HANDLE_INNER_CACHEABLE);

out:
	if (!err) {
		if (can_fast_cache_maint(pstart, pend, op))
			nvmap_stats_inc(NS_CFLUSH_DONE,
					cache_maint_inner_threshold);
		else
			nvmap_stats_inc(NS_CFLUSH_DONE, pend - pstart);
	}

	trace_nvmap_cache_flush(pend - pstart,
		nvmap_stats_read(NS_ALLOC),
		nvmap_stats_read(NS_CFLUSH_RQ),
		nvmap_stats_read(NS_CFLUSH_DONE));

	return 0;
}

int __nvmap_do_cache_maint(struct nvmap_client *client,
			struct nvmap_handle *h,
			unsigned long start, unsigned long end,
			unsigned int op, bool clean_only_dirty)
{
	int err;
	struct cache_maint_op cache_op;

	h = nvmap_handle_get(h);
	if (!h)
		return -EFAULT;

	if ((start >= h->size) || (end > h->size)) {
		nvmap_handle_put(h);
		return -EFAULT;
	}

	if (!(h->heap_type & nvmap_dev->cpu_access_mask)) {
		nvmap_handle_put(h);
		return -EPERM;
	}

	nvmap_kmaps_inc(h);
	if (op == NVMAP_CACHE_OP_INV)
		op = NVMAP_CACHE_OP_WB_INV;

	/* clean only dirty is applicable only for Write Back operation */
	if (op != NVMAP_CACHE_OP_WB)
		clean_only_dirty = false;

	cache_op.h = h;
	cache_op.start = start;
	cache_op.end = end;
	cache_op.op = op;
	cache_op.inner = h->flags == NVMAP_HANDLE_CACHEABLE ||
			 h->flags == NVMAP_HANDLE_INNER_CACHEABLE;
	cache_op.outer = h->flags == NVMAP_HANDLE_CACHEABLE;
	cache_op.clean_only_dirty = clean_only_dirty;

	nvmap_stats_inc(NS_CFLUSH_RQ, end - start);
	err = do_cache_maint(&cache_op);
	nvmap_kmaps_dec(h);
	nvmap_handle_put(h);
	return err;
}

int __nvmap_cache_maint(struct nvmap_client *client,
			       struct nvmap_cache_op *op)
{
	struct vm_area_struct *vma;
	struct nvmap_vma_priv *priv;
	struct nvmap_handle *handle;
	unsigned long start;
	unsigned long end;
	int err = 0;

	if (!op->addr || op->op < NVMAP_CACHE_OP_WB ||
	    op->op > NVMAP_CACHE_OP_WB_INV)
		return -EINVAL;

	handle = nvmap_handle_get_from_fd(op->handle);
	if (!handle)
		return -EINVAL;

	down_read(&current->mm->mmap_sem);

	vma = find_vma(current->active_mm, (unsigned long)op->addr);
	if (!vma || !is_nvmap_vma(vma) ||
	    (ulong)op->addr < vma->vm_start ||
	    (ulong)op->addr >= vma->vm_end ||
	    op->len > vma->vm_end - (ulong)op->addr) {
		err = -EADDRNOTAVAIL;
		goto out;
	}

	priv = (struct nvmap_vma_priv *)vma->vm_private_data;

	if (priv->handle != handle) {
		err = -EFAULT;
		goto out;
	}

	start = (unsigned long)op->addr - vma->vm_start +
		(vma->vm_pgoff << PAGE_SHIFT);
	end = start + op->len;

	err = __nvmap_do_cache_maint(client, priv->handle, start, end, op->op,
				     false);
out:
	up_read(&current->mm->mmap_sem);
	nvmap_handle_put(handle);
	return err;
}

/*
 * Perform cache op on the list of memory regions within passed handles.
 * A memory region within handle[i] is identified by offsets[i], sizes[i]
 *
 * sizes[i] == 0  is a special case which causes handle wide operation,
 * this is done by replacing offsets[i] = 0, sizes[i] = handles[i]->size.
 * So, the input arrays sizes, offsets  are not guaranteed to be read-only
 *
 * This will optimze the op if it can.
 * In the case that all the handles together are larger than the inner cache
 * maint threshold it is possible to just do an entire inner cache flush.
 *
 * NOTE: this omits outer cache operations which is fine for ARM64
 */
int nvmap_do_cache_maint_list(struct nvmap_handle **handles, u32 *offsets,
			      u32 *sizes, int op, int nr)
{
	int i;
	u64 total = 0;
	u64 thresh = ~0;

	WARN(!config_enabled(CONFIG_ARM64),
		"cache list operation may not function properly");

	if (nvmap_cache_maint_by_set_ways)
		thresh = cache_maint_inner_threshold;

	for (i = 0; i < nr; i++)
		if ((op == NVMAP_CACHE_OP_WB) && nvmap_handle_track_dirty(handles[i]))
			total += atomic_read(&handles[i]->pgalloc.ndirty);
		else
			total += sizes[i] ? sizes[i] : handles[i]->size;

	if (!total)
		return 0;

	/* Full flush in the case the passed list is bigger than our
	 * threshold. */
	if (total >= thresh) {
		for (i = 0; i < nr; i++) {
			if (handles[i]->userflags &
			    NVMAP_HANDLE_CACHE_SYNC) {
				nvmap_handle_mkclean(handles[i], 0,
						     handles[i]->size);
				nvmap_zap_handle(handles[i], 0,
						 handles[i]->size);
			}
		}

		if (op == NVMAP_CACHE_OP_WB) {
			inner_clean_cache_all();
			outer_clean_all();
		} else {
			inner_flush_cache_all();
			outer_flush_all();
		}
		nvmap_stats_inc(NS_CFLUSH_RQ, total);
		nvmap_stats_inc(NS_CFLUSH_DONE, thresh);
		trace_nvmap_cache_flush(total,
					nvmap_stats_read(NS_ALLOC),
					nvmap_stats_read(NS_CFLUSH_RQ),
					nvmap_stats_read(NS_CFLUSH_DONE));
	} else {
		for (i = 0; i < nr; i++) {
			u32 size = sizes[i] ? sizes[i] : handles[i]->size;
			u32 offset = sizes[i] ? offsets[i] : 0;
			int err = __nvmap_do_cache_maint(handles[i]->owner,
							 handles[i], offset,
							 offset + size,
							 op, false);
			if (err)
				return err;
		}
	}

	return 0;
}

static int cache_inner_threshold_show(struct seq_file *m, void *v)
{
	if (nvmap_cache_maint_by_set_ways)
		seq_printf(m, "%zuB\n", cache_maint_inner_threshold);
	else
		seq_printf(m, "%zuB\n", SIZE_MAX);
	return 0;
}

static int cache_inner_threshold_open(struct inode *inode, struct file *file)
{
	return single_open(file, cache_inner_threshold_show, inode->i_private);
}

static ssize_t cache_inner_threshold_write(struct file *file,
					const char __user *buffer,
					size_t count, loff_t *pos)
{
	int ret;
	struct seq_file *p = file->private_data;
	char str[] = "0123456789abcdef";

	count = min_t(size_t, strlen(str), count);
	if (copy_from_user(str, buffer, count))
		return -EINVAL;

	if (!nvmap_cache_maint_by_set_ways)
		return -EINVAL;

	mutex_lock(&p->lock);
	ret = sscanf(str, "%16zu", &cache_maint_inner_threshold);
	mutex_unlock(&p->lock);
	if (ret != 1)
		return -EINVAL;

	pr_debug("nvmap:cache_maint_inner_threshold is now :%zuB\n",
			cache_maint_inner_threshold);
	return count;
}

static const struct file_operations cache_inner_threshold_fops = {
	.open		= cache_inner_threshold_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= cache_inner_threshold_write,
};

static int cache_outer_threshold_show(struct seq_file *m, void *v)
{
	if (nvmap_outer_cache_maint_by_set_ways)
		seq_printf(m, "%zuB\n", cache_maint_outer_threshold);
	else
		seq_printf(m, "%zuB\n", SIZE_MAX);
	return 0;
}

static int cache_outer_threshold_open(struct inode *inode, struct file *file)
{
	return single_open(file, cache_outer_threshold_show, inode->i_private);
}

static ssize_t cache_outer_threshold_write(struct file *file,
					const char __user *buffer,
					size_t count, loff_t *pos)
{
	int ret;
	struct seq_file *p = file->private_data;
	char str[] = "0123456789abcdef";

	count = min_t(size_t, strlen(str), count);
	if (copy_from_user(str, buffer, count))
		return -EINVAL;

	if (!nvmap_outer_cache_maint_by_set_ways)
		return -EINVAL;

	mutex_lock(&p->lock);
	ret = sscanf(str, "%16zu", &cache_maint_outer_threshold);
	mutex_unlock(&p->lock);
	if (ret != 1)
		return -EINVAL;

	pr_debug("nvmap:cache_maint_outer_threshold is now :%zuB\n",
			cache_maint_outer_threshold);
	return count;
}

static const struct file_operations cache_outer_threshold_fops = {
	.open		= cache_outer_threshold_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= cache_outer_threshold_write,
};

int nvmap_cache_debugfs_init(struct dentry *nvmap_root)
{
	struct dentry *cache_root;

	if (!nvmap_root)
		return -ENODEV;

	cache_root = debugfs_create_dir("cache", nvmap_root);
	if (!cache_root)
		return -ENODEV;

	if (nvmap_cache_maint_by_set_ways) {
		debugfs_create_x32("nvmap_cache_maint_by_set_ways",
				   S_IRUSR | S_IWUSR,
				   cache_root,
				   &nvmap_cache_maint_by_set_ways);

	debugfs_create_file("cache_maint_inner_threshold",
			    S_IRUSR | S_IWUSR,
			    cache_root,
			    NULL,
			    &cache_inner_threshold_fops);
	}

	if (nvmap_cache_maint_by_set_ways_on_one_cpu) {
		debugfs_create_x32("nvmap_cache_maint_by_set_ways_on_one_cpu",
				   S_IRUSR | S_IWUSR,
				   cache_root,
				   &nvmap_cache_maint_by_set_ways_on_one_cpu);
	}

	if (nvmap_outer_cache_maint_by_set_ways) {
		debugfs_create_x32("nvmap_cache_outer_maint_by_set_ways",
				   S_IRUSR | S_IWUSR,
				   cache_root,
				   &nvmap_outer_cache_maint_by_set_ways);

		debugfs_create_file("cache_maint_outer_threshold",
				    S_IRUSR | S_IWUSR,
				    cache_root,
				    NULL,
				    &cache_outer_threshold_fops);
	}

	debugfs_create_atomic_t("nvmap_disable_vaddr_for_cache_maint",
				S_IRUSR | S_IWUSR,
				cache_root,
				&nvmap_disable_vaddr_for_cache_maint.enabled);

	return 0;
}
