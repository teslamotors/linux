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


#include <ia_css_psys_terminal_manifest.h>

/* Data object types on the terminals */
#include <ia_css_program_group_data.h>
/* for ia_css_kernel_bitmap_t, ia_css_kernel_bitmap_clear, ia_css_... */
#include <ia_css_kernel_bitmap.h>

#include "ia_css_psys_program_group_private.h"
#include "ia_css_terminal_manifest.h"
#include "ia_css_terminal_manifest_types.h"

#include <error_support.h>
#include <print_support.h>
#include <misc_support.h>
#include "ia_css_psys_static_trace.h"

/* We need to refactor those files in order to build in the firmware only
   what is needed, switches are put current to workaround compilation problems
   in the firmware (for example lack of uint64_t support)
   supported in the firmware
  */
#if !defined(__HIVECC)
static const char *terminal_type_strings[IA_CSS_N_TERMINAL_TYPES + 1] = {
	"IA_CSS_TERMINAL_TYPE_DATA_IN",
	"IA_CSS_TERMINAL_TYPE_DATA_OUT",
	"IA_CSS_TERMINAL_TYPE_PARAM_STREAM",
	/**< Type 1-5 parameter input */
	"IA_CSS_TERMINAL_TYPE_PARAM_CACHED_IN",
	/**< Type 1-5 parameter output */
	"IA_CSS_TERMINAL_TYPE_PARAM_CACHED_OUT",
	/**< Represent the new type of terminal for
	 * the "spatial dependent parameters", when params go in
	 */
	"IA_CSS_TERMINAL_TYPE_PARAM_SPATIAL_IN",
	/**< Represent the new type of terminal for
	 * the "spatial dependent parameters", when params go out
	 */
	"IA_CSS_TERMINAL_TYPE_PARAM_SPATIAL_OUT",
	/**< Represent the new type of terminal for
	 * the explicit slicing, when params go in
	 */
	"IA_CSS_TERMINAL_TYPE_PARAM_SLICED_IN",
	/**< Represent the new type of terminal for
	 * the explicit slicing, when params go out
	 */
	"IA_CSS_TERMINAL_TYPE_PARAM_SLICED_OUT",
	/**< State (private data) input */
	"IA_CSS_TERMINAL_TYPE_STATE_IN",
	/**< State (private data) output */
	"IA_CSS_TERMINAL_TYPE_STATE_OUT",
	"IA_CSS_TERMINAL_TYPE_PROGRAM",
	"UNDEFINED_TERMINAL_TYPE"};

#endif

bool ia_css_is_terminal_manifest_spatial_parameter_terminal(
	const ia_css_terminal_manifest_t *manifest)
{
	ia_css_terminal_type_t terminal_type;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_is_terminal_manifest_parameter_terminal(): enter:\n");

	terminal_type = ia_css_terminal_manifest_get_type(manifest);

	return ((terminal_type == IA_CSS_TERMINAL_TYPE_PARAM_SPATIAL_IN) ||
		(terminal_type == IA_CSS_TERMINAL_TYPE_PARAM_SPATIAL_OUT));
}

bool ia_css_is_terminal_manifest_program_terminal(
	const ia_css_terminal_manifest_t *manifest)
{
	ia_css_terminal_type_t terminal_type;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_is_terminal_manifest_parameter_terminal(): enter:\n");

	terminal_type = ia_css_terminal_manifest_get_type(manifest);

	return (terminal_type == IA_CSS_TERMINAL_TYPE_PROGRAM);
}

bool ia_css_is_terminal_manifest_parameter_terminal(
	const ia_css_terminal_manifest_t *manifest)
{
	/* will return an error value on error */
	ia_css_terminal_type_t terminal_type;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_is_terminal_manifest_parameter_terminal(): enter:\n");

	terminal_type = ia_css_terminal_manifest_get_type(manifest);

	return (terminal_type == IA_CSS_TERMINAL_TYPE_PARAM_CACHED_IN ||
		terminal_type == IA_CSS_TERMINAL_TYPE_PARAM_CACHED_OUT);
}

