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

#ifndef __IA_CSS_PSYS_PROCESS_GROUP_HSYS_KERNEL_H
#define __IA_CSS_PSYS_PROCESS_GROUP_HSYS_KERNEL_H

/*! \file */

/** @file ia_css_psys_process_group.hsys.kernel.h
 *
 * Define the methods on the process group object: Hsys kernel interface
 */

#include <ia_css_psys_process_types.h>

#include <ia_css_psys_buffer_set.h>
#include <vied_nci_psys_system_global.h>

#include <type_support.h>					/* uint8_t */

/*
 * Registration of user contexts / callback info
 */

/*! Get the user (callback) token as registered in the process group

 @param	process_group[in]		process group object

 @return 0 on error
 */
extern uint64_t ia_css_process_group_get_token(
	ia_css_process_group_t					*process_group);

/*! Set (register) a user (callback) token in the process group

 @param	process_group[in]		process group object
 @param	token[in]				user token

 Note: The token value shall be non-zero. This token is
 returned in each return message related to the process
 group the token is registered with.

 @return < 0 on error
 */
extern int ia_css_process_group_set_token(
	ia_css_process_group_t					*process_group,
	const uint64_t							token);

/*
 * Passing of a (fragment) watermark
 */

/*! Get the fragment progress limit of the process group

 @param	process_group[in]		process group object

 @return 0 on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
uint16_t ia_css_process_group_get_fragment_limit(
	const ia_css_process_group_t				*process_group);

/*! Set the new fragment progress limit of the process group

 @param	process_group[in]		process group object
 @param	fragment_limit[in]		New limit value

 Note: The limit value must be less or equal to the fragment
 count value. The process group will not make progress beyond
 the limit value. The limit value can be modified asynchronously
 If the limit value is reached before an update happens, the
 process group will suspend and will not automatically resume.

 The limit is monotonically increasing. The default value is
 equal to the fragment count

 @return < 0 on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
int ia_css_process_group_set_fragment_limit(
	ia_css_process_group_t				*process_group,
	const uint16_t					fragment_limit);

/*! Clear the fragment progress limit of the process group

 @param	process_group[in]		process group object

 Note: This function sets the fragment limit to zero.

 @return < 0 on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
int ia_css_process_group_clear_fragment_limit(
	ia_css_process_group_t					*process_group);

/*
 * Commands
 */

/*! Perform the start command on the process group

 @param	process_group[in]		process group object

 Note: Start is an action of the l-Scheduler it makes the
 process group eligible for execution

 Precondition: The external resources that are attached to
 the process group must be in the correct state, i.e. input
 buffers are not-empty and output buffers not-full

 @return < 0 on error
 */
extern int ia_css_process_group_start(
	ia_css_process_group_t					*process_group);

/*! Perform the suspend command on the process group

 @param	process_group[in]		process group object

 Note: Suspend indicates that the process group execution
 is halted at the next fragment boundary. The process group
 will not automatically resume

 Precondition: The process group must be running

 @return < 0 on error
 */
extern int ia_css_process_group_suspend(
	ia_css_process_group_t					*process_group);

/*! Perform the resume command on the process group

 @param	process_group[in]		process group object

 Note: Resume indicates that the process group is again
 eligible for execution

 Precondition: The process group must be started

 @return < 0 on error
 */
extern int ia_css_process_group_resume(
	ia_css_process_group_t					*process_group);

/*! Perform the reset command on the process group

 @param	process_group[in]		process group object

 Note: Return the process group to the started state

 Precondition: The process group must be running or stopped

 @return < 0 on error
 */
extern int ia_css_process_group_reset(
	ia_css_process_group_t					*process_group);

/*! Perform the abort command on the process group

 @param	process_group[in]		process group object

 Note: Force the process group to the stopped state

 Precondition: The process group must be running or started

 @return < 0 on error
 */
