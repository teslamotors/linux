/*
* Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2016, Intel Corporation.
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


#include <ia_css_psys_process.h>

#include <ia_css_psys_process_group.h>
#include <ia_css_psys_program_manifest.h>

#include <error_support.h>
#include <misc_support.h>
#include <assert_support.h>

#include <vied_nci_psys_system_global.h>

#include "ia_css_psys_dynamic_trace.h"

#define	N_UINT32_IN_PROCESS_STRUCT				1
#define	N_UINT16_IN_PROCESS_STRUCT				3
#define	N_UINT8_IN_PROCESS_STRUCT				2

#define SIZE_OF_PROCESS_STRUCT_BITS \
	(IA_CSS_KERNEL_BITMAP_BITS	\
	+ (N_UINT32_IN_PROCESS_STRUCT * 32) \
	+ IA_CSS_PROGRAM_ID_BITS \
	+ IA_CSS_PROCESS_STATE_BITS \
	+ (N_UINT16_IN_PROCESS_STRUCT * 16) \
	+ (VIED_NCI_N_MEM_TYPE_ID * VIED_NCI_RESOURCE_SIZE_BITS) \
	+ (VIED_NCI_N_DATA_MEM_TYPE_ID * VIED_NCI_RESOURCE_SIZE_BITS) \
	+ (VIED_NCI_N_DEV_CHN_ID * VIED_NCI_RESOURCE_SIZE_BITS) \
	+ VIED_NCI_RESOURCE_ID_BITS \
	+ (VIED_NCI_N_MEM_TYPE_ID * VIED_NCI_RESOURCE_ID_BITS) \
	+ (VIED_NCI_N_DATA_MEM_TYPE_ID * VIED_NCI_RESOURCE_ID_BITS) \
	+ (N_UINT8_IN_PROCESS_STRUCT * 8) \
	+ (N_PADDING_UINT8_IN_PROCESS_STRUCT * 8))

struct ia_css_process_s {
	/**< Indicate which kernels lead to this process being used */
	ia_css_kernel_bitmap_t kernel_bitmap;
	uint32_t size; /**< Size of this structure */
	ia_css_program_ID_t ID; /**< Referal ID to a specific program FW */
	/**< State of the process FSM dependent on the parent FSM */
	ia_css_process_state_t state;
	int16_t parent_offset; /**< Reference to the process group */
	/**< Array[dependency_count] of ID's of the cells that provide input */
	uint16_t cell_dependencies_offset;
	/*< Array[terminal_dependency_count] of indices of connected terminals*/
	uint16_t terminal_dependencies_offset;
	/**< (internal) Memory allocation offset given to this process */
	vied_nci_resource_size_t int_mem_offset[VIED_NCI_N_MEM_TYPE_ID];
	/**< (external) Memory allocation offset given to this process */
	vied_nci_resource_size_t ext_mem_offset[VIED_NCI_N_DATA_MEM_TYPE_ID];
	/**< Device channel allocation offset given to this process */
	vied_nci_resource_size_t dev_chn_offset[VIED_NCI_N_DEV_CHN_ID];
	/**< (mandatory) specification of a cell to be used by this process */
	vied_nci_resource_id_t cell_id;
	/**< (internal) Memory ID; This is redundant, derived from cell_id */
	vied_nci_resource_id_t int_mem_id[VIED_NCI_N_MEM_TYPE_ID];
	/**< (external) Memory ID */
	vied_nci_resource_id_t ext_mem_id[VIED_NCI_N_DATA_MEM_TYPE_ID];
	/**< Number of processes (mapped on cells) this process depends on */
	uint8_t cell_dependency_count;
	/**< Number of terminals this process depends on */
	uint8_t terminal_dependency_count;
	/**< Padding bytes for 64bit alignment*/
	uint8_t padding[N_PADDING_UINT8_IN_PROCESS_STRUCT];
};

/* This source file is created with the intention of sharing and
 * compiled for host and firmware. Since there is no native 64bit
 * data type support for firmware this wouldn't compile for SP
 * tile. The part of the file that is not compilable are marked
 * with the following __HIVECC marker and this comment. Once we
 * come up with a solution to address this issue this will be
 * removed.
 */
#if !defined(__HIVECC)
size_t ia_css_sizeof_process(
	const ia_css_program_manifest_t			*manifest,
	const ia_css_program_param_t			*param)
{
	size_t	size = 0, tmp_size;

	uint8_t	program_dependency_count;
	uint8_t terminal_dependency_count;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_sizeof_process(): enter:\n");

	COMPILATION_ERROR_IF(
		SIZE_OF_PROCESS_STRUCT_BITS !=
			(CHAR_BIT * sizeof(ia_css_process_t)));

	COMPILATION_ERROR_IF(0 != sizeof(ia_css_process_t)%sizeof(uint64_t));

	verifexit(manifest != NULL, EINVAL);
	verifexit(param != NULL, EINVAL);

	size += sizeof(ia_css_process_t);

	program_dependency_count =
		ia_css_program_manifest_get_program_dependency_count(manifest);
	terminal_dependency_count =
		ia_css_program_manifest_get_terminal_dependency_count(manifest);

	tmp_size = program_dependency_count*sizeof(vied_nci_resource_id_t);
	size += tot_bytes_for_pow2_align(sizeof(uint64_t), tmp_size);
	tmp_size = terminal_dependency_count*sizeof(uint8_t);
	size += tot_bytes_for_pow2_align(sizeof(uint64_t), tmp_size);

EXIT:
	if (NULL == manifest || NULL == param) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_sizeof_process invalid argument\n");
	}
	return size;
}

