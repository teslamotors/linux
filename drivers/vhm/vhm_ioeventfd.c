/*
 * ioeventfd for ACRN hypervisor
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright (c) 2018 Intel Corporation. All rights reserved.
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
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
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
 *
 */
#include <linux/syscalls.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/file.h>
#include <linux/list.h>
#include <linux/eventfd.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include <linux/vhm/acrn_common.h>
#include <linux/vhm/acrn_vhm_ioreq.h>
#include <linux/vhm/vhm_ioctl_defs.h>
#include <linux/vhm/vhm_hypercall.h>

static LIST_HEAD(vhm_ioeventfd_clients);
static DEFINE_MUTEX(vhm_ioeventfds_mutex);

/* use internally to record properties of each ioeventfd */
struct acrn_vhm_ioeventfd {
	/* list to link all ioventfd together */
	struct list_head list;
	/* eventfd of this ioeventfd */
	struct eventfd_ctx *eventfd;
	/* start address for IO range*/
	u64 addr;
	/* match data */
	u64 data;
	/* length for IO range */
	int length;
	/* IO range type, can be REQ_PORTIO and REQ_MMIO */
	int type;
	/* ignore match data if true */
	bool wildcard;
};

/* instance to bind ioeventfds of each VM */
struct vhm_ioeventfd_info {
	struct list_head list;
	int refcnt;
	/* vmid of VM */
	uint16_t vmid;
	/* vhm ioreq client for this instance */
	int vhm_client_id;
	/* vcpu number of this VM */
	int vcpu_num;
	/* ioreq shared buffer of this VM */
	struct vhm_request *req_buf;

	/* ioeventfds in this instance */
	struct list_head ioeventfds;
	struct mutex ioeventfds_lock;
};

static struct vhm_ioeventfd_info *get_ioeventfd_info_by_client(
		int client_id)
{
	struct vhm_ioeventfd_info *info = NULL;

	mutex_lock(&vhm_ioeventfds_mutex);
	list_for_each_entry(info, &vhm_ioeventfd_clients, list) {
		if (info->vhm_client_id == client_id) {
			info->refcnt++;
			mutex_unlock(&vhm_ioeventfds_mutex);
			return info;
		}
	}
	mutex_unlock(&vhm_ioeventfds_mutex);
	return NULL;
}

static struct vhm_ioeventfd_info *get_ioeventfd_info_by_vm(uint16_t vmid)
{
	struct vhm_ioeventfd_info *info = NULL;

	mutex_lock(&vhm_ioeventfds_mutex);
	list_for_each_entry(info, &vhm_ioeventfd_clients, list) {
		if (info->vmid == vmid) {
			info->refcnt++;
			mutex_unlock(&vhm_ioeventfds_mutex);
			return info;
		}
	}
	mutex_unlock(&vhm_ioeventfds_mutex);
	return NULL;
}

static void put_ioeventfd_info(struct vhm_ioeventfd_info *info)
{
	mutex_lock(&vhm_ioeventfds_mutex);
	info->refcnt--;
	if (info->refcnt == 0) {
		list_del(&info->list);
		mutex_unlock(&vhm_ioeventfds_mutex);
		acrn_ioreq_destroy_client(info->vhm_client_id);
		kfree(info);
		return;
	}
	mutex_unlock(&vhm_ioeventfds_mutex);
}

/* assumes info->ioeventfds_lock held */
static void vhm_ioeventfd_shutdown(struct acrn_vhm_ioeventfd *p)
{
	eventfd_ctx_put(p->eventfd);
	list_del(&p->list);
	kfree(p);
}

static inline int ioreq_type_from_flags(int flags)
{
	return flags & ACRN_IOEVENTFD_FLAG_PIO ?
			REQ_PORTIO : REQ_MMIO;
}

/* assumes info->ioeventfds_lock held */
static bool vhm_ioeventfd_is_duplicated(struct vhm_ioeventfd_info *info,
		struct acrn_vhm_ioeventfd *ioeventfd)
{
	struct acrn_vhm_ioeventfd *p;

	/*
	 * Treat same addr/type/data with different length combination
	 * as the same one.
	 *   Register PIO[0x100~0x107] with data 0x10 as ioeventfd A, later
	 *   PIO[0x100~0x103] with data 0x10 will be failed to register.
	 */
	list_for_each_entry(p, &info->ioeventfds, list)
		if (p->addr == ioeventfd->addr &&
		    p->type == ioeventfd->type &&
		    (p->wildcard || ioeventfd->wildcard ||
		       p->data == ioeventfd->data))
			return true;

