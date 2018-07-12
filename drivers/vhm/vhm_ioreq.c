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

#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/freezer.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/vhm/acrn_common.h>
#include <linux/vhm/acrn_vhm_ioreq.h>
#include <linux/vhm/vhm_vm_mngt.h>
#include <linux/vhm/vhm_hypercall.h>

struct ioreq_range {
	struct list_head list;
	uint32_t type;
	long start;
	long end;
};

struct ioreq_client {
	/* client name */
	char name[16];
	/* client id */
	int id;
	/* vm this client belongs to */
	unsigned long vmid;
	/* list node for this ioreq_client */
	struct list_head list;
	/*
	 * is this client fallback?
	 * there is only one fallback client in a vm - dm
	 * a fallback client shares IOReq buffer pages
	 * a fallback client handles all left IOReq not handled by other clients
	 * a fallback client does not need add io ranges
	 * a fallback client handles ioreq in its own context
	 */
	bool fallback;

	volatile bool destroying;
	volatile bool kthread_exit;

	/* client covered io ranges - N/A for fallback client */
	struct list_head range_list;
	spinlock_t range_lock;

	/*
	 *   this req records the req number this client need handle
	 */
	DECLARE_BITMAP(ioreqs_map, VHM_REQUEST_MAX);

	/*
	 * client ioreq handler:
	 *   if client provides a handler, it means vhm need create a kthread
	 *   to call the handler while there is ioreq.
	 *   if client doesn't provide a handler, client should handle ioreq
	 *   in its own context when calls acrn_ioreq_attach_client.
	 *
	 *   NOTE: for fallback client, there is no ioreq handler.
	 */
	ioreq_handler_t handler;
	bool vhm_create_kthread;
	struct task_struct *thread;
	wait_queue_head_t wq;

	/* pci bdf trap */
	bool trap_bdf;
	int pci_bus;
	int pci_dev;
	int pci_func;
};

#define MAX_CLIENT 64
static struct ioreq_client *clients[MAX_CLIENT];
static DECLARE_BITMAP(client_bitmap, MAX_CLIENT);

static void acrn_ioreq_notify_client(struct ioreq_client *client);

static inline bool is_range_type(uint32_t type)
{
	return (type == REQ_MMIO || type == REQ_PORTIO || type == REQ_WP);
}

static int alloc_client(void)
{
	struct ioreq_client *client;
	int i;

	i = find_first_zero_bit(client_bitmap, MAX_CLIENT);
	if (i >= MAX_CLIENT)
		return -ENOMEM;
	set_bit(i, client_bitmap);

	client = kzalloc(sizeof(struct ioreq_client), GFP_KERNEL);
	if (!client)
		return -ENOMEM;
	client->id = i;
	clients[i] = client;

	return i;
}

static void free_client(int i)
{
	if (i < MAX_CLIENT && i >= 0) {
		if (test_and_clear_bit(i, client_bitmap)) {
			kfree(clients[i]);
			clients[i] = NULL;
		}
	}
}

int acrn_ioreq_create_client(unsigned long vmid, ioreq_handler_t handler,
	char *name)
{
	struct vhm_vm *vm;
	struct ioreq_client *client;
	unsigned long flags;
	int client_id;

	might_sleep();

	vm = find_get_vm(vmid);
	if (unlikely(vm == NULL)) {
		pr_err("vhm-ioreq: failed to find vm from vmid %ld\n",
			vmid);
		return -EINVAL;
	}
	if (unlikely(vm->req_buf == NULL)) {
		pr_err("vhm-ioreq: vm[%ld]'s reqbuf is not ready\n",
			vmid);
		put_vm(vm);
		return -EINVAL;
	}

	client_id = alloc_client();
	if (unlikely(client_id < 0)) {
		pr_err("vhm-ioreq: vm[%ld] failed to alloc ioreq "
			"client id\n", vmid);
		put_vm(vm);
		return -EINVAL;
	}

	client = clients[client_id];

	if (handler) {
		client->handler = handler;
		client->vhm_create_kthread = true;
	}

	client->vmid = vmid;
	if (name)
		strncpy(client->name, name, sizeof(client->name) - 1);
	spin_lock_init(&client->range_lock);
	INIT_LIST_HEAD(&client->range_list);
	init_waitqueue_head(&client->wq);

	spin_lock_irqsave(&vm->ioreq_client_lock, flags);
	list_add(&client->list, &vm->ioreq_client_list);
	spin_unlock_irqrestore(&vm->ioreq_client_lock, flags);

	put_vm(vm);

	pr_info("vhm-ioreq: created ioreq client %d\n", client_id);

	return client_id;
}

