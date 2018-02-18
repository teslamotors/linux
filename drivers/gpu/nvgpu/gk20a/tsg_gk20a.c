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

#include <linux/fs.h>
#include <linux/file.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/nvhost.h>
#include <uapi/linux/nvgpu.h>
#include <linux/anon_inodes.h>

#include "gk20a.h"
#include "hw_ccsr_gk20a.h"

#define NVGPU_TSG_MIN_TIMESLICE_US 1000
#define NVGPU_TSG_MAX_TIMESLICE_US 50000

struct tsg_private {
	struct gk20a *g;
	struct tsg_gk20a *tsg;
};

bool gk20a_is_channel_marked_as_tsg(struct channel_gk20a *ch)
{
	return !(ch->tsgid == NVGPU_INVALID_TSG_ID);
}

int gk20a_enable_tsg(struct tsg_gk20a *tsg)
{
	struct gk20a *g = tsg->g;
	struct channel_gk20a *ch;

	mutex_lock(&tsg->ch_list_lock);
	list_for_each_entry(ch, &tsg->ch_list, ch_entry) {
		g->ops.fifo.enable_channel(ch);
	}
	mutex_unlock(&tsg->ch_list_lock);

	return 0;
}

int gk20a_disable_tsg(struct tsg_gk20a *tsg)
{
	struct gk20a *g = tsg->g;
	struct channel_gk20a *ch;

	mutex_lock(&tsg->ch_list_lock);
	list_for_each_entry(ch, &tsg->ch_list, ch_entry) {
		g->ops.fifo.disable_channel(ch);
	}
	mutex_unlock(&tsg->ch_list_lock);

	return 0;
}

static bool gk20a_is_channel_active(struct gk20a *g, struct channel_gk20a *ch)
{
	struct fifo_gk20a *f = &g->fifo;
	struct fifo_runlist_info_gk20a *runlist;
	int i;

	for (i = 0; i < f->max_runlists; ++i) {
		runlist = &f->runlist_info[i];
		if (test_bit(ch->hw_chid, runlist->active_channels))
			return true;
	}

	return false;
}

static int gk20a_tsg_bind_channel_fd(struct tsg_gk20a *tsg, int ch_fd)
{
	struct channel_gk20a *ch;
	int err;

	ch = gk20a_get_channel_from_file(ch_fd);
	if (!ch)
		return -EINVAL;

	err = ch->g->ops.fifo.tsg_bind_channel(tsg, ch);
	return err;
}

/*
 * API to mark channel as part of TSG
 *
 * Note that channel is not runnable when we bind it to TSG
 */
int gk20a_tsg_bind_channel(struct tsg_gk20a *tsg,
			struct channel_gk20a *ch)
{
	gk20a_dbg_fn("");

	/* check if channel is already bound to some TSG */
	if (gk20a_is_channel_marked_as_tsg(ch)) {
		return -EINVAL;
	}

	/* channel cannot be bound to TSG if it is already active */
	if (gk20a_is_channel_active(tsg->g, ch)) {
		return -EINVAL;
	}

	ch->tsgid = tsg->tsgid;

	/* all the channel part of TSG should need to be same runlist_id */
	if (tsg->runlist_id == ~0)
		tsg->runlist_id = ch->runlist_id;
	else if (tsg->runlist_id != ch->runlist_id) {
		gk20a_err(dev_from_gk20a(tsg->g),
			"Error: TSG channel should be share same runlist ch[%d] tsg[%d]\n",
			ch->runlist_id, tsg->runlist_id);
		return -EINVAL;
	}

	mutex_lock(&tsg->ch_list_lock);
	list_add_tail(&ch->ch_entry, &tsg->ch_list);
	mutex_unlock(&tsg->ch_list_lock);

	kref_get(&tsg->refcount);

	gk20a_dbg(gpu_dbg_fn, "BIND tsg:%d channel:%d\n",
					tsg->tsgid, ch->hw_chid);

	gk20a_dbg_fn("done");
	return 0;
}

int gk20a_tsg_unbind_channel(struct channel_gk20a *ch)
{
	struct fifo_gk20a *f = &ch->g->fifo;
	struct tsg_gk20a *tsg = &f->tsg[ch->tsgid];

	mutex_lock(&tsg->ch_list_lock);
	list_del_init(&ch->ch_entry);
	mutex_unlock(&tsg->ch_list_lock);

	kref_put(&tsg->refcount, gk20a_tsg_release);

	ch->tsgid = NVGPU_INVALID_TSG_ID;

	return 0;
}

