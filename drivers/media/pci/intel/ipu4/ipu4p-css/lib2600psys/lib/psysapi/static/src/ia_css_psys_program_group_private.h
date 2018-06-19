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

#ifndef __IA_CSS_PSYS_PROGRAM_GROUP_PRIVATE_H
#define __IA_CSS_PSYS_PROGRAM_GROUP_PRIVATE_H

#include "ia_css_psys_manifest_types.h"
#include "ia_css_terminal_manifest_types.h"
#include "ia_css_kernel_bitmap.h"
#include "ia_css_program_group_data.h"
#include "vied_nci_psys_resource_model.h"
#include "ia_css_rbm_manifest_types.h"
#include <type_support.h>
#include <math_support.h>
#include <platform_support.h>

#define SIZE_OF_PROGRAM_GROUP_MANIFEST_STRUCT_IN_BITS \
	((IA_CSS_KERNEL_BITMAP_BITS) \
	+ (IA_CSS_PROGRAM_GROUP_ID_BITS) \
	+ (5 * IA_CSS_UINT16_T_BITS) \
	+ (5 * IA_CSS_UINT8_T_BITS) \
	+ (5 * IA_CSS_UINT8_T_BITS))

struct ia_css_program_group_manifest_s {
	/**< Indicate kernels are present in this program group */
	ia_css_kernel_bitmap_t kernel_bitmap;
	/**< Referral ID to program group FW */
	ia_css_program_group_ID_t ID;
	uint16_t program_manifest_offset;
	uint16_t terminal_manifest_offset;
	/**< Offset to private data (not part of the official API) */
	uint16_t private_data_offset;
	/**< Offset to RBM manifest */
	uint16_t rbm_manifest_offset;
	/**< Size of this structure */
	uint16_t size;
	/**< Storage alignment requirement (in uint8_t) */
	uint8_t alignment;
	/**< Total number of kernels in this program group */
	uint8_t kernel_count;
	/**< Total number of program in this program group */
	uint8_t program_count;
	/**< Total number of terminals on this program group */
	uint8_t terminal_count;
	/**< Total number of independent subgraphs in this program group */
	uint8_t subgraph_count;
	/**< Padding; esnures that rbm_manifest starts on 64bit alignment */
	uint8_t reserved[5];
};

#define SIZE_OF_PROGRAM_MANIFEST_STRUCT_IN_BITS \
	(IA_CSS_KERNEL_BITMAP_BITS \
	+ IA_CSS_PROGRAM_ID_BITS \
	+ IA_CSS_PROGRAM_TYPE_BITS \
	+ (3 * IA_CSS_UINT32_T_BITS) \
	+ (VIED_NCI_RESOURCE_BITMAP_BITS * VIED_NCI_N_DEV_DFM_ID) \
	+ (VIED_NCI_RESOURCE_BITMAP_BITS * VIED_NCI_N_DEV_DFM_ID) \
	+ IA_CSS_UINT16_T_BITS \
	+ (VIED_NCI_RESOURCE_SIZE_BITS * VIED_NCI_N_MEM_TYPE_ID) \
	+ (VIED_NCI_RESOURCE_SIZE_BITS * VIED_NCI_N_DATA_MEM_TYPE_ID * 2) \
	+ (VIED_NCI_RESOURCE_SIZE_BITS * VIED_NCI_N_DEV_CHN_ID * 2) \
	+ (IA_CSS_UINT8_T_BITS * VIED_NCI_N_DEV_DFM_ID) \
	+ (IA_CSS_PROCESS_MAX_CELLS * VIED_NCI_RESOURCE_ID_BITS) \
	+ (VIED_NCI_RESOURCE_ID_BITS) \
	+ (2 * IA_CSS_UINT8_T_BITS) \
	+ (N_PADDING_UINT8_IN_PROGRAM_GROUP_MANFEST * IA_CSS_UINT8_T_BITS))
/*
 * This structure contains only the information required for resource
 * management and construction of the process group.
 * The header for the program binary load is separate
 */

