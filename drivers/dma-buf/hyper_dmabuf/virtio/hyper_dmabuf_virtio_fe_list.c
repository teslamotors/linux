/*
 * Copyright © 2017 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Mateusz Polrola <mateuszx.potrola@intel.com>
 *
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/hashtable.h>
#include "../hyper_dmabuf_drv.h"
#include "hyper_dmabuf_virtio_common.h"
#include "hyper_dmabuf_virtio_fe_list.h"

DECLARE_HASHTABLE(virtio_fe_hash, MAX_ENTRY_FE);

void virtio_fe_table_init(void)
{
	hash_init(virtio_fe_hash);
}

int virtio_fe_add(struct virtio_fe_info *fe_info)
{
	struct virtio_fe_info_entry *info_entry;

	info_entry = kmalloc(sizeof(*info_entry), GFP_KERNEL);

	if (!info_entry)
		return -ENOMEM;

	info_entry->info = fe_info;

	hash_add(virtio_fe_hash, &info_entry->node,
		info_entry->info->client_id);

	return 0;
}

struct virtio_fe_info *virtio_fe_find(int client_id)
{
	struct virtio_fe_info_entry *info_entry;
	int bkt;

	hash_for_each(virtio_fe_hash, bkt, info_entry, node)
		if (info_entry->info->client_id == client_id)
			return info_entry->info;

	return NULL;
}

struct virtio_fe_info *virtio_fe_find_by_vmid(int vmid)
{
	struct virtio_fe_info_entry *info_entry;
	int bkt;

	hash_for_each(virtio_fe_hash, bkt, info_entry, node)
		if (info_entry->info->vmid == vmid)
			return info_entry->info;

	return NULL;
}

int virtio_fe_remove(int client_id)
{
	struct virtio_fe_info_entry *info_entry;
	int bkt;

	hash_for_each(virtio_fe_hash, bkt, info_entry, node)
		if (info_entry->info->client_id == client_id) {
			hash_del(&info_entry->node);
			kfree(info_entry);
			return 0;
		}

	return -ENOENT;
}

void virtio_fe_foreach(
        void (*func)(struct virtio_fe_info *, void *attr),
        void *attr)
{
	struct virtio_fe_info_entry *info_entry;
        struct hlist_node *tmp;
        int bkt;

        hash_for_each_safe(virtio_fe_hash, bkt, tmp,
                           info_entry, node) {
                func(info_entry->info, attr);
        }
}