int gk20a_init_tsg_support(struct gk20a *g, u32 tsgid)
{
	struct tsg_gk20a *tsg = NULL;

	if (tsgid < 0 || tsgid >= g->fifo.num_channels)
		return -EINVAL;

	tsg = &g->fifo.tsg[tsgid];

	tsg->in_use = false;
	tsg->tsgid = tsgid;

	INIT_LIST_HEAD(&tsg->ch_list);
	mutex_init(&tsg->ch_list_lock);

	INIT_LIST_HEAD(&tsg->event_id_list);
	mutex_init(&tsg->event_id_list_lock);

	return 0;
}

static int gk20a_tsg_set_priority(struct gk20a *g, struct tsg_gk20a *tsg,
				u32 priority)
{
	switch (priority) {
	case NVGPU_PRIORITY_LOW:
		tsg->timeslice_us = g->timeslice_low_priority_us;
		break;
	case NVGPU_PRIORITY_MEDIUM:
		tsg->timeslice_us = g->timeslice_medium_priority_us;
		break;
	case NVGPU_PRIORITY_HIGH:
		tsg->timeslice_us = g->timeslice_high_priority_us;
		break;
	default:
		pr_err("Unsupported priority");
		return -EINVAL;
	}

	gk20a_channel_get_timescale_from_timeslice(g, tsg->timeslice_us,
			&tsg->timeslice_timeout, &tsg->timeslice_scale);

	g->ops.fifo.update_runlist(g, tsg->runlist_id, ~0, true, true);

	return 0;
}

static int gk20a_tsg_get_event_data_from_id(struct tsg_gk20a *tsg,
				int event_id,
				struct gk20a_event_id_data **event_id_data)
{
	struct gk20a_event_id_data *local_event_id_data;
	bool event_found = false;

	mutex_lock(&tsg->event_id_list_lock);
	list_for_each_entry(local_event_id_data, &tsg->event_id_list,
						 event_id_node) {
		if (local_event_id_data->event_id == event_id) {
			event_found = true;
			break;
		}
	}
	mutex_unlock(&tsg->event_id_list_lock);

	if (event_found) {
		*event_id_data = local_event_id_data;
		return 0;
	} else {
		return -1;
	}
}

void gk20a_tsg_event_id_post_event(struct tsg_gk20a *tsg,
				       int event_id)
{
	struct gk20a_event_id_data *event_id_data;
	int err = 0;

	err = gk20a_tsg_get_event_data_from_id(tsg, event_id,
						&event_id_data);
	if (err)
		return;

	mutex_lock(&event_id_data->lock);

	gk20a_dbg_info(
		"posting event for event_id=%d on tsg=%d\n",
		event_id, tsg->tsgid);
	event_id_data->event_posted = true;

	wake_up_interruptible_all(&event_id_data->event_id_wq);

	mutex_unlock(&event_id_data->lock);
}

static int gk20a_tsg_event_id_enable(struct tsg_gk20a *tsg,
					 int event_id,
					 int *fd)
{
	int err = 0;
	int local_fd;
	struct file *file;
	char *name;
	struct gk20a_event_id_data *event_id_data;

	err = gk20a_tsg_get_event_data_from_id(tsg,
				event_id, &event_id_data);
	if (err == 0) /* We already have event enabled */
		return -EINVAL;

	err = get_unused_fd_flags(O_RDWR);
	if (err < 0)
		return err;
	local_fd = err;

	name = kasprintf(GFP_KERNEL, "nvgpu-event%d-fd%d",
			event_id, local_fd);

	file = anon_inode_getfile(name, &gk20a_event_id_ops,
				  NULL, O_RDWR);
	kfree(name);
	if (IS_ERR(file)) {
		err = PTR_ERR(file);
		goto clean_up;
	}

	event_id_data = kzalloc(sizeof(*event_id_data), GFP_KERNEL);
	if (!event_id_data) {
		err = -ENOMEM;
		goto clean_up_file;
	}
	event_id_data->g = tsg->g;
	event_id_data->id = tsg->tsgid;
	event_id_data->is_tsg = true;
	event_id_data->event_id = event_id;

	init_waitqueue_head(&event_id_data->event_id_wq);
	mutex_init(&event_id_data->lock);
	INIT_LIST_HEAD(&event_id_data->event_id_node);

	mutex_lock(&tsg->event_id_list_lock);
	list_add_tail(&event_id_data->event_id_node, &tsg->event_id_list);
	mutex_unlock(&tsg->event_id_list_lock);

	fd_install(local_fd, file);
	file->private_data = event_id_data;

	*fd = local_fd;

	return 0;

clean_up_file:
	fput(file);
clean_up:
	put_unused_fd(local_fd);
	return err;
}

