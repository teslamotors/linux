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


#include <ia_css_psys_terminal.h>

#include <ia_css_psys_process_types.h>
#include <ia_css_psys_terminal_manifest.h>

#include <ia_css_program_group_data.h>
#include <ia_css_program_group_param.h>

#include <ia_css_psys_process_group.h>

#include <type_support.h>
#include <error_support.h>	/* for verifexit, verifjmpexit */
#include <assert_support.h>	/* for COMPILATION_ERROR_IF */
#include <misc_support.h>	/* for NOT_USED */
#include "ia_css_terminal_types.h"
#include "ia_css_terminal_manifest_types.h"
#include "ia_css_psys_dynamic_trace.h"

#define	N_UINT16_IN_DATA_TERMINAL_STRUCT	1
#define	N_UINT8_IN_DATA_TERMINAL_STRUCT		2
#define	N_PADDING_UINT8_IN_DATA_TERMINAL_STRUCT	4

#define SIZE_OF_DATA_TERMINAL_STRUCT_BITS \
	(SIZE_OF_TERMINAL_STRUCT_BITS \
	+ IA_CSS_FRAME_DESCRIPTOR_STRUCT_BITS \
	+ IA_CSS_FRAME_STRUCT_BITS \
	+ IA_CSS_STREAM_STRUCT_BITS \
	+ IA_CSS_FRAME_FORMAT_TYPE_BITS \
	+ IA_CSS_CONNECTION_TYPE_BITS \
	+ (N_UINT16_IN_DATA_TERMINAL_STRUCT * 16) \
	+ (N_UINT8_IN_DATA_TERMINAL_STRUCT * 8)	\
	+ (N_PADDING_UINT8_IN_DATA_TERMINAL_STRUCT * 8))

/*
 * The (data) terminal can be attached to a buffer or a stream.
 * The stream interface is not necessarily limited to strict in-order access.
 * For a stream the restriction is that contrary to a buffer it cannot be
 * addressed directly, i.e. it behaves as a port,
 * but it may support stream_pos() and/or seek() operations
 */
struct ia_css_data_terminal_s {
	/**< Data terminal base */
	ia_css_terminal_t base;
	/**< Properties of the data attached to the terminal */
	ia_css_frame_descriptor_t frame_descriptor;
	/**< Data buffer handle attached to the terminal */
	ia_css_frame_t frame;
	/**< (exclusive) Data stream handle attached to the terminal
	 * if the data is sourced over a device port
	 */
	ia_css_stream_t stream;
	/**< Indicates if this is a generic type or inbuild
	 * with variable size descriptor
	 */
	ia_css_frame_format_type_t frame_format_type;
	/**< Connection {buffer, stream, ...} */
	ia_css_connection_type_t connection_type;
	/**< Array[fragment_count] (fragment_count being equal for all
	 * terminals in a subgraph) of fragment descriptors
	 */
	uint16_t fragment_descriptor_offset;
	/**< Kernel id where this terminal is connected to */
	uint8_t kernel_id;
	/**< Indicate to which subgraph this terminal belongs
	 * for common constraints
	 */
	uint8_t subgraph_id;
	/**< Padding for 64bit alignment */
	uint8_t padding[N_PADDING_UINT8_IN_DATA_TERMINAL_STRUCT];
};

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

	verifexit(manifest != NULL, EINVAL);
	verifexit(param != NULL, EINVAL);

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

	verifexit(manifest != NULL, EINVAL);
	verifexit(param != NULL, EINVAL);

	terminal_raw_ptr = (char *) raw_mem;

	terminal = (ia_css_terminal_t *) terminal_raw_ptr;
	verifexit(terminal != NULL, EINVAL);

	terminal->size = (uint16_t)ia_css_sizeof_terminal(manifest, param);
	verifexit(ia_css_terminal_set_type(
		terminal, ia_css_terminal_manifest_get_type(manifest)) ==
			0, EINVAL);

	terminal->ID = ia_css_terminal_manifest_get_ID(manifest);

	verifexit(ia_css_terminal_set_buffer(terminal,
				VIED_NULL) == 0, EINVAL);

	if (ia_css_is_terminal_manifest_data_terminal(manifest) == true) {
		ia_css_data_terminal_t *dterminal =
			(ia_css_data_terminal_t *)terminal;
		ia_css_frame_t *frame =
			ia_css_data_terminal_get_frame(dterminal);
		ia_css_kernel_bitmap_t intersection =
			ia_css_kernel_bitmap_intersection(enable_bitmap,
			ia_css_data_terminal_manifest_get_kernel_bitmap(
			(const ia_css_data_terminal_manifest_t *)manifest));

		verifexit(frame != NULL, EINVAL);
		verifexit(ia_css_frame_set_buffer_state(
				frame, IA_CSS_BUFFER_NULL) == 0, EINVAL);
		verifexit(ia_css_is_kernel_bitmap_onehot(intersection) ==
					true, EINVAL);

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
#endif

int ia_css_terminal_print(
	const ia_css_terminal_t *terminal,
	void *fid)
{
	int retval = -1;
	int i;
	bool is_data = false;
	uint16_t fragment_count = 0;

	NOT_USED(fid);

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, INFO,
		"ia_css_terminal_print(): enter:\n");

	verifexit(terminal != NULL, EINVAL);

	is_data = ia_css_is_terminal_data_terminal(terminal);

	IA_CSS_TRACE_4(PSYSAPI_DYNAMIC, INFO,
		"\tTerminal %p sizeof %d, typeof %d, parent %p\n",
		terminal,
		(int)ia_css_terminal_get_size(terminal),
		(int)ia_css_terminal_get_type(terminal),
		(void *)ia_css_terminal_get_parent(terminal));

	if (is_data) {
		ia_css_data_terminal_t *dterminal =
			(ia_css_data_terminal_t *)terminal;

		fragment_count =
			ia_css_data_terminal_get_fragment_count(dterminal);
		verifexit(fragment_count != 0, EINVAL);
		retval = ia_css_frame_descriptor_print(
			ia_css_data_terminal_get_frame_descriptor(dterminal),
			fid);
		verifexit(retval == 0, EINVAL);
		retval = ia_css_frame_print(
			ia_css_data_terminal_get_frame(dterminal), fid);
		verifexit(retval == 0, EINVAL);
		for (i = 0; i < (int)fragment_count; i++) {
			retval = ia_css_fragment_descriptor_print(
				ia_css_data_terminal_get_fragment_descriptor(
					dterminal, i), fid);
			verifexit(retval == 0, EINVAL);
		}
	} else {
		/*TODO:
		 * FIXME print param terminal sections.
		 */
	}

	retval = 0;
EXIT:
	if (NULL == terminal) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_terminal_print invalid argument terminal\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_terminal_print failed (%i)\n", retval);
	}
	return retval;
}

