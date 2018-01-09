/*
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
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_VHM_IOCTL_DEFS_H_
#define	_VHM_IOCTL_DEFS_H_

/* Commmon structures for CWP/VHM/DM */
#include "cwp_common.h"

/*
 * Commmon IOCTL ID defination for VHM/DM
 */
#define _IC_ID(x, y) (((x)<<24)|(y))
#define IC_ID 0x5FUL

/* VM management */
#define IC_ID_VM_BASE                  0x0UL
#define IC_GET_API_VERSION             _IC_ID(IC_ID, IC_ID_VM_BASE + 0x00)
#define IC_CREATE_VM                   _IC_ID(IC_ID, IC_ID_VM_BASE + 0x01)
#define IC_DESTROY_VM                  _IC_ID(IC_ID, IC_ID_VM_BASE + 0x02)
#define IC_RESUME_VM                   _IC_ID(IC_ID, IC_ID_VM_BASE + 0x03)
#define IC_PAUSE_VM                    _IC_ID(IC_ID, IC_ID_VM_BASE + 0x04)
#define IC_QUERY_VMSTATE               _IC_ID(IC_ID, IC_ID_VM_BASE + 0x05)
#define	IC_CREATE_VCPU                 _IC_ID(IC_ID, IC_ID_VM_BASE + 0x06)

/* IRQ and Interrupts */
#define IC_ID_IRQ_BASE                 0x100UL
#define IC_ASSERT_IRQLINE              _IC_ID(IC_ID, IC_ID_IRQ_BASE + 0x00)
#define IC_DEASSERT_IRQLINE            _IC_ID(IC_ID, IC_ID_IRQ_BASE + 0x01)
#define IC_PULSE_IRQLINE               _IC_ID(IC_ID, IC_ID_IRQ_BASE + 0x02)
#define IC_INJECT_MSI                  _IC_ID(IC_ID, IC_ID_IRQ_BASE + 0x03)

/* DM ioreq management */
#define IC_ID_IOREQ_BASE                0x200UL
#define IC_SET_IOREQ_BUFFER             _IC_ID(IC_ID, IC_ID_IOREQ_BASE + 0x00)
#define IC_NOTIFY_REQUEST_FINISH        _IC_ID(IC_ID, IC_ID_IOREQ_BASE + 0x01)
#define IC_CREATE_IOREQ_CLIENT          _IC_ID(IC_ID, IC_ID_IOREQ_BASE + 0x02)
#define IC_ATTACH_IOREQ_CLIENT          _IC_ID(IC_ID, IC_ID_IOREQ_BASE + 0x03)
#define IC_DESTROY_IOREQ_CLIENT         _IC_ID(IC_ID, IC_ID_IOREQ_BASE + 0x04)


/* Guest memory management */
#define IC_ID_MEM_BASE                  0x300UL
#define IC_ALLOC_MEMSEG                 _IC_ID(IC_ID, IC_ID_MEM_BASE + 0x00)
#define IC_SET_MEMSEG                   _IC_ID(IC_ID, IC_ID_MEM_BASE + 0x01)

/* PCI assignment*/
#define IC_ID_PCI_BASE                  0x400UL
#define IC_ASSIGN_PTDEV                _IC_ID(IC_ID, IC_ID_PCI_BASE + 0x00)
#define IC_DEASSIGN_PTDEV              _IC_ID(IC_ID, IC_ID_PCI_BASE + 0x01)
#define IC_VM_PCI_MSIX_REMAP           _IC_ID(IC_ID, IC_ID_PCI_BASE + 0x02)
#define IC_SET_PTDEV_INTR_INFO         _IC_ID(IC_ID, IC_ID_PCI_BASE + 0x03)
#define IC_RESET_PTDEV_INTR_INFO       _IC_ID(IC_ID, IC_ID_PCI_BASE + 0x04)

#define SPECNAMELEN 63

enum {
	VM_SYSMEM,
	VM_BOOTROM,
	VM_FRAMEBUFFER,
	VM_MMIO,
};

struct vm_memseg {
	int segid;
	size_t len;
	char name[SPECNAMELEN + 1];
	unsigned long gpa;
};

struct vm_memmap {
	int segid;		/* memory segment */
	union {
		struct {
			uint64_t gpa;
			uint64_t segoff;	/* offset into memory segment */
			size_t len;		/* mmap length */
			int prot;		/* RWX */
			int flags;
		} mem;
		struct {
			uint64_t gpa;
			uint64_t hpa;
			size_t len;
			int prot;
		} mmio;
	};
};

struct ic_ptdev_irq {
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
			/* IN: vector count of MSI/MSIX,
                         * Keep this filed on top of msix */
			uint32_t vector_cnt;

			/* IN: size of MSI-X table (round up to 4K) */
			uint32_t table_size;

			/* IN: physical address of MSI-X table */
			uint64_t table_paddr;
		} msix;
	};
};

struct ioreq_notify {
       int32_t client_id;
       uint32_t vcpu;
};

#endif /* VHM_IOCTL_DEFS_H */
