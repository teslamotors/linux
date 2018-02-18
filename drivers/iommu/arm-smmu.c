/*
 * IOMMU API for ARM architected SMMU implementations.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) 2013 ARM Limited
 * Copyright (c) 2015-2016, NVIDIA CORPORATION. All rights reserved.
 *
 * Author: Will Deacon <will.deacon@arm.com>
 *
 * This driver currently supports:
 *	- SMMUv1 and v2 implementations
 *	- Stream-matching and stream-indexing
 *	- v7/v8 long-descriptor format
 *	- Non-secure access to the SMMU
 *	- 4k and 64k pages, with contiguous pte hints.
 *	- Up to 48-bit addressing (dependent on VA_BITS)
 *	- Context fault reporting
 */

#define pr_fmt(fmt) "arm-smmu: " fmt

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/dma-attrs.h>
#include <linux/tegra-soc.h>
#include <linux/ktime.h>
#include <linux/string.h>

#include <linux/amba/bus.h>


#include <asm/pgalloc.h>
#include <asm/dma-iommu.h>
#include <asm/pgtable.h>

#define CREATE_TRACE_POINTS
#include <trace/events/arm_smmu.h>

#include "of_tegra-smmu.h" /* FIXME: to parse implicitly */
#include "tegra-smmu.h"
#include <dt-bindings/memory/tegra-swgroup.h>
#include <linux/platform/tegra/tegra-mc-sid.h>

/* Maximum number of stream IDs assigned to a single device */
#define MAX_MASTER_STREAMIDS		MAX_PHANDLE_ARGS

/* Maximum number of context banks per SMMU */
#define ARM_SMMU_MAX_CBS		128

/* Maximum number of mapping groups per SMMU */
#define ARM_SMMU_MAX_SMRS		128

/* SMMU global address space */
#define ARM_SMMU_GR0(smmu)		((smmu)->base)
#define ARM_SMMU_GR1(smmu)		((smmu)->base + (1 << (smmu)->pgshift))
#define ARM_SMMU_PME(smmu)		((smmu)->base + (3 << (smmu)->pgshift))

/*
 * SMMU global address space with conditional offset to access secure
 * aliases of non-secure registers (e.g. nsCR0: 0x400, nsGFSR: 0x448,
 * nsGFSYNR0: 0x450)
 */
#define ARM_SMMU_GR0_NS(smmu)						\
	((smmu)->base +							\
		((smmu->options & ARM_SMMU_OPT_SECURE_CFG_ACCESS)	\
			? 0x400 : 0))

/* Page table bits */
#define ARM_SMMU_PTE_XN			(((pteval_t)3) << 53)
#define ARM_SMMU_PTE_CONT		(((pteval_t)1) << 52)
#define ARM_SMMU_PTE_AF			(((pteval_t)1) << 10)
#define ARM_SMMU_PTE_SH_NS		(((pteval_t)0) << 8)
#define ARM_SMMU_PTE_SH_OS		(((pteval_t)2) << 8)
#define ARM_SMMU_PTE_SH_IS		(((pteval_t)3) << 8)
#define ARM_SMMU_PTE_PAGE		(((pteval_t)3) << 0)

#if PAGE_SIZE == SZ_4K
#define ARM_SMMU_PTE_CONT_ENTRIES	16
#elif PAGE_SIZE == SZ_64K
#define ARM_SMMU_PTE_CONT_ENTRIES	32
#else
#define ARM_SMMU_PTE_CONT_ENTRIES	1
#endif

#define ARM_SMMU_PTE_CONT_SIZE		(PAGE_SIZE * ARM_SMMU_PTE_CONT_ENTRIES)
#define ARM_SMMU_PTE_CONT_MASK		(~(ARM_SMMU_PTE_CONT_SIZE - 1))

/* Stage-1 PTE */
#define ARM_SMMU_PTE_AP_UNPRIV		(((pteval_t)1) << 6)
#define ARM_SMMU_PTE_AP_RDONLY		(((pteval_t)2) << 6)
#define ARM_SMMU_PTE_ATTRINDX_SHIFT	2
#define ARM_SMMU_PTE_nG			(((pteval_t)1) << 11)

/* Stage-2 PTE */
#define ARM_SMMU_PTE_HAP_FAULT		(((pteval_t)0) << 6)
#define ARM_SMMU_PTE_HAP_READ		(((pteval_t)1) << 6)
#define ARM_SMMU_PTE_HAP_WRITE		(((pteval_t)2) << 6)
#define ARM_SMMU_PTE_MEMATTR_OIWB	(((pteval_t)0xf) << 2)
#define ARM_SMMU_PTE_MEMATTR_NC		(((pteval_t)0x5) << 2)
#define ARM_SMMU_PTE_MEMATTR_DEV	(((pteval_t)0x1) << 2)

/* Configuration registers */
#define ARM_SMMU_GR0_sCR0		0x0
#define sCR0_CLIENTPD			(1 << 0)
#define sCR0_GFRE			(1 << 1)
#define sCR0_GFIE			(1 << 2)
#define sCR0_GCFGFRE			(1 << 4)
#define sCR0_GCFGFIE			(1 << 5)
#define sCR0_USFCFG			(1 << 10)
#define sCR0_VMIDPNE			(1 << 11)
#define sCR0_PTM			(1 << 12)
#define sCR0_FB				(1 << 13)
#define sCR0_BSU_SHIFT			14
#define sCR0_BSU_MASK			0x3

/* Identification registers */
#define ARM_SMMU_GR0_ID0		0x20
#define ARM_SMMU_GR0_ID1		0x24
#define ARM_SMMU_GR0_ID2		0x28
#define ARM_SMMU_GR0_ID3		0x2c
#define ARM_SMMU_GR0_ID4		0x30
#define ARM_SMMU_GR0_ID5		0x34
#define ARM_SMMU_GR0_ID6		0x38
#define ARM_SMMU_GR0_ID7		0x3c
#define ARM_SMMU_GR0_sGFSR		0x48
#define ARM_SMMU_GR0_sGFSYNR0		0x50
#define ARM_SMMU_GR0_sGFSYNR1		0x54
#define ARM_SMMU_GR0_sGFSYNR2		0x58
#define ARM_SMMU_GR0_nsCR0		0x400
#define ARM_SMMU_GR0_nsGFSR		0x448
#define ARM_SMMU_GR0_nsGFSYNR0		0x450
#define ARM_SMMU_GR0_nsGFSYNR1		0x454
#define ARM_SMMU_GR0_nsGFSYNR2		0x458
#define ARM_SMMU_GR0_PIDR0		0xfe0
#define ARM_SMMU_GR0_PIDR1		0xfe4
#define ARM_SMMU_GR0_PIDR2		0xfe8

#define ID0_S1TS			(1 << 30)
#define ID0_S2TS			(1 << 29)
#define ID0_NTS				(1 << 28)
#define ID0_SMS				(1 << 27)
#define ID0_PTFS_SHIFT			24
#define ID0_PTFS_MASK			0x2
#define ID0_PTFS_V8_ONLY		0x2
#define ID0_CTTW			(1 << 14)
#define ID0_NUMIRPT_SHIFT		16
#define ID0_NUMIRPT_MASK		0xff
#define ID0_NUMSIDB_SHIFT		9
#define ID0_NUMSIDB_MASK		0xf
#define ID0_NUMSMRG_SHIFT		0
#define ID0_NUMSMRG_MASK		0xff

#define ID1_PAGESIZE			(1 << 31)
#define ID1_NUMPAGENDXB_SHIFT		28
#define ID1_NUMPAGENDXB_MASK		7
#define ID1_NUMS2CB_SHIFT		16
#define ID1_NUMS2CB_MASK		0xff
#define ID1_NUMCB_SHIFT			0
#define ID1_NUMCB_MASK			0xff

#define ID2_OAS_SHIFT			4
#define ID2_OAS_MASK			0xf
#define ID2_IAS_SHIFT			0
#define ID2_IAS_MASK			0xf
#define ID2_UBS_SHIFT			8
#define ID2_UBS_MASK			0xf
#define ID2_PTFS_4K			(1 << 12)
#define ID2_PTFS_16K			(1 << 13)
#define ID2_PTFS_64K			(1 << 14)

#define PIDR2_ARCH_SHIFT		4
#define PIDR2_ARCH_MASK			0xf

/* Perf Monitor registers */
#define ARM_SMMU_GNSR0_PMCNTENSET_0	0xc00
#define ARM_SMMU_GNSR0_PMCNTENCLR_0	0xc20
#define ARM_SMMU_GNSR0_PMINTENSET_0	0xc40
#define ARM_SMMU_GNSR0_PMINTENCLR_0	0xc60
#define ARM_SMMU_GNSR0_PMOVSCLR_0	0xc80
#define ARM_SMMU_GNSR0_PMOVSSET_0	0xcc0
#define ARM_SMMU_GNSR0_PMCFGR_0		0xe00
#define ARM_SMMU_GNSR0_PMCR_0		0xe04
#define ARM_SMMU_GNSR0_PMCEID0_0	0xe20
#define ARM_SMMU_GNSR0_PMAUTHSTATUS_0	0xfb8
#define ARM_SMMU_GNSR0_PMDEVTYPE_0	0xfcc

#define ARM_SMMU_GNSR0_PMEVTYPER(n)	(0x400 + ((n) << 2))
#define ARM_SMMU_GNSR0_PMEVCNTR(n)	(0x0 + ((n) << 2))
#define ARM_SMMU_GNSR0_PMCGCR(n)	(0x800 + ((n) << 2))
#define ARM_SMMU_GNSR0_PMCGSMR(n)	(0xa00 + ((n) << 2))

/* Counter group registers */
#define PMCG_SIZE	32
/* Event Counter registers */
#define PMEV_SIZE	8

/* Global TLB invalidation */
#define ARM_SMMU_GR0_STLBIALL		0x60
#define ARM_SMMU_GR0_TLBIVMID		0x64
#define ARM_SMMU_GR0_TLBIALLNSNH	0x68
#define ARM_SMMU_GR0_TLBIALLH		0x6c
#define ARM_SMMU_GR0_sTLBGSYNC		0x70
#define ARM_SMMU_GR0_sTLBGSTATUS	0x74
#define ARM_SMMU_GR0_nsTLBGSYNC		0x470
#define ARM_SMMU_GR0_nsTLBGSTATUS	0x474
#define sTLBGSTATUS_GSACTIVE		(1 << 0)
#define TLB_LOOP_TIMEOUT		1000000	/* 1s! */

/* Stream mapping registers */
#define ARM_SMMU_GR0_SMR(n)		(0x800 + ((n) << 2))
#define SMR_VALID			(1 << 31)
#define SMR_MASK_SHIFT			16
#define SMR_MASK_MASK			0x7f80
#define SMR_ID_SHIFT			0
#define SMR_ID_MASK			0x7f80

#define ARM_SMMU_GR0_S2CR(n)		(0xc00 + ((n) << 2))
#define S2CR_CBNDX_SHIFT		0
#define S2CR_CBNDX_MASK			0xff
#define S2CR_TYPE_SHIFT			16
#define S2CR_TYPE_MASK			0x3
#define S2CR_TYPE_TRANS			(0 << S2CR_TYPE_SHIFT)
#define S2CR_TYPE_BYPASS		(1 << S2CR_TYPE_SHIFT)
#define S2CR_TYPE_FAULT			(2 << S2CR_TYPE_SHIFT)

/* Context bank attribute registers */
#define ARM_SMMU_GR1_CBAR(n)		(0x0 + ((n) << 2))
#define CBAR_VMID_SHIFT			0
#define CBAR_VMID_MASK			0xff
#define CBAR_S1_BPSHCFG_SHIFT		8
#define CBAR_S1_BPSHCFG_MASK		3
#define CBAR_S1_BPSHCFG_NSH		3
#define CBAR_S1_MEMATTR_SHIFT		12
#define CBAR_S1_MEMATTR_MASK		0xf
#define CBAR_S1_MEMATTR_WB		0xf
#define CBAR_TYPE_SHIFT			16
#define CBAR_TYPE_MASK			0x3
#define CBAR_TYPE_S2_TRANS		(0 << CBAR_TYPE_SHIFT)
#define CBAR_TYPE_S1_TRANS_S2_BYPASS	(1 << CBAR_TYPE_SHIFT)
#define CBAR_TYPE_S1_TRANS_S2_FAULT	(2 << CBAR_TYPE_SHIFT)
#define CBAR_TYPE_S1_TRANS_S2_TRANS	(3 << CBAR_TYPE_SHIFT)
#define CBAR_IRPTNDX_SHIFT		24
#define CBAR_IRPTNDX_MASK		0xff

#define ARM_SMMU_GR1_FRSYNRA(n)		(0x400 + ((n) << 2))
#define FRSYNRA_STREAMID_MASK		0xffff
#define FRSYNRA_STREAMID_SHIFT		0x0

#define ARM_SMMU_GR1_CBA2R(n)		(0x800 + ((n) << 2))
#define CBA2R_RW64_32BIT		(0 << 0)
#define CBA2R_RW64_64BIT		(1 << 0)

/* Translation context bank */
#define ARM_SMMU_CB_BASE(smmu)		((smmu)->base + ((smmu)->size >> 1))
#define ARM_SMMU_CB(smmu, n)		((n) * (1 << (smmu)->pgshift))

#define ARM_SMMU_CB_SCTLR		0x0
#define ARM_SMMU_CB_RESUME		0x8
#define ARM_SMMU_CB_TTBCR2		0x10
#define ARM_SMMU_CB_TTBR0_LO		0x20
#define ARM_SMMU_CB_TTBR0_HI		0x24
#define ARM_SMMU_CB_TTBCR		0x30
#define ARM_SMMU_CB_S1_MAIR0		0x38
#define ARM_SMMU_CB_FSR			0x58
#define ARM_SMMU_CB_FAR_LO		0x60
#define ARM_SMMU_CB_FAR_HI		0x64
#define ARM_SMMU_CB_FSYNR0		0x68
#define ARM_SMMU_CB_S1_TLBIASID		0x610
#define ARM_SMMU_CB_S1_TLBIVA		0x600
#define ARM_SMMU_CB_S1_TLBIVAL		0x620
#define ARM_SMMU_CB_S2_TLBIIPAS2	0x630
#define ARM_SMMU_CB_S2_TLBIIPAS2L	0x638

#define SCTLR_S1_ASIDPNE		(1 << 12)
#define SCTLR_CFCFG			(1 << 7)
#define SCTLR_CFIE			(1 << 6)
#define SCTLR_CFRE			(1 << 5)
#define SCTLR_E				(1 << 4)
#define SCTLR_AFE			(1 << 2)
#define SCTLR_TRE			(1 << 1)
#define SCTLR_M				(1 << 0)
#define SCTLR_EAE_SBOP			(SCTLR_AFE | SCTLR_TRE)

#define RESUME_RETRY			(0 << 0)
#define RESUME_TERMINATE		(1 << 0)

#define TTBCR_EAE			(1 << 31)

