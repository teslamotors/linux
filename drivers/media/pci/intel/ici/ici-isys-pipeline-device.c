// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
/*
 * Copyright (C) 2018 Intel Corporation
 */

#include "./ici/ici-isys.h"

#ifdef ICI_ENABLED

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/compat.h>

#include "./ici/ici-isys-pipeline-device.h"
#include "./ici/ici-isys-pipeline.h"

static struct class *pipeline_class;

static struct ici_isys_node* find_node(
	struct ici_isys_pipeline_device *pipe_dev,
	unsigned id);

static int pipeline_device_open(struct inode *inode, struct file *file)
{
	struct ici_isys_pipeline_device *pipe_dev =
	    inode_to_ici_isys_pipeline_device(inode);
	int rval = 0;

	file->private_data = pipe_dev;

	get_device(&pipe_dev->dev);

	DEBUGK("pipeline_device_open\n");

	return rval;
}

static int pipeline_device_release(struct inode *inode,
	struct file *file)
{
	struct ici_isys_pipeline_device *pipe_dev =
	    inode_to_ici_isys_pipeline_device(inode);

	put_device(&pipe_dev->dev);

	DEBUGK("pipeline_device_release\n");

	return 0;
}

static int pipeline_enum_links(struct file *file, void *fh,
	struct ici_links_query *links_query)
{
	struct ici_isys_node *node;
	struct node_pipe* pipe;
	struct node_pad* pad;
	struct ici_isys_pipeline_device *pipe_dev =
			file->private_data;
	struct ici_link_desc* link;

	node = find_node(pipe_dev, links_query->pad.node_id);
	if (!node)
		return -ENODEV;
	if (links_query->pad.pad_idx >= node->nr_pads)
		return -EINVAL;

	pad = &node->node_pad[links_query->pad.pad_idx];
	if (pad->pad_id != links_query->pad.pad_idx)
		return -EINVAL;

	links_query->links_cnt = 0;

	list_for_each_entry(pipe, &node->node_pipes, list_entry) {
		if (pipe->src_pad != pad && pipe->sink_pad != pad)
			continue;
		link = &links_query->links[links_query->links_cnt];
		link->source.node_id = pipe->src_pad->node->node_id;
		link->source.pad_idx = pipe->src_pad->pad_id;
		link->source.flags = pipe->src_pad->flags;
		link->sink.node_id = pipe->sink_pad->node->node_id;
		link->sink.pad_idx = pipe->sink_pad->pad_id;
		link->sink.flags = pipe->sink_pad->flags;
		link->flags = pipe->flags;
		++links_query->links_cnt;
		if (WARN_ON(links_query->links_cnt >=
			ICI_MAX_LINKS)) {
			dev_warn(&pipe_dev->dev,
				"Too many links defined. %d\n",
				links_query->links_cnt);
			break;
		}
	}
	return 0;
}

static int pipeline_enum_nodes(struct file *file, void *fh,
	struct ici_node_desc *node_desc)
{
	struct ici_isys_pipeline_device* pipeline_dev =
		file->private_data;
	struct ici_isys_node *node;
	struct ici_pad_desc* pad_desc;
	int pad;
	bool found = false;

	node_desc->node_count = 0;
	list_for_each_entry(node, &pipeline_dev->nodes, node_entry) {
		node_desc->node_count++;
		if (node_desc->node_id != node->node_id)
			continue;

		/* fill out the node data */
		found = true;
		memcpy(node_desc->name, node->name,
				sizeof(node_desc->name));
		node_desc->nr_pads = node->nr_pads;
		for (pad=0; pad < node->nr_pads; pad++) {
			pad_desc = &node_desc->node_pad[pad];
			pad_desc->pad_idx = node->node_pad[pad].pad_id;
			pad_desc->node_id = node->node_id;
			pad_desc->flags = node->node_pad[pad].flags;
		}
	}
	if (node_desc->node_id == -1)
		return 0;
	if (!found)
		return -ENODEV;
	return 0;
}

