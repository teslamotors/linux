// SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
/*
 * Copyright(c) 2015 - 2018 Intel Corporation. All rights reserved.
 */
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/workqueue.h>

#include "cmd.h"
#include "spd.h"

#define spd_cmd_size(_cmd) \
	(sizeof(struct spd_cmd_hdr) + \
	 sizeof(struct spd_cmd_##_cmd))
#define spd_cmd_rpmb_size(_cmd) \
	(spd_cmd_size(_cmd) + SPD_CLIENT_RPMB_DATA_MAX_SIZE)

#define to_spd_hdr(_buf) (struct spd_cmd_hdr *)(_buf)
#define to_spd_cmd(_cmd, _buf) \
	(struct spd_cmd_##_cmd *)((_buf) + sizeof(struct spd_cmd_hdr))

const char *spd_cmd_str(enum spd_cmd_type cmd)
{
#define __SPD_CMD(_cmd) SPD_##_cmd##_CMD
#define SPD_CMD(cmd) case __SPD_CMD(cmd): return #cmd
	switch (cmd) {
	SPD_CMD(NONE);
	SPD_CMD(START_STOP);
	SPD_CMD(RPMB_WRITE);
	SPD_CMD(RPMB_READ);
	SPD_CMD(RPMB_GET_COUNTER);
	SPD_CMD(GPP_WRITE);
	SPD_CMD(GPP_READ);
	SPD_CMD(TRIM);
	SPD_CMD(INIT);
	SPD_CMD(STORAGE_STATUS);
	SPD_CMD(MAX);
	default:
		return "unknown";
	}
#undef SPD_CMD
#undef __SPD_CMD
}

const char *mei_spd_dev_str(enum spd_storage_type type)
{
#define SPD_TYPE(type) case SPD_TYPE_##type: return #type
	switch (type) {
	SPD_TYPE(UNDEF);
	SPD_TYPE(EMMC);
	SPD_TYPE(UFS);
	default:
		return "unknown";
	}
#undef SPD_TYPE
}

const char *mei_spd_state_str(enum mei_spd_state state)
{
#define SPD_STATE(state) case MEI_SPD_STATE_##state: return #state
	switch (state) {
	SPD_STATE(INIT);
	SPD_STATE(INIT_WAIT);
	SPD_STATE(INIT_DONE);
	SPD_STATE(RUNNING);
	SPD_STATE(STOPPING);
	default:
		return "unknown";
	}
#undef SPD_STATE
}

/**
 * mei_spd_init_req - send init request
 *
 * @spd: spd device
 *
 * Return: 0 on success
 *         -EPROTO if called in wrong state
 *         < 0 on write error
 */
int mei_spd_cmd_init_req(struct mei_spd *spd)
{
	const int req_len = sizeof(struct spd_cmd_hdr);
	struct spd_cmd_hdr *hdr;
	u32 cmd_type = SPD_INIT_CMD;
	ssize_t ret;

	spd_dbg(spd, "cmd [%d] %s : state [%d] %s\n",
		cmd_type, spd_cmd_str(cmd_type),
		spd->state, mei_spd_state_str(spd->state));

	if (spd->state != MEI_SPD_STATE_INIT)
		return -EPROTO;

	memset(spd->buf, 0, req_len);
	hdr = to_spd_hdr(spd->buf);

	hdr->command_type = cmd_type;
	hdr->is_response = 0;
	hdr->len = req_len;

	spd->state = MEI_SPD_STATE_INIT_WAIT;
	ret = mei_cldev_send(spd->cldev, spd->buf, req_len);
	if (ret != req_len) {
		spd_err(spd, "start send failed ret = %zd\n", ret);
		return ret;
	}

	return 0;
}

/**
 * mei_spd_cmd_init_rsp -  handle init response message
 *
 * @spd: spd device
 * @cmd: received spd command
 * @cmd_sz: received command size
 *
 * Return: 0 on success; < 0 otherwise
 */
static int mei_spd_cmd_init_rsp(struct mei_spd *spd, struct spd_cmd *cmd,
				ssize_t cmd_sz)
{
	int type;
	int gpp_id;
	int i;

	if (cmd_sz < spd_cmd_size(init_resp)) {
		spd_err(spd, "Wrong init response size\n");
		return -EINVAL;
	}

	if (spd->state != MEI_SPD_STATE_INIT_WAIT)
		return -EPROTO;

	type = cmd->init_rsp.type;
	gpp_id = cmd->init_rsp.gpp_partition_id;

	switch (type) {
	case SPD_TYPE_EMMC:
		if (gpp_id < 1 || gpp_id > 4) {
			spd_err(spd, "%s unsupported gpp id %d\n",
				mei_spd_dev_str(type), gpp_id);
			return -EINVAL;
		}
		break;

	case SPD_TYPE_UFS:
		if (gpp_id < 1 || gpp_id > 6) {
			spd_err(spd, "%s unsupported gpp id %d\n",
				mei_spd_dev_str(type), gpp_id);
			return -EINVAL;
		}
		break;

	default:
		spd_err(spd, "unsupported storage type %d\n",
			cmd->init_rsp.type);
		return -EINVAL;
	}

	spd->dev_type = type;
	spd->gpp_partition_id = gpp_id;

	if (cmd->init_rsp.serial_no_sz != 0) {
		if (cmd->init_rsp.serial_no_sz !=
		    cmd_sz - spd_cmd_size(init_resp)) {
			spd_err(spd, "wrong serial no size %u?=%zu\n",
				cmd->init_rsp.serial_no_sz,
				cmd_sz - spd_cmd_size(init_resp));
			return -EMSGSIZE;
		}

		if (cmd->init_rsp.serial_no_sz > 256) {
			spd_err(spd, "serial no is too large %u\n",
				cmd->init_rsp.serial_no_sz);
			return -EMSGSIZE;
		}

		spd->dev_id = kzalloc(cmd->init_rsp.serial_no_sz, GFP_KERNEL);
		if (!spd->dev_id)
			return -ENOMEM;

		spd->dev_id_sz = cmd->init_rsp.serial_no_sz;
		if (type == SPD_TYPE_EMMC) {
			/* FW have this in be32 format */
			__be32 *sno = (__be32 *)cmd->init_rsp.serial_no;
			u32 *dev_id = (u32 *)spd->dev_id;

			for (i = 0; i < spd->dev_id_sz / sizeof(u32); i++)
				dev_id[i] = be32_to_cpu(sno[i]);
		} else {
			memcpy(spd->dev_id, &cmd->init_rsp.serial_no,
			       cmd->init_rsp.serial_no_sz);
		}
	}

	spd->state = MEI_SPD_STATE_INIT_DONE;

	return 0;
}

/**
 * mei_spd_cmd_storage_status_req - send storage status message
 *
 * @spd: spd device
 *
 * Return: 0 on success
 *         -EPROTO if called in wrong state
 *         < 0 on write error
 */
int mei_spd_cmd_storage_status_req(struct mei_spd *spd)
{
	struct spd_cmd_hdr *hdr;
	struct spd_cmd_storage_status_req *req;
	const int req_len = spd_cmd_size(storage_status_req);
	u32 cmd_type = SPD_STORAGE_STATUS_CMD;
	ssize_t ret;

	spd_dbg(spd, "cmd [%d] %s : state [%d] %s\n",
		cmd_type, spd_cmd_str(cmd_type),
		spd->state, mei_spd_state_str(spd->state));

	if (spd->state < MEI_SPD_STATE_INIT_DONE)
		return -EPROTO;

	memset(spd->buf, 0, req_len);
	hdr = to_spd_hdr(spd->buf);

	hdr->command_type = cmd_type;
	hdr->is_response = 0;
	hdr->len = req_len;

	req = to_spd_cmd(storage_status_req, spd->buf);
	req->gpp_on = mei_spd_gpp_is_open(spd);
	req->rpmb_on = mei_spd_rpmb_is_open(spd);

	ret = mei_cldev_send(spd->cldev, spd->buf, req_len);
	if (ret != req_len) {
		spd_err(spd, "send storage status failed ret = %zd\n", ret);
		return ret;
	}

	if (req->gpp_on || req->rpmb_on)
		spd->state = MEI_SPD_STATE_RUNNING;
	else
		spd->state = MEI_SPD_STATE_INIT_DONE;

	spd_dbg(spd, "cmd [%d] %s : state [%d] %s\n",
		cmd_type, spd_cmd_str(cmd_type),
		spd->state, mei_spd_state_str(spd->state));

	return 0;
}

static int mei_spd_cmd_gpp_write(struct mei_spd *spd, struct spd_cmd *cmd,
				 ssize_t out_buf_sz)
{
	size_t len = SPD_GPP_WRITE_DATA_LEN(*cmd);
	int ret;

	if (out_buf_sz < spd_cmd_size(gpp_write_req)) {
		spd_err(spd, "Wrong request size\n");
		return SPD_STATUS_INVALID_COMMAND;
	}

	ret = mei_spd_gpp_write(spd, cmd->gpp_write_req.offset,
				cmd->gpp_write_req.data, len);
	if (ret) {
		spd_err(spd, "Failed to write to gpp ret = %d\n", ret);
		return SPD_STATUS_GENERAL_FAILURE;
	}

	spd_dbg(spd, "wrote %zd bytes of data\n", len);

	cmd->header.len = spd_cmd_size(gpp_write_rsp);

	return SPD_STATUS_SUCCESS;
}

static int mei_spd_cmd_gpp_read(struct mei_spd *spd, struct spd_cmd *cmd,
				ssize_t out_buf_sz)
{
	size_t len;
	int ret;

	if (out_buf_sz < spd_cmd_size(gpp_read_req)) {
		spd_err(spd, "Wrong request size\n");
		return SPD_STATUS_INVALID_COMMAND;
	}

	len = cmd->gpp_read_req.size_to_read;
	if (len > SPD_CLIENT_GPP_DATA_MAX_SIZE) {
		spd_err(spd, "Block is to large to read\n");
		return SPD_STATUS_INVALID_COMMAND;
	}

	ret = mei_spd_gpp_read(spd, cmd->gpp_read_req.offset,
			       cmd->gpp_read_resp.data, len);

	if (ret) {
		spd_err(spd, "Failed to read from gpp ret = %d\n", ret);
		return SPD_STATUS_GENERAL_FAILURE;
	}

	spd_dbg(spd, "read %zd bytes of data\n", len);

	cmd->header.len = spd_cmd_size(gpp_read_rsp) + len;

	return SPD_STATUS_SUCCESS;
}

static int mei_spd_cmd_rpmb_read(struct mei_spd *spd,
				 struct spd_cmd *cmd,
				 ssize_t out_buf_sz)
{
	u8 *frame = cmd->rpmb_read.rpmb_frame;

	if (out_buf_sz != spd_cmd_rpmb_size(rpmb_read)) {
		spd_err(spd, "Wrong request size\n");
		return SPD_STATUS_INVALID_COMMAND;
	}

	if (mei_spd_rpmb_cmd_req(spd, RPMB_READ_DATA, frame))
		return SPD_STATUS_GENERAL_FAILURE;

	spd_dbg(spd, "read RPMB frame performed\n");
	return SPD_STATUS_SUCCESS;
}

static int mei_spd_cmd_rpmb_write(struct mei_spd *spd,
				  struct spd_cmd *cmd,
				  ssize_t out_buf_sz)
{
	u8 *frame = cmd->rpmb_write.rpmb_frame;

	if (out_buf_sz != spd_cmd_rpmb_size(rpmb_write)) {
		spd_err(spd, "Wrong request size\n");
		return SPD_STATUS_INVALID_COMMAND;
	}

	if (mei_spd_rpmb_cmd_req(spd, RPMB_WRITE_DATA, frame))
		return SPD_STATUS_GENERAL_FAILURE;

	spd_dbg(spd, "write RPMB frame performed\n");
	return SPD_STATUS_SUCCESS;
}

static int mei_spd_cmd_rpmb_get_counter(struct mei_spd *spd,
					struct spd_cmd *cmd,
					ssize_t out_buf_sz)
{
	u8 *frame = cmd->rpmb_get_counter.rpmb_frame;

	if (out_buf_sz != spd_cmd_rpmb_size(rpmb_get_counter)) {
		spd_err(spd, "Wrong request size\n");
		return SPD_STATUS_INVALID_COMMAND;
	}

	if (mei_spd_rpmb_cmd_req(spd, RPMB_WRITE_DATA, frame))
		return SPD_STATUS_GENERAL_FAILURE;

	spd_dbg(spd, "get RPMB counter performed\n");
	return SPD_STATUS_SUCCESS;
}

static int mei_spd_cmd_response(struct mei_spd *spd, ssize_t out_buf_sz)
{
	struct spd_cmd *cmd = (struct spd_cmd *)spd->buf;
	u32 spd_cmd;
	int ret;

	spd_cmd = cmd->header.command_type;

	spd_dbg(spd, "rsp [%d] %s : state [%d] %s\n",
		spd_cmd, spd_cmd_str(spd_cmd),
		spd->state, mei_spd_state_str(spd->state));

	switch (spd_cmd) {
	case SPD_INIT_CMD:
		ret = mei_spd_cmd_init_rsp(spd, cmd, out_buf_sz);
		if (ret)
			break;
		mutex_unlock(&spd->lock);
		mei_spd_rpmb_init(spd);
		mei_spd_gpp_init(spd);
		mutex_lock(&spd->lock);
		break;
	default:
		ret = -EINVAL;
		spd_err(spd, "Wrong response command %d\n", spd_cmd);
		break;
	}

	return ret;
}

/**
 * mei_spd_cmd_request - dispatch command requests from the SPD device
 *
 * @spd: spd device
 * @out_buf_sz: buffer size
 *
 * Return: (TBD)
 */
static int mei_spd_cmd_request(struct mei_spd *spd, ssize_t out_buf_sz)
{
	struct spd_cmd *cmd = (struct spd_cmd *)spd->buf;
	ssize_t written;
	u32 spd_cmd;
	int ret;

	spd_cmd = cmd->header.command_type;

	spd_dbg(spd, "req [%d] %s : state [%d] %s\n",
		spd_cmd, spd_cmd_str(spd_cmd),
		spd->state, mei_spd_state_str(spd->state));

	if (spd->state < MEI_SPD_STATE_RUNNING) {
		spd_err(spd, "Wrong state %d\n", spd->state);
		ret = SPD_STATUS_INVALID_COMMAND;
		goto reply;
	}

	switch (spd_cmd) {
	case SPD_RPMB_WRITE_CMD:
		ret = mei_spd_cmd_rpmb_write(spd, cmd, out_buf_sz);
		break;
	case SPD_RPMB_READ_CMD:
		ret = mei_spd_cmd_rpmb_read(spd, cmd, out_buf_sz);
		break;
	case SPD_RPMB_GET_COUNTER_CMD:
		ret = mei_spd_cmd_rpmb_get_counter(spd, cmd, out_buf_sz);
		break;
	case SPD_GPP_WRITE_CMD:
		ret = mei_spd_cmd_gpp_write(spd, cmd, out_buf_sz);
		break;
	case SPD_GPP_READ_CMD:
		ret = mei_spd_cmd_gpp_read(spd, cmd, out_buf_sz);
		break;
	case SPD_TRIM_CMD:
		spd_err(spd, "Command %d is not supported\n", spd_cmd);
		ret = SPD_STATUS_NOT_SUPPORTED;
		break;
	default:
		spd_err(spd, "Wrong request command %d\n", spd_cmd);
		ret = SPD_STATUS_INVALID_COMMAND;
		break;
	}
reply:
	cmd->header.is_response = 1;
	cmd->header.status = ret;
	if (ret != SPD_STATUS_SUCCESS)
		cmd->header.len = sizeof(struct spd_cmd_hdr);

	written = mei_cldev_send(spd->cldev, spd->buf, cmd->header.len);
	if (written != cmd->header.len) {
		ret = SPD_STATUS_GENERAL_FAILURE;
		spd_err(spd, "Failed to send reply written = %zd\n", written);
	}

	/* FIXME: translate ret to errno */
	if (ret)
		return -EINVAL;

	return 0;
}

ssize_t mei_spd_cmd(struct mei_spd *spd)
{
	struct spd_cmd *cmd = (struct spd_cmd *)spd->buf;
	ssize_t out_buf_sz;
	int ret;

	out_buf_sz = mei_cldev_recv(spd->cldev, spd->buf, spd->buf_sz);
	if (out_buf_sz < 0) {
		spd_err(spd, "failure in receive ret = %zd\n", out_buf_sz);
		return out_buf_sz;
	}

	if (out_buf_sz == 0) {
		spd_err(spd, "received empty msg\n");
		return 0;
	}

	/* check that we've received at least sizeof(header) */
	if (out_buf_sz < sizeof(struct spd_cmd_hdr)) {
		spd_err(spd, "Request is too short\n");
		return -EFAULT;
	}

	if (cmd->header.is_response)
		ret = mei_spd_cmd_response(spd, out_buf_sz);
	else
		ret = mei_spd_cmd_request(spd, out_buf_sz);

	return ret;
}

static void mei_spd_status_send_work(struct work_struct *work)
{
	struct mei_spd *spd =
		container_of(work, struct mei_spd, status_send_w);

	mutex_lock(&spd->lock);
	mei_spd_cmd_storage_status_req(spd);
	mutex_unlock(&spd->lock);
}

void mei_spd_free(struct mei_spd *spd)
{
	if (!spd)
		return;

	cancel_work_sync(&spd->status_send_w);

	kfree(spd->buf);
	kfree(spd);
}

struct mei_spd *mei_spd_alloc(struct mei_cl_device *cldev)
{
	struct mei_spd *spd;
	u8 *buf;

	spd = kzalloc(sizeof(*spd), GFP_KERNEL);
	if (!spd)
		return NULL;

	spd->buf_sz = sizeof(struct spd_cmd) + SPD_CLIENT_GPP_DATA_MAX_SIZE;
	buf = kmalloc(spd->buf_sz, GFP_KERNEL);
	if (!buf)
		goto free;

	spd->cldev = cldev;
	spd->buf = buf;
	spd->state = MEI_SPD_STATE_INIT;
	mutex_init(&spd->lock);
	INIT_WORK(&spd->status_send_w, mei_spd_status_send_work);

	return spd;
free:
	kfree(spd);
	return NULL;
}
