/*
 * VI NOTIFY driver for T186
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

#include <asm/ioctls.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kfifo.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/sched.h>

#include "vi_notify.h"
#include <vi.h>

/* XXX: move ioctls to include/linux/ (after T18X merge) */
#include <linux/nvhost_vi_ioctl.h>

#define NVHOST_VI_SET_IGN_MASK _IOW(NVHOST_VI_IOCTL_MAGIC, 10, u32)
#define NVHOST_VI_SET_SYNCPTS \
	_IOW(NVHOST_VI_IOCTL_MAGIC, 13, struct tegra_vi4_syncpts_req)
#define NVHOST_VI_ENABLE_REPORTS \
	_IOW(NVHOST_VI_IOCTL_MAGIC, 14, struct tegra_vi4_syncpts_req)
#define NVHOST_VI_RESET_CHANNEL \
	_IOW(NVHOST_VI_IOCTL_MAGIC, 15, struct tegra_vi4_syncpts_req)
#define NVHOST_VI_GET_CAPTURE_STATUS \
	_IOWR(NVHOST_VI_IOCTL_MAGIC, 16, struct vi_capture_status)

struct vi_notify_dev {
	struct vi_notify_driver *driver;
	struct device *device;
	dev_t major;
	u8 num_channels;
	struct mutex lock;
	struct tegra_vi_mfi_ctx *mfi_ctx;
	struct vi_notify_channel __rcu *channels[];
};

#define VI_NOTIFY_TAG_CHANSEL_NLINES 0x08

static int vi_notify_dev_classify(struct vi_notify_dev *vnd)
{
	struct vi_notify_channel *chan;
	u32 ign_mask = -1;
	unsigned i;

	for (i = 0; i < vnd->num_channels; i++) {
		chan = rcu_access_pointer(vnd->channels[i]);
		if (chan != NULL)
			ign_mask &= atomic_read(&chan->ign_mask);
	}

	/* Unmask NLINES event, for Mid-Frame Interrupt */
	if (ign_mask != 0xffffffff && tegra_vi_has_mfi_callback())
		ign_mask &= ~(1u << VI_NOTIFY_TAG_CHANSEL_NLINES);

	return vnd->driver->classify(vnd->device, ~ign_mask);
}

static unsigned vi_notify_occupancy(struct vi_notify_channel *chan)
{
	unsigned ret;

	mutex_lock(&chan->read_lock);
	ret = kfifo_len(&chan->fifo);
	mutex_unlock(&chan->read_lock);

	return ret;
}

void vi_notify_channel_set_notify_funcs(struct vi_notify_channel *chan,
			vi_notify_status_callback notify,
			vi_notify_error_callback error,
			void *client_data)
{
	chan->notify_cb = notify;
	chan->error_cb = error;
	chan->client_data = client_data;
}
EXPORT_SYMBOL(vi_notify_channel_set_notify_funcs);

/* Interrupt handlers */
void vi_notify_dev_error(struct vi_notify_dev *vnd)
{
	struct vi_notify_channel *chan;
	unsigned i;

	rcu_read_lock();
	for (i = 0; i < vnd->num_channels; i++) {
		chan = rcu_dereference(vnd->channels[i]);

		if (chan != NULL) {
			atomic_set(&chan->errors, 1);
			if (chan->error_cb)
				chan->error_cb(chan->client_data);
			else
				wake_up(&chan->readq);
		}
	}
	rcu_read_unlock();
}
EXPORT_SYMBOL(vi_notify_dev_error);

void vi_notify_dev_report(struct vi_notify_dev *vnd, u8 channel,
			const struct vi_capture_status *status)
{
	struct vi_notify_channel *chan;

	rcu_read_lock();
	chan = rcu_dereference(vnd->channels[channel]);

	if (chan != NULL && !atomic_read(&chan->report)) {
		chan->status = *status;
		atomic_set(&chan->report, 1);
		if (chan->notify_cb)
			chan->notify_cb(chan, status, chan->client_data);
		else
			wake_up(&chan->readq);
	}

	rcu_read_unlock();
}
EXPORT_SYMBOL(vi_notify_dev_report);

