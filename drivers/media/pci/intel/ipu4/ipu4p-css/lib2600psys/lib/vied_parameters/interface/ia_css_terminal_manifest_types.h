/**
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

#ifndef __IA_CSS_TERMINAL_MANIFEST_TYPES_H
#define __IA_CSS_TERMINAL_MANIFEST_TYPES_H


#include "ia_css_terminal_defs.h"
#include "type_support.h"
#include "ia_css_base_types.h"
#include "ia_css_terminal_manifest_base_types.h"

#define N_PADDING_UINT8_IN_PARAM_TERMINAL_MANIFEST_SEC_STRUCT 1
#define SIZE_OF_PARAM_TERMINAL_MANIFEST_SEC_STRUCT_IN_BITS \
	(1 * IA_CSS_UINT32_T_BITS \
	+ 3 * IA_CSS_UINT8_T_BITS \
	+ N_PADDING_UINT8_IN_PARAM_TERMINAL_MANIFEST_SEC_STRUCT * IA_CSS_UINT8_T_BITS)

/* =============== Cached Param Terminal Manifest - START ============== */
struct ia_css_param_manifest_section_desc_s {
	/* Maximum size of the related parameter region */
	uint32_t max_mem_size;
	/* Indication of the kernel this parameter belongs to */
	uint8_t kernel_id;
	/* Memory targeted by this section
	 * (Register MMIO Interface/DMEM/VMEM/GMEM etc)
	 */
	uint8_t mem_type_id;
	/* Region id within the specified memory */
	uint8_t region_id;
	/* align to 64 */
	uint8_t padding[N_PADDING_UINT8_IN_PARAM_TERMINAL_MANIFEST_SEC_STRUCT];
};

typedef struct ia_css_param_manifest_section_desc_s
	ia_css_param_manifest_section_desc_t;


#define N_PADDING_UINT8_IN_PARAM_TERMINAL_MAN_STRUCT 4
#define SIZE_OF_PARAM_TERMINAL_MANIFEST_STRUCT_IN_BITS \
	(SIZE_OF_TERMINAL_MANIFEST_STRUCT_IN_BITS \
	+ (2*IA_CSS_UINT16_T_BITS) \
	+ (N_PADDING_UINT8_IN_PARAM_TERMINAL_MAN_STRUCT * IA_CSS_UINT8_T_BITS))

/* Frame constant parameters terminal manifest */
struct ia_css_param_terminal_manifest_s {
	/* Parameter terminal manifest base */
	ia_css_terminal_manifest_t base;
	/*
	 * Number of cached parameter sections, coming from manifest
	 * but also shared by the terminal
	 */
	uint16_t param_manifest_section_desc_count;
	/*
	 * Points to the variable array of
	 * struct ia_css_param_section_desc_s
	 */
	uint16_t param_manifest_section_desc_offset;
	/* align to 64 */
	uint8_t padding[N_PADDING_UINT8_IN_PARAM_TERMINAL_MAN_STRUCT];
};

typedef struct ia_css_param_terminal_manifest_s
	ia_css_param_terminal_manifest_t;
/* ================= Cached Param Terminal Manifest - End ================ */


/* ================= Spatial Param Terminal Manifest - START ============= */

#define SIZE_OF_FRAG_GRID_MAN_STRUCT_IN_BITS \
	((IA_CSS_N_DATA_DIMENSION*IA_CSS_UINT16_T_BITS) \
	+ (IA_CSS_N_DATA_DIMENSION*IA_CSS_UINT16_T_BITS))

struct ia_css_fragment_grid_manifest_desc_s {
	/* Min resolution width/height of the spatial parameters
	 * for the fragment measured in compute units
	 */
	uint16_t min_fragment_grid_dimension[IA_CSS_N_DATA_DIMENSION];
	/* Max resolution width/height of the spatial parameters
	 * for the fragment measured in compute units
	 */
	uint16_t max_fragment_grid_dimension[IA_CSS_N_DATA_DIMENSION];
};

typedef struct ia_css_fragment_grid_manifest_desc_s
	ia_css_fragment_grid_manifest_desc_t;

