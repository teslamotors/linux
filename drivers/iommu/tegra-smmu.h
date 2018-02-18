/*
 * Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef __INCLUDE_TEGRA_SMMU_H
#define __INCLUDE_TEGRA_SMMU_H

#include <linux/iommu.h>
#include <linux/platform_device.h>

/* bitmap of the page sizes currently supported */
#ifdef CONFIG_TEGRA_IOMMU_SMMU_NO4MB
#define SMMU_IOMMU_PGSIZES			SZ_4K
#else
#define SMMU_IOMMU_PGSIZES			(SZ_4K | SZ_4M)
#endif

#define SMMU_CONFIG				0x10
#define SMMU_CONFIG_DISABLE			0
#define SMMU_CONFIG_ENABLE			1

#define SMMU_CACHE_CONFIG_BASE			0x14
#define __SMMU_CACHE_CONFIG(mc, cache)		(SMMU_CACHE_CONFIG_BASE + 4 * cache)
#define SMMU_CACHE_CONFIG(cache)		__SMMU_CACHE_CONFIG(_MC, cache)

#define SMMU_CACHE_CONFIG_STATS_SHIFT		31
#define SMMU_CACHE_CONFIG_STATS_ENABLE		(1 << SMMU_CACHE_CONFIG_STATS_SHIFT)
#define SMMU_CACHE_CONFIG_STATS_TEST_SHIFT	30
#define SMMU_CACHE_CONFIG_STATS_TEST		(1 << SMMU_CACHE_CONFIG_STATS_TEST_SHIFT)

#define SMMU_TLB_CONFIG_HIT_UNDER_MISS__ENABLE	(1 << 29)
#define SMMU_TLB_CONFIG_ACTIVE_LINES__MASK	0x3f
#define SMMU_TLB_CONFIG_ACTIVE_LINES__VALUE	0x10
#define SMMU_TLB_CONFIG_RESET_VAL		0x30000000

#define SMMU_PTC_CONFIG_CACHE__ENABLE		(1 << 29)
#define SMMU_PTC_CONFIG_INDEX_MAP__PATTERN	0x3f
#define SMMU_PTC_CONFIG_RESET_VAL		0x2000003f
#define SMMU_PTC_REQ_LIMIT			(8 << 24)

#define SMMU_PTB_ASID				0x1c
#define SMMU_PTB_ASID_CURRENT_SHIFT		0

#define SMMU_PTB_DATA				0x20
#define SMMU_PTB_DATA_RESET_VAL			0
#define SMMU_PTB_DATA_ASID_NONSECURE_SHIFT	29
#define SMMU_PTB_DATA_ASID_WRITABLE_SHIFT	30
#define SMMU_PTB_DATA_ASID_READABLE_SHIFT	31

#define SMMU_TLB_FLUSH				0x30
#define SMMU_TLB_FLUSH_VA_MATCH_ALL		0
#define SMMU_TLB_FLUSH_VA_MATCH_SECTION		2
#define SMMU_TLB_FLUSH_VA_MATCH_GROUP		3
#define SMMU_TLB_FLUSH_ASID_SHIFT_BASE		31
#define SMMU_TLB_FLUSH_ASID_MATCH_DISABLE	0
#define SMMU_TLB_FLUSH_ASID_MATCH_ENABLE	1
#define SMMU_TLB_FLUSH_ASID_MATCH_SHIFT		31
#define SMMU_TLB_FLUSH_ASID_ENABLE		\
	(SMMU_TLB_FLUSH_ASID_MATCH_ENABLE << SMMU_TLB_FLUSH_ASID_MATCH_SHIFT)

#define SMMU_TLB_FLUSH_ASID_SHIFT(as)		\
	(SMMU_TLB_FLUSH_ASID_SHIFT_BASE -       \
			__ffs((as)->smmu->chip_data->num_asids))
#define SMMU_ASID_MASK  ((1 << __ffs((as)->smmu->chip_data->num_asids)) - 1)

#define SMMU_PTC_FLUSH				0x34
#define SMMU_PTC_FLUSH_TYPE_ALL			0
#define SMMU_PTC_FLUSH_TYPE_ADR			1
#define SMMU_PTC_FLUSH_ADR_MASK			0xfffffff0

#define SMMU_PTC_FLUSH_1			0x9b8

#define SMMU_STATS_CACHE_COUNT_BASE		0x1f0

#define SMMU_STATS_CACHE_COUNT(mc, cache, hitmiss)	\
	(SMMU_STATS_CACHE_COUNT_BASE + 8 * cache + 4 * hitmiss)

#define SMMU_AFI_ASID				0x238   /* PCIE */

#define SMMU_SWGRP_ASID_BASE			SMMU_AFI_ASID

#define HWGRP_COUNT				64

#define SMMU_PDE_NEXT_SHIFT			28

