/*
 * arch/arm/mach-tegra/iovmm-smmu.c
 *
 * Tegra I/O VMM implementation for SMMU devices for Tegra 3 series
 * systems-on-a-chip.
 *
 * Copyright (c) 2010-2013, NVIDIA Corporation.
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/threads.h>
#include <asm/io.h>
#include <asm/cacheflush.h>
#include <asm/page.h>
#include <asm/cacheflush.h>
#include "tegra_smmu.h"
#include <mach/iovmm.h>
#include <mach/iomap.h>

/* For debugging */
/*#define SMMU_DEBUG*/

#ifdef SMMU_DEBUG
#define SMMU_VERBOSE 1
#else
#define SMMU_VERBOSE 0
#endif

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
/*
 * ALL-CAP macros copied from armc.h
 */
#define MC_SMMU_CONFIG_0				0x10
#define MC_SMMU_CONFIG_0_SMMU_ENABLE_DISABLE		0
#define MC_SMMU_CONFIG_0_SMMU_ENABLE_ENABLE		1

#define MC_SMMU_TLB_CONFIG_0				0x14
#define MC_SMMU_TLB_CONFIG_0_TLB_STATS__MASK		(1<<31)
#define MC_SMMU_TLB_CONFIG_0_TLB_STATS__ENABLE		(1<<31)
#define MC_SMMU_TLB_CONFIG_0_TLB_HIT_UNDER_MISS__ENABLE	(1<<29)
#define MC_SMMU_TLB_CONFIG_0_TLB_ACTIVE_LINES__VALUE	0x10
#define MC_SMMU_TLB_CONFIG_0_RESET_VAL			0x20000010

#define MC_SMMU_PTC_CONFIG_0				0x18
#define MC_SMMU_PTC_CONFIG_0_PTC_STATS__MASK		(1<<31)
#define MC_SMMU_PTC_CONFIG_0_PTC_STATS__ENABLE		(1<<31)
#define MC_SMMU_PTC_CONFIG_0_PTC_CACHE__ENABLE		(1<<29)
#define MC_SMMU_PTC_CONFIG_0_PTC_INDEX_MAP__PATTERN	0x3f
#define MC_SMMU_PTC_CONFIG_0_RESET_VAL			0x2000003f

#define MC_SMMU_PTB_ASID_0				0x1c
#define MC_SMMU_PTB_ASID_0_CURRENT_ASID_SHIFT		0

#define MC_SMMU_PTB_DATA_0				0x20
#define MC_SMMU_PTB_DATA_0_RESET_VAL			0
#define MC_SMMU_PTB_DATA_0_ASID_NONSECURE_SHIFT		29
#define MC_SMMU_PTB_DATA_0_ASID_WRITABLE_SHIFT		30
#define MC_SMMU_PTB_DATA_0_ASID_READABLE_SHIFT		31

#define MC_SMMU_TLB_FLUSH_0				0x30
#define MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA_MATCH_ALL		0
#define MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA_MATCH_SECTION		2
#define MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA_MATCH_GROUP		3
#define MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_SHIFT		29
#define MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_MATCH_DISABLE	0
#define MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_MATCH_ENABLE		1
#define MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_MATCH_SHIFT		31

#define MC_SMMU_PTC_FLUSH_0				0x34
#define MC_SMMU_PTC_FLUSH_0_PTC_FLUSH_TYPE_ALL		0
#define MC_SMMU_PTC_FLUSH_0_PTC_FLUSH_TYPE_ADR		1
#define MC_SMMU_PTC_FLUSH_0_PTC_FLUSH_ADR_SHIFT		4

#define MC_SMMU_ASID_SECURITY_0				0x38

#define MC_SMMU_STATS_TLB_HIT_COUNT_0			0x1f0
#define MC_SMMU_STATS_TLB_MISS_COUNT_0			0x1f4
#define MC_SMMU_STATS_PTC_HIT_COUNT_0			0x1f8
#define MC_SMMU_STATS_PTC_MISS_COUNT_0			0x1fc

#define MC_SMMU_TRANSLATION_ENABLE_0_0			0x228
#define MC_SMMU_TRANSLATION_ENABLE_1_0			0x22c
#define MC_SMMU_TRANSLATION_ENABLE_2_0			0x230

#define MC_SMMU_AFI_ASID_0              0x238   /* PCIE */
#define MC_SMMU_AVPC_ASID_0             0x23c   /* AVP */
#define MC_SMMU_DC_ASID_0               0x240   /* Display controller */
#define MC_SMMU_DCB_ASID_0              0x244   /* Display controller B */
#define MC_SMMU_EPP_ASID_0              0x248   /* Encoder pre-processor */
#define MC_SMMU_G2_ASID_0               0x24c   /* 2D engine */
#define MC_SMMU_HC_ASID_0               0x250   /* Host1x */
#define MC_SMMU_HDA_ASID_0              0x254   /* High-def audio */
#define MC_SMMU_ISP_ASID_0              0x258   /* Image signal processor */
#define MC_SMMU_MPE_ASID_0              0x264   /* MPEG encoder */
#define MC_SMMU_NV_ASID_0               0x268   /* (3D) */
#define MC_SMMU_NV2_ASID_0              0x26c   /* (3D) */
#define MC_SMMU_PPCS_ASID_0             0x270   /* AHB */
#define MC_SMMU_SATA_ASID_0             0x278   /* SATA */
#define MC_SMMU_VDE_ASID_0              0x27c   /* Video decoder */
#define MC_SMMU_VI_ASID_0               0x280   /* Video input */

#define SMMU_PDE_NEXT_SHIFT		28

/* Copied from arahb_arbc.h */
#define AHB_ARBITRATION_XBAR_CTRL_0     0xe0
#define AHB_ARBITRATION_XBAR_CTRL_0_SMMU_INIT_DONE_DONE		1
#define AHB_ARBITRATION_XBAR_CTRL_0_SMMU_INIT_DONE_SHIFT	17

#endif

