/*
 * Virtualized GPU Memory Management
 *
 * Copyright (c) 2014-2017, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/dma-mapping.h>
#include "vgpu/vgpu.h"
#include "gk20a/semaphore_gk20a.h"
#include "gk20a/mm_gk20a.h"

static int vgpu_init_mm_setup_sw(struct gk20a *g)
{
	struct mm_gk20a *mm = &g->mm;
	struct vm_gk20a *vm = &mm->pmu.vm;
	u32 big_page_size = gk20a_get_platform(g->dev)->default_big_page_size;

	gk20a_dbg_fn("");

	if (mm->sw_ready) {
		gk20a_dbg_fn("skip init");
		return 0;
	}

	mm->g = g;

	/*TBD: make channel vm size configurable */
	mm->channel.user_size = NV_MM_DEFAULT_USER_SIZE;
	mm->channel.kernel_size = NV_MM_DEFAULT_KERNEL_SIZE;

	gk20a_dbg_info("channel vm size: user %dMB  kernel %dMB",
		       (int)(mm->channel.user_size >> 20),
		       (int)(mm->channel.kernel_size >> 20));

	/* gk20a_init_gpu_characteristics expects this to be populated */
	vm->big_page_size = big_page_size;
	vm->mmu_levels = (vm->big_page_size == SZ_64K) ?
			 gk20a_mm_levels_64k : gk20a_mm_levels_128k;

	mm->sw_ready = true;

	return 0;
}

int vgpu_init_mm_support(struct gk20a *g)
{
	int err;

	gk20a_dbg_fn("");

	err = vgpu_init_mm_setup_sw(g);
	if (err)
		return err;

	if (g->ops.mm.init_mm_setup_hw)
		err = g->ops.mm.init_mm_setup_hw(g);

	return err;
}

static u64 vgpu_locked_gmmu_map(struct vm_gk20a *vm,
				u64 map_offset,
				struct sg_table *sgt,
				u64 buffer_offset,
				u64 size,
				int pgsz_idx,
				u8 kind_v,
				u32 ctag_offset,
				u32 flags,
				int rw_flag,
				bool clear_ctags,
				bool sparse,
				bool priv,
				struct vm_gk20a_mapping_batch *batch,
				enum gk20a_aperture aperture)
{
	int err = 0;
	struct device *d = dev_from_vm(vm);
	struct gk20a *g = gk20a_from_vm(vm);
	struct gk20a_platform *platform = gk20a_get_platform(g->dev);
	struct dma_iommu_mapping *mapping = to_dma_iommu_mapping(d);
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_as_map_params *p = &msg.params.as_map;
	u64 addr = g->ops.mm.get_iova_addr(g, sgt->sgl, flags);
	u8 prot;

	gk20a_dbg_fn("");

	/* Allocate (or validate when map_offset != 0) the virtual address. */
	if (!map_offset) {
		map_offset = gk20a_vm_alloc_va(vm, size,
					  pgsz_idx);
		if (!map_offset) {
			gk20a_err(d, "failed to allocate va space\n");
			err = -ENOMEM;
			goto fail;
		}
	}

	if (rw_flag == gk20a_mem_flag_read_only)
		prot = TEGRA_VGPU_MAP_PROT_READ_ONLY;
	else if (rw_flag == gk20a_mem_flag_write_only)
		prot = TEGRA_VGPU_MAP_PROT_WRITE_ONLY;
	else
		prot = TEGRA_VGPU_MAP_PROT_NONE;

