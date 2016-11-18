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

#ifndef __IA_CSS_PSYS_PROGRAM_GROUP_MANIFEST_HSYS_USER_H
#define __IA_CSS_PSYS_PROGRAM_GROUP_MANIFEST_HSYS_USER_H

/*! \file */

/** @file ia_css_psys_program_group_manifest.hsys.user.h
 *
 * Define the methods on the program group manifest object: Hsys user interface
 */

#include <ia_css_psys_manifest_types.h>

#include <type_support.h>	/* bool */

/*! Print the program group manifest object to file/stream

 @param	manifest[in]		program group manifest object
 @param	fid[out]		file/stream handle

 @return < 0 on error
 */
extern int ia_css_program_group_manifest_print(
	const ia_css_program_group_manifest_t *manifest,
	void *fid);

/*! Read the program group manifest object from file/stream

 @param	fid[in]			file/stream handle

 @return NULL on error
 */
extern ia_css_program_group_manifest_t *ia_css_program_group_manifest_read(
	void *fid);

/*! Write the program group manifest object to file/stream

 @param	manifest[in]		program group manifest object
 @param	fid[out]		file/stream handle

 @return < 0 on error
 */
extern int ia_css_program_group_manifest_write(
	const ia_css_program_group_manifest_t *manifest,
	void *fid);

/*! Boolean test if the program group manifest is valid

 @param	manifest[in]		program group manifest

 @return true if program group manifest is correct, false on error
 */
extern bool ia_css_is_program_group_manifest_valid(
	const ia_css_program_group_manifest_t *manifest);

#endif /* __IA_CSS_PSYS_PROGRAM_GROUP_MANIFEST_HSYS_USER_H */



