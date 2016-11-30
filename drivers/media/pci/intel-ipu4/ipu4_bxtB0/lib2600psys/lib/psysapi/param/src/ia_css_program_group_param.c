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

#include <ia_css_program_group_param.h>
#include <ia_css_program_group_param_private.h>
#include <ia_css_psys_manifest_types.h>
#include <ia_css_psys_program_group_manifest.h>
#include <error_support.h>
#include <misc_support.h>
#include <assert_support.h>
#include <type_support.h>
#include <print_support.h>

#include "ia_css_psys_param_trace.h"

static int
ia_css_terminal_param_init(ia_css_terminal_param_t *terminal_param,
			   uint32_t offset,
			   enum ia_css_frame_format_type frame_format_type);

static int
ia_css_program_param_init(ia_css_program_param_t *program_param,
				int32_t offset);

size_t ia_css_sizeof_program_group_param(
	const uint8_t program_count,
	const uint8_t terminal_count,
	const uint16_t fragment_count)
{
	size_t	size = 0;

	IA_CSS_TRACE_0(PSYSAPI_PARAM, VERBOSE,
		"ia_css_sizeof_program_group_param(): enter:\n");

	verifexit(program_count != 0, EINVAL);
	verifexit(terminal_count != 0, EINVAL);
	verifexit(fragment_count != 0, EINVAL);

	size += sizeof(ia_css_program_group_param_t);
	size += program_count * fragment_count * sizeof(ia_css_program_param_t);
	size += terminal_count * sizeof(ia_css_terminal_param_t);
EXIT:
	if (0 == program_count || 0 == terminal_count || 0 == fragment_count) {
		IA_CSS_TRACE_0(PSYSAPI_PARAM, WARNING,
			"ia_css_sizeof_program_group_param invalid argument\n");
	}
	return size;
}

size_t ia_css_program_group_param_get_size(
	const ia_css_program_group_param_t		*program_group_param)
{
	size_t	size = 0;

	IA_CSS_TRACE_0(PSYSAPI_PARAM, VERBOSE,
		       "ia_css_program_group_param_get_size(): enter:\n");

	if (program_group_param != NULL) {
		size = program_group_param->size;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_PARAM, WARNING,
		      "ia_css_program_group_param_get_size invalid argument\n");
	}
	return size;
}

size_t ia_css_program_param_get_size(
	const ia_css_program_param_t			*param)
{
	size_t	size = 0;

	IA_CSS_TRACE_0(PSYSAPI_PARAM, VERBOSE,
		       "ia_css_program_param_get_size(): enter:\n");

	if (param != NULL) {
		size = param->size;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_PARAM, WARNING,
			"ia_css_program_param_get_size invalid argument\n");
	}
	return size;
}

ia_css_program_param_t *ia_css_program_group_param_get_program_param(
	const ia_css_program_group_param_t *param,
	const int i)
{
	ia_css_program_param_t	*program_param = NULL;
	ia_css_program_param_t	*program_param_base;
	int program_count = 0;

	IA_CSS_TRACE_0(PSYSAPI_PARAM, VERBOSE,
		"ia_css_program_group_param_get_program_param(): enter:\n");

	verifexit(param != NULL, ENOBUFS);

	program_count =
		(int)ia_css_program_group_param_get_program_count(param);

	verifexit(i < program_count, EINVAL);

	program_param_base = (ia_css_program_param_t *)
			(((char *)param) + param->program_param_offset);

	program_param = &program_param_base[i];

EXIT:
	if (NULL == param || i >= program_count) {
		IA_CSS_TRACE_0(PSYSAPI_PARAM, WARNING,
			"ia_css_program_group_param_get_program_param invalid argument\n");
	}
	return program_param;
}

size_t ia_css_terminal_param_get_size(
	const ia_css_terminal_param_t			*param)
{
	size_t	size = 0;

	IA_CSS_TRACE_0(PSYSAPI_PARAM, VERBOSE,
		"ia_css_terminal_param_get_size(): enter:\n");

	if (param != NULL) {
		size = param->size;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_PARAM, WARNING,
			"ia_css_terminal_param_get_size invalid argument\n");
	}

	return size;
}

