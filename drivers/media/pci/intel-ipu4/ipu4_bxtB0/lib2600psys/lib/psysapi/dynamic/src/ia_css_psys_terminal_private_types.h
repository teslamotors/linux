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

#ifndef __IA_CSS_PSYS_TERMINAL_PRIVATE_TYPES_H
#define __IA_CSS_PSYS_TERMINAL_PRIVATE_TYPES_H

#include "ia_css_terminal_types.h"
#include "ia_css_program_group_data.h"
#include "ia_css_psys_manifest_types.h"

#define	N_UINT16_IN_DATA_TERMINAL_STRUCT	1
#define	N_UINT8_IN_DATA_TERMINAL_STRUCT		2
#define	N_PADDING_UINT8_IN_DATA_TERMINAL_STRUCT	4

#define SIZE_OF_DATA_TERMINAL_STRUCT_BITS \
	(SIZE_OF_TERMINAL_STRUCT_BITS \
	+ IA_CSS_FRAME_DESCRIPTOR_STRUCT_BITS \
	+ IA_CSS_FRAME_STRUCT_BITS \
	+ IA_CSS_STREAM_STRUCT_BITS \
	+ IA_CSS_FRAME_FORMAT_TYPE_BITS \
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
	/**< Indicates if this is a generic type or inbuild
	 * with variable size descriptor
	 */
	ia_css_frame_format_type_t frame_format_type;
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
	/**< Padding for 64bit alignment */
	uint8_t padding[N_PADDING_UINT8_IN_DATA_TERMINAL_STRUCT];
};

#endif /* __IA_CSS_PSYS_TERMINAL_PRIVATE_TYPES_H */
