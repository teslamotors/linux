/*
 * irqfd for ACRN hypervisor
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
#include <linux/async.h>
#include <linux/slab.h>

#include <linux/vhm/acrn_common.h>
#include <linux/vhm/acrn_vhm_ioreq.h>
#include <linux/vhm/vhm_vm_mngt.h>
#include <linux/vhm/vhm_ioctl_defs.h>
#include <linux/vhm/vhm_hypercall.h>

static LIST_HEAD(vhm_irqfd_clients);
static DEFINE_MUTEX(vhm_irqfds_mutex);
static ASYNC_DOMAIN_EXCLUSIVE(irqfd_domain);

/* use internally to record properties of each irqfd */
struct acrn_vhm_irqfd {
	/* vhm_irqfd_info which this irqfd belong to */
	struct vhm_irqfd_info *info;
	/* wait queue node */
	wait_queue_entry_t wait;
	/* async shutdown work */
	struct work_struct shutdown;
	/* eventfd of this irqfd */
	struct eventfd_ctx *eventfd;
	/* list to link all ioventfd together */
	struct list_head list;
	/* poll_table of this irqfd */
	poll_table pt;
	/* msi to send when this irqfd triggerd */
	struct acrn_msi_entry msi;
};

/* instance to bind irqfds of each VM */
struct vhm_irqfd_info {
	struct list_head list;
	int refcnt;
	/* vmid of VM */
	uint16_t  vmid;
	/* workqueue for async shutdown work */
	struct workqueue_struct *wq;

	/* irqfds in this instance */
	struct list_head irqfds;
	spinlock_t irqfds_lock;
};

static struct vhm_irqfd_info *get_irqfd_info_by_vm(uint16_t vmid)
{
	struct vhm_irqfd_info *info = NULL;

	mutex_lock(&vhm_irqfds_mutex);
	list_for_each_entry(info, &vhm_irqfd_clients, list) {
		if (info->vmid == vmid) {
			info->refcnt++;
			mutex_unlock(&vhm_irqfds_mutex);
			return info;
		}
	}
	mutex_unlock(&vhm_irqfds_mutex);
	return NULL;
}

static void put_irqfd_info(struct vhm_irqfd_info *info)
{
	mutex_lock(&vhm_irqfds_mutex);
	info->refcnt--;
	if (info->refcnt == 0) {
		list_del(&info->list);
		kfree(info);
	}
	mutex_unlock(&vhm_irqfds_mutex);
}

static void vhm_irqfd_inject(struct acrn_vhm_irqfd *irqfd)
{
	struct vhm_irqfd_info *info = irqfd->info;

	vhm_inject_msi(info->vmid, irqfd->msi.msi_addr,
			irqfd->msi.msi_data);
}

/*
 * Try to find if the irqfd still in list info->irqfds
 *
 * assumes info->irqfds_lock is held
 */
static bool vhm_irqfd_is_active(struct vhm_irqfd_info *info,
		struct acrn_vhm_irqfd *irqfd)
{
	struct acrn_vhm_irqfd *_irqfd;

	list_for_each_entry(_irqfd, &info->irqfds, list)
		if (_irqfd == irqfd)
			return true;

	return false;
}

/*
 * Remove irqfd and free it.
 *
 * assumes info->irqfds_lock is held
 */
static void vhm_irqfd_shutdown(struct acrn_vhm_irqfd *irqfd)
{
	u64 cnt;

	/* remove from wait queue */
	list_del_init(&irqfd->list);
	eventfd_ctx_remove_wait_queue(irqfd->eventfd, &irqfd->wait, &cnt);
	eventfd_ctx_put(irqfd->eventfd);
	kfree(irqfd);
}

static void vhm_irqfd_shutdown_work(struct work_struct *work)
{
	unsigned long flags;
	struct acrn_vhm_irqfd *irqfd =
		container_of(work, struct acrn_vhm_irqfd, shutdown);
	struct vhm_irqfd_info *info = irqfd->info;

	spin_lock_irqsave(&info->irqfds_lock, flags);
	if (vhm_irqfd_is_active(info, irqfd))
		vhm_irqfd_shutdown(irqfd);
	spin_unlock_irqrestore(&info->irqfds_lock, flags);
}

/*
 * Called with wqh->lock held and interrupts disabled
 */
static int vhm_irqfd_wakeup(wait_queue_entry_t *wait, unsigned int mode,
		int sync, void *key)
{
	struct acrn_vhm_irqfd *irqfd =
		container_of(wait, struct acrn_vhm_irqfd, wait);
	unsigned long poll_bits = (unsigned long)key;
	struct vhm_irqfd_info *info = irqfd->info;

	if (poll_bits & POLLIN)
		/* An event has been signaled, inject an interrupt */
		vhm_irqfd_inject(irqfd);

	if (poll_bits & POLLHUP)
		/* async close eventfd as shutdown need hold wqh->lock */
		queue_work(info->wq, &irqfd->shutdown);

	return 0;
}

static void vhm_irqfd_poll_func(struct file *file,
		wait_queue_head_t *wqh,	poll_table *pt)
{
	struct acrn_vhm_irqfd *irqfd =
		container_of(pt, struct acrn_vhm_irqfd, pt);
	add_wait_queue(wqh, &irqfd->wait);
}

