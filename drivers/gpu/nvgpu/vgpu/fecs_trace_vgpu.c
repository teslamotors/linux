/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/string.h>
#include <linux/tegra-ivc.h>
#include <linux/tegra_vgpu.h>
#include <linux/version.h>

#include "gk20a/gk20a.h"
#include "gk20a/ctxsw_trace_gk20a.h"
#include "vgpu.h"
#include "fecs_trace_vgpu.h"

struct vgpu_fecs_trace {
	struct tegra_hv_ivm_cookie *cookie;
	struct nvgpu_ctxsw_ring_header *header;
	struct nvgpu_ctxsw_trace_entry *entries;
	int num_entries;
	bool enabled;
	void *buf;
};

static int vgpu_fecs_trace_init(struct gk20a *g)
{
	struct device *dev = g->dev;
	struct device_node *np = dev->of_node;
	struct of_phandle_args args;
	struct device_node *hv_np;
	struct vgpu_fecs_trace *vcst;
	u32 mempool;
	int err;

	gk20a_dbg_fn("");

	vcst = kzalloc(sizeof(*vcst), GFP_KERNEL);
	if (!vcst)
		return -ENOMEM;

	err = of_parse_phandle_with_fixed_args(np,
			"mempool-fecs-trace", 1, 0, &args);
	if (err) {
		dev_info(dev_from_gk20a(g), "does not support fecs trace\n");
		goto fail;
	}

	hv_np = args.np;
	mempool = args.args[0];
	vcst->cookie = tegra_hv_mempool_reserve(hv_np, mempool);
	if (IS_ERR(vcst->cookie)) {
		dev_info(dev_from_gk20a(g),
			"mempool  %u reserve failed\n", mempool);
		vcst->cookie = NULL;
		err = -EINVAL;
		goto fail;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,18,0)
	vcst->buf = ioremap_cached(vcst->cookie->ipa, vcst->cookie->size);
#else
	vcst->buf = ioremap_cache(vcst->cookie->ipa, vcst->cookie->size);
#endif
	if (!vcst->buf) {
		dev_info(dev_from_gk20a(g), "ioremap_cache failed\n");
		err = -EINVAL;
		goto fail;
	}
	vcst->header = vcst->buf;
	vcst->num_entries = vcst->header->num_ents;
	if (unlikely(vcst->header->ent_size != sizeof(*vcst->entries))) {
		dev_err(dev_from_gk20a(g),
			"entry size mismatch\n");
		goto fail;
	}
	vcst->entries = vcst->buf + sizeof(*vcst->header);
	g->fecs_trace = (struct gk20a_fecs_trace *)vcst;

	return 0;
fail:
	iounmap(vcst->buf);
	if (vcst->cookie)
		tegra_hv_mempool_unreserve(vcst->cookie);
	kfree(vcst);
	return err;
}

static int vgpu_fecs_trace_deinit(struct gk20a *g)
{
	struct vgpu_fecs_trace *vcst = (struct vgpu_fecs_trace *)g->fecs_trace;

	iounmap(vcst->buf);
	tegra_hv_mempool_unreserve(vcst->cookie);
	kfree(vcst);
	return 0;
}

static int vgpu_fecs_trace_enable(struct gk20a *g)
{
	struct vgpu_fecs_trace *vcst = (struct vgpu_fecs_trace *)g->fecs_trace;
	struct tegra_vgpu_cmd_msg msg = {
		.cmd = TEGRA_VGPU_CMD_FECS_TRACE_ENABLE,
		.handle = gk20a_get_platform(g->dev)->virt_handle,
	};
	int err;

	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err ? err : msg.ret;
	WARN_ON(err);
	vcst->enabled = !err;
	return err;
}

static int vgpu_fecs_trace_disable(struct gk20a *g)
{
	struct vgpu_fecs_trace *vcst = (struct vgpu_fecs_trace *)g->fecs_trace;
	struct tegra_vgpu_cmd_msg msg = {
		.cmd = TEGRA_VGPU_CMD_FECS_TRACE_DISABLE,
		.handle = gk20a_get_platform(g->dev)->virt_handle,
	};
	int err;

	vcst->enabled = false;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err ? err : msg.ret;
	WARN_ON(err);
	return err;
}

