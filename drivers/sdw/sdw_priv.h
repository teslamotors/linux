/*
 *  sdw_priv.h - Private definition for sdw bus interface.
 *
 *  Copyright (C) 2014-2015 Intel Corp
 *  Author:  Hardik Shah  <hardik.t.shah@intel.com>
 *
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

#ifndef _LINUX_SDW_PRIV_H
#define _LINUX_SDW_PRIV_H

#include <linux/kthread.h>	/* For kthread */
#include <linux/spinlock.h>

#define SDW_MAX_STREAM_TAG_KEY_SIZE	80
#define SDW_NUM_STREAM_TAGS		100
#define MAX_NUM_ROWS			23 /* As per SDW Spec */
#define MAX_NUM_COLS			8/* As per SDW Spec */
#define MAX_NUM_ROW_COLS		(MAX_NUM_ROWS * MAX_NUM_COLS)

#define SDW_STATE_INIT_STREAM_TAG	    0x1
#define SDW_STATE_ALLOC_STREAM              0x2
#define SDW_STATE_CONFIG_STREAM             0x3
#define SDW_STATE_PREPARE_STREAM            0x4
#define SDW_STATE_ENABLE_STREAM             0x5
#define SDW_STATE_DISABLE_STREAM            0x6
#define SDW_STATE_UNPREPARE_STREAM          0x7
#define SDW_STATE_RELEASE_STREAM            0x8
#define SDW_STATE_FREE_STREAM               0x9
#define SDW_STATE_FREE_STREAM_TAG           0xA
#define SDW_STATE_ONLY_XPORT_STREAM	    0xB

#define SDW_STATE_INIT_RT		0x1
#define SDW_STATE_CONFIG_RT		0x2
#define SDW_STATE_PREPARE_RT		0x3
#define SDW_STATE_ENABLE_RT		0x4
#define SDW_STATE_DISABLE_RT		0x5
#define SDW_STATE_UNPREPARE_RT		0x6
#define SDW_STATE_RELEASE_RT		0x7

#define SDW_SLAVE_BDCAST_ADDR		15

struct sdw_runtime;
/* Defined in sdw.c, used by multiple files of module */
extern struct sdw_core sdw_core;

enum sdw_port_state {
	SDW_PORT_STATE_CH_READY,
	SDW_PORT_STATE_CH_STOPPED,
	SDW_PORT_STATE_CH_PREPARING,
	SDW_PORT_STATE_CH_DEPREPARING,
};

enum sdw_stream_state {
	SDW_STREAM_ALLOCATED,
	SDW_STREAM_FREE,
	SDW_STREAM_ACTIVE,
	SDW_STREAM_INACTIVE,
};

enum sdw_clk_state {
	SDW_CLK_STATE_OFF = 0,
	SDW_CLK_STATE_ON = 1,
};

enum sdw_update_bs_state {
	SDW_UPDATE_BS_PRE,
	SDW_UPDATE_BS_BNKSWTCH,
	SDW_UPDATE_BS_POST,
	SDW_UPDATE_BS_BNKSWTCH_WAIT,
	SDW_UPDATE_BS_DIS_CHN,
};

enum sdw_port_en_state {
	SDW_PORT_STATE_PREPARE,
	SDW_PORT_STATE_ENABLE,
	SDW_PORT_STATE_DISABLE,
	SDW_PORT_STATE_UNPREPARE,
};

struct port_chn_en_state {
	bool is_activate;
	bool is_bank_sw;
};

struct temp_elements {
	int rate;
	int full_bw;
	int payload_bw;
	int hwidth;
};

struct sdw_stream_tag {
	int stream_tag;
	struct mutex stream_lock;
	int ref_count;
	enum sdw_stream_state stream_state;
	char key[SDW_MAX_STREAM_TAG_KEY_SIZE];
	struct sdw_runtime *sdw_rt;
};

struct sdw_stream_params {
	unsigned int rate;
	unsigned int channel_count;
	unsigned int bps;
};

struct sdw_port_runtime {
	int port_num;
	enum sdw_port_state port_state;
	int channel_mask;
	/* Frame params and stream params are per port based
	 * Single stream of audio may be split
	 * into mutliple port each handling
	 * subset of channels, channels should
	 * be contiguous in subset
	 */
	struct sdw_transport_params transport_params;
	struct sdw_port_params port_params;
	struct list_head port_node;
};

