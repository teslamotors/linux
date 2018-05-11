// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2015 - 2018 Intel Corporation

#include <linux/bitmap.h>
#include <linux/errno.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/device.h>

#include <uapi/linux/ipu-psys.h>

#include "ipu-fw-psys.h"
#include "ipu-psys.h"

static int ipu_resource_init(struct ipu_resource *res, u32 id, int elements)
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

static unsigned long
ipu_resource_alloc(struct ipu_resource *res, int n,
		   struct ipu_resource_alloc *alloc,
		   enum ipu_resource_type type)
{
	unsigned long p;

	if (n <= 0) {
		alloc->elements = 0;
		return 0;
	}

	if (!res->bitmap)
		return (unsigned long)(-ENOSPC);

	p = bitmap_find_next_zero_area(res->bitmap, res->elements, 0, n, 0);
	alloc->resource = NULL;

	if (p >= res->elements)
		return (unsigned long)(-ENOSPC);
	bitmap_set(res->bitmap, p, n);
	alloc->resource = res;
	alloc->elements = n;
	alloc->pos = p;
	alloc->type = type;

	return p;
}

static void ipu_resource_free(struct ipu_resource_alloc *alloc)
{
	if (alloc->elements <= 0)
		return;

	if (alloc->type == IPU_RESOURCE_DFM)
		*alloc->resource->bitmap &= ~(unsigned long)(alloc->elements);
	else
		bitmap_clear(alloc->resource->bitmap, alloc->pos,
			     alloc->elements);
	alloc->resource = NULL;
}

static void ipu_resource_cleanup(struct ipu_resource *res)
{
	kfree(res->bitmap);
	res->bitmap = NULL;
}

/********** IPU PSYS-specific resource handling **********/

int ipu_psys_resource_pool_init(struct ipu_psys_resource_pool
				*pool)
{
	int i, j, k, ret;

	pool->cells = 0;

	for (i = 0; i < res_defs->num_dev_channels; i++) {
		ret = ipu_resource_init(&pool->dev_channels[i], i,
					res_defs->dev_channels[i]);
		if (ret)
			goto error;
	}

	for (j = 0; j < res_defs->num_ext_mem_ids; j++) {
		ret = ipu_resource_init(&pool->ext_memory[j], j,
					res_defs->ext_mem_ids[j]);
		if (ret)
			goto memory_error;
	}

	for (k = 0; k < res_defs->num_dfm_ids; k++) {
		ret = ipu_resource_init(&pool->dfms[k], k, res_defs->dfms[k]);
		if (ret)
			goto dfm_error;
	}

	return 0;

dfm_error:
	for (k--; k >= 0; k--)
		ipu_resource_cleanup(&pool->dfms[k]);

memory_error:
	for (j--; j >= 0; j--)
		ipu_resource_cleanup(&pool->ext_memory[j]);

error:
	for (i--; i >= 0; i--)
		ipu_resource_cleanup(&pool->dev_channels[i]);
	return ret;
}


void ipu_psys_resource_pool_cleanup(struct ipu_psys_resource_pool
				    *pool)
{
	u32 i;

	for (i = 0; i < res_defs->num_dev_channels; i++)
		ipu_resource_cleanup(&pool->dev_channels[i]);

	for (i = 0; i < res_defs->num_ext_mem_ids; i++)
		ipu_resource_cleanup(&pool->ext_memory[i]);

	for (i = 0; i < res_defs->num_dfm_ids; i++)
		ipu_resource_cleanup(&pool->dfms[i]);
}

static int ipu_psys_allocate_one_resource(const struct device *dev,
				struct ipu_fw_psys_process *process,
				struct ipu_resource *resource,
				struct ipu_fw_generic_program_manifest *pm,
				u32 resource_id,
				struct ipu_psys_resource_alloc *alloc)
{
	const u16 resource_req = pm->dev_chn_size[resource_id];
	unsigned long retl;

	if (resource_req <= 0)
		return 0;

	if (alloc->resources >= IPU_MAX_RESOURCES) {
		dev_err(dev, "out of resource handles\n");
		return -ENOSPC;
	}
		retl = ipu_resource_alloc
		    (resource, resource_req,
		     &alloc->resource_alloc[alloc->resources],
		     IPU_RESOURCE_DEV_CHN);
	if (IS_ERR_VALUE(retl)) {
		dev_dbg(dev, "out of device channel resources\n");
		return (int)retl;
	}
	alloc->resources++;

	return 0;
}

/*
 * ext_mem_type_id is a generic type id for memory (like DMEM, VMEM)
 * ext_mem_bank_id is detailed type id for  memory (like DMEM0, DMEM1 etc.)
 */
static int ipu_psys_allocate_memory_resource(
				const struct device *dev,
				struct ipu_fw_psys_process *process,
				struct ipu_resource *resource,
				struct ipu_fw_generic_program_manifest *pm,
				u32 ext_mem_type_id, u32 ext_mem_bank_id,
				struct ipu_psys_resource_alloc *alloc)
{
	const u16 memory_resource_req = pm->ext_mem_size[ext_mem_type_id];

