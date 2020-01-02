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
#define pr_fmt(fmt) KBUILD_MODNAME ":%s: " fmt, __func__

#include <linux/types.h>
#include <linux/mei_cl_bus.h>
#include <linux/completion.h>
#include "bh_errcode.h"
#include "bh_external.h"
#include "bh_internal.h"
#include "dal_dev.h"

/* BH initialization state */
static atomic_t bh_state = ATOMIC_INIT(0);
static u64 bh_host_id_number = MSG_SEQ_START_NUMBER;

/**
 * struct bh_request_cmd - bh request command
 *
 * @link: link in the request list of bh service
 * @cmd: command header and data
 * @cmd_len: command buffer length
 * @conn_idx: connection index
 * @host_id: session host id
 * @response: response buffer
 * @complete: request completion
 * @refcnt: reference counter
 * @ret: return value of the request
 */
struct bh_request_cmd {
	struct list_head link;
	u8 *cmd;
	unsigned int cmd_len;
	unsigned int conn_idx;
	u64 host_id;
	void *response;
	struct completion complete;
	struct kref refcnt;
	int ret;
};

struct bh_service {
	struct work_struct work;
	struct mutex request_lock; /* request lock */
	struct list_head request_list;
};

static struct bh_service bh_srvc;

/*
 * dal device session records list (array of list per dal device)
 * represents opened sessions to dal fw client
 */
static struct list_head dal_dev_session_list[MAX_CONNECTIONS];

/**
 * bh_get_msg_host_id - increase the shared variable bh_host_id_number by 1
 *                      and wrap around if needed
 *
 * Return: the updated host id number
 */
u64 bh_get_msg_host_id(void)
{
	bh_host_id_number++;
	/* wrap around. sequence_number must
	 * not be 0, as required by Firmware VM
	 */
	if (bh_host_id_number == 0)
		bh_host_id_number = MSG_SEQ_START_NUMBER;

	return bh_host_id_number;
}

/**
 * bh_session_find - find session record by handle
 *
 * @conn_idx: DAL client connection idx
 * @host_id: session host id
 *
 * Return: pointer to bh_session_record if found
 *         NULL if the session wasn't found
 */
struct bh_session_record *bh_session_find(unsigned int conn_idx, u64 host_id)
{
	struct bh_session_record *pos;
	struct list_head *session_list = &dal_dev_session_list[conn_idx];

	list_for_each_entry(pos, session_list, link) {
		if (pos->host_id == host_id)
			return pos;
	}

	return NULL;
}

/**
 * bh_session_add - add session record to list
 *
 * @conn_idx: fw client connection idx
 * @session: session record
 */
void bh_session_add(unsigned int conn_idx, struct bh_session_record *session)
{
	list_add_tail(&session->link, &dal_dev_session_list[conn_idx]);
}

/**
 * bh_session_remove - remove session record from list, ad release its memory
 *
 * @conn_idx: fw client connection idx
 * @host_id: session host id
 */
void bh_session_remove(unsigned int conn_idx, u64 host_id)
{
	struct bh_session_record *session;

	session = bh_session_find(conn_idx, host_id);

	if (session) {
		list_del(&session->link);
		kfree(session);
	}
}

static void bh_request_free(struct bh_request_cmd *request)
{
	if (!request)
		return;
	kfree(request->cmd);
	kfree(request->response);
	kfree(request);
	request = NULL;
}

static struct bh_request_cmd *bh_request_alloc(const void *hdr,
					       unsigned int hdr_len,
					       const void *data,
					       unsigned int data_len,
					       unsigned int conn_idx,
					       u64 host_id)
{
	struct bh_request_cmd *request;

	if (!hdr || hdr_len < sizeof(struct bh_command_header))
		return ERR_PTR(-EINVAL);

	if (!data && data_len)
		return ERR_PTR(-EINVAL);
	/* check for unsigned-integer overflow */
	if (hdr_len + data_len < data_len)
		return ERR_PTR(-EINVAL);

	request = kzalloc(sizeof(*request), GFP_KERNEL);
	if (!request)
		return ERR_PTR(-ENOMEM);

	request->cmd = kmalloc(hdr_len + data_len, GFP_KERNEL);
	if (!request->cmd) {
		kfree(request);
		return ERR_PTR(-ENOMEM);
	}

	memcpy(request->cmd, hdr, hdr_len);
	request->cmd_len = hdr_len;

	if (data_len) {
		memcpy(request->cmd + hdr_len, data, data_len);
		request->cmd_len += data_len;
	}

	request->conn_idx = conn_idx;
	request->host_id = host_id;

	kref_init(&request->refcnt);

	return request;
}

