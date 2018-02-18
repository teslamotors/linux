/*
 * drivers/video/tegra/host/gk20a/pmu_gk20a.h
 *
 * GK20A PMU (aka. gPMU outside gk20a context)
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
#ifndef __PMU_GK20A_H__
#define __PMU_GK20A_H__

#include "pmu_api.h"
#include "pmu_common.h"
#include "pmuif/gpmuifboardobj.h"
#include "pmuif/gpmuifclk.h"
#include "pmuif/gpmuifperf.h"
#include "pmuif/gpmuifvolt.h"
#include "pmuif/gpmuifpmgr.h"
#include "pmuif/gpmuiftherm.h"

/* defined by pmu hw spec */
#define GK20A_PMU_VA_SIZE		(512 * 1024 * 1024)
#define GK20A_PMU_UCODE_SIZE_MAX	(256 * 1024)
#define GK20A_PMU_SEQ_BUF_SIZE		4096

#define ZBC_MASK(i)			(~(~(0) << ((i)+1)) & 0xfffe)

#define APP_VERSION_NC_2	20429989
#define APP_VERSION_NC_1	20313802
#define APP_VERSION_NC_0	20360931
#define APP_VERSION_GM206	20652057
#define APP_VERSION_NV_GPU	21307569
#define APP_VERSION_NV_GPU_1	21308030
#define APP_VERSION_GM20B_5 20490253
#define APP_VERSION_GM20B_4 19008461
#define APP_VERSION_GM20B_3 18935575
#define APP_VERSION_GM20B_2 18694072
#define APP_VERSION_GM20B_1 18547257
#define APP_VERSION_GM20B 17615280
#define APP_VERSION_3 18357968
#define APP_VERSION_2 18542378
#define APP_VERSION_1 17997577 /*Obsolete this once 18357968 gets in*/
#define APP_VERSION_0 16856675

/*Fuse defines*/
#define FUSE_GCPLEX_CONFIG_FUSE_0           0x2C8
#define PMU_MODE_MISMATCH_STATUS_MAILBOX_R  6
#define PMU_MODE_MISMATCH_STATUS_VAL        0xDEADDEAD

enum {
	GK20A_PMU_DMAIDX_UCODE		= 0,
	GK20A_PMU_DMAIDX_VIRT		= 1,
	GK20A_PMU_DMAIDX_PHYS_VID	= 2,
	GK20A_PMU_DMAIDX_PHYS_SYS_COH	= 3,
	GK20A_PMU_DMAIDX_PHYS_SYS_NCOH	= 4,
	GK20A_PMU_DMAIDX_RSVD		= 5,
	GK20A_PMU_DMAIDX_PELPG		= 6,
	GK20A_PMU_DMAIDX_END		= 7
};

struct pmu_cmdline_args_v2 {
	u32 cpu_freq_hz;		/* Frequency of the clock driving PMU */
	u32 falc_trace_size;		/* falctrace buffer size (bytes) */
	u32 falc_trace_dma_base;	/* 256-byte block address */
	u32 falc_trace_dma_idx;		/* dmaIdx for DMA operations */
	u8 secure_mode;
	u8 raise_priv_sec;     /*Raise priv level required for desired
					registers*/
	struct pmu_mem_v1 gc6_ctx;		/* dmem offset of gc6 context */
};

struct pmu_cmdline_args_v3 {
	u32 reserved;
	u32 cpu_freq_hz;		/* Frequency of the clock driving PMU */
	u32 falc_trace_size;		/* falctrace buffer size (bytes) */
	u32 falc_trace_dma_base;	/* 256-byte block address */
	u32 falc_trace_dma_idx;		/* dmaIdx for DMA operations */
	u8 secure_mode;
	u8 raise_priv_sec;     /*Raise priv level required for desired
					registers*/
	struct pmu_mem_v1 gc6_ctx;		/* dmem offset of gc6 context */
};

struct pmu_cmdline_args_v4 {
	u32 reserved;
	u32 cpu_freq_hz;		/* Frequency of the clock driving PMU */
	u32 falc_trace_size;		/* falctrace buffer size (bytes) */
	struct falc_dma_addr dma_addr;	/* 256-byte block address */
	u32 falc_trace_dma_idx;		/* dmaIdx for DMA operations */
	u8 secure_mode;
	u8 raise_priv_sec;     /*Raise priv level required for desired
					registers*/
	struct pmu_mem_desc_v0 gc6_ctx;		/* dmem offset of gc6 context */
	u8 pad;
};

