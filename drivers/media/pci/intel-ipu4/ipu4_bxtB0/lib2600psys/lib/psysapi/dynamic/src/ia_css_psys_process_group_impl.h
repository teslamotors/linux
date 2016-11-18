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

#ifndef __IA_CSS_PSYS_PROCESS_GROUP_IMPL_H
#define __IA_CSS_PSYS_PROCESS_GROUP_IMPL_H

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
#include <misc_support.h>

#include "ia_css_psys_dynamic_trace.h"

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
uint16_t ia_css_process_group_get_fragment_limit(
	const ia_css_process_group_t *process_group)
{
	DECLARE_ERRVAL
	uint16_t fragment_limit = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_fragment_limit(): enter:\n");

	verifexitval(process_group != NULL, EFAULT);

	fragment_limit = process_group->fragment_limit;

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_get_fragment_limit invalid argument\n");
	}
	return fragment_limit;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
int ia_css_process_group_set_fragment_limit(
	ia_css_process_group_t *process_group,
	const uint16_t fragment_limit)
{
	DECLARE_ERRVAL
	int retval = -1;
	uint16_t fragment_state;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_set_fragment_limit(): enter:\n");

	verifexitval(process_group != NULL, EFAULT);

	retval = ia_css_process_group_get_fragment_state(process_group,
		&fragment_state);

	verifexitval(retval == 0, EINVAL);
	verifexitval(fragment_limit > fragment_state, EINVAL);
	verifexitval(fragment_limit <= ia_css_process_group_get_fragment_count(
				process_group), EINVAL);

	process_group->fragment_limit = fragment_limit;

	retval = 0;
EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_get_fragment_limit invalid argument process_group\n");
	}
	if (!noerror()) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_get_fragment_limit failed (%i)\n",
			retval);
	}
	return retval;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
int ia_css_process_group_clear_fragment_limit(
	ia_css_process_group_t *process_group)
{
	DECLARE_ERRVAL
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_clear_fragment_limit(): enter:\n");

	verifexitval(process_group != NULL, EFAULT);
	process_group->fragment_limit = 0;

	retval = 0;
EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_clear_fragment_limit invalid argument process_group\n");
	}
	if (!noerror()) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_clear_fragment_limit failed (%i)\n",
			retval);
	}
	return retval;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
int ia_css_process_group_attach_buffer(
	ia_css_process_group_t *process_group,
	vied_vaddress_t buffer,
	const ia_css_buffer_state_t buffer_state,
	const unsigned int terminal_index)
{
	DECLARE_ERRVAL
	int retval = -1;
	ia_css_terminal_t *terminal = NULL;

	NOT_USED(buffer_state);

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, INFO,
		"ia_css_process_group_attach_buffer(): enter:\n");

	verifexitval(process_group != NULL, EFAULT);

	terminal = ia_css_process_group_get_terminal(
				process_group, terminal_index);

	verifexitval(terminal != NULL, EINVAL);
	verifexitval(ia_css_process_group_get_state(process_group) ==
		IA_CSS_PROCESS_GROUP_READY, EINVAL);

	ia_css_terminal_set_buffer(terminal, buffer);
	IA_CSS_TRACE_2(PSYSAPI_DYNAMIC, INFO,
		"\tTerminal %p has buffer 0x%x\n", terminal, buffer);

	if (ia_css_is_terminal_data_terminal(terminal) == true) {
		ia_css_frame_t *frame =
			ia_css_data_terminal_get_frame(
				(ia_css_data_terminal_t *)terminal);
		verifexitval(frame != NULL, EINVAL);

		retval = ia_css_frame_set_buffer_state(frame, buffer_state);
		verifexitval(retval == 0, EINVAL);
	}

	retval = 0;

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_attach_buffer invalid argument process_group\n");
	}
	if (!noerror()) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_attach_buffer failed (%i)\n",
			retval);
	}
	return retval;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
