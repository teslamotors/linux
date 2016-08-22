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


#include "ia_css_program_group_data.h"
#include "ia_css_psys_data_trace.h"
#include <error_support.h>	/* for verifexit */
#include <assert_support.h>
#include <misc_support.h>	/* for NOT_USED */

int ia_css_frame_print(
	const ia_css_frame_t *frame, void *fid)
{
	int retval = -1;

	NOT_USED(fid);

	IA_CSS_TRACE_0(PSYSAPI_DATA, INFO, "ia_css_frame_print(): enter:\n");

	verifexit(frame != NULL, EINVAL);

	IA_CSS_TRACE_1(PSYSAPI_DATA, INFO,
		"\tbuffer = %d\n", ia_css_frame_get_buffer(frame));
	IA_CSS_TRACE_1(PSYSAPI_DATA, INFO,
		"\tbuffer_state = %d\n", ia_css_frame_get_buffer_state(frame));
	/* IA_CSS_TRACE_1(PSYSAPI_DATA, INFO, "\tbuffer_state = %s\n",
	 * ia_css_buffer_state_string(ia_css_frame_get_buffer_state(frame)));
	 */
	IA_CSS_TRACE_1(PSYSAPI_DATA, INFO,
	       "\tpointer_state = %d\n", ia_css_frame_get_pointer_state(frame));
	/* IA_CSS_TRACE_1(PSYSAPI_DATA, INFO, "\tpointer_state = %s\n",
	 * ia_css_pointer_state_string(ia_css_frame_get_pointer_state(frame)));
	 */
	IA_CSS_TRACE_1(PSYSAPI_DATA, INFO,
		"\tdata_bytes = %d\n", frame->data_bytes);

	retval = 0;
EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DATA, ERROR,
			"ia_css_frame_print failed (%i)\n", retval);
	}
	return retval;
}

const vied_vaddress_t *ia_css_frame_get_buffer_host_virtual_address(
	const ia_css_frame_t *frame) {

	IA_CSS_TRACE_0(PSYSAPI_DATA, VERBOSE,
		"ia_css_frame_get_buffer_host_virtual_address(): enter:\n");

	verifexit(frame != NULL, EINVAL);
	return &(frame->data);

EXIT:
	if (NULL == frame) {
		IA_CSS_TRACE_0(PSYSAPI_DATA, WARNING,
			"ia_css_frame_get_buffer_host_virtual_address invalid argument\n");
	}
	return NULL;
}

vied_vaddress_t	ia_css_frame_get_buffer(
	const ia_css_frame_t *frame)
{
	vied_vaddress_t	buffer = VIED_NULL;

	IA_CSS_TRACE_0(PSYSAPI_DATA, VERBOSE,
		"ia_css_frame_get_buffer(): enter:\n");

	verifexit(frame != NULL, EINVAL);
	buffer = frame->data;

EXIT:
	if (NULL == frame) {
		IA_CSS_TRACE_0(PSYSAPI_DATA, WARNING,
		       "ia_css_frame_get_buffer invalid argument\n");
	}
	return buffer;
}

int ia_css_frame_set_buffer(
	ia_css_frame_t *frame,
	vied_vaddress_t buffer)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DATA, VERBOSE,
		"ia_css_frame_set_buffer(): enter:\n");

	verifexit(frame != NULL, EINVAL);
	frame->data = buffer;

	retval = 0;
EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DATA, ERROR,
			"ia_css_frame_set_buffer failed (%i)\n", retval);
	}
	return retval;
}

int ia_css_frame_set_data_bytes(
	ia_css_frame_t *frame,
	unsigned int size)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DATA, VERBOSE,
		"ia_css_frame_set_data_bytes(): enter:\n");

	verifexit(frame != NULL, EINVAL);
	frame->data_bytes = size;

	retval = 0;
EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DATA, ERROR,
			"ia_css_frame_set_data_bytes failed (%i)\n", retval);
	}
	return retval;
}

ia_css_buffer_state_t ia_css_frame_get_buffer_state(
	const ia_css_frame_t *frame)
{
	ia_css_buffer_state_t buffer_state = IA_CSS_N_BUFFER_STATES;

	IA_CSS_TRACE_0(PSYSAPI_DATA, VERBOSE,
		"ia_css_frame_get_buffer_state(): enter:\n");

	verifexit(frame != NULL, EINVAL);
	buffer_state = frame->buffer_state;

EXIT:
	if (NULL == frame) {
		IA_CSS_TRACE_0(PSYSAPI_DATA, WARNING,
			"ia_css_frame_get_buffer_state invalid argument\n");
	}
	return buffer_state;
}