	unsigned long retl;

	if (memory_resource_req <= 0)
		return 0;

	if (alloc->resources >= IPU_MAX_RESOURCES) {
		dev_err(dev, "out of resource handles\n");
		return -ENOSPC;
	}
		retl = ipu_resource_alloc
		    (resource, memory_resource_req,
		     &alloc->resource_alloc[alloc->resources],
		     IPU_RESOURCE_EXT_MEM);
	if (IS_ERR_VALUE(retl)) {
		dev_dbg(dev, "out of memory resources\n");
		return (int)retl;
	}

	alloc->resources++;

	return 0;
}

/*
 * Allocate resources for pg from `pool'. Mark the allocated
 * resources into `alloc'. Returns 0 on success, -ENOSPC
 * if there are no enough resources, in which cases resources
 * are not allocated at all, or some other error on other conditions.
 */
int ipu_psys_allocate_resources(const struct device *dev,
				struct ipu_fw_psys_process_group *pg,
				void *pg_manifest,
				struct ipu_psys_resource_alloc
				*alloc, struct ipu_psys_resource_pool
				*pool)
{
	u32 resid;
	u32 mem_type_id;
	int ret, i;
	u16 *process_offset_table;
	u8 processes;
	u32 cells = 0;

	if (!pg)
		return -EINVAL;
	process_offset_table = (u16 *)((u8 *) pg + pg->processes_offset);
	processes = pg->process_count;

	for (i = 0; i < processes; i++) {
		u32 cell;
		struct ipu_fw_psys_process *process =
		    (struct ipu_fw_psys_process *)
		    ((char *)pg + process_offset_table[i]);
		struct ipu_fw_generic_program_manifest pm;

		memset(&pm, 0, sizeof(pm));
		if (!process) {
			dev_err(dev, "can not get process\n");
			ret = -ENOENT;
			goto free_out;
		}

		ret = ipu_fw_psys_get_program_manifest_by_process(&pm,
								  pg_manifest,
								  process);
		if (ret < 0) {
			dev_err(dev, "can not get manifest\n");
			goto free_out;
		}

		if (pm.cell_id == res_defs->num_cells &&
		    pm.cell_type_id == res_defs->num_cells_type) {
			dev_dbg(dev, "ignore the cell requirement\n");
			cell = res_defs->num_cells;
		} else if ((pm.cell_id != res_defs->num_cells &&
			    pm.cell_type_id == res_defs->num_cells_type)) {
			cell = ipu_fw_psys_get_process_cell_id(process, 0);
		} else {
			/* Find a free cell of desired type */
			u32 type = pm.cell_type_id;

			for (cell = 0; cell < res_defs->num_cells; cell++)
				if (res_defs->cells[cell] == type &&
				    ((pool->cells | cells) & (1 << cell)) == 0)
					break;
			if (cell >= res_defs->num_cells) {
				dev_dbg(dev, "no free cells of right type\n");
				ret = -ENOSPC;
				goto free_out;
			}
			ret = ipu_fw_psys_set_process_cell_id(process, 0, cell);
			if (ret)
				goto free_out;
		}
		if (cell < res_defs->num_cells)
			cells |= 1 << cell;
		if (pool->cells & cells) {
			dev_dbg(dev, "out of cell resources\n");
			ret = -ENOSPC;
			goto free_out;
		}
		if (pm.dev_chn_size) {
			for (resid = 0; resid < res_defs->num_dev_channels; resid++) {
				ret = ipu_psys_allocate_one_resource
				    (dev, process,
				     &pool->dev_channels[resid], &pm, resid, alloc);
				if (ret)
					goto free_out;
				ret = ipu_fw_psys_set_process_dev_chn_offset(process, resid,
					alloc->resource_alloc[alloc->resources - 1].pos);
				if (ret)
					goto free_out;
			}
		}

		if (pm.ext_mem_size) {
			for (mem_type_id = 0;
			     mem_type_id < res_defs->num_ext_mem_types; mem_type_id++) {
				u32 mem_bank_id = res_defs->num_ext_mem_ids;

				if (cell != res_defs->num_cells)
					mem_bank_id =
					    res_defs->cell_mem[res_defs->cell_mem_row *
							       cell + mem_type_id];
				if (mem_bank_id == res_defs->num_ext_mem_ids)
					continue;

				ret = ipu_psys_allocate_memory_resource
				    (dev, process,
				     &pool->ext_memory[mem_bank_id],
				     &pm, mem_type_id, mem_bank_id, alloc);
				if (ret)
					goto free_out;
				/* no return value check here because fw api will
				 * do some checks, and would return non-zero
				 * except mem_type_id == 0. This may be caused by that
				 * above flow if allocating mem_bank_id is improper
				 */
				ipu_fw_psys_set_process_ext_mem
					(process, mem_type_id, mem_bank_id,
					alloc->resource_alloc[alloc->resources - 1].pos);
			}
		}
	}
	alloc->cells |= cells;
	pool->cells |= cells;
	return 0;

free_out:
	for (; i >= 0; i--) {
		struct ipu_fw_psys_process *process =
		    (struct ipu_fw_psys_process *)
		    ((char *)pg + process_offset_table[i]);
		struct ipu_fw_generic_program_manifest pm;
		int retval;

		if (!process)
			break;

		retval = ipu_fw_psys_get_program_manifest_by_process
		    (&pm, pg_manifest, process);
		if (retval < 0)
			break;
		if ((pm.cell_id != res_defs->num_cells &&
		     pm.cell_type_id == res_defs->num_cells_type))
			continue;
		/* no return value check here because if finding free cell
		 * failed, process cell would not set then calling clear_cell
		 * will return non-zero.
		 */
		ipu_fw_psys_clear_process_cell(process);
	}
	dev_dbg(dev, "failed to allocate resources, ret %d\n", ret);
	ipu_psys_free_resources(alloc, pool);
	return ret;
}

