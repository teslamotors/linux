/*
 * drivers/video/tegra/nvmap/nvmap_ioctl.c
 *
 * User-space interface to nvmap
 *
 * Copyright (c) 2011-2017, NVIDIA CORPORATION. All rights reserved.
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

#define pr_fmt(fmt)	"nvmap: %s() " fmt, __func__

#include <linux/dma-mapping.h>
#include <linux/export.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/nvmap.h>
#include <linux/vmalloc.h>
#include <linux/highmem.h>

#include <asm/memory.h>

#include <trace/events/nvmap.h>

#include "nvmap_ioctl.h"
#include "nvmap_priv.h"

#include <linux/list.h>
extern struct device tegra_vpr_dev;

static ssize_t rw_handle(struct nvmap_client *client, struct nvmap_handle *h,
			 int is_read, unsigned long h_offs,
			 unsigned long sys_addr, unsigned long h_stride,
			 unsigned long sys_stride, unsigned long elem_size,
			 unsigned long count);

/* NOTE: Callers of this utility function must invoke nvmap_handle_put after
 * using the returned nvmap_handle.
 */
struct nvmap_handle *nvmap_handle_get_from_fd(int fd)
{
	struct nvmap_handle *h;

	h = nvmap_handle_get_from_dmabuf_fd(NULL, fd);
	if (!IS_ERR(h))
		return h;
	return NULL;
}

static int nvmap_install_fd(struct nvmap_client *client,
	struct nvmap_handle *handle, int fd, void __user *arg,
	void *op, size_t op_size, bool free, struct dma_buf *dmabuf)
{
	int err = 0;

	if (IS_ERR_VALUE(fd)) {
		err = fd;
		goto fd_fail;
	}

	if (copy_to_user(arg, op, op_size)) {
		err = -EFAULT;
		goto copy_fail;
	}

	fd_install(fd, dmabuf->file);
	return err;

copy_fail:
	put_unused_fd(fd);
fd_fail:
	if (dmabuf)
		dma_buf_put(dmabuf);
	if (free && handle)
		nvmap_free_handle(client, handle);
	return err;
}

int nvmap_ioctl_getfd(struct file *filp, void __user *arg)
{
	struct nvmap_handle *handle;
	struct nvmap_create_handle op;
	struct nvmap_client *client = filp->private_data;
	struct dma_buf *dmabuf;

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	handle = nvmap_handle_get_from_fd(op.handle);
	if (handle) {
		op.fd = nvmap_get_dmabuf_fd(client, handle);
		nvmap_handle_put(handle);
		dmabuf = IS_ERR_VALUE(op.fd) ? NULL : handle->dmabuf;
	} else {
		/* if we get an error, the fd might be non-nvmap dmabuf fd */
		dmabuf = dma_buf_get(op.handle);
		if (IS_ERR(dmabuf))
			return PTR_ERR(dmabuf);
		op.fd = nvmap_dmabuf_duplicate_gen_fd(client, dmabuf);
	}

	return nvmap_install_fd(client, handle, op.fd, arg, &op, sizeof(op),
				0, dmabuf);
}

int nvmap_ioctl_alloc(struct file *filp, void __user *arg)
{
	struct nvmap_alloc_handle op;
	struct nvmap_client *client = filp->private_data;
	struct nvmap_handle *handle;
	int err;

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	if (op.align & (op.align - 1))
		return -EINVAL;

	handle = nvmap_handle_get_from_fd(op.handle);
	if (!handle)
		return -EINVAL;

	/* user-space handles are aligned to page boundaries, to prevent
	 * data leakage. */
	op.align = max_t(size_t, op.align, PAGE_SIZE);

	err = nvmap_alloc_handle(client, handle, op.heap_mask, op.align,
				  0, /* no kind */
				  op.flags & (~NVMAP_HANDLE_KIND_SPECIFIED),
				  NVMAP_IVM_INVALID_PEER);
	nvmap_handle_put(handle);
	return err;
}

int nvmap_ioctl_alloc_ivm(struct file *filp, void __user *arg)
{
	struct nvmap_alloc_ivm_handle op;
	struct nvmap_client *client = filp->private_data;
	struct nvmap_handle *handle;
	int err;

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	if (op.align & (op.align - 1))
		return -EINVAL;

	handle = nvmap_handle_get_from_fd(op.handle);
	if (!handle)
		return -EINVAL;

	/* user-space handles are aligned to page boundaries, to prevent
	 * data leakage. */
	op.align = max_t(size_t, op.align, PAGE_SIZE);

	err = nvmap_alloc_handle(client, handle, op.heap_mask, op.align,
				  0, /* no kind */
				  op.flags & (~NVMAP_HANDLE_KIND_SPECIFIED),
				  op.peer);
	nvmap_handle_put(handle);
	return err;
}

