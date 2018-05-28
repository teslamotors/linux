// SPDX-License_Identifier: GPL-2.0
// Copyright (C) 2013 - 2018 Intel Corporation

#include <asm/cacheflush.h>

#include <linux/device.h>
#include <linux/iommu.h>
#include <linux/iova.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/sizes.h>

#include "ipu.h"
#include "ipu-platform.h"
#include "ipu-bus.h"
#include "ipu-dma.h"
#include "ipu-mmu.h"
#include "ipu-platform-regs.h"

#define ISP_PAGE_SHIFT		12
#define ISP_PAGE_SIZE		BIT(ISP_PAGE_SHIFT)
#define ISP_PAGE_MASK		(~(ISP_PAGE_SIZE - 1))

#define ISP_L1PT_SHIFT		22
#define ISP_L1PT_MASK		(~((1U << ISP_L1PT_SHIFT) - 1))

#define ISP_L2PT_SHIFT		12
#define ISP_L2PT_MASK		(~(ISP_L1PT_MASK | (~(ISP_PAGE_MASK))))

#define ISP_L1PT_PTES           1024
#define ISP_L2PT_PTES           1024

#define ISP_PADDR_SHIFT		12

#define REG_TLB_INVALIDATE	0x0000

#define MMU0_TLB_INVALIDATE	1

#define MMU1_TLB_INVALIDATE	0xffff

#define REG_L1_PHYS		0x0004	/* 27-bit pfn */
#define REG_INFO		0x0008

/* The range of stream ID i in L1 cache is from 0 to 15 */
#define MMUV2_REG_L1_STREAMID(i)	(0x0c + ((i) * 4))

/* The range of stream ID i in L2 cache is from 0 to 15 */
#define MMUV2_REG_L2_STREAMID(i)	(0x4c + ((i) * 4))

/* ZLW Enable for each stream in L1 MMU AT where i : 0..15 */
#define MMUV2_AT_REG_L1_ZLW_EN_SID(i)		(0x100 + ((i) * 0x20))

/* ZLW 1D mode Enable for each stream in L1 MMU AT where i : 0..15 */
#define MMUV2_AT_REG_L1_ZLW_1DMODE_SID(i)	(0x100 + ((i) * 0x20) + 0x0004)

/* Set ZLW insertion N pages ahead per stream 1D where i : 0..15 */
#define MMUV2_AT_REG_L1_ZLW_INS_N_AHEAD_SID(i)	(0x100 + ((i) * 0x20) + 0x0008)

/* ZLW 2D mode Enable for each stream in L1 MMU AT where i : 0..15 */
#define MMUV2_AT_REG_L1_ZLW_2DMODE_SID(i)	(0x100 + ((i) * 0x20) + 0x0010)

/* ZLW Insertion for each stream in L1 MMU AT where i : 0..15 */
#define MMUV2_AT_REG_L1_ZLW_INSERTION(i)	(0x100 + ((i) * 0x20) + 0x000c)

#define MMUV2_AT_REG_L1_FW_ZLW_FIFO		(0x100 + \
			(IPU_MMU_MAX_TLB_L1_STREAMS * 0x20) + 0x003c)

/* FW ZLW has prioty - needed for ZLW invalidations */
#define MMUV2_AT_REG_L1_FW_ZLW_PRIO		(0x100 + \
			(IPU_MMU_MAX_TLB_L1_STREAMS * 0x20))

#define TBL_PHYS_ADDR(a)	((phys_addr_t)(a) << ISP_PADDR_SHIFT)
#define TBL_VIRT_ADDR(a)	phys_to_virt(TBL_PHYS_ADDR(a))

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
#define to_ipu_mmu_domain(dom) ((dom)->priv)
#else
#define to_ipu_mmu_domain(dom) \
	container_of(dom, struct ipu_mmu_domain, domain)
#endif