int acrn_ioreq_create_fallback_client(unsigned long vmid, char *name)
{
	struct vhm_vm *vm;
	int client_id;

	vm = find_get_vm(vmid);
	if (unlikely(vm == NULL)) {
		pr_err("vhm-ioreq: failed to find vm from vmid %ld\n",
			vmid);
		return -EINVAL;
	}

	if (unlikely(vm->ioreq_fallback_client > 0)) {
		pr_err("vhm-ioreq: there is already fallback "
				"client exist for vm %ld\n",
				vmid);
		put_vm(vm);
		return -EINVAL;
	}

	client_id = acrn_ioreq_create_client(vmid, NULL, name);
	if (unlikely(client_id < 0)) {
		put_vm(vm);
		return -EINVAL;
	}

	clients[client_id]->fallback = true;
	vm->ioreq_fallback_client = client_id;

	put_vm(vm);

	return client_id;
}

static void acrn_ioreq_destroy_client_pervm(struct ioreq_client *client,
		struct vhm_vm *vm)
{
	struct list_head *pos, *tmp;
	unsigned long flags;

	client->destroying = true;
	acrn_ioreq_notify_client(client);

	/* the client thread will mark kthread_exit flag as true before exit,
	 * so wait for it exited.
	 */
	while (!client->kthread_exit)
		msleep(10);

	spin_lock_irqsave(&client->range_lock, flags);
	list_for_each_safe(pos, tmp, &client->range_list) {
		struct ioreq_range *range =
			container_of(pos, struct ioreq_range, list);
		list_del(&range->list);
		kfree(range);
	}
	spin_unlock_irqrestore(&client->range_lock, flags);

	spin_lock_irqsave(&vm->ioreq_client_lock, flags);
	list_del(&client->list);
	spin_unlock_irqrestore(&vm->ioreq_client_lock, flags);
	free_client(client->id);

	if (client->id == vm->ioreq_fallback_client)
		vm->ioreq_fallback_client = -1;
}

void acrn_ioreq_destroy_client(int client_id)
{
	struct vhm_vm *vm;
	struct ioreq_client *client;

	if (client_id < 0 || client_id >= MAX_CLIENT) {
		pr_err("vhm-ioreq: no client for id %d\n", client_id);
		return;
	}
	client = clients[client_id];
	if (!client) {
		pr_err("vhm-ioreq: no client for id %d\n", client_id);
		return;
	}

	might_sleep();

	vm = find_get_vm(client->vmid);
	if (unlikely(vm == NULL)) {
		pr_err("vhm-ioreq: failed to find vm from vmid %ld\n",
			client->vmid);
		return;
	}

	acrn_ioreq_destroy_client_pervm(client, vm);

	put_vm(vm);
}

static void __attribute__((unused)) dump_iorange(struct ioreq_client *client)
{
	struct list_head *pos;
	unsigned long flags;

	spin_lock_irqsave(&client->range_lock, flags);
	list_for_each(pos, &client->range_list) {
		struct ioreq_range *range =
			container_of(pos, struct ioreq_range, list);
		pr_debug("\tio range: type %d, start 0x%lx, "
			"end 0x%lx\n", range->type, range->start, range->end);
	}
	spin_unlock_irqrestore(&client->range_lock, flags);
}

/*
 * NOTE: here just add iorange entry directly, no check for the overlap..
 * please client take care of it
 */
