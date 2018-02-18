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

#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/gfp.h>
#include <linux/pci.h>
#include <linux/tegra_smmu.h>

#include <asm/dma-iommu.h>

#include <dt-bindings/memory/tegra-swgroup.h>

#include "of_tegra-smmu.h"

/*
 * The ARM SMMUv2 supports at most 128 SIDs.
 */
#define SMMU_MAX_SIDS	128

#define SMMU_AFI_ASID		0x238   /* PCIE */
#define SMMU_SWGRP_ASID_BASE	SMMU_AFI_ASID

/* Anonymous address space(AS) attribute */
#define ANON_IOVA_START	SZ_2G
#define ANON_IOVA_SIZE	SZ_1G
#define ANON_IOVA_ALIGN	0
#define ANON_IOVA_PF	0
#define ANON_IOVA_GAP	1

static LIST_HEAD(smmu_addr_spaces);

size_t tegra_smmu_of_offset(int id)
{
	switch (id) {
	case TEGRA_SWGROUP_DC14:
		return 0x490;
	case TEGRA_SWGROUP_DC12:
		return 0xa88;
	case TEGRA_SWGROUP_AFI...TEGRA_SWGROUP_ISP:
	case TEGRA_SWGROUP_MPE...TEGRA_SWGROUP_PPCS1:
		return (id - TEGRA_SWGROUP_AFI) * sizeof(u32) + SMMU_AFI_ASID;
	case TEGRA_SWGROUP_SDMMC1A...63:
		return (id - TEGRA_SWGROUP_SDMMC1A) * sizeof(u32) + 0xa94;
	};

	BUG();
}

static struct dma_iommu_mapping *tegra_smmu_of_populate_mapping(
	struct device *dev,
	struct smmu_map_prop *prop)
{
	struct dma_iommu_mapping *map;

	map = arm_iommu_create_mapping(&platform_bus_type,
				       (dma_addr_t)prop->iova_start,
				       (size_t)prop->iova_size);
	if (IS_ERR(map)) {
		dev_err(dev, "fail to create iommu map prop=%p\n", prop);
		return NULL;
	}

	/* FIXME: residual data */
	map->alignment = prop->alignment;
	map->gap_page = !!prop->gap_page;
	map->num_pf_page = prop->num_pf_page;

	prop->map = map;
	return map;
}

struct dma_iommu_mapping *tegra_smmu_of_get_mapping(struct device *dev,
						    u64 swgids,
						    struct list_head *asprops)
{
	struct smmu_map_prop *tmp;
	struct dma_iommu_mapping *map;

	list_for_each_entry(tmp, asprops, list) {
		if (!(swgids & tmp->swgid_mask))
			continue;

		if ((swgids & tmp->swgid_mask) != swgids)
			dev_info(dev, "mask=%llx doesn't include swgids=%llx\n",
				 tmp->swgid_mask, swgids);

		if (tmp->map) {
			kref_get(&tmp->map->kref);
			return tmp->map;
		}

		return tegra_smmu_of_populate_mapping(dev, tmp);
	}

	WARN(1, "Empty mapping for %s! Making an anonymous one.\n",
		dev_name(dev));

	tmp = devm_kzalloc(dev, sizeof(*tmp), GFP_KERNEL);
	if (!tmp)
		return NULL;

	tmp->swgid_mask = swgids;
	tmp->iova_start = ANON_IOVA_START;
	tmp->iova_size = ANON_IOVA_SIZE;
	tmp->alignment = ANON_IOVA_ALIGN;
	tmp->num_pf_page = ANON_IOVA_PF;
	tmp->gap_page = ANON_IOVA_GAP;

	map = tegra_smmu_of_populate_mapping(dev, tmp);
	if (map) {
		list_add_tail(&tmp->list, asprops);
		dev_info(dev, "populated map=%p for swgids=%llx\n",
			 map, swgids);
	}
	return map;
}

/*
 * Parse the passed address space prop (referenced by np) into prop.
 */
