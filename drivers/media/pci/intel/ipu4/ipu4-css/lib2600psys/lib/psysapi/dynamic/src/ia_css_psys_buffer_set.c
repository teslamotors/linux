/*
 * Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2018, Intel Corporation.
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

#include "assert_support.h"
#include "ia_css_psys_dynamic_trace.h"
#include "ia_css_psys_buffer_set.h"
#include "ia_css_psys_process_group.h"

/*
 * Functions to possibly inline
 */
#ifndef __IA_CSS_PSYS_DYNAMIC_INLINE__
#include "ia_css_psys_buffer_set_impl.h"
#endif /* __IA_CSS_PSYS_DYNAMIC_INLINE__ */

STORAGE_CLASS_INLINE void __buffer_set_dummy_check_alignment(void)
{
	COMPILATION_ERROR_IF(SIZE_OF_BUFFER_SET !=
		CHAR_BIT * sizeof(ia_css_buffer_set_t));

	COMPILATION_ERROR_IF(0 !=
		sizeof(ia_css_buffer_set_t) % sizeof(uint64_t));
}

/*
 * Functions not to inline
 */

/* The below functions are not to be compiled for firmware */
#if !defined(__HIVECC)

ia_css_buffer_set_t *ia_css_buffer_set_create(
	void *buffer_set_mem,
	const ia_css_process_group_t *process_group,
	const unsigned int frame_counter)
{
	ia_css_buffer_set_t *buffer_set = NULL;
	unsigned int i;
	int ret = -1;

	verifexit(buffer_set_mem != NULL);
	verifexit(process_group != NULL);

	buffer_set = (ia_css_buffer_set_t *)buffer_set_mem;

	/*
	 * Set base struct members
	 */
	buffer_set->ipu_virtual_address = VIED_NULL;
	ia_css_process_group_get_ipu_vaddress(process_group,
		&buffer_set->process_group_handle);
	buffer_set->frame_counter = frame_counter;
	buffer_set->terminal_count =
		ia_css_process_group_get_terminal_count(process_group);

	/*
	 * Initialize adjacent buffer addresses
	 */
	for (i = 0; i < buffer_set->terminal_count; i++) {
		vied_vaddress_t *buffer =
			(vied_vaddress_t *)(
				(char *)buffer_set +
				sizeof(ia_css_buffer_set_t) +
				sizeof(vied_vaddress_t) * i);

		*buffer = VIED_NULL;
	}
	ret = 0;

EXIT:
	if (ret != 0) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_buffer_set_create failed\n");
	}
	return buffer_set;
}

size_t ia_css_sizeof_buffer_set(
	const ia_css_process_group_t *process_group)
{
	size_t size = 0;

	verifexit(process_group != NULL);

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_sizeof_buffer_set(): enter:\n");

	size = sizeof(ia_css_buffer_set_t) +
		ia_css_process_group_get_terminal_count(process_group) *
		sizeof(vied_vaddress_t);

EXIT:
	if (size == 0) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_sizeof_buffer_set failed\n");
	}
	return size;
}

#endif
