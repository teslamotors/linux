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

#include <asm/barrier.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/circ_buf.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/wait.h>
#include <linux/ktime.h>
#include <linux/nvgpu.h>
#include <linux/hashtable.h>
#include <linux/debugfs.h>
#include <linux/log2.h>
#include <uapi/linux/nvgpu.h>
#include "ctxsw_trace_gk20a.h"
#include "gk20a.h"
#include "gr_gk20a.h"
#include "hw_ctxsw_prog_gk20a.h"
#include "hw_gr_gk20a.h"
#include "sched_gk20a.h"

ssize_t gk20a_sched_dev_read(struct file *filp, char __user *buf,
	size_t size, loff_t *off)
{
	struct gk20a_sched_ctrl *sched = filp->private_data;
	struct nvgpu_sched_event_arg event = { 0 };
	int err;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_sched,
		"filp=%p buf=%p size=%zu", filp, buf, size);

	if (size < sizeof(event))
		return -EINVAL;
	size = sizeof(event);

	mutex_lock(&sched->status_lock);
	while (!sched->status) {
		mutex_unlock(&sched->status_lock);
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		err = wait_event_interruptible(sched->readout_wq,
			sched->status);
		if (err)
			return err;
		mutex_lock(&sched->status_lock);
	}

	event.reserved = 0;
	event.status = sched->status;

	if (copy_to_user(buf, &event, size)) {
		mutex_unlock(&sched->status_lock);
		return -EFAULT;
	}

	sched->status = 0;

	mutex_unlock(&sched->status_lock);

	return size;
}

unsigned int gk20a_sched_dev_poll(struct file *filp, poll_table *wait)
{
	struct gk20a_sched_ctrl *sched = filp->private_data;
	unsigned int mask = 0;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_sched, "");

	mutex_lock(&sched->status_lock);
	poll_wait(filp, &sched->readout_wq, wait);
	if (sched->status)
		mask |= POLLIN | POLLRDNORM;
	mutex_unlock(&sched->status_lock);

	return mask;
}

static int gk20a_sched_dev_ioctl_get_tsgs(struct gk20a_sched_ctrl *sched,
	struct nvgpu_sched_get_tsgs_args *arg)
{
	gk20a_dbg(gpu_dbg_fn | gpu_dbg_sched, "size=%u buffer=%llx",
			arg->size, arg->buffer);

	if ((arg->size < sched->bitmap_size) || (!arg->buffer)) {
		arg->size = sched->bitmap_size;
		return -ENOSPC;
	}

	mutex_lock(&sched->status_lock);
	if (copy_to_user((void __user *)(uintptr_t)arg->buffer,
		sched->active_tsg_bitmap, sched->bitmap_size)) {
		mutex_unlock(&sched->status_lock);
		return -EFAULT;
	}
	mutex_unlock(&sched->status_lock);

	return 0;
}

static int gk20a_sched_dev_ioctl_get_recent_tsgs(struct gk20a_sched_ctrl *sched,
	struct nvgpu_sched_get_tsgs_args *arg)
{
	gk20a_dbg(gpu_dbg_fn | gpu_dbg_sched, "size=%u buffer=%llx",
			arg->size, arg->buffer);

	if ((arg->size < sched->bitmap_size) || (!arg->buffer)) {
		arg->size = sched->bitmap_size;
		return -ENOSPC;
	}

	mutex_lock(&sched->status_lock);
	if (copy_to_user((void __user *)(uintptr_t)arg->buffer,
		sched->recent_tsg_bitmap, sched->bitmap_size)) {
		mutex_unlock(&sched->status_lock);
		return -EFAULT;
	}

	memset(sched->recent_tsg_bitmap, 0, sched->bitmap_size);
	mutex_unlock(&sched->status_lock);

	return 0;
}

