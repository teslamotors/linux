// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
/*
 * Copyright (C) 2018 Intel Corporation
 */


#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/kthread.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>

#include "./ici/ici-isys-stream.h"
#include "intel-ipu4-virtio-common.h"
#include "intel-ipu4-virtio-bridge.h"

#define MAX_STREAM_DEVICES 64

static dev_t virt_stream_dev_t;
static struct class *virt_stream_class;
static int virt_stream_devs_registered;
static int stream_dev_init;
static struct ici_stream_device *strm_dev;



static int ici_isys_set_format(struct file *file, void *fh,
	struct ici_stream_format *sf)
{
	struct ici_stream_device *dev = fh;
	struct ipu4_virtio_priv *fe_priv = dev->virt_priv;
	struct ipu4_virtio_req *req;
	int rval = 0;
	int op[10];

	if (!fe_priv)
		return -EINVAL;

	req = kcalloc(1, sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;
	op[0] = 0;
	op[1] = 0;
	op[2] = sf->ffmt.width;
	op[3] =	sf->ffmt.height;
	op[4] = sf->ffmt.pixelformat;
	op[5] = sf->ffmt.field;
	op[6] = sf->ffmt.colorspace;
	op[7] = 0;
	op[8] = 0;
	op[9] = 0;

	intel_ipu4_virtio_create_req(req, IPU4_CMD_SET_FORMAT, &op[0]);

	rval = fe_priv->bknd_ops->send_req(fe_priv->domid, req, true);
	if (rval) {
		printk(KERN_ERR "Failed to open virtual device\n");
		kfree(req);
		return rval;
	}
	kfree(req);

	return rval;
}

static int ici_isys_stream_on(struct file *file, void *fh)
{
	int rval = 0;

	return rval;
}

static int ici_isys_stream_off(struct file *file, void *fh)
{
	int rval = 0;

	return rval;
}

static int ici_isys_getbuf(struct file *file, void *fh,
	struct ici_frame_info *user_frame_info)
{
	int rval = 0;

	return rval;
}

static int ici_isys_putbuf(struct file *file, void *fh,
	struct ici_frame_info *user_frame_info)
{
	int rval = 0;

	return rval;
}

static unsigned int stream_fop_poll(struct file *file,
	struct poll_table_struct *poll)
{
	int rval = 0;

