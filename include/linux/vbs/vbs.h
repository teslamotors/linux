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
 *  Define data structures and runtime control APIs for VBS framework.
 *  - VBS-K is a kernel-level virtio framework that can be used for
 *    virtio backend driver development for CWP hypervisor.
 *  - VBS-K should be working with VBS-U (Virtio Backend Service in
 *    User) together, in order to connect with virtio frontend driver.
 */

#ifndef _VBS_H_
#define _VBS_H_

#include <linux/vbs/vbs_common_if.h>

/*
 * VBS-K device needs to handle frontend driver's kick in kernel.
 * For virtio 0.9.5, the kick register is a PIO register,
 * for virtio 1.0+, the kick register could be a MMIO register.
 */
enum IORangeType {
	PIO_RANGE = 0x0,		/* default */
	MMIO_RANGE = 0x1,
};

/* device context */
struct ctx {
	/* VHM required info */
	int vmid;
};

struct virtio_desc {			/* AKA vring_desc */
	uint64_t addr;			/* guest physical address */
	uint32_t len;			/* length of scatter/gather seg */
	uint16_t flags;			/* desc flags */
	uint16_t next;			/* next desc if F_NEXT */
} __attribute__((packed));

struct virtio_used {			/* AKA vring_used_elem */
	uint32_t idx;			/* head of used descriptor chain */
	uint32_t len;			/* length written-to */
} __attribute__((packed));

struct vring_avail {
	uint16_t flags;			/* vring_avail flags */
	uint16_t idx;			/* counts to 65535, then cycles */
	uint16_t ring[];		/* size N, reported in QNUM value */
} __attribute__((packed));

struct vring_used {
	uint16_t flags;			/* vring_used flags */
	uint16_t idx;			/* counts to 65535, then cycles */
	struct virtio_used ring[];	/* size N */
} __attribute__((packed));

/* struct used to maintain virtqueue info from userspace VBS */
struct virtio_vq_info {
	/* virtqueue info from VBS-U */
	uint16_t qsize;			/* size of this queue (a power of 2) */
	uint32_t pfn;			/* PFN of virt queue (not shifted!) */
	uint16_t msix_idx;		/* MSI-X index/VIRTIO_MSI_NO_VECTOR */
	uint64_t msix_addr;		/* MSI-X address specified by index */
	uint32_t msix_data;		/* MSI-X data specified by index */

	/* members created in kernel space VBS */
	int (*vq_notify)(int);		/* vq-wide notification */
	struct virtio_dev_info *dev;	/* backpointer to virtio_dev_info */
	uint16_t num;			/* we're the num'th virtqueue */
	uint16_t flags;			/* virtqueue flags */
	uint16_t last_avail;		/* a recent value of vq_avail->va_idx */
	uint16_t save_used;		/* saved vq_used->vu_idx */

	volatile struct virtio_desc *desc;   /* descriptor array */
	volatile struct vring_avail *avail;  /* the "avail" ring */
	volatile struct vring_used *used;    /* the "used" ring */
};

/* struct used to maintain virtio device info from userspace VBS */
struct virtio_dev_info {
	/* dev info from VBS */
	char name[VBS_NAME_LEN];	/* VBS device name */
	struct ctx _ctx;		/* device context */
	int nvq;			/* number of virtqueues */
	uint32_t negotiated_features;	/* features after guest loads driver */
	uint64_t io_range_start;	/* IO range start of VBS device */
	uint64_t io_range_len;		/* IO range len of VBS device */
	enum IORangeType io_range_type;	/* IO range type, PIO or MMIO */

	/* members created in kernel space VBS */
	void (*dev_notify)(void *, struct virtio_vq_info *);
					/* device-wide notification */
	struct virtio_vq_info *vqs;	/* virtqueue(s) */
	int curq;			/* current virtqueue index */
};

/* VBS Runtime Control APIs */
long virtio_dev_init(struct virtio_dev_info *dev, struct virtio_vq_info *vqs,
		     int nvq);
long virtio_dev_ioctl(struct virtio_dev_info *dev, unsigned int ioctl,
		      void __user *argp);
long virtio_vqs_ioctl(struct virtio_dev_info *dev, unsigned int ioctl,
		      void __user *argp);

#endif
