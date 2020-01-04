/*
 *  linux/arch/arm/mm/dma-mapping.c
 *
 *  Copyright (C) 2000-2004 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  DMA uncached mapping support.
 */
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/memblock.h>

#include <asm/memory.h>
#include <asm/highmem.h>
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <asm/sizes.h>
#include <asm/mach/map.h>

#include "mm.h"

static u64 get_coherent_dma_mask(struct device *dev)
{
	u64 mask = ISA_DMA_THRESHOLD;

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

		if ((~mask) & ISA_DMA_THRESHOLD) {
			dev_warn(dev, "coherent DMA mask %#llx is smaller "
				 "than system GFP_DMA mask %#llx\n",
				 mask, (unsigned long long)ISA_DMA_THRESHOLD);
			return 0;
		}
	}

	return mask;
}

/*
 * Allocate a DMA buffer for 'dev' of size 'size' using the
 * specified gfp mask.  Note that 'size' must be page aligned.
 */
static struct page *__dma_alloc_buffer(struct device *dev, size_t size, gfp_t gfp)
{
	unsigned long order = get_order(size);
	struct page *page, *p, *e;
	void *ptr;
	u64 mask = get_coherent_dma_mask(dev);

#ifdef CONFIG_DMA_API_DEBUG
	u64 limit = (mask + 1) & ~mask;
	if (limit && size >= limit) {
		dev_warn(dev, "coherent allocation too big (requested %#x mask %#llx)\n",
			size, mask);
		return NULL;
	}
#endif

	if (!mask)
		return NULL;

	if (mask < 0xffffffffULL)
		gfp |= GFP_DMA;

	page = alloc_pages(gfp, order);
	if (!page)
		return NULL;

	/*
	 * Now split the huge page and free the excess pages
	 */
	split_page(page, order);
	for (p = page + (size >> PAGE_SHIFT), e = page + (1 << order); p < e; p++)
		__free_page(p);

	/*
	 * Ensure that the allocated pages are zeroed, and that any data
	 * lurking in the kernel direct-mapped region is invalidated.
	 */
	ptr = page_address(page);
	memset(ptr, 0, size);
	dmac_flush_range(ptr, ptr + size);
	outer_flush_range(__pa(ptr), __pa(ptr) + size);

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
/* Sanity check sizes */
#if CONSISTENT_DMA_SIZE % SECTION_SIZE
#error "CONSISTENT_DMA_SIZE must be a multiple of the section size"
#endif
#if CONSISTENT_WC_SIZE % SECTION_SIZE
#error "CONSISTENT_WC_SIZE must be a multiple of the section size"
#endif
#if ((CONSISTENT_DMA_SIZE + CONSISTENT_WC_SIZE) % SZ_2M)
#error "Sum of CONSISTENT_DMA_SIZE and CONSISTENT_WC_SIZE must be \
	multiple of 2MiB"
#endif

#include "vmregion.h"

struct dma_coherent_area {
	struct arm_vmregion_head vm;
	unsigned long pfn;
	unsigned long pfn_end;
	unsigned int type;
	const char *name;
};

static struct dma_coherent_area coherent_wc_head = {
	.vm = {
		.vm_start	= CONSISTENT_WC_BASE,
		.vm_end		= CONSISTENT_WC_END,
	},
	.pfn = 0,
	.pfn_end = 0,
	.type = MT_WC_COHERENT,
	.name = "WC ",
};

static struct dma_coherent_area coherent_dma_head = {
	.vm = {
		.vm_start	= CONSISTENT_BASE,
		.vm_end		= CONSISTENT_END,
	},
	.pfn = 0,
	.pfn_end = 0,
	.type = MT_DMA_COHERENT,
	.name = "DMA coherent ",
};


static struct dma_coherent_area *coherent_areas[2] __initdata = {
	&coherent_wc_head,
	&coherent_dma_head
};

static struct dma_coherent_area *coherent_map[2];
#define coherent_wc_area coherent_map[0]
#define coherent_dma_area coherent_map[1]

void dma_coherent_reserve(void)
{

	phys_addr_t base;
	unsigned long size;
	int can_share, i;

	if (arch_is_coherent())
		return;

#ifdef CONFIG_ARM_DMA_MEM_BUFFERABLE
	/* ARMv6: only when DMA_MEM_BUFFERABLE is enabled */
	can_share = cpu_architecture() >= CPU_ARCH_ARMv6;
#else
	/* ARMv7+: WC and DMA areas have the same properties, so can share */
	can_share = cpu_architecture() >= CPU_ARCH_ARMv7;
#endif
	if (can_share) {
		coherent_wc_head.name = "DMA coherent/WC ";
		coherent_wc_head.vm.vm_end = coherent_dma_head.vm.vm_end;
		coherent_dma_head.vm.vm_start = coherent_dma_head.vm.vm_end;
		coherent_dma_area = coherent_wc_area = &coherent_wc_head;
	} else {
		memcpy(coherent_map, coherent_areas, sizeof(coherent_map));
	}

	for (i = 0; i < ARRAY_SIZE(coherent_areas); i++) {

		struct dma_coherent_area *area = coherent_areas[i];

		size = area->vm.vm_end - area->vm.vm_start;
		if (!size)
			continue;

		spin_lock_init(&area->vm.vm_lock);
		INIT_LIST_HEAD(&area->vm.vm_list);

		base = memblock_end_of_DRAM() - size;
		memblock_free(base, size);
		memblock_remove(base, size);

		area->pfn = __phys_to_pfn(base);
		area->pfn_end = __phys_to_pfn(base+size);

		pr_info("DMA: %luMiB %smemory allocated at 0x%08llx phys\n",
			size / 1048576, area->name, (unsigned long long)base);
	}
}

void __init dma_coherent_mapping(void)
{
	struct map_desc map[ARRAY_SIZE(coherent_areas)];
	int nr;
	for (nr = 0; nr < ARRAY_SIZE(map); nr++) {
		struct dma_coherent_area *area = coherent_areas[nr];

		map[nr].pfn = area->pfn;
		map[nr].virtual = area->vm.vm_start;
		map[nr].length = area->vm.vm_end - area->vm.vm_start;
		map[nr].type = area->type;
		if (map[nr].length == 0)
			break;
	}

	iotable_init(map, nr);
}

static void *dma_alloc_area(size_t size, unsigned long *pfn, gfp_t gfp,
	struct dma_coherent_area *area)

{
	struct arm_vmregion *c;
	size_t align;
	int bit;

