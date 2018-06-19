
/* SPDX-License_Identifier: GPL-2.0 */
/* Copyright (C) 2018 Intel Corporation */

#ifndef IPU_FW_RESOURCES_H
#define IPU_FW_RESOURCES_H

#include "ipu-resources.h"

/* common resource interface for both abi and api mode */
void ipu_fw_psys_set_process_cell_id(struct ipu_fw_psys_process *ptr, u8 index,
				     u8 value);
u8 ipu_fw_psys_get_process_cell_id(struct ipu_fw_psys_process *ptr, u8 index);
void ipu_fw_psys_set_process_dev_chn_offset(struct ipu_fw_psys_process *ptr,
					    u16 offset, u16 value);
void ipu_fw_psys_set_process_ext_mem_offset(struct ipu_fw_psys_process *ptr,
					    u16 offset, u16 value);
void ipu_fw_psys_set_process_ext_mem_id(struct ipu_fw_psys_process *ptr,
					u16 offset, u8 value);
int ipu_fw_psys_get_program_manifest_by_process(
	struct ipu_fw_generic_program_manifest *gen_pm,
	const struct ipu_fw_psys_program_group_manifest *pg_manifest,
	struct ipu_fw_psys_process *process);

#endif