vied_vaddress_t ia_css_process_group_detach_buffer(
	ia_css_process_group_t *process_group,
	const unsigned int terminal_index)
{
	DECLARE_ERRVAL
	int retval = -1;
	vied_vaddress_t buffer = VIED_NULL;

	ia_css_terminal_t *terminal = NULL;
	ia_css_process_group_state_t state;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, INFO,
		"ia_css_process_group_detach_buffer(): enter:\n");

	verifexitval(process_group != NULL, EFAULT);

	terminal =
		ia_css_process_group_get_terminal(
				process_group, terminal_index);
	state = ia_css_process_group_get_state(process_group);

	verifexitval(terminal != NULL, EINVAL);
	verifexitval(state == IA_CSS_PROCESS_GROUP_READY, EINVAL);

	buffer = ia_css_terminal_get_buffer(terminal);

	if (ia_css_is_terminal_data_terminal(terminal) == true) {
		ia_css_frame_t *frame =
			ia_css_data_terminal_get_frame(
					(ia_css_data_terminal_t *)terminal);
		verifexitval(frame != NULL, EINVAL);

		retval = ia_css_frame_set_buffer_state(frame, IA_CSS_BUFFER_NULL);
		verifexitval(retval == 0, EINVAL);
	}
	ia_css_terminal_set_buffer(terminal, VIED_NULL);

	retval = 0;
EXIT:
	/*
	 * buffer pointer will appear on output,
	 * regardless of subsequent fails to avoid memory leaks
	 */
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_detach_buffer invalid argument process_group\n");
	}
	if (!noerror()) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_detach_buffer failed (%i)\n",
			retval);
	}
	return buffer;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
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

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
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

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
int ia_css_process_group_set_barrier(
	ia_css_process_group_t *process_group,
	const vied_nci_barrier_ID_t barrier_index)
{
	DECLARE_ERRVAL
	int retval = -1;
	vied_nci_resource_bitmap_t bit_mask;
	vied_nci_resource_bitmap_t resource_bitmap;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_set_barrier(): enter:\n");

	verifexitval(process_group != NULL, EFAULT);

	resource_bitmap =
		ia_css_process_group_get_resource_bitmap(process_group);

	bit_mask = vied_nci_barrier_bit_mask(barrier_index);

	verifexitval(bit_mask != 0, EINVAL);
	verifexitval(vied_nci_is_bitmap_clear(bit_mask, resource_bitmap), EINVAL);

	resource_bitmap = vied_nci_bitmap_set(resource_bitmap, bit_mask);

	retval =
		ia_css_process_group_set_resource_bitmap(
			process_group, resource_bitmap);
EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_set_barrier invalid argument process_group\n");
	}
	if (!noerror()) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_set_barrier failed (%i)\n",
			retval);
	}
	return retval;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
int ia_css_process_group_clear_barrier(
	ia_css_process_group_t *process_group,
	const vied_nci_barrier_ID_t barrier_index)
{
	DECLARE_ERRVAL
	int retval = -1;
	vied_nci_resource_bitmap_t bit_mask, resource_bitmap;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_clear_barrier(): enter:\n");

	verifexitval(process_group != NULL, EFAULT);

	resource_bitmap =
		ia_css_process_group_get_resource_bitmap(process_group);

	bit_mask = vied_nci_barrier_bit_mask(barrier_index);

	verifexitval(bit_mask != 0, EINVAL);
	verifexitval(vied_nci_is_bitmap_set(bit_mask, resource_bitmap), EINVAL);

	resource_bitmap = vied_nci_bitmap_clear(resource_bitmap, bit_mask);

	retval =
		ia_css_process_group_set_resource_bitmap(
				process_group, resource_bitmap);
EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_clear_barrier invalid argument process_group\n");
	}
	if (!noerror()) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_clear_barrier failed (%i)\n",
			retval);
	}
	return retval;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
int ia_css_process_group_print(
	const ia_css_process_group_t *process_group,
	void *fid)
{
	DECLARE_ERRVAL
	int retval = -1;
	int i;

	uint8_t	process_count;
	uint8_t terminal_count;
	vied_vaddress_t ipu_vaddress = VIED_NULL;

	NOT_USED(fid);

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_print(): enter:\n");

	verifexitval(process_group != NULL, EFAULT);
	retval = ia_css_process_group_get_ipu_vaddress(process_group, &ipu_vaddress);
	verifexitval(retval == 0, EINVAL);

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
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_print invalid argument\n");
	}
	return retval;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
