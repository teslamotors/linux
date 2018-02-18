/*
 * GK20A Address Spaces
 *
 * Copyright (c) 2011-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

#include <trace/events/gk20a.h>

#include <uapi/linux/nvgpu.h>

#include "gk20a.h"

/* dumb allocator... */
static int generate_as_share_id(struct gk20a_as *as)
{
	gk20a_dbg_fn("");
	return ++as->last_share_id;
}
/* still dumb */
static void release_as_share_id(struct gk20a_as *as, int id)
{
	gk20a_dbg_fn("");
	return;
}

int gk20a_as_alloc_share(struct gk20a_as *as,
			 u32 big_page_size, u32 flags,
			 struct gk20a_as_share **out)
{
	struct gk20a *g = gk20a_from_as(as);
	struct gk20a_as_share *as_share;
	int err = 0;

	gk20a_dbg_fn("");

	*out = NULL;
	as_share = kzalloc(sizeof(*as_share), GFP_KERNEL);
	if (!as_share)
		return -ENOMEM;

	as_share->as = as;
	as_share->id = generate_as_share_id(as_share->as);

	/* this will set as_share->vm. */
	err = gk20a_busy(g->dev);
	if (err)
		goto failed;
	err = g->ops.mm.vm_alloc_share(as_share, big_page_size, flags);
	gk20a_idle(g->dev);

	if (err)
		goto failed;

	*out = as_share;
	return 0;

failed:
	kfree(as_share);
	return err;
}

/*
 * channels and the device nodes call this to release.
 * once the ref_cnt hits zero the share is deleted.
 */
int gk20a_as_release_share(struct gk20a_as_share *as_share)
{
	struct gk20a *g = as_share->vm->mm->g;
	int err;

	gk20a_dbg_fn("");

	err = gk20a_busy(g->dev);
	if (err)
		return err;

	err = gk20a_vm_release_share(as_share);

	gk20a_idle(g->dev);

	release_as_share_id(as_share->as, as_share->id);
	kfree(as_share);
	return err;
}

static int gk20a_as_ioctl_bind_channel(
		struct gk20a_as_share *as_share,
		struct nvgpu_as_bind_channel_args *args)
{
	int err = 0;
	struct channel_gk20a *ch;

	gk20a_dbg_fn("");

	ch = gk20a_get_channel_from_file(args->channel_fd);
	if (!ch || gk20a_channel_as_bound(ch))
		return -EINVAL;

	/* this will set channel_gk20a->vm */
	err = ch->g->ops.mm.vm_bind_channel(as_share, ch);
	if (err)
		return err;

	return err;
}

static int gk20a_as_ioctl_alloc_space(
		struct gk20a_as_share *as_share,
		struct nvgpu_as_alloc_space_args *args)
{
	gk20a_dbg_fn("");
	return gk20a_vm_alloc_space(as_share, args);
}

static int gk20a_as_ioctl_free_space(
		struct gk20a_as_share *as_share,
		struct nvgpu_as_free_space_args *args)
{
	gk20a_dbg_fn("");
	return gk20a_vm_free_space(as_share, args);
}

static int gk20a_as_ioctl_map_buffer_ex(
		struct gk20a_as_share *as_share,
		struct nvgpu_as_map_buffer_ex_args *args)
{
	gk20a_dbg_fn("");

	return gk20a_vm_map_buffer(as_share->vm, args->dmabuf_fd,
				   &args->offset, args->flags,
				   args->kind,
				   args->buffer_offset,
				   args->mapping_size,
				   NULL);
}

static int gk20a_as_ioctl_map_buffer(
		struct gk20a_as_share *as_share,
		struct nvgpu_as_map_buffer_args *args)
{
	gk20a_dbg_fn("");
	return gk20a_vm_map_buffer(as_share->vm, args->dmabuf_fd,
				   &args->o_a.offset,
				   args->flags, NV_KIND_DEFAULT,
				   0, 0, NULL);
	/* args->o_a.offset will be set if !err */
}

static int gk20a_as_ioctl_unmap_buffer(
		struct gk20a_as_share *as_share,
		struct nvgpu_as_unmap_buffer_args *args)
{
	gk20a_dbg_fn("");
	return gk20a_vm_unmap_buffer(as_share->vm, args->offset, NULL);
}

static int gk20a_as_ioctl_map_buffer_batch(
	struct gk20a_as_share *as_share,
	struct nvgpu_as_map_buffer_batch_args *args)
{
	struct gk20a *g = as_share->vm->mm->g;
	u32 i;
	int err = 0;

	struct nvgpu_as_unmap_buffer_args __user *user_unmap_args =
		(struct nvgpu_as_unmap_buffer_args __user *)(uintptr_t)
		args->unmaps;
	struct nvgpu_as_map_buffer_ex_args __user *user_map_args =
		(struct nvgpu_as_map_buffer_ex_args __user *)(uintptr_t)
		args->maps;

	struct vm_gk20a_mapping_batch batch;