struct ia_css_program_manifest_s {
	/**< Indicate which kernels lead to this program being used */
	ia_css_kernel_bitmap_t kernel_bitmap;
	/**< Referral ID to a specific program FW, valid ID's != 0 */
	ia_css_program_ID_t ID;
	/**< Specification of for exclusive or parallel programs */
	ia_css_program_type_t program_type;
	/**< offset to add to reach parent. This is negative value.*/
	int32_t parent_offset;
	uint32_t program_dependency_offset;
	uint32_t terminal_dependency_offset;
#if (VIED_NCI_N_DEV_DFM_ID > 0)
	/**< DFM port allocation of this program */
	vied_nci_resource_bitmap_t dfm_port_bitmap[VIED_NCI_N_DEV_DFM_ID];
	/**< Active DFM ports which need a kick
	 * If an empty port is configured to run in active mode, the empty
	 * port and the corresponding full port(s) in the stream must be kicked.
	 * The empty port must always be kicked aster the full port.
	 */
	vied_nci_resource_bitmap_t dfm_active_port_bitmap[VIED_NCI_N_DEV_DFM_ID];
#endif
	/**< Size of this structure */
	uint16_t size;
	/**< (internal) Memory allocation size needs of this program */
	vied_nci_resource_size_t int_mem_size[VIED_NCI_N_MEM_TYPE_ID];
	/**< (external) Memory allocation size needs of this program */
	vied_nci_resource_size_t ext_mem_size[VIED_NCI_N_DATA_MEM_TYPE_ID];
	vied_nci_resource_size_t ext_mem_offset[VIED_NCI_N_DATA_MEM_TYPE_ID];
	/**< Device channel allocation size needs of this program */
	vied_nci_resource_size_t dev_chn_size[VIED_NCI_N_DEV_CHN_ID];
	vied_nci_resource_size_t dev_chn_offset[VIED_NCI_N_DEV_CHN_ID];
#if (VIED_NCI_N_DEV_DFM_ID > 0)
	/**< DFM ports are relocatable if value is set to 1.
	 * The flag is per dfm port type.
	 * This will not be supported for now.
	 */
	uint8_t is_dfm_relocatable[VIED_NCI_N_DEV_DFM_ID];
#endif
	/** Array of all the cells this program needs */
#if IA_CSS_PROCESS_MAX_CELLS == 1
	vied_nci_resource_id_t cell_id;
#else
	vied_nci_resource_id_t cells[IA_CSS_PROCESS_MAX_CELLS];
#endif /* IA_CSS_PROCESS_MAX_CELLS == 1 */
	/**< (exclusive) indication of a cell type to be used by this program */
	vied_nci_resource_id_t cell_type_id;

	/**< Number of programs this program depends on */
	uint8_t program_dependency_count;
	/**< Number of terminals this program depends on */
	uint8_t terminal_dependency_count;
	/**< Padding bytes for 64bit alignment*/
#if N_PADDING_UINT8_IN_PROGRAM_GROUP_MANFEST > 0
	/*hivecc does not allow an array of zero length*/
	uint8_t padding[N_PADDING_UINT8_IN_PROGRAM_GROUP_MANFEST];
#endif
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
	/**< Data terminal base */
	ia_css_terminal_manifest_t base;
	/**< Supported (4CC / MIPI / parameter) formats */
	ia_css_frame_format_bitmap_t frame_format_bitmap;
	/**< Indicate which kernels lead to this terminal being used */
	ia_css_kernel_bitmap_t kernel_bitmap;
	/**< Minimum size of the frame */
	uint16_t min_size[IA_CSS_N_DATA_DIMENSION];
	/**< Maximum size of the frame */
	uint16_t max_size[IA_CSS_N_DATA_DIMENSION];
	/**< Minimum size of a fragment that the program port can accept */
	uint16_t min_fragment_size[IA_CSS_N_DATA_DIMENSION];
	/**< Maximum size of a fragment that the program port can accept */
	uint16_t max_fragment_size[IA_CSS_N_DATA_DIMENSION];
	/**< Indicate if this terminal is derived from a principal terminal */
	uint16_t terminal_dependency;
	/**< Indicate what (streaming) interface types this terminal supports */
	ia_css_connection_bitmap_t connection_bitmap;
	/**< Indicates if compression is supported on the data associated with
	 * this terminal. '1' indicates compression is supported,
	 * '0' otherwise
	 */
	uint8_t compression_support;
	uint8_t reserved[4];
};

/* ============  Program Control Init Terminal Manifest - START ============ */
#define N_PADDING_UINT8_IN_PROGCTRLINIT_MANIFEST_PROGRAM_DESC_STRUCT 4
struct ia_css_program_control_init_manifest_program_desc_s {
	uint16_t load_section_count;
	uint16_t connect_section_count;
	uint8_t padding[N_PADDING_UINT8_IN_PROGCTRLINIT_MANIFEST_PROGRAM_DESC_STRUCT];
};

#define N_PADDING_UINT8_IN_PROGCTRLINIT_TERMINAL_MANIFEST_STRUCT 2
struct ia_css_program_control_init_terminal_manifest_s {
	ia_css_terminal_manifest_t base;
	/* Number of programs in program group */
	uint32_t program_count;
	/*
	 * Points to array of ia_css_program_control_init_terminal_program_desc_t
	 * with size program_count.
	 */
	uint16_t program_desc_offset;
	/* align to 64 */
	uint8_t padding[N_PADDING_UINT8_IN_PROGCTRLINIT_TERMINAL_MANIFEST_STRUCT];
};
/* ============  Program Control Init Terminal Manifest - END ============ */

extern void ia_css_program_manifest_init(
	ia_css_program_manifest_t	*blob,
	const uint8_t	program_dependency_count,
	const uint8_t	terminal_dependency_count);

#endif /* __IA_CSS_PSYS_PROGRAM_GROUP_PRIVATE_H */
