/*
 * Copyright (C) 2018 Tesla, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __TRIP_H__
#define __TRIP_H__

#include <linux/platform_device.h>
#include <linux/devfreq.h>

/* Per-network instance data */
struct tripdev;
struct trip_net;

enum trip_req_state {
	TRIP_REQ_NEW = 0,
	TRIP_REQ_PENDING = 1,
	TRIP_REQ_DONE = 2,
};

struct trip_req;
typedef void (trip_req_cb)(struct trip_req *req, void *cb_data);

struct trip_req {
	/* Used internal to trip DMA implementation */
	struct list_head list;
	struct trip_net *tn;
	enum trip_req_state state;

	/* Initialized by callers of trip_submit_req */
	u32 sram_addr;
	dma_addr_t data_base;
	dma_addr_t weight_base;
	dma_addr_t output_base;

	/* Also initialized by callers, callback optional */
	struct completion completion;
	trip_req_cb *callback;
	void *cb_data;

	/* Updated upon completion */
	u32 nw_status;
};

struct trip_net {
	char name[32];
	struct tripdev *ptd;
	int idx;
	int done_irq;

	spinlock_t net_lock;
	struct list_head pending_reqs;
	u64 completions;

	wait_queue_head_t idle_wq;
};

enum trip_post_status {
	TRIP_POST_STATUS_PENDING = 0,
	TRIP_POST_STATUS_OK,
	TRIP_POST_STATUS_INIT_SRAM,
	TRIP_POST_STATUS_INIT_SRAM_FAILED,
	TRIP_POST_STATUS_RUN_NOPARITY,
	TRIP_POST_STATUS_RUN_NOPARITY_FAILED,
	TRIP_POST_STATUS_NOPARITY_SRAM_MISCOMPARE,
	TRIP_POST_STATUS_RUN_PARITY,
	TRIP_POST_STATUS_RUN_PARITY_FAILED,
	TRIP_POST_STATUS_PARITY_SRAM_MISCOMPARE,
	TRIP_POST_STATUS_INIT_SRAM_FINAL,
	TRIP_POST_STATUS_INIT_SRAM_FINAL_FAILED,
	TRIP_POST_STATUS_WAITING_ROOT,
	TRIP_POST_STATUS_LOADING_FIRMWARE,
	TRIP_POST_STATUS_LOAD_FIRMWARE_FAILED,
};

struct tripdev {
	void __iomem *td_regs;
	void __iomem *td_sram;
	struct clk *trip_pll;
	struct clk *trip_pll_clk;
	struct clk *trip_switch_mux_clk;
	struct clk *trip_switch_pll_mux_clk;
	unsigned long clk_rate;
	struct opp_table *opp_table;
	struct devfreq *devfreq;
	struct devfreq_dev_profile devfreq_profile;
	struct thermal_cooling_device *devfreq_cooling;

	u64 sram_len;
	u32 hw_ver;
	char name[32];
	int id;
	struct platform_device *pdev;
	struct cdev cdev;

	bool init_done;
	enum trip_post_status post_status;
	struct work_struct init_work;
	struct work_struct fw_work;
	struct trip_fw_info fw_info;

	struct trip_net net[TRIP_NET_MAX];

	/* Allows serializing userspace submission of work items */
	struct mutex submit_mutex;

	/* If true, use following queue instead of per-net queues */
	bool serialized;
	spinlock_t serial_lock;
	/* trip_req entries which have been submitted to hw */
	struct list_head started_reqs;
	/* trip_req entries not yet submitted to hw */
	struct list_head waiting_reqs;
	wait_queue_head_t idle_wq;

	int generic_irq;
	int error_irq;
};

#endif /* __TRIP_H__ */