bool ia_css_is_process_group_valid(
	const ia_css_process_group_t *process_group,
	const ia_css_program_group_manifest_t *pg_manifest,
	const ia_css_program_group_param_t *param)
{
	DECLARE_ERRVAL
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

	verifexitval(process_group != NULL, EFAULT);
	verifexitval(pg_manifest != NULL, EFAULT);
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
		verifexitval(NULL != process, EFAULT);
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
		verifexitval(NULL != terminal, EFAULT);
		man_term_idx =
			ia_css_terminal_get_terminal_manifest_index(terminal);
		terminal_manifest =
			ia_css_program_group_manifest_get_terminal_manifest(
					pg_manifest, man_term_idx);
		invalid_flag = invalid_flag ||
			!ia_css_is_terminal_valid(terminal, terminal_manifest);
	}

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_is_process_group_valid() invalid argument\n");
		return false;
	} else {
		return (!invalid_flag);
	}
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
bool ia_css_can_process_group_submit(
	const ia_css_process_group_t *process_group)
{
	DECLARE_ERRVAL
	int i;
	bool can_submit = false;
	int retval = -1;
	uint8_t	terminal_count =
		ia_css_process_group_get_terminal_count(process_group);

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, INFO,
		"ia_css_can_process_group_submit(): enter:\n");

	verifexitval(process_group != NULL, EFAULT);

	for (i = 0; i < (int)terminal_count; i++) {
		ia_css_terminal_t *terminal =
			ia_css_process_group_get_terminal(process_group, i);
		vied_vaddress_t buffer;
		ia_css_buffer_state_t buffer_state;

		verifexitval(terminal != NULL, EINVAL);
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

			verifexitval(frame != NULL, EINVAL);
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
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_can_process_group_submit invalid argument process_group\n");
	}
	if (!noerror()) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_can_process_group_submit failed (%i)\n",
			retval);
	}
	return can_submit;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
bool ia_css_can_process_group_start(
	const ia_css_process_group_t *process_group)
{
	DECLARE_ERRVAL
	int i;
	bool can_start = false;
	int retval = -1;
	uint8_t	terminal_count;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, INFO,
		"ia_css_can_process_group_start(): enter:\n");

	verifexitval(process_group != NULL, EFAULT);

	terminal_count =
		ia_css_process_group_get_terminal_count(process_group);
	for (i = 0; i < (int)terminal_count; i++) {
		ia_css_terminal_t *terminal =
			ia_css_process_group_get_terminal(process_group, i);
		ia_css_buffer_state_t buffer_state;
		bool ok = false;

		verifexitval(terminal != NULL, EINVAL);
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
			verifexitval(frame != NULL, EINVAL);
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
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_can_process_group_submit invalid argument process_group\n");
	}
	if (!noerror()) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_can_process_group_start failed (%i)\n",
			retval);
	}
	return can_start;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
size_t ia_css_process_group_get_size(
	const ia_css_process_group_t *process_group)
{
	DECLARE_ERRVAL
	size_t size = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_size(): enter:\n");

	verifexitval(process_group != NULL, EFAULT);

	size = process_group->size;

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_get_size invalid argument\n");
	}
	return size;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
ia_css_process_group_state_t ia_css_process_group_get_state(
	const ia_css_process_group_t *process_group)
{
	DECLARE_ERRVAL
	ia_css_process_group_state_t state = IA_CSS_N_PROCESS_GROUP_STATES;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_state(): enter:\n");

	verifexitval(process_group != NULL, EFAULT);

	state = process_group->state;

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_get_state invalid argument\n");
	}
	return state;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
uint16_t ia_css_process_group_get_fragment_count(
	const ia_css_process_group_t *process_group)
{
	DECLARE_ERRVAL
	uint16_t fragment_count = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_fragment_count(): enter:\n");

	verifexitval(process_group != NULL, EFAULT);

	fragment_count = process_group->fragment_count;

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_get_fragment_count invalid argument\n");
	}
	return fragment_count;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
uint8_t ia_css_process_group_get_process_count(
	const ia_css_process_group_t *process_group)
{
	DECLARE_ERRVAL
	uint8_t process_count = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_process_count(): enter:\n");

	verifexitval(process_group != NULL, EFAULT);

	process_count = process_group->process_count;

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_get_process_count invalid argument\n");
	}
	return process_count;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