struct pmu_cmdline_args_v5 {
	u32 cpu_freq_hz;		/* Frequency of the clock driving PMU */
	struct flcn_mem_desc_v0 trace_buf;
	u8 secure_mode;
	u8 raise_priv_sec;
	struct flcn_mem_desc_v0 gc6_ctx;
	struct flcn_mem_desc_v0 init_data_dma_info;
	u32 dummy;
};


#define GK20A_PMU_TRACE_BUFSIZE     0x4000   /* 4K */
#define GK20A_PMU_DMEM_BLKSIZE2		8

#define GK20A_PMU_UCODE_NB_MAX_OVERLAY	    32
#define GK20A_PMU_UCODE_NB_MAX_DATE_LENGTH  64

struct pmu_ucode_desc {
	u32 descriptor_size;
	u32 image_size;
	u32 tools_version;
	u32 app_version;
	char date[GK20A_PMU_UCODE_NB_MAX_DATE_LENGTH];
	u32 bootloader_start_offset;
	u32 bootloader_size;
	u32 bootloader_imem_offset;
	u32 bootloader_entry_point;
	u32 app_start_offset;
	u32 app_size;
	u32 app_imem_offset;
	u32 app_imem_entry;
	u32 app_dmem_offset;
	u32 app_resident_code_offset;  /* Offset from appStartOffset */
	u32 app_resident_code_size;    /* Exact size of the resident code ( potentially contains CRC inside at the end ) */
	u32 app_resident_data_offset;  /* Offset from appStartOffset */
	u32 app_resident_data_size;    /* Exact size of the resident code ( potentially contains CRC inside at the end ) */
	u32 nb_overlays;
	struct {u32 start; u32 size;} load_ovl[GK20A_PMU_UCODE_NB_MAX_OVERLAY];
	u32 compressed;
};

struct pmu_ucode_desc_v1 {
	u32 descriptor_size;
	u32 image_size;
	u32 tools_version;
	u32 app_version;
	char date[GK20A_PMU_UCODE_NB_MAX_DATE_LENGTH];
	u32 bootloader_start_offset;
	u32 bootloader_size;
	u32 bootloader_imem_offset;
	u32 bootloader_entry_point;
	u32 app_start_offset;
	u32 app_size;
	u32 app_imem_offset;
	u32 app_imem_entry;
	u32 app_dmem_offset;
	u32 app_resident_code_offset;
	u32 app_resident_code_size;
	u32 app_resident_data_offset;
	u32 app_resident_data_size;
	u32 nb_imem_overlays;
	u32 nb_dmem_overlays;
	struct {u32 start; u32 size; } load_ovl[64];
	u32 compressed;
};

#define PMU_UNIT_REWIND		(0x00)
#define PMU_UNIT_PG			(0x03)
#define PMU_UNIT_INIT		(0x07)
#define PMU_UNIT_ACR		(0x0A)
#define PMU_UNIT_PERFMON_T18X	(0x11)
#define PMU_UNIT_PERFMON	(0x12)
#define PMU_UNIT_PERF            (0x13)
#define PMU_UNIT_RC		(0x1F)
#define PMU_UNIT_FECS_MEM_OVERRIDE      (0x1E)
#define PMU_UNIT_CLK             (0x0D)
#define PMU_UNIT_VOLT            (0x0E)
#define PMU_UNIT_THERM           (0x14)
#define PMU_UNIT_PMGR            (0x18)

#define PMU_UNIT_END		(0x23)

#define PMU_UNIT_TEST_START	(0xFE)
#define PMU_UNIT_END_SIM	(0xFF)
#define PMU_UNIT_TEST_END	(0xFF)

#define PMU_UNIT_ID_IS_VALID(id)		\
		(((id) < PMU_UNIT_END) || ((id) >= PMU_UNIT_TEST_START))

#define PMU_DMEM_ALLOC_ALIGNMENT	(4)
#define PMU_DMEM_ALIGNMENT		(4)

#define PMU_CMD_FLAGS_PMU_MASK		(0xF0)

