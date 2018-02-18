/*
 * GK20A Graphics Engine
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef GR_GK20A_H
#define GR_GK20A_H

#include <linux/slab.h>
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
#include "gr_t18x.h"
#endif

#include "tsg_gk20a.h"
#include "gr_ctx_gk20a.h"

#define GR_IDLE_CHECK_DEFAULT		10 /* usec */
#define GR_IDLE_CHECK_MAX		200 /* usec */
#define GR_FECS_POLL_INTERVAL		5 /* usec */

#define INVALID_SCREEN_TILE_ROW_OFFSET	0xFFFFFFFF
#define INVALID_MAX_WAYS		0xFFFFFFFF

#define GK20A_FECS_UCODE_IMAGE	"fecs.bin"
#define GK20A_GPCCS_UCODE_IMAGE	"gpccs.bin"

#define GK20A_GR_MAX_PES_PER_GPC 3

enum /* global_ctx_buffer */ {
	CIRCULAR		= 0,
	PAGEPOOL		= 1,
	ATTRIBUTE		= 2,
	CIRCULAR_VPR		= 3,
	PAGEPOOL_VPR		= 4,
	ATTRIBUTE_VPR		= 5,
	GOLDEN_CTX		= 6,
	PRIV_ACCESS_MAP		= 7,
	NR_GLOBAL_CTX_BUF	= 8
};

/* either ATTRIBUTE or ATTRIBUTE_VPR maps to ATTRIBUTE_VA */
enum  /*global_ctx_buffer_va */ {
	CIRCULAR_VA		= 0,
	PAGEPOOL_VA		= 1,
	ATTRIBUTE_VA		= 2,
	GOLDEN_CTX_VA		= 3,
	PRIV_ACCESS_MAP_VA	= 4,
	NR_GLOBAL_CTX_BUF_VA	= 5
};

enum {
	WAIT_UCODE_LOOP,
	WAIT_UCODE_TIMEOUT,
	WAIT_UCODE_ERROR,
	WAIT_UCODE_OK
};

enum {
	GR_IS_UCODE_OP_EQUAL,
	GR_IS_UCODE_OP_NOT_EQUAL,
	GR_IS_UCODE_OP_AND,
	GR_IS_UCODE_OP_LESSER,
	GR_IS_UCODE_OP_LESSER_EQUAL,
	GR_IS_UCODE_OP_SKIP
};

enum {
	eUcodeHandshakeInitComplete = 1,
	eUcodeHandshakeMethodFinished
};

enum {
	ELCG_MODE = (1 << 0),
	BLCG_MODE = (1 << 1),
	INVALID_MODE = (1 << 2)
};

enum {
	ELCG_RUN,	/* clk always run, i.e. disable elcg */
	ELCG_STOP,	/* clk is stopped */
	ELCG_AUTO	/* clk will run when non-idle, standard elcg mode */
};

enum {
	BLCG_RUN,	/* clk always run, i.e. disable blcg */
	BLCG_AUTO	/* clk will run when non-idle, standard blcg mode */
};

#ifndef GR_GO_IDLE_BUNDLE
#define GR_GO_IDLE_BUNDLE	0x0000e100 /* --V-B */
#endif

struct gr_channel_map_tlb_entry {
	u32 curr_ctx;
	u32 hw_chid;
	u32 tsgid;
};

struct gr_zcull_gk20a {
	u32 aliquot_width;
	u32 aliquot_height;
	u32 aliquot_size;
	u32 total_aliquots;

	u32 width_align_pixels;
	u32 height_align_pixels;
	u32 pixel_squares_by_aliquots;
};