	msg.cmd = TEGRA_VGPU_CMD_AS_MAP;
	msg.handle = platform->virt_handle;
	p->handle = vm->handle;
	p->addr = addr;
	p->gpu_va = map_offset;
	p->size = size;
	if (pgsz_idx == gmmu_page_size_kernel) {
		u32 page_size = vm->gmmu_page_sizes[pgsz_idx];

		if (page_size == vm->gmmu_page_sizes[gmmu_page_size_small]) {
			pgsz_idx = gmmu_page_size_small;
		} else if (page_size ==
				vm->gmmu_page_sizes[gmmu_page_size_big]) {
			pgsz_idx = gmmu_page_size_big;
		} else {
			gk20a_err(d, "invalid kernel page size %d\n",
				page_size);
			goto fail;
		}
	}
	p->pgsz_idx = pgsz_idx;
	p->iova = mapping ? 1 : 0;
	p->kind = kind_v;
	p->cacheable =
		(flags & NVGPU_MAP_BUFFER_FLAGS_CACHEABLE_TRUE) ? 1 : 0;
	p->prot = prot;
	p->ctag_offset = ctag_offset;
	p->clear_ctags = clear_ctags;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err ? err : msg.ret;
	if (err)
		goto fail;

	/* TLB invalidate handled on server side */

	return map_offset;
fail:
	gk20a_err(d, "%s: failed with err=%d\n", __func__, err);
	return 0;
}

static void vgpu_locked_gmmu_unmap(struct vm_gk20a *vm,
				u64 vaddr,
				u64 size,
				int pgsz_idx,
				bool va_allocated,
				int rw_flag,
				bool sparse,
				struct vm_gk20a_mapping_batch *batch)
{
	struct gk20a *g = gk20a_from_vm(vm);
	struct gk20a_platform *platform = gk20a_get_platform(g->dev);
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_as_map_params *p = &msg.params.as_map;
	int err;

	gk20a_dbg_fn("");

	if (va_allocated) {
		err = gk20a_vm_free_va(vm, vaddr, size, pgsz_idx);
		if (err) {
			dev_err(dev_from_vm(vm),
				"failed to free va");
			return;
		}
	}

	msg.cmd = TEGRA_VGPU_CMD_AS_UNMAP;
	msg.handle = platform->virt_handle;
	p->handle = vm->handle;
	p->gpu_va = vaddr;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	if (err || msg.ret)
		dev_err(dev_from_vm(vm),
			"failed to update gmmu ptes on unmap");

	/* TLB invalidate handled on server side */
}

static void vgpu_vm_remove_support(struct vm_gk20a *vm)
{
	struct gk20a *g = vm->mm->g;
	struct gk20a_platform *platform = gk20a_get_platform(g->dev);
	struct mapped_buffer_node *mapped_buffer;
	struct vm_reserved_va_node *va_node, *va_node_tmp;
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_as_share_params *p = &msg.params.as_share;
	struct rb_node *node;
	int err;

	gk20a_dbg_fn("");
	mutex_lock(&vm->update_gmmu_lock);

	/* TBD: add a flag here for the unmap code to recognize teardown
	 * and short-circuit any otherwise expensive operations. */

	node = rb_first(&vm->mapped_buffers);
	while (node) {
		mapped_buffer =
			container_of(node, struct mapped_buffer_node, node);
		gk20a_vm_unmap_locked(mapped_buffer, NULL);
		node = rb_first(&vm->mapped_buffers);
	}

	/* destroy remaining reserved memory areas */
	list_for_each_entry_safe(va_node, va_node_tmp, &vm->reserved_va_list,
		reserved_va_list) {
		list_del(&va_node->reserved_va_list);
		kfree(va_node);
	}

	msg.cmd = TEGRA_VGPU_CMD_AS_FREE_SHARE;
	msg.handle = platform->virt_handle;
	p->handle = vm->handle;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	WARN_ON(err || msg.ret);

	gk20a_alloc_destroy(&vm->vma[gmmu_page_size_kernel]);
	if (gk20a_alloc_initialized(&vm->vma[gmmu_page_size_small]))
		gk20a_alloc_destroy(&vm->vma[gmmu_page_size_small]);
	if (gk20a_alloc_initialized(&vm->vma[gmmu_page_size_big]))
		gk20a_alloc_destroy(&vm->vma[gmmu_page_size_big]);

	mutex_unlock(&vm->update_gmmu_lock);

	/* vm is not used anymore. release it. */
	kfree(vm);
}

