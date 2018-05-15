/*
 * Copyright © 2017 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Dongwon Kim <dongwon.kim@intel.com>
 *    Mateusz Polrola <mateuszx.potrola@intel.com>
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/dma-buf.h>
#include "hyper_dmabuf_drv.h"
#include "hyper_dmabuf_ioctl.h"
#include "hyper_dmabuf_list.h"
#include "hyper_dmabuf_id.h"
#include "hyper_dmabuf_event.h"

#ifdef CONFIG_HYPER_DMABUF_XEN
#include "xen/hyper_dmabuf_xen_drv.h"
#elif defined (CONFIG_HYPER_DMABUF_ACRN)
#include "virtio/hyper_dmabuf_virtio_common.h"
#endif

MODULE_LICENSE("GPL and additional rights");
MODULE_AUTHOR("Intel Corporation");

struct hyper_dmabuf_private *hy_drv_priv;

static void force_free(struct exported_sgt_info *exported,
		       void *attr)
{
	struct ioctl_hyper_dmabuf_unexport unexport_attr;
	struct file *filp = (struct file *)attr;

	if (!filp || !exported)
		return;

	if (exported->filp == filp) {
		dev_dbg(hy_drv_priv->dev,
			"Forcefully releasing buffer {id:%d key:%d %d %d}\n",
			 exported->hid.id, exported->hid.rng_key[0],
			 exported->hid.rng_key[1], exported->hid.rng_key[2]);

		unexport_attr.hid = exported->hid;
		unexport_attr.delay_ms = 0;

		hyper_dmabuf_unexport_ioctl(filp, &unexport_attr);
	}
}

static int hyper_dmabuf_open(struct inode *inode, struct file *filp)
{
	int ret = 0;

	/* Do not allow exclusive open */
	if (filp->f_flags & O_EXCL)
		return -EBUSY;

	return ret;
}

static int hyper_dmabuf_release(struct inode *inode, struct file *filp)
{
	hyper_dmabuf_foreach_exported(force_free, filp);

	return 0;
}

#ifdef CONFIG_HYPER_DMABUF_EVENT_GEN

static unsigned int hyper_dmabuf_event_poll(struct file *filp,
				     struct poll_table_struct *wait)
{
	poll_wait(filp, &hy_drv_priv->event_wait, wait);

	if (!list_empty(&hy_drv_priv->event_list))
		return POLLIN | POLLRDNORM;

	return 0;
}

