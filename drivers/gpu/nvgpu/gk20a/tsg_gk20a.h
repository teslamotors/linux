/*
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef __TSG_GK20A_H_
#define __TSG_GK20A_H_

#define NVGPU_INVALID_TSG_ID (-1)

bool gk20a_is_channel_marked_as_tsg(struct channel_gk20a *ch);


int gk20a_tsg_dev_release(struct inode *inode, struct file *filp);
int gk20a_tsg_dev_open(struct inode *inode, struct file *filp);
void gk20a_tsg_release(struct kref *ref);
int gk20a_tsg_open(struct gk20a *g, struct file *filp);
long gk20a_tsg_dev_ioctl(struct file *filp,
			 unsigned int cmd, unsigned long arg);

int gk20a_init_tsg_support(struct gk20a *g, u32 tsgid);
void gk20a_init_tsg_ops(struct gpu_ops *gops);

struct tsg_gk20a {
	struct gk20a *g;

	bool in_use;
	int tsgid;

	struct kref refcount;

	struct list_head ch_list;
	int num_active_channels;
	struct mutex ch_list_lock;

	int timeslice_us;
	int timeslice_timeout;
	int timeslice_scale;

	struct gr_ctx_desc *tsg_gr_ctx;

	struct vm_gk20a *vm;

	u32 interleave_level;

	struct list_head event_id_list;
	struct mutex event_id_list_lock;

	u32 runlist_id;
	pid_t tgid;
};

int gk20a_enable_tsg(struct tsg_gk20a *tsg);
int gk20a_disable_tsg(struct tsg_gk20a *tsg);
int gk20a_tsg_bind_channel(struct tsg_gk20a *tsg,
			struct channel_gk20a *ch);
int gk20a_tsg_unbind_channel(struct channel_gk20a *ch);

void gk20a_tsg_event_id_post_event(struct tsg_gk20a *tsg,
				       int event_id);
int gk20a_tsg_set_runlist_interleave(struct tsg_gk20a *tsg, u32 level);
int gk20a_tsg_set_timeslice(struct tsg_gk20a *tsg, u32 timeslice);


#endif /* __TSG_GK20A_H_ */
