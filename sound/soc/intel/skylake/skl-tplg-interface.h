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

/*
 * Default types range from 0~12. type can range from 0 to 0xff
 * SST types start at higher to avoid any overlapping in future
 */
#define SKL_CONTROL_TYPE_BYTE_TLV	0x100
#define SKL_CONTROL_TYPE_MIC_SELECT	0x102
#define SKL_CONTROL_TYPE_BYTE_PROBE	0x101
#define SKL_CONTROL_TYPE_MULTI_IO_SELECT	0x103
#define SKL_CONTROL_TYPE_VOLUME		0x104
#define SKL_CONTROL_TYPE_RAMP_DURATION	0x105
#define SKL_CONTROL_TYPE_RAMP_TYPE	0x106

#define HDA_SST_CFG_MAX	900 /* size of copier cfg*/
#define MAX_IN_QUEUE 8
#define MAX_OUT_QUEUE 8

#define SDW_MAX_MASTERS 4

#define SKL_UUID_STR_SZ 40
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
	SKL_CH_CFG_4_CHANNEL = 12,
	SKL_CH_CFG_INVALID
};

enum skl_module_type {
	SKL_MODULE_TYPE_MIXER = 0,
	SKL_MODULE_TYPE_COPIER,
	SKL_MODULE_TYPE_UPDWMIX,
	SKL_MODULE_TYPE_SRCINT,
	SKL_MODULE_TYPE_ALGO,
	SKL_MODULE_TYPE_BASE_OUTFMT,
	SKL_MODULE_TYPE_KPB,
	SKL_MODULE_TYPE_MIC_SELECT,
	SKL_MODULE_TYPE_PROBE,
	SKL_MODULE_TYPE_ASRC,
	SKL_MODULE_TYPE_GAIN
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
	SKL_DEVICE_NONE = 0x6,
	SKL_DEVICE_SDW_PCM = 0x7,
	SKL_DEVICE_SDW_PDM = 0x8,
	SKL_DEVICE_MAX = SKL_DEVICE_SDW_PDM,
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

struct skl_dfw_sdw_aggdata {
	u32 alh_stream_num;
	u32 channel_mask;
} __packed;

struct skl_dfw_algo_data {
	u32 set_params:2;
	u32 rsvd:30;
	u32 param_id;
	u32 max;
	char params[0];
} __packed;

struct skl_gain_config {
	u32 channel_id;
	u32 target_volume;
	u32 ramp_type;
	u64 ramp_duration;
} __packed;

enum skl_tkn_dir {
	SKL_DIR_IN,
	SKL_DIR_OUT
};

enum skl_tuple_type {
	SKL_TYPE_TUPLE,
	SKL_TYPE_DATA
};

#endif
