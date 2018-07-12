/*
 * ACRN Project
 * Virtio Backend Service (VBS) for ACRN hypervisor
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright (c) 2018 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * Contact Information: Hao Li <hao.l.li@intel.com>
 *
 * BSD LICENSE
 *
 * Copyright (c) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Hao Li <hao.l.li@intel.com>
 *  Created Virtio Backend Service (VBS) framework:
 *  - VBS-K is a kernel-level virtio framework that can be used for
 *    virtio backend driver development for ACRN hypervisor.
 *  - VBS-K should be working with VBS-U (Virtio Backend Service in
 *    User) together, in order to connect with virtio frontend driver.
 *  - VBS-K mainly handles data plane part of a virtio backend driver,
 *    such as virtqueue parsing and processing, while VBS-U mainly
 *    hanldes control plane part.
 */

#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/vbs/vbs.h>
#include <linux/vbs/vq.h>

long virtio_dev_register(struct virtio_dev_info *dev)
{
	struct vm_info info;
	int ret;

	pr_debug("vmid is %d\n", dev->_ctx.vmid);

	if (dev->dev_notify == NULL) {
		pr_err("%s dev_notify empty!\n", dev->name);
		goto err;
	}

	/*
	 * dev->name is 32 chars while vhm only accepts 16 chars
	 * at most, so we make sure there will be a NULL
	 * terminator for the chars.
	 */
	dev->name[15] = '\0';
	dev->_ctx.vhm_client_id =
			acrn_ioreq_create_client(dev->_ctx.vmid,
						dev->dev_notify,
						dev->name);
	if (dev->_ctx.vhm_client_id < 0) {
		pr_err("failed to create client of ioreq!\n");
		goto err;
	}

	ret = acrn_ioreq_add_iorange(dev->_ctx.vhm_client_id,
				    dev->io_range_type ? REQ_MMIO : REQ_PORTIO,
				    dev->io_range_start,
				    dev->io_range_start + dev->io_range_len - 1);
	if (ret < 0) {
		pr_err("failed to add iorange to ioreq!\n");
		goto err;
	}

	/* feed up max_cpu and req_buf */
	ret = vhm_get_vm_info(dev->_ctx.vmid, &info);
	if (ret < 0) {
		pr_err("failed in vhm_get_vm_info!\n");
		goto range_err;
	}
	dev->_ctx.max_vcpu = info.max_vcpu;

	dev->_ctx.req_buf = acrn_ioreq_get_reqbuf(dev->_ctx.vhm_client_id);
	if (dev->_ctx.req_buf == NULL) {
		pr_err("failed in ioreq_get_reqbuf!\n");
		goto range_err;
	}

	acrn_ioreq_attach_client(dev->_ctx.vhm_client_id, 0);

	return 0;

range_err:
	acrn_ioreq_del_iorange(dev->_ctx.vhm_client_id,
			      dev->io_range_type ? REQ_MMIO : REQ_PORTIO,
			      dev->io_range_start,
			      dev->io_range_start + dev->io_range_len);

err:
	acrn_ioreq_destroy_client(dev->_ctx.vhm_client_id);

	return -EINVAL;
}

long virtio_dev_deregister(struct virtio_dev_info *dev)
{
	acrn_ioreq_del_iorange(dev->_ctx.vhm_client_id,
			      dev->io_range_type ? REQ_MMIO : REQ_PORTIO,
			      dev->io_range_start,
			      dev->io_range_start + dev->io_range_len);

	acrn_ioreq_destroy_client(dev->_ctx.vhm_client_id);

	return 0;
}

int virtio_vq_index_get(struct virtio_dev_info *dev, unsigned long *ioreqs_map)
{
	int val = -1;
	struct vhm_request *req;
	int vcpu;

	if (dev == NULL) {
		pr_err("%s: dev is NULL!\n", __func__);
		return -EINVAL;
	}

	while (1) {
		vcpu = find_first_bit(ioreqs_map, dev->_ctx.max_vcpu);
		if (vcpu == dev->_ctx.max_vcpu)
			break;
		req = &dev->_ctx.req_buf[vcpu];
		if (req->valid && req->processed == REQ_STATE_PROCESSING &&
		    req->client == dev->_ctx.vhm_client_id) {
			if (req->reqs.pio_request.direction == REQUEST_READ) {
				/* currently we handle kick only,
				 * so read will return 0
				 */
				pr_debug("%s: read request!\n", __func__);
				if (dev->io_range_type == PIO_RANGE)
					req->reqs.pio_request.value = 0;
				else
					req->reqs.mmio_request.value = 0;
			} else {
				pr_debug("%s: write request! type %d\n",
						__func__, req->type);
				if (dev->io_range_type == PIO_RANGE)
					val = req->reqs.pio_request.value;
				else
					val = req->reqs.mmio_request.value;
			}
			req->processed = REQ_STATE_SUCCESS;
			acrn_ioreq_complete_request(req->client, vcpu);
		}
	}

	return val;
}