bool ia_css_is_terminal_manifest_data_terminal(
	const ia_css_terminal_manifest_t *manifest)
{
	/* will return an error value on error */
	ia_css_terminal_type_t terminal_type;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_is_terminal_manifest_data_terminal(): enter:\n");

	terminal_type = ia_css_terminal_manifest_get_type(manifest);

	return ((terminal_type == IA_CSS_TERMINAL_TYPE_DATA_IN) ||
		(terminal_type == IA_CSS_TERMINAL_TYPE_DATA_OUT));
}

bool ia_css_is_terminal_manifest_sliced_terminal(
	const ia_css_terminal_manifest_t *manifest)
{
	ia_css_terminal_type_t terminal_type;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_is_terminal_manifest_sliced_terminal(): enter:\n");

	terminal_type = ia_css_terminal_manifest_get_type(manifest);

	return ((terminal_type == IA_CSS_TERMINAL_TYPE_PARAM_SLICED_IN) ||
		(terminal_type == IA_CSS_TERMINAL_TYPE_PARAM_SLICED_OUT));
}

size_t ia_css_terminal_manifest_get_size(
	const ia_css_terminal_manifest_t *manifest)
{
	size_t size = 0;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_terminal_manifest_get_size(): enter:\n");

	if (manifest != NULL) {
		size = manifest->size;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
			"ia_css_terminal_manifest_get_size: invalid argument\n");
	}
	return size;
}

ia_css_terminal_type_t ia_css_terminal_manifest_get_type(
	const ia_css_terminal_manifest_t *manifest)
{
	ia_css_terminal_type_t terminal_type = IA_CSS_N_TERMINAL_TYPES;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_terminal_manifest_get_type(): enter:\n");

	if (manifest != NULL) {
		terminal_type = manifest->terminal_type;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
			"ia_css_terminal_manifest_get_type: invalid argument\n");
	}
	return terminal_type;
}

int ia_css_terminal_manifest_set_type(
	ia_css_terminal_manifest_t *manifest,
	const ia_css_terminal_type_t terminal_type)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_terminal_manifest_set_type(): enter:\n");

	if (manifest != NULL) {
		manifest->terminal_type = terminal_type;
		retval = 0;
	} else {
		IA_CSS_TRACE_1(PSYSAPI_STATIC, ERROR,
			"ia_css_terminal_manifest_set_type failed (%i)\n",
			retval);
	}
	return retval;
}

int ia_css_terminal_manifest_set_ID(
	ia_css_terminal_manifest_t *manifest,
	const ia_css_terminal_ID_t ID)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_terminal_manifest_set_ID(): enter:\n");

	if (manifest != NULL) {
		manifest->ID = ID;
		retval = 0;
	} else {
		IA_CSS_TRACE_1(PSYSAPI_STATIC, ERROR,
			"ia_css_terminal_manifest_set_ID failed (%i)\n",
			retval);
	}
	return retval;
}

ia_css_terminal_ID_t ia_css_terminal_manifest_get_ID(
	const ia_css_terminal_manifest_t *manifest)
{
	ia_css_terminal_ID_t retval;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_terminal_manifest_get_ID(): enter:\n");

	if (manifest != NULL) {
		retval = manifest->ID;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, ERROR,
			"ia_css_terminal_manifest_get_ID failed\n");
		retval = IA_CSS_TERMINAL_INVALID_ID;
	}
	return retval;
}

ia_css_program_group_manifest_t *ia_css_terminal_manifest_get_parent(
	const ia_css_terminal_manifest_t *manifest)
{
	ia_css_program_group_manifest_t	*parent = NULL;
	char *base;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_terminal_manifest_get_parent(): enter:\n");

	verifexit(manifest != NULL);

	base = (char *)((char *)manifest + manifest->parent_offset);

	parent = (ia_css_program_group_manifest_t *)(base);
EXIT:
	return parent;
}

int ia_css_terminal_manifest_set_parent_offset(
	ia_css_terminal_manifest_t *manifest,
	int32_t terminal_offset)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_terminal_manifest_set_parent_offset(): enter:\n");

	verifexit(manifest != NULL);

	/* parent is at negative offset away from current terminal offset*/
	manifest->parent_offset = -terminal_offset;

	retval = 0;
EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_STATIC, ERROR,
			"ia_css_terminal_manifest_set_parent_offset failed (%i)\n",
			retval);
	}
	return retval;
}

ia_css_frame_format_bitmap_t
ia_css_data_terminal_manifest_get_frame_format_bitmap(
	const ia_css_data_terminal_manifest_t *manifest)
{
	ia_css_frame_format_bitmap_t bitmap = 0;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_data_terminal_manifest_get_frame_format_bitmap(): enter:\n");

	if (manifest != NULL) {
		bitmap = manifest->frame_format_bitmap;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
			"ia_css_data_terminal_manifest_get_frame_format_bitmap invalid argument\n");
	}
	return bitmap;
}

int ia_css_data_terminal_manifest_set_frame_format_bitmap(
	ia_css_data_terminal_manifest_t *manifest,
	ia_css_frame_format_bitmap_t bitmap)
{
	int ret = -1;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_data_terminal_manifest_set_frame_format_bitmap(): enter:\n");

	if (manifest != NULL) {
		manifest->frame_format_bitmap = bitmap;
		ret = 0;
	} else {
		IA_CSS_TRACE_1(PSYSAPI_STATIC, ERROR,
			"ia_css_data_terminal_manifest_set_frame_format_bitmap failed (%i)\n",
			ret);
	}

	return ret;
}

bool ia_css_data_terminal_manifest_can_support_compression(
	const ia_css_data_terminal_manifest_t *manifest)
{
	bool compression_support = false;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_data_terminal_manifest_get_compression_support(): enter:\n");

	if (manifest != NULL) {
		/* compression_support is used boolean encoded in uint8_t.
		 * So we only need to check
		 * if this is non-zero
		 */
		compression_support = (manifest->compression_support != 0);
	} else {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, ERROR,
			"ia_css_data_terminal_manifest_can_support_compression invalid argument\n");
	}

	return compression_support;
}

int ia_css_data_terminal_manifest_set_compression_support(
	ia_css_data_terminal_manifest_t *manifest,
	bool compression_support)
{
	int ret = -1;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_data_terminal_manifest_set_compression_support(): enter:\n");

	if (manifest != NULL) {
		manifest->compression_support =
			(compression_support == true) ? 1 : 0;
		ret = 0;
	} else {
		IA_CSS_TRACE_1(PSYSAPI_STATIC, ERROR,
			"ia_css_data_terminal_manifest_set_compression_support failed (%i)\n",
			ret);
	}

	return ret;
}

ia_css_connection_bitmap_t ia_css_data_terminal_manifest_get_connection_bitmap(
	const ia_css_data_terminal_manifest_t *manifest)
{
	ia_css_connection_bitmap_t connection_bitmap = 0;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_data_terminal_manifest_get_connection_bitmap(): enter:\n");

	if (manifest != NULL) {
		connection_bitmap = manifest->connection_bitmap;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
			"ia_css_data_terminal_manifest_get_connection_bitmap invalid argument\n");
	}
	return connection_bitmap;
}

/* We need to refactor those files in order to build in the firmware only
   what is needed, switches are put current to workaround compilation problems
   in the firmware (for example lack of uint64_t support)
   supported in the firmware
  */
#if !defined(__HIVECC)
ia_css_kernel_bitmap_t ia_css_data_terminal_manifest_get_kernel_bitmap(
	const ia_css_data_terminal_manifest_t *manifest)
{
	ia_css_kernel_bitmap_t kernel_bitmap = 0;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_data_terminal_manifest_get_kernel_bitmap(): enter:\n");

	if (manifest != NULL) {
		kernel_bitmap = manifest->kernel_bitmap;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
			"ia_css_data_terminal_manifest_get_kernel_bitmap: invalid argument\n");
	}
	return kernel_bitmap;
}

