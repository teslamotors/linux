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

#ifndef _IA_CSS_PSYS_PROGRAM_GROUP_PRIVATE_H_
#define _IA_CSS_PSYS_PROGRAM_GROUP_PRIVATE_H_

#include "ia_css_psys_program_group_manifest.h"
#include "ia_css_psys_program_manifest.h"
#include "ia_css_psys_terminal_manifest.h"
#include "type_support.h"
#include <math_support.h>
#include <platform_support.h>

#define SIZE_OF_PROGRAM_GROUP_MANIFEST_STRUCT_IN_BITS \
	(IA_CSS_KERNEL_BITMAP_BITS \
	+ IA_CSS_PROGRAM_GROUP_ID_BITS \
	+ (3 * IA_CSS_UINT32_T_BITS) \
	+ IA_CSS_UINT16_T_BITS \
	+ (5 * IA_CSS_UINT8_T_BITS) \
	+ (1 * IA_CSS_UINT8_T_BITS))

struct ia_css_program_group_manifest_s {
	ia_css_kernel_bitmap_t				kernel_bitmap;									/**< Indicate kernels are present in this program group */
	ia_css_program_group_ID_t			ID;												/**< Referal ID to program group FW */
	uint32_t	program_manifest_offset;
	uint32_t	terminal_manifest_offset;
	uint32_t	private_data_offset;														/**< Offset to private data (not part of the official API) */
	uint16_t							size;											/**< Size of this structure */
	uint8_t								alignment;										/**< Storage alignment requirement (in uint8_t) */
	uint8_t								kernel_count;									/**< Total number of kernels in this program group */
	uint8_t								program_count;									/**< Total number of program in this program group */
	uint8_t								terminal_count;									/**< Total number of terminals on this program group */
	uint8_t								subgraph_count;									/**< Total number of independent subgraphs in this program group */
	uint8_t								reserved[1];
};

#define SIZE_OF_PROGRAM_MANIFEST_STRUCT_IN_BITS \
	(IA_CSS_KERNEL_BITMAP_BITS \
	+ IA_CSS_PROGRAM_ID_BITS \
	+ IA_CSS_PROGRAM_TYPE_BITS \
	+ IA_CSS_UINT32_T_BITS + (2 * IA_CSS_UINT32_T_BITS) \
	+ IA_CSS_UINT16_T_BITS \
	+ (VIED_NCI_RESOURCE_SIZE_BITS * VIED_NCI_N_MEM_TYPE_ID) \
	+ (VIED_NCI_RESOURCE_SIZE_BITS * VIED_NCI_N_DATA_MEM_TYPE_ID) \
	+ (VIED_NCI_RESOURCE_SIZE_BITS * VIED_NCI_N_DEV_CHN_ID) \
	+ (2 * VIED_NCI_RESOURCE_ID_BITS) \
	+ (2 * IA_CSS_UINT8_T_BITS) \
	+ (N_PADDING_UINT8_IN_PROGRAM_GROUP_MANFEST * IA_CSS_UINT8_T_BITS))
/*
 * This structure contains only the information required for resource management and
 * construction of the process group. The header for the program binary load is separate
 */
struct ia_css_program_manifest_s {
	ia_css_kernel_bitmap_t				kernel_bitmap;									/**< Indicate which kernels lead to this program being used */
	ia_css_program_ID_t					ID;												/**< Referal ID to a specific program FW, valid ID's != 0 */
	ia_css_program_type_t				program_type;									/**< Specification of for exclusive or parallel programs */
	int32_t		parent_offset; /**< offset to add to reach parent. This is negative value.*/
	uint32_t		program_dependency_offset;
	uint32_t		terminal_dependency_offset;
	uint16_t							size;											/**< Size of this structure */
	vied_nci_resource_size_t			int_mem_size[VIED_NCI_N_MEM_TYPE_ID];			/**< (internal) Memory allocation size needs of this program */
	vied_nci_resource_size_t			ext_mem_size[VIED_NCI_N_DATA_MEM_TYPE_ID];		/**< (external) Memory allocation size needs of this program */
	vied_nci_resource_size_t			dev_chn_size[VIED_NCI_N_DEV_CHN_ID];			/**< Device channel allocation size needs of this program */
	vied_nci_resource_id_t				cell_id;										/**< (optional) specification of a cell to be used by this program */
	vied_nci_resource_id_t				cell_type_id;									/**< (exclusive) indication of a cell type to be used by this program */
	uint8_t						program_dependency_count;						/**< Number of programs this program depends on */
	uint8_t						terminal_dependency_count;						/**< Number of terminals this program depends on */
	uint8_t						reserved[N_PADDING_UINT8_IN_PROGRAM_GROUP_MANFEST];
};

/*
 *Calculation for manual size check for struct ia_css_data_terminal_manifest_s
 */
#define SIZE_OF_DATA_TERMINAL_MANIFEST_STRUCT_IN_BITS \
	(SIZE_OF_TERMINAL_MANIFEST_STRUCT_IN_BITS \
	+ IA_CSS_FRAME_FORMAT_BITMAP_BITS \
	+ IA_CSS_CONNECTION_BITMAP_BITS \
	+ IA_CSS_KERNEL_BITMAP_BITS \
	+ (4 * (IA_CSS_UINT16_T_BITS * IA_CSS_N_DATA_DIMENSION)) \
	+ IA_CSS_UINT16_T_BITS \
	+ IA_CSS_UINT8_T_BITS \
	+ (4*IA_CSS_UINT8_T_BITS))
/*
 * Inherited data terminal class
 */
struct ia_css_data_terminal_manifest_s {
	ia_css_terminal_manifest_t			base;											/**< Data terminal base */
	ia_css_frame_format_bitmap_t		frame_format_bitmap;							/**< Supported (4CC / MIPI / parameter) formats */
	ia_css_kernel_bitmap_t				kernel_bitmap;									/**< Indicate which kernels lead to this terminal being used */
	uint16_t							min_size[IA_CSS_N_DATA_DIMENSION];				/**< Minimum size of the frame */
	uint16_t							max_size[IA_CSS_N_DATA_DIMENSION];				/**< Maximum size of the frame */
	uint16_t							min_fragment_size[IA_CSS_N_DATA_DIMENSION];		/**< Minimum size of a fragment that the program port can accept */
	uint16_t							max_fragment_size[IA_CSS_N_DATA_DIMENSION];		/**< Maximum size of a fragment that the program port can accept */
	uint16_t							terminal_dependency;							/**< Indicate if this terminal is derived from a principal terminal */
	ia_css_connection_bitmap_t			connection_bitmap;								/**< Indicate what (streaming) interface types this terminal supports */
	uint8_t						compression_support;							/**< Indicates if compression is supported on the data associated with this terminal.
																 * '1' indicates compression is supported, '0' otherwise */
	uint8_t								reserved[4];
};

extern void ia_css_program_manifest_init(
	ia_css_program_manifest_t	*blob,
	const uint8_t	program_dependency_count,
	const uint8_t	terminal_dependency_count);

#endif /* _IA_CSS_PSYS_PROGRAM_GROUP_PRIVATE_H_ */