ia_css_process_t *ia_css_process_create(
	void					*raw_mem,
	const ia_css_program_manifest_t		*manifest,
	const ia_css_program_param_t		*param)
{
	size_t	tmp_size;
	int retval = -1;
	ia_css_process_t	*process = NULL;
	char *process_raw_ptr = (char *) raw_mem;

	/* size_t	size = ia_css_sizeof_process(manifest, param); */
	uint8_t	program_dependency_count;
	uint8_t	terminal_dependency_count;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, INFO,
		"ia_css_process_create(): enter:\n");

	verifexit(manifest != NULL, EINVAL);
	verifexit(param != NULL, EINVAL);
	verifexit(process_raw_ptr != NULL, EINVAL);

	process = (ia_css_process_t *) process_raw_ptr;
	verifexit(process != NULL, EINVAL);

	process->kernel_bitmap =
		ia_css_program_manifest_get_kernel_bitmap(manifest);
	process->state = IA_CSS_PROCESS_CREATED;

	program_dependency_count =
		ia_css_program_manifest_get_program_dependency_count(manifest);
	terminal_dependency_count =
		ia_css_program_manifest_get_terminal_dependency_count(manifest);

	/* A process requires at least one input or output */
	verifexit((program_dependency_count +
		   terminal_dependency_count) != 0, EINVAL);

	process_raw_ptr += sizeof(ia_css_process_t);
	if (program_dependency_count != 0) {
		process->cell_dependencies_offset =
			(uint16_t) (process_raw_ptr - (char *)process);
		tmp_size =
		      program_dependency_count * sizeof(vied_nci_resource_id_t);
		process_raw_ptr +=
			tot_bytes_for_pow2_align(sizeof(uint64_t), tmp_size);
	} else {
		process->cell_dependencies_offset = 0;
	}

	if (terminal_dependency_count != 0) {
		process->terminal_dependencies_offset =
			(uint16_t) (process_raw_ptr - (char *)process);
	}

	process->size = (uint32_t)ia_css_sizeof_process(manifest, param);

	process->ID = ia_css_program_manifest_get_program_ID(manifest);

	verifexit(process->ID != 0, EINVAL);

	process->cell_dependency_count = program_dependency_count;
	process->terminal_dependency_count = terminal_dependency_count;

	process->parent_offset = 0;

	verifexit(ia_css_process_clear_all(process) == 0, EINVAL);

	process->state = IA_CSS_PROCESS_READY;
	retval = 0;

	IA_CSS_TRACE_2(PSYSAPI_DYNAMIC, INFO,
		"ia_css_process_create(): Created successfully process %p ID 0x%x\n",
		process, process->ID);

EXIT:
	if (NULL == manifest || NULL == param) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_create invalid argument\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_create failed (%i)\n", retval);
		process = ia_css_process_destroy(process);
	}
	return process;
}

ia_css_process_t *ia_css_process_destroy(
	ia_css_process_t *process)
{

	return process;
}
#endif

int ia_css_process_set_cell(
	ia_css_process_t					*process,
	const vied_nci_cell_ID_t				cell_id)
{
	int	retval = -1;
	vied_nci_resource_bitmap_t		bit_mask;
	vied_nci_resource_bitmap_t		resource_bitmap;
	ia_css_process_group_t			*parent;
	ia_css_process_group_state_t	parent_state;
	ia_css_process_state_t			state;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_set_cell(): enter:\n");

	verifexit(process != NULL, EINVAL);

	parent = ia_css_process_get_parent(process);

	verifexit(parent != NULL, EINVAL);

	parent_state = ia_css_process_group_get_state(parent);
	state = ia_css_process_get_state(process);

/* Some programs are mapped on a fixed cell,
 * when the process group is created
 */
	verifexit(((parent_state == IA_CSS_PROCESS_GROUP_BLOCKED) ||
		(parent_state == IA_CSS_PROCESS_GROUP_STARTED) ||
		(parent_state == IA_CSS_PROCESS_GROUP_CREATED) ||
		/* If the process group has already been created, but no VP cell
		 * has been assigned to this process (i.e. not fixed in
		 * manifest), then we need to set the cell of this process
		 * while its parent state is READY (the ready state is set at
		 * the end of ia_css_process_group_create)
		 */
		(parent_state == IA_CSS_PROCESS_GROUP_READY)), EINVAL);
	verifexit(state == IA_CSS_PROCESS_READY, EINVAL);

/* Some programs are mapped on a fixed cell, thus check is not secure,
 * but it will detect a preset, the process manager will do the secure check
 */
	verifexit(ia_css_process_get_cell(process) ==
		  VIED_NCI_N_CELL_ID, EINVAL);

	bit_mask = vied_nci_cell_bit_mask(cell_id);
	resource_bitmap = ia_css_process_group_get_resource_bitmap(parent);

	verifexit(bit_mask != 0, EINVAL);
	verifexit(vied_nci_is_bitmap_clear(bit_mask, resource_bitmap), EINVAL);

	process->cell_id = (vied_nci_resource_id_t)cell_id;
	resource_bitmap = vied_nci_bitmap_set(resource_bitmap, bit_mask);

	retval = ia_css_process_group_set_resource_bitmap(
			parent, resource_bitmap);
EXIT:
	if (NULL == process) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_set_cell invalid argument process\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_set_cell failed (%i)\n", retval);
	}
	return retval;
}

