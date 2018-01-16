/*
 * virtio and hyperviosr service module (VHM): ioreq multi client feature
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
 * Jason Chen CJ <jason.cj.chen@intel.com>
 *
 */

#ifndef __CWP_VHM_IOREQ_H__
#define __CWP_VHM_IOREQ_H__

#include <linux/poll.h>
#include <linux/vhm/vhm_vm_mngt.h>

typedef	int (*ioreq_handler_t)(int client_id, int req);

int cwp_ioreq_create_client(unsigned long vmid, ioreq_handler_t handler,
	char *name);
void cwp_ioreq_destroy_client(int client_id);

int cwp_ioreq_add_iorange(int client_id, uint32_t type,
	long start, long end);
int cwp_ioreq_del_iorange(int client_id, uint32_t type,
	long start, long end);

struct vhm_request *cwp_ioreq_get_reqbuf(int client_id);
int cwp_ioreq_attach_client(int client_id, bool check_kthread_stop);

int cwp_ioreq_distribute_request(struct vhm_vm *vm);
int cwp_ioreq_complete_request(int client_id, uint64_t vcpu);

void cwp_ioreq_intercept_bdf(int client_id, int bus, int dev, int func);
void cwp_ioreq_unintercept_bdf(int client_id);

/* IOReq APIs */
int cwp_ioreq_init(struct vhm_vm *vm, unsigned long vma);
void cwp_ioreq_free(struct vhm_vm *vm);
int cwp_ioreq_create_fallback_client(unsigned long vmid, char *name);
unsigned int vhm_dev_poll(struct file *filep, poll_table *wait);

#endif