#define MC_SMMU_NUM_ASIDS	4
#define MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA_SECTION__MASK		0xffc00000
#define MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA_SECTION__SHIFT	12 /* right shift */
#define MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA_GROUP__MASK		0xffffc000
#define MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA_GROUP__SHIFT	12 /* right shift */
#define MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA(iova, which)	\
	((((iova) & MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA_##which##__MASK) >> \
		MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA_##which##__SHIFT) |	\
	MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA_MATCH_##which)
#define MC_SMMU_PTB_ASID_0_CURRENT_ASID(n)	\
		((n) << MC_SMMU_PTB_ASID_0_CURRENT_ASID_SHIFT)
#define MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_MATCH_disable		\
		(MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_MATCH_DISABLE <<	\
			MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_MATCH_SHIFT)
#define MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_MATCH__ENABLE		\
		(MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_MATCH_ENABLE <<	\
			MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_MATCH_SHIFT)

#define VMM_NAME "iovmm-smmu"
#define DRIVER_NAME "tegra_smmu"

#define SMMU_PAGE_SHIFT 12
#define SMMU_PAGE_SIZE		(1 << SMMU_PAGE_SHIFT)

typedef pmd_t smmu_pde_t;
typedef pte_t smmu_pte_t;

#define SMMU_PDIR_COUNT	1024
#define SMMU_PDIR_SIZE	(sizeof(smmu_pde_t) * SMMU_PDIR_COUNT)
#define SMMU_PTBL_COUNT	1024
#define SMMU_PTBL_SIZE	(sizeof(smmu_pte_t) * SMMU_PTBL_COUNT)
#define SMMU_PDIR_SHIFT	12
#define SMMU_PDE_SHIFT	12
#define SMMU_PTE_SHIFT	12
#define SMMU_PFN_MASK	0x000fffff

#define SMMU_ADDR_TO_PFN(addr)	((addr)>>12)
#define SMMU_ADDR_TO_PDN(addr)	((addr)>>22)
#define SMMU_PDN_TO_ADDR(addr)	((pdn)<<22)

#define _READABLE	(1<<MC_SMMU_PTB_DATA_0_ASID_READABLE_SHIFT)
#define _WRITABLE	(1<<MC_SMMU_PTB_DATA_0_ASID_WRITABLE_SHIFT)
#define _NONSECURE	(1<<MC_SMMU_PTB_DATA_0_ASID_NONSECURE_SHIFT)
#define _PDE_NEXT	(1<<SMMU_PDE_NEXT_SHIFT)
#define _MASK_ATTR	(_READABLE|_WRITABLE|_NONSECURE)

#define _PDIR_ATTR	(_READABLE|_WRITABLE|_NONSECURE)

#define _PDE_ATTR	(_READABLE|_WRITABLE|_NONSECURE)
#define _PDE_ATTR_N	(_PDE_ATTR|_PDE_NEXT)
#define _PDE_VACANT(pdn)	(((pdn)<<10)|_PDE_ATTR)

#define _PTE_ATTR	(_READABLE|_WRITABLE|_NONSECURE)
#define _PTE_VACANT(addr)	(((addr)>>SMMU_PAGE_SHIFT)|_PTE_ATTR)

#define SMMU_MK_PDIR(page, attr)	\
		((page_to_phys(page)>>SMMU_PDIR_SHIFT)|(attr))
#define SMMU_MK_PDE(page, attr)		\
		(smmu_pde_t)((page_to_phys(page)>>SMMU_PDE_SHIFT)|(attr))
#define SMMU_EX_PTBL_PAGE(pde)		\
		pfn_to_page((unsigned long)(pde) & SMMU_PFN_MASK)
#define SMMU_PFN_TO_PTE(pfn, attr)	(smmu_pte_t)((pfn)|(attr))

#define SMMU_ASID_ENABLE(asid)	((asid)|(1<<31))
#define SMMU_ASID_DISABLE	0
#define SMMU_ASID_ASID(n)	((n)&~SMMU_ASID_ENABLE(0))

/* Keep this as a "natural" enumeration (no assignments) */
enum smmu_hwclient {
	HWC_AFI,
	HWC_AVPC,
	HWC_DC,
	HWC_DCB,
	HWC_EPP,
	HWC_G2,
	HWC_HC,
	HWC_HDA,
	HWC_ISP,
	HWC_MPE,
	HWC_NV,
	HWC_NV2,
	HWC_PPCS,
	HWC_SATA,
	HWC_VDE,
	HWC_VI,

	HWC_COUNT
};

struct smmu_hwc_state {
	unsigned long reg;
	unsigned long enable_disable;
};

/* Hardware client mapping initializer */
#define HWC_INIT(client)	\
	[HWC_##client] = {MC_SMMU_##client##_ASID_0, SMMU_ASID_DISABLE},

static const struct smmu_hwc_state smmu_hwc_state_init[] = {
	HWC_INIT(AFI)
	HWC_INIT(AVPC)
	HWC_INIT(DC)
	HWC_INIT(DCB)
	HWC_INIT(EPP)
	HWC_INIT(G2)
	HWC_INIT(HC)
	HWC_INIT(HDA)
	HWC_INIT(ISP)
	HWC_INIT(MPE)
	HWC_INIT(NV)
	HWC_INIT(NV2)
	HWC_INIT(PPCS)
	HWC_INIT(SATA)
	HWC_INIT(VDE)
	HWC_INIT(VI)
};


struct domain_hwc_map {
	const char *dev_name;
	const enum smmu_hwclient *hwcs;
	const unsigned int nr_hwcs;
};

/* Enable all hardware clients for SMMU translation */
static const enum smmu_hwclient nvmap_hwcs[] = {
	HWC_AFI,
	HWC_AVPC,
	HWC_DC,
	HWC_DCB,
	HWC_EPP,
	HWC_G2,
	HWC_HC,
	HWC_HDA,
	HWC_ISP,
	HWC_MPE,
	HWC_NV,
	HWC_NV2,
	HWC_PPCS,
	HWC_SATA,
	HWC_VDE,
	HWC_VI
};

static const struct domain_hwc_map smmu_hwc_map[] = {
	{
		.dev_name = "nvmap",
		.hwcs = nvmap_hwcs,
		.nr_hwcs = ARRAY_SIZE(nvmap_hwcs),
	},
};

/*
 * Per address space
 */
struct smmu_as {
	struct smmu_device	*smmu;	/* back pointer to container */
	unsigned int		asid;
	const struct domain_hwc_map	*hwclients;
	struct semaphore	sem;
	struct tegra_iovmm_domain domain;
	bool		needs_barrier;	/* emulator WAR */
	struct page	*pdir_page;
	unsigned long	pdir_attr;
	unsigned long	pde_attr;
	unsigned long	pte_attr;
	unsigned int	*pte_count;
	struct device	sysfs_dev;
	int		sysfs_use_count;
};

/*
 * Per SMMU device
 */
struct smmu_device {
	void __iomem	*regs, *regs_ahbarb;
	tegra_iovmm_addr_t	iovmm_base;	/* remappable base address */
	unsigned long	page_count;		/* total remappable size */
	spinlock_t	lock;
	char		*name;
	struct tegra_iovmm_device iovmm_dev;
	int		num_ases;
	struct smmu_as	*as;			/* Run-time allocated array */
	struct smmu_hwc_state	hwc_state[HWC_COUNT];
	struct device	sysfs_dev;
	int		sysfs_use_count;
	bool		enable;
	struct page *avp_vector_page;	/* dummy page shared by all AS's */

	/*
	 * Register image savers for suspend/resume
	 */
	unsigned long translation_enable_0_0;
	unsigned long translation_enable_1_0;
	unsigned long translation_enable_2_0;
	unsigned long asid_security_0;

	unsigned long lowest_asid;	/* Variables for hardware testing */
	unsigned long debug_asid;
	unsigned long verbose;
	unsigned long signature_pid;	/* For debugging aid */
};

#define VA_PAGE_TO_PA(va, page)	\
	(page_to_phys(page) + ((unsigned long)(va) & ~PAGE_MASK))

#define FLUSH_CPU_DCACHE(va, page, size)	\
	do {	\
		unsigned long _pa_ = VA_PAGE_TO_PA(va, page);		\
		__cpuc_flush_dcache_area((void *)(va), (size_t)(size));	\
		outer_flush_range(_pa_, _pa_+(size_t)(size));		\
		wmb();	\
	} while (0)

#define FLUSH_SMMU_REGS(smmu)	\
	do { wmb(); (void)readl((smmu)->regs + MC_SMMU_CONFIG_0); } while(0)

/*
 * Flush all TLB entries and all PTC entries
 * Caller must lock smmu
 */
static void smmu_flush_regs(struct smmu_device *smmu, int enable)
{
	writel(MC_SMMU_PTC_FLUSH_0_PTC_FLUSH_TYPE_ALL,
		smmu->regs + MC_SMMU_PTC_FLUSH_0);
	FLUSH_SMMU_REGS(smmu);
	writel(MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA_MATCH_ALL |
			MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_MATCH_disable,
		smmu->regs + MC_SMMU_TLB_FLUSH_0);

	if (enable)
		writel(MC_SMMU_CONFIG_0_SMMU_ENABLE_ENABLE,
			smmu->regs + MC_SMMU_CONFIG_0);
	FLUSH_SMMU_REGS(smmu);
}

static void smmu_setup_regs(struct smmu_device *smmu)
{
	int i;

	if (smmu->as) {
		int asid;

		/* Set/restore page directory for each AS */
		for (asid = 0; asid < smmu->num_ases; asid++) {
			struct smmu_as *as = &smmu->as[asid];

			writel(MC_SMMU_PTB_ASID_0_CURRENT_ASID(as->asid),
				as->smmu->regs + MC_SMMU_PTB_ASID_0);
			writel(as->pdir_page
				? SMMU_MK_PDIR(as->pdir_page, as->pdir_attr)
				: MC_SMMU_PTB_DATA_0_RESET_VAL,
				as->smmu->regs + MC_SMMU_PTB_DATA_0);
		}
	}

	/* Set/restore ASID for each hardware client */
	for (i = 0; i < HWC_COUNT; i++) {
		struct smmu_hwc_state *hwcst = &smmu->hwc_state[i];
		writel(hwcst->enable_disable, smmu->regs + hwcst->reg);
	}

	writel(smmu->translation_enable_0_0,
		smmu->regs + MC_SMMU_TRANSLATION_ENABLE_0_0);
	writel(smmu->translation_enable_1_0,
		smmu->regs + MC_SMMU_TRANSLATION_ENABLE_1_0);
	writel(smmu->translation_enable_2_0,
		smmu->regs + MC_SMMU_TRANSLATION_ENABLE_2_0);
	writel(smmu->asid_security_0,
		smmu->regs + MC_SMMU_ASID_SECURITY_0);
	writel(MC_SMMU_TLB_CONFIG_0_RESET_VAL,
		smmu->regs + MC_SMMU_TLB_CONFIG_0);
	writel(MC_SMMU_PTC_CONFIG_0_RESET_VAL,
		smmu->regs + MC_SMMU_PTC_CONFIG_0);

	smmu_flush_regs(smmu, 1);
	writel(
		readl(smmu->regs_ahbarb + AHB_ARBITRATION_XBAR_CTRL_0) |
		(AHB_ARBITRATION_XBAR_CTRL_0_SMMU_INIT_DONE_DONE <<
			AHB_ARBITRATION_XBAR_CTRL_0_SMMU_INIT_DONE_SHIFT),
		smmu->regs_ahbarb + AHB_ARBITRATION_XBAR_CTRL_0);
}

static int smmu_suspend(struct tegra_iovmm_device *dev)
{
	struct smmu_device *smmu =
		container_of(dev, struct smmu_device, iovmm_dev);

	smmu->translation_enable_0_0 =
		readl(smmu->regs + MC_SMMU_TRANSLATION_ENABLE_0_0);
	smmu->translation_enable_1_0 =
		readl(smmu->regs + MC_SMMU_TRANSLATION_ENABLE_1_0);
	smmu->translation_enable_2_0 =
		readl(smmu->regs + MC_SMMU_TRANSLATION_ENABLE_2_0);
	smmu->asid_security_0 =
		readl(smmu->regs + MC_SMMU_ASID_SECURITY_0);
	return 0;
}

static void smmu_resume(struct tegra_iovmm_device *dev)
{
	struct smmu_device *smmu =
		container_of(dev, struct smmu_device, iovmm_dev);

	if (!smmu->enable)
		return;

	spin_lock(&smmu->lock);
	smmu_setup_regs(smmu);
	spin_unlock(&smmu->lock);
}

static void flush_ptc_and_tlb(struct smmu_device *smmu,
		struct smmu_as *as, unsigned long iova,
		pte_t *pte, struct page *ptpage, int is_pde)
{
	unsigned long tlb_flush_va = is_pde
			?  MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA(iova, SECTION)
			:  MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA(iova, GROUP);

	writel(MC_SMMU_PTC_FLUSH_0_PTC_FLUSH_TYPE_ADR |
		VA_PAGE_TO_PA(pte, ptpage),
		smmu->regs + MC_SMMU_PTC_FLUSH_0);
	FLUSH_SMMU_REGS(smmu);
	writel(tlb_flush_va |
		MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_MATCH__ENABLE |
		(as->asid << MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_SHIFT),
		smmu->regs + MC_SMMU_TLB_FLUSH_0);
	FLUSH_SMMU_REGS(smmu);
}

static void free_ptbl(struct smmu_as *as, unsigned long iova)
{
	unsigned long pdn = SMMU_ADDR_TO_PDN(iova);
	smmu_pde_t *pdir = kmap(as->pdir_page);

	if (pdir[pdn] != _PDE_VACANT(pdn)) {
		if (as->smmu->verbose)
			printk("%s:%d pdn=%lx\n", __func__, __LINE__, pdn);
		ClearPageReserved(SMMU_EX_PTBL_PAGE(pdir[pdn]));
		__free_page(SMMU_EX_PTBL_PAGE(pdir[pdn]));
		pdir[pdn] = _PDE_VACANT(pdn);
		FLUSH_CPU_DCACHE(&pdir[pdn], as->pdir_page, sizeof pdir[pdn]);
		flush_ptc_and_tlb(as->smmu, as, iova, &pdir[pdn],
				as->pdir_page, 1);
	}
	kunmap(as->pdir_page);
}

static void free_pdir(struct smmu_as *as)
{
	if (as->pdir_page) {
		unsigned addr = as->smmu->iovmm_base;
		int count = as->smmu->page_count;

		while (count-- > 0) {
			free_ptbl(as, addr);
			addr += SMMU_PAGE_SIZE * SMMU_PTBL_COUNT;
		}
		ClearPageReserved(as->pdir_page);
		__free_page(as->pdir_page);
		as->pdir_page = NULL;
		kfree(as->pte_count);
		as->pte_count = NULL;
	}
}

static int smmu_remove(struct platform_device *pdev)
{
	struct smmu_device *smmu = platform_get_drvdata(pdev);

	if (!smmu)
		return 0;

	if (smmu->enable) {
		writel(MC_SMMU_CONFIG_0_SMMU_ENABLE_DISABLE,
			smmu->regs + MC_SMMU_CONFIG_0);
		smmu->enable = 0;
	}
	platform_set_drvdata(pdev, NULL);

	if (smmu->as) {
		int asid;

		for (asid = 0; asid < smmu->num_ases; asid++)
			free_pdir(&smmu->as[asid]);
		kfree(smmu->as);
	}

	if (smmu->avp_vector_page)
		__free_page(smmu->avp_vector_page);
	if (smmu->regs)
		iounmap(smmu->regs);
	if (smmu->regs_ahbarb)
		iounmap(smmu->regs_ahbarb);
	tegra_iovmm_unregister(&smmu->iovmm_dev);
	kfree(smmu);
	return 0;
}

/*
 * Maps PTBL for given iova and returns the PTE address
 * Caller must unmap the mapped PTBL returned in *ptbl_page_p
 */
static smmu_pte_t *locate_pte(struct smmu_as *as,
		unsigned long iova, bool allocate,
		struct page **ptbl_page_p,
		unsigned int **pte_counter)
{
	unsigned long ptn = SMMU_ADDR_TO_PFN(iova);
	unsigned long pdn = SMMU_ADDR_TO_PDN(iova);
	smmu_pde_t *pdir = kmap(as->pdir_page);
	smmu_pte_t *ptbl;

	if (pdir[pdn] != _PDE_VACANT(pdn)) {
		/* Mapped entry table already exists */
		*ptbl_page_p = SMMU_EX_PTBL_PAGE(pdir[pdn]);
		ptbl = kmap(*ptbl_page_p);
	} else if (!allocate) {
		kunmap(as->pdir_page);
		return NULL;
	} else {
		/* Vacant - allocate a new page table */
		if (as->smmu->verbose)
			printk("%s:%d new PTBL pdn=%lx\n", __func__, __LINE__, pdn);
		*ptbl_page_p = alloc_page(GFP_KERNEL|__GFP_DMA);
		if (!*ptbl_page_p) {
			kunmap(as->pdir_page);
			pr_err(DRIVER_NAME
			": failed to allocate tegra_iovmm_device page table\n");
			return NULL;
		}
		SetPageReserved(*ptbl_page_p);
		ptbl = kmap(*ptbl_page_p);
		{
			int pn;
			unsigned long addr = SMMU_PDN_TO_ADDR(pdn);
			for (pn = 0; pn < SMMU_PTBL_COUNT;
				pn++, addr += SMMU_PAGE_SIZE) {
				ptbl[pn] = _PTE_VACANT(addr);
			}
		}
		FLUSH_CPU_DCACHE(ptbl, *ptbl_page_p, SMMU_PTBL_SIZE);
		pdir[pdn] = SMMU_MK_PDE(*ptbl_page_p,
				as->pde_attr|_PDE_NEXT);
		FLUSH_CPU_DCACHE(&pdir[pdn], as->pdir_page, sizeof pdir[pdn]);
		flush_ptc_and_tlb(as->smmu, as, iova, &pdir[pdn],
				as->pdir_page, 1);
	}
	*pte_counter = &as->pte_count[pdn];

	kunmap(as->pdir_page);
	return &ptbl[ptn % SMMU_PTBL_COUNT];
}

static void put_signature(struct smmu_as *as,
			unsigned long addr, unsigned long pfn)
{
	if (as->smmu->signature_pid == current->pid) {
		struct page *page = pfn_to_page(pfn);
		unsigned long *vaddr = kmap(page);
		if (vaddr) {
			vaddr[0] = addr;
			vaddr[1] = pfn << PAGE_SHIFT;
			FLUSH_CPU_DCACHE(vaddr, page, sizeof(vaddr[0]) * 2);
			kunmap(page);
		}
	}
}

static int smmu_map(struct tegra_iovmm_domain *domain,
		struct tegra_iovmm_area *iovma)
{
	struct smmu_as *as = container_of(domain, struct smmu_as, domain);
	unsigned long addr = iovma->iovm_start;
	unsigned long pcount = iovma->iovm_length >> SMMU_PAGE_SHIFT;
	int i;

	if (as->smmu->verbose)
		printk("%s:%d iova=%lx asid=%d\n", __func__, __LINE__,
			addr, as - as->smmu->as);

	for (i = 0; i < pcount; i++) {
		unsigned long pfn;
		smmu_pte_t *pte;
		unsigned int *pte_counter;
		struct page *ptpage;

		pfn = iovma->ops->lock_makeresident(iovma, i<<PAGE_SHIFT);
		if (!pfn_valid(pfn))
			goto fail;

		down(&as->sem);

		if (!(pte = locate_pte(as, addr, true, &ptpage, &pte_counter)))
			goto fail2;

		if (as->smmu->verbose)
			printk("%s:%d iova=%lx pfn=%lx asid=%d\n",
				__func__, __LINE__,
				addr, pfn, as - as->smmu->as);

		if (*pte == _PTE_VACANT(addr))
			(*pte_counter)++;
		*pte = SMMU_PFN_TO_PTE(pfn, as->pte_attr);
		if (unlikely((*pte == _PTE_VACANT(addr))))
			(*pte_counter)--;
		FLUSH_CPU_DCACHE(pte, ptpage, sizeof *pte);
		flush_ptc_and_tlb(as->smmu, as, addr, pte, ptpage, 0);
		kunmap(ptpage);
		up(&as->sem);
		put_signature(as, addr, pfn);
		addr += SMMU_PAGE_SIZE;
	}
	return 0;

fail:
	down(&as->sem);
fail2:

	while (i-- > 0) {
		smmu_pte_t *pte;
		unsigned int *pte_counter;
		struct page *page;

		iovma->ops->release(iovma, i<<PAGE_SHIFT);
		addr -= SMMU_PAGE_SIZE;
		if ((pte = locate_pte(as, addr, false, &page, &pte_counter))) {
			if (*pte != _PTE_VACANT(addr)) {
				*pte = _PTE_VACANT(addr);
				FLUSH_CPU_DCACHE(pte, page, sizeof *pte);
				flush_ptc_and_tlb(as->smmu, as, addr, pte,
						page, 0);
				kunmap(page);
				if (!--(*pte_counter))
					free_ptbl(as, addr);
			} else {
				kunmap(page);
			}
		}
	}
	up(&as->sem);
	return -ENOMEM;
}

static void smmu_unmap(struct tegra_iovmm_domain *domain,
	struct tegra_iovmm_area *iovma, bool decommit)
{
	struct smmu_as *as = container_of(domain, struct smmu_as, domain);
	unsigned long addr = iovma->iovm_start;
	unsigned int pcount = iovma->iovm_length >> SMMU_PAGE_SHIFT;
	unsigned int i, *pte_counter;

	if (as->smmu->verbose)
		printk("%s:%d iova=%lx asid=%d\n", __func__, __LINE__,
			addr, as - as->smmu->as);
	down(&as->sem);
	for (i = 0; i < pcount; i++) {
		smmu_pte_t *pte;
		struct page *page;

		if (iovma->ops && iovma->ops->release)
			iovma->ops->release(iovma, i<<PAGE_SHIFT);

		if ((pte = locate_pte(as, addr, false, &page, &pte_counter))) {
			if (*pte != _PTE_VACANT(addr)) {
				*pte = _PTE_VACANT(addr);
				FLUSH_CPU_DCACHE(pte, page, sizeof *pte);
				flush_ptc_and_tlb(as->smmu, as, addr, pte,
						page, 0);
				kunmap(page);
				if (!--(*pte_counter) && decommit) {
					free_ptbl(as, addr);
					smmu_flush_regs(as->smmu, 0);
				}
			}
		}
		addr += SMMU_PAGE_SIZE;
	}
	up(&as->sem);
}

static void smmu_map_pfn(struct tegra_iovmm_domain *domain,
	struct tegra_iovmm_area *iovma, tegra_iovmm_addr_t addr,
	unsigned long pfn)
{
	struct smmu_as *as = container_of(domain, struct smmu_as, domain);
	struct smmu_device *smmu = as->smmu;
	smmu_pte_t *pte;
	unsigned int *pte_counter;
	struct page *ptpage;

	if (smmu->verbose)
		printk("%s:%d iova=%lx pfn=%lx asid=%d\n", __func__, __LINE__,
			(unsigned long)addr, pfn, as - as->smmu->as);

	BUG_ON(!pfn_valid(pfn));
	down(&as->sem);
	if ((pte = locate_pte(as, addr, true, &ptpage, &pte_counter))) {
		if (*pte == _PTE_VACANT(addr))
			(*pte_counter)++;
		*pte = SMMU_PFN_TO_PTE(pfn, as->pte_attr);
		if (unlikely((*pte == _PTE_VACANT(addr))))
			(*pte_counter)--;
		FLUSH_CPU_DCACHE(pte, ptpage, sizeof *pte);
		flush_ptc_and_tlb(smmu, as, addr, pte, ptpage, 0);
		kunmap(ptpage);
		put_signature(as, addr, pfn);
	}
	up(&as->sem);
}

/*
 * Caller must lock/unlock as
 */
static int alloc_pdir(struct smmu_as *as)
{
	unsigned long *pdir;
	int pdn;

	if (as->pdir_page)
		return 0;

	as->pte_count = kzalloc(sizeof(as->pte_count[0]) * SMMU_PDIR_COUNT,
				GFP_KERNEL);
	if (!as->pte_count) {
		pr_err(DRIVER_NAME
		": failed to allocate tegra_iovmm_device PTE cunters\n");
		return -ENOMEM;
	}
	as->pdir_page = alloc_page(GFP_KERNEL|__GFP_DMA);
	if (!as->pdir_page) {
		pr_err(DRIVER_NAME
		": failed to allocate tegra_iovmm_device page directory\n");
		kfree(as->pte_count);
		as->pte_count = NULL;
		return -ENOMEM;
	}
	SetPageReserved(as->pdir_page);
	pdir = kmap(as->pdir_page);

	for (pdn = 0; pdn < SMMU_PDIR_COUNT; pdn++)
		pdir[pdn] = _PDE_VACANT(pdn);
	FLUSH_CPU_DCACHE(pdir, as->pdir_page, SMMU_PDIR_SIZE);
	writel(MC_SMMU_PTC_FLUSH_0_PTC_FLUSH_TYPE_ADR |
		VA_PAGE_TO_PA(pdir, as->pdir_page),
		as->smmu->regs + MC_SMMU_PTC_FLUSH_0);
	FLUSH_SMMU_REGS(as->smmu);
	writel(MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_VA_MATCH_ALL |
		MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_MATCH__ENABLE |
		(as->asid << MC_SMMU_TLB_FLUSH_0_TLB_FLUSH_ASID_SHIFT),
		as->smmu->regs + MC_SMMU_TLB_FLUSH_0);
	FLUSH_SMMU_REGS(as->smmu);
	kunmap(as->pdir_page);

	return 0;
}

static void _sysfs_create(struct smmu_as *as, struct device *sysfs_parent);

/*
 * Allocate resources for an AS
 *	TODO: split into "alloc" and "lock"
 */
static struct tegra_iovmm_domain *smmu_alloc_domain(
	struct tegra_iovmm_device *dev, struct tegra_iovmm_client *client)
{
	struct smmu_device *smmu =
		container_of(dev, struct smmu_device, iovmm_dev);
	struct smmu_as *as = NULL;
	const struct domain_hwc_map *map = NULL;
	int asid, i;

	/* Look for a free AS */
	for  (asid = smmu->lowest_asid; asid < smmu->num_ases; asid++) {
		down(&smmu->as[asid].sem);
		if (!smmu->as[asid].hwclients) {
			as = &smmu->as[asid];
			break;
		}
		up(&smmu->as[asid].sem);
	}

	if (!as) {
		pr_err(DRIVER_NAME ": no free AS\n");
		return NULL;
	}

	if (alloc_pdir(as) < 0)
		goto bad3;

	/* Look for a matching hardware client group */
	for (i = 0; ARRAY_SIZE(smmu_hwc_map); i++) {
		if (!strcmp(smmu_hwc_map[i].dev_name, client->misc_dev->name)) {
			map = &smmu_hwc_map[i];
			break;
		}
	}

	if (!map) {
		pr_err(DRIVER_NAME ": no SMMU resource for %s (%s)\n",
			client->name, client->misc_dev->name);
		goto bad2;
	}

	spin_lock(&smmu->lock);
	/* Update PDIR register */
	writel(MC_SMMU_PTB_ASID_0_CURRENT_ASID(as->asid),
		as->smmu->regs + MC_SMMU_PTB_ASID_0);
	writel(SMMU_MK_PDIR(as->pdir_page, as->pdir_attr),
		as->smmu->regs + MC_SMMU_PTB_DATA_0);
	FLUSH_SMMU_REGS(smmu);

	/* Put each hardware client in the group into the address space */
	for (i = 0; i < map->nr_hwcs; i++) {
		struct smmu_hwc_state *hwcst = &smmu->hwc_state[map->hwcs[i]];

		/* Is the hardware client busy? */
		if (hwcst->enable_disable != SMMU_ASID_DISABLE &&
			hwcst->enable_disable != SMMU_ASID_ENABLE(as->asid)) {
			pr_err(DRIVER_NAME
				": HW 0x%lx busy for ASID %ld (client!=%s)\n",
				hwcst->reg,
				SMMU_ASID_ASID(hwcst->enable_disable),
				client->name);
			goto bad;
		}
		hwcst->enable_disable = SMMU_ASID_ENABLE(as->asid);
		writel(hwcst->enable_disable, smmu->regs + hwcst->reg);
	}
	FLUSH_SMMU_REGS(smmu);
	spin_unlock(&smmu->lock);
	as->hwclients = map;
	_sysfs_create(as, client->misc_dev->this_device);
	up(&as->sem);

	/* Reserve "page zero" for AVP vectors using a common dummy page */
	smmu_map_pfn(&as->domain, NULL, 0,
		page_to_phys(as->smmu->avp_vector_page)>>SMMU_PAGE_SHIFT);
	return &as->domain;

bad:
	/* Reset hardware clients that have been enabled */
	while (--i >= 0) {
		struct smmu_hwc_state *hwcst = &smmu->hwc_state[map->hwcs[i]];

		hwcst->enable_disable = SMMU_ASID_DISABLE;
		writel(hwcst->enable_disable, smmu->regs + hwcst->reg);
	}
	FLUSH_SMMU_REGS(smmu);
	spin_unlock(&as->smmu->lock);
bad2:
	free_pdir(as);
bad3:
	up(&as->sem);
	return NULL;

}

/*
 * Release resources for an AS
 *	TODO: split into "unlock" and "free"
 */
static void smmu_free_domain(
	struct tegra_iovmm_domain *domain, struct tegra_iovmm_client *client)
{
	struct smmu_as *as = container_of(domain, struct smmu_as, domain);
	struct smmu_device *smmu = as->smmu;
	const struct domain_hwc_map *map = NULL;
	int i;

	down(&as->sem);
	map = as->hwclients;

	spin_lock(&smmu->lock);
	for (i = 0; i < map->nr_hwcs; i++) {
		struct smmu_hwc_state *hwcst = &smmu->hwc_state[map->hwcs[i]];

		hwcst->enable_disable = SMMU_ASID_DISABLE;
		writel(SMMU_ASID_DISABLE, smmu->regs + hwcst->reg);
	}
	FLUSH_SMMU_REGS(smmu);
	spin_unlock(&smmu->lock);

	as->hwclients = NULL;
	if (as->pdir_page) {
		spin_lock(&smmu->lock);
		writel(MC_SMMU_PTB_ASID_0_CURRENT_ASID(as->asid),
			smmu->regs + MC_SMMU_PTB_ASID_0);
		writel(MC_SMMU_PTB_DATA_0_RESET_VAL,
			smmu->regs + MC_SMMU_PTB_DATA_0);
		FLUSH_SMMU_REGS(smmu);
		spin_unlock(&smmu->lock);

		free_pdir(as);
	}
	up(&as->sem);
}

static struct tegra_iovmm_device_ops tegra_iovmm_smmu_ops = {
	.map = smmu_map,
	.unmap = smmu_unmap,
	.map_pfn = smmu_map_pfn,
	.alloc_domain = smmu_alloc_domain,
	.free_domain = smmu_free_domain,
	.suspend = smmu_suspend,
	.resume = smmu_resume,
};

static int smmu_probe(struct platform_device *pdev)
{
	struct smmu_device *smmu = NULL;
	struct resource *regs = NULL, *regs2 = NULL;
	struct tegra_smmu_window *window = NULL;
	int e, asid;

	if (!pdev) {
		pr_err(DRIVER_NAME ": platform_device required\n");
		return -ENODEV;
	}

	if (PAGE_SHIFT != SMMU_PAGE_SHIFT) {
		pr_err(DRIVER_NAME ": SMMU and CPU page sizes must match\n");
		return -ENXIO;
	}

	if (ARRAY_SIZE(smmu_hwc_state_init) != HWC_COUNT) {
		pr_err(DRIVER_NAME
			": sizeof smmu_hwc_state_init != enum smmu_hwclient\n");
		return -ENXIO;
	}

	regs = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mc");
	regs2 = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ahbarb");
	window = tegra_smmu_window(0);

	if (!regs || !regs2 || !window) {
		pr_err(DRIVER_NAME ": No SMMU resources\n");
		return -ENODEV;
	}
	smmu = kzalloc(sizeof(*smmu), GFP_KERNEL);
	if (!smmu) {
		pr_err(DRIVER_NAME ": failed to allocate smmu_device\n");
		return -ENOMEM;
	}

	smmu->num_ases = MC_SMMU_NUM_ASIDS;
	smmu->iovmm_base = (tegra_iovmm_addr_t)window->start;
	smmu->page_count = (window->end + 1 - window->start) >> SMMU_PAGE_SHIFT;
	smmu->regs = ioremap(regs->start, resource_size(regs));
	smmu->regs_ahbarb = ioremap(regs2->start, resource_size(regs2));
	if (!smmu->regs || !smmu->regs_ahbarb) {
		pr_err(DRIVER_NAME ": failed to remap SMMU registers\n");
		e = -ENXIO;
		goto fail;
	}

	smmu->translation_enable_0_0 = ~0;
	smmu->translation_enable_1_0 = ~0;
	smmu->translation_enable_2_0 = ~0;
	smmu->asid_security_0        = 0;
	smmu->verbose = SMMU_VERBOSE;

	memcpy(smmu->hwc_state, smmu_hwc_state_init, sizeof(smmu->hwc_state));

	smmu->iovmm_dev.name = VMM_NAME;
	smmu->iovmm_dev.ops = &tegra_iovmm_smmu_ops;
	smmu->iovmm_dev.pgsize_bits = SMMU_PAGE_SHIFT;
#ifdef CONFIG_TEGRA_ERRATA_1053704
	smmu->iovmm_dev.align = 14; /* SZ_16K */
#else
	smmu->iovmm_dev.align = SMMU_PAGE_SHIFT;
#endif

	e = tegra_iovmm_register(&smmu->iovmm_dev);
	if (WARN_ON(e))
		goto fail;

	smmu->as = kzalloc(sizeof(smmu->as[0]) * smmu->num_ases, GFP_KERNEL);
	if (!smmu->as) {
		pr_err(DRIVER_NAME ": failed to allocate smmu_as\n");
		e = -ENOMEM;
		goto fail;
	}

	/* Initialize address space structure array */
	for (asid = 0; asid < smmu->num_ases; asid++) {
		struct smmu_as *as = &smmu->as[asid];

		as->smmu = smmu;
		as->asid = asid;
		as->pdir_attr = _PDIR_ATTR;
		as->pde_attr  = _PDE_ATTR;
		as->pte_attr  = _PTE_ATTR;

		sema_init(&as->sem, 1);

		e = tegra_iovmm_domain_init(&as->domain, &smmu->iovmm_dev,
			smmu->iovmm_base,
			smmu->iovmm_base +
				(smmu->page_count << SMMU_PAGE_SHIFT));
		if (WARN_ON(e))
			goto fail;
	}
	spin_lock_init(&smmu->lock);
	smmu_setup_regs(smmu);
	smmu->enable = 1;
	platform_set_drvdata(pdev, smmu);

	smmu->avp_vector_page = alloc_page(GFP_KERNEL);
	if (WARN_ON(!smmu->avp_vector_page))
		goto fail;

	dev_info(&pdev->dev, "initialised smmu\n");
	return 0;

fail:
	if (smmu->avp_vector_page)
		__free_page(smmu->avp_vector_page);
	if (smmu->regs)
		iounmap(smmu->regs);
	if (smmu->regs_ahbarb)
		iounmap(smmu->regs_ahbarb);
	if (smmu && smmu->as) {
		for (asid = 0; asid < smmu->num_ases; asid++) {
			if (smmu->as[asid].pdir_page) {
				ClearPageReserved(smmu->as[asid].pdir_page);
				__free_page(smmu->as[asid].pdir_page);
			}
		}
		kfree(smmu->as);
	}
	kfree(smmu);
	dev_err(&pdev->dev, "failed to initialise\n");
	return e;
}

static struct platform_driver tegra_iovmm_smmu_drv = {
	.probe = smmu_probe,
	.remove = smmu_remove,
	.driver = {
		.name = DRIVER_NAME,
	},
};

static int __devinit smmu_init(void)
{
	return platform_driver_register(&tegra_iovmm_smmu_drv);
}

static void __exit smmu_exit(void)
{
	return platform_driver_unregister(&tegra_iovmm_smmu_drv);
}

subsys_initcall(smmu_init);
module_exit(smmu_exit);

/*
 * SMMU-global sysfs interface for debugging
 */
static ssize_t _sysfs_show_reg(struct device *d,
				struct device_attribute *da, char *buf);
static ssize_t _sysfs_store_reg(struct device *d,
				struct device_attribute *da, const char *buf,
				size_t count);

#define _NAME_MAP(_name)	{	\
	.name = __stringify(_name),	\
	.offset = _name##_0,		\
	.dev_attr = __ATTR(_name, S_IRUGO|S_IWUSR,	\
			_sysfs_show_reg, _sysfs_store_reg)	\
}

static
struct _reg_name_map {
	const char *name;
	unsigned	offset;
	struct device_attribute	dev_attr;
} _smmu_reg_name_map[] = {
	_NAME_MAP(MC_SMMU_CONFIG),
	_NAME_MAP(MC_SMMU_TLB_CONFIG),
	_NAME_MAP(MC_SMMU_PTC_CONFIG),
	_NAME_MAP(MC_SMMU_PTB_ASID),
	_NAME_MAP(MC_SMMU_PTB_DATA),
	_NAME_MAP(MC_SMMU_TLB_FLUSH),
	_NAME_MAP(MC_SMMU_PTC_FLUSH),
	_NAME_MAP(MC_SMMU_ASID_SECURITY),
	_NAME_MAP(MC_SMMU_STATS_TLB_HIT_COUNT),
	_NAME_MAP(MC_SMMU_STATS_TLB_MISS_COUNT),
	_NAME_MAP(MC_SMMU_STATS_PTC_HIT_COUNT),
	_NAME_MAP(MC_SMMU_STATS_PTC_MISS_COUNT),
	_NAME_MAP(MC_SMMU_TRANSLATION_ENABLE_0),
	_NAME_MAP(MC_SMMU_TRANSLATION_ENABLE_1),
	_NAME_MAP(MC_SMMU_TRANSLATION_ENABLE_2),
	_NAME_MAP(MC_SMMU_AFI_ASID),
	_NAME_MAP(MC_SMMU_AVPC_ASID),
	_NAME_MAP(MC_SMMU_DC_ASID),
	_NAME_MAP(MC_SMMU_DCB_ASID),
	_NAME_MAP(MC_SMMU_EPP_ASID),
	_NAME_MAP(MC_SMMU_G2_ASID),
	_NAME_MAP(MC_SMMU_HC_ASID),
	_NAME_MAP(MC_SMMU_HDA_ASID),
	_NAME_MAP(MC_SMMU_ISP_ASID),
	_NAME_MAP(MC_SMMU_MPE_ASID),
	_NAME_MAP(MC_SMMU_NV_ASID),
	_NAME_MAP(MC_SMMU_NV2_ASID),
	_NAME_MAP(MC_SMMU_PPCS_ASID),
	_NAME_MAP(MC_SMMU_SATA_ASID),
	_NAME_MAP(MC_SMMU_VDE_ASID),
	_NAME_MAP(MC_SMMU_VI_ASID),
};

static ssize_t lookup_reg(struct device_attribute *da)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(_smmu_reg_name_map); i++) {
		if (!strcmp(_smmu_reg_name_map[i].name, da->attr.name))
			return _smmu_reg_name_map[i].offset;
	}
	return -ENODEV;
}

static ssize_t _sysfs_show_reg(struct device *d,
					struct device_attribute *da, char *buf)
{
	struct smmu_device *smmu =
		container_of(d, struct smmu_device, sysfs_dev);
	ssize_t offset = lookup_reg(da);

	if (offset < 0)
		return offset;
	return sprintf(buf, "%08lx\n",
		(unsigned long)readl(smmu->regs + offset));
}

static ssize_t _sysfs_store_reg(struct device *d,
			struct device_attribute *da,
			const char *buf, size_t count)
{
	struct smmu_device *smmu =
		container_of(d, struct smmu_device, sysfs_dev);
	ssize_t offset = lookup_reg(da);
	unsigned long value;

	if (offset < 0)
		return offset;
	value = simple_strtoul(buf, NULL, 16);
#ifdef CONFIG_TEGRA_SMMU_SYSFS
	writel(value, smmu->regs + offset);
#else
	/* Allow writing to reg only for TLB/PTC stats enabling/disabling */
	{
		unsigned long mask = 0;
		switch (offset)
		{
		case MC_SMMU_TLB_CONFIG_0:
			mask = MC_SMMU_TLB_CONFIG_0_TLB_STATS__MASK;
			break;
		case MC_SMMU_PTC_CONFIG_0:
			mask = MC_SMMU_PTC_CONFIG_0_PTC_STATS__MASK;
			break;
		default:
			break;
		}

		if (mask) {
			unsigned long currval = readl(smmu->regs + offset);
			currval &= ~mask;
			value &= mask;
			value |= currval;
			writel(value, smmu->regs + offset);
		}
	}
#endif
	return count;
}

static ssize_t _sysfs_show_smmu(struct device *d,
				struct device_attribute *da, char *buf)
{
	struct smmu_device *smmu =
		container_of(d, struct smmu_device, sysfs_dev);
	ssize_t	rv = 0;

	rv += sprintf(buf + rv , "      regs: %p\n", smmu->regs);
	rv += sprintf(buf + rv , "iovmm_base: %p\n", (void *)smmu->iovmm_base);
	rv += sprintf(buf + rv , "page_count: %lx\n", smmu->page_count);
	rv += sprintf(buf + rv , "  num_ases: %d\n", smmu->num_ases);
	rv += sprintf(buf + rv , "        as: %p\n", smmu->as);
	rv += sprintf(buf + rv , "    enable: %s\n",
			smmu->enable ? "yes" : "no");
	return rv;
}

static struct device_attribute _attr_show_smmu
		 = __ATTR(show_smmu, S_IRUGO, _sysfs_show_smmu, NULL);

#define _SYSFS_SHOW_VALUE(name, field, fmt)		\
static ssize_t _sysfs_show_##name(struct device *d,	\
	struct device_attribute *da, char *buf)		\
{							\
	struct smmu_device *smmu =			\
		container_of(d, struct smmu_device, sysfs_dev);	\
	ssize_t rv = 0;					\
	rv += sprintf(buf + rv, fmt "\n", smmu->field);	\
	return rv;					\
}

