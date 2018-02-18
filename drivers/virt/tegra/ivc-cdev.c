/*
 * IVC character device driver
 *
 * Copyright (C) 2014-2016, NVIDIA CORPORATION. All rights reserved.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2.  This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 *
 */

#include <linux/tegra-ivc.h>
#include <linux/tegra-ivc-instance.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/workqueue.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/sched.h>

#include "tegra_hv.h"

#define ERR(...) pr_err("ivc: " __VA_ARGS__)
#define DBG(...) pr_debug("ivc: " __VA_ARGS__)

struct ivc_dev {
	int			minor;
	dev_t			dev;
	struct cdev		cdev;
	struct device		*device;
	char			name[32];

	/* channel configuration */
	struct tegra_hv_ivc_cookie *ivck;
	const struct tegra_hv_queue_data *qd;

	/* File mode */
	wait_queue_head_t	wq;
	/*
	 * Lock for synchronizing access to the IVC channel between the threaded
	 * IRQ handler's notification processing and file ops.
	 */
	struct mutex		file_lock;
};

static dev_t ivc_dev;
static const struct ivc_info_page *info;

static irqreturn_t ivc_dev_handler(int irq, void *data)
{
	struct ivc_dev *ivc = data;

	BUG_ON(!ivc->ivck);

	mutex_lock(&ivc->file_lock);
	tegra_ivc_channel_notified(tegra_hv_ivc_convert_cookie(ivc->ivck));
	mutex_unlock(&ivc->file_lock);

	/* simple implementation, just kick all waiters */
	wake_up_interruptible_all(&ivc->wq);

	return IRQ_HANDLED;
}

static irqreturn_t ivc_threaded_irq_handler(int irq, void *dev_id)
{
	/*
	 * Virtual IRQs are known to be edge-triggered, so no action is needed
	 * to acknowledge them.
	 */
	return IRQ_WAKE_THREAD;
}

static int ivc_dev_open(struct inode *inode, struct file *filp)
{
	struct cdev *cdev = inode->i_cdev;
	struct ivc_dev *ivc = container_of(cdev, struct ivc_dev, cdev);
	int ret;
	struct tegra_hv_ivc_cookie *ivck;
	struct ivc *ivcq;

	/*
	 * If we can reserve the corresponding IVC device successfully, then
	 * we have exclusive access to the ivc device.
	 */
	ivck = tegra_hv_ivc_reserve(NULL, ivc->minor, NULL);
	if (IS_ERR(ivck))
		return PTR_ERR(ivck);

	ivc->ivck = ivck;
	ivcq = tegra_hv_ivc_convert_cookie(ivck);

	mutex_lock(&ivc->file_lock);
	tegra_ivc_channel_reset(ivcq);
	mutex_unlock(&ivc->file_lock);

	/* request our irq */
	ret = devm_request_threaded_irq(ivc->device, ivc->qd->irq,
			ivc_threaded_irq_handler, ivc_dev_handler, 0,
			dev_name(ivc->device), ivc);
	if (ret < 0) {
		dev_err(ivc->device, "Failed to request irq %d\n",
				ivc->qd->irq);
		ivc->ivck = NULL;
		tegra_hv_ivc_unreserve(ivck);
		return ret;
	}

	/* all done */
	filp->private_data = ivc;

	return 0;
}

static int ivc_dev_release(struct inode *inode, struct file *filp)
{
	struct ivc_dev *ivc = filp->private_data;
	struct tegra_hv_ivc_cookie *ivck;

	filp->private_data = NULL;

	BUG_ON(!ivc);

	free_irq(ivc->qd->irq, ivc);

	ivck = ivc->ivck;
	ivc->ivck = NULL;
	/*
	 * Unreserve after clearing ivck; we no longer have exclusive
	 * access at this point.
	 */
	tegra_hv_ivc_unreserve(ivck);

	return 0;
}

