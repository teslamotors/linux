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

#ifndef __IA_CSS_TERMINAL_MANIFEST_BASE_TYPES_H
#define __IA_CSS_TERMINAL_MANIFEST_BASE_TYPES_H

#include "ia_css_terminal_defs.h"

#define N_PADDING_UINT8_IN_TERMINAL_MAN_STRUCT	5
#define SIZE_OF_TERMINAL_MANIFEST_STRUCT_IN_BITS \
	(IA_CSS_UINT16_T_BITS \
	+ IA_CSS_TERMINAL_ID_BITS \
	+ IA_CSS_TERMINAL_TYPE_BITS \
	+ IA_CSS_UINT32_T_BITS \
	+ (N_PADDING_UINT8_IN_TERMINAL_MAN_STRUCT*IA_CSS_UINT8_T_BITS))

/* ==================== Base Terminal Manifest - START ==================== */
struct ia_css_terminal_manifest_s {
	ia_css_terminal_type_t				terminal_type;		/**< Type ia_css_terminal_type_t */
	int16_t						parent_offset;		/**< Offset to the program group manifest */
	uint16_t					size;			/**< Size of this whole terminal-manifest layout-structure */
	ia_css_terminal_ID_t				ID;
	uint8_t						padding[N_PADDING_UINT8_IN_TERMINAL_MAN_STRUCT];
};

typedef struct ia_css_terminal_manifest_s
	ia_css_terminal_manifest_t;

/* ==================== Base Terminal Manifest - END ==================== */

#endif /* __IA_CSS_TERMINAL_MANIFEST_BASE_TYPES_H */