int nvmap_ioctl_vpr_floor_size(struct file *filp, void __user *arg)
{
	int err=0;
	u32 floor_size;

	if (copy_from_user(&floor_size, arg, sizeof(floor_size)))
		return -EFAULT;

	err = dma_set_resizable_heap_floor_size(&tegra_vpr_dev, floor_size);
	return err;
}

int nvmap_ioctl_create(struct file *filp, unsigned int cmd, void __user *arg)
{
	struct nvmap_create_handle op;
	struct nvmap_handle_ref *ref = NULL;
	struct nvmap_client *client = filp->private_data;
	struct dma_buf *dmabuf = NULL;
	struct nvmap_handle *handle = NULL;
	int fd;

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	if (!client)
		return -ENODEV;

	if (cmd == NVMAP_IOC_CREATE) {
		ref = nvmap_create_handle(client, PAGE_ALIGN(op.size));
		if (!IS_ERR(ref))
			ref->handle->orig_size = op.size;
	} else if (cmd == NVMAP_IOC_FROM_FD) {
		ref = nvmap_create_handle_from_fd(client, op.fd);

		/* if we get an error, the fd might be non-nvmap dmabuf fd */
		if (IS_ERR(ref)) {
			dmabuf = dma_buf_get(op.fd);
			if (IS_ERR(dmabuf))
				return PTR_ERR(dmabuf);
			fd = nvmap_dmabuf_duplicate_gen_fd(client, dmabuf);
			if (fd < 0)
				return fd;
		}
	} else {
		return -EINVAL;
	}

	if (!IS_ERR(ref)) {
		handle = ref->handle;
		dmabuf = handle->dmabuf;
		fd = nvmap_get_dmabuf_fd(client, ref->handle);
	} else if (!dmabuf) {
		return PTR_ERR(ref);
	}

	op.handle = fd;
	return nvmap_install_fd(client, handle, fd,
				arg, &op, sizeof(op), 1, dmabuf);
}

int nvmap_ioctl_create_from_va(struct file *filp, void __user *arg)
{
	int fd;
	int err;
	struct nvmap_create_handle_from_va op;
	struct nvmap_handle_ref *ref = NULL;
	struct nvmap_client *client = filp->private_data;

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	if (!client)
		return -ENODEV;

	ref = nvmap_create_handle_from_va(client, op.va, op.size);
	if (IS_ERR(ref))
		return PTR_ERR(ref);

	err = nvmap_alloc_handle_from_va(client, ref->handle,
					 op.va, op.flags);
	if (err) {
		nvmap_free_handle(client, ref->handle);
		return err;
	}

	fd = nvmap_get_dmabuf_fd(client, ref->handle);
	op.handle = fd;
	return nvmap_install_fd(client, ref->handle, fd,
				arg, &op, sizeof(op), 1, ref->handle->dmabuf);
}

int nvmap_ioctl_rw_handle(struct file *filp, int is_read, void __user *arg,
			  bool is32)
{
	struct nvmap_client *client = filp->private_data;
	struct nvmap_rw_handle __user *uarg = arg;
	struct nvmap_rw_handle op;
#ifdef CONFIG_COMPAT
	struct nvmap_rw_handle_32 __user *uarg32 = arg;
	struct nvmap_rw_handle_32 op32;
#endif
	struct nvmap_handle *h;
	ssize_t copied;
	int err = 0;

#ifdef CONFIG_COMPAT
	if (is32) {
		if (copy_from_user(&op32, arg, sizeof(op32)))
			return -EFAULT;
		op.addr = op32.addr;
		op.handle = op32.handle;
		op.offset = op32.offset;
		op.elem_size = op32.elem_size;
		op.hmem_stride = op32.hmem_stride;
		op.user_stride = op32.user_stride;
		op.count = op32.count;
	} else
#endif
		if (copy_from_user(&op, arg, sizeof(op)))
			return -EFAULT;

	if (!op.addr || !op.count || !op.elem_size)
		return -EINVAL;

	h = nvmap_handle_get_from_fd(op.handle);
	if (!h)
		return -EINVAL;

	nvmap_kmaps_inc(h);
	trace_nvmap_ioctl_rw_handle(client, h, is_read, op.offset,
				    op.addr, op.hmem_stride,
				    op.user_stride, op.elem_size, op.count);
	copied = rw_handle(client, h, is_read, op.offset,
			   (unsigned long)op.addr, op.hmem_stride,
			   op.user_stride, op.elem_size, op.count);
	nvmap_kmaps_dec(h);

	if (copied < 0) {
		err = copied;
		copied = 0;
	} else if (copied < (op.count * op.elem_size))
		err = -EINTR;

#ifdef CONFIG_COMPAT
	if (is32)
		__put_user(copied, &uarg32->count);
	else
#endif
		__put_user(copied, &uarg->count);

	nvmap_handle_put(h);

	return err;
}

