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


#include "ia_css_psys_device.h"
#include "ia_css_psys_device_trace.h"

#include <error_support.h>
#include <assert_support.h>
#include <print_support.h>
#include <misc_support.h>

#include "ia_css_psys_device_trace.h"

#include "ia_css_pkg_dir.h"
#include "ia_css_pkg_dir_iunit.h"
#include "ia_css_fw_load.h"
#include "ia_css_cell_program_load.h"
#include "ia_css_cell_program_group_load.h"
#include "ia_css_cell.h"
#include "regmem_access.h"
#include "ipu_device_cell_properties.h"
#include <vied/shared_memory_access.h>
#include "ipu_device_buttress_properties_struct.h"

#define IA_CSS_PSYS_CMD_QUEUE_SIZE				0x20
#define IA_CSS_PSYS_EVENT_QUEUE_SIZE			0x40

static struct ia_css_syscom_config	psys_syscom_config = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0
};

static bool	external_alloc = true;

struct ia_css_syscom_context	*psys_syscom = NULL;

static unsigned int ia_css_psys_cmd_queue_size[IA_CSS_N_PSYS_CMD_QUEUE_ID] = {
	IA_CSS_PSYS_CMD_QUEUE_SIZE,
	IA_CSS_PSYS_CMD_QUEUE_SIZE};
static unsigned int ia_css_psys_event_queue_size[IA_CSS_N_PSYS_EVENT_QUEUE_ID] = {
	IA_CSS_PSYS_EVENT_QUEUE_SIZE};

static unsigned int ia_css_psys_cmd_msg_size[IA_CSS_N_PSYS_CMD_QUEUE_ID] = {
	IA_CSS_PSYS_CMD_BITS/8,
	IA_CSS_PSYS_CMD_BITS/8};
static unsigned int ia_css_psys_event_msg_size[IA_CSS_N_PSYS_EVENT_QUEUE_ID] = {
	IA_CSS_PSYS_EVENT_BITS/8};

int ia_css_psys_config_print(
	const struct ia_css_syscom_config		*config,
	void									*fh)
{
	int	retval = -1;
	NOT_USED(fh);

	IA_CSS_TRACE_0(PSYSAPI_DEVICE, INFO, "ia_css_frame_print(): enter: \n");

	verifexit(config != NULL, EINVAL);

	retval = 0;
EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DEVICE, ERROR, "ia_css_frame_print failed (%i)\n", retval);
	}
	return retval;
}

int ia_css_psys_print(
	const struct ia_css_syscom_context		*context,
	void									*fh)
{
	int	retval = -1;
	NOT_USED(fh);

	IA_CSS_TRACE_0(PSYSAPI_DEVICE, INFO, "ia_css_psys_print(): enter: \n");

	verifexit(context != NULL, EINVAL);

	retval = 0;
EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_DEVICE, ERROR, "ia_css_psys_print failed (%i)\n", retval);
	}
	return retval;
}

struct ia_css_syscom_config *ia_css_psys_specify(void)
{
	struct ia_css_syscom_config	*config = &psys_syscom_config;

	IA_CSS_TRACE_0(PSYSAPI_DEVICE, INFO, "ia_css_psys_specify(): enter: \n");

	config->num_input_queues = IA_CSS_N_PSYS_CMD_QUEUE_ID;
	config->num_output_queues = IA_CSS_N_PSYS_EVENT_QUEUE_ID;
	config->input_queue_size = IA_CSS_PSYS_CMD_QUEUE_SIZE;
	config->output_queue_size = IA_CSS_PSYS_EVENT_QUEUE_SIZE;
	config->input_token_size = IA_CSS_PSYS_CMD_BITS/8;
	config->output_token_size = IA_CSS_PSYS_EVENT_BITS/8;

	return config;
}

size_t ia_css_sizeof_psys(
	struct ia_css_syscom_config				*config)
{
	size_t	size = 0;

	IA_CSS_TRACE_0(PSYSAPI_DEVICE, VERBOSE, "ia_css_sizeof_psys(): enter: \n");

	NOT_USED(config);

	return size;
}

/* This function will perform the 'intenal' initialization of SPC
 * icache base address and start address, using data from the pkg_dir.
 * The function will be removed when the drivers have switched to external
 * initialization, and all tests have been switched.
 */