/* AHB Arbiter Registers */
#define AHB_XBAR_CTRL				0xe0
#define AHB_XBAR_CTRL_SMMU_INIT_DONE_DONE	1
#define AHB_XBAR_CTRL_SMMU_INIT_DONE_SHIFT	17
#define AHB_MASTER_SWID_0			0x18
#define AHB_MASTER_SELECT_SDMMC			(BIT(9) | BIT(12) \
							| BIT(19) | BIT(20))
#define SMMU_NUM_ASIDS				4
#define SMMU_NUM_ASIDS_TEGRA12			128
#define SMMU_TLB_FLUSH_VA_SECTION__MASK		0xffc00000
#define SMMU_TLB_FLUSH_VA_SECTION__SHIFT	12 /* right shift */
#define SMMU_TLB_FLUSH_VA_GROUP__MASK		0xffffc000
#define SMMU_TLB_FLUSH_VA_GROUP__SHIFT		12 /* right shift */
#define SMMU_TLB_FLUSH_VA(iova, which)		\
		((((iova) & SMMU_TLB_FLUSH_VA_##which##__MASK) >> \
			SMMU_TLB_FLUSH_VA_##which##__SHIFT) |   \
				SMMU_TLB_FLUSH_VA_MATCH_##which)
#define SMMU_PTB_ASID_CUR(n)			\
					((n) << SMMU_PTB_ASID_CURRENT_SHIFT)

#define SMMU_TLB_FLUSH_ALL			0

#define SMMU_TLB_FLUSH_ASID_MATCH_disable	\
				(SMMU_TLB_FLUSH_ASID_MATCH_DISABLE <<	\
				SMMU_TLB_FLUSH_ASID_MATCH_SHIFT)
#define SMMU_TLB_FLUSH_ASID_MATCH__ENABLE	\
				(SMMU_TLB_FLUSH_ASID_MATCH_ENABLE <<	\
				SMMU_TLB_FLUSH_ASID_MATCH_SHIFT)

#define SMMU_PAGE_SHIFT				12
#define SMMU_PAGE_SIZE				(1 << SMMU_PAGE_SHIFT)

#define SMMU_PDIR_COUNT				1024
#define SMMU_PDIR_SIZE				(sizeof(u32) * SMMU_PDIR_COUNT)
#define SMMU_PTBL_COUNT				1024
#define SMMU_PTBL_SIZE				(sizeof(u32) * SMMU_PTBL_COUNT)
#define SMMU_PDIR_SHIFT				12
#define SMMU_PDE_SHIFT				12
#define SMMU_PTE_SHIFT				12
#define SMMU_PFN_MASK				0x0fffffff

#define SMMU_ADDR_TO_PTN(addr)			((((u32)addr) >> 12) & (BIT(10) - 1))
#define SMMU_ADDR_TO_PDN(addr)			(((u32)addr) >> 22)
#define SMMU_PDN_TO_ADDR(pdn)			((pdn) << 22)

#define _READABLE				(1 << SMMU_PTB_DATA_ASID_READABLE_SHIFT)
#define _WRITABLE				(1 << SMMU_PTB_DATA_ASID_WRITABLE_SHIFT)
#define _NONSECURE				(1 << SMMU_PTB_DATA_ASID_NONSECURE_SHIFT)
#define _PDE_NEXT				(1 << SMMU_PDE_NEXT_SHIFT)
#define _MASK_ATTR				(_READABLE | _WRITABLE | _NONSECURE)

#define _PDIR_ATTR				(_READABLE | _WRITABLE | _NONSECURE)

#define _PDE_ATTR				(_READABLE | _WRITABLE | _NONSECURE)
#define _PDE_ATTR_N				(_PDE_ATTR | _PDE_NEXT)
#define _PDE_VACANT(pdn)			(0)

#define _PTE_ATTR				(_READABLE | _WRITABLE | _NONSECURE)
#define _PTE_VACANT(addr)			(0)

#ifdef CONFIG_TEGRA_IOMMU_SMMU_LINEAR
#undef  _PDE_VACANT
#undef  _PTE_VACANT
#define _PDE_VACANT(pdn)			(((pdn) << 10) | _PDE_ATTR)
#define _PTE_VACANT(addr)			((((u32)addr) >> SMMU_PAGE_SHIFT) | _PTE_ATTR)
#endif

#define SMMU_MK_PDIR(page, attr)		\
						((page_to_phys(page) >> SMMU_PDIR_SHIFT) | (attr))
#define SMMU_MK_PDE(page, attr)			\
						(u32)((page_to_phys(page) >> SMMU_PDE_SHIFT) | (attr))
#define SMMU_EX_PTBL_PAGE(pde)			phys_to_page((phys_addr_t)(pde & SMMU_PFN_MASK) << SMMU_PDE_SHIFT)
#define SMMU_PFN_TO_PTE(pfn, attr)		(u32)((pfn) | (attr))

#define SMMU_ASID_ENABLE(asid, idx)		(((asid) << (idx * 8)) | (1 << 31))
#define SMMU_ASID_DISABLE			0
#define MAX_AS_PER_DEV				4
#define SMMU_ASID_GET_IDX(iova)			(int)(((u64)iova >> 32) & (MAX_AS_PER_DEV - 1))

#define SMMU_CLIENT_CONF0			0x40

struct smmu_domain {
	struct iommu_domain *iommu_domain;
	struct smmu_as *as[MAX_AS_PER_DEV];
	unsigned long bitmap[1];
};

/*
 * Per client for address space
 */
struct smmu_client {
	struct rb_node          node;
	struct device           *dev;
	struct list_head        list;
	struct smmu_domain      *domain;
	u64                     swgids;

	struct smmu_map_prop    *prop;

	struct dentry           *debugfs_root;
	struct dentry           *as_link[MAX_AS_PER_DEV];
};

/*
 * Per address space
 */
struct smmu_as {
	struct smmu_device      *smmu;  /* back pointer to container */
	unsigned int            asid;
	spinlock_t              lock;   /* for pagetable */
	struct page             *pdir_page;
	u32                     pdir_attr;
	u32                     pde_attr;
	u32                     pte_attr;
	unsigned int            *pte_count;

	struct list_head        client;
	spinlock_t              client_lock; /* for client list */

	void			*mempool_base;
	int			mempool_num_ent;
	int                     tegra_hv_comm_chan;
	spinlock_t              tegra_hv_comm_chan_lock;
	struct dentry           *debugfs_root;
};

struct smmu_debugfs_info {
	struct smmu_device *smmu;
	int mc;
	int cache;
	u64 val[2]; /* FIXME: per MC */
	struct timer_list stats_timer;
};

/*
 * Per SMMU device - IOMMU device
 */

/*
 * Per SMMU device - IOMMU device
 */
struct smmu_device {
	void __iomem    *regs, *regs_ahbarb;
	unsigned long   iovmm_base;     /* remappable base address */
	unsigned long   page_count;     /* total remappable size */
	spinlock_t      lock;
	spinlock_t      ptc_lock;
	char            *name;
	struct device   *dev;
	u64             swgids;         /* memory client ID bitmap */
	u32		ptc_cache_line;

	struct rb_root  clients;

	struct dentry *debugfs_root;
	struct smmu_debugfs_info *debugfs_info;
	struct dentry *masters_root;

	const struct tegra_smmu_chip_data *chip_data;
	struct list_head asprops;

	int     tegra_hv_comm_chan;
	int     tegra_hv_debug_chan;

	u32             num_as;
	struct smmu_as  as[0];          /* Run-time allocated array */
};

#define smmu_as_bitmap(domain)	(domain->as[__ffs(domain->bitmap[0])])

#define sg_num_pages(sg)	\
		(PAGE_ALIGN((sg)->offset + (sg)->length) >> PAGE_SHIFT)

struct smmu_as *domain_to_as(struct iommu_domain *_domain, unsigned long iova);
/* Override these functions */
extern int (*__smmu_client_set_hwgrp) (struct smmu_client *c, u64 map, int on);
extern struct smmu_as *(*smmu_as_alloc) (void);
extern void (*smmu_as_free) (struct smmu_domain *dom,
				unsigned long as_alloc_bitmap);
extern void (*smmu_domain_destroy) (struct smmu_device *smmu, struct smmu_as *as);
extern int (*__tegra_smmu_suspend) (struct device *dev);
extern int (*__tegra_smmu_resume) (struct device *dev);
extern int (*__tegra_smmu_probe)(struct platform_device *pdev,
						struct smmu_device *smmu);
extern struct iommu_ops *smmu_iommu_ops;
extern const struct file_operations *smmu_debugfs_stats_fops;

extern int (*__smmu_iommu_map_pfn)(struct smmu_as *as, dma_addr_t iova, unsigned long pfn, unsigned long prot);
extern int (*__smmu_iommu_map_largepage)(struct smmu_as *as, dma_addr_t iova, phys_addr_t pa, unsigned long prot);
extern size_t (*__smmu_iommu_unmap)(struct smmu_as *as, dma_addr_t iova, size_t bytes);
extern int (*__smmu_iommu_map_sg)(struct iommu_domain *domain, unsigned long iova, struct scatterlist *sgl, int npages, unsigned long prot);

extern void (*flush_ptc_and_tlb)(struct smmu_device *smmu, struct smmu_as *as, dma_addr_t iova, u32 *pte, struct page *page, int is_pde);
extern void (*flush_ptc_and_tlb_range)(struct smmu_device *smmu, struct smmu_as *as, dma_addr_t iova, u32 *pte, struct page *page, size_t count);
extern void (*flush_ptc_and_tlb_as)(struct smmu_as *as, dma_addr_t start, dma_addr_t end);
extern void (*free_pdir)(struct smmu_as *as);

#ifdef CONFIG_TEGRA_HV_MANAGER
extern int tegra_smmu_probe_hv(struct platform_device *pdev,
					struct smmu_device *smmu);
#else
static inline int tegra_smmu_probe_hv(struct platform_device *pdev,
	struct smmu_device *smmu)
{
	return -EINVAL;
}
#endif
#endif
