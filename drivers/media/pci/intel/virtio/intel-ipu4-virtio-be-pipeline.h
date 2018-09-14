/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/*
 * Copyright (C) 2018 Intel Corporation
 */

#ifndef __IPU4_VIRTIO_BE_PIPELINE__
#define __IPU4_VIRTIO_BE_PIPELINE__

#include <linux/kernel.h>
#include <linux/errno.h>

#include "intel-ipu4-virtio-common.h"

int process_pipeline_open(int domid, struct ipu4_virtio_req *req);
int process_pipeline_close(int domid, struct ipu4_virtio_req *req);
int process_enum_nodes(int domid, struct ipu4_virtio_req *req);
int process_enum_links(int domid, struct ipu4_virtio_req *req);
int process_get_supported_framefmt(int domid, struct ipu4_virtio_req *req);
int process_set_framefmt(int domid, struct ipu4_virtio_req *req);
int process_get_framefmt(int domid, struct ipu4_virtio_req *req);
int process_pad_set_sel(int domid, struct ipu4_virtio_req *req);
int process_pad_get_sel(int domid, struct ipu4_virtio_req *req);
int process_setup_pipe(int domid, struct ipu4_virtio_req *req);

#endif