u64 vgpu_bar1_map(struct gk20a *g, struct sg_table **sgt, u64 size)
{
	struct gk20a_platform *platform = gk20a_get_platform(g->dev);
	struct dma_iommu_mapping *mapping =
			to_dma_iommu_mapping(dev_from_gk20a(g));
	u64 addr = g->ops.mm.get_iova_addr(g, (*sgt)->sgl, 0);
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_as_map_params *p = &msg.params.as_map;
	int err;

	msg.cmd = TEGRA_VGPU_CMD_MAP_BAR1;
	msg.handle = platform->virt_handle;
	p->addr = addr;
	p->size = size;
	p->iova = mapping ? 1 : 0;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	if (err || msg.ret)
		addr = 0;
	else
		addr = p->gpu_va;

	return addr;
}

/* address space interfaces for the gk20a module */
static int vgpu_vm_alloc_share(struct gk20a_as_share *as_share,
			       u32 big_page_size, u32 flags)
{
	struct gk20a_as *as = as_share->as;
	struct gk20a *g = gk20a_from_as(as);
	struct gk20a_platform *platform = gk20a_get_platform(g->dev);
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_as_share_params *p = &msg.params.as_share;
	struct mm_gk20a *mm = &g->mm;
	struct vm_gk20a *vm;
	u64 small_vma_start, small_vma_limit, large_vma_start, large_vma_limit,
		kernel_vma_start, kernel_vma_limit;
	char name[32];
	int err, i;
	const bool userspace_managed =
		(flags & NVGPU_GPU_IOCTL_ALLOC_AS_FLAGS_USERSPACE_MANAGED) != 0;

	/* note: keep the page sizes sorted lowest to highest here */
	u32 gmmu_page_sizes[gmmu_nr_page_sizes] = {
		SZ_4K,
		big_page_size ? big_page_size : platform->default_big_page_size,
		SZ_4K
	};

	gk20a_dbg_fn("");

	if (userspace_managed) {
		gk20a_err(dev_from_gk20a(g),
			  "userspace-managed address spaces not yet supported");
		return -ENOSYS;
	}

	big_page_size = gmmu_page_sizes[gmmu_page_size_big];

	vm = kzalloc(sizeof(*vm), GFP_KERNEL);
	if (!vm)
		return -ENOMEM;

	as_share->vm = vm;

	vm->mm = mm;
	vm->as_share = as_share;

	for (i = 0; i < gmmu_nr_page_sizes; i++)
		vm->gmmu_page_sizes[i] = gmmu_page_sizes[i];

	vm->big_pages = !mm->disable_bigpage;
	vm->big_page_size = big_page_size;

	vm->va_start  = big_page_size << 10;   /* create a one pde hole */
	vm->va_limit  = mm->channel.user_size + mm->channel.kernel_size;

	msg.cmd = TEGRA_VGPU_CMD_AS_ALLOC_SHARE;
	msg.handle = platform->virt_handle;
	p->size = vm->va_limit;
	p->big_page_size = vm->big_page_size;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	if (err || msg.ret) {
		err = -ENOMEM;
		goto clean_up;
	}

	vm->handle = p->handle;

	/* setup vma limits */
	small_vma_start = vm->va_start;

	if (vm->big_pages) {
		/* First 16GB of the address space goes towards small
		 * pages. The kernel reserved pages are at the end.
		 * What ever remains is allocated to large pages.
		 */
		small_vma_limit = __nv_gmmu_va_small_page_limit();
		large_vma_start = small_vma_limit;
		large_vma_limit = vm->va_limit - mm->channel.kernel_size;
	} else {
		small_vma_limit = vm->va_limit - mm->channel.kernel_size;
		large_vma_start = 0;
		large_vma_limit = 0;
	}

	kernel_vma_start = vm->va_limit - mm->channel.kernel_size;
	kernel_vma_limit = vm->va_limit;

	gk20a_dbg_info(
		"small_vma=[0x%llx,0x%llx) large_vma=[0x%llx,0x%llx) kernel_vma=[0x%llx,0x%llx)\n",
		small_vma_start, small_vma_limit,
		large_vma_start, large_vma_limit,
		kernel_vma_start, kernel_vma_limit);

	/* check that starts do not exceed limits */
	WARN_ON(small_vma_start > small_vma_limit);
	WARN_ON(large_vma_start > large_vma_limit);
	/* kernel_vma must also be non-zero */
	WARN_ON(kernel_vma_start >= kernel_vma_limit);

	if (small_vma_start > small_vma_limit ||
	    large_vma_start > large_vma_limit ||
	    kernel_vma_start >= kernel_vma_limit) {
		err = -EINVAL;
		goto clean_up_share;
	}

	if (small_vma_start < small_vma_limit) {
		snprintf(name, sizeof(name), "gk20a_as_%d-%dKB", as_share->id,
			 gmmu_page_sizes[gmmu_page_size_small] >> 10);

		err = __gk20a_buddy_allocator_init(
					g,
					&vm->vma[gmmu_page_size_small],
					vm, name,
					small_vma_start,
					small_vma_limit - small_vma_start,
					SZ_4K,
					GPU_BALLOC_MAX_ORDER,
					GPU_ALLOC_GVA_SPACE);
		if (err)
			goto clean_up_share;
	}

	if (large_vma_start < large_vma_limit) {
		snprintf(name, sizeof(name), "gk20a_as_%d-%dKB", as_share->id,
			gmmu_page_sizes[gmmu_page_size_big] >> 10);
		err = __gk20a_buddy_allocator_init(
					g,
					&vm->vma[gmmu_page_size_big],
					vm, name,
					large_vma_start,
					large_vma_limit - large_vma_start,
					big_page_size,
					GPU_BALLOC_MAX_ORDER,
					GPU_ALLOC_GVA_SPACE);
		if (err)
			goto clean_up_small_allocator;
	}

	snprintf(name, sizeof(name), "gk20a_as_%dKB-sys",
		 gmmu_page_sizes[gmmu_page_size_kernel] >> 10);
	/*
	 * kernel reserved VMA is at the end of the aperture
	 */
	err = __gk20a_buddy_allocator_init(
				     g,
				     &vm->vma[gmmu_page_size_kernel],
				     vm, name,
				     kernel_vma_start,
				     kernel_vma_limit - kernel_vma_start,
				     SZ_4K,
				     GPU_BALLOC_MAX_ORDER,
				     GPU_ALLOC_GVA_SPACE);
	if (err)
		goto clean_up_big_allocator;

	vm->mapped_buffers = RB_ROOT;

	mutex_init(&vm->update_gmmu_lock);
	kref_init(&vm->ref);
	INIT_LIST_HEAD(&vm->reserved_va_list);

	vm->enable_ctag = true;

	return 0;

clean_up_big_allocator:
	if (large_vma_start < large_vma_limit)
		gk20a_alloc_destroy(&vm->vma[gmmu_page_size_big]);
clean_up_small_allocator:
	if (small_vma_start < small_vma_limit)
		gk20a_alloc_destroy(&vm->vma[gmmu_page_size_small]);
clean_up_share:
	msg.cmd = TEGRA_VGPU_CMD_AS_FREE_SHARE;
	msg.handle = platform->virt_handle;
	p->handle = vm->handle;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	WARN_ON(err || msg.ret);
clean_up:
	kfree(vm);
	as_share->vm = NULL;
	return err;
}

