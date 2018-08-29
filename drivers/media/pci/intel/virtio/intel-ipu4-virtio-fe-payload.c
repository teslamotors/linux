// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
/*
 * Copyright (C) 2018 Intel Corporation
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/syscalls.h>

#include "intel-ipu4-virtio-common.h"
#include "intel-ipu4-virtio-fe-payload.h"

void intel_ipu4_virtio_create_req(struct ipu4_virtio_req *req,
			     enum intel_ipu4_virtio_command cmd, int *op)
{
	int i;

	req->stat = IPU4_REQ_NOT_RESPONDED;
	req->cmd = cmd;

	switch (cmd) {
	case IPU4_CMD_POLL:
	case IPU4_CMD_DEVICE_OPEN:
	case IPU4_CMD_DEVICE_CLOSE:
	case IPU4_CMD_STREAM_ON:
	case IPU4_CMD_STREAM_OFF:
	case IPU4_CMD_PUT_BUF:
	case IPU4_CMD_SET_FORMAT:
	case IPU4_CMD_ENUM_NODES:
	case IPU4_CMD_ENUM_LINKS:
	case IPU4_CMD_SETUP_PIPE:
	case IPU4_CMD_SET_FRAMEFMT:
	case IPU4_CMD_GET_FRAMEFMT:
	case IPU4_CMD_GET_SUPPORTED_FRAMEFMT:
	case IPU4_CMD_SET_SELECTION:
	case IPU4_CMD_GET_SELECTION:
		/* Open video device node
		 * op0 - virtual device node number
		 * op1 - Actual device fd. By default set to 0
		 */
		for (i = 0; i < 2; i++)
			req->op[i] = op[i];
		break;
	case IPU4_CMD_GET_BUF:
		for (i = 0; i < 3; i++)
			req->op[i] = op[i];
		break;
	default:
		return;
	}
}
