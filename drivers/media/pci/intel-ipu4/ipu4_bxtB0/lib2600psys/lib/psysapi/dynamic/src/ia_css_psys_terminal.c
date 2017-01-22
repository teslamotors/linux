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

#include "ia_css_psys_dynamic_storage_class.h"
#include "ia_css_psys_terminal_private_types.h"
#include "ia_css_terminal_types.h"

/*
 * Functions to possibly inline
 */

#ifndef __IA_CSS_PSYS_DYNAMIC_INLINE__
#include "ia_css_psys_terminal_impl.h"
#endif /* __IA_CSS_PSYS_DYNAMIC_INLINE__ */

STORAGE_CLASS_INLINE void __terminal_dummy_check_alignment(void)
{
	COMPILATION_ERROR_IF(
		SIZE_OF_PARAM_TERMINAL_STRUCT_BITS !=
			(CHAR_BIT * sizeof(ia_css_param_terminal_t)));

	COMPILATION_ERROR_IF(0 !=
		sizeof(ia_css_param_terminal_t) % sizeof(uint64_t));

	COMPILATION_ERROR_IF(
		SIZE_OF_PARAM_SEC_STRUCT_BITS !=
			(CHAR_BIT * sizeof(ia_css_param_section_desc_t)));

	COMPILATION_ERROR_IF(0 !=
		sizeof(ia_css_param_section_desc_t) % sizeof(uint64_t));

	COMPILATION_ERROR_IF(
		SIZE_OF_SPATIAL_PARAM_TERM_STRUCT_BITS !=
			(CHAR_BIT * sizeof(ia_css_spatial_param_terminal_t)));

	COMPILATION_ERROR_IF(0 !=
		sizeof(ia_css_spatial_param_terminal_t) % sizeof(uint64_t));

	COMPILATION_ERROR_IF(
		SIZE_OF_FRAME_GRID_PARAM_SEC_STRUCT_BITS !=
			(CHAR_BIT * sizeof(
				ia_css_frame_grid_param_section_desc_t)));

	COMPILATION_ERROR_IF(0 != sizeof(
		ia_css_frame_grid_param_section_desc_t) % sizeof(uint64_t));

	COMPILATION_ERROR_IF(
		SIZE_OF_FRAG_GRID_STRUCT_BITS !=
			(CHAR_BIT * sizeof(ia_css_fragment_grid_desc_t)));

	COMPILATION_ERROR_IF(0 !=
		sizeof(ia_css_fragment_grid_desc_t) % sizeof(uint64_t));

	COMPILATION_ERROR_IF(
		SIZE_OF_SLICED_PARAM_TERM_STRUCT_BITS !=
			(CHAR_BIT * sizeof(ia_css_sliced_param_terminal_t)));

	COMPILATION_ERROR_IF(0 !=
		sizeof(ia_css_sliced_param_terminal_t)%sizeof(uint64_t));

	COMPILATION_ERROR_IF(
		SIZE_OF_FRAGMENT_SLICE_DESC_STRUCT_BITS !=
			(CHAR_BIT * sizeof(ia_css_fragment_slice_desc_t)));

	COMPILATION_ERROR_IF(0 !=
		sizeof(ia_css_fragment_slice_desc_t)%sizeof(uint64_t));

	COMPILATION_ERROR_IF(
		SIZE_OF_SLICE_PARAM_SECTION_DESC_STRUCT_BITS !=
			(CHAR_BIT * sizeof(
				ia_css_slice_param_section_desc_t)));

	COMPILATION_ERROR_IF(0 !=
		sizeof(ia_css_slice_param_section_desc_t)%sizeof(uint64_t));

	COMPILATION_ERROR_IF(
		SIZE_OF_PROG_TERM_STRUCT_BITS !=
			(CHAR_BIT * sizeof(ia_css_program_terminal_t)));

	COMPILATION_ERROR_IF(0 !=
		sizeof(ia_css_program_terminal_t)%sizeof(uint64_t));

	COMPILATION_ERROR_IF(
		SIZE_OF_FRAG_SEQ_INFO_STRUCT_BITS !=
		(CHAR_BIT * sizeof(
			ia_css_kernel_fragment_sequencer_info_desc_t)));

	COMPILATION_ERROR_IF(0 !=
		sizeof(ia_css_kernel_fragment_sequencer_info_desc_t) %
						sizeof(uint64_t));

	COMPILATION_ERROR_IF(
		SIZE_OF_FRAG_SEQ_COMMANDS_STRUCT_BITS !=
		(CHAR_BIT * sizeof(
			ia_css_kernel_fragment_sequencer_command_desc_t)));

	COMPILATION_ERROR_IF(0 !=
		sizeof(ia_css_kernel_fragment_sequencer_command_desc_t) %
						sizeof(uint64_t));

	COMPILATION_ERROR_IF(
		SIZE_OF_FRAG_PARAM_SEC_STRUCT_BITS !=
		(CHAR_BIT * sizeof(ia_css_fragment_param_section_desc_t)));

	COMPILATION_ERROR_IF(0 !=
		sizeof(ia_css_fragment_param_section_desc_t)%sizeof(uint64_t));
}

