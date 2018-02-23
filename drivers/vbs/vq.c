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
 * Chris Torek <torek @ torek net>
 * Hao Li <hao.l.li@intel.com>
 *  Created Virtqueue APIs for ACRN VBS framework:
 *  - VBS-K is a kernel-level virtio framework that can be used for
 *    virtio backend driver development for ACRN hypervisor.
 *  - Virtqueue APIs abstract away the details of the internal data
 *    structures of virtqueue, so that callers could easily access
 *    the data from guest through virtqueues.
 */

#include <linux/module.h>
#include <linux/vbs/vq.h>
#include <linux/vbs/vbs.h>
#include <linux/vhm/acrn_vhm_mm.h>

/* helper function for remote memory map */
void * paddr_guest2host(struct ctx *ctx, uintptr_t gaddr, size_t len)
{
	return map_guest_phys(ctx->vmid, gaddr, len);
}

/*
 * helper function for vq_getchain():
 * record the i'th "real" descriptor.
 */
static inline void _vq_record(int i, volatile struct virtio_desc *vd,
			      struct ctx *ctx, struct iovec *iov,
			      int n_iov, uint16_t *flags)
{
	if (i >= n_iov)
		return;

	iov[i].iov_base = paddr_guest2host(ctx, vd->addr, vd->len);
	iov[i].iov_len = vd->len;

	if (flags != NULL)
		flags[i] = vd->flags;
}

/*
 * Walk descriptor table and put requests into iovec.
 *
 * Examine the chain of descriptors starting at the "next one" to
 * make sure that they describe a sensible request.  If so, return
 * the number of "real" descriptors that would be needed/used in
 * acting on this request.  This may be smaller than the number of
 * available descriptors, e.g., if there are two available but
 * they are two separate requests, this just returns 1.  Or, it
 * may be larger: if there are indirect descriptors involved,
 * there may only be one descriptor available but it may be an
 * indirect pointing to eight more.  We return 8 in this case,
 * i.e., we do not count the indirect descriptors, only the "real"
 * ones.
 *
 * Basically, this vets the vd_flags and vd_next field of each
 * descriptor and tells you how many are involved.  Since some may
 * be indirect, this also needs the vmctx (in the pci_vdev
 * at vc->vc_pi) so that it can find indirect descriptors.
 *
 * As we process each descriptor, we copy and adjust it (guest to
 * host address wise, also using the vmtctx) into the given iov[]
 * array (of the given size).  If the array overflows, we stop
 * placing values into the array but keep processing descriptors,
 * up to VQ_MAX_DESCRIPTORS, before giving up and returning -1.
 * So you, the caller, must not assume that iov[] is as big as the
 * return value (you can process the same thing twice to allocate
 * a larger iov array if needed, or supply a zero length to find
 * out how much space is needed).
 *
 * If you want to verify the WRITE flag on each descriptor, pass a
 * non-NULL "flags" pointer to an array of "uint16_t" of the same size
 * as n_iov and we'll copy each vd_flags field after unwinding any
 * indirects.
 *
 * If some descriptor(s) are invalid, this prints a diagnostic message
 * and returns -1.  If no descriptors are ready now it simply returns 0.
 *
 * You are assumed to have done a vq_ring_ready() if needed (note
 * that vq_has_descs() does one).
 */
int virtio_vq_getchain(struct virtio_vq_info *vq, uint16_t *pidx,
		       struct iovec *iov, int n_iov, uint16_t *flags)
{
	int i;
	unsigned int ndesc, n_indir;
	unsigned int idx, next;
	struct ctx *ctx;
	struct virtio_dev_info *dev;
	const char *name;

	volatile struct virtio_desc *vdir, *vindir, *vp;

	dev = vq->dev;
	name = dev->name;

	/*
	 * Note: it's the responsibility of the guest not to
	 * update vq->vq_avail->va_idx until all of the descriptors
	 * the guest has written are valid (including all their
	 * vd_next fields and vd_flags).
	 *
	 * Compute (last_avail - va_idx) in integers mod 2**16.  This is
	 * the number of descriptors the device has made available
	 * since the last time we updated vq->vq_last_avail.
	 *
	 * We just need to do the subtraction as an unsigned int,
	 * then trim off excess bits.
	 */
	idx = vq->last_avail;
	ndesc = (uint16_t)((unsigned int)vq->avail->idx - idx);

	if (ndesc == 0)
		return 0;

	if (ndesc > vq->qsize) {
		/* XXX need better way to diagnose issues */
		pr_err("%s: ndesc (%u) out of range, driver confused?\r\n",
		       name, (unsigned int)ndesc);
		return -1;
	}