	return false;
}

static int acrn_assign_ioeventfd(struct vhm_ioeventfd_info *info,
		struct acrn_ioeventfd *args)
{
	struct eventfd_ctx *eventfd;
	struct acrn_vhm_ioeventfd *p;
	int ret = -ENOENT;

	/* check for range overflow */
	if (args->addr + args->len < args->addr)
		return -EINVAL;

	/* Only support 1,2,4,8 width registers */
	if (!(args->len == 1 || args->len == 2 ||
		args->len == 4 || args->len == 8))
		return -EINVAL;

	eventfd = eventfd_ctx_fdget(args->fd);
	if (IS_ERR(eventfd))
		return PTR_ERR(eventfd);

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (!p) {
		ret = -ENOMEM;
		goto fail;
	}

	INIT_LIST_HEAD(&p->list);
	p->addr    = args->addr;
	p->length  = args->len;
	p->eventfd = eventfd;
	p->type	   = ioreq_type_from_flags(args->flags);

	/* If datamatch enabled, we compare the data
	 * otherwise this is a wildcard
	 */
	if (args->flags & ACRN_IOEVENTFD_FLAG_DATAMATCH)
		p->data = args->data;
	else
		p->wildcard = true;

	mutex_lock(&info->ioeventfds_lock);

	/* Verify that there isn't a match already */
	if (vhm_ioeventfd_is_duplicated(info, p)) {
		ret = -EEXIST;
		goto unlock_fail;
	}

	/* register the IO range into vhm */
	ret = acrn_ioreq_add_iorange(info->vhm_client_id, p->type,
			p->addr, p->addr + p->length - 1);
	if (ret < 0)
		goto unlock_fail;

	list_add_tail(&p->list, &info->ioeventfds);
	mutex_unlock(&info->ioeventfds_lock);

	return 0;

unlock_fail:
	mutex_unlock(&info->ioeventfds_lock);
fail:
	kfree(p);
	eventfd_ctx_put(eventfd);
	return ret;
}

static int acrn_deassign_ioeventfd(struct vhm_ioeventfd_info *info,
		struct acrn_ioeventfd *args)
{
	struct acrn_vhm_ioeventfd *p, *tmp;
	struct eventfd_ctx *eventfd;
	int ret = 0;

	eventfd = eventfd_ctx_fdget(args->fd);
	if (IS_ERR(eventfd))
		return PTR_ERR(eventfd);

	mutex_lock(&info->ioeventfds_lock);

	list_for_each_entry_safe(p, tmp, &info->ioeventfds, list) {
		if (p->eventfd != eventfd)
			continue;

		ret = acrn_ioreq_del_iorange(info->vhm_client_id, p->type,
			p->addr, p->addr + p->length - 1);
		if (ret)
			break;
		vhm_ioeventfd_shutdown(p);
		break;
	}

	mutex_unlock(&info->ioeventfds_lock);

	eventfd_ctx_put(eventfd);

	return ret;
}

int acrn_ioeventfd(uint16_t vmid, struct acrn_ioeventfd *args)
{
	struct vhm_ioeventfd_info *info = NULL;
	int ret;

	info = get_ioeventfd_info_by_vm(vmid);
	if (!info)
		return -ENOENT;

	if (args->flags & ACRN_IOEVENTFD_FLAG_DEASSIGN)
		ret = acrn_deassign_ioeventfd(info, args);
	else
		ret = acrn_assign_ioeventfd(info, args);

	put_ioeventfd_info(info);
	return ret;
}

static struct acrn_vhm_ioeventfd *vhm_ioeventfd_match(
		struct vhm_ioeventfd_info *info,
		u64 addr, u64 data, int length, int type)
{
	struct acrn_vhm_ioeventfd *p = NULL;

	/*
	 * Same addr/type/data will be treated as hit, otherwise ignore.
	 *   Register PIO[0x100~0x107] with data 0x10 as ioeventfd A, later
	 *   request PIO[0x100~0x103] with data 0x10 will hit A.
	 */
	list_for_each_entry(p, &info->ioeventfds, list) {
		if (p->type == type &&
			p->addr == addr &&
			(p->wildcard || p->data == data))
			return p;
	}

	return NULL;
}

