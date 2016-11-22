/**
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

#ifndef __IA_CSS_TERMINAL_MANIFEST_IMPL_H
#define __IA_CSS_TERMINAL_MANIFEST_IMPL_H

#include "ia_css_terminal_manifest.h"
#include "error_support.h"
#include "assert_support.h"
#include "storage_class.h"

STORAGE_CLASS_INLINE void __terminal_manifest_dummy_check_alignment(void)
{
	COMPILATION_ERROR_IF(
		SIZE_OF_PARAM_TERMINAL_MANIFEST_STRUCT_IN_BITS !=
			(CHAR_BIT * sizeof(ia_css_param_terminal_manifest_t)));

	COMPILATION_ERROR_IF(0 !=
		sizeof(ia_css_param_terminal_manifest_t) % sizeof(uint64_t));

	COMPILATION_ERROR_IF(
		SIZE_OF_PARAM_TERMINAL_MANIFEST_SEC_STRUCT_IN_BITS !=
		(CHAR_BIT * sizeof(ia_css_param_manifest_section_desc_t)));

	COMPILATION_ERROR_IF(0 !=
		sizeof(ia_css_param_manifest_section_desc_t) %
			sizeof(uint64_t));

	COMPILATION_ERROR_IF(
		SIZE_OF_SPATIAL_PARAM_TERM_MAN_STRUCT_IN_BITS !=
		(CHAR_BIT * sizeof(ia_css_spatial_param_terminal_manifest_t)));

	COMPILATION_ERROR_IF(0 !=
		sizeof(ia_css_spatial_param_terminal_manifest_t) %
			sizeof(uint64_t));

	COMPILATION_ERROR_IF(
		SIZE_OF_FRAME_GRID_PARAM_MAN_SEC_STRUCT_IN_BITS !=
		(CHAR_BIT * sizeof(
			ia_css_frame_grid_param_manifest_section_desc_t)));

	COMPILATION_ERROR_IF(0 !=
		sizeof(ia_css_frame_grid_param_manifest_section_desc_t) %
			sizeof(uint64_t));

	COMPILATION_ERROR_IF(
		SIZE_OF_PROG_TERM_MAN_STRUCT_IN_BITS !=
		(CHAR_BIT * sizeof(ia_css_program_terminal_manifest_t)));

	COMPILATION_ERROR_IF(0 !=
		sizeof(ia_css_program_terminal_manifest_t)%sizeof(uint64_t));

	COMPILATION_ERROR_IF(
		SIZE_OF_FRAG_PARAM_MAN_SEC_STRUCT_IN_BITS !=
		(CHAR_BIT * sizeof(
			ia_css_fragment_param_manifest_section_desc_t)));

	COMPILATION_ERROR_IF(0 !=
		sizeof(ia_css_fragment_param_manifest_section_desc_t) %
			sizeof(uint64_t));

	COMPILATION_ERROR_IF(
		SIZE_OF_KERNEL_FRAG_SEQ_INFO_MAN_STRUCT_IN_BITS !=
		(CHAR_BIT * sizeof(
			ia_css_kernel_fragment_sequencer_info_manifest_desc_t))
		);

	COMPILATION_ERROR_IF(0 != sizeof(
		ia_css_kernel_fragment_sequencer_info_manifest_desc_t) %
			sizeof(uint64_t));

	COMPILATION_ERROR_IF(
		SIZE_OF_PARAM_TERMINAL_MANIFEST_STRUCT_IN_BITS !=
		(CHAR_BIT * sizeof(ia_css_sliced_param_terminal_manifest_t)));

	COMPILATION_ERROR_IF(0 !=
		sizeof(ia_css_sliced_param_terminal_manifest_t) %
			sizeof(uint64_t));

	COMPILATION_ERROR_IF(
		SIZE_OF_PARAM_TERMINAL_MANIFEST_SEC_STRUCT_IN_BITS !=
		(CHAR_BIT * sizeof
			(ia_css_sliced_param_manifest_section_desc_t)));

	COMPILATION_ERROR_IF(0 !=
		sizeof(ia_css_sliced_param_manifest_section_desc_t) %
			sizeof(uint64_t));
}

/* Parameter Terminal */
IA_CSS_PARAMETERS_STORAGE_CLASS_C
unsigned int ia_css_param_terminal_manifest_get_size(
	const unsigned int nof_sections)
{

	return sizeof(ia_css_param_terminal_manifest_t) +
		nof_sections*sizeof(ia_css_param_manifest_section_desc_t);
}

