/*
 * Copyright (C) 2015-2016 Intel Corp. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/slab.h>

#include <linux/rpmb.h>

static DEFINE_IDA(rpmb_ida);

/**
 * rpmb_dev_get - increase rpmb device ref counter
 *
 * @rdev: rpmb device
 */
struct rpmb_dev *rpmb_dev_get(struct rpmb_dev *rdev)
{
	return get_device(&rdev->dev) ? rdev : NULL;
}
EXPORT_SYMBOL_GPL(rpmb_dev_get);

/**
 * rpmb_dev_put - decrease rpmb device ref counter
 *
 * @rdev: rpmb device
 */
void rpmb_dev_put(struct rpmb_dev *rdev)
{
	put_device(&rdev->dev);
}
EXPORT_SYMBOL_GPL(rpmb_dev_put);

static int rpmb_request_verify(struct rpmb_dev *rdev, struct rpmb_data *rpmbd)
{
	u16 req_type, block_count;
	struct rpmb_cmd *in_cmd = &rpmbd->icmd;
	struct rpmb_cmd *out_cmd = &rpmbd->ocmd;

	if (!in_cmd->frames || !in_cmd->nframes ||
	    !out_cmd->frames || !out_cmd->nframes)
		return -EINVAL;

	req_type = be16_to_cpu(in_cmd->frames[0].req_resp);
	block_count = be16_to_cpu(in_cmd->frames[0].block_count);

	if (rpmbd->req_type != req_type) {
		dev_err(&rdev->dev, "rpmb req type doesn't match 0x%04X = 0x%04X\n",
			req_type, rpmbd->req_type);
		return -EINVAL;
	}

	switch (req_type) {
	case RPMB_PROGRAM_KEY:
		dev_dbg(&rdev->dev, "rpmb program key = 0x%1x blk = %d\n",
			req_type, block_count);
		break;
	case RPMB_GET_WRITE_COUNTER:
		dev_dbg(&rdev->dev, "rpmb get write counter = 0x%1x blk = %d\n",
			req_type, block_count);

		break;
	case RPMB_WRITE_DATA:
		dev_dbg(&rdev->dev, "rpmb write data = 0x%1x blk = %d\n",
			req_type, block_count);

		if (rdev->ops->reliable_wr_cnt &&
		    block_count > rdev->ops->reliable_wr_cnt) {
			dev_err(&rdev->dev, "rpmb write data: block count %u > reliable wr count %u\n",
				block_count, rdev->ops->reliable_wr_cnt);
			return -EINVAL;
		}

		if (block_count > in_cmd->nframes) {
			dev_err(&rdev->dev, "rpmb write data: block count %u > in frame count %u\n",
				block_count, in_cmd->nframes);
			return -EINVAL;
		}
		break;
	case RPMB_READ_DATA:
		dev_dbg(&rdev->dev, "rpmb read data = 0x%1x blk = %d\n",
			req_type, block_count);

		if (block_count > out_cmd->nframes) {
			dev_err(&rdev->dev, "rpmb read data: block count %u > out frame count %u\n",
				block_count, out_cmd->nframes);
			return -EINVAL;
		}
		break;
	case RPMB_RESULT_READ:
		/* Internal command not supported */
		dev_err(&rdev->dev, "NOTSUPPORTED rpmb resut read = 0x%1x blk = %d\n",
			req_type, block_count);
		return -EOPNOTSUPP;

	default:
		dev_err(&rdev->dev, "Error rpmb invalid command = 0x%1x blk = %d\n",
			req_type, block_count);
		return -EINVAL;
	}

	return 0;
}

/**
 * rpmb_cmd_seq - send RPMB command sequence
 *
 * @rdev: rpmb device
 * @cmds: rpmb command list
 * @ncmds: number of commands
 *
 * Return: 0 on success
 *         -EINVAL on wrong parameters
 *         -EOPNOTSUPP if device doesn't support the requested operation
 *         < 0 if the operation fails
 */
int rpmb_cmd_seq(struct rpmb_dev *rdev, struct rpmb_cmd *cmds, u32 ncmds)
{
	int err;

	if (!rdev || !cmds || !ncmds)
		return -EINVAL;

	mutex_lock(&rdev->lock);
	if (rdev->ops && rdev->ops->cmd_seq)
		err = rdev->ops->cmd_seq(rdev->dev.parent, cmds, ncmds);
	else
		err = -EOPNOTSUPP;
	mutex_unlock(&rdev->lock);
	return err;
}
EXPORT_SYMBOL_GPL(rpmb_cmd_seq);

