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
#ifndef VHM_VM_MNGT_H
#define VHM_VM_MNGT_H

#include <linux/list.h>

extern struct list_head vhm_vm_list;
extern struct mutex vhm_vm_list_lock;

struct vhm_vm {
	struct device *dev;
	struct list_head list;
	unsigned long vmid;
	int ioreq_fallback_client;
	long refcnt;
	struct mutex seg_lock;
	struct list_head memseg_list;
	spinlock_t ioreq_client_lock;
	struct list_head ioreq_client_list;
	struct vhm_request_buffer *req_buf;
	struct page *pg;
};

struct vhm_vm *find_get_vm(unsigned long vmid);
void put_vm(struct vhm_vm *vm);
int vhm_inject_msi(unsigned long vmid, unsigned long msi_addr,
	unsigned long msi_data);

void vm_list_add(struct list_head *list);
void vm_mutex_lock(struct mutex *mlock);
void vm_mutex_unlock(struct mutex *mlock);

#endif
