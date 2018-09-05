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
#include <sound/memalloc.h>
#include "../common/sst-ipc.h"
#include "skl-sst-dsp.h"
#include "skl-tplg-interface.h"

struct sst_dsp;
struct skl_sst;
struct sst_generic_ipc;

#define NO_OF_INJECTOR 6
#define NO_OF_EXTRACTOR 8
#define FW_REG_SZ 1024
#define	SKL_EVENT_GLB_MODULE_NOTIFICATION	12
#define	SKL_TPLG_CHG_NOTIFY	3

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

enum base_fw_run_time_param {
	ADSP_PROPERTIES = 0,
	ADSP_RESOURCE_STATE = 1,
	NOTIFICATION_MASK = 3,
	ASTATE_TABLE = 4,
	DMA_CONTROL = 5,
	ENABLE_LOGS = 6,
	FIRMWARE_CONFIG = 7,
	HARDWARE_CONFIG = 8,
	MODULES_INFO = 9,
	PIPELINE_LIST_INFO = 10,
	PIPELINE_PROPS = 11,
	SCHEDULERS_INFO = 12,
	GATEWAYS_INFO = 13,
	MEMORY_STATE_INFO = 14,
	POWER_STATE_INFO = 15
};

struct skl_ipc_dxstate_info {
	u32 core_mask;
	u32 dx_mask;
};

struct skl_ipc_header {
	u32 primary;
	u32 extension;
};

struct skl_dsp_cores {
	unsigned int count;
	enum skl_dsp_states *state;
	int *usage_count;
};

/**
 * skl_d0i3_data: skl D0i3 counters data struct
 *
 * @streaming: Count of usecases that can attempt streaming D0i3
 * @non_streaming: Count of usecases that can attempt non-streaming D0i3
 * @non_d0i3: Count of usecases that cannot attempt D0i3
 * @state: current state
 * @work: D0i3 worker thread
 */
struct skl_d0i3_data {
	int streaming;
	int non_streaming;
	int non_d0i3;
	enum skl_dsp_d0i3_states state;
	struct delayed_work work;
};

#define SKL_LIB_NAME_LENGTH 44
#define SKL_MAX_LIB 16

struct skl_lib_info {
	char name[SKL_LIB_NAME_LENGTH];
	const struct firmware *fw;
};

struct injector_data {
	/* connect or disconnect */
	u8 operation;
	/* Specifies EXTRACTOR or INJECTOR or INJECT_REEXTRACT */
	u32 purpose;
	/* Injector probe param */
	u32 probe_point_id;
	struct hdac_ext_stream *stream;
	int dma_id;
	int dma_buf_size;
	enum skl_probe_state_inj state;
};

struct extractor_data {
	/* Probe connect or disconnect */
	u8 operation;
	/* Specifies EXTRACTOR or INJECTOR or INJECT_REEXTRACT */
	u32 purpose;
	/* Extractor probe param */
	u32 probe_point_id;
	enum skl_probe_state_ext state;
};

struct skl_probe_config {
	struct snd_soc_dapm_widget *w;
	/* Number of extractor DMA's used */
	int e_refc;

	/* Number of injector DMA's used */
	int i_refc;

	int edma_id;
	int edma_type;
	int edma_buffsize;
	int no_extractor;
	int no_injector;
	struct hdac_ext_stream *estream;
	struct injector_data iprobe[NO_OF_INJECTOR];
	struct extractor_data eprobe[NO_OF_EXTRACTOR];
};

struct bra_conf {
	struct snd_dma_buffer pb_dmab;
	struct snd_dma_buffer cp_dmab;
	int pb_stream_tag;
	int cp_stream_tag;
	struct skl_pipe *pb_pipe;
	struct skl_pipe *cp_pipe;
};

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

enum skl_cavs_version {
	CAVS_VER_NA = 0x0,
	CAVS_VER_1_5 = 0x1005,
	CAVS_VER_1_8 = 0x1008
};

enum skl_i2s_version {
	I2S_VER_15_SKYLAKE = 0x00000,
	I2S_VER_15_BROXTON = 0x10000,
	I2S_VER_15_BROXTON_P = 0x20000
};

struct skl_i2s_capabilities {
	enum skl_i2s_version version;
	u32 controller_count;
	u32 *controller_base_addr;
};