#define N_PADDING_UINT8_IN_FRAME_GRID_PARAM_MAN_SEC_STRUCT 1
#define SIZE_OF_FRAME_GRID_PARAM_MAN_SEC_STRUCT_IN_BITS \
	(1 * IA_CSS_UINT32_T_BITS \
	+ 3 * IA_CSS_UINT8_T_BITS \
	+ N_PADDING_UINT8_IN_FRAME_GRID_PARAM_MAN_SEC_STRUCT * IA_CSS_UINT8_T_BITS)

struct ia_css_frame_grid_param_manifest_section_desc_s {
	/* Maximum buffer total size allowed for
	 * this frame of parameters
	 */
	uint32_t max_mem_size;
	/* Memory space targeted by this section
	 * (Register MMIO Interface/DMEM/VMEM/GMEM etc)
	 */
	uint8_t mem_type_id;
	/* Region id within the specified memory space */
	uint8_t region_id;
	/* size in bytes of each compute unit for
	 * the specified memory space and region
	 */
	uint8_t elem_size;
	/* align to 64 */
	uint8_t padding[N_PADDING_UINT8_IN_FRAME_GRID_PARAM_MAN_SEC_STRUCT];
};

typedef struct ia_css_frame_grid_param_manifest_section_desc_s
	ia_css_frame_grid_param_manifest_section_desc_t;

#define SIZE_OF_FRAME_GRID_MAN_STRUCT_IN_BITS \
	((IA_CSS_N_DATA_DIMENSION*IA_CSS_UINT16_T_BITS) \
	+ (IA_CSS_N_DATA_DIMENSION*IA_CSS_UINT16_T_BITS))

struct ia_css_frame_grid_manifest_desc_s {
	/* Min resolution width/height of the spatial parameters for
	 * the frame measured in compute units
	 */
	uint16_t min_frame_grid_dimension[IA_CSS_N_DATA_DIMENSION];
	/* Max resolution width/height of the spatial parameters for
	 * the frame measured in compute units
	 */
	uint16_t max_frame_grid_dimension[IA_CSS_N_DATA_DIMENSION];
};

typedef struct ia_css_frame_grid_manifest_desc_s
	ia_css_frame_grid_manifest_desc_t;

#define N_PADDING_UINT8_IN_SPATIAL_PARAM_TERM_MAN_STRUCT 2
#define SIZE_OF_SPATIAL_PARAM_TERM_MAN_STRUCT_IN_BITS \
	((SIZE_OF_TERMINAL_MANIFEST_STRUCT_IN_BITS) \
	+ (SIZE_OF_FRAME_GRID_MAN_STRUCT_IN_BITS) \
	+ (SIZE_OF_FRAG_GRID_MAN_STRUCT_IN_BITS) \
	+ (2 * IA_CSS_UINT16_T_BITS) \
	+ (2 * IA_CSS_UINT8_T_BITS) \
	+ (N_PADDING_UINT8_IN_SPATIAL_PARAM_TERM_MAN_STRUCT * \
	IA_CSS_UINT8_T_BITS))

struct ia_css_spatial_param_terminal_manifest_s {
	/* Spatial Parameter terminal manifest base */
	ia_css_terminal_manifest_t base;
	/* Contains limits for the frame spatial parameters */
	ia_css_frame_grid_manifest_desc_t frame_grid_desc;
	/*
	 * Constains limits for the fragment spatial parameters
	 * - COMMON AMONG FRAGMENTS
	 */
	ia_css_fragment_grid_manifest_desc_t common_fragment_grid_desc;
	/*
	 * Number of frame spatial parameter sections, they are set
	 * in slice-steps through frame processing
	 */
	uint16_t frame_grid_param_manifest_section_desc_count;
	/*
	 * Points to the variable array of
	 * ia_css_frame_spatial_param_manifest_section_desc_t
	 */
	uint16_t frame_grid_param_manifest_section_desc_offset;
	/*
	 * Indication of the kernel this spatial parameter terminal belongs to
	 * SHOULD MATCH TO INDEX AND BE USED ONLY FOR CHECK
	 */
	uint8_t kernel_id;
	/*
	 * Groups together compute units in order to achieve alignment
	 * requirements for transfes and to achieve canonical frame
	 * representation
	 */
	uint8_t compute_units_p_elem;
	/* align to 64 */
	uint8_t padding[N_PADDING_UINT8_IN_SPATIAL_PARAM_TERM_MAN_STRUCT];
};

