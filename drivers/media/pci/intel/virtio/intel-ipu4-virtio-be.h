/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/*
 * Copyright (C) 2018 Intel Corporation
 */

#ifndef __IPU4_VIRTIO_BE__
#define __IPU4_VIRTIO_BE__

#include <linux/vbs/vbs.h>

int notify_fe(void);
int ipu_virtio_vqs_index_get(struct virtio_dev_info *dev, unsigned long *ioreqs_map,
						int *vqs_index, int max_vqs_index);

#endif