static ssize_t hyper_dmabuf_event_read(struct file *filp, char __user *buffer,
		size_t count, loff_t *offset)
{
	int ret;

	/* only root can read events */
	if (!capable(CAP_DAC_OVERRIDE)) {
		dev_err(hy_drv_priv->dev,
			"Only root can read events\n");
		return -EPERM;
	}

	/* make sure user buffer can be written */
	if (!access_ok(VERIFY_WRITE, buffer, count)) {
		dev_err(hy_drv_priv->dev,
			"User buffer can't be written.\n");
		return -EINVAL;
	}

	ret = mutex_lock_interruptible(&hy_drv_priv->event_read_lock);
	if (ret)
		return ret;

	while (1) {
		struct hyper_dmabuf_event *e = NULL;

		spin_lock_irq(&hy_drv_priv->event_lock);
		if (!list_empty(&hy_drv_priv->event_list)) {
			e = list_first_entry(&hy_drv_priv->event_list,
					struct hyper_dmabuf_event, link);
			list_del(&e->link);
		}
		spin_unlock_irq(&hy_drv_priv->event_lock);

		if (!e) {
			if (ret)
				break;

			if (filp->f_flags & O_NONBLOCK) {
				ret = -EAGAIN;
				break;
			}

			mutex_unlock(&hy_drv_priv->event_read_lock);
			ret = wait_event_interruptible(hy_drv_priv->event_wait,
				  !list_empty(&hy_drv_priv->event_list));

			if (ret == 0)
				ret = mutex_lock_interruptible(
					&hy_drv_priv->event_read_lock);

			if (ret)
				return ret;
		} else {
			unsigned int length = (sizeof(e->event_data.hdr) +
						      e->event_data.hdr.size);

			if (length > count - ret) {
put_back_event:
				spin_lock_irq(&hy_drv_priv->event_lock);
				list_add(&e->link, &hy_drv_priv->event_list);
				spin_unlock_irq(&hy_drv_priv->event_lock);
				break;
			}

			if (copy_to_user(buffer + ret, &e->event_data.hdr,
					 sizeof(e->event_data.hdr))) {
				if (ret == 0)
					ret = -EFAULT;

				goto put_back_event;
			}

			ret += sizeof(e->event_data.hdr);

			if (copy_to_user(buffer + ret, e->event_data.data,
					 e->event_data.hdr.size)) {
				/* error while copying void *data */

				struct hyper_dmabuf_event_hdr dummy_hdr = {0};

				ret -= sizeof(e->event_data.hdr);

				/* nullifying hdr of the event in user buffer */
				if (copy_to_user(buffer + ret, &dummy_hdr,
						 sizeof(dummy_hdr))) {
					dev_err(hy_drv_priv->dev,
						"failed to nullify invalid hdr already in userspace\n");
				}

				ret = -EFAULT;

				goto put_back_event;
			}

			ret += e->event_data.hdr.size;
			hy_drv_priv->pending--;
			kfree(e);
		}
	}

	mutex_unlock(&hy_drv_priv->event_read_lock);

	return ret;
}

#endif

static const struct file_operations hyper_dmabuf_driver_fops = {
	.owner = THIS_MODULE,
	.open = hyper_dmabuf_open,
	.release = hyper_dmabuf_release,

/* poll and read interfaces are needed only for event-polling */
#ifdef CONFIG_HYPER_DMABUF_EVENT_GEN
	.read = hyper_dmabuf_event_read,
	.poll = hyper_dmabuf_event_poll,
#endif

	.unlocked_ioctl = hyper_dmabuf_ioctl,
};

static struct miscdevice hyper_dmabuf_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "hyper_dmabuf",
	.fops = &hyper_dmabuf_driver_fops,
};

static int register_device(void)
{
	int ret = 0;

	ret = misc_register(&hyper_dmabuf_miscdev);

	if (ret) {
		printk(KERN_ERR "hyper_dmabuf: driver can't be registered\n");
		return ret;
	}

	hy_drv_priv->dev = hyper_dmabuf_miscdev.this_device;

	/* TODO: Check if there is a different way to initialize dma mask */
	dma_coerce_mask_and_coherent(hy_drv_priv->dev, DMA_BIT_MASK(64));

	return ret;
}

static void unregister_device(void)
{
	dev_info(hy_drv_priv->dev,
		"hyper_dmabuf: unregister_device() is called\n");

	misc_deregister(&hyper_dmabuf_miscdev);
}

