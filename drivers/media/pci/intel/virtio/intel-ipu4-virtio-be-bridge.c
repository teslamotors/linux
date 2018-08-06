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

#include "intel-ipu4-virtio-be-bridge.h"
#include "./ici/ici-isys-frame-buf.h"
#include "intel-ipu4-virtio-be-pipeline.h"
#include "intel-ipu4-virtio-be-stream.h"

int intel_ipu4_virtio_msg_parse(int domid, struct ipu4_virtio_req *req)
{
	int ret = 0;
	if (!req) {
		pr_err("IPU mediator: request is NULL\n");
		return -EINVAL;
	}
	if ((req->cmd < IPU4_CMD_DEVICE_OPEN) ||
			(req->cmd >= IPU4_CMD_GET_N)) {
			pr_err("IPU mediator: invalid command\n");
			return -EINVAL;
	}
	switch (req->cmd) {
	case IPU4_CMD_POLL:
			/*
			 * Open video device node
			 * op0 - virtual device node number
			 * op1 - Actual device fd. By default set to 0
			 */
			pr_debug("POLL: virtual_dev_id:%d actual_fd:%d\n", req->op[0], req->op[1]);
			req->stat = IPU4_REQ_NEEDS_FOLLOW_UP;
			break;
	case IPU4_CMD_DEVICE_OPEN:
			/*
			 * Open video device node
			 * op0 - virtual device node number
			 * op1 - Actual device fd. By default set to 0
			 */
			pr_debug("DEVICE_OPEN: virtual_dev_id:%d actual_fd:%d\n", req->op[0], req->op[1]);
			ret = process_device_open(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_DEVICE_CLOSE:
			/*
			 * Close video device node
			 * op0 - virtual device node number
			 * op1 - Actual device fd. By default set to 0
			 */
			pr_debug("DEVICE_CLOSE: virtual_dev_id:%d actual_fd:%d\n", req->op[0], req->op[1]);
			ret = process_device_close(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_STREAM_ON:
			/* Start Stream
			 * op0 - virtual device node number
			 * op1 - Actual device fd. By default set to 0
			 */
			pr_debug("STREAM ON: virtual_dev_id:%d actual_fd:%d\n", req->op[0], req->op[1]);
			ret = process_stream_on(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_STREAM_OFF:
			/* Stop Stream
			 * op0 - virtual device node number
			 * op1 - Actual device fd. By default set to 0
			 */
			pr_debug("STREAM OFF: virtual_dev_id:%d actual_fd:%d\n", req->op[0], req->op[1]);
			ret = process_stream_off(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
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

			ret = process_get_buf(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_PUT_BUF:
			/* Set Format of a given video node
			 * op0 - virtual device node number
			 * op1 - Actual device fd. By default set to 0
			 * op2 - Memory Type 1: USER_PTR 2: DMA_PTR
			 */
			ret = process_put_buf(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_SET_FORMAT:
			ret = process_set_format(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_PIPELINE_OPEN:
			ret = process_pipeline_open(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_PIPELINE_CLOSE:
			ret = process_pipeline_close(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_ENUM_NODES:
			ret = process_enum_nodes(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_ENUM_LINKS:
			ret = process_enum_links(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_SETUP_PIPE:
			ret = process_setup_pipe(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_SET_FRAMEFMT:
			ret = process_set_framefmt(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_GET_FRAMEFMT:
			ret = process_get_framefmt(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_GET_SUPPORTED_FRAMEFMT:
			ret = process_get_supported_framefmt(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_SET_SELECTION:
			ret = process_pad_set_sel(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_GET_SELECTION:
			ret = process_pad_get_sel(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	default:
			return -EINVAL;
		}

	return ret;
}
