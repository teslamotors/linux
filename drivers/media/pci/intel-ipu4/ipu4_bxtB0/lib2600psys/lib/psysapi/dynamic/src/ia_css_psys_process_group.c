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

#include "ia_css_psys_process_group.h"
#include "ia_css_psys_dynamic_storage_class.h"

/*
 * Functions to possibly inline
 */

#ifndef __IA_CSS_PSYS_DYNAMIC_INLINE__
#include "ia_css_psys_process_group_impl.h"
#endif /* __IA_CSS_PSYS_DYNAMIC_INLINE__ */

/*
 * Functions not to inline
 */

/* This header is need for cpu memset to 0
* and process groups are not created in SP
*/
#if !defined(__HIVECC)
#include "cpu_mem_support.h"
#endif

/* This source file is created with the intention of sharing and
* compiled for host and firmware. Since there is no native 64bit
* data type support for firmware this wouldn't compile for SP
* tile. The part of the file that is not compilable are marked
* with the following __HIVECC marker and this comment. Once we
* come up with a solution to address this issue this will be
* removed.
*/
#if !defined(__HIVECC)
static bool ia_css_process_group_is_program_enabled(
	const ia_css_program_manifest_t *program_manifest,
	ia_css_kernel_bitmap_t enable_bitmap)
{
	ia_css_kernel_bitmap_t program_bitmap =
		ia_css_program_manifest_get_kernel_bitmap(program_manifest);
	ia_css_program_type_t program_type =
		ia_css_program_manifest_get_type(program_manifest);
	ia_css_kernel_bitmap_t program_enable_bitmap;

	if (!ia_css_is_kernel_bitmap_intersection_empty(enable_bitmap,
				program_bitmap)) {

		if (program_type == IA_CSS_PROGRAM_TYPE_EXCLUSIVE_SUB ||
			program_type == IA_CSS_PROGRAM_TYPE_EXCLUSIVE_SUPER ||
			program_type == IA_CSS_PROGRAM_TYPE_VIRTUAL_SUB) {
			/*
			 * EXCLUSIVE_SUB programs are subsets of
			 * EXCLUSIVE_SUPER so the bits of the enable_bitmap
			 * that refer to those are those of their
			 * EXCLUSIVE_SUPER program (on which the depend) and
			 * not the subset that their own program_bitmap has
			 */
			if (program_type ==
					IA_CSS_PROGRAM_TYPE_EXCLUSIVE_SUB ||
					program_type ==
					IA_CSS_PROGRAM_TYPE_VIRTUAL_SUB) {
				ia_css_kernel_bitmap_t super_program_bitmap;

				const ia_css_program_group_manifest_t *
					prog_group_manifest =
			ia_css_program_manifest_get_parent(program_manifest);
				uint8_t super_prog_idx =
				ia_css_program_manifest_get_program_dependency(
						program_manifest, 0);
				const ia_css_program_manifest_t	*
					super_program_manifest =
			ia_css_program_group_manifest_get_program_manifest(
					prog_group_manifest, super_prog_idx);

				verifexit(super_program_manifest != NULL);
				if (((program_type ==
					IA_CSS_PROGRAM_TYPE_EXCLUSIVE_SUB) &&
					(ia_css_program_manifest_get_type(
					super_program_manifest) !=
					IA_CSS_PROGRAM_TYPE_EXCLUSIVE_SUPER))
					|| ((program_type ==
					IA_CSS_PROGRAM_TYPE_VIRTUAL_SUB) &&
					(ia_css_program_manifest_get_type(
					super_program_manifest) !=
					IA_CSS_PROGRAM_TYPE_VIRTUAL_SUPER))) {
					IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
						"ia_css_process_group_is_program_enabled(): Error\n");
					verifexit(0);
				}

				super_program_bitmap =
				ia_css_program_manifest_get_kernel_bitmap(
						super_program_manifest);
				program_enable_bitmap =
					ia_css_kernel_bitmap_intersection(
						enable_bitmap,
						super_program_bitmap);
			} else {
				program_enable_bitmap =
					ia_css_kernel_bitmap_intersection(
						enable_bitmap, program_bitmap);
			}

			if (ia_css_is_kernel_bitmap_equal(
				program_enable_bitmap, program_bitmap)) {
				return true;
			}
		} else if (program_type == IA_CSS_PROGRAM_TYPE_VIRTUAL_SUPER) {
			/*
			 * Virtual super programs are not selectable
			 * only the virtual sub programs
			 */
			return false;
		} else {
			return true;
		}
	}