	/*
	 * Now count/parse "involved" descriptors starting from
	 * the head of the chain.
	 *
	 * To prevent loops, we could be more complicated and
	 * check whether we're re-visiting a previously visited
	 * index, but we just abort if the count gets excessive.
	 */
	ctx = &dev->_ctx;
	*pidx = next = vq->avail->ring[idx & (vq->qsize - 1)];
	vq->last_avail++;
	for (i = 0; i < VQ_MAX_DESCRIPTORS; next = vdir->next) {
		if (next >= vq->qsize) {
			pr_err("%s: descriptor index %u out of range, "
			       "driver confused?\r\n", name, next);
			return -1;
		}
		vdir = &vq->desc[next];
		if ((vdir->flags & VRING_DESC_F_INDIRECT) == 0) {
			_vq_record(i, vdir, ctx, iov, n_iov, flags);
			i++;
		} else if ((dev->negotiated_features &
			    VIRTIO_RING_F_INDIRECT_DESC) == 0) {
			pr_err("%s: descriptor has forbidden INDIRECT flag, "
			       "driver confused?\r\n", name);
			return -1;
		} else {
			n_indir = vdir->len / 16;
			if ((vdir->len & 0xf) || n_indir == 0) {
				pr_err("%s: invalid indir len 0x%x, "
				       "driver confused?\r\n", name,
				       (unsigned int)vdir->len);
				return -1;
			}
			vindir = paddr_guest2host(ctx, vdir->addr, vdir->len);
			/*
			 * Indirects start at the 0th, then follow
			 * their own embedded "next"s until those run
			 * out.  Each one's indirect flag must be off
			 * (we don't really have to check, could just
			 * ignore errors...).
			 */
			next = 0;
			for (;;) {
				vp = &vindir[next];
				if (vp->flags & VRING_DESC_F_INDIRECT) {
					pr_err("%s: indirect desc has INDIR flag,"
					       " driver confused?\r\n", name);
					return -1;
				}
				_vq_record(i, vp, ctx, iov, n_iov, flags);
				if (++i > VQ_MAX_DESCRIPTORS)
					goto loopy;
				if ((vp->flags & VRING_DESC_F_NEXT) == 0)
					break;
				next = vp->next;
				if (next >= n_indir) {
					pr_err("%s: invalid next %u > %u, "
					       "driver confused?\r\n",
					       name, (unsigned int)next, n_indir);
					return -1;
				}
			}
		}
		if ((vdir->flags & VRING_DESC_F_NEXT) == 0)
			return i;
	}
loopy:
	pr_err("%s: descriptor loop? count > %d - driver confused?\r\n",
	       name, i);
	return -1;
}

/*
 * Return the currently-first request chain back to the available queue.
 *
 * (This chain is the one you handled when you called vq_getchain()
 * and used its positive return value.)
 */
void virtio_vq_retchain(struct virtio_vq_info *vq)
{
	vq->last_avail--;
}

/*
 * Return specified request chain to the guest, setting its I/O length
 * to the provided value.
 *
 * (This chain is the one you handled when you called vq_getchain()
 * and used its positive return value.)
 */
void virtio_vq_relchain(struct virtio_vq_info *vq, uint16_t idx,
			uint32_t iolen)
{
	uint16_t uidx, mask;
	volatile struct vring_used *vuh;
	volatile struct virtio_used *vue;

	/*
	 * Notes:
	 *  - mask is N-1 where N is a power of 2 so computes x % N
	 *  - vuh points to the "used" data shared with guest
	 *  - vue points to the "used" ring entry we want to update
	 *  - head is the same value we compute in vq_iovecs().
	 *
	 * (I apologize for the two fields named vu_idx; the
	 * virtio spec calls the one that vue points to, "id"...)
	 */
	mask = vq->qsize - 1;
	vuh = vq->used;

	uidx = vuh->idx;
	vue = &vuh->ring[uidx++ & mask];
	vue->idx = idx;
	vue->len = iolen;
	vuh->idx = uidx;
}

/*
 * Driver has finished processing "available" chains and calling
 * vq_relchain on each one.  If driver used all the available
 * chains, used_all should be set.
 *
 * If the "used" index moved we may need to inform the guest, i.e.,
 * deliver an interrupt.  Even if the used index did NOT move we
 * may need to deliver an interrupt, if the avail ring is empty and
 * we are supposed to interrupt on empty.
 *
 * Note that used_all_avail is provided by the caller because it's
 * a snapshot of the ring state when he decided to finish interrupt
 * processing -- it's possible that descriptors became available after
 * that point.  (It's also typically a constant 1/True as well.)
 */
void virtio_vq_endchains(struct virtio_vq_info *vq, int used_all_avail)
{
	struct virtio_dev_info *dev;
	uint16_t event_idx, new_idx, old_idx;
	int intr;

	/*
	 * Interrupt generation: if we're using EVENT_IDX,
	 * interrupt if we've crossed the event threshold.
	 * Otherwise interrupt is generated if we added "used" entries,
	 * but suppressed by VRING_AVAIL_F_NO_INTERRUPT.
	 *
	 * In any case, though, if NOTIFY_ON_EMPTY is set and the
	 * entire avail was processed, we need to interrupt always.
	 */
	dev = vq->dev;
	old_idx = vq->save_used;
	vq->save_used = new_idx = vq->used->idx;
	if (used_all_avail &&
	    (dev->negotiated_features & VIRTIO_F_NOTIFY_ON_EMPTY))
		intr = 1;
	else if (dev->negotiated_features & VIRTIO_RING_F_EVENT_IDX) {
		event_idx = VQ_USED_EVENT_IDX(vq);
		/*
		 * This calculation is per docs and the kernel
		 * (see src/sys/dev/virtio/virtio_ring.h).
		 */
		intr = (uint16_t)(new_idx - event_idx - 1) <
			(uint16_t)(new_idx - old_idx);
	} else {
		intr = new_idx != old_idx &&
			!(vq->avail->flags & VRING_AVAIL_F_NO_INTERRUPT);
	}
	if (intr)
		virtio_vq_interrupt(dev, vq);
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