int ia_css_process_clear_cell(
	ia_css_process_t *process)
{
	int	retval = -1;
	vied_nci_cell_ID_t				cell_id;
	ia_css_process_group_t			*parent;
	vied_nci_resource_bitmap_t		resource_bitmap;
	vied_nci_resource_bitmap_t		bit_mask;
	ia_css_process_group_state_t	parent_state;
	ia_css_process_state_t			state;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		       "ia_css_process_clear_cell(): enter:\n");
	verifexit(process != NULL, EINVAL);

	cell_id = ia_css_process_get_cell(process);
	parent = ia_css_process_get_parent(process);

	verifexit(parent != NULL, EINVAL);

	parent_state = ia_css_process_group_get_state(parent);
	state = ia_css_process_get_state(process);

	verifexit(((parent_state == IA_CSS_PROCESS_GROUP_BLOCKED)
		   || (parent_state == IA_CSS_PROCESS_GROUP_STARTED)), EINVAL);
	verifexit(state == IA_CSS_PROCESS_READY, EINVAL);

	bit_mask = vied_nci_cell_bit_mask(cell_id);
	resource_bitmap = ia_css_process_group_get_resource_bitmap(parent);

	verifexit(bit_mask != 0, EINVAL);
	verifexit(vied_nci_is_bitmap_set(bit_mask, resource_bitmap), EINVAL);

	process->cell_id = (vied_nci_resource_id_t)VIED_NCI_N_CELL_ID;
	resource_bitmap = vied_nci_bitmap_clear(resource_bitmap, bit_mask);

	retval = ia_css_process_group_set_resource_bitmap(
			parent, resource_bitmap);
EXIT:
	if (NULL == process) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_clear_cell invalid argument process\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_clear_cell failed (%i)\n", retval);
	}
	return retval;
}

vied_nci_cell_ID_t ia_css_process_get_cell(
	const ia_css_process_t					*process)
{
	vied_nci_cell_ID_t	cell_id = VIED_NCI_N_CELL_ID;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_get_cell(): enter:\n");

	if (process != NULL) {
		cell_id = (vied_nci_cell_ID_t)(process->cell_id);
	} else {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			       "ia_css_process_get_cell invalid argument\n");
	}
	return cell_id;
}

int ia_css_process_set_int_mem(
	ia_css_process_t				*process,
	const vied_nci_mem_type_ID_t			mem_type_id,
	const vied_nci_resource_size_t			offset)
{
	int	retval = -1;
	ia_css_process_group_t	*parent;
	vied_nci_cell_ID_t	cell_id;
	ia_css_process_group_state_t	parent_state;
	ia_css_process_state_t	state;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		       "ia_css_process_set_int_mem(): enter:\n");

	verifexit(process != NULL, EINVAL);
	verifexit(mem_type_id < VIED_NCI_N_MEM_TYPE_ID, EINVAL);

	parent = ia_css_process_get_parent(process);
	cell_id = ia_css_process_get_cell(process);

	parent_state = ia_css_process_group_get_state(parent);
	state = ia_css_process_get_state(process);

	/* TODO : separate process group start and run from
	*	  process_group_exec_cmd()
	*/
	verifexit(((parent_state == IA_CSS_PROCESS_GROUP_BLOCKED) ||
		   (parent_state == IA_CSS_PROCESS_GROUP_STARTED) ||
		   (parent_state == IA_CSS_PROCESS_GROUP_RUNNING)), EINVAL);
	verifexit(state == IA_CSS_PROCESS_READY, EINVAL);

	if (vied_nci_is_cell_mem_of_type(cell_id, mem_type_id, mem_type_id)) {
		vied_nci_mem_ID_t mem_id =
			vied_nci_cell_get_mem(cell_id, mem_type_id);

			process->int_mem_id[mem_type_id] = mem_id;
			process->int_mem_offset[mem_type_id] = offset;
			retval = 0;
	}
EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_set_int_mem failed (%i)\n", retval);
	}
	return retval;
}

int ia_css_process_clear_int_mem(
	ia_css_process_t *process,
	const vied_nci_mem_type_ID_t mem_type_id)
{
	int	retval = -1;
	uint16_t	mem_index;
	ia_css_process_group_t	*parent;
	vied_nci_cell_ID_t	cell_id;
	ia_css_process_group_state_t	parent_state;
	ia_css_process_state_t	state;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		       "ia_css_process_clear_int_mem(): enter:\n");

	verifexit(process != NULL, EINVAL);
	verifexit(mem_type_id < VIED_NCI_N_MEM_TYPE_ID, EINVAL);

	parent = ia_css_process_get_parent(process);
	cell_id = ia_css_process_get_cell(process);

	/* We should have a check on NULL != parent but it parent is NULL
	 * ia_css_process_group_get_state will return
	 * IA_CSS_N_PROCESS_GROUP_STATES so it will be filtered anyway later.
	*/

	/* verifexit(parent != NULL, EINVAL); */

	parent_state = ia_css_process_group_get_state(parent);
	state = ia_css_process_get_state(process);

	verifexit(((parent_state == IA_CSS_PROCESS_GROUP_BLOCKED)
		   || (parent_state == IA_CSS_PROCESS_GROUP_STARTED)), EINVAL);
	verifexit(state == IA_CSS_PROCESS_READY, EINVAL);

