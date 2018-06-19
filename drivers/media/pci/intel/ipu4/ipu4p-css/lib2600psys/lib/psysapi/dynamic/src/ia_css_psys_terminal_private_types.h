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

#ifndef __IA_CSS_PSYS_TERMINAL_PRIVATE_TYPES_H
#define __IA_CSS_PSYS_TERMINAL_PRIVATE_TYPES_H

#include "ia_css_terminal_types.h"
#include "ia_css_program_group_data.h"
#include "ia_css_psys_manifest_types.h"

#define	N_UINT16_IN_DATA_TERMINAL_STRUCT	1
#define	N_UINT8_IN_DATA_TERMINAL_STRUCT		3
#define	N_PADDING_UINT8_IN_DATA_TERMINAL_STRUCT	3

/* ========================= Data terminal - START ========================= */

#define SIZE_OF_DATA_TERMINAL_STRUCT_BITS \
	(SIZE_OF_TERMINAL_STRUCT_BITS \
	+ IA_CSS_FRAME_DESCRIPTOR_STRUCT_BITS \
	+ IA_CSS_FRAME_STRUCT_BITS \
	+ IA_CSS_STREAM_STRUCT_BITS \
	+ IA_CSS_UINT32_T_BITS \
	+ IA_CSS_CONNECTION_TYPE_BITS \
	+ (N_UINT16_IN_DATA_TERMINAL_STRUCT * 16) \
	+ (N_UINT8_IN_DATA_TERMINAL_STRUCT * 8)	\
	+ (N_PADDING_UINT8_IN_DATA_TERMINAL_STRUCT * 8))

/*
 * The (data) terminal can be attached to a buffer or a stream.
 * The stream interface is not necessarily limited to strict in-order access.
 * For a stream the restriction is that contrary to a buffer it cannot be
 * addressed directly, i.e. it behaves as a port,
 * but it may support stream_pos() and/or seek() operations
 */
struct ia_css_data_terminal_s {
	/**< Data terminal base */
	ia_css_terminal_t base;
	/**< Properties of the data attached to the terminal */
	ia_css_frame_descriptor_t frame_descriptor;
	/**< Data buffer handle attached to the terminal */
	ia_css_frame_t frame;
	/**< (exclusive) Data stream handle attached to the terminal
	 * if the data is sourced over a device port
	 */
	ia_css_stream_t stream;
	/**< Reserved */
	uint32_t reserved;
	/**< Connection {buffer, stream, ...} */
	ia_css_connection_type_t connection_type;
	/**< Array[fragment_count] (fragment_count being equal for all
	 * terminals in a subgraph) of fragment descriptors
	 */
	uint16_t fragment_descriptor_offset;
	/**< Kernel id where this terminal is connected to */
	uint8_t kernel_id;
	/**< Indicate to which subgraph this terminal belongs
	 * for common constraints
	 */
	uint8_t subgraph_id;
	/* Link ID of the data terminal */
	uint8_t link_id;
	/**< Padding for 64bit alignment */
	uint8_t padding[N_PADDING_UINT8_IN_DATA_TERMINAL_STRUCT];
};
/* ========================== Data terminal - END ========================== */

/* ================= Program Control Init Terminal - START ================= */
#define SIZE_OF_PROG_CONTROL_INIT_LOAD_SECTION_DESC_STRUCT_BITS	\
	(DEVICE_DESCRIPTOR_ID_BITS	\
	+ (3 * IA_CSS_UINT32_T_BITS)	\
	)
struct ia_css_program_control_init_load_section_desc_s {
	/* Offset of the parameter allocation in memory */
	uint32_t mem_offset;
	/* Memory allocation size needs of this parameter */
	uint32_t mem_size;
	/* Device descriptor */
	device_descriptor_id_t device_descriptor_id;	/* 32 bits */
	/* (Applicable to) mode bitmask */
	uint32_t mode_bitmask;
};

#define MODE_BITMASK_MEMORY          (1u << IA_CSS_CONNECTION_MEMORY)
#define MODE_BITMASK_MEMORY_STREAM   (1u << IA_CSS_CONNECTION_MEMORY_STREAM)
#define MODE_BITMASK_STREAM          (1u << IA_CSS_CONNECTION_STREAM)
#define MODE_BITMASK_DONT_CARE       (MODE_BITMASK_MEMORY | MODE_BITMASK_MEMORY_STREAM | MODE_BITMASK_STREAM)

