// SPDX-License_Identifier: GPL-2.0
// Copyright (C) 2013 - 2018 Intel Corporation

#include <asm/cacheflush.h>

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/gfp.h>
#include <linux/highmem.h>
#include <linux/iommu.h>
#include <linux/iova.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/vmalloc.h>

#include "ipu-dma.h"
#include "ipu-mmu.h"

/* Begin of things adapted from arch/arm/mm/dma-mapping.c */
static void __dma_clear_buffer(struct page *page, size_t size,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
			       struct dma_attrs *attrs
#else
			       unsigned long attrs
#endif
				)
{
	/*
	 * Ensure that the allocated pages are zeroed, and that any data
	 * lurking in the kernel direct-mapped region is invalidated.
	 */
	if (PageHighMem(page)) {
		while (size > 0) {
			void *ptr = kmap_atomic(page);

			memset(ptr, 0, PAGE_SIZE);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
			if (!dma_get_attr(DMA_ATTR_SKIP_CPU_SYNC, attrs))
#else
			if ((attrs & DMA_ATTR_SKIP_CPU_SYNC) == 0)
#endif
				clflush_cache_range(ptr, PAGE_SIZE);
			kunmap_atomic(ptr);
			page++;
			size -= PAGE_SIZE;
		}
	} else {
		void *ptr = page_address(page);

		memset(ptr, 0, size);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
		if (!dma_get_attr(DMA_ATTR_SKIP_CPU_SYNC, attrs))
#else
		if ((attrs & DMA_ATTR_SKIP_CPU_SYNC) == 0)
#endif
			clflush_cache_range(ptr, size);
	}
}

static struct page **__iommu_alloc_buffer(struct device *dev, size_t size,
					  gfp_t gfp,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
					  struct dma_attrs *attrs
#else
					  unsigned long attrs
#endif
					)
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

		__dma_clear_buffer(pages[i], PAGE_SIZE << order, attrs);
		i += 1 << order;
		count -= 1 << order;
	}

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
			       size_t size,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
			       struct dma_attrs *attrs
#else
			       unsigned long attrs
#endif
				)
{
	int count = size >> PAGE_SHIFT;
	int array_size = count * sizeof(struct page *);
	int i;

	for (i = 0; i < count; i++) {
		if (pages[i]) {
			__dma_clear_buffer(pages[i], PAGE_SIZE, attrs);
			__free_pages(pages[i], 0);
		}
	}

	if (array_size <= PAGE_SIZE)
		kfree(pages);
	else
		vfree(pages);
	return 0;
}

/* End of things adapted from arch/arm/mm/dma-mapping.c */

static void ipu_dma_sync_single_for_cpu(struct device *dev,
					dma_addr_t dma_handle,
					size_t size,
					enum dma_data_direction dir)
{
	struct device *aiommu = to_ipu_bus_device(dev)->iommu;
	struct ipu_mmu *mmu = dev_get_drvdata(aiommu);
	unsigned long pa = iommu_iova_to_phys(mmu->dmap->domain, dma_handle);

	clflush_cache_range(phys_to_virt(pa), size);
}

static void ipu_dma_sync_sg_for_cpu(struct device *dev,
				    struct scatterlist *sglist,
				    int nents, enum dma_data_direction dir)
{
	struct scatterlist *sg;
	int i;

	for_each_sg(sglist, sg, nents, i)
		clflush_cache_range(page_to_virt(sg_page(sg)), sg->length);
}

static void *ipu_dma_alloc(struct device *dev, size_t size,
			   dma_addr_t *dma_handle, gfp_t gfp,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
			   struct dma_attrs *attrs
#else
			   unsigned long attrs
#endif
			)
{
	struct device *aiommu = to_ipu_bus_device(dev)->iommu;
	struct ipu_mmu *mmu = dev_get_drvdata(aiommu);
	struct page **pages;
	struct iova *iova;
	struct vm_struct *area;
	int i;
	int rval;

	size = PAGE_ALIGN(size);

	iova = alloc_iova(&mmu->dmap->iovad, size >> PAGE_SHIFT,
			  dma_get_mask(dev) >> PAGE_SHIFT, 0);
	if (!iova)
		return NULL;

	pages = __iommu_alloc_buffer(dev, size, gfp, attrs);
	if (!pages)
		goto out_free_iova;

	for (i = 0; iova->pfn_lo + i <= iova->pfn_hi; i++) {
		rval = iommu_map(mmu->dmap->domain,
				 (iova->pfn_lo + i) << PAGE_SHIFT,
				 page_to_phys(pages[i]), PAGE_SIZE, 0);
		if (rval)
			goto out_unmap;
	}

	area = __get_vm_area(size, 0, VMALLOC_START, VMALLOC_END);
	if (!area)
		goto out_unmap;

	area->pages = pages;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)
	if (map_vm_area(area, PAGE_KERNEL, &pages))
