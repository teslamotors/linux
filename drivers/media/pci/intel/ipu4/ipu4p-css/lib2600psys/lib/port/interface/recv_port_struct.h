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

#ifndef __RECV_PORT_STRUCT_H
#define __RECV_PORT_STRUCT_H

#include "buffer_type.h"

struct recv_port {
	buffer_address buffer;	/* address of buffer in DDR */
	unsigned int size;
	unsigned int token_size;
	unsigned int wr_reg;	/* index of write pointer located in regmem */
	unsigned int rd_reg;	/* index read pointer located in regmem */

	unsigned int mmid;
	unsigned int ssid;
	unsigned int mem_addr;	/* address of memory containing regmem */
};

#endif /* __RECV_PORT_STRUCT_H */