struct skl_gpdma_capabilities {
	u32 lp_ctrl_count;
	u32 *lp_ch_count;
	u32 hp_ctrl_count;
	u32 *hp_ch_count;
};

struct skl_hw_property_info {
	enum skl_cavs_version cavs_version;
	u32 dsp_cores;
	u32 mem_page_bytes;
	u32 total_phys_mem_pages;
	struct skl_i2s_capabilities i2s_caps;
	struct skl_gpdma_capabilities gpdma_caps;
	u32 gateway_count;
	u32 hb_ebb_count;
	u32 lp_ebb_count;
	u32 ebb_size_bytes;
};

struct skl_notify_kctrl_info {
	struct list_head list;
	u32 notify_id;
	struct snd_kcontrol *notify_kctl;
};

struct skl_sst {
	struct device *dev;
	struct sst_dsp *dsp;

	/* boot */
	wait_queue_head_t boot_wait;
	bool boot_complete;

	/* module load */
	wait_queue_head_t mod_load_wait;
	bool mod_load_complete;
	bool mod_load_status;

	/* IPC messaging */
	struct sst_generic_ipc ipc;

	/* callback for miscbdge */
	void (*enable_miscbdcge)(struct device *dev, bool enable);
	/* Is CGCTL.MISCBDCGE disabled */
	bool miscbdcg_disabled;

	/* Populate module information */
	struct list_head uuid_list;

	/* Is firmware loaded */
	bool fw_loaded;

	/* first boot ? */
	bool is_first_boot;

	/* multi-core */
	struct skl_dsp_cores cores;

	/* library info */
	struct skl_lib_info  lib_info[SKL_MAX_LIB];
	int lib_count;

	/* Callback to update D0i3C register */
	void (*update_d0i3c)(struct device *dev, bool enable);

	struct skl_dsp_notify_ops notify_ops;

	struct skl_d0i3_data d0i3;

	const struct skl_dsp_ops *dsp_ops;

	/* SDW Devices in DSP Space */
	int num_sdw_controllers;
	/* Array of sdw masters */
	struct sdw_master *mstr;

	struct skl_probe_config probe_config;

	/* BRA configuration data */
	struct bra_conf *bra_pipe_data;

	/* firmware configuration information */
	struct skl_fw_property_info fw_property;

	/* hardware configuration information */
	struct skl_hw_property_info hw_property;

	/* sysfs for module info */
	struct skl_sysfs_tree *sysfs_tree;

	struct list_head notify_kctls;
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
	struct	skl_log_state logs_core[4];
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

struct skl_module_notify {
	u32 unique_id;
	u32 event_id;
	u32 event_data_size;
	u32 event_data[0];
} __packed;

/* Timeout values in milliseconds for response from FW */
#define SKL_IPC_BOOT_MSECS              3000
#define SKL_IPC_LOAD_LIB_TIMEOUT        3000
#define SKL_IPC_DEFAULT_TIMEOUT         300

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

int skl_ipc_load_modules(struct sst_generic_ipc *ipc,
				u8 module_cnt, void *data);

int skl_ipc_unload_modules(struct sst_generic_ipc *ipc,
				u8 module_cnt, void *data);

int skl_ipc_set_dx(struct sst_generic_ipc *ipc,
		u8 instance_id, u16 module_id, struct skl_ipc_dxstate_info *dx);

int skl_ipc_set_large_config(struct sst_generic_ipc *ipc,
		struct skl_ipc_large_config_msg *msg, u32 *param);

int skl_ipc_get_large_config(struct sst_generic_ipc *ipc,
		struct skl_ipc_large_config_msg *msg, u32 *param,
		u32 *txparam, u32 tx_bytes, size_t *rx_bytes);

int skl_sst_ipc_load_library(struct sst_generic_ipc *ipc,
			u8 dma_id, u8 table_id, bool wait);

int skl_ipc_set_d0ix(struct sst_generic_ipc *ipc,
		struct skl_ipc_d0ix_msg *msg);

int skl_ipc_check_D0i0(struct sst_dsp *dsp, bool state);

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
int skl_notify_tplg_change(struct skl_sst *ctx, int type);
int skl_dsp_crash_dump_read(struct skl_sst *ctx);

void skl_ipc_set_dma_cfg(struct sst_generic_ipc *ipc, u8 instance_id,
			u16 module_id, u32 *data);
#endif /* __SKL_IPC_H */
