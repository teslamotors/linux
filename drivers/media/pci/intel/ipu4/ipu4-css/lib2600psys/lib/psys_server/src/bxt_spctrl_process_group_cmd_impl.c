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

#include "ia_css_psys_device.h"
#include "ia_css_psys_process_group_cmd_impl.h"
#include "ia_css_psysapi.h"
#include "ia_css_psys_terminal.h"
#include "ia_css_psys_process.h"
#include "ia_css_psys_process.psys.h"
#include "ia_css_psys_process_group.h"
#include "ia_css_psys_process_group.psys.h"
#include "ia_css_psys_program_group_manifest.h"
#include "type_support.h"
#include "error_support.h"
#include "misc_support.h"
#include "cpu_mem_support.h"
#include "ia_css_bxt_spctrl_trace.h"

#if HAS_DUAL_CMD_CTX_SUPPORT
#define MAX_CLIENT_PGS 8 /* same as test_params.h */
struct ia_css_process_group_context {
	ia_css_process_group_t *pg;
	bool secure;
};
struct ia_css_process_group_context pg_contexts[MAX_CLIENT_PGS];
static unsigned int num_of_pgs;

STORAGE_CLASS_INLINE
struct ia_css_syscom_context *ia_css_process_group_get_context(ia_css_process_group_t *process_group)
{
	unsigned int i;
	bool secure = false;

	IA_CSS_TRACE_0(BXT_SPCTRL, INFO,
		"ia_css_process_group_get_context(): enter:\n");

	for (i = 0; i < num_of_pgs; i++) {
		if (pg_contexts[i].pg == process_group) {
			secure = pg_contexts[i].secure;
			break;
		}
	}

	IA_CSS_TRACE_1(BXT_SPCTRL, INFO,
		"ia_css_process_group_get_context(): secure %d\n", secure);
	return secure ? psys_syscom_secure : psys_syscom;
}

int ia_css_process_group_store(ia_css_process_group_t *process_group, bool secure)
{
	IA_CSS_TRACE_2(BXT_SPCTRL, INFO,
		"ia_css_process_group_store(): pg instance %d secure %d\n", num_of_pgs, secure);

	pg_contexts[num_of_pgs].pg     = process_group;
	pg_contexts[num_of_pgs].secure = secure;
	num_of_pgs++;
	return 0;
}
#else /* HAS_DUAL_CMD_CTX_SUPPORT */
STORAGE_CLASS_INLINE
struct ia_css_syscom_context *ia_css_process_group_get_context(ia_css_process_group_t *process_group)
{
	NOT_USED(process_group);

	return psys_syscom;
}

int ia_css_process_group_store(ia_css_process_group_t *process_group, bool secure)
{
	NOT_USED(process_group);
	NOT_USED(secure);

	return 0;
}
#endif /* HAS_DUAL_CMD_CTX_SUPPORT */

int ia_css_process_group_on_create(
	ia_css_process_group_t			*process_group,
	const ia_css_program_group_manifest_t	*program_group_manifest,
	const ia_css_program_group_param_t	*program_group_param)
{
	NOT_USED(process_group);
	NOT_USED(program_group_manifest);
	NOT_USED(program_group_param);

	IA_CSS_TRACE_0(BXT_SPCTRL, INFO,
		"ia_css_process_group_on_create(): enter:\n");

	return 0;
}

int ia_css_process_group_on_destroy(
	ia_css_process_group_t			*process_group)
{
	NOT_USED(process_group);

	IA_CSS_TRACE_0(BXT_SPCTRL, INFO,
		"ia_css_process_group_on_destroy(): enter:\n");

	return 0;
}