static void (*_sysfs_null_callback)(struct smmu_device *, unsigned long *) =
	NULL;

#define _SYSFS_SET_VALUE_DO(name, field, base, ceil, callback)	\
static ssize_t _sysfs_set_##name(struct device *d,		\
		struct device_attribute *da, const char *buf, size_t count) \
{								\
	struct smmu_device *smmu =				\
		container_of(d, struct smmu_device, sysfs_dev);	\
	unsigned long value = simple_strtoul(buf, NULL, base);	\
	if (0 <= value && value < ceil) {			\
		smmu->field = value;				\
		if (callback)					\
			callback(smmu, &smmu->field);		\
	}							\
	return count;						\
}
#ifdef CONFIG_TEGRA_SMMU_SYSFS
#define _SYSFS_SET_VALUE	_SYSFS_SET_VALUE_DO
#else
#define _SYSFS_SET_VALUE(name, field, base, ceil, callback)	\
static ssize_t _sysfs_set_##name(struct device *d,		\
		struct device_attribute *da, const char *buf, size_t count) \
{								\
	return count;						\
}
#endif

_SYSFS_SHOW_VALUE(lowest_asid, lowest_asid, "%lu")
_SYSFS_SET_VALUE(lowest_asid, lowest_asid, 10,
		MC_SMMU_NUM_ASIDS, _sysfs_null_callback)
