/**
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

#ifndef __IA_CSS_TERMINAL_IMPL_H
#define __IA_CSS_TERMINAL_IMPL_H

#include "ia_css_terminal.h"
#include "ia_css_terminal_types.h"
#include "error_support.h"
#include "assert_support.h"
#include "storage_class.h"

/* Param Terminal */
IA_CSS_PARAMETERS_STORAGE_CLASS_C
unsigned int ia_css_param_in_terminal_get_descriptor_size(
	const unsigned int nof_sections)
{
	return sizeof(ia_css_param_terminal_t) +
		nof_sections*sizeof(ia_css_param_section_desc_t);
}

IA_CSS_PARAMETERS_STORAGE_CLASS_C
ia_css_param_section_desc_t *ia_css_param_in_terminal_get_param_section_desc(
	const ia_css_param_terminal_t *param_terminal,
	const unsigned int section_index)
{
	ia_css_param_section_desc_t *param_section_base;
	ia_css_param_section_desc_t *param_section_desc = NULL;

	verifjmpexit(param_terminal != NULL);

	param_section_base =
		(ia_css_param_section_desc_t *)
		(((const char *)param_terminal) +
				param_terminal->param_section_desc_offset);
	param_section_desc = &(param_section_base[section_index]);

EXIT:
	return param_section_desc;
}

IA_CSS_PARAMETERS_STORAGE_CLASS_C
unsigned int ia_css_param_out_terminal_get_descriptor_size(
	const unsigned int nof_sections,
	const unsigned int nof_fragments)
{
	return sizeof(ia_css_param_terminal_t) +
		nof_fragments*nof_sections*sizeof(ia_css_param_section_desc_t);
}

IA_CSS_PARAMETERS_STORAGE_CLASS_C
ia_css_param_section_desc_t *ia_css_param_out_terminal_get_param_section_desc(
	const ia_css_param_terminal_t *param_terminal,
	const unsigned int section_index,
	const unsigned int nof_sections,
	const unsigned int fragment_index)
{
	ia_css_param_section_desc_t *param_section_base;
	ia_css_param_section_desc_t *param_section_desc = NULL;

	verifjmpexit(param_terminal != NULL);

	param_section_base =
		(ia_css_param_section_desc_t *)
			(((const char *)param_terminal) +
				param_terminal->param_section_desc_offset);
	param_section_desc =
		&(param_section_base[(nof_sections * fragment_index) +
				section_index]);

EXIT:
	return param_section_desc;
}

IA_CSS_PARAMETERS_STORAGE_CLASS_C
int ia_css_param_terminal_create(
	ia_css_param_terminal_t *param_terminal,
	const uint16_t terminal_offset,
	const uint16_t terminal_size,
	const uint16_t is_input_terminal)
{
	if (param_terminal == NULL) {
		return -EFAULT;
	}

	if (terminal_offset > (1<<15)) {
		return -EINVAL;
	}

	param_terminal->base.terminal_type =
		is_input_terminal ?
		IA_CSS_TERMINAL_TYPE_PARAM_CACHED_IN :
		IA_CSS_TERMINAL_TYPE_PARAM_CACHED_OUT;
	param_terminal->base.parent_offset =
		0 - ((int16_t)terminal_offset);
	param_terminal->base.size = terminal_size;
	param_terminal->param_section_desc_offset =
		sizeof(ia_css_param_terminal_t);

	return 0;
}

/* Spatial Param Terminal */
IA_CSS_PARAMETERS_STORAGE_CLASS_C
unsigned int ia_css_spatial_param_terminal_get_descriptor_size(
	const unsigned int nof_frame_param_sections,
	const unsigned int nof_fragments)
{
	return sizeof(ia_css_spatial_param_terminal_t) +
		nof_frame_param_sections * sizeof(
				ia_css_frame_grid_param_section_desc_t) +
		nof_fragments * sizeof(ia_css_fragment_grid_desc_t);
}

IA_CSS_PARAMETERS_STORAGE_CLASS_C
ia_css_fragment_grid_desc_t *
ia_css_spatial_param_terminal_get_fragment_grid_desc(
	const ia_css_spatial_param_terminal_t *spatial_param_terminal,
	const unsigned int fragment_index)
{
	ia_css_fragment_grid_desc_t *fragment_grid_desc_base;
	ia_css_fragment_grid_desc_t *fragment_grid_desc = NULL;

	verifjmpexit(spatial_param_terminal != NULL);

	fragment_grid_desc_base =
		(ia_css_fragment_grid_desc_t *)
			(((const char *)spatial_param_terminal) +
			spatial_param_terminal->fragment_grid_desc_offset);
	fragment_grid_desc = &(fragment_grid_desc_base[fragment_index]);

EXIT:
	return fragment_grid_desc;
}