IA_CSS_PARAMETERS_STORAGE_CLASS_C
int ia_css_param_terminal_manifest_init(
	ia_css_param_terminal_manifest_t *param_terminal,
	const uint16_t section_count)
{
	if (param_terminal == NULL) {
		return -EFAULT;
	}

	param_terminal->param_manifest_section_desc_count = section_count;
	param_terminal->param_manifest_section_desc_offset = sizeof(
				ia_css_param_terminal_manifest_t);

	return 0;
}

IA_CSS_PARAMETERS_STORAGE_CLASS_C
ia_css_param_manifest_section_desc_t *
ia_css_param_terminal_manifest_get_prm_sct_desc(
	const ia_css_param_terminal_manifest_t *param_terminal_manifest,
	const unsigned int section_index)
{
	ia_css_param_manifest_section_desc_t *param_manifest_section_base;
	ia_css_param_manifest_section_desc_t *
		param_manifest_section_desc = NULL;

	verifjmpexit(param_terminal_manifest != NULL);

	param_manifest_section_base =
		(ia_css_param_manifest_section_desc_t *)
		(((const char *)param_terminal_manifest)
		+ param_terminal_manifest->param_manifest_section_desc_offset);

	param_manifest_section_desc =
		&(param_manifest_section_base[section_index]);

EXIT:
	return param_manifest_section_desc;
}

/* Keep old function name before Windows/Android change name */
IA_CSS_PARAMETERS_STORAGE_CLASS_C
ia_css_param_manifest_section_desc_t *
ia_css_param_terminal_manifest_get_param_manifest_section_desc(
	const ia_css_param_terminal_manifest_t *param_terminal_manifest,
	const unsigned int section_index)
{
	ia_css_param_manifest_section_desc_t *param_manifest_section_base;
	ia_css_param_manifest_section_desc_t *
		param_manifest_section_desc = NULL;

	verifjmpexit(param_terminal_manifest != NULL);

	param_manifest_section_base =
		(ia_css_param_manifest_section_desc_t *)
		(((const char *)param_terminal_manifest)
		+ param_terminal_manifest->param_manifest_section_desc_offset);

	param_manifest_section_desc =
		&(param_manifest_section_base[section_index]);

EXIT:
	return param_manifest_section_desc;
}

/* Spatial Parameter Terminal */
IA_CSS_PARAMETERS_STORAGE_CLASS_C
unsigned int ia_css_spatial_param_terminal_manifest_get_size(
	const unsigned int nof_frame_param_sections)
{
	return sizeof(ia_css_spatial_param_terminal_manifest_t) +
		nof_frame_param_sections * sizeof(
			ia_css_frame_grid_param_manifest_section_desc_t);
}

IA_CSS_PARAMETERS_STORAGE_CLASS_C
int ia_css_spatial_param_terminal_manifest_init(
	ia_css_spatial_param_terminal_manifest_t *spatial_param_terminal,
	const uint16_t section_count)
{
	if (spatial_param_terminal == NULL) {
		return -EFAULT;
	}

	spatial_param_terminal->
		frame_grid_param_manifest_section_desc_count = section_count;
	spatial_param_terminal->
		frame_grid_param_manifest_section_desc_offset =
		sizeof(ia_css_spatial_param_terminal_manifest_t);

	return 0;
}

IA_CSS_PARAMETERS_STORAGE_CLASS_C
ia_css_frame_grid_param_manifest_section_desc_t *
ia_css_spatial_param_terminal_manifest_get_frm_grid_prm_sct_desc(
	const ia_css_spatial_param_terminal_manifest_t *
		spatial_param_terminal_manifest,
	const unsigned int section_index)
{
	ia_css_frame_grid_param_manifest_section_desc_t *
		frame_param_manifest_section_base;
	ia_css_frame_grid_param_manifest_section_desc_t *
		frame_param_manifest_section_desc = NULL;

	verifjmpexit(spatial_param_terminal_manifest != NULL);

	frame_param_manifest_section_base =
		(ia_css_frame_grid_param_manifest_section_desc_t *)
		(((const char *)spatial_param_terminal_manifest) +
			spatial_param_terminal_manifest->
			frame_grid_param_manifest_section_desc_offset);
	frame_param_manifest_section_desc =
		&(frame_param_manifest_section_base[section_index]);

EXIT:
	return frame_param_manifest_section_desc;
}

