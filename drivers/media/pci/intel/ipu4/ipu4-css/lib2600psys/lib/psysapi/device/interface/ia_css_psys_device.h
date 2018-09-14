/*
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

#ifndef __IA_CSS_PSYS_DEVICE_H
#define __IA_CSS_PSYS_DEVICE_H

#include "ia_css_psys_init.h"
#include "ia_css_psys_transport.h"

/*! \file */

/** @file ia_css_psys_device.h
 *
 * Define the interface to open the psys specific communication layer
 * instance
 */

#include <vied_nci_psys_system_global.h> /* vied_vaddress_t */

#include <type_support.h>
#include <print_support.h>

#include <ia_css_syscom_config.h>
#include <ia_css_syscom.h>

#define IA_CSS_PSYS_STATE_READY_PATTERN (0xF7F7F7F7)
#define IA_CSS_PSYS_STATE_RUNNING_PATTERN (0xE6E6E6E6)
#define IA_CSS_PSYS_STATE_STARTING_PATTERN (0xD5D5D5D5)
#define IA_CSS_PSYS_STATE_STARTED_PATTERN (0xC4C4C4C4)
#define IA_CSS_PSYS_STATE_INITIALIZING_PATTERN (0xB3B3B3B3)
#define IA_CSS_PSYS_STATE_INITIALIZED_PATTERN (0xA0A0A0A0)

/*
 * Defines the state of psys:
 * - IA_CSS_PSYS_STATE_UNKNOWN = psys status is unknown (or not recognized)
 * - IA_CSS_PSYS_STATE_INITIALING = some of the psys components are
 *   not initialized yet
 * - IA_CSS_PSYS_STATE_INITIALIZED = psys components are initialized
 * - IA_CSS_PSYS_STATE_STARTING = some of the psys components are initialized
 *   but not started yet
 * - IA_CSS_PSYS_STATE_STARTED = psys components are started
 * - IA_CSS_PSYS_STATE_RUNNING = some of the psys components are started
 *   but not ready yet
 * - IA_CSS_PSYS_STATE_READY = psys is ready
 * The state of psys can be obtained calling ia_css_psys_check_state()
*/
typedef enum ia_css_psys_state {
	IA_CSS_PSYS_STATE_UNKNOWN = 0, /**< psys state is unknown */
	/*< some of the psys components are not initialized yet*/
	IA_CSS_PSYS_STATE_INITIALIZING = IA_CSS_PSYS_STATE_INITIALIZING_PATTERN,
	/**< psys components are initialized */
	IA_CSS_PSYS_STATE_INITIALIZED = IA_CSS_PSYS_STATE_INITIALIZED_PATTERN,
	/**< some of the psys components are not started yet */
	IA_CSS_PSYS_STATE_STARTING = IA_CSS_PSYS_STATE_STARTING_PATTERN,
	/**< psys components are started */
	IA_CSS_PSYS_STATE_STARTED = IA_CSS_PSYS_STATE_STARTED_PATTERN,
	/**< some of the psys components are not ready yet */
	IA_CSS_PSYS_STATE_RUNNING = IA_CSS_PSYS_STATE_RUNNING_PATTERN,
	/**< psys is ready */
	IA_CSS_PSYS_STATE_READY = IA_CSS_PSYS_STATE_READY_PATTERN,
} ia_css_psys_state_t;

extern struct ia_css_syscom_context *psys_syscom;
#if HAS_DUAL_CMD_CTX_SUPPORT
extern struct ia_css_syscom_context *psys_syscom_secure;
#endif

/*! Print the syscom creation descriptor to file/stream

 @param	config[in]	Psys syscom descriptor
 @param	fid[out]	file/stream handle

 @return < 0 on error
*/
extern int ia_css_psys_config_print(
	const struct ia_css_syscom_config *config, void *fid);

/*! Print the Psys syscom object to file/stream

 @param	context[in]	Psys syscom object
 @param	fid[out]	file/stream handle

 @return < 0 on error
 */
extern int ia_css_psys_print(
	const struct ia_css_syscom_context *context, void *fid);

/*! Create the syscom creation descriptor

 @return NULL on error
 */
extern struct ia_css_syscom_config *ia_css_psys_specify(void);

#if HAS_DUAL_CMD_CTX_SUPPORT
/*! Create the syscom creation descriptor for secure stream

 @param	vtl0_addr_mask[in]	VTL0 address mask that will be stored in 'secure' ctx
 @return NULL on error
 */
