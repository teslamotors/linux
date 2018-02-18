/*
 * arch/arm/mach-tegra/include/mach/tegra_smmu.h
 *
 * Copyright (c) 2011-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ARCH_ARM_MACH_TEGRA_SMMU_H
#define __ARCH_ARM_MACH_TEGRA_SMMU_H

#if defined(CONFIG_TEGRA_IOMMU_SMMU)
int tegra_smmu_save(void);
int tegra_smmu_restore(void);
#else
static inline int tegra_smmu_save(void) { return 0; }
static inline int tegra_smmu_restore(void) { return 0; }
#endif

struct iommu_linear_map {
	dma_addr_t start;
	size_t size;
};

/* FIXME: No need to expose */
extern struct notifier_block tegra_smmu_device_pci_nb;

#ifdef CONFIG_PLATFORM_ENABLE_IOMMU

void tegra_smmu_unmap_misc_device(struct device *dev);
void tegra_smmu_map_misc_device(struct device *dev);
int tegra_smmu_get_asid(struct device *dev);
#else

static inline void tegra_smmu_unmap_misc_device(struct device *dev)
{
}

static inline void tegra_smmu_map_misc_device(struct device *dev)
{
}

static inline int tegra_smmu_get_asid(struct device *dev)
{
	return 0;
}
#endif


extern u64 tegra_smmu_fixup_swgids(struct device *dev,
				   struct iommu_linear_map **map);

#endif /* __ARCH_ARM_MACH_TEGRA_SMMU_H */
