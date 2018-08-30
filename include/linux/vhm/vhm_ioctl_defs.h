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

/**
 * @file vhm_ioctl_defs.h
 *
 * @brief Virtio and Hypervisor Module definition for ioctl to user space
 */

#ifndef __VHM_IOCTL_DEFS_H__
#define __VHM_IOCTL_DEFS_H__

/* Common structures for ACRN/VHM/DM */
#include "acrn_common.h"

/*
 * Common IOCTL ID definition for VHM/DM
 */
#define _IC_ID(x, y) (((x)<<24)|(y))
#define IC_ID 0x43UL

/* General */
#define IC_ID_GEN_BASE                  0x0UL
#define IC_GET_API_VERSION             _IC_ID(IC_ID, IC_ID_GEN_BASE + 0x00)

/* VM management */
#define IC_ID_VM_BASE                  0x10UL
#define IC_CREATE_VM                   _IC_ID(IC_ID, IC_ID_VM_BASE + 0x00)
#define IC_DESTROY_VM                  _IC_ID(IC_ID, IC_ID_VM_BASE + 0x01)
#define IC_START_VM                    _IC_ID(IC_ID, IC_ID_VM_BASE + 0x02)
#define IC_PAUSE_VM                    _IC_ID(IC_ID, IC_ID_VM_BASE + 0x03)
#define	IC_CREATE_VCPU                 _IC_ID(IC_ID, IC_ID_VM_BASE + 0x04)
#define IC_RESET_VM                    _IC_ID(IC_ID, IC_ID_VM_BASE + 0x05)

/* IRQ and Interrupts */
#define IC_ID_IRQ_BASE                 0x20UL
#define IC_ASSERT_IRQLINE              _IC_ID(IC_ID, IC_ID_IRQ_BASE + 0x00)
#define IC_DEASSERT_IRQLINE            _IC_ID(IC_ID, IC_ID_IRQ_BASE + 0x01)
#define IC_PULSE_IRQLINE               _IC_ID(IC_ID, IC_ID_IRQ_BASE + 0x02)
#define IC_INJECT_MSI                  _IC_ID(IC_ID, IC_ID_IRQ_BASE + 0x03)

/* DM ioreq management */
#define IC_ID_IOREQ_BASE                0x30UL
#define IC_SET_IOREQ_BUFFER             _IC_ID(IC_ID, IC_ID_IOREQ_BASE + 0x00)
#define IC_NOTIFY_REQUEST_FINISH        _IC_ID(IC_ID, IC_ID_IOREQ_BASE + 0x01)
#define IC_CREATE_IOREQ_CLIENT          _IC_ID(IC_ID, IC_ID_IOREQ_BASE + 0x02)
#define IC_ATTACH_IOREQ_CLIENT          _IC_ID(IC_ID, IC_ID_IOREQ_BASE + 0x03)
#define IC_DESTROY_IOREQ_CLIENT         _IC_ID(IC_ID, IC_ID_IOREQ_BASE + 0x04)

/* Guest memory management */
#define IC_ID_MEM_BASE                  0x40UL
/* IC_ALLOC_MEMSEG not used */
#define IC_ALLOC_MEMSEG                 _IC_ID(IC_ID, IC_ID_MEM_BASE + 0x00)
#define IC_SET_MEMSEG                   _IC_ID(IC_ID, IC_ID_MEM_BASE + 0x01)

/* PCI assignment*/
#define IC_ID_PCI_BASE                  0x50UL
#define IC_ASSIGN_PTDEV                _IC_ID(IC_ID, IC_ID_PCI_BASE + 0x00)
#define IC_DEASSIGN_PTDEV              _IC_ID(IC_ID, IC_ID_PCI_BASE + 0x01)
#define IC_VM_PCI_MSIX_REMAP           _IC_ID(IC_ID, IC_ID_PCI_BASE + 0x02)
#define IC_SET_PTDEV_INTR_INFO         _IC_ID(IC_ID, IC_ID_PCI_BASE + 0x03)
#define IC_RESET_PTDEV_INTR_INFO       _IC_ID(IC_ID, IC_ID_PCI_BASE + 0x04)

/* Power management */
#define IC_ID_PM_BASE                   0x60UL
#define IC_PM_GET_CPU_STATE            _IC_ID(IC_ID, IC_ID_PM_BASE + 0x00)
#define IC_PM_SET_SSTATE_DATA          _IC_ID(IC_ID, IC_ID_PM_BASE + 0x01)

/**
 * struct vm_memseg - memory segment info for guest
 *
 * @len: length of memory segment
 * @gpa: guest physical start address of memory segment
 */
struct vm_memseg {
	uint64_t len;
	uint64_t gpa;
};

#define VM_MEMMAP_SYSMEM       0
#define VM_MEMMAP_MMIO         1

/**
 * struct vm_memmap - EPT memory mapping info for guest
 */
struct vm_memmap {
	/** @type: memory mapping type */
	uint32_t type;
	/** @using_vma: using vma_base to get vm0_gpa,
	 * only for type == VM_SYSTEM
	 */
	uint32_t using_vma;
	/** @gpa: user OS guest physical start address of memory mapping */
	uint64_t gpa;
	/** union */
	union {
		/** @hpa: host physical start address of memory,
		 * only for type == VM_MEMMAP_MMIO
		 */
		uint64_t hpa;
		/** @vma_base: service OS user virtual start address of
		 * memory, only for type == VM_MEMMAP_SYSMEM &&
		 * using_vma == true
		 */
		uint64_t vma_base;
	};
	/** @len: the length of memory range mapped */
	uint64_t len;	/* mmap length */
	/** @prot: memory mapping attribute */
	uint32_t prot;	/* RWX */
};

/**
 * struct ic_ptdev_irq - pass thru device irq data structure
 */
struct ic_ptdev_irq {
#define IRQ_INTX 0
#define IRQ_MSI 1
#define IRQ_MSIX 2
	/** @type: irq type */
	uint32_t type;
	/** @virt_bdf: virtual bdf description of pass thru device */
	uint16_t virt_bdf;	/* IN: Device virtual BDF# */
	/** @phys_bdf: physical bdf description of pass thru device */
	uint16_t phys_bdf;	/* IN: Device physical BDF# */
	/** union */
	union {
		/** struct intx - info of IOAPIC/PIC interrupt */
		struct {
			/** @virt_pin: virtual IOAPIC pin */
			uint32_t virt_pin;
			/** @phys_pin: physical IOAPIC pin */
			uint32_t phys_pin;
			/** @is_pic_pin: PIC pin */
			uint32_t is_pic_pin;
		} intx;

		/** struct msix - info of MSI/MSIX interrupt */
		struct {
                        /* Keep this filed on top of msix */
			/** @vector_cnt: vector count of MSI/MSIX */
			uint32_t vector_cnt;

			/** @table_size: size of MSIX table(round up to 4K) */
			uint32_t table_size;

			/** @table_paddr: physical address of MSIX table */
			uint64_t table_paddr;
		} msix;
	};
};

/**
 * struct ioreq_notify - data structure to notify hypervisor ioreq is handled
 *
 * @client_id: client id to identify ioreq client
 * @vcpu: identify the ioreq submitter
 */
struct ioreq_notify {
       int32_t client_id;
       uint32_t vcpu;
};

/**
 * struct api_version - data structure to track VHM API version
 *
 * @major_version: major version of VHM API
 * @minor_version: minor version of VHM API
 */
struct api_version {
	uint32_t major_version;
	uint32_t minor_version;
};

#endif /* __VHM_IOCTL_DEFS_H__ */
