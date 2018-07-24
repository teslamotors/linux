/*
 * virtio and hyperviosr service module (VHM): hypercall.h
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

#ifndef __VHM_HYPERCALL_H__
#define __VHM_HYPERCALL_H__

static inline long acrn_hypercall0(unsigned long hcall_id)
{

	/* x86-64 System V ABI register usage */
	register signed long    result asm("rax");
	register unsigned long  r8 asm("r8")  = hcall_id;

	/* Execute vmcall */
	asm volatile(".byte 0x0F,0x01,0xC1\n"
			: "=r"(result)
			:  "r"(r8));

	/* Return result to caller */
	return result;
}

static inline long acrn_hypercall1(unsigned long hcall_id, unsigned long param1)
{

	/* x86-64 System V ABI register usage */
	register signed long    result asm("rax");
	register unsigned long  r8 asm("r8")  = hcall_id;

	/* Execute vmcall */
	asm volatile(".byte 0x0F,0x01,0xC1\n"
			: "=r"(result)
			: "D"(param1), "r"(r8));

	/* Return result to caller */
	return result;
}

static inline long acrn_hypercall2(unsigned long hcall_id, unsigned long param1,
		unsigned long param2)
{

	/* x86-64 System V ABI register usage */
	register signed long    result asm("rax");
	register unsigned long  r8 asm("r8")  = hcall_id;

	/* Execute vmcall */
	asm volatile(".byte 0x0F,0x01,0xC1\n"
			: "=r"(result)
			: "D"(param1), "S"(param2), "r"(r8));

	/* Return result to caller */
	return result;
}

static inline long acrn_hypercall3(unsigned long hcall_id, unsigned long param1,
		unsigned long param2, unsigned long param3)
{

	/* x86-64 System V ABI register usage */
	register signed long    result asm("rax");
	register unsigned long  r8 asm("r8")  = hcall_id;

	/* Execute vmcall */
	asm volatile(".byte 0x0F,0x01,0xC1\n"
			: "=r"(result)
			: "D"(param1), "S"(param2), "d"(param3), "r"(r8));

	/* Return result to caller */
	return result;
}

static inline long acrn_hypercall4(unsigned long hcall_id, unsigned long param1,
		unsigned long param2, unsigned long param3,
		unsigned long param4)
{

	/* x86-64 System V ABI register usage */
	register signed long    result asm("rax");
	register unsigned long  r8 asm("r8")  = hcall_id;

	/* Execute vmcall */
	asm volatile(".byte 0x0F,0x01,0xC1\n"
			: "=r"(result)
			: "D"(param1), "S"(param2), "d"(param3),
			  "c"(param4), "r"(r8));

	/* Return result to caller */
	return result;
}

inline long hcall_sos_offline_cpu(unsigned long cpu);
inline long hcall_get_api_version(unsigned long api_version);
inline long hcall_create_vm(unsigned long vminfo);
inline long hcall_start_vm(unsigned long vmid);
inline long hcall_pause_vm(unsigned long vmid);
inline long hcall_destroy_vm(unsigned long vmid);
inline long hcall_reset_vm(unsigned long vmid);
inline long hcall_query_vm_state(unsigned long vmid);
inline long hcall_setup_sbuf(unsigned long sbuf_head);
inline long hcall_set_sstate_data(unsigned long sx_data_addr);
inline long hcall_get_cpu_state(unsigned long cmd, unsigned long state_pa);
inline long hcall_set_memmap(unsigned long vmid,
		unsigned long memmap);
inline long hcall_set_memmaps(unsigned long pa_memmaps);
inline long hcall_write_protect_page(unsigned long vmid,
		unsigned long wp);
inline long hcall_set_ioreq_buffer(unsigned long vmid,
		unsigned long buffer);
inline long hcall_notify_req_finish(unsigned long vmid, unsigned long vcpu);
inline long hcall_assert_irqline(unsigned long vmid, unsigned long irq);
inline long hcall_deassert_irqline(unsigned long vmid, unsigned long irq);
inline long hcall_pulse_irqline(unsigned long vmid, unsigned long irq);
inline long hcall_inject_msi(unsigned long vmid, unsigned long msi);
inline long hcall_assign_ptdev(unsigned long vmid, unsigned long bdf);
inline long hcall_deassign_ptdev(unsigned long vmid, unsigned long bdf);
inline long hcall_set_ptdev_intr_info(unsigned long vmid,
		unsigned long pt_irq);
inline long hcall_reset_ptdev_intr_info(unsigned long vmid,
		unsigned long pt_irq);
inline long hcall_remap_pci_msix(unsigned long vmid, unsigned long msi);
inline long hcall_vm_gpa2hpa(unsigned long vmid, unsigned long addr);

#endif /* __VHM_HYPERCALL_H__ */
