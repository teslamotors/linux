/*
 * Intel SKL IPC Support
 *
 * Copyright (C) 2014-15, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef __SKL_IPC_H
#define __SKL_IPC_H

#include <linux/irqreturn.h>
#include "../common/sst-ipc.h"
#include "skl-sst-dsp.h"
#include "skl-tplg-interface.h"
#include "skl-topology.h"

struct sst_dsp;
struct skl_sst;
struct sst_generic_ipc;

#define MAX_FW_REG_SZ 4096
#define FW_REG_SZ 1024
#define TYPE0_EXCEPTION 0
#define TYPE1_EXCEPTION 1
#define TYPE2_EXCEPTION 2

enum skl_ipc_pipeline_state {
	PPL_INVALID_STATE =	0,
	PPL_UNINITIALIZED =	1,
	PPL_RESET =		2,
	PPL_PAUSED =		3,
	PPL_RUNNING =		4,
	PPL_ERROR_STOP =	5,
	PPL_SAVED =		6,
	PPL_RESTORED =		7
};

struct skl_ipc_dxstate_info {
	u32 core_mask;
	u32 dx_mask;
};

struct skl_ipc_header {
	u32 primary;
	u32 extension;
};

struct skl_d0i3_data {
	int d0i3_stream_count;
	int non_d0i3_stream_count;
	struct delayed_work d0i3_work;
};

struct skl_notify_kctrl_info {
	struct list_head list;
	u32 notify_id;
	struct snd_kcontrol *notify_kctl;
};

#if 0
struct skl_fw_version {
	u16 major;
	u16 minor;
	u16 hotfix;
	u16 build;
};
#endif

struct skl_dma_buff_config {
	u32 min_size_bytes;
	u32 max_size_bytes;
};

enum skl_alh_support_level {
	ALH_NO_SUPPORT = 0x00000,
	ALH_CAVS_1_8_CNL = 0x10000,
};

struct skl_clk_config {
	u32 clock_source;
	u32 clock_param_mask;
};

struct skl_scheduler_config {
	u32  sys_tick_multiplier;
	u32  sys_tick_divider;
	u32  sys_tick_source;
	u32  sys_tick_cfg_length;
	u32  *sys_tick_cfg;
};

struct skl_fw_property_info {
	struct skl_fw_version version;
	u32 memory_reclaimed;
	u32 slow_clock_freq_hz;
	u32 fast_clock_freq_hz;
	enum skl_alh_support_level alh_support;
	u32 ipc_dl_mailbox_bytes;
	u32 ipc_ul_mailbox_bytes;
	u32 trace_log_bytes;
	u32 max_ppl_count;
	u32 max_astate_count;
	u32 max_module_pin_count;
	u32 modules_count;
	u32 max_mod_inst_count;
	u32 max_ll_tasks_per_pri_count;
	u32 ll_pri_count;
	u32 max_dp_tasks_count;
	u32 max_libs_count;
	u32 xtal_freq_hz;
	struct skl_clk_config clk_config;
	struct skl_scheduler_config scheduler_config;
	u32 num_dma_cfg;
	struct skl_dma_buff_config *dma_config;
};

struct skl_sst {
	struct device *dev;
	struct sst_dsp *dsp;
	/* Flag to indicate if fw is loaded or not */
	bool fw_loaded;

	/* boot */
	wait_queue_head_t boot_wait;
	bool boot_complete;

	/* IPC messaging */
	struct sst_generic_ipc ipc;

	const struct skl_dsp_ops *dsp_ops;

	struct skl_d0i3_data d0i3_data;

	struct skl_probe_config probe_config;
	struct skl_manifest manifest;

	int (*validate_fw_version)(struct skl_sst *ctx);

	/* Callback to update D0i3C register */
	void (*update_d0i3c)(struct device *dev,  bool enable);
	struct snd_soc_platform *platform;
	struct skl_dsp_notify_ops notify_ops;
	struct skl_dsp_notify_params *params;
	/* callback for miscbdge */
	void (*enable_miscbdcge)(struct device *dev, bool enable);
	/*Is CGCTL.MISCBDCGE disabled*/
	bool miscbdcg_disabled;
	/* is first boot yet to be done? */
	bool is_first_boot;
	struct list_head notify_kctls;
	/* Populate module information */
	struct list_head uuid_list;

	/* firmware configuration information */
	struct skl_fw_property_info fw_property;
};

struct skl_ipc_init_instance_msg {
	u32 module_id;
	u32 instance_id;
	u16 param_data_size;
	u8 ppl_instance_id;
	u8 core_id;
	u8 domain;
};

struct skl_ipc_bind_unbind_msg {
	u32 module_id;
	u32 instance_id;
	u32 dst_module_id;
	u32 dst_instance_id;
	u8 src_queue;
	u8 dst_queue;
	bool bind;
};

struct skl_ipc_large_payload {
	u32 module_id;
	u32 param_data_size;
};

struct skl_ipc_large_config_msg {
	u32 module_id;
	u32 instance_id;
	u32 large_param_id;
	u32 param_data_size;
};

struct skl_ipc_d0ix_msg {
	u32 module_id;
	u32 instance_id;
	u8 streaming;
	u8 wake;
};

struct skl_log_state {
	u32	enable;
	u32	priority;
};

