/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/*
 * Copyright (C) 2018 Intel Corporation
 */

#ifndef __IPU4_VIRTIO_BE_STREAM__
#define __IPU4_VIRTIO_BE_STREAM__

#include <linux/kernel.h>
#include <linux/errno.h>

#include "intel-ipu4-virtio-common.h"

int process_set_format(int domid, struct ipu4_virtio_req *req);
int process_device_open(int domid, struct ipu4_virtio_req *req);
int process_device_close(int domid, struct ipu4_virtio_req *req);
int process_poll(int domid, struct ipu4_virtio_req *req);
int process_put_buf(int domid, struct ipu4_virtio_req *req);
int process_stream_on(int domid, struct ipu4_virtio_req *req);
int process_stream_off(int domid, struct ipu4_virtio_req *req);
int process_get_buf(int domid, struct ipu4_virtio_req *req);


#endif