static void rpmb_cmd_set(struct rpmb_cmd *cmd, u32 flags,
			 struct rpmb_frame *frames, u32 nframes)
{
	cmd->flags = flags;
	cmd->frames = frames;
	cmd->nframes = nframes;
}

/**
 * rpmb_cmd_req_write - setup cmd request write sequence
 *
 * @cmd: cmd sequence
 * @rpmbd: rpmb request data
 * @cnt_in: number of input frames
 * @cnt_out: number of output frames
 *
 * Return: 3 - number of commands in the sequence
 */
static u32 rpmb_cmd_req_write(struct rpmb_cmd *cmd, struct rpmb_data *rpmbd,
			      u32 cnt_in, u32 cnt_out)
{
	struct rpmb_frame *res_frame;

	rpmb_cmd_set(&cmd[0], RPMB_F_WRITE | RPMB_F_REL_WRITE,
		     rpmbd->icmd.frames, cnt_in);
	res_frame = rpmbd->ocmd.frames;
	memset(res_frame, 0, sizeof(*res_frame));
	res_frame->req_resp = cpu_to_be16(RPMB_RESULT_READ);
	rpmb_cmd_set(&cmd[1], RPMB_F_WRITE, res_frame, 1);

	rpmb_cmd_set(&cmd[2], 0, rpmbd->ocmd.frames, cnt_out);

	return  3;
}

/**
 * rpmb_cmd_req_read - setup cmd request read sequence
 *
 * @cmd: cmd sequence
 * @rpmbd: rpmb request data
 * @cnt_in: number of input frames
 * @cnt_out: number of output frames
 *
 * Return: 2 - number of commands in the sequence
 */
static u32 rpmb_cmd_req_read(struct rpmb_cmd *cmd, struct rpmb_data *rpmbd,
			     u32 cnt_in, u32 cnt_out)
{
	rpmb_cmd_set(&cmd[0], RPMB_F_WRITE, rpmbd->icmd.frames, cnt_in);
	rpmb_cmd_set(&cmd[1], 0, rpmbd->ocmd.frames, cnt_out);

	return 2;
}

/**
 * rpmb_cmd_req - send rpmb request command
 *
 * @rdev: rpmb device
 * @rpmbd: rpmb request data
 *
 * Return: 0 on success
 *         -EINVAL on wrong parameters
 *         -EOPNOTSUPP if device doesn't support the requested operation
 *         < 0 if the operation fails
 */
