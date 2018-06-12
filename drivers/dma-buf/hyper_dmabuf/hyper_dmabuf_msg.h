/*
 * Copyright © 2017 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#ifndef __HYPER_DMABUF_MSG_H__
#define __HYPER_DMABUF_MSG_H__

#define MAX_NUMBER_OF_OPERANDS 64

struct hyper_dmabuf_req {
	unsigned int req_id;
	unsigned int stat;
	unsigned int cmd;
	unsigned int op[MAX_NUMBER_OF_OPERANDS];
};

struct hyper_dmabuf_resp {
	unsigned int resp_id;
	unsigned int stat;
	unsigned int cmd;
	unsigned int op[MAX_NUMBER_OF_OPERANDS];
};

enum hyper_dmabuf_command {
	HYPER_DMABUF_EXPORT = 0x10,
	HYPER_DMABUF_EXPORT_FD,
	HYPER_DMABUF_EXPORT_FD_FAILED,
	HYPER_DMABUF_NOTIFY_UNEXPORT,
	HYPER_DMABUF_OPS_TO_REMOTE,
	HYPER_DMABUF_OPS_TO_SOURCE,
};

enum hyper_dmabuf_ops {
	HYPER_DMABUF_OPS_ATTACH = 0x1000,
	HYPER_DMABUF_OPS_DETACH,
	HYPER_DMABUF_OPS_MAP,
	HYPER_DMABUF_OPS_UNMAP,
	HYPER_DMABUF_OPS_RELEASE,
	HYPER_DMABUF_OPS_BEGIN_CPU_ACCESS,
	HYPER_DMABUF_OPS_END_CPU_ACCESS,
	HYPER_DMABUF_OPS_KMAP_ATOMIC,
	HYPER_DMABUF_OPS_KUNMAP_ATOMIC,
	HYPER_DMABUF_OPS_KMAP,
	HYPER_DMABUF_OPS_KUNMAP,
	HYPER_DMABUF_OPS_MMAP,
	HYPER_DMABUF_OPS_VMAP,
	HYPER_DMABUF_OPS_VUNMAP,
};

enum hyper_dmabuf_req_feedback {
	HYPER_DMABUF_REQ_PROCESSED = 0x100,
	HYPER_DMABUF_REQ_NEEDS_FOLLOW_UP,
	HYPER_DMABUF_REQ_ERROR,
	HYPER_DMABUF_REQ_NOT_RESPONDED
};

/* create a request packet with given command and operands */
void hyper_dmabuf_create_req(struct hyper_dmabuf_req *req,
				 enum hyper_dmabuf_command command,
				 int *operands);

/* parse incoming request packet (or response) and take
 * appropriate actions for those
 */
int hyper_dmabuf_msg_parse(int domid, struct hyper_dmabuf_req *req);

#endif // __HYPER_DMABUF_MSG_H__