static int gk20a_sched_dev_ioctl_get_tsgs_by_pid(struct gk20a_sched_ctrl *sched,
	struct nvgpu_sched_get_tsgs_by_pid_args *arg)
{
	struct fifo_gk20a *f = &sched->g->fifo;
	struct tsg_gk20a *tsg;
	u64 *bitmap;
	int tsgid;
	/* pid at user level corresponds to kernel tgid */
	pid_t tgid = (pid_t)arg->pid;
	int err = 0;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_sched, "pid=%d size=%u buffer=%llx",
			(pid_t)arg->pid, arg->size, arg->buffer);

	if ((arg->size < sched->bitmap_size) || (!arg->buffer)) {
		arg->size = sched->bitmap_size;
		return -ENOSPC;
	}

	bitmap = kzalloc(sched->bitmap_size, GFP_KERNEL);
	if (!bitmap)
		return -ENOMEM;

	mutex_lock(&sched->status_lock);
	for (tsgid = 0; tsgid < f->num_channels; tsgid++) {
		if (NVGPU_SCHED_ISSET(tsgid, sched->active_tsg_bitmap)) {
			tsg = &f->tsg[tsgid];
			if (tsg->tgid == tgid)
				NVGPU_SCHED_SET(tsgid, bitmap);
		}
	}
	mutex_unlock(&sched->status_lock);

	if (copy_to_user((void __user *)(uintptr_t)arg->buffer,
		bitmap, sched->bitmap_size))
		err = -EFAULT;

	kfree(bitmap);

	return err;
}

static int gk20a_sched_dev_ioctl_get_params(struct gk20a_sched_ctrl *sched,
	struct nvgpu_sched_tsg_get_params_args *arg)
{
	struct gk20a *g = sched->g;
	struct fifo_gk20a *f = &g->fifo;
	struct tsg_gk20a *tsg;
	u32 tsgid = arg->tsgid;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_sched, "tsgid=%u", tsgid);

	if (tsgid >= f->num_channels)
		return -EINVAL;

	tsg = &f->tsg[tsgid];
	if (!kref_get_unless_zero(&tsg->refcount))
		return -ENXIO;

	arg->pid = tsg->tgid;	/* kernel tgid corresponds to user pid */
	arg->runlist_interleave = tsg->interleave_level;
	arg->timeslice = tsg->timeslice_us;

	if (tsg->tsg_gr_ctx) {
		arg->graphics_preempt_mode =
			tsg->tsg_gr_ctx->graphics_preempt_mode;
		arg->compute_preempt_mode =
			tsg->tsg_gr_ctx->compute_preempt_mode;
	} else {
		arg->graphics_preempt_mode = 0;
		arg->compute_preempt_mode = 0;
	}

	kref_put(&tsg->refcount, gk20a_tsg_release);

	return 0;
}

static int gk20a_sched_dev_ioctl_tsg_set_timeslice(
	struct gk20a_sched_ctrl *sched,
	struct nvgpu_sched_tsg_timeslice_args *arg)
{
	struct gk20a *g = sched->g;
	struct fifo_gk20a *f = &g->fifo;
	struct tsg_gk20a *tsg;
	u32 tsgid = arg->tsgid;
	int err;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_sched, "tsgid=%u", tsgid);

	if (tsgid >= f->num_channels)
		return -EINVAL;

	tsg = &f->tsg[tsgid];
	if (!kref_get_unless_zero(&tsg->refcount))
		return -ENXIO;

	err = gk20a_busy(g->dev);
	if (err)
		goto done;

	err = gk20a_tsg_set_timeslice(tsg, arg->timeslice);

	gk20a_idle(g->dev);

done:
	kref_put(&tsg->refcount, gk20a_tsg_release);

	return err;
}

