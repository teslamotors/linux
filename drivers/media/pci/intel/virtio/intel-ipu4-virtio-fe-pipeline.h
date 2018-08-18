/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/*
 * Copyright (C) 2018 Intel Corporation
 */

#ifndef IPU4_VIRTIO_FE_PIPELINE_H
#define IPU4_VIRTIO_FE_PIPELINE_H

#include <media/ici.h>

#include "virtio/intel-ipu4-virtio-common.h"

int process_pipeline(struct file *file,
						struct ipu4_virtio_ctx *fe_priv,
						void *data,
						int cmd);


#endif