static void zlw_invalidate(struct ipu_mmu *mmu, struct ipu_mmu_hw *mmu_hw)
{
	unsigned int retry = 0;
	unsigned int i, j;
	int ret;

	for (i = 0; i < mmu_hw->nr_l1streams; i++) {
		/* We need to invalidate only the zlw enabled stream IDs */
		if (mmu_hw->l1_zlw_en[i]) {
			/*
			 * Maximum 16 blocks per L1 stream
			 * Write trash buffer iova offset to the FW_ZLW
			 * register. This will trigger pre-fetching of next 16
			 * pages from the page table. So we need to increment
			 * iova address by 16 * 4K to trigger the next 16 pages.
			 * Once this loop is completed, the L1 cache will be
			 * filled with trash buffer translation.
			 *
			 * TODO: Instead of maximum 16 blocks, use the allocated
			 * block size
			 */
			for (j = 0; j < mmu_hw->l1_block_sz[i]; j++)
				writel(mmu->iova_addr_trash +
					   j * MMUV2_TRASH_L1_BLOCK_OFFSET,
					   mmu_hw->base +
					   MMUV2_AT_REG_L1_ZLW_INSERTION(i));

			/*
			 * Now we need to fill the L2 cache entry. L2 cache
			 * entries will be automatically updated, based on the
			 * L1 entry. The above loop for L1 will update only one
			 * of the two entries in L2 as the L1 is under 4MB
			 * range. To force the other entry in L2 to update, we
			 * just need to trigger another pre-fetch which is
			 * outside the above 4MB range.
			 */
			writel(mmu->iova_addr_trash +
				   MMUV2_TRASH_L2_BLOCK_OFFSET,
				   mmu_hw->base +
				   MMUV2_AT_REG_L1_ZLW_INSERTION(0));
		}
	}

	/*
	 * Wait until AT is ready. FIFO read should return 2 when AT is ready.
	 * Retry value of 1000 is just by guess work to avoid the forever loop.
	 */
	do {
		if (retry > 1000) {
			dev_err(mmu->dev, "zlw invalidation failed\n");
			return;
		}
		ret = readl(mmu_hw->base + MMUV2_AT_REG_L1_FW_ZLW_FIFO);
		retry++;
	} while (ret != 2);
}

static void tlb_invalidate(struct ipu_mmu *mmu)
{
	unsigned int i;
	unsigned long flags;

	spin_lock_irqsave(&mmu->ready_lock, flags);
	if (!mmu->ready) {
		spin_unlock_irqrestore(&mmu->ready_lock, flags);
		return;
	}

	for (i = 0; i < mmu->nr_mmus; i++) {
		u32 inv;

		/*
		 * To avoid the HW bug induced dead lock in some of the IPU4
		 * MMUs on successive invalidate calls, we need to first do a
		 * read to the page table base before writing the invalidate
		 * register. MMUs which need to implement this WA, will have
		 * the insert_read_before_invalidate flasg set as true.
		 * Disregard the return value of the read.
		 */
		if (mmu->mmu_hw[i].insert_read_before_invalidate)
			readl(mmu->mmu_hw[i].base + REG_L1_PHYS);

		/* Normal invalidate or zlw invalidate */
		if (mmu->mmu_hw[i].zlw_invalidate) {
			/* trash buffer must be mapped by now, just in case! */
			WARN_ON(!mmu->iova_addr_trash);

			zlw_invalidate(mmu, &mmu->mmu_hw[i]);
		} else {
			if (mmu->mmu_hw[i].nr_l1streams == 32)
				inv = 0xffffffff;
			else if (mmu->mmu_hw[i].nr_l1streams == 0)
				inv = MMU0_TLB_INVALIDATE;
			else
				inv = MMU1_TLB_INVALIDATE;
			writel(inv, mmu->mmu_hw[i].base +
				   REG_TLB_INVALIDATE);
		}
	}
	spin_unlock_irqrestore(&mmu->ready_lock, flags);
}