static ssize_t ivc_dev_read(struct file *filp, char __user *buf,
		size_t count, loff_t *ppos)
{
	struct ivc_dev *ivcd = filp->private_data;
	struct ivc *ivc;
	int left = count, ret = 0, chunk;

	BUG_ON(!ivcd);
	ivc = tegra_hv_ivc_convert_cookie(ivcd->ivck);

	if (!tegra_ivc_can_read(ivc)) {
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;

		ret = wait_event_interruptible(ivcd->wq,
				tegra_ivc_can_read(ivc));
		if (ret)
			return ret;
	}

	while (left > 0 && tegra_ivc_can_read(ivc)) {

		chunk = ivcd->qd->frame_size;
		if (chunk > left)
			chunk = left;
		mutex_lock(&ivcd->file_lock);
		ret = tegra_ivc_read_user(ivc, buf, chunk);
		mutex_unlock(&ivcd->file_lock);
		if (ret < 0)
			break;

		buf += chunk;
		left -= chunk;
	}

	if (left >= count)
		return ret;

	return count - left;
}

static ssize_t ivc_dev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *pos)
{
	struct ivc_dev *ivcd = filp->private_data;
	struct ivc *ivc;
	ssize_t done;
	size_t left, chunk;
	int ret = 0;

	BUG_ON(!ivcd);
	ivc = tegra_hv_ivc_convert_cookie(ivcd->ivck);

	done = 0;
	while (done < count) {

		left = count - done;

		if (left < ivcd->qd->frame_size)
			chunk = left;
		else
			chunk = ivcd->qd->frame_size;

		/* is queue full? */
		if (!tegra_ivc_can_write(ivc)) {

			/* check non-blocking mode */
			if (filp->f_flags & O_NONBLOCK) {
				ret = -EAGAIN;
				break;
			}

			ret = wait_event_interruptible(ivcd->wq,
					tegra_ivc_can_write(ivc));
			if (ret)
				break;
		}

		mutex_lock(&ivcd->file_lock);
		ret = tegra_ivc_write_user(ivc, buf, chunk);
		mutex_unlock(&ivcd->file_lock);
		if (ret < 0)
			break;

		buf += chunk;

		done += chunk;
		*pos += chunk;
	}


	if (done == 0)
		return ret;

	return done;
}

static unsigned int ivc_dev_poll(struct file *filp, poll_table *wait)
{
	struct ivc_dev *ivcd = filp->private_data;
	struct ivc *ivc;
	int mask = 0;

	BUG_ON(!ivcd);
	ivc = tegra_hv_ivc_convert_cookie(ivcd->ivck);

	poll_wait(filp, &ivcd->wq, wait);

	if (tegra_ivc_can_read(ivc))
		mask = POLLIN | POLLRDNORM;

	if (tegra_ivc_can_write(ivc))
		mask |= POLLOUT | POLLWRNORM;

	/* no exceptions */

	return mask;
}

static const struct file_operations ivc_fops = {
	.owner		= THIS_MODULE,
	.open		= ivc_dev_open,
	.release	= ivc_dev_release,
	.llseek		= noop_llseek,
	.read		= ivc_dev_read,
	.write		= ivc_dev_write,
	.poll		= ivc_dev_poll,
};

static ssize_t id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ivc_dev *ivc = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", ivc->qd->id);
}

static ssize_t frame_size_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ivc_dev *ivc = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", ivc->qd->frame_size);
}

static ssize_t nframes_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ivc_dev *ivc = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", ivc->qd->nframes);
}

static ssize_t reserved_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tegra_hv_ivc_cookie *ivck;
	struct ivc_dev *ivc = dev_get_drvdata(dev);
	int reserved;

	ivck = tegra_hv_ivc_reserve(NULL, ivc->minor, NULL);
	if (IS_ERR(ivck))
		reserved = 1;
	else {
		tegra_hv_ivc_unreserve(ivck);
		reserved = 0;
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", reserved);
}

static ssize_t peer_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ivc_dev *ivc = dev_get_drvdata(dev);
	int guestid = tegra_hv_get_vmid();

	return snprintf(buf, PAGE_SIZE, "%d\n", ivc->qd->peers[0] == guestid
			? ivc->qd->peers[1] : ivc->qd->peers[0]);
}

static DEVICE_ATTR_RO(id);
static DEVICE_ATTR_RO(frame_size);
static DEVICE_ATTR_RO(nframes);
static DEVICE_ATTR_RO(reserved);
static DEVICE_ATTR_RO(peer);

