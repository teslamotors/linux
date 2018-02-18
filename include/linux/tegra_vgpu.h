/*
 * Tegra GPU Virtualization Interfaces to Server
 *
 * Copyright (c) 2014-2016, NVIDIA Corporation. All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __TEGRA_VGPU_H
#define __TEGRA_VGPU_H

#ifdef CONFIG_ARCH_TEGRA_18x_SOC
#include <linux/tegra_vgpu_t18x.h>
#endif

enum {
	TEGRA_VGPU_MODULE_GPU = 0,
};

enum {
	/* Needs to follow last entry in TEGRA_VHOST_QUEUE_* list,
	 * in tegra_vhost.h
	 */
	TEGRA_VGPU_QUEUE_CMD = 3,
	TEGRA_VGPU_QUEUE_INTR
};

enum {
	TEGRA_VGPU_CMD_CONNECT = 0,
	TEGRA_VGPU_CMD_DISCONNECT = 1,
	TEGRA_VGPU_CMD_ABORT = 2,
	TEGRA_VGPU_CMD_CHANNEL_ALLOC_HWCTX = 3,
	TEGRA_VGPU_CMD_CHANNEL_FREE_HWCTX = 4,
	TEGRA_VGPU_CMD_GET_ATTRIBUTE = 5,
	TEGRA_VGPU_CMD_MAP_BAR1 = 6,
	TEGRA_VGPU_CMD_AS_ALLOC_SHARE = 7,
	TEGRA_VGPU_CMD_AS_BIND_SHARE = 8,
	TEGRA_VGPU_CMD_AS_FREE_SHARE = 9,
	TEGRA_VGPU_CMD_AS_MAP = 10,
	TEGRA_VGPU_CMD_AS_UNMAP = 11,
	TEGRA_VGPU_CMD_AS_INVALIDATE = 12,
	TEGRA_VGPU_CMD_CHANNEL_BIND = 13,
	TEGRA_VGPU_CMD_CHANNEL_UNBIND = 14,
	TEGRA_VGPU_CMD_CHANNEL_DISABLE = 15,
	TEGRA_VGPU_CMD_CHANNEL_PREEMPT = 16,
	TEGRA_VGPU_CMD_CHANNEL_SETUP_RAMFC = 17,
	TEGRA_VGPU_CMD_CHANNEL_ALLOC_GR_CTX = 18,	/* deprecated */
	TEGRA_VGPU_CMD_CHANNEL_FREE_GR_CTX = 19,	/* deprecated */
	TEGRA_VGPU_CMD_CHANNEL_COMMIT_GR_CTX = 20,
	TEGRA_VGPU_CMD_CHANNEL_ALLOC_GR_PATCH_CTX = 21,
	TEGRA_VGPU_CMD_CHANNEL_FREE_GR_PATCH_CTX = 22,
	TEGRA_VGPU_CMD_CHANNEL_MAP_GR_GLOBAL_CTX = 23,
	TEGRA_VGPU_CMD_CHANNEL_UNMAP_GR_GLOBAL_CTX = 24,
	TEGRA_VGPU_CMD_CHANNEL_COMMIT_GR_GLOBAL_CTX = 25,
	TEGRA_VGPU_CMD_CHANNEL_LOAD_GR_GOLDEN_CTX = 26,
	TEGRA_VGPU_CMD_CHANNEL_BIND_ZCULL = 27,
	TEGRA_VGPU_CMD_CACHE_MAINT = 28,
	TEGRA_VGPU_CMD_SUBMIT_RUNLIST = 29,
	TEGRA_VGPU_CMD_GET_ZCULL_INFO = 30,
	TEGRA_VGPU_CMD_ZBC_SET_TABLE = 31,
	TEGRA_VGPU_CMD_ZBC_QUERY_TABLE = 32,
	TEGRA_VGPU_CMD_AS_MAP_EX = 33,
	TEGRA_VGPU_CMD_CHANNEL_BIND_GR_CTXSW_BUFFERS = 34,
	TEGRA_VGPU_CMD_SET_MMU_DEBUG_MODE = 35,
	TEGRA_VGPU_CMD_SET_SM_DEBUG_MODE = 36,
	TEGRA_VGPU_CMD_REG_OPS = 37,
	TEGRA_VGPU_CMD_CHANNEL_SET_PRIORITY = 38,
	TEGRA_VGPU_CMD_CHANNEL_SET_RUNLIST_INTERLEAVE = 39,
	TEGRA_VGPU_CMD_CHANNEL_SET_TIMESLICE = 40,
	TEGRA_VGPU_CMD_FECS_TRACE_ENABLE = 41,
	TEGRA_VGPU_CMD_FECS_TRACE_DISABLE = 42,
	TEGRA_VGPU_CMD_FECS_TRACE_POLL = 43,
	TEGRA_VGPU_CMD_FECS_TRACE_SET_FILTER = 44,
	TEGRA_VGPU_CMD_CHANNEL_SET_SMPC_CTXSW_MODE = 45,
	TEGRA_VGPU_CMD_CHANNEL_SET_HWPM_CTXSW_MODE = 46,
	TEGRA_VGPU_CMD_CHANNEL_FREE_HWPM_CTX = 47,
	TEGRA_VGPU_CMD_GR_CTX_ALLOC = 48,
	TEGRA_VGPU_CMD_GR_CTX_FREE = 49,
	TEGRA_VGPU_CMD_CHANNEL_BIND_GR_CTX =50,
	TEGRA_VGPU_CMD_TSG_BIND_GR_CTX = 51,
	TEGRA_VGPU_CMD_TSG_BIND_CHANNEL = 52,
	TEGRA_VGPU_CMD_TSG_UNBIND_CHANNEL = 53,
	TEGRA_VGPU_CMD_TSG_PREEMPT = 54,
	TEGRA_VGPU_CMD_TSG_SET_TIMESLICE = 55,
	TEGRA_VGPU_CMD_TSG_SET_RUNLIST_INTERLEAVE = 56,
	TEGRA_VGPU_CMD_CHANNEL_FORCE_RESET = 57,
	TEGRA_VGPU_CMD_CHANNEL_ENABLE = 58,
	TEGRA_VGPU_CMD_READ_PTIMER = 59,
	TEGRA_VGPU_CMD_SET_POWERGATE = 60,
	TEGRA_VGPU_CMD_SET_GPU_CLK_RATE = 61,
};