	gk20a_dbg_fn("");

	if (args->num_unmaps > g->gpu_characteristics.map_buffer_batch_limit ||
	    args->num_maps > g->gpu_characteristics.map_buffer_batch_limit)
		return -EINVAL;

	gk20a_vm_mapping_batch_start(&batch);

	for (i = 0; i < args->num_unmaps; ++i) {
		struct nvgpu_as_unmap_buffer_args unmap_args;

		if (copy_from_user(&unmap_args, &user_unmap_args[i],
				   sizeof(unmap_args))) {
			err = -EFAULT;
			break;
		}

		err = gk20a_vm_unmap_buffer(as_share->vm, unmap_args.offset,
					    &batch);
		if (err)
			break;
	}

	if (err) {
		gk20a_vm_mapping_batch_finish(as_share->vm, &batch);

		args->num_unmaps = i;
		args->num_maps = 0;
		return err;
	}

	for (i = 0; i < args->num_maps; ++i) {
		struct nvgpu_as_map_buffer_ex_args map_args;
		memset(&map_args, 0, sizeof(map_args));

		if (copy_from_user(&map_args, &user_map_args[i],
				   sizeof(map_args))) {
			err = -EFAULT;
			break;
		}

		err = gk20a_vm_map_buffer(
			as_share->vm, map_args.dmabuf_fd,
			&map_args.offset, map_args.flags,
			map_args.kind,
			map_args.buffer_offset,
			map_args.mapping_size,
			&batch);
		if (err)
			break;
	}

	gk20a_vm_mapping_batch_finish(as_share->vm, &batch);

	if (err)
		args->num_maps = i;
	/* note: args->num_unmaps will be unmodified, which is ok
	 * since all unmaps are done */

	return err;
}

static int gk20a_as_ioctl_get_va_regions(
		struct gk20a_as_share *as_share,
		struct nvgpu_as_get_va_regions_args *args)
{
	unsigned int i;
	unsigned int write_entries;
	struct nvgpu_as_va_region __user *user_region_ptr;
	struct vm_gk20a *vm = as_share->vm;
	int page_sizes = gmmu_page_size_kernel;

	gk20a_dbg_fn("");

	if (!vm->big_pages)
		page_sizes--;

	write_entries = args->buf_size / sizeof(struct nvgpu_as_va_region);
	if (write_entries > page_sizes)
		write_entries = page_sizes;

	user_region_ptr =
		(struct nvgpu_as_va_region __user *)(uintptr_t)args->buf_addr;

	for (i = 0; i < write_entries; ++i) {
		struct nvgpu_as_va_region region;
		struct gk20a_allocator *vma =
			gk20a_alloc_initialized(&vm->fixed) ?
			&vm->fixed : &vm->vma[i];

		memset(&region, 0, sizeof(struct nvgpu_as_va_region));

		region.page_size = vm->gmmu_page_sizes[i];
		region.offset = gk20a_alloc_base(vma);
		/* No __aeabi_uldivmod() on some platforms... */
		region.pages = (gk20a_alloc_end(vma) -
			gk20a_alloc_base(vma)) >> ilog2(region.page_size);

		if (copy_to_user(user_region_ptr + i, &region, sizeof(region)))
			return -EFAULT;
	}

	args->buf_size =
		page_sizes * sizeof(struct nvgpu_as_va_region);

	return 0;
}

static int gk20a_as_ioctl_get_buffer_compbits_info(
		struct gk20a_as_share *as_share,
		struct nvgpu_as_get_buffer_compbits_info_args *args)
{
	gk20a_dbg_fn("");
	return gk20a_vm_get_compbits_info(as_share->vm,
					  args->mapping_gva,
					  &args->compbits_win_size,
					  &args->compbits_win_ctagline,
					  &args->mapping_ctagline,
					  &args->flags);
}

static int gk20a_as_ioctl_map_buffer_compbits(
		struct gk20a_as_share *as_share,
		struct nvgpu_as_map_buffer_compbits_args *args)
{
	gk20a_dbg_fn("");
	return gk20a_vm_map_compbits(as_share->vm,
				     args->mapping_gva,
				     &args->compbits_win_gva,
				     &args->mapping_iova,
				     args->flags);
}

int gk20a_as_dev_open(struct inode *inode, struct file *filp)
{
	struct gk20a_as_share *as_share;
	struct gk20a *g;
	int err;

	gk20a_dbg_fn("");

	g = container_of(inode->i_cdev, struct gk20a, as.cdev);

	err = gk20a_as_alloc_share(&g->as, 0, 0, &as_share);
	if (err) {
		gk20a_dbg_fn("failed to alloc share");
		return err;
	}

	filp->private_data = as_share;
	return 0;
}

int gk20a_as_dev_release(struct inode *inode, struct file *filp)
{
	struct gk20a_as_share *as_share = filp->private_data;

	gk20a_dbg_fn("");

	if (!as_share)
		return 0;

	return gk20a_as_release_share(as_share);
}