uint8_t ia_css_process_group_get_terminal_count(
	const ia_css_process_group_t *process_group)
{
	DECLARE_ERRVAL
	uint8_t terminal_count = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_terminal_count(): enter:\n");

	verifexitval(process_group != NULL, EFAULT);

	terminal_count = process_group->terminal_count;

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_get_terminal_count invalid argument\n");
	}
	return terminal_count;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
uint32_t ia_css_process_group_get_pg_load_start_ts(
	const ia_css_process_group_t *process_group)
{
	DECLARE_ERRVAL
	uint32_t pg_load_start_ts = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_pg_load_start_ts(): enter:\n");

	verifexitval(process_group != NULL, EFAULT);

	pg_load_start_ts = process_group->pg_load_start_ts;

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_get_pg_load_start_ts invalid argument\n");
	}
	return pg_load_start_ts;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
uint32_t ia_css_process_group_get_pg_load_cycles(
	const ia_css_process_group_t *process_group)
{
	DECLARE_ERRVAL
	uint32_t pg_load_cycles = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_pg_load_cycles(): enter:\n");

	verifexitval(process_group != NULL, EFAULT);

	pg_load_cycles = process_group->pg_load_cycles;

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_get_pg_load_cycles invalid argument\n");
	}
	return pg_load_cycles;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
uint32_t ia_css_process_group_get_pg_init_cycles(
	const ia_css_process_group_t *process_group)
{
	DECLARE_ERRVAL
	uint32_t pg_init_cycles = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_pg_init_cycles(): enter:\n");

	verifexitval(process_group != NULL, EFAULT);

	pg_init_cycles = process_group->pg_init_cycles;

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_get_pg_init_cycles invalid argument\n");
	}
	return pg_init_cycles;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
uint32_t ia_css_process_group_get_pg_processing_cycles(
	const ia_css_process_group_t *process_group)
{
	DECLARE_ERRVAL
	uint32_t pg_processing_cycles = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_pg_processing_cycles(): enter:\n");

	verifexitval(process_group != NULL, EFAULT);

	pg_processing_cycles = process_group->pg_processing_cycles;

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_get_pg_processing_cycles invalid argument\n");
	}
	return pg_processing_cycles;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
ia_css_terminal_t *ia_css_process_group_get_terminal(
	const ia_css_process_group_t *process_grp,
	const unsigned int terminal_num)
{
	DECLARE_ERRVAL
	ia_css_terminal_t *terminal_ptr = NULL;
	uint16_t *terminal_offset_table;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_terminal(): enter:\n");

	verifexitval(process_grp != NULL, EFAULT);
	verifexitval(terminal_num < process_grp->terminal_count, EINVAL);

	terminal_offset_table =
		(uint16_t *)((char *)process_grp +
				process_grp->terminals_offset);
	terminal_ptr =
		(ia_css_terminal_t *)((char *)process_grp +
				terminal_offset_table[terminal_num]);

	verifexitval(terminal_ptr != NULL, EFAULT);

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_get_terminal invalid argument\n");
	}
	return terminal_ptr;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
ia_css_process_t *ia_css_process_group_get_process(
	const ia_css_process_group_t *process_grp,
	const unsigned int process_num)
{
	DECLARE_ERRVAL
	ia_css_process_t *process_ptr = NULL;
	uint16_t *process_offset_table;

	verifexitval(process_grp != NULL, EFAULT);
	verifexitval(process_num < process_grp->process_count, EINVAL);

	process_offset_table =
		(uint16_t *)((char *)process_grp +
				process_grp->processes_offset);
	process_ptr =
		(ia_css_process_t *)((char *)process_grp +
				process_offset_table[process_num]);

	verifexitval(process_ptr != NULL, EFAULT);

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_get_process invalid argument\n");
	}
	return process_ptr;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
ia_css_program_group_ID_t ia_css_process_group_get_program_group_ID(
	const ia_css_process_group_t *process_group)
{
	DECLARE_ERRVAL
	ia_css_program_group_ID_t id = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_program_group_ID(): enter:\n");

	verifexitval(process_group != NULL, EFAULT);

	id = process_group->ID;

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_get_program_group_ID invalid argument\n");
	}
	return id;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
