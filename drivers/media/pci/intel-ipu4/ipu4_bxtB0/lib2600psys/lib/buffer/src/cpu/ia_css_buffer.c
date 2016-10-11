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

/* provided interface */
#include "ia_css_buffer.h"

/* used interfaces */
#include "vied/shared_memory_access.h"
#include "vied/shared_memory_map.h"
#include "cpu_mem_support.h"

ia_css_buffer_t
ia_css_buffer_alloc(vied_subsystem_t sid, vied_memory_t mid, unsigned int size)
{
	ia_css_buffer_t b;

	b = ia_css_cpu_mem_alloc(sizeof(*b));
	if (b == NULL)
		return NULL;

	b->mem = shared_memory_alloc(mid, size);

	if (b->mem == 0) {
		ia_css_cpu_mem_free(b);
		return NULL;
	}

	b->css_address = shared_memory_map(sid, mid, b->mem);
	b->size	= size;
	return b;
}


void
ia_css_buffer_free(vied_subsystem_t sid, vied_memory_t mid, ia_css_buffer_t b)
{
	shared_memory_unmap(sid, mid, b->css_address);
	shared_memory_free(mid, b->mem);
	ia_css_cpu_mem_free(b);
}