int nvmap_ioctl_cache_maint(struct file *filp, void __user *arg, bool is32)
{
	struct nvmap_client *client = filp->private_data;
	struct nvmap_cache_op op;
#ifdef CONFIG_COMPAT
	struct nvmap_cache_op_32 op32;
#endif

#ifdef CONFIG_COMPAT
	if (is32) {
		if (copy_from_user(&op32, arg, sizeof(op32)))
			return -EFAULT;
		op.addr = op32.addr;
		op.handle = op32.handle;
		op.len = op32.len;
		op.op = op32.op;
	} else
#endif
		if (copy_from_user(&op, arg, sizeof(op)))
			return -EFAULT;

	return __nvmap_cache_maint(client, &op);
}

int nvmap_ioctl_free(struct file *filp, unsigned long arg)
{
	struct nvmap_client *client = filp->private_data;

	if (!arg)
		return 0;

	nvmap_free_handle_fd(client, arg);
	return sys_close(arg);
}

static ssize_t rw_handle(struct nvmap_client *client, struct nvmap_handle *h,
			 int is_read, unsigned long h_offs,
			 unsigned long sys_addr, unsigned long h_stride,
			 unsigned long sys_stride, unsigned long elem_size,
			 unsigned long count)
{
	ssize_t copied = 0;
	void *addr;
	int ret = 0;

	if (!(h->heap_type & nvmap_dev->cpu_access_mask))
		return -EPERM;

	if (!elem_size || !count)
		return -EINVAL;

	if (!h->alloc)
		return -EFAULT;

	if (elem_size == h_stride && elem_size == sys_stride) {
		elem_size *= count;
		h_stride = elem_size;
		sys_stride = elem_size;
		count = 1;
	}

	if (elem_size > h->size ||
		h_offs >= h->size ||
		elem_size > sys_stride ||
		elem_size > h_stride ||
		sys_stride > (h->size - h_offs) / count ||
		h_offs + h_stride * (count - 1) + elem_size > h->size)
		return -EINVAL;

	if (!h->vaddr) {
		if (!__nvmap_mmap(h))
			return -ENOMEM;
		__nvmap_munmap(h, h->vaddr);
	}

	addr = h->vaddr + h_offs;

	while (count--) {
		if (h_offs + elem_size > h->size) {
			pr_warn("read/write outside of handle\n");
			ret = -EFAULT;
			break;
		}
		if (is_read &&
		    !(h->userflags & NVMAP_HANDLE_CACHE_SYNC_AT_RESERVE))
			__nvmap_do_cache_maint(client, h, h_offs,
				h_offs + elem_size, NVMAP_CACHE_OP_INV, false);

		if (is_read)
			ret = copy_to_user((void *)sys_addr, addr, elem_size);
		else
			ret = copy_from_user(addr, (void *)sys_addr, elem_size);

		if (ret)
			break;

		if (!is_read &&
		    !(h->userflags & NVMAP_HANDLE_CACHE_SYNC_AT_RESERVE))
			__nvmap_do_cache_maint(client, h, h_offs,
				h_offs + elem_size, NVMAP_CACHE_OP_WB_INV,
				false);

		copied += elem_size;
		sys_addr += sys_stride;
		h_offs += h_stride;
		addr += h_stride;
	}

	return ret ?: copied;
}

int nvmap_ioctl_get_ivcid(struct file *filp, void __user *arg)
{
	struct nvmap_create_handle op;
	struct nvmap_handle *h = NULL;

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	h = nvmap_handle_get_from_fd(op.handle);
	if (!h)
		return -EINVAL;

	if (!h->alloc) { /* || !h->ivm_id) { */
		nvmap_handle_put(h);
		return -EFAULT;
	}

	op.id = h->ivm_id;

	nvmap_handle_put(h);

	return copy_to_user(arg, &op, sizeof(op)) ? -EFAULT : 0;
}