static struct ici_isys_node* find_node(
	struct ici_isys_pipeline_device *pipe_dev,
	unsigned id)
{
	struct ici_isys_node *ici_node;

	list_for_each_entry(ici_node, &pipe_dev->nodes, node_entry) {
		if (ici_node->node_id == id)
			return ici_node;
	}
	return NULL;
}

static int ici_pipeline_get_supported_format(struct file *file,
	void *fh,
	struct ici_pad_supported_format_desc *format_desc)
{
	struct ici_isys_node *node;
	struct ici_isys_pipeline_device *pipe_dev =
			file->private_data;

	node = find_node(pipe_dev, format_desc->pad.node_id);
	if (!node)
		return -ENODEV;

	if (node->node_get_pad_supported_format)
		return node->node_get_pad_supported_format(node,
			format_desc);
	return -ENODEV;
}

static struct node_pipe* find_pipe(
			struct ici_isys_node* src_node,
			struct ici_link_desc *link)
{
	struct node_pipe *np;

	list_for_each_entry(np, &src_node->node_pipes, list_entry) {
		if (np->src_pad->node->node_id == link->source.node_id
			&& np->src_pad->pad_id == link->source.pad_idx
			&& np->sink_pad->node->node_id ==
				link->sink.node_id
			&& np->sink_pad->pad_id == link->sink.pad_idx)

			return np;
	}

	return NULL;
}

static int ici_setup_link(struct file *file, void *fh,
				struct ici_link_desc *link)
{
	int rval = 0;
	struct ici_isys_pipeline_device *pipe_dev =
			file->private_data;
	struct ici_isys_node *src_node, *sink_node;
	struct node_pipe *np;

	src_node = find_node(pipe_dev, link->source.node_id);
	if (!src_node)
		return -ENODEV;

	sink_node = find_node(pipe_dev, link->sink.node_id);
	if (!sink_node)
		return -ENODEV;

	np = find_pipe(src_node, link);

	if (np) {
		np->flags = link->flags;
	} else {
		dev_warn(&pipe_dev->dev, "Link not found\n");
		return -ENODEV;
	}

	np = find_pipe(sink_node, link);
	if (np)
		np->flags = link->flags | ICI_LINK_FLAG_BACKLINK;
	else
		dev_warn(&pipe_dev->dev, "Backlink not found\n");

	return rval;
}

int ici_pipeline_set_ffmt(struct file *file, void *fh,
	struct ici_pad_framefmt *ffmt)
{
	struct ici_isys_pipeline_device *pipe_dev =
			file->private_data;
	struct ici_isys_node *node;
	int rval = -ENODEV;

	node = find_node(pipe_dev, ffmt->pad.node_id);
	if (!node)
		return -ENODEV;

	if (node->node_set_pad_ffmt)
		rval = node->node_set_pad_ffmt(node, ffmt);

	return rval;
}

int ici_pipeline_get_ffmt(struct file *file, void *fh,
	struct ici_pad_framefmt *ffmt)
{
	struct ici_isys_pipeline_device *pipe_dev =
			file->private_data;
	struct ici_isys_node *node;
	int rval = -ENODEV;

	node = find_node(pipe_dev, ffmt->pad.node_id);
	if (!node)
		return -ENODEV;

	if (node->node_get_pad_ffmt)
		rval = node->node_get_pad_ffmt(node, ffmt);

	return rval;
}

static int ici_pipeline_set_sel(struct file *file, void *fh,
				struct ici_pad_selection *pad_sel)
{
	struct ici_isys_node *node;
	struct ici_isys_pipeline_device *pipe_dev =
			file->private_data;

	node = find_node(pipe_dev, pad_sel->pad.node_id);
	if (!node)
		return -ENODEV;

	if (node->node_set_pad_sel)
		return node->node_set_pad_sel(node, pad_sel);
	return -ENODEV;
}

