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


#include <ia_css_psys_process_group.h>
#include "ia_css_psys_process_group_cmd_impl.h"
#include <ia_css_psys_terminal.h>
#include <ia_css_psys_process.h>
#include <ia_css_psys_terminal_manifest.h>
#include <ia_css_psys_program_manifest.h>
#include <ia_css_psys_program_group_manifest.h>
#include "ia_css_terminal_manifest_types.h"

#include <ia_css_kernel_bitmap.h>	/* ia_css_kernel_bitmap_t */

#include <vied_nci_psys_system_global.h>
#include <ia_css_program_group_data.h>
#include <type_support.h>
#include <error_support.h>
#include <assert_support.h>
#include <misc_support.h>

/* This header is need for cpu memset to 0
* and process groups are not created in SP
*/
#if !defined(__HIVECC)
#include "cpu_mem_support.h"
#endif

#include "ia_css_psys_dynamic_trace.h"

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

				verifexit(super_program_manifest != NULL,
						EINVAL);
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
					verifexit(0, EINVAL);
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

	verifexit(manifest != NULL, EINVAL);
	verifexit(param != NULL, EINVAL);

	COMPILATION_ERROR_IF(
		SIZE_OF_PROCESS_GROUP_STRUCT_BITS !=
			(CHAR_BIT * sizeof(ia_css_process_group_t)));

	COMPILATION_ERROR_IF(0 !=
			sizeof(ia_css_process_group_t) % sizeof(uint64_t));

	process_count =
		ia_css_process_group_compute_process_count(manifest, param);
	terminal_count =
		ia_css_process_group_compute_terminal_count(manifest, param);

	verifexit(process_count != 0, EINVAL);
	verifexit(terminal_count != 0, EINVAL);

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
			verifexit(process_num < process_count, EINVAL);
			size += ia_css_sizeof_process(
					program_manifest, program_param);
			process_num++;
		}
	}

	verifexit(process_num == process_count, EINVAL);

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

	verifexit(process_grp_mem != NULL, EINVAL);
	verifexit(manifest != NULL, EINVAL);
	verifexit(param != NULL, EINVAL);
	verifexit(ia_css_is_program_group_manifest_valid(manifest), EINVAL);

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
				process_group, fragment_count) == 0, EINVAL);

	/* Set process group terminal dependency list */
	/* This list is used during creating the process dependency list */
	uint8_t manifest_terminal_count =
		ia_css_program_group_manifest_get_terminal_count(manifest);

	terminal_num = 0;
	for (i = 0; i < (int)manifest_terminal_count; i++) {
		ia_css_terminal_manifest_t *t_manifest =
			ia_css_program_group_manifest_get_terminal_manifest(
					manifest, i);

		verifexit(NULL != t_manifest, EINVAL);
		if (ia_css_process_group_is_terminal_enabled(
					t_manifest, enable_bitmap)) {
			ia_css_terminal_t *terminal = NULL;
			ia_css_terminal_param_t *terminal_param =
				ia_css_program_group_param_get_terminal_param(
						param, i);

			verifexit(NULL != terminal_param, EINVAL);
			terminal_tab_ptr[terminal_num] =
				(uint16_t)(process_grp_raw_ptr -
						(char *)process_group);
			terminal = ia_css_terminal_create(
					process_grp_raw_ptr, t_manifest,
					terminal_param, enable_bitmap);
			verifexit(terminal != NULL, ENOBUFS);
			verifexit((ia_css_terminal_set_parent(
					terminal, process_group) == 0),
					EINVAL);
			verifexit((ia_css_terminal_set_terminal_manifest_index(
					terminal, i) == 0), EINVAL);
			IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, INFO,
				"ia_css_process_group_create: terminal_manifest_index %d\n",
				i);

			process_grp_raw_ptr += ia_css_terminal_get_size(
							terminal);
			terminal_num++;
		}
	}
	verifexit(terminal_num == terminal_count, EINVAL);

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

			verifexit(process_num < process_count, EINVAL);

			process_tab_ptr[process_num] =
				(uint16_t)(process_grp_raw_ptr -
						(char *)process_group);
			process = ia_css_process_create(
					process_grp_raw_ptr,
					program_manifest,
					program_param);
			verifexit(process != NULL, ENOBUFS);

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

				verifexit(id != 0, EINVAL);
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

				verifexit(pm_term_index <
						manifest_terminal_count,
						EINVAL);
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
	verifexit(process_num == process_count, EINVAL);

	process_group->size =
		(uint32_t)ia_css_sizeof_process_group(manifest, param);
	process_group->ID =
		ia_css_program_group_manifest_get_program_group_ID(manifest);

	/* Initialize performance measurement fields to zero */
	process_group->pg_load_start_ts     = 0;
	process_group->pg_load_cycles       = 0;
	process_group->pg_init_cycles       = 0;
	process_group->pg_processing_cycles = 0;

	verifexit(process_group->ID != 0, EINVAL);

	ret = ia_css_process_group_on_create(process_group, manifest, param);
	verifexit(ret == 0, EINVAL);

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

	verifexit(process_group != NULL, EINVAL);

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

	verifexit(process_group != NULL, EINVAL);
	verifexit(token != 0, EINVAL);

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

	verifexit(process_group != NULL, EINVAL);

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

	verifexit(process_group != NULL, EINVAL);
	verifexit(token != 0, EINVAL);

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
#endif

