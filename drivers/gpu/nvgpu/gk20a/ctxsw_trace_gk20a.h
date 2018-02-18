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

#ifndef __CTXSW_TRACE_GK20A_H
#define __CTXSW_TRACE_GK20A_H

#define GK20A_CTXSW_TRACE_NUM_DEVS			1

struct gk20a;
struct gpu_ops;
struct nvgpu_ctxsw_trace_entry;
struct channel_gk20a;
struct channel_ctx_gk20a;
struct gk20a_ctxsw_dev;
struct gk20a_fecs_trace;
struct tsg_gk20a;


int gk20a_ctxsw_dev_release(struct inode *inode, struct file *filp);
int gk20a_ctxsw_dev_open(struct inode *inode, struct file *filp);
long gk20a_ctxsw_dev_ioctl(struct file *filp,
			 unsigned int cmd, unsigned long arg);
ssize_t gk20a_ctxsw_dev_read(struct file *, char __user *, size_t, loff_t *);
unsigned int gk20a_ctxsw_dev_poll(struct file *, struct poll_table_struct *);
int gk20a_ctxsw_dev_mmap(struct file *, struct vm_area_struct *);

int gk20a_ctxsw_trace_init(struct gk20a *);
void gk20a_ctxsw_trace_cleanup(struct gk20a *);
int gk20a_ctxsw_trace_write(struct gk20a *, struct nvgpu_ctxsw_trace_entry *);
void gk20a_ctxsw_trace_wake_up(struct gk20a *g, int vmid);
void gk20a_ctxsw_trace_init_ops(struct gpu_ops *ops);

void gk20a_ctxsw_trace_channel_reset(struct gk20a *g, struct channel_gk20a *ch);
void gk20a_ctxsw_trace_tsg_reset(struct gk20a *g, struct tsg_gk20a *tsg);


#endif /* __CTXSW_TRACE_GK20A_H */