#else
	if (map_vm_area(area, PAGE_KERNEL, pages))
#endif
		goto out_vunmap;

	*dma_handle = iova->pfn_lo << PAGE_SHIFT;

	mmu->tlb_invalidate(mmu);

	return area->addr;

out_vunmap:
	vunmap(area->addr);

out_unmap:
	for (i--; i >= 0; i--) {
		iommu_unmap(mmu->dmap->domain, (iova->pfn_lo + i) << PAGE_SHIFT,
			    PAGE_SIZE);
	}
	__iommu_free_buffer(dev, pages, size, attrs);

out_free_iova:
	__free_iova(&mmu->dmap->iovad, iova);

	return NULL;
}

static void ipu_dma_free(struct device *dev, size_t size, void *vaddr,
			 dma_addr_t dma_handle,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
			 struct dma_attrs *attrs
#else
			 unsigned long attrs
#endif
			)
{
	struct device *aiommu = to_ipu_bus_device(dev)->iommu;
	struct ipu_mmu *mmu = dev_get_drvdata(aiommu);
	struct vm_struct *area = find_vm_area(vaddr);
	struct page **pages;
	struct iova *iova = find_iova(&mmu->dmap->iovad,
				      dma_handle >> PAGE_SHIFT);

	if (WARN_ON(!area))
		return;

	if (WARN_ON(!area->pages))
		return;

	WARN_ON(!iova);

	size = PAGE_ALIGN(size);

	pages = area->pages;

	vunmap(vaddr);

	iommu_unmap(mmu->dmap->domain, iova->pfn_lo << PAGE_SHIFT,
		    (iova->pfn_hi - iova->pfn_lo + 1) << PAGE_SHIFT);

	__iommu_free_buffer(dev, pages, size, attrs);

	__free_iova(&mmu->dmap->iovad, iova);

	mmu->tlb_invalidate(mmu);
}

static int ipu_dma_mmap(struct device *dev, struct vm_area_struct *vma,
			void *addr, dma_addr_t iova, size_t size,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
			struct dma_attrs *attrs
#else
			unsigned long attrs
#endif
			)
{
	struct vm_struct *area = find_vm_area(addr);
	size_t count = PAGE_ALIGN(size) >> PAGE_SHIFT;
	size_t i;

	if (!area)
		return -EFAULT;

	if (vma->vm_start & ~PAGE_MASK)
		return -EINVAL;

	if (size > area->size)
		return -EFAULT;

	for (i = 0; i < count; i++)
		vm_insert_page(vma, vma->vm_start + (i << PAGE_SHIFT),
			       area->pages[i]);

	return 0;
}

static void ipu_dma_unmap_sg(struct device *dev,
			     struct scatterlist *sglist,
			     int nents, enum dma_data_direction dir,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
			     struct dma_attrs *attrs
#else
			     unsigned long attrs
#endif
			)
{
	struct device *aiommu = to_ipu_bus_device(dev)->iommu;
	struct ipu_mmu *mmu = dev_get_drvdata(aiommu);
	struct iova *iova = find_iova(&mmu->dmap->iovad,
				      sg_dma_address(sglist) >> PAGE_SHIFT);

	if (!nents)
		return;

	WARN_ON(!iova);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
	if (!dma_get_attr(DMA_ATTR_SKIP_CPU_SYNC, attrs))
#else
	if ((attrs & DMA_ATTR_SKIP_CPU_SYNC) == 0)
#endif
		ipu_dma_sync_sg_for_cpu(dev, sglist, nents, DMA_BIDIRECTIONAL);

	iommu_unmap(mmu->dmap->domain, iova->pfn_lo << PAGE_SHIFT,
		    (iova->pfn_hi - iova->pfn_lo + 1) << PAGE_SHIFT);

	mmu->tlb_invalidate(mmu);

	__free_iova(&mmu->dmap->iovad, iova);
}