static int ici_pipeline_get_sel(struct file *file, void *fh,
				struct ici_pad_selection *pad_sel)
{
	struct ici_isys_node *node;
	struct ici_isys_pipeline_device *pipe_dev =
			file->private_data;

	node = find_node(pipe_dev, pad_sel->pad.node_id);
	if (!node)
		return -ENODEV;

	if (node->node_get_pad_sel)
		return node->node_get_pad_sel(node, pad_sel);
	return -ENODEV;
}

static long ici_pipeline_ioctl_common(void __user *up,
	struct file *file, unsigned int ioctl_cmd,
	unsigned long ioctl_arg)
{
	union {
		struct ici_node_desc node_desc;
		struct ici_link_desc link;
		struct ici_pad_framefmt pad_prop;
		struct ici_pad_supported_format_desc
			format_desc;
		struct ici_links_query links_query;
		struct ici_pad_selection pad_sel;
	} isys_ioctl_cmd_args;
	int err = 0;
	struct ici_isys_pipeline_device *pipe_dev =
						file->private_data;
	const struct ici_pipeline_ioctl_ops *ops;

	if (_IOC_SIZE(ioctl_cmd) > sizeof(isys_ioctl_cmd_args))
		return -ENOTTY;

	if (_IOC_DIR(ioctl_cmd) & _IOC_WRITE) {
		err = copy_from_user(&isys_ioctl_cmd_args, up,
			_IOC_SIZE(ioctl_cmd));
		if (err)
			return -EFAULT;
	}

	mutex_lock(&pipe_dev->mutex);
	ops = pipe_dev->pipeline_ioctl_ops;
	switch(ioctl_cmd) {
	case ICI_IOC_ENUM_NODES:
		err = ops->pipeline_enum_nodes(file, pipe_dev,
			&isys_ioctl_cmd_args.node_desc);
		break;
	case ICI_IOC_ENUM_LINKS:
		err = ops->pipeline_enum_links(file, pipe_dev,
			&isys_ioctl_cmd_args.links_query);
		break;
	case ICI_IOC_SETUP_PIPE:
		err = ops->pipeline_setup_pipe(file, pipe_dev,
			&isys_ioctl_cmd_args.link);
		break;
	case ICI_IOC_SET_FRAMEFMT:
		err = ops->pad_set_ffmt(file, pipe_dev,
			&isys_ioctl_cmd_args.pad_prop);
		break;
	case ICI_IOC_GET_FRAMEFMT:
		err = ops->pad_get_ffmt(file, pipe_dev,
			&isys_ioctl_cmd_args.pad_prop);
		break;
	case ICI_IOC_GET_SUPPORTED_FRAMEFMT:
		err = ops->pad_get_supported_format(file, pipe_dev,
			&isys_ioctl_cmd_args.format_desc);
		break;
	case ICI_IOC_SET_SELECTION:
		err = ops->pad_set_sel(file, pipe_dev,
			&isys_ioctl_cmd_args.pad_sel);
		break;
	case ICI_IOC_GET_SELECTION:
		err = ops->pad_get_sel(file, pipe_dev,
			&isys_ioctl_cmd_args.pad_sel);
		break;
	default:
		err = -ENOTTY;
		break;
	}

	mutex_unlock(&pipe_dev->mutex);
	if (err < 0)
		return err;

	if (_IOC_DIR(ioctl_cmd) & _IOC_READ) {
		err = copy_to_user(up, &isys_ioctl_cmd_args,
			_IOC_SIZE(ioctl_cmd));
		if (err)
			return -EFAULT;
	}

	return 0;
}

static long ici_pipeline_ioctl(struct file *file,
	unsigned int ioctl_cmd, unsigned long ioctl_arg)
{
	long status = 0;
	void __user *up = (void __user *)ioctl_arg;
	status = ici_pipeline_ioctl_common(up, file, ioctl_cmd,
		ioctl_arg);

	return status;
}