static bool vi_notify_is_broadcast(u8 tag)
{
	return (1u << tag) & (
		(1u <<  0) /* CSI mux frame start */ |
		(1u <<  1) /* CSI mux frame end */ |
		(1u <<  2) /* CSI mux frame fault */ |
		(1u <<  3) /* CSI mux stream fault */ |
		(1u << 10) /* Frame end fault */ |
		(1u << 11) /* No match fault */ |
		(1u << 12) /* Match collision fault */ |
		(1u << 13) /* Short frame fault */ |
		(1u << 14) /* Load collision */);
}

static void vi_notify_recv(struct vi_notify_dev *vnd,
				const struct vi_notify_msg *msg, u8 channel)
{
	struct vi_notify_channel *chan;
	u8 tag = VI_NOTIFY_TAG_TAG(msg->tag);

	rcu_read_lock();
	chan = rcu_dereference(vnd->channels[channel]);

	/* Notify NLINES event to tegra_vi */
	if (tag == VI_NOTIFY_TAG_CHANSEL_NLINES)
		tegra_vi_mfi_event_notify(vnd->mfi_ctx, channel);

	if (chan != NULL && !((1u << tag) & atomic_read(&chan->ign_mask))) {
		if (!kfifo_put(&chan->fifo, *msg))
			atomic_set(&chan->overruns, 1);
		wake_up(&chan->readq);
	}
	rcu_read_unlock();
}

void vi_notify_dev_recv(struct vi_notify_dev *vnd,
			const struct vi_notify_msg *msg)
{
	u8 channel = VI_NOTIFY_TAG_CHANNEL(msg->tag);
	u8 tag = VI_NOTIFY_TAG_TAG(msg->tag);

	dev_dbg(vnd->device, "Message: tag:%2u channel:%02X frame:%04X\n",
		tag, channel, VI_NOTIFY_TAG_FRAME(msg->tag));
	dev_dbg(vnd->device, "         timestamp %u data 0x%08x",
		msg->stamp, msg->data);

	if (vi_notify_is_broadcast(tag)) {
		for (channel = 0; channel < vnd->num_channels; channel++)
			vi_notify_recv(vnd, msg, channel);
	} else if (channel >= vnd->num_channels)
		dev_warn(vnd->device, "Channel %u out of range!\n", channel);
	else
		vi_notify_recv(vnd, msg, channel);
}
EXPORT_SYMBOL(vi_notify_dev_recv);

/* File operations */
static ssize_t vi_notify_read(struct file *file, char __user *buf, size_t len,
				loff_t *offset)
{
	struct vi_notify_channel *chan = file->private_data;

	for (;;) {
		DEFINE_WAIT(wait);
		unsigned int copied;
		int ret = 0;
		unsigned report = atomic_read(&chan->report);

		if (len < (report
				? sizeof(struct vi_capture_status)
				: sizeof(struct vi_notify_msg)))
			return 0;

		if (mutex_lock_interruptible(&chan->read_lock))
			return -ERESTARTSYS;

		if (report) {
			copied = sizeof(chan->status);
			if (copy_to_user(buf, &chan->status, copied))
				ret = -EFAULT;
			atomic_set(&chan->report, 0);
		} else {
			ret = kfifo_to_user(&chan->fifo, buf, len, &copied);
		}

		mutex_unlock(&chan->read_lock);

		if (ret)
			return ret;
		if (copied > 0)
			return copied;

		prepare_to_wait(&chan->readq, &wait, TASK_INTERRUPTIBLE);

		if (atomic_xchg(&chan->overruns, 0))
			ret = -EOVERFLOW;
		else if (atomic_xchg(&chan->errors, 0))
			ret = -EIO;
		else if (signal_pending(current))
			ret = -ERESTARTSYS;
		else if (file->f_flags & O_NONBLOCK)
			ret = -EAGAIN;
		else if (!vi_notify_occupancy(chan))
			schedule();

		finish_wait(&chan->readq, &wait);

		if (ret)
			return ret;
	}
}

static unsigned int vi_notify_poll(struct file *file,
					struct poll_table_struct *table)
{
	struct vi_notify_channel *chan = file->private_data;
	unsigned ret = 0;

	poll_wait(file, &chan->readq, table);