uint16_t ia_css_process_group_get_fragment_limit(
	const ia_css_process_group_t *process_group)
{
	uint16_t fragment_limit = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_fragment_limit(): enter:\n");

	verifexit(process_group != NULL, EINVAL);

	fragment_limit = process_group->fragment_limit;

EXIT:
	if (NULL == process_group) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_get_fragment_limit invalid argument\n");
	}
	return fragment_limit;
}

int ia_css_process_group_set_fragment_limit(
	ia_css_process_group_t *process_group,
	const uint16_t fragment_limit)
{
	int retval = -1;
	uint16_t fragment_state;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_set_fragment_limit(): enter:\n");

	verifexit(process_group != NULL, EINVAL);

	retval = ia_css_process_group_get_fragment_state(process_group,
		&fragment_state);

	verifexit(retval == 0, EINVAL);
	verifexit(fragment_limit > fragment_state, EINVAL);
	verifexit(fragment_limit <= ia_css_process_group_get_fragment_count(
				process_group), EINVAL);

	process_group->fragment_limit = fragment_limit;

	retval = 0;
EXIT:
	if (NULL == process_group) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_get_fragment_limit invalid argument process_group\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_get_fragment_limit failed (%i)\n",
			retval);
	}
	return retval;
}

int ia_css_process_group_clear_fragment_limit(
	ia_css_process_group_t *process_group)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_clear_fragment_limit(): enter:\n");

	verifexit(process_group != NULL, EINVAL);
	process_group->fragment_limit = 0;

	retval = 0;
EXIT:
	if (NULL == process_group) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_clear_fragment_limit invalid argument process_group\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_clear_fragment_limit failed (%i)\n",
			retval);
	}
	return retval;
}
int ia_css_process_group_attach_buffer(
	ia_css_process_group_t *process_group,
	vied_vaddress_t buffer,
	const ia_css_buffer_state_t buffer_state,
	const unsigned int terminal_index)
{
	int retval = -1;
	ia_css_terminal_t *terminal = NULL;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, INFO,
		"ia_css_process_group_attach_buffer(): enter:\n");

	/* We should have a check that NULL != process_group but it
	 * process_group is NULL, ia_css_process_group_get_state will return
	 * IA_CSS_N_PROCESS_GROUP_STATES so it will be filtered anyway later.
	*/

	/* verifexit(process_group != NULL, EINVAL); */

	terminal = ia_css_process_group_get_terminal(
				process_group, terminal_index);

	verifexit(terminal != NULL, EINVAL);
	verifexit(ia_css_process_group_get_state(process_group) ==
		IA_CSS_PROCESS_GROUP_READY, EINVAL);

	ia_css_terminal_set_buffer(terminal, buffer);
	IA_CSS_TRACE_2(PSYSAPI_DYNAMIC, INFO,
		"\tTerminal %p has buffer 0x%x\n", terminal, buffer);

	if (ia_css_is_terminal_data_terminal(terminal) == true) {
		ia_css_frame_t *frame =
			ia_css_data_terminal_get_frame(
				(ia_css_data_terminal_t *)terminal);

		verifexit(frame != NULL, EINVAL);
		verifexit(ia_css_frame_set_buffer_state(
					frame, buffer_state) == 0, EINVAL);
	}

	retval = 0;

