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

#ifndef __IA_CSS_SYSCOM_CONFIG_FW_H__
#define __IA_CSS_SYSCOM_CONFIG_FW_H__

#include "type_support.h"

enum {
	/* Program load or explicit host setting should init to this */
	SYSCOM_STATE_UNINIT	= 0x57A7E000,
	/* SP Syscom sets this when it is ready for use */
	SYSCOM_STATE_READY	= 0x57A7E001,
	/* SP Syscom sets this when no more syscom accesses will happen */
	SYSCOM_STATE_INACTIVE	= 0x57A7E002
};

enum {
	/* Program load or explicit host setting should init to this */
	SYSCOM_COMMAND_UNINIT	= 0x57A7F000,
	/* Host Syscom requests syscom to become inactive */
	SYSCOM_COMMAND_INACTIVE = 0x57A7F001
};

/* firmware config: data that sent from the host to SP via DDR */
/* Cell copies data into a context */

struct ia_css_syscom_config_fw {
	unsigned int firmware_address;

	unsigned int num_input_queues;
	unsigned int num_output_queues;
	unsigned int input_queue; /* hmm_ptr / struct queue* */
	unsigned int output_queue; /* hmm_ptr / struct queue* */

	unsigned int specific_addr; /* vied virtual address */
	unsigned int specific_size;
};

#endif /*__IA_CSS_SYSCOM_CONFIG_FW_H__*/
