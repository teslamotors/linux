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

#ifndef __IA_CSS_PSYS_BUFFER_SET_IMPL_H
#define __IA_CSS_PSYS_BUFFER_SET_IMPL_H

#include "error_support.h"
#include "ia_css_psys_dynamic_trace.h"
#include "vied_nci_psys_system_global.h"
#include "ia_css_psys_terminal.hsys.user.h"

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
int ia_css_buffer_set_set_buffer(
	ia_css_buffer_set_t *buffer_set,
	const unsigned int terminal_index,
	const vied_vaddress_t buffer)
{
	DECLARE_ERRVAL
	vied_vaddress_t *buffer_ptr;
	int ret = -1;

	verifexitval(buffer_set != NULL, EFAULT);
	verifexitval(terminal_index < buffer_set->terminal_count, EFAULT);

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_buffer_set_set_buffer(): enter:\n");

	/*
	 * Set address in buffer set object
	 */
	buffer_ptr =
		(vied_vaddress_t *)(
			(char *)buffer_set +
			sizeof(ia_css_buffer_set_t) +
			terminal_index * sizeof(vied_vaddress_t));
	*buffer_ptr = buffer;

	ret = 0;

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_buffer_set_set_buffer: invalid argument\n");
	}
	return ret;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
vied_vaddress_t ia_css_buffer_set_get_buffer(
	const ia_css_buffer_set_t *buffer_set,
	const ia_css_terminal_t *terminal)
{
	DECLARE_ERRVAL
	vied_vaddress_t buffer = VIED_NULL;
	vied_vaddress_t *buffer_ptr;
	int terminal_index;

	verifexitval(buffer_set != NULL, EFAULT);
	verifexitval(terminal != NULL, EFAULT);

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_buffer_set_get_buffer(): enter:\n");

	/*
	 * Retrieve terminal index from terminal object
	 */
	terminal_index = ia_css_terminal_get_terminal_index(terminal);
	verifexitval(terminal_index >= 0, EFAULT);
	verifexitval(terminal_index < buffer_set->terminal_count, EFAULT);

	/*
	 * Retrieve address from buffer set object
	 */
	buffer_ptr =
		(vied_vaddress_t *)(
			(char *)buffer_set +
			sizeof(ia_css_buffer_set_t) +
			terminal_index * sizeof(vied_vaddress_t));
	buffer = *buffer_ptr;

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_buffer_set_get_buffer: invalid argument\n");
	}
	return buffer;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
int ia_css_buffer_set_set_ipu_address(
	ia_css_buffer_set_t *buffer_set,
	const vied_vaddress_t ipu_vaddress)
{
	DECLARE_ERRVAL
	int ret = -1;

	verifexitval(buffer_set != NULL, EFAULT);

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_buffer_set_set_ipu_address(): enter:\n");

	buffer_set->ipu_virtual_address = ipu_vaddress;

	ret = 0;

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_buffer_set_set_ipu_address invalid argument\n");
	}
	return ret;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
vied_vaddress_t ia_css_buffer_set_get_ipu_address(
	const ia_css_buffer_set_t *buffer_set)
{
	DECLARE_ERRVAL
	vied_vaddress_t ipu_virtual_address = VIED_NULL;

	verifexitval(buffer_set != NULL, EFAULT);

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_buffer_set_get_ipu_address(): enter:\n");

	ipu_virtual_address = buffer_set->ipu_virtual_address;

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_buffer_set_get_ipu_address: invalid argument\n");
	}
	return ipu_virtual_address;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
int ia_css_buffer_set_set_process_group_handle(
	ia_css_buffer_set_t *buffer_set,
	const vied_vaddress_t process_group_handle)
{
	DECLARE_ERRVAL
	int ret = -1;

	verifexitval(buffer_set != NULL, EFAULT);

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_buffer_set_set_process_group_context(): enter:\n");

	buffer_set->process_group_handle = process_group_handle;

	ret = 0;

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_buffer_set_set_process_group_context invalid argument\n");
	}
	return ret;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
vied_vaddress_t ia_css_buffer_set_get_process_group_handle(
	const ia_css_buffer_set_t *buffer_set)
{
	DECLARE_ERRVAL
	vied_vaddress_t process_group_handle = VIED_NULL;

	verifexitval(buffer_set != NULL, EFAULT);

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_buffer_set_get_process_group_handle(): enter:\n");

	process_group_handle = buffer_set->process_group_handle;

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_buffer_set_get_process_group_handle: invalid argument\n");
	}
	return process_group_handle;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
int ia_css_buffer_set_set_token(
	ia_css_buffer_set_t *buffer_set,
	const uint64_t token)
{
	DECLARE_ERRVAL
	int ret = -1;

	verifexitval(buffer_set != NULL, EFAULT);

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_buffer_set_set_token(): enter:\n");

	buffer_set->token = token;

	ret = 0;

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_buffer_set_set_token invalid argument\n");
	}
	return ret;
}

IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_C
uint64_t ia_css_buffer_set_get_token(
	const ia_css_buffer_set_t *buffer_set)
{
	DECLARE_ERRVAL
	uint64_t token = 0;

	verifexitval(buffer_set != NULL, EFAULT);

	IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, VERBOSE,
		"ia_css_buffer_set_get_token(): enter:\n");

	token = buffer_set->token;

EXIT:
	if (haserror(EFAULT)) {
		IA_CSS_TRACE_0(PSYSAPI_DYNAMIC, ERROR,
			"ia_css_buffer_set_get_token: invalid argument\n");
	}
	return token;
}

#endif /* __IA_CSS_PSYS_BUFFER_SET_IMPL_H */
