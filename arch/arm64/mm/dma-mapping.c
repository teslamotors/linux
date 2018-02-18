/*
 * SWIOTLB-based DMA API implementation
 *
 * Copyright (C) 2012 ARM Ltd.
 * Author: Catalin Marinas <catalin.marinas@arm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/gfp.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/genalloc.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/vmalloc.h>
#include <linux/swiotlb.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/highmem.h>
#include <linux/memblock.h>
#include <linux/iommu.h>
#include <linux/io.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/cma.h>

#include <asm/cacheflush.h>
#include <asm/outercache.h>
#include <asm/memory.h>
#include <asm/tlbflush.h>
#include <asm/mach/arch.h>
#include <asm/dma-iommu.h>
#include <asm/mach/map.h>
#include <asm/dma-contiguous.h>

#include "mm.h"

#define CREATE_TRACE_POINTS
#include <trace/events/dmadebug.h>

enum dma_operation {
	ALLOC_OR_FREE = 1,
	ATOMIC_ALLOC_OR_FREE,
	MAP_OR_UNMAP,
	CPU_MAP_OR_UNMAP
};

#ifdef CONFIG_DMA_API_DEBUG
#define NULL_DEV "null_dev"
struct iommu_usage {
	struct device		*dev;
	struct list_head	recordlist;
};

struct null_device {
	char const	*dev_name;
	atomic64_t	map_size;
	atomic64_t	atomic_alloc_size;
	atomic64_t	alloc_size;
	atomic64_t	cpu_map_size;
};

static struct null_device *dev_is_null;
static LIST_HEAD(iommu_rlist_head);
static size_t dmastats_alloc_or_map(struct device *dev, size_t size,
	const int type);
static size_t dmastats_free_or_unmap(struct device *dev, size_t size,
	const int type);
static void add_value(struct dma_iommu_mapping *iu, size_t size,
	const int type);
static void sub_value(struct dma_iommu_mapping *device_ref, size_t size,
	const int type);
#else
#define dmastats_alloc_or_map(dev, size, type)
#define dmastats_free_or_unmap(dev, size, type)
#endif

struct dma_map_ops *dma_ops;
EXPORT_SYMBOL(dma_ops);

static pgprot_t __get_dma_pgprot(struct dma_attrs *attrs, pgprot_t prot,
				 bool coherent)
{
	if (!coherent || dma_get_attr(DMA_ATTR_WRITE_COMBINE, attrs))
		return pgprot_writecombine(prot);
	return prot;
}

static struct gen_pool *atomic_pool;
static struct page **atomic_pool_pages;

static size_t atomic_pool_size = CONFIG_DEFAULT_DMA_COHERENT_POOL_SIZE;

static int __init early_coherent_pool(char *p)
{
	atomic_pool_size = memparse(p, &p);
	return 0;
}
early_param("coherent_pool", early_coherent_pool);

static void *__alloc_from_pool(size_t size, struct page **ret_page, gfp_t flags)
{
	unsigned long val;
	void *ptr = NULL;

	if (!atomic_pool) {
		WARN(1, "coherent pool not initialised!\n");
		return NULL;
	}

	val = gen_pool_alloc(atomic_pool, size);
	if (val) {
		phys_addr_t phys = gen_pool_virt_to_phys(atomic_pool, val);

		*ret_page = phys_to_page(phys);
		ptr = (void *)val;
		memset(ptr, 0, size);
	}

	return ptr;
}

static bool __in_atomic_pool(void *start, size_t size)
{
	if (!atomic_pool)
		return false;

	return addr_in_gen_pool(atomic_pool, (unsigned long)start, size);
}

static int __free_from_pool(void *start, size_t size)
{
	if (!__in_atomic_pool(start, size))
		return 0;

	gen_pool_free(atomic_pool, (unsigned long)start, size);

	return 1;
}

#ifdef CONFIG_DMA_API_DEBUG
static void *___alloc_from_pool(struct device *dev, size_t size,
				struct page **ret_page, gfp_t flags)
{
	struct dma_iommu_mapping *mapping;
	void *ptr = __alloc_from_pool(size, ret_page, flags);

	mapping = to_dma_iommu_mapping(dev);
	if (ptr && mapping)
		dmastats_alloc_or_map(dev, size, ATOMIC_ALLOC_OR_FREE);

	return ptr;
}

static int ___free_from_pool(struct device *dev, void *start, size_t size)
{
	int ret = __free_from_pool(start, size);

	if (ret)
		dmastats_free_or_unmap(dev, size, ATOMIC_ALLOC_OR_FREE);

	return ret;
}

#define __free_from_pool(start, size)	\
	___free_from_pool(dev, start, size)

#define __alloc_from_pool(size, ret_page, flags) \
	___alloc_from_pool(dev, size, ret_page, flags)
#endif

#ifdef CONFIG_SWIOTLB
static void *__dma_alloc_coherent(struct device *dev, size_t size,
				  dma_addr_t *dma_handle, gfp_t flags,
				  struct dma_attrs *attrs)
{
	if (dev == NULL) {
		WARN_ONCE(1, "Use an actual device structure for DMA allocation\n");
		return NULL;
	}

	if (IS_ENABLED(CONFIG_ZONE_DMA) &&
	    dev->coherent_dma_mask <= DMA_BIT_MASK(32))
		flags |= GFP_DMA;
	if (IS_ENABLED(CONFIG_DMA_CMA) && (flags & __GFP_WAIT)) {
		struct page *page;
		void *addr;

		size = PAGE_ALIGN(size);
		page = dma_alloc_from_contiguous(dev, size >> PAGE_SHIFT,
							get_order(size));
		if (!page)
			return NULL;

		*dma_handle = phys_to_dma(dev, page_to_phys(page));
		addr = page_address(page);
		memset(addr, 0, size);
		return addr;
	} else {
		return swiotlb_alloc_coherent(dev, size, dma_handle, flags);
	}
}

static void __dma_free_coherent(struct device *dev, size_t size,
				void *vaddr, dma_addr_t dma_handle,
				struct dma_attrs *attrs)
{
	bool freed;
	phys_addr_t paddr = dma_to_phys(dev, dma_handle);

	if (dev == NULL) {
		WARN_ONCE(1, "Use an actual device structure for DMA allocation\n");
		return;
	}

	freed = dma_release_from_contiguous(dev,
					phys_to_page(paddr),
					size >> PAGE_SHIFT);
	if (!freed)
		swiotlb_free_coherent(dev, size, vaddr, dma_handle);
}

static void *__dma_alloc_noncoherent(struct device *dev, size_t size,
				     dma_addr_t *dma_handle, gfp_t flags,
				     struct dma_attrs *attrs)
{
	struct page *page;
	void *ptr, *coherent_ptr;

	size = PAGE_ALIGN(size);

	if (!(flags & __GFP_WAIT)) {
		struct page *page = NULL;
		void *addr = __alloc_from_pool(size, &page, flags);

		if (addr)
			*dma_handle = phys_to_dma(dev, page_to_phys(page));

		return addr;

	}

	ptr = __dma_alloc_coherent(dev, size, dma_handle, flags, attrs);
	if (!ptr)
		goto no_mem;

	/* remove any dirty cache lines on the kernel alias */
	__dma_flush_range(ptr, ptr + size);

	/* create a coherent mapping */
	page = virt_to_page(ptr);
	coherent_ptr = dma_common_contiguous_remap(page, size, VM_USERMAP,
				__get_dma_pgprot(attrs,
					__pgprot(PROT_NORMAL_NC), false),
					NULL);
	if (!coherent_ptr)
		goto no_map;

	return coherent_ptr;

no_map:
	__dma_free_coherent(dev, size, ptr, *dma_handle, attrs);
no_mem:
	*dma_handle = DMA_ERROR_CODE;
	return NULL;
}

static void __dma_free_noncoherent(struct device *dev, size_t size,
				   void *vaddr, dma_addr_t dma_handle,
				   struct dma_attrs *attrs)
{
	void *swiotlb_addr = phys_to_virt(dma_to_phys(dev, dma_handle));

	if (__free_from_pool(vaddr, size))
		return;
	vunmap(vaddr);
	__dma_free_coherent(dev, size, swiotlb_addr, dma_handle, attrs);
}

static dma_addr_t __swiotlb_map_page(struct device *dev, struct page *page,
				     unsigned long offset, size_t size,
				     enum dma_data_direction dir,
				     struct dma_attrs *attrs)
{
	dma_addr_t dev_addr;

	dev_addr = swiotlb_map_page(dev, page, offset, size, dir, attrs);
	__dma_map_area(phys_to_virt(dma_to_phys(dev, dev_addr)), size, dir);

	return dev_addr;
}


static void __swiotlb_unmap_page(struct device *dev, dma_addr_t dev_addr,
				 size_t size, enum dma_data_direction dir,
				 struct dma_attrs *attrs)
{
	__dma_unmap_area(phys_to_virt(dma_to_phys(dev, dev_addr)), size, dir);
	swiotlb_unmap_page(dev, dev_addr, size, dir, attrs);
}

static int __swiotlb_map_sg_attrs(struct device *dev, struct scatterlist *sgl,
				  int nelems, enum dma_data_direction dir,
				  struct dma_attrs *attrs)
{
	struct scatterlist *sg;
	int i, ret;

	ret = swiotlb_map_sg_attrs(dev, sgl, nelems, dir, attrs);
	for_each_sg(sgl, sg, ret, i)
		__dma_map_area(phys_to_virt(dma_to_phys(dev, sg->dma_address)),
			       sg->length, dir);

	return ret;
}

static void __swiotlb_unmap_sg_attrs(struct device *dev,
				     struct scatterlist *sgl, int nelems,
				     enum dma_data_direction dir,
				     struct dma_attrs *attrs)
{
	struct scatterlist *sg;
	int i;

	for_each_sg(sgl, sg, nelems, i)
		__dma_unmap_area(phys_to_virt(dma_to_phys(dev, sg->dma_address)),
				 sg->length, dir);
	swiotlb_unmap_sg_attrs(dev, sgl, nelems, dir, attrs);
}

static void __swiotlb_sync_single_for_cpu(struct device *dev,
					  dma_addr_t dev_addr, size_t size,
					  enum dma_data_direction dir)
{
	__dma_unmap_area(phys_to_virt(dma_to_phys(dev, dev_addr)), size, dir);
	swiotlb_sync_single_for_cpu(dev, dev_addr, size, dir);
}

static void __swiotlb_sync_single_for_device(struct device *dev,
					     dma_addr_t dev_addr, size_t size,
					     enum dma_data_direction dir)
{
	swiotlb_sync_single_for_device(dev, dev_addr, size, dir);
	__dma_map_area(phys_to_virt(dma_to_phys(dev, dev_addr)), size, dir);
}

static void __swiotlb_sync_sg_for_cpu(struct device *dev,
				      struct scatterlist *sgl, int nelems,
				      enum dma_data_direction dir)
{
	struct scatterlist *sg;
	int i;

	for_each_sg(sgl, sg, nelems, i)
		__dma_unmap_area(phys_to_virt(dma_to_phys(dev, sg->dma_address)),
				 sg->length, dir);
	swiotlb_sync_sg_for_cpu(dev, sgl, nelems, dir);
}

static void __swiotlb_sync_sg_for_device(struct device *dev,
					 struct scatterlist *sgl, int nelems,
					 enum dma_data_direction dir)
{
	struct scatterlist *sg;
	int i;

	swiotlb_sync_sg_for_device(dev, sgl, nelems, dir);
	for_each_sg(sgl, sg, nelems, i)
		__dma_map_area(phys_to_virt(dma_to_phys(dev, sg->dma_address)),
			       sg->length, dir);
}

/* vma->vm_page_prot must be set appropriately before calling this function */
static int __dma_common_mmap(struct device *dev, struct vm_area_struct *vma,
			     void *cpu_addr, dma_addr_t dma_addr, size_t size)
{
	int ret = -ENXIO;
	unsigned long nr_vma_pages = (vma->vm_end - vma->vm_start) >>
					PAGE_SHIFT;
	unsigned long nr_pages = PAGE_ALIGN(size) >> PAGE_SHIFT;
	unsigned long pfn = dma_to_phys(dev, dma_addr) >> PAGE_SHIFT;
	unsigned long off = vma->vm_pgoff;

	if (dma_mmap_from_coherent(dev, vma, cpu_addr, size, &ret))
		return ret;

	if (off < nr_pages && nr_vma_pages <= (nr_pages - off)) {
		ret = remap_pfn_range(vma, vma->vm_start,
				      pfn + off,
				      vma->vm_end - vma->vm_start,
				      vma->vm_page_prot);
	}

	return ret;
}

static int __swiotlb_mmap_noncoherent(struct device *dev,
		struct vm_area_struct *vma,
		void *cpu_addr, dma_addr_t dma_addr, size_t size,
		struct dma_attrs *attrs)
{
	vma->vm_page_prot = __get_dma_pgprot(attrs, vma->vm_page_prot, false);
	return __dma_common_mmap(dev, vma, cpu_addr, dma_addr, size);
}

static int __swiotlb_mmap_coherent(struct device *dev,
		struct vm_area_struct *vma,
		void *cpu_addr, dma_addr_t dma_addr, size_t size,
		struct dma_attrs *attrs)
{
	/* Just use whatever page_prot attributes were specified */
	return __dma_common_mmap(dev, vma, cpu_addr, dma_addr, size);
}

struct dma_map_ops noncoherent_swiotlb_dma_ops = {
	.alloc = __dma_alloc_noncoherent,
	.free = __dma_free_noncoherent,
	.mmap = __swiotlb_mmap_noncoherent,
	.map_page = __swiotlb_map_page,
	.unmap_page = __swiotlb_unmap_page,
	.map_sg = __swiotlb_map_sg_attrs,
	.unmap_sg = __swiotlb_unmap_sg_attrs,
	.sync_single_for_cpu = __swiotlb_sync_single_for_cpu,
	.sync_single_for_device = __swiotlb_sync_single_for_device,
	.sync_sg_for_cpu = __swiotlb_sync_sg_for_cpu,
	.sync_sg_for_device = __swiotlb_sync_sg_for_device,
	.dma_supported = swiotlb_dma_supported,
	.mapping_error = swiotlb_dma_mapping_error,
};
EXPORT_SYMBOL(noncoherent_swiotlb_dma_ops);

