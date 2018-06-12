/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/*
 * Copyright (C) 2018 Intel Corporation
 */

#ifndef ICI_ISYS_PIPELINE_DEVICE_H
#define ICI_ISYS_PIPELINE_DEVICE_H

#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/list.h>

struct ici_pipeline_ioctl_ops;
struct ici_link_desc;
struct ici_pad_supported_format_desc;

struct ici_isys_pipeline_device {
	struct cdev cdev;
	struct device dev;
	struct device *parent;
	int minor;
	char name[32];
	struct mutex mutex;
	const struct file_operations *fops;
	struct list_head nodes;
	const struct ici_pipeline_ioctl_ops *pipeline_ioctl_ops;
	unsigned next_node_id;
};

/* Pipeline IOCTLs */
struct ici_pipeline_ioctl_ops {
	int (*pipeline_enum_nodes)(struct file *file, void *fh,
			struct ici_node_desc *node_desc);
	int (*pipeline_enum_links)(struct file *file, void *fh,
			struct ici_links_query *links_query);
	int (*pipeline_setup_pipe)(struct file *file, void *fh,
			struct ici_link_desc *link);
	int (*pad_set_ffmt)(struct file *file, void *fh,
			struct ici_pad_framefmt* pad_ffmt);
	int (*pad_get_ffmt)(struct file *file, void *fh,
			struct ici_pad_framefmt* pad_ffmt);
	int (*pad_get_supported_format)(struct file *file, void *fh,
			struct ici_pad_supported_format_desc *format_desc);
	int (*pad_set_sel)(struct file *file, void *fh,
			struct ici_pad_selection* pad_sel);
	int (*pad_get_sel)(struct file *file, void *fh,
			struct ici_pad_selection* pad_sel);
};

int pipeline_device_register(
			struct ici_isys_pipeline_device *pipe_dev,
			struct ici_isys *isys);
void pipeline_device_unregister(struct ici_isys_pipeline_device
								*pipe_dev);

#define inode_to_ici_isys_pipeline_device(inode) \
	container_of((inode)->i_cdev,\
	struct ici_isys_pipeline_device, cdev)

#endif /*ICI_ISYS_PIPELINE_DEVICE_H */
