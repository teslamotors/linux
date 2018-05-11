/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2013 - 2018 Intel Corporation */

#ifndef IPU_MMU_H
#define IPU_MMU_H

#include <linux/iommu.h>

#include "ipu.h"
#include "ipu-pdata.h"

#define ISYS_MMID 1
#define PSYS_MMID 0

/*
 * @pgtbl: virtual address of the l1 page table (one page)
 */
struct ipu_mmu_domain {
	u32 __iomem *pgtbl;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
	struct iommu_domain *domain;
#else
	struct iommu_domain domain;
#endif
	spinlock_t lock;	/* Serialize access to users */
	unsigned int users;
	struct ipu_dma_mapping *dmap;
	u32 dummy_l2_tbl;
	u32 dummy_page;

	/* Reference to the trash address to unmap on domain destroy */
	dma_addr_t iova_addr_trash;
};

/*
 * @pgtbl: physical address of the l1 page table
 */
struct ipu_mmu {
	struct list_head node;
	unsigned int users;

	struct ipu_mmu_hw *mmu_hw;
	unsigned int nr_mmus;
	int mmid;

	phys_addr_t pgtbl;
	struct device *dev;

	struct ipu_dma_mapping *dmap;

	struct page *trash_page;
	dma_addr_t iova_addr_trash;

	bool ready;
	spinlock_t ready_lock;	/* Serialize access to bool ready */

	void (*tlb_invalidate)(struct ipu_mmu *mmu);
	void (*set_mapping)(struct ipu_mmu *mmu,
			     struct ipu_dma_mapping *dmap);
};

#endif