struct dma_map_ops coherent_swiotlb_dma_ops = {
	.alloc = __dma_alloc_coherent,
	.free = __dma_free_coherent,
	.mmap = __swiotlb_mmap_coherent,
	.map_page = swiotlb_map_page,
	.unmap_page = swiotlb_unmap_page,
	.map_sg = swiotlb_map_sg_attrs,
	.unmap_sg = swiotlb_unmap_sg_attrs,
	.sync_single_for_cpu = swiotlb_sync_single_for_cpu,
	.sync_single_for_device = swiotlb_sync_single_for_device,
	.sync_sg_for_cpu = swiotlb_sync_sg_for_cpu,
	.sync_sg_for_device = swiotlb_sync_sg_for_device,
	.dma_supported = swiotlb_dma_supported,
	.mapping_error = swiotlb_dma_mapping_error,
};
EXPORT_SYMBOL(coherent_swiotlb_dma_ops);

extern int swiotlb_late_init_with_default_size(size_t default_size);

#endif /* CONFIG_SWIOTLB */

static int __init atomic_pool_init(void)
{
	pgprot_t prot = __pgprot(PROT_NORMAL_NC);
	unsigned long nr_pages = atomic_pool_size >> PAGE_SHIFT;
	struct page *page;
	struct page **pages = NULL;
	void *addr;
	unsigned int pool_size_order = get_order(atomic_pool_size);

	if (dev_get_cma_area(NULL))
		page = dma_alloc_from_contiguous(NULL, nr_pages,
							pool_size_order);
	else
		page = alloc_pages(GFP_DMA, pool_size_order);

	if (page) {
		int ret, i = 0;
		void *page_addr = page_address(page);

		memset(page_addr, 0, atomic_pool_size);
		__dma_flush_range(page_addr, page_addr + atomic_pool_size);

		atomic_pool = gen_pool_create(PAGE_SHIFT, -1);
		if (!atomic_pool)
			goto free_page;

		pages = kmalloc(sizeof(struct page *) << pool_size_order,
				GFP_KERNEL);
		if (!pages)
			goto free_page;

		for (; i < nr_pages; i++)
			pages[i] = page + i;

		addr = dma_common_pages_remap(pages, atomic_pool_size,
					VM_ARM_DMA_CONSISTENT | VM_USERMAP,
					prot, atomic_pool_init);

		if (!addr)
			goto destroy_genpool;

		ret = gen_pool_add_virt(atomic_pool, (unsigned long)addr,
					page_to_phys(page),
					atomic_pool_size, -1);
		if (ret)
			goto remove_mapping;

		atomic_pool_pages = pages;
		gen_pool_set_algo(atomic_pool,
				  gen_pool_first_fit_order_align,
				  (void *)PAGE_SHIFT);

		pr_info("DMA: preallocated %zu KiB pool for atomic allocations\n",
			atomic_pool_size / 1024);
		return 0;
	}
	goto out;

remove_mapping:
	dma_common_free_remap(addr, atomic_pool_size,
			      VM_ARM_DMA_CONSISTENT | VM_USERMAP);
destroy_genpool:
	gen_pool_destroy(atomic_pool);
	atomic_pool = NULL;
	kfree(pages);
free_page:
	if (!dma_release_from_contiguous(NULL, page, nr_pages))
		__free_pages(page, pool_size_order);
out:
	pr_err("DMA: failed to allocate %zu KiB pool for atomic coherent allocation\n",
		atomic_pool_size / 1024);
	return -ENOMEM;
}

#ifdef CONFIG_SWIOTLB
static int __init swiotlb_late_init(void)
{
	size_t swiotlb_size = min(SZ_64M, MAX_ORDER_NR_PAGES << PAGE_SHIFT);

	dma_ops = &noncoherent_swiotlb_dma_ops;

	return swiotlb_late_init_with_default_size(swiotlb_size);
}
#endif /* CONFIG_SWIOTLB */

static int __init arm64_dma_init(void)
{
	int ret = 0;

	dma_ops = &arm_dma_ops;
#ifdef CONFIG_SWIOTLB
	ret |= swiotlb_late_init();
#endif /* CONFIG_SWIOTLB */
	ret |= atomic_pool_init();

	return ret;
}
arch_initcall(arm64_dma_init);

/*
 *FIXME: from arm
 */
/*
 * The DMA API is built upon the notion of "buffer ownership".  A buffer
 * is either exclusively owned by the CPU (and therefore may be accessed
 * by it) or exclusively owned by the DMA device.  These helper functions
 * represent the transitions between these two ownership states.
 *
 * Note, however, that on later ARMs, this notion does not work due to
 * speculative prefetches.  We model our approach on the assumption that
 * the CPU does do speculative prefetches, which means we clean caches
 * before transfers and delay cache invalidation until transfer completion.
 *
 */
static void __dma_page_cpu_to_dev(struct page *, unsigned long,
		size_t, enum dma_data_direction);
static void __dma_page_dev_to_cpu(struct page *, unsigned long,
		size_t, enum dma_data_direction);

/**
 * arm_dma_map_page - map a portion of a page for streaming DMA
 * @dev: valid struct device pointer, or NULL for ISA and EISA-like devices
 * @page: page that buffer resides in
 * @offset: offset into page for start of buffer
 * @size: size of buffer to map
 * @dir: DMA transfer direction
 *
 * Ensure that any data held in the cache is appropriately discarded
 * or written back.
 *
 * The device owns this memory once this call has completed.  The CPU
 * can regain ownership by calling dma_unmap_page().
 */
static dma_addr_t arm_dma_map_page(struct device *dev, struct page *page,
	     unsigned long offset, size_t size, enum dma_data_direction dir,
	     struct dma_attrs *attrs)
{
	if (!dma_get_attr(DMA_ATTR_SKIP_CPU_SYNC, attrs))
		__dma_page_cpu_to_dev(page, offset, size, dir);
	return pfn_to_dma(dev, page_to_pfn(page)) + offset;
}

static dma_addr_t arm_coherent_dma_map_page(struct device *dev, struct page *page,
	     unsigned long offset, size_t size, enum dma_data_direction dir,
	     struct dma_attrs *attrs)
{
	return pfn_to_dma(dev, page_to_pfn(page)) + offset;
}

/**
 * arm_dma_unmap_page - unmap a buffer previously mapped through dma_map_page()
 * @dev: valid struct device pointer, or NULL for ISA and EISA-like devices
 * @handle: DMA address of buffer
 * @size: size of buffer (same as passed to dma_map_page)
 * @dir: DMA transfer direction (same as passed to dma_map_page)
 *
 * Unmap a page streaming mode DMA translation.  The handle and size
 * must match what was provided in the previous dma_map_page() call.
 * All other usages are undefined.
 *
 * After this call, reads by the CPU to the buffer are guaranteed to see
 * whatever the device wrote there.
 */
static void arm_dma_unmap_page(struct device *dev, dma_addr_t handle,
		size_t size, enum dma_data_direction dir,
		struct dma_attrs *attrs)
{
	if (!dma_get_attr(DMA_ATTR_SKIP_CPU_SYNC, attrs))
		__dma_page_dev_to_cpu(pfn_to_page(dma_to_pfn(dev, handle)),
				      handle & ~PAGE_MASK, size, dir);
}

static void arm_dma_sync_single_for_cpu(struct device *dev,
		dma_addr_t handle, size_t size, enum dma_data_direction dir)
{
	unsigned int offset = handle & (PAGE_SIZE - 1);
	struct page *page = pfn_to_page(dma_to_pfn(dev, handle-offset));
	__dma_page_dev_to_cpu(page, offset, size, dir);
}

static void arm_dma_sync_single_for_device(struct device *dev,
		dma_addr_t handle, size_t size, enum dma_data_direction dir)
{
	unsigned int offset = handle & (PAGE_SIZE - 1);
	struct page *page = pfn_to_page(dma_to_pfn(dev, handle-offset));
	__dma_page_cpu_to_dev(page, offset, size, dir);
}

struct dma_map_ops arm_dma_ops = {
	.alloc			= arm_dma_alloc,
	.free			= arm_dma_free,
	.mmap			= arm_dma_mmap,
	.get_sgtable		= arm_dma_get_sgtable,
	.map_page		= arm_dma_map_page,
	.unmap_page		= arm_dma_unmap_page,
	.map_sg			= arm_dma_map_sg,
	.unmap_sg		= arm_dma_unmap_sg,
	.sync_single_for_cpu	= arm_dma_sync_single_for_cpu,
	.sync_single_for_device	= arm_dma_sync_single_for_device,
	.sync_sg_for_cpu	= arm_dma_sync_sg_for_cpu,
	.sync_sg_for_device	= arm_dma_sync_sg_for_device,
	.set_dma_mask		= arm_dma_set_mask,
	.mapping_error		= arm_dma_mapping_error,
	.dma_supported		= arm_dma_supported,
};
EXPORT_SYMBOL(arm_dma_ops);

static void *arm_coherent_dma_alloc(struct device *dev, size_t size,
	dma_addr_t *handle, gfp_t gfp, struct dma_attrs *attrs);
static void arm_coherent_dma_free(struct device *dev, size_t size, void *cpu_addr,
				  dma_addr_t handle, struct dma_attrs *attrs);

struct dma_map_ops arm_coherent_dma_ops = {
	.alloc			= arm_coherent_dma_alloc,
	.free			= arm_coherent_dma_free,
	.mmap			= arm_dma_mmap,
	.get_sgtable		= arm_dma_get_sgtable,
	.map_page		= arm_coherent_dma_map_page,
	.map_sg			= arm_dma_map_sg,
	.set_dma_mask		= arm_dma_set_mask,
	.mapping_error		= arm_dma_mapping_error,
	.dma_supported		= arm_dma_supported,
};
EXPORT_SYMBOL(arm_coherent_dma_ops);

static u64 get_coherent_dma_mask(struct device *dev)
{
	u64 mask = (u64)arm_dma_limit;

	if (dev) {
		mask = dev->coherent_dma_mask;

		/*
		 * Sanity check the DMA mask - it must be non-zero, and
		 * must be able to be satisfied by a DMA allocation.
		 */
		if (mask == 0) {
			dev_warn(dev, "coherent DMA mask is unset\n");
			return 0;
		}

		if ((~mask) & (u64)arm_dma_limit) {
			dev_warn(dev, "coherent DMA mask %#llx is smaller "
				 "than system GFP_DMA mask %#llx\n",
				 mask, (u64)arm_dma_limit);
			return 0;
		}
	}

	return mask;
}

static void __dma_clear_buffer(struct page *page, size_t size)
{
	void *ptr;
	/*
	 * Ensure that the allocated pages are zeroed, and that any data
	 * lurking in the kernel direct-mapped region is invalidated.
	 */
	ptr = page_address(page);
	if (ptr) {
		memset(ptr, 0, size);
		__dma_flush_range(ptr, ptr + size);
		outer_flush_range(__pa(ptr), __pa(ptr) + size);
	}
}

/*
 * Allocate a DMA buffer for 'dev' of size 'size' using the
 * specified gfp mask.  Note that 'size' must be page aligned.
 */
static struct page *__dma_alloc_buffer(struct device *dev, size_t size, gfp_t gfp)
{
	unsigned long order = get_order(size);
	struct page *page, *p, *e;

	page = alloc_pages(gfp, order);
	if (!page)
		return NULL;

	/*
	 * Now split the huge page and free the excess pages
	 */
	split_page(page, order);
	for (p = page + (size >> PAGE_SHIFT), e = page + (1 << order); p < e; p++)
		__free_page(p);

	__dma_clear_buffer(page, size);

	return page;
}

/*
 * Free a DMA buffer.  'size' must be page aligned.
 */
static void __dma_free_buffer(struct page *page, size_t size)
{
	struct page *e = page + (size >> PAGE_SHIFT);

	while (page < e) {
		__free_page(page);
		page++;
	}
}

#ifdef CONFIG_MMU
#ifdef CONFIG_HUGETLB_PAGE
#error ARM Coherent DMA allocator does not (yet) support huge TLB
#endif

static void *__alloc_from_contiguous(struct device *dev, size_t size,
				     pgprot_t prot, struct page **ret_page);

static void *__alloc_remap_buffer(struct device *dev, size_t size, gfp_t gfp,
				 pgprot_t prot, struct page **ret_page,
				 const void *caller);

static void *
__dma_alloc_remap(struct page *page, size_t size, gfp_t gfp, pgprot_t prot,
	const void *caller)
{
	struct vm_struct *area;
	unsigned long addr;

	/*
	 * DMA allocation can be mapped to user space, so lets
	 * set VM_USERMAP flags too.
	 */
	area = get_vm_area_caller(size, VM_ARM_DMA_CONSISTENT | VM_USERMAP,
				  caller);
	if (!area)
		return NULL;
	addr = (unsigned long)area->addr;
	area->phys_addr = __pfn_to_phys(page_to_pfn(page));

	if (ioremap_page_range(addr, addr + size, area->phys_addr, prot)) {
		vunmap((void *)addr);
		return NULL;
	}
	return (void *)addr;
}

static void __dma_free_remap(void *cpu_addr, size_t size)
{
	unsigned int flags = VM_ARM_DMA_CONSISTENT | VM_USERMAP;
	struct vm_struct *area = find_vm_area(cpu_addr);
	if (!area || (area->flags & flags) != flags) {
		WARN(1, "trying to free invalid coherent area: %p\n", cpu_addr);
		return;
	}
	unmap_kernel_range((unsigned long)cpu_addr, size);
	vunmap(cpu_addr);
}

struct dma_contig_early_reserve {
	phys_addr_t base;
	unsigned long size;
};