ia_css_terminal_param_t *ia_css_program_group_param_get_terminal_param(
	const ia_css_program_group_param_t		*param,
	const int		i)
{
	ia_css_terminal_param_t	*terminal_param = NULL;
	ia_css_terminal_param_t	*terminal_param_base;
	int program_count = 0;

	IA_CSS_TRACE_0(PSYSAPI_PARAM, VERBOSE,
		"ia_css_program_group_param_get_terminal_param(): enter:\n");

	verifexit(param != NULL, ENOBUFS);

	program_count =
		(int)ia_css_program_group_param_get_terminal_count(param);

	verifexit(i < program_count, EINVAL);

	terminal_param_base = (ia_css_terminal_param_t *)
			(((char *)param) + param->terminal_param_offset);
	terminal_param = &terminal_param_base[i];
EXIT:
	if (NULL == param || i >= program_count) {
		IA_CSS_TRACE_0(PSYSAPI_PARAM, WARNING,
			"ia_css_program_group_param_get_terminal_param invalid argument\n");
	}
	return terminal_param;
}

uint8_t ia_css_program_group_param_get_program_count(
	const ia_css_program_group_param_t		*param)
{
	uint8_t	program_count = 0;

	IA_CSS_TRACE_0(PSYSAPI_PARAM, VERBOSE,
		"ia_css_program_group_param_get_program_count(): enter:\n");

	if (param != NULL) {
		program_count = param->program_count;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_PARAM, WARNING,
			"ia_css_program_group_param_get_program_count invalid argument\n");
	}
	return program_count;
}

uint8_t ia_css_program_group_param_get_terminal_count(
	const ia_css_program_group_param_t		*param)
{
	uint8_t	terminal_count = 0;

	IA_CSS_TRACE_0(PSYSAPI_PARAM, VERBOSE,
		"ia_css_program_group_param_get_terminal_count(): enter:\n");

	if (param != NULL) {
		terminal_count = param->terminal_count;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_PARAM, WARNING,
			"ia_css_program_group_param_get_terminal_count invalid argument\n");
	}
	return terminal_count;
}

uint16_t ia_css_program_group_param_get_fragment_count(
	const ia_css_program_group_param_t		*param)
{
	uint8_t	fragment_count = 0;

	IA_CSS_TRACE_0(PSYSAPI_PARAM, VERBOSE,
		"ia_css_program_group_param_get_fragment_count(): enter:\n");

	if (param != NULL) {
		fragment_count = (uint8_t)param->fragment_count;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_PARAM, WARNING,
			"ia_css_program_group_param_get_fragment_count invalid argument\n");
	}
	return fragment_count;
}

int ia_css_program_group_param_set_kernel_enable_bitmap(
	ia_css_program_group_param_t	*param,
	const ia_css_kernel_bitmap_t	bitmap)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_PARAM, VERBOSE,
	     "ia_css_program_group_param_set_kernel_enable_bitmap(): enter:\n");

	if (param != NULL) {
		param->kernel_enable_bitmap = bitmap;
		retval = 0;
	} else {
		IA_CSS_TRACE_1(PSYSAPI_PARAM, ERROR,
			"ia_css_program_group_param_set_kernel_enable_bitmap failed (%i)\n",
			retval);
	}
	return retval;
}

ia_css_kernel_bitmap_t ia_css_program_group_param_get_kernel_enable_bitmap(
	const ia_css_program_group_param_t		*param)
{
	ia_css_kernel_bitmap_t	bitmap = 0;

	IA_CSS_TRACE_0(PSYSAPI_PARAM, VERBOSE,
	     "ia_css_program_group_param_get_kernel_enable_bitmap(): enter:\n");

	if (param != NULL) {
		bitmap = param->kernel_enable_bitmap;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_PARAM, WARNING,
			"ia_css_program_group_param_get_kernel_enable_bitmap invalid argument\n");
	}
	return bitmap;
}