bool ia_css_is_terminal_input(
	const ia_css_terminal_t *terminal)
{
	bool is_input = false;
	ia_css_terminal_type_t terminal_type;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_is_terminal_input(): enter:\n");

	verifexit(terminal != NULL, EINVAL);

	terminal_type = ia_css_terminal_get_type(terminal);

	switch (terminal_type) {
	case IA_CSS_TERMINAL_TYPE_DATA_IN:		/* Fall through */
	case IA_CSS_TERMINAL_TYPE_STATE_IN:		/* Fall through */
	case IA_CSS_TERMINAL_TYPE_PARAM_STREAM:		/* Fall through */
	case IA_CSS_TERMINAL_TYPE_PARAM_CACHED_IN:
		is_input = true;
		break;
	case IA_CSS_TERMINAL_TYPE_DATA_OUT:		/* Fall through */
	case IA_CSS_TERMINAL_TYPE_STATE_OUT:
		is_input = false;
		break;
	default:
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_is_terminal_input: Unknown terminal type (%d)\n",
			terminal_type);
		goto EXIT;
	}

EXIT:
	if (NULL == terminal) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_is_terminal_input invalid argument\n");
	}
	return is_input;
}

size_t ia_css_terminal_get_size(
	const ia_css_terminal_t	*terminal)
{
	size_t size = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_terminal_get_size(): enter:\n");

	if (terminal != NULL) {
		size = terminal->size;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_terminal_get_size invalid argument\n");
	}
	return size;
}

ia_css_terminal_type_t ia_css_terminal_get_type(
	const ia_css_terminal_t	*terminal)
{
	ia_css_terminal_type_t	terminal_type = IA_CSS_N_TERMINAL_TYPES;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_terminal_get_type(): enter:\n");

	if (terminal != NULL) {
		terminal_type = terminal->terminal_type;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_terminal_get_type invalid argument\n");
	}
	return terminal_type;
}

int ia_css_terminal_set_type(
	ia_css_terminal_t *terminal,
	const ia_css_terminal_type_t terminal_type)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_terminal_set_type(): enter:\n");

	verifexit(terminal != NULL, EINVAL);
	terminal->terminal_type = terminal_type;

	retval = 0;
EXIT:
	if (NULL == terminal) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_terminal_set_type invalid argument terminal\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_terminal_set_type failed (%i)\n", retval);
	}
	return retval;
}

uint16_t ia_css_terminal_get_terminal_manifest_index(
	const ia_css_terminal_t *terminal)
{
	uint16_t terminal_manifest_index;

	terminal_manifest_index = 0xffff;
	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_terminal_get_terminal_manifest_index(): enter:\n");

	if (terminal != NULL) {
		terminal_manifest_index = terminal->tm_index;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_terminal_get_terminal_manifest_index: invalid argument\n");
	}
	return terminal_manifest_index;
}

int ia_css_terminal_set_terminal_manifest_index(
	ia_css_terminal_t *terminal,
	const uint16_t terminal_manifest_index)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_terminal_set_terminal_manifest_index(): enter:\n");

	verifexit(terminal != NULL, EINVAL);
	terminal->tm_index = terminal_manifest_index;

	retval = 0;
EXIT:
	if (NULL == terminal) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_terminal_set_terminal_manifest_index: invalid argument terminal\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_terminal_set_terminal_manifest_index: failed (%i)\n",
			retval);
	}
	return retval;
}

ia_css_terminal_ID_t ia_css_terminal_get_ID(
	const ia_css_terminal_t	*terminal)
{
	ia_css_terminal_ID_t retval;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_terminal_get_ID(): enter:\n");

	if (terminal != NULL) {
		retval = terminal->ID;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_terminal_get_ID invalid argument\n");
		retval = 0;
	}
	return retval;
}

uint8_t ia_css_data_terminal_get_kernel_id(
	const ia_css_data_terminal_t *dterminal)
{
	uint8_t retval;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_data_terminal_get_kernel_id(): enter:\n");

	if (dterminal != NULL) {
		retval = dterminal->kernel_id;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_data_terminal_get_kernel_id: invalid argument\n");
		retval =  0;
	}
	return retval;
}

ia_css_connection_type_t ia_css_data_terminal_get_connection_type(
	const ia_css_data_terminal_t *dterminal)
{
	ia_css_connection_type_t connection_type = IA_CSS_N_CONNECTION_TYPES;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_data_terminal_get_connection_type(): enter:\n");

	verifexit(dterminal != NULL, EINVAL);
	connection_type = dterminal->connection_type;

EXIT:
	if (NULL == dterminal) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_data_terminal_get_connection_type: invalid argument\n");
	}
	return connection_type;
}

int ia_css_data_terminal_set_connection_type(
	ia_css_data_terminal_t *dterminal,
	const ia_css_connection_type_t connection_type)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_data_terminal_set_connection_type(): enter:\n");

	verifexit(dterminal != NULL, EINVAL);
	dterminal->connection_type = connection_type;

	retval = 0;