struct sdw_slave_runtime {
	/* Simplified port or full port, there cannot be both types of
	 * data port for single stream, so data structure is kept per
	 * slave runtime, not per port
	 */
	enum sdw_dpn_type type;
	struct sdw_slave *slave;
	int direction;
	/* Stream may be split into multiple slaves, so this is for
	 * this particular slave
	 */
	struct sdw_stream_params stream_params;
	struct list_head port_rt_list;
	struct list_head slave_sdw_node;
	struct list_head slave_node;
	int rt_state; /* State of runtime structure */

};


struct sdw_mstr_runtime {
	struct sdw_master *mstr;
	int direction;
	/* Stream may be split between  multiple masters so this
	 * is for invidual master, if stream is split into multiple
	 * streams. For calculating the bandwidth on the particular bus
	 * stream params of master is taken into account.
	 */
	struct sdw_stream_params stream_params;
	struct list_head port_rt_list;
	/* Two nodes are required because BW calculation is based on master
	 * while stream enabling is based on stream_tag, where multiple
	 * masters may be involved
	 */
	struct list_head mstr_sdw_node; /* This is to add mstr_rt in sdw_rt */
	struct list_head mstr_node; /* This is to add mstr_rt in mstr */

	struct list_head slv_rt_list;
	 /* Individual stream bandwidth on given master */
	unsigned int	stream_bw;
	 /* State of runtime structure */
	int rt_state;
	int hstart;
	int hstop;
	int block_offset;
	int sub_block_offset;
};

struct sdw_runtime {
	int tx_ref_count;
	int rx_ref_count;
	/* This is stream params for whole stream
	 * but stream may be split between two
	 * masters, or two slaves.
	 */
	struct sdw_stream_params stream_params;
	struct list_head slv_rt_list;
	struct list_head mstr_rt_list;
	enum sdw_stream_type	type;
	int stream_state;
	int xport_state;

};

struct sdw_slv_status {
	struct list_head node;
	enum sdw_slave_status status[SOUNDWIRE_MAX_DEVICES];
};

/** Bus structure which handles bus related information */
struct sdw_bus {
	struct list_head bus_node;
	struct sdw_master *mstr;
	unsigned int	port_grp_mask[2];
	unsigned int	slave_grp_mask[2];
	unsigned int	clk_state;
	unsigned int	active_bank;
	unsigned int	clk_freq;
	unsigned int	clk_div;
	/* Bus total Bandwidth. Initialize and reset to zero */
	unsigned int	bandwidth;
	unsigned int	stream_interval; /* Stream Interval */
	unsigned int	system_interval; /* Bus System Interval */
	unsigned int	frame_freq;
	unsigned int	col;
	unsigned int	row;
	struct task_struct *status_thread;
	struct kthread_worker kworker;
	struct kthread_work kwork;
	struct list_head status_list;
	spinlock_t spinlock;
	struct sdw_async_xfer_data async_data;
};

/** Holds supported Row-Column combination related information */
struct sdw_rowcol {
	int     row;
	int     col;
	int     control_bits;
	int data_bits;
};

/**
 * Global soundwire structure. It handles all the streams spawned
 * across masters and has list of bus structure per every master
 * registered
 */
struct sdw_core {
	struct sdw_stream_tag stream_tags[SDW_NUM_STREAM_TAGS];
	struct sdw_rowcol     rowcolcomb[MAX_NUM_ROW_COLS];
	struct list_head bus_list;
	struct mutex core_lock;
	struct idr idr;
	int first_dynamic_bus_num;
};

/* Structure holding mapping of numbers to cols */
struct sdw_num_to_col {
	int num;
	int col;
};

/* Structure holding mapping of numbers to rows */
struct sdw_num_to_row {
	int num;
	int row;
};

int sdw_slave_port_config_port_params(struct sdw_slave_runtime *slv_rt);
int sdw_slave_port_prepare(struct sdw_slave_runtime, bool prepare);
int sdw_bus_bw_init(void);
int sdw_mstr_bw_init(struct sdw_bus *sdw_bs);
int sdw_bus_calc_bw(struct sdw_stream_tag *stream_tag, bool enable);
int sdw_bus_calc_bw_dis(struct sdw_stream_tag *stream_tag, bool unprepare);
int sdw_bus_bra_xport_config(struct sdw_bus *sdw_mstr_bs,
	struct sdw_bra_block *block, bool enable);
int sdw_chn_enable(void);
void sdw_unlock_mstr(struct sdw_master *mstr);
int sdw_trylock_mstr(struct sdw_master *mstr);
void sdw_lock_mstr(struct sdw_master *mstr);
int sdw_slave_transfer_async(struct sdw_master *mstr, struct sdw_msg *msg,
				int num,
				struct sdw_async_xfer_data *async_data);

#endif /* _LINUX_SDW_PRIV_H */
