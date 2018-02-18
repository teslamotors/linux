/*
 * Copyright (c) 2016 NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.	See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.	If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/cdev.h>
#include <linux/dcache.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/tegra-ivc.h>
#include <linux/tegra-ivc-bus.h>
#include <linux/tegra-ivc-instance.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <asm/ioctls.h>
#include <asm/uaccess.h>

#define DEVICE_ECHO 0
#define DEVICE_DBG 1
#define DEVICE_COUNT 2

#define CCIOGNFRAMES _IOR('c', 1, int)
#define CCIOGNBYTES _IOR('c', 2, int)

struct tegra_camchar_data {
	struct cdev cdev;
	struct mutex io_lock;
	wait_queue_head_t waitq;
	bool is_open;
};

static struct tegra_ivc_channel *tegra_camchar_channels[DEVICE_COUNT];
static DEFINE_MUTEX(tegra_camchar_lock_open);
static int tegra_camchar_major_number;
static struct class *tegra_camchar_class;

static int tegra_camchar_open(struct inode *in, struct file *f)
{
	int ret;
	unsigned minor = iminor(in);
	struct tegra_ivc_channel *ch;
	struct tegra_camchar_data *data;

	if (minor >= DEVICE_COUNT)
		return -EBADFD;

	ret = mutex_lock_interruptible(&tegra_camchar_lock_open);
	if (ret)
		return ret;

	ch = tegra_camchar_channels[minor];
	if (!ch) {
		pr_alert("camchar: tried to open a device without IVC channel");
		ret = -ENODEV;
		goto open_err;
	}
	data = tegra_ivc_channel_get_drvdata(ch);

	if (data->is_open) {
		mutex_unlock(&tegra_camchar_lock_open);
		ret = -EBUSY;
		goto open_err;
	}
	data->is_open = true;
	f->private_data = ch;
	nonseekable_open(in, f);

open_err:
	mutex_unlock(&tegra_camchar_lock_open);
	return ret;
}

static int tegra_camchar_release(struct inode *in, struct file *fp)
{
	struct tegra_ivc_channel *ch = fp->private_data;
	struct tegra_camchar_data *data;

	data = tegra_ivc_channel_get_drvdata(ch);
	mutex_lock(&tegra_camchar_lock_open);
	data->is_open = false;
	mutex_unlock(&tegra_camchar_lock_open);

	return 0;
}

static unsigned int tegra_camchar_poll(struct file *fp, poll_table *pt)
{
	unsigned int ret = 0;
	struct ivc *ivc = &((struct tegra_ivc_channel *)fp->private_data)->ivc;
	struct tegra_camchar_data *dev_data =
		tegra_ivc_channel_get_drvdata(fp->private_data);

	poll_wait(fp, &dev_data->waitq, pt);

	mutex_lock(&dev_data->io_lock);
	if (tegra_ivc_can_read(ivc))
		ret |= (POLLIN | POLLRDNORM);
	if (tegra_ivc_can_write(ivc))
		ret |= (POLLOUT | POLLWRNORM);
	mutex_unlock(&dev_data->io_lock);

	return ret;
}

static ssize_t tegra_camchar_read(struct file *fp, char __user *buffer, size_t len,
					loff_t *offset)
{
	struct ivc *ivc = &((struct tegra_ivc_channel *)fp->private_data)->ivc;
	struct tegra_camchar_data *dev_data =
		tegra_ivc_channel_get_drvdata(fp->private_data);
	DEFINE_WAIT(wait);
	size_t maxbytes = len > ivc->frame_size ? ivc->frame_size : len;
	ssize_t ret = 0;
	bool done;

	while (!ret && maxbytes) {
		ret = mutex_lock_interruptible(&dev_data->io_lock);
		if (ret)
			return ret;
		prepare_to_wait(&dev_data->waitq, &wait, TASK_INTERRUPTIBLE);

		done = tegra_ivc_can_read(ivc);
		if (done)
			ret = tegra_ivc_read_user(ivc, buffer, maxbytes);
		mutex_unlock(&dev_data->io_lock);

		if (done)
			;
		else if (signal_pending(current)) {
			ret = -EINTR;
			done = true;
		} else if (fp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			done = true;
		} else
			schedule();

		finish_wait(&dev_data->waitq, &wait);
	};

	return ret;
}

static ssize_t tegra_camchar_write(struct file *fp, const char __user *buffer,
					size_t len, loff_t *offset)
{
	struct ivc *ivc = &((struct tegra_ivc_channel *)fp->private_data)->ivc;
	struct tegra_camchar_data *dev_data =
		tegra_ivc_channel_get_drvdata(fp->private_data);
	DEFINE_WAIT(wait);
	size_t maxbytes = len > ivc->frame_size ? ivc->frame_size : len;
	ssize_t ret = 0;
	int done;

	while (!ret && maxbytes) {
		ret = mutex_lock_interruptible(&dev_data->io_lock);
		if (ret)
			return ret;
		prepare_to_wait(&dev_data->waitq, &wait, TASK_INTERRUPTIBLE);

		done = tegra_ivc_can_write(ivc);
		if (done)
			ret = tegra_ivc_write_user(ivc, buffer, maxbytes);
		mutex_unlock(&dev_data->io_lock);

		if (done)
			;
		else if (signal_pending(current)) {
			ret = -EINTR;
			done = true;
		} else if (fp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			done = true;
		} else
			schedule();

		finish_wait(&dev_data->waitq, &wait);
	}

	return ret;
}

static long tegra_camchar_ioctl(struct file *fp, unsigned int cmd,
				unsigned long arg)
{
	long ret;
	int val = 0;
	struct ivc *ivc = &((struct tegra_ivc_channel *)fp->private_data)->ivc;
	struct tegra_camchar_data *dev_data =
		tegra_ivc_channel_get_drvdata(fp->private_data);

	mutex_lock(&dev_data->io_lock);

	switch (cmd) {
	/* generic serial port ioctls */
	case FIONREAD:
		ret = 0;
		if (tegra_ivc_can_read(ivc))
			val = ivc->frame_size;
		ret = put_user(val, (int __user *)arg);
		break;
	/* ioctls specific to this device */
	case CCIOGNFRAMES:
		val = ivc->nframes;
		ret = put_user(val, (int __user *)arg);
		break;
	case CCIOGNBYTES:
		val = ivc->frame_size;
		ret = put_user(val, (int __user *)arg);
		break;

	default:
		ret = -ENOTTY;
	}

	mutex_unlock(&dev_data->io_lock);
	return ret;
}