static int gk20a_sched_dev_ioctl_tsg_set_runlist_interleave(
	struct gk20a_sched_ctrl *sched,
	struct nvgpu_sched_tsg_runlist_interleave_args *arg)
{
	struct gk20a *g = sched->g;
	struct fifo_gk20a *f = &g->fifo;
	struct tsg_gk20a *tsg;
	u32 tsgid = arg->tsgid;
	int err;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_sched, "tsgid=%u", tsgid);

	if (tsgid >= f->num_channels)
		return -EINVAL;

	tsg = &f->tsg[tsgid];
	if (!kref_get_unless_zero(&tsg->refcount))
		return -ENXIO;

	err = gk20a_busy(g->dev);
	if (err)
		goto done;

	err = gk20a_tsg_set_runlist_interleave(tsg, arg->runlist_interleave);

	gk20a_idle(g->dev);

done:
	kref_put(&tsg->refcount, gk20a_tsg_release);

	return err;
}

static int gk20a_sched_dev_ioctl_lock_control(struct gk20a_sched_ctrl *sched)
{
	gk20a_dbg(gpu_dbg_fn | gpu_dbg_sched, "");

	mutex_lock(&sched->control_lock);
	sched->control_locked = true;
	mutex_unlock(&sched->control_lock);
	return 0;
}

static int gk20a_sched_dev_ioctl_unlock_control(struct gk20a_sched_ctrl *sched)
{
	gk20a_dbg(gpu_dbg_fn | gpu_dbg_sched, "");

	mutex_lock(&sched->control_lock);
	sched->control_locked = false;
	mutex_unlock(&sched->control_lock);
	return 0;
}

static int gk20a_sched_dev_ioctl_get_api_version(struct gk20a_sched_ctrl *sched,
	struct nvgpu_sched_api_version_args *args)
{
	gk20a_dbg(gpu_dbg_fn | gpu_dbg_sched, "");

	args->version = NVGPU_SCHED_API_VERSION;
	return 0;
}

static int gk20a_sched_dev_ioctl_get_tsg(struct gk20a_sched_ctrl *sched,
	struct nvgpu_sched_tsg_refcount_args *arg)
{
	struct gk20a *g = sched->g;
	struct fifo_gk20a *f = &g->fifo;
	struct tsg_gk20a *tsg;
	u32 tsgid = arg->tsgid;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_sched, "tsgid=%u", tsgid);

	if (tsgid >= f->num_channels)
		return -EINVAL;

	tsg = &f->tsg[tsgid];
	if (!kref_get_unless_zero(&tsg->refcount))
		return -ENXIO;

	mutex_lock(&sched->status_lock);
	if (NVGPU_SCHED_ISSET(tsgid, sched->ref_tsg_bitmap)) {
		gk20a_warn(dev_from_gk20a(g),
			"tsgid=%d already referenced", tsgid);
		/* unlock status_lock as gk20a_tsg_release locks it */
		mutex_unlock(&sched->status_lock);
		kref_put(&tsg->refcount, gk20a_tsg_release);
		return -ENXIO;
	}

	/* keep reference on TSG, will be released on
	 * NVGPU_SCHED_IOCTL_PUT_TSG ioctl, or close
	 */
	NVGPU_SCHED_SET(tsgid, sched->ref_tsg_bitmap);
	mutex_unlock(&sched->status_lock);

	return 0;
}

static int gk20a_sched_dev_ioctl_put_tsg(struct gk20a_sched_ctrl *sched,
	struct nvgpu_sched_tsg_refcount_args *arg)
{
	struct gk20a *g = sched->g;
	struct fifo_gk20a *f = &g->fifo;
	struct tsg_gk20a *tsg;
	u32 tsgid = arg->tsgid;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_sched, "tsgid=%u", tsgid);

	if (tsgid >= f->num_channels)
		return -EINVAL;

	mutex_lock(&sched->status_lock);
	if (!NVGPU_SCHED_ISSET(tsgid, sched->ref_tsg_bitmap)) {
		mutex_unlock(&sched->status_lock);
		gk20a_warn(dev_from_gk20a(g),
			"tsgid=%d not previously referenced", tsgid);
		return -ENXIO;
	}
	NVGPU_SCHED_CLR(tsgid, sched->ref_tsg_bitmap);
	mutex_unlock(&sched->status_lock);

	tsg = &f->tsg[tsgid];
	kref_put(&tsg->refcount, gk20a_tsg_release);

	return 0;
}

