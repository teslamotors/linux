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

#ifndef __IA_CSS_SYSCOM_CONTEXT_H
#define __IA_CSS_SYSCOM_CONTEXT_H

#include <vied/shared_memory_access.h>

#include "port_env_struct.h"
#include <type_support.h>

/* host context */
struct ia_css_syscom_context {
	vied_virtual_address_t	cell_firmware_addr;
	unsigned int		cell_regs_addr;
	unsigned int		cell_dmem_addr;

	struct port_env env;

	unsigned int num_input_queues;
	unsigned int num_output_queues;

	/* array of input queues (from host to SP) */
	struct sys_queue *input_queue;
	/* array of output queues (from SP to host) */
	struct sys_queue *output_queue;

	struct send_port *send_port;
	struct recv_port *recv_port;

	unsigned int regmem_idx;
	unsigned int free_buf;

	host_virtual_address_t config_host_addr;
	host_virtual_address_t input_queue_host_addr;
	host_virtual_address_t output_queue_host_addr;
	host_virtual_address_t specific_host_addr;
	host_virtual_address_t ibuf_host_addr;
	host_virtual_address_t obuf_host_addr;

	vied_virtual_address_t config_vied_addr;
	vied_virtual_address_t input_queue_vied_addr;
	vied_virtual_address_t output_queue_vied_addr;
	vied_virtual_address_t specific_vied_addr;
	vied_virtual_address_t ibuf_vied_addr;
	vied_virtual_address_t obuf_vied_addr;

	/* if true; secure syscom object as in VTIO Case
	 * if false, non-secure syscom
	 */
	bool secure;
};

#endif /* __IA_CSS_SYSCOM_CONTEXT_H */
