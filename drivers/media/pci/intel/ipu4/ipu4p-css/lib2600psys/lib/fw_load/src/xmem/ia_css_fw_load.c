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

/* C file with (optionally) inlined files */

/* Global variable for tracking the number of fw_load transactions */
/* Needed in host side implementaion */
#ifndef __VIED_CELL
unsigned int started;
#endif

#ifdef __INLINE_IA_CSS_FW_LOAD__
static inline int __avoid_warning_on_empty_file(void) { return 0; }
#else
#include "ia_css_fw_load_blocking_impl.h"
#include "ia_css_fw_load_non_blocking_impl.h"
#include "ia_css_fw_load_impl.h"
#endif /* __INLINE_IA_CSS_FW_LOAD__ */

