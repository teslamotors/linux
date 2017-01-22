/**
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

#ifndef __IA_CSS_TERMINAL_BASE_TYPES_H
#define __IA_CSS_TERMINAL_BASE_TYPES_H


#include "type_support.h"
#include "ia_css_terminal_defs.h"

#define N_UINT16_IN_TERMINAL_STRUCT		3
#define N_PADDING_UINT8_IN_TERMINAL_STRUCT	5

#define SIZE_OF_TERMINAL_STRUCT_BITS \
	(IA_CSS_TERMINAL_TYPE_BITS \
	+ IA_CSS_TERMINAL_ID_BITS  \
	+ N_UINT16_IN_TERMINAL_STRUCT * IA_CSS_UINT16_T_BITS \
	+ N_PADDING_UINT8_IN_TERMINAL_STRUCT * IA_CSS_UINT8_T_BITS)

/* ==================== Base Terminal - START ==================== */
struct ia_css_terminal_s {						/**< Base terminal */
	ia_css_terminal_type_t			terminal_type;		/**< Type ia_css_terminal_type_t */
	int16_t					parent_offset;		/**< Offset to the process group */
	uint16_t				size;			/**< Size of this whole terminal layout-structure */
	uint16_t				tm_index;		/**< Index of the terminal manifest object */
	ia_css_terminal_ID_t			ID;			/**< Absolute referal ID for this terminal, valid ID's != 0 */
	uint8_t					padding[N_PADDING_UINT8_IN_TERMINAL_STRUCT];
};
/* ==================== Base Terminal - END ==================== */

#endif /* __IA_CSS_TERMINAL_BASE_TYPES_H */