static int acrn_ioeventfd_dispatch_ioreq(int client_id,
		unsigned long *ioreqs_map)
{
	struct vhm_request *req;
	struct acrn_vhm_ioeventfd *p;
	struct vhm_ioeventfd_info *info;
	u64 addr;
	u64 val;
	int size;
	int vcpu;

	info = get_ioeventfd_info_by_client(client_id);
	if (!info)
		return -EINVAL;

	/* get req buf */
	if (!info->req_buf) {
		info->req_buf = acrn_ioreq_get_reqbuf(info->vhm_client_id);
		if (!info->req_buf) {
			pr_err("Failed to get req_buf for client %d\n",
					info->vhm_client_id);
			put_ioeventfd_info(info);
			return -EINVAL;
		}
	}

	while (1) {
		vcpu = find_first_bit(ioreqs_map, info->vcpu_num);
		if (vcpu == info->vcpu_num)
			break;
		req = &info->req_buf[vcpu];
		if (atomic_read(&req->processed) == REQ_STATE_PROCESSING &&
			req->client == client_id) {
			if (req->type == REQ_MMIO) {
				if (req->reqs.mmio_request.direction ==
						REQUEST_READ) {
					/* reading does nothing and return 0 */
					req->reqs.mmio_request.value = 0;
					goto next_ioreq;
				}
				addr = req->reqs.mmio_request.address;
				size = req->reqs.mmio_request.size;
				val = req->reqs.mmio_request.value;
			} else {
				if (req->reqs.pio_request.direction ==
						REQUEST_READ) {
					/* reading does nothing and return 0 */
					req->reqs.pio_request.value = 0;
					goto next_ioreq;
				}
				addr = req->reqs.pio_request.address;
				size = req->reqs.pio_request.size;
				val = req->reqs.pio_request.value;
			}

			mutex_lock(&info->ioeventfds_lock);
			p = vhm_ioeventfd_match(info, addr, val, size,
					req->type);
			if (p)
				eventfd_signal(p->eventfd, 1);
			mutex_unlock(&info->ioeventfds_lock);

next_ioreq:
			atomic_set(&req->processed, REQ_STATE_COMPLETE);
			acrn_ioreq_complete_request(client_id, vcpu);
		}
	}

	put_ioeventfd_info(info);
	return 0;
}

int acrn_ioeventfd_init(uint16_t vmid)
{
	int ret = 0;
	char name[16];
	struct vm_info vm_info;
	struct vhm_ioeventfd_info *info;

	info = get_ioeventfd_info_by_vm(vmid);
	if (info) {
		put_ioeventfd_info(info);
		return -EEXIST;
	}

	info = kzalloc(sizeof(struct vhm_ioeventfd_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	snprintf(name, sizeof(name), "ioeventfd-%hu", vmid);
	info->vhm_client_id = acrn_ioreq_create_client(vmid,
			acrn_ioeventfd_dispatch_ioreq, name);
	if (info->vhm_client_id < 0) {
		pr_err("Failed to create ioeventfd client for ioreq!\n");
		ret = -EINVAL;
		goto fail;
	}

	ret = vhm_get_vm_info(vmid, &vm_info);
	if (ret < 0) {
		pr_err("Failed to get vm info for vmid %d\n", vmid);
		goto client_fail;
	}

	info->vcpu_num = vm_info.max_vcpu;

	ret = acrn_ioreq_attach_client(info->vhm_client_id, 0);
	if (ret < 0) {
		pr_err("Failed to attach vhm client %d!\n",
				info->vhm_client_id);
		goto client_fail;
	}
	mutex_init(&info->ioeventfds_lock);
	info->vmid = vmid;
	info->refcnt = 1;
	INIT_LIST_HEAD(&info->ioeventfds);

	mutex_lock(&vhm_ioeventfds_mutex);
	list_add(&info->list, &vhm_ioeventfd_clients);
	mutex_unlock(&vhm_ioeventfds_mutex);

	pr_info("ACRN vhm ioeventfd init done!\n");
	return 0;
client_fail:
	acrn_ioreq_destroy_client(info->vhm_client_id);
fail:
	kfree(info);
	return ret;
}

void acrn_ioeventfd_deinit(uint16_t vmid)
{
	struct acrn_vhm_ioeventfd *p, *tmp;
	struct vhm_ioeventfd_info *info = NULL;

	info = get_ioeventfd_info_by_vm(vmid);
	if (!info)
		return;

	put_ioeventfd_info(info);

	mutex_lock(&info->ioeventfds_lock);
	list_for_each_entry_safe(p, tmp, &info->ioeventfds, list)
		vhm_ioeventfd_shutdown(p);
	mutex_unlock(&info->ioeventfds_lock);

	/* put one more as we count it in finding */
	put_ioeventfd_info(info);
}