#define PMU_CMD_FLAGS_STATUS		BIT(0)
#define PMU_CMD_FLAGS_INTR		BIT(1)
#define PMU_CMD_FLAGS_EVENT		BIT(2)
#define PMU_CMD_FLAGS_WATERMARK		BIT(3)

#define PMU_MSG_HDR_SIZE	sizeof(struct pmu_hdr)
#define PMU_CMD_HDR_SIZE	sizeof(struct pmu_hdr)

#define PMU_QUEUE_COUNT		5

enum {
	PMU_INIT_MSG_TYPE_PMU_INIT = 0,
};

struct pmu_init_msg_pmu_v0 {
	u8 msg_type;
	u8 pad;

	struct {
		u16 size;
		u16 offset;
		u8  index;
		u8  pad;
	} queue_info[PMU_QUEUE_COUNT];

	u16 sw_managed_area_offset;
	u16 sw_managed_area_size;
};

struct pmu_init_msg_pmu_v1 {
	u8 msg_type;
	u8 pad;
	u16  os_debug_entry_point;

	struct {
		u16 size;
		u16 offset;
		u8  index;
		u8  pad;
	} queue_info[PMU_QUEUE_COUNT];

	u16 sw_managed_area_offset;
	u16 sw_managed_area_size;
};
struct pmu_init_msg_pmu_v2 {
	u8 msg_type;
	u8 pad;
	u16  os_debug_entry_point;

	struct {
		u16 size;
		u16 offset;
		u8  index;
		u8  pad;
	} queue_info[PMU_QUEUE_COUNT];

	u16 sw_managed_area_offset;
	u16 sw_managed_area_size;
	u8 dummy[18];
};

#define PMU_QUEUE_COUNT_FOR_V3 3
#define PMU_QUEUE_HPQ_IDX_FOR_V3 0
#define PMU_QUEUE_LPQ_IDX_FOR_V3 1
#define PMU_QUEUE_MSG_IDX_FOR_V3 2
struct pmu_init_msg_pmu_v3 {
	u8 msg_type;
	u8  queue_index[PMU_QUEUE_COUNT_FOR_V3];
	u16 queue_size[PMU_QUEUE_COUNT_FOR_V3];
	u16 queue_offset;

	u16 sw_managed_area_offset;
	u16 sw_managed_area_size;

	u16  os_debug_entry_point;

	u8 dummy[18];
};

union pmu_init_msg_pmu {
	struct pmu_init_msg_pmu_v0 v0;
	struct pmu_init_msg_pmu_v1 v1;
	struct pmu_init_msg_pmu_v2 v2;
	struct pmu_init_msg_pmu_v3 v3;
};

struct pmu_init_msg {
	union {
		u8 msg_type;
		struct pmu_init_msg_pmu_v1 pmu_init_v1;
		struct pmu_init_msg_pmu_v0 pmu_init_v0;
		struct pmu_init_msg_pmu_v2 pmu_init_v2;
		struct pmu_init_msg_pmu_v3 pmu_init_v3;
	};
};

enum {
	PMU_RC_MSG_TYPE_UNHANDLED_CMD = 0,
};

struct pmu_rc_msg_unhandled_cmd {
	u8 msg_type;
	u8 unit_id;
};

struct pmu_rc_msg {
	u8 msg_type;
	struct pmu_rc_msg_unhandled_cmd unhandled_cmd;
};

/***************************** ACR ERROR CODES  ******************************/
/*!
 * Error codes used in PMU-ACR Task
 *
 * LSF_ACR_INVALID_TRANSCFG_SETUP : Indicates that TRANSCFG Setup is not valid
 *  MAILBOX1 returns the CTXDMA ID of invalid setup
 *
 */
#define ACR_ERROR_INVALID_TRANSCFG_SETUP        (0xAC120001)

/* PERFMON */
#define PMU_DOMAIN_GROUP_PSTATE		0
#define PMU_DOMAIN_GROUP_GPC2CLK	1
#define PMU_DOMAIN_GROUP_NUM		2

/* TBD: smart strategy */
#define PMU_PERFMON_PCT_TO_INC		58
#define PMU_PERFMON_PCT_TO_DEC		23

struct pmu_perfmon_counter_v0 {
	u8 index;
	u8 flags;
	u8 group_id;
	u8 valid;
	u16 upper_threshold; /* units of 0.01% */
	u16 lower_threshold; /* units of 0.01% */
};