int ia_css_program_param_set_kernel_enable_bitmap(
	ia_css_program_param_t		*program_param,
	const ia_css_kernel_bitmap_t	bitmap)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_PARAM, VERBOSE,
		"ia_css_program_param_set_kernel_enable_bitmap(): enter:\n");

	if (program_param != NULL) {
		program_param->kernel_enable_bitmap = bitmap;
		retval = 0;
	} else {
		IA_CSS_TRACE_1(PSYSAPI_PARAM, ERROR,
			"ia_css_program_param_set_kernel_enable_bitmap failed (%i)\n",
			retval);
	}
	return retval;
}

ia_css_kernel_bitmap_t ia_css_program_param_get_kernel_enable_bitmap(
	const ia_css_program_param_t	*program_param)
{
	ia_css_kernel_bitmap_t	bitmap = 0;
	char *base;

	IA_CSS_TRACE_0(PSYSAPI_PARAM, VERBOSE,
		"ia_css_program_param_get_kernel_enable_bitmap(): enter:\n");

	verifexit(program_param != NULL, EINVAL);
	verifexit(program_param->parent_offset != 0, EINVAL);

	base = (char *)((char *)program_param + program_param->parent_offset);
	bitmap = ((ia_css_program_group_param_t *)base)->kernel_enable_bitmap;
EXIT:
	if (NULL == program_param || 0 == program_param->parent_offset)	{
		IA_CSS_TRACE_0(PSYSAPI_PARAM, WARNING,
			"ia_css_program_param_get_kernel_enable_bitmap invalid argument\n");
	}
	return bitmap;
}

ia_css_kernel_bitmap_t ia_css_terminal_param_get_kernel_enable_bitmap(
	const ia_css_terminal_param_t			*param)
{
	ia_css_kernel_bitmap_t	bitmap = 0;
	char *base;

	IA_CSS_TRACE_0(PSYSAPI_PARAM, VERBOSE,
		"ia_css_terminal_param_get_kernel_enable_bitmap(): enter:\n");

	verifexit(param != NULL, EINVAL);
	verifexit(param->parent_offset != 0, EINVAL);

	base = (char *)((char *)param + param->parent_offset);
	bitmap = ((ia_css_program_group_param_t *)base)->kernel_enable_bitmap;
EXIT:
	if (NULL == param || 0 == param->parent_offset) {
		IA_CSS_TRACE_0(PSYSAPI_PARAM, WARNING,
			"ia_css_terminal_param_get_kernel_enable_bitmap invalid argument\n");
	}
	return bitmap;
}

ia_css_frame_format_type_t ia_css_terminal_param_get_frame_format_type(
	const ia_css_terminal_param_t	*param)
{
	ia_css_frame_format_type_t ft = IA_CSS_N_FRAME_FORMAT_TYPES;

	IA_CSS_TRACE_0(PSYSAPI_PARAM, VERBOSE,
		"ia_css_terminal_param_get_frame_format_type(): enter:\n");

	verifexit(param != NULL, EINVAL);

	ft = param->frame_format_type;
EXIT:
	if (NULL == param) {
		IA_CSS_TRACE_0(PSYSAPI_PARAM, WARNING,
			"ia_css_terminal_param_get_frame_format_type invalid argument\n");
	}
	return ft;
}

int ia_css_terminal_param_set_frame_format_type(
	ia_css_terminal_param_t		*param,
	const ia_css_frame_format_type_t	data_format_type)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_PARAM, VERBOSE,
		"ia_css_terminal_param_set_frame_format_type(): enter:\n");

	if (param != NULL) {
		param->frame_format_type = data_format_type;
		retval = 0;
	} else {
		IA_CSS_TRACE_1(PSYSAPI_PARAM, ERROR,
			"ia_css_terminal_param_set_frame_format_type failed (%i)\n",
			retval);
	}
	return retval;
}

