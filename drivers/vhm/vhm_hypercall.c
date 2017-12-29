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
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/pci.h>
#include <linux/vhm/cwp_hv_defs.h>
#include <linux/vhm/vhm_hypercall.h>

/* max num of pass-through devices using msix */
#define MAX_ENTRY 3

struct table_iomems {
	/* device's virtual BDF */
	unsigned short virt_bdf;
	/* virtual base address of MSI-X table in memory space after ioremap */
	unsigned long mmap_addr;
} tables[MAX_ENTRY];

inline long hcall_inject_msi(unsigned long vmid, unsigned long msi)
{
	return cwp_hypercall2(HC_INJECT_MSI, vmid, msi);
}

inline long hcall_remap_pci_msix(unsigned long vmid, unsigned long msix)
{
	return cwp_hypercall2(HC_VM_PCI_MSIX_REMAP, vmid, msix);
}

inline long hcall_set_ioreq_buffer(unsigned long vmid, unsigned long buffer)
{
	return cwp_hypercall2(HC_SET_IOREQ_BUFFER, vmid, buffer);
}

inline long hcall_notify_req_finish(unsigned long vmid, unsigned long vcpu_mask)
{
	return cwp_hypercall2(HC_NOTIFY_REQUEST_FINISH,	vmid, vcpu_mask);
}

inline long hcall_set_memmap(unsigned long vmid, unsigned long memmap)
{
	return cwp_hypercall2(HC_VM_SET_MEMMAP, vmid, memmap);
}

inline long hcall_vm_gpa2hpa(unsigned long vmid, unsigned long gpa2hpa)
{
	return cwp_hypercall2(HC_VM_GPA2HPA, vmid, gpa2hpa);
}

inline long vhm_create_vm(struct vhm_vm *vm, unsigned long ioctl_param)
{
	long ret = 0;
	struct cwp_create_vm created_vm;

	if (copy_from_user(&created_vm, (void *)ioctl_param,
				sizeof(struct cwp_create_vm)))
		return -EFAULT;

	ret = cwp_hypercall2(HC_CREATE_VM, 0,
			virt_to_phys(&created_vm));
	if ((ret < 0) ||
			(created_vm.vmid == CWP_INVALID_VMID)) {
		pr_err("vhm: failed to create VM from Hypervisor !\n");
		return -EFAULT;
	}

	if (copy_to_user((void *)ioctl_param, &created_vm,
				sizeof(struct cwp_create_vm)))
		return -EFAULT;

	vm->vmid = created_vm.vmid;
	pr_info("vhm: VM %ld created\n", created_vm.vmid);

	return ret;
}

inline long vhm_resume_vm(struct vhm_vm *vm)
{
	long ret = 0;

	ret = cwp_hypercall1(HC_RESUME_VM, vm->vmid);
	if (ret < 0) {
		pr_err("vhm: failed to start VM %ld!\n", vm->vmid);
		return -EFAULT;
	}

	return ret;
}

inline long vhm_pause_vm(struct vhm_vm *vm)
{
	long ret = 0;

	ret = cwp_hypercall1(HC_PAUSE_VM, vm->vmid);
	if (ret < 0) {
		pr_err("vhm: failed to pause VM %ld!\n", vm->vmid);
		return -EFAULT;
	}

	return ret;
}

inline long vhm_destroy_vm(struct vhm_vm *vm)
{
	long ret = 0;

	ret = cwp_hypercall1(HC_DESTROY_VM, vm->vmid);
	if (ret < 0) {
		pr_err("failed to destroy VM %ld\n", vm->vmid);
		return -EFAULT;
	}
	vm->vmid = CWP_INVALID_VMID;

	return ret;
}

inline long vhm_query_vm_state(struct vhm_vm *vm)
{
	long ret = 0;

	ret = cwp_hypercall1(HC_QUERY_VMSTATE, vm->vmid);
	if (ret < 0) {
		pr_err("vhm: failed to query VM State%ld!\n", vm->vmid);
		return -EFAULT;
	}

	return ret;
}

inline long vhm_assert_irqline(struct vhm_vm *vm, unsigned long ioctl_param)
{
	long ret = 0;
	struct cwp_irqline irq;

	if (copy_from_user(&irq, (void *)ioctl_param, sizeof(irq)))
		return -EFAULT;

	ret = cwp_hypercall2(HC_ASSERT_IRQLINE, vm->vmid,
			virt_to_phys(&irq));
	if (ret < 0) {
		pr_err("vhm: failed to assert irq!\n");
		return -EFAULT;
	}

	return ret;
}

inline long vhm_deassert_irqline(struct vhm_vm *vm, unsigned long ioctl_param)
{
	long ret = 0;
	struct cwp_irqline irq;

	if (copy_from_user(&irq, (void *)ioctl_param, sizeof(irq)))
		return -EFAULT;

	ret = cwp_hypercall2(HC_DEASSERT_IRQLINE, vm->vmid,
			virt_to_phys(&irq));
	if (ret < 0) {
		pr_err("vhm: failed to deassert irq!\n");
		return -EFAULT;
	}

	return ret;
}

inline long vhm_pulse_irqline(struct vhm_vm *vm, unsigned long ioctl_param)
{
	long ret = 0;
	struct cwp_irqline irq;

	if (copy_from_user(&irq, (void *)ioctl_param, sizeof(irq)))
		return -EFAULT;

	ret = cwp_hypercall2(HC_PULSE_IRQLINE, vm->vmid,
			virt_to_phys(&irq));
	if (ret < 0) {
		pr_err("vhm: failed to assert irq!\n");
		return -EFAULT;
	}

	return ret;
}

