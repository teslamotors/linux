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
 *  Created Virtqueue APIs for CWP VBS framework:
 *  - VBS-K is a kernel-level virtio framework that can be used for
 *    virtio backend driver development for CWP hypervisor.
 *  - Virtqueue APIs abstract away the details of the internal data
 *    structures of virtqueue, so that callers could easily access
 *    the data from guest through virtqueues.
 */

#include <linux/module.h>
#include <linux/vbs/vq.h>
#include <linux/vbs/vbs.h>
#include <linux/vhm/cwp_vhm_mm.h>

/* helper function for remote memory map */
void * paddr_guest2host(struct ctx *ctx, uintptr_t gaddr, size_t len)
{
	return map_guest_phys(ctx->vmid, gaddr, len);
}

/*
 * Initialize the currently-selected virtqueue.
 * The guest just gave us a page frame number, from which we can
 * calculate the addresses of the queue.
 */
void virtio_vq_init(struct virtio_vq_info *vq, uint32_t pfn)
{
	uint64_t phys;
	size_t size;
	char *base;
	struct ctx *ctx;

	ctx = &vq->dev->_ctx;

	phys = (uint64_t)pfn << VRING_PAGE_BITS;
	size = virtio_vq_ring_size(vq->qsize);
	base = paddr_guest2host(ctx, phys, size);

	/* First page(s) are descriptors... */
	vq->desc = (struct virtio_desc *)base;
	base += vq->qsize * sizeof(struct virtio_desc);

	/* ... immediately followed by "avail" ring (entirely uint16_t's) */
	vq->avail = (struct vring_avail *)base;
	base += (2 + vq->qsize + 1) * sizeof(uint16_t);

	/* Then it's rounded up to the next page... */
	base = (char *)roundup2((uintptr_t)base, VRING_ALIGN);

	/* ... and the last page(s) are the used ring. */
	vq->used = (struct vring_used *)base;

	/* Mark queue as allocated, and start at 0 when we use it. */
	vq->flags = VQ_ALLOC;
	vq->last_avail = 0;
	vq->save_used = 0;
}

/* reset one virtqueue, make it invalid */
void virtio_vq_reset(struct virtio_vq_info *vq)
{
	if (!vq) {
		pr_info("%s: vq is NULL!\n", __func__);
		return;
	}

	vq->pfn = 0;
	vq->msix_idx = VIRTIO_MSI_NO_VECTOR;
	vq->flags = 0;
	vq->last_avail = 0;
	vq->save_used = 0;
}
