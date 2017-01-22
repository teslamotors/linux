/*
* Support for Intel Camera Imaging ISP subsystem.
* Copyright (c) 2010 - 2017, Intel Corporation.
*
* This program is free software; you can redistribute it and/or modify it
* under the terms and conditions of the GNU General Public License,
* version 2, as published by the Free Software Foundation.
*
* This program is distributed in the hope it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*/

#ifndef __IA_CSS_PSYS_PROCESS_IMPL_H
#define __IA_CSS_PSYS_PROCESS_IMPL_H

#include <ia_css_psys_process.h>

#include <ia_css_psys_process_group.h>
#include <ia_css_psys_program_manifest.h>

#include <error_support.h>
#include <misc_support.h>
#include <assert_support.h>

#include <vied_nci_psys_system_global.h>

#include "ia_css_psys_dynamic_trace.h"
#include "ia_css_psys_process_private_types.h"

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
vied_nci_cell_ID_t ia_css_process_get_cell(
	const ia_css_process_t					*process)
{
	DECLARE_ERRVAL
	vied_nci_cell_ID_t	cell_id = VIED_NCI_N_CELL_ID;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_get_cell(): enter:\n");

	verifexitval(process != NULL, EFAULT);

	cell_id = (vied_nci_cell_ID_t)(process->cell_id);

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_get_cell invalid argument\n");
	}

	return cell_id;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
vied_nci_mem_ID_t ia_css_process_get_ext_mem_id(
	const ia_css_process_t		*process,
	const vied_nci_mem_type_ID_t	mem_type)
{
	DECLARE_ERRVAL
	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_get_ext_mem(): enter:\n");

	verifexitval(process != NULL && mem_type < VIED_NCI_N_DATA_MEM_TYPE_ID, EFAULT);

EXIT:
	if (!noerror()) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_get_ext_mem invalid argument\n");
		return IA_CSS_PROCESS_INVALID_OFFSET;
	}
	return process->ext_mem_id[mem_type];
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
vied_nci_resource_size_t ia_css_process_get_dev_chn(
	const ia_css_process_t		*process,
	const vied_nci_dev_chn_ID_t	dev_chn_id)
{
	DECLARE_ERRVAL
	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_get_dev_chn(): enter:\n");

	verifexitval(process != NULL && dev_chn_id < VIED_NCI_N_DEV_CHN_ID, EFAULT);

EXIT:
	if (!noerror()) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_get_dev_chn(): invalid arguments\n");
		return IA_CSS_PROCESS_INVALID_OFFSET;
	}
	return process->dev_chn_offset[dev_chn_id];
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
vied_nci_resource_size_t ia_css_process_get_int_mem_offset(
	const ia_css_process_t				*process,
	const vied_nci_mem_type_ID_t			mem_id)
{
	DECLARE_ERRVAL
	vied_nci_resource_size_t int_mem_offset = IA_CSS_PROCESS_INVALID_OFFSET;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_get_int_mem_offset(): enter:\n");

	verifexitval(process != NULL && mem_id < VIED_NCI_N_MEM_TYPE_ID, EFAULT);

EXIT:
	if (noerror()) {
		int_mem_offset = process->int_mem_offset[mem_id];
	} else {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_get_int_mem_offset invalid argument\n");
	}

	return int_mem_offset;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
vied_nci_resource_size_t ia_css_process_get_ext_mem_offset(
	const ia_css_process_t				*process,
	const vied_nci_mem_type_ID_t			mem_type_id)
{
	DECLARE_ERRVAL
	vied_nci_resource_size_t ext_mem_offset = IA_CSS_PROCESS_INVALID_OFFSET;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_get_ext_mem_offset(): enter:\n");

	verifexitval(process != NULL && mem_type_id < VIED_NCI_N_DATA_MEM_TYPE_ID, EFAULT);

EXIT:
	if (noerror()) {
		ext_mem_offset = process->ext_mem_offset[mem_type_id];
	} else {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_get_ext_mem_offset invalid argument\n");
	}

	return ext_mem_offset;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
size_t ia_css_process_get_size(
	const ia_css_process_t					*process)
{
	DECLARE_ERRVAL
	size_t	size = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_get_size(): enter:\n");

	verifexitval(process != NULL, EFAULT);

EXIT:
	if (noerror()) {
		size = process->size;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			       "ia_css_process_get_size invalid argument\n");
	}

	return size;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
ia_css_process_state_t ia_css_process_get_state(
	const ia_css_process_t					*process)
{
	DECLARE_ERRVAL
	ia_css_process_state_t	state = IA_CSS_N_PROCESS_STATES;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		       "ia_css_process_get_state(): enter:\n");

	verifexitval(process != NULL, EFAULT);

EXIT:
	if (noerror()) {
		state = process->state;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			       "ia_css_process_get_state invalid argument\n");
	}

	return state;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
int ia_css_process_set_state(
	ia_css_process_t					*process,
	ia_css_process_state_t				state)
{
	DECLARE_ERRVAL
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		       "ia_css_process_set_state(): enter:\n");

	verifexitval(process != NULL, EFAULT);

	process->state = state;
	retval = 0;
EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_set_state invalid argument\n");
	}

	return retval;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
uint8_t ia_css_process_get_cell_dependency_count(
	const ia_css_process_t					*process)
{
	DECLARE_ERRVAL
	uint8_t	cell_dependency_count = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		       "ia_css_process_get_cell_dependency_count(): enter:\n");

	verifexitval(process != NULL, EFAULT);
	cell_dependency_count = process->cell_dependency_count;

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
		"ia_css_process_get_cell_dependency_count invalid argument\n");
	}
	return cell_dependency_count;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
