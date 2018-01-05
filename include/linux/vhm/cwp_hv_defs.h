/*
 * virtio and hyperviosr service module (VHM): hypercall header
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

#ifndef CWP_HV_DEFS_H
#define CWP_HV_DEFS_H

/*
 * Commmon structures for CWP/VHM/DM
 */
#include "cwp_common.h"

/*
 * Commmon structures for HV/VHM
 */

#define _HC_ID(x, y) (((x)<<24)|(y))

#define HC_ID 0x7FUL

/* VM management */
#define HC_ID_VM_BASE               0x0UL
#define HC_GET_API_VERSION          _HC_ID(HC_ID, HC_ID_VM_BASE + 0x00)
#define HC_CREATE_VM                _HC_ID(HC_ID, HC_ID_VM_BASE + 0x01)
#define HC_DESTROY_VM               _HC_ID(HC_ID, HC_ID_VM_BASE + 0x02)
#define HC_RESUME_VM                _HC_ID(HC_ID, HC_ID_VM_BASE + 0x03)
#define HC_PAUSE_VM                 _HC_ID(HC_ID, HC_ID_VM_BASE + 0x04)
#define HC_QUERY_VMSTATE            _HC_ID(HC_ID, HC_ID_VM_BASE + 0x05)
#define HC_CREATE_VCPU              _HC_ID(HC_ID, HC_ID_VM_BASE + 0x06)

/* IRQ and Interrupts */
#define HC_ID_IRQ_BASE              0x100UL
#define HC_ASSERT_IRQLINE           _HC_ID(HC_ID, HC_ID_IRQ_BASE + 0x00)
#define HC_DEASSERT_IRQLINE         _HC_ID(HC_ID, HC_ID_IRQ_BASE + 0x01)
#define HC_PULSE_IRQLINE            _HC_ID(HC_ID, HC_ID_IRQ_BASE + 0x02)
#define HC_INJECT_MSI               _HC_ID(HC_ID, HC_ID_IRQ_BASE + 0x03)

/* DM ioreq management */
#define HC_ID_IOREQ_BASE            0x200UL
#define HC_SET_IOREQ_BUFFER         _HC_ID(HC_ID, HC_ID_IOREQ_BASE + 0x00)
#define HC_NOTIFY_REQUEST_FINISH    _HC_ID(HC_ID, HC_ID_IOREQ_BASE + 0x01)


/* Guest memory management */
#define HC_ID_MEM_BASE              0x300UL
#define HC_VM_SET_MEMMAP            _HC_ID(HC_ID, HC_ID_MEM_BASE + 0x00)
#define HC_VM_GPA2HPA               _HC_ID(HC_ID, HC_ID_MEM_BASE + 0x01)

/* PCI assignment*/
#define HC_ID_PCI_BASE              0x400UL
#define HC_ASSIGN_PTDEV             _HC_ID(HC_ID, HC_ID_PCI_BASE + 0x00)
#define HC_DEASSIGN_PTDEV           _HC_ID(HC_ID, HC_ID_PCI_BASE + 0x01)
#define HC_VM_PCI_MSIX_REMAP        _HC_ID(HC_ID, HC_ID_PCI_BASE + 0x02)
#define HC_SET_PTDEV_INTR_INFO      _HC_ID(HC_ID, HC_ID_PCI_BASE + 0x03)
#define HC_RESET_PTDEV_INTR_INFO    _HC_ID(HC_ID, HC_ID_PCI_BASE + 0x04)

#define CWP_DOM0_VMID (0UL)
#define CWP_INVALID_VMID (-1UL)
#define CWP_INVALID_HPA (-1UL)

enum vm_memmap_type {
	MAP_MEM = 0,
	MAP_MMIO,
	MAP_UNMAP,
	MAP_UPDATE,
};

struct vm_set_memmap {
	enum vm_memmap_type type;
	/* IN: beginning guest GPA to map */
	unsigned long foreign_gpa;

	/* IN: VM0's GPA which foreign gpa will be mapped to */
	unsigned long hvm_gpa;

	/* IN: length of the range */
	unsigned long length;

	/* IN: not used right now */
	int prot;
} __attribute__((aligned(8)));

struct vm_gpa2hpa {
	unsigned long gpa;		/* IN: gpa to translation */
	unsigned long hpa;		/* OUT: -1 means invalid gpa */
} __attribute__((aligned(8)));

#endif /* CWP_HV_DEFS_H */
