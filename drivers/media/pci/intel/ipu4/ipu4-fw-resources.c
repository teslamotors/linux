// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2015 - 2018 Intel Corporation

#include "ipu-fw-psys.h"

#include <uapi/asm-generic/errno-base.h>

/* resources table */
/*
 * Cell types by cell IDs
 */
const u32 ipu_fw_psys_cell_types[IPU_FW_PSYS_N_CELL_ID] = {
	IPU_FW_PSYS_SP_CTRL_TYPE_ID,
	IPU_FW_PSYS_SP_SERVER_TYPE_ID,
	IPU_FW_PSYS_SP_SERVER_TYPE_ID,
	IPU_FW_PSYS_VP_TYPE_ID,
	IPU_FW_PSYS_VP_TYPE_ID,
	IPU_FW_PSYS_VP_TYPE_ID,
	IPU_FW_PSYS_VP_TYPE_ID,
	IPU_FW_PSYS_ACC_ISA_TYPE_ID,
	IPU_FW_PSYS_ACC_PSA_TYPE_ID,
	IPU_FW_PSYS_ACC_PSA_TYPE_ID,
	IPU_FW_PSYS_ACC_PSA_TYPE_ID,
	IPU_FW_PSYS_ACC_PSA_TYPE_ID,
	IPU_FW_PSYS_ACC_PSA_TYPE_ID,
	IPU_FW_PSYS_ACC_PSA_TYPE_ID,
	IPU_FW_PSYS_ACC_OSA_TYPE_ID,
	IPU_FW_PSYS_GDC_TYPE_ID,
	IPU_FW_PSYS_GDC_TYPE_ID
};

const u16 ipu_fw_num_dev_channels[IPU_FW_PSYS_N_DEV_CHN_ID] = {
	IPU_FW_PSYS_DEV_CHN_DMA_EXT0_MAX_SIZE,
	IPU_FW_PSYS_DEV_CHN_GDC_MAX_SIZE,
	IPU_FW_PSYS_DEV_CHN_DMA_EXT1_READ_MAX_SIZE,
	IPU_FW_PSYS_DEV_CHN_DMA_EXT1_WRITE_MAX_SIZE,
	IPU_FW_PSYS_DEV_CHN_DMA_INTERNAL_MAX_SIZE,
	IPU_FW_PSYS_DEV_CHN_DMA_IPFD_MAX_SIZE,
	IPU_FW_PSYS_DEV_CHN_DMA_ISA_MAX_SIZE,
	IPU_FW_PSYS_DEV_CHN_DMA_FW_MAX_SIZE,
#ifdef CONFIG_VIDEO_INTEL_IPU4P
	IPU_FW_PSYS_DEV_CHN_DMA_CMPRS_MAX_SIZE
#endif
};

const u16 ipu_fw_psys_mem_size[IPU_FW_PSYS_N_MEM_ID] = {
	IPU_FW_PSYS_VMEM0_MAX_SIZE,
	IPU_FW_PSYS_VMEM1_MAX_SIZE,
	IPU_FW_PSYS_VMEM2_MAX_SIZE,
	IPU_FW_PSYS_VMEM3_MAX_SIZE,
	IPU_FW_PSYS_VMEM4_MAX_SIZE,
	IPU_FW_PSYS_BAMEM0_MAX_SIZE,
	IPU_FW_PSYS_BAMEM1_MAX_SIZE,
	IPU_FW_PSYS_BAMEM2_MAX_SIZE,
	IPU_FW_PSYS_BAMEM3_MAX_SIZE,
	IPU_FW_PSYS_DMEM0_MAX_SIZE,
	IPU_FW_PSYS_DMEM1_MAX_SIZE,
	IPU_FW_PSYS_DMEM2_MAX_SIZE,
	IPU_FW_PSYS_DMEM3_MAX_SIZE,
	IPU_FW_PSYS_DMEM4_MAX_SIZE,
	IPU_FW_PSYS_DMEM5_MAX_SIZE,
	IPU_FW_PSYS_DMEM6_MAX_SIZE,
	IPU_FW_PSYS_DMEM7_MAX_SIZE,
	IPU_FW_PSYS_PMEM0_MAX_SIZE,
	IPU_FW_PSYS_PMEM1_MAX_SIZE,
	IPU_FW_PSYS_PMEM2_MAX_SIZE,
	IPU_FW_PSYS_PMEM3_MAX_SIZE
};