EXIT:
	return false;
}

static bool ia_css_process_group_is_terminal_enabled(
	const ia_css_terminal_manifest_t *terminal_manifest,
	ia_css_kernel_bitmap_t enable_bitmap)
{
	ia_css_terminal_type_t terminal_type;

	verifjmpexit(NULL != terminal_manifest);
	terminal_type = ia_css_terminal_manifest_get_type(terminal_manifest);

	if (ia_css_is_terminal_manifest_data_terminal(terminal_manifest)) {
		ia_css_data_terminal_manifest_t	*data_term_manifest =
			(ia_css_data_terminal_manifest_t *)terminal_manifest;
		ia_css_kernel_bitmap_t term_bitmap =
			ia_css_data_terminal_manifest_get_kernel_bitmap(
					data_term_manifest);
		/*
		 * Terminals depend on a kernel,
		 * if the kernel is present the program it contains and
		 * the terminal the program depends on are active
		 */
		if (!ia_css_is_kernel_bitmap_intersection_empty(
				enable_bitmap, term_bitmap)) {
			return true;
		}
	} else if (ia_css_is_terminal_manifest_spatial_parameter_terminal(
				terminal_manifest)) {
		ia_css_kernel_bitmap_t term_kernel_bitmap = 0;
		ia_css_spatial_param_terminal_manifest_t *spatial_term_man =
			(ia_css_spatial_param_terminal_manifest_t *)
			terminal_manifest;

		term_kernel_bitmap =
			ia_css_kernel_bitmap_set(
				term_kernel_bitmap,
				spatial_term_man->kernel_id);
		if (!ia_css_is_kernel_bitmap_intersection_empty(
				enable_bitmap, term_kernel_bitmap)) {
			return true;
		}

	} else if (ia_css_is_terminal_manifest_parameter_terminal(
			terminal_manifest) && terminal_type ==
			IA_CSS_TERMINAL_TYPE_PARAM_CACHED_IN) {
		return true;

	} else if (ia_css_is_terminal_manifest_parameter_terminal(
			terminal_manifest) && terminal_type ==
			IA_CSS_TERMINAL_TYPE_PARAM_CACHED_OUT) {
		/*
		 * For parameter out terminals, we disable the terminals
		 * if ALL the corresponding kernels are disabled,
		 * for parameter in terminals we cannot do this;
		 * even if kernels are disabled, it may be required that
		 * (HW) parameters must be supplied via the parameter
		 * in terminal (e.g. bypass bits).
		 */
		ia_css_kernel_bitmap_t term_kernel_bitmap = 0;
		ia_css_param_terminal_manifest_t *param_term_man =
			(ia_css_param_terminal_manifest_t *)terminal_manifest;
		ia_css_param_manifest_section_desc_t *section_desc;
		unsigned int section = 0;

		for (section = 0; section < param_term_man->
				param_manifest_section_desc_count; section++) {
			section_desc =
		ia_css_param_terminal_manifest_get_param_manifest_section_desc(
						param_term_man, section);
			verifjmpexit(section_desc != NULL);
			term_kernel_bitmap = ia_css_kernel_bitmap_set(
					term_kernel_bitmap,
					section_desc->kernel_id);
		}

		if (!ia_css_is_kernel_bitmap_intersection_empty(
					enable_bitmap, term_kernel_bitmap)) {
			return true;
		}
	} else if (ia_css_is_terminal_manifest_program_terminal(
				terminal_manifest)) {
		return true;
	}
EXIT:
	return false;
}

