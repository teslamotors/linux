/*
 * Copyright (c) 2015--2016 Intel Corporation.
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

#include <uapi/linux/intel-ipu4-psys.h>

#include "intel-ipu4-psys-abi-defs.h"
#include "intel-ipu4-psys-abi.h"
#include "intel-ipu4-psys.h"
#include "intel-ipu4-resources.h"

#define SANITY_CHECK 1

/*
 * Cell types by cell IDs
 */
#if IS_ENABLED(CONFIG_VIDEO_INTEL_IPU4)

static const u32 vied_nci_cell_types[VIED_NCI_N_CELL_ID] = {
	VIED_NCI_SP_CTRL_TYPE_ID,
	VIED_NCI_SP_SERVER_TYPE_ID,
	VIED_NCI_SP_SERVER_TYPE_ID,
	VIED_NCI_VP_TYPE_ID,
	VIED_NCI_VP_TYPE_ID,
	VIED_NCI_VP_TYPE_ID,
	VIED_NCI_VP_TYPE_ID,
	VIED_NCI_ACC_ISA_TYPE_ID,
	VIED_NCI_ACC_PSA_TYPE_ID,
	VIED_NCI_ACC_PSA_TYPE_ID,
	VIED_NCI_ACC_PSA_TYPE_ID,
	VIED_NCI_ACC_PSA_TYPE_ID,
	VIED_NCI_ACC_PSA_TYPE_ID,
	VIED_NCI_ACC_PSA_TYPE_ID,
	VIED_NCI_ACC_OSA_TYPE_ID,
	VIED_NCI_GDC_TYPE_ID,
	VIED_NCI_GDC_TYPE_ID
};

#elif IS_ENABLED(CONFIG_VIDEO_INTEL_IPU5)

static const u32 vied_nci_cell_types[VIED_NCI_N_CELL_ID] = {
	VIED_NCI_SP_CTRL_TYPE_ID,
	VIED_NCI_SP_SERVER_TYPE_ID,
	VIED_NCI_SP_SERVER_TYPE_ID,
	VIED_NCI_VP_TYPE_ID,
	VIED_NCI_ACC_ISA_TYPE_ID,
	VIED_NCI_ACC_PSA_TYPE_ID,
	VIED_NCI_ACC_PSA_TYPE_ID,
	VIED_NCI_ACC_PSA_TYPE_ID,
	VIED_NCI_ACC_PSA_TYPE_ID,
	VIED_NCI_ACC_PSA_TYPE_ID,
	VIED_NCI_ACC_PSA_TYPE_ID,
	VIED_NCI_ACC_OSA_TYPE_ID,
	VIED_NCI_GDC_TYPE_ID
};

#endif

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
static const u16
		intel_ipu4_num_dev_channels[VIED_NCI_N_DEV_CHN_ID] = {
	VIED_NCI_DEV_CHN_DMA_EXT0_MAX_SIZE,
	VIED_NCI_DEV_CHN_GDC_MAX_SIZE,
	VIED_NCI_DEV_CHN_DMA_EXT1_READ_MAX_SIZE,
	VIED_NCI_DEV_CHN_DMA_EXT1_WRITE_MAX_SIZE,
	VIED_NCI_DEV_CHN_DMA_INTERNAL_MAX_SIZE,
	VIED_NCI_DEV_CHN_DMA_IPFD_MAX_SIZE,
	VIED_NCI_DEV_CHN_DMA_ISA_MAX_SIZE,
	VIED_NCI_DEV_CHN_DMA_FW_MAX_SIZE
};

static struct ia_css_program_manifest *intel_ipu4_resource_get_program_manifest(
	const struct ia_css_program_group_manifest *manifest,
	const unsigned int program_index)
{
	struct ia_css_program_manifest *prg_manifest_base;
	u8 *program_manifest = NULL;
	u8 program_count;
	unsigned int i;

	program_count = manifest->program_count;

	prg_manifest_base = (struct ia_css_program_manifest *)
		((char *)manifest +
		manifest->program_manifest_offset);
	if (program_index < program_count) {
		program_manifest = (u8 *)prg_manifest_base;
		for (i = 0; i < program_index; i++)
			program_manifest +=
				((struct ia_css_program_manifest *)
				 program_manifest)->size;
	}

	return (struct ia_css_program_manifest *)program_manifest;
}