int gk20a_sched_dev_open(struct inode *inode, struct file *filp)
{
	struct gk20a *g = container_of(inode->i_cdev,
				struct gk20a, sched.cdev);
	struct gk20a_sched_ctrl *sched = &g->sched_ctrl;
	int err;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_sched, "g=%p", g);

	if (!sched->sw_ready) {
		err = gk20a_busy(g->dev);
		if (err)
			return err;

		gk20a_idle(g->dev);
	}

	if (!mutex_trylock(&sched->busy_lock))
		return -EBUSY;

	memcpy(sched->recent_tsg_bitmap, sched->active_tsg_bitmap,
			sched->bitmap_size);
	memset(sched->ref_tsg_bitmap, 0, sched->bitmap_size);

	filp->private_data = sched;
	gk20a_dbg(gpu_dbg_sched, "filp=%p sched=%p", filp, sched);

	return 0;
}

long gk20a_sched_dev_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	struct gk20a_sched_ctrl *sched = filp->private_data;
	struct gk20a *g = sched->g;
	u8 buf[NVGPU_CTXSW_IOCTL_MAX_ARG_SIZE];
	int err = 0;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_sched, "nr=%d", _IOC_NR(cmd));

	if ((_IOC_TYPE(cmd) != NVGPU_SCHED_IOCTL_MAGIC) ||
		(_IOC_NR(cmd) == 0) ||
		(_IOC_NR(cmd) > NVGPU_SCHED_IOCTL_LAST) ||
		(_IOC_SIZE(cmd) > NVGPU_SCHED_IOCTL_MAX_ARG_SIZE))
		return -EINVAL;

	memset(buf, 0, sizeof(buf));
	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if (copy_from_user(buf, (void __user *)arg, _IOC_SIZE(cmd)))
			return -EFAULT;
	}

	switch (cmd) {
	case NVGPU_SCHED_IOCTL_GET_TSGS:
		err = gk20a_sched_dev_ioctl_get_tsgs(sched,
			(struct nvgpu_sched_get_tsgs_args *)buf);
		break;
	case NVGPU_SCHED_IOCTL_GET_RECENT_TSGS:
		err = gk20a_sched_dev_ioctl_get_recent_tsgs(sched,
			(struct nvgpu_sched_get_tsgs_args *)buf);
		break;
	case NVGPU_SCHED_IOCTL_GET_TSGS_BY_PID:
		err = gk20a_sched_dev_ioctl_get_tsgs_by_pid(sched,
			(struct nvgpu_sched_get_tsgs_by_pid_args *)buf);
		break;
	case NVGPU_SCHED_IOCTL_TSG_GET_PARAMS:
		err = gk20a_sched_dev_ioctl_get_params(sched,
			(struct nvgpu_sched_tsg_get_params_args *)buf);
		break;
	case NVGPU_SCHED_IOCTL_TSG_SET_TIMESLICE:
		err = gk20a_sched_dev_ioctl_tsg_set_timeslice(sched,
			(struct nvgpu_sched_tsg_timeslice_args *)buf);
		break;
	case NVGPU_SCHED_IOCTL_TSG_SET_RUNLIST_INTERLEAVE:
		err = gk20a_sched_dev_ioctl_tsg_set_runlist_interleave(sched,
			(struct nvgpu_sched_tsg_runlist_interleave_args *)buf);
		break;
	case NVGPU_SCHED_IOCTL_LOCK_CONTROL:
		err = gk20a_sched_dev_ioctl_lock_control(sched);
		break;
	case NVGPU_SCHED_IOCTL_UNLOCK_CONTROL:
		err = gk20a_sched_dev_ioctl_unlock_control(sched);
		break;
	case NVGPU_SCHED_IOCTL_GET_API_VERSION:
		err = gk20a_sched_dev_ioctl_get_api_version(sched,
			(struct nvgpu_sched_api_version_args *)buf);
		break;
	case NVGPU_SCHED_IOCTL_GET_TSG:
		err = gk20a_sched_dev_ioctl_get_tsg(sched,
			(struct nvgpu_sched_tsg_refcount_args *)buf);
		break;
	case NVGPU_SCHED_IOCTL_PUT_TSG:
		err = gk20a_sched_dev_ioctl_put_tsg(sched,
			(struct nvgpu_sched_tsg_refcount_args *)buf);
		break;
	default:
		dev_dbg(dev_from_gk20a(g), "unrecognized gpu ioctl cmd: 0x%x",
			cmd);
		err = -ENOTTY;
	}

	/* Some ioctls like NVGPU_SCHED_IOCTL_GET_TSGS might be called on
	 * purpose with NULL buffer and/or zero size to discover TSG bitmap
	 * size. We need to update user arguments in this case too, even
	 * if we return an error.
	 */
	if ((!err || (err == -ENOSPC)) && (_IOC_DIR(cmd) & _IOC_READ)) {
		if (copy_to_user((void __user *)arg, buf, _IOC_SIZE(cmd)))
			err = -EFAULT;
	}

	return err;
}

