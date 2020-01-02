/*
 * skl-tplg-interface.h - Intel DSP FW private data interface
 *
 * Copyright (C) 2015 Intel Corp
 * Author: Jeeja KP <jeeja.kp@intel.com>
 *	    Nilofer, Samreen <samreen.nilofer@intel.com>
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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

#ifndef __HDA_TPLG_INTERFACE_H__
#define __HDA_TPLG_INTERFACE_H__

/* Default types range from 0~12. type can range from 0 to 0xff
 * SST types start at higher to avoid any overlapping in future */
#define SKL_CONTROL_TYPE_BYTE_EXT	0x100
#define SKL_CONTROL_TYPE_VOLUME         0x102
#define SKL_CONTROL_TYPE_RAMP_DURATION  0x103
#define SKL_CONTROL_TYPE_RAMP_TYPE      0x104
#define SKL_CONTROL_TYPE_DSP_LOG      0x105
#define SKL_CONTROL_TYPE_NOTIFICATION_LOG  0x106
#define SKL_CONTROL_TYPE_BYTE_PROBE	0x101

#define HDA_SST_CFG_MAX	900 /* size of copier cfg*/
#define SKL_MAX_IN_QUEUE 8
#define SKL_MAX_OUT_QUEUE 8
#define SKL_MAX_MODULES	32
#define SKL_MAX_MODULE_RESOURCES 64
#define SKL_MAX_MODULE_FORMATS 64
#define SKL_MAX_PATH_CONFIGS	32
#define SKL_MAX_MODULES_IN_PIPE	8
#define SKL_MAX_NAME_LENGTH	16
#define SKL_MOD_NAME 40 /* Length of GUID string */
#define STEREO 2

#define SKL_MAX_FW_BINARY 8
#define SKL_MAX_PARAMS_TYPES	4

#define LIB_NAME_LENGTH 128
#define HDA_MAX_LIB    16
#define MAX_DMA_CFG    24
#define MAX_SSP		6
#define SKL_UUID_STR_SZ 40
#define MAX_LL_SRC_CFG  8
/* Event types goes here */
/* Reserve event type 0 for no event handlers */
enum skl_event_types {
	SKL_EVENT_NONE = 0,
	SKL_MIXER_EVENT,
	SKL_MUX_EVENT,
	SKL_VMIXER_EVENT,
	SKL_PGA_EVENT
};

/**
 * enum skl_ch_cfg - channel configuration
 *
 * @SKL_CH_CFG_MONO:	One channel only
 * @SKL_CH_CFG_STEREO:	L & R
 * @SKL_CH_CFG_2_1:	L, R & LFE
 * @SKL_CH_CFG_3_0:	L, C & R
 * @SKL_CH_CFG_3_1:	L, C, R & LFE
 * @SKL_CH_CFG_QUATRO:	L, R, Ls & Rs
 * @SKL_CH_CFG_4_0:	L, C, R & Cs
 * @SKL_CH_CFG_5_0:	L, C, R, Ls & Rs
 * @SKL_CH_CFG_5_1:	L, C, R, Ls, Rs & LFE
 * @SKL_CH_CFG_DUAL_MONO: One channel replicated in two
 * @SKL_CH_CFG_I2S_DUAL_STEREO_0: Stereo(L,R) in 4 slots, 1st stream:[ L, R, -, - ]
 * @SKL_CH_CFG_I2S_DUAL_STEREO_1: Stereo(L,R) in 4 slots, 2nd stream:[ -, -, L, R ]
 * @SKL_CH_CFG_7_1:	L, C, R, Ls, Rs & LFE., LS, RS
 * @SKL_CH_CFG_7_0:     L, C, R, Ls, Rs, LS, RS
 * @SKL_CH_CFG_INVALID:	Invalid
 */
