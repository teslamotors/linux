/*
 * GP10B GPU GR
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _NVGPU_GR_GP10B_H_
#define _NVGPU_GR_GP10B_H_

#include <linux/version.h>

struct gpu_ops;

enum {
	PASCAL_CHANNEL_GPFIFO_A  = 0xC06F,
	PASCAL_A                 = 0xC097,
	PASCAL_COMPUTE_A         = 0xC0C0,
	PASCAL_DMA_COPY_A        = 0xC0B5,
	PASCAL_DMA_COPY_B        = 0xC1B5,
};

#define NVC097_SET_GO_IDLE_TIMEOUT		0x022c
#define NVC097_SET_ALPHA_CIRCULAR_BUFFER_SIZE	0x02dc
#define NVC097_SET_COALESCE_BUFFER_SIZE		0x1028
#define NVC097_SET_CIRCULAR_BUFFER_SIZE		0x1280
#define NVC097_SET_SHADER_EXCEPTIONS		0x1528
#define NVC0C0_SET_SHADER_EXCEPTIONS		0x1528

void gp10b_init_gr(struct gpu_ops *ops);
int gr_gp10b_init_fs_state(struct gk20a *g);
int gr_gp10b_alloc_buffer(struct vm_gk20a *vm, size_t size,
			struct mem_desc *mem);
void gr_gp10b_create_sysfs(struct device *dev);

struct ecc_stat {
	char **names;
	u32 *counters;
	struct hlist_node hash_node;
};

struct gr_t18x {
	struct {
		u32 preempt_image_size;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,4,0)
		u32 force_preemption_gfxp;
		u32 force_preemption_cilp;
		u32 dump_ctxsw_stats_on_channel_close;
#else
		bool force_preemption_gfxp;
		bool force_preemption_cilp;
		bool dump_ctxsw_stats_on_channel_close;
#endif
		struct dentry *debugfs_force_preemption_cilp;
		struct dentry *debugfs_force_preemption_gfxp;
		struct dentry *debugfs_dump_ctxsw_stats;
	} ctx_vars;

	struct {
		struct ecc_stat sm_lrf_single_err_count;
		struct ecc_stat sm_lrf_double_err_count;

		struct ecc_stat sm_shm_sec_count;
		struct ecc_stat sm_shm_sed_count;
		struct ecc_stat sm_shm_ded_count;

		struct ecc_stat tex_total_sec_pipe0_count;
		struct ecc_stat tex_total_ded_pipe0_count;
		struct ecc_stat tex_unique_sec_pipe0_count;
		struct ecc_stat tex_unique_ded_pipe0_count;
		struct ecc_stat tex_total_sec_pipe1_count;
		struct ecc_stat tex_total_ded_pipe1_count;
		struct ecc_stat tex_unique_sec_pipe1_count;
		struct ecc_stat tex_unique_ded_pipe1_count;

		struct ecc_stat l2_sec_count;
		struct ecc_stat l2_ded_count;
	} ecc_stats;

	u32 fecs_feature_override_ecc_val;

	int cilp_preempt_pending_chid;
};

struct gr_ctx_desc_t18x {
	struct mem_desc preempt_ctxsw_buffer;
	struct mem_desc spill_ctxsw_buffer;
	struct mem_desc betacb_ctxsw_buffer;
	struct mem_desc pagepool_ctxsw_buffer;
	u32 ctx_id;
	bool ctx_id_valid;
	bool cilp_preempt_pending;
};

#endif
