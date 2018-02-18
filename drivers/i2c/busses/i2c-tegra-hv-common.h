/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef __I2C_LIB_HV_H__
#define __I2C_LIB_HV_H__

#ifdef CONFIG_TEGRA_HV_MANAGER
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/tegra-ivc.h>
#include <linux/list.h>
#include <linux/workqueue.h>

typedef void (*i2c_isr_handler)(void *context);

struct tegra_hv_i2c_comm_chan;

int hv_i2c_transfer(struct tegra_hv_i2c_comm_chan *comm_chan, phys_addr_t base,
		int addr, int read, uint8_t *buf, size_t len, int *err,
		int seq_no, uint32_t flags);
int hv_i2c_get_max_payload(struct tegra_hv_i2c_comm_chan *comm_chan,
		phys_addr_t base, uint32_t *max_payload, int *err);
int hv_i2c_comm_chan_cleanup(struct tegra_hv_i2c_comm_chan *comm_chan,
		phys_addr_t base);
void hv_i2c_comm_chan_free(struct tegra_hv_i2c_comm_chan *comm_chan);
void *hv_i2c_comm_init(struct device *dev, i2c_isr_handler handler,
		void *data);
void tegra_hv_i2c_poll_cleanup(struct tegra_hv_i2c_comm_chan *comm_chan);

#define MAX_COMM_CHANS  7

enum i2c_ivc_msg_t {
	I2C_READ,
	I2C_READ_RESPONSE,
	I2C_WRITE,
	I2C_WRITE_RESPONSE,
	I2C_GET_MAX_PAYLOAD,
	I2C_GET_MAX_PAYLOAD_RESPONSE,
	I2C_CLEANUP,
	I2C_CLEANUP_RESPONSE,
	I2C_INVALID,
};

enum i2c_rx_state_t {
	I2C_RX_INIT,
	I2C_RX_PENDING,
	I2C_RX_PENDING_CLEANUP,
};

#define HV_I2C_FLAGS_HIGHSPEED_MODE	(1<<22)
#define HV_I2C_FLAGS_CONT_ON_NAK	(1<<21)
#define HV_I2C_FLAGS_SEND_START_BYTE	(1<<20)
#define HV_I2C_FLAGS_10BIT_ADDR		(1<<18)
#define HV_I2C_FLAGS_IE_ENABLE		(1<<17)
#define HV_I2C_FLAGS_REPEAT_START	(1<<16)
#define HV_I2C_FLAGS_CONTINUE_XFER	(1<<15)

struct i2c_ivc_msg_common {
	uint32_t s_marker;
	uint32_t msg_type;
	int32_t comm_chan_id;
	uint32_t controller_instance;
	uint32_t err;
	uint32_t e_marker;
};

struct i2c_ivc_msg_tx_rx_hdr {
	int32_t seq_no;
	uint32_t slave_address;
	uint32_t buf_len;
	uint32_t flags;
};

struct i2c_ivc_msg_tx_rx {
	struct i2c_ivc_msg_tx_rx_hdr fixed;
	uint8_t buffer[4096];
};

struct i2c_ivc_msg_max_payload {
	uint32_t max_payload;
};

struct i2c_ivc_msg {
	struct i2c_ivc_msg_common hdr;
	union {
		struct i2c_ivc_msg_tx_rx m;
		struct i2c_ivc_msg_max_payload p;
	} body;
};

#define I2C_IVC_COMMON_HEADER_LEN	sizeof(struct i2c_ivc_msg_common)

#define i2c_ivc_start_marker(_msg_ptr)	(_msg_ptr->hdr.s_marker)
#define i2c_ivc_end_marker(_msg_ptr)	(_msg_ptr->hdr.e_marker)
#define i2c_ivc_chan_id(_msg_ptr)	(_msg_ptr->hdr.comm_chan_id)
#define i2c_ivc_controller_instance(_msg_ptr)	\
					(_msg_ptr->hdr.controller_instance)
#define i2c_ivc_msg_type(_msg_ptr)	(_msg_ptr->hdr.msg_type)
#define i2c_ivc_error_field(_msg_ptr)	(_msg_ptr->hdr.err)

#define i2c_ivc_message_seq_nr(_msg_ptr)	\
					(_msg_ptr->body.m.fixed.seq_no)
#define i2c_ivc_message_slave_addr(_msg_ptr)	\
					(_msg_ptr->body.m.fixed.slave_address)
#define i2c_ivc_message_buf_len(_msg_ptr)	\
					(_msg_ptr->body.m.fixed.buf_len)
#define i2c_ivc_message_flags(_msg_ptr)	(_msg_ptr->body.m.fixed.flags)
#define i2c_ivc_message_buffer(_msg_ptr)	\
					(_msg_ptr->body.m.buffer[0])

#define i2c_ivc_max_payload_field(_msg_ptr)	\
					(_msg_ptr->body.p.max_payload)

struct tegra_hv_i2c_comm_dev;

/* channel is virtual abstraction over a single ivc queue
 * each channel holds messages that are independent of other channels
 * we allocate one channel per i2c adapter as these can operate in parallel
 */
struct tegra_hv_i2c_comm_chan {
	struct tegra_hv_ivc_cookie *ivck;
	struct device *dev;
	int id;
	i2c_isr_handler handler;
	void *data;
	void *rcvd_data;
	size_t data_len;
	int *rcvd_err;
	enum i2c_rx_state_t rx_state;
	struct tegra_hv_i2c_comm_dev *hv_comm_dev;
	spinlock_t lock;
};


struct tegra_hv_i2c_comm_dev {
	uint32_t queue_id;
	struct tegra_hv_ivc_cookie *ivck;
	spinlock_t ivck_tx_lock;
	spinlock_t lock;
	struct hlist_node list;
	struct work_struct work;
	struct tegra_hv_i2c_comm_chan *hv_comm_chan[MAX_COMM_CHANS];
};
#endif
#endif
