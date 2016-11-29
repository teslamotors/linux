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

#ifndef INTEL_IPU4_RESOURCES_H
#define INTEL_IPU4_RESOURCES_H

/********** Generic resource handling **********/

/* Opaque structure. Do not access fields. */
struct intel_ipu4_resource {
	u32 id;
	int elements;		/* Number of elements available to allocation */
	unsigned long *bitmap;	/* Allocation bitmap, a bit for each element */
};

/* Allocation of resource(s) */
/* Opaque structure. Do not access fields. */
struct intel_ipu4_resource_alloc {
	struct intel_ipu4_resource *resource;
	int elements;
	int pos;
};

int intel_ipu4_resource_init(struct intel_ipu4_resource *res,
			     u32 id, int elements);
unsigned long intel_ipu4_resource_alloc(struct intel_ipu4_resource *res, int n,
				struct intel_ipu4_resource_alloc *alloc);
void intel_ipu4_resource_free(struct intel_ipu4_resource_alloc *alloc);
void intel_ipu4_resource_cleanup(struct intel_ipu4_resource *res);

/********** IPU4 PSYS-specific resource handling **********/

#define INTEL_IPU4_MAX_RESOURCES 32

struct ipu_fw_psys_program_group_manifest {
	u64 kernel_bitmap;
	u32 ID;
	u32 program_manifest_offset;
	u32 terminal_manifest_offset;
	u32 private_data_offset;
	u16 size;
	u8 alignment;
	u8 kernel_count;
	u8 program_count;
	u8 terminal_count;
	u8 subgraph_count;
	u8 reserved[1];
};

struct ipu_fw_psys_program_manifest {
	u64 kernel_bitmap;
	u32 ID;
	u32 program_type;
	s32 parent_offset;
	u32 program_dependency_offset;
	u32 terminal_dependency_offset;
	u16 size;
	u16 int_mem_size[IPU_FW_PSYS_N_MEM_TYPE_ID];
	u16 ext_mem_size[IPU_FW_PSYS_N_DATA_MEM_TYPE_ID];
	u16 dev_chn_size[IPU_FW_PSYS_N_DEV_CHN_ID];
	u8 cell_id;
	u8 cell_type_id;
	u8 program_dependency_count;
	u8 terminal_dependency_count;
	u8 reserved[4];
};

/*
 * This struct represents all of the currently allocated
 * resources from IPU model. It is used also for allocating
 * resources for the next set of PGs to be run on IPU
 * (ie. those PGs which are not yet being run and which don't
 * yet reserve real IPU4 resources).
 */
struct intel_ipu4_psys_resource_pool {
	u32 cells;	/* Bitmask of cells allocated */
	struct intel_ipu4_resource dev_channels[IPU_FW_PSYS_N_DEV_CHN_ID];
};

/*
 * This struct keeps book of the resources allocated for a specific PG.
 * It is used for freeing up resources from struct intel_ipu4_psys_resources
 * when the PG is released from IPU4 (or model of IPU4).
 */
struct intel_ipu4_psys_resource_alloc {
	u32 cells;	/* Bitmask of cells needed */
	struct intel_ipu4_resource_alloc
		resource_alloc[INTEL_IPU4_MAX_RESOURCES];
	int resources;
};

struct ipu_fw_psys_process_group;

int intel_ipu4_psys_resource_pool_init(
				struct intel_ipu4_psys_resource_pool *pool);

void intel_ipu4_psys_resource_pool_cleanup(
				struct intel_ipu4_psys_resource_pool *pool);

void intel_ipu4_psys_resource_alloc_init(
				struct intel_ipu4_psys_resource_alloc *alloc);

int intel_ipu4_psys_allocate_resources(const struct device *dev,
			       struct ipu_fw_psys_process_group *pg,
			       void *pg_manifest,
			       struct intel_ipu4_psys_resource_alloc *alloc,
			       struct intel_ipu4_psys_resource_pool *pool);
int intel_ipu4_psys_move_resources(const struct device *dev,
			   struct intel_ipu4_psys_resource_alloc *alloc,
			   struct intel_ipu4_psys_resource_pool *source_pool,
			   struct intel_ipu4_psys_resource_pool *target_pool);

void intel_ipu4_psys_free_resources(
			struct intel_ipu4_psys_resource_alloc *alloc,
			struct intel_ipu4_psys_resource_pool *pool);

#endif