/* Keep old function name before Windows/Android change name */
IA_CSS_PARAMETERS_STORAGE_CLASS_C
ia_css_frame_grid_param_manifest_section_desc_t *
ia_css_spatial_param_terminal_manifest_get_frame_grid_param_manifest_section_desc(
	const ia_css_spatial_param_terminal_manifest_t *
		spatial_param_terminal_manifest,
	const unsigned int section_index)
{
	ia_css_frame_grid_param_manifest_section_desc_t *
		frame_param_manifest_section_base;
	ia_css_frame_grid_param_manifest_section_desc_t *
		frame_param_manifest_section_desc = NULL;

	verifjmpexit(spatial_param_terminal_manifest != NULL);

	frame_param_manifest_section_base =
		(ia_css_frame_grid_param_manifest_section_desc_t *)
		(((const char *)spatial_param_terminal_manifest) +
			spatial_param_terminal_manifest->
			frame_grid_param_manifest_section_desc_offset);
	frame_param_manifest_section_desc =
		&(frame_param_manifest_section_base[section_index]);

EXIT:
	return frame_param_manifest_section_desc;
}

/* Sliced Terminal */
IA_CSS_PARAMETERS_STORAGE_CLASS_C
unsigned int ia_css_sliced_param_terminal_manifest_get_size(
	const unsigned int nof_slice_param_sections)
{
	return sizeof(ia_css_spatial_param_terminal_manifest_t) +
		nof_slice_param_sections *
		sizeof(ia_css_sliced_param_manifest_section_desc_t);
}

IA_CSS_PARAMETERS_STORAGE_CLASS_C
int ia_css_sliced_param_terminal_manifest_init(
	ia_css_sliced_param_terminal_manifest_t *sliced_param_terminal,
	const uint16_t section_count)
{
	if (sliced_param_terminal == NULL) {
		return -EFAULT;
	}

	sliced_param_terminal->sliced_param_section_count = section_count;
	sliced_param_terminal->sliced_param_section_offset =
		sizeof(ia_css_sliced_param_terminal_manifest_t);

	return 0;
}

IA_CSS_PARAMETERS_STORAGE_CLASS_C
ia_css_sliced_param_manifest_section_desc_t *
ia_css_sliced_param_terminal_manifest_get_sliced_prm_sct_desc(
	const ia_css_sliced_param_terminal_manifest_t *
		sliced_param_terminal_manifest,
	const unsigned int section_index)
{
	ia_css_sliced_param_manifest_section_desc_t *
		sliced_param_manifest_section_base;
	ia_css_sliced_param_manifest_section_desc_t *
		sliced_param_manifest_section_desc = NULL;

	verifjmpexit(sliced_param_terminal_manifest != NULL);

	sliced_param_manifest_section_base =
		(ia_css_sliced_param_manifest_section_desc_t *)
		(((const char *)sliced_param_terminal_manifest) +
			sliced_param_terminal_manifest->
			sliced_param_section_offset);
	sliced_param_manifest_section_desc =
		&(sliced_param_manifest_section_base[section_index]);

EXIT:
	return sliced_param_manifest_section_desc;
}

/* Program Terminal */
IA_CSS_PARAMETERS_STORAGE_CLASS_C
unsigned int ia_css_program_terminal_manifest_get_size(
	const unsigned int nof_fragment_param_sections,
	const unsigned int nof_kernel_fragment_sequencer_infos)
{
	return sizeof(ia_css_program_terminal_manifest_t) +
		nof_fragment_param_sections *
		sizeof(ia_css_fragment_param_manifest_section_desc_t) +
		nof_kernel_fragment_sequencer_infos *
		sizeof(ia_css_kernel_fragment_sequencer_info_manifest_desc_t);
}