EXIT:
	if (NULL == process_group) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_attach_buffer invalid argument process_group\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_attach_buffer failed (%i)\n",
			retval);
	}
	return retval;
}

vied_vaddress_t ia_css_process_group_detach_buffer(
	ia_css_process_group_t *process_group,
	const unsigned int terminal_index)
{
	int retval = -1;
	vied_vaddress_t buffer = VIED_NULL;

	ia_css_terminal_t *terminal = NULL;
	ia_css_process_group_state_t state;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, INFO,
		"ia_css_process_group_detach_buffer(): enter:\n");

	/* We should have a check that NULL != process_group but it
	* process_group is NULL, ia_css_process_group_get_state will return
	* IA_CSS_N_PROCESS_GROUP_STATES so it will be filtered anyway later.
	*/

	/* verifexit(process_group != NULL, EINVAL); */

	terminal =
		ia_css_process_group_get_terminal(
				process_group, terminal_index);
	state = ia_css_process_group_get_state(process_group);

	verifexit(terminal != NULL, EINVAL);
	verifexit(state == IA_CSS_PROCESS_GROUP_READY, EINVAL);

	buffer = ia_css_terminal_get_buffer(terminal);

	if (ia_css_is_terminal_data_terminal(terminal) == true) {
		ia_css_frame_t *frame =
			ia_css_data_terminal_get_frame(
					(ia_css_data_terminal_t *)terminal);

		verifexit(frame != NULL, EINVAL);
		verifexit(ia_css_frame_set_buffer_state(
				frame, IA_CSS_BUFFER_NULL) == 0, EINVAL);
	}
	ia_css_terminal_set_buffer(terminal, VIED_NULL);

	retval = 0;
EXIT:
	/*
	 * buffer pointer will appear on output,
	 * regardless of subsequent fails to avoid memory leaks
	 */
	if (NULL == process_group) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_detach_buffer invalid argument process_group\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_detach_buffer failed (%i)\n",
			retval);
	}
	return buffer;
}

int ia_css_process_group_attach_stream(
	ia_css_process_group_t *process_group,
	uint32_t stream,
	const ia_css_buffer_state_t buffer_state,
	const unsigned int terminal_index)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, INFO,
		"ia_css_process_group_attach_stream(): enter:\n");

	NOT_USED(process_group);
	NOT_USED(stream);
	NOT_USED(buffer_state);
	NOT_USED(terminal_index);

	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_attach_stream failed (%i)\n",
			retval);
	}
	return retval;
}

uint32_t ia_css_process_group_detach_stream(
	ia_css_process_group_t *process_group,
	const unsigned int terminal_index)
{
	int retval = -1;
	uint32_t stream = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, INFO,
		"ia_css_process_group_detach_stream(): enter:\n");

	NOT_USED(process_group);
	NOT_USED(terminal_index);

	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_detach_stream failed (%i)\n",
			retval);
	}
	return stream;
}

int ia_css_process_group_set_barrier(
	ia_css_process_group_t *process_group,
	const vied_nci_barrier_ID_t barrier_index)
{
	int retval = -1;
	vied_nci_resource_bitmap_t bit_mask;
	vied_nci_resource_bitmap_t resource_bitmap;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_set_barrier(): enter:\n");

	verifexit(process_group != NULL, EINVAL);

	resource_bitmap =
		ia_css_process_group_get_resource_bitmap(process_group);

	bit_mask = vied_nci_barrier_bit_mask(barrier_index);

	verifexit(bit_mask != 0, EINVAL);
	verifexit(vied_nci_is_bitmap_clear(bit_mask, resource_bitmap), EINVAL);

	resource_bitmap = vied_nci_bitmap_set(resource_bitmap, bit_mask);

	retval =
		ia_css_process_group_set_resource_bitmap(
			process_group, resource_bitmap);
EXIT:
	if (NULL == process_group) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_set_barrier invalid argument process_group\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_set_barrier failed (%i)\n",
			retval);
	}
	return retval;
}

