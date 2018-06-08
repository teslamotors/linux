// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
/*
 * Copyright (C) 2018 Intel Corporation
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/compat.h>

#include "./ici/ici-isys-stream-device.h"
#include "./ici/ici-isys-pipeline-device.h"

#define MAX_STREAM_DEVICES 64

static dev_t ici_stream_dev_t;
static struct class*  stream_class;
static int stream_devices_registered = 0;
static int stream_device_init = 0;

static int ici_stream_init(void);
static void ici_stream_exit(void);

static int stream_device_open(struct inode *inode, struct file *file)
{
	struct ici_stream_device *strm_dev = inode_to_intel_ipu_stream_device(inode);
	int rval = 0;

	get_device(&strm_dev->dev);

	file->private_data = strm_dev;
	if (strm_dev->fops->open)
		rval = strm_dev->fops->open(inode, file);

	if (rval)
		put_device(&strm_dev->dev);

	return rval;
}

static int stream_device_release(struct inode *inode, struct file *file)
{
	struct ici_stream_device *strm_dev = inode_to_intel_ipu_stream_device(inode);
	int rval = 0;

	if (strm_dev->fops->release)
		rval = strm_dev->fops->release(inode, file);

	put_device(&strm_dev->dev);
	return rval;
}

static unsigned int ici_fop_poll(struct file *file, struct poll_table_struct *poll)
{
	struct ici_stream_device *strm_dev = file->private_data;
	unsigned int rval = POLLERR | POLLHUP;

	if (strm_dev->fops->poll)
		rval = strm_dev->fops->poll(file, poll);
	else
		return DEFAULT_POLLMASK;

	return rval;
}

#ifdef CONFIG_COMPAT
struct timeval32 {
	__u32 tv_sec;
	__u32 tv_usec;
} __attribute__((__packed__));

struct ici_frame_plane32 {
	__u32 bytes_used;
	__u32 length;
	union {
		compat_uptr_t userptr;
		__s32 dmafd;
	} mem;
	__u32 data_offset;
	__u32 reserved[2];
} __attribute__((__packed__));

struct ici_frame_info32 {
	__u32 frame_type;
	__u32 field;
	__u32 flag;
	__u32 frame_buf_id;
	struct timeval32 frame_timestamp;
	__u32 frame_sequence_id;
	__u32 mem_type; /* _DMA or _USER_PTR */
	struct ici_frame_plane32 frame_planes[ICI_MAX_PLANES]; /* multi-planar */
	__u32 num_planes; /* =1 single-planar &gt; 1 multi-planar array size */
	__u32 reserved[2];
} __attribute__((__packed__));

#define ICI_IOC_GET_BUF32 _IOWR(MAJOR_STREAM, 3, struct ici_frame_info32)
#define ICI_IOC_PUT_BUF32 _IOWR(MAJOR_STREAM, 4, struct ici_frame_info32)

static void copy_from_user_frame_info32(struct ici_frame_info *kp, struct ici_frame_info32 __user *up)
{
	int i;
	compat_uptr_t userptr;

	get_user(kp->frame_type, &up->frame_type);
	get_user(kp->field, &up->field);
	get_user(kp->flag, &up->flag);
	get_user(kp->frame_buf_id, &up->frame_buf_id);
	get_user(kp->frame_timestamp.tv_sec, &up->frame_timestamp.tv_sec);
	get_user(kp->frame_timestamp.tv_usec, &up->frame_timestamp.tv_usec);
	get_user(kp->frame_sequence_id, &up->frame_sequence_id);
	get_user(kp->mem_type, &up->mem_type);
	get_user(kp->num_planes, &up->num_planes);
	for (i=0; i<kp->num_planes; i++) {
		get_user(kp->frame_planes[i].bytes_used, &up->frame_planes[i].bytes_used);
		get_user(kp->frame_planes[i].length, &up->frame_planes[i].length);
		if(kp->mem_type==ICI_MEM_USERPTR) {
			get_user(userptr, &up->frame_planes[i].mem.userptr);
			kp->frame_planes[i].mem.userptr = (unsigned long) compat_ptr(userptr);
		} else if (kp->mem_type==ICI_MEM_DMABUF) {
			get_user(kp->frame_planes[i].mem.dmafd, &up->frame_planes[i].mem.dmafd);
		};
		get_user(kp->frame_planes[i].data_offset, &up->frame_planes[i].data_offset);
	}
}

