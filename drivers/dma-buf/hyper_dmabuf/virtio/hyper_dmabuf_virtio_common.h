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

#ifndef __HYPER_DMABUF_VIRTIO_COMMON_H__
#define __HYPER_DMABUF_VIRTIO_COMMON_H__

/*
 * ACRN uses physicall addresses for memory sharing,
 * so size of one page ref will be 64-bits
 */

#define REFS_PER_PAGE (PAGE_SIZE/sizeof(u64))

/* Defines size of requests circular buffer */
#define REQ_RING_SIZE 128

extern struct hyper_dmabuf_bknd_ops virtio_bknd_ops;
struct virtio_be_priv;
struct vhm_request;

/* Entry describing each connected frontend */
struct virtio_fe_info {
	struct virtio_be_priv *priv;
	int client_id;
	int vmid;
	int max_vcpu;
	struct vhm_request *req_buf;
};

extern struct hyper_dmabuf_private hyper_dmabuf_private;

int hyper_dmabuf_virtio_get_next_req_id(void);

#endif /* __HYPER_DMABUF_VIRTIO_COMMON_H__*/
