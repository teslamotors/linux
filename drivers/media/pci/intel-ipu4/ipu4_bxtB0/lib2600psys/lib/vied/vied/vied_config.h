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
#ifndef _HRT_VIED_CONFIG_H
#define _HRT_VIED_CONFIG_H

/* Defines from the compiler:
 *   HRT_HOST - this is code running on the host
 *   HRT_CELL - this is code running on a cell
 */
#ifdef HRT_HOST
# define CFG_VIED_SUBSYSTEM_ACCESS_LIB_IMPL 1
# undef CFG_VIED_SUBSYSTEM_ACCESS_INLINE_IMPL

#elif defined (HRT_CELL)
# undef CFG_VIED_SUBSYSTEM_ACCESS_LIB_IMPL
# define CFG_VIED_SUBSYSTEM_ACCESS_INLINE_IMPL 1

#else  /* !HRT_CELL */
/* Allow neither HRT_HOST nor HRT_CELL for testing purposes */
#endif /* !HRT_CELL */

#endif /* _HRT_VIED_CONFIG_H */