static char skip_buffer[DAL_MAX_BUFFER_SIZE] = {0};
/**
 * bh_transport_recv - receive message from DAL FW, using kdi callback
 *                     'dal_kdi_recv'
 *
 * @conn_idx: fw client connection idx
 * @buffer: output buffer to hold the received message
 * @size: output buffer size
 *
 * Return: 0 on success
 *         <0 on failure
 */
static int bh_transport_recv(unsigned int conn_idx, void *buffer, size_t size)
{
	size_t got;
	unsigned int count;
	char *buf = buffer;
	int ret;

	if (conn_idx > DAL_MEI_DEVICE_MAX)
		return -ENODEV;

	for (count = 0; count < size; count += got) {
		got = min_t(size_t, size - count, DAL_MAX_BUFFER_SIZE);
		if (buf)
			ret = dal_kdi_recv(conn_idx, buf + count, &got);
		else
			ret = dal_kdi_recv(conn_idx, skip_buffer, &got);

		if (!got)
			return -EFAULT;

		if (ret)
			return ret;
	}

	if (count != size)
		return -EFAULT;

	return 0;
}

/**
 * bh_recv_message_try - try to receive and prosses message from DAL
 *
 * @conn_idx: fw client connection idx
 * @response: output param to hold the response
 * @out_host_id: output param to hold the received message host id
 *               it should be identical to the sent message host id
 *
 * Return: 0 on success
 *         <0 on failure
 */