extern int ia_css_process_group_abort(
	ia_css_process_group_t					*process_group);

/*! Release ownership of the process group

 @param	process_group[in]		process group object

 Note: Release notifies PSYS and hands over ownership of the
 process group from SW to FW

 Precondition: The process group must be in the started state

 @return < 0 on error
 */
extern int ia_css_process_group_disown(
	ia_css_process_group_t					*process_group);

/*
 * External resources
 */

/*! Set (register) a data buffer to the indexed terminal in the process group

 @param	process_group[in]		process group object
 @param	buffer[in]			buffer handle
 @param	buffer_state[in]		state of the buffer
 @param	terminal_index[in]		index of the terminal

 Note: The buffer handle shall not be VIED_NULL, the buffer
 state can be undefined; BUFFER_UNDEFINED

 Note: The buffer can be in memory or streaming over memory

 @return < 0 on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
int ia_css_process_group_attach_buffer(
	ia_css_process_group_t			*process_group,
	vied_vaddress_t				buffer,
	const ia_css_buffer_state_t		buffer_state,
	const unsigned int			terminal_index);

/*! Get (unregister) the data buffer on the indexed terminal of
 * the process group

 @param	process_group[in]		process group object
 @param	terminal_index[in]		index of the terminal

 Precondition: The process group must be stopped

 Postcondition: The buffer handle shall be reset to VIED_NULL, the buffer
 state to BUFFER_NULL

 @return VIED_NULL on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
vied_vaddress_t ia_css_process_group_detach_buffer(
	ia_css_process_group_t			*process_group,
	const unsigned int			terminal_index);

/*! Set (register) a data buffer to the indexed terminal in the process group

 @param	process_group[in]		process group object
 @param	stream[in]				stream handle
 @param	buffer_state[in]		state of the buffer
 @param	terminal_index[in]		index of the terminal

 Note: The stream handle shall not be zero, the buffer
 state can be undefined; BUFFER_UNDEFINED

 Note: The stream is used exclusive to a buffer; the latter can be in memory
 or streaming over memory

 @return < 0 on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
int ia_css_process_group_attach_stream(
	ia_css_process_group_t			*process_group,
	uint32_t				stream,
	const ia_css_buffer_state_t		buffer_state,
	const unsigned int			terminal_index);

/*! Get (unregister) the stream handle on the indexed terminal of
 * the process group

 @param	process_group[in]		process group object
 @param	terminal_index[in]		index of the terminal

 Precondition: The process group must be stopped

 Postcondition: The stream handle shall be reset to zero, the buffer
 state to BUFFER_NULL

 @return 0 on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
uint32_t ia_css_process_group_detach_stream(
	ia_css_process_group_t			*process_group,
	const unsigned int			terminal_index);

/*
 * Sequencing resources
 */

/*! Set a(n artificial) blocking resource (barrier) in
 * the process group resource map

 @param	process_group[in]		process group object
 @param	barrier_index[in]		index of the barrier

 Note: The barriers have to be set to force sequence between started
 process groups

 @return < 0 on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
int ia_css_process_group_set_barrier(
	ia_css_process_group_t					*process_group,
	const vied_nci_barrier_ID_t				barrier_index);

/*! Clear a previously set blocking resource (barrier) in
 * the process group resource map

 @param	process_group[in]		process group object
 @param	barrier_index[in]		index of the barrier

 Precondition: The barriers must have been set

 @return < 0 on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
int ia_css_process_group_clear_barrier(
	ia_css_process_group_t					*process_group,
	const vied_nci_barrier_ID_t				barrier_index);

/*! Boolean test if the process group preconditions for start are satisfied

 @param	process_group[in]		process group object

 @return true if the process group can be started
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
bool ia_css_can_process_group_start(
	const ia_css_process_group_t			*process_group);

#endif /* __IA_CSS_PSYS_PROCESS_GROUP_HSYS_KERNEL_H */