int acrn_ioreq_add_iorange(int client_id, uint32_t type,
	long start, long end)
{
	struct ioreq_client *client;
	struct ioreq_range *range;
	unsigned long flags;

	if (client_id < 0 || client_id >= MAX_CLIENT) {
		pr_err("vhm-ioreq: no client for id %d\n", client_id);
		return -EFAULT;
	}
	client = clients[client_id];
	if (!client) {
		pr_err("vhm-ioreq: no client for id %d\n", client_id);
		return -EFAULT;
	}

	if (end < start) {
		pr_err("vhm-ioreq: end < start\n");
		return -EFAULT;
	}

	might_sleep();

	range = kzalloc(sizeof(struct ioreq_range), GFP_KERNEL);
	if (!range) {
		pr_err("vhm-ioreq: failed to alloc ioreq range\n");
		return -ENOMEM;
	}
	range->type = type;
	range->start = start;
	range->end = end;

	spin_lock_irqsave(&client->range_lock, flags);
	list_add(&range->list, &client->range_list);
	spin_unlock_irqrestore(&client->range_lock, flags);

	return 0;
}

int acrn_ioreq_del_iorange(int client_id, uint32_t type,
	long start, long end)
{
	struct ioreq_client *client;
	struct ioreq_range *range;
	struct list_head *pos, *tmp;
	unsigned long flags;

	if (client_id < 0 || client_id >= MAX_CLIENT) {
		pr_err("vhm-ioreq: no client for id %d\n", client_id);
		return -EFAULT;
	}
	client = clients[client_id];
	if (!client) {
		pr_err("vhm-ioreq: no client for id %d\n", client_id);
		return -EFAULT;
	}

	if (end < start) {
		pr_err("vhm-ioreq: end < start\n");
		return -EFAULT;
	}

	might_sleep();

	spin_lock_irqsave(&client->range_lock, flags);
	list_for_each_safe(pos, tmp, &client->range_list) {
		range = container_of(pos, struct ioreq_range, list);
		if (range->type == type) {
			if (is_range_type(type)) {
				if (start == range->start &&
					end == range->end) {
					list_del(&range->list);
					kfree(range);
					break;
				}
			} else {
				list_del(&range->list);
				kfree(range);
				break;
			}
		}
	}
	spin_unlock_irqrestore(&client->range_lock, flags);

	return 0;
}

static inline bool is_destroying(struct ioreq_client *client)
{
	if (client)
		return client->destroying;
	else
		return true;
}

static inline bool has_pending_request(struct ioreq_client *client)
{
	if (client)
		return !bitmap_empty(client->ioreqs_map, VHM_REQUEST_MAX);
	else
		return false;
}

struct vhm_request *acrn_ioreq_get_reqbuf(int client_id)
{
	struct ioreq_client *client;
	struct vhm_vm *vm;

	if (client_id < 0 || client_id >= MAX_CLIENT) {
		pr_err("vhm-ioreq: no client for id %d\n", client_id);
		return NULL;
	}
	client = clients[client_id];
	if (!client) {
		pr_err("vhm-ioreq: no client for id %d\n", client_id);
		return NULL;
	}
	vm = find_get_vm(client->vmid);
	if (unlikely(vm == NULL)) {
		pr_err("vhm-ioreq: failed to find vm from vmid %ld\n",
			client->vmid);
		return NULL;
	}

	if (vm->req_buf == NULL) {
		pr_warn("vhm-ioreq: the req buf page not ready yet "
			"for vmid %ld\n", client->vmid);
	}
	put_vm(vm);
	return (struct vhm_request *)vm->req_buf;
}

static int ioreq_client_thread(void *data)
{
	struct ioreq_client *client;
	int ret, client_id = (unsigned long)data;

	while (1) {
		client = clients[client_id];
		if (is_destroying(client)) {
			pr_info("vhm-ioreq: client destroying->stop thread\n");
			break;
		}
		if (has_pending_request(client)) {
			if (client->handler) {
				ret = client->handler(client->id,
					client->ioreqs_map);
				if (ret < 0)
					BUG();
			} else {
				pr_err("vhm-ioreq: no ioreq handler\n");
				break;
			}
		} else
			wait_event_freezable(client->wq,
				(has_pending_request(client) ||
				is_destroying(client)));
	}

	/* the client thread such as for hyper-dma will exit from here,
	 * so mark kthread_exit as true before exit */
	client->kthread_exit = true;

	return 0;
}