static int bh_recv_message_try(unsigned int conn_idx, void **response,
			       u64 *out_host_id)
{
	int ret;
	char *data;
	struct bh_response_header hdr;

	if (!response)
		return -EINVAL;

	*response = NULL;

	memset(&hdr, 0, sizeof(hdr));
	ret = bh_transport_recv(conn_idx, &hdr, sizeof(hdr));
	if (ret)
		return ret;

	if (hdr.h.length < sizeof(hdr))
		return -EBADMSG;

	/* check magic */
	if (hdr.h.magic != BH_MSG_RESP_MAGIC)
		return -EBADMSG;

	data = kzalloc(hdr.h.length, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	memcpy(data, &hdr, sizeof(hdr));

	/* message contains hdr only */
	if (hdr.h.length == sizeof(hdr))
		goto out;

	ret = bh_transport_recv(conn_idx, data + sizeof(hdr),
				hdr.h.length - sizeof(hdr));
out:
	if (out_host_id)
		*out_host_id = hdr.seq;

	*response = data;

	return ret;
}

#define MAX_RETRY_COUNT 3
static int bh_recv_message(struct bh_request_cmd *request)
{
	u32 retry;
	u64 res_host_id;
	void *resp;
	int ret;

	for (resp = NULL, retry = 0; retry < MAX_RETRY_COUNT; retry++) {
		kfree(resp);
		resp = NULL;

		ret = bh_recv_message_try(request->conn_idx,
					  &resp, &res_host_id);
		if (ret) {
			pr_debug("failed to recv msg = %d\n", ret);
			continue;
		}

		if (res_host_id != request->host_id) {
			pr_debug("recv message with host_id=%llu != sent host_id=%llu\n",
				 res_host_id, request->host_id);
			continue;
		}

		pr_debug("recv message with try=%d host_id=%llu\n",
			 retry, request->host_id);
		break;
	}

	if (retry == MAX_RETRY_COUNT) {
		pr_err("out of retry attempts\n");
		ret = -EFAULT;
	}

	if (ret) {
		kfree(resp);
		resp = NULL;
	}

	request->response = resp;
	return ret;
}

/**
 * bh_send_message - build and send command message to DAL
 *     using kdi callback 'dal_kdi_send'
 *
 * @request: all request details
 *
 * Return: 0 on success
 *         <0 on failure
 */
static int bh_send_message(const struct bh_request_cmd *request)
{
	int ret;
	size_t chunk_sz;
	unsigned int count;
	const char *buf;
	struct bh_command_header *h;

	if (!request)
		return -EINVAL;

	if (request->cmd_len < sizeof(*h) || !request->cmd)
		return -EINVAL;

	if (request->conn_idx > DAL_MEI_DEVICE_MAX)
		return -ENODEV;

	h = (struct bh_command_header *)request->cmd;
	h->h.magic = BH_MSG_CMD_MAGIC;
	h->h.length = request->cmd_len;
	h->seq = request->host_id;

	buf = request->cmd;
	for (count = 0; count < request->cmd_len; count += chunk_sz) {
		chunk_sz = min_t(size_t, request->cmd_len - count,
				 DAL_MAX_BUFFER_SIZE);
		ret = dal_kdi_send(request->conn_idx, buf + count, chunk_sz,
				   request->host_id);
		if (ret)
			return ret;
	}

	return 0;
}

void bh_prep_session_close_cmd(void *cmdbuf, u64 ta_session_id)
{
	struct bh_command_header *h = cmdbuf;
	struct bh_close_jta_session_cmd *cmd;

	cmd = (struct bh_close_jta_session_cmd *)h->cmd;
	h->id = BHP_CMD_CLOSE_JTASESSION;
	cmd->ta_session_id = ta_session_id;
}

static int bh_send_recv_message(struct bh_request_cmd *request)
{
	int ret;

	ret = bh_send_message(request);
	if (ret)
		return ret;

	return bh_recv_message(request);
}

static void bh_kref_release(struct kref *ref)
{
	struct bh_request_cmd *request =
		container_of(ref, struct bh_request_cmd, refcnt);

	bh_request_free(request);
}

/**
 * bh_kref_release_worker() - release bh_request from a background worker
 * @ref: reference counter of the bh_request object
 */
static void bh_kref_release_worker(struct kref *ref)
{
	struct bh_response_header *resp_hdr;
	struct bh_command_header *h;
	struct bh_request_cmd *request =
		container_of(ref, struct bh_request_cmd, refcnt);

	/* no one waits for the response - clean up is needed */
	pr_debug("no waiter - clean up is needed\n");

	if (!request->cmd_len || !request->cmd || !request->response)
		goto out;

	resp_hdr = (struct bh_response_header *)request->response;
	/*
	 * if the command was open_session and
	 * it was succeeded then close the session
	 */
	if (request->ret || resp_hdr->code)
		goto out;

	h = (struct bh_command_header *)request->cmd;
	if (bh_msg_is_cmd_open_session(h)) {
		char cmdbuf[CMD_BUF_SIZE(struct bh_close_jta_session_cmd)];
		struct bh_request_cmd *close_req;
		u64 host_id = request->host_id;

		bh_prep_session_close_cmd(cmdbuf, resp_hdr->ta_session_id);
		close_req = bh_request_alloc(cmdbuf, sizeof(cmdbuf), NULL, 0,
					     CONN_IDX_IVM, host_id);
		if (IS_ERR(close_req))
			goto out;

		bh_send_recv_message(close_req);
		bh_request_free(close_req);
	}
out:
	bh_request_free(request);
}

static void bh_request_work(struct work_struct *work)
{
	struct bh_service *bh_srv;
	struct bh_request_cmd *request, *next;
	int ret;

	bh_srv = container_of(work, struct bh_service, work);

	mutex_lock(&bh_srv->request_lock);
	list_for_each_entry_safe(request, next, &bh_srv->request_list, link) {
		list_del_init(&request->link);
		if (!request->cmd_len || !request->cmd)
			goto out_free;

		ret = bh_send_recv_message(request);
		request->ret = ret;

		complete(&request->complete);
out_free:
		kref_put(&request->refcnt, bh_kref_release_worker);
	}

	mutex_unlock(&bh_srv->request_lock);
}

/**
 * bh_request - send request to DAL FW and receive response back
 *
 * @conn_idx: fw client connection idx
 * @cmd_hdr: command header
 * @cmd_hdr_len: command header length
 * @cmd_data: command data (message content)
 * @cmd_data_len: data length
 * @host_id: message host id
 * @response: output param to hold the response
 *
 * Return: 0 on success
 *         <0 on failure
 */
int bh_request(unsigned int conn_idx, void *cmd_hdr, unsigned int cmd_hdr_len,
	       const void *cmd_data, unsigned int cmd_data_len,
	       u64 host_id, void **response)
{
	int ret;
	struct bh_request_cmd *request;

	mutex_lock(&bh_srvc.request_lock);
	request = bh_request_alloc(cmd_hdr, cmd_hdr_len, cmd_data, cmd_data_len,
				   conn_idx, host_id);
	if (IS_ERR(request)) {
		mutex_unlock(&bh_srvc.request_lock);
		return PTR_ERR(request);
	}

	/*
	 * Call kref_get before scheduling the worker thread,
	 * to avoid race condition
	 */

	init_completion(&request->complete);
	kref_get(&request->refcnt);

	list_add_tail(&request->link, &bh_srvc.request_list);
	mutex_unlock(&bh_srvc.request_lock);

	schedule_work(&bh_srvc.work);
	ret = wait_for_completion_interruptible(&request->complete);
	/*
	 * If wait was interrupted than decrease refcnt
	 * The worker thread will release the memory
	 */
	if (ret) {
		kref_put(&request->refcnt, bh_kref_release);
		return ret;
	}

	mutex_lock(&bh_srvc.request_lock);

	/* detach response buffer */
	*response = request->response;
	request->response = NULL;

	ret = request->ret;

	kref_put(&request->refcnt, bh_kref_release);

	mutex_unlock(&bh_srvc.request_lock);

	return ret;
}

/**
 * bh_session_list_free - free session list of given dal fw client
 *
 * @conn_idx: fw client connection idx
 */
static void bh_session_list_free(unsigned int conn_idx)
{
	struct bh_session_record *pos, *next;
	struct list_head *session_list =  &dal_dev_session_list[conn_idx];

	list_for_each_entry_safe(pos, next, session_list, link) {
		list_del(&pos->link);
		kfree(pos);
	}

	INIT_LIST_HEAD(session_list);
}

/**
 * bh_session_list_init - initialize session list of given dal fw client
 *
 * @conn_idx: fw client connection idx
 */
static void bh_session_list_init(unsigned int conn_idx)
{
	INIT_LIST_HEAD(&dal_dev_session_list[conn_idx]);
}

/**
 * bh_proxy_check_svl_jta_blocked_state - check if ta security version
 *    is blocked
 *
 * When installing a ta, a minimum security version is given,
 * so DAL will block installation of this ta from lower version.
 * (even after the ta will be uninstalled)
 *
 * @ta_id: trusted application (ta) id
 *
 * Return: 0 when ta security version isn't blocked
 *         <0 on system failure
 *         >0 on DAL FW failure
 */
int bh_proxy_check_svl_jta_blocked_state(uuid_t *ta_id)
{
	int ret;
	struct bh_command_header *h;
	struct bh_check_svl_jta_blocked_state_cmd *cmd;
	char cmdbuf[CMD_BUF_SIZE(*cmd)];
	struct bh_response_header *resp_hdr;
	u64 host_id;

	if (!ta_id)
		return -EINVAL;

	memset(cmdbuf, 0, sizeof(cmdbuf));
	resp_hdr = NULL;

	h = (struct bh_command_header *)cmdbuf;
	cmd = (struct bh_check_svl_jta_blocked_state_cmd *)h->cmd;
	h->id = BHP_CMD_CHECK_SVL_TA_BLOCKED_STATE;
	cmd->ta_id = *ta_id;

	host_id = bh_get_msg_host_id();
	ret = bh_request(CONN_IDX_SDM, h, CMD_BUF_SIZE(*cmd), NULL, 0,
			 host_id, (void **)&resp_hdr);

	if (!ret)
		ret = resp_hdr->code;

	kfree(resp_hdr);

	return ret;
}

/**
 * bh_proxy_list_jta_packages - get list of ta packages in DAL
 *
 * @conn_idx: fw client connection idx
 * @count: out param to hold the count of ta packages in DAL
 * @ta_ids: out param to hold pointer to the ids of ta packages in DAL
 *           The buffer which holds the ids is allocated in this function
 *           and freed by the caller
 *
 * Return: 0 when ta security version isn't blocked
 *         <0 on system failure
 *         >0 on DAL FW failure
 */
int bh_proxy_list_jta_packages(unsigned int conn_idx, unsigned int *count,
			       uuid_t **ta_ids)
{
	int ret;
	struct bh_command_header h;
	struct bh_response_header *resp_hdr;
	unsigned int resp_len;
	struct bh_resp_list_ta_packages *resp;
	uuid_t *outbuf;
	unsigned int i;
	u64 host_id;

	memset(&h, 0, sizeof(h));
	resp_hdr = NULL;

	if (!bh_is_initialized())
		return -EFAULT;

	if (!count || !ta_ids)
		return -EINVAL;

	*ta_ids = NULL;
	*count = 0;

	h.id = BHP_CMD_LIST_TA_PACKAGES;

	host_id = bh_get_msg_host_id();
	ret = bh_request(conn_idx, &h, sizeof(h), NULL, 0, host_id,
			 (void **)&resp_hdr);

	if (!ret)
		ret = resp_hdr->code;
	if (ret)
		goto out;

	resp_len = resp_hdr->h.length - sizeof(*resp_hdr);
	if (resp_len < sizeof(*resp)) {
		ret = -EBADMSG;
		goto out;
	}

	resp = (struct bh_resp_list_ta_packages *)resp_hdr->data;
	if (!resp->count) {
		/* return success, there are no ta packages loaded in DAL FW */
		ret = 0;
		goto out;
	}

	if (resp_len != sizeof(uuid_t) * resp->count + sizeof(*resp)) {
		ret = -EBADMSG;
		goto out;
	}

	outbuf = kcalloc(resp->count, sizeof(uuid_t), GFP_KERNEL);

	if (!outbuf) {
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < resp->count; i++)
		outbuf[i] = resp->ta_ids[i];

	*ta_ids = outbuf;
	*count = resp->count;

out:
	kfree(resp_hdr);
	return ret;
}

/**
 * bh_proxy_dnload_jta - download ta package to DAL
 *
 * @conn_idx: fw client connection idx
 * @ta_id: trusted application (ta) id
 * @ta_pkg: ta binary package
 * @pkg_len: ta binary package length
 *
 * Return: 0 on success
 *         <0 on system failure
 *         >0 on DAL FW failure
 */
int bh_proxy_dnload_jta(unsigned int conn_idx, uuid_t *ta_id,
			const char *ta_pkg, unsigned int pkg_len)
{
	struct bh_command_header *h;
	struct bh_download_jta_cmd *cmd;
	char cmdbuf[CMD_BUF_SIZE(*cmd)];
	struct bh_response_header *resp_hdr;
	u64 host_id;
	int ret;

	if (!ta_pkg || !pkg_len || !ta_id)
		return -EINVAL;

	memset(cmdbuf, 0, sizeof(cmdbuf));
	resp_hdr = NULL;


	h = (struct bh_command_header *)cmdbuf;
	cmd = (struct bh_download_jta_cmd *)h->cmd;
	h->id = BHP_CMD_DOWNLOAD_JAVATA;
	cmd->ta_id = *ta_id;

	host_id = bh_get_msg_host_id();
	ret = bh_request(conn_idx, h, CMD_BUF_SIZE(*cmd), ta_pkg, pkg_len,
			 host_id, (void **)&resp_hdr);

	if (!ret)
		ret = resp_hdr->code;

	kfree(resp_hdr);

	return ret;
}

/**
 * bh_proxy_open_jta_session - send open session command
 *
 * @conn_idx: fw client connection idx
 * @ta_id: trusted application (ta) id
 * @init_buffer: init parameters to the session (optional)
 * @init_len: length of the init parameters
 * @host_id: out param to hold the session host id
 * @ta_pkg: ta binary package
 * @pkg_len: ta binary package length
 *
 * Return: 0 on success
 *         <0 on system failure
 *         >0 on DAL FW failure
 */
int bh_proxy_open_jta_session(unsigned int conn_idx,
			      uuid_t *ta_id,
			      const char *init_buffer,
			      unsigned int init_len,
			      u64 *host_id,
			      const char *ta_pkg,
			      unsigned int pkg_len)
{
	int ret;
	struct bh_command_header *h;
	struct bh_open_jta_session_cmd *cmd;
	char cmdbuf[CMD_BUF_SIZE(*cmd)];
	struct bh_response_header *resp_hdr;
	struct bh_session_record *session;

	if (!host_id || !ta_id)
		return -EINVAL;

	if (!init_buffer && init_len > 0)
		return -EINVAL;

	memset(cmdbuf, 0, sizeof(cmdbuf));
	resp_hdr = NULL;

	h = (struct bh_command_header *)cmdbuf;
	cmd = (struct bh_open_jta_session_cmd *)h->cmd;

	session = kzalloc(sizeof(*session), GFP_KERNEL);
	if (!session)
		return -ENOMEM;

	session->host_id = bh_get_msg_host_id();
	bh_session_add(conn_idx, session);

	h->id = BHP_CMD_OPEN_JTASESSION;
	cmd->ta_id = *ta_id;

	ret = bh_request(conn_idx, h, CMD_BUF_SIZE(*cmd), init_buffer,
			 init_len, session->host_id, (void **)&resp_hdr);

	if (!ret && resp_hdr)
		ret = resp_hdr->code;

	if (ret == BHE_PACKAGE_NOT_FOUND) {
		/*
		 * VM might delete the TA pkg when no live session.
		 * Download the TA pkg and open session again
		 */
		ret = bh_proxy_dnload_jta(conn_idx, ta_id, ta_pkg, pkg_len);
		if (ret)
			goto out;

		kfree(resp_hdr);
		resp_hdr = NULL;
		ret = bh_request(conn_idx, h, CMD_BUF_SIZE(*cmd), init_buffer,
				 init_len, session->host_id,
				 (void **)&resp_hdr);

		if (!ret && resp_hdr)
			ret = resp_hdr->code;
	}

	if (resp_hdr)
		session->ta_session_id = resp_hdr->ta_session_id;
	*host_id = session->host_id;

out:
	if (ret)
		bh_session_remove(conn_idx, session->host_id);

	kfree(resp_hdr);

	return ret;
}

/**
 * bh_request_list_free - free request list of bh_service
 *
 * @request_list: request list
 */
static void bh_request_list_free(struct list_head *request_list)
{
	struct bh_request_cmd *pos, *next;

	list_for_each_entry_safe(pos, next, request_list, link) {
		list_del(&pos->link);
		bh_request_free(pos);
	}

	INIT_LIST_HEAD(request_list);
}

/**
 * bh_is_initialized - check if bh is initialized
 *
 * Return: true when bh is initialized and false otherwise
 */
bool bh_is_initialized(void)
{
	return atomic_read(&bh_state) == 1;
}

/**
 * bh_init_internal - BH initialization function
 *
 * The BH initialization creates the session lists for all
 * dal devices (dal fw clients)
 *
 * Return: 0
 */
void bh_init_internal(void)
{
	unsigned int i;

	if (!atomic_add_unless(&bh_state, 1, 1))
		return;

	for (i = CONN_IDX_START; i < MAX_CONNECTIONS; i++)
		bh_session_list_init(i);

	INIT_LIST_HEAD(&bh_srvc.request_list);
	mutex_init(&bh_srvc.request_lock);
	INIT_WORK(&bh_srvc.work, bh_request_work);
}

/**
 * bh_deinit_internal - BH  deinit function
 *
 * The deinitialization frees the session lists of all
 * dal devices (dal fw clients)
 */
void bh_deinit_internal(void)
{
	unsigned int i;

	if (!atomic_add_unless(&bh_state, -1, 0))
		return;

	for (i = CONN_IDX_START; i < MAX_CONNECTIONS; i++)
		bh_session_list_free(i);

	cancel_work_sync(&bh_srvc.work);
	bh_request_list_free(&bh_srvc.request_list);
}
