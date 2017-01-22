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

#ifndef __IA_CSS_PSYS_PROCESS_PRIVATE_TYPES_H
#define __IA_CSS_PSYS_PROCESS_PRIVATE_TYPES_H

#include "ia_css_psys_process_types.h"

#define	N_UINT32_IN_PROCESS_STRUCT				1
#define	N_UINT16_IN_PROCESS_STRUCT				3
#define	N_UINT8_IN_PROCESS_STRUCT				2

#define SIZE_OF_PROCESS_STRUCT_BITS \
	(IA_CSS_KERNEL_BITMAP_BITS	\
	+ (N_UINT32_IN_PROCESS_STRUCT * 32) \
	+ IA_CSS_PROGRAM_ID_BITS \
	+ IA_CSS_PROCESS_STATE_BITS \
	+ (N_UINT16_IN_PROCESS_STRUCT * 16) \
	+ (VIED_NCI_N_MEM_TYPE_ID * VIED_NCI_RESOURCE_SIZE_BITS) \
	+ (VIED_NCI_N_DATA_MEM_TYPE_ID * VIED_NCI_RESOURCE_SIZE_BITS) \
	+ (VIED_NCI_N_DEV_CHN_ID * VIED_NCI_RESOURCE_SIZE_BITS) \
	+ VIED_NCI_RESOURCE_ID_BITS \
	+ (VIED_NCI_N_MEM_TYPE_ID * VIED_NCI_RESOURCE_ID_BITS) \
	+ (VIED_NCI_N_DATA_MEM_TYPE_ID * VIED_NCI_RESOURCE_ID_BITS) \
	+ (N_UINT8_IN_PROCESS_STRUCT * 8) \
	+ (N_PADDING_UINT8_IN_PROCESS_STRUCT * 8))

struct ia_css_process_s {
	/**< Indicate which kernels lead to this process being used */
	ia_css_kernel_bitmap_t kernel_bitmap;
	uint32_t size; /**< Size of this structure */
	ia_css_program_ID_t ID; /**< Referal ID to a specific program FW */
	/**< State of the process FSM dependent on the parent FSM */
	ia_css_process_state_t state;
	int16_t parent_offset; /**< Reference to the process group */
	/**< Array[dependency_count] of ID's of the cells that provide input */
	uint16_t cell_dependencies_offset;
	/**< Array[terminal_dependency_count] of indices of connected terminals */
	uint16_t terminal_dependencies_offset;
	/**< (internal) Memory allocation offset given to this process */
	vied_nci_resource_size_t int_mem_offset[VIED_NCI_N_MEM_TYPE_ID];
	/**< (external) Memory allocation offset given to this process */
	vied_nci_resource_size_t ext_mem_offset[VIED_NCI_N_DATA_MEM_TYPE_ID];
	/**< Device channel allocation offset given to this process */
	vied_nci_resource_size_t dev_chn_offset[VIED_NCI_N_DEV_CHN_ID];
	/**< (mandatory) specification of a cell to be used by this process */
	vied_nci_resource_id_t cell_id;
	/**< (internal) Memory ID; This is redundant, derived from cell_id */
	vied_nci_resource_id_t int_mem_id[VIED_NCI_N_MEM_TYPE_ID];
	/**< (external) Memory ID */
	vied_nci_resource_id_t ext_mem_id[VIED_NCI_N_DATA_MEM_TYPE_ID];
	/**< Number of processes (mapped on cells) this process depends on */
	uint8_t cell_dependency_count;
	/**< Number of terminals this process depends on */
	uint8_t terminal_dependency_count;
	/**< Padding bytes for 64bit alignment*/
	uint8_t padding[N_PADDING_UINT8_IN_PROCESS_STRUCT];
};

#endif /* __IA_CSS_PSYS_PROCESS_PRIVATE_TYPES_H */