typedef struct ia_css_spatial_param_terminal_manifest_s
	ia_css_spatial_param_terminal_manifest_t;

/* ================= Spatial Param Terminal Manifest - END ================ */

/* ================= Sliced Param Terminal Manifest - START =============== */

#define N_PADDING_UINT8_IN_SLICED_TERMINAL_MAN_SECTION_STRUCT (2)
#define SIZE_OF_SLICED_PARAM_MAN_SEC_STRUCT_IN_BITS \
	(1 * IA_CSS_UINT32_T_BITS \
	+ 2 * IA_CSS_UINT8_T_BITS \
	+ N_PADDING_UINT8_IN_SLICED_TERMINAL_MAN_SECTION_STRUCT * IA_CSS_UINT8_T_BITS)

struct ia_css_sliced_param_manifest_section_desc_s {
	/* Maximum size of the related parameter region */
	uint32_t max_mem_size;
	/*
	 * Memory targeted by this section
	 * (Register MMIO Interface/DMEM/VMEM/GMEM etc)
	 */
	uint8_t mem_type_id;
	/* Region id within the specified memory */
	uint8_t region_id;
	/* align to 64 */
	uint8_t padding[N_PADDING_UINT8_IN_SLICED_TERMINAL_MAN_SECTION_STRUCT];
};

typedef struct ia_css_sliced_param_manifest_section_desc_s
	ia_css_sliced_param_manifest_section_desc_t;

#define N_PADDING_UINT8_IN_SLICED_TERMINAL_MANIFEST_STRUCT 3
#define SIZE_OF_SLICED_TERMINAL_MANIFEST_STRUCT_IN_BITS \
	(SIZE_OF_TERMINAL_MANIFEST_STRUCT_IN_BITS \
	+ 2 * IA_CSS_UINT16_T_BITS \
	+ 1 * IA_CSS_UINT8_T_BITS \
	+ N_PADDING_UINT8_IN_SLICED_TERMINAL_MANIFEST_STRUCT * IA_CSS_UINT8_T_BITS)

/* Frame constant parameters terminal manifest */
struct ia_css_sliced_param_terminal_manifest_s {
	/* Spatial Parameter terminal base */
	ia_css_terminal_manifest_t base;
	/*
	 * Number of the array elements
	 * sliced_param_section_offset points to
	 */
	uint16_t sliced_param_section_count;
	/*
	 * Points to array of ia_css_sliced_param_manifest_section_desc_s
	 * which constain info for the slicing of the parameters
	 */
	uint16_t sliced_param_section_offset;
	/* Kernel identifier */
	uint8_t kernel_id;
	/* align to 64 */
	uint8_t padding[N_PADDING_UINT8_IN_SLICED_TERMINAL_MANIFEST_STRUCT];
};

typedef struct ia_css_sliced_param_terminal_manifest_s
	ia_css_sliced_param_terminal_manifest_t;

/* ================= Slice Param Terminal Manifest - End =============== */

/* ================= Program Terminal Manifest - START ================= */

#define N_PADDING_UINT8_IN_FRAG_PARAM_MAN_SEC_STRUCT 1
#define SIZE_OF_FRAG_PARAM_MAN_SEC_STRUCT_IN_BITS \
	(1 * IA_CSS_UINT32_T_BITS \
	+ 3 * IA_CSS_UINT8_T_BITS \
	+ N_PADDING_UINT8_IN_FRAG_PARAM_MAN_SEC_STRUCT * IA_CSS_UINT8_T_BITS)

