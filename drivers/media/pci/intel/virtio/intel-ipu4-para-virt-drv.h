/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/*
 * Copyright (C) 2018 Intel Corporation
 */

#ifndef INTEL_IPU4_PARA_VIRT_H
#define INTEL_IPU4_PARA_VIRT_H

#include <linux/version.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/uaccess.h>

#include "./ici/ici-isys-stream-device.h"
#include "./ici/ici-isys-frame-buf.h"
#include "intel-ipu4-virtio-common.h"

#define MAX_STREAM_DEVICES 64
#define MAX_PIPELINE_DEVICES 1
#define MAX_ISYS_VIRT_STREAM 34

struct virtual_stream {
	struct mutex mutex;
	struct ici_stream_device strm_dev;
	int virt_dev_id;
	int actual_fd;
	struct ipu4_virtio_ctx *ctx;
	struct ici_isys_frame_buf_list buf_list;
};


#define dev_to_vstream(dev) \
	container_of(dev, struct virtual_stream, strm_dev)

#endif
