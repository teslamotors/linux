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

#ifndef __IA_CSS_PROGRAM_GROUP_PARAM_H_INCLUDED__
#define __IA_CSS_PROGRAM_GROUP_PARAM_H_INCLUDED__

/*! \file */

/** @file ia_css_program_group_param.h
 *
 * Define the methods on the program group parameter object that are not part
 * of a single interface
 */
#include <ia_css_program_group_param_types.h>

#include <ia_css_program_group_param.sim.h>

#include <ia_css_kernel_bitmap.h>	/* ia_css_kernel_bitmap_t */

#include <type_support.h>

/*! Get the stored size of the program group parameter object

 @param	param[in]			program group parameter object

 @return size, 0 on error
 */
extern size_t ia_css_program_group_param_get_size(
	const ia_css_program_group_param_t		*param);

/*! initialize program_group_param

 @param	blob[in]	program group parameter object
 @param	program_count[in]		number of  terminals.
 @param	terminal_count[in]		number of  terminals.
 @param	fragment_count[in]		number of  terminals.

 @return 0 if success, else failure.
 */
extern int ia_css_program_group_param_init(
	ia_css_program_group_param_t *blob,
	const uint8_t	program_count,
	const uint8_t	terminal_count,
	const uint16_t	fragment_count,
	const enum ia_css_frame_format_type *frame_format_types);
/*! Get the program parameter object from a program group parameter object

 @param	program_group_param[in]		program group parameter object
 @param	i[in]				program parameter index

 @return program parameter pointer, NULL on error
 */
extern ia_css_program_param_t *ia_css_program_group_param_get_program_param(
	const ia_css_program_group_param_t *param,
	const int i);

/*! Get the terminal parameter object from a program group parameter object

 @param	program_group_param[in]		program group parameter object
 @param	i[in]				terminal parameter index

 @return terminal parameter pointer, NULL on error
 */
extern ia_css_terminal_param_t *ia_css_program_group_param_get_terminal_param(
	const ia_css_program_group_param_t *param,
	const int i);

/*! Get the fragment count from a program group parameter object

 @param	program_group_param[in]		program group parameter object

 @return fragment count, 0 on error
 */
extern uint16_t ia_css_program_group_param_get_fragment_count(
	const ia_css_program_group_param_t		*param);

/*! Get the program count from a program group parameter object

 @param	program_group_param[in]		program group parameter object

 @return program count, 0 on error
 */
extern uint8_t ia_css_program_group_param_get_program_count(
	const ia_css_program_group_param_t		*param);

/*! Get the terminal count from a program group parameter object

 @param	program_group_param[in]		program group parameter object

 @return terminal count, 0 on error
 */
extern uint8_t ia_css_program_group_param_get_terminal_count(
	const ia_css_program_group_param_t		*param);


/*! Set the kernel enable bitmap from a program group parameter object

 @param	param[in]			program group parameter object
 @param	bitmap[in]			kernel enable bitmap

 @return non-zero on error
 */
extern int ia_css_program_group_param_set_kernel_enable_bitmap(
	ia_css_program_group_param_t	*param,
	const ia_css_kernel_bitmap_t	bitmap);

/*! Get the kernel enable bitmap from a program group parameter object

 @param	program_group_param[in]		program group parameter object

 @return kernel enable bitmap, 0 on error
*/
extern ia_css_kernel_bitmap_t
ia_css_program_group_param_get_kernel_enable_bitmap(
	const ia_css_program_group_param_t *param);


/*! Get the stored size of the program parameter object

 @param	param[in]			program parameter object

 @return size, 0 on error
 */
extern size_t ia_css_program_param_get_size(
	const ia_css_program_param_t			*param);

/*! Set the kernel enable bitmap from a program parameter object

 @param	program_param[in]		program parameter object
 @param	bitmap[in]			kernel enable bitmap

 @return non-zero on error
 */
extern int ia_css_program_param_set_kernel_enable_bitmap(
	ia_css_program_param_t	*program_param,
	const ia_css_kernel_bitmap_t	bitmap);