static struct smmu_map_prop *
__tegra_smmu_parse_as_prop(struct device *dev, struct device_node *np,
			   struct list_head *asprops)
{
	struct smmu_map_prop *prop;
	int err;

	prop = devm_kzalloc(dev, sizeof(*prop), GFP_KERNEL);
	if (!prop) {
		dev_err(dev, "no memory available\n");
		return ERR_PTR(-ENOMEM);
	}


	err  = of_property_read_u64(np, "iova-start",  &prop->iova_start);
	err |= of_property_read_u64(np, "iova-size",   &prop->iova_size);
	err |= of_property_read_u32(np, "num-pf-page", &prop->num_pf_page);
	err |= of_property_read_u32(np, "gap-page",    &prop->gap_page);
	if (err) {
		dev_err(dev, "invalid address-space-prop %s\n",	np->name);
		return ERR_PTR(-EINVAL);
	}

	err = of_property_read_u32(np, "alignment", &prop->alignment);
	if (err)
		prop->alignment = 0;

	list_add_tail(&prop->list, asprops);

	return prop;
}

/* FIXME: Add linear map logic as well */
int tegra_smmu_of_register_asprops(struct device *dev,
				   struct list_head *asprops)
{
	int count = 0, sum_hweight = 0;
	struct of_phandle_iter iter;
	u64 swgid_mask = 0;
	struct smmu_map_prop *prop, *temp;

	of_property_for_each_phandle_with_args(iter, dev->of_node,
					       "domains", NULL, 2) {
		struct of_phandle_args *ret = &iter.out_args;
		struct device_node *np = ret->np;

		if (ret->args_count < 2) {
			dev_err(dev,
				"domains expects 2 params but %d\n",
				ret->args_count);
			goto free_mem;
		}

		prop = __tegra_smmu_parse_as_prop(dev, np, asprops);
		if (IS_ERR_OR_NULL(prop))
			goto free_mem;

		memcpy(&prop->swgid_mask, ret->args, sizeof(u64));
		count += 1;

		/*
		 * The final entry in domains property is
		 * domains = <... &as_prop 0xFFFFFFFF 0xFFFFFFFF>;
		 * This entry is similar to SYSTEM_DEFAULT
		 * Skip the bit overlap check for this final entry
		 */
		if (prop->swgid_mask != ~0ULL) {
			swgid_mask |= prop->swgid_mask;
			sum_hweight += hweight64(prop->swgid_mask);
		}
	}

	if (sum_hweight == hweight64(swgid_mask))
		return count;

	/* check bit mask overlap in domains= property */
	dev_warn(dev, "overlapping bitmaps in domains!!!");
free_mem:
	list_for_each_entry_safe(prop, temp, asprops, list)
		devm_kfree(dev, prop);
	return 0;
}

u64 tegra_smmu_of_get_swgids(struct device *dev,
			       const struct of_device_id *matches,
			       struct iommu_linear_map **area)
{
	struct of_phandle_iter iter;
	u64 fixup, swgids = 0;

	if (dev_is_pci(dev))
		return SWGIDS_ERROR_CODE;

	of_property_for_each_phandle_with_args(iter, dev->of_node, "iommus",
					       "#iommu-cells", 0) {
		struct of_phandle_args *ret = &iter.out_args;

		if (!of_match_node(matches, ret->np))
			continue;

		if (ret->args_count != 1) {
			dev_err(dev, "iommus contains %d cells, expected 1\n",
				ret->args_count);
			break;
		}

		swgids |= (1ULL << ret->args[0]);
	}

	swgids = swgids ? swgids : SWGIDS_ERROR_CODE;

	fixup = tegra_smmu_fixup_swgids(dev, area);

	if (swgids_is_error(fixup))
		return swgids;

	if (swgids_is_error(swgids)) {
		dev_notice(dev,
			"No iommus property found in DT node, got swgids from fixup(%llx)\n",
			fixup);
		return fixup;
	}

	if (swgids != fixup) {
		dev_notice(dev,	"fixup(%llx) is different from DT(%llx)\n",
			   fixup, swgids);
		return fixup;
	}