static int gk20a_tsg_event_id_ctrl(struct gk20a *g, struct tsg_gk20a *tsg,
		struct nvgpu_event_id_ctrl_args *args)
{
	int err = 0;
	int fd = -1;

	if (args->event_id < 0 ||
	    args->event_id >= NVGPU_IOCTL_CHANNEL_EVENT_ID_MAX)
		return -EINVAL;

	switch (args->cmd) {
	case NVGPU_IOCTL_CHANNEL_EVENT_ID_CMD_ENABLE:
		err = gk20a_tsg_event_id_enable(tsg, args->event_id, &fd);
		if (!err)
			args->event_fd = fd;
		break;

	default:
		gk20a_err(dev_from_gk20a(tsg->g),
			   "unrecognized tsg event id cmd: 0x%x",
			   args->cmd);
		err = -EINVAL;
		break;
	}

	return err;
}

int gk20a_tsg_set_runlist_interleave(struct tsg_gk20a *tsg, u32 level)
{
	struct gk20a *g = tsg->g;
	int ret;

	switch (level) {
	case NVGPU_RUNLIST_INTERLEAVE_LEVEL_LOW:
	case NVGPU_RUNLIST_INTERLEAVE_LEVEL_MEDIUM:
	case NVGPU_RUNLIST_INTERLEAVE_LEVEL_HIGH:
		ret = g->ops.fifo.set_runlist_interleave(g, tsg->tsgid,
							true, 0, level);
		if (!ret)
			tsg->interleave_level = level;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret ? ret : g->ops.fifo.update_runlist(g, tsg->runlist_id, ~0, true, true);
}

int gk20a_tsg_set_timeslice(struct tsg_gk20a *tsg, u32 timeslice)
{
	struct gk20a *g = tsg->g;

	if (timeslice < NVGPU_TSG_MIN_TIMESLICE_US ||
		timeslice > NVGPU_TSG_MAX_TIMESLICE_US)
		return -EINVAL;

	gk20a_channel_get_timescale_from_timeslice(g, timeslice,
			&tsg->timeslice_timeout, &tsg->timeslice_scale);

	tsg->timeslice_us = timeslice;

	return g->ops.fifo.update_runlist(g, tsg->runlist_id, ~0, true, true);
}

static void release_used_tsg(struct fifo_gk20a *f, struct tsg_gk20a *tsg)
{
	mutex_lock(&f->tsg_inuse_mutex);
	f->tsg[tsg->tsgid].in_use = false;
	mutex_unlock(&f->tsg_inuse_mutex);
}

static struct tsg_gk20a *acquire_unused_tsg(struct fifo_gk20a *f)
{
	struct tsg_gk20a *tsg = NULL;
	int tsgid;

	mutex_lock(&f->tsg_inuse_mutex);
	for (tsgid = 0; tsgid < f->num_channels; tsgid++) {
		if (!f->tsg[tsgid].in_use) {
			f->tsg[tsgid].in_use = true;
			tsg = &f->tsg[tsgid];
			break;
		}
	}
	mutex_unlock(&f->tsg_inuse_mutex);

	return tsg;
}

int gk20a_tsg_open(struct gk20a *g, struct file *filp)
{
	struct tsg_private *priv;
	struct tsg_gk20a *tsg;
	struct device *dev;

	dev  = dev_from_gk20a(g);

	gk20a_dbg(gpu_dbg_fn, "tsg: %s", dev_name(dev));

	priv = kmalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	tsg = acquire_unused_tsg(&g->fifo);
	if (!tsg) {
		kfree(priv);
		return -ENOMEM;
	}

	tsg->g = g;
	tsg->num_active_channels = 0;
	kref_init(&tsg->refcount);

	tsg->tsg_gr_ctx = NULL;
	tsg->vm = NULL;
	tsg->interleave_level = NVGPU_RUNLIST_INTERLEAVE_LEVEL_LOW;
	tsg->timeslice_us = 0;
	tsg->timeslice_timeout = 0;
	tsg->timeslice_scale = 0;
	tsg->runlist_id = ~0;
	tsg->tgid = current->tgid;

	priv->g = g;
	priv->tsg = tsg;
	filp->private_data = priv;

	gk20a_dbg(gpu_dbg_fn, "tsg opened %d\n", tsg->tsgid);

	gk20a_sched_ctrl_tsg_added(g, tsg);

	return 0;
}

int gk20a_tsg_dev_open(struct inode *inode, struct file *filp)
{
	struct gk20a *g;
	int ret;

	g = container_of(inode->i_cdev,
			 struct gk20a, tsg.cdev);
	gk20a_dbg_fn("");
	ret = gk20a_tsg_open(g, filp);
	gk20a_dbg_fn("done");
	return ret;
}

void gk20a_tsg_release(struct kref *ref)
{
	struct tsg_gk20a *tsg = container_of(ref, struct tsg_gk20a, refcount);
	struct gk20a *g = tsg->g;
	struct gk20a_event_id_data *event_id_data, *event_id_data_temp;

	if (tsg->tsg_gr_ctx) {
		gr_gk20a_free_tsg_gr_ctx(tsg);
		tsg->tsg_gr_ctx = NULL;
	}
	if (tsg->vm) {
		gk20a_vm_put(tsg->vm);
		tsg->vm = NULL;
	}

	gk20a_sched_ctrl_tsg_removed(g, tsg);

	/* unhook all events created on this TSG */
	mutex_lock(&tsg->event_id_list_lock);
	list_for_each_entry_safe(event_id_data, event_id_data_temp,
				&tsg->event_id_list,
				event_id_node) {
		list_del_init(&event_id_data->event_id_node);
	}
	mutex_unlock(&tsg->event_id_list_lock);

	release_used_tsg(&g->fifo, tsg);

	tsg->runlist_id = ~0;

	gk20a_dbg(gpu_dbg_fn, "tsg released %d\n", tsg->tsgid);
}

int gk20a_tsg_dev_release(struct inode *inode, struct file *filp)
{
	struct tsg_private *priv = filp->private_data;
	struct tsg_gk20a *tsg = priv->tsg;
	struct gk20a *g = priv->g;

	if (g->driver_is_dying)
		return -ENODEV;

	kref_put(&tsg->refcount, gk20a_tsg_release);
	kfree(priv);
	return 0;
}

static int gk20a_tsg_ioctl_set_priority(struct gk20a *g,
	struct tsg_gk20a *tsg, struct nvgpu_set_priority_args *arg)
{
	struct gk20a_sched_ctrl *sched = &g->sched_ctrl;
	int err;

	mutex_lock(&sched->control_lock);
	if (sched->control_locked) {
		err = -EPERM;
		goto done;
	}

	err = gk20a_busy(g->dev);
	if (err) {
		gk20a_err(dev_from_gk20a(g), "failed to power on gpu");
		goto done;
	}

	err = gk20a_tsg_set_priority(g, tsg, arg->priority);

	gk20a_idle(g->dev);
done:
	mutex_unlock(&sched->control_lock);
	return err;
}

static int gk20a_tsg_ioctl_set_runlist_interleave(struct gk20a *g,
	struct tsg_gk20a *tsg, struct nvgpu_runlist_interleave_args *arg)
{
	struct gk20a_sched_ctrl *sched = &g->sched_ctrl;
	int err;

	mutex_lock(&sched->control_lock);
	if (sched->control_locked) {
		err = -EPERM;
		goto done;
	}
	err = gk20a_busy(g->dev);
	if (err) {
		gk20a_err(dev_from_gk20a(g), "failed to power on gpu");
		goto done;
	}

	err = gk20a_tsg_set_runlist_interleave(tsg, arg->level);

	gk20a_idle(g->dev);
done:
	mutex_unlock(&sched->control_lock);
	return err;
}

static int gk20a_tsg_ioctl_set_timeslice(struct gk20a *g,
	struct tsg_gk20a *tsg, struct nvgpu_timeslice_args *arg)
{
	struct gk20a_sched_ctrl *sched = &g->sched_ctrl;
	int err;

	mutex_lock(&sched->control_lock);
	if (sched->control_locked) {
		err = -EPERM;
		goto done;
	}
	err = gk20a_busy(g->dev);
	if (err) {
		gk20a_err(dev_from_gk20a(g), "failed to power on gpu");
		goto done;
	}
	err = gk20a_tsg_set_timeslice(tsg, arg->timeslice_us);
	gk20a_idle(g->dev);
done:
	mutex_unlock(&sched->control_lock);
	return err;
}


long gk20a_tsg_dev_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long arg)
{
	struct tsg_private *priv = filp->private_data;
	struct tsg_gk20a *tsg = priv->tsg;
	struct gk20a *g = tsg->g;
	u8 __maybe_unused buf[NVGPU_TSG_IOCTL_MAX_ARG_SIZE];
	int err = 0;

	gk20a_dbg(gpu_dbg_fn, "");

	if ((_IOC_TYPE(cmd) != NVGPU_TSG_IOCTL_MAGIC) ||
	    (_IOC_NR(cmd) == 0) ||
	    (_IOC_NR(cmd) > NVGPU_TSG_IOCTL_LAST))
		return -EINVAL;

	BUG_ON(_IOC_SIZE(cmd) > NVGPU_TSG_IOCTL_MAX_ARG_SIZE);

	memset(buf, 0, sizeof(buf));
	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if (copy_from_user(buf, (void __user *)arg, _IOC_SIZE(cmd)))
			return -EFAULT;
	}

	if (!g->gr.sw_ready) {
		err = gk20a_busy(g->dev);
		if (err)
			return err;

		gk20a_idle(g->dev);
	}

	switch (cmd) {
	case NVGPU_TSG_IOCTL_BIND_CHANNEL:
		{
		int ch_fd = *(int *)buf;
		if (ch_fd < 0) {
			err = -EINVAL;
			break;
		}
		err = gk20a_tsg_bind_channel_fd(tsg, ch_fd);
		break;
		}

	case NVGPU_TSG_IOCTL_UNBIND_CHANNEL:
		/* We do not support explicitly unbinding channel from TSG.
		 * Channel will be unbounded from TSG when it is closed.
		 */
		break;

	case NVGPU_IOCTL_TSG_ENABLE:
		{
		err = gk20a_busy(g->dev);
		if (err) {
			gk20a_err(g->dev,
			   "failed to host gk20a for ioctl cmd: 0x%x", cmd);
			return err;
		}
		gk20a_enable_tsg(tsg);
		gk20a_idle(g->dev);
		break;
		}

	case NVGPU_IOCTL_TSG_DISABLE:
		{
		err = gk20a_busy(g->dev);
		if (err) {
			gk20a_err(g->dev,
			   "failed to host gk20a for ioctl cmd: 0x%x", cmd);
			return err;
		}
		gk20a_disable_tsg(tsg);
		gk20a_idle(g->dev);
		break;
		}

	case NVGPU_IOCTL_TSG_PREEMPT:
		{
		err = gk20a_busy(g->dev);
		if (err) {
			gk20a_err(g->dev,
			   "failed to host gk20a for ioctl cmd: 0x%x", cmd);
			return err;
		}
		/* preempt TSG */
		err = g->ops.fifo.preempt_tsg(g, tsg->tsgid);
		gk20a_idle(g->dev);
		break;
		}

	case NVGPU_IOCTL_TSG_SET_PRIORITY:
		{
		err = gk20a_tsg_ioctl_set_priority(g, tsg,
			(struct nvgpu_set_priority_args *)buf);
		break;
		}

	case NVGPU_IOCTL_TSG_EVENT_ID_CTRL:
		{
		err = gk20a_tsg_event_id_ctrl(g, tsg,
			(struct nvgpu_event_id_ctrl_args *)buf);
		break;
		}

	case NVGPU_IOCTL_TSG_SET_RUNLIST_INTERLEAVE:
		err = gk20a_tsg_ioctl_set_runlist_interleave(g, tsg,
			(struct nvgpu_runlist_interleave_args *)buf);
		break;

	case NVGPU_IOCTL_TSG_SET_TIMESLICE:
		{
		err = gk20a_tsg_ioctl_set_timeslice(g, tsg,
			(struct nvgpu_timeslice_args *)buf);
		break;
		}

	default:
		gk20a_err(dev_from_gk20a(g),
			   "unrecognized tsg gpu ioctl cmd: 0x%x",
			   cmd);
		err = -ENOTTY;
		break;
	}

	if ((err == 0) && (_IOC_DIR(cmd) & _IOC_READ))
		err = copy_to_user((void __user *)arg,
				   buf, _IOC_SIZE(cmd));

	return err;
}

void gk20a_init_tsg_ops(struct gpu_ops *gops)
{
	gops->fifo.tsg_bind_channel = gk20a_tsg_bind_channel;
	gops->fifo.tsg_unbind_channel = gk20a_tsg_unbind_channel;
	gops->fifo.tsg_set_timeslice = gk20a_tsg_set_timeslice;
}