int rpmb_cmd_req(struct rpmb_dev *rdev, struct rpmb_data *rpmbd)
{
	struct rpmb_cmd cmd[3];
	u32 cnt_in, cnt_out;
	u32 ncmds;
	u16 type;
	int ret;

	if (!rdev || !rpmbd)
		return -EINVAL;

	ret = rpmb_request_verify(rdev, rpmbd);
	if (ret)
		return ret;

	if (!rdev->ops || !rdev->ops->cmd_seq)
		return -EOPNOTSUPP;

	cnt_in = rpmbd->icmd.nframes;
	cnt_out = rpmbd->ocmd.nframes;
	type = rpmbd->req_type;
	switch (type) {
	case RPMB_PROGRAM_KEY:
		ncmds = rpmb_cmd_req_write(cmd, rpmbd, 1, 1);
		break;

	case RPMB_WRITE_DATA:
		ncmds = rpmb_cmd_req_write(cmd, rpmbd, cnt_in, cnt_out);
		break;

	case RPMB_GET_WRITE_COUNTER:
		ncmds = rpmb_cmd_req_read(cmd, rpmbd, 1, 1);
		break;

	case RPMB_READ_DATA:
		ncmds = rpmb_cmd_req_write(cmd, rpmbd, cnt_in, cnt_out);
		break;

	default:
		return -EINVAL;
	}

	mutex_lock(&rdev->lock);
	ret = rdev->ops->cmd_seq(rdev->dev.parent, cmd, ncmds);
	mutex_unlock(&rdev->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(rpmb_cmd_req);

static void rpmb_dev_release(struct device *dev)
{
	struct rpmb_dev *rdev = to_rpmb_dev(dev);

	ida_simple_remove(&rpmb_ida, rdev->id);
	kfree(rdev);
}

struct class rpmb_class = {
	.name = "rpmb",
	.owner = THIS_MODULE,
	.dev_release = rpmb_dev_release,
};
EXPORT_SYMBOL(rpmb_class);

/**
 * rpmb_dev_find_device - return first matching rpmb device
 *
 * @data: data for the match function
 * @match: the matching function
 *
 * Return: matching rpmb device or NULL on failure
 */
struct rpmb_dev *rpmb_dev_find_device(void *data,
				      int (*match)(struct device *dev,
						   const void *data))
{
	struct device *dev;

	dev = class_find_device(&rpmb_class, NULL, data, match);

	return dev ? to_rpmb_dev(dev) : NULL;
}
EXPORT_SYMBOL_GPL(rpmb_dev_find_device);

static int match_by_type(struct device *dev, const void *data)
{
	struct rpmb_dev *rdev = to_rpmb_dev(dev);
	enum rpmb_type *type = (enum rpmb_type *)data;

	return (*type == RPMB_TYPE_ANY || rdev->ops->type == *type);
}

/**
 * rpmb_dev_get_by_type - return first registered rpmb device
 *      with matching type.
 *      If run with RPMB_TYPE_ANY the first an probably only
 *      device is returned
 *
 * @type: rpbm underlying device type
 *
 * Return: matching rpmb device or NULL/ERR_PTR on failure
 */
struct rpmb_dev *rpmb_dev_get_by_type(enum rpmb_type type)
{
	if (type > RPMB_TYPE_MAX)
		return ERR_PTR(-EINVAL);

	return rpmb_dev_find_device(&type, match_by_type);
}
EXPORT_SYMBOL_GPL(rpmb_dev_get_by_type);

static int match_by_parent(struct device *dev, const void *data)
{
	const struct device *parent = data;

	return (parent && dev->parent == parent);
}

/**
 * rpmb_dev_find_by_device - retrieve rpmb device from the parent device
 *
 * @parent: parent device of the rpmb device
 *
 * Return: NULL if there is no rpmb device associated with the parent device
 */
struct rpmb_dev *rpmb_dev_find_by_device(struct device *parent)
{
	if (!parent)
		return NULL;

	return rpmb_dev_find_device(parent, match_by_parent);
}
EXPORT_SYMBOL_GPL(rpmb_dev_find_by_device);

/**
 * rpmb_dev_unregister - unregister RPMB partition from the RPMB subsystem
 *
 * @dev: parent device of the rpmb device
 */
int rpmb_dev_unregister(struct device *dev)
{
	struct rpmb_dev *rdev;

	if (!dev)
		return -EINVAL;

	rdev = rpmb_dev_find_by_device(dev);
	if (!rdev) {
		dev_warn(dev, "no disk found %s\n", dev_name(dev->parent));
		return -ENODEV;
	}

	rpmb_dev_put(rdev);

	mutex_lock(&rdev->lock);
	device_del(&rdev->dev);
	mutex_unlock(&rdev->lock);

	rpmb_dev_put(rdev);

	return 0;
}
EXPORT_SYMBOL_GPL(rpmb_dev_unregister);

/**
 * rpmb_dev_register - register RPMB partition with the RPMB subsystem
 *
 * @dev: storage device of the rpmb device
 * @ops: device specific operations
 */
struct rpmb_dev *rpmb_dev_register(struct device *dev,
				   const struct rpmb_ops *ops)
{
	struct rpmb_dev *rdev;
	int id;
	int ret;

	if (!dev || !ops)
		return ERR_PTR(-EINVAL);

	if (!ops->cmd_seq)
		return ERR_PTR(-EINVAL);

	if (ops->type == RPMB_TYPE_ANY || ops->type > RPMB_TYPE_MAX)
		return ERR_PTR(-EINVAL);

	rdev = kzalloc(sizeof(*rdev), GFP_KERNEL);
	if (!rdev)
		return ERR_PTR(-ENOMEM);

	id = ida_simple_get(&rpmb_ida, 0, 0, GFP_KERNEL);
	if (id < 0) {
		ret = id;
		goto exit;
	}

	mutex_init(&rdev->lock);
	rdev->ops = ops;
	rdev->id = id;

	dev_set_name(&rdev->dev, "rpmb%d", id);
	rdev->dev.class = &rpmb_class;
	rdev->dev.parent = dev;
	ret = device_register(&rdev->dev);
	if (ret)
		goto exit;

	dev_dbg(&rdev->dev, "registered disk\n");

	return rdev;

exit:
	if (id >= 0)
		ida_simple_remove(&rpmb_ida, id);
	kfree(rdev);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(rpmb_dev_register);

static int __init rpmb_init(void)
{
	ida_init(&rpmb_ida);
	class_register(&rpmb_class);
	return 0;
}

static void __exit rpmb_exit(void)
{
	class_unregister(&rpmb_class);
	ida_destroy(&rpmb_ida);
}

subsys_initcall(rpmb_init);
module_exit(rpmb_exit);

MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("RPMB class");
MODULE_LICENSE("GPL");
