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

#ifndef __IA_CSS_PSYS_TERMINAL_MANIFEST_H
#define __IA_CSS_PSYS_TERMINAL_MANIFEST_H

/*! \file */

/** @file ia_css_psys_terminal_manifest.h
 *
 * Define the methods on the terminal manifest object that are not part of a
 * single interface
 */

#include <ia_css_psys_manifest_types.h>

#include <ia_css_psys_terminal_manifest.sim.h>

#include <ia_css_psys_terminal_manifest.hsys.user.h>

#include <ia_css_program_group_data.h>	/* ia_css_frame_format_bitmap_t */
#include <ia_css_kernel_bitmap.h>	/* ia_css_kernel_bitmap_t */

#include <type_support.h>		/* size_t */
#include "ia_css_terminal_manifest.h"
#include "ia_css_terminal_manifest_base_types.h"


/*! Check if the terminal manifest object specifies a spatial param terminal
 * type

 @param	manifest[in]			terminal manifest object

 @return is_parameter_terminal, false on invalid manifest argument
 */
extern bool ia_css_is_terminal_manifest_spatial_parameter_terminal(
	const ia_css_terminal_manifest_t		*manifest);

/*! Check if the terminal manifest object specifies a program terminal type

 @param	manifest[in]			terminal manifest object

 @return is_parameter_terminal, false on invalid manifest argument
 */
extern bool ia_css_is_terminal_manifest_program_terminal(
	const ia_css_terminal_manifest_t		*manifest);


/*! Check if the terminal manifest object specifies a program control init terminal type
 *
 * @param	manifest[in]			terminal manifest object
 *
 * @return is_parameter_terminal, false on invalid manifest argument
 */
extern bool ia_css_is_terminal_manifest_program_control_init_terminal(
	const ia_css_terminal_manifest_t		*manifest);

/*! Check if the terminal manifest object specifies a (cached) parameter
 * terminal type

 @param	manifest[in]			terminal manifest object

 @return is_parameter_terminal, false on invalid manifest argument
 */
extern bool ia_css_is_terminal_manifest_parameter_terminal(
	const ia_css_terminal_manifest_t		*manifest);

/*! Check if the terminal manifest object specifies a (sliced) parameter
 * terminal type

 @param	manifest[in]			terminal manifest object

 @return is_parameter_terminal, false on invalid manifest argument
 */
extern bool ia_css_is_terminal_manifest_sliced_terminal(
	const ia_css_terminal_manifest_t		*manifest);

/*! Check if the terminal manifest object specifies a data terminal type

 @param	manifest[in]			terminal manifest object

 @return is_data_terminal, false on invalid manifest argument
 */
extern bool ia_css_is_terminal_manifest_data_terminal(
	const ia_css_terminal_manifest_t		*manifest);

/*! Get the stored size of the terminal manifest object

 @param	manifest[in]			terminal manifest object

 @return size, 0 on invalid manifest argument
 */
extern size_t ia_css_terminal_manifest_get_size(
	const ia_css_terminal_manifest_t		*manifest);

/*! Get the (pointer to) the program group manifest parent of the terminal
 * manifest object

 @param	manifest[in]			terminal manifest object

 @return the pointer to the parent, NULL on invalid manifest argument
 */
extern ia_css_program_group_manifest_t *ia_css_terminal_manifest_get_parent(
	const ia_css_terminal_manifest_t		*manifest);

/*! Set the (pointer to) the program group manifest parent of the terminal
 * manifest object

 @param	manifest[in]			terminal manifest object
 @param	terminal_offset[in]		this terminal's offset from
					program_group_manifest base address.

 @return < 0 on invalid arguments
 */
extern int ia_css_terminal_manifest_set_parent_offset(
	ia_css_terminal_manifest_t			*manifest,
	int32_t						terminal_offset);

/*! Get the type of the terminal manifest object

 @param	manifest[in]			terminal manifest object

 @return terminal type, limit value (IA_CSS_N_TERMINAL_TYPES) on invalid
	manifest argument
*/
extern ia_css_terminal_type_t ia_css_terminal_manifest_get_type(
	const ia_css_terminal_manifest_t		*manifest);

/*! Set the type of the terminal manifest object

 @param	manifest[in]			terminal manifest object
 @param	terminal_type[in]		terminal type

 @return < 0 on invalid manifest argument
 */
extern int ia_css_terminal_manifest_set_type(
	ia_css_terminal_manifest_t			*manifest,
	const ia_css_terminal_type_t			terminal_type);

/*! Set the ID of the terminal manifest object

 @param	manifest[in]			terminal manifest object
 @param	ID[in]				terminal ID

 @return < 0 on invalid manifest argument
 */
int ia_css_terminal_manifest_set_ID(
	ia_css_terminal_manifest_t			*manifest,
	const ia_css_terminal_ID_t			ID);

