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

#ifndef __IA_CSS_OUTPUT_BUFFER_CPU_H
#define __IA_CSS_OUTPUT_BUFFER_CPU_H

#include "vied/shared_memory_map.h"
#include "ia_css_output_buffer.h"

ia_css_output_buffer
ia_css_output_buffer_alloc(
	vied_subsystem_t sid,
	vied_memory_t mid,
	unsigned int size);

void
ia_css_output_buffer_free(
	vied_subsystem_t sid,
	vied_memory_t mid,
	ia_css_output_buffer b);

ia_css_output_buffer_css_address
ia_css_output_buffer_css_map(ia_css_output_buffer b);

ia_css_output_buffer_css_address
ia_css_output_buffer_css_unmap(ia_css_output_buffer b);

ia_css_output_buffer_cpu_address
ia_css_output_buffer_cpu_map(vied_memory_t mid, ia_css_output_buffer b);
ia_css_output_buffer_cpu_address
ia_css_output_buffer_cpu_map_no_invalidate(vied_memory_t mid, ia_css_output_buffer b);

ia_css_output_buffer_cpu_address
ia_css_output_buffer_cpu_unmap(ia_css_output_buffer b);


#endif /* __IA_CSS_OUTPUT_BUFFER_CPU_H */