struct pmu_perfmon_counter_v2 {
	u8 index;
	u8 flags;
	u8 group_id;
	u8 valid;
	u16 upper_threshold; /* units of 0.01% */
	u16 lower_threshold; /* units of 0.01% */
	u32 scale;
};

#define PMU_PERFMON_FLAG_ENABLE_INCREASE	(0x00000001)
#define PMU_PERFMON_FLAG_ENABLE_DECREASE	(0x00000002)
#define PMU_PERFMON_FLAG_CLEAR_PREV		(0x00000004)


struct pmu_cmd {
	struct pmu_hdr hdr;
	union {
		struct pmu_perfmon_cmd perfmon;
		struct pmu_pg_cmd pg;
		struct pmu_zbc_cmd zbc;
		struct pmu_acr_cmd acr;
		struct pmu_lrf_tex_ltc_dram_cmd lrf_tex_ltc_dram;
		struct nv_pmu_boardobj_cmd boardobj;
		struct nv_pmu_perf_cmd perf;
		struct nv_pmu_volt_cmd volt;
		struct nv_pmu_clk_cmd clk;
		struct nv_pmu_pmgr_cmd pmgr;
		struct nv_pmu_therm_cmd therm;
	} cmd;
};

struct pmu_msg {
	struct pmu_hdr hdr;
	union {
		struct pmu_init_msg init;
		struct pmu_perfmon_msg perfmon;
		struct pmu_pg_msg pg;
		struct pmu_rc_msg rc;
		struct pmu_acr_msg acr;
		struct pmu_lrf_tex_ltc_dram_msg lrf_tex_ltc_dram;
		struct nv_pmu_boardobj_msg boardobj;
		struct nv_pmu_perf_msg perf;
		struct nv_pmu_volt_msg volt;
		struct nv_pmu_clk_msg clk;
		struct nv_pmu_pmgr_msg pmgr;
		struct nv_pmu_therm_msg therm;
	} msg;
};

#define PMU_SHA1_GID_SIGNATURE		0xA7C66AD2
#define PMU_SHA1_GID_SIGNATURE_SIZE	4

#define PMU_SHA1_GID_SIZE	16

struct pmu_sha1_gid {
	bool valid;
	u8 gid[PMU_SHA1_GID_SIZE];
};

struct pmu_sha1_gid_data {
	u8 signature[PMU_SHA1_GID_SIGNATURE_SIZE];
	u8 gid[PMU_SHA1_GID_SIZE];
};

#define PMU_COMMAND_QUEUE_HPQ		0	/* write by sw, read by pmu, protected by sw mutex lock */
#define PMU_COMMAND_QUEUE_LPQ		1	/* write by sw, read by pmu, protected by sw mutex lock */
#define PMU_COMMAND_QUEUE_BIOS		2	/* read/write by sw/hw, protected by hw pmu mutex, id = 2 */
#define PMU_COMMAND_QUEUE_SMI		3	/* read/write by sw/hw, protected by hw pmu mutex, id = 3 */
#define PMU_MESSAGE_QUEUE		4	/* write by pmu, read by sw, accessed by interrupt handler, no lock */
#define PMU_QUEUE_COUNT			5

enum {
	PMU_MUTEX_ID_RSVD1 = 0	,
	PMU_MUTEX_ID_GPUSER	,
	PMU_MUTEX_ID_QUEUE_BIOS	,
	PMU_MUTEX_ID_QUEUE_SMI	,
	PMU_MUTEX_ID_GPMUTEX	,
	PMU_MUTEX_ID_I2C	,
	PMU_MUTEX_ID_RMLOCK	,
	PMU_MUTEX_ID_MSGBOX	,
	PMU_MUTEX_ID_FIFO	,
	PMU_MUTEX_ID_PG		,
	PMU_MUTEX_ID_GR		,
	PMU_MUTEX_ID_CLK	,
	PMU_MUTEX_ID_RSVD6	,
	PMU_MUTEX_ID_RSVD7	,
	PMU_MUTEX_ID_RSVD8	,
	PMU_MUTEX_ID_RSVD9	,
	PMU_MUTEX_ID_INVALID
};

#define PMU_IS_COMMAND_QUEUE(id)	\
		((id)  < PMU_MESSAGE_QUEUE)

#define PMU_IS_SW_COMMAND_QUEUE(id)	\
		(((id) == PMU_COMMAND_QUEUE_HPQ) || \
		 ((id) == PMU_COMMAND_QUEUE_LPQ))