struct tegra_vgpu_connect_params {
	u32 module;
	u64 handle;
};

struct tegra_vgpu_channel_hwctx_params {
	u32 id;
	u64 pid;
	u64 handle;
};

enum {
	TEGRA_VGPU_ATTRIB_NUM_CHANNELS = 0,
	TEGRA_VGPU_ATTRIB_GOLDEN_CTX_SIZE = 1,
	TEGRA_VGPU_ATTRIB_ZCULL_CTX_SIZE = 2,
	TEGRA_VGPU_ATTRIB_COMPTAG_LINES = 3,
	TEGRA_VGPU_ATTRIB_GPC_COUNT = 4,
	TEGRA_VGPU_ATTRIB_MAX_TPC_PER_GPC_COUNT = 5,
	TEGRA_VGPU_ATTRIB_MAX_TPC_COUNT = 6,
	TEGRA_VGPU_ATTRIB_PMC_BOOT_0 = 7,
	TEGRA_VGPU_ATTRIB_L2_SIZE = 8,
	TEGRA_VGPU_ATTRIB_GPC0_TPC0_SM_ARCH = 9,
	TEGRA_VGPU_ATTRIB_NUM_FBPS = 10,
	TEGRA_VGPU_ATTRIB_FBP_EN_MASK = 11,
	TEGRA_VGPU_ATTRIB_MAX_LTC_PER_FBP = 12,
	TEGRA_VGPU_ATTRIB_MAX_LTS_PER_LTC = 13,
	TEGRA_VGPU_ATTRIB_GPC0_TPC_MASK = 14,
	TEGRA_VGPU_ATTRIB_CACHELINE_SIZE = 15,
	TEGRA_VGPU_ATTRIB_COMPTAGS_PER_CACHELINE = 16,
	TEGRA_VGPU_ATTRIB_SLICES_PER_LTC = 17,
	TEGRA_VGPU_ATTRIB_LTC_COUNT = 18,
	TEGRA_VGPU_ATTRIB_TPC_COUNT = 19,
	TEGRA_VGPU_ATTRIB_GPC0_TPC_COUNT = 20,
	TEGRA_VGPU_ATTRIB_MAX_FREQ = 21,
};

struct tegra_vgpu_attrib_params {
	u32 attrib;
	u32 value;
};

struct tegra_vgpu_as_share_params {
	u64 size;
	u64 handle;
	u32 big_page_size;
};

struct tegra_vgpu_as_bind_share_params {
	u64 as_handle;
	u64 chan_handle;
};

enum {
	TEGRA_VGPU_MAP_PROT_NONE = 0,
	TEGRA_VGPU_MAP_PROT_READ_ONLY,
	TEGRA_VGPU_MAP_PROT_WRITE_ONLY
};