int gk20a_sched_dev_release(struct inode *inode, struct file *filp)
{
	struct gk20a_sched_ctrl *sched = filp->private_data;
	struct gk20a *g = sched->g;
	struct fifo_gk20a *f = &g->fifo;
	struct tsg_gk20a *tsg;
	int tsgid;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_sched, "sched: %p", sched);

	/* release any reference to TSGs */
	for (tsgid = 0; tsgid < f->num_channels; tsgid++) {
		if (NVGPU_SCHED_ISSET(tsgid, sched->ref_tsg_bitmap)) {
			tsg = &f->tsg[tsgid];
			kref_put(&tsg->refcount, gk20a_tsg_release);
		}
	}

	/* unlock control */
	mutex_lock(&sched->control_lock);
	sched->control_locked = false;
	mutex_unlock(&sched->control_lock);

	mutex_unlock(&sched->busy_lock);
	return 0;
}

#ifdef CONFIG_DEBUG_FS
static int gk20a_sched_debugfs_show(struct seq_file *s, void *unused)
{
	struct device *dev = s->private;
	struct gk20a *g = gk20a_get_platform(dev)->g;
	struct gk20a_sched_ctrl *sched = &g->sched_ctrl;

	int n = sched->bitmap_size / sizeof(u64);
	int i;
	int err;

	err = gk20a_busy(g->dev);
	if (err)
		return err;

	seq_printf(s, "control_locked=%d\n", sched->control_locked);
	seq_printf(s, "busy=%d\n", mutex_is_locked(&sched->busy_lock));
	seq_printf(s, "bitmap_size=%zu\n", sched->bitmap_size);

	mutex_lock(&sched->status_lock);

	seq_puts(s, "active_tsg_bitmap\n");
	for (i = 0; i < n; i++)
		seq_printf(s, "\t0x%016llx\n", sched->active_tsg_bitmap[i]);

	seq_puts(s, "recent_tsg_bitmap\n");
	for (i = 0; i < n; i++)
		seq_printf(s, "\t0x%016llx\n", sched->recent_tsg_bitmap[i]);

	mutex_unlock(&sched->status_lock);

	gk20a_idle(g->dev);

	return 0;
}

static int gk20a_sched_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, gk20a_sched_debugfs_show, inode->i_private);
}

static const struct file_operations gk20a_sched_debugfs_fops = {
	.open		= gk20a_sched_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

void gk20a_sched_debugfs_init(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);

	debugfs_create_file("sched_ctrl", S_IRUGO, platform->debugfs,
			dev, &gk20a_sched_debugfs_fops);
}
#endif /* CONFIG_DEBUG_FS */