EXIT:
	if (NULL == dterminal) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_data_terminal_set_connection_type: invalid argument dterminal\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_data_terminal_set_connection_type failed (%i)\n",
			retval);
	}
	return retval;
}

ia_css_process_group_t *ia_css_terminal_get_parent(
	const ia_css_terminal_t	*terminal)
{
	ia_css_process_group_t *parent = NULL;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_terminal_get_parent(): enter:\n");

	verifexit(terminal != NULL, EINVAL);

	parent = (ia_css_process_group_t *) ((char *)terminal +
					terminal->parent_offset);

EXIT:
	if (NULL == terminal) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_terminal_get_parent invalid argument\n");
	}
	return parent;
}

int ia_css_terminal_set_parent(
	ia_css_terminal_t *terminal,
	ia_css_process_group_t *parent)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_terminal_set_parent(): enter:\n");

	verifexit(terminal != NULL, EINVAL);
	verifexit(parent != NULL, EINVAL);

	terminal->parent_offset = (uint16_t) ((char *)parent -
						(char *)terminal);

	retval = 0;
EXIT:
	if (NULL == terminal || NULL == parent) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_terminal_set_parent invalid argument\n");
	}
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_terminal_set_parent failed (%i)\n", retval);
	}
	return retval;
}

ia_css_frame_t *ia_css_data_terminal_get_frame(
	const ia_css_data_terminal_t *dterminal)
{
	ia_css_frame_t *frame = NULL;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_data_terminal_get_frame(): enter:\n");

	verifexit(dterminal != NULL, EINVAL);

	frame = (ia_css_frame_t	*)(&(dterminal->frame));
EXIT:
	if (NULL == dterminal) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_data_terminal_get_frame invalid argument\n");
	}
	return frame;
}

ia_css_frame_descriptor_t *ia_css_data_terminal_get_frame_descriptor(
	const ia_css_data_terminal_t *dterminal)
{
	ia_css_frame_descriptor_t *frame_descriptor = NULL;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_data_terminal_get_frame_descriptor(): enter:\n");

	verifexit(dterminal != NULL, EINVAL);

	frame_descriptor =
		(ia_css_frame_descriptor_t *)(&(dterminal->frame_descriptor));
EXIT:
	if (NULL == dterminal) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_data_terminal_get_frame_descriptor: invalid argument\n");
	}
	return frame_descriptor;
}

ia_css_fragment_descriptor_t *ia_css_data_terminal_get_fragment_descriptor(
	const ia_css_data_terminal_t *dterminal,
	const unsigned int fragment_index)
{
	ia_css_fragment_descriptor_t *fragment_descriptor = NULL;
	uint16_t fragment_count = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_data_terminal_get_frame_descriptor(): enter:\n");

	fragment_count = ia_css_data_terminal_get_fragment_count(dterminal);

	verifexit(dterminal != NULL, EINVAL);

	verifexit(fragment_count != 0, EINVAL);

	verifexit(fragment_index < fragment_count, EINVAL);

	fragment_descriptor = (ia_css_fragment_descriptor_t *)
		((char *)dterminal + dterminal->fragment_descriptor_offset);

	fragment_descriptor += fragment_index;
EXIT:
	if (NULL == dterminal) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_data_terminal_get_frame_descriptor: invalid argument\n");
	}
	return fragment_descriptor;
}

uint16_t ia_css_data_terminal_get_fragment_count(
	const ia_css_data_terminal_t *dterminal)
{
	ia_css_process_group_t *parent;
	uint16_t fragment_count = 0;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_data_terminal_get_frame_descriptor(): enter:\n");

	parent = ia_css_terminal_get_parent((ia_css_terminal_t *)dterminal);

	verifexit(dterminal != NULL, EINVAL);

	verifexit(parent != NULL, EINVAL);

	fragment_count = ia_css_process_group_get_fragment_count(parent);
EXIT:
	if (NULL == dterminal) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_data_terminal_get_fragment_count: invalid argument\n");
	}
	return fragment_count;
}

bool ia_css_is_terminal_parameter_terminal(
	const ia_css_terminal_t	*terminal)
{
	ia_css_terminal_type_t terminal_type;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_is_terminal_parameter_terminal(): enter:\n");

	/* will return an error value on error */
	terminal_type = ia_css_terminal_get_type(terminal);

	if (NULL == terminal) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_is_terminal_parameter_terminal: invalid argument\n");
	}
	return (terminal_type == IA_CSS_TERMINAL_TYPE_PARAM_CACHED_IN ||
		terminal_type == IA_CSS_TERMINAL_TYPE_PARAM_CACHED_OUT);
}

bool ia_css_is_terminal_data_terminal(
	const ia_css_terminal_t *terminal)
{
	ia_css_terminal_type_t terminal_type;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_is_terminal_data_terminal(): enter:\n");

	/* will return an error value on error */
	terminal_type = ia_css_terminal_get_type(terminal);

	if (NULL == terminal) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_is_terminal_data_terminal invalid argument\n");
	}
	return (terminal_type == IA_CSS_TERMINAL_TYPE_DATA_IN ||
			terminal_type == IA_CSS_TERMINAL_TYPE_DATA_OUT);
}

bool ia_css_is_terminal_program_terminal(
	const ia_css_terminal_t *terminal)
{
	ia_css_terminal_type_t	terminal_type;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_is_terminal_program_terminal(): enter:\n");

	/* will return an error value on error */
	terminal_type = ia_css_terminal_get_type(terminal);

	if (NULL == terminal) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_is_terminal_program_terminal: invalid argument\n");
	}
	return (terminal_type == IA_CSS_TERMINAL_TYPE_PROGRAM);
}