#define N_PADDING_UINT8_IN_PROG_CTRL_INIT_CONNECT_SECT_STRUCT (5)
#define SIZE_OF_PROG_CONTROL_INIT_CONNECT_SECTION_DESC_STRUCT_BITS	\
	(DEVICE_DESCRIPTOR_ID_BITS	\
	+ (1 * IA_CSS_UINT32_T_BITS)	\
	+ (1 * IA_CSS_UINT16_T_BITS)	\
	+ IA_CSS_TERMINAL_ID_BITS	\
	+ (N_PADDING_UINT8_IN_PROG_CTRL_INIT_CONNECT_SECT_STRUCT * \
	   IA_CSS_UINT8_T_BITS)		\
	)
struct ia_css_program_control_init_connect_section_desc_s {
	/* Device descriptor */
	device_descriptor_id_t device_descriptor_id;	/* 32 bits */
	/* (Applicable to) mode bitmask */
	uint32_t mode_bitmask;
	/* Connected terminal section (plane) index */
	uint16_t connect_section_idx;
	/* Absolute referral ID for the connected terminal */
	ia_css_terminal_ID_t connect_terminal_ID;
	/* align to 64 */
	uint8_t padding[N_PADDING_UINT8_IN_PROG_CTRL_INIT_CONNECT_SECT_STRUCT];
};

#define N_PADDING_UINT8_IN_PROG_DESC_CONTROL_INFO	(1)
#define N_PADDING_UINT8_IN_PROG_CTRL_INIT_PROGRAM_DESC_STRUCT (4)
#define SIZE_OF_PROGRAM_DESC_CONTROL_INFO_STRUCT_BITS \
	(1 * IA_CSS_UINT16_T_BITS)	\
	+ (1 * IA_CSS_UINT8_T_BITS)	\
	+ (N_PADDING_UINT8_IN_PROG_DESC_CONTROL_INFO * IA_CSS_UINT8_T_BITS)

#define SIZE_OF_PROG_CONTROL_INIT_PROG_DESC_STRUCT_BITS	\
	(4 * IA_CSS_UINT16_T_BITS)	\
	+ (SIZE_OF_PROGRAM_DESC_CONTROL_INFO_STRUCT_BITS) \
	+ (N_PADDING_UINT8_IN_PROG_CTRL_INIT_PROGRAM_DESC_STRUCT * \
		IA_CSS_UINT8_T_BITS)

struct ia_css_program_desc_control_info_s {
	/* 12-bit process identifier */
	ia_css_process_id_t process_id;
	/* number of done acks required to close the process */
	uint8_t num_done_events;
	uint8_t padding[N_PADDING_UINT8_IN_PROG_DESC_CONTROL_INFO];
};

struct ia_css_program_control_init_program_desc_s {
	/* Number of load sections in this program */
	uint16_t load_section_count;
	/* Points to variable size array of
	 * ia_css_program_control_init_load_section_desc_s
	 * in relation to its program_desc
	 */
	uint16_t load_section_desc_offset;
	/* Number of connect sections in this program */
	uint16_t connect_section_count;
	/* Points to variable size array of
	 * ia_css_program_control_init_connect_section_desc_s
	 * in relation to its program_desc
	 */
	uint16_t connect_section_desc_offset;
	struct ia_css_program_desc_control_info_s control_info;
	/* align to 64 bits */
	uint8_t padding[N_PADDING_UINT8_IN_PROG_CTRL_INIT_PROGRAM_DESC_STRUCT];
};

#define SIZE_OF_PROG_CONTROL_INIT_TERM_STRUCT_BITS \
	(SIZE_OF_TERMINAL_STRUCT_BITS \
	+ IA_CSS_PARAM_PAYLOAD_STRUCT_BITS \
	+ (1 * IA_CSS_UINT32_T_BITS) \
	+ (2 * IA_CSS_UINT16_T_BITS) \
	)
struct ia_css_program_control_init_terminal_s {
	/* Parameter terminal base */
	ia_css_terminal_t base;
	/* Parameter buffer handle attached to the terminal */
	ia_css_param_payload_t param_payload;
	/* Fragment stride for the payload, used to find the base
	 * of the payload for a given fragment
	 */
	uint32_t payload_fragment_stride;
	/* Points to the variable array of
	 * ia_css_program_control_init_program_desc_s
	 */
	uint16_t program_section_desc_offset;
	/* Number of instantiated programs in program group (processes) */
	uint16_t program_count;
};
/* ================= Program Control Init Terminal - END ================= */

#endif /* __IA_CSS_PSYS_TERMINAL_PRIVATE_TYPES_H */