/* We could just clear the field, but lets check the state for
 * consistency first
 */
	for (mem_index = 0; mem_index < (int)VIED_NCI_N_MEM_TYPE_ID;
	     mem_index++) {
		if (vied_nci_is_cell_mem_of_type(
			cell_id, mem_index, mem_type_id)) {
			vied_nci_mem_ID_t mem_id =
				vied_nci_cell_get_mem(cell_id, mem_index);
			int mem_of_type;

			mem_of_type =
				vied_nci_is_mem_of_type(mem_id, mem_type_id);

			assert(mem_of_type);
			assert((process->int_mem_id[mem_type_id] == mem_id) ||
				(process->int_mem_id[mem_type_id] ==
				VIED_NCI_N_MEM_ID));
			process->int_mem_id[mem_type_id] = VIED_NCI_N_MEM_ID;
			process->int_mem_offset[mem_type_id] =
				IA_CSS_PROCESS_INVALID_OFFSET;
			retval = 0;
		}
	}

EXIT:
	if (NULL == process || mem_type_id >= VIED_NCI_N_MEM_TYPE_ID) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_clear_int_mem invalid argument\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_clear_int_mem failed (%i)\n", retval);
}
return retval;
}

vied_nci_mem_ID_t ia_css_process_get_ext_mem_id(
	const ia_css_process_t		*process,
	const vied_nci_mem_type_ID_t	mem_type)
	{
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
			"ia_css_process_get_ext_mem(): enter:\n");

		if (process != NULL && mem_type < VIED_NCI_N_DATA_MEM_TYPE_ID) {
			return process->ext_mem_id[mem_type];
	} else {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_get_ext_mem invalid argument\n");
		return IA_CSS_PROCESS_INVALID_OFFSET;
	}
}

int ia_css_process_set_ext_mem(
	ia_css_process_t *process,
	const vied_nci_mem_ID_t mem_id,
	const vied_nci_resource_size_t offset)
{
	int	retval = -1;
	ia_css_process_group_t	*parent;
	vied_nci_cell_ID_t	cell_id;
	ia_css_process_group_state_t	parent_state;
	ia_css_process_state_t	state;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		       "ia_css_process_set_ext_mem(): enter:\n");

	verifexit(process != NULL, EINVAL);

	parent = ia_css_process_get_parent(process);
	cell_id = ia_css_process_get_cell(process);

	/* We should have a check on NULL != parent but it parent is NULL
	 * ia_css_process_group_get_state will return
	 * IA_CSS_N_PROCESS_GROUP_STATES so it will be filtered anyway later.
	*/

	/* verifexit(parent != NULL, EINVAL); */

	parent_state = ia_css_process_group_get_state(parent);
	state = ia_css_process_get_state(process);

	/* TODO : separate process group start and run from
	*	  process_group_exec_cmd()
	*/
	verifexit(((parent_state == IA_CSS_PROCESS_GROUP_BLOCKED) ||
		   (parent_state == IA_CSS_PROCESS_GROUP_STARTED) ||
		   (parent_state == IA_CSS_PROCESS_GROUP_RUNNING)), EINVAL);
	verifexit(state == IA_CSS_PROCESS_READY, EINVAL);

/* Check that the memory actually exists, "vied_nci_has_cell_mem_of_id()"
 * will return false on error
 */
	if (((!vied_nci_has_cell_mem_of_id(cell_id, mem_id) &&
		(vied_nci_mem_get_type(mem_id) != VIED_NCI_PMEM_TYPE_ID)) ||
		(vied_nci_mem_get_type(mem_id) == VIED_NCI_GMEM_TYPE_ID)) &&
		(mem_id < VIED_NCI_N_MEM_ID)) {

		vied_nci_mem_type_ID_t mem_type_id =
			vied_nci_mem_get_type(mem_id);

		verifexit(mem_type_id < VIED_NCI_N_DATA_MEM_TYPE_ID, EINVAL);
		process->ext_mem_id[mem_type_id] = mem_id;
		process->ext_mem_offset[mem_type_id] = offset;
		retval = 0;
	}

EXIT:
	if (NULL == process) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_set_ext_mem invalid argument process\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_set_ext_mem failed (%i)\n", retval);
}
return retval;
}

int ia_css_process_clear_ext_mem(
	ia_css_process_t *process,
	const vied_nci_mem_type_ID_t mem_type_id)
{
	int	retval = -1;
	ia_css_process_group_t			*parent;
	ia_css_process_group_state_t	parent_state;
	ia_css_process_state_t			state;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		       "ia_css_process_clear_ext_mem(): enter:\n");

	verifexit(process != NULL, EINVAL);
	verifexit(mem_type_id < VIED_NCI_N_DATA_MEM_TYPE_ID, EINVAL);

	parent = ia_css_process_get_parent(process);
	state = ia_css_process_get_state(process);

	verifexit(parent != NULL, EINVAL);
	verifexit(state == IA_CSS_PROCESS_READY, EINVAL);

	parent_state = ia_css_process_group_get_state(parent);

	verifexit(((parent_state == IA_CSS_PROCESS_GROUP_BLOCKED) ||
		   (parent_state == IA_CSS_PROCESS_GROUP_STARTED)), EINVAL);

	process->ext_mem_id[mem_type_id] = VIED_NCI_N_MEM_ID;
	process->ext_mem_offset[mem_type_id] = IA_CSS_PROCESS_INVALID_OFFSET;

	retval = 0;
EXIT:
	if (NULL == process || mem_type_id >= VIED_NCI_N_DATA_MEM_TYPE_ID) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_clear_ext_mem invalid argument\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_clear_ext_mem failed (%i)\n", retval);
	}
	return retval;
}

