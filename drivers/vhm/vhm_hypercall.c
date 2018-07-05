/*
 * virtio and hyperviosr service module (VHM): hypercall wrap
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
#include <linux/types.h>
#include <linux/vhm/acrn_hv_defs.h>
#include <linux/vhm/vhm_hypercall.h>

inline long hcall_get_api_version(unsigned long api_version)
{
	return acrn_hypercall1(HC_GET_API_VERSION, api_version);
}

inline long hcall_create_vm(unsigned long vminfo)
{
	return acrn_hypercall1(HC_CREATE_VM, vminfo);
}

inline long hcall_start_vm(unsigned long vmid)
{
	return  acrn_hypercall1(HC_START_VM, vmid);
}

inline long hcall_pause_vm(unsigned long vmid)
{
	return acrn_hypercall1(HC_PAUSE_VM, vmid);
}

inline long hcall_restart_vm(unsigned long vmid)
{
	return acrn_hypercall1(HC_RESTART_VM, vmid);
}

inline long hcall_destroy_vm(unsigned long vmid)
{
	return  acrn_hypercall1(HC_DESTROY_VM, vmid);
}

inline long hcall_setup_sbuf(unsigned long sbuf_head)
{
	return acrn_hypercall1(HC_SETUP_SBUF, sbuf_head);
}

inline long hcall_set_sstate_data(unsigned long sx_data_addr)
{
	return acrn_hypercall1(HC_PM_SET_SSTATE_DATA, sx_data_addr);
}

inline long hcall_get_cpu_state(unsigned long cmd, unsigned long state_pa)
{
	return acrn_hypercall2(HC_PM_GET_CPU_STATE, cmd, state_pa);
}

inline long hcall_set_memmap(unsigned long vmid, unsigned long memmap)
{
	return acrn_hypercall2(HC_VM_SET_MEMMAP, vmid, memmap);
}

inline long hcall_set_memmaps(unsigned long pa_memmaps)
{
	return acrn_hypercall1(HC_VM_SET_MEMMAPS, pa_memmaps);
}

inline long hcall_set_ioreq_buffer(unsigned long vmid, unsigned long buffer)
{
	return acrn_hypercall2(HC_SET_IOREQ_BUFFER, vmid, buffer);
}

inline long hcall_notify_req_finish(unsigned long vmid, unsigned long vcpu)
{
	return acrn_hypercall2(HC_NOTIFY_REQUEST_FINISH, vmid, vcpu);
}

inline long hcall_assert_irqline(unsigned long vmid, unsigned long irq)
{
	return acrn_hypercall2(HC_ASSERT_IRQLINE, vmid, irq);
}

inline long hcall_deassert_irqline(unsigned long vmid, unsigned long irq)
{
	return acrn_hypercall2(HC_DEASSERT_IRQLINE, vmid, irq);
}

inline long hcall_pulse_irqline(unsigned long vmid, unsigned long irq)
{
	return acrn_hypercall2(HC_PULSE_IRQLINE, vmid, irq);
}

inline long hcall_inject_msi(unsigned long vmid, unsigned long msi)
{
	return acrn_hypercall2(HC_INJECT_MSI, vmid, msi);
}

inline long hcall_assign_ptdev(unsigned long vmid, unsigned long bdf)
{
	return acrn_hypercall2(HC_ASSIGN_PTDEV, vmid, bdf);
}

inline long hcall_deassign_ptdev(unsigned long vmid, unsigned long bdf)
{
	return acrn_hypercall2(HC_DEASSIGN_PTDEV, vmid, bdf);
}

inline long hcall_set_ptdev_intr_info(unsigned long vmid, unsigned long pt_irq)
{
	return acrn_hypercall2(HC_SET_PTDEV_INTR_INFO, vmid, pt_irq);
}

inline long hcall_reset_ptdev_intr_info(unsigned long vmid,
		unsigned long pt_irq)
{
	return acrn_hypercall2(HC_RESET_PTDEV_INTR_INFO, vmid, pt_irq);
}

inline long hcall_remap_pci_msix(unsigned long vmid, unsigned long msi)
{
	return  acrn_hypercall2(HC_VM_PCI_MSIX_REMAP, vmid, msi);
}

inline long hcall_vm_gpa2hpa(unsigned long vmid, unsigned long addr)
{
	return  acrn_hypercall2(HC_VM_GPA2HPA, vmid, addr);
}