void gk20a_sched_ctrl_tsg_added(struct gk20a *g, struct tsg_gk20a *tsg)
{
	struct gk20a_sched_ctrl *sched = &g->sched_ctrl;
	int err;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_sched, "tsgid=%u", tsg->tsgid);

	if (!sched->sw_ready) {
		err = gk20a_busy(g->dev);
		if (err) {
			WARN_ON(err);
			return;
		}

		gk20a_idle(g->dev);
	}

	mutex_lock(&sched->status_lock);
	NVGPU_SCHED_SET(tsg->tsgid, sched->active_tsg_bitmap);
	NVGPU_SCHED_SET(tsg->tsgid, sched->recent_tsg_bitmap);
	sched->status |= NVGPU_SCHED_STATUS_TSG_OPEN;
	mutex_unlock(&sched->status_lock);
	wake_up_interruptible(&sched->readout_wq);
}

void gk20a_sched_ctrl_tsg_removed(struct gk20a *g, struct tsg_gk20a *tsg)
{
	struct gk20a_sched_ctrl *sched = &g->sched_ctrl;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_sched, "tsgid=%u", tsg->tsgid);

	mutex_lock(&sched->status_lock);
	NVGPU_SCHED_CLR(tsg->tsgid, sched->active_tsg_bitmap);

	/* clear recent_tsg_bitmap as well: if app manager did not
	 * notice that TSG was previously added, no need to notify it
	 * if the TSG has been released in the meantime. If the
	 * TSG gets reallocated, app manager will be notified as usual.
	 */
	NVGPU_SCHED_CLR(tsg->tsgid, sched->recent_tsg_bitmap);

	/* do not set event_pending, we only want to notify app manager
	 * when TSGs are added, so that it can apply sched params
	 */
	mutex_unlock(&sched->status_lock);
}

int gk20a_sched_ctrl_init(struct gk20a *g)
{
	struct gk20a_sched_ctrl *sched = &g->sched_ctrl;
	struct fifo_gk20a *f = &g->fifo;

	if (sched->sw_ready)
		return 0;

	sched->g = g;
	sched->bitmap_size = roundup(f->num_channels, 64) / 8;
	sched->status = 0;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_sched, "g=%p sched=%p size=%zu",
			g, sched, sched->bitmap_size);

	sched->active_tsg_bitmap = kzalloc(sched->bitmap_size, GFP_KERNEL);
	if (!sched->active_tsg_bitmap)
		return -ENOMEM;

	sched->recent_tsg_bitmap = kzalloc(sched->bitmap_size, GFP_KERNEL);
	if (!sched->recent_tsg_bitmap)
		goto free_active;

	sched->ref_tsg_bitmap = kzalloc(sched->bitmap_size, GFP_KERNEL);
	if (!sched->ref_tsg_bitmap)
		goto free_recent;

	init_waitqueue_head(&sched->readout_wq);
	mutex_init(&sched->status_lock);
	mutex_init(&sched->control_lock);
	mutex_init(&sched->busy_lock);

	sched->sw_ready = true;

	return 0;

free_recent:
	kfree(sched->recent_tsg_bitmap);

free_active:
	kfree(sched->active_tsg_bitmap);

	return -ENOMEM;
}

void gk20a_sched_ctrl_cleanup(struct gk20a *g)
{
	struct gk20a_sched_ctrl *sched = &g->sched_ctrl;

	kfree(sched->active_tsg_bitmap);
	kfree(sched->recent_tsg_bitmap);
	kfree(sched->ref_tsg_bitmap);
	sched->active_tsg_bitmap = NULL;
	sched->recent_tsg_bitmap = NULL;
	sched->ref_tsg_bitmap = NULL;
	sched->sw_ready = false;
}
