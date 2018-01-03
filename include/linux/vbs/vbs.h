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

/* struct used to maintain virtio device info from userspace VBS */
struct virtio_dev_info {
	/* dev info from VBS */
	char name[VBS_NAME_LEN];	/* VBS device name */
	struct ctx _ctx;		/* device context */
	uint32_t negotiated_features;	/* features after guest loads driver */
	uint64_t io_range_start;	/* IO range start of VBS device */
	uint64_t io_range_len;		/* IO range len of VBS device */
	enum IORangeType io_range_type;	/* IO range type, PIO or MMIO */
};

/* VBS Runtime Control APIs */
long virtio_dev_ioctl(struct virtio_dev_info *dev, unsigned int ioctl,
		      void __user *argp);

#endif