#ifdef DEBUG
static void page_table_dump(struct ipu_mmu_domain *adom)
{
	u32 l1_idx;

	pr_debug("begin IOMMU page table dump\n");

	for (l1_idx = 0; l1_idx < ISP_L1PT_PTES; l1_idx++) {
		u32 l2_idx;
		u32 iova = (phys_addr_t) l1_idx << ISP_L1PT_SHIFT;

		if (adom->pgtbl[l1_idx] == adom->dummy_l2_tbl)
			continue;
		pr_debug("l1 entry %u; iovas 0x%8.8x--0x%8.8x, at %p\n",
			 l1_idx, iova, iova + ISP_PAGE_SIZE,
			 (void *)TBL_PHYS_ADDR(adom->pgtbl[l1_idx]));

		for (l2_idx = 0; l2_idx < ISP_L2PT_PTES; l2_idx++) {
			u32 *l2_pt = TBL_VIRT_ADDR(adom->pgtbl[l1_idx]);
			u32 iova2 = iova + (l2_idx << ISP_L2PT_SHIFT);

			if (l2_pt[l2_idx] == adom->dummy_page)
				continue;

			pr_debug("\tl2 entry %u; iova 0x%8.8x, phys %p\n",
				 l2_idx, iova2,
				 (void *)TBL_PHYS_ADDR(l2_pt[l2_idx]));
		}
	}

	pr_debug("end IOMMU page table dump\n");
}
#endif /* DEBUG */