int ipu_psys_move_resources(const struct device *dev,
			    struct ipu_psys_resource_alloc *alloc,
			    struct ipu_psys_resource_pool
			    *source_pool, struct ipu_psys_resource_pool
			    *target_pool)
{
	int i;

	if (target_pool->cells & alloc->cells) {
		dev_dbg(dev, "out of cell resources\n");
		return -ENOSPC;
	}

	for (i = 0; i < alloc->resources; i++) {
		unsigned long bitmap = 0;
		unsigned int id = alloc->resource_alloc[i].resource->id;
		unsigned long fbit, end;

		switch (alloc->resource_alloc[i].type) {
		case IPU_RESOURCE_DEV_CHN:
			bitmap_set(&bitmap, alloc->resource_alloc[i].pos,
				   alloc->resource_alloc[i].elements);
			if (*target_pool->dev_channels[id].bitmap & bitmap)
				return -ENOSPC;
			break;
		case IPU_RESOURCE_EXT_MEM:
			end = alloc->resource_alloc[i].elements +
			    alloc->resource_alloc[i].pos;

			fbit = find_next_bit(target_pool->ext_memory[id].bitmap,
					     end, alloc->resource_alloc[i].pos);
			/* if find_next_bit returns "end" it didn't find 1bit */
			if (end != fbit)
				return -ENOSPC;
			break;
		case IPU_RESOURCE_DFM:
			bitmap = alloc->resource_alloc[i].elements;
			if (*target_pool->dfms[id].bitmap & bitmap)
				return -ENOSPC;
			break;
		default:
			dev_err(dev, "Illegal resource type\n");
			return -EINVAL;
		}
	}

	for (i = 0; i < alloc->resources; i++) {
		u32 id = alloc->resource_alloc[i].resource->id;

		switch (alloc->resource_alloc[i].type) {
		case IPU_RESOURCE_DEV_CHN:
			bitmap_set(target_pool->dev_channels[id].bitmap,
				   alloc->resource_alloc[i].pos,
				   alloc->resource_alloc[i].elements);
			ipu_resource_free(&alloc->resource_alloc[i]);
			alloc->resource_alloc[i].resource =
			    &target_pool->dev_channels[id];
			break;
		case IPU_RESOURCE_EXT_MEM:
			bitmap_set(target_pool->ext_memory[id].bitmap,
				   alloc->resource_alloc[i].pos,
				   alloc->resource_alloc[i].elements);
			ipu_resource_free(&alloc->resource_alloc[i]);
			alloc->resource_alloc[i].resource =
			    &target_pool->ext_memory[id];
			break;
		case IPU_RESOURCE_DFM:
			*target_pool->dfms[id].bitmap |=
			    alloc->resource_alloc[i].elements;
			*alloc->resource_alloc[i].resource->bitmap &=
			    ~(alloc->resource_alloc[i].elements);
			alloc->resource_alloc[i].resource =
			    &target_pool->dfms[id];
			break;
		default:
			/*
			 * Just keep compiler happy. This case failed already
			 * in above loop.
			 */
			break;
		}
	}

	target_pool->cells |= alloc->cells;
	source_pool->cells &= ~alloc->cells;

	return 0;
}

/* Free resources marked in `alloc' from `resources' */
void ipu_psys_free_resources(struct ipu_psys_resource_alloc
			     *alloc, struct ipu_psys_resource_pool *pool)
{
	unsigned int i;

	pool->cells &= ~alloc->cells;
	alloc->cells = 0;
	for (i = 0; i < alloc->resources; i++)
		ipu_resource_free(&alloc->resource_alloc[i]);
	alloc->resources = 0;
}
