// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
/*
 * Copyright (C) 2018 Intel Corporation
 */

#ifndef __IPU4_VIRTIO_FE_PAYLOAD__
#define __IPU4_VIRTIO_FE_PAYLOAD__

#include "intel-ipu4-virtio-common.h"

void intel_ipu4_virtio_create_req(struct ipu4_virtio_req *req,
			     enum intel_ipu4_virtio_command cmd, int *op);

#endif