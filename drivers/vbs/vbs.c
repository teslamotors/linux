/*
 * Clearwater Pass (CWP) Project
 * Virtio Backend Service (VBS) for CWP hypervisor
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright (c) 2017 Intel Corporation. All rights reserved.
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
 * Copyright (c) 2017 Intel Corporation. All rights reserved.
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
 *    virtio backend driver development for CWP hypervisor.
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
MODULE_DESCRIPTION("Virtio Backend Service framework for CWP hypervisor");