bool ia_css_is_terminal_spatial_parameter_terminal(
	const ia_css_terminal_t *terminal)
{
	ia_css_terminal_type_t terminal_type;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_is_terminal_spatial_param_terminal(): enter:\n");

	/* will return an error value on error */
	terminal_type = ia_css_terminal_get_type(terminal);

	if (NULL == terminal) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_is_terminal_spatial_param_terminal: invalid argument\n");
	}
	return (terminal_type == IA_CSS_TERMINAL_TYPE_PARAM_SPATIAL_IN ||
		terminal_type == IA_CSS_TERMINAL_TYPE_PARAM_SPATIAL_OUT);
}

/*
 * This function is only used during terminal creation. Beside
 * that, this function has a call to function in
 * ia_css_psys_terminal_manifest.c which will add more code which
 * will not be used in SP.
 */
#if !defined(__HIVECC)
uint16_t ia_css_param_terminal_compute_section_count(
	const ia_css_terminal_manifest_t *manifest,
	const ia_css_program_group_param_t *param) /* Delete 2nd argument*/
{
	uint16_t section_count = 0;

	NOT_USED(param);

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_param_terminal_compute_section_count(): enter:\n");

	verifexit(manifest != NULL, EINVAL);
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

uint8_t ia_css_data_terminal_compute_plane_count(
	const ia_css_terminal_manifest_t *manifest,
	const ia_css_program_group_param_t *param)
{
	uint8_t	plane_count = 1;

	verifexit(manifest != NULL, EINVAL);
	verifexit(param != NULL, EINVAL);
	/* TODO: Implementation Missing*/

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_data_terminal_compute_plane_count(): enter:\n");
EXIT:
	if (NULL == manifest || NULL == param) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_data_terminal_compute_plane_count: invalid argument\n");
	}
	return plane_count;
}

vied_vaddress_t  ia_css_terminal_get_buffer(
	const ia_css_terminal_t *terminal)
{
	vied_vaddress_t buffer = VIED_NULL;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_terminal_get_buffer(): enter:\n");

	if (ia_css_is_terminal_data_terminal(terminal)) {
		ia_css_frame_t *frame = ia_css_data_terminal_get_frame(
					(ia_css_data_terminal_t *)terminal);

		verifexit(frame != NULL, EINVAL);
		buffer = ia_css_frame_get_buffer(frame);
	} else if (ia_css_is_terminal_parameter_terminal(terminal)) {
		const ia_css_param_terminal_t *param_terminal =
			(const ia_css_param_terminal_t *)terminal;

		buffer = param_terminal->param_payload.buffer;
	}  else if (ia_css_is_terminal_program_terminal(terminal)) {
		const ia_css_program_terminal_t *program_terminal =
			(const ia_css_program_terminal_t *)terminal;

		buffer = program_terminal->param_payload.buffer;
	} else if (ia_css_is_terminal_spatial_parameter_terminal(terminal)) {
		const ia_css_spatial_param_terminal_t *spatial_terminal =
			(const ia_css_spatial_param_terminal_t *)terminal;

		buffer = spatial_terminal->param_payload.buffer;
	}
EXIT:
	if (NULL == terminal) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_terminal_get_buffer: invalid argument terminal\n");
	}
	return buffer;
}

int ia_css_terminal_set_buffer(
	ia_css_terminal_t *terminal,
	vied_vaddress_t buffer)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_terminal_set_buffer(): enter:\n");

	if (ia_css_is_terminal_data_terminal(terminal) == true) {
		/* Currently using Frames inside data terminal ,
		 * TODO: start directly using data.
		 */
		ia_css_data_terminal_t *dterminal =
			(ia_css_data_terminal_t *)terminal;
		ia_css_frame_t *frame =
			ia_css_data_terminal_get_frame(dterminal);

		verifexit(frame != NULL, EINVAL);
		retval = ia_css_frame_set_buffer(frame, buffer);
		verifexit(retval == 0, EINVAL);
	} else if (ia_css_is_terminal_parameter_terminal(terminal) == true) {
		ia_css_param_terminal_t *pterminal =
			(ia_css_param_terminal_t *)terminal;

		pterminal->param_payload.buffer = buffer;
		retval = 0;
	} else if (ia_css_is_terminal_program_terminal(terminal) == true) {
		ia_css_program_terminal_t *pterminal =
			(ia_css_program_terminal_t *)terminal;

		pterminal->param_payload.buffer = buffer;
		retval = 0;
	} else if (ia_css_is_terminal_spatial_parameter_terminal(terminal) ==
			true) {
		ia_css_spatial_param_terminal_t *pterminal =
			(ia_css_spatial_param_terminal_t *)terminal;

		pterminal->param_payload.buffer = buffer;
		retval = 0;
	} else {
		return retval;
	}

	retval = 0;
EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_terminal_set_buffer failed (%i)\n", retval);
	}
	return retval;
}