#define TTBCR_PASIZE_SHIFT		16
#define TTBCR_PASIZE_MASK		0x7

#define TTBCR_TG0_4K			(0 << 14)
#define TTBCR_TG0_64K			(1 << 14)

#define TTBCR_SH0_SHIFT			12
#define TTBCR_SH0_MASK			0x3
#define TTBCR_SH_NS			0
#define TTBCR_SH_OS			2
#define TTBCR_SH_IS			3

#define TTBCR_ORGN0_SHIFT		10
#define TTBCR_IRGN0_SHIFT		8
#define TTBCR_RGN_MASK			0x3
#define TTBCR_RGN_NC			0
#define TTBCR_RGN_WBWA			1
#define TTBCR_RGN_WT			2
#define TTBCR_RGN_WB			3

#define TTBCR_SL0_SHIFT			6
#define TTBCR_SL0_MASK			0x3
#define TTBCR_SL0_LVL_2			0
#define TTBCR_SL0_LVL_1			1

#define TTBCR_T1SZ_SHIFT		16
#define TTBCR_T0SZ_SHIFT		0
#define TTBCR_SZ_MASK			0xf

#define TTBCR2_SEP_SHIFT		15
#define TTBCR2_SEP_MASK			0x7

#define TTBCR2_PASIZE_SHIFT		0
#define TTBCR2_PASIZE_MASK		0x7

/* Common definitions for PASize and SEP fields */
#define TTBCR2_ADDR_32			0
#define TTBCR2_ADDR_36			1
#define TTBCR2_ADDR_40			2
#define TTBCR2_ADDR_42			3
#define TTBCR2_ADDR_44			4
#define TTBCR2_ADDR_48			5

#define TTBRn_HI_ASID_SHIFT		16

#define MAIR_ATTR_SHIFT(n)		((n) << 3)
#define MAIR_ATTR_MASK			0xff
#define MAIR_ATTR_DEVICE		0x04
#define MAIR_ATTR_NC			0x44
#define MAIR_ATTR_WBRWA			0xff
#define MAIR_ATTR_IDX_NC		0
#define MAIR_ATTR_IDX_CACHE		1
#define MAIR_ATTR_IDX_DEV		2

#define FSR_MULTI			(1 << 31)
#define FSR_SS				(1 << 30)
#define FSR_UUT				(1 << 8)
#define FSR_ASF				(1 << 7)
#define FSR_TLBLKF			(1 << 6)
#define FSR_TLBMCF			(1 << 5)
#define FSR_EF				(1 << 4)
#define FSR_PF				(1 << 3)
#define FSR_AFF				(1 << 2)
#define FSR_TF				(1 << 1)

#define FSR_IGN				(FSR_AFF | FSR_ASF | \
					 FSR_TLBMCF | FSR_TLBLKF)
#define FSR_FAULT			(FSR_MULTI | FSR_SS | FSR_UUT | \
					 FSR_EF | FSR_PF | FSR_TF | FSR_IGN)

#define FSYNR0_WNR			(1 << 4)

#define NUM_SID				64