static int acrn_irqfd_assign(struct vhm_irqfd_info *info,
		struct acrn_irqfd *args)
{
	struct acrn_vhm_irqfd *irqfd, *tmp;
	struct fd f;
	struct eventfd_ctx *eventfd = NULL;
	int ret = 0;
	unsigned int events;

	irqfd = kzalloc(sizeof(*irqfd), GFP_KERNEL);
	if (!irqfd)
		return -ENOMEM;

	irqfd->info = info;
	memcpy(&irqfd->msi, &args->msi, sizeof(args->msi));
	INIT_LIST_HEAD(&irqfd->list);
	INIT_WORK(&irqfd->shutdown, vhm_irqfd_shutdown_work);

	f = fdget(args->fd);
	if (!f.file) {
		ret = -EBADF;
		goto out;
	}

	eventfd = eventfd_ctx_fileget(f.file);
	if (IS_ERR(eventfd)) {
		ret = PTR_ERR(eventfd);
		goto fail;
	}

	irqfd->eventfd = eventfd;

	/*
	 * Install our own custom wake-up handling so we are notified via
	 * a callback whenever someone signals the underlying eventfd
	 */
	init_waitqueue_func_entry(&irqfd->wait, vhm_irqfd_wakeup);
	init_poll_funcptr(&irqfd->pt, vhm_irqfd_poll_func);

	spin_lock_irq(&info->irqfds_lock);

	list_for_each_entry(tmp, &info->irqfds, list) {
		if (irqfd->eventfd != tmp->eventfd)
			continue;
		/* This fd is used for another irq already. */
		ret = -EBUSY;
		spin_unlock_irq(&info->irqfds_lock);
		goto fail;
	}
	list_add_tail(&irqfd->list, &info->irqfds);

	spin_unlock_irq(&info->irqfds_lock);

	/* Check the pending event in this stage */
	events = f.file->f_op->poll(f.file, &irqfd->pt);

	if (events & POLLIN)
		vhm_irqfd_inject(irqfd);

	fdput(f);

	return 0;
fail:
	if (eventfd && !IS_ERR(eventfd))
		eventfd_ctx_put(eventfd);

	fdput(f);
out:
	kfree(irqfd);
	return ret;
}

static int acrn_irqfd_deassign(struct vhm_irqfd_info *info,
		struct acrn_irqfd *args)
{
	struct acrn_vhm_irqfd *irqfd, *tmp;
	struct eventfd_ctx *eventfd;

	eventfd = eventfd_ctx_fdget(args->fd);
	if (IS_ERR(eventfd))
		return PTR_ERR(eventfd);

	spin_lock_irq(&info->irqfds_lock);

	list_for_each_entry_safe(irqfd, tmp, &info->irqfds, list) {
		if (irqfd->eventfd == eventfd) {
			vhm_irqfd_shutdown(irqfd);
			break;
		}
	}

	spin_unlock_irq(&info->irqfds_lock);
	eventfd_ctx_put(eventfd);

	return 0;
}

int acrn_irqfd(uint16_t vmid, struct acrn_irqfd *args)
{
	struct vhm_irqfd_info *info;
	int ret;

	info = get_irqfd_info_by_vm(vmid);
	if (!info)
		return -ENOENT;

	if (args->flags & ACRN_IRQFD_FLAG_DEASSIGN)
		ret = acrn_irqfd_deassign(info, args);
	else
		ret = acrn_irqfd_assign(info, args);

	put_irqfd_info(info);
	return ret;
}

int acrn_irqfd_init(uint16_t vmid)
{
	struct vhm_irqfd_info *info;

	info = get_irqfd_info_by_vm(vmid);
	if (info) {
		put_irqfd_info(info);
		return -EEXIST;
	}

	info = kzalloc(sizeof(struct vhm_irqfd_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	info->vmid = vmid;
	info->refcnt = 1;
	INIT_LIST_HEAD(&info->irqfds);
	spin_lock_init(&info->irqfds_lock);

	info->wq = alloc_workqueue("acrn_irqfd-%d", 0, 0, vmid);
	if (!info->wq) {
		kfree(info);
		return -ENOMEM;
	}

	mutex_lock(&vhm_irqfds_mutex);
	list_add(&info->list, &vhm_irqfd_clients);
	mutex_unlock(&vhm_irqfds_mutex);

	pr_info("ACRN vhm irqfd init done!\n");
	return 0;
}

void acrn_irqfd_deinit(uint16_t vmid)
{
	struct acrn_vhm_irqfd *irqfd, *tmp;
	struct vhm_irqfd_info *info;

	info = get_irqfd_info_by_vm(vmid);
	if (!info)
		return;
	put_irqfd_info(info);

	destroy_workqueue(info->wq);

	spin_lock_irq(&info->irqfds_lock);
	list_for_each_entry_safe(irqfd, tmp, &info->irqfds, list)
		vhm_irqfd_shutdown(irqfd);
	spin_unlock_irq(&info->irqfds_lock);

	/* put one more to release it */
	put_irqfd_info(info);
}