static long virtio_vqs_info_set(struct virtio_dev_info *dev,
				struct vbs_vqs_info __user *i)
{
	struct vbs_vqs_info info;
	struct virtio_vq_info *vq;
	int j;

	vq = dev->vqs;

	if (copy_from_user(&info, i, sizeof(struct vbs_vqs_info)))
		return -EFAULT;

	/* setup struct virtio_vq_info based on info in struct vbs_vq_info */
	if (dev->nvq && dev->nvq != info.nvq) {
		pr_err("Oops! dev's nvq != vqs's nvq. Not the same device?\n");
		return -EFAULT;
	}

	for (j = 0; j < info.nvq; j++) {
		vq->qsize = info.vqs[j].qsize;
		vq->pfn = info.vqs[j].pfn;
		vq->msix_idx = info.vqs[j].msix_idx;
		vq->msix_addr = info.vqs[j].msix_addr;
		vq->msix_data = info.vqs[j].msix_data;

		pr_debug("msix id %x, addr %llx, data %x\n", vq->msix_idx,
			 vq->msix_addr, vq->msix_data);

		virtio_vq_init(vq, vq->pfn);

		vq++;
	}

	return 0;
}

/* invoked by VBS-K device's ioctl routine */
long virtio_vqs_ioctl(struct virtio_dev_info *dev, unsigned int ioctl,
		      void __user *argp)
{
	long ret;

	/*
	 * Currently we don't conduct ownership checking,
	 * but assuming caller would have device mutex.
	 */

	switch (ioctl) {
	case VBS_SET_VQ:
		ret = virtio_vqs_info_set(dev, argp);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(virtio_vqs_ioctl);

static long virtio_dev_info_set(struct virtio_dev_info *dev,
				struct vbs_dev_info __user *i)
{
	struct vbs_dev_info info;

	if (copy_from_user(&info, i, sizeof(struct vbs_dev_info)))
		return -EFAULT;

	/* setup struct virtio_dev_info based on info in vbs_dev_info */
	strncpy(dev->name, info.name, VBS_NAME_LEN);
	dev->_ctx.vmid = info.vmid;
	dev->nvq = info.nvq;
	dev->negotiated_features = info.negotiated_features;
	dev->io_range_start = info.pio_range_start;
	dev->io_range_len = info.pio_range_len;
	dev->io_range_type = PIO_RANGE;

	return 0;
}

/* invoked by VBS-K device's ioctl routine */
long virtio_dev_ioctl(struct virtio_dev_info *dev, unsigned int ioctl,
		      void __user *argp)
{
	long ret;

	/*
	 * Currently we don't conduct ownership checking,
	 * but assuming caller would have device mutex.
	 */

	switch (ioctl) {
	case VBS_SET_DEV:
		ret = virtio_dev_info_set(dev, argp);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(virtio_dev_ioctl);

/* called in VBS-K device's .open() */
long virtio_dev_init(struct virtio_dev_info *dev,
		     struct virtio_vq_info *vqs, int nvq)
{
	int i;

	for (i = 0; i < nvq; i++)
		virtio_vq_reset(&vqs[i]);

	return 0;
}
EXPORT_SYMBOL_GPL(virtio_dev_init);

static int __init vbs_init(void)
{
	return 0;
}

static void __exit vbs_exit(void)
{
}

module_init(vbs_init);
module_exit(vbs_exit);

MODULE_VERSION("0.1");
MODULE_AUTHOR("Intel Corporation");
MODULE_LICENSE("GPL and additional rights");
MODULE_DESCRIPTION("Virtio Backend Service framework for ACRN hypervisor");
