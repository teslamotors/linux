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

#ifndef __IA_CSS_SYSCOM_H__
#define __IA_CSS_SYSCOM_H__


/*
 * The CSS Subsystem Communication Interface - Host side
 *
 * It provides subsystem initialzation, send ports and receive ports
 * The PSYS and ISYS interfaces are implemented on top of this interface.
 */

#include "ia_css_syscom_config.h"

struct ia_css_syscom_context;


/**
 * ia_css_syscom_size() - provide syscom external buffer requirements
 * @config: pointer to the configuration data (read)
 * @size: pointer to the buffer size (write)
 *
 * Purpose:
 * - Provide external buffer requirements
 * - To be used for external buffer allocation
 *
 * Return: none.
 */
extern void
ia_css_syscom_size(
	const struct ia_css_syscom_config *cfg,
	struct ia_css_syscom_size *size);

/**
 * ia_css_syscom_open() - initialize a subsystem context
 * @config: pointer to the configuration data (read)
 * @buf: pointer to externally allocated buffers (read)
 *
 * Purpose:
 * - initialize host side data structures
 * - boot the subsystem?
 *
 * Return: struct ia_css_syscom_context* on success, 0 otherwise.
 */
extern struct ia_css_syscom_context*
ia_css_syscom_open(
	struct ia_css_syscom_config  *config,
	struct ia_css_syscom_buf   *buf
);

/**
 * ia_css_syscom_close() - signal close to cell
 * @context: pointer to the subsystem context
 *
 * Purpose:
 * Request from the Cell to terminate
 */
extern int
ia_css_syscom_close(
	struct ia_css_syscom_context *context
);

/**
 * ia_css_syscom_release() - free context
 * @context: pointer to the subsystem context
 * @force: flag which specifies whether cell
 * state will be checked before freeing the
 * context.
 *
 * Purpose:
 * 2 modes, with first (force==true) immediately
 * free context, and second (force==false) verifying
 * that the cell state is ok and freeing context if so,
 * returning error otherwise.
 */
extern int
ia_css_syscom_release(
	struct ia_css_syscom_context *context,
	unsigned int force
);

/**
 * Open a port for sending tokens to the subsystem
 * @context: pointer to the subsystem context
 * @port: send port index
 */
extern int
ia_css_syscom_send_port_open(
	struct ia_css_syscom_context *context,
	unsigned int port
);

extern int
ia_css_syscom_send_port_close(
	struct ia_css_syscom_context *context,
	unsigned int port
);

/**
 * Get the number of tokens that can be sent to a port without error.
 * @context: pointer to the subsystem context
 * @port: send port index
 * @num_tokens (output): number of tokens that can be sent
 */
extern int
ia_css_syscom_send_port_available(
	struct ia_css_syscom_context *context,
        unsigned int port,
	unsigned int* num_tokens
);

/**
 * Send a token to the subsystem port
 * Returns an error when there is no space for the token
 * The token size is determined during initialization
 * @context: pointer to the subsystem context
 * @port: send port index
 * @token: pointer to the token value that is transferred to the subsystem
 */
extern int
ia_css_syscom_send_port_transfer(
	struct ia_css_syscom_context *context,
        unsigned int port,
	const void* token
);

/**
 * Open a port for receiving tokens to the subsystem
 * @context: pointer to the subsystem context
 * @port: receive port index
 */
extern int
ia_css_syscom_recv_port_open(
	struct ia_css_syscom_context* context,
	unsigned int port
);

extern int
ia_css_syscom_recv_port_close(
	struct ia_css_syscom_context* context,
	unsigned int port
);

/**
 * Get the number of tokens that can be received from a port without errors.
 * @context: pointer to the subsystem context
 * @port: receive port index
 * @num_tokens (output): number of tokens that can be received
 */
extern int
ia_css_syscom_recv_port_available(
	struct ia_css_syscom_context* context,
        unsigned int port,
	unsigned int* num_tokens
);

/**
 * Receive a token from the subsystem port
 * Returns an error when there are no tokens available
 * The token size is determined during initialization
 * @context: pointer to the subsystem context
 * @port: receive port index
 * @token (output): pointer to (space for) the token to be received
 */
extern int
ia_css_syscom_recv_port_transfer(
	struct ia_css_syscom_context* context,
        unsigned int port,
	void* token
);

#endif /* __IA_CSS_SYSCOM_H__*/
