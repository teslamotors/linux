/*
 * drivers/video/tegra/nvmap/nvmap_handle.c
 *
 * Handle allocation and freeing routines for nvmap
 *
 * Copyright (c) 2009-2016, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/rbtree.h>
#include <linux/dma-buf.h>
#include <linux/moduleparam.h>
#include <linux/nvmap.h>
#include <linux/tegra-soc.h>

#include <asm/pgtable.h>

#include <trace/events/nvmap.h>

#include "nvmap_priv.h"
#include "nvmap_ioctl.h"

static void add_handle_ref(struct nvmap_client *client,
			   struct nvmap_handle_ref *ref)
{
	struct rb_node **p, *parent = NULL;

	nvmap_ref_lock(client);
	p = &client->handle_refs.rb_node;
	while (*p) {
		struct nvmap_handle_ref *node;
		parent = *p;
		node = rb_entry(parent, struct nvmap_handle_ref, node);
		if (ref->handle > node->handle)
			p = &parent->rb_right;
		else
			p = &parent->rb_left;
	}
	rb_link_node(&ref->node, parent, p);
	rb_insert_color(&ref->node, &client->handle_refs);
	client->handle_count++;
	if (client->handle_count > nvmap_max_handle_count)
		nvmap_max_handle_count = client->handle_count;
	atomic_inc(&ref->handle->share_count);
	nvmap_ref_unlock(client);
}

struct nvmap_handle_ref *nvmap_create_handle_from_va(struct nvmap_client *client,
						     ulong vaddr, size_t size)
{
	struct vm_area_struct *vma;
	struct nvmap_handle_ref *ref;

	/* don't allow non-page aligned addresses. */
	if (vaddr & ~PAGE_MASK)
		return ERR_PTR(-EINVAL);

	vma = find_vma(current->mm, vaddr);

	if (unlikely(!vma) || (unlikely(vaddr < vma->vm_start )) ||
	    unlikely(vaddr >= vma->vm_end) ||
	    unlikely(size > vma->vm_end - vma->vm_start)) {
		return ERR_PTR(-EINVAL);
	}

	if (!size)
		size = vma->vm_end - vaddr;
	ref = nvmap_create_handle(client, PAGE_ALIGN(size));
	if (!IS_ERR(ref))
		ref->handle->orig_size = size;
	return ref;
}

struct nvmap_handle_ref *nvmap_create_handle(struct nvmap_client *client,
					     size_t size)
{
	void *err = ERR_PTR(-ENOMEM);
	struct nvmap_handle *h;
	struct nvmap_handle_ref *ref = NULL;

	if (!client)
		return ERR_PTR(-EINVAL);

	if (!size)
		return ERR_PTR(-EINVAL);

	h = kzalloc(sizeof(*h), GFP_KERNEL);
	if (!h)
		return ERR_PTR(-ENOMEM);

	ref = kzalloc(sizeof(*ref), GFP_KERNEL);
	if (!ref)
		goto ref_alloc_fail;

	atomic_set(&h->ref, 1);
	atomic_set(&h->pin, 0);
	h->owner = client;
	BUG_ON(!h->owner);
	h->size = h->orig_size = size;
	h->flags = NVMAP_HANDLE_WRITE_COMBINE;
	h->peer = NVMAP_IVM_INVALID_PEER;
	mutex_init(&h->lock);
	INIT_LIST_HEAD(&h->vmas);
	INIT_LIST_HEAD(&h->lru);
	INIT_LIST_HEAD(&h->dmabuf_priv);

	/*
	 * This takes out 1 ref on the dambuf. This corresponds to the
	 * handle_ref that gets automatically made by nvmap_create_handle().
	 */
	h->dmabuf = __nvmap_make_dmabuf(client, h);
	if (IS_ERR(h->dmabuf)) {
		err = h->dmabuf;
		goto make_dmabuf_fail;
	}

	nvmap_handle_add(nvmap_dev, h);

	/*
	 * Major assumption here: the dma_buf object that the handle contains
	 * is created with a ref count of 1.
	 */
	atomic_set(&ref->dupes, 1);
	ref->handle = h;
	add_handle_ref(client, ref);
	trace_nvmap_create_handle(client, client->name, h, size, ref);
	return ref;

make_dmabuf_fail:
	kfree(ref);
ref_alloc_fail:
	kfree(h);
	return err;
}

struct nvmap_handle_ref *nvmap_try_duplicate_by_ivmid(
		struct nvmap_client *client, unsigned int ivm_id,
		struct nvmap_heap_block **block)
{
	struct nvmap_handle *h = NULL;
	struct nvmap_handle_ref *ref = NULL;
	struct rb_node *n;