struct skl_log_state_msg {
	uint32_t  aging_timer_period;
	uint32_t  fifo_full_timer_period;

	u32	core_mask;
	struct	skl_log_state logs_core[2];
};

struct SystemTime {
	uint32_t  val_l;
	uint32_t  val_u;
};

struct fw_version {
	u16 major;
	u16 minor;
	u16 hotfix;
	u16 build;
} __packed;

struct sw_version {
	u16 major;
	u16 minor;
	u16 hotfix;
	u16 build;
} __packed;

struct type0_crash_data {
	u16 type;
	u16 length;
	u32 crash_dump_ver;
	u16 bus_dev_id;
	u16 cavs_hw_version;
	struct fw_version fw_ver;
	struct sw_version sw_ver;
} __packed;

struct adsp_type1_crash_data {
	u32 mod_uuid[4];
	u32 hash1[2];
	u16 mod_id;
	u16 rsvd;
} __packed;

struct type1_crash_data {
	u16 type;
	u16 length;
	struct adsp_type1_crash_data type1_data[0];
} __packed;

struct type2_crash_data {
	u16 type;
	u16 length;
	u32 fwreg[FW_REG_SZ];
} __packed;

struct skl_module_notify {
	u32 unique_id;
	u32 event_id;
	u32 event_data_size;
	u32 event_data[0];
} __packed;

/* Timeout values in milliseconds for response from FW */
#define SKL_IPC_BOOT_MSECS              3000
#define SKL_IPC_LOAD_LIB_TIMEOUT        3000
#define SKL_IPC_DEFAULT_TIMEOUT         1000

#define SKL_IPC_D3_MASK	0
#define SKL_IPC_D0_MASK	3

irqreturn_t skl_dsp_irq_thread_handler(int irq, void *context);

int skl_ipc_create_pipeline(struct sst_generic_ipc *sst_ipc,
		u16 ppl_mem_size, u8 ppl_type, u8 instance_id, u8 lp_mode);

int skl_ipc_delete_pipeline(struct sst_generic_ipc *sst_ipc, u8 instance_id);

int skl_ipc_set_pipeline_state(struct sst_generic_ipc *sst_ipc,
		u8 instance_id,	enum skl_ipc_pipeline_state state);

int skl_ipc_save_pipeline(struct sst_generic_ipc *ipc,
		u8 instance_id, int dma_id);

int skl_ipc_restore_pipeline(struct sst_generic_ipc *ipc, u8 instance_id);

int skl_ipc_init_instance(struct sst_generic_ipc *sst_ipc,
		struct skl_ipc_init_instance_msg *msg, void *param_data);

int skl_ipc_delete_instance(struct sst_generic_ipc *sst_ipc,
				struct skl_ipc_init_instance_msg *msg);

int skl_ipc_bind_unbind(struct sst_generic_ipc *sst_ipc,
		struct skl_ipc_bind_unbind_msg *msg);

int skl_ipc_load_modules(struct sst_generic_ipc *ipc, u8 module_cnt,
							void *data);

int skl_ipc_unload_modules(struct sst_generic_ipc *ipc, u8 module_cnt,
							void *data);

int skl_ipc_set_dx(struct sst_generic_ipc *ipc,
		u8 instance_id, u16 module_id, struct skl_ipc_dxstate_info *dx);

int skl_ipc_set_large_config(struct sst_generic_ipc *ipc,
		struct skl_ipc_large_config_msg *msg, u32 *param);

int skl_ipc_get_large_config(struct sst_generic_ipc *ipc,
		struct skl_ipc_large_config_msg *msg, u32 *param,
		u32 *txparam, u32 size);

int skl_sst_ipc_load_library(struct sst_generic_ipc *ipc, u8 dma_id,
		u8 table_id);

int skl_ipc_set_d0ix(struct sst_generic_ipc *ipc,
		struct skl_ipc_d0ix_msg *msg);
int skl_ipc_set_dma_cfg(struct sst_generic_ipc *ipc, u8 instance_id,
		u16 module_id, u32 *data);
int skl_ipc_set_sched_cfg(struct sst_generic_ipc *ipc, u8 instance_id,
			u16 module_id, u32 *data);

int skl_dsp_enable_logging(struct sst_generic_ipc *ipc, int core, int enable);
int skl_dsp_set_system_time(struct skl_sst *skl_sst);

void skl_ipc_int_enable(struct sst_dsp *dsp);
void skl_ipc_op_int_enable(struct sst_dsp *ctx);
void skl_ipc_op_int_disable(struct sst_dsp *ctx);
void skl_ipc_int_disable(struct sst_dsp *dsp);

bool skl_ipc_int_status(struct sst_dsp *dsp);
void skl_ipc_free(struct sst_generic_ipc *ipc);
int skl_ipc_init(struct device *dev, struct skl_sst *skl);
void skl_clear_module_cnt(struct sst_dsp *ctx);

void skl_ipc_process_reply(struct sst_generic_ipc *ipc,
		struct skl_ipc_header header);
int skl_ipc_process_notification(struct sst_generic_ipc *ipc,
		struct skl_ipc_header header);
void skl_ipc_tx_data_copy(struct ipc_message *msg, char *tx_data,
		size_t tx_size);
#endif /* __SKL_IPC_H */