IA_CSS_PARAMETERS_STORAGE_CLASS_C
ia_css_frame_grid_param_section_desc_t *
ia_css_spatial_param_terminal_get_frame_grid_param_section_desc(
	const ia_css_spatial_param_terminal_t *spatial_param_terminal,
	const unsigned int section_index)
{
	ia_css_frame_grid_param_section_desc_t *
		frame_grid_param_section_base;
	ia_css_frame_grid_param_section_desc_t *
		frame_grid_param_section_desc = NULL;

	verifjmpexit(spatial_param_terminal != NULL);

	frame_grid_param_section_base =
		(ia_css_frame_grid_param_section_desc_t *)
			(((const char *)spatial_param_terminal) +
		spatial_param_terminal->frame_grid_param_section_desc_offset);
	frame_grid_param_section_desc =
		&(frame_grid_param_section_base[section_index]);

EXIT:
	return frame_grid_param_section_desc;
}

IA_CSS_PARAMETERS_STORAGE_CLASS_C
int ia_css_spatial_param_terminal_create(
	ia_css_spatial_param_terminal_t *spatial_param_terminal,
	const uint16_t terminal_offset,
	const uint16_t terminal_size,
	const uint16_t is_input_terminal,
	const unsigned int nof_fragments,
	const uint32_t kernel_id)
{
	if (spatial_param_terminal == NULL) {
		return -EFAULT;
	}

	if (terminal_offset > (1<<15)) {
		return -EINVAL;
	}

	spatial_param_terminal->base.terminal_type =
		is_input_terminal ?
		IA_CSS_TERMINAL_TYPE_PARAM_SPATIAL_IN :
		IA_CSS_TERMINAL_TYPE_PARAM_SPATIAL_OUT;
	spatial_param_terminal->base.parent_offset =
		0 - ((int16_t)terminal_offset);
	spatial_param_terminal->base.size = terminal_size;
	spatial_param_terminal->kernel_id = kernel_id;
	spatial_param_terminal->fragment_grid_desc_offset =
		sizeof(ia_css_spatial_param_terminal_t);
	spatial_param_terminal->frame_grid_param_section_desc_offset =
		spatial_param_terminal->fragment_grid_desc_offset +
		(nof_fragments * sizeof(ia_css_fragment_grid_desc_t));

	return 0;
}

/* Sliced terminal */
IA_CSS_PARAMETERS_STORAGE_CLASS_C
unsigned int ia_css_sliced_param_terminal_get_descriptor_size(
	const unsigned int nof_slice_param_sections,
	const unsigned int nof_slices[],
	const unsigned int nof_fragments)
{
	unsigned int descriptor_size = 0;
	unsigned int fragment_index;
	unsigned int nof_slices_total = 0;

	verifjmpexit(nof_slices != NULL);

	for (fragment_index = 0;
			fragment_index < nof_fragments; fragment_index++) {
		nof_slices_total += nof_slices[fragment_index];
	}

	descriptor_size =
		sizeof(ia_css_sliced_param_terminal_t) +
		nof_fragments*sizeof(ia_css_fragment_slice_desc_t) +
		nof_slices_total*nof_slice_param_sections*sizeof(
			ia_css_fragment_param_section_desc_t);

EXIT:
	return descriptor_size;
}

IA_CSS_PARAMETERS_STORAGE_CLASS_C
ia_css_fragment_slice_desc_t *
ia_css_sliced_param_terminal_get_fragment_slice_desc(
	const ia_css_sliced_param_terminal_t *sliced_param_terminal,
	const unsigned int fragment_index
)
{
	ia_css_fragment_slice_desc_t *fragment_slice_desc_base;
	ia_css_fragment_slice_desc_t *fragment_slice_desc = NULL;

	verifjmpexit(sliced_param_terminal != NULL);

	fragment_slice_desc_base =
		(ia_css_fragment_slice_desc_t *)
			(((const char *)sliced_param_terminal) +
			sliced_param_terminal->fragment_slice_desc_offset);
	fragment_slice_desc = &(fragment_slice_desc_base[fragment_index]);

EXIT:
	return fragment_slice_desc;
}