#define  PMU_IS_MESSAGE_QUEUE(id)	\
		((id) == PMU_MESSAGE_QUEUE)

enum
{
	OFLAG_READ = 0,
	OFLAG_WRITE
};

#define QUEUE_SET		(true)
#define QUEUE_GET		(false)

#define QUEUE_ALIGNMENT		(4)

#define PMU_PGENG_GR_BUFFER_IDX_INIT	(0)
#define PMU_PGENG_GR_BUFFER_IDX_ZBC	(1)
#define PMU_PGENG_GR_BUFFER_IDX_FECS	(2)

enum
{
    PMU_DMAIDX_UCODE         = 0,
    PMU_DMAIDX_VIRT          = 1,
    PMU_DMAIDX_PHYS_VID      = 2,
    PMU_DMAIDX_PHYS_SYS_COH  = 3,
    PMU_DMAIDX_PHYS_SYS_NCOH = 4,
    PMU_DMAIDX_RSVD          = 5,
    PMU_DMAIDX_PELPG         = 6,
    PMU_DMAIDX_END           = 7
};

struct pmu_gk20a;
struct pmu_queue;

struct pmu_queue {

	/* used by hw, for BIOS/SMI queue */
	u32 mutex_id;
	u32 mutex_lock;
	/* used by sw, for LPQ/HPQ queue */
	struct mutex mutex;

	/* current write position */
	u32 position;
	/* physical dmem offset where this queue begins */
	u32 offset;
	/* logical queue identifier */
	u32 id;
	/* physical queue index */
	u32 index;
	/* in bytes */
	u32 size;

	/* open-flag */
	u32 oflag;
	bool opened; /* opened implies locked */
};


#define PMU_MUTEX_ID_IS_VALID(id)	\
		((id) < PMU_MUTEX_ID_INVALID)

#define PMU_INVALID_MUTEX_OWNER_ID	(0)

struct pmu_mutex {
	u32 id;
	u32 index;
	u32 ref_cnt;
};

#define PMU_MAX_NUM_SEQUENCES		(256)
#define PMU_SEQ_BIT_SHIFT		(5)
#define PMU_SEQ_TBL_SIZE	\
		(PMU_MAX_NUM_SEQUENCES >> PMU_SEQ_BIT_SHIFT)

#define PMU_INVALID_SEQ_DESC		(~0)

enum
{
	PMU_SEQ_STATE_FREE = 0,
	PMU_SEQ_STATE_PENDING,
	PMU_SEQ_STATE_USED,
	PMU_SEQ_STATE_CANCELLED
};

struct pmu_payload {
	struct {
		void *buf;
		u32 offset;
		u32 size;
		u32 fb_size;
	} in, out;
};

struct pmu_surface {
	struct mem_desc vidmem_desc;
	struct mem_desc sysmem_desc;
	struct flcn_mem_desc_v0 params;
};

typedef void (*pmu_callback)(struct gk20a *, struct pmu_msg *, void *, u32,
	u32);

struct pmu_sequence {
	u8 id;
	u32 state;
	u32 desc;
	struct pmu_msg *msg;
	union {
		struct pmu_allocation_v0 in_v0;
		struct pmu_allocation_v1 in_v1;
		struct pmu_allocation_v2 in_v2;
		struct pmu_allocation_v3 in_v3;
	};
	struct mem_desc *in_mem;
	union {
		struct pmu_allocation_v0 out_v0;
		struct pmu_allocation_v1 out_v1;
		struct pmu_allocation_v2 out_v2;
		struct pmu_allocation_v3 out_v3;
	};
	struct mem_desc *out_mem;
	u8 *out_payload;
	pmu_callback callback;
	void* cb_params;
};

struct pmu_pg_stats_data {
	u32 gating_cnt;
	u32 ingating_time;
	u32 ungating_time;
	u32 avg_entry_latency_us;
	u32 avg_exit_latency_us;
};

struct pmu_pg_stats_v2 {
	u32 entry_count;
	u32 exit_count;
	u32 abort_count;
	u32 detection_count;
	u32 prevention_activate_count;
	u32 prevention_deactivate_count;
	u32 powered_up_time_us;
	u32 entry_latency_us;
	u32 exit_latency_us;
	u32 resident_time_us;
	u32 entry_latency_avg_us;
	u32 exit_latency_avg_us;
	u32 entry_latency_max_us;
	u32 exit_latency_max_us;
	u32 total_sleep_time_us;
	u32 total_non_sleep_time_us;
};