static int __init hyper_dmabuf_drv_init(void)
{
	int ret = 0;

	printk(KERN_NOTICE "hyper_dmabuf_starting: Initialization started\n");

	hy_drv_priv = kcalloc(1, sizeof(struct hyper_dmabuf_private),
			      GFP_KERNEL);

	if (!hy_drv_priv)
		return -ENOMEM;

	ret = register_device();
	if (ret < 0)
		return ret;

/* currently only supports XEN hypervisor */
#ifdef CONFIG_HYPER_DMABUF_XEN
	hy_drv_priv->bknd_ops = &xen_bknd_ops;
#elif defined (CONFIG_HYPER_DMABUF_ACRN)
	hy_drv_priv->bknd_ops = &virtio_bknd_ops;
#else
	hy_drv_priv->bknd_ops = NULL;
	printk(KERN_ERR "No backend configured for hyper_dmabuf in kernel config\n");
#endif

	if (hy_drv_priv->bknd_ops == NULL) {
		printk(KERN_ERR "Hyper_dmabuf: no backend found\n");
		return -1;
	}

	mutex_init(&hy_drv_priv->lock);

	mutex_lock(&hy_drv_priv->lock);

	hy_drv_priv->initialized = false;

	dev_info(hy_drv_priv->dev,
		 "initializing database for imported/exported dmabufs\n");

	hy_drv_priv->work_queue = create_workqueue("hyper_dmabuf_wqueue");

	ret = hyper_dmabuf_table_init();
	if (ret < 0) {
		dev_err(hy_drv_priv->dev,
			"fail to init table for exported/imported entries\n");
		mutex_unlock(&hy_drv_priv->lock);
		kfree(hy_drv_priv);
		return ret;
	}

#ifdef CONFIG_HYPER_DMABUF_SYSFS
	ret = hyper_dmabuf_register_sysfs(hy_drv_priv->dev);
	if (ret < 0) {
		dev_err(hy_drv_priv->dev,
			"failed to initialize sysfs\n");
		mutex_unlock(&hy_drv_priv->lock);
		kfree(hy_drv_priv);
		return ret;
	}
#endif

#ifdef CONFIG_HYPER_DMABUF_EVENT_GEN
	mutex_init(&hy_drv_priv->event_read_lock);
	spin_lock_init(&hy_drv_priv->event_lock);

	/* Initialize event queue */
	INIT_LIST_HEAD(&hy_drv_priv->event_list);
	init_waitqueue_head(&hy_drv_priv->event_wait);

	/* resetting number of pending events */
	hy_drv_priv->pending = 0;
#endif

	if (hy_drv_priv->bknd_ops->init) {
		ret = hy_drv_priv->bknd_ops->init();

		if (ret < 0) {
			dev_dbg(hy_drv_priv->dev,
				"failed to initialize backend.\n");
			return ret;
		}
	}

	hy_drv_priv->domid = hy_drv_priv->bknd_ops->get_vm_id();

	hy_drv_priv->initialized = true;
	if (hy_drv_priv->bknd_ops->init_comm_env) {
		ret = hy_drv_priv->bknd_ops->init_comm_env();
		if (ret < 0) {
			hy_drv_priv->initialized = false;
			dev_dbg(hy_drv_priv->dev,
				"failed to initialize comm-env.\n");
		}
	}

	mutex_unlock(&hy_drv_priv->lock);

	dev_info(hy_drv_priv->dev,
		"Finishing up initialization of hyper_dmabuf drv\n");

	/* interrupt for comm should be registered here: */
	return ret;
}

static void hyper_dmabuf_drv_exit(void)
{
#ifdef CONFIG_HYPER_DMABUF_SYSFS
	hyper_dmabuf_unregister_sysfs(hy_drv_priv->dev);
#endif

	mutex_lock(&hy_drv_priv->lock);

	/* hash tables for export/import entries and ring_infos */
	hyper_dmabuf_table_destroy();

	if (hy_drv_priv->bknd_ops->destroy_comm) {
		hy_drv_priv->bknd_ops->destroy_comm();
	}

	if (hy_drv_priv->bknd_ops->cleanup) {
		hy_drv_priv->bknd_ops->cleanup();
	};

	/* destroy workqueue */
	if (hy_drv_priv->work_queue)
		destroy_workqueue(hy_drv_priv->work_queue);

	/* destroy id_queue */
	if (hy_drv_priv->id_queue)
		hyper_dmabuf_free_hid_list();

#ifdef CONFIG_HYPER_DMABUF_EVENT_GEN
	/* clean up event queue */
	hyper_dmabuf_events_release();
#endif

	mutex_unlock(&hy_drv_priv->lock);

	dev_info(hy_drv_priv->dev,
		 "hyper_dmabuf driver: Exiting\n");

	kfree(hy_drv_priv);

	unregister_device();
}

module_init(hyper_dmabuf_drv_init);
module_exit(hyper_dmabuf_drv_exit);