/*
 * Functions not to inline
 */

/*
 * This source file is created with the intention of sharing and
 * compiled for host and firmware. Since there is no native 64bit
 * data type support for firmware this wouldn't compile for SP
 * tile. The part of the file that is not compilable are marked
 * with the following __HIVECC marker and this comment. Once we
 * come up with a solution to address this issue this will be
 * removed.
 */
#if !defined(__HIVECC)
size_t ia_css_sizeof_terminal(
	const ia_css_terminal_manifest_t *manifest,
	const ia_css_program_group_param_t *param)
{
	size_t size = 0;
	uint16_t fragment_count =
		ia_css_program_group_param_get_fragment_count(param);

	COMPILATION_ERROR_IF(
		SIZE_OF_DATA_TERMINAL_STRUCT_BITS !=
		(CHAR_BIT * sizeof(ia_css_data_terminal_t)));

	COMPILATION_ERROR_IF(
		0 != sizeof(ia_css_data_terminal_t)%sizeof(uint64_t));

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_sizeof_terminal(): enter:\n");

	verifexit(manifest != NULL);
	verifexit(param != NULL);

	if (ia_css_is_terminal_manifest_parameter_terminal(manifest)) {
		const ia_css_param_terminal_manifest_t *param_term_man =
			(const ia_css_param_terminal_manifest_t *)manifest;
		if (ia_css_terminal_manifest_get_type(manifest) ==
				IA_CSS_TERMINAL_TYPE_PARAM_CACHED_IN) {
			size = ia_css_param_in_terminal_get_descriptor_size(
			param_term_man->param_manifest_section_desc_count);
		} else if (ia_css_terminal_manifest_get_type(manifest) ==
				IA_CSS_TERMINAL_TYPE_PARAM_CACHED_OUT) {
			size = ia_css_param_out_terminal_get_descriptor_size(
			param_term_man->param_manifest_section_desc_count,
			fragment_count);
		} else {
			assert(NULL == "Invalid parameter terminal type");
			IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
				"ia_css_sizeof_terminal(): Invalid parameter terminal type:\n");
			verifjmpexit(0);
		}
	} else if (ia_css_is_terminal_manifest_data_terminal(manifest)) {
		size += sizeof(ia_css_data_terminal_t);
		size += fragment_count * sizeof(ia_css_fragment_descriptor_t);
	} else if (ia_css_is_terminal_manifest_program_terminal(manifest)) {
		ia_css_program_terminal_manifest_t *prog_term_man =
			(ia_css_program_terminal_manifest_t *)manifest;

		size = ia_css_program_terminal_get_descriptor_size(
			fragment_count,
			prog_term_man->
			fragment_param_manifest_section_desc_count,
			prog_term_man->
			kernel_fragment_sequencer_info_manifest_info_count,
			(fragment_count * prog_term_man->
			max_kernel_fragment_sequencer_command_desc));
	} else if (ia_css_is_terminal_manifest_spatial_parameter_terminal(
				manifest)) {
		ia_css_spatial_param_terminal_manifest_t *spatial_param_term =
			(ia_css_spatial_param_terminal_manifest_t *)manifest;
		size = ia_css_spatial_param_terminal_get_descriptor_size(
		spatial_param_term->
		frame_grid_param_manifest_section_desc_count,
			fragment_count);
	}