static u32 *alloc_page_table(struct ipu_mmu_domain *adom, bool l1)
{
	u32 *pt = (u32 *) __get_free_page(GFP_KERNEL | GFP_DMA32);
	int i;

	if (!pt)
		return NULL;

	pr_debug("__get_free_page() == %p\n", pt);

	for (i = 0; i < ISP_L1PT_PTES; i++)
		pt[i] = l1 ? adom->dummy_l2_tbl : adom->dummy_page;

	return pt;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
static int ipu_mmu_domain_init(struct iommu_domain *domain)
{
	struct ipu_mmu_domain *adom;
	void *ptr;

	adom = kzalloc(sizeof(*adom), GFP_KERNEL);
	if (!adom)
		return -ENOMEM;

	domain->priv = adom;
	adom->domain = domain;

	ptr = (void *)__get_free_page(GFP_KERNEL | GFP_DMA32);
	if (!ptr)
		goto err;

	adom->dummy_page = virt_to_phys(ptr) >> ISP_PAGE_SHIFT;

	ptr = alloc_page_table(adom, false);
	if (!ptr)
		goto err;

	adom->dummy_l2_tbl = virt_to_phys(ptr) >> ISP_PAGE_SHIFT;

	/*
	 * We always map the L1 page table (a single page as well as
	 * the L2 page tables).
	 */
	adom->pgtbl = alloc_page_table(adom, true);
	if (!adom->pgtbl)
		goto err;

	spin_lock_init(&adom->lock);

	pr_debug("domain initialised\n");
	pr_debug("ops %p\n", domain->ops);

	return 0;

err:
	free_page((unsigned long)TBL_VIRT_ADDR(adom->dummy_page));
	free_page((unsigned long)TBL_VIRT_ADDR(adom->dummy_l2_tbl));
	kfree(adom);

	return -ENOMEM;
}
#else
static struct iommu_domain *ipu_mmu_domain_alloc(unsigned int type)
{
	struct ipu_mmu_domain *adom;
	void *ptr;

	if (type != IOMMU_DOMAIN_UNMANAGED)
		return NULL;

	adom = kzalloc(sizeof(*adom), GFP_KERNEL);
	if (!adom)
		return NULL;

	adom->domain.geometry.aperture_start = 0;
	adom->domain.geometry.aperture_end = DMA_BIT_MASK(IPU_MMU_ADDRESS_BITS);
	adom->domain.geometry.force_aperture = true;

	ptr = (void *)__get_free_page(GFP_KERNEL | GFP_DMA32);
	if (!ptr)
		goto err_mem;

	adom->dummy_page = virt_to_phys(ptr) >> ISP_PAGE_SHIFT;

	ptr = alloc_page_table(adom, false);
	if (!ptr)
		goto err;

	adom->dummy_l2_tbl = virt_to_phys(ptr) >> ISP_PAGE_SHIFT;

	/*
	 * We always map the L1 page table (a single page as well as
	 * the L2 page tables).
	 */
	adom->pgtbl = alloc_page_table(adom, true);
	if (!adom->pgtbl)
		goto err;

	spin_lock_init(&adom->lock);

	pr_debug("domain initialised\n");
	pr_debug("ops %p\n", adom->domain.ops);

	return &adom->domain;

err:
	free_page((unsigned long)TBL_VIRT_ADDR(adom->dummy_page));
	free_page((unsigned long)TBL_VIRT_ADDR(adom->dummy_l2_tbl));
err_mem:
	kfree(adom);

	return NULL;
}
#endif

static void ipu_mmu_domain_destroy(struct iommu_domain *domain)
{
	struct ipu_mmu_domain *adom = to_ipu_mmu_domain(domain);
	struct iova *iova;
	u32 l1_idx;

	if (adom->iova_addr_trash) {
		iova = find_iova(&adom->dmap->iovad, adom->iova_addr_trash >>
				 PAGE_SHIFT);
		/* unmap and free the corresponding trash buffer iova */
		iommu_unmap(domain, iova->pfn_lo << PAGE_SHIFT,
			    (iova->pfn_hi - iova->pfn_lo + 1) << PAGE_SHIFT);
		__free_iova(&adom->dmap->iovad, iova);

		/*
		 * Set iova_addr_trash in mmu to 0, so that on next HW init
		 * this will be mapped again.
		 */
		adom->iova_addr_trash = 0;
	}

	for (l1_idx = 0; l1_idx < ISP_L1PT_PTES; l1_idx++)
		if (adom->pgtbl[l1_idx] != adom->dummy_l2_tbl)
			free_page((unsigned long)
				  TBL_VIRT_ADDR(adom->pgtbl[l1_idx]));

	free_page((unsigned long)TBL_VIRT_ADDR(adom->dummy_page));
	free_page((unsigned long)TBL_VIRT_ADDR(adom->dummy_l2_tbl));
	free_page((unsigned long)adom->pgtbl);
	kfree(adom);
}

static int ipu_mmu_attach_dev(struct iommu_domain *domain, struct device *dev)
{
	struct ipu_mmu_domain *adom = to_ipu_mmu_domain(domain);

	spin_lock(&adom->lock);

	adom->users++;

	dev_dbg(dev, "domain attached\n");

	spin_unlock(&adom->lock);

	return 0;
}

static void ipu_mmu_detach_dev(struct iommu_domain *domain, struct device *dev)
{
	struct ipu_mmu_domain *adom = to_ipu_mmu_domain(domain);

	spin_lock(&adom->lock);

	adom->users--;
	dev_dbg(dev, "domain detached\n");

	spin_unlock(&adom->lock);
}

static int l2_map(struct iommu_domain *domain, unsigned long iova,
		  phys_addr_t paddr, size_t size)
{
	struct ipu_mmu_domain *adom = to_ipu_mmu_domain(domain);
	u32 l1_idx = iova >> ISP_L1PT_SHIFT;
	u32 l1_entry = adom->pgtbl[l1_idx];
	u32 *l2_pt;
	u32 iova_start = iova;
	unsigned int l2_idx;
	unsigned long flags;

	pr_debug("mapping l2 page table for l1 index %u (iova %8.8x)\n",
		 l1_idx, (u32) iova);

	if (l1_entry == adom->dummy_l2_tbl) {
		u32 *l2_virt = alloc_page_table(adom, false);

		if (!l2_virt)
			return -ENOMEM;

		l1_entry = virt_to_phys(l2_virt) >> ISP_PADDR_SHIFT;
		pr_debug("allocated page for l1_idx %u\n", l1_idx);

		spin_lock_irqsave(&adom->lock, flags);
		if (adom->pgtbl[l1_idx] == adom->dummy_l2_tbl) {
			adom->pgtbl[l1_idx] = l1_entry;
#ifdef CONFIG_X86
			clflush_cache_range(&adom->pgtbl[l1_idx],
					    sizeof(adom->pgtbl[l1_idx]));
#endif /* CONFIG_X86 */
		} else {
			spin_unlock_irqrestore(&adom->lock, flags);
			free_page((unsigned long)TBL_VIRT_ADDR(l1_entry));
			spin_lock_irqsave(&adom->lock, flags);
		}
	} else {
		spin_lock_irqsave(&adom->lock, flags);
	}

	l2_pt = TBL_VIRT_ADDR(adom->pgtbl[l1_idx]);

	pr_debug("l2_pt at %p\n", l2_pt);

	paddr = ALIGN(paddr, ISP_PAGE_SIZE);

	l2_idx = (iova_start & ISP_L2PT_MASK) >> ISP_L2PT_SHIFT;

	pr_debug("l2_idx %u, phys 0x%8.8x\n", l2_idx, l2_pt[l2_idx]);
	if (l2_pt[l2_idx] != adom->dummy_page) {
		spin_unlock_irqrestore(&adom->lock, flags);
		return -EBUSY;
	}

	l2_pt[l2_idx] = paddr >> ISP_PADDR_SHIFT;

	spin_unlock_irqrestore(&adom->lock, flags);

#ifdef CONFIG_X86
	clflush_cache_range(&l2_pt[l2_idx], sizeof(l2_pt[l2_idx]));
#endif /* CONFIG_X86 */

	pr_debug("l2 index %u mapped as 0x%8.8x\n", l2_idx, l2_pt[l2_idx]);

	return 0;
}

static int ipu_mmu_map(struct iommu_domain *domain, unsigned long iova,
		       phys_addr_t paddr, size_t size, int prot)
{
	u32 iova_start = round_down(iova, ISP_PAGE_SIZE);
	u32 iova_end = ALIGN(iova + size, ISP_PAGE_SIZE);

	pr_debug
	    ("mapping iova 0x%8.8x--0x%8.8x, size %zu at paddr 0x%10.10llx\n",
	     iova_start, iova_end, size, paddr);

	return l2_map(domain, iova_start, paddr, size);
}

static size_t l2_unmap(struct iommu_domain *domain, unsigned long iova,
		       phys_addr_t dummy, size_t size)
{
	struct ipu_mmu_domain *adom = to_ipu_mmu_domain(domain);
	u32 l1_idx = iova >> ISP_L1PT_SHIFT;
	u32 *l2_pt = TBL_VIRT_ADDR(adom->pgtbl[l1_idx]);
	u32 iova_start = iova;
	unsigned int l2_idx;
	size_t unmapped = 0;

	pr_debug("unmapping l2 page table for l1 index %u (iova 0x%8.8lx)\n",
		 l1_idx, iova);

	if (adom->pgtbl[l1_idx] == adom->dummy_l2_tbl)
		return -EINVAL;

	pr_debug("l2_pt at %p\n", l2_pt);

	for (l2_idx = (iova_start & ISP_L2PT_MASK) >> ISP_L2PT_SHIFT;
	     (iova_start & ISP_L1PT_MASK) + (l2_idx << ISP_PAGE_SHIFT)
	     < iova_start + size && l2_idx < ISP_L2PT_PTES; l2_idx++) {
		unsigned long flags;

		pr_debug("l2 index %u unmapped, was 0x%10.10llx\n",
			 l2_idx, TBL_PHYS_ADDR(l2_pt[l2_idx]));
		spin_lock_irqsave(&adom->lock, flags);
		l2_pt[l2_idx] = adom->dummy_page;
		spin_unlock_irqrestore(&adom->lock, flags);
#ifdef CONFIG_X86
		clflush_cache_range(&l2_pt[l2_idx], sizeof(l2_pt[l2_idx]));
#endif /* CONFIG_X86 */
		unmapped++;
	}

	return unmapped << ISP_PAGE_SHIFT;
}

static size_t ipu_mmu_unmap(struct iommu_domain *domain,
			    unsigned long iova, size_t size)
{
	return l2_unmap(domain, iova, 0, size);
}

static phys_addr_t ipu_mmu_iova_to_phys(struct iommu_domain *domain,
					dma_addr_t iova)
{
	struct ipu_mmu_domain *adom = to_ipu_mmu_domain(domain);
	u32 *l2_pt = TBL_VIRT_ADDR(adom->pgtbl[iova >> ISP_L1PT_SHIFT]);

	return (phys_addr_t) l2_pt[(iova & ISP_L2PT_MASK) >> ISP_L2PT_SHIFT]
	    << ISP_PAGE_SHIFT;
}

static int allocate_trash_buffer(struct ipu_bus_device *adev)
{
	struct ipu_mmu *mmu = ipu_bus_get_drvdata(adev);
	unsigned int n_pages = PAGE_ALIGN(IPU_MMUV2_TRASH_RANGE) >> PAGE_SHIFT;
	struct iova *iova;
	u32 iova_addr;
	unsigned int i;
	int ret;

	/* Allocate 8MB in iova range */
	iova = alloc_iova(&mmu->dmap->iovad, n_pages,
			  dma_get_mask(mmu->dev) >> PAGE_SHIFT, 0);
	if (!iova) {
		dev_err(&adev->dev, "cannot allocate iova range for trash\n");
		return -ENOMEM;
	}

	/*
	 * Map the 8MB iova address range to the same physical trash page
	 * mmu->trash_page which is already reserved at the probe
	 */
	iova_addr = iova->pfn_lo;
	for (i = 0; i < n_pages; i++) {
		ret = iommu_map(mmu->dmap->domain, iova_addr << PAGE_SHIFT,
				page_to_phys(mmu->trash_page), PAGE_SIZE, 0);
		if (ret) {
			dev_err(&adev->dev,
				"mapping trash buffer range failed\n");
			goto out_unmap;
		}

		iova_addr++;
	}

	/* save the address for the ZLW invalidation */
	mmu->iova_addr_trash = iova->pfn_lo << PAGE_SHIFT;
	dev_info(&adev->dev, "iova trash buffer for MMUID: %d is %u\n",
		 mmu->mmid, (unsigned int)mmu->iova_addr_trash);
	return 0;

out_unmap:
	iommu_unmap(mmu->dmap->domain, iova->pfn_lo << PAGE_SHIFT,
		    (iova->pfn_hi - iova->pfn_lo + 1) << PAGE_SHIFT);
	__free_iova(&mmu->dmap->iovad, iova);
	return ret;
}

static int ipu_mmu_hw_init(struct device *dev)
{
	struct ipu_bus_device *adev = to_ipu_bus_device(dev);
	struct ipu_mmu *mmu = ipu_bus_get_drvdata(adev);
	struct ipu_mmu_domain *adom;
	unsigned int i;
	unsigned long flags;

	dev_dbg(dev, "mmu hw init\n");
	/*
	 * FIXME: following fix for null pointer check is not a complete one.
	 * if mmu is not powered cycled before being used, the page table
	 * address will still not be set into HW.
	 */
	if (!mmu->dmap) {
		dev_warn(dev, "mmu is not ready yet. skipping.\n");
		return 0;
	}
	adom = to_ipu_mmu_domain(mmu->dmap->domain);

	adom->dmap = mmu->dmap;

	/* Initialise the each MMU HW block */
	for (i = 0; i < mmu->nr_mmus; i++) {
		struct ipu_mmu_hw *mmu_hw = &mmu->mmu_hw[i];
		bool zlw_invalidate = false;
		unsigned int j;
		u16 block_addr;

		/* Write page table address per MMU */
		writel((phys_addr_t) virt_to_phys(adom->pgtbl)
			   >> ISP_PADDR_SHIFT,
			   mmu->mmu_hw[i].base + REG_L1_PHYS);

		/* Set info bits per MMU */
		writel(mmu->mmu_hw[i].info_bits,
			   mmu->mmu_hw[i].base + REG_INFO);

		/* Configure MMU TLB stream configuration for L1 */
		for (j = 0, block_addr = 0; j < mmu_hw->nr_l1streams;
		     block_addr += mmu->mmu_hw[i].l1_block_sz[j], j++) {
			if (block_addr > IPU_MAX_LI_BLOCK_ADDR) {
				dev_err(dev, "invalid L1 configuration\n");
				return -EINVAL;
			}

			/* Write block start address for each streams */
			writel(block_addr, mmu_hw->base +
				   mmu_hw->l1_stream_id_reg_offset + 4 * j);

#if defined(CONFIG_VIDEO_INTEL_IPU4) || defined(CONFIG_VIDEO_INTEL_IPU4P)
			/* Enable ZLW for streams based on the init table */
			writel(mmu->mmu_hw[i].l1_zlw_en[j],
				   mmu_hw->base +
				   MMUV2_AT_REG_L1_ZLW_EN_SID(j));

			/* To track if zlw is enabled in any streams */
			zlw_invalidate |= mmu->mmu_hw[i].l1_zlw_en[j];

			/* Enable ZLW 1D mode for streams from the init table */
			writel(mmu->mmu_hw[i].l1_zlw_1d_mode[j],
				   mmu_hw->base +
				   MMUV2_AT_REG_L1_ZLW_1DMODE_SID(j));

			/* Set when the ZLW insertion will happen */
			writel(mmu->mmu_hw[i].l1_ins_zlw_ahead_pages[j],
				   mmu_hw->base +
				   MMUV2_AT_REG_L1_ZLW_INS_N_AHEAD_SID(j));

			/* Set if ZLW 2D mode active for each streams */
			writel(mmu->mmu_hw[i].l1_zlw_2d_mode[j],
				   mmu_hw->base +
				   MMUV2_AT_REG_L1_ZLW_2DMODE_SID(j));
#endif
		}

#if defined(CONFIG_VIDEO_INTEL_IPU4) || defined(CONFIG_VIDEO_INTEL_IPU4P)
		/*
		 * If ZLW invalidate is enabled even for one stream in a MMU1,
		 * we need to set the FW ZLW operations have higher priority
		 * on that MMU1
		 */
		if (zlw_invalidate)
			writel(1, mmu_hw->base +
				   MMUV2_AT_REG_L1_FW_ZLW_PRIO);
#endif
		/* Configure MMU TLB stream configuration for L2 */
		for (j = 0, block_addr = 0; j < mmu_hw->nr_l2streams;
		     block_addr += mmu->mmu_hw[i].l2_block_sz[j], j++) {
			if (block_addr > IPU_MAX_L2_BLOCK_ADDR) {
				dev_err(dev, "invalid L2 configuration\n");
				return -EINVAL;
			}

			writel(block_addr, mmu_hw->base +
				   mmu_hw->l2_stream_id_reg_offset + 4 * j);
		}
	}

	/* Allocate trash buffer, if not allocated. Only once per MMU */
	if (!mmu->iova_addr_trash) {
		int ret;

		ret = allocate_trash_buffer(adev);
		if (ret) {
			dev_err(dev, "trash buffer allocation failed\n");
			return ret;
		}

		/*
		 * Update the domain pointer to trash buffer to release it on
		 * domain destroy
		 */
		adom->iova_addr_trash = mmu->iova_addr_trash;
	}

	spin_lock_irqsave(&mmu->ready_lock, flags);
	mmu->ready = true;
	spin_unlock_irqrestore(&mmu->ready_lock, flags);

	return 0;
}

static void set_mapping(struct ipu_mmu *mmu, struct ipu_dma_mapping *dmap)
{
	mmu->dmap = dmap;

	if (!dmap)
		return;

	pm_runtime_get_sync(mmu->dev);
	ipu_mmu_hw_init(mmu->dev);
	pm_runtime_put(mmu->dev);
}

static int ipu_mmu_add_device(struct device *dev)
{
	struct device *aiommu = to_ipu_bus_device(dev)->iommu;
	struct ipu_dma_mapping *dmap;
	int rval;

	if (!aiommu || !dev->iommu_group)
		return 0;

	dmap = iommu_group_get_iommudata(dev->iommu_group);
	if (!dmap)
		return 0;

	pr_debug("attach dev %s\n", dev_name(dev));

	rval = iommu_attach_device(dmap->domain, dev);
	if (rval)
		return rval;

	kref_get(&dmap->ref);

	return 0;
}

static struct iommu_ops ipu_iommu_ops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
	.domain_init = ipu_mmu_domain_init,
	.domain_destroy = ipu_mmu_domain_destroy,
#else
	.domain_alloc = ipu_mmu_domain_alloc,
	.domain_free = ipu_mmu_domain_destroy,
#endif
	.attach_dev = ipu_mmu_attach_dev,
	.detach_dev = ipu_mmu_detach_dev,
	.map = ipu_mmu_map,
	.unmap = ipu_mmu_unmap,
	.iova_to_phys = ipu_mmu_iova_to_phys,
	.add_device = ipu_mmu_add_device,
	.pgsize_bitmap = SZ_4K,
};