const enum ipu_mem_id
ipu_fw_psys_cell_mem[IPU_FW_PSYS_N_CELL_ID][IPU_FW_PSYS_N_MEM_TYPE_ID] = {
	{
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_DMEM0_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID
	},
	{
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_DMEM1_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID
	},
	{
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_DMEM2_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID
	},
	{
	 IPU_FW_PSYS_VMEM4_ID,
	 IPU_FW_PSYS_DMEM4_ID,
	 IPU_FW_PSYS_VMEM0_ID,
	 IPU_FW_PSYS_BAMEM0_ID,
	 IPU_FW_PSYS_PMEM0_ID
	},
	{
	 IPU_FW_PSYS_VMEM4_ID,
	 IPU_FW_PSYS_DMEM5_ID,
	 IPU_FW_PSYS_VMEM1_ID,
	 IPU_FW_PSYS_BAMEM1_ID,
	 IPU_FW_PSYS_PMEM1_ID
	},
	{
	 IPU_FW_PSYS_VMEM4_ID,
	 IPU_FW_PSYS_DMEM6_ID,
	 IPU_FW_PSYS_VMEM2_ID,
	 IPU_FW_PSYS_BAMEM2_ID,
	 IPU_FW_PSYS_PMEM2_ID,
	},
	{
	 IPU_FW_PSYS_VMEM4_ID,
	 IPU_FW_PSYS_DMEM7_ID,
	 IPU_FW_PSYS_VMEM3_ID,
	 IPU_FW_PSYS_BAMEM3_ID,
	 IPU_FW_PSYS_PMEM3_ID,
	},
	{
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID
	},
	{
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID
	},
	{
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID
	},
	{
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID
	},
	{
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID
	},
	{
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID
	},
	{
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID
	},
	{
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID
	},
	{
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID
	},
	{
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID,
	 IPU_FW_PSYS_N_MEM_ID
	}
};

static const struct ipu_fw_resource_definitions default_defs = {
	.cells = ipu_fw_psys_cell_types,
	.num_cells = IPU_FW_PSYS_N_CELL_ID,
	.num_cells_type = IPU_FW_PSYS_N_CELL_TYPE_ID,

	.dev_channels = ipu_fw_num_dev_channels,
	.num_dev_channels = IPU_FW_PSYS_N_DEV_CHN_ID,

	.num_ext_mem_types = IPU_FW_PSYS_N_DATA_MEM_TYPE_ID,
	.num_ext_mem_ids = IPU_FW_PSYS_N_MEM_ID,
	.ext_mem_ids = ipu_fw_psys_mem_size,

	.num_dfm_ids = IPU_FW_PSYS_N_DEV_DFM_ID,

	.cell_mem_row = IPU_FW_PSYS_N_MEM_TYPE_ID,
	.cell_mem = &ipu_fw_psys_cell_mem[0][0],

	.process.ext_mem_id = offsetof(struct ipu_fw_psys_process,
				       ext_mem_id[0]),
	.process.ext_mem_offset = offsetof(struct ipu_fw_psys_process,
					   ext_mem_offset[0]),
	.process.dev_chn_offset = offsetof(struct ipu_fw_psys_process,
					   dev_chn_offset[0]),
	.process.cell_id = offsetof(struct ipu_fw_psys_process, cell_id),
};

const struct ipu_fw_resource_definitions *res_defs = &default_defs;

