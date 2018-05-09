// SPDX-License_Identifier: GPL-2.0
// Copyright (C) 2015 - 2018 Intel Corporation

#include "ipu-fw-psys.h"

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
void ipu_fw_psys_set_process_cell_id(struct ipu_fw_psys_process *ptr, u8 index,
				     u8 value)
{
	ptr->cell_id = value;
}

u8 ipu_fw_psys_get_process_cell_id(struct ipu_fw_psys_process *ptr, u8 index)
{
	return ptr->cell_id;
}

void ipu_fw_psys_set_process_dev_chn_offset(struct ipu_fw_psys_process *ptr,
					    u16 offset, u16 value)
{
	ptr->dev_chn_offset[offset] = value;
}

void ipu_fw_psys_set_process_ext_mem_offset(struct ipu_fw_psys_process *ptr,
					    u16 offset, u16 value)
{
	ptr->ext_mem_offset[offset] = value;
}

void ipu_fw_psys_set_process_ext_mem_id(struct ipu_fw_psys_process *ptr,
					u16 offset, u8 value)
{
	ptr->ext_mem_id[offset] = value;
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