struct pmu_pg_stats_v1 {
	/* Number of time PMU successfully engaged sleep state */
	u32 entry_count;
	/* Number of time PMU exit sleep state */
	u32 exit_count;
	/* Number of time PMU aborted in entry sequence */
	u32 abort_count;
	/*
	* Time for which GPU was neither in Sleep state not
	* executing sleep sequence.
	* */
	u32 poweredup_timeus;
	/* Entry and exit latency of current sleep cycle */
	u32 entry_latency_us;
	u32 exitlatencyus;
	/* Resident time for current sleep cycle. */
	u32 resident_timeus;
	/* Rolling average entry and exit latencies */
	u32 entrylatency_avgus;
	u32 exitlatency_avgus;
	/* Max entry and exit latencies */
	u32 entrylatency_maxus;
	u32 exitlatency_maxus;
	/* Total time spent in sleep and non-sleep state */
	u32 total_sleep_timeus;
	u32 total_nonsleep_timeus;
};

struct pmu_pg_stats {
	u64 pg_entry_start_timestamp;
	u64 pg_ingating_start_timestamp;
	u64 pg_exit_start_timestamp;
	u64 pg_ungating_start_timestamp;
	u32 pg_avg_entry_time_us;
	u32 pg_ingating_cnt;
	u32 pg_ingating_time_us;
	u32 pg_avg_exit_time_us;
	u32 pg_ungating_count;
	u32 pg_ungating_time_us;
	u32 pg_gating_cnt;
	u32 pg_gating_deny_cnt;
};

#define PMU_PG_IDLE_THRESHOLD_SIM		1000
#define PMU_PG_POST_POWERUP_IDLE_THRESHOLD_SIM	4000000
/* TBD: QT or else ? */
#define PMU_PG_IDLE_THRESHOLD			15000
#define PMU_PG_POST_POWERUP_IDLE_THRESHOLD	1000000

#define PMU_PG_ELPG_ENGINE_ID_GRAPHICS (0x00000000)
#define PMU_PG_ELPG_ENGINE_ID_MS       (0x00000004)
#define PMU_PG_ELPG_ENGINE_ID_INVALID_ENGINE (0x00000005)
#define PMU_PG_ELPG_ENGINE_MAX    PMU_PG_ELPG_ENGINE_ID_INVALID_ENGINE

/* state transition :
    OFF => [OFF_ON_PENDING optional] => ON_PENDING => ON => OFF
    ON => OFF is always synchronized */
#define PMU_ELPG_STAT_OFF		0   /* elpg is off */
#define PMU_ELPG_STAT_ON		1   /* elpg is on */
#define PMU_ELPG_STAT_ON_PENDING	2   /* elpg is off, ALLOW cmd has been sent, wait for ack */
#define PMU_ELPG_STAT_OFF_PENDING	3   /* elpg is on, DISALLOW cmd has been sent, wait for ack */
#define PMU_ELPG_STAT_OFF_ON_PENDING	4   /* elpg is off, caller has requested on, but ALLOW
					       cmd hasn't been sent due to ENABLE_ALLOW delay */

#define PMU_MSCG_DISABLED 0
#define PMU_MSCG_ENABLED 1