int acrn_ioreq_attach_client(int client_id, bool check_kthread_stop)
{
	struct ioreq_client *client;

	if (client_id < 0 || client_id >= MAX_CLIENT) {
		pr_err("vhm-ioreq: no client for id %d\n", client_id);
		return -EFAULT;
	}
	client = clients[client_id];
	if (!client) {
		pr_err("vhm-ioreq: no client for id %d\n", client_id);
		return -EFAULT;
	}

	if (client->vhm_create_kthread) {
		if (client->thread) {
			pr_warn("vhm-ioreq: kthread already exist"
					" for client %s\n", client->name);
			return 0;
		}
		client->thread = kthread_run(ioreq_client_thread,
				(void *)(unsigned long)client_id,
				"ioreq_client[%ld]:%s",
				client->vmid, client->name);
		if (IS_ERR(client->thread)) {
			pr_err("vhm-ioreq: failed to run kthread "
					"for client %s\n", client->name);
			return -ENOMEM;
		}
	} else {
		might_sleep();

		if (check_kthread_stop) {
			wait_event_freezable(client->wq,
				(kthread_should_stop() ||
				has_pending_request(client) ||
				is_destroying(client)));
			if (kthread_should_stop())
				client->kthread_exit = true;
		} else {
			wait_event_freezable(client->wq,
				(has_pending_request(client) ||
				is_destroying(client)));
		}

		if (is_destroying(client)) {
			/* the client thread for vcpu will exit from here,
			 * so mark kthread_exit as true before exit */
			client->kthread_exit = true;
			return 1;
		}
	}

	return 0;
}

void acrn_ioreq_intercept_bdf(int client_id, int bus, int dev, int func)
{
	struct ioreq_client *client;

	if (client_id < 0 || client_id >= MAX_CLIENT) {
		pr_err("vhm-ioreq: no client for id %d\n", client_id);
		return;
	}
	client = clients[client_id];
	if (!client) {
		pr_err("vhm-ioreq: no client for id %d\n", client_id);
		return;
	}
	client->trap_bdf = true;
	client->pci_bus = bus;
	client->pci_dev = dev;
	client->pci_func = func;
}

void acrn_ioreq_unintercept_bdf(int client_id)
{
	struct ioreq_client *client;

	if (client_id < 0 || client_id >= MAX_CLIENT) {
		pr_err("vhm-ioreq: no client for id %d\n", client_id);
		return;
	}
	client = clients[client_id];
	if (!client) {
		pr_err("vhm-ioreq: no client for id %d\n", client_id);
		return;
	}
	client->trap_bdf = false;
	client->pci_bus = -1;
	client->pci_dev = -1;
	client->pci_func = -1;
}

static void acrn_ioreq_notify_client(struct ioreq_client *client)
{
	/* if client thread is in waitqueue, wake up it */
	if (waitqueue_active(&client->wq))
		wake_up_interruptible(&client->wq);
}

static bool req_in_range(struct ioreq_range *range, struct vhm_request *req)
{
	bool ret = false;

	if (range->type == req->type) {
		switch (req->type) {
		case REQ_MMIO:
		case REQ_WP:
		{
			if (req->reqs.mmio_request.address >= range->start &&
				(req->reqs.mmio_request.address +
				req->reqs.mmio_request.size - 1) <= range->end)
				ret = true;
			break;
		}
		case REQ_PORTIO: {
			if (req->reqs.pio_request.address >= range->start &&
				(req->reqs.pio_request.address +
				req->reqs.pio_request.size - 1) <= range->end)
				ret = true;
			break;
		}

		default:
			ret = false;
			break;
		}
	}

	return ret;
}

static bool is_cfg_addr(struct vhm_request *req)
{
	return (req->type == REQ_PORTIO &&
		(req->reqs.pio_request.address >= 0xcf8 &&
		req->reqs.pio_request.address < 0xcf8+4));
}

static bool is_cfg_data(struct vhm_request *req)
{
	return (req->type == REQ_PORTIO &&
		(req->reqs.pio_request.address >= 0xcfc &&
		req->reqs.pio_request.address < 0xcfc+4));
}