	if (vi_notify_occupancy(chan))
		ret |= POLLIN | POLLRDNORM;
	if (atomic_read(&chan->overruns) || atomic_read(&chan->errors))
		ret |= POLLERR;
	if (atomic_read(&chan->report))
		ret |= POLLPRI;

	return ret;
}

int vi_notify_channel_set_ign_mask(struct vi_notify_channel *chan, u32 mask)
{
	struct vi_notify_dev *vnd = chan->vnd;
	u32 old_mask;
	int err;

	if (mutex_lock_interruptible(&vnd->lock))
		return -ERESTARTSYS;

	old_mask = atomic_xchg(&chan->ign_mask, mask);

	err = vi_notify_dev_classify(vnd);
	if (err)
		atomic_set(&chan->ign_mask, old_mask);

	mutex_unlock(&vnd->lock);

	return err;
}
EXPORT_SYMBOL(vi_notify_channel_set_ign_mask);

int vi_notify_channel_set_syncpts(unsigned channel,
			struct vi_notify_channel *chan,
			struct tegra_vi4_syncpts_req *req)
{
	struct vi_notify_dev *vnd = chan->vnd;
	int err;

	if (!vnd->driver->set_syncpts)
		return -ENOTSUPP;
	if (req->pad)
		return -EINVAL;
	if (mutex_lock_interruptible(&vnd->lock))
		return -ERESTARTSYS;

	err = vnd->driver->set_syncpts(vnd->device, channel,
					req->syncpt_ids);
	mutex_unlock(&vnd->lock);

	return err;
}
EXPORT_SYMBOL(vi_notify_channel_set_syncpts);

int vi_notify_channel_enable_reports(unsigned channel,
			struct vi_notify_channel *chan,
			struct tegra_vi4_syncpts_req *req)
{
	struct vi_notify_dev *vnd = chan->vnd;
	int err;

	if (!vnd->driver->enable_reports)
		return -ENOTSUPP;
	if (req->pad)
		return -EINVAL;
	if (mutex_lock_interruptible(&vnd->lock))
		return -ERESTARTSYS;

	err = vnd->driver->enable_reports(vnd->device, channel,
				req->stream, req->vc, req->syncpt_ids);
	mutex_unlock(&vnd->lock);

	dev_dbg(vnd->device, "vi_notify_channel_enable_reports: ch:%d",
		channel);

	return err;
}
EXPORT_SYMBOL(vi_notify_channel_enable_reports);

int vi_notify_channel_reset(unsigned channel,
			struct vi_notify_channel *chan,
			struct tegra_vi4_syncpts_req *req)
{
	struct vi_notify_dev *vnd = chan->vnd;

	if (!vnd->driver->reset_channel)
		return -ENOTSUPP;
	if (req->pad)
		return -EINVAL;
	if (mutex_lock_interruptible(&vnd->lock))
		return -ERESTARTSYS;

	vnd->driver->reset_channel(vnd->device, channel);
	mutex_unlock(&vnd->lock);

	return 0;
}
EXPORT_SYMBOL(vi_notify_channel_reset);

int vi_notify_get_capture_status(struct vi_notify_channel *chan,
			unsigned channel,
			u64 index,
			struct vi_capture_status *status)
{
	struct vi_notify_dev *vnd = chan->vnd;
	int err = 0;

	if (!vnd->driver->get_capture_status)
		return -ENOTSUPP;
	if (mutex_lock_interruptible(&vnd->lock))
		return -ERESTARTSYS;

	err = vnd->driver->get_capture_status(vnd->device,
						channel, index, status);
	mutex_unlock(&vnd->lock);

	dev_dbg(vnd->device, "vi_notify_get_capture_status: ch:%2d sof_ts:%llu eof_ts:%llu idx:%llu\n",
		channel, status->sof_ts, status->eof_ts, index);

	return err;
}
EXPORT_SYMBOL(vi_notify_get_capture_status);

