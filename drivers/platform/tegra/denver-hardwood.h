/*
 * Copyright (c) 2014-2015 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MACH_DENVER_HARDWOOD_H_
#define _MACH_DENVER_HARDWOOD_H_

#define N_CPU				2
#define N_BUFFER			16
#define N_BUFFER_LEGACY		4
#define BUFFER_SIZE			(1 << 20)
#define MAX_BUFFER_SIZE		(1 << 22)
#define BUFFER_SIZE_LEGACY	(1 << 22)
#define BUFFER_ORDER		8

#define OSDUMP_VER_TRACER_NAMES		101
#define OSDUMP_VER_OSDUMP_IRQS		102
#define OSDUMP_VER_NUM_BUFFERS		104

struct hardwood_cmd {
	__u8 core_id;
	__u8 buffer_id;
	__u64 data;
	__u64 data2;
} __packed;

/* Shouldn't be called by userspace */
#define HARDWOOD_SET_PHYS_ADDR		0x00
#define HARDWOOD_SET_BUFFER_SIZE	0x01

#define HARDWOOD_ENABLE_TRACE_DUMP	0x02
#define HARDWOOD_DISABLE_TRACE_DUMP	0x03
#define HARDWOOD_GET_IP_ADDRESS		0x04
#define HARDWOOD_SET_TRACER_MASK	0x05
#define HARDWOOD_CLR_TRACER_MASK	0x06
#define HARDWOOD_SET_IRQ_TARGET		0x07
#define HARDWOOD_SET_VIRT_ADDR		0x08
#define HARDWOOD_GET_OSDUMP_VER		0x09
#define HARDWOOD_GET_TR_NAMES		0x0a
#define HARDWOOD_GET_TR_NAMES_SZ	0x0b
#define HARDWOOD_SET_CLIENT_VER		0x0c
#define HARDWOOD_SET_OSDUMP_IRQS	0x0d
#define HARDWOOD_SET_NUM_BUFFERS	0x0e

#define HARDWOOD_GET_STATUS			0x10
#define HARDWOOD_GET_BYTES_USED		0x11
#define HARDWOOD_MARK_EMPTY			0x12
#define HARDWOOD_RELEASE_BUFFER		0x13
#define HARDWOOD_OVERFLOW_COUNT		0x14

#define HARDWOOD_GET_PHYS_ADDR		0x21
#define HARDWOOD_WAKEUP_READERS		0x22

/* IOCTL numbers */
#define __HD_NR		0xec
#define __HD_ST	struct hardwood_cmd

#define HARDWOOD_IOCTL_SET_PHYS_ADDR		_IOWR(__HD_NR, 0x00, __HD_ST)
#define HARDWOOD_IOCTL_SET_BUFFER_SIZE		_IOWR(__HD_NR, 0x01, __HD_ST)

#define HARDWOOD_IOCTL_ENABLE_TRACE_DUMP	_IOW(__HD_NR, 0x02, __HD_ST)
#define HARDWOOD_IOCTL_DISABLE_TRACE_DUMP	_IOW(__HD_NR, 0x03, __HD_ST)
#define HARDWOOD_IOCTL_GET_IP_ADDRESS		_IOWR(__HD_NR, 0x04, __HD_ST)
#define HARDWOOD_IOCTL_SET_TRACER_MASK		_IOWR(__HD_NR, 0x05, __HD_ST)
#define HARDWOOD_IOCTL_CLR_TRACER_MASK		_IOW(__HD_NR, 0x06, __HD_ST)
#define HARDWOOD_IOCTL_SET_IRQ_TARGET		_IOW(__HD_NR, 0x07, __HD_ST)
#define HARDWOOD_IOCTL_SET_VIRT_ADDR		_IOW(__HD_NR, 0x08, __HD_ST)
#define HARDWOOD_IOCTL_GET_OSDUMP_VER		_IOWR(__HD_NR, 0x09, __HD_ST)
#define HARDWOOD_IOCTL_GET_TR_NAMES		_IOWR(__HD_NR, 0x0a, __HD_ST)
#define HARDWOOD_IOCTL_GET_TR_NAMES_SZ		_IOWR(__HD_NR, 0x0b, __HD_ST)
#define HARDWOOD_IOCTL_SET_CLIENT_VER		_IOW(__HD_NR, 0x0c, __HD_ST)
#define HARDWOOD_IOCTL_SET_OSDUMP_IRQS		_IOW(__HD_NR, 0x0d, __HD_ST)
#define HARDWOOD_IOCTL_SET_NUM_BUFFERS		_IOW(__HD_NR, 0x0e, __HD_ST)

#define HARDWOOD_IOCTL_GET_STATUS		_IOWR(__HD_NR, 0x10, __HD_ST)
#define HARDWOOD_IOCTL_GET_BYTES_USED		_IOWR(__HD_NR, 0x11, __HD_ST)
#define HARDWOOD_IOCTL_MARK_EMPTY		_IOW(__HD_NR, 0x12, __HD_ST)
#define HARDWOOD_IOCTL_RELEASE_BUFFER		_IOW(__HD_NR, 0x13, __HD_ST)
#define HARDWOOD_IOCTL_OVERFLOW_COUNT		_IOWR(__HD_NR, 0x14, __HD_ST)

#define HARDWOOD_IOCTL_GET_PHYS_ADDR		_IOWR(__HD_NR, 0x21, __HD_ST)
#define HARDWOOD_IOCTL_WAKEUP_READERS		_IOW(__HD_NR, 0x22, __HD_ST)

#endif