int ia_css_process_group_clear_barrier(
	ia_css_process_group_t *process_group,
	const vied_nci_barrier_ID_t barrier_index)
{
	int retval = -1;
	vied_nci_resource_bitmap_t bit_mask, resource_bitmap;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_clear_barrier(): enter:\n");

	verifexit(process_group != NULL, EINVAL);

	resource_bitmap =
		ia_css_process_group_get_resource_bitmap(process_group);

	bit_mask = vied_nci_barrier_bit_mask(barrier_index);

	verifexit(bit_mask != 0, EINVAL);
	verifexit(vied_nci_is_bitmap_set(bit_mask, resource_bitmap), EINVAL);

	resource_bitmap = vied_nci_bitmap_clear(resource_bitmap, bit_mask);

	retval =
		ia_css_process_group_set_resource_bitmap(
				process_group, resource_bitmap);
EXIT:
	if (NULL == process_group) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_clear_barrier invalid argument process_group\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_clear_barrier failed (%i)\n",
			retval);
	}
	return retval;
}

int ia_css_process_group_print(
	const ia_css_process_group_t *process_group,
	void *fid)
{
	int retval = -1;
	int i;

	uint8_t	process_count;
	uint8_t terminal_count;
	vied_vaddress_t ipu_vaddress;

	NOT_USED(fid);

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_print(): enter:\n");

	verifexit(process_group != NULL, EINVAL);
	verifexit(ia_css_process_group_get_ipu_vaddress(
			process_group, &ipu_vaddress) == 0, EINVAL);

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, INFO,
		"=============== Process group print start ===============\n");
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, INFO,
		"\tprocess_group cpu address = %p\n", process_group);
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, INFO,
		"\tipu_virtual_address = %#x\n", ipu_vaddress);
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, INFO,
		"\tsizeof(process_group) = %d\n",
		(int)ia_css_process_group_get_size(process_group));
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, INFO,
		"\tfragment_count = %d\n",
		(int)ia_css_process_group_get_fragment_count(process_group));
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, INFO,
		"\tprogram_group(process_group) = %d\n",
		(int)ia_css_process_group_get_program_group_ID(process_group));

	process_count = ia_css_process_group_get_process_count(process_group);
	terminal_count =
		ia_css_process_group_get_terminal_count(process_group);

	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, INFO,
		"\t%d processes\n", (int)process_count);
	for (i = 0; i < (int)process_count; i++) {
		ia_css_process_t *process =
			ia_css_process_group_get_process(process_group, i);

		retval = ia_css_process_print(process, fid);
		verifjmpexit(retval == 0);
	}
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, INFO,
		"\t%d terminals\n", (int)terminal_count);
	for (i = 0; i < (int)terminal_count; i++) {
		ia_css_terminal_t *terminal =
			ia_css_process_group_get_terminal(process_group, i);

		retval = ia_css_terminal_print(terminal, fid);
		verifjmpexit(retval == 0);
	}
	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, INFO,
		"=============== Process group print end ===============\n");
	retval = 0;
EXIT:
	if (NULL == process_group) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_print invalid argument\n");
	}
	return retval;
}

