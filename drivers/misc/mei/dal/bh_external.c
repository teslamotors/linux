/******************************************************************************
 * Intel mei_dal Linux driver
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2016-2017 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * Contact Information:
 *	Intel Corporation.
 *	linux-mei@linux.intel.com
 *	http://www.intel.com
 *
 * BSD LICENSE
 *
 * Copyright(c) 2016-2017 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/

#include <linux/string.h>
#include <linux/uuid.h>
#include <linux/ctype.h>
#include <linux/dal.h>

#include "bh_errcode.h"
#include "bh_external.h"
#include "bh_internal.h"

/* BH initialization state */
static atomic_t bh_state = ATOMIC_INIT(0);

/**
 * uuid_is_valid_hyphenless - check if uuid is valid in hyphenless format
 *
 * @uuid_str: uuid string
 *
 * Return: true when uuid is valid in hyphenless format
 *         false when uuid is invalid
 */
static bool uuid_is_valid_hyphenless(const char *uuid_str)
{
	unsigned int i;

	/* exclude (i == 8 || i == 13 || i == 18 || i == 23) */
	for (i = 0; i < UUID_STRING_LEN - 4; i++)
		if (!isxdigit(uuid_str[i]))
			return false;

	return true;
}

/**
 * uuid_normalize_hyphenless - convert uuid from hyphenless format
 *                             to standard format
 *
 * @uuid_hl: uuid string in hyphenless format
 * @uuid_str: output param to hold uuid string in standard format
 */
static void uuid_normalize_hyphenless(const char *uuid_hl, char *uuid_str)
{
	unsigned int i;

	for (i = 0; i < UUID_STRING_LEN; i++) {
		if (i == 8 || i == 13 || i == 18 || i == 23)
			uuid_str[i] = '-';
		else
			uuid_str[i] = *uuid_hl++;
	}
	uuid_str[i] = '\0';
}

/**
 * dal_uuid_parse  - convert uuid string to binary form
 *
 * Input uuid is in either hyphenless or standard format
 *
 * @uuid_str: uuid string
 * @uuid: output param to hold uuid bin
 *
 * Return: 0 on success
 *         <0 on failure
 */
int dal_uuid_parse(const char *uuid_str, uuid_t *uuid)
{
	char __uuid_str[UUID_STRING_LEN + 1];

	if (!uuid_str || !uuid)
		return -EINVAL;

	if (uuid_is_valid_hyphenless(uuid_str)) {
		uuid_normalize_hyphenless(uuid_str, __uuid_str);
		uuid_str = __uuid_str;
	}

	return uuid_parse(uuid_str, uuid);
}
EXPORT_SYMBOL(dal_uuid_parse);

/**
 * bh_msg_is_response - check if message is response
 *
 * @msg: message
 * @len: message length
 *
 * Return: true when message is response
 *         false otherwise
 */
bool bh_msg_is_response(const void *msg, size_t len)
{
	const struct bh_response_header *r =  msg;

	return (len >= sizeof(*r) && r->h.magic == BH_MSG_RESP_MAGIC);
}

/**
 * bh_msg_is_cmd - check if message is command
 *
 * @msg: message
 * @len: message length
 *
 * Return: true when message is command
 *         false otherwise
 */
bool bh_msg_is_cmd(const void *msg, size_t len)
{
	const struct bh_command_header *c = msg;

	return (len >= sizeof(*c) && c->h.magic == BH_MSG_CMD_MAGIC);
}

/**
 * bh_msg_cmd_hdr - get the command header if message is command
 *
 * @msg: message
 * @len: message length
 *
 * Return: pointer to the command header when message is command
 *         NULL otherwise
 */
const struct bh_command_header *bh_msg_cmd_hdr(const void *msg, size_t len)
{
	if (!bh_msg_is_cmd(msg, len))
		return NULL;

	return msg;
}

/**
 * bh_msg_is_cmd_open_session - check if command is open session command
 *
 * @hdr: message header
 *
 * Return: true when command is open session command
 *         false otherwise
 */
bool bh_msg_is_cmd_open_session(const struct bh_command_header *hdr)
{
	return hdr->id == BHP_CMD_OPEN_JTASESSION;
}

/**
 * bh_open_session_ta_id - get ta id from open session command
 *
 * @hdr: message header
 * @count: message size
 *
 * Return: pointer to ta id when command is valid
 *         NULL otherwise
 */
const uuid_t *bh_open_session_ta_id(const struct bh_command_header *hdr,
				    size_t count)
{
	struct bh_open_jta_session_cmd *open_cmd;

	if (count < sizeof(*hdr) + sizeof(*open_cmd))
		return NULL;

	open_cmd = (struct bh_open_jta_session_cmd *)hdr->cmd;

	return &open_cmd->ta_id;
}

/**
 * bh_session_is_killed - check if session is killed
 *
 * @code: the session return code
 *
 * Return: true when the session is killed
 *         false otherwise
 */