static long vi_notify_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	struct vi_notify_channel *chan = file->private_data;
	unsigned channel = iminor(file_inode(file));

	switch (cmd) {
	case FIONREAD: {
		int val;

		val = vi_notify_occupancy(chan);
		val *= sizeof(struct vi_notify_msg);
		return put_user(val, (int __user *)arg);
	}

	case NVHOST_VI_SET_IGN_MASK: {
		u32 mask;

		if (get_user(mask, (u32 __user *)arg))
			return -EFAULT;

		return vi_notify_channel_set_ign_mask(chan, mask);
	}

	case NVHOST_VI_SET_SYNCPTS: {
		struct tegra_vi4_syncpts_req req;

		if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
			return -EFAULT;

		return vi_notify_channel_set_syncpts(channel, chan, &req);
	}

	case NVHOST_VI_ENABLE_REPORTS: {
		struct tegra_vi4_syncpts_req req;

		if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
			return -EFAULT;

		return vi_notify_channel_enable_reports(channel, chan, &req);
	}

	case NVHOST_VI_RESET_CHANNEL: {
		struct tegra_vi4_syncpts_req req;

		if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
			return -EFAULT;

		return vi_notify_channel_reset(channel, chan, &req);
	}

	case NVHOST_VI_GET_CAPTURE_STATUS: {
		int err = 0;
		u64 index = 0;
		struct vi_capture_status status;

		if (copy_from_user(&index, (void __user *)arg, sizeof(u64)))
			return -EFAULT;

		err = vi_notify_get_capture_status(chan, channel,
					index, &status);

		if (likely(err == 0)) {
			if (copy_to_user((void __user *)arg, &status,
								sizeof(status)))
				return -EFAULT;
		}

		return err;
	}
	}

	return -ENOIOCTLCMD;
}

static struct vi_notify_dev *vnd_;
static DEFINE_MUTEX(vnd_lock);

struct vi_notify_channel *vi_notify_channel_open(unsigned channel)
{
	struct vi_notify_dev *vnd;
	struct vi_notify_channel *chan;

	if (mutex_lock_interruptible(&vnd_lock))
		return ERR_PTR(-ERESTARTSYS);

	vnd = vnd_;

	if (vnd == NULL || channel >= vnd->num_channels ||
		!try_module_get(vnd->driver->owner)) {
		mutex_unlock(&vnd_lock);
		return ERR_PTR(-ENODEV);
	}
	mutex_unlock(&vnd_lock);

	if (vnd->driver->has_notifier_backend) {
		if (!vnd->driver->has_notifier_backend(vnd->device)) {
			/* TODO: poll/wait and recheck before returning error */
			dev_err(vnd->device,
				"vi_notifier_backend is not up and running\n");
			return ERR_PTR(-ENODEV);
		}
	}

	chan = kzalloc(sizeof(*chan), GFP_KERNEL);
	if (unlikely(chan == NULL)) {
		module_put(vnd->driver->owner);
		return ERR_PTR(-ENOMEM);
	}

	chan->vnd = vnd;
	atomic_set(&chan->ign_mask, 0xffffffff);
	init_waitqueue_head(&chan->readq);
	mutex_init(&chan->read_lock);

	atomic_set(&chan->overruns, 0);
	atomic_set(&chan->errors, 0);
	atomic_set(&chan->report, 0);
	INIT_KFIFO(chan->fifo);

	mutex_lock(&vnd->lock);
	if (rcu_access_pointer(vnd->channels[channel]) != NULL) {
		mutex_unlock(&vnd->lock);
		kfree(chan);
		module_put(vnd->driver->owner);
		return ERR_PTR(-EBUSY);
	}

	rcu_assign_pointer(vnd->channels[channel], chan);
	mutex_unlock(&vnd->lock);

	return chan;
}
EXPORT_SYMBOL(vi_notify_channel_open);

int vi_notify_channel_close(unsigned channel, struct vi_notify_channel *chan)
{
	struct vi_notify_dev *vnd = chan->vnd;

	mutex_lock(&vnd->lock);
	if (vnd->driver->reset_channel)
		vnd->driver->reset_channel(vnd->device, channel);

	WARN_ON(rcu_access_pointer(vnd->channels[channel]) != chan);
	RCU_INIT_POINTER(vnd->channels[channel], NULL);

	vi_notify_dev_classify(vnd);
	mutex_unlock(&vnd->lock);
	kfree_rcu(chan, rcu);
	module_put(vnd->driver->owner);

	return 0;
}
EXPORT_SYMBOL(vi_notify_channel_close);