size_t ia_css_sizeof_process_group(
	const ia_css_program_group_manifest_t *manifest,
	const ia_css_program_group_param_t *param)
{
	size_t size = 0, tmp_size;
	int i, error_val = -1;
	uint8_t	process_count, process_num;
	uint8_t terminal_count;
	ia_css_kernel_bitmap_t enable_bitmap;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_sizeof_process_group(): enter:\n");

	verifexit(manifest != NULL);
	verifexit(param != NULL);

	COMPILATION_ERROR_IF(
		SIZE_OF_PROCESS_GROUP_STRUCT_BITS !=
			(CHAR_BIT * sizeof(ia_css_process_group_t)));

	COMPILATION_ERROR_IF(0 !=
			sizeof(ia_css_process_group_t) % sizeof(uint64_t));

	process_count =
		ia_css_process_group_compute_process_count(manifest, param);
	terminal_count =
		ia_css_process_group_compute_terminal_count(manifest, param);

	verifexit(process_count != 0);
	verifexit(terminal_count != 0);

	size += sizeof(ia_css_process_group_t);

	tmp_size = process_count * sizeof(uint16_t);
	size += tot_bytes_for_pow2_align(sizeof(uint64_t), tmp_size);

	tmp_size = terminal_count * sizeof(uint16_t);
	size += tot_bytes_for_pow2_align(sizeof(uint64_t), tmp_size);

	enable_bitmap =
		ia_css_program_group_param_get_kernel_enable_bitmap(param);
	process_num = 0;
	for (i = 0; i < (int)ia_css_program_group_manifest_get_program_count(
				manifest); i++) {
		ia_css_program_manifest_t *program_manifest =
		ia_css_program_group_manifest_get_program_manifest(manifest, i);
		ia_css_program_param_t *program_param =
			ia_css_program_group_param_get_program_param(param, i);

		if (ia_css_process_group_is_program_enabled(
					program_manifest, enable_bitmap)) {
			verifexit(process_num < process_count);
			size += ia_css_sizeof_process(
					program_manifest, program_param);
			process_num++;
		}
	}

	verifexit(process_num == process_count);

	for (i = 0; i < (int)ia_css_program_group_manifest_get_terminal_count(
				manifest); i++) {
		ia_css_terminal_manifest_t *terminal_manifest =
			ia_css_program_group_manifest_get_terminal_manifest(
					manifest, i);

		if (ia_css_process_group_is_terminal_enabled(
					terminal_manifest, enable_bitmap)) {
			size += ia_css_sizeof_terminal(
					terminal_manifest, param);
		}
	}

	error_val = 0;

EXIT:
	if (NULL == manifest || NULL == param) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_sizeof_process_group invalid argument\n");
	}
	if (error_val != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_sizeof_process_group ERROR(%d)\n", error_val);
	}
	return size;
}