/* Fragment constant parameters manifest */
struct ia_css_fragment_param_manifest_section_desc_s {
	/* Maximum size of the related parameter region */
	uint32_t max_mem_size;
	/* Indication of the kernel this parameter belongs to */
	uint8_t kernel_id;
	/* Memory targeted by this section
	 * (Register MMIO Interface/DMEM/VMEM/GMEM etc)
	 */
	uint8_t mem_type_id;
	/* Region id within the specified memory space */
	uint8_t region_id;
	/* align to 64 */
	uint8_t padding[N_PADDING_UINT8_IN_FRAG_PARAM_MAN_SEC_STRUCT];
};

typedef struct ia_css_fragment_param_manifest_section_desc_s
	ia_css_fragment_param_manifest_section_desc_t;

#define SIZE_OF_KERNEL_FRAG_SEQ_INFO_MAN_STRUCT_IN_BITS \
	(10*IA_CSS_N_DATA_DIMENSION*IA_CSS_UINT16_T_BITS)

struct ia_css_kernel_fragment_sequencer_info_manifest_desc_s {
	/* Slice dimensions */
	uint16_t min_fragment_grid_slice_dimension[IA_CSS_N_DATA_DIMENSION];
	/* Slice dimensions */
	uint16_t max_fragment_grid_slice_dimension[IA_CSS_N_DATA_DIMENSION];
	/* Nof slices */
	uint16_t min_fragment_grid_slice_count[IA_CSS_N_DATA_DIMENSION];
	/* Nof slices */
	uint16_t max_fragment_grid_slice_count[IA_CSS_N_DATA_DIMENSION];
	/* Grid point decimation factor */
	uint16_t
	min_fragment_grid_point_decimation_factor[IA_CSS_N_DATA_DIMENSION];
	/* Grid point decimation factor */
	uint16_t
	max_fragment_grid_point_decimation_factor[IA_CSS_N_DATA_DIMENSION];
	/* Relative position of grid origin to pixel origin */
	int16_t
	min_fragment_grid_overlay_pixel_topleft_index[IA_CSS_N_DATA_DIMENSION];
	/* Relative position of grid origin to pixel origin */
	int16_t
	max_fragment_grid_overlay_pixel_topleft_index[IA_CSS_N_DATA_DIMENSION];
	/* Dimension of grid */
	int16_t
	min_fragment_grid_overlay_pixel_dimension[IA_CSS_N_DATA_DIMENSION];
	/* Dimension of grid */
	int16_t
	max_fragment_grid_overlay_pixel_dimension[IA_CSS_N_DATA_DIMENSION];
};

typedef struct ia_css_kernel_fragment_sequencer_info_manifest_desc_s
	ia_css_kernel_fragment_sequencer_info_manifest_desc_t;

#define N_PADDING_UINT8_IN_PROGRAM_TERM_MAN_STRUCT 2
#define SIZE_OF_PROG_TERM_MAN_STRUCT_IN_BITS \
	((SIZE_OF_TERMINAL_MANIFEST_STRUCT_IN_BITS) \
	+ (IA_CSS_UINT32_T_BITS) \
	+ (5*IA_CSS_UINT16_T_BITS) \
	+ (N_PADDING_UINT8_IN_PROGRAM_TERM_MAN_STRUCT * IA_CSS_UINT8_T_BITS))

struct ia_css_program_terminal_manifest_s {
	ia_css_terminal_manifest_t base;
	/* Connection manager passes seq info as single blob at the moment */
	uint32_t sequencer_info_kernel_id;
	/* Maximum number of command secriptors supported
	 * by the program group
	 */
	uint16_t max_kernel_fragment_sequencer_command_desc;
	uint16_t fragment_param_manifest_section_desc_count;
	uint16_t fragment_param_manifest_section_desc_offset;
	uint16_t kernel_fragment_sequencer_info_manifest_info_count;
	uint16_t kernel_fragment_sequencer_info_manifest_info_offset;
	/* align to 64 */
	uint8_t padding[N_PADDING_UINT8_IN_PROGRAM_TERM_MAN_STRUCT];
};

typedef struct ia_css_program_terminal_manifest_s
	ia_css_program_terminal_manifest_t;

/* ==================== Program Terminal Manifest - END ==================== */

#endif /* __IA_CSS_TERMINAL_MANIFEST_TYPES_H */