_SYSFS_SHOW_VALUE(debug_asid, debug_asid, "%lu")
_SYSFS_SET_VALUE(debug_asid, debug_asid, 10,
		MC_SMMU_NUM_ASIDS, _sysfs_null_callback)
_SYSFS_SHOW_VALUE(verbose, verbose, "%lu")
_SYSFS_SET_VALUE(verbose, verbose, 10, 3, _sysfs_null_callback)
_SYSFS_SHOW_VALUE(signature_pid, signature_pid, "%lu")
_SYSFS_SET_VALUE_DO(signature_pid, signature_pid, 10, PID_MAX_LIMIT+1,
		_sysfs_null_callback)

#ifdef CONFIG_TEGRA_SMMU_SYSFS
static void _sysfs_mask_attr(struct smmu_device *smmu, unsigned long *field)
{
	*field &= _MASK_ATTR;
}

static void _sysfs_mask_pdir_attr(struct smmu_device *smmu,
	unsigned long *field)
{
	unsigned long pdir;

	_sysfs_mask_attr(smmu, field);
	writel(MC_SMMU_PTB_ASID_0_CURRENT_ASID(smmu->debug_asid),
		smmu->regs + MC_SMMU_PTB_ASID_0);
	pdir = readl(smmu->regs + MC_SMMU_PTB_DATA_0);
	pdir &= ~_MASK_ATTR;
	pdir |= *field;
	writel(pdir, smmu->regs + MC_SMMU_PTB_DATA_0);
	FLUSH_SMMU_REGS(smmu);
}