static int ipu_mmu_probe(struct ipu_bus_device *adev)
{
	struct ipu_mmu_pdata *pdata;
	struct ipu_mmu *mmu;
	int rval;

	mmu = devm_kzalloc(&adev->dev, sizeof(*mmu), GFP_KERNEL);
	if (!mmu)
		return -ENOMEM;

	dev_dbg(&adev->dev, "mmu probe %p %p\n", adev, &adev->dev);
	ipu_bus_set_drvdata(adev, mmu);

	rval = ipu_bus_set_iommu(&ipu_iommu_ops);
	if (rval)
		return rval;

	pdata = adev->pdata;

	mmu->mmid = pdata->mmid;

	mmu->mmu_hw = pdata->mmu_hw;
	mmu->nr_mmus = pdata->nr_mmus;
	mmu->tlb_invalidate = tlb_invalidate;
	mmu->set_mapping = set_mapping;
	mmu->dev = &adev->dev;
	mmu->ready = false;
	spin_lock_init(&mmu->ready_lock);

	/*
	 * Allocate 1 page of physical memory for the trash buffer
	 *
	 * TODO! Could be further optimized by allocating only one page per ipu
	 * instance instead of per mmu
	 */
	mmu->trash_page = alloc_page(GFP_KERNEL);
	if (!mmu->trash_page) {
		dev_err(&adev->dev, "insufficient memory for trash buffer\n");
		return -ENOMEM;
	}
	dev_info(&adev->dev, "MMU: %d, allocated page for trash: 0x%p\n",
		 mmu->mmid, mmu->trash_page);

	pm_runtime_allow(&adev->dev);
	pm_runtime_enable(&adev->dev);

	/*
	 * FIXME: We can't unload this --- bus_set_iommu() will
	 * register a notifier which must stay until the devices are
	 * gone.
	 */
	__module_get(THIS_MODULE);

	return 0;
}

