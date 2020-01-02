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
 *    Dongwon Kim <dongwon.kim@intel.com>
 *    Mateusz Polrola <mateuszx.potrola@intel.com>
 *
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/hashtable.h>
#include <xen/grant_table.h>
#include "../hyper_dmabuf_drv.h"
#include "hyper_dmabuf_xen_comm.h"
#include "hyper_dmabuf_xen_comm_list.h"

DECLARE_HASHTABLE(xen_comm_tx_ring_hash, MAX_ENTRY_TX_RING);
DECLARE_HASHTABLE(xen_comm_rx_ring_hash, MAX_ENTRY_RX_RING);

void xen_comm_ring_table_init(void)
{
	hash_init(xen_comm_rx_ring_hash);
	hash_init(xen_comm_tx_ring_hash);
}

int xen_comm_add_tx_ring(struct xen_comm_tx_ring_info *ring_info)
{
	struct xen_comm_tx_ring_info_entry *info_entry;

	info_entry = kmalloc(sizeof(*info_entry), GFP_KERNEL);

	if (!info_entry)
		return -ENOMEM;

	info_entry->info = ring_info;

	hash_add(xen_comm_tx_ring_hash, &info_entry->node,
		info_entry->info->rdomain);

	return 0;
}

int xen_comm_add_rx_ring(struct xen_comm_rx_ring_info *ring_info)
{
	struct xen_comm_rx_ring_info_entry *info_entry;

	info_entry = kmalloc(sizeof(*info_entry), GFP_KERNEL);

	if (!info_entry)
		return -ENOMEM;

	info_entry->info = ring_info;

	hash_add(xen_comm_rx_ring_hash, &info_entry->node,
		info_entry->info->sdomain);

	return 0;
}

struct xen_comm_tx_ring_info *xen_comm_find_tx_ring(int domid)
{
	struct xen_comm_tx_ring_info_entry *info_entry;
	int bkt;

	hash_for_each(xen_comm_tx_ring_hash, bkt, info_entry, node)
		if (info_entry->info->rdomain == domid)
			return info_entry->info;

	return NULL;
}

struct xen_comm_rx_ring_info *xen_comm_find_rx_ring(int domid)
{
	struct xen_comm_rx_ring_info_entry *info_entry;
	int bkt;

	hash_for_each(xen_comm_rx_ring_hash, bkt, info_entry, node)
		if (info_entry->info->sdomain == domid)
			return info_entry->info;

	return NULL;
}

int xen_comm_remove_tx_ring(int domid)
{
	struct xen_comm_tx_ring_info_entry *info_entry;
	int bkt;

	hash_for_each(xen_comm_tx_ring_hash, bkt, info_entry, node)
		if (info_entry->info->rdomain == domid) {
			hash_del(&info_entry->node);
			kfree(info_entry);
			return 0;
		}

	return -ENOENT;
}

int xen_comm_remove_rx_ring(int domid)
{
	struct xen_comm_rx_ring_info_entry *info_entry;
	int bkt;

	hash_for_each(xen_comm_rx_ring_hash, bkt, info_entry, node)
		if (info_entry->info->sdomain == domid) {
			hash_del(&info_entry->node);
			kfree(info_entry);
			return 0;
		}

	return -ENOENT;
}

void xen_comm_foreach_tx_ring(void (*func)(int domid))
{
	struct xen_comm_tx_ring_info_entry *info_entry;
	struct hlist_node *tmp;
	int bkt;

	hash_for_each_safe(xen_comm_tx_ring_hash, bkt, tmp,
			   info_entry, node) {
		func(info_entry->info->rdomain);
	}
}

void xen_comm_foreach_rx_ring(void (*func)(int domid))
{
	struct xen_comm_rx_ring_info_entry *info_entry;
	struct hlist_node *tmp;
	int bkt;

	hash_for_each_safe(xen_comm_rx_ring_hash, bkt, tmp,
			   info_entry, node) {
		func(info_entry->info->sdomain);
	}
}