STORAGE_CLASS_INLINE bool ia_css_is_data_terminal_valid(
	const ia_css_terminal_t *terminal,
	const ia_css_terminal_manifest_t *terminal_manifest,
	const uint16_t nof_fragments)
{
	bool invalid_flag = false;

	const ia_css_data_terminal_t *dterminal =
		(ia_css_data_terminal_t *)terminal;
	const ia_css_data_terminal_manifest_t *dt_manifest =
		(ia_css_data_terminal_manifest_t *)terminal_manifest;
	const ia_css_frame_descriptor_t *frame_descriptor;
	ia_css_frame_format_bitmap_t man_frame_format_bitmap;
	ia_css_frame_format_bitmap_t proc_frame_format_bitmap;
	uint16_t max_value[IA_CSS_N_DATA_DIMENSION];
	uint16_t min_value[IA_CSS_N_DATA_DIMENSION];

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_is_data_terminal_valid enter\n");

	frame_descriptor =
		ia_css_data_terminal_get_frame_descriptor(dterminal);
	verifjmpexit(frame_descriptor != NULL);
	man_frame_format_bitmap =
		ia_css_data_terminal_manifest_get_frame_format_bitmap(
					dt_manifest);
	proc_frame_format_bitmap =
		ia_css_frame_format_bit_mask(
				frame_descriptor->frame_format_type);
	/*
	 * TODO: Replace by 'validation of frame format type'.
	 * Currently frame format type is not correctly set by manifest,
	 * waiting for HSD 1804260604
	 */
	if (man_frame_format_bitmap > 0) {
		if ((man_frame_format_bitmap &
					proc_frame_format_bitmap) == 0) {
			uint32_t *bitmap_arr =
				(uint32_t *)&man_frame_format_bitmap;

			NOT_USED(bitmap_arr);
			IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
				"Frame format type not defined in manifest\n");
			IA_CSS_TRACE_2(PSYSAPI_DYNAMIC, INFO,
				" man bitmap_arr[]: %d,%d\n",
				bitmap_arr[1], bitmap_arr[0]);
			bitmap_arr = (uint32_t *)&proc_frame_format_bitmap;
			IA_CSS_TRACE_2(PSYSAPI_DYNAMIC, INFO,
				" proc bitmap_arr[]: %d,%d\n",
				bitmap_arr[1], bitmap_arr[0]);
		}
	} else {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"Frame format bitmap not defined in manifest\n");
	}
	ia_css_data_terminal_manifest_get_min_size(dt_manifest, min_value);
	/*
	 * TODO: Replace by validation of Minimal frame column dimensions.
	 *  Currently not correctly set by manifest yet,
	 *  waiting for HSD 1804260604
	 */
	if ((frame_descriptor->dimension[IA_CSS_COL_DIMENSION] <
		min_value[IA_CSS_COL_DIMENSION])) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"Minimal frame column dimensions not set correctly (by manifest)\n");
	}
	/*
	 * TODO: Replace by validation of Minimal frame row dimensions.
	 * Currently not correctly set by manifest yet,
	 * waiting for HSD 1804260604
	 */
	if (frame_descriptor->dimension[IA_CSS_ROW_DIMENSION] <
		min_value[IA_CSS_ROW_DIMENSION]) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"Minimal frame row dimensions not set correctly (by manifest)\n");
	}

	ia_css_data_terminal_manifest_get_max_size(dt_manifest, max_value);
	/*
	 * TODO: Replace by validation of Maximal frame column dimensions.
	 * Currently not correctly set by manifest yet,
	 * waiting for HSD 1804260604
	 */
	if (frame_descriptor->dimension[IA_CSS_COL_DIMENSION] >
		max_value[IA_CSS_COL_DIMENSION]) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"Maximal frame column dimensions not set correctly (by manifest)\n");
	}
	/*
	 * TODO: Replace by validation of Maximal frame row dimensions.
	 * Currently not correctly set by manifest yet,
	 * waiting for HSD 1804260604
	 */
	if (frame_descriptor->dimension[IA_CSS_ROW_DIMENSION] >
		max_value[IA_CSS_ROW_DIMENSION]) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"Maximal frame row dimensions not set correctly (by manifest)\n");
	}
	IA_CSS_TRACE_2(PSYSAPI_DYNAMIC, VERBOSE, "min_value: [%d,%d]\n",
		min_value[IA_CSS_COL_DIMENSION],
		min_value[IA_CSS_ROW_DIMENSION]);
	IA_CSS_TRACE_2(PSYSAPI_DYNAMIC, VERBOSE, "max_value: [%d,%d]\n",
		max_value[IA_CSS_COL_DIMENSION],
		max_value[IA_CSS_ROW_DIMENSION]);
	IA_CSS_TRACE_2(PSYSAPI_DYNAMIC, VERBOSE, "frame dim: [%d,%d]\n",
		frame_descriptor->dimension[IA_CSS_COL_DIMENSION],
		frame_descriptor->dimension[IA_CSS_ROW_DIMENSION]);
	/*
	 * TODO: Add validation of fragment dimensions.
	 * Currently not set by manifest yet, waiting for HSD 1804260604
	 */
	NOT_USED(nof_fragments);

	return (!invalid_flag);

EXIT:
	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
		"ia_css_is_data_terminal_valid() invalid argument\n");
	return false;
}