static void (*_sysfs_mask_attr_callback)(struct smmu_device *,
				unsigned long *field) = &_sysfs_mask_attr;
static void (*_sysfs_mask_pdir_attr_callback)(struct smmu_device *,
				unsigned long *field) = &_sysfs_mask_pdir_attr;
#endif

_SYSFS_SHOW_VALUE(pdir_attr, as[smmu->debug_asid].pdir_attr, "%lx")
_SYSFS_SET_VALUE(pdir_attr, as[smmu->debug_asid].pdir_attr, 16,
		_PDIR_ATTR+1, _sysfs_mask_pdir_attr_callback)
_SYSFS_SHOW_VALUE(pde_attr, as[smmu->debug_asid].pde_attr, "%lx")
_SYSFS_SET_VALUE(pde_attr, as[smmu->debug_asid].pde_attr, 16,
		_PDE_ATTR+1, _sysfs_mask_attr_callback)
_SYSFS_SHOW_VALUE(pte_attr, as[smmu->debug_asid].pte_attr, "%lx")
_SYSFS_SET_VALUE(pte_attr, as[smmu->debug_asid].pte_attr, 16,
		_PTE_ATTR+1, _sysfs_mask_attr_callback)

static struct device_attribute _attr_values[] = {
	__ATTR(lowest_asid, S_IRUGO|S_IWUSR,
		_sysfs_show_lowest_asid, _sysfs_set_lowest_asid),
	__ATTR(debug_asid, S_IRUGO|S_IWUSR,
		_sysfs_show_debug_asid, _sysfs_set_debug_asid),
	__ATTR(verbose, S_IRUGO|S_IWUSR,
		_sysfs_show_verbose, _sysfs_set_verbose),
	__ATTR(signature_pid, S_IRUGO|S_IWUSR,
		_sysfs_show_signature_pid, _sysfs_set_signature_pid),