static struct dma_contig_early_reserve dma_mmu_remap[MAX_CMA_AREAS] __initdata;

static int dma_mmu_remap_num __initdata;

void __init dma_contiguous_early_fixup(phys_addr_t base, unsigned long size)
{
	dma_mmu_remap[dma_mmu_remap_num].base = base;
	dma_mmu_remap[dma_mmu_remap_num].size = size;
	dma_mmu_remap_num++;
}

__init void iotable_init_va(struct map_desc *io_desc, int nr);
__init void iotable_init_mapping(struct map_desc *io_desc, int nr);

void __init dma_contiguous_remap(void)
{
	int i;
	for (i = 0; i < dma_mmu_remap_num; i++) {
		phys_addr_t start = dma_mmu_remap[i].base;
		phys_addr_t end = start + dma_mmu_remap[i].size;
		struct map_desc map;
		unsigned long addr;

		if (start >= end)
			continue;

		map.type = MT_MEMORY_KERNEL_EXEC;

		/*
		 * Clear previous low-memory mapping
		 */
		for (addr = __phys_to_virt(start); addr < __phys_to_virt(end);
		     addr += PMD_SIZE)
			pmd_clear(pmd_off_k(addr));

		for (addr = start; addr < end; addr += PAGE_SIZE) {
			map.pfn = __phys_to_pfn(addr);
			map.virtual = __phys_to_virt(addr);
			map.length = PAGE_SIZE;
			iotable_init_mapping(&map, 1);
		}

		map.pfn = __phys_to_pfn(start);
		map.virtual = __phys_to_virt(start);
		map.length = end - start;
		iotable_init_va(&map, 1);
	}
}

static void *__alloc_remap_buffer(struct device *dev, size_t size, gfp_t gfp,
				 pgprot_t prot, struct page **ret_page,
				 const void *caller)
{
	struct page *page;
	void *ptr;
	page = __dma_alloc_buffer(dev, size, gfp);
	if (!page)
		return NULL;

	ptr = __dma_alloc_remap(page, size, gfp, prot, caller);
	if (!ptr) {
		__dma_free_buffer(page, size);
		return NULL;
	}

	*ret_page = page;
	return ptr;
}

static void *__alloc_from_contiguous(struct device *dev, size_t size,
				     pgprot_t prot, struct page **ret_page)
{
	unsigned long order = get_order(size);
	size_t count = size >> PAGE_SHIFT;
	struct page *page;

	page = dma_alloc_from_contiguous(dev, count, order);
	if (!page)
		return NULL;

	*ret_page = page;
	return page_address(page);
}

static void __free_from_contiguous(struct device *dev, struct page *page,
				   size_t size)
{
	dma_release_from_contiguous(dev, page, size >> PAGE_SHIFT);
}

#define nommu() 0

#else	/* !CONFIG_MMU */

#define nommu() 1

#define __alloc_remap_buffer(dev, size, gfp, prot, ret, c)	NULL
#define __alloc_from_pool(size, ret_page)			NULL
#define __alloc_from_contiguous(dev, size, prot, ret)		NULL
#define __free_from_pool(cpu_addr, size)			0
#define __free_from_contiguous(dev, page, size)			do { } while (0)
#define __dma_free_remap(cpu_addr, size)			do { } while (0)

#endif	/* CONFIG_MMU */

static void *__alloc_simple_buffer(struct device *dev, size_t size, gfp_t gfp,
				   struct page **ret_page)
{
	struct page *page;
	page = __dma_alloc_buffer(dev, size, gfp);
	if (!page)
		return NULL;

	*ret_page = page;
	return page_address(page);
}



static void *__dma_alloc(struct device *dev, size_t size, dma_addr_t *handle,
			 gfp_t gfp, pgprot_t prot, bool is_coherent,
			 struct dma_attrs *attrs, const void *caller)
{
	u64 mask = get_coherent_dma_mask(dev);
	struct page *page = NULL;
	void *addr;

#ifdef CONFIG_DMA_API_DEBUG
	u64 limit = (mask + 1) & ~mask;
	if (limit && size >= limit) {
		dev_warn(dev, "coherent allocation too big (requested %#zx mask %#llx)\n",
			size, mask);
		return NULL;
	}
#endif

	if (!mask)
		return NULL;

#ifdef CONFIG_ZONE_DMA
	if (mask == (u64)arm_dma_limit) {
		if ((gfp & GFP_ZONEMASK) != GFP_DMA &&
		    (gfp & GFP_ZONEMASK) != 0) {
			dev_warn(dev, "Invalid GFP flags(%x) passed. "
				"GFP_DMA should only be set.",
				 gfp & GFP_ZONEMASK);
			return NULL;
		}
		gfp |= GFP_DMA;
	}
#endif

	/*
	 * Following is a work-around (a.k.a. hack) to prevent pages
	 * with __GFP_COMP being passed to split_page() which cannot
	 * handle them.  The real problem is that this flag probably
	 * should be 0 on ARM as it is not supported on this
	 * platform; see CONFIG_HUGETLBFS.
	 */
	gfp &= ~(__GFP_COMP);

	size = PAGE_ALIGN(size);

	if (is_coherent || nommu())
		addr = __alloc_simple_buffer(dev, size, gfp, &page);
	else if (!(gfp & __GFP_WAIT))
		addr = __alloc_from_pool(size, &page, gfp);
	else if (!IS_ENABLED(CONFIG_CMA))
		addr = __alloc_remap_buffer(dev, size, gfp, prot, &page, caller);
	else
		addr = __alloc_from_contiguous(dev, size, prot, &page);

	if (!addr)
		return NULL;

	*handle = pfn_to_dma(dev, page_to_pfn(page));

	if (dma_get_attr(DMA_ATTR_NO_KERNEL_MAPPING, attrs)) {
		int i;
		int count = (size >> PAGE_SHIFT);
		int array_size = count * sizeof(struct page *);
		struct page **pages;

		if (array_size <= PAGE_SIZE)
			pages = kzalloc(array_size, GFP_KERNEL);
		else
			pages = vzalloc(array_size);
		for (i = 0; i < count; i++)
			pages[i] = page + i;
		return pages;
	}
	return addr;
}

/*
 * Allocate DMA-coherent memory space and return both the kernel remapped
 * virtual and bus address for that space.
 */
void *arm_dma_alloc(struct device *dev, size_t size, dma_addr_t *handle,
		    gfp_t gfp, struct dma_attrs *attrs)
{
	pgprot_t prot = __get_dma_pgprot(attrs, PG_PROT_KERNEL, false);
	void *memory;

	if (dma_alloc_from_coherent_attr(dev, size, handle, &memory, attrs))
		return memory;

	return __dma_alloc(dev, size, handle, gfp, prot, false,
			   attrs, __builtin_return_address(0));
}

static void *arm_coherent_dma_alloc(struct device *dev, size_t size,
	dma_addr_t *handle, gfp_t gfp, struct dma_attrs *attrs)
{
	pgprot_t prot = __get_dma_pgprot(attrs, PG_PROT_KERNEL, true);
	void *memory;

	if (dma_alloc_from_coherent_attr(dev, size, handle, &memory, attrs))
		return memory;

	return __dma_alloc(dev, size, handle, gfp, prot, true,
			   attrs, __builtin_return_address(0));
}

/*
 * Create userspace mapping for the DMA-coherent memory.
 */
int arm_dma_mmap(struct device *dev, struct vm_area_struct *vma,
		 void *cpu_addr, dma_addr_t dma_addr, size_t size,
		 struct dma_attrs *attrs)
{
	int ret = -ENXIO;
#ifdef CONFIG_MMU
	unsigned long nr_vma_pages = (vma->vm_end - vma->vm_start) >> PAGE_SHIFT;
	unsigned long nr_pages = PAGE_ALIGN(size) >> PAGE_SHIFT;
	unsigned long pfn = dma_to_pfn(dev, dma_addr);
	unsigned long off = vma->vm_pgoff;

	vma->vm_page_prot = __get_dma_pgprot(attrs, vma->vm_page_prot, false);

	if (dma_mmap_from_coherent(dev, vma, cpu_addr, size, &ret))
		return ret;

	if (off < nr_pages && nr_vma_pages <= (nr_pages - off)) {
		ret = remap_pfn_range(vma, vma->vm_start,
				      pfn + off,
				      vma->vm_end - vma->vm_start,
				      vma->vm_page_prot);
	}
#endif	/* CONFIG_MMU */

	return ret;
}

/*
 * Free a buffer as defined by the above mapping.
 */
static void __arm_dma_free(struct device *dev, size_t size, void *cpu_addr,
			   dma_addr_t handle, struct dma_attrs *attrs,
			   bool is_coherent)
{
	struct page *page = pfn_to_page(dma_to_pfn(dev, handle));

	if (dma_release_from_coherent_attr(dev, size, cpu_addr,
		attrs, handle))
		return;

	if (dma_get_attr(DMA_ATTR_NO_KERNEL_MAPPING, attrs)) {
		int count = size >> PAGE_SHIFT;
		int array_size = count * sizeof(struct page *);
		struct page **pages = (struct page **)cpu_addr;

		cpu_addr = (void *)page_address(pages[0]);
		if (array_size <= PAGE_SIZE)
			kfree(pages);
		else
			vfree(pages);
	}

	size = PAGE_ALIGN(size);

	if (is_coherent || nommu()) {
		__dma_free_buffer(page, size);
	} else if (__free_from_pool(cpu_addr, size)) {
		return;
	} else if (!IS_ENABLED(CONFIG_CMA)) {
		__dma_free_remap(cpu_addr, size);
		__dma_free_buffer(page, size);
	} else {
		/*
		 * Non-atomic allocations cannot be freed with IRQs disabled
		 */
		WARN_ON(irqs_disabled());
		__free_from_contiguous(dev, page, size);
	}
}

void arm_dma_free(struct device *dev, size_t size, void *cpu_addr,
		  dma_addr_t handle, struct dma_attrs *attrs)
{
	__arm_dma_free(dev, size, cpu_addr, handle, attrs, false);
}

static void arm_coherent_dma_free(struct device *dev, size_t size, void *cpu_addr,
				  dma_addr_t handle, struct dma_attrs *attrs)
{
	__arm_dma_free(dev, size, cpu_addr, handle, attrs, true);
}

int arm_dma_get_sgtable(struct device *dev, struct sg_table *sgt,
		 void *cpu_addr, dma_addr_t handle, size_t size,
		 struct dma_attrs *attrs)
{
	struct page *page = pfn_to_page(dma_to_pfn(dev, handle));
	int ret;

	ret = sg_alloc_table(sgt, 1, GFP_KERNEL);
	if (unlikely(ret))
		return ret;

	sg_set_page(sgt->sgl, page, PAGE_ALIGN(size), 0);
	return 0;
}

static void dma_cache_maint_page(struct page *page, unsigned long offset,
	size_t size, enum dma_data_direction dir,
	void (*op)(const void *, size_t, int))
{
	op(page_address(page) + offset, size, dir);
}

/*
 * Make an area consistent for devices.
 * Note: Drivers should NOT use this function directly, as it will break
 * platforms with CONFIG_DMABOUNCE.
 * Use the driver DMA support - see dma-mapping.h (dma_sync_*)
 */
static void __dma_page_cpu_to_dev(struct page *page, unsigned long off,
	size_t size, enum dma_data_direction dir)
{
	unsigned long paddr;

	dma_cache_maint_page(page, off, size, dir, __dma_map_area);

	paddr = page_to_phys(page) + off;
	if (dir == DMA_FROM_DEVICE) {
		outer_inv_range(paddr, paddr + size);
	} else {
		outer_clean_range(paddr, paddr + size);
	}
	/* FIXME: non-speculating: flush on bidirectional mappings? */
}

static void __dma_page_dev_to_cpu(struct page *page, unsigned long off,
	size_t size, enum dma_data_direction dir)
{
	unsigned long paddr = page_to_phys(page) + off;

	/* FIXME: non-speculating: not required */
	/* don't bother invalidating if DMA to device */
	if (dir != DMA_TO_DEVICE)
		outer_inv_range(paddr, paddr + size);

	dma_cache_maint_page(page, off, size, dir, __dma_unmap_area);

	/*
	 * Mark the D-cache clean for this page to avoid extra flushing.
	 */
	if (dir != DMA_TO_DEVICE && off == 0 && size >= PAGE_SIZE)
		set_bit(PG_dcache_clean, &page->flags);
}

/**
 * arm_dma_map_sg - map a set of SG buffers for streaming mode DMA
 * @dev: valid struct device pointer, or NULL for ISA and EISA-like devices
 * @sg: list of buffers
 * @nents: number of buffers to map
 * @dir: DMA transfer direction
 *
 * Map a set of buffers described by scatterlist in streaming mode for DMA.
 * This is the scatter-gather version of the dma_map_single interface.
 * Here the scatter gather list elements are each tagged with the
 * appropriate dma address and length.  They are obtained via
 * sg_dma_{address,length}.
 *
 * Device ownership issues as mentioned for dma_map_single are the same
 * here.
 */
int arm_dma_map_sg(struct device *dev, struct scatterlist *sg, int nents,
		enum dma_data_direction dir, struct dma_attrs *attrs)
{
	struct dma_map_ops *ops = get_dma_ops(dev);
	struct scatterlist *s;
	int i, j;

	for_each_sg(sg, s, nents, i) {
#ifdef CONFIG_NEED_SG_DMA_LENGTH
		s->dma_length = s->length;
#endif
		s->dma_address = ops->map_page(dev, sg_page(s), s->offset,
						s->length, dir, attrs);
		if (dma_mapping_error(dev, s->dma_address))
			goto bad_mapping;
	}
	return nents;

 bad_mapping:
	for_each_sg(sg, s, i, j)
		ops->unmap_page(dev, sg_dma_address(s), sg_dma_len(s), dir, attrs);
	return 0;
}