long gk20a_as_dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct gk20a_as_share *as_share = filp->private_data;
	struct gk20a *g = gk20a_from_as(as_share->as);

	u8 buf[NVGPU_AS_IOCTL_MAX_ARG_SIZE];

	if ((_IOC_TYPE(cmd) != NVGPU_AS_IOCTL_MAGIC) ||
		(_IOC_NR(cmd) == 0) ||
		(_IOC_NR(cmd) > NVGPU_AS_IOCTL_LAST))
		return -EINVAL;

	BUG_ON(_IOC_SIZE(cmd) > NVGPU_AS_IOCTL_MAX_ARG_SIZE);

	memset(buf, 0, sizeof(buf));
	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if (copy_from_user(buf, (void __user *)arg, _IOC_SIZE(cmd)))
			return -EFAULT;
	}

	err = gk20a_busy(g->dev);
	if (err)
		return err;

	switch (cmd) {
	case NVGPU_AS_IOCTL_BIND_CHANNEL:
		trace_gk20a_as_ioctl_bind_channel(dev_name(dev_from_gk20a(g)));
		err = gk20a_as_ioctl_bind_channel(as_share,
			       (struct nvgpu_as_bind_channel_args *)buf);

		break;
	case NVGPU32_AS_IOCTL_ALLOC_SPACE:
	{
		struct nvgpu32_as_alloc_space_args *args32 =
			(struct nvgpu32_as_alloc_space_args *)buf;
		struct nvgpu_as_alloc_space_args args;

		args.pages = args32->pages;
		args.page_size = args32->page_size;
		args.flags = args32->flags;
		args.o_a.offset = args32->o_a.offset;
		trace_gk20a_as_ioctl_alloc_space(dev_name(dev_from_gk20a(g)));
		err = gk20a_as_ioctl_alloc_space(as_share, &args);
		args32->o_a.offset = args.o_a.offset;
		break;
	}
	case NVGPU_AS_IOCTL_ALLOC_SPACE:
		trace_gk20a_as_ioctl_alloc_space(dev_name(dev_from_gk20a(g)));
		err = gk20a_as_ioctl_alloc_space(as_share,
				(struct nvgpu_as_alloc_space_args *)buf);
		break;
	case NVGPU_AS_IOCTL_FREE_SPACE:
		trace_gk20a_as_ioctl_free_space(dev_name(dev_from_gk20a(g)));
		err = gk20a_as_ioctl_free_space(as_share,
				(struct nvgpu_as_free_space_args *)buf);
		break;
	case NVGPU_AS_IOCTL_MAP_BUFFER:
		trace_gk20a_as_ioctl_map_buffer(dev_name(dev_from_gk20a(g)));
		err = gk20a_as_ioctl_map_buffer(as_share,
				(struct nvgpu_as_map_buffer_args *)buf);
		break;
	case NVGPU_AS_IOCTL_MAP_BUFFER_EX:
		trace_gk20a_as_ioctl_map_buffer(dev_name(dev_from_gk20a(g)));
		err = gk20a_as_ioctl_map_buffer_ex(as_share,
				(struct nvgpu_as_map_buffer_ex_args *)buf);
		break;
	case NVGPU_AS_IOCTL_UNMAP_BUFFER:
		trace_gk20a_as_ioctl_unmap_buffer(dev_name(dev_from_gk20a(g)));
		err = gk20a_as_ioctl_unmap_buffer(as_share,
				(struct nvgpu_as_unmap_buffer_args *)buf);
		break;
	case NVGPU_AS_IOCTL_GET_VA_REGIONS:
		trace_gk20a_as_ioctl_get_va_regions(dev_name(dev_from_gk20a(g)));
		err = gk20a_as_ioctl_get_va_regions(as_share,
				(struct nvgpu_as_get_va_regions_args *)buf);
		break;
	case NVGPU_AS_IOCTL_GET_BUFFER_COMPBITS_INFO:
		err = gk20a_as_ioctl_get_buffer_compbits_info(as_share,
				(struct nvgpu_as_get_buffer_compbits_info_args *)buf);
		break;
	case NVGPU_AS_IOCTL_MAP_BUFFER_COMPBITS:
		err = gk20a_as_ioctl_map_buffer_compbits(as_share,
				(struct nvgpu_as_map_buffer_compbits_args *)buf);
		break;
	case NVGPU_AS_IOCTL_MAP_BUFFER_BATCH:
		err = gk20a_as_ioctl_map_buffer_batch(as_share,
				(struct nvgpu_as_map_buffer_batch_args *)buf);
		break;
	default:
		dev_dbg(dev_from_gk20a(g), "unrecognized as ioctl: 0x%x", cmd);
		err = -ENOTTY;
		break;
	}

	gk20a_idle(g->dev);

	if ((err == 0) && (_IOC_DIR(cmd) & _IOC_READ))
		if (copy_to_user((void __user *)arg, buf, _IOC_SIZE(cmd)))
			err = -EFAULT;

	return err;
}