static const struct file_operations tegra_camchar_fops = {
	.open = tegra_camchar_open,
	.poll = tegra_camchar_poll,
	.read = tegra_camchar_read,
	.write = tegra_camchar_write,
	.release = tegra_camchar_release,
	.unlocked_ioctl = tegra_camchar_ioctl,
	.compat_ioctl = tegra_camchar_ioctl,
	.llseek = no_llseek,
};

static int __init tegra_camchar_init(struct tegra_ivc_driver *drv)
{
	int ret;
	dev_t start;

	ret = alloc_chrdev_region(&start, 0, DEVICE_COUNT, "camchar");
	if (ret) {
		pr_alert("camchar: failed to allocate device numbers\n");
		return ret;
	}
	tegra_camchar_major_number = MAJOR(start);

	tegra_camchar_class = class_create(THIS_MODULE, "camchar_class");
	if (IS_ERR(tegra_camchar_class)) {
		pr_alert("camchar: failed to create class\n");
		ret = PTR_ERR(tegra_camchar_class);
		goto init_err_class;
	}

	ret = tegra_ivc_driver_register(drv);
	if (ret) {
		pr_alert("camchar: ivc driver registration failed\n");
		goto init_err_ivc;
	}

	pr_info("camchar: rtcpu character device driver loaded\n");
	return ret;

init_err_ivc:
	class_destroy(tegra_camchar_class);
init_err_class:
	unregister_chrdev_region(start, DEVICE_COUNT);
	return ret;
}