	/*
	 * Align the virtual region allocation - maximum alignment is
	 * a section size, minimum is a page size.  This helps reduce
	 * fragmentation of the DMA space, and also prevents allocations
	 * smaller than a section from crossing a section boundary.
	 */
	bit = fls(size - 1) + 1;
	if (bit > SECTION_SHIFT)
		bit = SECTION_SHIFT;
	align = 1 << bit;

	/*
	 * Allocate a virtual address in the consistent mapping region.
	 */
	c = arm_vmregion_alloc(&area->vm, align, size,
			    gfp & ~(__GFP_DMA | __GFP_HIGHMEM));
	if (!c)
		return NULL;

	memset((void *)c->vm_start, 0, size);
	*pfn = area->pfn + ((c->vm_start - area->vm.vm_start) >> PAGE_SHIFT);
	c->vm_pages = pfn_to_page(*pfn);

	return (void *)c->vm_start;
}

static void dma_free_area(void *cpu_addr, size_t size,
	struct dma_coherent_area *area)
{
	struct arm_vmregion *c;

	c = arm_vmregion_find_remove(&area->vm, (unsigned long)cpu_addr);
	if (!c) {
		printk(KERN_ERR "%s: trying to free invalid coherent area: %p\n",
		       __func__, cpu_addr);
		dump_stack();
		return;
	}

	if ((c->vm_end - c->vm_start) != size) {
		printk(KERN_ERR "%s: freeing wrong coherent size (%ld != %d)\n",
		       __func__, c->vm_end - c->vm_start, size);
		dump_stack();
		size = c->vm_end - c->vm_start;
	}

	arm_vmregion_free(&area->vm, c);
}

#define nommu() (0)

#else	/* !CONFIG_MMU */
#define dma_alloc_area(size, pfn, gfp, area)	({ *(pfn) = 0; NULL })
#define dma_free_area(addr, size, area)		do { } while (0)

#define nommu()	(1)
#define coherent_wc_area NULL
#define coherent_dma_area NULL

void dma_coherent_reserve(void)
{
}

#endif	/* CONFIG_MMU */

static void *
__dma_alloc(struct device *dev, size_t size, dma_addr_t *handle, gfp_t gfp,
	    struct dma_coherent_area *area)
{