EXIT:
	if (NULL == manifest || NULL == param) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_sizeof_terminal invalid argument\n");
	}
	return size;
}

ia_css_terminal_t *ia_css_terminal_create(
	void *raw_mem,
	const ia_css_terminal_manifest_t *manifest,
	const ia_css_terminal_param_t *terminal_param,
	ia_css_kernel_bitmap_t enable_bitmap)
{
	char *terminal_raw_ptr;
	ia_css_terminal_t *terminal = NULL;
	uint16_t fragment_count;
	int i, j;
	int retval = -1;
	ia_css_program_group_param_t *param;

	IA_CSS_TRACE_2(PSYSAPI_DYNAMIC, INFO,
		"ia_css_terminal_create(manifest %p, terminal_param %p): enter:\n",
		manifest, terminal_param);

	param = ia_css_terminal_param_get_parent(terminal_param);
	fragment_count = ia_css_program_group_param_get_fragment_count(param);

	verifexit(manifest != NULL);
	verifexit(param != NULL);

	terminal_raw_ptr = (char *) raw_mem;

	terminal = (ia_css_terminal_t *) terminal_raw_ptr;
	verifexit(terminal != NULL);

	terminal->size = (uint16_t)ia_css_sizeof_terminal(manifest, param);
	verifexit(ia_css_terminal_set_type(
		terminal, ia_css_terminal_manifest_get_type(manifest)) == 0);

	terminal->ID = ia_css_terminal_manifest_get_ID(manifest);

	verifexit(ia_css_terminal_set_buffer(terminal,
				VIED_NULL) == 0);

	if (ia_css_is_terminal_manifest_data_terminal(manifest) == true) {
		ia_css_data_terminal_t *dterminal =
			(ia_css_data_terminal_t *)terminal;
		ia_css_frame_t *frame =
			ia_css_data_terminal_get_frame(dterminal);
		ia_css_kernel_bitmap_t intersection =
			ia_css_kernel_bitmap_intersection(enable_bitmap,
			ia_css_data_terminal_manifest_get_kernel_bitmap(
			(const ia_css_data_terminal_manifest_t *)manifest));

		verifexit(frame != NULL);
		verifexit(ia_css_frame_set_buffer_state(
				frame, IA_CSS_BUFFER_NULL) == 0);
		verifexit(ia_css_is_kernel_bitmap_onehot(intersection) ==
					true);

		terminal_raw_ptr += sizeof(ia_css_data_terminal_t);
		dterminal->fragment_descriptor_offset =
			(uint16_t) (terminal_raw_ptr - (char *)terminal);

		dterminal->kernel_id = 0;
		while (!ia_css_is_kernel_bitmap_empty(intersection)) {
			intersection = ia_css_kernel_bitmap_shift(
					intersection);
			dterminal->kernel_id++;
		}
		assert(dterminal->kernel_id > 0);
		dterminal->kernel_id -= 1;

		/* some terminal and fragment initialization */
		dterminal->frame_descriptor.frame_format_type =
			terminal_param->frame_format_type;
		for (i = 0; i < IA_CSS_N_DATA_DIMENSION; i++) {
			dterminal->frame_descriptor.dimension[i] =
				terminal_param->dimensions[i];
		}
		dterminal->frame_descriptor.stride[0] = terminal_param->stride;
		dterminal->frame_descriptor.bpp = terminal_param->bpp;
		dterminal->frame_descriptor.bpe = terminal_param->bpe;
		/*
		 * Initial solution for single fragment initialization
		 * TODO:
		 * where to get the fragment description params from???
		 */
		if (fragment_count > 0) {
			ia_css_fragment_descriptor_t *fragment_descriptor =
				(ia_css_fragment_descriptor_t *)
				terminal_raw_ptr;

			fragment_descriptor->index[IA_CSS_COL_DIMENSION] =
				terminal_param->index[IA_CSS_COL_DIMENSION];
			fragment_descriptor->index[IA_CSS_ROW_DIMENSION] =
				terminal_param->index[IA_CSS_ROW_DIMENSION];
			fragment_descriptor->offset[0] =
				terminal_param->offset;
			for (i = 0; i < IA_CSS_N_DATA_DIMENSION; i++) {
				fragment_descriptor->dimension[i] =
					terminal_param->fragment_dimensions[i];
			}
		}
		/* end fragment stuff */
	} else if (ia_css_is_terminal_manifest_parameter_terminal(manifest) ==
			true) {
		ia_css_param_terminal_t *pterminal =
			(ia_css_param_terminal_t *)terminal;
		uint16_t section_count =
			((const ia_css_param_terminal_manifest_t *)manifest)->
			param_manifest_section_desc_count;
		size_t curr_offset = 0;

		pterminal->param_section_desc_offset =
			sizeof(ia_css_param_terminal_t);

		for (i = 0; i < section_count; i++) {
			ia_css_param_section_desc_t *section =
			ia_css_param_in_terminal_get_param_section_desc(
					pterminal, i);
			const ia_css_param_manifest_section_desc_t *
				man_section =
		ia_css_param_terminal_manifest_get_param_manifest_section_desc(
			(const ia_css_param_terminal_manifest_t *)manifest, i);

			verifjmpexit(man_section != NULL);
			verifjmpexit(section != NULL);

			section->mem_size = man_section->max_mem_size;
			section->mem_offset = curr_offset;
			curr_offset += man_section->max_mem_size;
		}
	} else if (ia_css_is_terminal_manifest_program_terminal(manifest) ==
			true &&
		ia_css_terminal_manifest_get_type(manifest) ==
		IA_CSS_TERMINAL_TYPE_PROGRAM) { /* for program terminal */
		ia_css_program_terminal_t *prog_terminal =
			(ia_css_program_terminal_t *)terminal;
		const ia_css_program_terminal_manifest_t *prog_terminal_man =
			(const ia_css_program_terminal_manifest_t *)manifest;
		ia_css_kernel_fragment_sequencer_info_desc_t
		*sequencer_info_desc_base = NULL;
		uint16_t section_count = prog_terminal_man->
		fragment_param_manifest_section_desc_count;
		uint16_t manifest_info_count =
			prog_terminal_man->
			kernel_fragment_sequencer_info_manifest_info_count;
		/* information needs to come from user or manifest once
		 * the size sizeof function is updated.
		 */
		uint16_t nof_command_objs = 0;
		size_t curr_offset = 0;

		prog_terminal->kernel_fragment_sequencer_info_desc_offset =
			sizeof(ia_css_program_terminal_t);
		prog_terminal->fragment_param_section_desc_offset =
			prog_terminal->
			kernel_fragment_sequencer_info_desc_offset +
			(fragment_count * manifest_info_count *
			sizeof(ia_css_kernel_fragment_sequencer_info_desc_t)) +
			(nof_command_objs *
			sizeof(
			ia_css_kernel_fragment_sequencer_command_desc_t));

		NOT_USED(sequencer_info_desc_base);
		for (i = 0; i < fragment_count; i++) {
			for (j = 0; j < section_count; j++) {
				ia_css_fragment_param_section_desc_t *section =
			ia_css_program_terminal_get_fragment_param_section_desc(
					prog_terminal, i, j, section_count);
			const ia_css_fragment_param_manifest_section_desc_t *
			man_section =
ia_css_program_terminal_manifest_get_fragment_param_manifest_section_desc
				(prog_terminal_man, j);

				verifjmpexit(man_section != NULL);
				verifjmpexit(section != NULL);

				section->mem_size = man_section->max_mem_size;
				section->mem_offset = curr_offset;
				curr_offset += man_section->max_mem_size;
			}

			sequencer_info_desc_base =
		ia_css_program_terminal_get_kernel_fragment_sequencer_info_desc(
					prog_terminal, i, 0,
					manifest_info_count);

			/*
			 * This offset cannot be initialized properly
			 * since the number of commands in every sequencer
			 * is not known at this point
			 */
			/*for (j = 0; j < manifest_info_count; j++) {
				sequencer_info_desc_base[j].
					command_desc_offset =
				prog_terminal->
				kernel_fragment_sequencer_info_desc_offset +
				(manifest_info_count *
				sizeof(
				ia_css_kernel_fragment_sequencer_info_desc_t) +
				(nof_command_objs *
				sizeof(
				ia_css_kernel_fragment_sequencer_command_desc_t
				));
			}*/
		}
	} else if (ia_css_is_terminal_manifest_spatial_parameter_terminal(
				manifest) == true) {
		ia_css_spatial_param_terminal_t *spatial_param_terminal =
			(ia_css_spatial_param_terminal_t *)terminal;
		ia_css_spatial_param_terminal_manifest_t *
			spatia_param_terminal_man =
			(ia_css_spatial_param_terminal_manifest_t *)manifest;

		/* Initialize the spatial terminal structure */
		spatial_param_terminal->fragment_grid_desc_offset =
			sizeof(ia_css_spatial_param_terminal_t);
		spatial_param_terminal->frame_grid_param_section_desc_offset =
			spatial_param_terminal->fragment_grid_desc_offset +
			(fragment_count * sizeof(ia_css_fragment_grid_desc_t));
		spatial_param_terminal->kernel_id =
			spatia_param_terminal_man->kernel_id;
	} else if (ia_css_is_terminal_manifest_sliced_terminal(manifest) ==
			true) {
		ia_css_sliced_param_terminal_t *sliced_param_terminal =
			(ia_css_sliced_param_terminal_t *)terminal;
		ia_css_sliced_param_terminal_manifest_t
		*sliced_param_terminal_man =
			(ia_css_sliced_param_terminal_manifest_t *)manifest;

		/* Initialize the sliced terminal structure */
		sliced_param_terminal->fragment_slice_desc_offset =
			sizeof(ia_css_sliced_param_terminal_t);
		sliced_param_terminal->kernel_id =
			sliced_param_terminal_man->kernel_id;
	} else {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_terminal_create failed, not a data or param terminal. Returning (%i)\n",
			EFAULT);
		goto EXIT;
	}

	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, INFO,
		"ia_css_terminal_create(): Created successfully terminal %p\n",
		terminal);

	retval = 0;
EXIT:
	if (NULL == manifest) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_terminal_create invalid argument\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_terminal_create failed (%i)\n", retval);
		terminal = ia_css_terminal_destroy(terminal);
	}
	return terminal;
}

ia_css_terminal_t *ia_css_terminal_destroy(
	ia_css_terminal_t *terminal)
{
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, INFO,
		"ia_css_terminal_destroy(terminal %p): enter:\n", terminal);
	return terminal;
}

uint16_t ia_css_param_terminal_compute_section_count(
	const ia_css_terminal_manifest_t *manifest,
	const ia_css_program_group_param_t *param) /* Delete 2nd argument*/
{
	uint16_t section_count = 0;

	NOT_USED(param);

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_param_terminal_compute_section_count(): enter:\n");

	verifexit(manifest != NULL);
	section_count = ((const ia_css_param_terminal_manifest_t *)manifest)->
				param_manifest_section_desc_count;
EXIT:
	if (NULL == manifest || NULL == param) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_param_terminal_compute_section_count: invalid argument\n");
	}
	return section_count;
}
#endif
