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

#include "send_port.h"
#include "queue_struct.h"        /* for sys_queue                       */
#include "send_port_struct.h"    /* for send_port                       */
#include "port_env_struct.h"     /* for port_env                        */
#include "regmem_access.h"       /* for regmem_load_32, regmem_store_32 */
#include "buffer_access.h"       /* for buffer_store, buffer_address    */
#include "storage_class.h"       /* for STORAGE_CLASS_INLINE            */
#include "math_support.h"        /* for OP_std_modadd                   */
#include "type_support.h"        /* for HOST_ADDRESS                    */

#ifndef __VIED_CELL
#include "cpu_mem_support.h"     /* for ia_css_cpu_mem_cache_flush      */
#endif

void
send_port_open(struct send_port *p, const struct sys_queue *q,
	       const struct port_env *env)
{
	p->mmid		= env->mmid;
	p->ssid		= env->ssid;
	p->mem_addr	= env->mem_addr;

	p->size         = q->size;
	p->token_size   = q->token_size;
	p->wr_reg	= q->wr_reg;
	p->rd_reg	= q->rd_reg;
#ifdef __VIED_CELL
	p->buffer       = q->vied_address;
#else
	p->buffer       = q->host_address;
#endif
}

STORAGE_CLASS_INLINE unsigned int
send_port_index(const struct send_port *p, unsigned int i)
{
	unsigned int wr = regmem_load_32(p->mem_addr, p->wr_reg, p->ssid);

	return OP_std_modadd(wr, i, p->size);
}

unsigned int
send_port_available(const struct send_port *p)
{
	int rd   = (int)regmem_load_32(p->mem_addr, p->rd_reg, p->ssid);
	int wr   = (int)regmem_load_32(p->mem_addr, p->wr_reg, p->ssid);

	return OP_std_modadd(rd, -(wr+1), p->size);
}

STORAGE_CLASS_INLINE void
send_port_copy(const struct send_port *p, unsigned int i, const void *data)
{
	unsigned int wr   = send_port_index(p, i);
	unsigned int token_size = p->token_size;
	buffer_address addr = p->buffer + (wr * token_size);

	buffer_store(addr, data, token_size, p->mmid);
#ifndef __VIED_CELL
	ia_css_cpu_mem_cache_flush((void *)HOST_ADDRESS(addr), token_size);
#endif
}

STORAGE_CLASS_INLINE void
send_port_release(const struct send_port *p, unsigned int i)
{
	unsigned int wr = send_port_index(p, i);

	regmem_store_32(p->mem_addr, p->wr_reg, wr, p->ssid);
}

unsigned int
send_port_transfer(const struct send_port *p, const void *data)
{
	if (!send_port_available(p))
		return 0;
	send_port_copy(p, 0, data);
	send_port_release(p, 1);
	return 1;
}