extern struct ia_css_syscom_config *ia_css_psys_specify_secure(unsigned int vtl0_addr_mask);
#endif

/*! Compute the size of storage required for allocating the Psys syscom object

 @param	config[in]	Psys syscom descriptor

 @return 0 on error
 */
extern size_t ia_css_sizeof_psys(
	struct ia_css_syscom_config *config);

#if HAS_DUAL_CMD_CTX_SUPPORT
/*! Open (and map the storage for) the Psys syscom object
    This is the same as ia_css_psys_open() excluding server start.
    Target for VTIO usage where multiple syscom objects need to be
    created first before this API is invoked.

 @param	buffer[in] storage buffers for the syscom object
		   in the kernel virtual memory space and
		   its Psys mapped version
 @param	config[in] Psys syscom descriptor
 @return NULL on error
 */

extern struct ia_css_syscom_context *ia_css_psys_context_create(
	const struct ia_css_psys_buffer_s *buffer,
	struct ia_css_syscom_config *config);

/*! Store the parameters of the Psys syscom object in DMEM, so
    they can be communicated with FW. This step needs to be invoked
    after SPC starts in ia_css_psys_open(), so SPC DMEM access blocker
    programming already takes effective.

 @param	context[in]	Psys syscom object
 @param	config[in]	Psys syscom descriptor
 @return 0 if successful
 */
extern int ia_css_psys_context_store_dmem(
	struct ia_css_syscom_context *context,
	struct ia_css_syscom_config *config);

/*! Start PSYS Server. Psys syscom object must have been created already.
    Target for VTIO usage where multiple syscom objects need to be
    created first before this API is invoked.
 @param	config[in] Psys syscom descriptor

 @return true if psys open started successfully
 */
extern int ia_css_psys_open(
	struct ia_css_syscom_config *config);
#else
/*! Open (and map the storage for) the Psys syscom object

 @param	buffer[in] storage buffers for the syscom object
		   in the kernel virtual memory space and
		   its Psys mapped version
 @param	config[in] Psys syscom descriptor

 Precondition(1): The buffer must be large enough to hold the syscom object.
 Its size must be computed with the function "ia_css_sizeof_psys()".
 The buffer must be created in the kernel memory space.

 Precondition(2):  If buffer == NULL, the storage allocations and mapping
 is performed in this function. Config must hold the handle to the Psys
 virtual memory space

 Postcondition: The context is initialised in the provided/created buffer.
 The syscom context pointer is the kernel space handle to the syscom object

 @return NULL on error
 */
extern struct ia_css_syscom_context *ia_css_psys_open(
	const struct ia_css_psys_buffer_s *buffer,
	struct ia_css_syscom_config *config);
#endif /* HAS_DUAL_CMD_CTX_SUPPORT */

/*! completes the psys open procedure. Must be called multiple times
    until it succeeds or driver determines the boot sequence has failed.

 @param	context[in]	Psys syscom object

 @return false if psys open has not completed successfully
 */
extern bool ia_css_psys_open_is_ready(
	struct ia_css_syscom_context *context);

#if HAS_DUAL_CMD_CTX_SUPPORT
/*! Request close of a PSYS context
 * The functionatlity is the same as ia_css_psys_close() which closes PSYS syscom object.
 * Counterpart of ia_css_psys_context_create()
 * @param	context[in]:	Psys context
 * @return	NULL if close is successful context otherwise
 */
extern struct ia_css_syscom_context *ia_css_psys_context_destroy(
	struct ia_css_syscom_context *context);

/*! Request close of a PSYS device for VTIO case
 * @param	None
 * @return	0 if successful
 */
extern int ia_css_psys_close(void);
#else
/*! Request close of a PSYS context
 * @param	context[in]:	Psys context
 * @return	NULL if close is successful context otherwise
 */
extern struct ia_css_syscom_context *ia_css_psys_close(
	struct ia_css_syscom_context *context);
#endif /* HAS_DUAL_CMD_CTX_SUPPORT*/

/*! Unmap and free the storage of the PSYS context
 * @param	context[in]	Psys context
 * @param	force[in]	Force release even if device is busy
 * @return	0      if release is successful
 *		EINVAL if context is invalid
 *		EBUSY  if device is not yet idle, and force==0
 */
