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

#include "queue.h"

#include "regmem_access.h"
#include "port_env_struct.h"

unsigned int sys_queue_buf_size(unsigned int size, unsigned int token_size)
{
	return (size + 1) * token_size;
}

void
sys_queue_init(struct sys_queue *q, unsigned int size, unsigned int token_size,
	       struct sys_queue_res *res)
{
	unsigned int buf_size;

	q->size         = size + 1;
	q->token_size   = token_size;
	buf_size = sys_queue_buf_size(size, token_size);

	/* acquire the shared buffer space */
	q->host_address = res->host_address;
	res->host_address += buf_size;
	q->vied_address	= res->vied_address;
	res->vied_address += buf_size;

	/* acquire the shared read and writer pointers */
	q->wr_reg = res->reg;
	res->reg++;
	q->rd_reg = res->reg;
	res->reg++;

}
