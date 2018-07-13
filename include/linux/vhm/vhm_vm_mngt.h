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
 * Xiao Zheng <xiao.zheng@intel.com>
 * Jason Chen CJ <jason.cj.chen@intel.com>
 *
 */

/**
 * DOC: brief Virtio and Hypervisor Module(VHM) management APIs
 */

#ifndef __VHM_VM_MNGT_H__
#define __VHM_VM_MNGT_H__

#include <linux/list.h>

extern struct list_head vhm_vm_list;
extern struct mutex vhm_vm_list_lock;

#define HUGEPAGE_2M_HLIST_ARRAY_SIZE	16
#define HUGEPAGE_1G_HLIST_ARRAY_SIZE	1
#define HUGEPAGE_HLIST_ARRAY_SIZE	(HUGEPAGE_2M_HLIST_ARRAY_SIZE + \
						HUGEPAGE_1G_HLIST_ARRAY_SIZE)
/**
 * struct vhm_vm - data structure to track guest
 *
 * @dev: pointer to dev of linux device mode
 * @list: list of vhm_vm
 * @vmid: guest vmid
 * @trusty_host_gpa: host physical address of continuous memory for Trusty
 * @ioreq_fallback_client: default ioreq client
 * @refcnt: reference count of guest
 * @hugepage_lock:  mutex to protect hugepage_hlist
 * @hugepage_hlist: hash list of hugepage
 * @max_gfn: maximum guest page frame number
 * @ioreq_client_lock: spinlock to protect ioreq_client_list
 * @ioreq_client_list: list of ioreq clients
 * @req_buf: request buffer shared between HV, SOS and UOS
 * @pg: pointer to linux page which holds req_buf
 * @hugetlb_enabled: flag to enable/disable hugetlb page ept mapping
 */
struct vhm_vm {
	struct device *dev;
	struct list_head list;
	unsigned long vmid;
	unsigned long trusty_host_gpa;
	int ioreq_fallback_client;
	long refcnt;
	struct mutex hugepage_lock;
	struct hlist_head hugepage_hlist[HUGEPAGE_HLIST_ARRAY_SIZE];
	int max_gfn;
	spinlock_t ioreq_client_lock;
	struct list_head ioreq_client_list;
	struct vhm_request_buffer *req_buf;
	struct page *pg;
	int hugetlb_enabled;
};

/**
 * struct vm_info - data structure to track guest info
 *
 * @max_vcpu: maximum vcpu number of guest
 * @max_gfn: maximum guest page frame number
 */
struct vm_info {
	int max_vcpu;
	int max_gfn;
};

/**
 * find_get_vm() - find and keep guest vhm_vm based on the vmid
 *
 * @vmid: guest vmid
 *
 * Return: pointer to vhm_vm, NULL if can't find vm matching vmid
 */
struct vhm_vm *find_get_vm(unsigned long vmid);

/**
 * put_vm() - release vhm_vm of guest according to guest vmid
 * If the latest reference count drops to zero, free vhm_vm as well
 * @vm: pointer to vhm_vm which identify specific guest
 *
 * Return:
 */
void put_vm(struct vhm_vm *vm);

/**
 * vhm_get_vm_info() - get vm_info of specific guest
 *
 * @vmid: guest vmid
 * @info: pointer to vm_info for returned vm_info
 *
 * Return: 0 on success, <0 on error
 */
int vhm_get_vm_info(unsigned long vmid, struct vm_info *info);

/**
 * vhm_inject_msi() - inject MSI interrupt to guest
 *
 * @vmid: guest vmid
 * @msi_addr: MSI addr matches MSI spec
 * @msi_data: MSI data matches MSI spec
 *
 * Return: 0 on success, <0 on error
 */
int vhm_inject_msi(unsigned long vmid, unsigned long msi_addr,
	unsigned long msi_data);

/**
 * vhm_vm_gpa2hpa() - convert guest physical address to
 * host physical address
 *
 * @vmid: guest vmid
 * @gpa: guest physical address
 *
 * Return: host physical address, <0 on error
 */
unsigned long vhm_vm_gpa2hpa(unsigned long vmid, unsigned long gpa);

void vm_list_add(struct list_head *list);
void vm_mutex_lock(struct mutex *mlock);
void vm_mutex_unlock(struct mutex *mlock);

#endif
