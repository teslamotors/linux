/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/*
 * Copyright (C) 2018 Intel Corporation
 */

#ifndef __IPU4_VIRTIO_BE_BRIDGE__
#define __IPU4_VIRTIO_BE_BRIDGE__

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/vhm/acrn_vhm_mm.h>

#include "intel-ipu4-virtio-common.h"

int intel_ipu4_virtio_msg_parse(int domid, struct ipu4_virtio_req *req);

void intel_ipu4_virtio_create_req(struct ipu4_virtio_req *req,
			     enum intel_ipu4_virtio_command cmd, int *op);

int intel_ipu4_virtio_msg_parse(int domid, struct ipu4_virtio_req *req);


#endif