	unsigned long pfn;
	void *ret;

	*handle = ~0;
	size = PAGE_ALIGN(size);

	if (arch_is_coherent() || nommu()) {
		struct page *page = __dma_alloc_buffer(dev, size, gfp);
		if (!page)
			return NULL;
		pfn = page_to_pfn(page);
		ret = page_address(page);
	} else {
		ret = dma_alloc_area(size, &pfn, gfp, area);
	}

	if (ret)
		*handle = page_to_dma(dev, pfn_to_page(pfn));

	return ret;
}

static void __dma_free(struct device *dev, size_t size, void *cpu_addr,
	dma_addr_t handle, struct dma_coherent_area *area)
{
	size = PAGE_ALIGN(size);

	if (arch_is_coherent() || nommu())
		__dma_free_buffer(dma_to_page(dev, handle), size);
	else
		dma_free_area(cpu_addr, size, area);
}

/*
 * Allocate DMA-coherent memory space and return both the kernel remapped
 * virtual and bus address for that space.
 */
void *
dma_alloc_coherent(struct device *dev, size_t size, dma_addr_t *handle, gfp_t gfp)
{
	void *memory;

	if (dma_alloc_from_coherent(dev, size, handle, &memory))
		return memory;

	return __dma_alloc(dev, size, handle, gfp, coherent_dma_area);
}
EXPORT_SYMBOL(dma_alloc_coherent);

/*
 * Allocate a writecombining region, in much the same way as
 * dma_alloc_coherent above.
 */
void *
dma_alloc_writecombine(struct device *dev, size_t size, dma_addr_t *handle, gfp_t gfp)
{
	return __dma_alloc(dev, size, handle, gfp, coherent_wc_area);
}
EXPORT_SYMBOL(dma_alloc_writecombine);

static int dma_mmap(struct device *dev, struct vm_area_struct *vma,
		    void *cpu_addr, dma_addr_t dma_addr, size_t size,
		    struct dma_coherent_area *area)
{
	int ret = -ENXIO;
#ifdef CONFIG_MMU
	unsigned long user_size, kern_size;
	struct arm_vmregion *c;

	user_size = (vma->vm_end - vma->vm_start) >> PAGE_SHIFT;

	c = arm_vmregion_find(&area->vm, (unsigned long)cpu_addr);
	if (c) {
		unsigned long off = vma->vm_pgoff;

		kern_size = (c->vm_end - c->vm_start) >> PAGE_SHIFT;

		if (off < kern_size &&
		    user_size <= (kern_size - off)) {
			ret = remap_pfn_range(vma, vma->vm_start,
					      page_to_pfn(c->vm_pages) + off,
					      user_size << PAGE_SHIFT,
					      vma->vm_page_prot);
		}
	}
#endif	/* CONFIG_MMU */

	return ret;
}

int dma_mmap_coherent(struct device *dev, struct vm_area_struct *vma,
		      void *cpu_addr, dma_addr_t dma_addr, size_t size)
{
	vma->vm_page_prot = pgprot_dmacoherent(vma->vm_page_prot);
	return dma_mmap(dev, vma, cpu_addr, dma_addr, size, coherent_dma_area);
}
EXPORT_SYMBOL(dma_mmap_coherent);

int dma_mmap_writecombine(struct device *dev, struct vm_area_struct *vma,
			  void *cpu_addr, dma_addr_t dma_addr, size_t size)
{
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	return dma_mmap(dev, vma, cpu_addr, dma_addr, size, coherent_wc_area);
}
EXPORT_SYMBOL(dma_mmap_writecombine);

/*
 * free a page as defined by the above mapping.
 * Must not be called with IRQs disabled.
 */
void dma_free_coherent(struct device *dev, size_t size, void *cpu_addr, dma_addr_t handle)
{
	WARN_ON(irqs_disabled());

	if (dma_release_from_coherent(dev, get_order(size), cpu_addr))
		return;
	__dma_free(dev, size, cpu_addr, handle, coherent_dma_area);
}
EXPORT_SYMBOL(dma_free_coherent);

void dma_free_writecombine(struct device *dev, size_t size, void *cpu_addr,
	dma_addr_t handle)
{
	WARN_ON(irqs_disabled());

	__dma_free(dev, size, cpu_addr, handle, coherent_wc_area);
}
EXPORT_SYMBOL(dma_free_writecombine);

#define is_coherent_pfn(x) ((((x) >= coherent_wc_head.pfn) &&	\
			((x) < coherent_wc_head.pfn_end)) ||	\
			(((x) >= coherent_dma_head.pfn) &&	\
			((x) < coherent_dma_head.pfn_end)))