/*
 * Leave iommu ops as they were --- this means we must be called as
 * the very last.
 */
static void ipu_mmu_remove(struct ipu_bus_device *adev)
{
	struct ipu_mmu *mmu = ipu_bus_get_drvdata(adev);

	__free_page(mmu->trash_page);
	dev_dbg(&adev->dev, "removed\n");
}

static irqreturn_t ipu_mmu_isr(struct ipu_bus_device *adev)
{
	dev_info(&adev->dev, "Yeah!\n");
	return IRQ_NONE;
}

#ifdef CONFIG_PM
static int ipu_mmu_suspend(struct device *dev)
{
	struct ipu_bus_device *adev = to_ipu_bus_device(dev);
	struct ipu_mmu *mmu = ipu_bus_get_drvdata(adev);
	unsigned long flags;

	spin_lock_irqsave(&mmu->ready_lock, flags);
	mmu->ready = false;
	spin_unlock_irqrestore(&mmu->ready_lock, flags);

	return 0;
}

static const struct dev_pm_ops ipu_mmu_pm_ops = {
	.resume = ipu_mmu_hw_init,
	.suspend = ipu_mmu_suspend,
	.runtime_resume = ipu_mmu_hw_init,
	.runtime_suspend = ipu_mmu_suspend,
};

#define IPU_MMU_PM_OPS	(&ipu_mmu_pm_ops)

#else /* !CONFIG_PM */

#define IPU_MMU_PM_OPS	NULL

#endif /* !CONFIG_PM */

static struct ipu_bus_driver ipu_mmu_driver = {
	.probe = ipu_mmu_probe,
	.remove = ipu_mmu_remove,
	.isr = ipu_mmu_isr,
	.wanted = IPU_MMU_NAME,
	.drv = {
		.name = IPU_MMU_NAME,
		.owner = THIS_MODULE,
		.pm = IPU_MMU_PM_OPS,
	},
};
module_ipu_bus_driver(ipu_mmu_driver);

static const struct pci_device_id ipu_pci_tbl[] = {
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, IPU_PCI_ID)},
	{0,}
};
MODULE_DEVICE_TABLE(pci, ipu_pci_tbl);

MODULE_AUTHOR("Sakari Ailus <sakari.ailus@linux.intel.com>");
MODULE_AUTHOR("Samu Onkalo <samu.onkalo@intel.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Intel ipu mmu driver");