struct gr_zcull_info {
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

#define GK20A_ZBC_COLOR_VALUE_SIZE	4  /* RGBA */

#define GK20A_STARTOF_ZBC_TABLE		1   /* index zero reserved to indicate "not ZBCd" */
#define GK20A_SIZEOF_ZBC_TABLE		16  /* match ltcs_ltss_dstg_zbc_index_address width (4) */
#define GK20A_ZBC_TABLE_SIZE		(16 - 1)

#define GK20A_ZBC_TYPE_INVALID		0
#define GK20A_ZBC_TYPE_COLOR		1
#define GK20A_ZBC_TYPE_DEPTH		2

struct zbc_color_table {
	u32 color_ds[GK20A_ZBC_COLOR_VALUE_SIZE];
	u32 color_l2[GK20A_ZBC_COLOR_VALUE_SIZE];
	u32 format;
	u32 ref_cnt;
};

struct zbc_depth_table {
	u32 depth;
	u32 format;
	u32 ref_cnt;
};

struct zbc_entry {
	u32 color_ds[GK20A_ZBC_COLOR_VALUE_SIZE];
	u32 color_l2[GK20A_ZBC_COLOR_VALUE_SIZE];
	u32 depth;
	u32 type;	/* color or depth */
	u32 format;
};

struct zbc_query_params {
	u32 color_ds[GK20A_ZBC_COLOR_VALUE_SIZE];
	u32 color_l2[GK20A_ZBC_COLOR_VALUE_SIZE];
	u32 depth;
	u32 ref_cnt;
	u32 format;
	u32 type;	/* color or depth */
	u32 index_size;	/* [out] size, [in] index */
};

struct sm_info {
	u8 gpc_index;
	u8 tpc_index;
};

#if defined(CONFIG_GK20A_CYCLE_STATS)
struct gk20a_cs_snapshot_client;
struct gk20a_cs_snapshot;
#endif

struct gr_gk20a_isr_data {
	u32 addr;
	u32 data_lo;
	u32 data_hi;
	u32 curr_ctx;
	u32 chid;
	u32 offset;
	u32 sub_chan;
	u32 class_num;
};

struct nvgpu_preemption_modes_rec {
	u32 graphics_preemption_mode_flags; /* supported preemption modes */
	u32 compute_preemption_mode_flags; /* supported preemption modes */

	u32 default_graphics_preempt_mode; /* default mode */
	u32 default_compute_preempt_mode; /* default mode */
};

struct gr_gk20a {
	struct gk20a *g;
	struct {
		bool dynamic;

		u32 buffer_size;
		u32 buffer_total_size;

		bool golden_image_initialized;
		u32 golden_image_size;
		u32 *local_golden_image;

		u32 hwpm_ctxsw_buffer_offset_map_count;
		struct ctxsw_buf_offset_map_entry *hwpm_ctxsw_buffer_offset_map;

		u32 zcull_ctxsw_image_size;

		u32 pm_ctxsw_image_size;

		u32 buffer_header_size;

		u32 priv_access_map_size;

		struct gr_ucode_gk20a ucode;

		struct av_list_gk20a  sw_bundle_init;
		struct av_list_gk20a  sw_method_init;
		struct aiv_list_gk20a sw_ctx_load;
		struct av_list_gk20a  sw_non_ctx_load;
		struct {
			struct aiv_list_gk20a sys;
			struct aiv_list_gk20a gpc;
			struct aiv_list_gk20a tpc;
			struct aiv_list_gk20a zcull_gpc;
			struct aiv_list_gk20a ppc;
			struct aiv_list_gk20a pm_sys;
			struct aiv_list_gk20a pm_gpc;
			struct aiv_list_gk20a pm_tpc;
			struct aiv_list_gk20a pm_ppc;
			struct aiv_list_gk20a perf_sys;
			struct aiv_list_gk20a perf_gpc;
			struct aiv_list_gk20a fbp;
			struct aiv_list_gk20a fbp_router;
			struct aiv_list_gk20a gpc_router;
			struct aiv_list_gk20a pm_ltc;
			struct aiv_list_gk20a pm_fbpa;
			struct aiv_list_gk20a perf_sys_router;
			struct aiv_list_gk20a perf_pma;
			struct aiv_list_gk20a pm_rop;
			struct aiv_list_gk20a pm_ucgpc;
		} ctxsw_regs;
		int regs_base_index;
		bool valid;
	} ctx_vars;