ia_css_process_group_t *ia_css_process_group_create(
	void *process_grp_mem,
	const ia_css_program_group_manifest_t *manifest,
	const ia_css_program_group_param_t *param)
{
	size_t size = ia_css_sizeof_process_group(manifest, param);
	int retval = -1;
	int ret;
	int i;
	ia_css_process_group_t *process_group = NULL;
	uint8_t process_count, process_num;
	uint8_t	terminal_count, terminal_num;
	uint16_t fragment_count;
	char *process_grp_raw_ptr;
	uint16_t *process_tab_ptr, *terminal_tab_ptr;
	ia_css_kernel_bitmap_t enable_bitmap;

	IA_CSS_TRACE_3(PSYSAPI_DYNAMIC, INFO,
		"ia_css_process_group_create(process_grp_mem %p, manifest %p, group_param %p): enter:\n",
		process_grp_mem, manifest, param);

	verifexit(process_grp_mem != NULL);
	verifexit(manifest != NULL);
	verifexit(param != NULL);
	verifexit(ia_css_is_program_group_manifest_valid(manifest));

	process_group = (ia_css_process_group_t	*)process_grp_mem;
	ia_css_cpu_mem_set_zero(process_group, size);
	process_grp_raw_ptr = (char *) process_group;

	process_group->state = IA_CSS_PROCESS_GROUP_CREATED;

	fragment_count = ia_css_program_group_param_get_fragment_count(param);
	process_count =
		ia_css_process_group_compute_process_count(manifest, param);
	terminal_count =
		ia_css_process_group_compute_terminal_count(manifest, param);
	enable_bitmap =
		ia_css_program_group_param_get_kernel_enable_bitmap(param);

	process_group->fragment_count = fragment_count;
	process_group->process_count = process_count;
	process_group->terminal_count = terminal_count;

	process_grp_raw_ptr += sizeof(ia_css_process_group_t);
	process_tab_ptr = (uint16_t *) process_grp_raw_ptr;
	process_group->processes_offset =
		(uint16_t)(process_grp_raw_ptr - (char *)process_group);

	process_grp_raw_ptr += tot_bytes_for_pow2_align(
			sizeof(uint64_t), process_count * sizeof(uint16_t));
	terminal_tab_ptr = (uint16_t *) process_grp_raw_ptr;
	process_group->terminals_offset =
		(uint16_t)(process_grp_raw_ptr - (char *)process_group);

	/* Move raw pointer to the first process */
	process_grp_raw_ptr += tot_bytes_for_pow2_align(
			sizeof(uint64_t), terminal_count * sizeof(uint16_t));

	/* Set default */
	verifexit(ia_css_process_group_set_fragment_limit(
				process_group, fragment_count) == 0);

	/* Set process group terminal dependency list */
	/* This list is used during creating the process dependency list */
	uint8_t manifest_terminal_count =
		ia_css_program_group_manifest_get_terminal_count(manifest);

	terminal_num = 0;
	for (i = 0; i < (int)manifest_terminal_count; i++) {
		ia_css_terminal_manifest_t *t_manifest =
			ia_css_program_group_manifest_get_terminal_manifest(
					manifest, i);

		verifexit(NULL != t_manifest);
		if (ia_css_process_group_is_terminal_enabled(
					t_manifest, enable_bitmap)) {
			ia_css_terminal_t *terminal = NULL;
			ia_css_terminal_param_t *terminal_param =
				ia_css_program_group_param_get_terminal_param(
						param, i);

			verifexit(NULL != terminal_param);
			terminal_tab_ptr[terminal_num] =
				(uint16_t)(process_grp_raw_ptr -
						(char *)process_group);
			terminal = ia_css_terminal_create(
					process_grp_raw_ptr, t_manifest,
					terminal_param, enable_bitmap);
			verifexit(terminal != NULL);
			verifexit((ia_css_terminal_set_parent(
					terminal, process_group) == 0));
			verifexit((ia_css_terminal_set_terminal_manifest_index(
					terminal, i) == 0));
			IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, INFO,
				"ia_css_process_group_create: terminal_manifest_index %d\n",
				i);

			process_grp_raw_ptr += ia_css_terminal_get_size(
							terminal);
			terminal_num++;
		}
	}
	verifexit(terminal_num == terminal_count);

	process_num = 0;
	for (i = 0; i < (int)ia_css_program_group_manifest_get_program_count(
				manifest); i++) {
		ia_css_process_t *process = NULL;
		ia_css_program_manifest_t *program_manifest =
			ia_css_program_group_manifest_get_program_manifest(
					manifest, i);
		ia_css_program_param_t *program_param =
			ia_css_program_group_param_get_program_param(param, i);
		unsigned int prog_dep_index, proc_dep_index;
		unsigned int term_dep_index, term_index;

		if (ia_css_process_group_is_program_enabled(
					program_manifest, enable_bitmap)) {

			verifexit(process_num < process_count);

			process_tab_ptr[process_num] =
				(uint16_t)(process_grp_raw_ptr -
						(char *)process_group);
			process = ia_css_process_create(
					process_grp_raw_ptr,
					program_manifest,
					program_param);
			verifexit(process != NULL);

			ia_css_process_set_parent(process, process_group);
			if (ia_css_has_program_manifest_fixed_cell(
						program_manifest)) {
				vied_nci_cell_ID_t cell_id =
					ia_css_program_manifest_get_cell_ID(
							program_manifest);

				IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, INFO,
					"ia_css_process_group_create: cell_id %d\n",
					cell_id);
				ia_css_process_set_cell(process, cell_id);
			}

			process_grp_raw_ptr += ia_css_process_get_size(
					process);
			/*
			 * Set process dependencies of process derived
			 * from program manifest
			 */
			for (prog_dep_index = 0; prog_dep_index <
			ia_css_program_manifest_get_program_dependency_count(
				program_manifest); prog_dep_index++) {
				uint8_t dep_prog_idx =
				ia_css_program_manifest_get_program_dependency(
					program_manifest, prog_dep_index);
				const ia_css_program_manifest_t *
				dep_prg_manifest =
			ia_css_program_group_manifest_get_program_manifest(
					manifest, dep_prog_idx);
				ia_css_program_ID_t id =
				ia_css_program_manifest_get_program_ID(
						dep_prg_manifest);

				verifexit(id != 0);
				for (proc_dep_index = 0;
						proc_dep_index < process_num;
						proc_dep_index++) {
					ia_css_process_t *dep_process =
					ia_css_process_group_get_process(
							process_group,
							proc_dep_index);

					ia_css_process_set_cell_dependency(
							process,
							prog_dep_index, 0);

				if (ia_css_process_get_program_ID(
						dep_process) == id) {
					ia_css_process_set_cell_dependency(
							process,
							prog_dep_index,
							proc_dep_index);
						break;
					}
				}
			}
			process_num++;

			/*
			 * Set terminal dependencies of process derived
			 * from program manifest
			 */
			for (term_dep_index = 0; term_dep_index <
			ia_css_program_manifest_get_terminal_dependency_count(
				program_manifest); term_dep_index++) {
				uint8_t pm_term_index =
				ia_css_program_manifest_get_terminal_dependency
					(program_manifest, term_dep_index);

				verifexit(pm_term_index < manifest_terminal_count);
				IA_CSS_TRACE_2(PSYSAPI_DYNAMIC, INFO,
					"ia_css_process_group_create(): term_dep_index: %d, pm_term_index: %d\n",
					term_dep_index, pm_term_index);
				for (term_index = 0;
					term_index < terminal_count;
					term_index++) {
					ia_css_terminal_t *terminal =
					ia_css_process_group_get_terminal(
							process_group,
							term_index);

				if (ia_css_terminal_get_terminal_manifest_index
						(terminal) == pm_term_index) {
					ia_css_process_set_terminal_dependency(
							process,
							term_dep_index,
							term_index);
					IA_CSS_TRACE_3(PSYSAPI_DYNAMIC, INFO,
						"ia_css_process_group_create() set_terminal_dependency(process: %d, dep_idx: %d, term_idx: %d)\n",
						i, term_dep_index, term_index);

						break;
					}
				}
			}
		}
	}
	verifexit(process_num == process_count);

	process_group->size =
		(uint32_t)ia_css_sizeof_process_group(manifest, param);
	process_group->ID =
		ia_css_program_group_manifest_get_program_group_ID(manifest);

	/* Initialize performance measurement fields to zero */
	process_group->pg_load_start_ts     = 0;
	process_group->pg_load_cycles       = 0;
	process_group->pg_init_cycles       = 0;
	process_group->pg_processing_cycles = 0;

	verifexit(process_group->ID != 0);

	ret = ia_css_process_group_on_create(process_group, manifest, param);
	verifexit(ret == 0);

	process_group->state = IA_CSS_PROCESS_GROUP_READY;
	retval = 0;

	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, INFO,
		"ia_css_process_group_create(): Created successfully process group ID 0x%x\n",
		process_group->ID);