static bool bh_session_is_killed(int code)
{
	return (code == BHE_WD_TIMEOUT ||
		code == BHE_UNCAUGHT_EXCEPTION ||
		code == BHE_APPLET_CRASHED);
}

/**
 * bh_ta_session_open - open session to ta
 *
 * This function will block until VM replied the response
 *
 * @host_id: out param to hold the session host_id
 * @ta_id: trusted application (ta) id
 * @ta_pkg: ta binary package
 * @pkg_len: ta binary package length
 * @init_param: init parameters to the session (optional)
 * @init_len: length of the init parameters
 *
 * Return: 0 on success
 *         <0 on system failure
 *         >0 on DAL FW failure
 */
int bh_ta_session_open(u64 *host_id, const char *ta_id,
		       const u8 *ta_pkg, size_t pkg_len,
		       const u8 *init_param, size_t init_len)
{
	int ret;
	uuid_t bin_ta_id;
	unsigned int conn_idx;
	unsigned int count;
	bool found;
	uuid_t *ta_ids = NULL;
	int i;

	if (!ta_id || !host_id)
		return -EINVAL;

	if (!ta_pkg || !pkg_len)
		return -EINVAL;

	if (!init_param && init_len != 0)
		return -EINVAL;

	if (dal_uuid_parse(ta_id, &bin_ta_id))
		return -EINVAL;

	*host_id = 0;

	ret = bh_proxy_check_svl_jta_blocked_state(&bin_ta_id);
	if (ret)
		return ret;

	/* 1: vm conn_idx is IVM dal FW client */
	conn_idx = CONN_IDX_IVM;

	/* 2.1: check whether the ta pkg existed in VM or not */
	count = 0;
	ret = bh_proxy_list_jta_packages(conn_idx, &count, &ta_ids);
	if (ret)
		return ret;

	found = false;
	for (i = 0; i < count; i++) {
		if (uuid_equal(&bin_ta_id, &ta_ids[i])) {
			found = true;
			break;
		}
	}
	kfree(ta_ids);

	/* 2.2: download ta pkg if not already present. */
	if (!found) {
		ret = bh_proxy_dnload_jta(conn_idx, &bin_ta_id,
					  ta_pkg, pkg_len);
		if (ret && ret != BHE_PACKAGE_EXIST)
			return ret;
	}

	/* 3: send open session command to VM */
	ret = bh_proxy_open_jta_session(conn_idx, &bin_ta_id,
					init_param, init_len,
					host_id, ta_pkg, pkg_len);
	return ret;
}

/**
 * bh_ta_session_command - send and receive data to/from ta
 *
 * This function will block until VM replied the response
 *
 * @host_id: session host id
 * @command_id: command id
 * @input: message to be sent
 * @length: sent message size
 * @output: output param to hold pointer to the buffer which
 *          will contain received message.
 *          This buffer is allocated by Beihai and freed by the user
 * @output_length: input and output param -
 *                 - input: the expected maximum length of the received message
 *                 - output: size of the received message
 * @response_code: output param to hold the return value from the applet
 *
 * Return: 0 on success
 *         <0 on system failure
 *         >0 on DAL FW failure
 */
int bh_ta_session_command(u64 host_id, int command_id,
			  const void *input, size_t length,
			  void **output, size_t *output_length,
			  int *response_code)
{
	int ret;
	struct bh_command_header *h;
	struct bh_cmd *cmd;
	char cmdbuf[CMD_BUF_SIZE(*cmd)];
	struct bh_response_header *resp_hdr;
	unsigned int resp_len;
	struct bh_session_record *session;
	struct bh_resp *resp;
	unsigned int conn_idx = CONN_IDX_IVM;
	unsigned int len;

	memset(cmdbuf, 0, sizeof(cmdbuf));
	resp_hdr = NULL;

	if (!bh_is_initialized())
		return -EFAULT;

	if (!input && length != 0)
		return -EINVAL;

	if (!output_length)
		return -EINVAL;

	if (output)
		*output = NULL;

	session = bh_session_find(conn_idx, host_id);
	if (!session)
		return -EINVAL;

	h = (struct bh_command_header *)cmdbuf;
	cmd = (struct bh_cmd *)h->cmd;
	h->id = BHP_CMD_SENDANDRECV;
	cmd->ta_session_id = session->ta_session_id;
	cmd->command = command_id;
	cmd->outlen = *output_length;

	ret = bh_request(conn_idx, h, CMD_BUF_SIZE(*cmd), input, length,
			 host_id, (void **)&resp_hdr);
	if (!resp_hdr)
		return ret ? ret : -EFAULT;

	if (!ret)
		ret = resp_hdr->code;

	session->ta_session_id = resp_hdr->ta_session_id;
	resp_len = resp_hdr->h.length - sizeof(*resp_hdr);

	if (ret == BHE_APPLET_SMALL_BUFFER &&
	    resp_len == sizeof(struct bh_resp_bof)) {
		struct bh_resp_bof *bof =
			(struct bh_resp_bof *)resp_hdr->data;

		if (response_code)
			*response_code = be32_to_cpu(bof->response);

		*output_length = be32_to_cpu(bof->request_length);
	}

	if (ret)
		goto out;

	if (resp_len < sizeof(struct bh_resp)) {
		ret = -EBADMSG;
		goto out;
	}

	resp = (struct bh_resp *)resp_hdr->data;

	if (response_code)
		*response_code = be32_to_cpu(resp->response);

	len = resp_len - sizeof(*resp);

	if (*output_length < len) {
		ret = -EMSGSIZE;
		goto out;
	}

	if (len && output) {
		*output = kmemdup(resp->buffer, len, GFP_KERNEL);
		if (!*output) {
			ret = -ENOMEM;
			goto out;
		}
	}

	*output_length = len;

out:
	if (bh_session_is_killed(resp_hdr->code))
		bh_session_remove(conn_idx, session->host_id);

	kfree(resp_hdr);

	return ret;
}