int ia_css_data_terminal_manifest_set_kernel_bitmap(
	ia_css_data_terminal_manifest_t	*manifest,
	const ia_css_kernel_bitmap_t kernel_bitmap)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_data_terminal_manifest_set_kernel_bitmap(): enter:\n");

	if (manifest != NULL) {
		manifest->kernel_bitmap = kernel_bitmap;
		retval = 0;
	} else {
		IA_CSS_TRACE_1(PSYSAPI_STATIC, ERROR,
			"ia_css_data_terminal_manifest_set_kernel_bitmap: failed (%i)\n",
			retval);
	}

	return retval;
}

int ia_css_data_terminal_manifest_set_kernel_bitmap_unique(
	ia_css_data_terminal_manifest_t *manifest,
	const unsigned int index)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_data_terminal_manifest_set_kernel_bitmap_unique(): enter:\n");

	if (manifest != NULL) {
		ia_css_kernel_bitmap_t kernel_bitmap =
					ia_css_kernel_bitmap_clear();

		kernel_bitmap = ia_css_kernel_bitmap_set(kernel_bitmap, index);
		verifexit(kernel_bitmap != 0);
		verifexit(ia_css_data_terminal_manifest_set_kernel_bitmap(
				manifest, kernel_bitmap) == 0);
		retval = 0;
	}

EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_STATIC, ERROR,
			"ia_css_data_terminal_manifest_set_kernel_bitmap_unique failed (%i)\n",
			retval);
	}
	return retval;
}
#endif

int ia_css_data_terminal_manifest_set_min_size(
	ia_css_data_terminal_manifest_t	*manifest,
	const uint16_t min_size[IA_CSS_N_DATA_DIMENSION])
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_data_terminal_manifest_set_min_size(): enter:\n");

	verifexit(manifest != NULL);

	manifest->min_size[IA_CSS_COL_DIMENSION] =
		min_size[IA_CSS_COL_DIMENSION];
	manifest->min_size[IA_CSS_ROW_DIMENSION] =
		min_size[IA_CSS_ROW_DIMENSION];
	retval = 0;

EXIT:
	if (NULL == manifest) {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, ERROR,
			"ia_css_data_terminal_manifest_set_min_size: invalid argument\n");
	}
	return retval;
}

int ia_css_data_terminal_manifest_set_max_size(
	ia_css_data_terminal_manifest_t	*manifest,
	const uint16_t max_size[IA_CSS_N_DATA_DIMENSION])
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_data_terminal_manifest_set_max_size(): enter:\n");

	verifexit(manifest != NULL);

	manifest->max_size[IA_CSS_COL_DIMENSION] =
		max_size[IA_CSS_COL_DIMENSION];
	manifest->max_size[IA_CSS_ROW_DIMENSION] =
		max_size[IA_CSS_ROW_DIMENSION];
	retval = 0;

EXIT:
	if (NULL == manifest) {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, ERROR,
			"ia_css_data_terminal_manifest_set_max_size: invalid argument\n");
	}
	return retval;
}

int ia_css_data_terminal_manifest_get_min_size(
	const ia_css_data_terminal_manifest_t *manifest,
	uint16_t min_size[IA_CSS_N_DATA_DIMENSION])
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_data_terminal_manifest_get_min_size(): enter:\n");

	verifexit(manifest != NULL);

	min_size[IA_CSS_COL_DIMENSION] =
		manifest->min_size[IA_CSS_COL_DIMENSION];
	min_size[IA_CSS_ROW_DIMENSION] =
		manifest->min_size[IA_CSS_ROW_DIMENSION];
	retval = 0;

EXIT:
	if (NULL == manifest) {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, ERROR,
			"ia_css_data_terminal_manifest_get_min_size: invalid argument\n");
	}
	return retval;
}

int ia_css_data_terminal_manifest_get_max_size(
	const ia_css_data_terminal_manifest_t *manifest,
	uint16_t max_size[IA_CSS_N_DATA_DIMENSION])
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_data_terminal_manifest_get_max_size(): enter:\n");

	verifexit(manifest != NULL);

	max_size[IA_CSS_COL_DIMENSION] =
		manifest->max_size[IA_CSS_COL_DIMENSION];
	max_size[IA_CSS_ROW_DIMENSION] =
		manifest->max_size[IA_CSS_ROW_DIMENSION];
	retval = 0;