static int
ia_css_psys_init_icache(unsigned int ssid, unsigned int mmid,
			host_virtual_address_t host_pkg_dir_address,
			vied_virtual_address_t vied_pkg_dir_address)
{
	ia_css_pkg_dir_entry_t pkg_dir[2];
	const ia_css_pkg_dir_entry_t *server_entry;
	unsigned int server_offset;

        /* Load pkg_dir header and entry 0 */
	ia_css_fw_load_init();
	ia_css_fw_load(mmid, host_pkg_dir_address, pkg_dir,
		2 * sizeof(ia_css_pkg_dir_entry_t));

	/* check pkg_dir header */
	if (ia_css_pkg_dir_verify_header(pkg_dir) != 0)
		return 1;

	/* Get PSYS server pkg address */
	server_entry = ia_css_pkg_dir_get_entry(pkg_dir, IA_CSS_PKG_DIR_PSYS_INDEX);
	/* check entry type */

	server_offset = ia_css_pkg_dir_entry_get_address_lo(server_entry);

	/* intialize SPC icache and start address */
	if (ia_css_cell_program_load_icache(ssid, mmid,
			host_pkg_dir_address + server_offset,
			vied_pkg_dir_address + server_offset) != 0)
		return 1;

	/* write pkg_dir address in SPC DMEM */
	regmem_store_32(ipu_device_cell_memory_address(SPC0, IPU_DEVICE_SP2600_CONTROL_DMEM),
		PKG_DIR_ADDR_REG, vied_pkg_dir_address, SSID);

	return 0;
}

struct ia_css_syscom_context *ia_css_psys_open(
	const struct ia_css_psys_buffer_s *buffer,
	struct ia_css_syscom_config *config)
{
	struct ia_css_syscom_context *context;
	ia_css_psys_server_init_t *server_conf;
	int i;

	IA_CSS_TRACE_0(PSYSAPI_DEVICE, INFO, "ia_css_psys_open(): enter:\n");

	if (config == NULL)
		goto EXIT;

	if (buffer == NULL) {
		/* Allocate locally */
		external_alloc = false;
	}

	/* conditionally initialize icache */
	server_conf = config->specific_addr;
	if (server_conf->host_ddr_pkg_dir) {
		if (ia_css_psys_init_icache(config->ssid, config->mmid,
			server_conf->host_ddr_pkg_dir, server_conf->ddr_pkg_dir_address)) {
			goto EXIT;
		}
	}

	/*
	 * Here we would like to pass separately the sub-system ID
	 * and optionally the user pointer to be mapped, depending on
	 * where this open is called, and which virtual memory handles
	 * we see here.
	 */
	/* context = ia_css_syscom_open(get_virtual_memory_handle(vied_psys_ID), buffer, config); */
	context = ia_css_syscom_open(config, NULL);
	if (context == NULL)
		goto EXIT;

	for (i=0; i < IA_CSS_N_PSYS_CMD_QUEUE_ID; i++) {
		verifexit(ia_css_syscom_send_port_open(context, (unsigned int)i) == 0, EINVAL);
	}

	for (i=0; i < IA_CSS_N_PSYS_EVENT_QUEUE_ID; i++) {
		verifexit(ia_css_syscom_recv_port_open(context, (unsigned int)i) == 0, EINVAL);
	}

	/* start SPC */
	ia_css_cell_start(config->ssid, SPC0);

	return context;

EXIT:
	IA_CSS_TRACE_0(PSYSAPI_DEVICE, ERROR, "ia_css_psys_open failed\n");
	return NULL;
}

struct ia_css_syscom_context* ia_css_psys_close(
	struct ia_css_syscom_context			*context)
{
	int 							i = 0;
	int								retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DEVICE, INFO, "ia_css_psys_close(): enter: \n");

	verifexit(context != NULL, EINVAL);

	for (i=0; i < IA_CSS_N_PSYS_CMD_QUEUE_ID; i++) {
		verifexit(ia_css_syscom_send_port_close(context, (unsigned int)i) == 0, EINVAL);
	}

	for (i=0; i < IA_CSS_N_PSYS_EVENT_QUEUE_ID; i++) {
		verifexit(ia_css_syscom_recv_port_close(context, (unsigned int)i) == 0, EINVAL);
	}

	verifexit((ia_css_syscom_close(context) == 0), EFAULT);
	context = NULL;

	retval = 0;

	if (external_alloc) {
		/* memset(); */
	} else {
		/* Free local allocations */
		/* Reset */
		external_alloc = true;
	}
