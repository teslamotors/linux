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

#ifndef __IA_CSS_PSYS_BUFFER_SET_H
#define __IA_CSS_PSYS_BUFFER_SET_H

#include "ia_css_base_types.h"
#include "ia_css_psys_dynamic_storage_class.h"
#include "ia_css_psys_process_types.h"
#include "ia_css_terminal_types.h"

#define N_UINT64_IN_BUFFER_SET_STRUCT		1
#define N_UINT16_IN_BUFFER_SET_STRUCT		1
#define N_UINT8_IN_BUFFER_SET_STRUCT		1
#define N_PADDING_UINT8_IN_BUFFER_SET_STRUCT	5
#define SIZE_OF_BUFFER_SET \
	(N_UINT64_IN_BUFFER_SET_STRUCT * IA_CSS_UINT64_T_BITS \
	+ VIED_VADDRESS_BITS \
	+ VIED_VADDRESS_BITS \
	+ N_UINT16_IN_BUFFER_SET_STRUCT * IA_CSS_UINT16_T_BITS \
	+ N_UINT8_IN_BUFFER_SET_STRUCT * IA_CSS_UINT8_T_BITS \
	+ N_PADDING_UINT8_IN_BUFFER_SET_STRUCT * IA_CSS_UINT8_T_BITS)

typedef struct ia_css_buffer_set_s ia_css_buffer_set_t;

struct ia_css_buffer_set_s {
	/* Token for user context reference */
	uint64_t token;
	/* IPU virtual address of this buffer set */
	vied_vaddress_t ipu_virtual_address;
	/* IPU virtual address of the process group corresponding to this buffer set */
	vied_vaddress_t process_group_handle;
	/* Number of terminal buffer addresses in this structure */
	uint16_t terminal_count;
	/* Frame id to associate with this buffer set */
	uint8_t frame_counter;
	/* Padding for 64bit alignment */
	uint8_t padding[N_PADDING_UINT8_IN_BUFFER_SET_STRUCT];
};


/*! Construct a buffer set object at specified location

 @param	buffer_set_mem[in]	memory location to create buffer set object
 @param	process_group[in]	process group corresponding to this buffer set
 @param	frame_counter[in]	frame number for this buffer set object

 @return pointer to buffer set object on success, NULL on error
 */
ia_css_buffer_set_t *ia_css_buffer_set_create(
	void *buffer_set_mem,
	const ia_css_process_group_t *process_group,
	const unsigned int frame_counter);

/*! Compute size (in bytes) required for full buffer set object

 @param	process_group[in]	process group corresponding to this buffer set

 @return size in bytes of buffer set object on success, 0 on error
 */
size_t ia_css_sizeof_buffer_set(
	const ia_css_process_group_t *process_group);

/*! Set a buffer address in a buffer set object

 @param	buffer_set[in]		buffer set object to set buffer in
 @param	terminal_index[in]	terminal index to use as a reference between
				buffer and terminal
 @param	buffer[in]		buffer address to store

 @return 0 on success, -1 on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
int ia_css_buffer_set_set_buffer(
	ia_css_buffer_set_t *buffer_set,
	const unsigned int terminal_index,
	const vied_vaddress_t buffer);

/*! Get virtual buffer address from a buffer set object and terminal object by
   resolving the index used

 @param	buffer_set[in]		buffer set object to get buffer from
 @param	terminal[in]		terminal object to get buffer of

 @return virtual buffer address on success, VIED_NULL on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
vied_vaddress_t ia_css_buffer_set_get_buffer(
	const ia_css_buffer_set_t *buffer_set,
	const ia_css_terminal_t *terminal);

/*! Set ipu virtual address of a buffer set object within the buffer set object

 @param	buffer_set[in]		buffer set object to set ipu address in
 @param	ipu_vaddress[in]	ipu virtual address of the buffer set object

 @return 0 on success, -1 on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
int ia_css_buffer_set_set_ipu_address(
	ia_css_buffer_set_t *buffer_set,
	const vied_vaddress_t ipu_vaddress);

/*! Get ipu virtual address from a buffer set object

 @param	buffer_set[in]		buffer set object to get ipu address from

 @return virtual buffer set address on success, VIED_NULL on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
vied_vaddress_t ia_css_buffer_set_get_ipu_address(
	const ia_css_buffer_set_t *buffer_set);

/*! Set process group handle in a buffer set object

 @param	buffer_set[in]			buffer set object to set handle in
 @param	process_group_handle[in]	process group handle of the buffer set
					object

 @return 0 on success, -1 on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
int ia_css_buffer_set_set_process_group_handle(
	ia_css_buffer_set_t *buffer_set,
	const vied_vaddress_t process_group_handle);

/*! Get process group handle from a buffer set object

 @param	buffer_set[in]		buffer set object to get handle from

 @return virtual process group address on success, VIED_NULL on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
vied_vaddress_t ia_css_buffer_set_get_process_group_handle(
	const ia_css_buffer_set_t *buffer_set);

/*! Set token of a buffer set object within the buffer set object

 @param	buffer_set[in]		buffer set object to set ipu address in
 @param	token[in]		token of the buffer set object

 @return 0 on success, -1 on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
int ia_css_buffer_set_set_token(
	ia_css_buffer_set_t *buffer_set,
	const uint64_t token);

/*! Get token from a buffer set object

 @param	buffer_set[in]		buffer set object to get token from

 @return token on success, NULL on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
uint64_t ia_css_buffer_set_get_token(
	const ia_css_buffer_set_t *buffer_set);

#ifdef __IA_CSS_PSYS_DYNAMIC_INLINE__
#include "ia_css_psys_buffer_set_impl.h"
#endif /* __IA_CSS_PSYS_DYNAMIC_INLINE__ */

#endif /* __IA_CSS_PSYS_BUFFER_SET_H */
