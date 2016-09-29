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

#ifndef __BUFFER_H__
#define __BUFFER_H__

/* workaround: needed because <vied/shared_memory_map.h> uses size_t */
#include "type_support.h"
#include "vied/shared_memory_map.h"

typedef enum {
	buffer_unmapped,	/* buffer is not accessible by cpu, nor css */
	buffer_write,		/* output buffer: css has write access */
				/* input  buffer: cpu has write access */
	buffer_read,		/* input  buffer: css has read access */
				/* output buffer: cpu has read access */
	buffer_cpu,		/* shared buffer: cpu has read/write access */
	buffer_css		/* shared buffer: css has read/write access */
} buffer_state;

struct ia_css_buffer_s {
	/* number of bytes bytes allocated */
	unsigned int		size;
	/* allocated virtual memory object */
	host_virtual_address_t	mem;
	/* virtual address to be used on css/firmware */
	vied_virtual_address_t	css_address;
	/* virtual address to be used on cpu/host */
	void *cpu_address;
	buffer_state		state;
};

typedef struct ia_css_buffer_s *ia_css_buffer_t;

ia_css_buffer_t
ia_css_buffer_alloc(
	vied_subsystem_t sid,
	vied_memory_t mid,
	unsigned int size);

void
ia_css_buffer_free(
	vied_subsystem_t sid,
	vied_memory_t mid,
	ia_css_buffer_t b);

#endif /*__BUFFER_H__*/
