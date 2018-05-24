/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/*
 * Copyright (C) 2018 Intel Corporation
 */

#ifndef INTEL_IPU4_PARA_VIRT_H
#define INTEL_IPU4_PARA_VIRT_H

#include "./ici/ici-isys-stream-device.h"
#include "./ici/ici-isys-frame-buf.h"
#include "intel-ipu4-virtio-common.h"

struct virtual_stream {
	struct mutex mutex;
	struct ici_stream_device strm_dev;
	int virt_dev_id;
	struct ipu4_virtio_priv *priv;
	struct ici_isys_frame_buf_list buf_list;
};

#endif INTEL_IPU4_PARA_VIRT_H