	return swgids;
}

/*
 * T18x domains parsing. Instead of parsing a bitmap parse a list of SIDs. Also,
 * instead of using an external list to keep the address space props, use an
 * internal list. It makes no sense to keep the tegra specific address space
 * data structures in the SMMU device struct.
 */
int tegra_smmu_of_parse_sids(struct device *dev)
{
	struct device_node *domain_node, *child;
	struct smmu_map_prop *prop, *temp;
	u16 *sid_list;
	int err, i;

	sid_list = kzalloc(sizeof(u16) * SMMU_MAX_SIDS, GFP_KERNEL);
	if (!sid_list) {
		pr_warn("No mem for %s?\n", __func__);
		return -ENOMEM;
	}

	domain_node = of_get_child_by_name(dev->of_node, "domains");
	if (!domain_node) {
		err = -EINVAL;
		goto free_mem;
	}

	for_each_child_of_node(domain_node, child) {
		int ret;
		phandle as;
		u32 sid_list_len, sid;
		struct device_node *as_node;

		ret = of_property_read_u32(child, "address-space", &as);
		if (ret) {
			err = -EINVAL;
			of_node_put(child);
			goto free_mem;
		}

		as_node = of_find_node_by_phandle(as);
		if (!as_node) {
			err = -EINVAL;
			of_node_put(child);
			goto free_mem;
		}

		prop = __tegra_smmu_parse_as_prop(dev, as_node,
						  &smmu_addr_spaces);
		if (IS_ERR_OR_NULL(prop)) {
			err = PTR_ERR(prop);
			goto free_mem;
		}

		/* Read the SIDs. */
		ret = of_property_read_u32(child, "sid-list-len",
					   &sid_list_len);
		if (ret || sid_list_len > MAX_PHANDLE_ARGS) {
			err = -EINVAL;
			of_node_put(child);
			dev_err(dev, "Failed to parse address-space prop!\n");
			goto free_mem;
		}

		prop->nr_sids = sid_list_len;
		for (i = 0; i < sid_list_len; i++) {
			ret = of_property_read_u32_index(child, "sid-list",
							 i, &sid);
			if (ret) {
				err = -EINVAL;
				of_node_put(child);
				dev_err(dev, "Failed to read sid-list prop!\n");
				goto free_mem;
			}

			sid_list[sid]++;
			prop->sid_list[i] = sid;
		}
	}

	for (i = 0; i < SMMU_MAX_SIDS; i++) {
		if (sid_list[i] > 1) {
			pr_err("Duplicate SID in domains property (%d)!\n", i);
			err = -EINVAL;
			goto free_mem;
		}
	}

	kfree(sid_list);
	return 0;

free_mem:
	kfree(sid_list);
	list_for_each_entry_safe(prop, temp, &smmu_addr_spaces, list)
		devm_kfree(dev, prop);
	return err;
}

/*
 * A master could have multiple SIDs associated with it; however, on tegra we do
 * not do that. Instead we just treat the first SID as the SID which picks the
 * domain.
 */
struct dma_iommu_mapping *tegra_smmu_of_get_master_map(struct device *dev,
						       u16 *sids, int nr_sids)
{
	struct smmu_map_prop *prop;

	list_for_each_entry(prop, &smmu_addr_spaces, list) {
		int i;
		int found = 0;

		for (i = 0; i < prop->nr_sids; i++) {
			if (sids[0] == prop->sid_list[i]) {
				found = 1;
				break;
			}
		}

		if (!found)
			continue;

		if (prop->map)
			return prop->map;

		/* Oh well, no pre-existing map. Populate a new one. */
		return tegra_smmu_of_populate_mapping(dev, prop);
	}

	/*
	 * No mapping was found! Warn and return an error. This will prevent
	 * the passed device from using the SMMU unlike in previous tegra chips
	 * where an anonymous mapping was generated.
	 */
	WARN(1, "No SMMU mapping found for %s!\n", dev_name(dev));
	return NULL;
}