/*! Get the kernel enable bitmap from a program parameter object

 @param	program_param[in]		program parameter object

 Note: This function returns in fact the kernel enable of the program group
      parameters

 @return kernel enable bitmap, 0 on error
 */
extern ia_css_kernel_bitmap_t ia_css_program_param_get_kernel_enable_bitmap(
	const ia_css_program_param_t			*param);

/*! Get the stored size of the terminal parameter object

 @param	param[in]			terminal parameter object

 @return size, 0 on error
 */
extern size_t ia_css_terminal_param_get_size(
	const ia_css_terminal_param_t			*param);

/*! Get the kernel enable bitmap from a terminal parameter object

 @param	terminal_param[in]		terminal parameter object

 Note: This function returns in fact the kernel enable of the program group
       parameters

 @return kernel enable bitmap, 0 on error
 */
extern ia_css_kernel_bitmap_t ia_css_terminal_param_get_kernel_enable_bitmap(
	const ia_css_terminal_param_t			*param);

/*! Get the parent object for this terminal param.

 @param	terminal_param[in]		terminal parameter object

 @return parent program group param object
 */
extern ia_css_program_group_param_t *ia_css_terminal_param_get_parent(
	const ia_css_terminal_param_t			*param);

/*! Get the data format type associated with the terminal.

 @param	terminal_param[in]		terminal parameter object

 @return data format type (ia_css_data_format_type_t)
 */
extern ia_css_frame_format_type_t ia_css_terminal_param_get_frame_format_type(
	const ia_css_terminal_param_t	*terminal_param);

/*! Set the data format type associated with the terminal.

 @param	terminal_param[in]		terminal parameter object
 @param data_format_type[in]		data format type

 @return non-zero on error.
 */
extern int ia_css_terminal_param_set_frame_format_type(
	ia_css_terminal_param_t	*terminal_param,
	const ia_css_frame_format_type_t data_format_type);

/*! Get bits per pixel on the frame associated with the terminal.

 @param	terminal_param[in]		terminal parameter object

 @return bits per pixel
 */
extern uint8_t ia_css_terminal_param_get_bpp(
	const ia_css_terminal_param_t	*terminal_param);

/*! Set bits per pixel on the frame associated with the terminal.

 @param	terminal_param[in]		terminal parameter object
 @param bpp[in]				bits per pixel

 @return non-zero on error.
 */
extern int ia_css_terminal_param_set_bpp(
	ia_css_terminal_param_t	*terminal_param,
	const uint8_t bpp);

/*! Get dimensions on the frame associated with the terminal.

 @param	terminal_param[in]		terminal parameter object
 @param	dimensions[out]			dimension array

 @return non-zero on error.
 */
extern int ia_css_terminal_param_get_dimensions(
	const ia_css_terminal_param_t	*terminal_param,
	uint16_t dimensions[IA_CSS_N_DATA_DIMENSION]);

/*! Set dimensions on the frame associated with the terminal.

 @param	terminal_param[in]		terminal parameter object
 @param	dimensions[in]			dimension array

 @return non-zero on error.
 */
extern int ia_css_terminal_param_set_dimensions(
	ia_css_terminal_param_t	*terminal_param,
	const uint16_t dimensions[IA_CSS_N_DATA_DIMENSION]);

/*! Get stride on the frame associated with the terminal.

 @param	terminal_param[in]		terminal parameter object

 @return stride of the frame to be attached.
 */
extern uint32_t ia_css_terminal_param_get_stride(
	const ia_css_terminal_param_t	*terminal_param);

/*! Set stride on the frame associated with the terminal.

 @param	terminal_param[in]		terminal parameter object
 @param	stride[in]				stride

 @return non-zero on error.
 */
extern int ia_css_terminal_param_set_stride(
	ia_css_terminal_param_t	*terminal_param,
	const uint32_t stride);

#endif /* __IA_CSS_PROGRAM_GROUP_PARAM_H_INCLUDED__  */