bool ia_css_is_process_group_valid(
	const ia_css_process_group_t *process_group,
	const ia_css_program_group_manifest_t *pg_manifest,
	const ia_css_program_group_param_t *param)
{
	bool invalid_flag = false;
	uint8_t proc_idx;
	uint8_t prog_idx;
	uint8_t proc_term_idx;
	uint8_t	process_count;
	uint8_t	program_count;
	uint8_t terminal_count;
	uint8_t man_terminal_count;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_is_process_group_valid(): enter:\n");

	verifexit(process_group != NULL, EINVAL);
	verifexit(pg_manifest != NULL, EINVAL);
	NOT_USED(param);

	process_count = process_group->process_count;
	terminal_count = process_group->terminal_count;
	program_count =
		ia_css_program_group_manifest_get_program_count(pg_manifest);
	man_terminal_count =
		ia_css_program_group_manifest_get_terminal_count(pg_manifest);

	/* Validate process group */
	invalid_flag = invalid_flag ||
		!(program_count >= process_count) ||
		!(man_terminal_count >= terminal_count) ||
		!(process_group->size > process_group->processes_offset) ||
		!(process_group->size > process_group->terminals_offset);
	/* Validate processes */
	for (proc_idx = 0; proc_idx < process_count; proc_idx++) {
		const ia_css_process_t *process;
		ia_css_program_ID_t prog_id;
		bool no_match_found = true;

		process = ia_css_process_group_get_process(
					process_group, proc_idx);
		verifjmpexit(NULL != process);
		prog_id = ia_css_process_get_program_ID(process);
		for (prog_idx = 0; prog_idx < program_count; prog_idx++) {
			ia_css_program_manifest_t *p_manifest = NULL;

			p_manifest =
			ia_css_program_group_manifest_get_program_manifest(
						pg_manifest, prog_idx);
			if (prog_id ==
				ia_css_program_manifest_get_program_ID(
					p_manifest)) {
				invalid_flag = invalid_flag ||
					!ia_css_is_process_valid(
						process, p_manifest);
				no_match_found = false;
				break;
			}
		}
		invalid_flag = invalid_flag || no_match_found;
	}
	/* Validate terminals */
	for (proc_term_idx = 0; proc_term_idx < terminal_count;
			proc_term_idx++) {
		int man_term_idx;
		const ia_css_terminal_t *terminal;
		const ia_css_terminal_manifest_t *terminal_manifest;

		terminal =
			ia_css_process_group_get_terminal(
					process_group, proc_term_idx);
		verifjmpexit(NULL != terminal);
		man_term_idx =
			ia_css_terminal_get_terminal_manifest_index(terminal);
		terminal_manifest =
			ia_css_program_group_manifest_get_terminal_manifest(
					pg_manifest, man_term_idx);
		invalid_flag = invalid_flag ||
			!ia_css_is_terminal_valid(terminal, terminal_manifest);
	}
	return (!invalid_flag);
EXIT:
	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
		"ia_css_is_process_group_valid() invalid argument\n");
	return false;
}

bool ia_css_can_process_group_submit(
	const ia_css_process_group_t *process_group)
{
	int i;
	bool can_submit = false;
	int retval = -1;
	uint8_t	terminal_count =
		ia_css_process_group_get_terminal_count(process_group);

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, INFO,
		"ia_css_can_process_group_submit(): enter:\n");

	verifexit(process_group != NULL, EINVAL);

	for (i = 0; i < (int)terminal_count; i++) {
		ia_css_terminal_t *terminal =
			ia_css_process_group_get_terminal(process_group, i);
		vied_vaddress_t buffer;
		ia_css_buffer_state_t buffer_state;

		verifexit(terminal != NULL, EINVAL);
		buffer = ia_css_terminal_get_buffer(terminal);
		IA_CSS_TRACE_3(PSYSAPI_DYNAMIC, INFO,
			"\tH: Terminal number(%d) is %p having buffer 0x%x\n",
			i, terminal, buffer);
		/* FAS allows for attaching NULL buffers to satisfy SDF,
		* but only if l-Scheduler is embedded
		*/
		if (buffer == VIED_NULL)
			break;

		/* buffer_state is applicable only for data terminals*/
		if (ia_css_is_terminal_data_terminal(terminal) == true) {
			ia_css_frame_t *frame =
				ia_css_data_terminal_get_frame(
					(ia_css_data_terminal_t *)terminal);

			verifexit(frame != NULL, EINVAL);
			buffer_state = ia_css_frame_get_buffer_state(frame);
			if ((buffer_state == IA_CSS_BUFFER_NULL) ||
				(buffer_state == IA_CSS_N_BUFFER_STATES)) {
				break;
			}
		} else if (
			(ia_css_is_terminal_parameter_terminal(terminal)
				!= true) &&
			(ia_css_is_terminal_program_terminal(terminal)
				!= true) &&
			(ia_css_is_terminal_spatial_parameter_terminal(
				terminal) != true)) {
			/* neither data nor parameter terminal, so error.*/
			break;
		}

	}
	/* Only true if no check failed */
	can_submit = (i == terminal_count);

	retval = 0;
EXIT:
	if (NULL == process_group) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_can_process_group_submit invalid argument process_group\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_can_process_group_submit failed (%i)\n",
			retval);
	}
	return can_submit;
}

