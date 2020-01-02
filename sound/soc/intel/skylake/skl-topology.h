/*
 *  skl_topology.h - Intel HDA Platform topology header file
 *
 *  Copyright (C) 2014-15 Intel Corp
 *  Author: Jeeja KP <jeeja.kp@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */

#ifndef __SKL_TOPOLOGY_H__
#define __SKL_TOPOLOGY_H__

#include <linux/types.h>

#include <sound/hdaudio_ext.h>
#include <sound/soc.h>
#include "skl.h"
#include "skl-tplg-interface.h"

#define BITS_PER_BYTE 8
#define MAX_TS_GROUPS 8
#define MAX_DMIC_TS_GROUPS 4
#define MAX_FIXED_DMIC_PARAMS_SIZE 727
#define MAX_ADSP_SZ 1024
#define STEREO 2

/* Maximum number of coefficients up down mixer module */
#define UP_DOWN_MIXER_MAX_COEFF		8

#define MODULE_MAX_IN_PINS	8
#define MODULE_MAX_OUT_PINS	8
#define SKL_TPLG_CHG_NOTIFY	3

#define NO_OF_INJECTOR 6
#define NO_OF_EXTRACTOR 8
#define OUTPUT_PIN     0
#define INPUT_PIN      1
#define SKL_MAX_PARAMS_TYPES   4

enum skl_channel_index {
	SKL_CHANNEL_LEFT = 0,
	SKL_CHANNEL_RIGHT = 1,
	SKL_CHANNEL_CENTER = 2,
	SKL_CHANNEL_LEFT_SURROUND = 3,
	SKL_CHANNEL_CENTER_SURROUND = 3,
	SKL_CHANNEL_RIGHT_SURROUND = 4,
	SKL_CHANNEL_LFE = 7,
	SKL_CHANNEL_INVALID = 0xF,
};

enum skl_bitdepth {
	SKL_DEPTH_8BIT = 8,
	SKL_DEPTH_16BIT = 16,
	SKL_DEPTH_24BIT = 24,
	SKL_DEPTH_32BIT = 32,
	SKL_DEPTH_INVALID
};

enum skl_format {
	SKL_FMT_S16LE = 2,
	SKL_FMT_S24LE = 6,
	SKL_FMT_S32LE = 10,
	SKL_FMT_FLOATLE = 14,
	SKL_FMT_S24_3LE = 32
};

enum skl_s_freq {
	SKL_FS_8000 = 8000,
	SKL_FS_11025 = 11025,
	SKL_FS_12000 = 12000,
	SKL_FS_16000 = 16000,
	SKL_FS_22050 = 22050,
	SKL_FS_24000 = 24000,
	SKL_FS_32000 = 32000,
	SKL_FS_44100 = 44100,
	SKL_FS_48000 = 48000,
	SKL_FS_64000 = 64000,
	SKL_FS_88200 = 88200,
	SKL_FS_96000 = 96000,
	SKL_FS_128000 = 128000,
	SKL_FS_176400 = 176400,
	SKL_FS_192000 = 192000,
	SKL_FS_INVALID
};

enum skl_widget_type {
	SKL_WIDGET_VMIXER = 1,
	SKL_WIDGET_MIXER = 2,
	SKL_WIDGET_PGA = 3,
	SKL_WIDGET_MUX = 4
};

enum skl_pipe_type {
	SKL_PIPE_SOURCE = 0,
	SKL_PIPE_INTERMEDIATE = 3,
	SKL_PIPE_SINK = 7
};

struct probe_pt_param {
	u32 params;
	u32 connection;
	u32 node_id;
};

enum skl_widget_access {
	SKL_WIDGET_READ = 0,
	SKL_WIDGET_WRITE = 1,
	SKL_WIDGET_READ_WRITE = 2
};