uint8_t ia_css_terminal_param_get_bpp(
	const ia_css_terminal_param_t	*param)
{
	uint8_t bpp = 0;

	IA_CSS_TRACE_0(PSYSAPI_PARAM, VERBOSE,
		       "ia_css_terminal_param_get_bpp(): enter:\n");

	verifexit(param != NULL, EINVAL);

	bpp = param->bpp;

EXIT:
	if (NULL == param) {
		IA_CSS_TRACE_0(PSYSAPI_PARAM, WARNING,
			"ia_css_terminal_param_get_bpp invalid argument\n");
	}
	return bpp;
}

int ia_css_terminal_param_set_bpp(
	ia_css_terminal_param_t	*param,
	const uint8_t bpp)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_PARAM, VERBOSE,
		       "ia_css_terminal_param_set_bpp(): enter:\n");

	if (param != NULL) {
		param->bpp = bpp;
		retval = 0;
	} else {
		IA_CSS_TRACE_1(PSYSAPI_PARAM, ERROR,
			"ia_css_terminal_param_set_bpp failed (%i)\n", retval);
	}
	return retval;
}

int ia_css_terminal_param_get_dimensions(
	const ia_css_terminal_param_t	*param,
	uint16_t dimensions[IA_CSS_N_DATA_DIMENSION])
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_PARAM, VERBOSE,
		"ia_css_terminal_param_get_dimensions(): enter:\n");

	if (param != NULL) {
		dimensions[IA_CSS_COL_DIMENSION] =
			param->dimensions[IA_CSS_COL_DIMENSION];
		dimensions[IA_CSS_ROW_DIMENSION] =
			param->dimensions[IA_CSS_ROW_DIMENSION];
		retval = 0;
	} else {
		IA_CSS_TRACE_1(PSYSAPI_PARAM, ERROR,
		  "ia_css_terminal_param_get_dimensions failed (%i)\n", retval);
	}
	return retval;
}

int ia_css_terminal_param_set_dimensions(
	ia_css_terminal_param_t	*param,
	const uint16_t dimensions[IA_CSS_N_DATA_DIMENSION])
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_PARAM, VERBOSE,
		"ia_css_terminal_param_set_dimensions(): enter:\n");

	if (param != NULL) {
		param->dimensions[IA_CSS_COL_DIMENSION] =
				dimensions[IA_CSS_COL_DIMENSION];
		param->dimensions[IA_CSS_ROW_DIMENSION] =
				dimensions[IA_CSS_ROW_DIMENSION];
		retval = 0;
	} else {
		IA_CSS_TRACE_1(PSYSAPI_PARAM, ERROR,
		  "ia_css_terminal_param_set_dimensions failed (%i)\n", retval);
	}
	return retval;
}

int ia_css_terminal_param_set_stride(
	ia_css_terminal_param_t		*param,
	const uint32_t stride)
{
	int retval = -1;

	verifexit(param != NULL, EINVAL);
	param->stride = stride;
	retval = 0;

EXIT:
	return retval;
}

uint32_t ia_css_terminal_param_get_stride(
	const ia_css_terminal_param_t	*param)
{
	uint32_t stride = 0;

	verifexit(param != NULL, EINVAL);
	stride = param->stride;

EXIT:
	return stride;
}


static int ia_css_program_param_init(
		ia_css_program_param_t *program_param,
		int32_t offset)
{
	int retval = -1;

	COMPILATION_ERROR_IF(
		SIZE_OF_PROGRAM_PARAM_STRUCT_IN_BITS !=
				(CHAR_BIT * sizeof(ia_css_program_param_t)));
	verifexit(program_param != NULL, EINVAL);

	IA_CSS_TRACE_0(PSYSAPI_PARAM, INFO,
		"ia_css_program_param_init(): enter:\n");

	program_param->size = sizeof(ia_css_program_param_t);
	/* parent is at negative offset from current program.*/
	program_param->parent_offset = -offset;
	/*TODO: Kernel_bitmap setting. ?*/
	retval = 0;
EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_PARAM, ERROR,
			"ia_css_program_param_init failed (%i)\n", retval);
	}
	return retval;
}