vied_nci_resource_bitmap_t ia_css_process_group_get_resource_bitmap(
	const ia_css_process_group_t *process_group)
{
	DECLARE_ERRVAL
	vied_nci_resource_bitmap_t resource_bitmap = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_resource_bitmap(): enter:\n");

	verifexitval(process_group != NULL, EFAULT);

	resource_bitmap = process_group->resource_bitmap;

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_get_resource_bitmap invalid argument\n");
	}
	return resource_bitmap;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
int ia_css_process_group_set_resource_bitmap(
	ia_css_process_group_t *process_group,
	const vied_nci_resource_bitmap_t resource_bitmap)
{
	DECLARE_ERRVAL
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_set_resource_bitmap(): enter:\n");

	verifexitval(process_group != NULL, EFAULT);

	process_group->resource_bitmap = resource_bitmap;

	retval = 0;
EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_set_resource_bitmap invalid argument process_group\n");
	}
	if (!noerror()) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_set_resource_bitmap failed (%i)\n",
			retval);
	}
	return retval;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
uint32_t ia_css_process_group_compute_cycle_count(
	const ia_css_program_group_manifest_t *manifest,
	const ia_css_program_group_param_t *param)
{
	DECLARE_ERRVAL
	uint32_t cycle_count = 0;

	NOT_USED(manifest);
	NOT_USED(param);

	verifexitval(manifest != NULL, EFAULT);
	verifexitval(param != NULL, EFAULT);

	cycle_count = 1;

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_compute_cycle_count invalid argument\n");
	}
	return cycle_count;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
int ia_css_process_group_set_fragment_state(
	ia_css_process_group_t *process_group,
	uint16_t fragment_state)
{
	DECLARE_ERRVAL
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, INFO,
		"ia_css_process_group_set_fragment_state(): enter:\n");

	verifexitval(process_group != NULL, EFAULT);
	verifexitval(fragment_state <= ia_css_process_group_get_fragment_count(
				process_group), EINVAL);

	process_group->fragment_state = fragment_state;
	retval = 0;
EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_set_fragment_state invalid argument process_group\n");
	}
	if (!noerror()) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_set_fragment_state failed (%i)\n",
			retval);
	}
	return retval;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
int ia_css_process_group_get_fragment_state(
	const ia_css_process_group_t *process_group,
	uint16_t *fragment_state)
{
	DECLARE_ERRVAL
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_fragment_state(): enter:\n");

	verifexitval(process_group != NULL, EFAULT);
	verifexitval(fragment_state != NULL, EFAULT);

	*fragment_state = process_group->fragment_state;
	retval = 0;

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_get_fragment_state invalid argument\n");
	}
	if (!noerror()) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_get_fragment_state failed (%i)\n",
			retval);
	}
	return retval;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
int ia_css_process_group_get_ipu_vaddress(
	const ia_css_process_group_t *process_group,
	vied_vaddress_t *ipu_vaddress)
{
	DECLARE_ERRVAL
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_get_ipu_vaddress(): enter:\n");

	verifexitval(process_group != NULL, EFAULT);
	verifexitval(ipu_vaddress != NULL, EFAULT);

	*ipu_vaddress = process_group->ipu_virtual_address;
	retval = 0;

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_get_ipu_vaddress invalid argument\n");
	}
	if (!noerror()) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_get_ipu_vaddress failed (%i)\n",
			retval);
	}
	return retval;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
int ia_css_process_group_set_ipu_vaddress(
	ia_css_process_group_t *process_group,
	vied_vaddress_t ipu_vaddress)
{
	DECLARE_ERRVAL
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_process_group_set_ipu_vaddress(): enter:\n");

	verifexitval(process_group != NULL, EFAULT);

	process_group->ipu_virtual_address = ipu_vaddress;
	retval = 0;

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_set_ipu_vaddress invalid argument\n");
	}
	if (!noerror()) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_process_group_set_ipu_vaddress failed (%i)\n",
			retval);
	}
	return retval;
}

#endif /* __IA_CSS_PSYS_PROCESS_GROUP_IMPL_H */