static int vi_notify_open(struct inode *inode, struct file *file)
{
	struct vi_notify_channel *chan;
	unsigned channel = iminor(inode);

	if ((file->f_flags & O_ACCMODE) != O_RDONLY)
		return -EINVAL;

	chan = vi_notify_channel_open(channel);
	if (IS_ERR(chan))
		return PTR_ERR(chan);

	file->private_data = chan;

	return nonseekable_open(inode, file);
}

static int vi_notify_release(struct inode *inode, struct file *file)
{
	struct vi_notify_channel *chan = file->private_data;
	unsigned channel = iminor(inode);

	return vi_notify_channel_close(channel, chan);
}

static const struct file_operations vi_notify_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.read = vi_notify_read,
	.poll = vi_notify_poll,
	.unlocked_ioctl = vi_notify_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = vi_notify_ioctl,
#endif
	.open = vi_notify_open,
	.release = vi_notify_release,
};

/* Character device */
static struct class *vi_notify_class;
static int vi_notify_major;

int vi_notify_register(struct vi_notify_driver *drv, struct device *dev,
			u8 num_channels)
{
	struct vi_notify_dev *vnd;
	int err;
	unsigned i;

	vnd = devm_kzalloc(dev, sizeof(*vnd) + num_channels *
			sizeof(struct vi_notify_channel *), GFP_KERNEL);
	if (unlikely(vnd == NULL))
		return -ENOMEM;

	vnd->driver = drv;
	vnd->device = dev;
	vnd->num_channels = num_channels;
	mutex_init(&vnd->lock);

	err = vnd->driver->probe(vnd->device, vnd);
	if (err)
		return err;

	err = vi_notify_dev_classify(vnd);
	if (err)
		goto error;

	err = tegra_vi_init_mfi(&vnd->mfi_ctx, num_channels);
	if (err)
		goto error;

	mutex_lock(&vnd_lock);
	if (vnd_ != NULL) {
		mutex_unlock(&vnd_lock);
		tegra_vi_deinit_mfi(&vnd->mfi_ctx);
		WARN_ON(1);
		err = -EBUSY;
		goto error;
	}
	vnd_ = vnd;
	mutex_unlock(&vnd_lock);

	for (i = 0; i < vnd->num_channels; i++) {
		dev_t devt = MKDEV(vi_notify_major, i);

		device_create(vi_notify_class, vnd->device, devt, NULL,
				"tegra-vi0-channel%u", i);
	}

	return 0;
error:
	if (vnd->driver->remove)
		vnd->driver->remove(dev);
	return err;
}
EXPORT_SYMBOL(vi_notify_register);

void vi_notify_unregister(struct vi_notify_driver *drv, struct device *dev)
{
	struct vi_notify_dev *vnd;
	unsigned i;

	mutex_lock(&vnd_lock);
	vnd = vnd_;
	vnd_ = NULL;
	WARN_ON(vnd->driver != drv);
	WARN_ON(vnd->device != dev);
	mutex_unlock(&vnd_lock);

	tegra_vi_deinit_mfi(&vnd->mfi_ctx);

	for (i = 0; i < vnd->num_channels; i++) {
		dev_t devt = MKDEV(vi_notify_major, i);

		device_destroy(vi_notify_class, devt);
	}

	if (vnd->driver->remove)
		vnd->driver->remove(vnd->device);
	devm_kfree(vnd->device, vnd);
}
EXPORT_SYMBOL(vi_notify_unregister);

static int __init vi_notify_init(void)
{
	vi_notify_class = class_create(THIS_MODULE, "tegra-vi-channel");
	if (IS_ERR(vi_notify_class))
		return PTR_ERR(vi_notify_class);

	vi_notify_major = register_chrdev(0, "tegra-vi-channel",
						&vi_notify_fops);
	if (vi_notify_major < 0) {
		class_destroy(vi_notify_class);
		return vi_notify_major;
	}
	return 0;
}

static void __exit vi_notify_exit(void)
{
	unregister_chrdev(vi_notify_major, "tegra-vi-channel");
	class_destroy(vi_notify_class);
}

subsys_initcall(vi_notify_init);
module_exit(vi_notify_exit);