static struct ia_css_program_manifest *
intel_ipu4_psys_get_program_manifest_by_process(
	const struct ia_css_program_group_manifest *pg_manifest,
	struct ia_css_process *process)
{
	u32 process_id = process->ID;
	int programs = pg_manifest->program_count;
	int i;

	for (i = 0; i < programs; i++) {
		u32 program_id;
		struct ia_css_program_manifest *pm =
			intel_ipu4_resource_get_program_manifest(
				pg_manifest, i);
		if (!pm)
			continue;
		program_id = pm->ID;
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
	u32 i;

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
			struct ia_css_process *process,
			struct intel_ipu4_resource *resource,
			struct ia_css_program_manifest *pm,
			u32 resource_id,
			struct intel_ipu4_psys_resource_alloc *alloc)
{
	const u16 resource_req = pm->dev_chn_size[resource_id];
	unsigned long retl;

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
	process->dev_chn_offset[resource_id] = retl;

	return 0;
}

/*
 * Allocate resources for pg from `pool'. Mark the allocated
 * resources into `alloc'. Returns 0 on success, -ENOSPC
 * if there are no enough resources, in which cases resources
 * are not allocated at all, or some other error on other conditions.
 */
int intel_ipu4_psys_allocate_resources(const struct device *dev,
			struct ia_css_process_group *pg,
			void *pg_manifest,
			struct intel_ipu4_psys_resource_alloc *alloc,
			struct intel_ipu4_psys_resource_pool *pool)
{
	u32 res_id;
	int ret, i;
	u16 *process_offset_table = (u16 *)((u8 *)pg + pg->processes_offset);
	uint8_t processes = pg->process_count;
	u32 cells = 0;

	for (i = 0; i < processes; i++) {
		u32 cell;
		struct ia_css_process *process = (struct ia_css_process *)
			((char *)pg + process_offset_table[i]);
		struct ia_css_program_manifest *pm;

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

		if ((pm->cell_id != VIED_NCI_N_CELL_ID &&
		     pm->cell_type_id != VIED_NCI_N_CELL_ID)) {
			cell = process->cell_id;
		} else {
			/* Find a free cell of desired type */
			u32 type = pm->cell_type_id;
			for (cell = 0; cell < VIED_NCI_N_CELL_ID; cell++)
				if (vied_nci_cell_types[cell] == type &&
				    ((pool->cells | cells) & (1 << cell)) == 0)
					break;
			if (cell >= VIED_NCI_N_CELL_ID) {
				dev_dbg(dev, "no free cells of right type\n");
				ret = -ENOSPC;
				goto free_out;
			}
			pg->resource_bitmap |= 1 << cell;
			process->cell_id = cell;
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
		struct ia_css_process *process = (struct ia_css_process *)
			((char *)pg + process_offset_table[i]);
		struct ia_css_program_manifest *pm;

		if (process == NULL)
			break;
		pm = intel_ipu4_psys_get_program_manifest_by_process(
						pg_manifest, process);
		if (pm == NULL)
			break;
		if ((pm->cell_id != VIED_NCI_N_CELL_ID &&
		     pm->cell_type_id != VIED_NCI_N_CELL_ID))
			continue;
		pg->resource_bitmap &= ~(1 << process->cell_id);
		process->cell_id = 0;
	}

	intel_ipu4_psys_free_resources(alloc, pool);
	return ret;
}

int intel_ipu4_psys_move_resources(const struct device *dev,
			   struct intel_ipu4_psys_resource_alloc *alloc,
			   struct intel_ipu4_psys_resource_pool *source_pool,
			   struct intel_ipu4_psys_resource_pool *target_pool)
{
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