bool ia_css_can_process_group_start(
	const ia_css_process_group_t *process_group)
{
	int i;
	bool can_start = false;
	int retval = -1;
	uint8_t	terminal_count;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, INFO,
		"ia_css_can_process_group_start(): enter:\n");

	verifexit(process_group != NULL, EINVAL);

	terminal_count =
		ia_css_process_group_get_terminal_count(process_group);
	for (i = 0; i < (int)terminal_count; i++) {
		ia_css_terminal_t *terminal =
			ia_css_process_group_get_terminal(process_group, i);
		ia_css_buffer_state_t buffer_state;
		bool ok = false;

		verifexit(terminal != NULL, EINVAL);
		if (ia_css_is_terminal_data_terminal(terminal) == true) {
			/*
			 * buffer_state is applicable only for data terminals
			 */
			ia_css_frame_t *frame =
				ia_css_data_terminal_get_frame(
					(ia_css_data_terminal_t *)terminal);
			bool is_input = ia_css_is_terminal_input(terminal);
			/*
			 * check for NULL here.
			 * then invoke next 2 statements
			 */
			verifexit(frame != NULL, EINVAL);
			IA_CSS_TRACE_5(PSYSAPI_DYNAMIC, VERBOSE,
				"\tTerminal %d: buffer_state %u, access_type %u, data_bytes %u, data %u\n",
				i, frame->buffer_state, frame->access_type,
				frame->data_bytes, frame->data);
			buffer_state = ia_css_frame_get_buffer_state(frame);

			ok = ((is_input &&
				(buffer_state == IA_CSS_BUFFER_FULL)) ||
					(!is_input && (buffer_state ==
							IA_CSS_BUFFER_EMPTY)));

		} else if (ia_css_is_terminal_parameter_terminal(terminal) ==
				true) {
			/*
			 * FIXME:
			 * is there any pre-requisite for param_terminal?
			 */
			ok = true;
		} else if (ia_css_is_terminal_program_terminal(terminal) ==
				true) {
			ok = true;
		} else if (ia_css_is_terminal_spatial_parameter_terminal(
					terminal) == true) {
			ok = true;
		} else {
			/* neither data nor parameter terminal, so error.*/
			break;
		}

		if (!ok)
			break;
	}
	/* Only true if no check failed */
	can_start = (i == terminal_count);

	retval = 0;
EXIT:
	if (NULL == process_group) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_can_process_group_submit invalid argument process_group\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_can_process_group_start failed (%i)\n",
			retval);
	}
	return can_start;
}

size_t ia_css_process_group_get_size(
	const ia_css_process_group_t *process_group)
{
	size_t size = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_size(): enter:\n");

	if (process_group != NULL) {
		size = process_group->size;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_get_size invalid argument\n");
	}

	return size;
}

ia_css_process_group_state_t ia_css_process_group_get_state(
	const ia_css_process_group_t *process_group)
{
	ia_css_process_group_state_t state = IA_CSS_N_PROCESS_GROUP_STATES;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_state(): enter:\n");

	if (process_group != NULL) {
		state = process_group->state;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_state_t invalid argument\n");
	}

	return state;
}

uint16_t ia_css_process_group_get_fragment_count(
	const ia_css_process_group_t *process_group)
{
	uint16_t fragment_count = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_fragment_count(): enter:\n");

	if (process_group != NULL) {
		fragment_count = process_group->fragment_count;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_get_fragment_count invalid argument\n");
	}

	return fragment_count;
}

uint8_t ia_css_process_group_get_process_count(
	const ia_css_process_group_t *process_group)
{
	uint8_t process_count = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_process_count(): enter:\n");

	if (process_group != NULL) {
		process_count = process_group->process_count;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_get_process_count invalid argument\n");
	}

	return process_count;
}

uint8_t ia_css_process_group_get_terminal_count(
	const ia_css_process_group_t *process_group)
{
	uint8_t terminal_count = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_terminal_count(): enter:\n");

	if (process_group != NULL) {
		terminal_count = process_group->terminal_count;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_get_terminal_count invalid argument\n");
	}

	return terminal_count;
}

uint32_t ia_css_process_group_get_pg_load_start_ts(
	const ia_css_process_group_t *process_group)
{
	uint32_t pg_load_start_ts = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_pg_load_start_ts(): enter:\n");

	if (process_group != NULL) {
		pg_load_start_ts = process_group->pg_load_start_ts;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_get_pg_load_start_ts invalid argument\n");
	}

	return pg_load_start_ts;
}

