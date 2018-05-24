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

#include "intel-ipu4-virtio-bridge.h"

int intel_ipu4_virtio_msg_parse(int domid, struct ipu4_virtio_req *req)
{
	int ret = 0;

	if (!req) {
			printk(KERN_ERR "request is NULL\n");
			return -EINVAL;
		}
	if ((req->cmd < IPU4_CMD_DEVICE_OPEN) ||
			(req->cmd >= IPU4_CMD_GET_N)) {
			printk(KERN_ERR "invalid command\n");
			return -EINVAL;
	}
	switch (req->cmd) {
	case IPU4_CMD_DEVICE_OPEN:
			/*
			 * Open video device node
			 * op0 - virtual device node number
			 * op1 - Actual device fd. By default set to 0
			 */
			printk(KERN_INFO "DEVICE_OPEN: virtual_dev_id:%d actual_fd:%d\n", req->op[0], req->op[1]);
			req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_DEVICE_CLOSE:
			/*
			 * Close video device node
			 * op0 - virtual device node number
			 * op1 - Actual device fd. By default set to 0
			 */
			break;
	case IPU4_CMD_STREAM_ON:
			/* Start Stream
			 * op0 - virtual device node number
			 * op1 - Actual device fd. By default set to 0
			 */
			break;
	case IPU4_CMD_STREAM_OFF:
			/* Stop Stream
			 * op0 - virtual device node number
			 * op1 - Actual device fd. By default set to 0
			 */
			break;
	case IPU4_CMD_GET_BUF:
			/* Set Format of a given video node
			 * op0 - virtual device node number
			 * op1 - Actual device fd. By default set to 0
			 * op2 - Memory Type 1: USER_PTR 2: DMA_PTR
			 * op3 - Number of planes
			 * op4 - Buffer ID
			 * op5 - Length of Buffer
			 */
			break;
	case IPU4_CMD_PUT_BUF:
			/* Set Format of a given video node
			 * op0 - virtual device node number
			 * op1 - Actual device fd. By default set to 0
			 * op2 - Memory Type 1: USER_PTR 2: DMA_PTR
			 */
			break;
	case IPU4_CMD_SET_FORMAT:
			/* Set Format of a given video node
			 * op0 - virtual device node number
			 * op1 - Actual device fd. By default set to 0
			 * op2 - Width
			 * op3 - Height
			 * op4 - Pixel Format
			 * op5 - Field
			 * op6 - Colorspace
			 * op7 - Size of Image
			 * op8 - Number of planes
			 * op9 - flags
			 */
			printk(KERN_INFO "VBS SET_FMT: virtual_dev_id:%d actual_fd:%d\n", req->op[2], req->op[3]);
			req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_ENUM_NODES:
			break;
	case IPU4_CMD_ENUM_LINKS:
			break;
	case IPU4_CMD_SETUP_PIPE:
			break;
	case IPU4_CMD_SET_FRAMEFMT:
			break;
	case IPU4_CMD_GET_FRAMEFMT:
			break;
	case IPU4_CMD_GET_SUPPORTED_FRAMEFMT:
			break;
	case IPU4_CMD_SET_SELECTION:
			break;
	case IPU4_CMD_GET_SELECTION:
			break;
	default:
			return -EINVAL;
		}

	return ret;
}

void intel_ipu4_virtio_create_req(struct ipu4_virtio_req *req,
			     enum intel_ipu4_virtio_command cmd, int *op)
{
	int i;

	req->stat = IPU4_REQ_NOT_RESPONDED;
	req->cmd = cmd;

	switch (cmd) {
	case IPU4_CMD_DEVICE_OPEN:
			/* Open video device node
			 * op0 - virtual device node number
			 * op1 - Actual device fd. By default set to 0
			 */
			for (i = 0; i < 2; i++)
				req->op[i] = op[i];
			break;
	case IPU4_CMD_DEVICE_CLOSE:
			/* Close video device node
			 * op0 - virtual device node number
			 * op1 - Actual device fd. By default set to 0
			 */
			break;
	case IPU4_CMD_STREAM_ON:
			/* Start Stream
			 * op0 - virtual device node number
			 * op1 - Actual device fd. By default set to 0
			 */
			break;
	case IPU4_CMD_STREAM_OFF:
			/* Stop Stream
			 * op0 - virtual device node number
			 * op1 - Actual device fd. By default set to 0
			 */
			break;
	case IPU4_CMD_GET_BUF:
			/* Set Format of a given video node
			 * op0 - virtual device node number
			 * op1 - Actual device fd. By default set to 0
			 * op2 - Memory Type 1: USER_PTR 2: DMA_PTR
			 * op3 - Number of planes
			 * op4 - Buffer ID
			 * op5 - Length of Buffer
			 */
			break;
	case IPU4_CMD_PUT_BUF:
			/* Set Format of a given video node
			 * op0 - virtual device node number
			 * op1 - Actual device fd. By default set to 0
			 * op2 - Memory Type 1: USER_PTR 2: DMA_PTR
			 */
			break;
	case IPU4_CMD_SET_FORMAT:
			/* Set Format of a given video node
			 * op0 - virtual device node number
			 * op1 - Actual device fd. By default set to 0
			 * op2 - Width
			 * op3 - Height
			 * op4 - Pixel Format
			 * op5 - Field
			 * op6 - Colorspace
			 * op7 - Size of Image
			 * op8 - Number of planes
			 * op9 - flags
			 */
			for (i = 0; i < 10; i++)
				req->op[i] = op[i];
			break;
	case IPU4_CMD_ENUM_NODES:
			break;
	case IPU4_CMD_ENUM_LINKS:
			break;
	case IPU4_CMD_SETUP_PIPE:
			break;
	case IPU4_CMD_SET_FRAMEFMT:
			break;
	case IPU4_CMD_GET_FRAMEFMT:
			break;
	case IPU4_CMD_GET_SUPPORTED_FRAMEFMT:
			break;
	case IPU4_CMD_SET_SELECTION:
			break;
	case IPU4_CMD_GET_SELECTION:
			break;
	default:
			return;
	}
}