static int
ia_css_terminal_param_init(ia_css_terminal_param_t *terminal_param,
			uint32_t offset,
			enum ia_css_frame_format_type frame_format_type)
{
	int retval = -1;

	COMPILATION_ERROR_IF(
		SIZE_OF_TERMINAL_PARAM_STRUCT_IN_BITS !=
			(CHAR_BIT * sizeof(ia_css_terminal_param_t)));
	verifexit(terminal_param != NULL, EINVAL);

	IA_CSS_TRACE_0(PSYSAPI_PARAM, INFO,
		"ia_css_terminal_param_init(): enter:\n");

	terminal_param->size = sizeof(ia_css_terminal_param_t);
	/* parent is at negative offset from current program.*/
	terminal_param->parent_offset = -((int32_t)offset);
	/*TODO: Kernel_bitmap setting. ?*/
	terminal_param->frame_format_type = frame_format_type;

	retval = 0;
EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_PARAM, ERROR,
			"ia_css_terminal_param_init failed (%i)\n", retval);
	}
	return retval;
}

ia_css_program_group_param_t *
ia_css_terminal_param_get_parent(
	const ia_css_terminal_param_t			*param)
{
	ia_css_program_group_param_t *parent = NULL;
	char *base;

	IA_CSS_TRACE_0(PSYSAPI_PARAM, VERBOSE,
		       "ia_css_terminal_param_get_parent(): enter:\n");

	verifexit(NULL != param, EINVAL);

	base = (char *)((char *)param + param->parent_offset);

	parent = (ia_css_program_group_param_t *)(base);
EXIT:
	if (NULL == param) {
		IA_CSS_TRACE_0(PSYSAPI_PARAM, WARNING,
			"ia_css_terminal_param_get_parent invalid argument\n");
	}
	return parent;
}

int ia_css_program_group_param_init(
	ia_css_program_group_param_t *blob,
	const uint8_t	program_count,
	const uint8_t	terminal_count,
	const uint16_t	fragment_count,
	const enum ia_css_frame_format_type *frame_format_types)
{
	int i = 0;
	char *param_base;
	uint32_t offset;
	int  retval = -1;

	COMPILATION_ERROR_IF(
		SIZE_OF_PROGRAM_GROUP_PARAM_STRUCT_IN_BITS !=
			(CHAR_BIT * sizeof(ia_css_program_group_param_t)));

	IA_CSS_TRACE_0(PSYSAPI_PARAM, INFO,
		"ia_css_program_group_param_init(): enter:\n");

	assert(blob != 0);

	verifexit(blob != NULL, EINVAL);
	verifexit(frame_format_types != NULL, EINVAL);

	blob->program_count = program_count;
	blob->fragment_count = fragment_count;
	blob->terminal_count = terminal_count;
	blob->program_param_offset = sizeof(ia_css_program_group_param_t);
	blob->terminal_param_offset = blob->program_param_offset +
				sizeof(ia_css_program_param_t) * program_count;

	param_base = (char *)((char *)blob + blob->program_param_offset);
	offset = blob->program_param_offset;

	for (i = 0; i < program_count; i++) {
		ia_css_program_param_init(
			(ia_css_program_param_t *)param_base, offset);
		offset += sizeof(ia_css_program_param_t);
		param_base += sizeof(ia_css_program_param_t);
	}

	param_base = (char *)((char *)blob + blob->terminal_param_offset);
	offset = blob->terminal_param_offset;

	for (i = 0; i < terminal_count; i++) {
		ia_css_terminal_param_init(
			(ia_css_terminal_param_t *)param_base,
			offset,
			frame_format_types[i]);

		offset += sizeof(ia_css_terminal_param_t);
		param_base += sizeof(ia_css_terminal_param_t);
	}

	blob->size = (uint32_t)ia_css_sizeof_program_group_param(program_count,
								terminal_count,
								fragment_count);
	retval = 0;
EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_PARAM, ERROR,
		       "ia_css_program_group_param_init failed (%i)\n", retval);
	}
	return retval;
}