	struct mutex ctx_mutex; /* protect golden ctx init */
	struct mutex fecs_mutex; /* protect fecs method */

#define GR_NETLIST_DYNAMIC	-1
#define GR_NETLIST_STATIC_A	'A'
	int netlist;

	wait_queue_head_t init_wq;
	int initialized;

	u32 num_fbps;

	u32 comptags_per_cacheline;
	u32 slices_per_ltc;
	u32 cacheline_size;
	u32 gobs_per_comptagline_per_slice;

	u32 max_gpc_count;
	u32 max_fbps_count;
	u32 max_tpc_per_gpc_count;
	u32 max_zcull_per_gpc_count;
	u32 max_tpc_count;

	u32 sys_count;
	u32 gpc_count;
	u32 pe_count_per_gpc;
	u32 ppc_count;
	u32 *gpc_ppc_count;
	u32 tpc_count;
	u32 *gpc_tpc_count;
	u32 *gpc_tpc_mask;
	u32 zcb_count;
	u32 *gpc_zcb_count;
	u32 *pes_tpc_count[GK20A_GR_MAX_PES_PER_GPC];
	u32 *pes_tpc_mask[GK20A_GR_MAX_PES_PER_GPC];
	u32 *gpc_skip_mask;

	u32 bundle_cb_default_size;
	u32 min_gpm_fifo_depth;
	u32 bundle_cb_token_limit;
	u32 attrib_cb_default_size;
	u32 attrib_cb_size;
	u32 alpha_cb_default_size;
	u32 alpha_cb_size;
	u32 timeslice_mode;

	struct gr_ctx_buffer_desc global_ctx_buffer[NR_GLOBAL_CTX_BUF];

	struct mem_desc mmu_wr_mem;
	struct mem_desc mmu_rd_mem;

	u8 *map_tiles;
	u32 map_tile_count;
	u32 map_row_offset;

	u32 max_comptag_mem; /* max memory size (MB) for comptag */
	struct compbit_store_desc compbit_store;
	struct gk20a_comptag_allocator {
		struct mutex lock;
		/* this bitmap starts at ctag 1. 0th cannot be taken */
		unsigned long *bitmap;
		/* size of bitmap, not max ctags, so one less */
		unsigned long size;
	} comp_tags;

	struct gr_zcull_gk20a zcull;

	struct mutex zbc_lock;
	struct zbc_color_table zbc_col_tbl[GK20A_ZBC_TABLE_SIZE];
	struct zbc_depth_table zbc_dep_tbl[GK20A_ZBC_TABLE_SIZE];

	s32 max_default_color_index;
	s32 max_default_depth_index;

	s32 max_used_color_index;
	s32 max_used_depth_index;

#define GR_CHANNEL_MAP_TLB_SIZE		2 /* must of power of 2 */
	struct gr_channel_map_tlb_entry chid_tlb[GR_CHANNEL_MAP_TLB_SIZE];
	u32 channel_tlb_flush_index;
	spinlock_t ch_tlb_lock;

	void (*remove_support)(struct gr_gk20a *gr);
	bool sw_ready;
	bool skip_ucode_init;

