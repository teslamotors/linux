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

#ifndef __IA_CSS_PSYS_TERMINAL_H_INCLUDED__
#define __IA_CSS_PSYS_TERMINAL_H_INCLUDED__

/*! \file */

/** @file ia_css_psys_terminal.h
 *
 * Define the methods on the terminal object that are not part of
 * a single interface
 */

#include <ia_css_program_group_data.h>	/* ia_css_frame_t */
#include <ia_css_program_group_param.h>	/* ia_css_program_group_param_t */

#include <ia_css_psys_process_types.h>
#include <ia_css_psys_manifest_types.h>

#include <type_support.h>		/* bool */
#include <print_support.h>		/* FILE */
#include "ia_css_terminal.h"
#include "ia_css_terminal_manifest_base_types.h"
/*
 * Creation
 */
#include <ia_css_psys_terminal.hsys.user.h>

/*! Boolean test if the terminal object type is input

 @param	terminal[in]			terminal object

 @return true if the terminal is input, false otherwise or on error
 */
extern bool ia_css_is_terminal_input(
	const ia_css_terminal_t					*terminal);

/*! Get the stored size of the terminal object

 @param	terminal[in]			terminal object

 @return size, 0 on error
 */
extern size_t ia_css_terminal_get_size(
	const ia_css_terminal_t					*terminal);

/*! Get the type of the terminal object

 @param	terminal[in]			terminal object

 @return the type of the terminal, limit value on error
 */
extern ia_css_terminal_type_t ia_css_terminal_get_type(
	const ia_css_terminal_t					*terminal);

/*! Set the type of the terminal object

 @param	terminal[in]			terminal object
 @param	terminal_type[in]		type of the terminal

 @return < 0 on error
 */
extern int ia_css_terminal_set_type(
	ia_css_terminal_t		*terminal,
	const ia_css_terminal_type_t	terminal_type);

/*! Get the index of the terminal manifest object

 @param	terminal[in]			terminal object

 @return the index of the terminal manifest object, limit value on error
 */
extern uint16_t ia_css_terminal_get_terminal_manifest_index(
	const ia_css_terminal_t					*terminal);

/*! Set the index of the terminal manifest object

 @param	terminal[in]			terminal object
 @param	tm_index[in]			terminal manifest index

 @return < 0 on error
 */
extern int ia_css_terminal_set_terminal_manifest_index(
	ia_css_terminal_t	*terminal,
	const uint16_t		tm_index);

/*! Get id of the terminal object

 @param	terminal[in]			terminal object

 @return id of terminal
 */
ia_css_terminal_ID_t ia_css_terminal_get_ID(
	const ia_css_terminal_t			*terminal);

/*! Get kernel id of the data terminal object

 @param	dterminal[in]			data terminal object

 @return kernel id of terminal
 */
uint8_t ia_css_data_terminal_get_kernel_id(
	const ia_css_data_terminal_t			*dterminal);

/*! Get the connection type from the terminal object

 @param	terminal[in]			terminal object

 @return buffer type, limit value on error
 */
extern ia_css_connection_type_t ia_css_data_terminal_get_connection_type(
	const ia_css_data_terminal_t	*dterminal);

/*! Set the connection type of the terminal object

 @param	terminal[in]			terminal object
 @param	connection_type[in]		connection type

 @return < 0 on error
 */
extern int ia_css_data_terminal_set_connection_type(
	ia_css_data_terminal_t				*dterminal,
	const ia_css_connection_type_t			connection_type);


/*! Get the (pointer to) the process group parent of the terminal object

 @param	terminal[in]			terminal object

 @return the pointer to the parent, NULL on error
 */
extern ia_css_process_group_t *ia_css_terminal_get_parent(
	const ia_css_terminal_t					*terminal);

/*! Set the (pointer to) the process group parent of the terminal object

 @param	terminal[in]	terminal object
 @param	parent[in]	(pointer to the) process group parent object

 @return < 0 on error
 */
extern int ia_css_terminal_set_parent(
	ia_css_terminal_t	*terminal,
	ia_css_process_group_t	*parent);

/*! Boolean test if the terminal object type is valid

 @param	terminal[in]			process terminal object
 @param	terminal_manifest[in]		program terminal manifest

 @return true if the process terminal object is correct, false on error
 */
extern bool ia_css_is_terminal_valid(
	const ia_css_terminal_t		 *terminal,
	const ia_css_terminal_manifest_t *terminal_manifest);

#endif /* __IA_CSS_PSYS_TERMINAL_H_INCLUDED__ */