enum skl_ch_cfg {
	SKL_CH_CFG_MONO = 0,
	SKL_CH_CFG_STEREO = 1,
	SKL_CH_CFG_2_1 = 2,
	SKL_CH_CFG_3_0 = 3,
	SKL_CH_CFG_3_1 = 4,
	SKL_CH_CFG_QUATRO = 5,
	SKL_CH_CFG_4_0 = 6,
	SKL_CH_CFG_5_0 = 7,
	SKL_CH_CFG_5_1 = 8,
	SKL_CH_CFG_DUAL_MONO = 9,
	SKL_CH_CFG_I2S_DUAL_STEREO_0 = 10,
	SKL_CH_CFG_I2S_DUAL_STEREO_1 = 11,
	SKL_CH_CFG_7_1 = 12,
	CHANNEL_CONFIG_7_0 = 13,
	SKL_CH_CFG_INVALID
};

enum skl_module_type {
	SKL_MODULE_TYPE_MIXER = 0,
	SKL_MODULE_TYPE_COPIER,
	SKL_MODULE_TYPE_UPDWMIX,
	SKL_MODULE_TYPE_SRCINT,
	SKL_MODULE_TYPE_ALGO,
	SKL_MODULE_TYPE_MIC_SELECT,
	SKL_MODULE_TYPE_GAIN,
	SKL_MODULE_TYPE_PROBE,
	SKL_MODULE_TYPE_ASRC,
	SKL_MODULE_TYPE_BASE_OUTFMT,
};

enum skl_core_affinity {
	SKL_AFFINITY_CORE_0 = 0,
	SKL_AFFINITY_CORE_1,
	SKL_AFFINITY_CORE_MAX
};

enum skl_pipe_conn_type {
	SKL_PIPE_CONN_TYPE_NONE = 0,
	SKL_PIPE_CONN_TYPE_FE,
	SKL_PIPE_CONN_TYPE_BE
};

enum skl_hw_conn_type {
	SKL_CONN_NONE = 0,
	SKL_CONN_SOURCE = 1,
	SKL_CONN_SINK = 2
};

enum skl_dev_type {
	SKL_DEVICE_BT = 0x0,
	SKL_DEVICE_DMIC = 0x1,
	SKL_DEVICE_I2S = 0x2,
	SKL_DEVICE_SLIMBUS = 0x3,
	SKL_DEVICE_HDALINK = 0x4,
	SKL_DEVICE_HDAHOST = 0x5,
	SKL_DEVICE_NONE
};

/**
 * enum skl_interleaving - interleaving style
 *
 * @SKL_INTERLEAVING_PER_CHANNEL: [s1_ch1...s1_chN,...,sM_ch1...sM_chN]
 * @SKL_INTERLEAVING_PER_SAMPLE: [s1_ch1...sM_ch1,...,s1_chN...sM_chN]
 */
enum skl_interleaving {
	SKL_INTERLEAVING_PER_CHANNEL = 0,
	SKL_INTERLEAVING_PER_SAMPLE = 1,
};

enum skl_sample_type {
	SKL_SAMPLE_TYPE_INT_MSB = 0,
	SKL_SAMPLE_TYPE_INT_LSB = 1,
	SKL_SAMPLE_TYPE_INT_SIGNED = 2,
	SKL_SAMPLE_TYPE_INT_UNSIGNED = 3,
	SKL_SAMPLE_TYPE_FLOAT = 4
};

enum module_pin_type {
	/* All pins of the module takes same PCM inputs or outputs
	* e.g. mixout
	*/
	SKL_PIN_TYPE_HOMOGENEOUS,
	/* All pins of the module takes different PCM inputs or outputs
	* e.g mux
	*/
	SKL_PIN_TYPE_HETEROGENEOUS,
};

enum skl_module_param_type {
	SKL_PARAM_DEFAULT = 0,
	SKL_PARAM_INIT,
	SKL_PARAM_SET,
	SKL_PARAM_BIND
};

enum skl_probe_param_id_type {
	SKL_PROBE_INJECT_DMA_ATTACH = 1,
	SKL_PROBE_INJECT_DMA_DETACH,
	SKL_PROBE_CONNECT,
	SKL_PROBE_DISCONNECT
};

enum skl_probe_purpose {
	SKL_PROBE_EXTRACT = 0,
	SKL_PROBE_INJECT,
	SKL_PROBE_INJECT_REEXTRACT
};

