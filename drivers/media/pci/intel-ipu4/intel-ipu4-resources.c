/*
 * Copyright (c) 2015 Intel Corporation.
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

#include <linux/bitmap.h>
#include <linux/errno.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/device.h>
#include "intel-ipu4-resources.h"
#include "ia_css_psys_process_group.h"
#include "ia_css_psys_process.h"
#include "ia_css_psys_program_manifest.h"
#include "ia_css_psys_program_group_manifest.h"
#include "ia_css_psys_process.hsys.kernel.h"
#include "ia_css_psys_program_manifest.hsys.kernel.h"
#include "vied_nci_psys_system_global.h"

#define SANITY_CHECK 1

/********** Generic resource handling **********/

int intel_ipu4_resource_init(struct intel_ipu4_resource *res,
			     u32 id, int elements)
{
	if (elements <= 0) {
		res->bitmap = NULL;
		return 0;
	}

	res->bitmap = kcalloc(BITS_TO_LONGS(elements), sizeof(long),
			      GFP_KERNEL);
	if (!res->bitmap)
		return -ENOMEM;
	res->elements = elements;
	res->id = id;
	return 0;
}

unsigned long intel_ipu4_resource_alloc(struct intel_ipu4_resource *res, int n,
					struct intel_ipu4_resource_alloc *alloc)
{
	unsigned long p;

	if (n <= 0) {
		alloc->elements = 0;
		return 0;
	}

	if (!res->bitmap)
		return (unsigned long)(-ENOSPC);

	p = bitmap_find_next_zero_area(res->bitmap, res->elements, 0, n, 0);
	if (SANITY_CHECK)
		alloc->resource = NULL;
	if (p >= res->elements)
		return (unsigned long)(-ENOSPC);
	bitmap_set(res->bitmap, p, n);
	alloc->resource = res;
	alloc->elements = n;
	alloc->pos = p;
	return p;
}

void intel_ipu4_resource_free(struct intel_ipu4_resource_alloc *alloc)
{
	if (alloc->elements <= 0)
		return;

	bitmap_clear(alloc->resource->bitmap, alloc->pos, alloc->elements);
	if (SANITY_CHECK)
		alloc->resource = NULL;
}

void intel_ipu4_resource_cleanup(struct intel_ipu4_resource *res)
{
	kfree(res->bitmap);
	if (SANITY_CHECK)
		res->bitmap = NULL;
}

/********** IPU4 PSYS-specific resource handling **********/

/* Process group resource sizes */
static const vied_nci_resource_size_t
		intel_ipu4_num_dev_channels[VIED_NCI_N_DEV_CHN_ID] = {
	30,		/* VIED_NCI_DEV_CHN_DMA_EXT0_ID */
	0,		/* VIED_NCI_DEV_CHN_GDC_ID */
	30,		/* VIED_NCI_DEV_CHN_DMA_EXT1_READ_ID */
	20,		/* VIED_NCI_DEV_CHN_DMA_EXT1_WRITE_ID */
	2,		/* VIED_NCI_DEV_CHN_DMA_INTERNAL_ID */
	5,		/* VIED_NCI_DEV_CHN_DMA_IPFD_ID */
	2,		/* VIED_NCI_DEV_CHN_DMA_ISA_ID */
	0,		/* VIED_NCI_DEV_CHN_DMA_FW_ID */
};

static ia_css_program_manifest_t *
intel_ipu4_psys_get_program_manifest_by_process(
			const ia_css_program_group_manifest_t *pg_manifest,
			ia_css_process_t *process)
{
	ia_css_program_ID_t process_id =
		ia_css_process_get_program_ID(process);
	int programs =
		ia_css_program_group_manifest_get_program_count(pg_manifest);
	int i;

	for (i = 0; i < programs; i++) {
		ia_css_program_ID_t program_id;
		ia_css_program_manifest_t *pm =
			ia_css_program_group_manifest_get_program_manifest(
								pg_manifest, i);
		if (!pm)
			continue;
		program_id = ia_css_program_manifest_get_program_ID(pm);
		if (program_id == process_id)
			return pm;
	}
	return NULL;
}

