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
#include <linux/vhm/cwp_hv_defs.h>
#include <linux/vhm/vhm_hypercall.h>

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
