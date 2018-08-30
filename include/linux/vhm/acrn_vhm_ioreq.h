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

/**
 * DOC: Virtio and Hypervisor Module(VHM) ioreq APIs
 */

#ifndef __ACRN_VHM_IOREQ_H__
#define __ACRN_VHM_IOREQ_H__

#include <linux/poll.h>
#include <linux/vhm/vhm_vm_mngt.h>

typedef	int (*ioreq_handler_t)(int client_id, unsigned long *ioreqs_map);

/**
 * acrn_ioreq_create_client - create ioreq client
 *
 * @vmid: ID to identify guest
 * @handler: ioreq_handler of ioreq client
 *           If client want request handled in client thread context, set
 *           this parameter to NULL. If client want request handled out of
 *           client thread context, set handler function pointer of its own.
 *           VHM will create kernel thread and call handler to handle request
 *
 * @name: the name of ioreq client
 *
 * Return: client id on success, <0 on error
 */
int acrn_ioreq_create_client(unsigned long vmid, ioreq_handler_t handler,
	char *name);

/**
 * acrn_ioreq_destroy_client - destroy ioreq client
 *
 * @client_id: client id to identify ioreq client
 *
 * Return:
 */
void acrn_ioreq_destroy_client(int client_id);

/**
 * acrn_ioreq_add_iorange - add iorange monitored by ioreq client
 *
 * @client_id: client id to identify ioreq client
 * @type: iorange type
 * @start: iorange start address
 * @end: iorange end address
 *
 * Return: 0 on success, <0 on error
 */
int acrn_ioreq_add_iorange(int client_id, uint32_t type,
	long start, long end);

/**
 * acrn_ioreq_del_iorange - del iorange monitored by ioreq client
 *
 * @client_id: client id to identify ioreq client
 * @type: iorange type
 * @start: iorange start address
 * @end: iorange end address
 *
 * Return: 0 on success, <0 on error
 */
int acrn_ioreq_del_iorange(int client_id, uint32_t type,
	long start, long end);

/**
 * acrn_ioreq_get_reqbuf - get request buffer
 * request buffer is shared by all clients in one guest
 *
 * @client_id: client id to identify ioreq client
 *
 * Return: pointer to request buffer, NULL on error
 */
struct vhm_request *acrn_ioreq_get_reqbuf(int client_id);

/**
 * acrn_ioreq_attach_client - start handle request for ioreq client
 * If request is handled out of client thread context, this function is
 * only called once to be ready to handle new request.
 *
 * If request is handled in client thread context, this function must
 * be called every time after the previous request handling is completed
 * to be ready to handle new request.
 *
 * @client_id: client id to identify ioreq client
 * @check_kthread_stop: whether check current kthread should be stopped
 *
 * Return: 0 on success, <0 on error, 1 if ioreq client is destroying
 */
int acrn_ioreq_attach_client(int client_id, bool check_kthread_stop);

/**
 * acrn_ioreq_distribute_request - deliver request to corresponding client
 *
 * @vm: pointer to guest
 *
 * Return: 0 always
 */
int acrn_ioreq_distribute_request(struct vhm_vm *vm);

/**
 * acrn_ioreq_complete_request - notify guest request handling is completed
 *
 * @client_id: client id to identify ioreq client
 * @vcpu: identify request submitter
 *
 * Return: 0 on success, <0 on error
 */
int acrn_ioreq_complete_request(int client_id, uint64_t vcpu);

/**
 * acrn_ioreq_intercept_bdf - set intercept bdf info of ioreq client
 *
 * @client_id: client id to identify ioreq client
 * @bus: bus number
 * @dev: device number
 * @func: function number
 *
 * Return:
 */
void acrn_ioreq_intercept_bdf(int client_id, int bus, int dev, int func);

/**
 * acrn_ioreq_unintercept_bdf - clear intercept bdf info of ioreq client
 *
 * @client_id: client id to identify ioreq client
 *
 * Return:
 */
void acrn_ioreq_unintercept_bdf(int client_id);

/* IOReq APIs */
int acrn_ioreq_init(struct vhm_vm *vm, unsigned long vma);
void acrn_ioreq_free(struct vhm_vm *vm);
int acrn_ioreq_create_fallback_client(unsigned long vmid, char *name);
unsigned int vhm_dev_poll(struct file *filep, poll_table *wait);

#endif
