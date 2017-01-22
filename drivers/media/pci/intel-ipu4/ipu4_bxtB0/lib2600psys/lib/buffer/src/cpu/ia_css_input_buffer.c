/**
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


#include "ia_css_input_buffer_cpu.h"
#include "ia_css_buffer.h"
#include "vied/shared_memory_access.h"
#include "vied/shared_memory_map.h"
#include "cpu_mem_support.h"


ia_css_input_buffer
ia_css_input_buffer_alloc(
	vied_subsystem_t sid,
	vied_memory_t mid,
	unsigned int size)
{
	ia_css_input_buffer b;

	/* allocate buffer container */
	b = ia_css_cpu_mem_alloc(sizeof(*b));
	if (b == NULL)
		return NULL;

	b->mem = shared_memory_alloc(mid, size);
	if (b->mem == 0) {
		ia_css_cpu_mem_free(b);
		return NULL;
	}

#ifndef HRT_HW
	/* initialize the buffer to avoid warnings when copying */
	shared_memory_zero(mid, b->mem, size);

	/* in simulation, we need to allocate a shadow host buffer */
	b->cpu_address = ia_css_cpu_mem_alloc_page_aligned(size);
	if (b->cpu_address == NULL) {
		shared_memory_free(mid, b->mem);
		ia_css_cpu_mem_free(b);
		return NULL;
	}
#else
	/* on hw / real platform we can use the pointer from
	 * shared memory alloc
	 */
	b->cpu_address = (void *)HOST_ADDRESS(b->mem);
#endif

	b->css_address = shared_memory_map(sid, mid, b->mem);

	b->size	= size;
	b->state = buffer_unmapped;

	return b;
}


void
ia_css_input_buffer_free(
	vied_subsystem_t sid,
	vied_memory_t mid,
	ia_css_input_buffer b)
{
	if (b == NULL)
		return;
	if (b->state != buffer_unmapped)
		return;

#ifndef HRT_HW
	/* only free if we actually allocated it separately */
	ia_css_cpu_mem_free(b->cpu_address);
#endif
	shared_memory_unmap(sid, mid, b->css_address);
	shared_memory_free(mid, b->mem);
	ia_css_cpu_mem_free(b);
}


ia_css_input_buffer_cpu_address
ia_css_input_buffer_cpu_map(ia_css_input_buffer b)
{
	if (b == NULL)
		return NULL;
	if (b->state != buffer_unmapped)
		return NULL;

	/* map input buffer to CPU address space, acquire write access */
	b->state = buffer_write;

	/* return pre-mapped buffer */
	return b->cpu_address;
}


ia_css_input_buffer_cpu_address
ia_css_input_buffer_cpu_unmap(ia_css_input_buffer b)
{
	if (b == NULL)
		return NULL;
	if (b->state != buffer_write)
		return NULL;

	/* unmap input buffer from CPU address space, release write access */
	b->state = buffer_unmapped;

	/* return pre-mapped buffer */
	return b->cpu_address;
}


ia_css_input_buffer_css_address
ia_css_input_buffer_css_map(vied_memory_t mid, ia_css_input_buffer b)
{
	if (b == NULL)
		return 0;
	if (b->state != buffer_unmapped)
		return 0;

	/* map input buffer to CSS address space, acquire read access */
	b->state = buffer_read;

	/* now flush the cache */
	ia_css_cpu_mem_cache_flush(b->cpu_address, b->size);
#ifndef HRT_HW
	/* only copy in case of simulation, otherwise it should just work */
	/* copy data from CPU address space to CSS address space */
	shared_memory_store(mid, b->mem, b->cpu_address, b->size);
#else
	(void)mid;
#endif

	return (ia_css_input_buffer_css_address)b->css_address;
}


ia_css_input_buffer_css_address
ia_css_input_buffer_css_unmap(ia_css_input_buffer b)
{
	if (b == NULL)
		return 0;
	if (b->state != buffer_read)
		return 0;

	/* unmap input buffer from CSS address space, release read access */
	b->state = buffer_unmapped;

	/* input buffer only, no need to invalidate cache */

	return (ia_css_input_buffer_css_address)b->css_address;
}