static int vgpu_vm_bind_channel(struct gk20a_as_share *as_share,
				struct channel_gk20a *ch)
{
	struct vm_gk20a *vm = as_share->vm;
	struct gk20a_platform *platform = gk20a_get_platform(ch->g->dev);
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_as_bind_share_params *p = &msg.params.as_bind_share;
	int err;

	gk20a_dbg_fn("");

	ch->vm = vm;
	msg.cmd = TEGRA_VGPU_CMD_AS_BIND_SHARE;
	msg.handle = platform->virt_handle;
	p->as_handle = vm->handle;
	p->chan_handle = ch->virt_ctx;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));

	if (err || msg.ret) {
		ch->vm = NULL;
		err = -ENOMEM;
	}

	if (ch->vm)
		gk20a_vm_get(ch->vm);

	return err;
}

static void vgpu_cache_maint(u64 handle, u8 op)
{
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_cache_maint_params *p = &msg.params.cache_maint;
	int err;

	msg.cmd = TEGRA_VGPU_CMD_CACHE_MAINT;
	msg.handle = handle;
	p->op = op;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	WARN_ON(err || msg.ret);
}

static int vgpu_mm_fb_flush(struct gk20a *g)
{
	struct gk20a_platform *platform = gk20a_get_platform(g->dev);

	gk20a_dbg_fn("");

	vgpu_cache_maint(platform->virt_handle, TEGRA_VGPU_FB_FLUSH);
	return 0;
}