EXIT:
	if ((context != NULL) || (retval !=0)) {
		IA_CSS_TRACE_0(PSYSAPI_DEVICE, ERROR, "ia_css_psys_close failed\n");
	}
	return context;
}

ia_css_psys_state_t ia_css_psys_check_state(
	struct ia_css_syscom_context			*context)
{
	IA_CSS_TRACE_0(PSYSAPI_DEVICE, VERBOSE, "ia_css_psys_check_state(): enter: \n");

	NOT_USED(context);

	/* For the time being, return the READY state to be used by SPC test */
	return IA_CSS_PSYS_STATE_READY;
}

bool ia_css_is_psys_cmd_queue_full(
	struct ia_css_syscom_context		*context,
	ia_css_psys_cmd_queue_ID_t		id)
{
	bool			is_full = false;
	int	num_tokens;
	int				retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DEVICE, VERBOSE, "ia_css_is_psys_cmd_queue_full(): enter: \n");
	verifexit(context != NULL, EINVAL);

	num_tokens = ia_css_syscom_send_port_available(context, (unsigned int)id);
	verifexit(num_tokens >= 0, EINVAL);

	is_full = (num_tokens == 0);
	retval = 0;
EXIT:
	if (retval != 0) {
		is_full = true;
		IA_CSS_TRACE_0(PSYSAPI_DEVICE, ERROR, "ia_css_is_psys_cmd_queue_full failed\n");
	}
	return is_full;
}

bool ia_css_is_psys_cmd_queue_not_full(
	struct ia_css_syscom_context		*context,
	ia_css_psys_cmd_queue_ID_t		id)
{
	bool			is_not_full = false;
	int	num_tokens;
	int				retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DEVICE, VERBOSE, "ia_css_is_psys_cmd_queue_not_full(): enter: \n");
	verifexit(context != NULL, EINVAL);

	num_tokens = ia_css_syscom_send_port_available(context, (unsigned int)id);
	verifexit(num_tokens >= 0, EINVAL);

	is_not_full = (num_tokens != 0);
	retval = 0;
EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_0(PSYSAPI_DEVICE, ERROR, "ia_css_is_psys_cmd_queue_not_full failed\n");
	}
	return is_not_full;
}

bool ia_css_has_psys_cmd_queue_N_space(
	struct ia_css_syscom_context		*context,
	ia_css_psys_cmd_queue_ID_t		id,
	const unsigned int						N)
{
	bool			has_N_space = false;
	int	num_tokens;
	int				retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DEVICE, VERBOSE, "ia_css_has_psys_cmd_queue_N_space(): enter: \n");
	verifexit(context != NULL, EINVAL);

	num_tokens = ia_css_syscom_send_port_available(context, (unsigned int)id);
	verifexit(num_tokens >= 0, EINVAL);

	has_N_space = ((unsigned int)num_tokens >= N);
EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_0(PSYSAPI_DEVICE, ERROR, "ia_css_has_psys_cmd_queue_N_space failed\n");
	}
	return has_N_space;
}

int ia_css_psys_cmd_queue_get_available_space(
	struct ia_css_syscom_context		*context,
	ia_css_psys_cmd_queue_ID_t		id)
{
	int				N_space = -1;
	int	num_tokens;

	IA_CSS_TRACE_0(PSYSAPI_DEVICE, VERBOSE, "ia_css_psys_cmd_queue_get_available_space(): enter: \n");
	verifexit(context != NULL, EINVAL);

	num_tokens = ia_css_syscom_send_port_available(context, (unsigned int)id);
	verifexit(num_tokens >= 0, EINVAL);

	N_space = (int)(num_tokens);
EXIT:
	if (N_space < 0) {
		IA_CSS_TRACE_0(PSYSAPI_DEVICE, ERROR, "ia_css_psys_cmd_queue_get_available_space failed\n");
	}
	return N_space;
}

bool ia_css_any_psys_event_queue_not_empty(
	struct ia_css_syscom_context		*context)
{
	ia_css_psys_event_queue_ID_t	i;
	bool	any_msg = false;