	return rval;
}

static int virt_stream_fop_open(struct inode *inode, struct file *file)
{
	struct ici_stream_device *dev = inode_to_intel_ipu_stream_device(inode);
	struct ipu4_virtio_req *req;
	struct ipu4_virtio_priv *fe_priv = dev->virt_priv;
	int rval = 0;
	int op[2];
	printk(KERN_INFO "virt stream open\n");
	get_device(&dev->dev);

	file->private_data = dev;

	if (!fe_priv)
		return -EINVAL;

	req = kcalloc(1, sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	op[0] = dev->virt_dev_id;
	op[1] = 0;

	intel_ipu4_virtio_create_req(req, IPU4_CMD_DEVICE_OPEN, &op[0]);

	rval = fe_priv->bknd_ops->send_req(fe_priv->domid, req, true);
	if (rval) {
		printk(KERN_ERR "Failed to open virtual device\n");
		kfree(req);
		return rval;
	}
	kfree(req);

	return rval;
}

static int virt_stream_fop_release(struct inode *inode, struct file *file)
{
	int rval = 0;

	return rval;
}

static unsigned int virt_stream_fop_poll(struct file *file,
	struct poll_table_struct *poll)
{
	struct ici_stream_device *as = file->private_data;
	unsigned int res = POLLERR | POLLHUP;

	printk(KERN_NOTICE "virt_stream_fop_poll for:%s\n", as->name);

	res = stream_fop_poll(file, poll);

	res = POLLIN;

	printk(KERN_NOTICE "virt_stream_fop_poll res %u\n", res);

	return res;
}

static long virt_stream_ioctl(struct file *file,
				unsigned int ioctl_cmd,
				unsigned long ioctl_arg)
{
	union {
			struct ici_frame_info frame_info;
			struct ici_stream_format sf;
		} isys_ioctl_cmd_args;
		int err = 0;
		struct ici_stream_device *dev = file->private_data;
		void __user *up = (void __user *)ioctl_arg;

		bool copy = (ioctl_cmd != ICI_IOC_STREAM_ON &&
				ioctl_cmd != ICI_IOC_STREAM_OFF);

		if (copy) {
			if (_IOC_SIZE(ioctl_cmd) > sizeof(isys_ioctl_cmd_args))
				return -ENOTTY;

			if (_IOC_DIR(ioctl_cmd) & _IOC_WRITE) {
				err = copy_from_user(&isys_ioctl_cmd_args, up,
					_IOC_SIZE(ioctl_cmd));
				if (err)
					return -EFAULT;
			}
		}

		//mutex_lock(dev->mutex);
		switch (ioctl_cmd) {
		case ICI_IOC_STREAM_ON:
			printk(KERN_INFO "IPU FE IOCTL STREAM_ON\n");
			err = ici_isys_stream_on(file, dev);
			break;
		case ICI_IOC_STREAM_OFF:
			printk(KERN_INFO "IPU FE IOCTL STREAM_OFF\n");
			err = ici_isys_stream_off(file, dev);
			break;
		case ICI_IOC_GET_BUF:
			printk(KERN_INFO "IPU FE IOCTL GET_BUF\n");
			err = ici_isys_getbuf(file, dev, &isys_ioctl_cmd_args.frame_info);
			break;
		case ICI_IOC_PUT_BUF:
			printk(KERN_INFO "IPU FE IOCTL PUT_BUF\n");
			err = ici_isys_putbuf(file, dev, &isys_ioctl_cmd_args.frame_info);
			break;
		case ICI_IOC_SET_FORMAT:
			printk(KERN_INFO "IPU FE IOCTL SET_FORMAT\n");
			err = ici_isys_set_format(file, dev, &isys_ioctl_cmd_args.sf);
			break;

		default:
			err = -ENOTTY;
			break;
		}

		//mutex_unlock(dev->mutex);
	return 0;
}


static const struct file_operations virt_stream_fops = {
	.owner = THIS_MODULE,
	.open = virt_stream_fop_open,			/* calls strm_dev->fops->open() */
	.unlocked_ioctl = virt_stream_ioctl,	/* calls strm_dev->ipu_ioctl_ops->() */
	.release = virt_stream_fop_release,		/* calls strm_dev->fops->release() */
	.poll = virt_stream_fop_poll,		/* calls strm_dev->fops->poll() */
};

/* Called on device_unregister */
static void base_device_release(struct device *sd)
{
}

static int virt_ici_stream_init(void)
{
	int rval;
	int num;
	struct ipu4_virtio_priv *fe_priv;
	printk(KERN_NOTICE "Initializing Para virt IPU FE\n");
	if (!stream_dev_init) {
		virt_stream_dev_t = MKDEV(MAJOR_STREAM, 0);

		rval = register_chrdev_region(virt_stream_dev_t,
		MAX_STREAM_DEVICES, ICI_STREAM_DEVICE_NAME);
		if (rval) {
			printk(
					KERN_WARNING "can't register virt_ici stream chrdev region (%d)\n",
					rval);
			return rval;
		}

		virt_stream_class = class_create(THIS_MODULE, ICI_STREAM_DEVICE_NAME);
		if (IS_ERR(virt_stream_class)) {
			unregister_chrdev_region(virt_stream_dev_t, MAX_STREAM_DEVICES);
			printk(KERN_WARNING "Failed to register device class %s\n",
					ICI_STREAM_DEVICE_NAME);
			return PTR_ERR(virt_stream_class);
		}
		stream_dev_init++;
	}
	strm_dev = kzalloc(sizeof(*strm_dev), GFP_KERNEL);
	if (!strm_dev)
		return -ENOMEM;
	num = virt_stream_devs_registered;
	strm_dev->minor = -1;
	cdev_init(&strm_dev->cdev, &virt_stream_fops);
	strm_dev->cdev.owner = virt_stream_fops.owner;

	rval = cdev_add(&strm_dev->cdev, MKDEV(MAJOR(virt_stream_dev_t), num), 1);
	if (rval) {
			printk(KERN_WARNING "%s: failed to add cdevice\n", __func__);
			return rval;
	}

	strm_dev->dev.class = virt_stream_class;
	strm_dev->dev.devt = MKDEV(MAJOR(virt_stream_dev_t), num);
	dev_set_name(&strm_dev->dev, "%s%d", ICI_STREAM_DEVICE_NAME, num);

	rval = device_register(&strm_dev->dev);
	if (rval < 0) {
		printk(KERN_WARNING "%s: device_register failed\n", __func__);
		cdev_del(&strm_dev->cdev);
		return rval;
	}
	strm_dev->dev.release = base_device_release;
	strlcpy(strm_dev->name, strm_dev->dev.kobj.name, sizeof(strm_dev->name));
	strm_dev->minor = num;
	strm_dev->virt_dev_id = num;

	//mutex_init(strm_dev->mutex);
	//virt_stream_devs_registered++;

	fe_priv = kcalloc(1, sizeof(struct ipu4_virtio_priv),
					      GFP_KERNEL);

	if (!fe_priv)
		return -ENOMEM;

	fe_priv->bknd_ops = &ipu4_virtio_bknd_ops;

	if (fe_priv->bknd_ops->init) {
		rval = fe_priv->bknd_ops->init();
		if (rval < 0) {
			printk(KERN_NOTICE
				"failed to initialize backend.\n");
			return rval;
		}
	}

	fe_priv->domid = fe_priv->bknd_ops->get_vm_id();
	strm_dev->virt_priv = fe_priv;
	printk("FE registered with domid:%d\n", fe_priv->domid);

	return 0;
}

static void virt_ici_stream_exit(void)
{
	class_unregister(virt_stream_class);
	unregister_chrdev_region(virt_stream_dev_t, MAX_STREAM_DEVICES);

	printk(KERN_INFO "virt_ici stream device unregistered\n");
}

static int __init virt_ici_init(void)
{
	return virt_ici_stream_init();
}

static void __exit virt_ici_exit(void)
{
	virt_ici_stream_exit();
}

module_init(virt_ici_init);
module_exit(virt_ici_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Intel IPU Para virtualize ici input system driver");
MODULE_AUTHOR("Kushal Bandi <kushal.bandi@intel.com>");


