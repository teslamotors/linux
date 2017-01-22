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

#ifndef __IA_CSS_PSYS_PROGRAM_GROUP_MANIFEST_SIM_H
#define __IA_CSS_PSYS_PROGRAM_GROUP_MANIFEST_SIM_H

/*! \file */

/** @file ia_css_psys_program_group_manifest.sim.h
 *
 * Define the methods on the program group manifest object: Simulation only
 */

#include <ia_css_psys_manifest_types.h>

#include <type_support.h>	/* uint8_t */
#include "ia_css_terminal_defs.h"

/*! Create a program group manifest object from specification

 @param	specification[in]		specification (index)

 @return NULL on error
 */
extern ia_css_program_group_manifest_t *ia_css_program_group_manifest_create(
	const unsigned int specification);

/*! Destroy the program group manifest object

 @param	manifest[in]			program group manifest

 @return NULL
 */
extern ia_css_program_group_manifest_t *ia_css_program_group_manifest_destroy(
	ia_css_program_group_manifest_t			*manifest);

/*! Compute the size of storage required for allocating
 * the program group (PG) manifest object

 @param	program_count[in]			Number of programs in the PG
 @param	terminal_count[in]			Number of terminals on the PG
 @param	program_dependency_count[in]		Array[program_count] with the PG
 @param	terminal_dependency_count[in]		Array[program_count] with the
						terminal dependencies
 @param	terminal_type[in]			Array[terminal_count] with the
						terminal type
 @param	cached_in_param_section_count[in]	Number of parameter
						in terminal sections
 @param	cached_out_param_section_count[in]	Number of parameter
						out terminal sections
 @param	sliced_param_section_count[in]		Array[sliced_terminal_count]
						with sections per
						sliced in terminal
 @param	sliced_out_param_section_count[in]	Array[sliced_terminal_count]
						with sections per
						sliced out terminal
 @param	spatial_param_section_count[in]		Array[spatial_terminal_count]
						with sections per
						spatial terminal
 @param	fragment_param_section_count[in]	Number of fragment parameter
						sections of the
						program init terminal,
 @param	kernel_fragment_seq_count[in]		Number of
						kernel_fragment_seq_count.

 @return 0 on error
 */
size_t ia_css_sizeof_program_group_manifest(
	const uint8_t			program_count,
	const uint8_t			terminal_count,
	const uint8_t			*program_dependency_count,
	const uint8_t			*terminal_dependency_count,
	const ia_css_terminal_type_t	*terminal_type,
	const uint16_t			cached_in_param_section_count,
	const uint16_t			cached_out_param_section_count,
	const uint16_t			*spatial_param_section_count,
	const uint16_t			fragment_param_section_count,
	const uint16_t			*sliced_param_section_count,
	const uint16_t			*sliced_out_param_section_count,
	const uint16_t			kernel_fragment_seq_count);

/*! Create (the storage for) the program group manifest object

 @param	program_count[in]		Number of programs in the program group
 @param	terminal_count[in]		Number of terminals on the program group
 @param	program_dependency_count[in]	Array[program_count] with the
					program dependencies
 @param	terminal_dependency_count[in]	Array[program_count] with the
					terminal dependencies
 @param	terminal_type[in]		Array[terminal_count] with the
					terminal type

 @return NULL on error
 */
extern ia_css_program_group_manifest_t *ia_css_program_group_manifest_alloc(
	const uint8_t			program_count,
	const uint8_t			terminal_count,
	const uint8_t			*program_dependency_count,
	const uint8_t			*terminal_dependency_count,
	const ia_css_terminal_type_t	*terminal_type);

/*! Free (the storage of) the program group manifest object

 @param	manifest[in]			program group manifest

 @return NULL
 */
extern ia_css_program_group_manifest_t *ia_css_program_group_manifest_free(
	ia_css_program_group_manifest_t *manifest);

#endif /* __IA_CSS_PSYS_PROGRAM_GROUP_MANIFEST_SIM_H */