	IA_CSS_TRACE_0(PSYSAPI_DEVICE, VERBOSE, "ia_css_any_psys_event_queue_not_empty(): enter: \n");
	verifexit(context != NULL, EINVAL);

	for (i = (ia_css_psys_event_queue_ID_t)0; i < IA_CSS_N_PSYS_EVENT_QUEUE_ID; i++) {
		any_msg = any_msg || ia_css_is_psys_event_queue_not_empty(context, i);
	}

EXIT:
	return any_msg;
}

bool ia_css_is_psys_event_queue_empty(
	struct ia_css_syscom_context		*context,
	ia_css_psys_event_queue_ID_t		id)
{
	bool			is_empty = false;
	int	num_tokens;
	int				retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DEVICE, VERBOSE, "ia_css_is_psys_event_queue_empty(): enter: \n");
	verifexit(context != NULL, EINVAL);

	num_tokens = ia_css_syscom_recv_port_available(context, (unsigned int)id);
	verifexit(num_tokens >= 0, EINVAL);

	is_empty = (num_tokens == 0);
	retval = 0;
EXIT:
	if (retval != 0) {
		is_empty = true;
		IA_CSS_TRACE_0(PSYSAPI_DEVICE, ERROR, "ia_css_is_psys_event_queue_empty failed\n");
	}
	return is_empty;
}

bool ia_css_is_psys_event_queue_not_empty(
	struct ia_css_syscom_context		*context,
	ia_css_psys_event_queue_ID_t		id)
{
	bool			is_not_empty = false;
	int	num_tokens;
	int				retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DEVICE, VERBOSE, "ia_css_is_psys_event_queue_not_empty(): enter: \n");
	verifexit(context != NULL, EINVAL);

	num_tokens = ia_css_syscom_recv_port_available(context, (unsigned int)id);
	verifexit(num_tokens >= 0, EINVAL);

	is_not_empty = (num_tokens != 0);
	retval = 0;
EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_0(PSYSAPI_DEVICE, ERROR, "ia_css_is_psys_event_queue_not_empty failed\n");
	}
	return is_not_empty;
}

bool ia_css_has_psys_event_queue_N_msgs(
	struct ia_css_syscom_context		*context,
	ia_css_psys_event_queue_ID_t		id,
	const unsigned int						N)
{
	bool			has_N_msgs = false;
	int	num_tokens;
	int				retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DEVICE, VERBOSE, "ia_css_has_psys_event_queue_N_msgs(): enter: \n");
	verifexit(context != NULL, EINVAL);

	num_tokens = ia_css_syscom_recv_port_available(context, (unsigned int)id);
	verifexit(num_tokens >= 0, EINVAL);

	has_N_msgs = ((unsigned int)num_tokens >= N);
	retval = 0;
EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_0(PSYSAPI_DEVICE, ERROR, "ia_css_has_psys_event_queue_N_msgs failed\n");
	}
	return has_N_msgs;
}

int ia_css_psys_event_queue_get_available_msgs(
	struct ia_css_syscom_context		*context,
	ia_css_psys_event_queue_ID_t		id)
{
	int				N_msgs = -1;
	int	num_tokens;

	IA_CSS_TRACE_0(PSYSAPI_DEVICE, VERBOSE, "ia_css_psys_event_queue_get_available_msgs(): enter: \n");
	verifexit(context != NULL, EINVAL);

	num_tokens = ia_css_syscom_recv_port_available(context, (unsigned int)id);
	verifexit(num_tokens >= 0, EINVAL);

	N_msgs = (int)(num_tokens);
EXIT:
	if (N_msgs < 0) {
		IA_CSS_TRACE_0(PSYSAPI_DEVICE, ERROR, "ia_css_psys_event_queue_get_available_msgs failed\n");
	}
	return N_msgs;
}