	struct nvgpu_preemption_modes_rec preemption_mode_rec;
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
	struct gr_t18x t18x;
#endif
	u32 fbp_en_mask;
	u32 *fbp_rop_l2_en_mask;
	u32 no_of_sm;
	struct sm_info *sm_to_cluster;
	struct nvgpu_dbg_gpu_sm_error_state_record *sm_error_states;
#if defined(CONFIG_GK20A_CYCLE_STATS)
	struct mutex			cs_lock;
	struct gk20a_cs_snapshot	*cs_data;
#endif
};

void gk20a_fecs_dump_falcon_stats(struct gk20a *g);

struct gk20a_ctxsw_ucode_segment {
	u32 offset;
	u32 size;
};

struct gk20a_ctxsw_ucode_segments {
	u32 boot_entry;
	u32 boot_imem_offset;
	u32 boot_signature;
	struct gk20a_ctxsw_ucode_segment boot;
	struct gk20a_ctxsw_ucode_segment code;
	struct gk20a_ctxsw_ucode_segment data;
};

/* sums over the ucode files as sequences of u32, computed to the
 * boot_signature field in the structure above */

/* T18X FECS remains same as T21X,
 * so FALCON_UCODE_SIG_T21X_FECS_WITH_RESERVED used
 * for T18X*/
#define FALCON_UCODE_SIG_T18X_GPCCS_WITH_RESERVED	0x68edab34
#define FALCON_UCODE_SIG_T21X_FECS_WITH_DMEM_SIZE	0x9121ab5c
#define FALCON_UCODE_SIG_T21X_FECS_WITH_RESERVED	0x9125ab5c
#define FALCON_UCODE_SIG_T12X_FECS_WITH_RESERVED	0x8a621f78
#define FALCON_UCODE_SIG_T12X_FECS_WITHOUT_RESERVED	0x67e5344b
#define FALCON_UCODE_SIG_T12X_FECS_OLDER		0x56da09f

#define FALCON_UCODE_SIG_T21X_GPCCS_WITH_RESERVED	0x3d3d65e2
#define FALCON_UCODE_SIG_T12X_GPCCS_WITH_RESERVED	0x303465d5
#define FALCON_UCODE_SIG_T12X_GPCCS_WITHOUT_RESERVED	0x3fdd33d3
#define FALCON_UCODE_SIG_T12X_GPCCS_OLDER		0x53d7877

#define FALCON_UCODE_SIG_T21X_FECS_WITHOUT_RESERVED	0x93671b7d
#define FALCON_UCODE_SIG_T21X_FECS_WITHOUT_RESERVED2	0x4d6cbc10

#define FALCON_UCODE_SIG_T21X_GPCCS_WITHOUT_RESERVED	0x393161da

struct gk20a_ctxsw_ucode_info {
	u64 *p_va;
	struct mem_desc inst_blk_desc;
	struct mem_desc surface_desc;
	struct gk20a_ctxsw_ucode_segments fecs;
	struct gk20a_ctxsw_ucode_segments gpccs;
};

struct gk20a_ctxsw_bootloader_desc {
	u32 start_offset;
	u32 size;
	u32 imem_offset;
	u32 entry_point;
};

struct fecs_method_op_gk20a {
	struct {
		u32 addr;
		u32 data;
	} method;

	struct {
		u32 id;
		u32 data;
		u32 clr;
		u32 *ret;
		u32 ok;
		u32 fail;
	} mailbox;