	__ATTR(pdir_attr, S_IRUGO|S_IWUSR,
		_sysfs_show_pdir_attr, _sysfs_set_pdir_attr),
	__ATTR(pde_attr, S_IRUGO|S_IWUSR,
		_sysfs_show_pde_attr, _sysfs_set_pde_attr),
	__ATTR(pte_attr, S_IRUGO|S_IWUSR,
		_sysfs_show_pte_attr, _sysfs_set_pte_attr),
};

static struct attribute *_smmu_attrs[
	ARRAY_SIZE(_smmu_reg_name_map) + ARRAY_SIZE(_attr_values)+ 3];
static struct attribute_group _smmu_attr_group = {
	.attrs = _smmu_attrs
};

static void _sysfs_smmu(struct smmu_device *smmu, struct device *parent)
{
	int i, j;

	if (smmu->sysfs_use_count++ > 0)
		return;
	for (i = 0; i < ARRAY_SIZE(_smmu_reg_name_map); i++)
		_smmu_attrs[i] = &_smmu_reg_name_map[i].dev_attr.attr;
	for (j = 0; j < ARRAY_SIZE(_attr_values); j++)
		_smmu_attrs[i++] = &_attr_values[j].attr;
	_smmu_attrs[i++] = &_attr_show_smmu.attr;
	_smmu_attrs[i] = NULL;

	dev_set_name(&smmu->sysfs_dev, "smmu");
	smmu->sysfs_dev.parent = parent;
	smmu->sysfs_dev.driver = NULL;
	smmu->sysfs_dev.release = NULL;
	if (device_register(&smmu->sysfs_dev)) {
		pr_err("%s: failed to register smmu_sysfs_dev\n", __func__);
		smmu->sysfs_use_count--;
		return;
	}
	if (sysfs_create_group(&smmu->sysfs_dev.kobj, &_smmu_attr_group)) {
		pr_err("%s: failed to create group for smmu_sysfs_dev\n",
			__func__);
		smmu->sysfs_use_count--;
		return;
	}
}

static void _sysfs_create(struct smmu_as *as, struct device *parent)
{
	_sysfs_smmu(as->smmu, parent);
}