static int force_stage;
module_param_named(force_stage, force_stage, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(force_stage,
	"Force SMMU mappings to be installed at a particular stage of translation. A value of '1' or '2' forces the corresponding stage. All other values are ignored (i.e. no stage is forced). Note that selecting a specific stage will disable support for nested translation.");

enum arm_smmu_arch_version {
	ARM_SMMU_V1 = 1,
	ARM_SMMU_V2,
};

struct arm_smmu_smr {
	u8				idx;
	u16				mask;
	u16				id;
};

struct arm_smmu_master_cfg {
	int				num_streamids;
	u16				streamids[MAX_MASTER_STREAMIDS];
	struct arm_smmu_smr		*smrs;
};

struct arm_smmu_master {
	struct device_node		*of_node;
	struct rb_node			node;
	struct arm_smmu_master_cfg	*cfg;
	struct dentry			*debugfs_root;
};

struct arm_smmu_device {
	struct device			*dev;

	void __iomem			*base;
	unsigned long			size;
	unsigned long			pgshift;

#define ARM_SMMU_FEAT_COHERENT_WALK	(1 << 0)
#define ARM_SMMU_FEAT_STREAM_MATCH	(1 << 1)
#define ARM_SMMU_FEAT_TRANS_S1		(1 << 2)
#define ARM_SMMU_FEAT_TRANS_S2		(1 << 3)
#define ARM_SMMU_FEAT_TRANS_NESTED	(1 << 4)
	u32				features;

#define ARM_SMMU_OPT_SECURE_CFG_ACCESS (1 << 0)
#define ARM_SMMU_OPT_BROKEN_SIM_IRQ    (1 << 1)
	u32				options;
	enum arm_smmu_arch_version	version;

	u32				num_context_banks;
	u32				num_s2_context_banks;
	DECLARE_BITMAP(context_map, ARM_SMMU_MAX_CBS);
	atomic_t			irptndx;

	u32				num_mapping_groups;
	DECLARE_BITMAP(smr_map, ARM_SMMU_MAX_SMRS);

	unsigned long			s1_input_size;
	unsigned long			s1_output_size;
	unsigned long			s2_input_size;
	unsigned long			s2_output_size;

	u32				num_global_irqs;
	u32				num_context_irqs;
	unsigned int			*irqs;

	struct list_head		list;
	struct rb_root			masters;
	struct dentry			*masters_root;

	struct dentry			*debugfs_root;
	struct debugfs_regset32		*regset;
	struct debugfs_regset32         *perf_regset;
	DECLARE_BITMAP(context_filter, ARM_SMMU_MAX_CBS);
};

struct arm_smmu_cfg {
	u8				cbndx;
	u8				irptndx;
	u32				cbar;
	pgd_t				*pgd;
};
#define INVALID_IRPTNDX			0xff

#define ARM_SMMU_CB_ASID(cfg)		((cfg)->cbndx)
#define ARM_SMMU_CB_VMID(cfg)		((cfg)->cbndx + 1)

struct arm_smmu_domain {
	struct arm_smmu_device		*smmu;
	struct arm_smmu_cfg		cfg;
	struct page *arm_dummy_page;   /* dummy page for faulted address*/
	spinlock_t			lock;

	dma_addr_t			inquired_iova;
	phys_addr_t			inquired_phys;
};

static struct iommu_domain *iommu_domains[NUM_SID]; /* To keep all allocated domains */

static DEFINE_SPINLOCK(arm_smmu_devices_lock);
static LIST_HEAD(arm_smmu_devices);

static struct arm_smmu_device *smmu_handle; /* assmu only one smmu device */
static u32 arm_smmu_skip_mapping; /* For debug */
static u32 arm_smmu_gr0_tlbiallnsnh; /* Insert TLBIALLNSNH at all */
static u32 arm_smmu_tlb_inv_by_addr = 1; /* debugfs: tlb inv context by default */
static u32 arm_smmu_tlb_inv_at_map;	/* debugfs: tlb inv at map additionally */

#ifdef CONFIG_ARM_SMMU_WAR
/*
 * linsim hacks: Indirect register accessor
 */
#undef readl_relaxed
#undef writel_relaxed
#undef writel
#define __readl_relaxed(c)						\
	({ u32 __v = le32_to_cpu((__force __le32)__raw_readl(c)); __v; })
#define __writel(v,c)							\
	({ __iowmb(); ((void)__raw_writel((__force u32)cpu_to_le32(v),(c))); })

static volatile void __iomem *mc_base;

#define MC_BASE				0x22c10000 /* HACK: for c-model */
#define MC_SIZE				0x00010000
#define ARM_SMMU_CONTROL_ADDRESS	0x24
#define ARM_SMMU_CONTROL_DATA		0x28

static inline void writel(u32 val, volatile void __iomem *virt_addr)
{
	u32 offset;
	volatile void __iomem *ctl_addr;

	if (!tegra_platform_is_linsim()) {
		__writel(val, virt_addr);
		return;
	}

	ctl_addr = mc_base + ARM_SMMU_CONTROL_ADDRESS;
	offset = virt_addr - smmu_handle->base;
	__writel(offset, ctl_addr);
	pr_debug("Indirect write(ADDRESS) offset=%08x ctl_addr=%p\n",
		 offset, ctl_addr);

	ctl_addr = mc_base + ARM_SMMU_CONTROL_DATA;
	__writel(val, ctl_addr);
	pr_debug("Indirect write(DATA) val=%08x ctl_addr=%p\n", val, ctl_addr);
}
#define writel_relaxed(v,a) writel(v,a)

static inline u32 readl_relaxed(const volatile void __iomem *virt_addr)
{
	u32 val, offset;
	volatile void __iomem *ctl_addr;

	if (!tegra_platform_is_linsim())
		return __readl_relaxed(virt_addr);

	ctl_addr = mc_base + ARM_SMMU_CONTROL_ADDRESS;
	offset = virt_addr - smmu_handle->base;
	__writel(offset, ctl_addr);
	pr_debug("Indirect read(ADDRESS) offset=%08x ctl_addr=%p\n",
		 offset, ctl_addr);

	ctl_addr = mc_base + ARM_SMMU_CONTROL_DATA;
	val = __readl_relaxed(ctl_addr);
	pr_debug("Indirect read(DATA) val=%08x ctl_addr=%p\n", val, ctl_addr);
	return val;
}
#endif

void __weak platform_override_streamid(int streamid)
{
}

static phys_addr_t arm_smmu_iova_to_phys(struct iommu_domain *domain,
					dma_addr_t iova);

struct arm_smmu_option_prop {
	u32 opt;
	const char *prop;
};

static struct arm_smmu_option_prop arm_smmu_options[] = {
	{ ARM_SMMU_OPT_SECURE_CFG_ACCESS, "calxeda,smmu-secure-config-access" },
	{ ARM_SMMU_OPT_SECURE_CFG_ACCESS, "-calxeda,smmu-secure-config-access" },
	{ ARM_SMMU_OPT_BROKEN_SIM_IRQ, "linsim,smmu-broken-sim-irq" },
	{ 0, NULL},
};

static void parse_driver_options(struct arm_smmu_device *smmu)
{
	int i = 0;

	do {
		if (of_property_read_bool(smmu->dev->of_node,
						arm_smmu_options[i].prop)) {
			if (arm_smmu_options[i].prop[0] == '-')
				smmu->options &= ~arm_smmu_options[i].opt;
			else
				smmu->options |= arm_smmu_options[i].opt;
			dev_notice(smmu->dev, "option %s\n",
				arm_smmu_options[i].prop);
		}
	} while (arm_smmu_options[++i].opt);

	/* FIXME: remove if linsim is fixed */
	if (tegra_platform_is_linsim())
		smmu->options |= ARM_SMMU_OPT_SECURE_CFG_ACCESS;
	else
		smmu->options &= ~ARM_SMMU_OPT_BROKEN_SIM_IRQ;
}

static struct device_node *dev_get_dev_node(struct device *dev)
{
	if (dev_is_pci(dev)) {
		struct pci_bus *bus = to_pci_dev(dev)->bus;

		while (!pci_is_root_bus(bus))
			bus = bus->parent;
		return bus->bridge->parent->of_node;
	}

	return dev->of_node;
}

static struct arm_smmu_master *find_smmu_master(struct arm_smmu_device *smmu,
						struct device_node *dev_node)
{
	struct rb_node *node = smmu->masters.rb_node;

	while (node) {
		struct arm_smmu_master *master;

		master = container_of(node, struct arm_smmu_master, node);

		if (dev_node < master->of_node)
			node = node->rb_left;
		else if (dev_node > master->of_node)
			node = node->rb_right;
		else
			return master;
	}

	return NULL;
}

static struct arm_smmu_master_cfg *
find_smmu_master_cfg(struct device *dev)
{
	struct arm_smmu_master_cfg *cfg = NULL;
	struct iommu_group *group = iommu_group_get(dev);

	if (group) {
		cfg = iommu_group_get_iommudata(group);
		iommu_group_put(group);
	}

	return cfg;
}

static int insert_smmu_master(struct arm_smmu_device *smmu,
			      struct arm_smmu_master *master)
{
	struct rb_node **new, *parent;

	new = &smmu->masters.rb_node;
	parent = NULL;
	while (*new) {
		struct arm_smmu_master *this
			= container_of(*new, struct arm_smmu_master, node);

		parent = *new;
		if (master->of_node < this->of_node)
			new = &((*new)->rb_left);
		else if (master->of_node > this->of_node)
			new = &((*new)->rb_right);
		else
			return -EEXIST;
	}

	rb_link_node(&master->node, parent, new);
	rb_insert_color(&master->node, &smmu->masters);
	return 0;
}

/*
 * Look for a master_cfg which is identical to the masterspec passed. This
 * allows us to support multiple devices all sharing the same master_cfg.
 * Ultimately that allows multiple devices to share the same set of stream
 * IDs.
 */
static struct arm_smmu_master_cfg *
find_identical_smmu_master_cfg(struct arm_smmu_device *smmu,
			       struct of_phandle_args *masterspec)
{
	struct rb_node *node;
	struct arm_smmu_master *master;
	int i;

	for (node = rb_first(&smmu->masters); node; node = rb_next(node)) {
		int sids_match = 1;

		master = container_of(node, struct arm_smmu_master, node);

		/* If we don't have the same number of StreamIDs then we are
		 * certainly not identical. */
		if (master->cfg->num_streamids != masterspec->args_count)
			continue;

		/* And now check the StreamIDs themselves. */
		for (i = 0; i < masterspec->args_count; i++) {
			if (masterspec->args[i] != master->cfg->streamids[i]) {
				sids_match = 0;
				break;
			}
		}

		if (!sids_match)
			continue;

		/* Found an identical master cfg! */
		return master->cfg;
	}

	return NULL;
}

static int register_smmu_master(struct arm_smmu_device *smmu,
				struct device *dev,
				struct of_phandle_args *masterspec)
{
	int i;
	struct arm_smmu_master *master;
	struct arm_smmu_master_cfg *master_cfg;

	master = find_smmu_master(smmu, masterspec->np);
	if (master) {
		dev_err(dev,
			"rejecting multiple registrations for master device %s\n",
			masterspec->np->name);
		return -EBUSY;
	}

	if (masterspec->args_count > MAX_MASTER_STREAMIDS) {
		dev_err(dev,
			"reached maximum number (%d) of stream IDs for master device %s\n",
			MAX_MASTER_STREAMIDS, masterspec->np->name);
		return -ENOSPC;
	}

	master = devm_kzalloc(dev, sizeof(*master), GFP_KERNEL);
	if (!master)
		return -ENOMEM;

	master->of_node = masterspec->np;

	master_cfg = find_identical_smmu_master_cfg(smmu, masterspec);
	if (master_cfg) {
		master->cfg = master_cfg;
		return insert_smmu_master(smmu, master);
	}

	/* Make a new master_cfg */
	master_cfg = devm_kzalloc(dev, sizeof(*master_cfg), GFP_KERNEL);
	if (!master_cfg) {
		kfree(master);
		return -ENOMEM;
	}

	master->cfg = master_cfg;
	master->cfg->num_streamids = masterspec->args_count;
	for (i = 0; i < master->cfg->num_streamids; ++i) {
		u16 streamid = masterspec->args[i];

		if (!(smmu->features & ARM_SMMU_FEAT_STREAM_MATCH) &&
		     (streamid >= smmu->num_mapping_groups)) {
			dev_err(dev,
				"stream ID for master device %s greater than maximum allowed (%d)\n",
				masterspec->np->name, smmu->num_mapping_groups);
			return -ERANGE;
		}
		master->cfg->streamids[i] = streamid;
		platform_override_streamid(streamid);
	}

	return insert_smmu_master(smmu, master);
}

static struct arm_smmu_device *find_smmu_for_device(struct device *dev)
{
	struct arm_smmu_device *smmu;
	struct arm_smmu_master *master = NULL;
	struct device_node *dev_node = dev_get_dev_node(dev);

	spin_lock(&arm_smmu_devices_lock);
	list_for_each_entry(smmu, &arm_smmu_devices, list) {
		master = find_smmu_master(smmu, dev_node);
		if (master)
			break;
	}
	spin_unlock(&arm_smmu_devices_lock);

	return master ? smmu : NULL;
}

static int __arm_smmu_alloc_bitmap(unsigned long *map, int start, int end)
{
	int idx;

	do {
		idx = find_next_zero_bit(map, end, start);
		if (idx == end)
			return -ENOSPC;
	} while (test_and_set_bit(idx, map));

	return idx;
}

static void __arm_smmu_free_bitmap(unsigned long *map, int idx)
{
	clear_bit(idx, map);
}

/* Wait for any pending TLB invalidations to complete */
static void arm_smmu_tlb_sync(struct arm_smmu_device *smmu)
{
	int count = 0;
	void __iomem *gr0_base = ARM_SMMU_GR0(smmu);

	if (tegra_platform_is_linsim() || arm_smmu_gr0_tlbiallnsnh)
		writel_relaxed(0, gr0_base + ARM_SMMU_GR0_TLBIALLNSNH);

	writel_relaxed(0, gr0_base + ARM_SMMU_GR0_sTLBGSYNC);
	while (readl_relaxed(gr0_base + ARM_SMMU_GR0_sTLBGSTATUS)
	       & sTLBGSTATUS_GSACTIVE) {
		cpu_relax();
		if (++count == TLB_LOOP_TIMEOUT) {
			dev_err_ratelimited(smmu->dev,
			"TLB sync timed out -- SMMU may be deadlocked\n");
			return;
		}
		udelay(1);
	}
}

static void arm_smmu_tlb_inv_context(struct arm_smmu_domain *smmu_domain)
{
	u64 time_before = 0;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	void __iomem *base = ARM_SMMU_GR0(smmu);
	bool stage1 = cfg->cbar != CBAR_TYPE_S2_TRANS;

#ifdef CONFIG_TRACEPOINTS
	if (static_key_false(&__tracepoint_arm_smmu_tlb_inv_context.key)
		&& test_bit(cfg->cbndx, smmu->context_filter))
		time_before = local_clock();
#endif

	if (stage1) {
		base = ARM_SMMU_CB_BASE(smmu) + ARM_SMMU_CB(smmu, cfg->cbndx);
		writel_relaxed(ARM_SMMU_CB_ASID(cfg),
			       base + ARM_SMMU_CB_S1_TLBIASID);
	} else {
		base = ARM_SMMU_GR0(smmu);
		writel_relaxed(ARM_SMMU_CB_VMID(cfg),
			       base + ARM_SMMU_GR0_TLBIVMID);
	}

	arm_smmu_tlb_sync(smmu);

	if (time_before)
		trace_arm_smmu_tlb_inv_context(time_before, cfg->cbndx);
}

static void __arm_smmu_tlb_inv_range(struct arm_smmu_domain *smmu_domain,
				     unsigned long iova)
{
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	bool stage1 = cfg->cbar != CBAR_TYPE_S2_TRANS;
	void __iomem *reg;

	if (stage1) {
		reg = ARM_SMMU_CB_BASE(smmu) + ARM_SMMU_CB(smmu, cfg->cbndx);
		reg += ARM_SMMU_CB_S1_TLBIVA;
		iova >>= 12;
		iova |= (u64)ARM_SMMU_CB_ASID(cfg) << 48;
		writeq_relaxed(iova, reg);
	} else if (smmu->version == ARM_SMMU_V2) {
		reg = ARM_SMMU_CB_BASE(smmu) + ARM_SMMU_CB(smmu, cfg->cbndx);
		reg += ARM_SMMU_CB_S2_TLBIIPAS2;
		writeq_relaxed(iova >> 12, reg);
	} else {
		reg = ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_TLBIVMID;
		writel_relaxed(ARM_SMMU_CB_VMID(cfg), reg);
	}
}

static void arm_smmu_tlb_inv_range(struct arm_smmu_domain *smmu_domain,
				   unsigned long iova, size_t size)
{
	int i;
	u64 time_before = 0;
	unsigned long iova_orig = iova;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct arm_smmu_device *smmu = smmu_domain->smmu;

#ifdef CONFIG_TRACEPOINTS
	if (static_key_false(&__tracepoint_arm_smmu_tlb_inv_range.key)
		&& test_bit(cfg->cbndx, smmu->context_filter))
		time_before = local_clock();
#endif

	for (i = 0; i < size / PAGE_SIZE; i++) {
		__arm_smmu_tlb_inv_range(smmu_domain, iova);
		iova += PAGE_SIZE;
	}
	arm_smmu_tlb_sync(smmu);

	if (time_before)
		trace_arm_smmu_tlb_inv_range(time_before, cfg->cbndx,
			iova_orig, size);
}

static irqreturn_t __arm_smmu_context_fault(int irq, void *dev)
{
	int flags, ret, sid;
	u32 fsr, far, fsynr, fsynra, resume;
	unsigned long iova;
	struct iommu_domain *domain = dev;
	struct arm_smmu_domain *smmu_domain = domain->priv;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	void __iomem *cb_base;

	cb_base = ARM_SMMU_CB_BASE(smmu) + ARM_SMMU_CB(smmu, cfg->cbndx);
	fsr = readl_relaxed(cb_base + ARM_SMMU_CB_FSR);

	if (!(fsr & FSR_FAULT))
		return IRQ_NONE;

	if (fsr & FSR_IGN)
		dev_err_ratelimited(smmu->dev,
				    "Unexpected context fault (fsr 0x%x)\n",
				    fsr);

	fsynr = readl_relaxed(cb_base + ARM_SMMU_CB_FSYNR0);
	fsynra = readl_relaxed(ARM_SMMU_GR1(smmu) +
			       ARM_SMMU_GR1_FRSYNRA(cfg->cbndx));
	flags = fsynr & FSYNR0_WNR ? IOMMU_FAULT_WRITE : IOMMU_FAULT_READ;

	far = readl_relaxed(cb_base + ARM_SMMU_CB_FAR_LO);
	iova = far;
#ifdef CONFIG_64BIT
	far = readl_relaxed(cb_base + ARM_SMMU_CB_FAR_HI);
	iova |= ((unsigned long)far << 32);
#endif
	sid = (fsynra & FRSYNRA_STREAMID_MASK) >> FRSYNRA_STREAMID_SHIFT;
	sid &= 0xff; /* Tegra hack. Why are the top 8bits not RAZ? */

	if (!report_iommu_fault(domain, smmu->dev, iova, flags)) {
		ret = IRQ_HANDLED;
		resume = RESUME_RETRY;
	} else {
		dev_err_ratelimited(smmu->dev,
		    "Unhandled context fault: iova=0x%08lx, fsynr=0x%x, cb=%d, sid=%d(0x%x - %s)\n",
				    iova, fsynr, cfg->cbndx, sid, sid,
				    tegra_mc_get_sid_name(sid));
		trace_printk("Unhandled context fault: iova=0x%08lx, fsynr=0x%x, cb=%d\n",
			iova, fsynr, cfg->cbndx);
		ret = IRQ_NONE;
		resume = RESUME_TERMINATE;
	}

	/* Clear the faulting FSR */
	writel(fsr, cb_base + ARM_SMMU_CB_FSR);

	/* Retry or terminate any stalled transactions */
	if (fsr & FSR_SS)
		writel_relaxed(resume, cb_base + ARM_SMMU_CB_RESUME);

	return ret;
}

static irqreturn_t arm_smmu_context_fault(int irq, void *dev)
{
	int i;
	struct arm_smmu_device *smmu = dev;

	for (i = 0; i < smmu->num_context_banks; i++) {
		void __iomem *cb_base;
		struct iommu_domain *domain;
		u32 fsr;

		cb_base = ARM_SMMU_CB_BASE(smmu) + ARM_SMMU_CB(smmu, i);
		fsr = readl_relaxed(cb_base + ARM_SMMU_CB_FSR);
		if (fsr & FSR_FAULT) {
			domain = iommu_domains[i];
			if (!domain) {
				pr_err("%s: domain(%d) doesn't exist\n",
				       __func__, i);
				continue;
			}
			__arm_smmu_context_fault(irq, domain);
			return IRQ_HANDLED;
		}
	}
	return IRQ_NONE;
}

static irqreturn_t arm_smmu_global_fault(int irq, void *dev)
{
	u32 gfsr, gfsynr0, gfsynr1, gfsynr2;
	struct arm_smmu_device *smmu = dev;
	void __iomem *gr0_base = ARM_SMMU_GR0_NS(smmu);

	gfsr = readl_relaxed(gr0_base + ARM_SMMU_GR0_sGFSR);
	gfsynr0 = readl_relaxed(gr0_base + ARM_SMMU_GR0_sGFSYNR0);
	gfsynr1 = readl_relaxed(gr0_base + ARM_SMMU_GR0_sGFSYNR1);
	gfsynr2 = readl_relaxed(gr0_base + ARM_SMMU_GR0_sGFSYNR2);

	if (!gfsr) {
		int ret;

		ret = arm_smmu_context_fault(irq, dev);
		if (ret == IRQ_HANDLED)
			return ret;
	}

	dev_err_ratelimited(smmu->dev,
		"Unexpected {global,context} fault, this could be serious\n");
	dev_err_ratelimited(smmu->dev,
		"\tGFSR 0x%08x, GFSYNR0 0x%08x, GFSYNR1 0x%08x, GFSYNR2 0x%08x\n",
		gfsr, gfsynr0, gfsynr1, gfsynr2);

	trace_printk("Unexpected {global,context} fault, this could be serious\n");
	trace_printk("\tGFSR 0x%08x, GFSYNR0 0x%08x, GFSYNR1 0x%08x, GFSYNR2 0x%08x\n",
		gfsr, gfsynr0, gfsynr1, gfsynr2);

	writel(gfsr, gr0_base + ARM_SMMU_GR0_sGFSR);
	return IRQ_HANDLED;
}

static void arm_smmu_flush_pgtable(struct arm_smmu_device *smmu, void *addr,
				   size_t size)
{
	unsigned long offset = (unsigned long)addr & ~PAGE_MASK;


	/* Ensure new page tables are visible to the hardware walker */
	if (smmu->features & ARM_SMMU_FEAT_COHERENT_WALK) {
		dsb(ishst);
	} else {
		/*
		 * If the SMMU can't walk tables in the CPU caches, treat them
		 * like non-coherent DMA since we need to flush the new entries
		 * all the way out to memory. There's no possibility of
		 * recursion here as the SMMU table walker will not be wired
		 * through another SMMU.
		 */
		dma_map_page(smmu->dev, virt_to_page(addr), offset, size,
				DMA_TO_DEVICE);
	}
}

static void arm_smmu_init_context_bank(struct arm_smmu_domain *smmu_domain)
{
	u32 reg;
	bool stage1;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	void __iomem *cb_base, *gr0_base, *gr1_base;

	gr0_base = ARM_SMMU_GR0(smmu);
	gr1_base = ARM_SMMU_GR1(smmu);
	stage1 = cfg->cbar != CBAR_TYPE_S2_TRANS;
	cb_base = ARM_SMMU_CB_BASE(smmu) + ARM_SMMU_CB(smmu, cfg->cbndx);

	/* CBAR */
	reg = cfg->cbar;
	if (smmu->version == ARM_SMMU_V1)
		reg |= cfg->irptndx << CBAR_IRPTNDX_SHIFT;

	/*
	 * Use the weakest shareability/memory types, so they are
	 * overridden by the ttbcr/pte.
	 */
	if (stage1) {
		reg |= (CBAR_S1_BPSHCFG_NSH << CBAR_S1_BPSHCFG_SHIFT) |
			(CBAR_S1_MEMATTR_WB << CBAR_S1_MEMATTR_SHIFT);
	} else {
		reg |= ARM_SMMU_CB_VMID(cfg) << CBAR_VMID_SHIFT;
	}
	writel_relaxed(reg, gr1_base + ARM_SMMU_GR1_CBAR(cfg->cbndx));

	if (smmu->version > ARM_SMMU_V1) {
		/* CBA2R */
#ifdef CONFIG_64BIT
		reg = CBA2R_RW64_64BIT;
#else
		reg = CBA2R_RW64_32BIT;
#endif
		writel_relaxed(reg,
			       gr1_base + ARM_SMMU_GR1_CBA2R(cfg->cbndx));

		/* TTBCR2 */
		switch (smmu->s1_input_size) {
		case 32:
			reg = (TTBCR2_ADDR_32 << TTBCR2_SEP_SHIFT);
			break;
		case 36:
			reg = (TTBCR2_ADDR_36 << TTBCR2_SEP_SHIFT);
			break;
		case 39:
		case 40:
			reg = (TTBCR2_ADDR_40 << TTBCR2_SEP_SHIFT);
			break;
		case 42:
			reg = (TTBCR2_ADDR_42 << TTBCR2_SEP_SHIFT);
			break;
		case 44:
			reg = (TTBCR2_ADDR_44 << TTBCR2_SEP_SHIFT);
			break;
		case 48:
			reg = (TTBCR2_ADDR_48 << TTBCR2_SEP_SHIFT);
			break;
		}

		switch (smmu->s1_output_size) {
		case 32:
			reg |= (TTBCR2_ADDR_32 << TTBCR2_PASIZE_SHIFT);
			break;
		case 36:
			reg |= (TTBCR2_ADDR_36 << TTBCR2_PASIZE_SHIFT);
			break;
		case 39:
		case 40:
			reg |= (TTBCR2_ADDR_40 << TTBCR2_PASIZE_SHIFT);
			break;
		case 42:
			reg |= (TTBCR2_ADDR_42 << TTBCR2_PASIZE_SHIFT);
			break;
		case 44:
			reg |= (TTBCR2_ADDR_44 << TTBCR2_PASIZE_SHIFT);
			break;
		case 48:
			reg |= (TTBCR2_ADDR_48 << TTBCR2_PASIZE_SHIFT);
			break;
		}

		if (stage1)
			writel_relaxed(reg, cb_base + ARM_SMMU_CB_TTBCR2);
	}

	/* TTBR0 */
	arm_smmu_flush_pgtable(smmu, cfg->pgd,
			       PTRS_PER_PGD * sizeof(pgd_t));
	reg = __pa(cfg->pgd);
	writel_relaxed(reg, cb_base + ARM_SMMU_CB_TTBR0_LO);
	reg = (phys_addr_t)__pa(cfg->pgd) >> 32;
	if (stage1)
		reg |= ARM_SMMU_CB_ASID(cfg) << TTBRn_HI_ASID_SHIFT;
	writel_relaxed(reg, cb_base + ARM_SMMU_CB_TTBR0_HI);

	/*
	 * TTBCR
	 * We use long descriptor, with inner-shareable WBWA tables in TTBR0.
	 */
	if (smmu->version > ARM_SMMU_V1) {
		if (PAGE_SIZE == SZ_4K)
			reg = TTBCR_TG0_4K;
		else
			reg = TTBCR_TG0_64K;

		if (!stage1) {
			reg |= (64 - smmu->s2_input_size) << TTBCR_T0SZ_SHIFT;

			switch (smmu->s2_output_size) {
			case 32:
				reg |= (TTBCR2_ADDR_32 << TTBCR_PASIZE_SHIFT);
				break;
			case 36:
				reg |= (TTBCR2_ADDR_36 << TTBCR_PASIZE_SHIFT);
				break;
			case 40:
				reg |= (TTBCR2_ADDR_40 << TTBCR_PASIZE_SHIFT);
				break;
			case 42:
				reg |= (TTBCR2_ADDR_42 << TTBCR_PASIZE_SHIFT);
				break;
			case 44:
				reg |= (TTBCR2_ADDR_44 << TTBCR_PASIZE_SHIFT);
				break;
			case 48:
				reg |= (TTBCR2_ADDR_48 << TTBCR_PASIZE_SHIFT);
				break;
			}
		} else {
			reg |= (64 - smmu->s1_input_size) << TTBCR_T0SZ_SHIFT;
		}
	} else {
		reg = 0;
	}

	reg |= TTBCR_EAE |
	      (TTBCR_SH_IS << TTBCR_SH0_SHIFT) |
	      (TTBCR_RGN_WBWA << TTBCR_ORGN0_SHIFT) |
	      (TTBCR_RGN_WBWA << TTBCR_IRGN0_SHIFT);

	if (!stage1)
		reg |= (TTBCR_SL0_LVL_1 << TTBCR_SL0_SHIFT);

	writel_relaxed(reg, cb_base + ARM_SMMU_CB_TTBCR);

	/* MAIR0 (stage-1 only) */
	if (stage1) {
		reg = (MAIR_ATTR_NC << MAIR_ATTR_SHIFT(MAIR_ATTR_IDX_NC)) |
		      (MAIR_ATTR_WBRWA << MAIR_ATTR_SHIFT(MAIR_ATTR_IDX_CACHE)) |
		      (MAIR_ATTR_DEVICE << MAIR_ATTR_SHIFT(MAIR_ATTR_IDX_DEV));
		writel_relaxed(reg, cb_base + ARM_SMMU_CB_S1_MAIR0);
	}

	/* SCTLR */
	reg = SCTLR_CFIE | SCTLR_CFRE | SCTLR_M | SCTLR_EAE_SBOP;
	if (stage1)
		reg |= SCTLR_S1_ASIDPNE;
#ifdef __BIG_ENDIAN
	reg |= SCTLR_E;
#endif
	writel_relaxed(reg, cb_base + ARM_SMMU_CB_SCTLR);
}

static int arm_smmu_init_domain_context(struct iommu_domain *domain,
					struct arm_smmu_device *smmu)
{
	int irq, start, ret = 0;
	unsigned long flags;
	struct arm_smmu_domain *smmu_domain = domain->priv;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;

	spin_lock_irqsave(&smmu_domain->lock, flags);
	if (smmu_domain->smmu)
		goto out_unlock;

	if (smmu->features & ARM_SMMU_FEAT_TRANS_NESTED) {
		/*
		 * We will likely want to change this if/when KVM gets
		 * involved.
		 */
		cfg->cbar = CBAR_TYPE_S1_TRANS_S2_BYPASS;
		start = smmu->num_s2_context_banks;
	} else if (smmu->features & ARM_SMMU_FEAT_TRANS_S1) {
		cfg->cbar = CBAR_TYPE_S1_TRANS_S2_BYPASS;
		start = smmu->num_s2_context_banks;
	} else {
		cfg->cbar = CBAR_TYPE_S2_TRANS;
		start = 0;
	}

	ret = __arm_smmu_alloc_bitmap(smmu->context_map, start,
				      smmu->num_context_banks);
	if (IS_ERR_VALUE(ret))
		goto out_unlock;

	cfg->cbndx = ret;
	if (smmu->version == ARM_SMMU_V1) {
		cfg->irptndx = atomic_inc_return(&smmu->irptndx);
		cfg->irptndx %= smmu->num_context_irqs;
	} else if (smmu->num_context_banks == smmu->num_context_irqs) {
		cfg->irptndx = cfg->cbndx;
	} else {
		cfg->irptndx = 0;
	}

	ACCESS_ONCE(smmu_domain->smmu) = smmu;
	arm_smmu_init_context_bank(smmu_domain);
	spin_unlock_irqrestore(&smmu_domain->lock, flags);

	if (smmu->num_context_irqs) {

		irq = smmu->irqs[smmu->num_global_irqs + cfg->irptndx];
		ret = request_irq(irq, arm_smmu_context_fault, IRQF_SHARED,
			  "arm-smmu-context-fault", domain);
		if (IS_ERR_VALUE(ret)) {
			dev_err(smmu->dev,
				"failed to request context IRQ %d (%u)\n",
				cfg->irptndx, irq);
			cfg->irptndx = INVALID_IRPTNDX;
		}
	}

	BUG_ON(iommu_domains[cfg->cbndx]);
	iommu_domains[cfg->cbndx] = domain;
	return 0;

out_unlock:
	spin_unlock_irqrestore(&smmu_domain->lock, flags);
	return ret;
}

static void arm_smmu_destroy_domain_context(struct iommu_domain *domain)
{
	struct arm_smmu_domain *smmu_domain = domain->priv;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	void __iomem *cb_base;
	int irq;

	if (!smmu)
		return;

	/* Disable the context bank and nuke the TLB before freeing it. */
	cb_base = ARM_SMMU_CB_BASE(smmu) + ARM_SMMU_CB(smmu, cfg->cbndx);
	writel_relaxed(0, cb_base + ARM_SMMU_CB_SCTLR);
	arm_smmu_tlb_inv_context(smmu_domain);

	if ((smmu->num_context_irqs) &&
		(cfg->irptndx != INVALID_IRPTNDX)) {
		irq = smmu->irqs[smmu->num_global_irqs + cfg->irptndx];
		free_irq(irq, domain);
	}

	__arm_smmu_free_bitmap(smmu->context_map, cfg->cbndx);
	iommu_domains[cfg->cbndx] = NULL;
}

static int arm_smmu_get_hwid(struct iommu_domain *domain,
			     struct device *dev, unsigned int id)
{
	struct arm_smmu_master_cfg *cfg;

	cfg = find_smmu_master_cfg(dev);
	if (!cfg)
		return -EINVAL;

	if (id >= cfg->num_streamids)
		return -EINVAL;

	return cfg->streamids[id];
}

static int arm_smmu_domain_init(struct iommu_domain *domain)
{
	struct arm_smmu_domain *smmu_domain;
	pgd_t *pgd;
	unsigned int order;


	/*
	 * Allocate the domain and initialise some of its data structures.
	 * We can't really do anything meaningful until we've added a
	 * master.
	 */
	smmu_domain = kzalloc(sizeof(*smmu_domain), GFP_KERNEL);
	if (!smmu_domain)
		return -ENOMEM;

	order = get_order(PAGE_ALIGN(PTRS_PER_PGD * sizeof(pgd_t)));
	pgd = (pgd_t *)__get_free_pages(GFP_KERNEL | __GFP_ZERO, order);
	if (!pgd)
		goto out_free_domain;

	smmu_domain->arm_dummy_page = alloc_page(GFP_KERNEL | __GFP_ZERO);
	if (!smmu_domain->arm_dummy_page) {
		kfree(pgd);
		goto out_free_domain;
	}

	smmu_domain->cfg.pgd = pgd;

	spin_lock_init(&smmu_domain->lock);

	domain->priv = smmu_domain;
	return 0;

out_free_domain:
	kfree(smmu_domain);
	return -ENOMEM;
}

static void arm_smmu_free_ptes(pmd_t *pmd)
{
	pgtable_t table = pmd_pgtable(*pmd);

	__free_page(table);
}

static void arm_smmu_free_pmds(pud_t *pud)
{
	int i;
	pmd_t *pmd, *pmd_base = pmd_offset(pud, 0);

	pmd = pmd_base;
	for (i = 0; i < PTRS_PER_PMD; ++i) {
		if (pmd_none(*pmd))
			continue;

		arm_smmu_free_ptes(pmd);
		pmd++;
	}

	pmd_free(NULL, pmd_base);
}

static void arm_smmu_free_puds(pgd_t *pgd)
{
	int i;
	pud_t *pud, *pud_base = pud_offset(pgd, 0);

	pud = pud_base;
	for (i = 0; i < PTRS_PER_PUD; ++i) {
		if (pud_none(*pud))
			continue;

		arm_smmu_free_pmds(pud);
		pud++;
	}

	pud_free(NULL, pud_base);
}

static void arm_smmu_free_pgtables(struct arm_smmu_domain *smmu_domain)
{
	int i;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	pgd_t *pgd, *pgd_base = cfg->pgd;

	/*
	 * Recursively free the page tables for this domain. We don't
	 * care about speculative TLB filling because the tables should
	 * not be active in any context bank at this point (SCTLR.M is 0).
	 */
	pgd = pgd_base;
	for (i = 0; i < PTRS_PER_PGD; ++i) {
		if (pgd_none(*pgd))
			continue;
		arm_smmu_free_puds(pgd);
		pgd++;
	}

	kfree(pgd_base);
}

static void arm_smmu_domain_destroy(struct iommu_domain *domain)
{
	struct arm_smmu_domain *smmu_domain = domain->priv;

	/*
	 * Free the domain resources. We assume that all devices have
	 * already been detached.
	 */
	arm_smmu_destroy_domain_context(domain);
	arm_smmu_free_pgtables(smmu_domain);
	__free_page(smmu_domain->arm_dummy_page);
	kfree(smmu_domain);
}

static int arm_smmu_master_configure_smrs(struct arm_smmu_device *smmu,
					  struct arm_smmu_master_cfg *cfg)
{
	int i;
	struct arm_smmu_smr *smrs;
	void __iomem *gr0_base = ARM_SMMU_GR0(smmu);

	if (!(smmu->features & ARM_SMMU_FEAT_STREAM_MATCH))
		return 0;

	if (cfg->smrs) {
		pr_debug("%s() cfg->smrs=%p exists\n", __func__, cfg->smrs);
		return -EEXIST;
	}

	smrs = kmalloc_array(cfg->num_streamids, sizeof(*smrs), GFP_KERNEL);
	if (!smrs) {
		dev_err(smmu->dev, "failed to allocate %d SMRs\n",
			cfg->num_streamids);
		return -ENOMEM;
	}

	/* Allocate the SMRs on the SMMU */
	for (i = 0; i < cfg->num_streamids; ++i) {
		int idx = __arm_smmu_alloc_bitmap(smmu->smr_map, 0,
						  smmu->num_mapping_groups);
		if (IS_ERR_VALUE(idx)) {
			dev_err(smmu->dev, "failed to allocate free SMR\n");
			goto err_free_smrs;
		}

		smrs[i] = (struct arm_smmu_smr) {
			.idx	= idx,
			.mask	= SMR_ID_MASK,
			.id	= cfg->streamids[i],
		};
	}

	/* It worked! Now, poke the actual hardware */
	for (i = 0; i < cfg->num_streamids; ++i) {
		u32 reg = SMR_VALID | smrs[i].id << SMR_ID_SHIFT |
			  smrs[i].mask << SMR_MASK_SHIFT;
		writel_relaxed(reg, gr0_base + ARM_SMMU_GR0_SMR(smrs[i].idx));
	}

	cfg->smrs = smrs;
	pr_debug("%s() set cfg->smrs=%p\n", __func__, cfg->smrs);
	return 0;

err_free_smrs:
	while (--i >= 0)
		__arm_smmu_free_bitmap(smmu->smr_map, smrs[i].idx);
	kfree(smrs);
	return -ENOSPC;
}

static void arm_smmu_master_free_smrs(struct arm_smmu_device *smmu,
				      struct arm_smmu_master_cfg *cfg)
{
	int i;
	void __iomem *gr0_base = ARM_SMMU_GR0(smmu);
	struct arm_smmu_smr *smrs = cfg->smrs;

	if (!smrs)
		return;

	/* Invalidate the SMRs before freeing back to the allocator */
	for (i = 0; i < cfg->num_streamids; ++i) {
		u8 idx = smrs[i].idx;

		writel_relaxed(~SMR_VALID, gr0_base + ARM_SMMU_GR0_SMR(idx));
		__arm_smmu_free_bitmap(smmu->smr_map, idx);
	}

	cfg->smrs = NULL;
	kfree(smrs);
}

#ifdef CONFIG_ARM_SMMU_WAR
/* HACK: c-model uses legacy swgroup interface */
static void tegra_smmu_conf_swgroup(struct arm_smmu_device *smmu, int swgid,
				    u8 cbndx)
{
	size_t offs;
	u32 val;

	offs = tegra_smmu_of_offset(swgid);
	val = BIT(31) | swgid; /* swgid == streamID */
	__writel(val, mc_base + offs);

	pr_info("%s() ASID_0=0x%zx val=0x%08x streamID=%d cbndx=%d\n",
		__func__, offs, val, swgid, cbndx);
}
#else
static inline void tegra_smmu_conf_swgroup(struct arm_smmu_device *smmu,
					   int swgid, u8 cbndx)
{
}
#endif

static int arm_smmu_domain_add_master(struct arm_smmu_domain *smmu_domain,
				      struct arm_smmu_master_cfg *cfg)
{
	int i, ret;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	void __iomem *gr0_base = ARM_SMMU_GR0(smmu);

	/* Devices in an IOMMU group may already be configured */
	ret = arm_smmu_master_configure_smrs(smmu, cfg);
	if (ret)
		return ret == -EEXIST ? 0 : ret;

	for (i = 0; i < cfg->num_streamids; ++i) {
		u32 idx, s2cr;

		idx = cfg->smrs ? cfg->smrs[i].idx : cfg->streamids[i];
		s2cr = S2CR_TYPE_TRANS |
		       (smmu_domain->cfg.cbndx << S2CR_CBNDX_SHIFT);
		writel_relaxed(s2cr, gr0_base + ARM_SMMU_GR0_S2CR(idx));

		if (config_enabled(CONFIG_ARM_SMMU_WAR) &&
		    tegra_platform_is_linsim())
			tegra_smmu_conf_swgroup(smmu, cfg->streamids[i],
						smmu_domain->cfg.cbndx);
	}

	return 0;
}

static void arm_smmu_domain_remove_master(struct arm_smmu_domain *smmu_domain,
					  struct arm_smmu_master_cfg *cfg)
{
	int i;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	void __iomem *gr0_base = ARM_SMMU_GR0(smmu);

	/* An IOMMU group is torn down by the first device to be removed */
	if ((smmu->features & ARM_SMMU_FEAT_STREAM_MATCH) && !cfg->smrs)
		return;

	/*
	 * We *must* clear the S2CR first, because freeing the SMR means
	 * that it can be re-allocated immediately.
	 */
	for (i = 0; i < cfg->num_streamids; ++i) {
		u32 idx = cfg->smrs ? cfg->smrs[i].idx : cfg->streamids[i];

		writel_relaxed(S2CR_TYPE_BYPASS,
			       gr0_base + ARM_SMMU_GR0_S2CR(idx));
	}

	arm_smmu_master_free_smrs(smmu, cfg);
}

static int smmu_ptdump_show(struct seq_file *s, void *unused)
{
	struct arm_smmu_domain *smmu_domain = s->private;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	int i, j, k, l;
	unsigned long addr = 0;

	pgd = cfg->pgd;
	for (i = 0; i < PTRS_PER_PGD;
		++i, pgd++, addr += PGDIR_SIZE) {
		if (pgd_none(*pgd))
			continue;
		pud = pud_offset(pgd, addr);
		for (j = 0; j < PTRS_PER_PUD; ++j, pud++) {
			if (pud_none(*pud)) {
				if ((ulong)pgd != (ulong)pud)
					addr += PUD_SIZE;
				continue;
			}
			pmd = pmd_offset(pud, addr);
			for (k = 0; k < PTRS_PER_PMD;
				++k, pmd++, addr += PMD_SIZE) {
				if (pmd_none(*pmd))
					continue;
				pte = pmd_page_vaddr(*pmd) + pte_index(addr);
				for (l = 0; l < PTRS_PER_PTE;
					++l, pte++, addr += PAGE_SIZE) {
					phys_addr_t pa;

					pa = __pfn_to_phys(pte_pfn(*pte));
					if (!pa)
						continue;
					seq_printf(s,
						   "va=0x%016lx pa=%pap *pte=%pad\n",
						   addr, &pa, &(*pte));
				}
			}
			if ((ulong)pgd != (ulong)pud)
				addr += PUD_SIZE;
		}
	}
	return 0;
}

static int smmu_ptdump_open(struct inode *inode, struct file *file)
{
	return single_open(file, smmu_ptdump_show, inode->i_private);
}

static const struct file_operations smmu_ptdump_fops = {
	.open           = smmu_ptdump_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

#define defreg_cb(_name)			\
	{					\
		.name = __stringify(_name),	\
		.offset = ARM_SMMU_CB_ ## _name,\
	}

static const struct debugfs_reg32 arm_smmu_cb_regs[] = {
	defreg_cb(SCTLR),
	defreg_cb(TTBCR2),
	defreg_cb(TTBR0_LO),
	defreg_cb(TTBR0_HI),
	defreg_cb(TTBCR),
	defreg_cb(S1_MAIR0),
	defreg_cb(FSR),
	defreg_cb(FAR_LO),
	defreg_cb(FAR_HI),
	defreg_cb(FSYNR0),
};

static ssize_t smmu_debugfs_iova2phys_write(struct file *file,
					    const char __user *buffer,
					    size_t count, loff_t *pos)
{
	int ret;
	struct iommu_domain *domain = file_inode(file)->i_private;
	struct arm_smmu_domain *smmu_domain = domain->priv;
	char str[] = "0x0123456789abcdef";
	unsigned long flags;
	dma_addr_t tmp;

	count = min_t(size_t, strlen(str), count);
	if (copy_from_user(str, buffer, count))
		return -EINVAL;

	ret = sscanf(str, "0x%16llx", &tmp);
	if (ret != 1)
		return -EINVAL;

	spin_lock_irqsave(&smmu_domain->lock, flags);
	smmu_domain->inquired_iova = tmp;
	smmu_domain->inquired_phys =
		arm_smmu_iova_to_phys(domain, smmu_domain->inquired_iova);
	pr_info("iova=%pa pa=%pa\n",
		&smmu_domain->inquired_iova, &smmu_domain->inquired_phys);
	spin_unlock_irqrestore(&smmu_domain->lock, flags);
	return count;
}

static int smmu_iova2phys_show(struct seq_file *m, void *v)
{
	struct iommu_domain *domain = m->private;
	struct arm_smmu_domain *smmu_domain = domain->priv;
	unsigned long flags;

	spin_lock_irqsave(&smmu_domain->lock, flags);

	seq_printf(m, "iova=%pa pa=%pa\n",
		   &smmu_domain->inquired_iova,
		   &smmu_domain->inquired_phys);

	spin_unlock_irqrestore(&smmu_domain->lock, flags);
	return 0;
}

static int smmu_iova2phys_open(struct inode *inode, struct file *file)
{
	return single_open(file, smmu_iova2phys_show, inode->i_private);
}

static const struct file_operations smmu_iova2phys_fops = {
	.open		= smmu_iova2phys_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= smmu_debugfs_iova2phys_write,
};

static void debugfs_create_smmu_cb(struct iommu_domain *domain,
				   struct device *dev)
{
	struct dentry *dent;
	char name[] = "cb000";
	struct debugfs_regset32	*cb;
	struct arm_smmu_domain *smmu_domain = domain->priv;
	u8 cbndx = smmu_domain->cfg.cbndx;
	struct arm_smmu_device *smmu = smmu_domain->smmu;

	sprintf(name, "cb%03d", cbndx);
	dent = debugfs_create_dir(name, smmu->debugfs_root);
	if (!dent)
		return;
	cb = smmu->regset + 1 + cbndx;
	cb->regs = arm_smmu_cb_regs;
	cb->nregs = ARRAY_SIZE(arm_smmu_cb_regs);
	cb->base = smmu->base + (smmu->size >> 1) +
		cbndx * (1 << smmu->pgshift);
	debugfs_create_regset32("regdump", S_IRUGO, dent, cb);
	debugfs_create_file("ptdump", S_IRUGO, dent, smmu_domain,
			    &smmu_ptdump_fops);
	debugfs_create_file("iova_to_phys", S_IRUSR, dent, domain,
			    &smmu_iova2phys_fops);
}

static int smmu_master_show(struct seq_file *s, void *unused)
{
	int i;
	struct arm_smmu_master *master = s->private;

	for (i = 0; i < master->cfg->num_streamids; i++)
		seq_printf(s, "streamids: % 3d ", master->cfg->streamids[i]);
	seq_printf(s, "\n");
	for (i = 0; i < master->cfg->num_streamids; i++)
		seq_printf(s, "smrs:      % 3d ", master->cfg->smrs[i].idx);
	seq_printf(s, "\n");
	return 0;
}

static int smmu_master_open(struct inode *inode, struct file *file)
{
	return single_open(file, smmu_master_show, inode->i_private);
}

static const struct file_operations smmu_master_fops = {
	.open           = smmu_master_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static void add_smmu_master_debugfs(struct iommu_domain *domain,
				    struct device *dev,
				    struct arm_smmu_master *master)
{
	struct dentry *dent;
	struct arm_smmu_domain *smmu_domain = domain->priv;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	char name[] = "cb000";
	char target[] = "../../cb000";
	u8 cbndx = smmu_domain->cfg.cbndx;

	dent = debugfs_create_dir(dev_name(dev), smmu->masters_root);
	if (!dent)
		return;

	debugfs_create_file("streamids", 0444, dent, master, &smmu_master_fops);
	debugfs_create_u8("cbndx", 0444, dent, &smmu_domain->cfg.cbndx);
	debugfs_create_smmu_cb(domain, dev);
	sprintf(name, "cb%03d", cbndx);
	sprintf(target, "../../cb%03d", cbndx);
	debugfs_create_symlink(name, dent, target);
	master->debugfs_root = dent;
}

static int arm_smmu_attach_dev(struct iommu_domain *domain, struct device *dev)
{
	int ret;
	struct arm_smmu_domain *smmu_domain = domain->priv;
	struct arm_smmu_device *smmu, *dom_smmu;
	struct arm_smmu_master_cfg *cfg;

	smmu = find_smmu_for_device(dev);
	if (!smmu) {
		dev_err(dev, "cannot attach to SMMU, is it on the same bus?\n");
		return -ENXIO;
	}

	if (dev->archdata.iommu) {
		dev_err(dev, "already attached to IOMMU domain\n");
		return -EEXIST;
	}

	/*
	 * Sanity check the domain. We don't support domains across
	 * different SMMUs.
	 */
	dom_smmu = ACCESS_ONCE(smmu_domain->smmu);
	if (!dom_smmu) {
		/* Now that we have a master, we can finalise the domain */
		ret = arm_smmu_init_domain_context(domain, smmu);
		if (IS_ERR_VALUE(ret))
			return ret;

		dom_smmu = smmu_domain->smmu;
	}

	if (dom_smmu != smmu) {
		dev_err(dev,
			"cannot attach to SMMU %s whilst already attached to domain on SMMU %s\n",
			dev_name(smmu_domain->smmu->dev), dev_name(smmu->dev));
		return -EINVAL;
	}

	/* Looks ok, so add the device to the domain */
	cfg = find_smmu_master_cfg(dev);
	if (!cfg)
		return -ENODEV;

	ret = arm_smmu_domain_add_master(smmu_domain, cfg);
	if (!ret) {
		dev->archdata.iommu = domain;
		add_smmu_master_debugfs(domain, dev,
				find_smmu_master(smmu, dev_get_dev_node(dev)));
	}
	return ret;
}

static void arm_smmu_detach_dev(struct iommu_domain *domain, struct device *dev)
{
	struct arm_smmu_domain *smmu_domain = domain->priv;
	struct arm_smmu_master_cfg *cfg;

	cfg = find_smmu_master_cfg(dev);
	if (!cfg)
		return;

	dev->archdata.iommu = NULL;
	arm_smmu_domain_remove_master(smmu_domain, cfg);
}

static bool arm_smmu_pte_is_contiguous_range(unsigned long addr,
					unsigned long end, unsigned long phys)
{
	return !(addr & ~ARM_SMMU_PTE_CONT_MASK) &&
		(addr + ARM_SMMU_PTE_CONT_SIZE <= end) &&
		!(phys & ~ARM_SMMU_PTE_CONT_MASK);
}

static int arm_smmu_alloc_init_pte(struct arm_smmu_device *smmu, pmd_t *pmd,
				   unsigned long addr, unsigned long end,
				   unsigned long pfn, int prot, int stage)
{
	pte_t *pte, *start;
	pteval_t pteval = ARM_SMMU_PTE_PAGE | ARM_SMMU_PTE_AF | ARM_SMMU_PTE_XN;

	if (pmd_none(*pmd)) {
		/* Allocate a new set of tables */
		pgtable_t table = alloc_page(GFP_ATOMIC|__GFP_ZERO);

		if (!table)
			return -ENOMEM;

		arm_smmu_flush_pgtable(smmu, page_address(table), PAGE_SIZE);
		pmd_populate(NULL, pmd, table);
		arm_smmu_flush_pgtable(smmu, pmd, sizeof(*pmd));
	}

	if (stage == 1) {
		pteval |= ARM_SMMU_PTE_AP_UNPRIV | ARM_SMMU_PTE_nG;
		if (!(prot & IOMMU_WRITE) && (prot & IOMMU_READ))
			pteval |= ARM_SMMU_PTE_AP_RDONLY;

		if (prot & IOMMU_CACHE)
			pteval |= (MAIR_ATTR_IDX_CACHE <<
				   ARM_SMMU_PTE_ATTRINDX_SHIFT);
	} else {
		pteval |= ARM_SMMU_PTE_HAP_FAULT;
		if (prot & IOMMU_READ)
			pteval |= ARM_SMMU_PTE_HAP_READ;
		if (prot & IOMMU_WRITE)
			pteval |= ARM_SMMU_PTE_HAP_WRITE;
		if (prot & IOMMU_CACHE)
			pteval |= ARM_SMMU_PTE_MEMATTR_OIWB;
		else
			pteval |= ARM_SMMU_PTE_MEMATTR_NC;
	}

	/* If no access, create a faulting entry to avoid TLB fills */
	if (prot & IOMMU_EXEC)
		pteval &= ~ARM_SMMU_PTE_XN;
	else if (!(prot & (IOMMU_READ | IOMMU_WRITE)))
		pteval &= ~ARM_SMMU_PTE_PAGE;

	pteval |= ARM_SMMU_PTE_SH_IS;
	start = pmd_page_vaddr(*pmd) + pte_index(addr);
	pte = start;

	/*
	 * Install the page table entries. This is fairly complicated
	 * since we attempt to make use of the contiguous hint in the
	 * ptes where possible. The contiguous hint indicates a series
	 * of ARM_SMMU_PTE_CONT_ENTRIES ptes mapping a physically
	 * contiguous region with the following constraints:
	 *
	 *   - The region start is aligned to ARM_SMMU_PTE_CONT_SIZE
	 *   - Each pte in the region has the contiguous hint bit set
	 *
	 * This complicates unmapping (also handled by this code, when
	 * neither IOMMU_READ or IOMMU_WRITE are set) because it is
	 * possible, yet highly unlikely, that a client may unmap only
	 * part of a contiguous range. This requires clearing of the
	 * contiguous hint bits in the range before installing the new
	 * faulting entries.
	 *
	 * Note that re-mapping an address range without first unmapping
	 * it is not supported, so TLB invalidation is not required here
	 * and is instead performed at unmap and domain-init time.
	 */
	do {
		int i = 1;

		pteval &= ~ARM_SMMU_PTE_CONT;

		if (arm_smmu_pte_is_contiguous_range(addr, end, __pfn_to_phys(pfn))) {
			i = ARM_SMMU_PTE_CONT_ENTRIES;
			pteval |= ARM_SMMU_PTE_CONT;
		} else if (pte_val(*pte) &
			   (ARM_SMMU_PTE_CONT | ARM_SMMU_PTE_PAGE)) {
			int j;
			pte_t *cont_start;
			unsigned long idx = pte_index(addr);

			idx &= ~(ARM_SMMU_PTE_CONT_ENTRIES - 1);
			cont_start = pmd_page_vaddr(*pmd) + idx;
			for (j = 0; j < ARM_SMMU_PTE_CONT_ENTRIES; ++j)
				pte_val(*(cont_start + j)) &=
					~ARM_SMMU_PTE_CONT;

			arm_smmu_flush_pgtable(smmu, cont_start,
					       sizeof(*pte) *
					       ARM_SMMU_PTE_CONT_ENTRIES);
		}

		if (!pfn) {
			memset(pte, 0, i * sizeof(*pte));
			addr += i * PAGE_SIZE;
			pte += i;
			continue;
		}

		do {
			*pte = pfn_pte(pfn, __pgprot(pteval));
		} while (pte++, pfn++, addr += PAGE_SIZE, --i);
	} while (addr != end);

	arm_smmu_flush_pgtable(smmu, start, sizeof(*pte) * (pte - start));
	return 0;
}

static int arm_smmu_alloc_init_pmd(struct arm_smmu_device *smmu, pud_t *pud,
				   unsigned long addr, unsigned long end,
				   phys_addr_t phys, int prot, int stage)
{
	int ret;
	pmd_t *pmd;
	unsigned long next, pfn = __phys_to_pfn(phys);

#ifndef __PAGETABLE_PMD_FOLDED
	if (pud_none(*pud)) {
		pmd = (pmd_t *)get_zeroed_page(GFP_ATOMIC);
		if (!pmd)
			return -ENOMEM;

		arm_smmu_flush_pgtable(smmu, pmd, PAGE_SIZE);
		pud_populate(NULL, pud, pmd);
		arm_smmu_flush_pgtable(smmu, pud, sizeof(*pud));

		pmd += pmd_index(addr);
	} else
#endif
		pmd = pmd_offset(pud, addr);

	do {
		next = pmd_addr_end(addr, end);
		ret = arm_smmu_alloc_init_pte(smmu, pmd, addr, next, pfn,
					      prot, stage);
		if (phys)
			phys += next - addr;
		pfn = __phys_to_pfn(phys);
	} while (pmd++, addr = next, addr < end);

	return ret;
}

static int arm_smmu_alloc_init_pud(struct arm_smmu_device *smmu, pgd_t *pgd,
				   unsigned long addr, unsigned long end,
				   phys_addr_t phys, int prot, int stage)
{
	int ret = 0;
	pud_t *pud;
	unsigned long next;

#ifndef __PAGETABLE_PUD_FOLDED
	if (pgd_none(*pgd)) {
		pud = (pud_t *)get_zeroed_page(GFP_ATOMIC);
		if (!pud)
			return -ENOMEM;

		arm_smmu_flush_pgtable(smmu, pud, PAGE_SIZE);
		pgd_populate(NULL, pgd, pud);
		arm_smmu_flush_pgtable(smmu, pgd, sizeof(*pgd));

		pud += pud_index(addr);
	} else
#endif
		pud = pud_offset(pgd, addr);

	do {
		next = pud_addr_end(addr, end);
		ret = arm_smmu_alloc_init_pmd(smmu, pud, addr, next, phys,
					      prot, stage);
		if (phys)
			phys += next - addr;
	} while (pud++, addr = next, addr < end);

	return ret;
}

static int arm_smmu_handle_mapping(struct arm_smmu_domain *smmu_domain,
				   unsigned long iova, phys_addr_t paddr,
				   size_t size, unsigned long attrs)
{
	int ret, stage, prot = IOMMU_WRITE | IOMMU_READ;
	unsigned long end, iova_orig = iova;
	phys_addr_t input_mask, output_mask;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	pgd_t *pgd = cfg->pgd;
	unsigned long flags;

	u64 time_before = 0;

	/* FIXME: follow the upstream prot */
	if (dma_get_attr(DMA_ATTR_READ_ONLY, (struct dma_attrs *)attrs))
		prot &= ~IOMMU_WRITE;
	else if (dma_get_attr(DMA_ATTR_WRITE_ONLY, (struct dma_attrs *)attrs))
		prot &= ~IOMMU_READ;

	if (cfg->cbar == CBAR_TYPE_S2_TRANS) {
		stage = 2;
		input_mask = (1ULL << smmu->s2_input_size) - 1;
		output_mask = (1ULL << smmu->s2_output_size) - 1;
	} else {
		stage = 1;
		input_mask = (1ULL << smmu->s1_input_size) - 1;
		output_mask = (1ULL << smmu->s1_output_size) - 1;
	}

	if (!pgd)
		return -EINVAL;

	if (size & ~PAGE_MASK)
		return -EINVAL;

	if ((phys_addr_t)iova & ~input_mask)
		return -ERANGE;

	if (paddr & ~output_mask)
		return -ERANGE;

	if (test_bit(cfg->cbndx, smmu->context_filter)) {
		pr_debug("cbndx=%d iova=%pad paddr=%pap size=%zx prot=%x skip=%d\n",
			 cfg->cbndx, &iova, &paddr, size, prot,
			 arm_smmu_skip_mapping);
	}

	if (arm_smmu_skip_mapping)
		return 0;

#ifdef CONFIG_TRACEPOINTS
	if (static_key_false(&__tracepoint_arm_smmu_handle_mapping.key)
		&& test_bit(cfg->cbndx, smmu->context_filter))
		time_before = local_clock();
#endif

	spin_lock_irqsave(&smmu_domain->lock, flags);
	pgd += pgd_index(iova);
	end = iova + size;
	do {
		unsigned long next = pgd_addr_end(iova, end);

		ret = arm_smmu_alloc_init_pud(smmu, pgd, iova, next, paddr,
					      prot, stage);
		if (ret)
			goto out_unlock;

		if (paddr)
			paddr += next - iova;
		iova = next;
	} while (pgd++, iova != end);

out_unlock:
	if (arm_smmu_tlb_inv_at_map) {
		if (arm_smmu_tlb_inv_by_addr)
			arm_smmu_tlb_inv_range(smmu_domain,
					       iova, iova - iova_orig);
		else
			arm_smmu_tlb_inv_context(smmu_domain);
	}
	spin_unlock_irqrestore(&smmu_domain->lock, flags);

	if (time_before)
		trace_arm_smmu_handle_mapping(time_before, cfg->cbndx,
			iova_orig, paddr, size, prot);

	return ret;
}

static int arm_smmu_map_sg(struct iommu_domain *domain, unsigned long iova,
			struct scatterlist *sgl, int npages, unsigned long prot)
{
	int i;
	struct scatterlist *sg;
	struct arm_smmu_domain *smmu_domain = domain->priv;

	for (i = 0, sg = sgl; i < npages; sg = sg_next(sg)) {
		int err;
		phys_addr_t pa = sg_phys(sg) & PAGE_MASK;
		unsigned int len = PAGE_ALIGN(sg->offset + sg->length);

		pr_debug("%s() iova=%pad pa=%pap size=%x\n",
			__func__, &iova, &pa, len);
		err = arm_smmu_handle_mapping(smmu_domain, iova, pa, len, prot);
		if (err)
			return err;

		i += len >> PAGE_SHIFT;
		iova += len;
	}

	return 0;
}

static int arm_smmu_map(struct iommu_domain *domain, unsigned long iova,
			phys_addr_t paddr, size_t size, unsigned long prot)
{
	struct arm_smmu_domain *smmu_domain = domain->priv;

	if (!smmu_domain)
		return -ENODEV;

	return arm_smmu_handle_mapping(smmu_domain, iova, paddr, size, prot);
}

static size_t arm_smmu_unmap(struct iommu_domain *domain, unsigned long iova,
			     size_t size)
{
	int ret;
	u64 time_before = 0;

	struct arm_smmu_domain *smmu_domain = domain->priv;
#ifdef CONFIG_TRACEPOINTS
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;

	if (static_key_false(&__tracepoint_arm_smmu_unmap.key)
		&& test_bit(cfg->cbndx, smmu->context_filter))
		time_before = local_clock();
#endif

	ret = arm_smmu_handle_mapping(smmu_domain, iova, 0, size, 0);
	if (!arm_smmu_tlb_inv_at_map) {
		if (arm_smmu_tlb_inv_by_addr)
			arm_smmu_tlb_inv_range(smmu_domain, iova, size);
		else
			arm_smmu_tlb_inv_context(smmu_domain);
	}
	if (time_before)
		trace_arm_smmu_unmap(time_before, iova, size);

	return ret ? 0 : size;
}

static phys_addr_t arm_smmu_iova_to_phys(struct iommu_domain *domain,
					 dma_addr_t iova)
{
	pgd_t *pgdp, pgd;
	pud_t pud;
	pmd_t pmd;
	pte_t pte;
	struct arm_smmu_domain *smmu_domain = domain->priv;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;

	pgdp = cfg->pgd;
	if (!pgdp)
		return 0;

	pgd = *(pgdp + pgd_index(iova));
	if (pgd_none(pgd))
		return 0;

	pud = *pud_offset(&pgd, iova);
	if (pud_none(pud))
		return 0;

	pmd = *pmd_offset(&pud, iova);
	if (pmd_none(pmd))
		return 0;

	pte = *(pmd_page_vaddr(pmd) + pte_index(iova));
	if (pte_none(pte))
		return 0;

	return __pfn_to_phys(pte_pfn(pte)) | (iova & ~PAGE_MASK);
}

static bool arm_smmu_capable(enum iommu_cap cap)
{
	switch (cap) {
	case IOMMU_CAP_CACHE_COHERENCY:
		/*
		 * Return true here as the SMMU can always send out coherent
		 * requests.
		 */
		return true;
	case IOMMU_CAP_INTR_REMAP:
		return true; /* MSIs are just memory writes */
	default:
		return false;
	}
}

/*
 * On Tegra we set the same SID for all PCI devices. As such this function does
 * not need to do anything.
 */
static int __arm_smmu_get_pci_sid(struct pci_dev *pdev, u16 alias, void *data)
{
	return 0; /* Continue walking */
}

static int arm_iommu_fault(struct iommu_domain *domain, struct device *dev,
		unsigned long iova, int flags, void *token)
{
	struct arm_smmu_domain *smmu_domain = domain->priv;
	phys_addr_t dummy = page_to_phys(smmu_domain->arm_dummy_page);
	phys_addr_t pa = arm_smmu_iova_to_phys(domain, iova);

	dev_err(dev, "%s: iova=0x%lx pa=%pa flags=0x%x cb=%d\n",
		__func__, iova, &pa, flags, smmu_domain->cfg.cbndx);

	if (arm_smmu_skip_mapping)
		arm_smmu_skip_mapping = 0;

	dev_err(dev, "%s: iova=0x%lx dummy=%pa flags=0x%x cb=%d\n",
		__func__, iova, &dummy, flags, smmu_domain->cfg.cbndx);

	arm_smmu_handle_mapping(smmu_domain, iova, dummy, PAGE_SIZE, 0);
	arm_smmu_tlb_inv_context(smmu_domain);

	return 0;
}

static int arm_smmu_add_device(struct device *dev)
{
	struct arm_smmu_device *smmu;
	struct arm_smmu_master_cfg *cfg;
	struct iommu_group *group;
	struct arm_smmu_master *master;
	struct dma_iommu_mapping *mapping;
	int ret;
	int i;
	u64 swgids = 0;

	smmu = find_smmu_for_device(dev);
	if (!smmu)
		return -ENODEV;

	group = iommu_group_alloc();
	if (IS_ERR(group)) {
		dev_err(dev, "Failed to allocate IOMMU group\n");
		return PTR_ERR(group);
	}

	/*
	 * Don't just use the dev here. That dev won't be a registered SMMU
	 * master so instead we should use the PCIe root.
	 */
	master = find_smmu_master(smmu, dev_get_dev_node(dev));
	if (!master) {
		ret = -ENODEV;
		goto out_put_group;
	}

	cfg = master->cfg;

	for (i = 0; i < cfg->num_streamids; i++)
		swgids |= BIT(cfg->streamids[i]);

	if (dev_is_pci(dev)) {
		struct pci_dev *pdev = to_pci_dev(dev);

		pci_for_each_dma_alias(pdev, __arm_smmu_get_pci_sid,
				       &cfg->streamids[0]);
	}

	if (tegra_platform_is_linsim() &&
		(swgids == BIT(TEGRA_SWGROUP_BPMP))) {
		dev_info(dev, "No support BPMP(SMMU) in linsim\n");
		return 0;
	}

	iommu_group_set_iommudata(group, cfg, NULL);
	ret = iommu_group_add_device(group, dev);
	iommu_group_put(group);
	if (ret)
		return ret;

	mapping = tegra_smmu_of_get_master_map(dev, master->cfg->streamids,
					       master->cfg->num_streamids);
	if (IS_ERR_OR_NULL(mapping)) {
		ret = PTR_ERR(mapping);
		goto out_put_group;
	}

	ret = arm_iommu_attach_device(dev, mapping);
	if (ret)
		goto err_attach_dev;

	if (smmu->options & ARM_SMMU_OPT_BROKEN_SIM_IRQ)
		iommu_set_fault_handler(mapping->domain, arm_iommu_fault, 0);

	pr_debug("Device added to SMMU: %s\n", dev_name(dev));
	return 0;

err_attach_dev:
	arm_iommu_release_mapping(mapping);
out_put_group:
	iommu_group_put(group);
	return ret;
}

static void arm_smmu_remove_device(struct device *dev)
{
	iommu_group_remove_device(dev);
}

static const struct iommu_ops arm_smmu_ops = {
	.capable	= arm_smmu_capable,
	.domain_init	= arm_smmu_domain_init,
	.domain_destroy	= arm_smmu_domain_destroy,
	.attach_dev	= arm_smmu_attach_dev,
	.detach_dev	= arm_smmu_detach_dev,
	.get_hwid	= arm_smmu_get_hwid,
	.map_sg		= arm_smmu_map_sg,
	.map		= arm_smmu_map,
	.unmap		= arm_smmu_unmap,
	.iova_to_phys	= arm_smmu_iova_to_phys,
	.add_device	= arm_smmu_add_device,
	.remove_device	= arm_smmu_remove_device,
	.pgsize_bitmap	= (SECTION_SIZE |
			   ARM_SMMU_PTE_CONT_SIZE |
			   PAGE_SIZE),
};

static void arm_smmu_device_reset(struct arm_smmu_device *smmu)
{
	void __iomem *gr0_base = ARM_SMMU_GR0(smmu);
	void __iomem *cb_base;
	int i = 0;
	u32 reg;

	/* clear global FSR */
	reg = readl_relaxed(ARM_SMMU_GR0_NS(smmu) + ARM_SMMU_GR0_sGFSR);
	writel(reg, ARM_SMMU_GR0_NS(smmu) + ARM_SMMU_GR0_sGFSR);

	/* Mark all SMRn as invalid and all S2CRn as bypass */
	for (i = 0; i < smmu->num_mapping_groups; ++i) {
		writel_relaxed(0, gr0_base + ARM_SMMU_GR0_SMR(i));
		writel_relaxed(S2CR_TYPE_BYPASS,
			gr0_base + ARM_SMMU_GR0_S2CR(i));
	}

	/* Make sure all context banks are disabled and clear CB_FSR  */
	for (i = 0; i < smmu->num_context_banks; ++i) {
		cb_base = ARM_SMMU_CB_BASE(smmu) + ARM_SMMU_CB(smmu, i);
		writel_relaxed(0, cb_base + ARM_SMMU_CB_SCTLR);
		writel_relaxed(FSR_FAULT, cb_base + ARM_SMMU_CB_FSR);
	}

	/* Invalidate the TLB, just in case */
	writel_relaxed(0, gr0_base + ARM_SMMU_GR0_STLBIALL);
	writel_relaxed(0, gr0_base + ARM_SMMU_GR0_TLBIALLH);
	writel_relaxed(0, gr0_base + ARM_SMMU_GR0_TLBIALLNSNH);

	reg = readl_relaxed(ARM_SMMU_GR0_NS(smmu) + ARM_SMMU_GR0_sCR0);

	/* Enable fault reporting */
	reg |= (sCR0_GFRE | sCR0_GFIE | sCR0_GCFGFRE | sCR0_GCFGFIE | sCR0_USFCFG);

	/* Disable TLB broadcasting. */
	reg |= (sCR0_VMIDPNE | sCR0_PTM);

	/* Enable client access, but bypass when no mapping is found */
	reg &= ~(sCR0_CLIENTPD);

	/* Disable forced broadcasting */
	reg &= ~sCR0_FB;

	/* Don't upgrade barriers */
	reg &= ~(sCR0_BSU_MASK << sCR0_BSU_SHIFT);

	/* Push the button */
	arm_smmu_tlb_sync(smmu);
	writel(reg, ARM_SMMU_GR0_NS(smmu) + ARM_SMMU_GR0_sCR0);
}

static int arm_smmu_id_size_to_bits(int size)
{
	switch (size) {
	case 0:
		return 32;
	case 1:
		return 36;
	case 2:
		return 40;
	case 3:
		return 42;
	case 4:
		return 44;
	case 5:
	default:
		return 48;
	}
}

static int arm_smmu_device_cfg_probe(struct arm_smmu_device *smmu)
{
	unsigned long size;
	void __iomem *gr0_base = ARM_SMMU_GR0(smmu);
	u32 id;

	dev_notice(smmu->dev, "probing hardware configuration...\n");
	dev_notice(smmu->dev, "SMMUv%d with:\n", smmu->version);

	/* ID0 */
	id = readl_relaxed(gr0_base + ARM_SMMU_GR0_ID0);
#ifndef CONFIG_64BIT
	if (((id >> ID0_PTFS_SHIFT) & ID0_PTFS_MASK) == ID0_PTFS_V8_ONLY) {
		dev_err(smmu->dev, "\tno v7 descriptor support!\n");
		return -ENODEV;
	}
#endif

	/* Restrict available stages based on module parameter */
	if (force_stage == 1)
		id &= ~(ID0_S2TS | ID0_NTS);
	else if (force_stage == 2)
		id &= ~(ID0_S1TS | ID0_NTS);

	if (id & ID0_S1TS) {
		smmu->features |= ARM_SMMU_FEAT_TRANS_S1;
		dev_notice(smmu->dev, "\tstage 1 translation\n");
	}

	if (id & ID0_S2TS) {
		smmu->features |= ARM_SMMU_FEAT_TRANS_S2;
		dev_notice(smmu->dev, "\tstage 2 translation\n");
	}

	if (id & ID0_NTS) {
		smmu->features |= ARM_SMMU_FEAT_TRANS_NESTED;
		dev_notice(smmu->dev, "\tnested translation\n");
	}

	if (!(smmu->features &
		(ARM_SMMU_FEAT_TRANS_S1 | ARM_SMMU_FEAT_TRANS_S2))) {
		dev_err(smmu->dev, "\tno translation support!\n");
		return -ENODEV;
	}

	if (id & ID0_CTTW) {
		smmu->features |= ARM_SMMU_FEAT_COHERENT_WALK;
		dev_notice(smmu->dev, "\tcoherent table walk\n");
	}

	smmu->num_mapping_groups = (id >> ID0_NUMSMRG_SHIFT) &
		ID0_NUMSMRG_MASK;

	if (id & ID0_SMS) {
		u32 smr, sid, mask;

		smmu->features |= ARM_SMMU_FEAT_STREAM_MATCH;
		if (smmu->num_mapping_groups == 0) {
			dev_err(smmu->dev,
				"stream-matching supported, but no SMRs present!\n");
			return -ENODEV;
		}

		smr = SMR_MASK_MASK << SMR_MASK_SHIFT;
		smr |= (SMR_ID_MASK << SMR_ID_SHIFT);
		writel_relaxed(smr, gr0_base + ARM_SMMU_GR0_SMR(0));
		smr = readl_relaxed(gr0_base + ARM_SMMU_GR0_SMR(0));

		mask = (smr >> SMR_MASK_SHIFT) & SMR_MASK_MASK;
		sid = (smr >> SMR_ID_SHIFT) & SMR_ID_MASK;
		if ((mask & sid) != sid) {
			dev_err(smmu->dev,
				"SMR mask bits (0x%x) insufficient for ID field (0x%x)\n",
				mask, sid);
			return -ENODEV;
		}

		dev_notice(smmu->dev,
			   "\tstream matching with %u register groups, mask 0x%x",
			   smmu->num_mapping_groups, mask);
	} else {
		dev_notice(smmu->dev,
			   "\tstream indexing with %u indices",
			   smmu->num_mapping_groups);
	}

	/* ID1 */
	id = readl_relaxed(gr0_base + ARM_SMMU_GR0_ID1);
	smmu->pgshift = (id & ID1_PAGESIZE) ? 16 : 12;

	/* Check for size mismatch of SMMU address space from mapped region */
	size = 1 <<
		(((id >> ID1_NUMPAGENDXB_SHIFT) & ID1_NUMPAGENDXB_MASK) + 1);
	size *= 2 << smmu->pgshift;
	if (smmu->size != size) {
		dev_info(smmu->dev,
			"SMMU address space size (0x%lx) differs from mapped region size (0x%lx)!\n",
			size, smmu->size);
		smmu->size = size;
	}

	smmu->num_s2_context_banks = (id >> ID1_NUMS2CB_SHIFT) &
				      ID1_NUMS2CB_MASK;
	smmu->num_context_banks = (id >> ID1_NUMCB_SHIFT) & ID1_NUMCB_MASK;
	if (smmu->num_s2_context_banks > smmu->num_context_banks) {
		dev_err(smmu->dev, "impossible number of S2 context banks!\n");
		return -ENODEV;
	}
	dev_notice(smmu->dev, "\t%u context banks (%u stage-2 only)\n",
		   smmu->num_context_banks, smmu->num_s2_context_banks);

	/* ID2 */
	id = readl_relaxed(gr0_base + ARM_SMMU_GR0_ID2);
	size = arm_smmu_id_size_to_bits((id >> ID2_IAS_SHIFT) & ID2_IAS_MASK);
	smmu->s1_output_size = min_t(unsigned long, PHYS_MASK_SHIFT, size);

	/* Stage-2 input size limited due to pgd allocation (PTRS_PER_PGD) */
#ifdef CONFIG_64BIT
	smmu->s2_input_size = min_t(unsigned long, VA_BITS, size);
#else
	smmu->s2_input_size = min(32UL, size);
#endif

	/* The stage-2 output mask is also applied for bypass */
	size = arm_smmu_id_size_to_bits((id >> ID2_OAS_SHIFT) & ID2_OAS_MASK);
	smmu->s2_output_size = min_t(unsigned long, PHYS_MASK_SHIFT, size);

	if (smmu->version == ARM_SMMU_V1) {
		smmu->s1_input_size = 32;
	} else {
#ifdef CONFIG_64BIT
		size = (id >> ID2_UBS_SHIFT) & ID2_UBS_MASK;
		size = min(VA_BITS, arm_smmu_id_size_to_bits(size));
#else
		size = 32;
#endif
		smmu->s1_input_size = size;

		if ((PAGE_SIZE == SZ_4K && !(id & ID2_PTFS_4K)) ||
		    (PAGE_SIZE == SZ_64K && !(id & ID2_PTFS_64K)) ||
		    (PAGE_SIZE != SZ_4K && PAGE_SIZE != SZ_64K)) {
			dev_err(smmu->dev, "CPU page size 0x%lx unsupported\n",
				PAGE_SIZE);
			return -ENODEV;
		}
	}

	if (smmu->features & ARM_SMMU_FEAT_TRANS_S1)
		dev_notice(smmu->dev, "\tStage-1: %lu-bit VA -> %lu-bit IPA\n",
			   smmu->s1_input_size, smmu->s1_output_size);

	if (smmu->features & ARM_SMMU_FEAT_TRANS_S2)
		dev_notice(smmu->dev, "\tStage-2: %lu-bit IPA -> %lu-bit PA\n",
			   smmu->s2_input_size, smmu->s2_output_size);

	return 0;
}

#define defreg(_name)				\
	{					\
		.name = __stringify(_name),	\
		.offset = ARM_SMMU_ ## _name,	\
	}
#define defreg_gr0(_name) defreg(GR0_ ## _name)

static const struct debugfs_reg32 arm_smmu_gr0_regs[] = {
	defreg_gr0(sCR0),
	defreg_gr0(ID0),
	defreg_gr0(ID1),
	defreg_gr0(ID2),
	defreg_gr0(sGFSR),
	defreg_gr0(sGFSYNR0),
	defreg_gr0(sGFSYNR1),
	defreg_gr0(sTLBGSTATUS),
	defreg_gr0(nsCR0),
	defreg_gr0(nsGFSR),
	defreg_gr0(nsGFSYNR0),
	defreg_gr0(nsGFSYNR1),
	defreg_gr0(nsTLBGSTATUS),
	defreg_gr0(PIDR2),
};

#define defreg_gnsr0(_name) defreg(GNSR0_ ## _name)

static const struct debugfs_reg32 arm_smmu_gnsr0_regs[] = {
	defreg_gnsr0(PMCNTENSET_0),
	defreg_gnsr0(PMCNTENCLR_0),
	defreg_gnsr0(PMINTENSET_0),
	defreg_gnsr0(PMINTENCLR_0),
	defreg_gnsr0(PMOVSCLR_0),
	defreg_gnsr0(PMOVSSET_0),
	defreg_gnsr0(PMCFGR_0),
	defreg_gnsr0(PMCR_0),
	defreg_gnsr0(PMCEID0_0),
	defreg_gnsr0(PMAUTHSTATUS_0),
	defreg_gnsr0(PMDEVTYPE_0)
};

static ssize_t smmu_context_filter_write(struct file *file,
					 const char __user *user_buf,
					 size_t count, loff_t *ppos)
{
	s16 cbndx;
	char *pbuf, *temp, *val;
	bool first_times = 1;
	ssize_t ret = count;
	struct seq_file *seqf = file->private_data;
	struct arm_smmu_device *smmu = seqf->private;
	unsigned long *bitmap = smmu->context_filter;

	/* Clear bitmap in case of user buf empty */
	if (count == 1 && *user_buf == '\n') {
		bitmap_zero(bitmap, smmu->num_context_banks);
		return ret;
	}

	pbuf = vmalloc(count + 1);
	if (!pbuf)
		return -ENOMEM;

	strncpy(pbuf, user_buf, count);

	if (pbuf[count - 1] == '\n')
		pbuf[count - 1] = '\0';
	else
		pbuf[count] = '\0';

	temp = pbuf;

	do {
		val = strsep(&temp, ",");
		if (*val) {
			if (kstrtos16(val, 10, &cbndx))
				continue;

			/* Reset bitmap in case of negative index */
			if (cbndx < 0) {
				bitmap_fill(bitmap, smmu->num_context_banks);
				goto end;
			}

			if (cbndx >= smmu->num_context_banks) {
				dev_err(smmu->dev,
					"context filter index out of range\n");
				ret = -EINVAL;
				goto end;
			}

			if (first_times) {
				bitmap_zero(bitmap, smmu->num_context_banks);
				first_times = 0;
			}

			set_bit(cbndx, bitmap);
		}
	} while (temp);

end:
	vfree(pbuf);
	return ret;
}

static int smmu_context_filter_show(struct seq_file *s, void *unused)
{
	struct arm_smmu_device *smmu = s->private;
	unsigned long *bitmap = smmu->context_filter;
	int idx = 0;

	while (1) {
		idx = find_next_bit(bitmap, ARM_SMMU_MAX_CBS, idx);
		if (idx >= smmu->num_context_banks)
			break;
		seq_printf(s, "%d,", idx);
		idx++;
	}
	seq_putc(s, '\n');
	return 0;
}

static int smmu_context_filter_open(struct inode *inode, struct file *file)
{
	return single_open(file, smmu_context_filter_show, inode->i_private);
}

static const struct file_operations smmu_context_filter_fops = {
	.open		= smmu_context_filter_open,
	.read		= seq_read,
	.write		= smmu_context_filter_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int smmu_reg32_debugfs_set(void *data, u64 val)
{
	struct debugfs_reg32 *regs = (struct debugfs_reg32 *)data;

	writel(val, (smmu_handle->base + regs->offset));
	return 0;
}

static int smmu_reg32_debugfs_get(void *data, u64 *val)
{
	struct debugfs_reg32 *regs = (struct debugfs_reg32 *)data;

	*val = readl(smmu_handle->base + regs->offset);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(smmu_reg32_debugfs_fops,
			smmu_reg32_debugfs_get,
			smmu_reg32_debugfs_set, "%08llx\n");

static int smmu_perf_regset_debugfs_set(void *data, u64 val)
{
	struct debugfs_reg32 *regs = (struct debugfs_reg32 *)data;

	writel(val, (smmu_handle->perf_regset->base + regs->offset));
	return 0;
}

static int smmu_perf_regset_debugfs_get(void *data, u64 *val)
{
	struct debugfs_reg32 *regs = (struct debugfs_reg32 *)data;

	*val = readl(smmu_handle->perf_regset->base + regs->offset);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(smmu_perf_regset_debugfs_fops,
			smmu_perf_regset_debugfs_get,
			smmu_perf_regset_debugfs_set, "%08llx\n");

static void arm_smmu_debugfs_delete(struct arm_smmu_device *smmu)
{
	int i;

	if (smmu->regset) {
		const struct debugfs_reg32 *regs = smmu->regset->regs;

		regs += ARRAY_SIZE(arm_smmu_gr0_regs);
		for (i = 0; i < 4 * smmu->num_context_banks; i++)
			kfree(regs[i].name);

		kfree(smmu->regset);
	}

	if (smmu->perf_regset) {
		const struct debugfs_reg32 *regs = smmu->perf_regset->regs;

		i = ARRAY_SIZE(arm_smmu_gnsr0_regs);
		for (; i < smmu->perf_regset->nregs ; i++)
			kfree(regs[i].name);

		kfree(smmu->perf_regset);
		smmu->perf_regset = NULL;
	}

	debugfs_remove_recursive(smmu->debugfs_root);
}

static void arm_smmu_debugfs_create(struct arm_smmu_device *smmu)
{
	int i;
	struct debugfs_reg32 *regs;
	size_t bytes;
	struct dentry *dent_gr, *dent_gnsr;

	smmu->debugfs_root = debugfs_create_dir(dev_name(smmu->dev), NULL);
	if (!smmu->debugfs_root)
		return;

	dent_gr = debugfs_create_dir("gr", smmu->debugfs_root);
	if (!dent_gr)
		goto err_out;

	dent_gnsr = debugfs_create_dir("gnsr", smmu->debugfs_root);
	if (!dent_gnsr)
		goto err_out;

	smmu->masters_root = debugfs_create_dir("masters", smmu->debugfs_root);
	if (!smmu->masters_root)
		goto err_out;

	bytes = (smmu->num_context_banks + 1) * sizeof(*smmu->regset);
	bytes += ARRAY_SIZE(arm_smmu_gr0_regs) * sizeof(*regs);
	bytes += 4 * smmu->num_context_banks * sizeof(*regs);
	smmu->regset = kzalloc(bytes, GFP_KERNEL);
	if (!smmu->regset)
		goto err_out;

	smmu->regset->base = smmu->base;
	smmu->regset->nregs = ARRAY_SIZE(arm_smmu_gr0_regs) +
		4 * smmu->num_context_banks;
	smmu->regset->regs = (struct debugfs_reg32 *)(smmu->regset +
						smmu->num_context_banks + 1);
	regs = (struct debugfs_reg32 *)smmu->regset->regs;
	for (i = 0; i < ARRAY_SIZE(arm_smmu_gr0_regs); i++) {
		regs->name = arm_smmu_gr0_regs[i].name;
		regs->offset = arm_smmu_gr0_regs[i].offset;
		regs++;
	}

	for (i = 0; i < smmu->num_context_banks; i++) {
		regs->name = kasprintf(GFP_KERNEL, "GR0_SMR%03d", i);
		if (!regs->name)
			goto err_out;
		regs->offset = ARM_SMMU_GR0_SMR(i);
		regs++;

		regs->name = kasprintf(GFP_KERNEL, "GR0_S2CR%03d", i);
		if (!regs->name)
			goto err_out;
		regs->offset = ARM_SMMU_GR0_S2CR(i);
		regs++;

		regs->name = kasprintf(GFP_KERNEL, "GR1_CBAR%03d", i);
		if (!regs->name)
			goto err_out;
		regs->offset = (1 << smmu->pgshift) + ARM_SMMU_GR1_CBAR(i);
		regs++;

		regs->name = kasprintf(GFP_KERNEL, "GR1_CBA2R%03d", i);
		if (!regs->name)
			goto err_out;
		regs->offset = (1 << smmu->pgshift) + ARM_SMMU_GR1_CBA2R(i);
		regs++;
	}

	regs = (struct debugfs_reg32 *)smmu->regset->regs;
	for (i = 0; i < smmu->regset->nregs; i++) {
		debugfs_create_file(regs->name, S_IRUGO | S_IWUSR,
				dent_gr, regs, &smmu_reg32_debugfs_fops);
		regs++;
	}

	debugfs_create_regset32("regdump", S_IRUGO, smmu->debugfs_root,
				smmu->regset);

	bytes = sizeof(*smmu->perf_regset);
	bytes += ARRAY_SIZE(arm_smmu_gnsr0_regs) * sizeof(*regs);
	/*
	 * Account the number of bytes for two sets of
	 * counter group registers
	 */
	bytes += 2 * PMCG_SIZE * sizeof(*regs);
	/*
	 * Account the number of bytes for two sets of
	 * event counter registers
	 */
	bytes += 2 * PMEV_SIZE * sizeof(*regs);

	/* Allocate memory for Perf Monitor registers */
	smmu->perf_regset =  kzalloc(bytes, GFP_KERNEL);
	if (!smmu->perf_regset)
		goto err_out;

	/*
	 * perf_regset base address is placed at offset (3 * smmu_pagesize)
	 * from smmu->base address
	 */
	smmu->perf_regset->base = smmu->base + 3 * (1 << smmu->pgshift);
	smmu->perf_regset->nregs = ARRAY_SIZE(arm_smmu_gnsr0_regs) +
		2 * PMCG_SIZE + 2 * PMEV_SIZE;
	smmu->perf_regset->regs =
		(struct debugfs_reg32 *)(smmu->perf_regset + 1);

	regs = (struct debugfs_reg32 *)smmu->perf_regset->regs;

	for (i = 0; i < ARRAY_SIZE(arm_smmu_gnsr0_regs); i++) {
		regs->name = arm_smmu_gnsr0_regs[i].name;
		regs->offset = arm_smmu_gnsr0_regs[i].offset;
		regs++;
	}

	for (i = 0; i < PMEV_SIZE; i++) {
		regs->name = kasprintf(GFP_KERNEL, "GNSR0_PMEVTYPER%d_0", i);
		if (!regs->name)
			goto err_out;
		regs->offset = ARM_SMMU_GNSR0_PMEVTYPER(i);
		regs++;

		regs->name = kasprintf(GFP_KERNEL, "GNSR0_PMEVCNTR%d_0", i);
		if (!regs->name)
			goto err_out;
		regs->offset = ARM_SMMU_GNSR0_PMEVCNTR(i);
		regs++;
	}

	for (i = 0; i < PMCG_SIZE; i++) {
		regs->name = kasprintf(GFP_KERNEL, "GNSR0_PMCGCR%d_0", i);
		if (!regs->name)
			goto err_out;
		regs->offset = ARM_SMMU_GNSR0_PMCGCR(i);
		regs++;

		regs->name = kasprintf(GFP_KERNEL, "GNSR0_PMCGSMR%d_0", i);
		if (!regs->name)
			goto err_out;
		regs->offset = ARM_SMMU_GNSR0_PMCGSMR(i);
		regs++;
	}

	regs = (struct debugfs_reg32 *)smmu->perf_regset->regs;
	for (i = 0; i < smmu->perf_regset->nregs; i++) {
		debugfs_create_file(regs->name, S_IRUGO | S_IWUSR,
				dent_gnsr, regs, &smmu_perf_regset_debugfs_fops);
		regs++;
	}

	debugfs_create_file("context_filter", S_IRUGO | S_IWUSR,
			    smmu->debugfs_root, smmu,
			    &smmu_context_filter_fops);
	debugfs_create_bool("skip_mapping",  S_IRUGO | S_IWUSR,
			    smmu->debugfs_root, &arm_smmu_skip_mapping);
	debugfs_create_bool("gr0_tlbiallnsnh",  S_IRUGO | S_IWUSR,
			smmu->debugfs_root, &arm_smmu_gr0_tlbiallnsnh);
	debugfs_create_bool("tlb_inv_by_addr",  S_IRUGO | S_IWUSR,
			smmu->debugfs_root, &arm_smmu_tlb_inv_by_addr);
	debugfs_create_bool("tlb_inv_at_map",  S_IRUGO | S_IWUSR,
			smmu->debugfs_root, &arm_smmu_tlb_inv_at_map);
	return;

err_out:
	arm_smmu_debugfs_delete(smmu);
}

static const struct of_device_id arm_smmu_of_match[] = {
	{ .compatible = "arm,smmu-v1", .data = (void *)ARM_SMMU_V1 },
	{ .compatible = "arm,smmu-v2", .data = (void *)ARM_SMMU_V2 },
	{ .compatible = "arm,mmu-400", .data = (void *)ARM_SMMU_V1 },
	{ .compatible = "arm,mmu-401", .data = (void *)ARM_SMMU_V1 },
	{ .compatible = "arm,mmu-500", .data = (void *)ARM_SMMU_V2 },
	{ },
};
MODULE_DEVICE_TABLE(of, arm_smmu_of_match);

static int arm_smmu_device_dt_probe(struct platform_device *pdev)
{
	const struct of_device_id *of_id;
	struct resource *res;
	struct arm_smmu_device *smmu;
	struct device *dev = &pdev->dev;
	struct rb_node *node;
	struct of_phandle_args masterspec;
	int num_irqs, i, err;

	if (tegra_platform_is_unit_fpga())
		return -ENODEV;

	smmu = devm_kzalloc(dev, sizeof(*smmu), GFP_KERNEL);
	if (!smmu) {
		dev_err(dev, "failed to allocate arm_smmu_device\n");
		return -ENOMEM;
	}
	smmu_handle = smmu;
	smmu->dev = dev;

	of_id = of_match_node(arm_smmu_of_match, dev->of_node);
	smmu->version = (enum arm_smmu_arch_version)of_id->data;

	err = tegra_smmu_of_parse_sids(smmu->dev);
	if (err) {
		pr_err("Unable to parse tegra SIDs!\n");
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	smmu->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(smmu->base))
		return PTR_ERR(smmu->base);
	smmu->size = resource_size(res);

	if (of_property_read_u32(dev->of_node, "#global-interrupts",
				 &smmu->num_global_irqs)) {
		dev_err(dev, "missing #global-interrupts property\n");
		return -ENODEV;
	}

	num_irqs = 0;
	while ((res = platform_get_resource(pdev, IORESOURCE_IRQ, num_irqs))) {
		num_irqs++;
		if (num_irqs > smmu->num_global_irqs)
			smmu->num_context_irqs++;
	}

	if (!smmu->num_context_irqs) {
		dev_err(dev, "found %d interrupts but expected at least %d\n",
			num_irqs, smmu->num_global_irqs + 1);
	}

	smmu->irqs = devm_kzalloc(dev, sizeof(*smmu->irqs) * num_irqs,
				  GFP_KERNEL);
	if (!smmu->irqs) {
		dev_err(dev, "failed to allocate %d irqs\n", num_irqs);
		return -ENOMEM;
	}

	for (i = 0; i < num_irqs; ++i) {
		int irq = platform_get_irq(pdev, i);

		if (irq < 0) {
			dev_err(dev, "failed to get irq index %d\n", i);
			return -ENODEV;
		}
		smmu->irqs[i] = irq;
	}

	err = arm_smmu_device_cfg_probe(smmu);
	if (err)
		return err;

	bitmap_fill(smmu->context_filter, smmu->num_context_banks);

	i = 0;
	smmu->masters = RB_ROOT;
	while (!of_parse_phandle_with_args(dev->of_node, "mmu-masters",
					   "#stream-id-cells", i,
					   &masterspec)) {

		dev_dbg(dev, "%s() masterspec.np->name=%s\n",
			__func__, masterspec.np->name);
		err = register_smmu_master(smmu, dev, &masterspec);
		if (err) {
			dev_err(dev, "failed to add master %s\n",
				masterspec.np->name);
			goto out_put_masters;
		}

		i++;
	}
	dev_notice(dev, "registered %d master devices\n", i);

	parse_driver_options(smmu);

	if (smmu->version > ARM_SMMU_V1 &&
	    smmu->num_context_banks != smmu->num_context_irqs) {
		dev_info(dev,
			"found only %d context interrupt(s) but %d required\n",
			smmu->num_context_irqs, smmu->num_context_banks);
	}

	for (i = 0; i < smmu->num_global_irqs; ++i) {
		err = request_irq(smmu->irqs[i],
				  arm_smmu_global_fault,
				  IRQF_SHARED,
				  "arm-smmu global fault",
				  smmu);
		if (err) {
			dev_err(dev, "failed to request global IRQ %d (%u)\n",
				i, smmu->irqs[i]);
			goto out_free_irqs;
		}
	}

	INIT_LIST_HEAD(&smmu->list);
	spin_lock(&arm_smmu_devices_lock);
	list_add(&smmu->list, &arm_smmu_devices);
	spin_unlock(&arm_smmu_devices_lock);

	arm_smmu_device_reset(smmu);
	arm_smmu_debugfs_create(smmu);
	return 0;

out_free_irqs:
	while (i--)
		free_irq(smmu->irqs[i], smmu);

out_put_masters:
	for (node = rb_first(&smmu->masters); node; node = rb_next(node)) {
		struct arm_smmu_master *master
			= container_of(node, struct arm_smmu_master, node);
		of_node_put(master->of_node);
	}

	return err;
}

static int arm_smmu_device_remove(struct platform_device *pdev)
{
	int i;
	struct device *dev = &pdev->dev;
	struct arm_smmu_device *curr, *smmu = NULL;
	struct rb_node *node;

	spin_lock(&arm_smmu_devices_lock);
	list_for_each_entry(curr, &arm_smmu_devices, list) {
		if (curr->dev == dev) {
			smmu = curr;
			list_del(&smmu->list);
			break;
		}
	}
	spin_unlock(&arm_smmu_devices_lock);

	if (!smmu)
		return -ENODEV;

	arm_smmu_debugfs_delete(smmu);

	for (node = rb_first(&smmu->masters); node; node = rb_next(node)) {
		struct arm_smmu_master *master
			= container_of(node, struct arm_smmu_master, node);
		of_node_put(master->of_node);
	}

	if (!bitmap_empty(smmu->context_map, ARM_SMMU_MAX_CBS))
		dev_err(dev, "removing device with active domains!\n");

	for (i = 0; i < smmu->num_global_irqs; ++i)
		free_irq(smmu->irqs[i], smmu);

	/* Turn the thing off */
	writel(sCR0_CLIENTPD, ARM_SMMU_GR0_NS(smmu) + ARM_SMMU_GR0_sCR0);
	return 0;
}

static struct platform_driver arm_smmu_driver = {
	.driver	= {
		.owner		= THIS_MODULE,
		.name		= "arm-smmu",
		.of_match_table	= of_match_ptr(arm_smmu_of_match),
		.suppress_bind_attrs = true,
	},
	.probe	= arm_smmu_device_dt_probe,
	.remove	= arm_smmu_device_remove,
};

static int __init arm_smmu_init(void)
{
	int ret;

#ifdef CONFIG_ARM_SMMU_WAR
	if (tegra_platform_is_linsim()) {
		mc_base = ioremap_nocache(MC_BASE, MC_SIZE);
		if (!mc_base)
			return -EINVAL;

		pr_info("%s(): 0x%08x is mapped to %p\n",
			__func__, MC_BASE, mc_base);
		__writel(SMMU_CONFIG_ENABLE, mc_base + SMMU_CONFIG);
	}
#endif
	ret = platform_driver_register(&arm_smmu_driver);
	if (ret)
		return ret;

	/* Oh, for a proper bus abstraction */
	if (!iommu_present(&platform_bus_type))
		bus_set_iommu(&platform_bus_type, &arm_smmu_ops);

#ifdef CONFIG_ARM_AMBA
	if (!iommu_present(&amba_bustype))
		bus_set_iommu(&amba_bustype, &arm_smmu_ops);
#endif

#ifdef CONFIG_PCI
	if (!iommu_present(&pci_bus_type))
		bus_set_iommu(&pci_bus_type, &arm_smmu_ops);
#endif

	return 0;
}

static void __exit arm_smmu_exit(void)
{
	return platform_driver_unregister(&arm_smmu_driver);
}

subsys_initcall(arm_smmu_init);
module_exit(arm_smmu_exit);

MODULE_DESCRIPTION("IOMMU API for ARM architected SMMU implementations");
MODULE_AUTHOR("Will Deacon <will.deacon@arm.com>");
MODULE_LICENSE("GPL v2");