struct tegra_vgpu_as_map_params {
	u64 handle;
	u64 addr;
	u64 gpu_va;
	u64 size;
	u8 pgsz_idx;
	u8 iova;
	u8 kind;
	u8 cacheable;
	u8 clear_ctags;
	u8 prot;
	u32 ctag_offset;
};

struct tegra_vgpu_as_map_ex_params {
	u64 handle;
	u64 gpu_va;
	u64 size;
	u32 mem_desc_count;
	u8 pgsz_idx;
	u8 iova;
	u8 kind;
	u8 cacheable;
	u8 clear_ctags;
	u8 prot;
	u32 ctag_offset;
};

struct tegra_vgpu_mem_desc {
	u64 addr;
	u64 length;
};

struct tegra_vgpu_as_invalidate_params {
	u64 handle;
};

struct tegra_vgpu_channel_config_params {
	u64 handle;
};

struct tegra_vgpu_ramfc_params {
	u64 handle;
	u64 gpfifo_va;
	u32 num_entries;
	u64 userd_addr;
	u8 iova;
};

struct tegra_vgpu_ch_ctx_params {
	u64 handle;
	u64 gr_ctx_va;
	u64 patch_ctx_va;
	u64 cb_va;
	u64 attr_va;
	u64 page_pool_va;
	u64 priv_access_map_va;
	u32 class_num;
};

struct tegra_vgpu_zcull_bind_params {
	u64 handle;
	u64 zcull_va;
	u32 mode;
};

enum {
	TEGRA_VGPU_L2_MAINT_FLUSH = 0,
	TEGRA_VGPU_L2_MAINT_INV,
	TEGRA_VGPU_L2_MAINT_FLUSH_INV,
	TEGRA_VGPU_FB_FLUSH
};

struct tegra_vgpu_cache_maint_params {
	u8 op;
};

struct tegra_vgpu_runlist_params {
	u8 runlist_id;
	u32 num_entries;
};

struct tegra_vgpu_golden_ctx_params {
	u32 size;
};

struct tegra_vgpu_zcull_info_params {
	u32 width_align_pixels;
	u32 height_align_pixels;
	u32 pixel_squares_by_aliquots;
	u32 aliquot_total;
	u32 region_byte_multiplier;
	u32 region_header_size;
	u32 subregion_header_size;
	u32 subregion_width_align_pixels;
	u32 subregion_height_align_pixels;
	u32 subregion_count;
};

#define TEGRA_VGPU_ZBC_COLOR_VALUE_SIZE		4
#define TEGRA_VGPU_ZBC_TYPE_INVALID		0
#define TEGRA_VGPU_ZBC_TYPE_COLOR		1
#define TEGRA_VGPU_ZBC_TYPE_DEPTH		2

struct tegra_vgpu_zbc_set_table_params {
	u32 color_ds[TEGRA_VGPU_ZBC_COLOR_VALUE_SIZE];
	u32 color_l2[TEGRA_VGPU_ZBC_COLOR_VALUE_SIZE];
	u32 depth;
	u32 format;
	u32 type;     /* color or depth */
};

struct tegra_vgpu_zbc_query_table_params {
	u32 color_ds[TEGRA_VGPU_ZBC_COLOR_VALUE_SIZE];
	u32 color_l2[TEGRA_VGPU_ZBC_COLOR_VALUE_SIZE];
	u32 depth;
	u32 ref_cnt;
	u32 format;
	u32 type;             /* color or depth */
	u32 index_size;       /* [out] size, [in] index */
};

#define TEGRA_VGPU_GR_BIND_CTXSW_BUFFER_MAX 4

struct tegra_vgpu_gr_bind_ctxsw_buffers_params {
	u64 handle;	/* deprecated */
	u64 gpu_va[TEGRA_VGPU_GR_BIND_CTXSW_BUFFER_MAX];
	u64 size[TEGRA_VGPU_GR_BIND_CTXSW_BUFFER_MAX];
	u32 mode;
	u64 gr_ctx_handle;
};

struct tegra_vgpu_mmu_debug_mode {
	u32 enable;
};

struct tegra_vgpu_sm_debug_mode {
	u64 handle;
	u64 sms;
	u32 enable;
};

struct tegra_vgpu_reg_op {
	u8 op;
	u8 type;
	u8 status;
	u8 quad;
	u32 group_mask;
	u32 sub_group_mask;
	u32 offset;
	u32 value_lo;
	u32 value_hi;
	u32 and_n_mask_lo;
	u32 and_n_mask_hi;
};

