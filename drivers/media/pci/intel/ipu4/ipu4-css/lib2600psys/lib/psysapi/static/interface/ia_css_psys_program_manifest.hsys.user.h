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

#ifndef __IA_CSS_PSYS_PROGRAM_MANIFEST_HSYS_USER_H
#define __IA_CSS_PSYS_PROGRAM_MANIFEST_HSYS_USER_H

/*! \file */

/** @file ia_css_psys_program_manifest.hsys.user.h
 *
 * Define the methods on the program manifest object: Hsys user interface
 */

#include <ia_css_psys_manifest_types.h>

/*! Print the program manifest object to file/stream

 @param	manifest[in]			program manifest object
 @param	fid[out]				file/stream handle

 @return < 0 on error
 */
extern int ia_css_program_manifest_print(
	const ia_css_program_manifest_t	*manifest,
	void				*fid);

#endif /* __IA_CSS_PSYS_PROGRAM_MANIFEST_HSYS_USER_H */
