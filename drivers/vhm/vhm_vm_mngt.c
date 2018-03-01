/*
 * virtio and hyperviosr service module (VHM): vm management
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
 * Liang Ding <liang.ding@intel.com>
 * Jason Zeng <jason.zeng@intel.com>
 *
 */

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <asm/processor.h>
#include <linux/vhm/acrn_hv_defs.h>
#include <linux/vhm/vhm_ioctl_defs.h>
#include <linux/vhm/acrn_vhm_ioreq.h>
#include <linux/vhm/acrn_vhm_mm.h>
#include <linux/vhm/vhm_hypercall.h>

LIST_HEAD(vhm_vm_list);
DEFINE_MUTEX(vhm_vm_list_lock);

struct vhm_vm *find_get_vm(unsigned long vmid)
{
	struct vhm_vm *vm;

	mutex_lock(&vhm_vm_list_lock);
	list_for_each_entry(vm, &vhm_vm_list, list) {
		if (vm->vmid == vmid) {
			vm->refcnt++;
			mutex_unlock(&vhm_vm_list_lock);
			return vm;
		}
	}
	mutex_unlock(&vhm_vm_list_lock);
	return NULL;
}

void put_vm(struct vhm_vm *vm)
{
	mutex_lock(&vhm_vm_list_lock);
	vm->refcnt--;
	if (vm->refcnt == 0) {
		list_del(&vm->list);
		free_guest_mem(vm);
		acrn_ioreq_free(vm);
		kfree(vm);
		pr_info("vhm: freed vm\n");
	}
	mutex_unlock(&vhm_vm_list_lock);
}

int vhm_get_vm_info(unsigned long vmid, struct vm_info *info)
{
	struct vhm_vm *vm;

	vm = find_get_vm(vmid);
	if (unlikely(vm == NULL)) {
		pr_err("vhm: failed to find vm from vmid %ld\n",
			vmid);
		return -EINVAL;
	}
	/*TODO: hardcode max_vcpu here, should be fixed by getting at runtime */
	info->max_vcpu = 4;
	info->max_gfn = vm->max_gfn;
	put_vm(vm);
	return 0;
}

int vhm_inject_msi(unsigned long vmid, unsigned long msi_addr,
		unsigned long msi_data)
{
	struct acrn_msi_entry msi;
	int ret;

	/* msi_addr: addr[19:12] with dest vcpu id */
	/* msi_data: data[7:0] with vector */
	msi.msi_addr = msi_addr;
	msi.msi_data = msi_data;
	ret = hcall_inject_msi(vmid, virt_to_phys(&msi));
	if (ret < 0) {
		pr_err("vhm: failed to inject!\n");
		return -EFAULT;
	}
	return 0;
}

unsigned long vhm_vm_gpa2hpa(unsigned long vmid, unsigned long gpa)
{
	struct vm_gpa2hpa gpa2hpa;
	int ret;

	gpa2hpa.gpa = gpa;
	gpa2hpa.hpa = -1UL; /* Init value as invalid gpa */
	ret = hcall_vm_gpa2hpa(vmid, virt_to_phys(&gpa2hpa));
	if (ret < 0) {
		pr_err("vhm: failed to inject!\n");
		return -EFAULT;
	}
	mb();
	return gpa2hpa.hpa;
}

void vm_list_add(struct list_head *list)
{
	list_add(list, &vhm_vm_list);
}

void vm_mutex_lock(struct mutex *mlock)
{
	mutex_lock(mlock);
}

void vm_mutex_unlock(struct mutex *mlock)
{
	mutex_unlock(mlock);
}
