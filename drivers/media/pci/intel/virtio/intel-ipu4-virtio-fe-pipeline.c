/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/*
 * Copyright (C) 2018 Intel Corporation
 */

#include <linux/ioctl.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include "intel-ipu4-virtio-fe-payload.h"
#include "intel-ipu4-virtio-fe-pipeline.h"

int process_pipeline(struct file *file, struct ipu4_virtio_ctx *fe_priv,
			void *data, int cmd)
{
	struct ipu4_virtio_req *req;
	int rval = 0;
	int op[10];

	op[0] = 0;
	op[1] = 0;

	req = kcalloc(1, sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	req->payload = virt_to_phys(data);

	intel_ipu4_virtio_create_req(req, cmd, &op[0]);

	rval = fe_priv->bknd_ops->send_req(fe_priv->domid, req, true, IPU_VIRTIO_QUEUE_0);
	if (rval) {
		pr_err("Failed to send request to BE\n");
		kfree(req);
		return rval;
	}

	kfree(req);

	return rval;
}