int nvmap_ioctl_get_ivc_heap(struct file *filp, void __user *arg)
{
	struct nvmap_device *dev = nvmap_dev;
	int i;
	unsigned int heap_mask = 0;

	for (i = 0; i < dev->nr_carveouts; i++) {
		struct nvmap_carveout_node *co_heap = &dev->heaps[i];
		int peer;

		if (!(co_heap->heap_bit & NVMAP_HEAP_CARVEOUT_IVM))
			continue;

		peer = nvmap_query_heap_peer(co_heap->carveout);
		if (peer < 0)
			return -EINVAL;

		heap_mask |= BIT(peer);
	}

	if (copy_to_user(arg, &heap_mask, sizeof(heap_mask)))
		return -EFAULT;

	return 0;
}

int nvmap_ioctl_create_from_ivc(struct file *filp, void __user *arg)
{
	struct nvmap_create_handle op;
	struct nvmap_handle_ref *ref;
	struct nvmap_client *client = filp->private_data;
	int fd;
	phys_addr_t offs;
	size_t size = 0;
	int peer;
	struct nvmap_heap_block *block = NULL;

	/* First create a new handle and then fake carveout allocation */
	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	if (!client)
		return -ENODEV;

	ref = nvmap_try_duplicate_by_ivmid(client, op.id, &block);
	if (!ref) {
		/*
		 * See nvmap_heap_alloc() for encoding details.
		 */
		offs = (((unsigned long)op.id &
			~(NVMAP_IVM_IVMID_MASK << NVMAP_IVM_IVMID_SHIFT)) >>
			NVMAP_IVM_LENGTH_WIDTH) << ffs(NVMAP_IVM_ALIGNMENT);
		size = (op.id &
			((1 << NVMAP_IVM_LENGTH_WIDTH) - 1)) << PAGE_SHIFT;
		peer = (op.id >> NVMAP_IVM_IVMID_SHIFT);

		ref = nvmap_create_handle(client, PAGE_ALIGN(size));
		if (IS_ERR(ref)) {
			nvmap_heap_free(block);
			return PTR_ERR(ref);
		}
		ref->handle->orig_size = size;

		ref->handle->peer = peer;
		if (!block)
			block = nvmap_carveout_alloc(client, ref->handle,
					NVMAP_HEAP_CARVEOUT_IVM, &offs);
		if (!block) {
			nvmap_free_handle(client, ref->handle);
			return -ENOMEM;
		}

		ref->handle->heap_type = NVMAP_HEAP_CARVEOUT_IVM;
		ref->handle->heap_pgalloc = false;
		ref->handle->ivm_id = op.id;
		ref->handle->carveout = block;
		block->handle = ref->handle;
		mb();
		ref->handle->alloc = true;
		NVMAP_TAG_TRACE(trace_nvmap_alloc_handle_done,
			NVMAP_TP_ARGS_CHR(client, ref->handle, ref));
	}

	fd = nvmap_get_dmabuf_fd(client, ref->handle);
	op.handle = fd;
	return nvmap_install_fd(client, ref->handle, fd,
				arg, &op, sizeof(op), 1, ref->handle->dmabuf);
}