vied_nci_resource_size_t ia_css_process_get_dev_chn(
	const ia_css_process_t		*process,
	const vied_nci_dev_chn_ID_t	dev_chn_id)
{

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
i		"ia_css_process_get_dev_chn(): enter:\n");

	if (process == NULL || dev_chn_id >= VIED_NCI_N_DEV_CHN_ID) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_get_dev_chn(): invalid arguments\n");
		return IA_CSS_PROCESS_INVALID_OFFSET;
	}

	return process->dev_chn_offset[dev_chn_id];
}

int ia_css_process_set_dev_chn(
	ia_css_process_t *process,
	const vied_nci_dev_chn_ID_t dev_chn_id,
	const vied_nci_resource_size_t offset)
{
	int	retval = -1;
	ia_css_process_group_t			*parent;
	ia_css_process_group_state_t	parent_state;
	ia_css_process_state_t			state;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		       "ia_css_process_set_dev_chn(): enter:\n");

	verifexit(process != NULL, EINVAL);
	verifexit(dev_chn_id <= VIED_NCI_N_DEV_CHN_ID, EINVAL);

	parent = ia_css_process_get_parent(process);
	state = ia_css_process_get_state(process);

	parent_state = ia_css_process_group_get_state(parent);

	/* TODO : separate process group start and run from
	*	  process_group_exec_cmd()
	*/
	verifexit(((parent_state == IA_CSS_PROCESS_GROUP_BLOCKED) ||
		   (parent_state == IA_CSS_PROCESS_GROUP_STARTED) ||
		   (parent_state == IA_CSS_PROCESS_GROUP_RUNNING)), EINVAL);
	verifexit(state == IA_CSS_PROCESS_READY, EINVAL);

	process->dev_chn_offset[dev_chn_id] = offset;

	retval = 0;
EXIT:
	if (NULL == process || dev_chn_id >= VIED_NCI_N_DEV_CHN_ID) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_set_dev_chn invalid argument\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_set_dev_chn failed (%i)\n", retval);
	}
	return retval;
}

int ia_css_process_clear_dev_chn(
	ia_css_process_t *process,
	const vied_nci_dev_chn_ID_t dev_chn_id)
{
	int	retval = -1;
	ia_css_process_group_t			*parent;
	ia_css_process_group_state_t	parent_state;
	ia_css_process_state_t			state;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		       "ia_css_process_clear_dev_chn(): enter:\n");

	verifexit(process != NULL, EINVAL);

	parent = ia_css_process_get_parent(process);

	/* We should have a check on NULL != parent but it parent is NULL
	 * ia_css_process_group_get_state will return
	 * IA_CSS_N_PROCESS_GROUP_STATES so it will be filtered anyway later.
	*/

	/* verifexit(parent != NULL, EINVAL); */

	parent_state = ia_css_process_group_get_state(parent);
	state = ia_css_process_get_state(process);

	verifexit(((parent_state == IA_CSS_PROCESS_GROUP_BLOCKED)
		   || (parent_state == IA_CSS_PROCESS_GROUP_STARTED)), EINVAL);
	verifexit(state == IA_CSS_PROCESS_READY, EINVAL);

	verifexit(dev_chn_id <= VIED_NCI_N_DEV_CHN_ID, EINVAL);

	process->dev_chn_offset[dev_chn_id] = IA_CSS_PROCESS_INVALID_OFFSET;

	retval = 0;
EXIT:
	if (NULL == process) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
		     "ia_css_process_clear_dev_chn invalid argument process\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_clear_dev_chn failed (%i)\n", retval);
	}
	return retval;
}

int ia_css_process_clear_all(
	ia_css_process_t *process)
{
	int	retval = -1;
	ia_css_process_group_t			*parent;
	ia_css_process_group_state_t	parent_state;
	ia_css_process_state_t			state;
	int	mem_index;
	int	dev_chn_index;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		       "ia_css_process_clear_all(): enter:\n");

	verifexit(process != NULL, EINVAL);

	parent = ia_css_process_get_parent(process);
	state = ia_css_process_get_state(process);

	/* We should have a check on NULL != parent but it parent is NULL
	 * ia_css_process_group_get_state will return
	 * IA_CSS_N_PROCESS_GROUP_STATES so it will be filtered anyway later.
	*/

	/* verifexit(parent != NULL, EINVAL); */

	parent_state = ia_css_process_group_get_state(parent);

/* Resource clear can only be called in excluded states contrary to set */
	verifexit((parent_state != IA_CSS_PROCESS_GROUP_RUNNING) ||
		   (parent_state == IA_CSS_N_PROCESS_GROUP_STATES), EINVAL);
	verifexit((state == IA_CSS_PROCESS_CREATED) ||
		  (state == IA_CSS_PROCESS_READY), EINVAL);

	for (dev_chn_index = 0; dev_chn_index < VIED_NCI_N_DEV_CHN_ID;
		dev_chn_index++) {
		process->dev_chn_offset[dev_chn_index] =
			IA_CSS_PROCESS_INVALID_OFFSET;
	}
/* No difference whether a cell_id has been set or not, clear all */
	for (mem_index = 0; mem_index < VIED_NCI_N_DATA_MEM_TYPE_ID;
		mem_index++) {
		process->ext_mem_id[mem_index] = VIED_NCI_N_MEM_ID;
		process->ext_mem_offset[mem_index] =
			IA_CSS_PROCESS_INVALID_OFFSET;
	}
	for (mem_index = 0; mem_index < VIED_NCI_N_MEM_TYPE_ID; mem_index++) {
		process->int_mem_id[mem_index] = VIED_NCI_N_MEM_ID;
		process->int_mem_offset[mem_index] =
			IA_CSS_PROCESS_INVALID_OFFSET;
	}
	process->cell_id = (vied_nci_resource_id_t)VIED_NCI_N_CELL_ID;

	retval = 0;