extern int ia_css_psys_release(
	struct ia_css_syscom_context *context,
	bool force);

/*! Checks the state of the Psys syscom object

 @param	context[in]	Psys syscom object

 @return State of the syscom object
 */
extern ia_css_psys_state_t ia_css_psys_check_state(
	struct ia_css_syscom_context *context);

/*!Indicate if the designated cmd queue in the Psys syscom object is full

 @param	context[in]	Psys syscom object
 @param	id[in]		Psys syscom cmd queue ID

 @return false if the cmd queue is not full or on error
 */

extern bool ia_css_is_psys_cmd_queue_full(
	struct ia_css_syscom_context *context,
	ia_css_psys_cmd_queue_ID_t id);

/*!Indicate if the designated cmd queue in the Psys syscom object is notfull

 @param	context[in]	Psys syscom object
 @param	id[in]		Psys syscom cmd queue ID

 @return false if the cmd queue is full on error
 */
extern bool ia_css_is_psys_cmd_queue_not_full(
	struct ia_css_syscom_context *context,
	ia_css_psys_cmd_queue_ID_t id);

/*!Indicate if the designated cmd queue in the Psys syscom object holds N space

 @param	context[in]	Psys syscom object
 @param	id[in]		Psys syscom cmd queue ID
 @param	N[in]		Number of messages

 @return false if the cmd queue space is unavailable or on error
 */
extern bool ia_css_has_psys_cmd_queue_N_space(
	struct ia_css_syscom_context *context,
	ia_css_psys_cmd_queue_ID_t id,
	const unsigned int N);

/*!Return the free space count in the designated cmd queue in the
 * Psys syscom object

 @param	context[in]	Psys syscom object
 @param	id[in]		Psys syscom cmd queue ID

 @return the space, < 0 on error
 */
extern int ia_css_psys_cmd_queue_get_available_space(
	struct ia_css_syscom_context *context,
	ia_css_psys_cmd_queue_ID_t id);

/*!Indicate if there are any messages pending in the Psys syscom
 * object event queues

 @param	context[in]	Psys syscom object

 @return false if there are no messages or on error
 */
extern bool ia_css_any_psys_event_queue_not_empty(
	struct ia_css_syscom_context *context);

/*!Indicate if the designated event queue in the Psys syscom object is empty

 @param	context[in]	Psys syscom object
 @param	id[in]		Psys syscom event queue ID

 @return false if the event queue is not empty or on error
 */
extern bool ia_css_is_psys_event_queue_empty(
	struct ia_css_syscom_context *context,
	ia_css_psys_event_queue_ID_t id);

/*!Indicate if the designated event queue in the Psys syscom object is not empty

 @param	context[in]	Psys syscom object
 @param	id[in]		Psys syscom event queue ID

 @return false if the receive queue is empty or on error
 */
extern bool ia_css_is_psys_event_queue_not_empty(
	struct ia_css_syscom_context *context,
	ia_css_psys_event_queue_ID_t id);

/*!Indicate if the designated event queue
 * in the Psys syscom object holds N items

 @param	context[in]	Psys syscom object
 @param	id[in]		Psys syscom event queue ID
 @param	N[in]		Number of messages

 @return false if the event queue has insufficient messages
	available or on error
*/
extern bool ia_css_has_psys_event_queue_N_msgs(
	struct ia_css_syscom_context *context,
	ia_css_psys_event_queue_ID_t id,
	const unsigned int N);

/*!Return the message count in the designated event queue in the
 * Psys syscom object

 @param	context[in]	Psys syscom object
 @param	id[in]		Psys syscom event queue ID

 @return the messages, < 0 on error
 */
extern int ia_css_psys_event_queue_get_available_msgs(
	struct ia_css_syscom_context *context,
	ia_css_psys_event_queue_ID_t id);

/*! Send (pass by value) a command on a queue in the Psys syscom object

 @param	context[in]		Psys syscom object
 @param	id[in]			Psys syscom cmd queue ID
@param	cmd_msg_buffer[in]	pointer to the command message buffer

Precondition: The command message buffer must be large enough
	      to hold the command

Postcondition: Either 0 or 1 commands have been sent

Note: The message size is fixed and determined on creation

 @return the number of sent commands (1), <= 0 on error
 */
extern int ia_css_psys_cmd_queue_send(
	struct ia_css_syscom_context *context,
	ia_css_psys_cmd_queue_ID_t id,
	const void *cmd_msg_buffer);

