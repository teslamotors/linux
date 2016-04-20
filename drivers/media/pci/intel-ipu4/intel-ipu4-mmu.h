/*
 * Copyright (c) 2013--2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef INTEL_IPU4_MMU_H
#define INTEL_IPU4_MMU_H

#include <linux/iommu.h>

#include "intel-ipu4.h"
#include "intel-ipu4-pdata.h"

struct pci_dev;

/*
 * @pgtbl: virtual address of the l1 page table (one page)
 */
struct intel_ipu4_mmu_domain {
	uint32_t __iomem *pgtbl;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
	struct iommu_domain *domain;
#else
	struct iommu_domain domain;
#endif
	spinlock_t lock;
	unsigned int users;
	struct intel_ipu4_dma_mapping *dmap;
	uint32_t dummy_l2_tbl;
	uint32_t dummy_page;

	/* Reference to the trash address to unmap on domain destroy */
	dma_addr_t iova_addr_trash;
};

/*
 * @pgtbl: physical address of the l1 page table
 */
struct intel_ipu4_mmu {
	struct list_head node;
	unsigned int users;

	struct intel_ipu4_mmu_hw *mmu_hw;
	unsigned int nr_mmus;
	int mmid;

	phys_addr_t pgtbl;
	struct device *dev;

	struct intel_ipu4_dma_mapping *dmap;

	struct page *trash_page;
	dma_addr_t iova_addr_trash;

	bool ready;
	spinlock_t ready_lock;

	void (*tlb_invalidate)(struct intel_ipu4_mmu *mmu);
	void (*set_mapping)(struct intel_ipu4_mmu *mmu,
			   struct intel_ipu4_dma_mapping *dmap);
};

#endif