EXIT:
	if (NULL == process) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_clear_all invalid argument process\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_clear_all failed (%i)\n", retval);
	}
	return retval;
}

int ia_css_process_acquire(
	ia_css_process_t *process)
{
	int	retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_acquire(): enter:\n");

	verifexit(process != NULL, EINVAL);

	retval = 0;
EXIT:
	if (NULL == process) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_acquire invalid argument process\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_acquire failed (%i)\n", retval);
	}
	return retval;
}

int ia_css_process_release(
	ia_css_process_t *process)
{
	int	retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, INFO,
		"ia_css_process_release(): enter:\n");

	verifexit(process != NULL, EINVAL);

	retval = 0;
EXIT:
	if (NULL == process) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_t invalid argument process\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_release failed (%i)\n", retval);
	}
	return retval;
}

int ia_css_process_print(const ia_css_process_t *process, void *fid)
{
	int		retval = -1;
	int		i, dev_chn_index;
	uint16_t mem_index;
	uint8_t	cell_dependency_count, terminal_dependency_count;
	vied_nci_cell_ID_t	cell_id = ia_css_process_get_cell(process);

	NOT_USED(fid);

	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, INFO,
		"ia_css_process_print(process %p): enter:\n", process);

	verifexit(process != NULL, EINVAL);

	IA_CSS_TRACE_6(PSYSAPI_DYNAMIC, INFO,
	"\tprocess %p, sizeof %d, programID %d, state %d, parent %p, cell %d\n",
		process,
		(int)ia_css_process_get_size(process),
		(int)ia_css_process_get_program_ID(process),
		(int)ia_css_process_get_state(process),
		(void *)ia_css_process_get_parent(process),
		(int)process->cell_id);

	for (mem_index = 0; mem_index < (int)VIED_NCI_N_MEM_TYPE_ID;
		mem_index++) {
		vied_nci_mem_ID_t mem_id =
			(vied_nci_mem_ID_t)(process->int_mem_id[mem_index]);

		verifexit(((mem_id == vied_nci_cell_get_mem(cell_id, mem_index))
			|| (mem_id == VIED_NCI_N_MEM_ID)), EINVAL);

		IA_CSS_TRACE_4(PSYSAPI_DYNAMIC, INFO,
			"\tinternal index %d, type %d, id %d offset 0x%x\n",
			mem_index,
			(int)vied_nci_cell_get_mem_type(cell_id, mem_index),
			(int)mem_id,
			process->int_mem_offset[mem_index]);
	}

	for (mem_index = 0; mem_index < (int)VIED_NCI_N_DATA_MEM_TYPE_ID;
		mem_index++) {
		vied_nci_mem_ID_t mem_id =
			(vied_nci_mem_ID_t)(process->ext_mem_id[mem_index]);

		verifexit((vied_nci_has_cell_mem_of_id(cell_id, mem_id) ||
			  (mem_id == VIED_NCI_N_MEM_ID)), EINVAL);

		IA_CSS_TRACE_4(PSYSAPI_DYNAMIC, INFO,
			"\texternal index %d, type %d, id %d offset 0x%x\n",
			mem_index,
			(int)vied_nci_cell_get_mem_type(cell_id, mem_index),
			(int)mem_id,
			process->int_mem_offset[mem_index]);
	}
	for (dev_chn_index = 0; dev_chn_index < (int)VIED_NCI_N_DEV_CHN_ID;
		dev_chn_index++) {
		IA_CSS_TRACE_3(PSYSAPI_DYNAMIC, INFO,
			"\tdevice channel index %d, type %d, offset 0x%x\n",
			dev_chn_index,
			(int)dev_chn_index,
			process->dev_chn_offset[dev_chn_index]);
	}

	cell_dependency_count =
		ia_css_process_get_cell_dependency_count(process);
	if (cell_dependency_count == 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, INFO,
			"\tcell_dependencies[%d] {};\n", cell_dependency_count);
	} else {
		vied_nci_resource_id_t cell_dependency;

		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, INFO,
			"\tcell_dependencies[%d] {", cell_dependency_count);
		for (i = 0; i < (int)cell_dependency_count - 1; i++) {
			cell_dependency =
				ia_css_process_get_cell_dependency(process, i);
			IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, INFO,
				"%4d, ", cell_dependency);
		}
		cell_dependency =
			ia_css_process_get_cell_dependency(process, i);
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, INFO,
			"%4d}\n", cell_dependency);
		(void)cell_dependency;
	}

	terminal_dependency_count =
		ia_css_process_get_terminal_dependency_count(process);
	if (terminal_dependency_count == 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, INFO,
			"\tterminal_dependencies[%d] {};\n",
			terminal_dependency_count);
	} else {
		uint8_t terminal_dependency;

		terminal_dependency_count =
			ia_css_process_get_terminal_dependency_count(process);
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, INFO,
			"\tterminal_dependencies[%d] {",
			terminal_dependency_count);
		for (i = 0; i < (int)terminal_dependency_count - 1; i++) {
			terminal_dependency =
			     ia_css_process_get_terminal_dependency(process, i);
			IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, INFO,
				"%4d, ", terminal_dependency);
		}
		terminal_dependency =
			ia_css_process_get_terminal_dependency(process, i);
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, INFO,
			"%4d}\n", terminal_dependency);
		(void)terminal_dependency;
	}

	retval = 0;