EXIT:
	if (NULL == process_grp_mem || NULL == manifest || NULL == param) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_create invalid argument\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_create failed (%i)\n", retval);
		process_group = ia_css_process_group_destroy(process_group);
	}
	return process_group;
}

ia_css_process_group_t *ia_css_process_group_destroy(
	ia_css_process_group_t *process_group)
{
	if (process_group != NULL) {
		ia_css_process_group_on_destroy(process_group);
		process_group = NULL;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_destroy invalid argument\n");
	}
	return process_group;
}

int ia_css_process_group_submit(
	ia_css_process_group_t *process_group)
{
	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, INFO,
		"ia_css_process_group_submit(): enter:\n");

	return ia_css_process_group_exec_cmd(process_group,
		IA_CSS_PROCESS_GROUP_CMD_SUBMIT);
}

int ia_css_process_group_start(
	ia_css_process_group_t *process_group)
{
	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, INFO,
		"ia_css_process_group_start(): enter:\n");

	return ia_css_process_group_exec_cmd(process_group,
		IA_CSS_PROCESS_GROUP_CMD_START);
}

int ia_css_process_group_stop(
	ia_css_process_group_t *process_group)
{
	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, INFO,
		"ia_css_process_group_stop(): enter:\n");

	return ia_css_process_group_exec_cmd(process_group,
		IA_CSS_PROCESS_GROUP_CMD_STOP);
}

