/*
 * include/linux/tegra_smmu.h
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

#ifndef __INCLUDE_LINUX_TEGRA_SMMU_H
#define __INCLUDE_LINUX_TEGRA_SMMU_H

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

#ifdef CONFIG_PLATFORM_ENABLE_IOMMU
/* Maximum number of iommu address spaces in the system */
#define TEGRA_IOMMU_NUM_ASIDS NUM_ASIDS
void tegra_smmu_unmap_misc_device(struct device *dev);
void tegra_smmu_map_misc_device(struct device *dev);
#else
#define TEGRA_IOMMU_NUM_ASIDS 1

static inline void tegra_smmu_unmap_misc_device(struct device *dev)
{
}

static inline void tegra_smmu_map_misc_device(struct device *dev)
{
}

#endif


extern u64 tegra_smmu_fixup_swgids(struct device *dev,
				   struct iommu_linear_map **map);

#endif /* __INCLUDE_LINUX_TEGRA_SMMU_H */
