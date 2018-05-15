/* SPDX-License_Identifier: GPL-2.0 */
/* Copyright (C) 2015 - 2018 Intel Corporation */

#include "ipu-fw-psys.h"
#include "ipu-platform-resources.h"

#ifndef IPU_RESOURCES_H
#define IPU_RESOURCES_H

/********** Generic resource handling **********/

extern const u32 ipu_fw_psys_cell_types[];
extern const u16 ipu_num_dev_channels[];
extern const u16 ipu_fw_psys_mem_size[];

extern const enum ipu_mem_id ipu_fw_psys_cell_mem
	[IPU_FW_PSYS_N_CELL_ID][IPU_FW_PSYS_N_DATA_MEM_TYPE_ID];
extern const struct ipu_resource_definitions *res_defs;

/* Opaque structure. Do not access fields. */
struct ipu_resource {
	u32 id;
	int elements;	/* Number of elements available to allocation */
	unsigned long *bitmap;	/* Allocation bitmap, a bit for each element */
};

enum ipu_resource_type {
	IPU_RESOURCE_DEV_CHN = 0,
	IPU_RESOURCE_EXT_MEM,
	IPU_RESOURCE_DFM
};

/* Allocation of resource(s) */
/* Opaque structure. Do not access fields. */
struct ipu_resource_alloc {
	enum ipu_resource_type type;
	struct ipu_resource *resource;
	int elements;
	int pos;
};

/********** IPU PSYS-specific resource handling **********/

#define IPU_MAX_RESOURCES 32

struct ipu_fw_psys_program_group_manifest {
	u32 kernel_bitmap[IPU_FW_PSYS_KERNEL_BITMAP_NOF_ELEMS];
	u32 ID;
	u16 program_manifest_offset;
	u16 terminal_manifest_offset;
	u16 private_data_offset;
	u16 rbm_manifest_offset;
	u16 size;
	u8 alignment;
	u8 kernel_count;
	u8 program_count;
	u8 terminal_count;
	u8 subgraph_count;
	u8 reserved[5];
};

struct ipu_fw_generic_program_manifest {
	u16 *dev_chn_size;
	u16 *dev_chn_offset;
	u16 *ext_mem_size;
	u16 *ext_mem_offset;
	u8 cell_id;
	u8 cells[IPU_FW_PSYS_PROCESS_MAX_CELLS];
	u8 cell_type_id;
	u8 *is_dfm_relocatable;
	u32 *dfm_port_bitmap;
	u32 *dfm_active_port_bitmap;
};

struct ipu_fw_generic_process {
	u16 ext_mem_id;
	u16 ext_mem_offset;
	u16 dev_chn_offset;
	u16 cell_id;
	u16 dfm_port_bitmap;
	u16 dfm_active_port_bitmap;
};

struct ipu_resource_definitions {
	u32 num_cells;
	u32 num_cells_type;
	const u32 *cells;
	u32 num_dev_channels;
	const u16 *dev_channels;

	u32 num_ext_mem_types;
	u32 num_ext_mem_ids;
	const u16 *ext_mem_ids;

	u32 num_dfm_ids;
	const u16 *dfms;

	u32 cell_mem_row;
	const enum ipu_mem_id *cell_mem;
	struct ipu_fw_generic_process process;
};

/*
 * This struct represents all of the currently allocated
 * resources from IPU model. It is used also for allocating
 * resources for the next set of PGs to be run on IPU
 * (ie. those PGs which are not yet being run and which don't
 * yet reserve real IPU resources).
 */
#define IPU_PSYS_RESOURCE_OVERALLOC 2	/* Some room for ABI / ext lib delta */
struct ipu_psys_resource_pool {
	u32 cells;	/* Bitmask of cells allocated */
	struct ipu_resource dev_channels[IPU_FW_PSYS_N_DEV_CHN_ID +
					 IPU_PSYS_RESOURCE_OVERALLOC];
	struct ipu_resource ext_memory[IPU_FW_PSYS_N_MEM_ID +
				       IPU_PSYS_RESOURCE_OVERALLOC];
	struct ipu_resource dfms[IPU_FW_PSYS_N_DEV_DFM_ID +
				 IPU_PSYS_RESOURCE_OVERALLOC];
};

/*
 * This struct keeps book of the resources allocated for a specific PG.
 * It is used for freeing up resources from struct ipu_psys_resources
 * when the PG is released from IPU4 (or model of IPU4).
 */
struct ipu_psys_resource_alloc {
	u32 cells;	/* Bitmask of cells needed */
	struct ipu_resource_alloc
	 resource_alloc[IPU_MAX_RESOURCES];
	int resources;
};

struct ipu_fw_psys_process_group;

int ipu_psys_resource_pool_init(struct ipu_psys_resource_pool *pool);

void ipu_psys_resource_pool_cleanup(struct ipu_psys_resource_pool *pool);

void ipu_psys_resource_alloc_init(struct ipu_psys_resource_alloc *alloc);

int ipu_psys_allocate_resources(const struct device *dev,
				struct ipu_fw_psys_process_group *pg,
				void *pg_manifest,
				struct ipu_psys_resource_alloc *alloc,
				struct ipu_psys_resource_pool *pool);
int ipu_psys_move_resources(const struct device *dev,
			    struct ipu_psys_resource_alloc *alloc,
			    struct ipu_psys_resource_pool *source_pool,
			    struct ipu_psys_resource_pool *target_pool);

void ipu_psys_free_resources(struct ipu_psys_resource_alloc *alloc,
			     struct ipu_psys_resource_pool *pool);

#endif
