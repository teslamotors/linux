/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __SCHED_GK20A_H
#define __SCHED_GK20A_H

struct gk20a;
struct gpu_ops;
struct tsg_gk20a;

struct gk20a_sched_ctrl {
	struct gk20a *g;

	struct mutex control_lock;
	bool control_locked;
	bool sw_ready;
	struct mutex status_lock;
	struct mutex busy_lock;

	u64 status;

	size_t bitmap_size;
	u64 *active_tsg_bitmap;
	u64 *recent_tsg_bitmap;
	u64 *ref_tsg_bitmap;

	wait_queue_head_t readout_wq;
};

int gk20a_sched_dev_release(struct inode *inode, struct file *filp);
int gk20a_sched_dev_open(struct inode *inode, struct file *filp);
long gk20a_sched_dev_ioctl(struct file *, unsigned int, unsigned long);
ssize_t gk20a_sched_dev_read(struct file *, char __user *, size_t, loff_t *);
unsigned int gk20a_sched_dev_poll(struct file *, struct poll_table_struct *);

void gk20a_sched_ctrl_tsg_added(struct gk20a *, struct tsg_gk20a *);
void gk20a_sched_ctrl_tsg_removed(struct gk20a *, struct tsg_gk20a *);
int gk20a_sched_ctrl_init(struct gk20a *);

void gk20a_sched_debugfs_init(struct device *dev);
void gk20a_sched_ctrl_cleanup(struct gk20a *g);

#endif /* __SCHED_GK20A_H */