/**
 * arm_dma_unmap_sg - unmap a set of SG buffers mapped by dma_map_sg
 * @dev: valid struct device pointer, or NULL for ISA and EISA-like devices
 * @sg: list of buffers
 * @nents: number of buffers to unmap (same as was passed to dma_map_sg)
 * @dir: DMA transfer direction (same as was passed to dma_map_sg)
 *
 * Unmap a set of streaming mode DMA translations.  Again, CPU access
 * rules concerning calls here are the same as for dma_unmap_single().
 */
void arm_dma_unmap_sg(struct device *dev, struct scatterlist *sg, int nents,
		enum dma_data_direction dir, struct dma_attrs *attrs)
{
	struct dma_map_ops *ops = get_dma_ops(dev);
	struct scatterlist *s;

	int i;

	for_each_sg(sg, s, nents, i)
		ops->unmap_page(dev, sg_dma_address(s), sg_dma_len(s), dir, attrs);
}

/**
 * arm_dma_sync_sg_for_cpu
 * @dev: valid struct device pointer, or NULL for ISA and EISA-like devices
 * @sg: list of buffers
 * @nents: number of buffers to map (returned from dma_map_sg)
 * @dir: DMA transfer direction (same as was passed to dma_map_sg)
 */
void arm_dma_sync_sg_for_cpu(struct device *dev, struct scatterlist *sg,
			int nents, enum dma_data_direction dir)
{
	struct dma_map_ops *ops = get_dma_ops(dev);
	struct scatterlist *s;
	int i;

	for_each_sg(sg, s, nents, i)
		ops->sync_single_for_cpu(dev, sg_dma_address(s), s->length,
					 dir);
}

/**
 * arm_dma_sync_sg_for_device
 * @dev: valid struct device pointer, or NULL for ISA and EISA-like devices
 * @sg: list of buffers
 * @nents: number of buffers to map (returned from dma_map_sg)
 * @dir: DMA transfer direction (same as was passed to dma_map_sg)
 */
void arm_dma_sync_sg_for_device(struct device *dev, struct scatterlist *sg,
			int nents, enum dma_data_direction dir)
{
	struct dma_map_ops *ops = get_dma_ops(dev);
	struct scatterlist *s;
	int i;

	for_each_sg(sg, s, nents, i)
		ops->sync_single_for_device(dev, sg_dma_address(s), s->length,
					    dir);
}

int arm_dma_supported(struct device *dev, u64 mask)
{
	if (mask < (u64)arm_dma_limit)
		return 0;
	return 1;
}

int arm_dma_set_mask(struct device *dev, u64 dma_mask)
{
	if (!dev->dma_mask || !dma_supported(dev, dma_mask))
		return -EIO;

	*dev->dma_mask = dma_mask;

	return 0;
}

int arm_dma_mapping_error(struct device *dev, dma_addr_t dev_addr)
{
	return dev_addr == DMA_ERROR_CODE;
}

#if defined(CONFIG_ARM_DMA_USE_IOMMU)

static LIST_HEAD(iommu_mapping_list);
static DEFINE_SPINLOCK(iommu_mapping_list_lock);

#if defined(CONFIG_DEBUG_FS)
static dma_addr_t bit_to_addr(size_t pos, dma_addr_t base)
{
	return base + pos * (1 << PAGE_SHIFT);
}

static void seq_print_dma_areas(struct seq_file *s, void *bitmap,
				dma_addr_t base, size_t bits)
{
	/* one bit = one (page + order) sized block */
	size_t pos = find_first_bit(bitmap, bits), end;

	for (; pos < bits; pos = find_next_bit(bitmap, bits, end + 1)) {
		dma_addr_t start_addr, end_addr;

		end = find_next_zero_bit(bitmap, bits, pos);
		start_addr = bit_to_addr(pos, base);
		end_addr = bit_to_addr(end, base) - 1;
		seq_printf(s, "    %pa-%pa pages=%zu\n",
			   &start_addr, &end_addr, end - pos);
	}
}

static void seq_print_mapping(struct seq_file *s,
			      struct dma_iommu_mapping *mapping)
{
	int i;
	size_t mapping_size = mapping->bits << PAGE_SHIFT;

	seq_printf(s, "  memory map: base=%pa size=%zu domain=%p\n",
		   &mapping->base, (size_t)(mapping->end - mapping->base),
		   mapping->domain);

	for (i = 0; i < mapping->nr_bitmaps; i++)
		seq_print_dma_areas(s, mapping->bitmaps[i],
			mapping->base + mapping_size * i, mapping->bits);
}

static void debug_dma_seq_print_mappings(struct seq_file *s)
{
	struct dma_iommu_mapping *mapping;
	int i = 0;

	list_for_each_entry(mapping, &iommu_mapping_list, list) {
		seq_printf(s, "Map %d (%p):\n", i, mapping);
		seq_print_mapping(s, mapping);
		i++;
	}
}

static int dump_iommu_mappings(struct seq_file *s, void *data)
{
	debug_dma_seq_print_mappings(s);
	return 0;
}

static int dump_iommu_mappings_open(struct inode *inode, struct file *file)
{
	return single_open(file, dump_iommu_mappings, NULL);
}