struct skl_audio_data_format {
	enum skl_s_freq s_freq;
	enum skl_bitdepth bit_depth;
	u32 channel_map;
	enum skl_ch_cfg ch_cfg;
	enum skl_interleaving interleaving;
	u8 number_of_channels;
	u8 valid_bit_depth;
	u8 sample_type;
	u8 reserved[1];
} __packed;

struct skl_base_cfg {
	u32 cps;
	u32 ibs;
	u32 obs;
	u32 is_pages;
	struct skl_audio_data_format audio_fmt;
};

struct skl_pin_format {
	u32	pin_index;
	u32	buff_size;
	struct skl_audio_data_format audio_fmt;
} __packed;

struct skl_base_cfg_ext {
	struct	skl_base_cfg base_cfg;
	u16	nr_input_pins;
	u16	nr_output_pins;
	u8	reserved[8];
	u32	priv_param_length;
	char	pin_formats[0];
	char params[0];
} __packed;

struct skl_probe_gtw_cfg {
	u32 node_id;
	u32 dma_buffer_size;
} __packed;

struct skl_probe_cfg {
	struct skl_base_cfg base_cfg;
	struct skl_probe_gtw_cfg prb_cfg;
} __packed;

struct skl_cpr_gtw_cfg {
	u32 node_id;
	u32 dma_buffer_size;
	u32 config_length;
	/* not mandatory; required only for DMIC/I2S */
	u32 config_data[1];
} __packed;

struct skl_i2s_config_blob {
	u32 gateway_attrib;
	u32 tdm_ts_group[8];
	u32 ssc0;
	u32 ssc1;
	u32 sscto;
	u32 sspsp;
	u32 sstsa;
	u32 ssrsa;
	u32 ssc2;
	u32 sspsp2;
	u32 ssc3;
	u32 ssioc;
	u32 mdivc;
	u32 mdivr;
} __packed;

struct skl_dma_control {
	u32 node_id;
	u32 config_length;
	u32 config_data[0];
} __packed;

struct skl_cpr_cfg {
	struct skl_base_cfg base_cfg;
	struct skl_audio_data_format out_fmt;
	u32 cpr_feature_mask;
	struct skl_cpr_gtw_cfg gtw_cfg;
} __packed;


struct skl_src_module_cfg {
	struct skl_base_cfg base_cfg;
	enum skl_s_freq src_cfg;
	u32 mode;
} __packed;

struct notification_mask {
	u32 notify;
	u32 enable;
} __packed;

struct skl_up_down_mixer_cfg {
	struct skl_base_cfg base_cfg;
	enum skl_ch_cfg out_ch_cfg;
	/* This should be set to 1 if user coefficients are required */
	u32 coeff_sel;
	/* Pass the user coeff in this array */
	s32 coeff[UP_DOWN_MIXER_MAX_COEFF];
	u32 ch_map;
} __packed;

struct skl_algo_cfg {
	struct skl_base_cfg  base_cfg;
	char params[0];
} __packed;

struct skl_base_outfmt_cfg {
	struct skl_base_cfg base_cfg;
	struct skl_audio_data_format out_fmt;
} __packed;

enum skl_dma_type {
	SKL_DMA_HDA_HOST_OUTPUT_CLASS = 0,
	SKL_DMA_HDA_HOST_INPUT_CLASS = 1,
	SKL_DMA_HDA_HOST_INOUT_CLASS = 2,
	SKL_DMA_HDA_LINK_OUTPUT_CLASS = 8,
	SKL_DMA_HDA_LINK_INPUT_CLASS = 9,
	SKL_DMA_HDA_LINK_INOUT_CLASS = 0xA,
	SKL_DMA_DMIC_LINK_INPUT_CLASS = 0xB,
	SKL_DMA_I2S_LINK_OUTPUT_CLASS = 0xC,
	SKL_DMA_I2S_LINK_INPUT_CLASS = 0xD,
};

union skl_ssp_dma_node {
	u8 val;
	struct {
		u8 time_slot_index:4;
		u8 i2s_instance:4;
	} dma_node;
};