struct attribute *ivc_attrs[] = {
	&dev_attr_id.attr,
	&dev_attr_frame_size.attr,
	&dev_attr_nframes.attr,
	&dev_attr_peer.attr,
	&dev_attr_reserved.attr,
	NULL,
};
ATTRIBUTE_GROUPS(ivc);

static dev_t ivc_dev;
static uint32_t max_qid;
static struct ivc_dev *ivc_dev_array;
static struct class *ivc_class;

static int __init add_ivc(int i)
{
	const struct tegra_hv_queue_data *qd = &ivc_info_queue_array(info)[i];
	struct ivc_dev *ivc = &ivc_dev_array[i];
	int ret;

	ivc->minor = qd->id;
	ivc->dev = MKDEV(MAJOR(ivc_dev), qd->id);
	ivc->qd = qd;

	cdev_init(&ivc->cdev, &ivc_fops);
	snprintf(ivc->name, sizeof(ivc->name) - 1, "ivc%d", qd->id);
	ret = cdev_add(&ivc->cdev, ivc->dev, 1);
	if (ret != 0) {
		ERR("cdev_add() failed\n");
		return ret;
	}

	mutex_init(&ivc->file_lock);
	init_waitqueue_head(&ivc->wq);

	/* parent is this hvd dev */
	ivc->device = device_create(ivc_class, NULL, ivc->dev, ivc,
			ivc->name);
	if (IS_ERR(ivc->device)) {
		ERR("device_create() failed for %s\n", ivc->name);
		return PTR_ERR(ivc->device);
	}
	/* point to ivc */
	dev_set_drvdata(ivc->device, ivc);

	return 0;
}

static int __init setup_ivc(void)
{
	uint32_t i;
	int result;

	max_qid = 0;
	for (i = 0; i < info->nr_queues; i++) {
		const struct tegra_hv_queue_data *qd =
				&ivc_info_queue_array(info)[i];
		if (qd->id > max_qid)
			max_qid = qd->id;
	}

	/* allocate the whole chardev range */
	result = alloc_chrdev_region(&ivc_dev, 0, max_qid, "ivc");
	if (result < 0) {
		ERR("alloc_chrdev_region() failed\n");
		return result;
	}

	ivc_class = class_create(THIS_MODULE, "ivc");
	if (IS_ERR(ivc_class)) {
		ERR("failed to create ivc class: %ld\n", PTR_ERR(ivc_class));
		return PTR_ERR(ivc_class);
	}
	ivc_class->dev_groups = ivc_groups;

	ivc_dev_array = kcalloc(info->nr_queues, sizeof(*ivc_dev_array),
			GFP_KERNEL);
	if (!ivc_dev_array) {
		ERR("failed to allocate ivc_dev_array");
		return -ENOMEM;
	}

	/*
	 * Make a second pass through the queues to instantiate the char devs
	 * corresponding to existent queues.
	 */
	for (i = 0; i < info->nr_queues; i++) {
		result = add_ivc(i);
		if (result != 0)
			return result;
	}

	return 0;
}

static void __init cleanup_ivc(void)
{
	uint32_t i;

	if (ivc_dev_array) {
		for (i = 0; i < info->nr_queues; i++) {
			struct ivc_dev *ivc = &ivc_dev_array[i];

			if (ivc->device) {
				cdev_del(&ivc->cdev);
				device_del(ivc->device);
			}
		}
		kfree(ivc_dev_array);
		ivc_dev_array = NULL;
	}

	if (!IS_ERR_OR_NULL(ivc_class)) {
		class_destroy(ivc_class);
		ivc_class = NULL;
	}

	if (ivc_dev) {
		unregister_chrdev_region(ivc_dev, max_qid);
		ivc_dev = 0;
	}
}

static int __init ivc_init(void)
{
	int result;

	info = tegra_hv_get_ivc_info();
	if (IS_ERR(info))
		return -ENODEV;

	result = setup_ivc();
	if (result != 0)
		cleanup_ivc();

	return result;
}

module_init(ivc_init);
