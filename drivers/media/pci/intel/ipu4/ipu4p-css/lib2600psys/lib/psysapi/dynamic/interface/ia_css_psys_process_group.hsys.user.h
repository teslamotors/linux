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

#ifndef __IA_CSS_PSYS_PROCESS_GROUP_HSYS_USER_H
#define __IA_CSS_PSYS_PROCESS_GROUP_HSYS_USER_H

/*! \file */

/** @file ia_css_psys_process_group.hsys.user.h
 *
 * Define the methods on the process group object: Hsys user interface
 */

#include <ia_css_program_group_param.h>	/* ia_css_program_group_param_t */

#include <ia_css_psys_process_types.h>
#include <ia_css_psys_manifest_types.h>
#include <ia_css_psys_buffer_set.h>

#include "ia_css_psys_dynamic_storage_class.h"

#include <type_support.h>					/* uint8_t */

/*
 * Creation
 */

/*! Compute the size of storage required for allocating the process group object

 @param	manifest[in]			program group manifest
 @param	param[in]			program group parameters

 @return 0 on error
 */
extern size_t ia_css_sizeof_process_group(
	const ia_css_program_group_manifest_t	*manifest,
	const ia_css_program_group_param_t	*param);

/*! Create (the storage for) the process group object

 @param	process_grp_mem[in/out]	raw memory for process group
 @param	manifest[in]			program group manifest
 @param	param[in]			program group parameters

 @return NULL on error
 */
extern ia_css_process_group_t *ia_css_process_group_create(
	void					*process_grp_mem,
	const ia_css_program_group_manifest_t	*manifest,
	const ia_css_program_group_param_t	*param);

/*! Destroy (the storage of) the process group object

 @param	process_group[in]		process group object

 @return NULL
 */
extern ia_css_process_group_t *ia_css_process_group_destroy(
	ia_css_process_group_t					*process_group);

/*! Print the process group object to file/stream

 @param	process_group[in]		process group object
 @param	fid[out]				file/stream handle

 @return < 0 on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
int ia_css_process_group_print(
	const ia_css_process_group_t		*process_group,
	void					*fid);

/*
 * Commands
 */

/*! Perform the submit command on the process group

 @param	process_group[in]		process group object

 Note: Submit is an action of the h-Scheduler it makes the
 process group eligible for the l-Scheduler

 Precondition: The external resources must be attached to
 the process group

 @return < 0 on error
 */
extern int ia_css_process_group_submit(
	ia_css_process_group_t					*process_group);

/*! Boolean test if the process group object type is valid

 @param	process_group[in]		process group object
 @param	manifest[in]			program group manifest
 @param	param[in]				program group parameters

 @return true if the process group is correct, false on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
bool ia_css_is_process_group_valid(
	const ia_css_process_group_t		*process_group,
	const ia_css_program_group_manifest_t	*manifest,
	const ia_css_program_group_param_t	*param);

/*! Boolean test if the process group preconditions for submit are satisfied

 @param	process_group[in]		process group object

 @return true if the process group can be submitted
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
bool ia_css_can_process_group_submit(
	const ia_css_process_group_t			*process_group);

/*! Boolean test if the preconditions on process group and buffer set are
    satisfied for enqueuing buffer set

 @param	process_group[in]		process group object
 @param	buffer_set[in]			buffer set object

 @return true if the buffer set can be enqueued
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
bool ia_css_can_enqueue_buffer_set(
	const ia_css_process_group_t			*process_group,
	const ia_css_buffer_set_t			*buffer_set);

/*! Compute the cyclecount required for executing the process group object

 @param	manifest[in]			program group manifest
 @param	param[in]				program group parameters

 @return 0 on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
uint32_t ia_css_process_group_compute_cycle_count(
	const ia_css_program_group_manifest_t	*manifest,
	const ia_css_program_group_param_t	*param);

/*! Compute the number of processes required for
 * executing the process group object

 @param	manifest[in]			program group manifest
 @param	param[in]				program group parameters

 @return 0 on error
 */
extern uint8_t ia_css_process_group_compute_process_count(
	const ia_css_program_group_manifest_t	*manifest,
	const ia_css_program_group_param_t		*param);

/*! Compute the number of terminals required for
 * executing the process group object

 @param	manifest[in]			program group manifest
 @param	param[in]				program group parameters

 @return 0 on error
 */
extern uint8_t ia_css_process_group_compute_terminal_count(
	const ia_css_program_group_manifest_t	*manifest,
	const ia_css_program_group_param_t		*param);

/*! Get private token as registered in the process group by the implementation

 @param	process_group[in]		process group object

 @return 0 on error
 */
extern uint64_t ia_css_process_group_get_private_token(
	ia_css_process_group_t					*process_group);

/*! Set private token in the process group as needed by the implementation

 @param	process_group[in]		process group object
 @param	token[in]				user token

 Note: The token value shall be non-zero. This token is private
 to the implementation. This is in addition to the user token

 @return < 0 on error, 0 on success
 */
extern int ia_css_process_group_set_private_token(
	ia_css_process_group_t					*process_group,
	const uint64_t							token);

#endif /* __IA_CSS_PSYS_PROCESS_GROUP_HSYS_USER_H */