EXIT:
	if (NULL == process) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_print invalid argument process\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_print failed (%i)\n", retval);
	}
	return retval;
}

vied_nci_resource_size_t ia_css_process_get_int_mem_offset(
	const ia_css_process_t				*process,
	const vied_nci_mem_type_ID_t			mem_id)
{
	vied_nci_resource_size_t int_mem_offset = IA_CSS_PROCESS_INVALID_OFFSET;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_get_int_mem_offset(): enter:\n");

	if (process == NULL || mem_id >= VIED_NCI_N_MEM_TYPE_ID) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_get_int_mem_offset invalid argument\n");
	} else {
		int_mem_offset = process->int_mem_offset[mem_id];
	}

	return int_mem_offset;
}

vied_nci_resource_size_t ia_css_process_get_ext_mem_offset(
	const ia_css_process_t				*process,
	const vied_nci_mem_type_ID_t			mem_type_id)
{
	vied_nci_resource_size_t ext_mem_offset = IA_CSS_PROCESS_INVALID_OFFSET;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_get_ext_mem_offset(): enter:\n");

	if (process == NULL || mem_type_id >= VIED_NCI_N_DATA_MEM_TYPE_ID) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_get_ext_mem_offset invalid argument\n");
	} else {
		ext_mem_offset = process->ext_mem_offset[mem_type_id];
	}

	return ext_mem_offset;
}

size_t ia_css_process_get_size(
	const ia_css_process_t					*process)
{
	size_t	size = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_get_size(): enter:\n");

	if (process != NULL) {
		size = process->size;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			       "ia_css_process_get_size invalid argument\n");
	}
	return size;
}

ia_css_process_state_t ia_css_process_get_state(
	const ia_css_process_t					*process)
{
	ia_css_process_state_t	state = IA_CSS_N_PROCESS_STATES;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		       "ia_css_process_get_state(): enter:\n");

	if (process != NULL) {
		state = process->state;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			       "ia_css_process_get_state invalid argument\n");
	}
	return state;
}

int ia_css_process_set_state(
	ia_css_process_t					*process,
	ia_css_process_state_t				state)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		       "ia_css_process_set_state(): enter:\n");
	verifexit(process != NULL, EINVAL);

	process->state = state;
	retval = 0;
EXIT:
	if (NULL == process) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_set_state invalid argument\n");
	}

	return retval;
}

uint8_t ia_css_process_get_cell_dependency_count(
	const ia_css_process_t					*process)
{
	uint8_t	cell_dependency_count = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		       "ia_css_process_get_cell_dependency_count(): enter:\n");

	verifexit(process != NULL, EINVAL);
	cell_dependency_count = process->cell_dependency_count;

EXIT:
	if (NULL == process) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
		"ia_css_process_get_cell_dependency_count invalid argument\n");
	}
	return cell_dependency_count;
}

uint8_t ia_css_process_get_terminal_dependency_count(
	const ia_css_process_t					*process)
{
	uint8_t	terminal_dependency_count = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_get_terminal_dependency_count(): enter:\n");

	verifexit(process != NULL, EINVAL);
	terminal_dependency_count = process->terminal_dependency_count;

EXIT:
	if (NULL == process) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_get_terminal_dependency_count invalid argument process\n");
	}
	return terminal_dependency_count;
}

int ia_css_process_set_parent(
	ia_css_process_t					*process,
	ia_css_process_group_t					*parent)
{
	int	retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		       "ia_css_process_set_parent(): enter:\n");

	verifexit(process != NULL, EINVAL);
	verifexit(parent != NULL, EINVAL);

	process->parent_offset = (uint16_t) ((char *)parent - (char *)process);
	retval = 0;
EXIT:
	if (NULL == process || NULL == parent) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_set_parent invalid argument\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_set_parent failed (%i)\n", retval);
	}
	return retval;
}

ia_css_process_group_t *ia_css_process_get_parent(
	const ia_css_process_t					*process)
{
	ia_css_process_group_t	*parent = NULL;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_get_parent(): enter:\n");

	verifexit(process != NULL, EINVAL);

	parent =
	(ia_css_process_group_t *) ((char *)process + process->parent_offset);

EXIT:
	if (NULL == process) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_get_parent invalid argument process\n");
	}
	return parent;
}

ia_css_program_ID_t ia_css_process_get_program_ID(
	const ia_css_process_t					*process)
{
	ia_css_program_ID_t		id = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		       "ia_css_process_get_program_ID(): enter:\n");

	verifexit(process != NULL, EINVAL);

	id = process->ID;

EXIT:
	if (NULL == process) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
		    "ia_css_process_get_program_ID invalid argument process\n");
	}
	return id;
}

vied_nci_resource_id_t ia_css_process_get_cell_dependency(
	const ia_css_process_t *process,
	const unsigned int cell_num)
{
	vied_nci_resource_id_t cell_dependency =
		IA_CSS_PROCESS_INVALID_DEPENDENCY;
	vied_nci_resource_id_t *cell_dep_ptr = NULL;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_get_cell_dependency(): enter:\n");
	verifexit(process != NULL, EINVAL);
	if (cell_num < process->cell_dependency_count) {
		cell_dep_ptr =
			(vied_nci_resource_id_t *)
			((char *)process + process->cell_dependencies_offset);
		cell_dependency = *(cell_dep_ptr + cell_num);
	} else {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
		       "ia_css_process_get_cell_dependency invalid argument\n");
	}

