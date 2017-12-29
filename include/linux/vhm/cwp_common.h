/*
 * virtio and hyperviosr service module (VHM): commom.h
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
 * BSD LICENSE
 *
 * Copyright (C) 2017 Intel Corporation. All rights reserved.
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
 */

#ifndef CWP_COMMON_H
#define CWP_COMMON_H

/*
 * Commmon structures for CWP/VHM/DM
 */

/*
 * IO request
 */
#define VHM_REQUEST_MAX 16

enum request_state {
	REQ_STATE_SUCCESS = 1,
	REQ_STATE_PENDING = 0,
	REQ_STATE_PROCESSING = 2,
	REQ_STATE_FAILED = -1,
} __attribute__((aligned(4)));

enum request_type {
	REQ_MSR,
	REQ_CPUID,
	REQ_PORTIO,
	REQ_MMIO,
	REQ_PCICFG,
	REQ_WP,
	REQ_EXIT,
	REQ_MAX,
} __attribute__((aligned(4)));

enum request_direction {
	REQUEST_READ,
	REQUEST_WRITE,
	DIRECTION_MAX,
} __attribute__((aligned(4)));

struct msr_request {
	enum request_direction direction;
	long index;
	long value;
} __attribute__((aligned(8)));

struct cpuid_request {
	long eax_in;
	long ecx_in;
	long eax_out;
	long ebx_out;
	long ecx_out;
	long edx_out;
} __attribute__((aligned(8)));

struct mmio_request {
	enum request_direction direction;
	long address;
	long size;
	long value;
} __attribute__((aligned(8)));

struct io_request {
	enum request_direction direction;
	long address;
	long size;
	int value;
} __attribute__((aligned(8)));

struct pci_request {
	enum request_direction direction;
	long reserve; /*io_request address*/
	long size;
	int value;
	int bus;
	int dev;
	int func;
	int reg;
} __attribute__((aligned(8)));

/* vhm_request are 256Bytes aligned */
struct vhm_request {
	/* offset: 0bytes - 63bytes */
	enum request_type type;
	int reserved0[15];

	/* offset: 64bytes-127bytes */
	union {
		struct msr_request msr_request;
		struct cpuid_request cpuid_request;
		struct io_request pio_request;
		struct pci_request pci_request;
		struct mmio_request mmio_request;
		long reserved1[8];
	} reqs;

	/* True: valid req which need VHM to process.
	 * CWP write, VHM read only
	 **/
	int valid;

	/* the client which is distributed to handle this request */
	int client;

	/* 1: VHM had processed and success
	 *  0: VHM had not yet processed
	 * -1: VHM failed to process. Invalid request
	 * VHM write, CWP read only
	 **/
	enum request_state processed;
} __attribute__((aligned(256)));

struct vhm_request_buffer {
	union {
		struct vhm_request req_queue[VHM_REQUEST_MAX];
		char reserved[4096];
	};
} __attribute__((aligned(4096)));

/* Common API params */
struct cwp_create_vm {
	unsigned long vmid;		/* OUT: HV return vmid to VHM */
	unsigned long vcpu_num;		/* IN: VM vcpu number */
} __attribute__((aligned(8)));

struct cwp_set_ioreq_buffer {
	long req_buf;			/* IN: gpa of per VM request_buffer*/
} __attribute__((aligned(8)));

struct cwp_ioreq_notify {
	int client_id;
	unsigned long vcpu_mask;
} __attribute__((aligned(8)));

#endif /* CWP_COMMON_H */
