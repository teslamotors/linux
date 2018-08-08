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

#ifndef __IA_CSS_SYSCOM_H
#define __IA_CSS_SYSCOM_H


/*
 * The CSS Subsystem Communication Interface - Host side
 *
 * It provides subsystem initialzation, send ports and receive ports
 * The PSYS and ISYS interfaces are implemented on top of this interface.
 */

#include "ia_css_syscom_config.h"

#define FW_ERROR_INVALID_PARAMETER	(-1)
#define FW_ERROR_BAD_ADDRESS		(-2)
#define FW_ERROR_BUSY			(-3)
#define FW_ERROR_NO_MEMORY		(-4)

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
 */
extern void
ia_css_syscom_size(
	const struct ia_css_syscom_config *cfg,
	struct ia_css_syscom_size *size
);

/**
 * ia_css_syscom_open() - initialize a subsystem context
 * @config: pointer to the configuration data (read)
 * @buf: pointer to externally allocated buffers (read)
 * @returns: struct ia_css_syscom_context* on success, 0 otherwise.
 *
 * Purpose:
 * - initialize host side data structures
 * - boot the subsystem?
 *
 */
extern struct ia_css_syscom_context*
ia_css_syscom_open(
	struct ia_css_syscom_config  *config,
	struct ia_css_syscom_buf   *buf
);

/**
 * ia_css_syscom_close() - signal close to cell
 * @context: pointer to the subsystem context
 * @returns: 0 on success, -2 (FW_ERROR_BUSY) if SPC is not ready yet.
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
 * @returns: 0 on success, -2 (FW_ERROR_BUSY) if cell
 * is busy and call was not forced.
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
 * @returns: 0 on success, -1 (FW_ERROR_INVALID_PARAMETER) otherwise.
 */
extern int
ia_css_syscom_send_port_open(
	struct ia_css_syscom_context *context,
	unsigned int port
);

/**
 * Closes a port for sending tokens to the subsystem
 * @context: pointer to the subsystem context
 * @port: send port index
 * @returns: 0 on success, -1 (FW_ERROR_INVALID_PARAMETER) otherwise.
 */
extern int
ia_css_syscom_send_port_close(
	struct ia_css_syscom_context *context,
	unsigned int port
);

/**
 * Get the number of tokens that can be sent to a port without error.
 * @context: pointer to the subsystem context
 * @port: send port index
 * @returns: number of available tokens on success,
 * -1 (FW_ERROR_INVALID_PARAMETER) otherwise.
  */
extern int
ia_css_syscom_send_port_available(
	struct ia_css_syscom_context *context,
	unsigned int port
);

/**
 * Send a token to the subsystem port
 * The token size is determined during initialization
 * @context: pointer to the subsystem context
 * @port: send port index
 * @token: pointer to the token value that is transferred to the subsystem
 * @returns: number of tokens sent on success,
 * -1 (FW_ERROR_INVALID_PARAMETER) otherwise.
 */
extern int
ia_css_syscom_send_port_transfer(
	struct ia_css_syscom_context *context,
	unsigned int port,
	const void *token
);

/**
 * Open a port for receiving tokens to the subsystem
 * @context: pointer to the subsystem context
 * @port: receive port index
 * @returns: 0 on success, -1 (FW_ERROR_INVALID_PARAMETER) otherwise.
 */
extern int
ia_css_syscom_recv_port_open(
	struct ia_css_syscom_context *context,
	unsigned int port
);

/**
 * Closes a port for receiving tokens to the subsystem
 * Returns 0 on success, otherwise negative value of error code
 * @context: pointer to the subsystem context
 * @port: receive port index
 * @returns: 0 on success, -1 (FW_ERROR_INVALID_PARAMETER) otherwise.
 */
extern int
ia_css_syscom_recv_port_close(
	struct ia_css_syscom_context *context,
	unsigned int port
);

/**
 * Get the number of tokens that can be received from a port without errors.
 * @context: pointer to the subsystem context
 * @port: receive port index
 * @returns: number of available tokens on success,
 * -1 (FW_ERROR_INVALID_PARAMETER) otherwise.
 */
extern int
ia_css_syscom_recv_port_available(
	struct ia_css_syscom_context *context,
	unsigned int port
);

/**
 * Receive a token from the subsystem port
 * The token size is determined during initialization
 * @context: pointer to the subsystem context
 * @port: receive port index
 * @token (output): pointer to (space for) the token to be received
 * @returns: number of tokens received on success,
 * -1 (FW_ERROR_INVALID_PARAMETER) otherwise.
 */
extern int
ia_css_syscom_recv_port_transfer(
	struct ia_css_syscom_context *context,
	unsigned int port,
	void *token
);

#if HAS_DUAL_CMD_CTX_SUPPORT
/**
 * ia_css_syscom_store_dmem() - store subsystem context information in DMEM
 * @context: pointer to the subsystem context
 * @ssid: subsystem id
 * @vtl0_addr_mask: VTL0 address mask; only applicable when the passed in context is secure
 * @returns: 0 on success, -1 (FW_ERROR_INVALID_PARAMETER) otherwise.
 */
extern int
ia_css_syscom_store_dmem(
	struct ia_css_syscom_context *context,
	unsigned int ssid,
	unsigned int vtl0_addr_mask
);

/**
 * ia_css_syscom_set_trustlet_status() - store truslet configuration setting
 * @context: pointer to the subsystem context
 * @trustlet_exist: 1 if trustlet exists
 */
extern void
ia_css_syscom_set_trustlet_status(
	unsigned int dmem_addr,
	unsigned int ssid,
	bool trustlet_exist
);

/**
 * ia_css_syscom_is_ab_spc_ready() - check if SPC access blocker programming is completed
 * @context: pointer to the subsystem context
 * @returns: 1 when status is ready. 0 otherwise
 */
bool
ia_css_syscom_is_ab_spc_ready(
	struct ia_css_syscom_context *ctx
);
#endif /* HAS_DUAL_CMD_CTX_SUPPORT */

#endif /* __IA_CSS_SYSCOM_H */
