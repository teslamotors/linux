/*
 * common definition
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
 * Common structures for CWP/VHM/DM
 */

/*
 * IO request
 */
#define VHM_REQUEST_MAX 16

#define REQ_STATE_PENDING	0
#define REQ_STATE_SUCCESS	1
#define REQ_STATE_PROCESSING	2
#define REQ_STATE_FAILED	-1

#define REQ_PORTIO	0
#define REQ_MMIO	1
#define REQ_PCICFG	2
#define REQ_WP		3

#define REQUEST_READ	0
#define REQUEST_WRITE	1

struct mmio_request {
	uint32_t direction;
	uint32_t reserved;
	int64_t address;
	int64_t size;
	int64_t value;
} __attribute__((aligned(8)));

struct pio_request {
	uint32_t direction;
	uint32_t reserved;
	int64_t address;
	int64_t size;
	int32_t value;
} __attribute__((aligned(8)));

struct pci_request {
	uint32_t direction;
	uint32_t reserved[3];/* need keep same header fields with pio_request */
	int64_t size;
	int32_t value;
	int32_t bus;
	int32_t dev;
	int32_t func;
	int32_t reg;
} __attribute__((aligned(8)));

/* vhm_request are 256Bytes aligned */
struct vhm_request {
	/* offset: 0bytes - 63bytes */
	union {
		uint32_t type;
		int32_t reserved0[16];
	};
	/* offset: 64bytes-127bytes */
	union {
		struct pio_request pio_request;
		struct pci_request pci_request;
		struct mmio_request mmio_request;
		int64_t reserved1[8];
	} reqs;

	/* True: valid req which need VHM to process.
	 * CWP write, VHM read only
	 **/
	int32_t valid;

	/* the client which is distributed to handle this request */
	int32_t client;

	/* 1: VHM had processed and success
	 *  0: VHM had not yet processed
	 * -1: VHM failed to process. Invalid request
	 * VHM write, CWP read only
	 **/
	int32_t processed;
} __attribute__((aligned(256)));

struct vhm_request_buffer {
	union {
		struct vhm_request req_queue[VHM_REQUEST_MAX];
		int8_t reserved[4096];
	};
} __attribute__((aligned(4096)));

/* Common API params */
struct cwp_create_vm {
	int32_t vmid;	/* OUT: return vmid to VHM. Keep it first field */
	uint32_t vcpu_num;	/* IN: VM vcpu number */
	uint8_t	 GUID[16];	/* IN: GUID of this vm */
	uint8_t	 trusty_enabled;/* IN: whether trusty is enabled */
	uint8_t  reserved[31];	/* Reserved for future use */
} __attribute__((aligned(8)));

struct cwp_create_vcpu {
	uint32_t vcpu_id;	/* IN: vcpu id */
	uint32_t pcpu_id;	/* IN: pcpu id */
} __attribute__((aligned(8)));

struct cwp_set_ioreq_buffer {
	uint64_t req_buf;			/* IN: gpa of per VM request_buffer*/
} __attribute__((aligned(8)));

/*
 * intr type
 * IOAPIC: inject interrupt to IOAPIC
 * ISA: inject interrupt to both PIC and IOAPIC
 */
#define	CWP_INTR_TYPE_ISA	0
#define	CWP_INTR_TYPE_IOAPIC	1

/* For ISA, PIC, IOAPIC etc */
struct cwp_irqline {
	uint32_t intr_type;
	uint32_t reserved;
	uint64_t pic_irq;        /* IN: for ISA type */
	uint64_t ioapic_irq;    /* IN: for IOAPIC type, -1 don't inject */
} __attribute__((aligned(8)));

/* For MSI type inject */
struct cwp_msi_entry {
	uint64_t msi_addr;	/* IN: addr[19:12] with dest vcpu id */
	uint64_t msi_data;	/* IN: data[7:0] with vector */
} __attribute__((aligned(8)));

/* For NMI inject */
struct cwp_nmi_entry {
	int64_t vcpuid;		/* IN: -1 means vcpu0 */
} __attribute__((aligned(8)));

struct cwp_vm_pci_msix_remap {
	uint16_t virt_bdf;	/* IN: Device virtual BDF# */
	uint16_t phys_bdf;	/* IN: Device physical BDF# */
	uint16_t msi_ctl;		/* IN: PCI MSI/x cap control data */
	uint16_t reserved;
	uint64_t msi_addr;		/* IN/OUT: msi address to fix */
	uint32_t msi_data;		/* IN/OUT: msi data to fix */
	int32_t msix;			/* IN: 0 - MSI, 1 - MSI-X */
	int32_t msix_entry_index;	/* IN: MSI-X the entry table index */
	/* IN: Vector Control for MSI-X Entry, field defined in MSIX spec */
	uint32_t vector_ctl;
} __attribute__((aligned(8)));

#endif /* CWP_COMMON_H */