int ia_css_psys_cmd_queue_send(
	struct ia_css_syscom_context			*context,
	ia_css_psys_cmd_queue_ID_t		id,
	const void								*cmd_msg_buffer)
{
	int	count = 0;

	IA_CSS_TRACE_0(PSYSAPI_DEVICE, VERBOSE, "ia_css_psys_cmd_queue_send(): enter: \n");
	verifexit(context != NULL, EINVAL);

	verifexit(context != NULL, EINVAL);
	/* The ~full check fails on receive queues */
	verifexit(ia_css_is_psys_cmd_queue_not_full(context, id), EBUSY);
	verifexit(cmd_msg_buffer != NULL, EINVAL);

	verifexit(ia_css_syscom_send_port_transfer(context, (unsigned int)id, cmd_msg_buffer) >= 0, EBUSY);

	count = 1;
EXIT:
	if (count == 0) {
		IA_CSS_TRACE_0(PSYSAPI_DEVICE, ERROR, "ia_css_psys_cmd_queue_send failed\n");
	}
	return count;
}

int ia_css_psys_cmd_queue_send_N(
	struct ia_css_syscom_context			*context,
	ia_css_psys_cmd_queue_ID_t		id,
	const void								*cmd_msg_buffer,
	const unsigned int						N)
{
	struct ia_css_psys_cmd_s	*cmd_msg_buffer_loc = (struct ia_css_psys_cmd_s *)cmd_msg_buffer;
	int	count = 0;

	IA_CSS_TRACE_0(PSYSAPI_DEVICE, VERBOSE, "ia_css_psys_cmd_queue_send_N(): enter: \n");
	verifexit(context != NULL, EINVAL);

	for (count = 0; count < (int)N; count++) {
		int	count_loc = ia_css_psys_cmd_queue_send(context, id , (void *)(&cmd_msg_buffer_loc[count]));
		verifexit(count_loc == 1, EINVAL);
	}

EXIT:
	if ((unsigned int) count < N) {
		IA_CSS_TRACE_0(PSYSAPI_DEVICE, ERROR, "ia_css_psys_cmd_queue_send_N failed\n");
	}
	return count;
}

int ia_css_psys_event_queue_receive(
	struct ia_css_syscom_context			*context,
	ia_css_psys_event_queue_ID_t			id,
	void									*event_msg_buffer)
{
	int	count = 0;

	IA_CSS_TRACE_0(PSYSAPI_DEVICE, VERBOSE, "ia_css_psys_event_queue_receive(): enter: \n");

	verifexit(context != NULL, EINVAL);
	/* The ~empty check fails on send queues */
	verifexit(ia_css_is_psys_event_queue_not_empty(context, id), EBUSY);
	verifexit(event_msg_buffer != NULL, EINVAL);

	verifexit(ia_css_syscom_recv_port_transfer(context, (unsigned int)id, event_msg_buffer) >= 0, EBUSY);

	count = 1;
EXIT:
	if (count == 0) {
		IA_CSS_TRACE_0(PSYSAPI_DEVICE, ERROR, "ia_css_psys_event_queue_receive failed\n");
	}
	return count;
}

int ia_css_psys_event_queue_receive_N(
	struct ia_css_syscom_context			*context,
	ia_css_psys_event_queue_ID_t		id,
	void									*event_msg_buffer,
	const unsigned int						N)
{
	struct ia_css_psys_event_s	*event_msg_buffer_loc;
	int	count;

	IA_CSS_TRACE_0(PSYSAPI_DEVICE, VERBOSE, "ia_css_psys_event_queue_receive_N(): enter: \n");

	event_msg_buffer_loc = (struct ia_css_psys_event_s *)event_msg_buffer;

	for (count = 0; count < (int)N; count++) {
		int	count_loc = ia_css_psys_event_queue_receive(context, id , (void *)(&event_msg_buffer_loc[count]));
		verifexit(count_loc == 1, EINVAL);
	}

EXIT:
	if ((unsigned int) count < N) {
		IA_CSS_TRACE_0(PSYSAPI_DEVICE, ERROR, "ia_css_psys_event_queue_receive_N failed\n");
	}
	return count;
}

size_t ia_css_psys_get_size(
	const struct ia_css_syscom_context		*context)
{
	size_t	size = 0;

	IA_CSS_TRACE_0(PSYSAPI_DEVICE, VERBOSE, "ia_css_psys_get_size(): enter: \n");

	verifexit (context != NULL, EINVAL);
	/* How can I query the context ? */
EXIT:
	if (size == 0) {
		IA_CSS_TRACE_0(PSYSAPI_DEVICE, ERROR, "ia_css_psys_get_size failed\n");
	}
	return size;
}