union skl_connector_node_id {
	u32 val;
	struct {
		u32 vindex:8;
		u32 dma_type:4;
		u32 rsvd:20;
	} node;
};

struct skl_module_fmt {
	u32 channels;
	u32 s_freq;
	u32 bit_depth;
	u32 valid_bit_depth;
	u32 ch_cfg;
	u32 interleaving_style;
	u32 sample_type;
	u32 ch_map;
};

struct skl_module_cfg;

struct skl_gain_module_config {
	struct skl_base_cfg mconf;
	struct skl_gain_config gain_cfg;
};

struct skl_module_inst_id {
	uuid_le pin_uuid;
	int module_id;
	u32 instance_id;
};

enum skl_module_pin_state {
	SKL_PIN_UNBIND = 0,
	SKL_PIN_BIND_DONE = 1,
};

struct skl_module_pin {
	struct skl_module_inst_id id;
	bool is_dynamic;
	bool in_use;
	enum skl_module_pin_state pin_state;
	struct skl_module_cfg *tgt_mcfg;
};

struct skl_specific_cfg {
	u32 set_params;
	u32 param_id;
	u32 caps_size;
	u32 *caps;
};

enum skl_pipe_state {
	SKL_PIPE_INVALID = 0,
	SKL_PIPE_CREATED = 1,
	SKL_PIPE_PAUSED = 2,
	SKL_PIPE_STARTED = 3,
	SKL_PIPE_RESET = 4
};

struct skl_pipe_module {
	struct snd_soc_dapm_widget *w;
	struct list_head node;
};

struct skl_pipe_params {
	u8 host_dma_id;
	u8 link_dma_id;
	u32 ch;
	u32 s_freq;
	u32 s_fmt;
	u8 linktype;
	int stream;
};

struct skl_pipe_fmt {
	u32 freq;
	u8 channels;
	u8 bps;
};

struct skl_pipe_mcfg {
	uuid_le uuid;
	u8 instance_id;
	u8 res_idx;
	u8 fmt_idx;
};

struct skl_path_config {
	char name[SKL_MAX_NAME_LENGTH];
	u8 idx;
	u8 mem_pages;
	struct skl_pipe_mcfg mod_cfg[SKL_MAX_MODULES_IN_PIPE];
	struct skl_pipe_fmt in_fmt;
	struct skl_pipe_fmt out_fmt;
};

struct skl_pipe {
	u8 ppl_id;
	u8 pipe_priority;
	u16 conn_type;
	u32 memory_pages;
	u8 lp_mode;
	char name[SKL_MAX_NAME_LENGTH];
	char device[32];
	u8 create_priority;
	u8 order;
	u8 direction;
	u8 link_type;
	u8 vbus_id;
	u8 pipe_mode;
	struct skl_pipe_params *p_params;
	enum skl_pipe_state state;
	u8 nr_modules;
	u8 cur_config_idx;
	u8 nr_cfgs;
	struct skl_path_config *configs;
	struct list_head w_list;
	bool passthru;
};

enum skl_module_state {
	SKL_MODULE_UNINIT = 0,
	SKL_MODULE_LOADED = 1,
	SKL_MODULE_INIT_DONE = 2,
	SKL_MODULE_BIND_DONE = 3,
	SKL_MODULE_UNLOADED = 4,
};

struct skl_gain_data {
	u64 ramp_duration;
	u32 ramp_type;
	u32 volume[STEREO];
};

struct skl_module_pin_fmt {
	u8	pin_id;
	struct	skl_module_fmt	pin_fmt;
};

struct skl_module_intf {
	u8 fmt_idx;
	u8 nr_input_fmt;
	u8 nr_output_fmt;
	struct	skl_module_pin_fmt	input[SKL_MAX_IN_QUEUE];
	struct	skl_module_pin_fmt	output[SKL_MAX_OUT_QUEUE];
};