EXIT:
	if (NULL == manifest) {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, ERROR,
			"ia_css_data_terminal_manifest_get_max_size: invalid argument\n");
	}
	return retval;
}

int ia_css_data_terminal_manifest_set_min_fragment_size(
	ia_css_data_terminal_manifest_t	*manifest,
	const uint16_t min_size[IA_CSS_N_DATA_DIMENSION])
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_data_terminal_manifest_set_min_fragment_size(): enter:\n");

	verifexit(manifest != NULL);

	manifest->min_fragment_size[IA_CSS_COL_DIMENSION] =
		min_size[IA_CSS_COL_DIMENSION];
	manifest->min_fragment_size[IA_CSS_ROW_DIMENSION] =
		min_size[IA_CSS_ROW_DIMENSION];
	retval = 0;

EXIT:
	if (NULL == manifest) {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, ERROR,
			"ia_css_data_terminal_manifest_set_min_fragment_size invalid argument\n");
	}
	return retval;
}

int ia_css_data_terminal_manifest_set_max_fragment_size(
	ia_css_data_terminal_manifest_t	*manifest,
	const uint16_t max_size[IA_CSS_N_DATA_DIMENSION])
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_data_terminal_manifest_set_max_fragment_size(): enter:\n");

	verifexit(manifest != NULL);

	manifest->max_fragment_size[IA_CSS_COL_DIMENSION] =
		max_size[IA_CSS_COL_DIMENSION];
	manifest->max_fragment_size[IA_CSS_ROW_DIMENSION] =
		max_size[IA_CSS_ROW_DIMENSION];
	retval = 0;

EXIT:
	if (NULL == manifest) {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, ERROR,
			"ia_css_data_terminal_manifest_set_max_fragment_size invalid argument\n");
	}
	return retval;
}

int ia_css_data_terminal_manifest_get_min_fragment_size(
	const ia_css_data_terminal_manifest_t *manifest,
	uint16_t min_size[IA_CSS_N_DATA_DIMENSION])
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_data_terminal_manifest_get_min_fragment_size(): enter:\n");

	verifexit(manifest != NULL);

	min_size[IA_CSS_COL_DIMENSION] =
		manifest->min_fragment_size[IA_CSS_COL_DIMENSION];
	min_size[IA_CSS_ROW_DIMENSION] =
		manifest->min_fragment_size[IA_CSS_ROW_DIMENSION];
	retval = 0;

EXIT:
	if (NULL == manifest) {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, ERROR,
			"ia_css_data_terminal_manifest_get_min_fragment_size invalid argument\n");
	}
	return retval;
}

int ia_css_data_terminal_manifest_get_max_fragment_size(
	const ia_css_data_terminal_manifest_t *manifest,
	uint16_t max_size[IA_CSS_N_DATA_DIMENSION])
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_data_terminal_manifest_get_max_fragment_size(): enter:\n");

	verifexit(manifest != NULL);

	max_size[IA_CSS_COL_DIMENSION] =
		manifest->max_fragment_size[IA_CSS_COL_DIMENSION];
	max_size[IA_CSS_ROW_DIMENSION] =
		manifest->max_fragment_size[IA_CSS_ROW_DIMENSION];
	retval = 0;

EXIT:
	if (NULL == manifest) {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, ERROR,
			"ia_css_data_terminal_manifest_get_max_fragment_size invalid argument\n");
	}
	return retval;
}

/* We need to refactor those files in order to build in the firmware only
   what is needed, switches are put current to workaround compilation problems
   in the firmware (for example lack of uint64_t support)
   supported in the firmware
  */
#if !defined(__HIVECC)

