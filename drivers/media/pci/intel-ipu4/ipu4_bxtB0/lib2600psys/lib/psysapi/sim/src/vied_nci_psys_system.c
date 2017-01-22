/*
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

#include "ia_css_psys_sim_storage_class.h"

/*
 * Functions to possibly inline
 */

#ifdef __IA_CSS_PSYS_SIM_INLINE__
STORAGE_CLASS_INLINE int
__ia_css_psys_system_global_avoid_warning_on_empty_file(void) { return 0; }
#else /* __IA_CSS_PSYS_SIM_INLINE__ */
#include "psys_system_global_impl.h"
#endif /* __IA_CSS_PSYS_SIM_INLINE__ */
