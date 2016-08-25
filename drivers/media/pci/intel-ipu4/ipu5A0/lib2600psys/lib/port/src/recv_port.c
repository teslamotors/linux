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

/* implemented: */
#include "recv_port.h"

/* used: */
#include "queue_struct.h"
#include "recv_port_struct.h"
#include "port_env_struct.h"
#include "regmem_access.h"
#include "buffer_access.h"
#include "storage_class.h"
#include "math_support.h"
#ifndef __VIED_CELL
#include "cpu_mem_support.h"
#endif

void
recv_port_open(struct recv_port *p, const struct sys_queue *q,
	       const struct port_env *env)
{
	p->mmid = env->mmid;
	p->ssid = env->ssid;
	p->mem_addr = env->mem_addr;

	p->size   = q->size;
	p->token_size = q->token_size;
	p->wr_reg = q->wr_reg;
	p->rd_reg = q->rd_reg;

#ifdef __VIED_CELL
	p->buffer = q->vied_address;
#else
	p->buffer = q->host_address;
#endif
}

STORAGE_CLASS_INLINE unsigned int
recv_port_index(const struct recv_port *p, unsigned int i)
{
	unsigned int rd = regmem_load_32(p->mem_addr, p->rd_reg, p->ssid);

	return OP_std_modadd(rd, i, p->size);
}

unsigned int
recv_port_available(const struct recv_port *p)
{
	int wr = (int)regmem_load_32(p->mem_addr, p->wr_reg, p->ssid);
	int rd = (int)regmem_load_32(p->mem_addr, p->rd_reg, p->ssid);

	return OP_std_modadd(wr, -rd, p->size);
}

STORAGE_CLASS_INLINE void
recv_port_copy(const struct recv_port *p, unsigned int i, void *data)
{
	unsigned int rd   = recv_port_index(p, i);
	unsigned int token_size = p->token_size;
	buffer_address addr = p->buffer + (rd * token_size);
#ifndef __VIED_CELL
	ia_css_cpu_mem_cache_invalidate((void *)HOST_ADDRESS(p->buffer),
					token_size*p->size);
#endif
	buffer_load(addr, data, token_size, p->mmid);
}

STORAGE_CLASS_INLINE void
recv_port_release(const struct recv_port *p, unsigned int i)
{
	unsigned int rd = recv_port_index(p, i);

	regmem_store_32(p->mem_addr, p->rd_reg, rd, p->ssid);
}

unsigned int
recv_port_transfer(const struct recv_port *p, void *data)
{
	if (!recv_port_available(p))
		return 0;
	recv_port_copy(p, 0, data);
	recv_port_release(p, 1);
	return 1;
}