static int cached_bus;
static int cached_dev;
static int cached_func;
static int cached_reg;
static int cached_enable;
#define PCI_REGMAX      255     /* highest supported config register addr.*/
#define PCI_FUNCMAX	7       /* highest supported function number */
#define PCI_SLOTMAX	31      /* highest supported slot number */
#define PCI_BUSMAX	255     /* highest supported bus number */
#define CONF1_ENABLE	0x80000000ul
static int handle_cf8cfc(struct vhm_vm *vm, struct vhm_request *req, int vcpu)
{
	int req_handled = 0;

	/*XXX: like DM, assume cfg address write is size 4 */
	if (is_cfg_addr(req)) {
		if (req->reqs.pio_request.direction == REQUEST_WRITE) {
			if (req->reqs.pio_request.size == 4) {
				int value = req->reqs.pio_request.value;

				cached_bus = (value >> 16) & PCI_BUSMAX;
				cached_dev = (value >> 11) & PCI_SLOTMAX;
				cached_func = (value >> 8) & PCI_FUNCMAX;
				cached_reg = value & PCI_REGMAX;
				cached_enable =
					(value & CONF1_ENABLE) == CONF1_ENABLE;
				req_handled = 1;
			}
		} else {
			if (req->reqs.pio_request.size == 4) {
				req->reqs.pio_request.value =
					(cached_bus << 16) |
					(cached_dev << 11) | (cached_func << 8)
					| cached_reg;
				if (cached_enable)
					req->reqs.pio_request.value |=
					CONF1_ENABLE;
				req_handled = 1;
			}
		}
	} else if (is_cfg_data(req)) {
		if (!cached_enable) {
			if (req->reqs.pio_request.direction == REQUEST_READ)
				req->reqs.pio_request.value = 0xffffffff;
			req_handled = 1;
		} else {
			/* pci request is same as io request at top */
			int offset = req->reqs.pio_request.address - 0xcfc;

			req->type = REQ_PCICFG;
			req->reqs.pci_request.bus = cached_bus;
			req->reqs.pci_request.dev = cached_dev;
			req->reqs.pci_request.func = cached_func;
			req->reqs.pci_request.reg = cached_reg + offset;
		}
	}

	if (req_handled) {
		req->processed = REQ_STATE_SUCCESS;
		if (hcall_notify_req_finish(vm->vmid, vcpu) < 0) {
			pr_err("vhm-ioreq: failed to "
				"notify request finished !\n");
			return -EFAULT;
		}
	}

	return req_handled;
}

static bool bdf_match(struct ioreq_client *client)
{
	return (client->trap_bdf &&
		client->pci_bus == cached_bus &&
		client->pci_dev == cached_dev &&
		client->pci_func == cached_func);
}

static struct ioreq_client *acrn_ioreq_find_client_by_request(struct vhm_vm *vm,
	struct vhm_request *req)
{
	struct list_head *pos, *range_pos;
	struct ioreq_client *client;
	struct ioreq_client *target_client = NULL, *fallback_client = NULL;
	struct ioreq_range *range;
	bool found = false;

	spin_lock(&vm->ioreq_client_lock);
	list_for_each(pos, &vm->ioreq_client_list) {
		client = container_of(pos, struct ioreq_client, list);

		if (client->fallback) {
			fallback_client = client;
			continue;
		}

		if (req->type == REQ_PCICFG) {
			if (bdf_match(client)) { /* bdf match client */
				target_client = client;
				break;
			} else /* other or fallback client */
				continue;
		}

		spin_lock(&client->range_lock);
		list_for_each(range_pos, &client->range_list) {
			range =
			container_of(range_pos, struct ioreq_range, list);
			if (req_in_range(range, req)) {
				found = true;
				target_client = client;
				break;
			}
		}
		spin_unlock(&client->range_lock);

		if (found)
			break;
	}
	spin_unlock(&vm->ioreq_client_lock);

	if (target_client)
		return target_client;

	if (fallback_client)
		return fallback_client;

	return NULL;
}