/* Injector probe states */
enum skl_probe_state_inj {
	SKL_PROBE_STATE_INJ_NONE = 1,
	SKL_PROBE_STATE_INJ_DMA_ATTACHED,
	SKL_PROBE_STATE_INJ_CONNECTED,
	SKL_PROBE_STATE_INJ_DISCONNECTED
};

/* Extractor probe states */
enum skl_probe_state_ext {
	SKL_PROBE_STATE_EXT_NONE = 1,
	SKL_PROBE_STATE_EXT_CONNECTED
};

struct skl_dfw_module_pin {
	u8 pin_uuid[16];
	u16 module_id;
	u16 instance_id;
} __packed;

struct skl_dfw_module_fmt {
	u32 channels;
	u32 freq;
	u32 bit_depth;
	u32 valid_bit_depth;
	u32 ch_cfg;
	u32 interleaving_style;
	u32 sample_type;
	u32 ch_map;
} __packed;

struct skl_dfw_module_caps {
	u32 set_params:2;
	u32 rsvd:30;
	u32 param_id;
	u32 caps_size;
	u32 caps[HDA_SST_CFG_MAX];
};

struct skl_dfw_pipe_fmt {
	u32 freq;
	u8 channels;
	u8 bps;
} __packed;

struct skl_dfw_pipe_mcfg {
	u8 uuid[16];
	u8 instance_id;
	u8 res_idx;
	u8 fmt_idx;
} __packed;

/*
 * For each pipe config i.e., the combination of input and output format,
 * module resource and format may be different. This strucuture contains
 * module reseource and format index for each pipe config.
 */
struct dfw_pipe_module_cfg {
	u8		uuid[16];
	u8		instance_id;
	u8		res_idx;
	u8		fmt_idx;
} __packed;

/*
 * Multiple path configurations are possible for a pipeline depending on the
 * input and outpput formats supported by the pipepile. This structure stores
 * all unique configurations supported by a pipeline.
 */
struct skl_dfw_path_config {
	char	config_name[SKL_MAX_NAME_LENGTH];
	u8	config_idx;
	u8	mem_pages;
	struct dfw_pipe_module_cfg mod_cfg[SKL_MAX_MODULES_IN_PIPE];
	struct skl_dfw_pipe_fmt	in_pcm_format;
	struct skl_dfw_pipe_fmt	out_pcm_format;
} __packed;

/*
 * This represents a pipe from the topology.
 * name: Name of the pipe
 * pipe_id: unique pipe number across topology
 * pipe_priority: priority of the pipe task
 * order: Source to Sink in ascending order. For example, for a playback path,
	order will be 0 for pipe connected to Host DMA and N for pipe connected
	to LINK DMA.
 * create_priority: pipeline gets schedule based on “priority”.
	For the same ordered pipeline, they are scheduled based on the order.
 * direction: playback or capture
 * conn_type: Indicates pipe is connected to -HOST_DMA, LINK_DMA,
	NONE (if pipe is loop), INTERNAL_SRC (like tone generator)
	and INTERNAL_SINK (like aware or wov)
 * device: Device name, if pipeline is connected to the host dma. It should
	match the FE DAI stream name
 * port: Port name, if pipeline is connected to the link dma. If should match
	BE AIF widget name in machine driver
 * lp_mode: what it will continue running in low power mode
 */
struct skl_dfw_pipe {
	char name[SKL_MAX_NAME_LENGTH];
	u32 pipe_id:8;
	u32 pipe_priority:8;
	u32 create_priority:8;
	u32 rsvd1:8;
	char device[32];
	u32 order:3;
	u32 direction:2;
	u32 conn_type:3;
	u32 link_type:3;
	u32 vbus_id:3;
	u32 pipe_mode:2;
	u32 lp_mode:1;
	u32 nr_modules:4;
	u32 nr_cfgs:11;
	struct skl_dfw_path_config configs[SKL_MAX_PATH_CONFIGS];
} __packed;

struct skl_dfw_module {
	u8 uuid[16];

	u32 module_id:16;
	u32 instance_id:16;
	u32 vbus_id;

