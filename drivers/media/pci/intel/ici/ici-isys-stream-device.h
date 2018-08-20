/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/*
 * Copyright (C) 2018 Intel Corporation
 */

#ifndef ICI_ISYS_STREAM_DEVICE_H
#define ICI_ISYS_STREAM_DEVICE_H

#include <linux/ioctl.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <media/ici.h>

#include "ici-isys-frame-buf.h"
#include "ici-isys-pipeline.h"
#include "virtio/intel-ipu4-virtio-common.h"

struct ici_ioctl_ops;
struct ici_frame_plane;

struct ici_stream_device {
	struct device dev;		/* intel stream base dev */
	struct cdev cdev;		/* character device */
	struct device *dev_parent;	/* parent device ipu_bus */
	struct mutex *mutex;
	const struct file_operations *fops; /* standard Linux fops */
	struct ici_isys_frame_buf_list *frame_buf_list;	/* frame buffer wrapper pointer */
	char name[32];			/* device name */
	int minor;			/* driver minor */
	unsigned long flags;		/* stream device state machine */
	const struct ici_ioctl_ops *ipu_ioctl_ops;
	//Mediator param
	int virt_dev_id;
	struct ipu4_virtio_priv *virt_priv;
};

struct ici_ioctl_ops {
	int (*ici_set_format) (struct file *file, void *fh,
		struct ici_stream_format *psf);
	int (*ici_stream_on)	(struct file *file, void *fh);
	int (*ici_stream_off) (struct file *file, void *fh);
	int (*ici_get_buf) (struct file *file, void *fh,
		struct ici_frame_info *fram);
	int (*ici_get_buf_virt) (struct file *file, void *fh,
		struct ici_frame_buf_wrapper *fram, struct page **pages);
	int (*ici_put_buf) (struct file *file, void *fh,
		struct ici_frame_info *fram);
};

#define inode_to_intel_ipu_stream_device(inode) \
	container_of((inode)->i_cdev, struct ici_stream_device, cdev)

int stream_device_register(struct ici_stream_device *strm_dev);

void stream_device_unregister(struct ici_stream_device *strm_dev);

#endif /* ICI_ISYS_STREAM_DEVICE_H */