/* Falcon Register index */
#define PMU_FALCON_REG_R0		(0)
#define PMU_FALCON_REG_R1		(1)
#define PMU_FALCON_REG_R2		(2)
#define PMU_FALCON_REG_R3		(3)
#define PMU_FALCON_REG_R4		(4)
#define PMU_FALCON_REG_R5		(5)
#define PMU_FALCON_REG_R6		(6)
#define PMU_FALCON_REG_R7		(7)
#define PMU_FALCON_REG_R8		(8)
#define PMU_FALCON_REG_R9		(9)
#define PMU_FALCON_REG_R10		(10)
#define PMU_FALCON_REG_R11		(11)
#define PMU_FALCON_REG_R12		(12)
#define PMU_FALCON_REG_R13		(13)
#define PMU_FALCON_REG_R14		(14)
#define PMU_FALCON_REG_R15		(15)
#define PMU_FALCON_REG_IV0		(16)
#define PMU_FALCON_REG_IV1		(17)
#define PMU_FALCON_REG_UNDEFINED	(18)
#define PMU_FALCON_REG_EV		(19)
#define PMU_FALCON_REG_SP		(20)
#define PMU_FALCON_REG_PC		(21)
#define PMU_FALCON_REG_IMB		(22)
#define PMU_FALCON_REG_DMB		(23)
#define PMU_FALCON_REG_CSW		(24)
#define PMU_FALCON_REG_CCR		(25)
#define PMU_FALCON_REG_SEC		(26)
#define PMU_FALCON_REG_CTX		(27)
#define PMU_FALCON_REG_EXCI		(28)
#define PMU_FALCON_REG_RSVD0		(29)
#define PMU_FALCON_REG_RSVD1		(30)
#define PMU_FALCON_REG_RSVD2		(31)
#define PMU_FALCON_REG_SIZE		(32)

/* Choices for pmu_state */
#define PMU_STATE_OFF			0 /* PMU is off */
#define PMU_STATE_STARTING		1 /* PMU is on, but not booted */
#define PMU_STATE_INIT_RECEIVED		2 /* PMU init message received */
#define PMU_STATE_ELPG_BOOTING		3 /* PMU is booting */
#define PMU_STATE_ELPG_BOOTED		4 /* ELPG is initialized */
#define PMU_STATE_LOADING_PG_BUF	5 /* Loading PG buf */
#define PMU_STATE_LOADING_ZBC		6 /* Loading ZBC buf */
#define PMU_STATE_STARTED		7 /* Fully unitialized */



struct pmu_gk20a {

	union {
		struct pmu_ucode_desc *desc;
		struct pmu_ucode_desc_v1 *desc_v1;
	};
	struct mem_desc ucode;

	struct mem_desc pg_buf;
	/* TBD: remove this if ZBC seq is fixed */
	struct mem_desc seq_buf;
	struct mem_desc trace_buf;
	struct mem_desc wpr_buf;
	bool buf_loaded;

	struct pmu_sha1_gid gid_info;

	struct pmu_queue queue[PMU_QUEUE_COUNT];

	struct pmu_sequence *seq;
	unsigned long pmu_seq_tbl[PMU_SEQ_TBL_SIZE];
	u32 next_seq_desc;

	struct pmu_mutex *mutex;
	u32 mutex_cnt;

	struct mutex pmu_copy_lock;
	struct mutex pmu_seq_lock;

	struct gk20a_allocator dmem;

	u32 *ucode_image;
	bool pmu_ready;

	u32 zbc_save_done;

	u32 stat_dmem_offset[PMU_PG_ELPG_ENGINE_ID_INVALID_ENGINE];

	u32 elpg_stat;

	u32 mscg_stat;
	u32 mscg_transition_state;

	int pmu_state;

#define PMU_ELPG_ENABLE_ALLOW_DELAY_MSEC	1 /* msec */
	struct work_struct pg_init;
	struct mutex pg_mutex; /* protect pg-RPPG/MSCG enable/disable */
	struct mutex elpg_mutex; /* protect elpg enable/disable */
	int elpg_refcnt; /* disable -1, enable +1, <=0 elpg disabled, > 0 elpg enabled */

	union {
		struct pmu_perfmon_counter_v2 perfmon_counter_v2;
		struct pmu_perfmon_counter_v0 perfmon_counter_v0;
	};
	u32 perfmon_state_id[PMU_DOMAIN_GROUP_NUM];

	bool initialized;

	void (*remove_support)(struct pmu_gk20a *pmu);
	bool sw_ready;
	bool perfmon_ready;

	u32 sample_buffer;
	u32 load_shadow;
	u32 load_avg;

	struct mutex isr_mutex;
	bool isr_enabled;

	bool zbc_ready;
	union {
		struct pmu_cmdline_args_v0 args_v0;
		struct pmu_cmdline_args_v1 args_v1;
		struct pmu_cmdline_args_v2 args_v2;
		struct pmu_cmdline_args_v3 args_v3;
		struct pmu_cmdline_args_v4 args_v4;
		struct pmu_cmdline_args_v5 args_v5;
	};
	unsigned long perfmon_events_cnt;
	bool perfmon_sampling_enabled;
	u8 pmu_mode; /*Added for GM20b, and ACR*/
	u32 falcon_id;
	u32 aelpg_param[5];
	u32 override_done;