static bool vpgpu_fecs_trace_is_enabled(struct gk20a *g)
{
	struct vgpu_fecs_trace *vcst = (struct vgpu_fecs_trace *)g->fecs_trace;

	return (vcst && vcst->enabled);
}

static int vgpu_fecs_trace_poll(struct gk20a *g)
{
	struct tegra_vgpu_cmd_msg msg = {
		.cmd = TEGRA_VGPU_CMD_FECS_TRACE_POLL,
		.handle = gk20a_get_platform(g->dev)->virt_handle,
	};
	int err;

	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err ? err : msg.ret;
	WARN_ON(err);
	return err;
}

static int vgpu_alloc_user_buffer(struct gk20a *g, void **buf, size_t *size)
{
	struct vgpu_fecs_trace *vcst = (struct vgpu_fecs_trace *)g->fecs_trace;

	*buf = vcst->buf;
	*size = vcst->cookie->size;
	return 0;
}

static int vgpu_free_user_buffer(struct gk20a *g)
{
	return 0;
}

static int vgpu_mmap_user_buffer(struct gk20a *g, struct vm_area_struct *vma)
{
	struct vgpu_fecs_trace *vcst = (struct vgpu_fecs_trace *)g->fecs_trace;
	unsigned long size = vcst->cookie->size;
	unsigned long vsize = vma->vm_end - vma->vm_start;

	size = min(size, vsize);
	size = round_up(size, PAGE_SIZE);

	return remap_pfn_range(vma, vma->vm_start,
			vcst->cookie->ipa >> PAGE_SHIFT,
			size,
			vma->vm_page_prot);
}

static int vgpu_fecs_trace_max_entries(struct gk20a *g,
			struct nvgpu_ctxsw_trace_filter *filter)
{
	struct vgpu_fecs_trace *vcst = (struct vgpu_fecs_trace *)g->fecs_trace;

	return vcst->header->num_ents;
}

#if NVGPU_CTXSW_FILTER_SIZE != TEGRA_VGPU_FECS_TRACE_FILTER_SIZE
#error "FECS trace filter size mismatch!"
#endif

static int vgpu_fecs_trace_set_filter(struct gk20a *g,
			struct nvgpu_ctxsw_trace_filter *filter)
{
	struct tegra_vgpu_cmd_msg msg = {
		.cmd = TEGRA_VGPU_CMD_FECS_TRACE_SET_FILTER,
		.handle = gk20a_get_platform(g->dev)->virt_handle,
	};
	struct tegra_vgpu_fecs_trace_filter *p = &msg.params.fecs_trace_filter;
	int err;

	memcpy(&p->tag_bits, &filter->tag_bits, sizeof(p->tag_bits));
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err ? err : msg.ret;
	WARN_ON(err);
	return err;
}

void vgpu_init_fecs_trace_ops(struct gpu_ops *ops)
{
	ops->fecs_trace.init = vgpu_fecs_trace_init;
	ops->fecs_trace.deinit = vgpu_fecs_trace_deinit;
	ops->fecs_trace.enable = vgpu_fecs_trace_enable;
	ops->fecs_trace.disable = vgpu_fecs_trace_disable;
	ops->fecs_trace.is_enabled = vpgpu_fecs_trace_is_enabled;
	ops->fecs_trace.reset = NULL;
	ops->fecs_trace.flush = NULL;
	ops->fecs_trace.poll = vgpu_fecs_trace_poll;
	ops->fecs_trace.bind_channel = NULL;
	ops->fecs_trace.unbind_channel = NULL;
	ops->fecs_trace.max_entries = vgpu_fecs_trace_max_entries;
	ops->fecs_trace.alloc_user_buffer = vgpu_alloc_user_buffer;
	ops->fecs_trace.free_user_buffer = vgpu_free_user_buffer;
	ops->fecs_trace.mmap_user_buffer = vgpu_mmap_user_buffer;
	ops->fecs_trace.set_filter = vgpu_fecs_trace_set_filter;
}

void vgpu_fecs_trace_data_update(struct gk20a *g)
{
	gk20a_ctxsw_trace_wake_up(g, 0);
}
