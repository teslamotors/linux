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
 *  - Define data structures shared between VBS userspace and VBS kernel
 *    space.
 */

#ifndef _VBS_COMMON_IF_H_
#define _VBS_COMMON_IF_H_

#define VBS_MAX_VQ_CNT		10
#define VBS_NAME_LEN		32
#define VIRTIO_MSI_NO_VECTOR	0xFFFF

struct vbs_vq_info {
	uint16_t qsize;		/* size of this virtqueue (a power of 2) */
	uint32_t pfn;		/* PFN of virtqueue (not shifted!) */
	uint16_t msix_idx;	/* MSI-X index, or VIRTIO_MSI_NO_VECTOR */
	uint64_t msix_addr;	/* MSI-X address specified by index */
	uint32_t msix_data;	/* MSI-X data specified by index */
};

struct vbs_vqs_info {
	uint32_t nvq;		/* number of virtqueues */
	struct vbs_vq_info vqs[VBS_MAX_VQ_CNT];
				/* array of struct vbs_vq_info */
};

struct vbs_dev_info {
	char name[VBS_NAME_LEN];/* VBS name */
	int vmid;		/* id of VM this device belongs to */
	int nvq;		/* number of virtqueues */
	uint32_t negotiated_features;
				/* features after VIRTIO_CONFIG_S_DRIVER_OK */
	uint64_t pio_range_start;
				/* start of PIO range initialized by guest OS */
	uint64_t pio_range_len;	/* len of PIO range initialized by guest OS */
};

#define VBS_IOCTL	0xAF

#define VBS_SET_DEV _IOW(VBS_IOCTL, 0x00, struct vbs_dev_info)
#define VBS_SET_VQ _IOW(VBS_IOCTL, 0x01, struct vbs_vqs_info)
#define VBS_RESET_DEV _IO(VBS_IOCTL, 0x02)

#endif