int ia_css_process_group_exec_cmd(
	ia_css_process_group_t			*process_group,
	const ia_css_process_group_cmd_t	cmd)
{
	int	retval = -1;
	ia_css_process_group_state_t	state;
	struct ia_css_psys_cmd_s	psys_cmd;
	bool	cmd_queue_full;
	unsigned int queue_id;

	IA_CSS_TRACE_0(BXT_SPCTRL, INFO,
		"ia_css_process_group_exec_cmd(): enter:\n");

	verifexit(process_group != NULL);

	state = ia_css_process_group_get_state(process_group);

	verifexit(state != IA_CSS_PROCESS_GROUP_ERROR);
	verifexit(state < IA_CSS_N_PROCESS_GROUP_STATES);

	switch (cmd) {
	case IA_CSS_PROCESS_GROUP_CMD_SUBMIT:

		IA_CSS_TRACE_0(BXT_SPCTRL, INFO,
			"ia_css_process_group_exec_cmd(): IA_CSS_PROCESS_GROUP_CMD_SUBMIT:\n");
		verifexit(state == IA_CSS_PROCESS_GROUP_READY);

		/* External resource availability checks */
		verifexit(ia_css_can_process_group_submit(process_group));

		process_group->state = IA_CSS_PROCESS_GROUP_BLOCKED;
		break;
	case IA_CSS_PROCESS_GROUP_CMD_START:

		IA_CSS_TRACE_0(BXT_SPCTRL, INFO,
			"ia_css_process_group_exec_cmd(): IA_CSS_PROCESS_GROUP_CMD_START:\n");
		verifexit(state == IA_CSS_PROCESS_GROUP_BLOCKED);

		/* External resource state checks */
		verifexit(ia_css_can_process_group_start(process_group));

		process_group->state = IA_CSS_PROCESS_GROUP_STARTED;
		break;
	case IA_CSS_PROCESS_GROUP_CMD_DISOWN:

		IA_CSS_TRACE_0(BXT_SPCTRL, INFO,
			"ia_css_process_group_exec_cmd(): IA_CSS_PROCESS_GROUP_CMD_DISOWN:\n");
		verifexit(state == IA_CSS_PROCESS_GROUP_STARTED);

		cmd_queue_full = ia_css_is_psys_cmd_queue_full(ia_css_process_group_get_context(process_group),
					IA_CSS_PSYS_CMD_QUEUE_COMMAND_ID);
		retval = EBUSY;
		verifexit(cmd_queue_full == false);

		psys_cmd.command = IA_CSS_PROCESS_GROUP_CMD_START;
		psys_cmd.msg = 0;
		psys_cmd.context_handle = process_group->ipu_virtual_address;

		verifexit(ia_css_process_group_print(process_group, NULL) == 0);

		retval = ia_css_psys_cmd_queue_send(ia_css_process_group_get_context(process_group),
				IA_CSS_PSYS_CMD_QUEUE_COMMAND_ID, &psys_cmd);
		verifexit(retval > 0);
		break;
	case IA_CSS_PROCESS_GROUP_CMD_STOP:

		IA_CSS_TRACE_0(BXT_SPCTRL, INFO,
			"ia_css_process_group_exec_cmd(): IA_CSS_PROCESS_GROUP_CMD_STOP:\n");

		cmd_queue_full = ia_css_is_psys_cmd_queue_full(ia_css_process_group_get_context(process_group),
					IA_CSS_PSYS_CMD_QUEUE_COMMAND_ID);
		retval = EBUSY;
		verifexit(cmd_queue_full == false);

		psys_cmd.command = IA_CSS_PROCESS_GROUP_CMD_STOP;
		psys_cmd.msg = 0;
		psys_cmd.context_handle = process_group->ipu_virtual_address;

		queue_id = ia_css_process_group_get_base_queue_id(process_group);
		verifexit(queue_id < IA_CSS_N_PSYS_CMD_QUEUE_ID);

		retval = ia_css_psys_cmd_queue_send(ia_css_process_group_get_context(process_group),
				queue_id, &psys_cmd);
		verifexit(retval > 0);
		break;
	case IA_CSS_PROCESS_GROUP_CMD_ABORT:

		IA_CSS_TRACE_0(BXT_SPCTRL, INFO,
			"ia_css_process_group_exec_cmd(): IA_CSS_PROCESS_GROUP_CMD_ABORT:\n");

		/* Once the flushing of shared buffers is fixed this verifexit
		 * should be changed to be state = IA_CSS_PROCESS_GROUP_STARTED
		 */
		verifexit(state == IA_CSS_PROCESS_GROUP_BLOCKED);

		cmd_queue_full = ia_css_is_psys_cmd_queue_full(ia_css_process_group_get_context(process_group),
					IA_CSS_PSYS_CMD_QUEUE_COMMAND_ID);
		retval = EBUSY;
		verifexit(cmd_queue_full == false);

		psys_cmd.command = IA_CSS_PROCESS_GROUP_CMD_ABORT;
		psys_cmd.msg = 0;
		psys_cmd.context_handle = process_group->ipu_virtual_address;

		retval = ia_css_psys_cmd_queue_send(ia_css_process_group_get_context(process_group),
			IA_CSS_PSYS_CMD_QUEUE_DEVICE_ID, &psys_cmd);
		verifexit(retval > 0);
		break;
	default:
		verifexit(false);
		break;
	}

	retval = 0;
EXIT:
	if (0 != retval) {
		IA_CSS_TRACE_1(BXT_SPCTRL, ERROR,
			"ia_css_process_group_exec_cmd failed (%i)\n", retval);
	}
	return retval;
}