/*! Send (pass by value) N commands on a queue in the Psys syscom object

 @param	context[in]		Psys syscom object
 @param	id[in]			Psys syscom cmd queue ID
 @param	cmd_msg_buffer[in]	Pointer to the command message buffer
@param	N[in]			Number of commands

Precondition: The command message buffer must be large enough
	      to hold the commands

Postcondition: Either 0 or up to and including N commands have been sent

 Note: The message size is fixed and determined on creation

 @return the number of sent commands, <= 0 on error
 */
extern int ia_css_psys_cmd_queue_send_N(
	struct ia_css_syscom_context *context,
	ia_css_psys_cmd_queue_ID_t id,
	const void *cmd_msg_buffer,
	const unsigned int N);

/*! Receive (pass by value) an event from an event queue in the
 *  Psys syscom object

 @param	context[in]		Psys syscom object
 @param	id[in]			Psys syscom event queue ID
 @param	event_msg_buffer[out]	pointer to the event message buffer

 Precondition: The event message buffer must be large enough to hold the event

 Postcondition: Either 0 or 1 events have been received

 Note: The event size is fixed and determined on creation

 @return the number of received events (1), <= 0 on error
 */
extern int ia_css_psys_event_queue_receive(
	struct ia_css_syscom_context *context,
	ia_css_psys_event_queue_ID_t id,
	void *event_msg_buffer);

/*! Receive (pass by value) N events from an event queue in the
 * Psys syscom object

 @param	context[in]		Psys syscom object
 @param	id[in]			Psys syscom event queue ID
 @param	event_msg_buffer[out]	pointer to the event message buffer
 @param	N[in]			Number of events

 Precondition: The event buffer must be large enough to hold the events

 Postcondition: Either 0 or up to and including N events have been received

 Note: The message size is fixed and determined on creation

 @return the number of received event messages, <= 0 on error
 */
extern int ia_css_psys_event_queue_receive_N(
	struct ia_css_syscom_context *context,
	ia_css_psys_event_queue_ID_t id,
	void *event_msg_buffer,
	const unsigned int N);


/*
 * Access functions to query the object stats
 */


/*!Return the size of the Psys syscom object

 @param	context[in] Psys syscom object

 @return 0 on error
 */
extern size_t ia_css_psys_get_size(
	const struct ia_css_syscom_context *context);

/*!Return the number of cmd queues in the Psys syscom object

 @param	context[in]	Psys syscom object

 @return 0 on error
 */
extern unsigned int ia_css_psys_get_cmd_queue_count(
	const struct ia_css_syscom_context *context);

/*!Return the number of event queues in the Psys syscom object

 @param	context[in]	Psys syscom object

 @return 0 on error
 */
extern unsigned int ia_css_psys_get_event_queue_count(
	const struct ia_css_syscom_context *context);

/*!Return the size of the indicated Psys command queue

 @param	context[in]	Psys syscom object
 @param	id[in]		Psys syscom cmd queue ID

 Note: The queue size is expressed in the number of fields

 @return 0 on error
 */
extern size_t ia_css_psys_get_cmd_queue_size(
	const struct ia_css_syscom_context *context,
	ia_css_psys_cmd_queue_ID_t id);

/*!Return the size of the indicated Psys event queue

 @param	context[in]	Psys syscom object
 @param	id[in]		Psys syscom event queue ID

 Note: The queue size is expressed in the number of fields

 @return 0 on error
 */
extern size_t ia_css_psys_get_event_queue_size(
	const struct ia_css_syscom_context *context,
	ia_css_psys_event_queue_ID_t id);

/*!Return the command message size of the indicated Psys command queue

 @param	context[in]	Psys syscom object

 Note: The message size is expressed in uint8_t

 @return 0 on error
 */
extern size_t ia_css_psys_get_cmd_msg_size(
	const struct ia_css_syscom_context *context,
	ia_css_psys_cmd_queue_ID_t id);

/*!Return the event message size of the indicated Psys event queue

 @param	context[in]	Psys syscom object

 Note: The message size is expressed in uint8_t

 @return 0 on error
 */
extern size_t ia_css_psys_get_event_msg_size(
	const struct ia_css_syscom_context *context,
	ia_css_psys_event_queue_ID_t id);

#endif /* __IA_CSS_PSYS_DEVICE_H */