uint32_t ia_css_process_group_get_pg_load_cycles(
	const ia_css_process_group_t *process_group)
{
	uint32_t pg_load_cycles = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_pg_load_cycles(): enter:\n");

	if (process_group != NULL) {
		pg_load_cycles = process_group->pg_load_cycles;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_get_pg_load_cycles invalid argument\n");
	}

	return pg_load_cycles;
}

uint32_t ia_css_process_group_get_pg_init_cycles(
	const ia_css_process_group_t *process_group)
{
	uint32_t pg_init_cycles = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_pg_init_cycles(): enter:\n");

	if (process_group != NULL) {
		pg_init_cycles = process_group->pg_init_cycles;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_get_pg_init_cycles invalid argument\n");
	}

	return pg_init_cycles;
}

uint32_t ia_css_process_group_get_pg_processing_cycles(
	const ia_css_process_group_t *process_group)
{
	uint32_t pg_processing_cycles = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_pg_processing_cycles(): enter:\n");

	if (process_group != NULL) {
		pg_processing_cycles = process_group->pg_processing_cycles;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_get_pg_processing_cycles invalid argument\n");
	}

	return pg_processing_cycles;
}

ia_css_terminal_t *ia_css_process_group_get_terminal(
	const ia_css_process_group_t *process_grp,
	const unsigned int terminal_num)
{
	ia_css_terminal_t *terminal_ptr = NULL;
	uint16_t *terminal_offset_table;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_terminal(): enter:\n");

	verifexit(process_grp != NULL, NULL);
	verifexit(terminal_num < process_grp->terminal_count, NULL);

	terminal_offset_table =
		(uint16_t *)((char *)process_grp +
				process_grp->terminals_offset);
	terminal_ptr =
		(ia_css_terminal_t *)((char *)process_grp +
				terminal_offset_table[terminal_num]);

EXIT:
	if (NULL == process_grp || NULL == terminal_ptr) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_get_terminal invalid argument\n");
	}
	return terminal_ptr;
}

ia_css_process_t *ia_css_process_group_get_process(
	const ia_css_process_group_t *process_grp,
	const unsigned int process_num)
{
	ia_css_process_t *process_ptr = NULL;
	uint16_t *process_offset_table;

	verifexit(process_grp != NULL, NULL);
	verifexit(process_num < process_grp->process_count, NULL);

	process_offset_table =
		(uint16_t *)((char *)process_grp +
				process_grp->processes_offset);
	process_ptr =
		(ia_css_process_t *)((char *)process_grp +
				process_offset_table[process_num]);

EXIT:
	if (NULL == process_grp || NULL == process_ptr) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_get_process invalid argument\n");
	}
	return process_ptr;
}

ia_css_program_group_ID_t ia_css_process_group_get_program_group_ID(
	const ia_css_process_group_t *process_group)
{
	ia_css_program_group_ID_t id = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_program_group_ID(): enter:\n");

	verifexit(process_group != NULL, EINVAL);

	id = process_group->ID;

EXIT:
	if (NULL == process_group) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_get_program_group_ID invalid argument\n");
	}
	return id;
}

vied_nci_resource_bitmap_t ia_css_process_group_get_resource_bitmap(
	const ia_css_process_group_t *process_group)
{
	vied_nci_resource_bitmap_t resource_bitmap = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_resource_bitmap(): enter:\n");

	verifexit(process_group != NULL, EINVAL);

	resource_bitmap = process_group->resource_bitmap;

EXIT:
	if (NULL == process_group) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_get_resource_bitmap invalid argument\n");
	}
	return resource_bitmap;
}

int ia_css_process_group_set_resource_bitmap(
	ia_css_process_group_t *process_group,
	const vied_nci_resource_bitmap_t resource_bitmap)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_set_resource_bitmap(): enter:\n");

	verifexit(process_group != NULL, EINVAL);

	process_group->resource_bitmap = resource_bitmap;

	retval = 0;
EXIT:
	if (NULL == process_group) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_set_resource_bitmap invalid argument process_group\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_set_resource_bitmap failed (%i)\n",
			retval);
	}
	return retval;
}