int ia_css_frame_set_buffer_state(
	ia_css_frame_t *frame,
	const ia_css_buffer_state_t buffer_state)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DATA, VERBOSE,
		"ia_css_frame_set_buffer_state(): enter:\n");

	verifexit(frame != NULL, EINVAL);
	frame->buffer_state = buffer_state;

	retval = 0;
EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DATA, ERROR,
			"ia_css_frame_set_buffer_state failed (%i)\n", retval);
	}
	return retval;
}

ia_css_pointer_state_t ia_css_frame_get_pointer_state(
	const ia_css_frame_t *frame)
{
	ia_css_pointer_state_t pointer_state = IA_CSS_N_POINTER_STATES;

	IA_CSS_TRACE_0(PSYSAPI_DATA, VERBOSE,
		"ia_css_frame_get_pointer_state(): enter:\n");

	verifexit(frame != NULL, EINVAL);
	pointer_state = frame->pointer_state;

EXIT:
	if (NULL == frame) {
		IA_CSS_TRACE_0(PSYSAPI_DATA, WARNING,
			"ia_css_frame_get_pointer_state invalid argument\n");
	}
	return pointer_state;
}

int ia_css_frame_set_pointer_state(
	ia_css_frame_t *frame,
	const ia_css_pointer_state_t pointer_state)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DATA, VERBOSE,
		"ia_css_frame_set_pointer_state(): enter:\n");

	verifexit(frame != NULL, EINVAL);
	frame->pointer_state = pointer_state;

	retval = 0;
EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DATA, ERROR,
			"ia_css_frame_set_pointer_state failed (%i)\n", retval);
	}
	return retval;
}

int ia_css_frame_descriptor_print(
	const ia_css_frame_descriptor_t *frame_descriptor,
	void *fid)
{
	int retval = -1;
	int i;
	uint8_t frame_plane_count;

	NOT_USED(fid);

	IA_CSS_TRACE_0(PSYSAPI_DATA, INFO,
		"ia_css_frame_descriptor_print(): enter:\n");

	assert(IA_CSS_N_DATA_DIMENSION > 0);

	verifexit(frame_descriptor != NULL, EINVAL);

	IA_CSS_TRACE_0(PSYSAPI_DATA, INFO,
		"ia_css_frame_descriptor_print(): enter:\n");

	IA_CSS_TRACE_1(PSYSAPI_DATA, INFO,
		"\tframe_format_type = %d\n",
		frame_descriptor->frame_format_type);
	/* IA_CSS_TRACE_1(PSYSAPI_DATA, INFO, "\tframe_format_type = %s\n",
	 * ia_css_frame_format_string(frame_descriptor->frame_format_type));
	 */

	IA_CSS_TRACE_1(PSYSAPI_DATA, INFO,
		"\tbpp = %d\n", frame_descriptor->bpp);
	IA_CSS_TRACE_1(PSYSAPI_DATA, INFO,
		"\tbpe = %d\n", frame_descriptor->bpe);
	IA_CSS_TRACE_1(PSYSAPI_DATA, INFO,
		"\tis_compressed = %d\n", frame_descriptor->is_compressed);

	frame_plane_count = IA_CSS_N_FRAME_PLANES;
	/* frame_plane_count =
	 * ia_css_frame_plane_count(frame_descriptor->frame_format_type);
	 */

	verifexit(frame_plane_count > 0, EINVAL);

	IA_CSS_TRACE_1(PSYSAPI_DATA, INFO,
		"\tplane_offsets[%d]: [\n", frame_plane_count);
	for (i = 0; i < (int)frame_plane_count - 1; i++) {
		IA_CSS_TRACE_1(PSYSAPI_DATA, INFO,
			"\t%4d,\n", frame_descriptor->plane_offsets[i]);
	}
	IA_CSS_TRACE_1(PSYSAPI_DATA, INFO,
		"\t%4d ]\n", frame_descriptor->plane_offsets[i]);

	IA_CSS_TRACE_1(PSYSAPI_DATA, INFO,
		"\tdimension[%d] = {\n", IA_CSS_N_DATA_DIMENSION);
	for (i = 0; i < (int)IA_CSS_N_DATA_DIMENSION - 1; i++) {
		IA_CSS_TRACE_1(PSYSAPI_DATA, INFO,
			"\t%4d,\n", frame_descriptor->dimension[i]);
	}
	IA_CSS_TRACE_1(PSYSAPI_DATA, INFO,
		"\t%4d }\n", frame_descriptor->dimension[i]);

	assert(0 <= IA_CSS_N_DATA_DIMENSION - 2);
	IA_CSS_TRACE_1(PSYSAPI_DATA, INFO,
		"\tstride[%d] = {\n", IA_CSS_N_DATA_DIMENSION - 1);
	for (i = 0; i < (int)IA_CSS_N_DATA_DIMENSION - 2; i++) {
		IA_CSS_TRACE_1(PSYSAPI_DATA, INFO,
			"\t%4d,\n", frame_descriptor->stride[i]);
	}
	IA_CSS_TRACE_1(PSYSAPI_DATA, INFO,
		"\t%4d }\n", frame_descriptor->stride[i]);

	retval = 0;
EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DATA, ERROR,
			"ia_css_frame_descriptor_print failed (%i)\n", retval);
	}
	return retval;
}

