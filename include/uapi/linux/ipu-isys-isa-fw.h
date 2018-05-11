/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2014 - 2018 Intel Corporation */

#ifndef IPU_ISYS_ISA_FW_H
#define IPU_ISYS_ISA_FW_H

#define ia_css_terminal_offsets(pg)			\
	((uint16_t *)((void *)(pg) +			\
		      (pg)->terminals_offset_offset))

#define to_ia_css_terminal(pg, i)				\
	((struct ia_css_terminal *)(				\
		 (void *)(pg) + ia_css_terminal_offsets(pg)[i]))

#define ia_css_terminal_offset(pg, i)					\
	(!(i) ? sizeof *(pg) + ((((pg)->terminal_count - 1) | 3) + 1)	\
	 * sizeof(uint16_t) :						\
	 ia_css_terminal_offsets(pg)[(i) - 1]				\
	 + to_ia_css_terminal(pg, (i) - 1)->size)

/* BEGIN DEFINITIONS IMPORTED FROM FIRMWARE */

#define N_IPU_FW_ISYS_KERNEL_ID 20

struct ia_css_process_group_light {
	uint32_t size;
	uint16_t terminals_offset_offset;
	uint16_t terminal_count;
};

enum ia_css_terminal_type {
	IPU_FW_TERMINAL_TYPE_DATA_IN = 0,
	IPU_FW_TERMINAL_TYPE_DATA_OUT,
	IPU_FW_TERMINAL_TYPE_PARAM_STREAM,
	IPU_FW_TERMINAL_TYPE_PARAM_CACHED_IN,
	IPU_FW_TERMINAL_TYPE_PARAM_CACHED_OUT,
	IPU_FW_TERMINAL_TYPE_PARAM_SPATIAL_IN,
	IPU_FW_TERMINAL_TYPE_PARAM_SPATIAL_OUT,
	IPU_FW_TERMINAL_TYPE_PARAM_SLICED_IN,
	IPU_FW_TERMINAL_TYPE_PARAM_SLICED_OUT,
	IPU_FW_TERMINAL_TYPE_STATE_IN,
	IPU_FW_TERMINAL_TYPE_STATE_OUT,
	IPU_FW_TERMINAL_TYPE_PROGRAM,
	IPU_FW_N_TERMINAL_TYPES
};

struct ia_css_terminal {
	enum ia_css_terminal_type terminal_type;
	int16_t parent_offset;
	uint16_t size;
	uint16_t tm_index;
	uint8_t id;
	uint8_t padding[5];
};

struct ia_css_param_payload {
	uint64_t host_buffer;
	uint32_t buffer;
	uint8_t padding[4];
};

struct ia_css_param_section_desc {
	uint32_t mem_offset;
	uint32_t mem_size;
};

struct ia_css_param_terminal {
	struct ia_css_terminal base;
	struct ia_css_param_payload param_payload;
	uint16_t param_section_desc_offset;
	uint8_t padding[6];
};

struct ia_css_program_terminal {
	struct ia_css_terminal base;
	struct ia_css_param_payload param_payload;
	uint16_t fragment_param_section_desc_offset;
	uint16_t kernel_fragment_sequencer_info_desc_offset;
	uint8_t padding[4];
};

struct ia_css_sliced_param_terminal {
	struct ia_css_terminal base;
	struct ia_css_param_payload param_payload;
	uint32_t kernel_id;
	uint16_t fragment_slice_desc_offset;
	uint8_t padding[2];
};

enum ia_css_dimension {
	IPU_FW_COL_DIMENSION = 0,
	IPU_FW_ROW_DIMENSION = 1,
	IPU_FW_N_DATA_DIMENSION = 2
};

struct ia_css_frame_grid_desc {
	uint16_t frame_grid_dimension[IPU_FW_N_DATA_DIMENSION];
	uint8_t padding[4];
};

struct ia_css_spatial_param_terminal {
	struct ia_css_terminal base;
	struct ia_css_param_payload param_payload;
	struct ia_css_frame_grid_desc frame_grid_desc;
	uint32_t kernel_id;
	uint16_t frame_grid_param_section_desc_offset;
	uint16_t fragment_grid_desc_offset;
};

/* END DEFINITIONS IMPORTED FROM FIRMWARE */

#endif /* IPU_ISYS_ISA_FW_H */