	u32 time_slot:8;
	u32 core_id:4;
	u32 module_type:8;
	u32 conn_type:4;
	u32 dev_type:4;
	u32 hw_conn_type:4;

	u32 is_dynamic_in_pin:1;
	u32 is_dynamic_out_pin:1;
	u32 fast_mode:1;
	u32 proc_domain:1;
	u32 rsvd:28;

	struct skl_dfw_pipe pipe;
	struct skl_dfw_module_pin in_pin[SKL_MAX_IN_QUEUE];
	struct skl_dfw_module_pin out_pin[SKL_MAX_OUT_QUEUE];
	struct skl_dfw_module_caps caps[SKL_MAX_PARAMS_TYPES];
} __packed;

struct skl_gain_config {
	u32 channel_id;
	u32 target_volume;
	u32 ramp_type;
	u64 ramp_duration;
} __packed;

struct skl_dfw_algo_data {
	u32 set_params:2;
	u32 runtime_applicable:1;
	u32 access_type:2;
	u32 value_cacheable:1;
	u32 notification_ctrl:1;
	u32 rsvd:25;
	u32 param_id;
	u32 max;
	char params[0];
} __packed;

struct skl_dfw_dma_ctrl_hdr {
	u32 vbus_id;
	u32 freq;
	u32 tdm_slot;
	u32 fmt;
	u32 direction;
	u32 chan;
	u32 data_size;
	u8 *data[0];
}  __packed;

struct skl_dfw_sclk_fs_cfg {
	struct skl_dfw_dma_ctrl_hdr hdr;

	/* DMA SClk&FS  TLV params */
	u32 type;
	u32 size;
	u32 sampling_frequency;
	u32 bit_depth;
	u32 channel_map;
	u32 channel_config;
	u32 interleaving_style;
	u32 number_of_channels : 8;
	u32 valid_bit_depth : 8;
	u32 sample_type : 8;
	u32 reserved : 8;
} __packed;

struct skl_dfw_clk_cfg {
	struct skl_dfw_dma_ctrl_hdr hdr;

	/* DMA Clk TLV params */
	u32 type;
	u32 size;

	u32 clk_warm_up:16;
	u32 mclk:1;
	u32 warm_up_over:1;
	u32 rsvd0:14;

	u32 clk_stop_delay:16;
	u32 keep_running:1;
	u32 clk_stop_over:1;
	u32 rsvd1:14;
} __packed;

struct lib_info {
	char name[LIB_NAME_LENGTH];
} __packed;

struct skl_dma_config {
	u32 min_size;
	u32 max_size;
} __packed;

struct skl_mem_status {
	u32 type;
	u32 size;
	u32 mem_reclaim;
} __packed;

struct skl_dsp_freq {
	u32 type;
	u32 size;
	u32 freq;
} __packed;

struct skl_dma_buff_cfg {
	u32 type;
	u32 size;
	struct skl_dma_config dma_cfg[MAX_DMA_CFG];
} __packed;

struct skl_sch_config {
	u32 type;
	u32 length;
	u32 sys_tick_mul;
	u32 sys_tick_div;
	u32 ll_src;
	u32 num_cfg;
	u32 node_info[MAX_LL_SRC_CFG];
} __packed;

struct fw_cfg_info {
	struct skl_mem_status mem_sts;
	struct skl_dsp_freq slw_frq;
	struct skl_dsp_freq fst_frq;
	struct skl_dma_buff_cfg dmacfg;
	struct skl_sch_config sch_cfg;
} __packed;

/*
 * This represents the pin format of a module.
 */
struct skl_dfw_pin_fmt {
	u8	pin_id;
	struct	skl_dfw_module_fmt	pin_fmt;
} __packed;

/*
 * Global structure to hold all module interface related information in a
 * single place. It contains information such as channel config, sample size,
 * channel map etc. for each module pin.
 */
struct skl_dfw_module_intf {
	u8 fmt_idx;
	u8 nr_input_fmt;
	u8 nr_output_fmt;
	struct	skl_dfw_pin_fmt	input[SKL_MAX_IN_QUEUE];
	struct	skl_dfw_pin_fmt	output[SKL_MAX_OUT_QUEUE];
} __packed;