int acrn_ioreq_distribute_request(struct vhm_vm *vm)
{
	struct vhm_request *req;
	struct list_head *pos;
	struct ioreq_client *client;
	int i, vcpu_num;

	vcpu_num = atomic_read(&vm->vcpu_num);
	for (i = 0; i < vcpu_num; i++) {
		req = vm->req_buf->req_queue + i;
		if (req->valid && (req->processed == REQ_STATE_PENDING)) {
			if (handle_cf8cfc(vm, req, i))
				continue;
			client = acrn_ioreq_find_client_by_request(vm, req);
			if (client == NULL) {
				pr_err("vhm-ioreq: failed to "
						"find ioreq client -> "
						"BUG\n");
				BUG();
			} else {
				req->processed = REQ_STATE_PROCESSING;
				req->client = client->id;
				set_bit(i, client->ioreqs_map);
			}
		}
	}

	spin_lock(&vm->ioreq_client_lock);
	list_for_each(pos, &vm->ioreq_client_list) {
		client = container_of(pos, struct ioreq_client, list);
		if (has_pending_request(client))
			acrn_ioreq_notify_client(client);
	}
	spin_unlock(&vm->ioreq_client_lock);

	return 0;
}

int acrn_ioreq_complete_request(int client_id, uint64_t vcpu)
{
	struct ioreq_client *client;
	int ret;

	if (client_id < 0 || client_id >= MAX_CLIENT) {
		pr_err("vhm-ioreq: no client for id %d\n", client_id);
		return -EINVAL;
	}
	client = clients[client_id];
	if (!client) {
		pr_err("vhm-ioreq: no client for id %d\n", client_id);
		return -EINVAL;
	}

	clear_bit(vcpu, client->ioreqs_map);
	ret = hcall_notify_req_finish(client->vmid, vcpu);
	if (ret < 0) {
		pr_err("vhm-ioreq: failed to notify request finished !\n");
		return -EFAULT;
	}

	return 0;
}

unsigned int vhm_dev_poll(struct file *filep, poll_table *wait)
{
	struct vhm_vm *vm = filep->private_data;
	struct ioreq_client *fallback_client;
	unsigned int ret = 0;

	if (vm == NULL || vm->req_buf == NULL ||
		vm->ioreq_fallback_client <= 0) {
		pr_err("vhm: invalid VM !\n");
		ret = POLLERR;
		return ret;
	}

	fallback_client = clients[vm->ioreq_fallback_client];
	if (!fallback_client) {
		pr_err("vhm-ioreq: no client for id %d\n",
			vm->ioreq_fallback_client);
		return -EINVAL;
	}

	poll_wait(filep, &fallback_client->wq, wait);
	if (has_pending_request(fallback_client) ||
		is_destroying(fallback_client))
		ret = POLLIN | POLLRDNORM;

	return ret;
}

int acrn_ioreq_init(struct vhm_vm *vm, unsigned long vma)
{
	struct acrn_set_ioreq_buffer set_buffer;
	struct page *page;
	int ret;

	if (vm->req_buf)
		BUG();

	ret = get_user_pages_fast(vma, 1, 1, &page);
	if (unlikely(ret != 1) || (page == NULL)) {
		pr_err("vhm-ioreq: failed to pin request buffer!\n");
		return -ENOMEM;
	}

	vm->req_buf = page_address(page);
	vm->pg = page;

	set_buffer.req_buf = page_to_phys(page);

	ret = hcall_set_ioreq_buffer(vm->vmid, virt_to_phys(&set_buffer));
	if (ret < 0) {
		pr_err("vhm-ioreq: failed to set request buffer !\n");
		return -EFAULT;
	}

	/* reserve 0, let client_id start from 1 */
	set_bit(0, client_bitmap);

	pr_info("vhm-ioreq: init request buffer @ %p!\n",
		vm->req_buf);

	return 0;
}

void acrn_ioreq_free(struct vhm_vm *vm)
{
	struct list_head *pos, *tmp;

	list_for_each_safe(pos, tmp, &vm->ioreq_client_list) {
		struct ioreq_client *client =
			container_of(pos, struct ioreq_client, list);
		acrn_ioreq_destroy_client_pervm(client, vm);
	}

	if (vm->req_buf && vm->pg) {
		put_page(vm->pg);
		vm->pg = NULL;
		vm->req_buf = NULL;
	}
}