int ia_css_fragment_descriptor_print(
	const ia_css_fragment_descriptor_t *fragment_descriptor,
	void *fid)
{
	int retval = -1;
	int i;

	NOT_USED(fid);

	IA_CSS_TRACE_0(PSYSAPI_DATA, INFO,
		"ia_css_fragment_descriptor_print(): enter:\n");

	verifexit(fragment_descriptor != NULL, EINVAL);

	IA_CSS_TRACE_1(PSYSAPI_DATA, INFO,
		"dimension[%d] = {\n", IA_CSS_N_DATA_DIMENSION);
	for (i = 0; i < (int)IA_CSS_N_DATA_DIMENSION - 1; i++) {
		IA_CSS_TRACE_1(PSYSAPI_DATA, INFO,
			"\t%4d,\n", fragment_descriptor->dimension[i]);
	}
	IA_CSS_TRACE_1(PSYSAPI_DATA, INFO,
		"\t%4d }\n", fragment_descriptor->dimension[i]);

	IA_CSS_TRACE_1(PSYSAPI_DATA, INFO,
		"index[%d] = {\n", IA_CSS_N_DATA_DIMENSION);
	for (i = 0; i < (int)IA_CSS_N_DATA_DIMENSION - 1; i++) {
		IA_CSS_TRACE_1(PSYSAPI_DATA, INFO,
			"\t%4d,\n", fragment_descriptor->index[i]);
	}
	IA_CSS_TRACE_1(PSYSAPI_DATA, INFO,
		"\t%4d }\n", fragment_descriptor->index[i]);

	IA_CSS_TRACE_1(PSYSAPI_DATA, INFO,
		"offset[%d] = {\n", IA_CSS_N_DATA_DIMENSION);
	for (i = 0; i < (int)IA_CSS_N_DATA_DIMENSION - 1; i++) {
		IA_CSS_TRACE_1(PSYSAPI_DATA, INFO,
			"\t%4d,\n", fragment_descriptor->offset[i]);
	}
	IA_CSS_TRACE_1(PSYSAPI_DATA, INFO, "\t%4d }\n",
		fragment_descriptor->offset[i]);

	retval = 0;
EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DATA, ERROR,
		      "ia_css_fragment_descriptor_print failed (%i)\n", retval);
	}
	return retval;
}

ia_css_frame_format_bitmap_t ia_css_frame_format_bit_mask(
	const ia_css_frame_format_type_t frame_format_type)
{
	ia_css_frame_format_bitmap_t bit_mask = 0;

	IA_CSS_TRACE_0(PSYSAPI_DATA, VERBOSE,
		"ia_css_frame_format_bit_mask(): enter:\n");

	if ((frame_format_type < IA_CSS_N_FRAME_FORMAT_TYPES) &&
	    (frame_format_type < IA_CSS_FRAME_FORMAT_BITMAP_BITS)) {
		bit_mask = (ia_css_frame_format_bitmap_t)1 << frame_format_type;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_DATA, WARNING,
			"ia_css_frame_format_bit_mask invalid argument\n");
	}

	return bit_mask;
}

ia_css_frame_format_bitmap_t ia_css_frame_format_bitmap_clear(void)
{
	IA_CSS_TRACE_0(PSYSAPI_DATA, VERBOSE,
		       "ia_css_frame_format_bitmap_clear(): enter:\n");

	return 0;
}

size_t ia_css_sizeof_frame_descriptor(
		const uint8_t plane_count)
{
	size_t size = 0;

	IA_CSS_TRACE_0(PSYSAPI_DATA, VERBOSE,
		       "ia_css_sizeof_frame_descriptor(): enter:\n");

	verifexit(plane_count > 0, EINVAL);
	size += sizeof(ia_css_frame_descriptor_t);
	size += plane_count * sizeof(uint32_t);

EXIT:
	if (0 == plane_count) {
		IA_CSS_TRACE_0(PSYSAPI_DATA, WARNING,
			"ia_css_sizeof_frame_descriptor invalid argument\n");
	}
	return size;
}