int intel_ipu4_psys_resource_pool_init(
				struct intel_ipu4_psys_resource_pool *pool)
{
	int i, ret;

	pool->cells = 0;

	for (i = 0; i < VIED_NCI_N_DEV_CHN_ID; i++) {
		ret = intel_ipu4_resource_init(&pool->dev_channels[i], i,
					       intel_ipu4_num_dev_channels[i]);
		if (ret)
			goto error;
	}
	return 0;

error:
	for (i--; i >= 0; i--)
		intel_ipu4_resource_cleanup(&pool->dev_channels[i]);
	return ret;
}

void intel_ipu4_psys_resource_pool_cleanup(
				struct intel_ipu4_psys_resource_pool *pool)
{
	vied_nci_dev_chn_ID_t i;

	for (i = 0; i < VIED_NCI_N_DEV_CHN_ID; i++)
		intel_ipu4_resource_cleanup(&pool->dev_channels[i]);
}

void intel_ipu4_psys_resource_alloc_init(
				struct intel_ipu4_psys_resource_alloc *alloc)
{
	alloc->cells = 0;
	alloc->resources = 0;
}

static int intel_ipu4_psys_allocate_one_resource(const struct device *dev,
			ia_css_process_t *process,
			struct intel_ipu4_resource *resource,
			ia_css_program_manifest_t *pm,
			vied_nci_dev_chn_ID_t resource_id,
			struct intel_ipu4_psys_resource_alloc *alloc)
{
	const vied_nci_resource_size_t resource_req =
		ia_css_program_manifest_get_dev_chn_size(pm, resource_id);
	unsigned long retl;
	int ret;

	if (resource_req <= 0)
		return 0;

	if (alloc->resources >= INTEL_IPU4_MAX_RESOURCES) {
		dev_err(dev, "out of resource handles\n");
		return -ENOSPC;
	}

	retl = intel_ipu4_resource_alloc(resource, resource_req,
				&alloc->resource_alloc[alloc->resources]);
	if (IS_ERR_VALUE(retl)) {
		dev_dbg(dev, "out of resources\n");
		return (int)retl;
	}
	alloc->resources++;
	ret = ia_css_process_set_dev_chn(process, resource_id, retl);
	if (ret < 0) {
		dev_err(dev, "can not set resources\n");
		return -EIO;
	}

	return 0;
}

/*
 * Allocate resources for pg from `pool'. Mark the allocated
 * resources into `alloc'. Returns 0 on success, -ENOSPC
 * if there are no enough resources, in which cases resources
 * are not allocated at all, or some other error on other conditions.
 */
int intel_ipu4_psys_allocate_resources(const struct device *dev,
			ia_css_process_group_t *pg,
			void *pg_manifest,
			struct intel_ipu4_psys_resource_alloc *alloc,
			struct intel_ipu4_psys_resource_pool *pool)
{
	vied_nci_dev_chn_ID_t res_id;
	unsigned int i;
	int ret;
	uint8_t processes = ia_css_process_group_get_process_count(pg);
	vied_nci_resource_bitmap_t cells = 0;

