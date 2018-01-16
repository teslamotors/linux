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
 * Chris Torek <torek @ torek net>
 * Hao Li <hao.l.li@intel.com>
 *  Define virtqueue data structures and APIs for VBS framework.
 *  - VBS-K is a kernel-level virtio framework that can be used for
 *    virtio backend driver development for CWP hypervisor.
 *  - VBS-K should be working with VBS-U (Virtio Backend Service in
 *    User) together, in order to connect with virtio frontend driver.
 */

#ifndef _VQ_H_
#define _VQ_H_

#include <linux/uio.h>
#include <linux/vbs/vbs.h>
#include <linux/vhm/vhm_vm_mngt.h>

/* virtqueue alignment */
#define VRING_ALIGN			4096
#define roundup2(x, y) (((x)+((y)-1))&(~((y)-1)))

/* PFN register shift amount */
#define VRING_PAGE_BITS			12

/* virtqueue flags */
#define	VQ_ALLOC			0x01
#define	VQ_BROKED			0x02

/*
 * Feature flags.
 * Note: bits 0 through 23 are reserved to each device type.
 */
#define VIRTIO_F_NOTIFY_ON_EMPTY	(1 << 24)
#define VIRTIO_RING_F_INDIRECT_DESC	(1 << 28)
#define VIRTIO_RING_F_EVENT_IDX		(1 << 29)

#define	VQ_MAX_DESCRIPTORS		512

/* virtio_desc flags */
#define VRING_DESC_F_NEXT		(1 << 0)
#define VRING_DESC_F_WRITE		(1 << 1)
#define VRING_DESC_F_INDIRECT		(1 << 2)

/* vring_avail flags */
#define VRING_AVAIL_F_NO_INTERRUPT	1

/* vring_used flags */
#define VRING_USED_F_NO_NOTIFY		1

/* Functions for dealing with generalized "virtual devices" */
#define VQ_USED_EVENT_IDX(vq) ((vq)->avail->ring[(vq)->qsize])

/* get virtqueue size according to virtio specification */
static inline size_t virtio_vq_ring_size(unsigned int qsz)
{
	size_t size;

	/* constant 3 below = va_flags, va_idx, va_used_event */
	size = sizeof(struct virtio_desc) * qsz + sizeof(uint16_t) * (3 + qsz);
	size = roundup2(size, VRING_ALIGN);

	/* constant 3 below = vu_flags, vu_idx, vu_avail_event */
	size += sizeof(uint16_t) * 3 + sizeof(struct virtio_used) * qsz;
	size = roundup2(size, VRING_ALIGN);

	return size;
}

/* Is this ring ready for I/O? */
static inline int virtio_vq_ring_ready(struct virtio_vq_info *vq)
{
	return (vq->flags & VQ_ALLOC);
}

/*
 * Are there "available" descriptors?  (This does not count
 * how many, just returns True if there are some).
 */
static inline int virtio_vq_has_descs(struct virtio_vq_info *vq)
{
	return (virtio_vq_ring_ready(vq) &&
			vq->last_avail != vq->avail->idx);
}

/* Deliver an interrupt to guest on the given virtual queue */
static inline void virtio_vq_interrupt(struct virtio_dev_info *dev,
				       struct virtio_vq_info *vq)
{
	uint16_t msix_idx;
	uint64_t msix_addr;
	uint32_t msix_data;

	/* Currently we only support MSIx */
	msix_idx = vq->msix_idx;

	if (msix_idx == VIRTIO_MSI_NO_VECTOR) {
		pr_err("msix idx is VIRTIO_MSI_NO_VECTOR!\n");
		return;
	}

	msix_addr = vq->msix_addr;
	msix_data = vq->msix_data;

	pr_debug("virtio_vq_interrupt: vmid is %d\n", dev->_ctx.vmid);
	vhm_inject_msi(dev->_ctx.vmid, msix_addr, msix_data);
}


/* virtqueue initialization APIs */
void virtio_vq_init(struct virtio_vq_info *vq, uint32_t pfn);
void virtio_vq_reset(struct virtio_vq_info *vq);

/* virtqueue runtime APIs */
int virtio_vq_getchain(struct virtio_vq_info *vq, uint16_t *pidx,
		       struct iovec *iov, int n_iov, uint16_t *flags);
void virtio_vq_retchain(struct virtio_vq_info *vq);
void virtio_vq_relchain(struct virtio_vq_info *vq, uint16_t idx,
			uint32_t iolen);
void virtio_vq_endchains(struct virtio_vq_info *vq, int used_all_avail);

#endif
