/*
 * drivers/video/tegra/nvmap/nvmap_mm.c
 *
 * Some MM related functionality specific to nvmap.
 *
 * Copyright (c) 2013-2017, NVIDIA CORPORATION. All rights reserved.
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

#include <trace/events/nvmap.h>

#include <asm/pgtable.h>

#include "nvmap_priv.h"

enum NVMAP_PROT_OP {
	NVMAP_HANDLE_PROT_NONE = 1,
	NVMAP_HANDLE_PROT_RESTORE = 2,
};

void nvmap_zap_handle(struct nvmap_handle *handle, u32 offset, u32 size)
{
	struct list_head *vmas;
	struct nvmap_vma_list *vma_list;
	struct vm_area_struct *vma;

	if (!handle->heap_pgalloc)
		return;

	/* if no dirty page is present, no need to zap */
	if (nvmap_handle_track_dirty(handle) && !atomic_read(&handle->pgalloc.ndirty))
		return;

	if (!size) {
		offset = 0;
		size = handle->size;
	}

	size = PAGE_ALIGN((offset & ~PAGE_MASK) + size);

	mutex_lock(&handle->lock);
	vmas = &handle->vmas;
	list_for_each_entry(vma_list, vmas, list) {
		struct nvmap_vma_priv *priv;
		u32 vm_size = size;

		vma = vma_list->vma;
		priv = vma->vm_private_data;
		if ((offset + size) > (vma->vm_end - vma->vm_start))
			vm_size = vma->vm_end - vma->vm_start - offset;
		if (priv->offs || vma->vm_pgoff)
			/* vma mapping starts in the middle of handle memory.
			 * zapping needs special care. zap entire range for now.
			 * FIXME: optimze zapping.
			 */
			zap_page_range(vma, vma->vm_start,
				vma->vm_end - vma->vm_start, NULL);
		else
			zap_page_range(vma, vma->vm_start + offset,
				vm_size, NULL);
	}
	mutex_unlock(&handle->lock);
}

static int nvmap_prot_handle(struct nvmap_handle *handle, u32 offset,
		u32 size, int op)
{
	struct list_head *vmas;
	struct nvmap_vma_list *vma_list;
	struct vm_area_struct *vma;
	int err = -EINVAL;

	if (!handle->heap_pgalloc)
		return err;

	if ((offset >= handle->size) || (offset > handle->size - size) ||
	    (size > handle->size))
		return err;

	if (!size)
		size = handle->size;

	size = PAGE_ALIGN((offset & ~PAGE_MASK) + size);

	mutex_lock(&handle->lock);
	vmas = &handle->vmas;
	list_for_each_entry(vma_list, vmas, list) {
		struct nvmap_vma_priv *priv;
		u32 vm_size = size;
		struct vm_area_struct *prev;

		vma = vma_list->vma;
		prev = vma->vm_prev;
		priv = vma->vm_private_data;
		if ((offset + size) > (vma->vm_end - vma->vm_start))
			vm_size = vma->vm_end - vma->vm_start - offset;

		if ((priv->offs || vma->vm_pgoff) ||
		    (size > (vma->vm_end - vma->vm_start)))
			vm_size = vma->vm_end - vma->vm_start;
		if (vma->vm_mm != current->mm)
			down_write(&vma->vm_mm->mmap_sem);
		switch (op) {
		case NVMAP_HANDLE_PROT_NONE:
			vma->vm_flags = vma_list->save_vm_flags;
			(void)vma_set_page_prot(vma);
			if (nvmap_handle_track_dirty(handle) &&
			    !atomic_read(&handle->pgalloc.ndirty)) {
				err = 0;
				break;
			}
			err = mprotect_fixup(vma, &prev, vma->vm_start,
					vma->vm_start + vm_size, VM_NONE);
			if (err)
				goto try_unlock;
			vma->vm_flags = vma_list->save_vm_flags;
			(void)vma_set_page_prot(vma);
			break;
		case NVMAP_HANDLE_PROT_RESTORE:
			vma->vm_flags = VM_NONE;
			(void)vma_set_page_prot(vma);
			err = mprotect_fixup(vma, &prev, vma->vm_start,
					vma->vm_start + vm_size,
					vma_list->save_vm_flags);
			if (err)
				goto try_unlock;
			_nvmap_handle_mkdirty(handle, 0, size);
			break;
		default:
			BUG();
		};
try_unlock:
		if (vma->vm_mm != current->mm)
			up_write(&vma->vm_mm->mmap_sem);
		if (err)
			goto finish;
	}
finish:
	mutex_unlock(&handle->lock);
	return err;
}

static int nvmap_prot_handles(struct nvmap_handle **handles, u32 *offsets,
		       u32 *sizes, u32 nr, int op)
{
	int i, err = 0;

	down_write(&current->mm->mmap_sem);
	for (i = 0; i < nr; i++) {
		err = nvmap_prot_handle(handles[i], offsets[i],
				sizes[i], op);
		if (err)
			goto finish;
	}
finish:
	up_write(&current->mm->mmap_sem);
	return err;
}

int nvmap_reserve_pages(struct nvmap_handle **handles, u32 *offsets, u32 *sizes,
			u32 nr, u32 op)
{
	int i, err;

	for (i = 0; i < nr; i++) {
		u32 size = sizes[i] ? sizes[i] : handles[i]->size;
		u32 offset = sizes[i] ? offsets[i] : 0;

		if ((offset != 0) || (size != handles[i]->size))
			return -EINVAL;

		if (op == NVMAP_PAGES_PROT_AND_CLEAN)
			continue;

		/*
		 * NOTE: This unreserves the handle even when
		 * NVMAP_PAGES_INSERT_ON_UNRESERVE is called on some portion
		 * of the handle
		 */
		atomic_set(&handles[i]->pgalloc.reserved,
				(op == NVMAP_PAGES_RESERVE) ? 1 : 0);
	}

	if (op == NVMAP_PAGES_PROT_AND_CLEAN)
		op = NVMAP_PAGES_RESERVE;

	switch (op) {
	case NVMAP_PAGES_RESERVE:
		err = nvmap_prot_handles(handles, offsets, sizes, nr,
						NVMAP_HANDLE_PROT_NONE);
		if (err)
			return err;
		break;
	case NVMAP_INSERT_PAGES_ON_UNRESERVE:
		err = nvmap_prot_handles(handles, offsets, sizes, nr,
						NVMAP_HANDLE_PROT_RESTORE);
		if (err)
			return err;
		break;
	case NVMAP_PAGES_UNRESERVE:
		for (i = 0; i < nr; i++)
			if (nvmap_handle_track_dirty(handles[i]))
				atomic_set(&handles[i]->pgalloc.ndirty, 0);
		break;
	default:
		return -EINVAL;
	}

	if (!(handles[0]->userflags & NVMAP_HANDLE_CACHE_SYNC_AT_RESERVE))
		return 0;

	if (op == NVMAP_PAGES_RESERVE) {
		err = nvmap_do_cache_maint_list(handles, offsets, sizes,
					  NVMAP_CACHE_OP_WB, nr);
		if (err)
			return err;
		for (i = 0; i < nr; i++)
			nvmap_handle_mkclean(handles[i], offsets[i],
					     sizes[i] ? sizes[i] : handles[i]->size);
	} else if ((op == NVMAP_PAGES_UNRESERVE) && handles[0]->heap_pgalloc) {
		/* Do nothing */
	} else {
		err = nvmap_do_cache_maint_list(handles, offsets, sizes,
					  NVMAP_CACHE_OP_WB_INV, nr);
		if (err)
			return err;
	}
	return 0;
}

