/*
 * hypercall definition
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

#ifndef __ACRN_HV_DEFS_H__
#define __ACRN_HV_DEFS_H__

/*
 * Common structures for ACRN/VHM/DM
 */
#include "acrn_common.h"

/*
 * Common structures for HV/VHM
 */

#define _HC_ID(x, y) (((x)<<24)|(y))

#define HC_ID 0x80UL

/* general */
#define HC_ID_GEN_BASE               0x0UL
#define HC_GET_API_VERSION          _HC_ID(HC_ID, HC_ID_GEN_BASE + 0x00)
#define HC_SOS_OFFLINE_CPU          _HC_ID(HC_ID, HC_ID_GEN_BASE + 0x01)

/* VM management */
#define HC_ID_VM_BASE               0x10UL
#define HC_CREATE_VM                _HC_ID(HC_ID, HC_ID_VM_BASE + 0x00)
#define HC_DESTROY_VM               _HC_ID(HC_ID, HC_ID_VM_BASE + 0x01)
#define HC_START_VM                 _HC_ID(HC_ID, HC_ID_VM_BASE + 0x02)
#define HC_PAUSE_VM                 _HC_ID(HC_ID, HC_ID_VM_BASE + 0x03)
#define HC_CREATE_VCPU              _HC_ID(HC_ID, HC_ID_VM_BASE + 0x04)
#define HC_RESET_VM                 _HC_ID(HC_ID, HC_ID_VM_BASE + 0x05)

/* IRQ and Interrupts */
#define HC_ID_IRQ_BASE              0x20UL
#define HC_ASSERT_IRQLINE           _HC_ID(HC_ID, HC_ID_IRQ_BASE + 0x00)
#define HC_DEASSERT_IRQLINE         _HC_ID(HC_ID, HC_ID_IRQ_BASE + 0x01)
#define HC_PULSE_IRQLINE            _HC_ID(HC_ID, HC_ID_IRQ_BASE + 0x02)
#define HC_INJECT_MSI               _HC_ID(HC_ID, HC_ID_IRQ_BASE + 0x03)

/* DM ioreq management */
#define HC_ID_IOREQ_BASE            0x30UL
#define HC_SET_IOREQ_BUFFER         _HC_ID(HC_ID, HC_ID_IOREQ_BASE + 0x00)
#define HC_NOTIFY_REQUEST_FINISH    _HC_ID(HC_ID, HC_ID_IOREQ_BASE + 0x01)

/* Guest memory management */
#define HC_ID_MEM_BASE              0x40UL
#define HC_VM_SET_MEMMAP            _HC_ID(HC_ID, HC_ID_MEM_BASE + 0x00)
#define HC_VM_GPA2HPA               _HC_ID(HC_ID, HC_ID_MEM_BASE + 0x01)
#define HC_VM_SET_MEMMAPS           _HC_ID(HC_ID, HC_ID_MEM_BASE + 0x02)

/* PCI assignment*/
#define HC_ID_PCI_BASE              0x50UL
#define HC_ASSIGN_PTDEV             _HC_ID(HC_ID, HC_ID_PCI_BASE + 0x00)
#define HC_DEASSIGN_PTDEV           _HC_ID(HC_ID, HC_ID_PCI_BASE + 0x01)
#define HC_VM_PCI_MSIX_REMAP        _HC_ID(HC_ID, HC_ID_PCI_BASE + 0x02)
#define HC_SET_PTDEV_INTR_INFO      _HC_ID(HC_ID, HC_ID_PCI_BASE + 0x03)
#define HC_RESET_PTDEV_INTR_INFO    _HC_ID(HC_ID, HC_ID_PCI_BASE + 0x04)

/* DEBUG */
#define HC_ID_DBG_BASE              0x60UL
#define HC_SETUP_SBUF               _HC_ID(HC_ID, HC_ID_DBG_BASE + 0x00)

/* Power management */
#define HC_ID_PM_BASE               0x80UL
#define HC_PM_GET_CPU_STATE         _HC_ID(HC_ID, HC_ID_PM_BASE + 0x00)
#define HC_PM_SET_SSTATE_DATA       _HC_ID(HC_ID, HC_ID_PM_BASE + 0x01)

#define ACRN_DOM0_VMID (0UL)
#define ACRN_INVALID_VMID (-1)
#define ACRN_INVALID_HPA (-1UL)

/* Generic memory attributes */
#define	MEM_ACCESS_READ                 0x00000001
#define	MEM_ACCESS_WRITE                0x00000002
#define	MEM_ACCESS_EXEC	                0x00000004
#define	MEM_ACCESS_RWX			(MEM_ACCESS_READ | MEM_ACCESS_WRITE | \
						MEM_ACCESS_EXEC)
#define MEM_ACCESS_RIGHT_MASK           0x00000007
#define	MEM_TYPE_WB                     0x00000040
#define	MEM_TYPE_WT                     0x00000080
#define	MEM_TYPE_UC                     0x00000100
#define	MEM_TYPE_WC                     0x00000200
#define	MEM_TYPE_WP                     0x00000400
#define MEM_TYPE_MASK                   0x000007C0

struct vm_set_memmap {
#define MAP_MEM		0
#define MAP_UNMAP	2
	uint32_t type;

	/* IN: mem attr */
	uint32_t prot;

	/* IN: beginning guest GPA to map */
	uint64_t remote_gpa;

	/* IN: VM0's GPA which foreign gpa will be mapped to */
	uint64_t vm0_gpa;

	/* IN: length of the range */
	uint64_t length;

	uint32_t prot_2;
} __attribute__((aligned(8)));

struct memory_map {
	uint32_t type;

	/* IN: mem attr */
	uint32_t prot;

	/* IN: beginning guest GPA to map */
	uint64_t remote_gpa;

	/* IN: VM0's GPA which foreign gpa will be mapped to */
	uint64_t vm0_gpa;

	/* IN: length of the range */
	uint64_t length;
} __attribute__((aligned(8)));

struct set_memmaps {
	/*IN: vmid for this hypercall */
	uint64_t vmid;

	/* IN: multi memmaps numbers */
	uint32_t memmaps_num;

	/* IN:
	 * the gpa of memmaps buffer, point to the memmaps array:
	 *  	struct memory_map memmap_array[memmaps_num]
	 * the max buffer size is one page.
	 */
	uint64_t memmaps_gpa;
} __attribute__((aligned(8)));

struct sbuf_setup_param {
	uint32_t pcpu_id;
	uint32_t sbuf_id;
	uint64_t gpa;
} __attribute__((aligned(8)));

struct vm_gpa2hpa {
	uint64_t gpa;		/* IN: gpa to translation */
	uint64_t hpa;		/* OUT: -1 means invalid gpa */
} __attribute__((aligned(8)));

struct hc_ptdev_irq {
#define IRQ_INTX 0
#define IRQ_MSI 1
#define IRQ_MSIX 2
	uint32_t type;
	uint16_t virt_bdf;	/* IN: Device virtual BDF# */
	uint16_t phys_bdf;	/* IN: Device physical BDF# */
	union {
		struct {
			uint32_t virt_pin;	/* IN: virtual IOAPIC pin */
			uint32_t phys_pin;	/* IN: physical IOAPIC pin */
			uint32_t pic_pin;	/* IN: pin from PIC? */
		} intx;
		struct {
			/* IN: vector count of MSI/MSIX */
			uint32_t vector_cnt;
		} msix;
	};
} __attribute__((aligned(8)));

struct hc_api_version {
	uint32_t major_version;
	uint32_t minor_version;
} __attribute__((aligned(8)));

#endif /* __ACRN_HV_DEFS_H__ */
