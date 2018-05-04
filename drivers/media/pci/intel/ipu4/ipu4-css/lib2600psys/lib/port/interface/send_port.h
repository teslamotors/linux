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

#ifndef __SEND_PORT_H
#define __SEND_PORT_H


/*
 * A send port can be used to send tokens into a queue.
 * The interface can be used on any type of processor (host, SP, ...)
 */

struct send_port;
struct sys_queue;
struct port_env;

/*
 * Open a send port on a queue. After the port is opened, tokens can be sent
 */
void
send_port_open(struct send_port *p, const struct sys_queue *q,
		const struct port_env *env);

/*
 * Determine how many tokens can be sent
 */
unsigned int
send_port_available(const struct send_port *p);

/*
 * Send a token via a send port. The function returns the number of
 * tokens that have been sent:
 * 1: the token was accepted
 * 0: the token was not accepted (full queue)
 * The size of a token is determined at initialization.
 */
unsigned int
send_port_transfer(const struct send_port *p, const void *data);


#endif /* __SEND_PORT_H */