int ia_css_program_group_param_print(
	const ia_css_program_group_param_t		*param,
	void						*fid)
{
	int	retval = -1;
	int		i;
	uint8_t	program_count, terminal_count;
	ia_css_kernel_bitmap_t	bitmap;

	IA_CSS_TRACE_0(PSYSAPI_PARAM, INFO,
		       "ia_css_program_group_param_print(): enter:\n");

	verifexit(param != NULL, EINVAL);
	NOT_USED(fid);

	IA_CSS_TRACE_1(PSYSAPI_PARAM, INFO,
		"sizeof(program_group_param) = %d\n",
		(int)ia_css_program_group_param_get_size(param));

	program_count = ia_css_program_group_param_get_program_count(param);
	terminal_count = ia_css_program_group_param_get_terminal_count(param);

	bitmap = ia_css_program_group_param_get_kernel_enable_bitmap(param);
	verifexit(ia_css_kernel_bitmap_print(bitmap, fid) == 0, EINVAL);

	IA_CSS_TRACE_1(PSYSAPI_PARAM, INFO,
		"%d program params\n", (int)program_count);
	for (i = 0; i < (int)program_count; i++) {
		ia_css_program_param_t *program_param =
			ia_css_program_group_param_get_program_param(param, i);

		retval = ia_css_program_param_print(program_param, fid);
		verifjmpexit(retval == 0);
	}
	IA_CSS_TRACE_1(PSYSAPI_PARAM, INFO, "%d terminal params\n",
		       (int)terminal_count);
	for (i = 0; i < (int)terminal_count; i++) {
		ia_css_terminal_param_t	*terminal_param =
			ia_css_program_group_param_get_terminal_param(param, i);

		retval = ia_css_terminal_param_print(terminal_param, fid);
		verifjmpexit(retval == 0);
	}

	retval = 0;
EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_PARAM, ERROR,
		      "ia_css_program_group_param_print failed (%i)\n", retval);
	}
	return retval;
}

int ia_css_terminal_param_print(
	const ia_css_terminal_param_t			*param,
	void						*fid)
{
	int	retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_PARAM, INFO,
		"ia_css_terminal_param_print(): enter:\n");

	verifexit(param != NULL, EINVAL);
	NOT_USED(fid);

	IA_CSS_TRACE_1(PSYSAPI_PARAM, INFO,
		"sizeof(terminal_param) = %d\n",
		(int)ia_css_terminal_param_get_size(param));

	IA_CSS_TRACE_1(PSYSAPI_PARAM, INFO,
		"\tframe_format_type = %d\n", param->frame_format_type);

	retval = 0;
EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_PARAM, ERROR,
			"ia_css_terminal_param_print failed (%i)\n", retval);
	}
	return retval;
}

int ia_css_program_param_print(
	const ia_css_program_param_t			*param,
	void						*fid)
{
	int	retval = -1;
	ia_css_kernel_bitmap_t	bitmap;

	IA_CSS_TRACE_0(PSYSAPI_PARAM, INFO,
		"ia_css_program_param_print(): enter:\n");

	verifexit(param != NULL, EINVAL);
	NOT_USED(fid);

	IA_CSS_TRACE_1(PSYSAPI_PARAM, INFO, "sizeof(program_param) = %d\n",
		       (int)ia_css_program_param_get_size(param));

	bitmap = ia_css_program_param_get_kernel_enable_bitmap(param);
	verifexit(ia_css_kernel_bitmap_print(bitmap, fid) == 0, EINVAL);

	retval = 0;
EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_PARAM, ERROR,
			"ia_css_program_param_print failed (%i)\n", retval);
	}
	return retval;
}

