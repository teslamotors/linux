/*
 * drivers/video/tegra/host/vi/vi_notify.h
 *
 * Tegra Graphics Host VI
 *
 * Copyright (c) 2015-2017 NVIDIA Corporation.  All rights reserved.
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

#ifndef __TEGRA_VI_NOTIFY_H__
#define __TEGRA_VI_NOTIFY_H__

#include <linux/kfifo.h>

#define VI_NOTIFY_MAX_VI_CHANS (12)
/* make sure that status entries always power of 2 */
#define VI_NOTIFY_STATUS_ENTRIES (1 << 7)

struct vi_notify_msg {
	u32 tag;  /* LSB=1 */
	u32 stamp;
	u32 data;
	u32 reserve;
};

#define VI_NOTIFY_TAG_VALID(tag)	((tag) & 1)
#define VI_NOTIFY_TAG_TAG(tag)		(((tag) >> 1) & 0x7f)
#define VI_NOTIFY_TAG_CHANNEL(tag)	(((tag) >> 8) & 0xff)
#define VI_NOTIFY_TAG_FRAME(tag)	(((tag) >> 16) & 0xffff)

/* NOTE: vi_capture_status structure should match with
 * the one declared in RTCPU vi-notifier FW driver.
 */
struct vi_capture_status {
	u8 st;
	u8 vc;
	u16 frame;
	u32 status;
	u64 sof_ts;
	u64 eof_ts;
	u32 data;
	u32 capture_id;
};

struct vi_notify_dev;

struct vi_notify_driver {
	struct module *owner;
	int (*probe)(struct device *, struct vi_notify_dev *);
	void (*remove)(struct device *);
	int (*classify)(struct device *, u32);
	int (*set_syncpts)(struct device *, u8, const u32[3]);
	int (*enable_reports)(struct device *dev, u8 ch,
			u8 st, u8 vc, const u32 ids[3]);
	void (*reset_channel)(struct device *, u8);
	bool (*has_notifier_backend)(struct device *);
	int (*get_capture_status)(struct device *, unsigned,
			u64, struct vi_capture_status *);
};

void vi_notify_dev_error(struct vi_notify_dev *);
void vi_notify_dev_report(struct vi_notify_dev *, u8,
			const struct vi_capture_status *);
void vi_notify_dev_recv(struct vi_notify_dev *, const struct vi_notify_msg *);

int vi_notify_register(struct vi_notify_driver *, struct device *, u8);
void vi_notify_unregister(struct vi_notify_driver *, struct device *);

struct vi_notify_channel;
typedef void (*vi_notify_status_callback)(struct vi_notify_channel *,
				const struct vi_capture_status *, void *);
typedef void (*vi_notify_error_callback)(void *);

struct tegra_vi4_syncpts_req {
	u32 syncpt_ids[3];
	u8 stream;
	u8 vc;
	u16 pad;
};

struct vi_notify_channel {
	struct vi_notify_dev *vnd;
	atomic_t ign_mask;

	wait_queue_head_t readq;
	struct mutex read_lock;
	struct rcu_head rcu;

	atomic_t overruns;
	atomic_t errors;
	atomic_t report;
	DECLARE_KFIFO(fifo, struct vi_notify_msg, 128);
	struct vi_capture_status status;

	vi_notify_status_callback notify_cb;
	vi_notify_error_callback error_cb;

	void *client_data;
};

/* internal vi_notify_channel API for kernel vi-mode driver */
int vi_notify_channel_set_ign_mask(struct vi_notify_channel *, u32);
int vi_notify_channel_set_syncpts(unsigned,
			struct vi_notify_channel *,
			struct tegra_vi4_syncpts_req *);
int vi_notify_channel_enable_reports(unsigned channel,
			struct vi_notify_channel *,
			struct tegra_vi4_syncpts_req *);
int vi_notify_channel_reset(unsigned channel,
			struct vi_notify_channel *,
			struct tegra_vi4_syncpts_req *);
struct vi_notify_channel *vi_notify_channel_open(unsigned channel);
int vi_notify_channel_close(unsigned, struct vi_notify_channel *);
void vi_notify_channel_set_notify_funcs(struct vi_notify_channel *,
			vi_notify_status_callback,
			vi_notify_error_callback,
			void *);
int vi_notify_get_capture_status(struct vi_notify_channel *,
			unsigned,
			u64,
			struct vi_capture_status *);

#endif