struct skl_module_pin_resources {
	u8 pin_index;
	u32 buf_size;
};

struct skl_module_res {
	u8 res_idx;
	u32 is_pages;
	u32 cps;
	u32 ibs;
	u32 obs;
	u32 dma_buffer_size;
	u32 cpc;
	u32 module_flags;
	u32 obls;
	u8 nr_input_pins;
	u8 nr_output_pins;
	struct skl_module_pin_resources input[SKL_MAX_IN_QUEUE];
	struct skl_module_pin_resources output[SKL_MAX_OUT_QUEUE];
};

struct skl_module {
	u16 major_version;
	u16 minor_version;
	u16 hotfix_version;
	u16 build_version;
	uuid_le	uuid;
	u8 loadable;
	u8 input_pin_type;
	u8 output_pin_type;
	u8 auto_start;
	u8 max_input_pins;
	u8 max_output_pins;
	u8 max_instance_count;
	char library_name[LIB_NAME_LENGTH];
	u8 nr_resources;
	u8 nr_interfaces;
	struct skl_module_res *resources;
	struct skl_module_intf *formats;
};

struct skl_fw_info {
	char binary_name[LIB_NAME_LENGTH];
	u32 pre_load_pages;
	u16 man_major;
	u16 man_minor;
	u16 man_hotfix;
	u16 man_build;
	u16 ext_man_major;
	u16 ext_man_minor;
	u8 man_nr_modules;
	u8 ext_man_nr_modules;
	u8 binary_type;
};

struct skl_manifest {
	u8 nr_modules;
	u8 lib_count;
	u8 nr_fw_bins;
	struct fw_cfg_info cfg;
	struct lib_info lib[HDA_MAX_LIB];
	struct skl_module *modules;
	struct skl_fw_info *fw_info;
	struct skl_dfw_dma_ctrl_info dma_ctrl;
};

struct skl_module_cfg {
	u8 guid[16];
	struct skl_module_inst_id id;
	struct skl_module *module;
	u8 res_idx;
	u8 fmt_idx;
	u8 domain;
	u8 core_id;
	u8 dev_type;
	u8 dma_id;
	u8 time_slot;
	u32 vbus_id;
	u32 fast_mode;
	struct skl_module_pin *m_in_pin;
	struct skl_module_pin *m_out_pin;
	enum skl_module_type m_type;
	enum skl_hw_conn_type  hw_conn_type;
	enum skl_module_state m_state;
	struct skl_pipe *pipe;
	struct skl_specific_cfg formats_config[SKL_MAX_PARAMS_TYPES];
	struct skl_gain_data gain_data;
};

struct skl_algo_data {
	u32 param_id;
	u32 runtime_applicable:1;
	u32 access_type:2;
	u32 value_cacheable:1;
	u32 notification_ctrl:1;
	u32 rsvd:27;
	u32 set_params;
	u32 max;
	char *params;
};

struct skl_probe_data {
	/* connect or disconnect */
	u8 operation;
	/* extractor or injector or inject-reextract */
	u32 purpose;
	u32 probe_point_id;
	u32 node_id;
} __packed;

struct skl_probe_attach_inj_dma {
	union skl_connector_node_id node_id;
	u32 dma_buff_size;
} __packed;
struct skl_pipeline {
	struct skl_pipe *pipe;
	struct list_head node;
};

static inline struct skl *get_skl_ctx(struct device *dev)
{
	struct hdac_ext_bus *ebus = dev_get_drvdata(dev);

	return ebus_to_skl(ebus);
}