	for (i = 0; i < processes; i++) {
		vied_nci_cell_ID_t cell;
		ia_css_process_t *process =
			ia_css_process_group_get_process(pg, i);
		ia_css_program_manifest_t *pm;

		if (process == NULL) {
			dev_err(dev, "can not get process\n");
			ret = -ENOENT;
			goto free_out;
		}

		pm = intel_ipu4_psys_get_program_manifest_by_process(
						pg_manifest, process);
		if (pm == NULL) {
			dev_err(dev, "can not get manifest\n");
			ret = -ENOENT;
			goto free_out;
		}

		if (ia_css_has_program_manifest_fixed_cell(pm)) {
			cell = ia_css_process_get_cell(process);
		} else {
			/* Find a free cell of desired type */
			vied_nci_cell_type_ID_t type =
				ia_css_program_manifest_get_cell_type_ID(pm);
			for (cell = 0; cell < VIED_NCI_N_CELL_ID; cell++)
				if (vied_nci_cell_get_type(cell) == type &&
				    ((pool->cells | cells) & (1 << cell)) == 0)
					break;
			if (cell >= VIED_NCI_N_CELL_ID) {
				dev_dbg(dev, "no free cells of right type\n");
				ret = -ENOSPC;
				goto free_out;
			}
			if (ia_css_process_set_cell(process, cell)) {
				dev_err(dev, "could not assign cell\n");
				ret = -EIO;
				goto free_out;
			}
		}
		cells |= 1 << cell;
		if (pool->cells & cells) {
			dev_dbg(dev, "out of cell resources\n");
			ret = -ENOSPC;
			goto free_out;
		}

		for (res_id = 0; res_id < VIED_NCI_N_DEV_CHN_ID; res_id++) {
			ret = intel_ipu4_psys_allocate_one_resource(dev,
				process, &pool->dev_channels[res_id],
				pm, res_id, alloc);
			if (ret)
				goto free_out;
		}
	}
	alloc->cells |= cells;
	pool->cells |= cells;
	return 0;

free_out:
	for (; i >= 0; i--) {
		ia_css_process_t *process =
			ia_css_process_group_get_process(pg, i);
		ia_css_program_manifest_t *pm;

		if (process == NULL)
			break;
		pm = intel_ipu4_psys_get_program_manifest_by_process(
						pg_manifest, process);
		if (pm == NULL)
			break;
		if (ia_css_has_program_manifest_fixed_cell(pm))
			continue;
		ia_css_process_clear_cell(process);
	}

	intel_ipu4_psys_free_resources(alloc, pool);
	return ret;
}

int intel_ipu4_psys_move_resources(const struct device *dev,
			   struct intel_ipu4_psys_resource_alloc *alloc,
			   struct intel_ipu4_psys_resource_pool *source_pool,
			   struct intel_ipu4_psys_resource_pool *target_pool)
{
	vied_nci_dev_chn_ID_t res_id;
	int i;

	if (target_pool->cells & alloc->cells) {
		dev_dbg(dev, "out of cell resources\n");
		return -ENOSPC;
	}

	for (i = 0; i < alloc->resources; i++) {
		unsigned long bitmap = 0;

		bitmap_set(&bitmap, alloc->resource_alloc[i].pos,
			   alloc->resource_alloc[i].elements);
		if (*target_pool->dev_channels[alloc->resource_alloc[i].resource->id].bitmap &
		    bitmap)
			return -ENOSPC;
	}

	for (i = 0; i < alloc->resources; i++) {
		unsigned long bitmap = 0;
		u32 id = alloc->resource_alloc[i].resource->id;

		bitmap_set(&bitmap, alloc->resource_alloc[i].pos,
			   alloc->resource_alloc[i].elements);
		*target_pool->dev_channels[id].bitmap |= bitmap;
		intel_ipu4_resource_free(&alloc->resource_alloc[i]);
		alloc->resource_alloc[i].resource =
			&target_pool->dev_channels[id];
	}

	target_pool->cells |= alloc->cells;
	source_pool->cells &= ~alloc->cells;

	return 0;
}

/* Free resources marked in `alloc' from `resources' */
void intel_ipu4_psys_free_resources(
			struct intel_ipu4_psys_resource_alloc *alloc,
			struct intel_ipu4_psys_resource_pool *pool)
{
	unsigned int i;

	pool->cells &= ~alloc->cells;
	alloc->cells = 0;
	for (i = 0; i < alloc->resources; i++)
		intel_ipu4_resource_free(&alloc->resource_alloc[i]);
	alloc->resources = 0;
}