IA_CSS_PARAMETERS_STORAGE_CLASS_C
int ia_css_program_terminal_manifest_init(
	ia_css_program_terminal_manifest_t *program_terminal,
	const uint16_t fragment_param_section_count,
	const uint16_t kernel_fragment_seq_info_section_count)
{
	if (program_terminal == NULL) {
		return -EFAULT;
	}

	program_terminal->fragment_param_manifest_section_desc_count =
		fragment_param_section_count;
	program_terminal->fragment_param_manifest_section_desc_offset =
		sizeof(ia_css_program_terminal_manifest_t);

	program_terminal->kernel_fragment_sequencer_info_manifest_info_count =
		kernel_fragment_seq_info_section_count;
	program_terminal->kernel_fragment_sequencer_info_manifest_info_offset =
		sizeof(ia_css_program_terminal_manifest_t) +
		fragment_param_section_count*sizeof(
			ia_css_fragment_param_manifest_section_desc_t);

	return 0;
}

IA_CSS_PARAMETERS_STORAGE_CLASS_C
ia_css_fragment_param_manifest_section_desc_t *
ia_css_program_terminal_manifest_get_frgmnt_prm_sct_desc(
	const ia_css_program_terminal_manifest_t *program_terminal_manifest,
	const unsigned int section_index)
{
	ia_css_fragment_param_manifest_section_desc_t *
		fragment_param_manifest_section_base;
	ia_css_fragment_param_manifest_section_desc_t *
		fragment_param_manifest_section = NULL;

	verifjmpexit(program_terminal_manifest != NULL);

	fragment_param_manifest_section_base =
		(ia_css_fragment_param_manifest_section_desc_t *)
		(((const char *)program_terminal_manifest) +
		program_terminal_manifest->
		fragment_param_manifest_section_desc_offset);
	fragment_param_manifest_section =
		&(fragment_param_manifest_section_base[section_index]);

EXIT:
	return fragment_param_manifest_section;
}

/* Keep old function name before Windows/Android change name */
IA_CSS_PARAMETERS_STORAGE_CLASS_C
ia_css_fragment_param_manifest_section_desc_t *
ia_css_program_terminal_manifest_get_fragment_param_manifest_section_desc(
	const ia_css_program_terminal_manifest_t *program_terminal_manifest,
	const unsigned int section_index)
{
	ia_css_fragment_param_manifest_section_desc_t *
		fragment_param_manifest_section_base;
	ia_css_fragment_param_manifest_section_desc_t *
		fragment_param_manifest_section = NULL;

	verifjmpexit(program_terminal_manifest != NULL);

	fragment_param_manifest_section_base =
		(ia_css_fragment_param_manifest_section_desc_t *)
		(((const char *)program_terminal_manifest) +
		program_terminal_manifest->
		fragment_param_manifest_section_desc_offset);
	fragment_param_manifest_section =
		&(fragment_param_manifest_section_base[section_index]);

EXIT:
	return fragment_param_manifest_section;
}

IA_CSS_PARAMETERS_STORAGE_CLASS_C
ia_css_kernel_fragment_sequencer_info_manifest_desc_t *
ia_css_program_terminal_manifest_get_kernel_frgmnt_seq_info_desc(
	const ia_css_program_terminal_manifest_t *program_terminal_manifest,
	const unsigned int info_index)
{
	ia_css_kernel_fragment_sequencer_info_manifest_desc_t *
		kernel_manifest_fragment_sequencer_info_manifest_desc_base;
	ia_css_kernel_fragment_sequencer_info_manifest_desc_t *
		kernel_manifest_fragment_sequencer_info_manifest_desc = NULL;

	verifjmpexit(program_terminal_manifest != NULL);

	kernel_manifest_fragment_sequencer_info_manifest_desc_base =
		(ia_css_kernel_fragment_sequencer_info_manifest_desc_t *)
		(((const char *)program_terminal_manifest) +
		program_terminal_manifest->
		kernel_fragment_sequencer_info_manifest_info_offset);

	kernel_manifest_fragment_sequencer_info_manifest_desc =
		&(kernel_manifest_fragment_sequencer_info_manifest_desc_base[
				info_index]);

EXIT:
	return kernel_manifest_fragment_sequencer_info_manifest_desc;
}

#endif /* __IA_CSS_TERMINAL_MANIFEST_IMPL_H */
