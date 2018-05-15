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

#ifdef __INLINE_IA_CSS_CELL_PROGRAM_LOAD__

#include "storage_class.h"
STORAGE_CLASS_INLINE void __dummy(void) { }

#else

/* low-level functions */
#include "ia_css_cell_program_load_prog_impl.h"

/* functions for single, unmapped program load */
#include "ia_css_cell_program_load_impl.h"

/* functions for program group load */
#include "ia_css_cell_program_group_load_impl.h"

#endif