uint8_t ia_css_process_get_terminal_dependency_count(
	const ia_css_process_t					*process)
{
	DECLARE_ERRVAL
	uint8_t	terminal_dependency_count = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_get_terminal_dependency_count(): enter:\n");

	verifexitval(process != NULL, EFAULT);
	terminal_dependency_count = process->terminal_dependency_count;

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_get_terminal_dependency_count invalid argument process\n");
	}
	return terminal_dependency_count;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
ia_css_process_group_t *ia_css_process_get_parent(
	const ia_css_process_t					*process)
{
	DECLARE_ERRVAL
	ia_css_process_group_t	*parent = NULL;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_get_parent(): enter:\n");

	verifexitval(process != NULL, EFAULT);

	parent =
	(ia_css_process_group_t *) ((char *)process + process->parent_offset);

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_get_parent invalid argument process\n");
	}
	return parent;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
ia_css_program_ID_t ia_css_process_get_program_ID(
	const ia_css_process_t					*process)
{
	DECLARE_ERRVAL
	ia_css_program_ID_t		id = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		       "ia_css_process_get_program_ID(): enter:\n");

	verifexitval(process != NULL, EFAULT);

	id = process->ID;

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
		    "ia_css_process_get_program_ID invalid argument process\n");
	}
	return id;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
vied_nci_resource_id_t ia_css_process_get_cell_dependency(
	const ia_css_process_t *process,
	const unsigned int cell_num)
{
	DECLARE_ERRVAL
	vied_nci_resource_id_t cell_dependency =
		IA_CSS_PROCESS_INVALID_DEPENDENCY;
	vied_nci_resource_id_t *cell_dep_ptr = NULL;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_get_cell_dependency(): enter:\n");

	verifexitval(process != NULL, EFAULT);
	verifexitval(cell_num < process->cell_dependency_count, EFAULT);

	cell_dep_ptr =
		(vied_nci_resource_id_t *)
		((char *)process + process->cell_dependencies_offset);
	cell_dependency = *(cell_dep_ptr + cell_num);
EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
		       "ia_css_process_get_cell_dependency invalid argument\n");
	}
	return cell_dependency;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
uint8_t ia_css_process_get_terminal_dependency(
	const ia_css_process_t					*process,
	const unsigned int					terminal_num)
{
	DECLARE_ERRVAL
	uint8_t *ter_dep_ptr = NULL;
	uint8_t ter_dep = IA_CSS_PROCESS_INVALID_DEPENDENCY;

	verifexitval(process != NULL, EFAULT);
	verifexitval(terminal_num < process->terminal_dependency_count, EFAULT);

	ter_dep_ptr = (uint8_t *) ((char *)process +
				   process->terminal_dependencies_offset);

	ter_dep = *(ter_dep_ptr + terminal_num);

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
		       "ia_css_process_get_terminal_dependency invalid argument\n");
	}
	return ter_dep;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
ia_css_kernel_bitmap_t ia_css_process_get_kernel_bitmap(
	const ia_css_process_t					*process)
{
	DECLARE_ERRVAL
	ia_css_kernel_bitmap_t		bitmap = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		       "ia_css_process_get_kernel_bitmap(): enter:\n");

	verifexitval(process != NULL, EFAULT);

	bitmap = process->kernel_bitmap;

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
		 "ia_css_process_get_kernel_bitmap invalid argument process\n");
	}
	return bitmap;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
bool ia_css_is_process_valid(
	const ia_css_process_t		*process,
	const ia_css_program_manifest_t	*p_manifest)
{
	DECLARE_ERRVAL
	bool invalid_flag = false;
	ia_css_program_ID_t prog_id;
	ia_css_kernel_bitmap_t prog_kernel_bitmap;

	verifexitval(NULL != process, EFAULT);
	verifexitval(NULL != p_manifest, EFAULT);

	prog_id = ia_css_process_get_program_ID(process);
	verifjmpexit(prog_id ==
		ia_css_program_manifest_get_program_ID(p_manifest));

	prog_kernel_bitmap =
		ia_css_program_manifest_get_kernel_bitmap(p_manifest);

	invalid_flag = (process->size <= process->cell_dependencies_offset) ||
		   (process->size <= process->terminal_dependencies_offset) ||
		   !ia_css_is_kernel_bitmap_subset(prog_kernel_bitmap,
						   process->kernel_bitmap);

	if (ia_css_has_program_manifest_fixed_cell(p_manifest)) {
		vied_nci_cell_ID_t cell_id;

		cell_id = ia_css_program_manifest_get_cell_ID(p_manifest);
		invalid_flag = invalid_flag ||
			    (cell_id != (vied_nci_cell_ID_t)(process->cell_id));
	}
	invalid_flag = invalid_flag ||
		((process->cell_dependency_count +
		  process->terminal_dependency_count) == 0) ||
		(process->cell_dependency_count !=
	ia_css_program_manifest_get_program_dependency_count(p_manifest)) ||
		(process->terminal_dependency_count !=
	ia_css_program_manifest_get_terminal_dependency_count(p_manifest));

	/* TODO: to be removed once all PGs pass validation */
	if (invalid_flag == true) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, INFO,
			"ia_css_is_process_valid(): false\n");
	}

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_is_process_valid() invalid argument\n");
		return false;
	} else {
		return (!invalid_flag);
	}
}

#endif /* __IA_CSS_PSYS_PROCESS_IMPL_H */