int ia_css_process_group_run(
	ia_css_process_group_t *process_group)
{
	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, INFO,
		"ia_css_process_group_run(): enter:\n");

	return ia_css_process_group_exec_cmd(process_group,
		IA_CSS_PROCESS_GROUP_CMD_RUN);
}

int ia_css_process_group_suspend(
	ia_css_process_group_t *process_group)
{
	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, INFO,
		"ia_css_process_group_suspend(): enter:\n");

	return ia_css_process_group_exec_cmd(process_group,
		IA_CSS_PROCESS_GROUP_CMD_SUSPEND);
}

int ia_css_process_group_resume(
	ia_css_process_group_t *process_group)
{
	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, INFO,
		"ia_css_process_group_resume(): enter:\n");

	return ia_css_process_group_exec_cmd(process_group,
		IA_CSS_PROCESS_GROUP_CMD_RESUME);
}

int ia_css_process_group_reset(
	ia_css_process_group_t *process_group)
{
	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, INFO,
		"ia_css_process_group_reset(): enter:\n");

	return ia_css_process_group_exec_cmd(process_group,
		IA_CSS_PROCESS_GROUP_CMD_RESET);
}

int ia_css_process_group_abort(
	ia_css_process_group_t *process_group)
{
	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, INFO,
		"ia_css_process_group_abort(): enter:\n");

	return ia_css_process_group_exec_cmd(process_group,
		IA_CSS_PROCESS_GROUP_CMD_ABORT);
}

int ia_css_process_group_disown(
	ia_css_process_group_t *process_group)
{
	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, INFO,
		"ia_css_process_group_disown(): enter:\n");

	return ia_css_process_group_exec_cmd(process_group,
		IA_CSS_PROCESS_GROUP_CMD_DISOWN);
}

extern uint64_t ia_css_process_group_get_token(
	ia_css_process_group_t *process_group)
{
	uint64_t token = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_token(): enter:\n");

	verifexit(process_group != NULL);

	token = process_group->token;

EXIT:
	if (NULL == process_group) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_get_token invalid argument\n");
	}
	return token;
}

int ia_css_process_group_set_token(
	ia_css_process_group_t *process_group,
	const uint64_t token)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_set_token(): enter:\n");

	verifexit(process_group != NULL);
	verifexit(token != 0);

	process_group->token = token;

	retval = 0;
EXIT:
	if (NULL == process_group || 0 == token) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_set_token invalid argument\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_set_token failed (%i)\n",
			retval);
	}
	return retval;
}

extern uint64_t ia_css_process_group_get_private_token(
	ia_css_process_group_t *process_group)
{
	uint64_t token = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_private_token(): enter:\n");

	verifexit(process_group != NULL);

	token = process_group->private_token;

EXIT:
	if (NULL == process_group) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_get_private_token invalid argument\n");
	}
	return token;
}

