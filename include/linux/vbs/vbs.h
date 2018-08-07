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
 *  Define data structures and runtime control APIs for VBS framework.
 *  - VBS-K is a kernel-level virtio framework that can be used for
 *    virtio backend driver development for ACRN hypervisor.
 *  - VBS-K should be working with VBS-U (Virtio Backend Service in
 *    User) together, in order to connect with virtio frontend driver.
 */

#ifndef _VBS_H_
#define _VBS_H_

#include <linux/vbs/vbs_common_if.h>
#include <linux/vhm/acrn_common.h>
#include <linux/vhm/acrn_vhm_ioreq.h>

/**
 * enum IORangeType - type of registers to be handled in VBS-K
 *
 * @PIO_RANGE	: Port I/O registers, for virtio 0.9.5
 * @MMIO_RANGE	: Memory-Mapped I/O registers, for virtio 1.0+
 */
enum IORangeType {
	PIO_RANGE = 0x0,		/* default */
	MMIO_RANGE = 0x1,
};

/**
 * struct ctx - VM context this device belongs to
 *
 * @vmid		: ID of VM this device belongs to
 * @vhm_client_id	: ID of VHM client this device registers
 * @max_vcpu		: number of VCPU in this VM
 * @req_buf		: request buffers
 */
struct ctx {
	int vmid;
	int vhm_client_id;
	int max_vcpu;
	struct vhm_request *req_buf;
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

/**
 * struct virtio_vq_info - virtqueue data structure
 */
struct virtio_vq_info {
	/* virtqueue info from VBS-U */
	/** @qsize: size of this queue (a power of 2) */
	uint16_t qsize;
	/** @pfn: PFN of virt queue (not shifted!) */
	uint32_t pfn;
	/** @msix_idx: MSI-X index/VIRTIO_MSI_NO_VECTOR */
	uint16_t msix_idx;
	/** @msix_addr: MSI-X address specified by index */
	uint64_t msix_addr;
	/** @msix_data: MSI-X data specified by index */
	uint32_t msix_data;

	/* members created in kernel space VBS */
	/** @vq_notify: vq-wide notification */
	int (*vq_notify)(int);
	/** @dev: backpointer to virtio_dev_info */
	struct virtio_dev_info *dev;
	/** @num: we're the num'th virtqueue */
	uint16_t num;
	/** @flags: virtqueue flags */
	uint16_t flags;
	/* private: a recent value of vq_avail->va_idx */
	uint16_t last_avail;
	/* private: saved vq_used->vu_idx */
	uint16_t save_used;

	/* private: descriptor array */
	volatile struct virtio_desc *desc;
	/* private: the "avail" ring */
	volatile struct vring_avail *avail;
	/* private: the "used" ring */
	volatile struct vring_used *used;
};

/**
 * struct virtio_dev_info - VBS-K device data structure
 */
struct virtio_dev_info {
	/* dev info from VBS */
	/** @name[]: VBS device name */
	char name[VBS_NAME_LEN];
	/** @_ctx: VM context this device belongs to */
	struct ctx _ctx;
	/** @nvq: number of virtqueues */
	int nvq;
	/** @negotiated_features: features after guest loads driver */
	uint32_t negotiated_features;
	/** @io_range_start: start of an IO range VBS needs to handle */
	uint64_t io_range_start;
	/** @io_range_len: len of an IO range VBS needs to handle */
	uint64_t io_range_len;
	/** @io_range_type: IO range type, PIO or MMIO */
	enum IORangeType io_range_type;

	/* members created in kernel space VBS */
	/**
	 * @dev_notify: device-wide notification
	 *
	 * This is the callback function to be registered to VHM,
	 * so that VBS gets notified when frontend accessed the register.
	 */
	int (*dev_notify)(int, unsigned long *);
	/** @vqs: virtqueue(s) of this device */
	struct virtio_vq_info *vqs;
	/** @curq: current virtqueue index */
	int curq;
};

/**
 * virtio_dev_client_id - get device's VHM client ID
 *
 * @dev: VBS-K device data struct
 *
 * Return: device's VHM client ID
 */
static inline int virtio_dev_client_id(struct virtio_dev_info *dev)
{
	return dev->_ctx.vhm_client_id;
}

/* VBS Runtime Control APIs */

/**
 * virtio_dev_init - Initialize VBS-K device data structures
 *
 * @dev: Pointer to VBS-K device data struct
 * @vqs: Pointer to VBS-K virtqueue data struct, normally in an array
 * @nvq: Number of virtqueues this device has
 *
 * Return: 0 on success, <0 on error
 */
long virtio_dev_init(struct virtio_dev_info *dev, struct virtio_vq_info *vqs,
		     int nvq);

/**
 * virtio_dev_ioctl - VBS-K device's common ioctl routine
 *
 * @dev: Pointer to VBS-K device data struct
 * @ioctl: Command of ioctl to device
 * @argp: Data from user space
 *
 * Return: 0 on success, <0 on error
 */
long virtio_dev_ioctl(struct virtio_dev_info *dev, unsigned int ioctl,
		      void __user *argp);

/**
 * virtio_vqs_ioctl - VBS-K vq's common ioctl routine
 *
 * @dev: Pointer to VBS-K device data struct
 * @ioctl: Command of ioctl to virtqueue
 * @argp: Data from user space
 *
 * Return: 0 on success, <0 on error
 */
long virtio_vqs_ioctl(struct virtio_dev_info *dev, unsigned int ioctl,
		      void __user *argp);

/**
 * virtio_dev_register - register a VBS-K device to VHM
 *
 * Each VBS-K device will be registered as a VHM client, with the
 * information including "kick" register location, callback, etc.
 *
 * @dev: Pointer to VBS-K device data struct
 *
 * Return: 0 on success, <0 on error
 */
long virtio_dev_register(struct virtio_dev_info *dev);

/**
 * virtio_dev_register - unregister a VBS-K device from VHM
 *
 * Destroy the client corresponding to the VBS-K device specified.
 *
 * @dev: Pointer to VBS-K device data struct
 *
 * Return: 0 on success, <0 on error
 */
long virtio_dev_deregister(struct virtio_dev_info *dev);

/**
 * virtio_vq_index_get - get virtqueue index that frontend kicks
 *
 * This API is normally called in the VBS-K device's callback
 * function, to get value write to the "kick" register from
 * frontend.
 *
 * @dev: Pointer to VBS-K device data struct
 * @ioreqs_map: requests bitmap need to handle, provided by VHM
 *
 * Return: >=0 on virtqueue index, <0 on error
 */
int virtio_vq_index_get(struct virtio_dev_info *dev, unsigned long *ioreqs_map);

#endif