STORAGE_CLASS_INLINE void ia_css_program_terminal_seq_info_print(
	const ia_css_kernel_fragment_sequencer_info_manifest_desc_t
		*man_seq_info_desc,
	const ia_css_kernel_fragment_sequencer_info_desc_t
		*term_seq_info_desc)
{
	NOT_USED(man_seq_info_desc);
	NOT_USED(term_seq_info_desc);

	/* slice dimension column */
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"fragment_grid_slice_dimension: %d\n",
		term_seq_info_desc->
		fragment_grid_slice_dimension[IA_CSS_COL_DIMENSION]);
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"max_fragment_grid_slice_dimension: %d\n",
		man_seq_info_desc->
		max_fragment_grid_slice_dimension[IA_CSS_COL_DIMENSION]);
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"min_fragment_grid_slice_dimension: %d\n",
		man_seq_info_desc->
		min_fragment_grid_slice_dimension[IA_CSS_COL_DIMENSION]);

	/* slice dimension row */
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"fragment_grid_slice_dimension: %d\n",
		term_seq_info_desc->
		fragment_grid_slice_dimension[IA_CSS_ROW_DIMENSION]);
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"max_fragment_grid_slice_dimension: %d\n",
		man_seq_info_desc->
		max_fragment_grid_slice_dimension[IA_CSS_ROW_DIMENSION]);
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"min_fragment_grid_slice_dimension: %d\n",
		man_seq_info_desc->
		min_fragment_grid_slice_dimension[IA_CSS_ROW_DIMENSION]);

	/* slice count column */
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"fragment_grid_slice_count: %d\n",
		term_seq_info_desc->
		fragment_grid_slice_count[IA_CSS_COL_DIMENSION]);
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"max_fragment_grid_slice_count: %d\n",
		man_seq_info_desc->
		max_fragment_grid_slice_count[IA_CSS_COL_DIMENSION]);
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"min_fragment_grid_slice_count: %d\n",
		man_seq_info_desc->
		min_fragment_grid_slice_count[IA_CSS_COL_DIMENSION]);

	/* slice count row */
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"fragment_grid_slice_count: %d\n",
		term_seq_info_desc->
		fragment_grid_slice_count[IA_CSS_ROW_DIMENSION]);
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"max_fragment_grid_slice_count: %d\n",
		man_seq_info_desc->
		max_fragment_grid_slice_count[IA_CSS_ROW_DIMENSION]);
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"min_fragment_grid_slice_count: %d\n",
		man_seq_info_desc->
		min_fragment_grid_slice_count[IA_CSS_ROW_DIMENSION]);

	/* decimation factor column */
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"fragment_grid_point_decimation_factor: %d\n",
		term_seq_info_desc->
		fragment_grid_point_decimation_factor[IA_CSS_COL_DIMENSION]);
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"max_fragment_grid_point_decimation_factor: %d\n",
		man_seq_info_desc->
		max_fragment_grid_point_decimation_factor[IA_CSS_COL_DIMENSION]
		);
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"min_fragment_grid_point_decimation_factor: %d\n",
		man_seq_info_desc->
		min_fragment_grid_point_decimation_factor[IA_CSS_COL_DIMENSION]
		);

	/* decimation factor row */
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"fragment_grid_point_decimation_factor: %d\n",
		term_seq_info_desc->
		fragment_grid_point_decimation_factor[IA_CSS_ROW_DIMENSION]);
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"max_fragment_grid_point_decimation_factor: %d\n",
		man_seq_info_desc->
		max_fragment_grid_point_decimation_factor[
					IA_CSS_ROW_DIMENSION]);
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"min_fragment_grid_point_decimation_factor: %d\n",
		man_seq_info_desc->
		min_fragment_grid_point_decimation_factor[
					IA_CSS_ROW_DIMENSION]);

	/* index column */
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"fragment_grid_overlay_pixel_topleft_index: %d\n",
		term_seq_info_desc->
		fragment_grid_overlay_pixel_topleft_index[
						IA_CSS_COL_DIMENSION]);
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"max_fragment_grid_overlay_pixel_topleft_index: %d\n",
		man_seq_info_desc->
		max_fragment_grid_overlay_pixel_topleft_index[
						IA_CSS_COL_DIMENSION]);
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"min_fragment_grid_overlay_pixel_topleft_index: %d\n",
		man_seq_info_desc->
		min_fragment_grid_overlay_pixel_topleft_index[
						IA_CSS_COL_DIMENSION]);

	/* index row */
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"fragment_grid_overlay_pixel_topleft_index: %d\n",
		term_seq_info_desc->
		fragment_grid_overlay_pixel_topleft_index[
						IA_CSS_ROW_DIMENSION]);
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"max_fragment_grid_overlay_pixel_topleft_index: %d\n",
		man_seq_info_desc->
		max_fragment_grid_overlay_pixel_topleft_index[
						IA_CSS_ROW_DIMENSION]);
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"min_fragment_grid_overlay_pixel_topleft_index: %d\n",
		man_seq_info_desc->
		min_fragment_grid_overlay_pixel_topleft_index[
						IA_CSS_ROW_DIMENSION]);

	/* dimension column */
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"fragment_grid_overlay_pixel_dimension: %d\n",
		term_seq_info_desc->
		fragment_grid_overlay_pixel_dimension[
						IA_CSS_COL_DIMENSION]);
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"max_fragment_grid_overlay_pixel_dimension: %d\n",
		man_seq_info_desc->
		max_fragment_grid_overlay_pixel_dimension[
						IA_CSS_COL_DIMENSION]);
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"min_fragment_grid_overlay_pixel_dimension: %d\n",
		man_seq_info_desc->
		min_fragment_grid_overlay_pixel_dimension[
						IA_CSS_COL_DIMENSION]);

	/* dimension column */
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"fragment_grid_overlay_pixel_dimension: %d\n",
		term_seq_info_desc->
		fragment_grid_overlay_pixel_dimension[
						IA_CSS_ROW_DIMENSION]);
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"max_fragment_grid_overlay_pixel_dimension: %d\n",
		man_seq_info_desc->
		max_fragment_grid_overlay_pixel_dimension[
						IA_CSS_ROW_DIMENSION]);
	IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, VERBOSE,
		"min_fragment_grid_overlay_pixel_dimension: %d\n",
		man_seq_info_desc->
		min_fragment_grid_overlay_pixel_dimension[
						IA_CSS_ROW_DIMENSION]);
}

