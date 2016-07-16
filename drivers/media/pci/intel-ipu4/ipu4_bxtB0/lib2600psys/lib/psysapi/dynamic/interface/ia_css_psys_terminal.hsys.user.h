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

#ifndef __IA_CSS_PSYS_TERMINAL_HSYS_USER_H_INCLUDED__
#define __IA_CSS_PSYS_TERMINAL_HSYS_USER_H_INCLUDED__

/*! \file */

/** @file ia_css_psys_terminal.hsys.user.h
 *
 * Define the methods on the terminal object: Hsys user interface
 */

#include <ia_css_program_group_data.h>	/* ia_css_frame_t */
#include <ia_css_program_group_param.h>	/* ia_css_program_group_param_t */

#include <ia_css_psys_process_types.h>
#include <ia_css_psys_manifest_types.h>

#include <type_support.h>					/* bool */
#include "ia_css_terminal.h"
#include "ia_css_terminal_manifest.h"
#include "ia_css_kernel_bitmap.h"

/*
 * Creation
 */

/*! Compute the size of storage required for allocating the terminal object

 @param	manifest[in]			terminal manifest
 @param	param[in]				program group parameters

 @return 0 on error
 */
extern size_t ia_css_sizeof_terminal(
	const ia_css_terminal_manifest_t		*manifest,
	const ia_css_program_group_param_t		*param);

/*! Create the terminal object

 @param	raw_mem[in]				pre allocated memory
 @param	manifest[in]			terminal manifest
 @param	terminal_param[in]		terminal parameter
 @param enable_bitmap			program group enable bitmap

 @return NULL on error
 */
extern ia_css_terminal_t *ia_css_terminal_create(
	void *raw_mem,
	const ia_css_terminal_manifest_t		*manifest,
	const ia_css_terminal_param_t			*terminal_param,
	ia_css_kernel_bitmap_t				enable_bitmap);

/*! Destroy (the storage of) the process object

 @param	terminal[in]			terminal object

 @return NULL
 */
extern ia_css_terminal_t *ia_css_terminal_destroy(
	ia_css_terminal_t *terminal);

/*! Print the terminal object to file/stream

 @param	terminal[in]			terminal object
 @param	fid[out]				file/stream handle

 @return < 0 on error
 */
extern int ia_css_terminal_print(
	const ia_css_terminal_t					*terminal,
	void *fid);

/*! Get the (pointer to) the frame object in the terminal object

 @param	terminal[in]			terminal object

 @return the pointer to the frame, NULL on error
 */
extern ia_css_frame_t *ia_css_data_terminal_get_frame(
	const ia_css_data_terminal_t		*terminal);

/*! Get the (pointer to) the frame descriptor object in the terminal object

 @param	terminal[in]			terminal object

 @return the pointer to the frame descriptor, NULL on error
 */
extern ia_css_frame_descriptor_t *ia_css_data_terminal_get_frame_descriptor(
	const ia_css_data_terminal_t		*dterminal);

/*! Get the (pointer to) the fragment descriptor object in the terminal object

 @param	terminal[in]			terminal object

@return the pointer to the fragment descriptor, NULL on error
*/
extern ia_css_fragment_descriptor_t
	*ia_css_data_terminal_get_fragment_descriptor(
		const ia_css_data_terminal_t		*dterminal,
		const unsigned int			fragment_index);

/*! Get the number of fragments on the terminal

 @param	terminal[in]			terminal object

 @return the fragment count, 0 on error
 */
extern uint16_t ia_css_data_terminal_get_fragment_count(
	const ia_css_data_terminal_t		*dterminal);

/*! Get the number of section on the (param)terminal
 @param	manifest[in]			terminal manifest
 @param	terminal_param[in]		terminal parameter

 @return the section count, 0 on error
 */
extern uint16_t ia_css_param_terminal_compute_section_count(
	const ia_css_terminal_manifest_t	*manifest,
	const ia_css_program_group_param_t	*param);

/*! Get the number of planes on the (data)terminal
 @param	manifest[in]			terminal manifest
 @param	terminal_param[in]		terminal parameter

 @return the plane count, 1(default) on error
 */
extern uint8_t ia_css_data_terminal_compute_plane_count(
	const ia_css_terminal_manifest_t		*manifest,
	const ia_css_program_group_param_t		*param);

/*! check if given terminal is parameter terminal.

 @param	terminal[in]			(base)terminal object

 @return true on success, false on error
 */
extern bool ia_css_is_terminal_parameter_terminal(
	const ia_css_terminal_t					*terminal);

/*! check if given terminal is program terminal.

 @program	terminal[in]			(base)terminal object

 @return true on success, false on error
 */
extern bool ia_css_is_terminal_program_terminal(
	const ia_css_terminal_t					*terminal);

/*! check if given terminal is spatial parameter terminal.

 @spatial	terminal[in]			(base)terminal object

 @return true on success, false on error
 */
extern bool ia_css_is_terminal_spatial_parameter_terminal(
	const ia_css_terminal_t					*terminal);

/*! check if given terminal is data terminal.

 @param	terminal[in]			(base)terminal object

 @return true on success, false on error
 */
extern bool ia_css_is_terminal_data_terminal(
	const ia_css_terminal_t					*terminal);

/*! check if given terminal is spatial paramter terminal.

 @param	terminal[in]			(base)terminal object

 @return true on success, false on error
 */
extern bool ia_css_is_terminal_spatial_param_terminal(
	const ia_css_terminal_t					*terminal);

/*! obtain buffer out of terminal(both data & param terminals can call this)

 @param	terminal[in]	(base)terminal object of either data or param terminal.

 @return vied address of buffer stored in terminal
 */
extern vied_vaddress_t  ia_css_terminal_get_buffer(
		const ia_css_terminal_t *terminal);

/*!store a buffer in the terminal.

 @param	terminal[in]	(base)terminal object of either data or param terminal.
 @param buffer[in]	buffer in vied (hrt address) space.

 @return vied address of buffer stored in terminal
 */
extern int ia_css_terminal_set_buffer(ia_css_terminal_t *terminal,
				vied_vaddress_t buffer);
#endif /* __IA_CSS_PSYS_TERMINAL_HSYS_USER_H_INCLUDED__ */