EXIT:
	return cell_dependency;
}

int ia_css_process_set_cell_dependency(
	const ia_css_process_t					*process,
	const unsigned int					dep_index,
	const vied_nci_resource_id_t				id)
{
	int retval = -1;
	uint8_t *process_dep_ptr;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_set_cell_dependency(): enter:\n");
	verifexit(process != NULL, EINVAL);

	process_dep_ptr =
		(uint8_t *)process + process->cell_dependencies_offset +
			   dep_index*sizeof(vied_nci_resource_id_t);


	*process_dep_ptr = id;
	retval = 0;
EXIT:
	return retval;
}

uint8_t ia_css_process_get_terminal_dependency(
	const ia_css_process_t					*process,
	const unsigned int					terminal_num)
{
	uint8_t *ter_dep_ptr = NULL;
	uint8_t ter_dep = IA_CSS_PROCESS_INVALID_DEPENDENCY;

	verifexit(process != NULL, EINVAL);
	verifexit(terminal_num < process->terminal_dependency_count, EINVAL);

	ter_dep_ptr = (uint8_t *) ((char *)process +
				   process->terminal_dependencies_offset);

	ter_dep = *(ter_dep_ptr + terminal_num);

EXIT:
	return ter_dep;
}


int ia_css_process_set_terminal_dependency(
	const ia_css_process_t				*process,
	const unsigned int				dep_index,
	const vied_nci_resource_id_t		id)
{
	int retval = -1;
	uint8_t *terminal_dep_ptr;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_set_terminal_dependency(): enter:\n");
	verifexit(process != NULL, EINVAL);
	verifexit(ia_css_process_get_terminal_dependency_count(process) >
		  dep_index, EINVAL);

	terminal_dep_ptr =
		(uint8_t *)process + process->terminal_dependencies_offset +
			   dep_index*sizeof(uint8_t);

	*terminal_dep_ptr = id;
	retval = 0;
EXIT:
	return retval;
}


ia_css_kernel_bitmap_t ia_css_process_get_kernel_bitmap(
	const ia_css_process_t					*process)
{
	ia_css_kernel_bitmap_t		bitmap = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		       "ia_css_process_get_kernel_bitmap(): enter:\n");

	verifexit(process != NULL, EINVAL);

	bitmap = process->kernel_bitmap;

EXIT:
	if (NULL == process) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
		 "ia_css_process_get_kernel_bitmap invalid argument process\n");
	}
	return bitmap;
}

int ia_css_process_cmd(
	ia_css_process_t					*process,
	const ia_css_process_cmd_t				cmd)
{
	int	retval = -1;
	ia_css_process_state_t	state;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, INFO, "ia_css_process_cmd(): enter:\n");

	verifexit(process != NULL, EINVAL);

	state = ia_css_process_get_state(process);

	verifexit(state != IA_CSS_PROCESS_ERROR, EINVAL);
	verifexit(state < IA_CSS_N_PROCESS_STATES, EINVAL);

	switch (cmd) {
	case IA_CSS_PROCESS_CMD_NOP:
		break;
	case IA_CSS_PROCESS_CMD_ACQUIRE:
		verifexit(state == IA_CSS_PROCESS_READY, EINVAL);
		break;
	case IA_CSS_PROCESS_CMD_RELEASE:
		verifexit(state == IA_CSS_PROCESS_READY, EINVAL);
		break;
	case IA_CSS_PROCESS_CMD_START:
		verifexit((state == IA_CSS_PROCESS_READY)
			  || (state == IA_CSS_PROCESS_STOPPED), EINVAL);
		process->state = IA_CSS_PROCESS_STARTED;
		break;
	case IA_CSS_PROCESS_CMD_LOAD:
		verifexit(state == IA_CSS_PROCESS_STARTED, EINVAL);
		process->state = IA_CSS_PROCESS_RUNNING;
		break;
	case IA_CSS_PROCESS_CMD_STOP:
		verifexit((state == IA_CSS_PROCESS_RUNNING)
			  || (state == IA_CSS_PROCESS_SUSPENDED), EINVAL);
		process->state = IA_CSS_PROCESS_STOPPED;
		break;
	case IA_CSS_PROCESS_CMD_SUSPEND:
		verifexit(state == IA_CSS_PROCESS_RUNNING, EINVAL);
		process->state = IA_CSS_PROCESS_SUSPENDED;
		break;
	case IA_CSS_PROCESS_CMD_RESUME:
		verifexit(state == IA_CSS_PROCESS_SUSPENDED, EINVAL);
		process->state = IA_CSS_PROCESS_RUNNING;
		break;
	case IA_CSS_N_PROCESS_CMDS:	/* Fall through */
	default:
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_cmd invalid cmd (0x%x)\n", cmd);
		goto EXIT;
	}
	retval = 0;
EXIT:
	if (NULL == process) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_cmd invalid argument process\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_cmd failed (%i)\n", retval);
	}
	return retval;
}

bool ia_css_is_process_valid(
	const ia_css_process_t		*process,
	const ia_css_program_manifest_t	*p_manifest)
{
	bool invalid_flag = false;
	ia_css_program_ID_t prog_id;
	ia_css_kernel_bitmap_t prog_kernel_bitmap;

	verifjmpexit(NULL != process);
	verifjmpexit(NULL != p_manifest);

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
	return (!invalid_flag);

EXIT:
	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
		"ia_css_is_process_valid() invalid argument\n");
	return false;
}