static long ici_pipeline_ioctl32(struct file *file,
	unsigned int ioctl_cmd, unsigned long ioctl_arg)
{
	long status = 0;
	void __user *up = compat_ptr(ioctl_arg);
	status = ici_pipeline_ioctl_common(up, file, ioctl_cmd,
		ioctl_arg);

	return status;
}

static const struct ici_pipeline_ioctl_ops pipeline_ioctls =
{
	.pipeline_setup_pipe = ici_setup_link,
	.pipeline_enum_nodes = pipeline_enum_nodes,
	.pipeline_enum_links = pipeline_enum_links,
	.pad_set_ffmt = ici_pipeline_set_ffmt,
	.pad_get_ffmt = ici_pipeline_get_ffmt,
	.pad_get_supported_format =
		ici_pipeline_get_supported_format,
	.pad_set_sel = ici_pipeline_set_sel,
	.pad_get_sel = ici_pipeline_get_sel,

};

static const struct file_operations ici_isys_pipeline_fops =
{
	.owner = THIS_MODULE,
	.open = pipeline_device_open,
	.unlocked_ioctl = ici_pipeline_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ici_pipeline_ioctl32,
#endif
	.release = pipeline_device_release,
};

static void pipeline_device_main_release(struct device *sd)
{
}

int pipeline_device_register(
	struct ici_isys_pipeline_device *pipe_dev,
	struct ici_isys *isys)
{
	int rval = 0;

	pipeline_class =
		class_create(THIS_MODULE,
			ICI_PIPELINE_DEVICE_NAME);
	if (IS_ERR(pipeline_class)) {
		printk(KERN_WARNING "Failed to register device class %s\n",
		       ICI_PIPELINE_DEVICE_NAME);
		return PTR_ERR(pipeline_class);
	}

	pipe_dev->parent = &isys->adev->dev;
	pipe_dev->minor = -1;

	cdev_init(&pipe_dev->cdev, &ici_isys_pipeline_fops);
	pipe_dev->cdev.owner = ici_isys_pipeline_fops.owner;

	rval = cdev_add(&pipe_dev->cdev,
		MKDEV(MAJOR_PIPELINE, MINOR_PIPELINE), 1);
	if (rval) {
		printk(KERN_ERR "%s: failed to add cdevice\n", __func__);
		goto fail;
	}

	pipe_dev->dev.class = pipeline_class;
	pipe_dev->dev.devt = MKDEV(MAJOR_PIPELINE, MINOR_PIPELINE);
	pipe_dev->dev.parent = pipe_dev->parent;
	pipe_dev->dev.release = pipeline_device_main_release;
	dev_set_name(&pipe_dev->dev, "%s",
		ICI_PIPELINE_DEVICE_NAME);
	rval = device_register(&pipe_dev->dev);
	if (rval < 0) {
		printk(KERN_ERR "%s: device_register failed\n", __func__);
		goto out_cdev_del;
	}

	strlcpy(pipe_dev->name, pipe_dev->dev.kobj.name,
		sizeof(pipe_dev->name));
	pipe_dev->minor = MINOR_PIPELINE;

	DEBUGK("Device registered: %s\n", pipe_dev->name);
	pipe_dev->pipeline_ioctl_ops = &pipeline_ioctls;
	mutex_init(&pipe_dev->mutex);
	INIT_LIST_HEAD(&pipe_dev->nodes);

	return 0;

out_cdev_del:
	cdev_del(&pipe_dev->cdev);

fail:
	return rval;
}
EXPORT_SYMBOL(pipeline_device_register);

void pipeline_device_unregister(
	struct ici_isys_pipeline_device* pipe_dev)
{
	DEBUGK("Pipeline device unregistering...");
	device_unregister(&pipe_dev->dev);
	cdev_del(&pipe_dev->cdev);
	class_destroy(pipeline_class);
	mutex_destroy(&pipe_dev->mutex);
}
EXPORT_SYMBOL(pipeline_device_unregister);


#endif /*ICI_ENABLED*/
