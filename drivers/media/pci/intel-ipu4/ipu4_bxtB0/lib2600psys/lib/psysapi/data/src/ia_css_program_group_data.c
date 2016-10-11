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

#include "ia_css_psys_data_storage_class.h"

/*
 * Functions to possibly inline
 */

#ifdef _IA_CSS_PSYS_DATA_INLINE_
STORAGE_CLASS_INLINE int
__ia_css_program_group_data_avoid_warning_on_empty_file(void) { return 0; }
#else /* _IA_CSS_PSYS_DATA_INLINE_ */
#include "ia_css_program_group_data_impl.h"
#endif /* _IA_CSS_PSYS_DATA_INLINE_ */
