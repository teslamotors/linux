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

#ifndef __QUEUE_H
#define __QUEUE_H

#include "queue_struct.h"
#include "port_env_struct.h"

/*
 * SYS queues are created by the host
 * SYS queues cannot be accessed through the queue interface
 * To send data into a queue a send_port must be opened.
 * To receive data from a queue, a recv_port must be opened.
 */

/* return required buffer size for queue */
unsigned int
sys_queue_buf_size(unsigned int size, unsigned int token_size);

/*
 * initialize a queue that can hold at least 'size' tokens of
 * 'token_size' bytes.
 */
void
sys_queue_init(struct sys_queue *q, unsigned int size,
		unsigned int token_size, struct sys_queue_res *res);

#endif /* __QUEUE_H */