struct tegra_vgpu_reg_ops_params {
	u64 handle;
	u64 num_ops;
	u32 is_profiler;
};

struct tegra_vgpu_channel_priority_params {
	u64 handle;
	u32 priority;
};

/* level follows nvgpu.h definitions */
struct tegra_vgpu_channel_runlist_interleave_params {
	u64 handle;
	u32 level;
};

struct tegra_vgpu_channel_timeslice_params {
	u64 handle;
	u32 timeslice_us;
};

#define TEGRA_VGPU_FECS_TRACE_FILTER_SIZE 256
struct tegra_vgpu_fecs_trace_filter {
	u64 tag_bits[(TEGRA_VGPU_FECS_TRACE_FILTER_SIZE + 63) / 64];
};

enum {
	TEGRA_VGPU_CTXSW_MODE_NO_CTXSW = 0,
	TEGRA_VGPU_CTXSW_MODE_CTXSW,
};

struct tegra_vgpu_channel_set_ctxsw_mode {
	u64 handle;
	u32 mode;
};

struct tegra_vgpu_channel_free_hwpm_ctx {
	u64 handle;
};

struct tegra_vgpu_gr_ctx_params {
	u64 gr_ctx_handle;
	u64 as_handle;
	u64 gr_ctx_va;
	u32 class_num;
};

struct tegra_vgpu_channel_bind_gr_ctx_params {
	u64 ch_handle;
	u64 gr_ctx_handle;
};

struct tegra_vgpu_tsg_bind_gr_ctx_params {
	u32 tsg_id;
	u64 gr_ctx_handle;
};

struct tegra_vgpu_tsg_bind_unbind_channel_params {
	u32 tsg_id;
	u64 ch_handle;
};

struct tegra_vgpu_tsg_preempt_params {
	u32 tsg_id;
};

struct tegra_vgpu_tsg_timeslice_params {
	u32 tsg_id;
	u32 timeslice_us;
};

/* level follows nvgpu.h definitions */
struct tegra_vgpu_tsg_runlist_interleave_params {
	u32 tsg_id;
	u32 level;
};

struct tegra_vgpu_read_ptimer_params {
	u64 time;
};

struct tegra_vgpu_set_powergate_params {
	u32 mode;
};

struct tegra_vgpu_gpu_clk_rate_params {
	u32 rate; /* in kHz */
};

struct tegra_vgpu_cmd_msg {
	u32 cmd;
	int ret;
	u64 handle;
	union {
		struct tegra_vgpu_connect_params connect;
		struct tegra_vgpu_channel_hwctx_params channel_hwctx;
		struct tegra_vgpu_attrib_params attrib;
		struct tegra_vgpu_as_share_params as_share;
		struct tegra_vgpu_as_bind_share_params as_bind_share;
		struct tegra_vgpu_as_map_params as_map;
		struct tegra_vgpu_as_map_ex_params as_map_ex;
		struct tegra_vgpu_as_invalidate_params as_invalidate;
		struct tegra_vgpu_channel_config_params channel_config;
		struct tegra_vgpu_ramfc_params ramfc;
		struct tegra_vgpu_ch_ctx_params ch_ctx;
		struct tegra_vgpu_zcull_bind_params zcull_bind;
		struct tegra_vgpu_cache_maint_params cache_maint;
		struct tegra_vgpu_runlist_params runlist;
		struct tegra_vgpu_golden_ctx_params golden_ctx;
		struct tegra_vgpu_zcull_info_params zcull_info;
		struct tegra_vgpu_zbc_set_table_params zbc_set_table;
		struct tegra_vgpu_zbc_query_table_params zbc_query_table;
		struct tegra_vgpu_gr_bind_ctxsw_buffers_params gr_bind_ctxsw_buffers;
		struct tegra_vgpu_mmu_debug_mode mmu_debug_mode;
		struct tegra_vgpu_sm_debug_mode sm_debug_mode;
		struct tegra_vgpu_reg_ops_params reg_ops;
		struct tegra_vgpu_channel_priority_params channel_priority;
		struct tegra_vgpu_channel_runlist_interleave_params channel_interleave;
		struct tegra_vgpu_channel_timeslice_params channel_timeslice;
		struct tegra_vgpu_fecs_trace_filter fecs_trace_filter;
		struct tegra_vgpu_channel_set_ctxsw_mode set_ctxsw_mode;
		struct tegra_vgpu_channel_free_hwpm_ctx free_hwpm_ctx;
		struct tegra_vgpu_gr_ctx_params gr_ctx;
		struct tegra_vgpu_channel_bind_gr_ctx_params ch_bind_gr_ctx;
		struct tegra_vgpu_tsg_bind_gr_ctx_params tsg_bind_gr_ctx;
		struct tegra_vgpu_tsg_bind_unbind_channel_params tsg_bind_unbind_channel;
		struct tegra_vgpu_tsg_preempt_params tsg_preempt;
		struct tegra_vgpu_tsg_timeslice_params tsg_timeslice;
		struct tegra_vgpu_tsg_runlist_interleave_params tsg_interleave;
		struct tegra_vgpu_read_ptimer_params read_ptimer;
		struct tegra_vgpu_set_powergate_params set_powergate;
		struct tegra_vgpu_gpu_clk_rate_params gpu_clk_rate;
		char padding[192];
	} params;
};