/**
 * bh_ta_session_close - close ta session
 *
 * This function will block until VM replied the response
 *
 * @host_id: session host id
 *
 * Return: 0 on success
 *         <0 on system failure
 *         >0 on DAL FW failure
 */
int bh_ta_session_close(u64 host_id)
{
	int ret;
	struct bh_command_header *h;
	struct bh_close_jta_session_cmd *cmd;
	char cmdbuf[CMD_BUF_SIZE(*cmd)];
	struct bh_response_header *resp_hdr;
	struct bh_session_record *session;
	unsigned int conn_idx = CONN_IDX_IVM;

	memset(cmdbuf, 0, sizeof(cmdbuf));
	resp_hdr = NULL;

	session = bh_session_find(conn_idx, host_id);
	if (!session)
		return -EINVAL;

	h = (struct bh_command_header *)cmdbuf;
	cmd = (struct bh_close_jta_session_cmd *)h->cmd;
	h->id = BHP_CMD_CLOSE_JTASESSION;
	cmd->ta_session_id = session->ta_session_id;

	ret = bh_request(conn_idx, h, CMD_BUF_SIZE(*cmd), NULL, 0, host_id,
			 (void **)&resp_hdr);

	if (!ret)
		ret = resp_hdr->code;

	kfree(resp_hdr);
	/*
	 * An internal session exists, so we should not close the session.
	 * It means that host app should call this API at appropriate time.
	 */
	if (ret != BHE_IAC_EXIST_INTERNAL_SESSION)
		bh_session_remove(conn_idx, host_id);

	return ret;
}

/**
 * bh_filter_hdr - filter the sent message
 *
 * Allow to send valid messages only.
 * The filtering is done using given filter functions table
 *
 * @hdr: message header
 * @count: message size
 * @ctx: context to send to the filter functions
 * @tbl: filter functions table
 *
 * Return: 0 when message is valid
 *         <0 on otherwise
 */
int bh_filter_hdr(const struct bh_command_header *hdr, size_t count, void *ctx,
		  const bh_filter_func tbl[])
{
	int i;
	int ret;

	for (i = 0; tbl[i]; i++) {
		ret = tbl[i](hdr, count, ctx);
		if (ret < 0)
			return ret;
	}
	return 0;
}

/**
 * bh_prep_access_denied_response - prepare package with 'access denied'
 *                                  response code.
 *
 * This function is used to send in band error to user who trying to send
 * message when he lacks the needed permissions
 *
 * @cmd: the invalid command message
 * @res: out param to hold the response header
 */
void bh_prep_access_denied_response(const char *cmd,
				    struct bh_response_header *res)
{
	struct bh_command_header *cmd_hdr = (struct bh_command_header *)cmd;

	res->h.magic = BH_MSG_RESP_MAGIC;
	res->h.length = sizeof(*res);
	res->code = BHE_OPERATION_NOT_PERMITTED;
	res->seq = cmd_hdr->seq;
}

/**
 * bh_is_initialized - check if bhp is initialized
 *
 * Return: true when bhp is initialized
 *         false when bhp is not initialized
 */
bool bh_is_initialized(void)
{
	return atomic_read(&bh_state) == 1;
}

/**
 * bh_init_internal - Beihai init function
 *
 * The plugin initialization includes initializing the session lists of all
 * dal devices (dal fw clients)
 *
 * Return: 0
 */
void bh_init_internal(void)
{
	unsigned int i;

	if (atomic_add_unless(&bh_state, 1, 1))
		for (i = CONN_IDX_START; i < MAX_CONNECTIONS; i++)
			bh_session_list_init(i);
}

/**
 * bh_deinit_internal - Beihai plugin deinit function
 *
 * The plugin deinitialization includes deinit the session lists of all
 * dal devices (dal fw clients)
 */
void bh_deinit_internal(void)
{
	unsigned int i;

	if (atomic_add_unless(&bh_state, -1, 0))
		for (i = CONN_IDX_START; i < MAX_CONNECTIONS; i++)
			bh_session_list_free(i);
}