int ia_css_process_group_set_private_token(
	ia_css_process_group_t *process_group,
	const uint64_t token)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_set_private_token(): enter:\n");

	verifexit(process_group != NULL);
	verifexit(token != 0);

	process_group->private_token = token;

	retval = 0;
EXIT:
	if (NULL == process_group || 0 == token) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_set_private_token invalid argument\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_set_private_token failed (%i)\n",
			retval);
	}
	return retval;
}

uint8_t ia_css_process_group_compute_process_count(
	const ia_css_program_group_manifest_t *manifest,
	const ia_css_program_group_param_t *param)
{
	uint8_t process_count = 0;
	ia_css_kernel_bitmap_t total_bitmap;
	ia_css_kernel_bitmap_t enable_bitmap;
	int i;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_compute_process_count(): enter:\n");

	verifexit(manifest != NULL);
	verifexit(param != NULL);

	total_bitmap =
		ia_css_program_group_manifest_get_kernel_bitmap(manifest);
	enable_bitmap =
		ia_css_program_group_param_get_kernel_enable_bitmap(param);

	verifexit(ia_css_is_program_group_manifest_valid(manifest));
	verifexit(ia_css_is_kernel_bitmap_subset(total_bitmap, enable_bitmap));
	verifexit(!ia_css_is_kernel_bitmap_empty(enable_bitmap));

	for (i = 0; i <
		(int)ia_css_program_group_manifest_get_program_count(manifest);
			i++) {
		ia_css_program_manifest_t *program_manifest =
			ia_css_program_group_manifest_get_program_manifest(
					manifest, i);
		ia_css_kernel_bitmap_t program_bitmap =
			ia_css_program_manifest_get_kernel_bitmap(
					program_manifest);
		/*
		 * Programs can be orthogonal,
		 * a mutually exclusive subset,
		 * or a concurrent subset
		 */
		if (!ia_css_is_kernel_bitmap_intersection_empty(enable_bitmap,
					program_bitmap)) {
			ia_css_program_type_t program_type =
				ia_css_program_manifest_get_type(
						program_manifest);
			/*
			 * An exclusive subnode < exclusive supernode,
			 * so simply don't count it
			 */
			if (program_type !=
				IA_CSS_PROGRAM_TYPE_EXCLUSIVE_SUB &&
				program_type !=
				IA_CSS_PROGRAM_TYPE_VIRTUAL_SUB) {
				process_count++;
			}
		}
	}

EXIT:
	if (NULL == manifest || NULL == param) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_compute_process_count invalid argument\n");
	}
	return process_count;
}

uint8_t ia_css_process_group_compute_terminal_count(
	const ia_css_program_group_manifest_t *manifest,
	const ia_css_program_group_param_t *param)
{
	uint8_t terminal_count = 0;
	ia_css_kernel_bitmap_t total_bitmap, enable_bitmap;
	int i;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_compute_terminal_count(): enter:\n");

	verifexit(manifest != NULL);
	verifexit(param != NULL);

	total_bitmap =
		ia_css_program_group_manifest_get_kernel_bitmap(manifest);
	enable_bitmap =
		ia_css_program_group_param_get_kernel_enable_bitmap(param);

	verifexit(ia_css_is_program_group_manifest_valid(manifest));
	verifexit(ia_css_is_kernel_bitmap_subset(total_bitmap, enable_bitmap));
	verifexit(!ia_css_is_kernel_bitmap_empty(enable_bitmap));

	for (i = 0; i <
		(int)ia_css_program_group_manifest_get_terminal_count(
			manifest); i++) {
		ia_css_terminal_manifest_t *tmanifest =
			ia_css_program_group_manifest_get_terminal_manifest(
					manifest, i);

		if (ia_css_process_group_is_terminal_enabled(
					tmanifest, enable_bitmap)) {
			terminal_count++;
		}
	}

EXIT:
	if (NULL == manifest || NULL == param) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_compute_terminal_count invalid argument\n");
	}
	return terminal_count;
}
#endif /* !defined(__HIVECC) */