enum {
	TEGRA_VGPU_GR_INTR_NOTIFY = 0,
	TEGRA_VGPU_GR_INTR_SEMAPHORE_TIMEOUT = 1,
	TEGRA_VGPU_GR_INTR_ILLEGAL_NOTIFY = 2,
	TEGRA_VGPU_GR_INTR_ILLEGAL_METHOD = 3,
	TEGRA_VGPU_GR_INTR_ILLEGAL_CLASS = 4,
	TEGRA_VGPU_GR_INTR_FECS_ERROR = 5,
	TEGRA_VGPU_GR_INTR_CLASS_ERROR = 6,
	TEGRA_VGPU_GR_INTR_FIRMWARE_METHOD = 7,
	TEGRA_VGPU_GR_INTR_EXCEPTION = 8,
	TEGRA_VGPU_GR_INTR_SEMAPHORE = 9,
	TEGRA_VGPU_FIFO_INTR_PBDMA = 10,
	TEGRA_VGPU_FIFO_INTR_CTXSW_TIMEOUT = 11,
	TEGRA_VGPU_FIFO_INTR_MMU_FAULT = 12,
	TEGRA_VGPU_GR_NONSTALL_INTR_SEMAPHORE = 13,
	TEGRA_VGPU_FIFO_NONSTALL_INTR_CHANNEL = 14,
	TEGRA_VGPU_CE2_NONSTALL_INTR_NONBLOCKPIPE = 15,
	TEGRA_VGPU_GR_INTR_SM_EXCEPTION = 16,
};

struct tegra_vgpu_gr_intr_info {
	u32 type;
	u32 chid;
};

struct tegra_vgpu_gr_nonstall_intr_info {
	u32 type;
};

struct tegra_vgpu_fifo_intr_info {
	u32 type;
	u32 chid;
};

struct tegra_vgpu_fifo_nonstall_intr_info {
	u32 type;
};

struct tegra_vgpu_ce2_nonstall_intr_info {
	u32 type;
};

enum {
	TEGRA_VGPU_FECS_TRACE_DATA_UPDATE = 0
};

struct tegra_vgpu_fecs_trace_event_info {
	u32 type;
};

struct tegra_vgpu_general_event_info {
	u32 event_id;
	u32 is_tsg;
	u32 id; /* channel id or tsg id */
};

enum {

	TEGRA_VGPU_INTR_GR = 0,
	TEGRA_VGPU_INTR_FIFO = 1,
	TEGRA_VGPU_INTR_CE2 = 2,
	TEGRA_VGPU_NONSTALL_INTR_GR = 3,
	TEGRA_VGPU_NONSTALL_INTR_FIFO = 4,
	TEGRA_VGPU_NONSTALL_INTR_CE2 = 5,
};

enum {
	TEGRA_VGPU_EVENT_INTR = 0,
	TEGRA_VGPU_EVENT_ABORT = 1,
	TEGRA_VGPU_EVENT_FECS_TRACE = 2,
	TEGRA_VGPU_EVENT_CHANNEL = 3,
};

struct tegra_vgpu_intr_msg {
	unsigned int event;
	u32 unit;
	union {
		struct tegra_vgpu_gr_intr_info gr_intr;
		struct tegra_vgpu_gr_nonstall_intr_info gr_nonstall_intr;
		struct tegra_vgpu_fifo_intr_info fifo_intr;
		struct tegra_vgpu_fifo_nonstall_intr_info fifo_nonstall_intr;
		struct tegra_vgpu_ce2_nonstall_intr_info ce2_nonstall_intr;
		struct tegra_vgpu_fecs_trace_event_info fecs_trace;
		struct tegra_vgpu_general_event_info general_event;
		char padding[32];
	} info;
};

#define TEGRA_VGPU_QUEUE_SIZES	\
	512,			\
	sizeof(struct tegra_vgpu_intr_msg)

#endif