IA_CSS_PARAMETERS_STORAGE_CLASS_C
ia_css_slice_param_section_desc_t *
ia_css_sliced_param_terminal_get_slice_param_section_desc(
	const ia_css_sliced_param_terminal_t *sliced_param_terminal,
	const unsigned int fragment_index,
	const unsigned int slice_index,
	const unsigned int section_index,
	const unsigned int nof_slice_param_sections)
{
	ia_css_fragment_slice_desc_t *fragment_slice_desc;
	ia_css_slice_param_section_desc_t *slice_param_section_desc_base;
	ia_css_slice_param_section_desc_t *slice_param_section_desc = NULL;

	fragment_slice_desc =
		ia_css_sliced_param_terminal_get_fragment_slice_desc(
			sliced_param_terminal,
			fragment_index
			);
	verifjmpexit(fragment_slice_desc != NULL);

	slice_param_section_desc_base =
		(ia_css_slice_param_section_desc_t *)
		(((const char *)sliced_param_terminal) +
		fragment_slice_desc->slice_section_desc_offset);
	slice_param_section_desc =
		&(slice_param_section_desc_base[(
			slice_index * nof_slice_param_sections) +
				section_index]);

EXIT:
	return slice_param_section_desc;
}

IA_CSS_PARAMETERS_STORAGE_CLASS_C
int ia_css_sliced_param_terminal_create(
	ia_css_sliced_param_terminal_t *sliced_param_terminal,
	const uint16_t terminal_offset,
	const uint16_t terminal_size,
	const uint16_t is_input_terminal,
	const unsigned int nof_slice_param_sections,
	const unsigned int nof_slices[],
	const unsigned int nof_fragments,
	const uint32_t kernel_id)
{
	unsigned int fragment_index;
	unsigned int nof_slices_total = 0;

	if (sliced_param_terminal == NULL) {
		return -EFAULT;
	}

	if (terminal_offset > (1<<15)) {
		return -EINVAL;
	}

	sliced_param_terminal->base.terminal_type =
		is_input_terminal ?
		IA_CSS_TERMINAL_TYPE_PARAM_SLICED_IN :
		IA_CSS_TERMINAL_TYPE_PARAM_SLICED_OUT;
	sliced_param_terminal->base.parent_offset =
		0 - ((int16_t)terminal_offset);
	sliced_param_terminal->base.size = terminal_size;
	sliced_param_terminal->kernel_id = kernel_id;
	/* set here to use below to find the pointer */
	sliced_param_terminal->fragment_slice_desc_offset =
		sizeof(ia_css_sliced_param_terminal_t);
	for (fragment_index = 0;
			fragment_index < nof_fragments; fragment_index++) {
		ia_css_fragment_slice_desc_t *fragment_slice_desc =
			ia_css_sliced_param_terminal_get_fragment_slice_desc(
				sliced_param_terminal,
				fragment_index);
		/*
		 * Error handling not required at this point
		 * since everything has been constructed/validated just above
		 */
		fragment_slice_desc->slice_count = nof_slices[fragment_index];
		fragment_slice_desc->slice_section_desc_offset =
			sliced_param_terminal->fragment_slice_desc_offset +
			(nof_fragments * sizeof(
					ia_css_fragment_slice_desc_t)) +
			(nof_slices_total * nof_slice_param_sections * sizeof(
					ia_css_slice_param_section_desc_t));
		nof_slices_total += nof_slices[fragment_index];
	}

	return 0;
}

/* Program terminal */
IA_CSS_PARAMETERS_STORAGE_CLASS_C
unsigned int ia_css_program_terminal_get_descriptor_size(
	const unsigned int nof_fragments,
	const unsigned int nof_fragment_param_sections,
	const unsigned int nof_kernel_fragment_sequencer_infos,
	const unsigned int nof_command_objs)
{
	return sizeof(ia_css_program_terminal_t) +
		nof_fragments * nof_fragment_param_sections *
		sizeof(ia_css_fragment_param_section_desc_t) +
		nof_fragments * nof_kernel_fragment_sequencer_infos *
		sizeof(ia_css_kernel_fragment_sequencer_info_desc_t) +
		nof_command_objs * sizeof(
			ia_css_kernel_fragment_sequencer_command_desc_t);
}

IA_CSS_PARAMETERS_STORAGE_CLASS_C
ia_css_fragment_param_section_desc_t *
ia_css_program_terminal_get_frgmnt_prm_sct_desc(
	const ia_css_program_terminal_t *program_terminal,
	const unsigned int fragment_index,
	const unsigned int section_index,
	const unsigned int nof_fragment_param_sections)
{
	ia_css_fragment_param_section_desc_t *
		fragment_param_section_desc_base;
	ia_css_fragment_param_section_desc_t *
		fragment_param_section_desc = NULL;

	verifjmpexit(program_terminal != NULL);
	verifjmpexit(section_index < nof_fragment_param_sections);

	fragment_param_section_desc_base =
		(ia_css_fragment_param_section_desc_t *)
			(((const char *)program_terminal) +
			program_terminal->fragment_param_section_desc_offset);
	fragment_param_section_desc =
		&(fragment_param_section_desc_base[(fragment_index *
			nof_fragment_param_sections) + section_index]);

EXIT:
	return fragment_param_section_desc;
}