STORAGE_CLASS_INLINE int enqueue_buffer_set_cmd(
	ia_css_process_group_t		*process_group,
	ia_css_buffer_set_t		*buffer_set,
	unsigned int			queue_offset,
	uint16_t			command
	)
{
	int retval = -1;
	struct ia_css_psys_cmd_s psys_cmd;
	bool cmd_queue_full;
	unsigned int queue_id;

	verifexit(ia_css_process_group_get_state(process_group)
		== IA_CSS_PROCESS_GROUP_STARTED);

	verifexit(queue_offset <
		ia_css_process_group_get_num_queues(process_group));

	queue_id =
		ia_css_process_group_get_base_queue_id(process_group) +
		queue_offset;
	verifexit(queue_id < IA_CSS_N_PSYS_CMD_QUEUE_ID);

	cmd_queue_full = ia_css_is_psys_cmd_queue_full(ia_css_process_group_get_context(process_group), queue_id);
	retval = EBUSY;
	verifexit(cmd_queue_full == false);

	psys_cmd.command = command;
	psys_cmd.msg = 0;
	psys_cmd.context_handle =
		ia_css_buffer_set_get_ipu_address(buffer_set);

	retval = ia_css_psys_cmd_queue_send(ia_css_process_group_get_context(process_group), queue_id, &psys_cmd);
	verifexit(retval > 0);

	retval = 0;

EXIT:
	if (0 != retval) {
		IA_CSS_TRACE_1(BXT_SPCTRL, ERROR,
			"enqueue_buffer_set failed (%i)\n", retval);
	}
	return retval;
}

int ia_css_enqueue_buffer_set(
	ia_css_process_group_t		*process_group,
	ia_css_buffer_set_t		*buffer_set,
	unsigned int			queue_offset)
{
	int retval = -1;

	IA_CSS_TRACE_0(BXT_SPCTRL, INFO,
			"ia_css_enqueue_buffer_set():\n");
	retval = enqueue_buffer_set_cmd(
			process_group,
			buffer_set,
			queue_offset,
			IA_CSS_PROCESS_GROUP_CMD_RUN);

	if (0 != retval) {
		IA_CSS_TRACE_1(BXT_SPCTRL, ERROR,
			"ia_css_enqueue_buffer_set failed (%i)\n", retval);
	}
	return retval;
}

int ia_css_enqueue_param_buffer_set(
	ia_css_process_group_t		*process_group,
	ia_css_buffer_set_t		*param_buffer_set)
{
#if (HAS_LATE_BINDING_SUPPORT == 1)
	int retval = -1;

	IA_CSS_TRACE_0(BXT_SPCTRL, INFO,
			"ia_css_enqueue_param_buffer_set():\n");

	retval = enqueue_buffer_set_cmd(
			process_group,
			param_buffer_set,
			IA_CSS_PSYS_LATE_BINDING_QUEUE_OFFSET,
			IA_CSS_PROCESS_GROUP_CMD_SUBMIT);

	if (0 != retval) {
		IA_CSS_TRACE_1(BXT_SPCTRL, ERROR,
			"ia_css_enqueue_param_buffer_set failed (%i)\n", retval);
	}
#else
	int retval = -1;

	NOT_USED(process_group);
	NOT_USED(param_buffer_set);
	IA_CSS_TRACE_0(BXT_SPCTRL, ERROR,
		"ia_css_enqueue_param_buffer_set failed, no late binding supported\n");
#endif /* (HAS_LATE_BINDING_SUPPORT == 1) */
	return retval;
}