int nvmap_ioctl_cache_maint_list(struct file *filp, void __user *arg,
				 bool is_reserve_ioctl)
{
	struct nvmap_cache_op_list op;
	u32 *handle_ptr;
	u32 *offset_ptr;
	u32 *size_ptr;
	struct nvmap_handle **refs;
	int err = 0;
	u32 i, n_unmarshal_handles = 0, count = 0;

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	if (!op.nr || op.nr > UINT_MAX / sizeof(u32))
		return -EINVAL;

	if (!access_ok(VERIFY_READ, op.handles, op.nr * sizeof(u32)))
		return -EFAULT;

	if (!access_ok(VERIFY_READ, op.offsets, op.nr * sizeof(u32)))
		return -EFAULT;

	if (!access_ok(VERIFY_READ, op.sizes, op.nr * sizeof(u32)))
		return -EFAULT;

	if (!op.offsets || !op.sizes)
		return -EINVAL;

	refs = kcalloc(op.nr, sizeof(*refs), GFP_KERNEL);

	if (!refs)
		return -ENOMEM;

	handle_ptr = (u32 *)(uintptr_t)op.handles;
	offset_ptr = (u32 *)(uintptr_t)op.offsets;
	size_ptr = (u32 *)(uintptr_t)op.sizes;

	for (i = 0; i < op.nr; i++) {
		u32 handle;

		if (copy_from_user(&handle, &handle_ptr[i], sizeof(handle))) {
			err = -EFAULT;
			goto free_mem;
		}

		refs[i] = nvmap_handle_get_from_fd(handle);
		if (!refs[i]) {
			err = -EINVAL;
			goto free_mem;
		}
		if (!(refs[i]->heap_type & nvmap_dev->cpu_access_mask)) {
			err = -EPERM;
			goto free_mem;
		}

		n_unmarshal_handles++;
	}

	/*
	 * Either all handles should have NVMAP_HANDLE_CACHE_SYNC_AT_RESERVE
	 * or none should have it.
	 */
	for (i = 0; i < op.nr; i++)
		if (refs[i]->userflags & NVMAP_HANDLE_CACHE_SYNC_AT_RESERVE)
			count++;

	if (count % op.nr) {
		err = -EINVAL;
		goto free_mem;
	}

	/*
	 * when NVMAP_HANDLE_CACHE_SYNC_AT_RESERVE is specified mix can cause
	 * cache WB_INV at unreserve op on iovmm handles increasing overhead.
	 * So, either all handles should have pages from carveout or from iovmm.
	 */
	if (count) {
		for (i = 0; i < op.nr; i++)
			if (refs[i]->heap_pgalloc)
				count++;

		if (count % op.nr) {
			err = -EINVAL;
			goto free_mem;
		}
	}

	if (is_reserve_ioctl)
		err = nvmap_reserve_pages(refs, offset_ptr, size_ptr,
					  op.nr, op.op);
	else
		err = nvmap_do_cache_maint_list(refs, offset_ptr, size_ptr,
						op.op, op.nr);

free_mem:
	for (i = 0; i < n_unmarshal_handles; i++)
		nvmap_handle_put(refs[i]);
	kfree(refs);
	return err;
}

int nvmap_ioctl_gup_test(struct file *filp, void __user *arg)
{
	int i, err = -EINVAL;
	struct nvmap_gup_test op;
	struct vm_area_struct *vma;
	struct nvmap_handle *handle;
	int nr_page;
	int user_pages;
	struct page **pages;

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	op.result = 1;
	vma = find_vma(current->mm, op.va);
	if (unlikely(!vma) || (unlikely(op.va < vma->vm_start )) ||
	    unlikely(op.va >= vma->vm_end))
		goto exit;

	handle = nvmap_handle_get_from_fd(op.handle);
	if (!handle)
		goto exit;

	if (vma->vm_end - vma->vm_start != handle->size) {
		pr_err("handle size(0x%zx) and vma size(0x%lx) don't match\n",
			 handle->size, vma->vm_end - vma->vm_start);
		goto put_handle;
	}

	err = -ENOMEM;
	nr_page = handle->size >> PAGE_SHIFT;
	pages = nvmap_altalloc(nr_page * sizeof(*pages));
	if (!pages)
		goto put_handle;

	user_pages = get_user_pages(current, current->mm,
					op.va & PAGE_MASK, nr_page,
					1 /*write*/, 1 /* force */,
					pages, NULL);
	if (user_pages != nr_page)
		goto put_user_pages;

	for (i = 0; i < nr_page; i++) {
		if (handle->pgalloc.pages[i] != pages[i]) {
			pr_err("page pointers don't match, %p %p\n",
			       handle->pgalloc.pages[i], pages[i]);
			op.result = 0;
		}
	}

	if (op.result)
		err = 0;

	if (copy_to_user(arg, &op, sizeof(op)))
		err = -EFAULT;

put_user_pages:
	pr_info("get_user_pages requested/got: %d/%d]\n", nr_page, user_pages);
	while (--user_pages >= 0)
		put_page(pages[user_pages]);
	nvmap_altfree(pages, nr_page * sizeof(*pages));
put_handle:
	nvmap_handle_put(handle);
exit:
	pr_info("GUP Test %s\n", err ? "failed" : "passed");
	return err;
}

int nvmap_ioctl_set_tag_label(struct file *filp, void __user *arg)
{
	struct nvmap_set_tag_label op;
	struct nvmap_device *dev = nvmap_dev;
	int err;

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	if (op.len > NVMAP_TAG_LABEL_MAXLEN)
		op.len = NVMAP_TAG_LABEL_MAXLEN;

	if (op.len)
		err = nvmap_define_tag(dev, op.tag,
			(const char __user *)op.addr, op.len);
	else
		err = nvmap_remove_tag(dev, op.tag);

	return err;
}