/*! Get the type of the terminal manifest object

 @param	manifest[in]			terminal manifest object

 @return  terminal id, IA_CSS_TERMINAL_INVALID_ID on invalid manifest argument
 */
extern ia_css_terminal_ID_t ia_css_terminal_manifest_get_ID(
	const ia_css_terminal_manifest_t		*manifest);

/*! Get the supported frame types of the (data) terminal manifest object

 @param	manifest[in]			(data) terminal manifest object

 @return frame format bitmap, 0 on invalid manifest argument
*/
extern ia_css_frame_format_bitmap_t
	ia_css_data_terminal_manifest_get_frame_format_bitmap(
		const ia_css_data_terminal_manifest_t		*manifest);

/*! Set the chosen frame type for the (data) terminal manifest object

 @param	manifest[in]			(data) terminal manifest object
 @param	bitmap[in]			frame format bitmap

 @return < 0 on invalid manifest argument
 */
extern int ia_css_data_terminal_manifest_set_frame_format_bitmap(
	ia_css_data_terminal_manifest_t			*manifest,
	ia_css_frame_format_bitmap_t			bitmap);

/*! Check if the (data) terminal manifest object supports compression

 @param	manifest[in]			(data) terminal manifest object

 @return compression_support, true if compression is supported
 */
extern bool ia_css_data_terminal_manifest_can_support_compression(
	const ia_css_data_terminal_manifest_t		*manifest);

/*! Set the compression support feature of the (data) terminal manifest object

 @param	manifest[in]			(data) terminal manifest object
 @param	compression_support[in]		set true to support compression

 @return < 0 on invalid manifest argument
 */
extern int ia_css_data_terminal_manifest_set_compression_support(
	ia_css_data_terminal_manifest_t			*manifest,
	bool						compression_support);

/*! Set the supported connection types of the terminal manifest object

 @param	manifest[in]			(data) terminal manifest object
 @param	bitmap[in]			connection bitmap

 @return < 0 on invalid manifest argument
 */
extern int ia_css_data_terminal_manifest_set_connection_bitmap(
	ia_css_data_terminal_manifest_t *manifest, ia_css_connection_bitmap_t bitmap);

/*! Get the connection bitmap of the (data) terminal manifest object

 @param	manifest[in]			(data) terminal manifest object

 @return connection bitmap, 0 on invalid manifest argument
*/
extern ia_css_connection_bitmap_t
	ia_css_data_terminal_manifest_get_connection_bitmap(
		const ia_css_data_terminal_manifest_t		*manifest);

/*! Get the kernel dependency of the (data) terminal manifest object

 @param	manifest[in]			(data) terminal manifest object

 @return kernel bitmap, 0 on invalid manifest argument
 */
extern ia_css_kernel_bitmap_t ia_css_data_terminal_manifest_get_kernel_bitmap(
	const ia_css_data_terminal_manifest_t		*manifest);

/*! Set the kernel dependency of the (data) terminal manifest object

 @param	manifest[in]			(data) terminal manifest object
 @param	kernel_bitmap[in]		kernel dependency bitmap

 @return < 0 on invalid manifest argument
 */
extern int ia_css_data_terminal_manifest_set_kernel_bitmap(
	ia_css_data_terminal_manifest_t			*manifest,
	const ia_css_kernel_bitmap_t			kernel_bitmap);

/*! Set the unique kernel dependency of the (data) terminal manifest object

 @param	manifest[in]			(data) terminal manifest object
 @param	index[in]			kernel dependency bitmap index

 @return < 0 on invalid argument(s)
 */
extern int ia_css_data_terminal_manifest_set_kernel_bitmap_unique(
	ia_css_data_terminal_manifest_t			*manifest,
	const unsigned int				index);

/*! Set the min size of the (data) terminal manifest object

 @param	manifest[in]			(data) terminal manifest object
 @param	min_size[in]			Minimum size of the frame array

 @return < 0 on invalid manifest argument
 */
extern int ia_css_data_terminal_manifest_set_min_size(
	ia_css_data_terminal_manifest_t *manifest,
	const uint16_t min_size[IA_CSS_N_DATA_DIMENSION]);

/*! Set the max size of the (data) terminal manifest object

 @param	manifest[in]			(data) terminal manifest object
 @param	max_size[in]			Maximum size of the frame array

  @return < 0 on invalid manifest argument
  */
extern int ia_css_data_terminal_manifest_set_max_size(
	ia_css_data_terminal_manifest_t *manifest,
	const uint16_t max_size[IA_CSS_N_DATA_DIMENSION]);

/*! Get the min size of the (data) terminal manifest object

 @param	manifest[in]			(data) terminal manifest object
 @param	min_size[in]			Minimum size of the frame array

 @return < 0 on invalid manifest argument
 */