IA_CSS_PARAMETERS_STORAGE_CLASS_C
ia_css_kernel_fragment_sequencer_info_desc_t *
ia_css_program_terminal_get_kernel_frgmnt_seq_info_desc(
	const ia_css_program_terminal_t *program_terminal,
	const unsigned int fragment_index,
	const unsigned int info_index,
	const unsigned int nof_kernel_fragment_sequencer_infos)
{
	ia_css_kernel_fragment_sequencer_info_desc_t *
		kernel_fragment_sequencer_info_desc_base;
	ia_css_kernel_fragment_sequencer_info_desc_t *
		kernel_fragment_sequencer_info_desc = NULL;

	verifjmpexit(program_terminal != NULL);
	if (nof_kernel_fragment_sequencer_infos > 0) {
		verifjmpexit(info_index < nof_kernel_fragment_sequencer_infos);
	}

	kernel_fragment_sequencer_info_desc_base =
		(ia_css_kernel_fragment_sequencer_info_desc_t *)
		(((const char *)program_terminal) +
		program_terminal->kernel_fragment_sequencer_info_desc_offset);
	kernel_fragment_sequencer_info_desc =
		&(kernel_fragment_sequencer_info_desc_base[(fragment_index *
			nof_kernel_fragment_sequencer_infos) + info_index]);

EXIT:
	return kernel_fragment_sequencer_info_desc;
}

IA_CSS_PARAMETERS_STORAGE_CLASS_C
int ia_css_program_terminal_create(
	ia_css_program_terminal_t *program_terminal,
	const uint16_t terminal_offset,
	const uint16_t terminal_size,
	const unsigned int nof_fragments,
	const unsigned int nof_kernel_fragment_sequencer_infos,
	const unsigned int nof_command_objs)
{
	if (program_terminal == NULL) {
		return -EFAULT;
	}

	if (terminal_offset > (1<<15)) {
		return -EINVAL;
	}

	program_terminal->base.terminal_type = IA_CSS_TERMINAL_TYPE_PROGRAM;
	program_terminal->base.parent_offset = 0-((int16_t)terminal_offset);
	program_terminal->base.size = terminal_size;
	program_terminal->kernel_fragment_sequencer_info_desc_offset =
		sizeof(ia_css_program_terminal_t);
	program_terminal->fragment_param_section_desc_offset =
		program_terminal->kernel_fragment_sequencer_info_desc_offset +
		(nof_fragments * nof_kernel_fragment_sequencer_infos *
		sizeof(ia_css_kernel_fragment_sequencer_info_desc_t)) +
		(nof_command_objs * sizeof(
			ia_css_kernel_fragment_sequencer_command_desc_t));

	return 0;
}

IA_CSS_PARAMETERS_STORAGE_CLASS_C
int ia_css_program_terminal_get_command_base_offset(
	const ia_css_program_terminal_t *program_terminal,
	const unsigned int nof_fragments,
	const unsigned int nof_kernel_fragment_sequencer_infos,
	const unsigned int commands_slots_used,
	uint16_t *command_desc_offset)
{
	if (command_desc_offset == NULL) {
		return -EFAULT;
	}

	*command_desc_offset = 0;

	if (program_terminal == NULL) {
		return -EFAULT;
	}

	*command_desc_offset =
		program_terminal->kernel_fragment_sequencer_info_desc_offset +
		(nof_fragments * nof_kernel_fragment_sequencer_infos *
		sizeof(ia_css_kernel_fragment_sequencer_info_desc_t)) +
		(commands_slots_used * sizeof(
			ia_css_kernel_fragment_sequencer_command_desc_t));

	return 0;
}

IA_CSS_PARAMETERS_STORAGE_CLASS_C
uint16_t *ia_css_program_terminal_get_line_count(
	const ia_css_kernel_fragment_sequencer_command_desc_t
	*kernel_fragment_sequencer_command_desc_base,
	const unsigned int set_count)
{
	uint16_t *line_count = NULL;

	verifjmpexit(kernel_fragment_sequencer_command_desc_base != NULL);
	line_count =
		(uint16_t *)&(kernel_fragment_sequencer_command_desc_base[
			set_count >> 2].line_count[set_count & 0x00000003]);
EXIT:
	return line_count;
}

#endif /* __IA_CSS_TERMINAL_IMPL_H */
