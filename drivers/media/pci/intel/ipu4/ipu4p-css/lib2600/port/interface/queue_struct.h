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

#ifndef __QUEUE_STRUCT_H
#define __QUEUE_STRUCT_H

/* queue description, shared between sender and receiver */

#include "type_support.h"

#ifdef __VIED_CELL
typedef struct {uint32_t v[2]; } host_buffer_address_t;
#else
typedef uint64_t		host_buffer_address_t;
#endif

typedef uint32_t		vied_buffer_address_t;


struct sys_queue {
	host_buffer_address_t host_address;
	vied_buffer_address_t vied_address;
	unsigned int size;
	unsigned int token_size;
	unsigned int wr_reg; /* reg no in subsystem's regmem */
	unsigned int rd_reg;
	unsigned int _align;
};

struct sys_queue_res {
	host_buffer_address_t host_address;
	vied_buffer_address_t vied_address;
	unsigned int reg;
};

#endif /* __QUEUE_STRUCT_H */