/*
 * Make an area consistent for devices.
 * Note: Drivers should NOT use this function directly, as it will break
 * platforms with CONFIG_DMABOUNCE.
 * Use the driver DMA support - see dma-mapping.h (dma_sync_*)
 */
void ___dma_single_cpu_to_dev(struct device *dev, const void *kaddr,
	size_t size, enum dma_data_direction dir)
{
	unsigned long paddr;
	unsigned long kaddr_pfn;

	kaddr_pfn = dma_to_pfn(dev, virt_to_dma(dev, (void *)kaddr));

	if (is_coherent_pfn(kaddr_pfn))
		return;

	BUG_ON(!virt_addr_valid(kaddr) || !virt_addr_valid(kaddr + size - 1));

	dmac_map_area(kaddr, size, dir);

	paddr = __pa(kaddr);
	if (dir == DMA_FROM_DEVICE) {
		outer_inv_range(paddr, paddr + size);
	} else {
		outer_clean_range(paddr, paddr + size);
	}
	/* FIXME: non-speculating: flush on bidirectional mappings? */
}
EXPORT_SYMBOL(___dma_single_cpu_to_dev);

void ___dma_single_dev_to_cpu(struct device *dev, const void *kaddr,
	size_t size, enum dma_data_direction dir)
{
	unsigned long kaddr_pfn;

	kaddr_pfn = dma_to_pfn(dev, virt_to_dma(dev, (void *)kaddr));

	if (is_coherent_pfn(kaddr_pfn))
		return;

	BUG_ON(!virt_addr_valid(kaddr) || !virt_addr_valid(kaddr + size - 1));

	/* FIXME: non-speculating: not required */
	/* don't bother invalidating if DMA to device */
	if (dir != DMA_TO_DEVICE) {
		unsigned long paddr = __pa(kaddr);
		outer_inv_range(paddr, paddr + size);
	}

	dmac_unmap_area(kaddr, size, dir);
}
EXPORT_SYMBOL(___dma_single_dev_to_cpu);

static void dma_cache_maint_page(struct page *page, unsigned long offset,
	size_t size, enum dma_data_direction dir,
	void (*op)(const void *, size_t, int))
{
	/*
	 * A single sg entry may refer to multiple physically contiguous
	 * pages.  But we still need to process highmem pages individually.
	 * If highmem is not configured then the bulk of this loop gets
	 * optimized out.
	 */
	size_t left = size;
	do {
		size_t len = left;
		void *vaddr;

		if (PageHighMem(page)) {
			if (len + offset > PAGE_SIZE) {
				if (offset >= PAGE_SIZE) {
					page += offset / PAGE_SIZE;
					offset %= PAGE_SIZE;
				}
				len = PAGE_SIZE - offset;
			}
			vaddr = kmap_high_get(page);
			if (vaddr) {
				vaddr += offset;
				op(vaddr, len, dir);
				kunmap_high(page);
			} else if (cache_is_vipt()) {
				pte_t saved_pte;
				vaddr = kmap_high_l1_vipt(page, &saved_pte);
				op(vaddr + offset, len, dir);
				kunmap_high_l1_vipt(page, saved_pte);
			}
		} else {
			vaddr = page_address(page) + offset;
			op(vaddr, len, dir);
		}
		offset = 0;
		page++;
		left -= len;
	} while (left);
}

void ___dma_page_cpu_to_dev(struct page *page, unsigned long off,
	size_t size, enum dma_data_direction dir)
{
	unsigned long paddr;

	dma_cache_maint_page(page, off, size, dir, dmac_map_area);

	paddr = page_to_phys(page) + off;
	if (dir == DMA_FROM_DEVICE) {
		outer_inv_range(paddr, paddr + size);
	} else {
		outer_clean_range(paddr, paddr + size);
	}
	/* FIXME: non-speculating: flush on bidirectional mappings? */
}
EXPORT_SYMBOL(___dma_page_cpu_to_dev);