/********** Generic resource handling **********/

/*
 * Extension library gives byte offsets to its internal structures.
 * use those offsets to update fields. Without extension lib access
 * structures directly.
 */
int ipu_fw_psys_set_process_cell_id(struct ipu_fw_psys_process *ptr, u8 index,
				    u8 value)
{
	struct ipu_fw_psys_process_group *parent =
		(struct ipu_fw_psys_process_group *) ((char *)ptr +
		ptr->parent_offset);

	ptr->cell_id = value;
	parent->resource_bitmap |= 1 << value;

	return 0;
}

u8 ipu_fw_psys_get_process_cell_id(struct ipu_fw_psys_process *ptr, u8 index)
{
	return ptr->cell_id;
}

int ipu_fw_psys_clear_process_cell(struct ipu_fw_psys_process *ptr)
{
	struct ipu_fw_psys_process_group *parent;
	u8 cell_id = ipu_fw_psys_get_process_cell_id(ptr, 0);
	int retval = -1;

	parent = (struct ipu_fw_psys_process_group *) ((char *)ptr +
		ptr->parent_offset);
	if ((1 << cell_id) && ((1 << cell_id) & parent->resource_bitmap)) {
		ipu_fw_psys_set_process_cell_id(ptr, 0, IPU_FW_PSYS_N_CELL_ID);
		parent->resource_bitmap &= ~(1 << cell_id);
		retval = 0;
	}

	return retval;
}

int ipu_fw_psys_set_process_dev_chn_offset(struct ipu_fw_psys_process *ptr,
					   u16 offset, u16 value)
{
	ptr->dev_chn_offset[offset] = value;

	return 0;
}

int ipu_fw_psys_set_process_ext_mem(struct ipu_fw_psys_process *ptr,
				    u16 type_id, u16 mem_id, u16 offset)
{
	ptr->ext_mem_offset[type_id] = offset;
	ptr->ext_mem_id[type_id] = mem_id;

	return 0;
}

static struct ipu_fw_psys_program_manifest *
ipu_resource_get_program_manifest(
		const struct ipu_fw_psys_program_group_manifest *manifest,
		const unsigned int program_index)
{
	struct ipu_fw_psys_program_manifest *prg_manifest_base;
	u8 *program_manifest = NULL;
	u8 program_count;
	unsigned int i;

	program_count = manifest->program_count;

	prg_manifest_base = (struct ipu_fw_psys_program_manifest *)
	    ((char *)manifest + manifest->program_manifest_offset);
	if (program_index < program_count) {
		program_manifest = (u8 *) prg_manifest_base;
		for (i = 0; i < program_index; i++)
			program_manifest +=
			    ((struct ipu_fw_psys_program_manifest *)
			     program_manifest)->size;
	}

	return (struct ipu_fw_psys_program_manifest *)program_manifest;
}

int ipu_fw_psys_get_program_manifest_by_process(
		struct ipu_fw_generic_program_manifest *gen_pm,
		const struct ipu_fw_psys_program_group_manifest	*pg_manifest,
		struct ipu_fw_psys_process *process)
{
	u32 process_id = process->ID;
	int programs = pg_manifest->program_count;
	int i;

	for (i = 0; i < programs; i++) {
		u32 program_id;
		struct ipu_fw_psys_program_manifest *pm =
		    ipu_resource_get_program_manifest(pg_manifest, i);
		if (!pm)
			continue;
		program_id = pm->ID;
		if (program_id == process_id) {
			gen_pm->dev_chn_size = pm->dev_chn_size;
			gen_pm->dev_chn_offset = NULL;
			gen_pm->ext_mem_offset = NULL;
			gen_pm->cell_id = pm->cell_id;
			gen_pm->cell_type_id = pm->cell_type_id;
			gen_pm->ext_mem_size = pm->ext_mem_size;
			return 0;
		}
	}
	return -ENOENT;
}