inline long vhm_assign_ptdev(struct vhm_vm *vm, unsigned long ioctl_param)
{
	long ret = 0;
	uint16_t bdf;

	if (copy_from_user(&bdf,
				(void *)ioctl_param, sizeof(uint16_t)))
		return -EFAULT;

	ret = cwp_hypercall2(HC_ASSIGN_PTDEV, vm->vmid,
			virt_to_phys(&bdf));
	if (ret < 0) {
		pr_err("vhm: failed to assign ptdev!\n");
		return -EFAULT;
	}

	return ret;
}

inline long vhm_deassign_ptdev(struct vhm_vm *vm, unsigned long ioctl_param)
{
	long ret = 0;
	uint16_t bdf;

	if (copy_from_user(&bdf,
				(void *)ioctl_param, sizeof(uint16_t)))
		return -EFAULT;

	ret = cwp_hypercall2(HC_DEASSIGN_PTDEV, vm->vmid,
			virt_to_phys(&bdf));
	if (ret < 0) {
		pr_err("vhm: failed to deassign ptdev!\n");
		return -EFAULT;
	}

	return ret;
}

inline long vhm_set_ptdev_intr_info(struct vhm_vm *vm,
		unsigned long ioctl_param)
{
	long ret = 0;
	struct cwp_ptdev_irq pt_irq;
	int i;

	if (copy_from_user(&pt_irq,
				(void *)ioctl_param, sizeof(pt_irq)))
		return -EFAULT;

	ret = cwp_hypercall2(HC_SET_PTDEV_INTR_INFO, vm->vmid,
			virt_to_phys(&pt_irq));
	if (ret < 0) {
		pr_err("vhm: failed to set intr info for ptdev!\n");
		return -EFAULT;
	}

	if (pt_irq.msix.table_paddr) {
		for (i = 0; i < MAX_ENTRY; i++) {
			if (tables[i].virt_bdf)
				continue;

			tables[i].virt_bdf = pt_irq.virt_bdf;
			tables[i].mmap_addr = (unsigned long)
				ioremap_nocache(pt_irq.msix.table_paddr,
						pt_irq.msix.table_size);
			break;
		}
	}

	return ret;
}

inline long vhm_reset_ptdev_intr_info(struct vhm_vm *vm,
		unsigned long ioctl_param)
{
	long ret = 0;
	struct cwp_ptdev_irq pt_irq;
	int i;

	if (copy_from_user(&pt_irq,
				(void *)ioctl_param, sizeof(pt_irq)))
		return -EFAULT;

	ret = cwp_hypercall2(HC_RESET_PTDEV_INTR_INFO, vm->vmid,
			virt_to_phys(&pt_irq));
	if (ret < 0) {
		pr_err("vhm: failed to reset intr info for ptdev!\n");
		return -EFAULT;
	}

	if (pt_irq.msix.table_paddr) {
		for (i = 0; i < MAX_ENTRY; i++) {
			if (tables[i].virt_bdf)
				continue;

			tables[i].virt_bdf = pt_irq.virt_bdf;
			tables[i].mmap_addr = (unsigned long)
				ioremap_nocache(pt_irq.msix.table_paddr,
						pt_irq.msix.table_size);
			break;
		}
	}

	return ret;
}

inline long vhm_remap_pci_msix(struct vhm_vm *vm, unsigned long ioctl_param)
{
	long ret = 0;
	struct cwp_vm_pci_msix_remap msix_remap;

	if (copy_from_user(&msix_remap,
				(void *)ioctl_param, sizeof(msix_remap)))
		return -EFAULT;

	ret = cwp_hypercall2(HC_VM_PCI_MSIX_REMAP, vm->vmid,
			virt_to_phys(&msix_remap));

	if (copy_to_user((void *)ioctl_param,
				&msix_remap, sizeof(msix_remap)))
		return -EFAULT;

	if (msix_remap.msix) {
		void __iomem *msix_entry;
		int i;

		for (i = 0; i < MAX_ENTRY; i++) {
			if (tables[i].virt_bdf == msix_remap.virt_bdf)
				break;
		}

		if (!tables[i].mmap_addr)
			return -EFAULT;

		msix_entry = (void *)(tables[i].mmap_addr +
			msix_remap.msix_entry_index *
			PCI_MSIX_ENTRY_SIZE);

		/* mask the entry when setup */
		writel(PCI_MSIX_ENTRY_CTRL_MASKBIT,
				msix_entry + PCI_MSIX_ENTRY_VECTOR_CTRL);

		/* setup the msi entry */
		writel((uint32_t)msix_remap.msi_addr,
				msix_entry + PCI_MSIX_ENTRY_LOWER_ADDR);
		writel((uint32_t)(msix_remap.msi_addr >> 32),
				msix_entry + PCI_MSIX_ENTRY_UPPER_ADDR);
		writel(msix_remap.msi_data,
				msix_entry + PCI_MSIX_ENTRY_DATA);

		/* unmask the entry */
		writel(msix_remap.vector_ctl &
				PCI_MSIX_ENTRY_CTRL_MASKBIT,
				msix_entry + PCI_MSIX_ENTRY_VECTOR_CTRL);
	}

	return ret;
}