struct mod_set_get {
	u32 size;
	u32 primary;
	u32 extension;
	u32 mailbx[1024];
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

struct fw_ipc_data {
	u32 replysz;
	u32 adsp_id;
	u32 mailbx[MAX_ADSP_SZ];
	struct mutex mutex;
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

int skl_tplg_be_update_params(struct snd_soc_dai *dai,
	struct skl_pipe_params *params);
int skl_dsp_set_dma_clk_controls(struct skl_sst *ctx);
u32 skl_get_node_id(struct skl_sst *ctx, struct skl_module_cfg *mconfig);
void skl_tplg_set_be_dmic_config(struct snd_soc_dai *dai,
	struct skl_pipe_params *params, int stream);
int skl_tplg_init(struct snd_soc_platform *platform,
				struct hdac_ext_bus *ebus);
struct skl_module_cfg *skl_tplg_fe_get_cpr_module(
		struct snd_soc_dai *dai, int stream);
int skl_tplg_update_pipe_params(struct device *dev,
			struct skl_module_cfg *mconfig,
			struct skl_pipe_params *params,
			snd_pcm_format_t fmt);
void skl_tplg_update_d0i3_stream_count(struct snd_soc_dai *dai, bool open);

int skl_create_pipeline(struct skl_sst *ctx, struct skl_pipe *pipe);

int skl_run_pipe(struct skl_sst *ctx, struct skl_pipe *pipe);

int skl_pause_pipe(struct skl_sst *ctx, struct skl_pipe *pipe);

int skl_delete_pipe(struct skl_sst *ctx, struct skl_pipe *pipe);

int skl_stop_pipe(struct skl_sst *ctx, struct skl_pipe *pipe);

int skl_reset_pipe(struct skl_sst *ctx, struct skl_pipe *pipe);

int skl_init_module(struct skl_sst *ctx, struct skl_module_cfg *module_config);

int skl_init_probe_module(struct skl_sst *ctx, struct skl_module_cfg *module_config);

int skl_uninit_probe_module(struct skl_sst *ctx, struct skl_module_cfg *module_config);

int skl_probe_get_index(struct snd_soc_dai *dai,
				struct skl_probe_config *pconfig);

int skl_probe_attach_inj_dma(struct snd_soc_dapm_widget *w,
					struct skl_sst *ctx, int index);
int skl_probe_detach_inj_dma(struct skl_sst *ctx,
					struct snd_soc_dapm_widget *w, int index);
int skl_probe_point_set_config(struct snd_soc_dapm_widget *w,
						struct skl_sst *ctx, int direction,
						struct snd_soc_dai *dai);
int skl_tplg_set_module_params(struct snd_soc_dapm_widget *w,
						struct skl_sst *ctx);

int skl_bind_modules(struct skl_sst *ctx, struct skl_module_cfg
	*src_module, struct skl_module_cfg *dst_module);

int skl_unbind_modules(struct skl_sst *ctx, struct skl_module_cfg
	*src_module, struct skl_module_cfg *dst_module);
int skl_probe_point_disconnect_ext(struct skl_sst *ctx,
					struct snd_soc_dapm_widget *w);
int skl_probe_point_disconnect_inj(struct skl_sst *ctx,
					struct snd_soc_dapm_widget *w, int index);
int skl_set_module_params(struct skl_sst *ctx, u32 *params, int size,
			u32 param_id, struct skl_module_cfg *mcfg);
int skl_get_module_params(struct skl_sst *ctx, u32 *params, int size,
			  u32 param_id, struct skl_module_cfg *mcfg);

int skl_load_modules(struct skl_sst *ctx, struct skl_module_cfg *mcfg);

int skl_unload_modules(struct skl_sst *ctx, struct skl_module_cfg *mcfg);

int is_skl_dsp_widget_type(struct snd_soc_dapm_widget *w);

enum skl_bitdepth skl_get_bit_depth(int params);
int snd_skl_get_module_info(struct skl_sst *ctx, struct skl_module_cfg *mconfig);
void fw_exception_dump_read(struct skl_sst *ctx, int stack_size);

struct nhlt_specific_cfg *skl_get_nhlt_specific_cfg(struct skl *skl,
	u32 instance, u8 link_type, u8 s_fmt, u8 num_ch, u32 s_rate, u8 dir);

#endif
