/**
* Support for Intel Camera Imaging ISP subsystem.
* Copyright (c) 2010 - 2015, Intel Corporation.
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

#ifndef __IA_CSS_SYSCOM_CONFIG_H__
#define __IA_CSS_SYSCOM_CONFIG_H__

#include <vied/shared_memory_access.h>

/* syscom size struct, output of ia_css_syscom_size, input for (external) allocation */
struct ia_css_syscom_size {
	unsigned int cpu;  /* Size of host buffer */
	unsigned int shm;  /* Size of shared config buffer        (host to cell) */
	unsigned int ibuf; /* Size of shared input queue buffers  (host to cell) */
	unsigned int obuf; /* Size of shared output queue buffers (cell to host) */
};

/* syscom buffer struct, output of (external) allocation, input for ia_css_syscom_open */
struct ia_css_syscom_buf {
	char *cpu; /* host buffer */

	host_virtual_address_t shm_host; /* shared memory buffer host address */
	vied_virtual_address_t shm_cell; /* shared memory buffer cell address */

	host_virtual_address_t ibuf_host; /* input queue shared buffer host address */
	vied_virtual_address_t ibuf_cell; /* input queue shared buffer cell address */

	host_virtual_address_t obuf_host; /* output queue shared buffer host address */
	vied_virtual_address_t obuf_cell; /* output queue shared buffer cell address */
};

/**
  * Parameter struct for ia_css_syscom_open
  */
struct ia_css_syscom_config {
	/* This member in no longer used in syscom.
	   It is kept to not break any driver builds, and will be removed when
	   all assignments have been removed from driver code */
	/* address of firmware in DDR/IMR */
	unsigned long long host_firmware_address;

	/* address of firmware in DDR, seen from SPC */
	unsigned int vied_firmware_address;

	unsigned int ssid;
	unsigned int mmid;
	unsigned int num_input_queues;
	unsigned int num_output_queues;
	unsigned int input_queue_size; /* max # tokens per queue */
	unsigned int output_queue_size; /* max # tokens per queue */
	unsigned int input_token_size; /* in bytes */
	unsigned int output_token_size; /* in bytes */

	unsigned int regs_addr;
	unsigned int dmem_addr;

	/* firmware-specific configuration data */
	void* specific_addr;
	unsigned int specific_size;
};

#endif /*__IA_CSS_SYSCOM_CONFIG_H__*/