STORAGE_CLASS_INLINE bool ia_css_is_program_terminal_valid(
	const ia_css_terminal_t	*terminal,
	const ia_css_terminal_manifest_t *terminal_manifest,
	const uint16_t nof_fragments)
{
	bool invalid_flag = false;
	uint16_t frag_idx;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_is_program_terminal_valid enter\n");

	for (frag_idx = 0; frag_idx < nof_fragments; frag_idx++) {
		uint16_t frag_seq_info_count, seq_idx;
		const ia_css_program_terminal_t *prog_term;
		const ia_css_program_terminal_manifest_t *prog_term_man;

		prog_term = (const ia_css_program_terminal_t *)terminal;
		prog_term_man =
			(const ia_css_program_terminal_manifest_t *)
			terminal_manifest;
		frag_seq_info_count =
			prog_term_man->
			kernel_fragment_sequencer_info_manifest_info_count;

		for (seq_idx = 0; seq_idx < frag_seq_info_count; seq_idx++) {
			const ia_css_kernel_fragment_sequencer_info_desc_t
			*term_seq_info_desc;
			const
			ia_css_kernel_fragment_sequencer_info_manifest_desc_t *
			man_seq_info_desc;

			term_seq_info_desc =
		ia_css_program_terminal_get_kernel_fragment_sequencer_info_desc(
					prog_term, frag_idx, seq_idx,
					frag_seq_info_count);
			verifjmpexit(term_seq_info_desc != NULL);
			man_seq_info_desc =
		ia_css_program_terminal_manifest_get_kernel_frgmnt_seq_info_desc
					(prog_term_man, seq_idx);
			verifjmpexit(man_seq_info_desc != NULL);

			ia_css_program_terminal_seq_info_print(
				man_seq_info_desc, term_seq_info_desc);
			/* slice dimension column */
			invalid_flag = invalid_flag ||
				(term_seq_info_desc->
				fragment_grid_slice_dimension[
						IA_CSS_COL_DIMENSION] >
				man_seq_info_desc->
				max_fragment_grid_slice_dimension[
						IA_CSS_COL_DIMENSION]);
			invalid_flag = invalid_flag ||
				(term_seq_info_desc->
				fragment_grid_slice_dimension[
						IA_CSS_COL_DIMENSION] <
				man_seq_info_desc->
				min_fragment_grid_slice_dimension[
						IA_CSS_COL_DIMENSION]);

			/* slice dimension row */
			invalid_flag = invalid_flag ||
				(term_seq_info_desc->
				fragment_grid_slice_dimension[
						IA_CSS_ROW_DIMENSION] >
				man_seq_info_desc->
				max_fragment_grid_slice_dimension[
						IA_CSS_ROW_DIMENSION]);
			invalid_flag = invalid_flag ||
				(term_seq_info_desc->
				fragment_grid_slice_dimension[
						IA_CSS_ROW_DIMENSION] <
				man_seq_info_desc->
				min_fragment_grid_slice_dimension[
						IA_CSS_ROW_DIMENSION]);

			/* slice count column */
			invalid_flag = invalid_flag ||
				(term_seq_info_desc->
				fragment_grid_slice_count[
						IA_CSS_COL_DIMENSION] >
				man_seq_info_desc->
				max_fragment_grid_slice_count[
						IA_CSS_COL_DIMENSION]);
			invalid_flag = invalid_flag ||
				(term_seq_info_desc->
				fragment_grid_slice_count[
						IA_CSS_COL_DIMENSION] <
				man_seq_info_desc->
				min_fragment_grid_slice_count[
						IA_CSS_COL_DIMENSION]);

			/* slice count row */
			invalid_flag = invalid_flag ||
				(term_seq_info_desc->
				fragment_grid_slice_count[
						IA_CSS_ROW_DIMENSION] >
				man_seq_info_desc->
				max_fragment_grid_slice_count[
						IA_CSS_ROW_DIMENSION]);
			invalid_flag = invalid_flag ||
				(term_seq_info_desc->
				fragment_grid_slice_count[
						IA_CSS_ROW_DIMENSION] <
				man_seq_info_desc->
				min_fragment_grid_slice_count[
						IA_CSS_ROW_DIMENSION]);

			/* decimation factor column */
			invalid_flag = invalid_flag ||
				(term_seq_info_desc->
				fragment_grid_point_decimation_factor[
						IA_CSS_COL_DIMENSION] >
				man_seq_info_desc->
				max_fragment_grid_point_decimation_factor[
						IA_CSS_COL_DIMENSION]);
			invalid_flag = invalid_flag ||
				(term_seq_info_desc->
				fragment_grid_point_decimation_factor[
						IA_CSS_COL_DIMENSION] <
				man_seq_info_desc->
				min_fragment_grid_point_decimation_factor[
						IA_CSS_COL_DIMENSION]);

			/* decimation factor row */
			invalid_flag = invalid_flag ||
				(term_seq_info_desc->
				fragment_grid_point_decimation_factor[
						IA_CSS_ROW_DIMENSION] >
				man_seq_info_desc->
				max_fragment_grid_point_decimation_factor[
						IA_CSS_ROW_DIMENSION]);
			invalid_flag = invalid_flag ||
				(term_seq_info_desc->
				fragment_grid_point_decimation_factor[
						IA_CSS_ROW_DIMENSION] <
				man_seq_info_desc->
				min_fragment_grid_point_decimation_factor[
						IA_CSS_ROW_DIMENSION]);

			/* index column */
			invalid_flag = invalid_flag ||
				(term_seq_info_desc->
				fragment_grid_overlay_pixel_topleft_index[
						IA_CSS_COL_DIMENSION] >
				man_seq_info_desc->
				max_fragment_grid_overlay_pixel_topleft_index[
						IA_CSS_COL_DIMENSION]);
			invalid_flag = invalid_flag ||
				(term_seq_info_desc->
				fragment_grid_overlay_pixel_topleft_index[
						IA_CSS_COL_DIMENSION] <
				man_seq_info_desc->
				min_fragment_grid_overlay_pixel_topleft_index[
						IA_CSS_COL_DIMENSION]);

			/* index row */
			invalid_flag = invalid_flag ||
				(term_seq_info_desc->
				fragment_grid_overlay_pixel_topleft_index[
						IA_CSS_ROW_DIMENSION] >
				man_seq_info_desc->
				max_fragment_grid_overlay_pixel_topleft_index[
						IA_CSS_ROW_DIMENSION]);
			invalid_flag = invalid_flag ||
				(term_seq_info_desc->
				fragment_grid_overlay_pixel_topleft_index[
						IA_CSS_ROW_DIMENSION] <
				man_seq_info_desc->
				min_fragment_grid_overlay_pixel_topleft_index[
						IA_CSS_ROW_DIMENSION]);

			/* dimension column */
			invalid_flag = invalid_flag ||
				(term_seq_info_desc->
				fragment_grid_overlay_pixel_dimension[
						IA_CSS_COL_DIMENSION] >
				man_seq_info_desc->
				max_fragment_grid_overlay_pixel_dimension[
						IA_CSS_COL_DIMENSION]);
			invalid_flag = invalid_flag ||
				(term_seq_info_desc->
				fragment_grid_overlay_pixel_dimension[
						IA_CSS_COL_DIMENSION] <
				man_seq_info_desc->
				min_fragment_grid_overlay_pixel_dimension[
						IA_CSS_COL_DIMENSION]);

			/* dimension column */
			invalid_flag = invalid_flag ||
				(term_seq_info_desc->
				fragment_grid_overlay_pixel_dimension[
						IA_CSS_ROW_DIMENSION] >
				man_seq_info_desc->
				max_fragment_grid_overlay_pixel_dimension[
						IA_CSS_ROW_DIMENSION]);
			invalid_flag = invalid_flag ||
				(term_seq_info_desc->
				fragment_grid_overlay_pixel_dimension[
						IA_CSS_ROW_DIMENSION] <
				man_seq_info_desc->
				min_fragment_grid_overlay_pixel_dimension[
						IA_CSS_ROW_DIMENSION]);
		}
	}
	if (invalid_flag == true) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_is_program_terminal_valid(): validation failed\n");
		/* TODO: program terminal parameters not correctly defined,
		 * disable validation result until issues has been solved
		 */
		return true;
	}
	return (!invalid_flag);