static void copy_to_user_frame_info32(struct ici_frame_info *kp, struct ici_frame_info32 __user *up)
{
	int i;
	compat_uptr_t userptr;

	put_user(kp->frame_type, &up->frame_type);
	put_user(kp->field, &up->field);
	put_user(kp->flag, &up->flag);
	put_user(kp->frame_buf_id, &up->frame_buf_id);
	put_user(kp->frame_timestamp.tv_sec, &up->frame_timestamp.tv_sec);
	put_user(kp->frame_timestamp.tv_usec, &up->frame_timestamp.tv_usec);
	put_user(kp->frame_sequence_id, &up->frame_sequence_id);
	put_user(kp->mem_type, &up->mem_type);
	put_user(kp->num_planes, &up->num_planes);
	for (i=0; i<kp->num_planes; i++) {
		put_user(kp->frame_planes[i].bytes_used, &up->frame_planes[i].bytes_used);
		put_user(kp->frame_planes[i].length, &up->frame_planes[i].length);
		if(kp->mem_type==ICI_MEM_USERPTR) {
			userptr = (unsigned long)compat_ptr(kp->frame_planes[i].mem.userptr);
			put_user(userptr, &up->frame_planes[i].mem.userptr);
		} else if (kp->mem_type==ICI_MEM_DMABUF) {
			get_user(kp->frame_planes[i].mem.dmafd, &up->frame_planes[i].mem.dmafd);
		}
		put_user(kp->frame_planes[i].data_offset, &up->frame_planes[i].data_offset);
	}
}

static long ici_stream_ioctl32(struct file *file, __u32 ioctl_cmd,
			       unsigned long ioctl_arg) {
	union {
		struct ici_frame_info frame_info;
		struct ici_stream_format sf;
	} isys_ioctl_cmd_args;

	int err = 0;
	struct ici_stream_device *strm_dev = file->private_data;
	void __user *up = compat_ptr(ioctl_arg);

	mutex_lock(strm_dev->mutex);

	switch(ioctl_cmd) {
	case ICI_IOC_STREAM_ON:
		err = strm_dev->ipu_ioctl_ops->ici_stream_on(file, strm_dev);
		break;
	case ICI_IOC_STREAM_OFF:
		err = strm_dev->ipu_ioctl_ops->ici_stream_off(file, strm_dev);
		break;
	case ICI_IOC_GET_BUF32:
		copy_from_user_frame_info32(&isys_ioctl_cmd_args.frame_info, up);
		err = strm_dev->ipu_ioctl_ops->ici_get_buf(file, strm_dev, &isys_ioctl_cmd_args.frame_info);
		if (err)
			break;
		copy_to_user_frame_info32(&isys_ioctl_cmd_args.frame_info, up);
		break;
	case ICI_IOC_PUT_BUF32:
		copy_from_user_frame_info32(&isys_ioctl_cmd_args.frame_info, up);
		err = strm_dev->ipu_ioctl_ops->ici_put_buf(file, strm_dev, &isys_ioctl_cmd_args.frame_info);
		if (err)
			break;
		copy_to_user_frame_info32(&isys_ioctl_cmd_args.frame_info, up);
		break;
	case ICI_IOC_SET_FORMAT:
		if (_IOC_SIZE(ioctl_cmd) > sizeof(isys_ioctl_cmd_args))
			return -ENOTTY;

		err = copy_from_user(&isys_ioctl_cmd_args, up,
			_IOC_SIZE(ioctl_cmd));
		if (err)
			return -EFAULT;

		err = strm_dev->ipu_ioctl_ops->ici_set_format(file, strm_dev, &isys_ioctl_cmd_args.sf);
		if (err)
			break;

		err = copy_to_user(up, &isys_ioctl_cmd_args, _IOC_SIZE(ioctl_cmd));
		if (err) {
			return -EFAULT;
		}
		break;
	default:
		err = -ENOTTY;
		break;
	}

	mutex_unlock(strm_dev->mutex);
	if (err) {
		return err;
	}

	return 0;
}
#endif

static long ici_stream_ioctl(struct file *file, unsigned int ioctl_cmd,
			       unsigned long ioctl_arg) {
	union {
		struct ici_frame_info frame_info;
		struct ici_stream_format sf;
	} isys_ioctl_cmd_args;
	int err = 0;
	struct ici_stream_device *strm_dev = file->private_data;
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

	mutex_lock(strm_dev->mutex);

	switch(ioctl_cmd) {
	case ICI_IOC_STREAM_ON:
		err = strm_dev->ipu_ioctl_ops->ici_stream_on(file, strm_dev);
		break;
	case ICI_IOC_STREAM_OFF:
		err = strm_dev->ipu_ioctl_ops->ici_stream_off(file, strm_dev);
		break;
	case ICI_IOC_GET_BUF:
		err = strm_dev->ipu_ioctl_ops->ici_get_buf(file, strm_dev, &isys_ioctl_cmd_args.frame_info);
		break;
	case ICI_IOC_PUT_BUF:
		err = strm_dev->ipu_ioctl_ops->ici_put_buf(file, strm_dev, &isys_ioctl_cmd_args.frame_info);
		break;
	case ICI_IOC_SET_FORMAT:
		err = strm_dev->ipu_ioctl_ops->ici_set_format(file, strm_dev, &isys_ioctl_cmd_args.sf);
		break;
	default:
		err = -ENOTTY;
		break;
	}

	mutex_unlock(strm_dev->mutex);
	if (err)
		return err;

	if (copy && _IOC_DIR(ioctl_cmd) & _IOC_READ) {
		err = copy_to_user(up, &isys_ioctl_cmd_args, _IOC_SIZE(ioctl_cmd));
		if (err)
			return -EFAULT;
	}

	return 0;
}