static void __exit tegra_camchar_exit(struct tegra_ivc_driver *drv)
{
	dev_t num = MKDEV(tegra_camchar_major_number, 0);

	tegra_ivc_driver_unregister(drv);
	class_destroy(tegra_camchar_class);
	unregister_chrdev_region(num, DEVICE_COUNT);
	pr_info("camchar: unloaded rtcpu character device driver\n");
}

static void tegra_camchar_notify(struct tegra_ivc_channel *ch)
{
	struct tegra_camchar_data *dev_data = tegra_ivc_channel_get_drvdata(ch);

	wake_up_interruptible(&dev_data->waitq);
}

static const struct of_device_id camchar_of_match[] = {
	{ .compatible = "nvidia,tegra186-camera-ivc-protocol-echo",
		.data = (void *) DEVICE_ECHO, },
	{ .compatible = "nvidia,tegra186-camera-ivc-protocol-dbg",
		.data = (void *) DEVICE_DBG, },
	{ },
};

static int tegra_camchar_probe(struct tegra_ivc_channel *ch)
{
	const char *tegra_camchar_devnames[] = {"echo", "dbg"};
	const struct of_device_id *of_id =
				of_match_device(camchar_of_match, &ch->dev);
	dev_t num;
	struct tegra_camchar_data *data;
	struct device *dev;
	int ret;

	dev_info(&ch->dev, "probing /dev/camchar-%s",
			tegra_camchar_devnames[(unsigned long)of_id->data]);
	if (!of_id || (unsigned long)of_id->data >= DEVICE_COUNT)
		return -ENODEV;

	data = devm_kzalloc(&ch->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	cdev_init(&data->cdev, &tegra_camchar_fops);
	data->cdev.owner = THIS_MODULE;
	init_waitqueue_head(&data->waitq);
	mutex_init(&data->io_lock);

	tegra_camchar_channels[(unsigned long)of_id->data] = ch;
	tegra_ivc_channel_set_drvdata(ch, data);
	num = MKDEV(tegra_camchar_major_number, (unsigned long)of_id->data);

	ret = cdev_add(&data->cdev, num, 1);
	if (ret) {
		dev_err(&ch->dev, "camchar unable to add character device\n");
		return ret;
	}
	dev = device_create(tegra_camchar_class, &ch->dev, num, NULL,
			"camchar-%s",
			tegra_camchar_devnames[(unsigned long)of_id->data]);
	if (IS_ERR(dev)) {
		dev_err(&ch->dev, "camchar could not create device\n");
		return PTR_ERR(dev);
	}

	return ret;
}

static void tegra_camchar_remove(struct tegra_ivc_channel *ch)
{
	struct tegra_camchar_data *data = tegra_ivc_channel_get_drvdata(ch);
	dev_t num = data->cdev.dev;

	mutex_lock(&tegra_camchar_lock_open);
	device_destroy(tegra_camchar_class, num);
	cdev_del(&data->cdev);
	tegra_camchar_channels[MINOR(num)] = NULL;
	mutex_unlock(&tegra_camchar_lock_open);
}

static const struct tegra_ivc_channel_ops tegra_ivc_channel_chardev_ops = {
	.probe	= tegra_camchar_probe,
	.remove	= tegra_camchar_remove,
	.notify	= tegra_camchar_notify,
};

static struct tegra_ivc_driver camchar_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.bus	= &tegra_ivc_bus_type,
		.name	= "tegra-camera-rtcpu-character-device",
		.of_match_table = camchar_of_match,
	},
	.dev_type	= &tegra_ivc_channel_type,
	.ops.channel	= &tegra_ivc_channel_chardev_ops,
};

module_driver(camchar_driver, tegra_camchar_init, tegra_camchar_exit);
MODULE_AUTHOR("Jan Solanti <jsolanti@nvidia.com>");
MODULE_DESCRIPTION("A Character device for debugging the camrtc");
MODULE_LICENSE("GPL v2");
