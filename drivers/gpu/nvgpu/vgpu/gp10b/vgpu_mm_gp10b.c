/*
 * Virtualized GPU Memory Management
 *
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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
#include "vgpu_mm_gp10b.h"
#include "gk20a/semaphore_gk20a.h"
#include "gk20a/mm_gk20a.h"

static int vgpu_gp10b_init_mm_setup_hw(struct gk20a *g)
{
	g->mm.bypass_smmu = true;
	g->mm.disable_bigpage = true;
	return 0;
}

static inline int add_mem_desc(struct tegra_vgpu_mem_desc *mem_desc,
				u64 addr, u64 size, size_t *oob_size)
{
	if (*oob_size < sizeof(*mem_desc))
		return -ENOMEM;

	mem_desc->addr = addr;
	mem_desc->length = size;
	*oob_size -= sizeof(*mem_desc);
	return 0;
}

static u64 vgpu_gp10b_locked_gmmu_map(struct vm_gk20a *vm,
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
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_as_map_ex_params *p = &msg.params.as_map_ex;
	struct tegra_vgpu_mem_desc *mem_desc;
	u32 page_size  = vm->gmmu_page_sizes[pgsz_idx];
	u64 space_to_skip = buffer_offset;
	u64 buffer_size = 0;
	u32 mem_desc_count = 0;
	struct scatterlist *sgl;
	void *handle = NULL;
	size_t oob_size;
	u8 prot;

	gk20a_dbg_fn("");

	/* FIXME: add support for sparse mappings */

	if (WARN_ON(!sgt) || WARN_ON(!g->mm.bypass_smmu))
		return -EINVAL;

	if (space_to_skip & (page_size - 1))
		return -EINVAL;

	/* Allocate (or validate when map_offset != 0) the virtual address. */
	if (!map_offset) {
		map_offset = gk20a_vm_alloc_va(vm, size, pgsz_idx);
		if (!map_offset) {
			gk20a_err(d, "failed to allocate va space");
			err = -ENOMEM;
			goto fail;
		}
	}

	handle = tegra_gr_comm_oob_get_ptr(TEGRA_GR_COMM_CTX_CLIENT,
					tegra_gr_comm_get_server_vmid(),
					TEGRA_VGPU_QUEUE_CMD,
					(void **)&mem_desc, &oob_size);
	if (!handle) {
		err = -EINVAL;
		goto fail;
	}

	sgl = sgt->sgl;
	while (space_to_skip && sgl &&
		(space_to_skip + page_size > sgl->length)) {
		space_to_skip -= sgl->length;
		sgl = sg_next(sgl);
	}
	WARN_ON(!sgl);

	if (add_mem_desc(&mem_desc[mem_desc_count++],
			sg_phys(sgl) + space_to_skip,
			sgl->length - space_to_skip,
			&oob_size)) {
		err = -ENOMEM;
		goto fail;
	}
	buffer_size += sgl->length - space_to_skip;

	sgl = sg_next(sgl);
	while (sgl && buffer_size < size) {
		if (add_mem_desc(&mem_desc[mem_desc_count++], sg_phys(sgl),
				sgl->length, &oob_size)) {
			err = -ENOMEM;
			goto fail;
		}

		buffer_size += sgl->length;
		sgl = sg_next(sgl);
	}

	if (rw_flag == gk20a_mem_flag_read_only)
		prot = TEGRA_VGPU_MAP_PROT_READ_ONLY;
	else if (rw_flag == gk20a_mem_flag_write_only)
		prot = TEGRA_VGPU_MAP_PROT_WRITE_ONLY;
	else
		prot = TEGRA_VGPU_MAP_PROT_NONE;

	if (pgsz_idx == gmmu_page_size_kernel) {
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

	msg.cmd = TEGRA_VGPU_CMD_AS_MAP_EX;
	msg.handle = platform->virt_handle;
	p->handle = vm->handle;
	p->gpu_va = map_offset;
	p->size = size;
	p->mem_desc_count = mem_desc_count;
	p->pgsz_idx = pgsz_idx;
	p->iova = 0;
	p->kind = kind_v;
	p->cacheable =
		(flags & NVGPU_MAP_BUFFER_FLAGS_CACHEABLE_TRUE) ? 1 : 0;
	p->prot = prot;
	p->ctag_offset = ctag_offset;
	p->clear_ctags = clear_ctags;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	if (err || msg.ret)
		goto fail;

	/* TLB invalidate handled on server side */

	tegra_gr_comm_oob_put_ptr(handle);
	return map_offset;
fail:
	if (handle)
		tegra_gr_comm_oob_put_ptr(handle);
	gk20a_err(d, "%s: failed with err=%d\n", __func__, err);
	return 0;
}

void vgpu_gp10b_init_mm_ops(struct gpu_ops *gops)
{
	gk20a_dbg_fn("");

	gops->mm.gmmu_map = vgpu_gp10b_locked_gmmu_map;
	gops->mm.init_mm_setup_hw = vgpu_gp10b_init_mm_setup_hw;
}
