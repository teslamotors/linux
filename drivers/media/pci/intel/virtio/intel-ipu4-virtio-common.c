// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
/*
 * Copyright (C) 2018 Intel Corporation
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/hashtable.h>

#include "intel-ipu4-virtio-common.h"

DECLARE_HASHTABLE(ipu4_virtio_fe_hash, MAX_ENTRY_FE);

void ipu4_virtio_fe_table_init(void)
{
	hash_init(ipu4_virtio_fe_hash);
}

int ipu4_virtio_fe_add(struct ipu4_virtio_fe_info *fe_info)
{
	struct ipu4_virtio_fe_info_entry *info_entry;

	info_entry = kmalloc(sizeof(*info_entry), GFP_KERNEL);

	if (!info_entry)
		return -ENOMEM;

	info_entry->info = fe_info;

	hash_add(ipu4_virtio_fe_hash, &info_entry->node,
		info_entry->info->client_id);

	return 0;
}

struct ipu4_virtio_fe_info *ipu4_virtio_fe_find(int client_id)
{
	struct ipu4_virtio_fe_info_entry *info_entry;
	int bkt;

	hash_for_each(ipu4_virtio_fe_hash, bkt, info_entry, node)
		if (info_entry->info->client_id == client_id)
			return info_entry->info;

	return NULL;
}

struct ipu4_virtio_fe_info *ipu4_virtio_fe_find_by_vmid(int vmid)
{
	struct ipu4_virtio_fe_info_entry *info_entry;
	int bkt;

	hash_for_each(ipu4_virtio_fe_hash, bkt, info_entry, node)
		if (info_entry->info->vmid == vmid)
			return info_entry->info;

	return NULL;
}

int ipu4_virtio_fe_remove(int client_id)
{
	struct ipu4_virtio_fe_info_entry *info_entry;
	int bkt;

	hash_for_each(ipu4_virtio_fe_hash, bkt, info_entry, node)
		if (info_entry->info->client_id == client_id) {
			hash_del(&info_entry->node);
			kfree(info_entry);
			return 0;
		}

	return -ENOENT;
}