unsigned int ia_css_psys_get_cmd_queue_count(
	const struct ia_css_syscom_context		*context)
{
	unsigned int	count = 0;
	int 			retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DEVICE, VERBOSE, "ia_css_psys_get_cmd_queue_count(): enter: \n");

	verifexit (context != NULL, EINVAL);
	/* How can I query the context ? */
	NOT_USED(context);
	count = (unsigned int)IA_CSS_N_PSYS_CMD_QUEUE_ID;
	retval = 0;
EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_0(PSYSAPI_DEVICE, ERROR, "ia_css_psys_get_cmd_queue_count failed\n");
	}
	return count;
}

unsigned int ia_css_psys_get_event_queue_count(
	const struct ia_css_syscom_context		*context)
{
	unsigned int	count = 0;
	int				retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_DEVICE, VERBOSE, "ia_css_psys_get_event_queue_count(): enter: \n");

	verifexit (context != NULL, EINVAL);
	/* How can I query the context ? */
	NOT_USED(context);
	count = (unsigned int)IA_CSS_N_PSYS_EVENT_QUEUE_ID;
	retval = 0;
EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_0(PSYSAPI_DEVICE, ERROR, "ia_css_psys_get_event_queue_count failed\n");
	}
	return count;
}

size_t ia_css_psys_get_cmd_queue_size(
	const struct ia_css_syscom_context		*context,
	ia_css_psys_cmd_queue_ID_t		id)
{
	size_t	queue_size = 0;

	IA_CSS_TRACE_0(PSYSAPI_DEVICE, VERBOSE, "ia_css_psys_get_cmd_queue_size(): enter: \n");

	verifexit (context != NULL, EINVAL);
	/* How can I query the context ? */
	NOT_USED(context);
	NOT_USED(id);
	queue_size = ia_css_psys_cmd_queue_size[0];
EXIT:
	if (queue_size == 0) {
		IA_CSS_TRACE_0(PSYSAPI_DEVICE, ERROR, "ia_css_psys_get_cmd_queue_size failed\n");
	}
	return queue_size;
}

size_t ia_css_psys_get_event_queue_size(
	const struct ia_css_syscom_context		*context,
	ia_css_psys_event_queue_ID_t		id)
{
	size_t	queue_size = 0;

	IA_CSS_TRACE_0(PSYSAPI_DEVICE, VERBOSE, "ia_css_psys_get_event_queue_size(): enter: \n");

	verifexit (context != NULL, EINVAL);
	/* How can I query the context ? */
	NOT_USED(context);
	NOT_USED(id);
	queue_size = ia_css_psys_event_queue_size[0];
EXIT:
	if (queue_size == 0) {
		IA_CSS_TRACE_0(PSYSAPI_DEVICE, ERROR, "ia_css_psys_get_event_queue_size failed\n");
	}
	return queue_size;
}

size_t ia_css_psys_get_cmd_msg_size(
	const struct ia_css_syscom_context		*context,
	ia_css_psys_cmd_queue_ID_t		id)
{
	size_t	msg_size = 0;

	IA_CSS_TRACE_0(PSYSAPI_DEVICE, VERBOSE, "ia_css_psys_get_cmd_msg_size(): enter: \n");

	verifexit (context != NULL, EINVAL);
	/* How can I query the context ? */
	NOT_USED(context);
	NOT_USED(id);
	msg_size = ia_css_psys_cmd_msg_size[0];
EXIT:
	if (msg_size == 0) {
		IA_CSS_TRACE_0(PSYSAPI_DEVICE, ERROR, "ia_css_psys_get_cmd_msg_size failed\n");
	}
	return msg_size;
}

size_t ia_css_psys_get_event_msg_size(
	const struct ia_css_syscom_context		*context,
	ia_css_psys_event_queue_ID_t		id)
{
	size_t	msg_size = 0;

	IA_CSS_TRACE_0(PSYSAPI_DEVICE, VERBOSE, "ia_css_psys_get_event_msg_size(): enter: \n");

	verifexit (context != NULL, EINVAL);
	/* How can I query the context ? */
	NOT_USED(context);
	NOT_USED(id);
	msg_size = ia_css_psys_event_msg_size[0];
EXIT:
	if (msg_size == 0) {
		IA_CSS_TRACE_0(PSYSAPI_DEVICE, ERROR, "ia_css_psys_get_cmd_msg_size failed\n");
	}
	return msg_size;
}