	spin_lock(&nvmap_dev->handle_lock);

	n = nvmap_dev->handles.rb_node;
	for (n = rb_first(&nvmap_dev->handles); n; n = rb_next(n)) {
		h = rb_entry(n, struct nvmap_handle, node);
		if (h->ivm_id == ivm_id) {
			BUG_ON(!virt_addr_valid(h));
			/* get handle's ref only if non-zero */
			if (atomic_inc_not_zero(&h->ref) == 0) {
				*block = h->carveout;
				/* strip handle's block and fail duplication */
				h->carveout = NULL;
				break;
			}
			spin_unlock(&nvmap_dev->handle_lock);
			goto found;
		}
	}

	spin_unlock(&nvmap_dev->handle_lock);
	/* handle is either freed or being freed, don't duplicate it */
	goto finish;

	/*
	 * From this point, handle and its buffer are valid and won't be
	 * freed as a reference is taken on it. The dmabuf can still be
	 * freed anytime till reference is taken on it below.
	 */
found:
	mutex_lock(&h->lock);
	/*
	 * Save this block. If dmabuf's reference is not held in time,
	 * this can be reused to avoid the delay to free the buffer
	 * in this old handle and allocate it for a new handle from
	 * the ivm allocation ioctl.
	 */
	*block = h->carveout;
	if (!h->dmabuf)
		goto fail;
	BUG_ON(!h->dmabuf->file);
	/* This is same as get_dma_buf() if file->f_count was non-zero */
	if (atomic_long_inc_not_zero(&h->dmabuf->file->f_count) == 0)
		goto fail;
	mutex_unlock(&h->lock);

	/* h->dmabuf can't be NULL anymore. Duplicate the handle. */
	ref = nvmap_duplicate_handle(client, h, true);
	/* put the extra ref taken using get_dma_buf. */
	dma_buf_put(h->dmabuf);
finish:
	return ref;
fail:
	/* free handle but not its buffer */
	h->carveout = NULL;
	mutex_unlock(&h->lock);
	nvmap_handle_put(h);
	return NULL;
}

struct nvmap_handle_ref *nvmap_duplicate_handle(struct nvmap_client *client,
					struct nvmap_handle *h, bool skip_val)
{
	struct nvmap_handle_ref *ref = NULL;

	BUG_ON(!client);

	if (!skip_val)
		/* on success, the reference count for the handle should be
		 * incremented, so the success paths will not call
		 * nvmap_handle_put */
		h = nvmap_validate_get(h);

	if (!h) {
		pr_debug("%s duplicate handle failed\n",
			    current->group_leader->comm);
		return ERR_PTR(-EPERM);
	}

	if (!h->alloc) {
		pr_err("%s duplicating unallocated handle\n",
			current->group_leader->comm);
		nvmap_handle_put(h);
		return ERR_PTR(-EINVAL);
	}

	nvmap_ref_lock(client);
	ref = __nvmap_validate_locked(client, h);

	if (ref) {
		/* handle already duplicated in client; just increment
		 * the reference count rather than re-duplicating it */
		atomic_inc(&ref->dupes);
		nvmap_ref_unlock(client);
		goto out;
	}

	nvmap_ref_unlock(client);

	ref = kzalloc(sizeof(*ref), GFP_KERNEL);
	if (!ref) {
		nvmap_handle_put(h);
		return ERR_PTR(-ENOMEM);
	}

	atomic_set(&ref->dupes, 1);
	ref->handle = h;
	add_handle_ref(client, ref);

	/*
	 * Ref counting on the dma_bufs follows the creation and destruction of
	 * nvmap_handle_refs. That is every time a handle_ref is made the
	 * dma_buf ref count goes up and everytime a handle_ref is destroyed
	 * the dma_buf ref count goes down.
	 */
	get_dma_buf(h->dmabuf);

out:
	NVMAP_TAG_TRACE(trace_nvmap_duplicate_handle,
		NVMAP_TP_ARGS_CHR(client, h, ref));
	return ref;
}

struct nvmap_handle_ref *nvmap_create_handle_from_fd(
			struct nvmap_client *client, int fd)
{
	struct nvmap_handle *handle;
	struct nvmap_handle_ref *ref;

	BUG_ON(!client);

	handle = nvmap_handle_get_from_dmabuf_fd(client, fd);
	if (IS_ERR(handle))
		return ERR_CAST(handle);
	ref = nvmap_duplicate_handle(client, handle, false);
	nvmap_handle_put(handle);
	return ref;
}