static const struct file_operations ici_stream_fops = {
	.owner = THIS_MODULE,
	.open = stream_device_open,			/* calls strm_dev->fops->open() */
	.unlocked_ioctl = ici_stream_ioctl,	/* calls strm_dev->ipu_ioctl_ops->() */
#ifdef CONFIG_COMPAT
	.compat_ioctl = ici_stream_ioctl32,
#endif
	.release = stream_device_release,		/* calls strm_dev->fops->release() */
	.poll = ici_fop_poll,		/* calls strm_dev->fops->poll() */
};

/* Called on device_unregister */
static void base_device_release(struct device *sd)
{
}

int stream_device_register(struct ici_stream_device *strm_dev)
{
	int rval = 0;
	int num;

	if (!stream_device_init) {
		rval = ici_stream_init();
		if (rval) {
			printk(KERN_ERR "%s: failed to init stream device\n", __func__);
			return rval;
		}
		stream_device_init++;
	}
	num = stream_devices_registered;

	if (!(num < MAX_STREAM_DEVICES)) {
		printk(KERN_WARNING "%s: wrong minor of stream device: %d\n",
			__func__, num);
		return -EINVAL;
	}
	strm_dev->minor = -1;

	cdev_init(&strm_dev->cdev, &ici_stream_fops);
	strm_dev->cdev.owner = ici_stream_fops.owner;

	rval = cdev_add(&strm_dev->cdev, MKDEV(MAJOR(ici_stream_dev_t), num), 1);
	if (rval) {
		printk(KERN_WARNING "%s: failed to add cdevice\n", __func__);
		return rval;
	}

	strm_dev->dev.class = stream_class;
	strm_dev->dev.devt = MKDEV(MAJOR(ici_stream_dev_t), num);
	strm_dev->dev.parent = strm_dev->dev_parent;
	dev_set_name(&strm_dev->dev, "%s%d", ICI_STREAM_DEVICE_NAME, num);
	rval = device_register(&strm_dev->dev);
	if (rval < 0) {
		printk(KERN_WARNING "%s: device_register failed\n", __func__);
		cdev_del(&strm_dev->cdev);
		return rval;
	}

	/* Release function will be called on device unregister,
	   it is needed to avoid errors */
	strm_dev->dev.release = base_device_release;
	strlcpy(strm_dev->name, strm_dev->dev.kobj.name, sizeof(strm_dev->name));
	strm_dev->minor = num;

	printk(KERN_INFO "Device registered: %s\n", strm_dev->name);
	stream_devices_registered++;

	return 0;
}

void stream_device_unregister(struct ici_stream_device *strm_dev)
{
	device_unregister(&strm_dev->dev);
	cdev_del(&strm_dev->cdev);

	stream_devices_registered--;
        if (!stream_devices_registered) {
                ici_stream_exit();
                stream_device_init--;
        }
}

static int ici_stream_init(void)
{
	int rval;
	ici_stream_dev_t = MKDEV(MAJOR_STREAM, 0);

	rval = register_chrdev_region(ici_stream_dev_t,
				MAX_STREAM_DEVICES, ICI_STREAM_DEVICE_NAME);
	if (rval) {
		printk(KERN_WARNING "can't register intel_ipu_ici stream chrdev region (%d)\n", rval);
		return rval;
	}

	stream_class = class_create(THIS_MODULE, ICI_STREAM_DEVICE_NAME);
	if (IS_ERR(stream_class)) {
		unregister_chrdev_region(ici_stream_dev_t, MAX_STREAM_DEVICES);
		printk(KERN_WARNING "Failed to register device class %s\n", ICI_STREAM_DEVICE_NAME);
		return PTR_ERR(stream_class);
	}

	return 0;
}

static void ici_stream_exit(void)
{
	class_unregister(stream_class);
	//class_destroy(stream_class);
	unregister_chrdev_region(ici_stream_dev_t, MAX_STREAM_DEVICES);

	printk(KERN_INFO "intel_ipu_ici stream device unregistered\n");
}