static const struct file_operations dump_iommu_mappings_fops = {
	.open           = dump_iommu_mappings_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

#endif /* CONFIG_DEBUG_FS */

void dma_debugfs_platform_info(struct dentry *dent)
{
#ifdef CONFIG_DEBUG_FS
	debugfs_create_file("dump_mappings", S_IRUGO, dent, NULL,
			    &dump_iommu_mappings_fops);
#endif
}

#else /* !CONFIG_ARM_DMA_USE_IOMMU */
static inline void dma_debugfs_platform_info(struct dentry *dent)
{
}
#endif /* !CONFIG_ARM_DMA_USE_IOMMU */

#if defined(CONFIG_DMA_API_DEBUG)
static inline void dma_debug_platform(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("dma-api", NULL);
	if (dent)
		dma_debugfs_platform_info(dent);
}
#else /* !CONFIG_DMA_API_DEBUG */
static void dma_debug_platform(void)
{
}
#endif /* !CONFIG_DMA_API_DEBUG */


#define PREALLOC_DMA_DEBUG_ENTRIES	4096

static int __init dma_debug_do_init(void)
{
	dma_debug_init(PREALLOC_DMA_DEBUG_ENTRIES);
	dma_debug_platform();
	return 0;
}
fs_initcall(dma_debug_do_init);

char *__weak debug_dma_platformdata(struct device *dev)
{
	/* empty string by default */
	static char buf[1];

	return buf;
}

#ifdef CONFIG_ARM_DMA_USE_IOMMU

/* IOMMU */

static unsigned int prefetch_page_count = 1;
static unsigned int gap_page_count = 1;

#define PF_PAGES_SIZE (prefetch_page_count << PAGE_SHIFT)
#define PG_PAGES (prefetch_page_count + gap_page_count)

static struct page *iova_gap_pages;
static phys_addr_t iova_gap_phys;

static int __init iova_gap_pages_init(void)
{
	unsigned long order = get_order(PF_PAGES_SIZE);

	iova_gap_pages = alloc_pages(GFP_KERNEL, order);
	if (WARN_ON(!iova_gap_pages)) {
		prefetch_page_count = 0;
		return 0;
	}

	__dma_clear_buffer(iova_gap_pages, PAGE_SIZE << order);
	iova_gap_phys = page_to_phys(iova_gap_pages);
	return 0;
}
core_initcall(iova_gap_pages_init);

static void iommu_mapping_list_add(struct dma_iommu_mapping *mapping)
{
	unsigned long flags;

	spin_lock_irqsave(&iommu_mapping_list_lock, flags);
	list_add_tail(&mapping->list, &iommu_mapping_list);
	spin_unlock_irqrestore(&iommu_mapping_list_lock, flags);
}

static void iommu_mapping_list_del(struct dma_iommu_mapping *mapping)
{
	unsigned long flags;

	spin_lock_irqsave(&iommu_mapping_list_lock, flags);
	list_del(&mapping->list);
	spin_unlock_irqrestore(&iommu_mapping_list_lock, flags);
}

static inline int iommu_get_num_pf_pages(struct dma_iommu_mapping *mapping,
					 struct dma_attrs *attrs)
{
	/* XXX: give priority to DMA_ATTR_SKIP_IOVA_GAP */
	if (dma_get_attr(DMA_ATTR_SKIP_IOVA_GAP, attrs))
		return 0;

	/* XXX: currently we support only 1 prefetch page */
	WARN_ON(mapping->num_pf_page > prefetch_page_count);

	return mapping->num_pf_page;
}

static inline int iommu_gap_pg_count(struct dma_iommu_mapping *mapping,
				     struct dma_attrs *attrs)
{
	if (dma_get_attr(DMA_ATTR_SKIP_IOVA_GAP, attrs))
		return 0;

	return mapping->gap_page ? gap_page_count : 0;
}

#ifdef CONFIG_DMA_API_DEBUG
static size_t _iommu_unmap(struct dma_iommu_mapping *mapping,
		unsigned long iova, size_t bytes)
{
	unsigned long start, offs, end;
	u32 bitmap_index, bitmap_last_index;
	dma_addr_t bitmap_base;
	size_t mapping_size = mapping->bits << PAGE_SHIFT;
	int i;

	bitmap_index = (u32)((iova - mapping->base) / mapping_size);
	bitmap_last_index = DIV_ROUND_UP(iova + bytes - mapping->base,
				mapping_size);
	bitmap_base = mapping->base + mapping_size * bitmap_index;

	if ((iova < mapping->base) || bitmap_index > mapping->extensions ||
			bitmap_last_index > mapping->extensions) {
		WARN(1, "trying to unmap invalid iova\n");
		return -EINVAL;
	}

	offs = (iova - bitmap_base) >> PAGE_SHIFT;
	end = offs + (PAGE_ALIGN(bytes) >> PAGE_SHIFT);
	/*
	 * [offs, end) is the portion of the requested region for unmap
	 * that falls into current bitmap.
	 * NOTE: This logic can't guarantee detection at the first faulty unmap
	 * unless the page 'end' is free. IOW this will work always when you
	 * enable gap pages.
	 */
	for (i = bitmap_index; i < bitmap_last_index;
	     i++, offs = 0, end -= mapping->bits) {
		start = find_next_zero_bit(mapping->bitmaps[i],
					   min_t(u32, mapping->bits, end),
					   offs);
		if ((start >= offs) && (start < end)) {
			WARN(1, "trying to unmap already unmapped area\n");
			return -EINVAL;
		}
	}

	return iommu_unmap(mapping->domain, iova, bytes);
}
#else
#define _iommu_unmap(mapping, iova, bytes) \
		iommu_unmap(mapping->domain, iova, bytes)
#endif

static int pg_iommu_map(struct dma_iommu_mapping *mapping, unsigned long iova,
			phys_addr_t phys, size_t len, unsigned long prot)
{
	int err;
	struct dma_attrs *attrs = (struct dma_attrs *)prot;
	struct iommu_domain *domain = mapping->domain;
	bool need_prefetch_page = !!iommu_get_num_pf_pages(mapping, attrs);

	if (need_prefetch_page) {
		err = iommu_map(domain, iova + len, iova_gap_phys,
				PF_PAGES_SIZE, prot);
		if (err)
			return err;
	}

	err = iommu_map(domain, iova, phys, len, prot);
	if (err && need_prefetch_page)
		_iommu_unmap(mapping, iova + len, PF_PAGES_SIZE);

	return err;
}

static size_t pg_iommu_unmap(struct dma_iommu_mapping *mapping,
			     unsigned long iova, size_t len, ulong prot)
{
	struct dma_attrs *attrs = (struct dma_attrs *)prot;
	struct iommu_domain *domain = mapping->domain;
	bool need_prefetch_page = !!iommu_get_num_pf_pages(mapping, attrs);

	if (need_prefetch_page) {
		phys_addr_t phys_addr;

		phys_addr = iommu_iova_to_phys(domain, iova + len);
		BUG_ON(phys_addr != iova_gap_phys);
		_iommu_unmap(mapping, iova + len, PF_PAGES_SIZE);
	}

	return _iommu_unmap(mapping, iova, len);
}

static int pg_iommu_map_sg(struct dma_iommu_mapping *mapping, unsigned long iova,
		 struct scatterlist *sgl, int nents, unsigned long prot)
{
	int err;
	struct dma_attrs *attrs = (struct dma_attrs *)prot;
	struct iommu_domain *domain = mapping->domain;
	bool need_prefetch_page = !!iommu_get_num_pf_pages(mapping, attrs);

	if (need_prefetch_page) {
		err = iommu_map(domain, iova + (nents << PAGE_SHIFT),
				iova_gap_phys, PF_PAGES_SIZE, prot);
		if (err)
			return err;
	}

	err = iommu_map_sg(domain, iova, sgl, nents, prot);
	if (err && need_prefetch_page)
		_iommu_unmap(mapping, iova + (nents << PAGE_SHIFT),
				PF_PAGES_SIZE);

	return err;
}

static size_t arm_iommu_iova_get_free_total(struct device *dev)
{
	struct dma_iommu_mapping *mapping = dev->archdata.mapping;
	unsigned long flags;
	size_t size = 0;
	size_t start = 0;
	int i;

	BUG_ON(!dev);
	BUG_ON(!mapping);

	spin_lock_irqsave(&mapping->lock, flags);
	for (i = 0; i < mapping->nr_bitmaps; i++) {
		start = 0;

		while (1) {
			size_t end;

			start = bitmap_find_next_zero_area(mapping->bitmaps[i],
							   mapping->bits, start, 1, 0);
			if (start > mapping->bits)
				break;

			end = find_next_bit(mapping->bitmaps[i], mapping->bits, start);
			size += end - start;
			start = end;
		}
	}
	spin_unlock_irqrestore(&mapping->lock, flags);
	return size << PAGE_SHIFT;
}

static size_t arm_iommu_iova_get_free_max(struct device *dev)
{
	struct dma_iommu_mapping *mapping = dev->archdata.mapping;
	unsigned long flags;
	size_t max_free = 0;
	size_t start = 0;
	int i;

	spin_lock_irqsave(&mapping->lock, flags);
	for (i = 0; i < mapping->nr_bitmaps; i++) {
		start = 0;

		while (1) {
			size_t end;

			start = bitmap_find_next_zero_area(mapping->bitmaps[i],
							   mapping->bits, start, 1, 0);
			if (start > mapping->bits)
				break;

			end = find_next_bit(mapping->bitmaps[i], mapping->bits, start);
			max_free = max_t(size_t, max_free, end - start);
			start = end;
		}
	}
	spin_unlock_irqrestore(&mapping->lock, flags);
	return max_free << PAGE_SHIFT;
}

static int extend_iommu_mapping(struct dma_iommu_mapping *mapping);

static inline dma_addr_t __alloc_iova(struct dma_iommu_mapping *mapping,
				      size_t size, struct dma_attrs *attrs)
{
	unsigned int order = get_order(size);
	unsigned int align = 0;
	unsigned int count;
	unsigned long flags;
	dma_addr_t iova;
	size_t start = 0;
	int i;

	if (mapping->alignment && order > get_order(mapping->alignment))
		order = get_order(mapping->alignment);

	count = PAGE_ALIGN(size) >> PAGE_SHIFT;
	count += iommu_get_num_pf_pages(mapping, attrs);
	count += iommu_gap_pg_count(mapping, attrs);

	align = (1 << order) - 1;

	spin_lock_irqsave(&mapping->lock, flags);
	for (i = 0; i < mapping->nr_bitmaps; i++) {
		start = bitmap_find_next_zero_area(mapping->bitmaps[i],
				mapping->bits, 0, count, align);

		if (start > mapping->bits)
			continue;

		bitmap_set(mapping->bitmaps[i], start, count);
		break;
	}

	/*
	 * No unused range found. Try to extend the existing mapping
	 * and perform a second attempt to reserve an IO virtual
	 * address range of size bytes.
	 */
	if (i == mapping->nr_bitmaps) {
		if (extend_iommu_mapping(mapping)) {
			spin_unlock_irqrestore(&mapping->lock, flags);
			return DMA_ERROR_CODE;
		}

		start = bitmap_find_next_zero_area(mapping->bitmaps[i],
				mapping->bits, 0, count, align);

		if (start > mapping->bits) {
			spin_unlock_irqrestore(&mapping->lock, flags);
			return DMA_ERROR_CODE;
		}

		bitmap_set(mapping->bitmaps[i], start, count);
	}
	spin_unlock_irqrestore(&mapping->lock, flags);

	iova = mapping->base + ((mapping->bits << PAGE_SHIFT) * i);
	iova += start << PAGE_SHIFT;

	return iova;
}

static dma_addr_t __alloc_iova_at(struct dma_iommu_mapping *mapping,
				  dma_addr_t *iova, size_t size,
				  struct dma_attrs *attrs)
{
	size_t count, start, orig;
	size_t current_size, total_size;
	size_t mapping_size = mapping->bits << PAGE_SHIFT;
	unsigned long flags;
	size_t bytes;
	size_t bitmap_index, bitmap_last_index, num_bitmaps;
	dma_addr_t bitmap_base;
	unsigned int i;

	count = PAGE_ALIGN(size) >> PAGE_SHIFT;

	count += iommu_get_num_pf_pages(mapping, attrs);
	count += iommu_gap_pg_count(mapping, attrs);

	bytes = count << PAGE_SHIFT;

	bitmap_index = (*iova - mapping->base) / mapping_size;
	bitmap_last_index = (*iova + bytes - mapping->base) / mapping_size;
	num_bitmaps = bitmap_last_index - bitmap_index + 1;
	bitmap_base = mapping->base + mapping_size * bitmap_index;

	if ((*iova < mapping->base) || bitmap_index > mapping->extensions ||
			bitmap_last_index > mapping->extensions ||
			(bytes > (bitmap_base + mapping_size * num_bitmaps) - *iova)) {
		*iova = -ENXIO;
		return DMA_ERROR_CODE;
	}

	orig = (*iova - bitmap_base) >> PAGE_SHIFT;

	spin_lock_irqsave(&mapping->lock, flags);

	while (mapping->nr_bitmaps <= bitmap_last_index) {
		if (extend_iommu_mapping(mapping)) {
			*iova = -ENXIO;
			goto err_out;
		}
	}

	current_size = min(((size_t)bitmap_base + mapping_size - (size_t)*iova) >> PAGE_SHIFT, count);
	total_size = count - current_size;
	start = bitmap_find_next_zero_area(mapping->bitmaps[bitmap_index],
					   mapping->bits, orig, current_size, 0);

	if ((start > mapping->bits) || (orig != start)) {
		*iova = -EINVAL;
		goto err_out;
	}

	for (i = bitmap_index + 1; i <= bitmap_last_index; i++) {
		current_size = min(mapping->bits, total_size);
		total_size = total_size - current_size;
		start = bitmap_find_next_zero_area(mapping->bitmaps[i],
					mapping->bits, 0, current_size, 0);

		if (start != 0) {
			*iova = -EINVAL;
			goto err_out;
		}
	}

	current_size = min(((size_t)bitmap_base + mapping_size - (size_t)*iova) >> PAGE_SHIFT, count);
	total_size = count - current_size;
	bitmap_set(mapping->bitmaps[bitmap_index], orig, current_size);

	for (i = bitmap_index + 1; i <= bitmap_last_index; i++) {
		current_size = min(mapping->bits, total_size);
		total_size = total_size - current_size;
		bitmap_set(mapping->bitmaps[i], 0, current_size);
	}

	spin_unlock_irqrestore(&mapping->lock, flags);

	return bitmap_base + (start << PAGE_SHIFT);

err_out:
	spin_unlock_irqrestore(&mapping->lock, flags);
	return DMA_ERROR_CODE;
}

static dma_addr_t arm_iommu_iova_alloc_at(struct device *dev, dma_addr_t *iova,
					  size_t size, struct dma_attrs *attrs)
{
	struct dma_iommu_mapping *mapping = dev->archdata.mapping;

	return __alloc_iova_at(mapping, iova, size, attrs);
}

static dma_addr_t arm_iommu_iova_alloc(struct device *dev, size_t size,
				       struct dma_attrs *attrs)
{
	struct dma_iommu_mapping *mapping = dev->archdata.mapping;

	return __alloc_iova(mapping, size, attrs);
}

static inline void __free_iova(struct dma_iommu_mapping *mapping,
			       dma_addr_t addr, size_t size,
			       struct dma_attrs *attrs)
{
	unsigned int count;
	size_t mapping_size = mapping->bits << PAGE_SHIFT;
	unsigned long flags;
	dma_addr_t bitmap_base;
	u32 bitmap_index;
	size_t start;

	if (!size)
		return;

	bitmap_index = (u32) ((addr - mapping->base) / mapping_size);
	BUG_ON(addr < mapping->base || bitmap_index > mapping->extensions);

	bitmap_base = mapping->base + mapping_size * bitmap_index;

	start = (addr - bitmap_base) >>	PAGE_SHIFT;

	if (addr + size > bitmap_base + mapping_size) {
		/*
		 * The address range to be freed reaches into the iova
		 * range of the next bitmap. This should not happen as
		 * we don't allow this in __alloc_iova (at the
		 * moment).
		 */
		BUG();
	} else
		count = size >> PAGE_SHIFT;

	count += iommu_get_num_pf_pages(mapping, attrs);
	count += iommu_gap_pg_count(mapping, attrs);

	spin_lock_irqsave(&mapping->lock, flags);
	bitmap_clear(mapping->bitmaps[bitmap_index], start, count);
	spin_unlock_irqrestore(&mapping->lock, flags);
}

static void arm_iommu_iova_free(struct device *dev, dma_addr_t addr,
				size_t size, struct dma_attrs *attrs)
{
	struct dma_iommu_mapping *mapping = dev->archdata.mapping;

	__free_iova(mapping, addr, size, attrs);
}

static struct page **__iommu_alloc_buffer(struct device *dev, size_t size,
					  gfp_t gfp, struct dma_attrs *attrs)
{
	struct page **pages;
	int count = size >> PAGE_SHIFT;
	int array_size = count * sizeof(struct page *);
	int i = 0;

	if (array_size <= PAGE_SIZE)
		pages = kzalloc(array_size, GFP_KERNEL);
	else
		pages = vzalloc(array_size);
	if (!pages)
		return NULL;

	if (dma_get_attr(DMA_ATTR_FORCE_CONTIGUOUS, attrs))
	{
		unsigned long order = get_order(size);
		struct page *page;

		page = dma_alloc_from_contiguous(dev, count, order);
		if (!page)
			goto error;

		for (i = 0; i < count; i++)
			pages[i] = page + i;

		dmastats_alloc_or_map(dev, size, ALLOC_OR_FREE);
		return pages;
	}

	/*
	 * IOMMU can map any pages, so himem can also be used here
	 */
	if (!(gfp & GFP_DMA) && !(gfp & GFP_DMA32))
		gfp |= __GFP_HIGHMEM;

	gfp |= __GFP_NOWARN;

	while (count) {
		int j, order = __fls(count);

		pages[i] = alloc_pages(gfp, order);
		while (!pages[i] && order)
			pages[i] = alloc_pages(gfp, --order);
		if (!pages[i])
			goto error;

		if (order) {
			split_page(pages[i], order);
			j = 1 << order;
			while (--j)
				pages[i + j] = pages[i] + j;
		}

		__dma_clear_buffer(pages[i], PAGE_SIZE << order);
		i += 1 << order;
		count -= 1 << order;
	}

	dmastats_alloc_or_map(dev, size, ALLOC_OR_FREE);
	return pages;
error:
	while (i--)
		if (pages[i])
			__free_pages(pages[i], 0);
	if (array_size <= PAGE_SIZE)
		kfree(pages);
	else
		vfree(pages);
	return NULL;
}

static int __iommu_free_buffer(struct device *dev, struct page **pages,
			       size_t size, struct dma_attrs *attrs)
{
	int count = size >> PAGE_SHIFT;
	int array_size = count * sizeof(struct page *);
	int i;

	if (dma_get_attr(DMA_ATTR_FORCE_CONTIGUOUS, attrs)) {
		dma_release_from_contiguous(dev, pages[0], count);
	} else {
		for (i = 0; i < count; i++)
			if (pages[i])
				__free_pages(pages[i], 0);
	}

	if (array_size <= PAGE_SIZE)
		kfree(pages);
	else
		vfree(pages);

	dmastats_free_or_unmap(dev, size, ALLOC_OR_FREE);
	return 0;
}

/*
 * Create a CPU mapping for a specified pages
 */
static void *
__iommu_alloc_remap(struct page **pages, size_t size, gfp_t gfp, pgprot_t prot,
		    const void *caller)
{
	unsigned int i, nr_pages = PAGE_ALIGN(size) >> PAGE_SHIFT;
	struct vm_struct *area;
	unsigned long p;

	area = get_vm_area_caller(size, VM_ARM_DMA_CONSISTENT | VM_USERMAP,
				  caller);
	if (!area)
		return NULL;

	area->pages = pages;
	area->nr_pages = nr_pages;
	p = (unsigned long)area->addr;

	for (i = 0; i < nr_pages; i++) {
		phys_addr_t phys = __pfn_to_phys(page_to_pfn(pages[i]));
		if (ioremap_page_range(p, p + PAGE_SIZE, phys, prot))
			goto err;
		p += PAGE_SIZE;
	}
	return area->addr;
err:
	unmap_kernel_range((unsigned long)area->addr, size);
	vunmap(area->addr);
	return NULL;
}

#ifdef CONFIG_DMA_API_DEBUG
static void *
___iommu_alloc_remap(struct device *dev, struct page **pages, size_t size,
		    gfp_t gfp, pgprot_t prot, const void *caller)
{
	void *ptr = __iommu_alloc_remap(pages, size, gfp, prot, caller);

	if (ptr)
		dmastats_alloc_or_map(dev, size, CPU_MAP_OR_UNMAP);
	return ptr;
}
#define  __iommu_alloc_remap(pages, size, gfp, prot, caller)	\
	 ___iommu_alloc_remap(dev, pages, size, gfp, prot, caller)
#endif

/*
 * Create a mapping in device IO address space for specified pages
 */
static dma_addr_t
____iommu_create_mapping(struct device *dev, dma_addr_t *req,
			 struct page **pages, size_t size,
			 struct dma_attrs *attrs)
{
	struct dma_iommu_mapping *mapping = dev->archdata.mapping;
	unsigned int count = PAGE_ALIGN(size) >> PAGE_SHIFT;
	dma_addr_t dma_addr, iova;
	int i;

	if (req)
		dma_addr = __alloc_iova_at(mapping, req, size, attrs);
	else
		dma_addr = __alloc_iova(mapping, size, attrs);

	if (dma_addr == DMA_ERROR_CODE)
		return dma_addr;

	iova = dma_addr;
	for (i = 0; i < count; ) {
		int ret;
		unsigned int next_pfn = page_to_pfn(pages[i]) + 1;
		phys_addr_t phys = page_to_phys(pages[i]);
		unsigned int len, j;

		for (j = i + 1; j < count; j++, next_pfn++)
			if (page_to_pfn(pages[j]) != next_pfn)
				break;

		len = (j - i) << PAGE_SHIFT;
		ret = iommu_map(mapping->domain, iova, phys, len, (ulong)attrs);
		if (ret < 0)
			goto fail;
		iova += len;
		i = j;
	}

	if (iommu_get_num_pf_pages(mapping, attrs)) {
		int err = iommu_map(mapping->domain, iova, iova_gap_phys,
				    PF_PAGES_SIZE, (ulong)attrs);
		if (err)
			goto fail;
	}

	dmastats_alloc_or_map(dev, iova - dma_addr, MAP_OR_UNMAP);
	return dma_addr;
fail:
	_iommu_unmap(mapping, dma_addr, iova - dma_addr);
	__free_iova(mapping, dma_addr, size, attrs);
	return DMA_ERROR_CODE;
}

static dma_addr_t
__iommu_create_mapping(struct device *dev, struct page **pages, size_t size,
		struct dma_attrs *attrs)
{
	return ____iommu_create_mapping(dev, NULL, pages, size, attrs);
}

static int __iommu_remove_mapping(struct device *dev, dma_addr_t iova,
				  size_t size, struct dma_attrs *attrs)
{
	struct dma_iommu_mapping *mapping = dev->archdata.mapping;

	/*
	 * add optional in-page offset from iova to size and align
	 * result to page size
	 */
	size = PAGE_ALIGN((iova & ~PAGE_MASK) + size);
	iova &= PAGE_MASK;

	pg_iommu_unmap(mapping, iova, size, (ulong)attrs);
	__free_iova(mapping, iova, size, attrs);

	dmastats_alloc_or_map(dev, size, MAP_OR_UNMAP);
	return 0;
}

static struct page **__iommu_get_pages(void *cpu_addr, struct dma_attrs *attrs)
{
	struct vm_struct *area;
	int offset = 0;

	if (__in_atomic_pool(cpu_addr, PAGE_SIZE)) {
		phys_addr_t phys = gen_pool_virt_to_phys(atomic_pool,
						(unsigned long)cpu_addr);
		struct gen_pool_chunk *chunk;

		rcu_read_lock();
		/* NOTE: this works as only a single chunk is present
		 * for atomic pool
		 */
		chunk = list_first_or_null_rcu(&atomic_pool->chunks,
					       struct gen_pool_chunk,
					       next_chunk);
		phys -= chunk->phys_addr;
		rcu_read_unlock();
		offset = phys >> PAGE_SHIFT;
		return atomic_pool_pages + offset;
	}

	if (dma_get_attr(DMA_ATTR_NO_KERNEL_MAPPING, attrs))
		return cpu_addr;

	area = find_vm_area(cpu_addr);
	if (area && (area->flags & VM_ARM_DMA_CONSISTENT))
		return &area->pages[offset];
	return NULL;
}

static void *__iommu_alloc_atomic(struct device *dev, size_t size,
				  dma_addr_t *handle, struct dma_attrs *attrs)
{
	struct page *page;
	struct page **pages;
	void *addr;

	addr = __alloc_from_pool(size, &page, GFP_ATOMIC);
	if (!addr)
		return NULL;

	pages = __iommu_get_pages(addr, attrs);
	*handle = __iommu_create_mapping(dev, pages, size, attrs);
	if (*handle == DMA_ERROR_CODE)
		goto err_mapping;

	dev_dbg(dev, "%s() %16llx(%zx)\n", __func__, *handle, size);
	return addr;

err_mapping:
	__free_from_pool(addr, size);
	return NULL;
}

static void __iommu_free_atomic(struct device *dev, struct page **pages,
				dma_addr_t handle, size_t size, struct dma_attrs *attrs)
{
	__iommu_remove_mapping(dev, handle, size, attrs);
	__free_from_pool(page_address(pages[0]), size);
	dev_dbg(dev, "%s() %16llx(%zx)\n", __func__, handle, size);
}

static void *arm_iommu_alloc_attrs(struct device *dev, size_t size,
	    dma_addr_t *handle, gfp_t gfp, struct dma_attrs *attrs)
{
	pgprot_t prot = __get_dma_pgprot(attrs, PG_PROT_KERNEL, false);
	struct page **pages;
	void *addr = NULL;

	/* Following is a work-around (a.k.a. hack) to prevent pages
	 * with __GFP_COMP being passed to split_page() which cannot
	 * handle them.  The real problem is that this flag probably
	 * should be 0 on ARM as it is not supported on this
	 * platform--see CONFIG_HUGETLB_PAGE. */
	gfp &= ~(__GFP_COMP);

	size = PAGE_ALIGN(size);

	if (gfp & GFP_ATOMIC)
		return __iommu_alloc_atomic(dev, size, handle, attrs);

	pages = __iommu_alloc_buffer(dev, size, gfp, attrs);
	if (!pages)
		return NULL;

	if (*handle == DMA_ERROR_CODE)
		*handle = __iommu_create_mapping(dev, pages, size, attrs);
	else
		*handle = ____iommu_create_mapping(dev, handle, pages, size,
						   attrs);

	if (*handle == DMA_ERROR_CODE)
		goto err_buffer;

	if (dma_get_attr(DMA_ATTR_NO_KERNEL_MAPPING, attrs))
		return pages;

	addr = __iommu_alloc_remap(pages, size, gfp, prot,
				   __builtin_return_address(0));
	if (!addr)
		goto err_mapping;

	return addr;

err_mapping:
	__iommu_remove_mapping(dev, *handle, size, attrs);
err_buffer:
	__iommu_free_buffer(dev, pages, size, attrs);
	return NULL;
}

static int arm_iommu_mmap_attrs(struct device *dev, struct vm_area_struct *vma,
		    void *cpu_addr, dma_addr_t dma_addr, size_t size,
		    struct dma_attrs *attrs)
{
	unsigned long uaddr = vma->vm_start;
	unsigned long usize = vma->vm_end - vma->vm_start;
	struct page **pages = __iommu_get_pages(cpu_addr, attrs);

	vma->vm_page_prot = __get_dma_pgprot(attrs, vma->vm_page_prot, false);

	if (!pages)
		return -ENXIO;

	do {
		int ret = vm_insert_page(vma, uaddr, *pages++);
		if (ret) {
			pr_err("Remapping memory failed: %d\n", ret);
			return ret;
		}
		uaddr += PAGE_SIZE;
		usize -= PAGE_SIZE;
	} while (usize > 0);

	return 0;
}

/*
 * free a page as defined by the above mapping.
 * Must not be called with IRQs disabled.
 */
void arm_iommu_free_attrs(struct device *dev, size_t size, void *cpu_addr,
			  dma_addr_t handle, struct dma_attrs *attrs)
{
	struct page **pages = __iommu_get_pages(cpu_addr, attrs);
	size = PAGE_ALIGN(size);

	if (!pages) {
		WARN(1, "trying to free invalid coherent area: %p\n", cpu_addr);
		return;
	}

	if (__in_atomic_pool(cpu_addr, size)) {
		__iommu_free_atomic(dev, pages, handle, size, attrs);
		return;
	}

	if (!dma_get_attr(DMA_ATTR_NO_KERNEL_MAPPING, attrs)) {
		unmap_kernel_range((unsigned long)cpu_addr, size);
		vunmap(cpu_addr);
	}

	__iommu_remove_mapping(dev, handle, size, attrs);
	__iommu_free_buffer(dev, pages, size, attrs);
}

static int arm_iommu_get_sgtable(struct device *dev, struct sg_table *sgt,
				 void *cpu_addr, dma_addr_t dma_addr,
				 size_t size, struct dma_attrs *attrs)
{
	unsigned int count = PAGE_ALIGN(size) >> PAGE_SHIFT;
	struct page **pages = __iommu_get_pages(cpu_addr, attrs);

	if (!pages)
		return -ENXIO;

	return sg_alloc_table_from_pages(sgt, pages, count, 0, size,
					 GFP_KERNEL);
}

/*
 * Map a part of the scatter-gather list into contiguous io address space
 */
static int __map_sg_chunk(struct device *dev, struct scatterlist *sg,
			  size_t size, dma_addr_t *handle,
			  enum dma_data_direction dir, struct dma_attrs *attrs,
			  bool is_coherent)
{
	struct dma_iommu_mapping *mapping = dev->archdata.mapping;
	dma_addr_t iova, iova_base;
	int ret = 0;
	unsigned int count;
	struct scatterlist *s;

	size = PAGE_ALIGN(size);
	*handle = DMA_ERROR_CODE;

	iova_base = iova = __alloc_iova(mapping, size, attrs);
	if (iova == DMA_ERROR_CODE)
		return -ENOMEM;

	if (is_coherent || dma_get_attr(DMA_ATTR_SKIP_CPU_SYNC, attrs))
		goto skip_cmaint;

	for (count = 0, s = sg; count < (size >> PAGE_SHIFT); s = sg_next(s)) {
		unsigned int len = PAGE_ALIGN(s->offset + s->length);

		__dma_page_cpu_to_dev(sg_page(s), s->offset, s->length, dir);

		count += len >> PAGE_SHIFT;
		iova += len;
	}

skip_cmaint:
	count = size >> PAGE_SHIFT;
	ret = pg_iommu_map_sg(mapping, iova_base, sg, count,
			      (ulong)attrs);
	if (WARN_ON(ret < 0))
		goto fail;

	*handle = iova_base;

	return 0;
fail:
	__free_iova(mapping, iova_base, size, attrs);
	return ret;
}

static int __iommu_map_sg(struct device *dev, struct scatterlist *sg, int nents,
		     enum dma_data_direction dir, struct dma_attrs *attrs,
		     bool is_coherent)
{
	struct scatterlist *s = sg, *dma = sg, *start = sg;
	int i, count = 0;
	unsigned int offset = s->offset;
	unsigned int size = s->offset + s->length;
	unsigned int max = dma_get_max_seg_size(dev);

	for (i = 1; i < nents; i++) {
		s = sg_next(s);

		s->dma_address = DMA_ERROR_CODE;
		s->dma_length = 0;

		if (s->offset || (size & ~PAGE_MASK) || size + s->length > max) {
			if (__map_sg_chunk(dev, start, size, &dma->dma_address,
			    dir, attrs, is_coherent) < 0)
				goto bad_mapping;

			dma->dma_address += offset;
			dma->dma_length = size - offset;

			size = offset = s->offset;
			start = s;
			dma = sg_next(dma);
			count += 1;
		}
		size += s->length;
	}
	if (__map_sg_chunk(dev, start, size, &dma->dma_address, dir, attrs,
		is_coherent) < 0)
		goto bad_mapping;

	dma->dma_address += offset;
	dma->dma_length = size - offset;

	trace_dmadebug_map_sg(dev, dma->dma_address, dma->dma_length,
			      sg_page(sg));
	dmastats_alloc_or_map(dev, size, MAP_OR_UNMAP);
	return count+1;

bad_mapping:
	for_each_sg(sg, s, count, i)
		__iommu_remove_mapping(dev, sg_dma_address(s), sg_dma_len(s),
			attrs);
	return 0;
}

/**
 * arm_coherent_iommu_map_sg - map a set of SG buffers for streaming mode DMA
 * @dev: valid struct device pointer
 * @sg: list of buffers
 * @nents: number of buffers to map
 * @dir: DMA transfer direction
 *
 * Map a set of i/o coherent buffers described by scatterlist in streaming
 * mode for DMA. The scatter gather list elements are merged together (if
 * possible) and tagged with the appropriate dma address and length. They are
 * obtained via sg_dma_{address,length}.
 */
int arm_coherent_iommu_map_sg(struct device *dev, struct scatterlist *sg,
		int nents, enum dma_data_direction dir, struct dma_attrs *attrs)
{
	return __iommu_map_sg(dev, sg, nents, dir, attrs, true);
}

/**
 * arm_iommu_map_sg - map a set of SG buffers for streaming mode DMA
 * @dev: valid struct device pointer
 * @sg: list of buffers
 * @nents: number of buffers to map
 * @dir: DMA transfer direction
 *
 * Map a set of buffers described by scatterlist in streaming mode for DMA.
 * The scatter gather list elements are merged together (if possible) and
 * tagged with the appropriate dma address and length. They are obtained via
 * sg_dma_{address,length}.
 */
int arm_iommu_map_sg(struct device *dev, struct scatterlist *sg,
		int nents, enum dma_data_direction dir, struct dma_attrs *attrs)
{
	return __iommu_map_sg(dev, sg, nents, dir, attrs, false);
}

static void __iommu_unmap_sg(struct device *dev, struct scatterlist *sg,
		int nents, enum dma_data_direction dir, struct dma_attrs *attrs,
		bool is_coherent)
{
	struct scatterlist *s;
	int i;

	for_each_sg(sg, s, nents, i) {
		if (sg_dma_len(s))
			__iommu_remove_mapping(dev, sg_dma_address(s),
					       sg_dma_len(s), attrs);
		if (!is_coherent &&
		    !dma_get_attr(DMA_ATTR_SKIP_CPU_SYNC, attrs))
			__dma_page_dev_to_cpu(sg_page(s), s->offset,
					      s->length, dir);
	}

	trace_dmadebug_unmap_sg(dev, sg_dma_address(sg), sg_dma_len(sg),
				sg_page(sg));
}

/**
 * arm_coherent_iommu_unmap_sg - unmap a set of SG buffers mapped by dma_map_sg
 * @dev: valid struct device pointer
 * @sg: list of buffers
 * @nents: number of buffers to unmap (same as was passed to dma_map_sg)
 * @dir: DMA transfer direction (same as was passed to dma_map_sg)
 *
 * Unmap a set of streaming mode DMA translations.  Again, CPU access
 * rules concerning calls here are the same as for dma_unmap_single().
 */
void arm_coherent_iommu_unmap_sg(struct device *dev, struct scatterlist *sg,
		int nents, enum dma_data_direction dir, struct dma_attrs *attrs)
{
	__iommu_unmap_sg(dev, sg, nents, dir, attrs, true);
}

/**
 * arm_iommu_unmap_sg - unmap a set of SG buffers mapped by dma_map_sg
 * @dev: valid struct device pointer
 * @sg: list of buffers
 * @nents: number of buffers to unmap (same as was passed to dma_map_sg)
 * @dir: DMA transfer direction (same as was passed to dma_map_sg)
 *
 * Unmap a set of streaming mode DMA translations.  Again, CPU access
 * rules concerning calls here are the same as for dma_unmap_single().
 */
void arm_iommu_unmap_sg(struct device *dev, struct scatterlist *sg, int nents,
			enum dma_data_direction dir, struct dma_attrs *attrs)
{
	__iommu_unmap_sg(dev, sg, nents, dir, attrs, false);
}

/**
 * arm_iommu_sync_sg_for_cpu
 * @dev: valid struct device pointer
 * @sg: list of buffers
 * @nents: number of buffers to map (returned from dma_map_sg)
 * @dir: DMA transfer direction (same as was passed to dma_map_sg)
 */
void arm_iommu_sync_sg_for_cpu(struct device *dev, struct scatterlist *sg,
			int nents, enum dma_data_direction dir)
{
	struct scatterlist *s;
	int i;

	for_each_sg(sg, s, nents, i)
		__dma_page_dev_to_cpu(sg_page(s), s->offset, s->length, dir);

}

/**
 * arm_iommu_sync_sg_for_device
 * @dev: valid struct device pointer
 * @sg: list of buffers
 * @nents: number of buffers to map (returned from dma_map_sg)
 * @dir: DMA transfer direction (same as was passed to dma_map_sg)
 */
void arm_iommu_sync_sg_for_device(struct device *dev, struct scatterlist *sg,
			int nents, enum dma_data_direction dir)
{
	struct scatterlist *s;
	int i;

	for_each_sg(sg, s, nents, i)
		__dma_page_cpu_to_dev(sg_page(s), s->offset, s->length, dir);
}


/**
 * arm_coherent_iommu_map_page
 * @dev: valid struct device pointer
 * @page: page that buffer resides in
 * @offset: offset into page for start of buffer
 * @size: size of buffer to map
 * @dir: DMA transfer direction
 *
 * Coherent IOMMU aware version of arm_dma_map_page()
 */
static dma_addr_t arm_coherent_iommu_map_page(struct device *dev, struct page *page,
	     unsigned long offset, size_t size, enum dma_data_direction dir,
	     struct dma_attrs *attrs)
{
	struct dma_iommu_mapping *mapping = dev->archdata.mapping;
	dma_addr_t dma_addr;
	int ret, len = PAGE_ALIGN(size + offset);
	int page_offset;

	/*
	 * In some cases the offset is greater than 1 page. In such cases we
	 * allocate multiple pages in the IOVA address space. However, when
	 * freeing that mapping we only have the base address (which includes
	 * the offset addition) and size so the pages mapped before offset get
	 * lost.
	 *
	 * What we do here to avoid leaking IOVA pages is only map the pages
	 * actually necessary for the mapping. Any pages fully before the
	 * offset are not mapped since those are the pages that will be missed
	 * by the unmap.
	 *
	 * The primary culprit here is the network stack. Sometimes the skbuffs
	 * are backed by multiple pages and fragments above the page boundary
	 * are mapped. The skbuff code does not work out the specific page that
	 * needs mapping though.
	 */
	page_offset = offset >> PAGE_SHIFT;
	len -= page_offset * PAGE_SIZE;

	/*
	 * However, if page_offset is greater than 0, and the passed page is not
	 * compound page then there's probably a bug somewhere.
	 */
	if (page_offset > 0)
		BUG_ON(page_offset > (1 << compound_order(compound_head(page)))
			- ((page - compound_head(page)) << PAGE_SHIFT));

	dma_addr = __alloc_iova(mapping, len, attrs);
	if (dma_addr == DMA_ERROR_CODE)
		return dma_addr;

	ret = pg_iommu_map(mapping, dma_addr,
			   page_to_phys(page + page_offset), len, (ulong)attrs);
	if (ret < 0)
		goto fail;

	trace_dmadebug_map_page(dev, dma_addr + offset, size, page);
	return dma_addr + (offset & ~PAGE_MASK);
fail:
	__free_iova(mapping, dma_addr, len, attrs);
	return DMA_ERROR_CODE;
}

/**
 * arm_iommu_map_page
 * @dev: valid struct device pointer
 * @page: page that buffer resides in
 * @offset: offset into page for start of buffer
 * @size: size of buffer to map
 * @dir: DMA transfer direction
 *
 * IOMMU aware version of arm_dma_map_page()
 */
static dma_addr_t arm_iommu_map_page(struct device *dev, struct page *page,
	     unsigned long offset, size_t size, enum dma_data_direction dir,
	     struct dma_attrs *attrs)
{
	if (!dma_get_attr(DMA_ATTR_SKIP_CPU_SYNC, attrs))
		__dma_page_cpu_to_dev(page, offset, size, dir);

	return arm_coherent_iommu_map_page(dev, page, offset, size, dir, attrs);
}

static dma_addr_t arm_iommu_map_page_at(struct device *dev, struct page *page,
		 dma_addr_t dma_addr, unsigned long offset, size_t size,
		 enum dma_data_direction dir, struct dma_attrs *attrs)
{
	struct dma_iommu_mapping *mapping = dev->archdata.mapping;
	int ret, len = PAGE_ALIGN(size + offset);

	if (!dma_get_attr(DMA_ATTR_SKIP_CPU_SYNC, attrs))
		__dma_page_cpu_to_dev(page, offset, size, dir);

	ret = pg_iommu_map(mapping, dma_addr,
			   page_to_phys(page), len, (ulong)attrs);
	if (ret < 0)
		return DMA_ERROR_CODE;

	trace_dmadebug_map_page(dev, dma_addr, size, page);
	return dma_addr + offset;
}

/**
 * arm_coherent_iommu_unmap_page
 * @dev: valid struct device pointer
 * @handle: DMA address of buffer
 * @size: size of buffer (same as passed to dma_map_page)
 * @dir: DMA transfer direction (same as passed to dma_map_page)
 *
 * Coherent IOMMU aware version of arm_dma_unmap_page()
 */
static void arm_coherent_iommu_unmap_page(struct device *dev, dma_addr_t handle,
		size_t size, enum dma_data_direction dir,
		struct dma_attrs *attrs)
{
	struct dma_iommu_mapping *mapping = dev->archdata.mapping;
	dma_addr_t iova = handle & PAGE_MASK;
	int offset = handle & ~PAGE_MASK;
	int len = PAGE_ALIGN(size + offset);

	if (!iova)
		return;

	trace_dmadebug_unmap_page(dev, handle, size,
		  phys_to_page(iommu_iova_to_phys(mapping->domain, handle)));
	pg_iommu_unmap(mapping, iova, len, (ulong)attrs);
	if (!dma_get_attr(DMA_ATTR_SKIP_FREE_IOVA, attrs))
		__free_iova(mapping, iova, len, attrs);
}

/**
 * arm_iommu_unmap_page
 * @dev: valid struct device pointer
 * @handle: DMA address of buffer
 * @size: size of buffer (same as passed to dma_map_page)
 * @dir: DMA transfer direction (same as passed to dma_map_page)
 *
 * IOMMU aware version of arm_dma_unmap_page()
 */
static void arm_iommu_unmap_page(struct device *dev, dma_addr_t handle,
		size_t size, enum dma_data_direction dir,
		struct dma_attrs *attrs)
{
	struct dma_iommu_mapping *mapping = dev->archdata.mapping;
	dma_addr_t iova = handle & PAGE_MASK;
	struct page *page = phys_to_page(iommu_iova_to_phys(mapping->domain, iova));
	int offset = handle & ~PAGE_MASK;
	int len = PAGE_ALIGN(size + offset);

	if (!iova)
		return;

	if (!dma_get_attr(DMA_ATTR_SKIP_CPU_SYNC, attrs))
		__dma_page_dev_to_cpu(page, offset, size, dir);

	trace_dmadebug_unmap_page(dev, handle, size,
		  phys_to_page(iommu_iova_to_phys(mapping->domain, handle)));
	pg_iommu_unmap(mapping, iova, len, (ulong)attrs);
	if (!dma_get_attr(DMA_ATTR_SKIP_FREE_IOVA, attrs))
		__free_iova(mapping, iova, len, attrs);
}

static void arm_iommu_sync_single_for_cpu(struct device *dev,
		dma_addr_t handle, size_t size, enum dma_data_direction dir)
{
	struct dma_iommu_mapping *mapping = dev->archdata.mapping;
	dma_addr_t iova = handle & PAGE_MASK;
	struct page *page = phys_to_page(iommu_iova_to_phys(mapping->domain, iova));
	unsigned int offset = handle & ~PAGE_MASK;

	if (!iova)
		return;

	if (WARN_ON(!pfn_valid(page_to_pfn(page))))
		return;

	__dma_page_dev_to_cpu(page, offset, size, dir);
}

static void arm_iommu_sync_single_for_device(struct device *dev,
		dma_addr_t handle, size_t size, enum dma_data_direction dir)
{
	struct dma_iommu_mapping *mapping = dev->archdata.mapping;
	dma_addr_t iova = handle & PAGE_MASK;
	struct page *page = phys_to_page(iommu_iova_to_phys(mapping->domain, iova));
	unsigned int offset = handle & ~PAGE_MASK;

	if (!iova)
		return;

	if (WARN_ON(!pfn_valid(page_to_pfn(page))))
		return;

	__dma_page_cpu_to_dev(page, offset, size, dir);
}

int arm_iommu_mapping_error(struct device *dev, dma_addr_t dev_addr)
{
	return dev_addr == DMA_ERROR_CODE;
}

static phys_addr_t arm_iommu_iova_to_phys(struct device *dev, dma_addr_t iova)
{
	struct dma_iommu_mapping *mapping = dev->archdata.mapping;

	return iommu_iova_to_phys(mapping->domain, iova);
}

struct dma_map_ops iommu_ops = {
	.alloc		= arm_iommu_alloc_attrs,
	.free		= arm_iommu_free_attrs,
	.mmap		= arm_iommu_mmap_attrs,
	.get_sgtable	= arm_iommu_get_sgtable,
	.mapping_error	= arm_iommu_mapping_error,

	.map_page		= arm_iommu_map_page,
	.map_page_at		= arm_iommu_map_page_at,
	.unmap_page		= arm_iommu_unmap_page,
	.sync_single_for_cpu	= arm_iommu_sync_single_for_cpu,
	.sync_single_for_device	= arm_iommu_sync_single_for_device,

	.map_sg			= arm_iommu_map_sg,
	.unmap_sg		= arm_iommu_unmap_sg,
	.sync_sg_for_cpu	= arm_iommu_sync_sg_for_cpu,
	.sync_sg_for_device	= arm_iommu_sync_sg_for_device,

	.iova_alloc		= arm_iommu_iova_alloc,
	.iova_alloc_at		= arm_iommu_iova_alloc_at,
	.iova_free		= arm_iommu_iova_free,
	.iova_get_free_total	= arm_iommu_iova_get_free_total,
	.iova_get_free_max	= arm_iommu_iova_get_free_max,

	.iova_to_phys		= arm_iommu_iova_to_phys,

	.dma_supported	= arm_dma_supported,
};

struct dma_map_ops iommu_coherent_ops = {
	.alloc		= arm_iommu_alloc_attrs,
	.free		= arm_iommu_free_attrs,
	.mmap		= arm_iommu_mmap_attrs,
	.get_sgtable	= arm_iommu_get_sgtable,
	.mapping_error	= arm_iommu_mapping_error,

	.map_page	= arm_coherent_iommu_map_page,
	.unmap_page	= arm_coherent_iommu_unmap_page,

	.map_sg		= arm_coherent_iommu_map_sg,
	.unmap_sg	= arm_coherent_iommu_unmap_sg,
	.dma_supported	= arm_dma_supported,
};

bool device_is_iommuable(struct device *dev)
{
	return (dev->archdata.dma_ops == &iommu_ops) ||
		(dev->archdata.dma_ops == &iommu_coherent_ops);
}
EXPORT_SYMBOL(device_is_iommuable);

static inline void __dummy_common(void)
{ WARN(1, "DMA API should be called after ->probe() is done.\n"); }

static void *__dummy_alloc_attrs(struct device *dev, size_t size,
				 dma_addr_t *dma_handle, gfp_t gfp,
				 struct dma_attrs *attrs)
{ __dummy_common(); return NULL; }

static void __dummy_free_attrs(struct device *dev, size_t size,
			       void *vaddr, dma_addr_t dma_handle,
			       struct dma_attrs *attrs)
{ __dummy_common(); }

static int __dummy_mmap_attrs(struct device *dev, struct vm_area_struct *vma,
			      void *cpu_addr, dma_addr_t dma_addr, size_t size,
			      struct dma_attrs *attrs)
{ __dummy_common(); return -ENXIO; }

static int __dummy_get_sgtable(struct device *dev, struct sg_table *sgt,
			       void *cpu_addr, dma_addr_t dma_addr,
			       size_t size, struct dma_attrs *attrs)
{ __dummy_common(); return -ENXIO; }

static dma_addr_t __dummy_map_page(struct device *dev, struct page *page,
				   unsigned long offset, size_t size,
				   enum dma_data_direction dir,
				   struct dma_attrs *attrs)
{ __dummy_common(); return DMA_ERROR_CODE; }

static dma_addr_t __dummy_map_page_at(struct device *dev, struct page *page,
				      dma_addr_t dma_handle,
				      unsigned long offset, size_t size,
				      enum dma_data_direction dir,
				      struct dma_attrs *attrs)
{ __dummy_common(); return DMA_ERROR_CODE; }

static void __dummy_unmap_page(struct device *dev, dma_addr_t dma_handle,
			       size_t size, enum dma_data_direction dir,
			       struct dma_attrs *attrs)
{ __dummy_common(); }

static int __dummy_map_sg(struct device *dev, struct scatterlist *sg,
			  int nents, enum dma_data_direction dir,
			  struct dma_attrs *attrs)
{ __dummy_common(); return 0; }

static void __dummy_unmap_sg(struct device *dev,
			     struct scatterlist *sg, int nents,
			     enum dma_data_direction dir,
			     struct dma_attrs *attrs)
{ __dummy_common(); }

static void __dummy_sync_single_for_cpu(struct device *dev,
					dma_addr_t dma_handle, size_t size,
					enum dma_data_direction dir)
{ __dummy_common(); }

static void __dummy_sync_single_for_device(struct device *dev,
					   dma_addr_t dma_handle, size_t size,
					   enum dma_data_direction dir)
{ __dummy_common(); }

static void __dummy_sync_sg_for_cpu(struct device *dev,
				    struct scatterlist *sg, int nents,
				    enum dma_data_direction dir)
{ __dummy_common(); }

static void __dummy_sync_sg_for_device(struct device *dev,
				       struct scatterlist *sg, int nents,
				       enum dma_data_direction dir)
{ __dummy_common(); }

static dma_addr_t __dummy_iova_alloc(struct device *dev, size_t size,
				     struct dma_attrs *attrs)
{ __dummy_common(); return DMA_ERROR_CODE; }

static dma_addr_t __dummy_iova_alloc_at(struct device *dev, dma_addr_t *dma_addr,
					size_t size, struct dma_attrs *attrs)
{ __dummy_common(); return DMA_ERROR_CODE; }

static void __dummy_iova_free(struct device *dev, dma_addr_t addr, size_t size,
			      struct dma_attrs *attrs)
{ __dummy_common(); }

static size_t __dummy_iova_get_free_total(struct device *dev)
{ __dummy_common(); return 0; }

static size_t __dummy_iova_get_free_max(struct device *dev)
{ __dummy_common(); return 0; }

static struct dma_map_ops __dummy_ops = {
	.alloc		= __dummy_alloc_attrs,
	.free		= __dummy_free_attrs,
	.mmap		= __dummy_mmap_attrs,
	.get_sgtable	= __dummy_get_sgtable,

	.map_page		= __dummy_map_page,
	.map_page_at		= __dummy_map_page_at,
	.unmap_page		= __dummy_unmap_page,
	.sync_single_for_cpu	= __dummy_sync_single_for_cpu,
	.sync_single_for_device	= __dummy_sync_single_for_device,

	.map_sg			= __dummy_map_sg,
	.unmap_sg		= __dummy_unmap_sg,
	.sync_sg_for_cpu	= __dummy_sync_sg_for_cpu,
	.sync_sg_for_device	= __dummy_sync_sg_for_device,

	.iova_alloc		= __dummy_iova_alloc,
	.iova_alloc_at		= __dummy_iova_alloc_at,
	.iova_free		= __dummy_iova_free,
	.iova_get_free_total	= __dummy_iova_get_free_total,
	.iova_get_free_max	= __dummy_iova_get_free_max,
};

void set_dummy_dma_ops(struct device *dev)
{
	set_dma_ops(dev, &__dummy_ops);
}


/* __alloc_iova_at() is broken with extensions enabled.
 * Disable the extensions till it is fixed.
 */
#define DISABLE_EXTENSIONS 1

/**
 * arm_iommu_create_mapping
 * @bus: pointer to the bus holding the client device (for IOMMU calls)
 * @base: start address of the valid IO address space
 * @size: maximum size of the valid IO address space
 *
 * Creates a mapping structure which holds information about used/unused
 * IO address ranges, which is required to perform memory allocation and
 * mapping with IOMMU aware functions.
 *
 * The client device need to be attached to the mapping with
 * arm_iommu_attach_device function.
 */
struct dma_iommu_mapping *
arm_iommu_create_mapping(struct bus_type *bus, dma_addr_t base, size_t size)
{
	unsigned int bits = size >> PAGE_SHIFT;
	unsigned int bitmap_size = BITS_TO_LONGS(bits) * sizeof(long);
	struct dma_iommu_mapping *mapping;
	int extensions = 1;
	int err = -ENOMEM;

	if (!bitmap_size)
		return ERR_PTR(-EINVAL);

	if (!DISABLE_EXTENSIONS && bitmap_size > PAGE_SIZE) {
		extensions = bitmap_size / PAGE_SIZE;
		bitmap_size = PAGE_SIZE;
	}

	mapping = kzalloc(sizeof(struct dma_iommu_mapping), GFP_KERNEL);
	if (!mapping)
		goto err;

	mapping->bitmap_size = bitmap_size;
	mapping->bitmaps = kzalloc(extensions * sizeof(unsigned long *),
				GFP_KERNEL);
	if (!mapping->bitmaps)
		goto err2;

	mapping->bitmaps[0] = kzalloc(bitmap_size, GFP_KERNEL);
	if (!mapping->bitmaps[0])
		goto err3;

	mapping->nr_bitmaps = 1;
	mapping->extensions = extensions;
	mapping->base = base;
	mapping->end = base + size;
	mapping->bits = BITS_PER_BYTE * bitmap_size;

	spin_lock_init(&mapping->lock);

	mapping->domain = iommu_domain_alloc(bus);
	if (!mapping->domain)
		goto err4;

	kref_init(&mapping->kref);

	iommu_mapping_list_add(mapping);
	return mapping;
err4:
	kfree(mapping->bitmaps[0]);
err3:
	kfree(mapping->bitmaps);
err2:
	kfree(mapping);
err:
	return ERR_PTR(err);
}

static void release_iommu_mapping(struct kref *kref)
{
	int i;
	struct dma_iommu_mapping *mapping =
		container_of(kref, struct dma_iommu_mapping, kref);

	iommu_mapping_list_del(mapping);
	iommu_domain_free(mapping->domain);
	for (i = 0; i < mapping->nr_bitmaps; i++)
		kfree(mapping->bitmaps[i]);
	kfree(mapping->bitmaps);
	kfree(mapping);
}

static int extend_iommu_mapping(struct dma_iommu_mapping *mapping)
{
	int next_bitmap;

	if (mapping->nr_bitmaps >= mapping->extensions)
		return -EINVAL;

	next_bitmap = mapping->nr_bitmaps;
	mapping->bitmaps[next_bitmap] = kzalloc(mapping->bitmap_size,
						GFP_ATOMIC);
	if (!mapping->bitmaps[next_bitmap])
		return -ENOMEM;

	mapping->nr_bitmaps++;

	return 0;
}

void arm_iommu_release_mapping(struct dma_iommu_mapping *mapping)
{
	if (mapping)
		kref_put(&mapping->kref, release_iommu_mapping);
}

/**
 * arm_iommu_attach_device
 * @dev: valid struct device pointer
 * @mapping: io address space mapping structure (returned from
 *	arm_iommu_create_mapping)
 *
 * Attaches specified io address space mapping to the provided device,
 * this replaces the dma operations (dma_map_ops pointer) with the
 * IOMMU aware version. More than one client might be attached to
 * the same io address space mapping.
 */
int arm_iommu_attach_device(struct device *dev,
			    struct dma_iommu_mapping *mapping)
{
	int err;
	struct dma_map_ops *org_ops;
	struct dma_iommu_mapping *org_map;

#ifdef CONFIG_DMA_API_DEBUG
	struct iommu_usage *device_ref;

	device_ref = kzalloc(sizeof(*device_ref), GFP_KERNEL);
	if (!device_ref)
		return -ENOMEM;

	device_ref->dev = dev;
	list_add(&device_ref->recordlist, &iommu_rlist_head);
#endif

	org_ops = get_dma_ops(dev);
	set_dma_ops(dev, &iommu_ops);

	org_map = dev->archdata.mapping;
	dev->archdata.mapping = mapping;

	err = iommu_attach_device(mapping->domain, dev);
	if (err) {
		set_dma_ops(dev, org_ops);
		dev->archdata.mapping = org_map;
#ifdef CONFIG_DMA_API_DEBUG
		list_del(&device_ref->recordlist);
		kfree(device_ref);
#endif
		return err;
	}

	kref_get(&mapping->kref);

	pr_debug("Attached IOMMU controller to %s device.\n", dev_name(dev));
	return 0;
}

/**
 * arm_iommu_detach_device
 * @dev: valid struct device pointer
 *
 * Detaches the provided device from a previously attached map.
 * This voids the dma operations (dma_map_ops pointer)
 */
void arm_iommu_detach_device(struct device *dev)
{
	struct dma_iommu_mapping *mapping;
#ifdef CONFIG_DMA_API_DEBUG
	struct iommu_usage *device_ref, *tmp;
#endif

	mapping = to_dma_iommu_mapping(dev);
	if (!mapping) {
		dev_warn(dev, "Not attached\n");
		return;
	}

#ifdef CONFIG_DMA_API_DEBUG
	list_for_each_entry_safe(device_ref, tmp, &iommu_rlist_head,
		recordlist) {
		if (dev == device_ref->dev) {
			list_del(&device_ref->recordlist);
			kfree(device_ref);
			break;
		}
	}
#endif

	iommu_detach_device(mapping->domain, dev);
	kref_put(&mapping->kref, release_iommu_mapping);
	mapping = NULL;
	set_dma_ops(dev, NULL);

	pr_debug("Detached IOMMU controller from %s device.\n", dev_name(dev));
}

#endif

#ifdef CONFIG_DMA_API_DEBUG
static void
add_value(struct dma_iommu_mapping *device_ref, size_t size, const int type)
{
	switch (type) {
	case ALLOC_OR_FREE:
		atomic64_add(size, &device_ref->alloc_size);
		break;
	case ATOMIC_ALLOC_OR_FREE:
		atomic64_add(size, &device_ref->atomic_alloc_size);
		break;
	case MAP_OR_UNMAP:
		atomic64_add(size, &device_ref->map_size);
		break;
	case CPU_MAP_OR_UNMAP:
		atomic64_add(size, &device_ref->cpu_map_size);
		break;
	default:
		pr_info("Invalid argument in function %s\n", __func__);
	}
}

static void
sub_value(struct dma_iommu_mapping *device_ref, size_t size, const int type)
{
	switch (type) {
	case ALLOC_OR_FREE:
		atomic64_sub(size, &device_ref->alloc_size);
		break;
	case ATOMIC_ALLOC_OR_FREE:
		atomic64_sub(size, &device_ref->atomic_alloc_size);
		break;
	case MAP_OR_UNMAP:
		atomic64_sub(size, &device_ref->map_size);
		break;
	case CPU_MAP_OR_UNMAP:
		atomic64_sub(size, &device_ref->cpu_map_size);
		break;
	default:
		pr_info("Invalid argument in function %s\n", __func__);
	}
}

static size_t
dmastats_alloc_or_map(struct device *dev, size_t size, const int type)
{
	struct dma_iommu_mapping *mapping;

	mapping = to_dma_iommu_mapping(dev);
	if (mapping)
		add_value(mapping, size, type);
	else
		atomic64_add(size, &dev_is_null->alloc_size);
	return 0;
}

static size_t dmastats_free_or_unmap(struct device *dev, size_t size,
	const int type)
{
	struct dma_iommu_mapping *mapping;

	mapping = to_dma_iommu_mapping(dev);
	if (mapping)
		sub_value(mapping, size, type);
	else
		atomic64_sub(size, &dev_is_null->alloc_size);
	return 0;
}

static int dmastats_debug_show(struct seq_file *s, void *data)
{
	struct iommu_usage *device_ref = NULL, *tmp;
	size_t alloc_size = 0, map_size = 0;
	size_t atomic_alloc_size = 0, cpu_map_size = 0;
	struct dma_iommu_mapping *mapping;

	seq_printf(s, "%-24s %18s %18s %18s %18s\n", "DEV_NAME", "ALLOCATED",
			"ATOMIC_ALLOCATED", "MAPPED", "CPU_MAPPED");
	list_for_each_entry_safe(device_ref, tmp, &iommu_rlist_head,
		recordlist) {
		mapping = to_dma_iommu_mapping(device_ref->dev);
		alloc_size += atomic64_read(&mapping->alloc_size);
		map_size += atomic64_read(&mapping->map_size);
		atomic_alloc_size += atomic64_read(&mapping->atomic_alloc_size);
		cpu_map_size += atomic64_read(&mapping->cpu_map_size);

		seq_printf(s, "%-24s %18ld %18ld %18ld %18ld\n",
			dev_name(device_ref->dev),
			atomic64_read(&mapping->alloc_size),
			atomic64_read(&mapping->atomic_alloc_size),
			atomic64_read(&mapping->map_size),
			atomic64_read(&mapping->cpu_map_size));
	}

	alloc_size += atomic64_read(&dev_is_null->alloc_size);
	map_size += atomic64_read(&dev_is_null->map_size);
	atomic_alloc_size += atomic64_read(&dev_is_null->atomic_alloc_size);
	cpu_map_size += atomic64_read(&dev_is_null->cpu_map_size);

	seq_printf(s, "%-24s %18ld %18ld %18ld %18ld\n",
		dev_is_null->dev_name,
		atomic64_read(&dev_is_null->alloc_size),
		atomic64_read(&dev_is_null->atomic_alloc_size),
		atomic64_read(&dev_is_null->map_size),
		atomic64_read(&dev_is_null->cpu_map_size));

	seq_printf(s, "\n%-24s %18zu %18zu %18zu %18zu\n", "Total",
		alloc_size, atomic_alloc_size, map_size, cpu_map_size);
	return 0;
}

static int dmastats_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, dmastats_debug_show, NULL);
}

static const struct file_operations debug_dmastats_fops = {
	.open		= dmastats_debug_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init dmastats_debug_init(void)
{
	struct dentry *rootdir, *ret;

	rootdir = debugfs_create_dir("dma", NULL);
	if (!rootdir) {
		pr_err("Failed to create dma directory!\n");
		return -ENOMEM;
	}

	ret = debugfs_create_file("usage", S_IRUGO, rootdir, NULL,
		&debug_dmastats_fops);
	if (!ret) {
		pr_err("Failed to create usage debug file!\n");
		return -ENOMEM;
	}

	dev_is_null = kzalloc(sizeof(*dev_is_null), GFP_KERNEL);
	if (!dev_is_null)
		return -ENOMEM;

	dev_is_null->dev_name = NULL_DEV;
	return 0;
}

static void __exit dmastats_debug_exit(void)
{
	struct iommu_usage *device_ref = NULL, *tmp;

	list_for_each_entry_safe(device_ref, tmp, &iommu_rlist_head,
		recordlist) {
		list_del(&device_ref->recordlist);
		kfree(device_ref);
	}

	kfree(dev_is_null);
}

late_initcall(dmastats_debug_init);
module_exit(dmastats_debug_exit);
#endif