EXIT:
IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
		"ia_css_is_program_terminal_valid() invalid argument\n");
return false;

}

STORAGE_CLASS_INLINE bool ia_css_is_sliced_terminal_valid(
	const ia_css_terminal_t *terminal,
	const ia_css_terminal_manifest_t *terminal_manifest,
	const uint16_t nof_fragments)
{
	bool invalid_flag = false;
	uint16_t frag_idx;

	uint16_t slice_idx, section_idx;

	const ia_css_sliced_param_terminal_t *sliced_term =
		(const ia_css_sliced_param_terminal_t *)terminal;
	const ia_css_sliced_param_terminal_manifest_t *sliced_term_man =
		(const ia_css_sliced_param_terminal_manifest_t *)
		terminal_manifest;

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_is_sliced_terminal_valid enter\n");

	for (frag_idx = 0; frag_idx < nof_fragments; frag_idx++) {
		const ia_css_fragment_slice_desc_t *fragment_slice_desc =
			ia_css_sliced_param_terminal_get_fragment_slice_desc(
				sliced_term, frag_idx);

		verifjmpexit(fragment_slice_desc != NULL);

		for (slice_idx = 0;
			slice_idx < fragment_slice_desc->slice_count;
			slice_idx++) {
			for (section_idx = 0;
				section_idx <
				sliced_term_man->sliced_param_section_count;
				section_idx++) {
				const
				ia_css_sliced_param_manifest_section_desc_t *
					slice_man_section_desc;
				const ia_css_slice_param_section_desc_t *
					slice_section_desc;

				slice_man_section_desc =
		ia_css_sliced_param_terminal_manifest_get_sliced_prm_sct_desc(
						sliced_term_man, section_idx);
				slice_section_desc =
		ia_css_sliced_param_terminal_get_slice_param_section_desc(
						sliced_term, frag_idx,
						slice_idx, section_idx,
						sliced_term_man->
						sliced_param_section_count);
				verifjmpexit(slice_man_section_desc != NULL);
				verifjmpexit(slice_section_desc != NULL);

				invalid_flag = invalid_flag ||
					(slice_section_desc->mem_size >
					slice_man_section_desc->max_mem_size);
			}
		}
	}
	return (!invalid_flag);

EXIT:
IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
		"ia_css_is_sliced_terminal_valid() invalid argument\n");
return false;

}

bool ia_css_is_terminal_valid(
	const ia_css_terminal_t	*terminal,
	const ia_css_terminal_manifest_t *terminal_manifest)
{
	bool is_valid = false;
	uint16_t nof_fragments;
	ia_css_terminal_type_t terminal_type;

	verifjmpexit(NULL != terminal);
	verifjmpexit(NULL != terminal_manifest);

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_is_terminal_valid enter\n");

	nof_fragments = ia_css_data_terminal_get_fragment_count(
				(const ia_css_data_terminal_t *)terminal);
	terminal_type = ia_css_terminal_get_type(terminal);

	switch (terminal_type) {
	case IA_CSS_TERMINAL_TYPE_DATA_IN:
	case IA_CSS_TERMINAL_TYPE_DATA_OUT:
		is_valid = ia_css_is_data_terminal_valid(terminal,
				terminal_manifest, nof_fragments);
		break;
	case IA_CSS_TERMINAL_TYPE_PROGRAM:
		is_valid = ia_css_is_program_terminal_valid(terminal,
				terminal_manifest, nof_fragments);
		break;
	case IA_CSS_TERMINAL_TYPE_PARAM_CACHED_IN:
	case IA_CSS_TERMINAL_TYPE_PARAM_CACHED_OUT:
	case IA_CSS_TERMINAL_TYPE_PARAM_SPATIAL_IN:
	case IA_CSS_TERMINAL_TYPE_PARAM_SPATIAL_OUT:
		/* Nothing to be validated for cached and spatial
		*  parameters, return valid
		*/
		is_valid = true;
		break;
	case IA_CSS_TERMINAL_TYPE_PARAM_SLICED_IN:
	case IA_CSS_TERMINAL_TYPE_PARAM_SLICED_OUT:
		is_valid = ia_css_is_sliced_terminal_valid(terminal,
				terminal_manifest, nof_fragments);
		break;
	default:
		/* Terminal type unknown, return invalid */
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, WARNING,
			"ia_css_is_terminal_valid() Terminal type %x unknown\n",
			(int)terminal_type);
		is_valid = false;
		break;
	}

	/* TODO: to be removed once all PGs pass validation */
	if (is_valid == false) {
		IA_CSS_TRACE_1(PSYSAPI_DYNAMIC, INFO,
			"ia_css_is_terminal_valid(): type: %d validation failed\n",
			terminal_type);
	}
	return is_valid;

EXIT:
	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
		"ia_css_is_terminal_valid() invalid argument\n");
	return false;
}