int ia_css_terminal_manifest_print(
	const ia_css_terminal_manifest_t *manifest,
	void *fid)
{
	int retval = -1;
	ia_css_terminal_type_t terminal_type =
		ia_css_terminal_manifest_get_type(manifest);

	IA_CSS_TRACE_0(PSYSAPI_STATIC, INFO,
		"ia_css_terminal_manifest_print(): enter:\n");

	verifexit(manifest != NULL);
	NOT_USED(fid);

	IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO, "sizeof(manifest) = %d\n",
		(int)ia_css_terminal_manifest_get_size(manifest));

	PRINT("typeof(manifest) = %s\n", terminal_type_strings[terminal_type]);

	if (terminal_type == IA_CSS_TERMINAL_TYPE_PARAM_CACHED_IN ||
		terminal_type == IA_CSS_TERMINAL_TYPE_PARAM_CACHED_OUT) {
		ia_css_param_terminal_manifest_t *pterminal_manifest =
			(ia_css_param_terminal_manifest_t *)manifest;
		uint16_t section_count =
			pterminal_manifest->param_manifest_section_desc_count;
		int	i;

		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"sections(manifest) = %d\n", (int)section_count);
		for (i = 0; i < section_count; i++) {
			const ia_css_param_manifest_section_desc_t *manifest =
		ia_css_param_terminal_manifest_get_param_manifest_section_desc(
			pterminal_manifest, i);
			verifjmpexit(manifest != NULL);
			IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
				"kernel_id = %d\n", (int)manifest->kernel_id);
			IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
				"mem_type_id = %d\n",
				(int)manifest->mem_type_id);
			IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
				"max_mem_size = %d\n",
				(int)manifest->max_mem_size);
			IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
				"region_id = %d\n",
				(int)manifest->region_id);
		}
	} else if (terminal_type == IA_CSS_TERMINAL_TYPE_PARAM_SLICED_IN ||
		terminal_type == IA_CSS_TERMINAL_TYPE_PARAM_SLICED_OUT) {
		ia_css_sliced_param_terminal_manifest_t
		*sliced_terminal_manifest =
			(ia_css_sliced_param_terminal_manifest_t *)manifest;
		uint32_t kernel_id;
		uint16_t section_count;
		uint16_t section_idx;

		kernel_id = sliced_terminal_manifest->kernel_id;
		section_count =
			sliced_terminal_manifest->sliced_param_section_count;

		NOT_USED(kernel_id);

		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"kernel_id = %d\n", (int)kernel_id);
		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"section_count = %d\n", (int)section_count);

		for (section_idx = 0; section_idx < section_count;
			section_idx++) {
			ia_css_sliced_param_manifest_section_desc_t
				*sliced_param_manifest_section_desc;

			IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
				"section %d\n", (int)section_idx);
			sliced_param_manifest_section_desc =
		ia_css_sliced_param_terminal_manifest_get_sliced_prm_sct_desc(
				sliced_terminal_manifest, section_idx);
			verifjmpexit(sliced_param_manifest_section_desc !=
				NULL);
			IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
				"mem_type_id = %d\n",
			(int)sliced_param_manifest_section_desc->mem_type_id);
			IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
				"region_id = %d\n",
			(int)sliced_param_manifest_section_desc->region_id);
			IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
				"max_mem_size = %d\n",
			(int)sliced_param_manifest_section_desc->max_mem_size);
		}
	} else if (terminal_type == IA_CSS_TERMINAL_TYPE_PROGRAM) {
		ia_css_program_terminal_manifest_t *program_terminal_manifest =
			(ia_css_program_terminal_manifest_t *)manifest;
		uint32_t sequencer_info_kernel_id;
		uint16_t max_kernel_fragment_sequencer_command_desc;
		uint16_t kernel_fragment_sequencer_info_manifest_info_count;
		uint16_t seq_info_idx;

		sequencer_info_kernel_id =
			program_terminal_manifest->sequencer_info_kernel_id;
		max_kernel_fragment_sequencer_command_desc =
			program_terminal_manifest->
			max_kernel_fragment_sequencer_command_desc;
		kernel_fragment_sequencer_info_manifest_info_count =
			program_terminal_manifest->
			kernel_fragment_sequencer_info_manifest_info_count;

		NOT_USED(sequencer_info_kernel_id);
		NOT_USED(max_kernel_fragment_sequencer_command_desc);

		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"sequencer_info_kernel_id = %d\n",
			(int)sequencer_info_kernel_id);
		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"max_kernel_fragment_sequencer_command_desc = %d\n",
			(int)max_kernel_fragment_sequencer_command_desc);
		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"kernel_fragment_sequencer_info_manifest_info_count = %d\n",
			(int)
			kernel_fragment_sequencer_info_manifest_info_count);

		for (seq_info_idx = 0; seq_info_idx <
			kernel_fragment_sequencer_info_manifest_info_count;
				seq_info_idx++) {
			ia_css_kernel_fragment_sequencer_info_manifest_desc_t
				*sequencer_info_manifest_desc;

			IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
				"sequencer info %d\n", (int)seq_info_idx);
			sequencer_info_manifest_desc =
		ia_css_program_terminal_manifest_get_kernel_frgmnt_seq_info_desc
				(program_terminal_manifest, seq_info_idx);
			verifjmpexit(sequencer_info_manifest_desc != NULL);
			IA_CSS_TRACE_2(PSYSAPI_STATIC, INFO,
				"min_fragment_grid_slice_dimension[] = {%d, %d}\n",
				(int)sequencer_info_manifest_desc->
				min_fragment_grid_slice_dimension[
						IA_CSS_COL_DIMENSION],
				(int)sequencer_info_manifest_desc->
				min_fragment_grid_slice_dimension[
						IA_CSS_ROW_DIMENSION]);
			IA_CSS_TRACE_2(PSYSAPI_STATIC, INFO,
				"max_fragment_grid_slice_dimension[] = {%d, %d}\n",
				(int)sequencer_info_manifest_desc->
				max_fragment_grid_slice_dimension[
						IA_CSS_COL_DIMENSION],
				(int)sequencer_info_manifest_desc->
				max_fragment_grid_slice_dimension[
						IA_CSS_ROW_DIMENSION]);
			IA_CSS_TRACE_2(PSYSAPI_STATIC, INFO,
				"min_fragment_grid_slice_count[] = {%d, %d}\n",
				(int)sequencer_info_manifest_desc->
				min_fragment_grid_slice_count[
						IA_CSS_COL_DIMENSION],
				(int)sequencer_info_manifest_desc->
				min_fragment_grid_slice_count[
						IA_CSS_ROW_DIMENSION]);
			IA_CSS_TRACE_2(PSYSAPI_STATIC, INFO,
				"max_fragment_grid_slice_count[] = {%d, %d}\n",
				(int)sequencer_info_manifest_desc->
				max_fragment_grid_slice_count[
						IA_CSS_COL_DIMENSION],
				(int)sequencer_info_manifest_desc->
				max_fragment_grid_slice_count[
						IA_CSS_ROW_DIMENSION]);
			IA_CSS_TRACE_2(PSYSAPI_STATIC, INFO,
				"min_fragment_grid_point_decimation_factor[] = {%d, %d}\n",
				(int)sequencer_info_manifest_desc->
				min_fragment_grid_point_decimation_factor[
						IA_CSS_COL_DIMENSION],
				(int)sequencer_info_manifest_desc->
				min_fragment_grid_point_decimation_factor[
						IA_CSS_ROW_DIMENSION]);
			IA_CSS_TRACE_2(PSYSAPI_STATIC, INFO,
				"max_fragment_grid_point_decimation_factor[] = {%d, %d}\n",
				(int)sequencer_info_manifest_desc->
				max_fragment_grid_point_decimation_factor[
						IA_CSS_COL_DIMENSION],
				(int)sequencer_info_manifest_desc->
				max_fragment_grid_point_decimation_factor[
						IA_CSS_ROW_DIMENSION]);
			IA_CSS_TRACE_2(PSYSAPI_STATIC, INFO,
				"min_fragment_grid_overlay_on_pixel_topleft_index[] = {%d, %d}\n",
				(int)sequencer_info_manifest_desc->
			min_fragment_grid_overlay_pixel_topleft_index[
						IA_CSS_COL_DIMENSION],
				(int)sequencer_info_manifest_desc->
			min_fragment_grid_overlay_pixel_topleft_index[
						IA_CSS_ROW_DIMENSION]);
			IA_CSS_TRACE_2(PSYSAPI_STATIC, INFO,
				"max_fragment_grid_overlay_on_pixel_topleft_index[] = {%d, %d}\n",
				(int)sequencer_info_manifest_desc->
			max_fragment_grid_overlay_pixel_topleft_index[
						IA_CSS_COL_DIMENSION],
				(int)sequencer_info_manifest_desc->
			max_fragment_grid_overlay_pixel_topleft_index[
						IA_CSS_ROW_DIMENSION]);
			IA_CSS_TRACE_2(PSYSAPI_STATIC, INFO,
				"min_fragment_grid_overlay_on_pixel_dimension[] = {%d, %d}\n",
				(int)sequencer_info_manifest_desc->
				min_fragment_grid_overlay_pixel_dimension[
						IA_CSS_COL_DIMENSION],
				(int)sequencer_info_manifest_desc->
				min_fragment_grid_overlay_pixel_dimension[
						IA_CSS_ROW_DIMENSION]);
			IA_CSS_TRACE_2(PSYSAPI_STATIC, INFO,
				"max_fragment_grid_overlay_on_pixel_dimension[] = {%d, %d}\n",
				(int)sequencer_info_manifest_desc->
				max_fragment_grid_overlay_pixel_dimension[
						IA_CSS_COL_DIMENSION],
				(int)sequencer_info_manifest_desc->
				max_fragment_grid_overlay_pixel_dimension[
						IA_CSS_ROW_DIMENSION]);
		}
	} else if (terminal_type < IA_CSS_N_TERMINAL_TYPES) {
		ia_css_data_terminal_manifest_t *dterminal_manifest =
			(ia_css_data_terminal_manifest_t *)manifest;
		int i;

		NOT_USED(dterminal_manifest);

		verifexit(ia_css_kernel_bitmap_print(
			ia_css_data_terminal_manifest_get_kernel_bitmap(
				dterminal_manifest), fid) == 0);
		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"formats(manifest) = %04x\n",
		(int)ia_css_data_terminal_manifest_get_frame_format_bitmap(
			dterminal_manifest));
		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"connection(manifest) = %04x\n",
		(int)ia_css_data_terminal_manifest_get_connection_bitmap(
			dterminal_manifest));
		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"dependent(manifest) = %d\n",
			(int)dterminal_manifest->terminal_dependency);

		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"\tmin_size[%d]   = {\n",
			IA_CSS_N_DATA_DIMENSION);
		for (i = 0; i < (int)IA_CSS_N_DATA_DIMENSION - 1; i++) {
			IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
				"\t\t%4d,\n", dterminal_manifest->min_size[i]);
		}
		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"\t\t%4d }\n", dterminal_manifest->min_size[i]);

		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"\tmax_size[%d]   = {\n", IA_CSS_N_DATA_DIMENSION);
		for (i = 0; i < (int)IA_CSS_N_DATA_DIMENSION - 1; i++) {
			IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"\t\t%4d,\n", dterminal_manifest->max_size[i]);
		}
		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"\t\t%4d }\n", dterminal_manifest->max_size[i]);

		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"\tmin_fragment_size[%d]   = {\n",
			IA_CSS_N_DATA_DIMENSION);
		for (i = 0; i < (int)IA_CSS_N_DATA_DIMENSION - 1; i++) {
			IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
				"\t\t%4d,\n",
				dterminal_manifest->min_fragment_size[i]);
		}
		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"\t\t%4d }\n",
			dterminal_manifest->min_fragment_size[i]);

		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"\tmax_fragment_size[%d]   = {\n",
			IA_CSS_N_DATA_DIMENSION);
		for (i = 0; i < (int)IA_CSS_N_DATA_DIMENSION - 1; i++) {
			IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
				"\t\t%4d,\n",
				dterminal_manifest->max_fragment_size[i]);
		}
		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"\t\t%4d }\n",
			dterminal_manifest->max_fragment_size[i]);
	}

	retval = 0;
EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_STATIC, ERROR,
			"ia_css_terminal_manifest_print failed (%i)\n",
			retval);
	}
	return retval;
}
#endif