extern int ia_css_data_terminal_manifest_get_min_size(
	const ia_css_data_terminal_manifest_t *manifest,
	uint16_t min_size[IA_CSS_N_DATA_DIMENSION]);

/*! Get the max size of the (data) terminal manifest object

 @param	manifest[in]			(data) terminal manifest object
 @param	max_size[in]			Maximum size of the frame array

 @return < 0 on invalid manifest argument
 */
extern int ia_css_data_terminal_manifest_get_max_size(
	const ia_css_data_terminal_manifest_t *manifest,
	uint16_t max_size[IA_CSS_N_DATA_DIMENSION]);

/*! Set the min fragment size of the (data) terminal manifest object

 @param	manifest[in]			(data) terminal manifest object
 @param	min_size[in]			Minimum size of the fragment array

 @return < 0 on invalid manifest argument
 */
extern int ia_css_data_terminal_manifest_set_min_fragment_size(
	ia_css_data_terminal_manifest_t *manifest,
	const uint16_t min_size[IA_CSS_N_DATA_DIMENSION]);

/*! Set the max fragment size of the (data) terminal manifest object

 @param	manifest[in]			(data) terminal manifest object
 @param	max_size[in]			Maximum size of the fragment array

  @return < 0 on invalid manifest argument
  */
extern int ia_css_data_terminal_manifest_set_max_fragment_size(
	ia_css_data_terminal_manifest_t *manifest,
	const uint16_t max_size[IA_CSS_N_DATA_DIMENSION]);

/*! Get the min fragment size of the (data) terminal manifest object

 @param	manifest[in]			(data) terminal manifest object
 @param	min_size[in]			Minimum size of the fragment array

 @return < 0 on invalid manifest argument
 */
extern int ia_css_data_terminal_manifest_get_min_fragment_size(
	const ia_css_data_terminal_manifest_t *manifest,
	uint16_t min_size[IA_CSS_N_DATA_DIMENSION]);

/*! Get the max fragment size of the (data) terminal manifest object

 @param	manifest[in]			(data) terminal manifest object
 @param	max_size[in]			Maximum size of the fragment array

 @return < 0 on invalid manifest argument
 */
extern int ia_css_data_terminal_manifest_get_max_fragment_size(
	const ia_css_data_terminal_manifest_t *manifest,
	uint16_t max_size[IA_CSS_N_DATA_DIMENSION]);

/*!
 * Get the program control init connect section count for program prog.
 * @param prog[in] program control init terminal program desc
 * @return number of connect section for program prog.
 */

extern
unsigned int ia_css_program_control_init_terminal_manifest_get_connect_section_count(
	const ia_css_program_control_init_manifest_program_desc_t *prog);


/*!
 * Get the program control init load section count for program prog.
 * @param prog[in] program control init terminal program desc
 * @return number of load section for program prog.
 */

extern
unsigned int ia_css_program_control_init_terminal_manifest_get_load_section_count(
	const ia_css_program_control_init_manifest_program_desc_t *prog);

/*!
 * Get the program control init terminal manifest size.
 * @param nof_programs[in]		Number of programs.
 * @param nof_load_sections[in]		Array of size nof_programs,
 *					encoding the number of load sections.
 * @param nof_connect_sections[in]	Array of size nof_programs,
 *					encoding the number of connect sections.
 * @return < 0 on invalid manifest argument
 */
extern
unsigned int ia_css_program_control_init_terminal_manifest_get_size(
	const uint16_t nof_programs,
	const uint16_t *nof_load_sections,
	const uint16_t *nof_connect_sections);

/*!
 * Get the program control init terminal manifest program desc.
 * @param terminal[in]		Program control init terminal.
 * @param program[in]		Number of programs.
 * @return program control init terminal program desc (or NULL if error).
 */
extern
ia_css_program_control_init_manifest_program_desc_t *
ia_css_program_control_init_terminal_manifest_get_program_desc(
	const ia_css_program_control_init_terminal_manifest_t *terminal,
	unsigned int program);

/*!
 * Initialize the program control init terminal manifest.
 * @param nof_programs[in]		Number of programs
 * @param nof_load_sections[in]		Array of size nof_programs,
 *					encoding the number of load sections.
 * @param nof_connect_sections[in]	Array of size nof_programs,
 *					encoding the number of connect sections.
 * @return < 0 on invalid manifest argument
 */
extern
int ia_css_program_control_init_terminal_manifest_init(
	ia_css_program_control_init_terminal_manifest_t *terminal,
	const uint16_t nof_programs,
	const uint16_t *nof_load_sections,
	const uint16_t *nof_connect_sections);

/*!
 * Pretty prints the program control init terminal manifest.
 * @param terminal[in]		Program control init terminal.
 */
extern
void ia_css_program_control_init_terminal_manifest_print(
	ia_css_program_control_init_terminal_manifest_t *terminal);

#endif /* __IA_CSS_PSYS_TERMINAL_MANIFEST_H */
