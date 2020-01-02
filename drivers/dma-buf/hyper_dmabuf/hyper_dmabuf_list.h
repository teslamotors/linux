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
 */

#ifndef __HYPER_DMABUF_LIST_H__
#define __HYPER_DMABUF_LIST_H__

#include "hyper_dmabuf_struct.h"

/* number of bits to be used for exported dmabufs hash table */
#define MAX_ENTRY_EXPORTED 7
/* number of bits to be used for imported dmabufs hash table */
#define MAX_ENTRY_IMPORTED 7

struct list_entry_exported {
	struct exported_sgt_info *exported;
	struct hlist_node node;
};

struct list_entry_imported {
	struct imported_sgt_info *imported;
	struct hlist_node node;
};

int hyper_dmabuf_table_init(void);

int hyper_dmabuf_table_destroy(void);

int hyper_dmabuf_register_exported(struct exported_sgt_info *info);

/* search for pre-exported sgt and return id of it if it exist */
hyper_dmabuf_id_t hyper_dmabuf_find_hid_exported(struct dma_buf *dmabuf,
						 int domid);

int hyper_dmabuf_register_imported(struct imported_sgt_info *info);

struct exported_sgt_info *hyper_dmabuf_find_exported(hyper_dmabuf_id_t hid);

struct imported_sgt_info *hyper_dmabuf_find_imported(hyper_dmabuf_id_t hid);

int hyper_dmabuf_remove_exported(hyper_dmabuf_id_t hid);

int hyper_dmabuf_remove_imported(hyper_dmabuf_id_t hid);

void hyper_dmabuf_foreach_exported(void (*func)(struct exported_sgt_info *,
				   void *attr), void *attr);

int hyper_dmabuf_register_sysfs(struct device *dev);
int hyper_dmabuf_unregister_sysfs(struct device *dev);

#endif /* __HYPER_DMABUF_LIST_H__ */
