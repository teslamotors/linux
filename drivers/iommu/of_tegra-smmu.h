/*
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _OF_TEGRA_SMMU_H
#define _OF_TEGRA_SMMU_H

#include <linux/of.h>

struct smmu_map_prop {
	u64 swgid_mask;
	u64 iova_start;
	u64 iova_size;
	u32 alignment;
	u32 num_pf_page;
	u32 gap_page;
	struct iommu_linear_map *area;
	struct list_head list;
	struct dma_iommu_mapping *map;

	/*
	 * ARM SMMU uses SIDs not SWGIDs. Define an array here since the amount
	 * of memory used here is much less than the minimum granularity of
	 * kmalloc() allocations.
	 */
	u16 sid_list[MAX_PHANDLE_ARGS];
	int nr_sids;
};

size_t tegra_smmu_of_offset(int swgroupid);

struct dma_iommu_mapping *tegra_smmu_of_get_mapping(struct device *dev,
						    u64 swgids,
						    struct list_head *asprops);

int tegra_smmu_of_register_asprops(struct device *dev,
				   struct list_head *asprops);

u64 tegra_smmu_of_get_swgids(struct device *dev,
			       const struct of_device_id *matches,
			       struct iommu_linear_map **area);

int tegra_smmu_of_parse_sids(struct device *dev);
struct dma_iommu_mapping *tegra_smmu_of_get_master_map(struct device *dev,
						       u16 *sids, int nr_sids);

#endif /* _OF_TEGRA_SMMU_H */
