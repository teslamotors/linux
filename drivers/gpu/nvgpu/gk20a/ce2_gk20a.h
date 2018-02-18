/*
 * drivers/video/tegra/host/gk20a/fifo_gk20a.h
 *
 * GK20A graphics copy engine (gr host)
 *
 * Copyright (c) 2011-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef __CE2_GK20A_H__
#define __CE2_GK20A_H__

#include "channel_gk20a.h"
#include "tsg_gk20a.h"

void gk20a_init_ce2(struct gpu_ops *gops);
void gk20a_ce2_isr(struct gk20a *g, u32 inst_id, u32 pri_base);
void gk20a_ce2_nonstall_isr(struct gk20a *g, u32 inst_id, u32 pri_base);

/* CE command utility macros */
#define NVGPU_CE_LOWER_ADDRESS_OFFSET_MASK 0xffffffff
#define NVGPU_CE_UPPER_ADDRESS_OFFSET_MASK 0xff

#define NVGPU_CE_COMMAND_BUF_SIZE     4096
#define NVGPU_CE_MAX_COMMAND_BUFF_SIZE_PER_KICKOFF 128
#define NVGPU_CE_MAX_COMMAND_BUFF_SIZE_FOR_TRACING 8

typedef void (*ce_event_callback)(u32 ce_ctx_id, u32 ce_event_flag);

/* dma launch_flags */
enum {
	/* location */
	NVGPU_CE_SRC_LOCATION_COHERENT_SYSMEM                    = (1 << 0),
	NVGPU_CE_SRC_LOCATION_NONCOHERENT_SYSMEM                 = (1 << 1),
	NVGPU_CE_SRC_LOCATION_LOCAL_FB                           = (1 << 2),
	NVGPU_CE_DST_LOCATION_COHERENT_SYSMEM                    = (1 << 3),
	NVGPU_CE_DST_LOCATION_NONCOHERENT_SYSMEM                 = (1 << 4),
	NVGPU_CE_DST_LOCATION_LOCAL_FB                           = (1 << 5),

	/* memory layout */
	NVGPU_CE_SRC_MEMORY_LAYOUT_PITCH                         = (1 << 6),
	NVGPU_CE_SRC_MEMORY_LAYOUT_BLOCKLINEAR                   = (1 << 7),
	NVGPU_CE_DST_MEMORY_LAYOUT_PITCH                         = (1 << 8),
	NVGPU_CE_DST_MEMORY_LAYOUT_BLOCKLINEAR                   = (1 << 9),

	/* transfer type */
	NVGPU_CE_DATA_TRANSFER_TYPE_PIPELINED                   = (1 << 10),
	NVGPU_CE_DATA_TRANSFER_TYPE_NON_PIPELINED               = (1 << 11),
};

/* CE operation mode */
enum {
	NVGPU_CE_PHYS_MODE_TRANSFER        = (1 << 0),
	NVGPU_CE_MEMSET                    = (1 << 1),
};

/* CE event flags */
enum {
	NVGPU_CE_CONTEXT_JOB_COMPLETED               = (1 << 0),
	NVGPU_CE_CONTEXT_JOB_TIMEDOUT                = (1 << 1),
	NVGPU_CE_CONTEXT_SUSPEND                     = (1 << 2),
	NVGPU_CE_CONTEXT_RESUME                      = (1 << 3),
};

/* CE app state machine flags */
enum {
	NVGPU_CE_ACTIVE                    = (1 << 0),
	NVGPU_CE_SUSPEND                   = (1 << 1),
};

/* gpu context state machine flags */
enum {
	NVGPU_CE_GPU_CTX_ALLOCATED         = (1 << 0),
	NVGPU_CE_GPU_CTX_DELETED           = (1 << 1),
};

/* global ce app db */
struct gk20a_ce_app {
	bool initialised;
	struct mutex app_mutex;
	int app_state;

	struct list_head allocated_contexts;
	u32 ctx_count;
	u32 next_ctx_id;
};

/* ce context db */
struct gk20a_gpu_ctx {
	struct gk20a *g;
	struct device *dev;
	u32 ctx_id;
	struct mutex gpu_ctx_mutex;
	int gpu_ctx_state;
	ce_event_callback user_event_callback;

	/* channel related data */
	struct channel_gk20a *ch;
	struct vm_gk20a *vm;

	/* cmd buf mem_desc */
	struct mem_desc cmd_buf_mem;

	struct list_head list;

	u64 submitted_seq_number;
	u64 completed_seq_number;

	u32 cmd_buf_read_queue_offset;
	u32 cmd_buf_end_queue_offset;
};

/* global CE app related apis */
int gk20a_init_ce_support(struct gk20a *g);
void gk20a_ce_suspend(struct gk20a *g);
void gk20a_ce_destroy(struct gk20a *g);

/* CE app utility functions */
u32 gk20a_ce_create_context_with_cb(struct device *dev,
		int runlist_id,
		int priority,
		int timeslice,
		int runlist_level,
		ce_event_callback user_event_callback);
int gk20a_ce_execute_ops(struct device *dev,
		u32 ce_ctx_id,
		u64 src_buf,
		u64 dst_buf,
		u64 size,
		unsigned int payload,
		int launch_flags,
		int request_operation,
		struct gk20a_fence *gk20a_fence_in,
		u32 submit_flags,
		struct gk20a_fence **gk20a_fence_out);
void gk20a_ce_delete_context(struct device *dev,
		u32 ce_ctx_id);

#ifdef CONFIG_DEBUG_FS
/* CE app debugfs api */
void gk20a_ce_debugfs_init(struct device *dev);
#endif

#endif /*__CE2_GK20A_H__*/