/*
 * Each pin resources whether it is input pin or output pin.
 * pin_index: Index of the pin of a module
 * buf_size: input/output frame size
 */
struct module_pin_resources {
	u8	pin_index;
	u32	buf_size;
} __packed;

/*
 * This represents module resources:
 * res_idx: index of res in the array of module resources
 * is_pages: memory pages to be allocated for this module instance
 * cps: DSP cycles needed to process one second of data
 * ibs: size of the module instance input frame (in bytes)
 * obs: Size of the module instance output frame (in bytes)
 * dma_buffer_size: buffer size allocated for gateway DMA (in bytes)
 * module_flags: flags for the module
 * cpc: DSP cycles required to process one input frame
 * obls: Output block size
 */
struct skl_dfw_module_res {
	u8	res_idx;
	u32	is_pages;
	u32	cps;
	u32	ibs;
	u32	obs;
	u32	dma_buffer_size;
	u32	cpc;
	u32	module_flags;
	u32	obls;
	u8	nr_input_pins;
	u8	nr_output_pins;
	struct module_pin_resources	input[SKL_MAX_IN_QUEUE];
	struct module_pin_resources	output[SKL_MAX_OUT_QUEUE];
} __packed;

/*
 * Previously, much of the common information for a module was getting
 * dupliated across all instances of a module. This structure is a common
 * placeholder for global information for each module.
 *
 * uuid: Unique identifier for the module
 * loadable:  is this module loadable
 * input_pin_type: Whether all the input pins take same PCM params
 * output_pin_type: Whether all the output pins take same PCM params
 * auto_start: whether a module instance can be created at base-fw startup
 *		without a parent pipeline
 * max_input_pins: Maximum number of input pins
 * max_output_pins: Maximum number of output pins
 * max_instance_count: Maximum instances that can co-exist simultanesouly
 * nr_resources: number of resources for the module
 * nr_interfaces: number of module interfaces
 */
struct skl_dfw_module_type {
	u16	major_version;
	u16	minor_version;
	u16	hotfix_version;
	u16	build_version;
	u8	uuid[16];
	u32	loadable:1;
	u32	input_pin_type:1;
	u32	output_pin_type:1;
	u32	auto_start:1;
	u32	max_input_pins:8;
	u32	max_output_pins:8;
	u32	max_instance_count:8;
	u32	rsvd:4;
	char	library_name[LIB_NAME_LENGTH];
	u8	nr_resources;
	u8	nr_interfaces;
	struct skl_dfw_module_res	resources[SKL_MAX_MODULE_RESOURCES];
	struct skl_dfw_module_intf	formats[SKL_MAX_MODULE_FORMATS];
} __packed;

struct skl_dfw_fw_info {
	char	binary_name[LIB_NAME_LENGTH];
	u32	man_major:16;
	u32	man_minor:16;
	u32	man_hotfix:16;
	u32	man_build:16;
	u32	ext_man_major:16;
	u32	ext_man_minor:16;
	u32	man_nr_modules:8;
	u32	ext_man_nr_modules:8;
	u32	binary_type:8;
	u32	rsvd:8;
	u32	pre_load_pages;
} __packed;

struct skl_dfw_dma_ctrl_info {
	u32 type;
	u32 size;
	struct skl_dfw_clk_cfg clk_cfg;
	struct skl_dfw_sclk_fs_cfg sclk_fs_cfg[MAX_SSP];
} __packed;

struct skl_dfw_manifest {
	u32 nr_modules:8;
	u32 lib_count:8;
	u32 nr_fw_bins:8;
	u32 rsvd:8;

	/* fw config info */
	struct fw_cfg_info cfg;
	struct lib_info lib[HDA_MAX_LIB];
	struct skl_dfw_module_type module[SKL_MAX_MODULES];
	struct skl_dfw_fw_info fw_info[SKL_MAX_FW_BINARY];
	struct skl_dfw_dma_ctrl_info dma_ctrl;
} __packed;

#endif