	const struct firmware *fw;
};

int gk20a_init_pmu_support(struct gk20a *g);
int gk20a_init_pmu_bind_fecs(struct gk20a *g);

void gk20a_pmu_isr(struct gk20a *g);

/* send a cmd to pmu */
int gk20a_pmu_cmd_post(struct gk20a *g, struct pmu_cmd *cmd, struct pmu_msg *msg,
		struct pmu_payload *payload, u32 queue_id,
		pmu_callback callback, void* cb_param,
		u32 *seq_desc, unsigned long timeout);

int gk20a_pmu_enable_elpg(struct gk20a *g);
int gk20a_pmu_disable_elpg(struct gk20a *g);
int gk20a_pmu_pg_global_enable(struct gk20a *g, u32 enable_pg);

u32 gk20a_pmu_pg_engines_list(struct gk20a *g);
u32 gk20a_pmu_pg_feature_list(struct gk20a *g, u32 pg_engine_id);

void gk20a_pmu_save_zbc(struct gk20a *g, u32 entries);

int gk20a_pmu_perfmon_enable(struct gk20a *g, bool enable);

int pmu_mutex_acquire(struct pmu_gk20a *pmu, u32 id, u32 *token);
int pmu_mutex_release(struct pmu_gk20a *pmu, u32 id, u32 *token);
int gk20a_pmu_destroy(struct gk20a *g);
int gk20a_pmu_load_norm(struct gk20a *g, u32 *load);
int gk20a_pmu_load_update(struct gk20a *g);
int gk20a_pmu_debugfs_init(struct device *dev);
void gk20a_pmu_reset_load_counters(struct gk20a *g);
void gk20a_pmu_get_load_counters(struct gk20a *g, u32 *busy_cycles,
		u32 *total_cycles);
void gk20a_init_pmu_ops(struct gpu_ops *gops);

void pmu_copy_to_dmem(struct pmu_gk20a *pmu,
		u32 dst, u8 *src, u32 size, u8 port);
void pmu_copy_from_dmem(struct pmu_gk20a *pmu,
		u32 src, u8 *dst, u32 size, u8 port);
int pmu_reset(struct pmu_gk20a *pmu);
int pmu_bootstrap(struct pmu_gk20a *pmu);
int gk20a_init_pmu(struct pmu_gk20a *pmu);
void pmu_dump_falcon_stats(struct pmu_gk20a *pmu);
void gk20a_remove_pmu_support(struct pmu_gk20a *pmu);
void pmu_setup_hw(struct work_struct *work);
void pmu_seq_init(struct pmu_gk20a *pmu);

int gk20a_init_pmu(struct pmu_gk20a *pmu);

int gk20a_pmu_ap_send_command(struct gk20a *g,
		union pmu_ap_cmd *p_ap_cmd, bool b_block);
int gk20a_aelpg_init(struct gk20a *g);
int gk20a_aelpg_init_and_enable(struct gk20a *g, u8 ctrl_id);
void pmu_enable_irq(struct pmu_gk20a *pmu, bool enable);
int pmu_wait_message_cond(struct pmu_gk20a *pmu, u32 timeout,
				 u32 *var, u32 val);
void pmu_handle_fecs_boot_acr_msg(struct gk20a *g, struct pmu_msg *msg,
				void *param, u32 handle, u32 status);
void gk20a_pmu_elpg_statistics(struct gk20a *g, u32 pg_engine_id,
		struct pmu_pg_stats_data *pg_stat_data);
int gk20a_pmu_reset(struct gk20a *g);
int pmu_idle(struct pmu_gk20a *pmu);
int pmu_enable_hw(struct pmu_gk20a *pmu, bool enable);

void gk20a_pmu_surface_free(struct gk20a *g, struct mem_desc *mem);
void gk20a_pmu_surface_describe(struct gk20a *g, struct mem_desc *mem,
		struct flcn_mem_desc_v0 *fb);
int gk20a_pmu_vidmem_surface_alloc(struct gk20a *g, struct mem_desc *mem,
		u32 size);
int gk20a_pmu_sysmem_surface_alloc(struct gk20a *g, struct mem_desc *mem,
		u32 size);

#endif /*__PMU_GK20A_H__*/