	struct {
		u32 ok;
		u32 fail;
	} cond;

};

struct gpu_ops;
int gr_gk20a_load_golden_ctx_image(struct gk20a *g,
					struct channel_gk20a *c);
void gk20a_init_gr(struct gk20a *g);
void gk20a_init_gr_ops(struct gpu_ops *gops);
int gk20a_init_gr_support(struct gk20a *g);
int gk20a_enable_gr_hw(struct gk20a *g);
int gk20a_gr_reset(struct gk20a *g);
void gk20a_gr_wait_initialized(struct gk20a *g);
/* real size here, but first (ctag 0) isn't used */
int gk20a_comptag_allocator_init(struct gk20a_comptag_allocator *allocator,
		unsigned long size);
void gk20a_comptag_allocator_destroy(struct gk20a_comptag_allocator *allocator);

int gk20a_init_gr_channel(struct channel_gk20a *ch_gk20a);

int gr_gk20a_init_ctx_vars(struct gk20a *g, struct gr_gk20a *gr);

struct nvgpu_alloc_obj_ctx_args;
struct nvgpu_free_obj_ctx_args;

int gk20a_alloc_obj_ctx(struct channel_gk20a *c,
			struct nvgpu_alloc_obj_ctx_args *args);
int gk20a_free_obj_ctx(struct channel_gk20a *c,
			struct nvgpu_free_obj_ctx_args *args);
void gk20a_free_channel_ctx(struct channel_gk20a *c);

int gk20a_gr_isr(struct gk20a *g);
int gk20a_gr_nonstall_isr(struct gk20a *g);

/* zcull */
u32 gr_gk20a_get_ctxsw_zcull_size(struct gk20a *g, struct gr_gk20a *gr);
int gr_gk20a_bind_ctxsw_zcull(struct gk20a *g, struct gr_gk20a *gr,
			struct channel_gk20a *c, u64 zcull_va, u32 mode);
int gr_gk20a_get_zcull_info(struct gk20a *g, struct gr_gk20a *gr,
			struct gr_zcull_info *zcull_params);
/* zbc */
int gr_gk20a_add_zbc(struct gk20a *g, struct gr_gk20a *gr,
			struct zbc_entry *zbc_val);
int gr_gk20a_query_zbc(struct gk20a *g, struct gr_gk20a *gr,
			struct zbc_query_params *query_params);
int gk20a_gr_zbc_set_table(struct gk20a *g, struct gr_gk20a *gr,
			struct zbc_entry *zbc_val);
int gr_gk20a_load_zbc_default_table(struct gk20a *g, struct gr_gk20a *gr);

/* pmu */
int gr_gk20a_fecs_get_reglist_img_size(struct gk20a *g, u32 *size);
int gr_gk20a_fecs_set_reglist_bind_inst(struct gk20a *g,
		struct mem_desc *inst_block);
int gr_gk20a_fecs_set_reglist_virtual_addr(struct gk20a *g, u64 pmu_va);

void gr_gk20a_init_elcg_mode(struct gk20a *g, u32 mode, u32 engine);
void gr_gk20a_init_blcg_mode(struct gk20a *g, u32 mode, u32 engine);

void gr_gk20a_init_cg_mode(struct gk20a *g, u32 cgmode, u32 mode_config);

/* sm */
bool gk20a_gr_sm_debugger_attached(struct gk20a *g);
void gk20a_gr_clear_sm_hww(struct gk20a *g,
				  u32 gpc, u32 tpc, u32 global_esr);

#define gr_gk20a_elpg_protected_call(g, func) \
	({ \
		int err = 0; \
		if (support_gk20a_pmu(g->dev) && g->elpg_enabled) {\
			err = gk20a_pmu_disable_elpg(g); \
			if (err) \
				gk20a_pmu_enable_elpg(g); \
		} \
		if (!err) { \
			err = func; \
			if (support_gk20a_pmu(g->dev) && g->elpg_enabled) \
				gk20a_pmu_enable_elpg(g); \
		} \
		err; \
	})

int gk20a_gr_suspend(struct gk20a *g);

struct nvgpu_dbg_gpu_reg_op;
int gr_gk20a_exec_ctx_ops(struct channel_gk20a *ch,
			  struct nvgpu_dbg_gpu_reg_op *ctx_ops, u32 num_ops,
			  u32 num_ctx_wr_ops, u32 num_ctx_rd_ops);
int gr_gk20a_get_ctx_buffer_offsets(struct gk20a *g,
				    u32 addr,
				    u32 max_offsets,
				    u32 *offsets, u32 *offset_addrs,
				    u32 *num_offsets,
				    bool is_quad, u32 quad);
int gr_gk20a_get_pm_ctx_buffer_offsets(struct gk20a *g,
				       u32 addr,
				       u32 max_offsets,
				       u32 *offsets, u32 *offset_addrs,
				       u32 *num_offsets);
int gr_gk20a_update_smpc_ctxsw_mode(struct gk20a *g,
				    struct channel_gk20a *c,
				    bool enable_smpc_ctxsw);
int gr_gk20a_update_hwpm_ctxsw_mode(struct gk20a *g,
				  struct channel_gk20a *c,
				  bool enable_hwpm_ctxsw);

struct channel_ctx_gk20a;
void gr_gk20a_ctx_patch_write(struct gk20a *g, struct channel_ctx_gk20a *ch_ctx,
				    u32 addr, u32 data, bool patch);
int gr_gk20a_ctx_patch_write_begin(struct gk20a *g,
					  struct channel_ctx_gk20a *ch_ctx);
void gr_gk20a_ctx_patch_write_end(struct gk20a *g,
					struct channel_ctx_gk20a *ch_ctx);
void gr_gk20a_commit_global_pagepool(struct gk20a *g,
				     struct channel_ctx_gk20a *ch_ctx,
				     u64 addr, u32 size, bool patch);
void gk20a_gr_set_shader_exceptions(struct gk20a *g, u32 data);
void gr_gk20a_enable_hww_exceptions(struct gk20a *g);
int gr_gk20a_init_fs_state(struct gk20a *g);
int gr_gk20a_setup_rop_mapping(struct gk20a *g, struct gr_gk20a *gr);
int gr_gk20a_init_ctxsw_ucode(struct gk20a *g);
int gr_gk20a_load_ctxsw_ucode(struct gk20a *g);
void gr_gk20a_load_falcon_bind_instblk(struct gk20a *g);
void gr_gk20a_load_ctxsw_ucode_header(struct gk20a *g, u64 addr_base,
	struct gk20a_ctxsw_ucode_segments *segments, u32 reg_offset);
void gr_gk20a_load_ctxsw_ucode_boot(struct gk20a *g, u64 addr_base,
	struct gk20a_ctxsw_ucode_segments *segments, u32 reg_offset);


void gr_gk20a_free_tsg_gr_ctx(struct tsg_gk20a *c);
int gr_gk20a_disable_ctxsw(struct gk20a *g);
int gr_gk20a_enable_ctxsw(struct gk20a *g);
void gk20a_resume_single_sm(struct gk20a *g,
		u32 gpc, u32 tpc);
void gk20a_resume_all_sms(struct gk20a *g);
void gk20a_suspend_single_sm(struct gk20a *g,
		u32 gpc, u32 tpc,
		u32 global_esr_mask, bool check_errors);
void gk20a_suspend_all_sms(struct gk20a *g,
		u32 global_esr_mask, bool check_errors);
int gk20a_gr_lock_down_sm(struct gk20a *g,
				 u32 gpc, u32 tpc, u32 global_esr_mask,
				 bool check_errors);
int gr_gk20a_set_sm_debug_mode(struct gk20a *g,
	struct channel_gk20a *ch, u64 sms, bool enable);
bool gk20a_is_channel_ctx_resident(struct channel_gk20a *ch);
int gr_gk20a_add_zbc_color(struct gk20a *g, struct gr_gk20a *gr,
			   struct zbc_entry *color_val, u32 index);
int gr_gk20a_add_zbc_depth(struct gk20a *g, struct gr_gk20a *gr,
			   struct zbc_entry *depth_val, u32 index);
int _gk20a_gr_zbc_set_table(struct gk20a *g, struct gr_gk20a *gr,
			struct zbc_entry *zbc_val);
void gr_gk20a_pmu_save_zbc(struct gk20a *g, u32 entries);
int gr_gk20a_wait_idle(struct gk20a *g, unsigned long end_jiffies,
		       u32 expect_delay);
int gr_gk20a_handle_sm_exception(struct gk20a *g, u32 gpc, u32 tpc,
		bool *post_event, struct channel_gk20a *fault_ch,
		u32 *hww_global_esr);
int gr_gk20a_handle_tex_exception(struct gk20a *g, u32 gpc, u32 tpc,
					bool *post_event);
int gr_gk20a_init_ctx_state(struct gk20a *g);
int gr_gk20a_submit_fecs_method_op(struct gk20a *g,
				   struct fecs_method_op_gk20a op,
				   bool sleepduringwait);
int gr_gk20a_submit_fecs_sideband_method_op(struct gk20a *g,
		struct fecs_method_op_gk20a op);
int gr_gk20a_alloc_gr_ctx(struct gk20a *g,
			  struct gr_ctx_desc **__gr_ctx, struct vm_gk20a *vm,
			  u32 class, u32 padding);
void gr_gk20a_free_gr_ctx(struct gk20a *g,
			  struct vm_gk20a *vm, struct gr_ctx_desc *gr_ctx);
int gr_gk20a_halt_pipe(struct gk20a *g);
int gr_gk20a_debugfs_init(struct gk20a *g);

#if defined(CONFIG_GK20A_CYCLE_STATS)
int gr_gk20a_css_attach(struct gk20a *g,	/* in - main hw structure */
			u32 dmabuf_fd,		/* in - dma mapped memory */
			u32 perfmon_id_count,	/* in - number of perfmons*/
			u32 *perfmon_id_start,	/* out- index of first pm */
			/* out - pointer to client data used in later     */
			struct gk20a_cs_snapshot_client **css_client);

int gr_gk20a_css_detach(struct gk20a *g,
				struct gk20a_cs_snapshot_client *css_client);
int gr_gk20a_css_flush(struct gk20a *g,
				struct gk20a_cs_snapshot_client *css_client);

void gr_gk20a_free_cyclestats_snapshot_data(struct gk20a *g);

#else
/* fake empty cleanup function if no cyclestats snapshots enabled */
static inline void gr_gk20a_free_cyclestats_snapshot_data(struct gk20a *g)
{
	(void)g;
}
#endif


int gk20a_gr_handle_fecs_error(struct gk20a *g, struct channel_gk20a *ch,
		struct gr_gk20a_isr_data *isr_data);
int gk20a_gr_wait_for_sm_lock_down(struct gk20a *g, u32 gpc, u32 tpc,
		u32 global_esr_mask, bool check_errors);
void gk20a_gr_clear_sm_hww(struct gk20a *g,
		u32 gpc, u32 tpc, u32 global_esr);
int gr_gk20a_ctx_wait_ucode(struct gk20a *g, u32 mailbox_id,
			    u32 *mailbox_ret, u32 opc_success,
			    u32 mailbox_ok, u32 opc_fail,
			    u32 mailbox_fail, bool sleepduringwait);

int gr_gk20a_get_ctx_id(struct gk20a *g,
		struct channel_gk20a *c,
		u32 *ctx_id);

u32 gk20a_mask_hww_warp_esr(u32 hww_warp_esr);

bool gr_gk20a_suspend_context(struct channel_gk20a *ch);
bool gr_gk20a_resume_context(struct channel_gk20a *ch);
int gr_gk20a_suspend_contexts(struct gk20a *g,
			      struct dbg_session_gk20a *dbg_s,
			      int *ctx_resident_ch_fd);
int gr_gk20a_resume_contexts(struct gk20a *g,
			      struct dbg_session_gk20a *dbg_s,
			      int *ctx_resident_ch_fd);

static inline const char *gr_gk20a_graphics_preempt_mode_name(u32 graphics_preempt_mode)
{
	switch (graphics_preempt_mode) {
	case NVGPU_GRAPHICS_PREEMPTION_MODE_WFI:
		return "WFI";
	default:
		return "?";
	}
}

static inline const char *gr_gk20a_compute_preempt_mode_name(u32 compute_preempt_mode)
{
	switch (compute_preempt_mode) {
	case NVGPU_COMPUTE_PREEMPTION_MODE_WFI:
		return "WFI";
	case NVGPU_COMPUTE_PREEMPTION_MODE_CTA:
		return "CTA";
	default:
		return "?";
	}
}

#endif /*__GR_GK20A_H__*/
