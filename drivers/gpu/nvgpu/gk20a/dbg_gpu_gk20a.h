/*
 * Tegra GK20A GPU Debugger Driver
 *
 * Copyright (c) 2013-2017, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef DBG_GPU_GK20A_H
#define DBG_GPU_GK20A_H
#include <linux/poll.h>

/* module debug driver interface */
int gk20a_dbg_gpu_dev_release(struct inode *inode, struct file *filp);
int gk20a_dbg_gpu_dev_open(struct inode *inode, struct file *filp);
long gk20a_dbg_gpu_dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
unsigned int gk20a_dbg_gpu_dev_poll(struct file *filep, poll_table *wait);

/* used by profiler driver interface */
int gk20a_prof_gpu_dev_open(struct inode *inode, struct file *filp);

/* used by the interrupt handler to post events */
void gk20a_dbg_gpu_post_events(struct channel_gk20a *fault_ch);

struct channel_gk20a *
nvgpu_dbg_gpu_get_session_channel(struct dbg_session_gk20a *dbg_s);

struct dbg_gpu_session_ops {
	int (*exec_reg_ops)(struct dbg_session_gk20a *dbg_s,
			    struct nvgpu_dbg_gpu_reg_op *ops,
			    u64 num_ops);
	int (*dbg_set_powergate)(struct dbg_session_gk20a *dbg_s, u32 mode);
};

struct dbg_gpu_session_events {
	wait_queue_head_t wait_queue;
	bool events_enabled;
	int num_pending_events;
};

struct dbg_session_gk20a {
	/* dbg session id used for trace/prints */
	int id;

	/* profiler session, if any */
	bool is_profiler;

	/* power enabled or disabled */
	bool is_pg_disabled;

	/* timeouts enabled or disabled */
	bool is_timeout_disabled;

	/*
	 * There can be different versions of the whitelists
	 * between both global and per-context sets; as well
	 * as between debugger and profiler interfaces.
	 */
	struct regops_whitelist *global;
	struct regops_whitelist *per_context;

	/* gpu module vagaries */
	struct device             *dev;
	struct gk20a              *g;

	/* list of bound channels, if any */
	struct list_head ch_list;
	struct mutex ch_list_lock;

	/* session operations */
	struct dbg_gpu_session_ops *ops;

	/* event support */
	struct dbg_gpu_session_events dbg_events;

	bool broadcast_stop_trigger;

	struct mutex ioctl_lock;
};

struct dbg_session_data {
	struct dbg_session_gk20a *dbg_s;
	struct list_head dbg_s_entry;
};

struct dbg_session_channel_data {
	struct file          *ch_f;
	int channel_fd;
	int chid;
	struct list_head ch_entry;
	struct dbg_session_data *session_data;
};

extern struct dbg_gpu_session_ops dbg_gpu_session_ops_gk20a;

int dbg_unbind_single_channel_gk20a(struct dbg_session_gk20a *dbg_s,
			struct dbg_session_channel_data *ch_data);

bool gk20a_dbg_gpu_broadcast_stop_trigger(struct channel_gk20a *ch);
int gk20a_dbg_gpu_clear_broadcast_stop_trigger(struct channel_gk20a *ch);

#endif /* DBG_GPU_GK20A_H */