static void vgpu_mm_l2_invalidate(struct gk20a *g)
{
	struct gk20a_platform *platform = gk20a_get_platform(g->dev);

	gk20a_dbg_fn("");

	vgpu_cache_maint(platform->virt_handle, TEGRA_VGPU_L2_MAINT_INV);
}

static void vgpu_mm_l2_flush(struct gk20a *g, bool invalidate)
{
	struct gk20a_platform *platform = gk20a_get_platform(g->dev);
	u8 op;

	gk20a_dbg_fn("");

	if (invalidate)
		op = TEGRA_VGPU_L2_MAINT_FLUSH_INV;
	else
		op =  TEGRA_VGPU_L2_MAINT_FLUSH;

	vgpu_cache_maint(platform->virt_handle, op);
}

static void vgpu_mm_tlb_invalidate(struct vm_gk20a *vm)
{
	struct gk20a *g = gk20a_from_vm(vm);
	struct gk20a_platform *platform = gk20a_get_platform(g->dev);
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_as_invalidate_params *p = &msg.params.as_invalidate;
	int err;

	gk20a_dbg_fn("");

	msg.cmd = TEGRA_VGPU_CMD_AS_INVALIDATE;
	msg.handle = platform->virt_handle;
	p->handle = vm->handle;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	WARN_ON(err || msg.ret);
}

static void vgpu_mm_mmu_set_debug_mode(struct gk20a *g, bool enable)
{
	struct gk20a_platform *platform = gk20a_get_platform(g->dev);
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_mmu_debug_mode *p = &msg.params.mmu_debug_mode;
	int err;

	gk20a_dbg_fn("");

	msg.cmd = TEGRA_VGPU_CMD_SET_MMU_DEBUG_MODE;
	msg.handle = platform->virt_handle;
	p->enable = (u32)enable;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	WARN_ON(err || msg.ret);
}

void vgpu_init_mm_ops(struct gpu_ops *gops)
{
	gops->mm.is_debug_mode_enabled = NULL;
	gops->mm.set_debug_mode = vgpu_mm_mmu_set_debug_mode;
	gops->mm.gmmu_map = vgpu_locked_gmmu_map;
	gops->mm.gmmu_unmap = vgpu_locked_gmmu_unmap;
	gops->mm.vm_remove = vgpu_vm_remove_support;
	gops->mm.vm_alloc_share = vgpu_vm_alloc_share;
	gops->mm.vm_bind_channel = vgpu_vm_bind_channel;
	gops->mm.fb_flush = vgpu_mm_fb_flush;
	gops->mm.l2_invalidate = vgpu_mm_l2_invalidate;
	gops->mm.l2_flush = vgpu_mm_l2_flush;
	gops->mm.tlb_invalidate = vgpu_mm_tlb_invalidate;
	gops->mm.get_physical_addr_bits = gk20a_mm_get_physical_addr_bits;
	gops->mm.get_iova_addr = gk20a_mm_iova_addr;
	gops->mm.init_mm_setup_hw = NULL;
}
