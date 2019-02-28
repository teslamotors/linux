/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/*
 * Copyright (C) 2018 Intel Corporation
 */

#ifndef IPU4_VIRTIO_FE_REQUEST_QUEUE_H
#define IPU4_VIRTIO_FE_REQUEST_QUEUE_H

struct ipu4_virtio_vq_info {
	int vq_idx;
	int req_len;
	uint16_t vq_buf_idx;
};

struct ipu4_virtio_req_info {
	struct ipu4_virtio_req *request;
	struct ipu4_virtio_vq_info vq_info;
	int domid;
	int client_id;
};

int ipu4_virtio_be_req_queue_init(void);
void ipu4_virtio_be_req_queue_free(void);
struct ipu4_virtio_req_info *ipu4_virtio_be_req_queue_get(void);
int ipu4_virtio_be_req_queue_put(struct ipu4_virtio_req_info *req);

#endif
