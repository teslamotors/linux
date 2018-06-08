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

#ifndef __IA_CSS_PROGRAM_GROUP_PARAM_PRIVATE_H
#define __IA_CSS_PROGRAM_GROUP_PARAM_PRIVATE_H

#include <ia_css_program_group_param.h>
#include <ia_css_psys_manifest_types.h>
#include <ia_css_psys_program_group_manifest.h>
#include <ia_css_psys_terminal_manifest.h>
#include <ia_css_kernel_bitmap.h>
#include <ia_css_program_group_data.h>
#include <type_support.h>

#define N_PADDING_UINT8_IN_PROGRAM_GROUP_PARAM_STRUCT 7
#define SIZE_OF_PROGRAM_GROUP_PARAM_STRUCT_IN_BITS \
	(IA_CSS_KERNEL_BITMAP_BITS \
	+ (3 * IA_CSS_UINT32_T_BITS) \
	+ IA_CSS_UINT16_T_BITS \
	+ (3 * IA_CSS_UINT8_T_BITS) \
	+ (N_PADDING_UINT8_IN_PROGRAM_GROUP_PARAM_STRUCT * IA_CSS_UINT8_T_BITS))

/* tentative; co-design with ISP algorithm */
struct ia_css_program_group_param_s {
	/* The enable bits for each individual kernel */
	ia_css_kernel_bitmap_t kernel_enable_bitmap;
	/* Size of this structure */
	uint32_t size;
	uint32_t program_param_offset;
	uint32_t terminal_param_offset;
	/* Number of (explicit) fragments to use in a frame */
	uint16_t fragment_count;
	/* Number of active programs */
	uint8_t program_count;
	/* Number of active terminals */
	uint8_t terminal_count;
	/* Program group protocol version */
	uint8_t protocol_version;
	uint8_t padding[N_PADDING_UINT8_IN_PROGRAM_GROUP_PARAM_STRUCT];
};

#define SIZE_OF_PROGRAM_PARAM_STRUCT_IN_BITS \
	(IA_CSS_KERNEL_BITMAP_BITS \
	+ IA_CSS_UINT32_T_BITS \
	+ IA_CSS_INT32_T_BITS)

/* private */
struct ia_css_program_param_s {
	/* What to use this one for ? */
	ia_css_kernel_bitmap_t kernel_enable_bitmap;
	/* Size of this structure */
	uint32_t size;
	/* offset to add to reach parent. This is negative value.*/
	int32_t parent_offset;
};

#define SIZE_OF_TERMINAL_PARAM_STRUCT_IN_BITS \
	(IA_CSS_UINT32_T_BITS \
	+ IA_CSS_FRAME_FORMAT_TYPE_BITS \
	+ IA_CSS_INT32_T_BITS \
	+ (IA_CSS_UINT16_T_BITS * IA_CSS_N_DATA_DIMENSION) \
	+ (IA_CSS_UINT16_T_BITS * IA_CSS_N_DATA_DIMENSION) \
	+ (IA_CSS_UINT16_T_BITS * IA_CSS_N_DATA_DIMENSION) \
	+ IA_CSS_INT32_T_BITS \
	+ IA_CSS_UINT16_T_BITS \
	+ IA_CSS_UINT8_T_BITS \
	+ (IA_CSS_UINT8_T_BITS * 1))

#endif /* __IA_CSS_PROGRAM_GROUP_PARAM_PRIVATE_H */