void ___dma_page_dev_to_cpu(struct page *page, unsigned long off,
	size_t size, enum dma_data_direction dir)
{
	unsigned long paddr = page_to_phys(page) + off;

	/* FIXME: non-speculating: not required */
	/* don't bother invalidating if DMA to device */
	if (dir != DMA_TO_DEVICE)
		outer_inv_range(paddr, paddr + size);

	dma_cache_maint_page(page, off, size, dir, dmac_unmap_area);

	/*
	 * Mark the D-cache clean for this page to avoid extra flushing.
	 */
	if (dir != DMA_TO_DEVICE)
		set_bit(PG_dcache_clean, &page->flags);
}
EXPORT_SYMBOL(___dma_page_dev_to_cpu);

/**
 * dma_map_sg - map a set of SG buffers for streaming mode DMA
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
int dma_map_sg(struct device *dev, struct scatterlist *sg, int nents,
		enum dma_data_direction dir)
{
	struct scatterlist *s;
	int i, j;

	for_each_sg(sg, s, nents, i) {
		s->dma_address = dma_map_page(dev, sg_page(s), s->offset,
						s->length, dir);
		if (dma_mapping_error(dev, s->dma_address))
			goto bad_mapping;
	}
	return nents;

 bad_mapping:
	for_each_sg(sg, s, i, j)
		dma_unmap_page(dev, sg_dma_address(s), sg_dma_len(s), dir);
	return 0;
}
EXPORT_SYMBOL(dma_map_sg);

/**
 * dma_unmap_sg - unmap a set of SG buffers mapped by dma_map_sg
 * @dev: valid struct device pointer, or NULL for ISA and EISA-like devices
 * @sg: list of buffers
 * @nents: number of buffers to unmap (returned from dma_map_sg)
 * @dir: DMA transfer direction (same as was passed to dma_map_sg)
 *
 * Unmap a set of streaming mode DMA translations.  Again, CPU access
 * rules concerning calls here are the same as for dma_unmap_single().
 */
void dma_unmap_sg(struct device *dev, struct scatterlist *sg, int nents,
		enum dma_data_direction dir)
{
	struct scatterlist *s;
	int i;

	for_each_sg(sg, s, nents, i)
		dma_unmap_page(dev, sg_dma_address(s), sg_dma_len(s), dir);
}
EXPORT_SYMBOL(dma_unmap_sg);

/**
 * dma_sync_sg_for_cpu
 * @dev: valid struct device pointer, or NULL for ISA and EISA-like devices
 * @sg: list of buffers
 * @nents: number of buffers to map (returned from dma_map_sg)
 * @dir: DMA transfer direction (same as was passed to dma_map_sg)
 */
void dma_sync_sg_for_cpu(struct device *dev, struct scatterlist *sg,
			int nents, enum dma_data_direction dir)
{
	struct scatterlist *s;
	int i;

	for_each_sg(sg, s, nents, i) {
		if (!dmabounce_sync_for_cpu(dev, sg_dma_address(s), 0,
					    sg_dma_len(s), dir))
			continue;

		__dma_page_dev_to_cpu(sg_page(s), s->offset,
				      s->length, dir);
	}
}
EXPORT_SYMBOL(dma_sync_sg_for_cpu);

/**
 * dma_sync_sg_for_device
 * @dev: valid struct device pointer, or NULL for ISA and EISA-like devices
 * @sg: list of buffers
 * @nents: number of buffers to map (returned from dma_map_sg)
 * @dir: DMA transfer direction (same as was passed to dma_map_sg)
 */
void dma_sync_sg_for_device(struct device *dev, struct scatterlist *sg,
			int nents, enum dma_data_direction dir)
{
	struct scatterlist *s;
	int i;

	for_each_sg(sg, s, nents, i) {
		if (!dmabounce_sync_for_device(dev, sg_dma_address(s), 0,
					sg_dma_len(s), dir))
			continue;

		__dma_page_cpu_to_dev(sg_page(s), s->offset,
				      s->length, dir);
	}
}
EXPORT_SYMBOL(dma_sync_sg_for_device);