static int ipu_dma_map_sg(struct device *dev, struct scatterlist *sglist,
			  int nents, enum dma_data_direction dir,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
			  struct dma_attrs *attrs
#else
			  unsigned long attrs
#endif
			)
{
	struct device *aiommu = to_ipu_bus_device(dev)->iommu;
	struct ipu_mmu *mmu = dev_get_drvdata(aiommu);
	struct scatterlist *sg;
	struct iova *iova;
	size_t size = 0;
	u32 iova_addr;
	int i;

	for_each_sg(sglist, sg, nents, i)
		size += PAGE_ALIGN(sg->length) >> PAGE_SHIFT;

	dev_dbg(dev, "dmamap: mapping sg %d entries, %zu pages\n", nents, size);

	iova = alloc_iova(&mmu->dmap->iovad, size,
			  dma_get_mask(dev) >> PAGE_SHIFT, 0);
	if (!iova)
		return 0;

	dev_dbg(dev, "dmamap: iova low pfn %lu, high pfn %lu\n", iova->pfn_lo,
		iova->pfn_hi);

	iova_addr = iova->pfn_lo;

	for_each_sg(sglist, sg, nents, i) {
		int rval;

		dev_dbg(dev, "mapping entry %d: iova 0x%8.8x,phy 0x%16.16llx\n",
			i, iova_addr << PAGE_SHIFT,
			(unsigned long long)page_to_phys(sg_page(sg)));
		rval = iommu_map(mmu->dmap->domain, iova_addr << PAGE_SHIFT,
				 page_to_phys(sg_page(sg)),
				 PAGE_ALIGN(sg->length), 0);
		if (rval)
			goto out_fail;
		sg_dma_address(sg) = iova_addr << PAGE_SHIFT;
#ifdef CONFIG_NEED_SG_DMA_LENGTH
		sg_dma_len(sg) = sg->length;
#endif /* CONFIG_NEED_SG_DMA_LENGTH */

		iova_addr += PAGE_ALIGN(sg->length) >> PAGE_SHIFT;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
	if (!dma_get_attr(DMA_ATTR_SKIP_CPU_SYNC, attrs))
#else
	if ((attrs & DMA_ATTR_SKIP_CPU_SYNC) == 0)
#endif
		ipu_dma_sync_sg_for_cpu(dev, sglist, nents, DMA_BIDIRECTIONAL);

	mmu->tlb_invalidate(mmu);

	return nents;

out_fail:
	ipu_dma_unmap_sg(dev, sglist, i, dir, attrs);

	return 0;
}

/*
 * Create scatter-list for the already allocated DMA buffer
 */
static int ipu_dma_get_sgtable(struct device *dev, struct sg_table *sgt,
			       void *cpu_addr, dma_addr_t handle, size_t size,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
			       struct dma_attrs *attrs
#else
			       unsigned long attrs
#endif
				)
{
	struct vm_struct *area = find_vm_area(cpu_addr);
	int n_pages;
	int ret = 0;

	if (WARN_ON(!area->pages))
		return -ENOMEM;

	n_pages = PAGE_ALIGN(size) >> PAGE_SHIFT;

	ret = sg_alloc_table_from_pages(sgt, area->pages, n_pages, 0, size,
					GFP_KERNEL);
	if (ret)
		dev_dbg(dev, "IPU get sgt table fail\n");

	return ret;
}

const struct dma_map_ops ipu_dma_ops = {
	.alloc = ipu_dma_alloc,
	.free = ipu_dma_free,
	.mmap = ipu_dma_mmap,
	.map_sg = ipu_dma_map_sg,
	.unmap_sg = ipu_dma_unmap_sg,
	.sync_single_for_cpu = ipu_dma_sync_single_for_cpu,
	.sync_single_for_device = ipu_dma_sync_single_for_cpu,
	.sync_sg_for_cpu = ipu_dma_sync_sg_for_cpu,
	.sync_sg_for_device = ipu_dma_sync_sg_for_cpu,
	.get_sgtable = ipu_dma_get_sgtable,
};
EXPORT_SYMBOL_GPL(ipu_dma_ops);