uint32_t ia_css_process_group_compute_cycle_count(
	const ia_css_program_group_manifest_t *manifest,
	const ia_css_program_group_param_t *param)
{
	uint32_t cycle_count = 0;

	verifexit(manifest != NULL, EINVAL);
	verifexit(param != NULL, EINVAL);

	cycle_count = 1;

EXIT:
	if (NULL == manifest || NULL == param) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_compute_cycle_count invalid argument\n");
	}
	return cycle_count;
}

/* This source file is created with the intention of sharing and
 * compiled for host and firmware. Since there is no native 64bit
 * data type support for firmware this wouldn't compile for SP
 * tile. The part of the file that is not compilable are marked
 * with the following __HIVECC marker and this comment. Once we
 * come up with a solution to address this issue this will be
 * removed.
 */
#if !defined(__HIVECC)
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

	verifexit(manifest != NULL, EINVAL);
	verifexit(param != NULL, EINVAL);

	total_bitmap =
		ia_css_program_group_manifest_get_kernel_bitmap(manifest);
	enable_bitmap =
		ia_css_program_group_param_get_kernel_enable_bitmap(param);

	verifexit(ia_css_is_program_group_manifest_valid(manifest), EINVAL);
	verifexit(ia_css_is_kernel_bitmap_subset(total_bitmap, enable_bitmap),
								EINVAL);
	verifexit(!ia_css_is_kernel_bitmap_empty(enable_bitmap), EINVAL);

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

	verifexit(manifest != NULL, EINVAL);
	verifexit(param != NULL, EINVAL);

	total_bitmap =
		ia_css_program_group_manifest_get_kernel_bitmap(manifest);
	enable_bitmap =
		ia_css_program_group_param_get_kernel_enable_bitmap(param);

	verifexit(ia_css_is_program_group_manifest_valid(manifest), EINVAL);
	verifexit(ia_css_is_kernel_bitmap_subset(total_bitmap, enable_bitmap),
								EINVAL);
	verifexit(!ia_css_is_kernel_bitmap_empty(enable_bitmap), EINVAL);

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
#endif

int ia_css_process_group_set_fragment_state(
	ia_css_process_group_t *process_group,
	uint16_t fragment_state)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, INFO,
		"ia_css_process_group_set_fragment_state(): enter:\n");

	verifexit(process_group != NULL, EINVAL);
	verifexit(fragment_state <= ia_css_process_group_get_fragment_count(
				process_group), EINVAL);

	process_group->fragment_state = fragment_state;
	retval = 0;
EXIT:
	if (NULL == process_group) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_set_fragment_state invalid argument process_group\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_set_fragment_state failed (%i)\n",
			retval);
	}
	return retval;
}

int ia_css_process_group_get_fragment_state(
	const ia_css_process_group_t *process_group,
	uint16_t *fragment_state)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_fragment_state(): enter:\n");

	verifexit(process_group != NULL, EINVAL);
	verifexit(fragment_state != NULL, EINVAL);

	*fragment_state = process_group->fragment_state;
	retval = 0;
EXIT:
	if (NULL == process_group || NULL == fragment_state) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_get_fragment_state invalid argument\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_get_fragment_state failed (%i)\n",
			retval);
	}
	return retval;
}

int ia_css_process_group_get_ipu_vaddress(
	const ia_css_process_group_t *process_group,
	vied_vaddress_t *ipu_vaddress)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_ipu_vaddress(): enter:\n");

	verifexit(process_group != NULL, EINVAL);
	verifexit(ipu_vaddress != NULL, EINVAL);

	*ipu_vaddress = process_group->ipu_virtual_address;
	retval = 0;

EXIT:
	if (NULL == process_group || NULL == ipu_vaddress) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_get_ipu_vaddress invalid argument\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_get_ipu_vaddress failed (%i)\n",
			retval);
	}
	return retval;
}

int ia_css_process_group_set_ipu_vaddress(
	ia_css_process_group_t *process_group,
	vied_vaddress_t ipu_vaddress)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_set_ipu_vaddress(): enter:\n");

	verifexit(process_group != NULL, EINVAL);

	process_group->ipu_virtual_address = ipu_vaddress;
	retval = 0;

EXIT:
	if (NULL == process_group) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_process_group_set_ipu_vaddress invalid argument\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_set_ipu_vaddress failed (%i)\n",
			retval);
	}
	return retval;
}
