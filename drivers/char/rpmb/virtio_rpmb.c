// SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
/*
 * Virtio RPMB Front End Driver
 *
 * Copyright (c) 2018 Intel Corporation. All rights reserved.
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/err.h>
#include <linux/scatterlist.h>
#include <linux/spinlock.h>
#include <linux/virtio.h>
#include <linux/module.h>
#include <linux/virtio_ids.h>
#include <linux/fs.h>
#include <linux/virtio_config.h>
#include <linux/uaccess.h>
#include <linux/rpmb.h>

static const char id[] = "RPMB:VIRTIO";
#ifndef VIRTIO_ID_RPMB
#define	VIRTIO_ID_RPMB		0xFFFF
#endif

#define RPMB_MAX_FRAMES 255

#define SEQ_CMD_MAX	3	/*support up to 3 cmds*/

struct virtio_rpmb_info {
	struct virtqueue *vq;
	struct completion have_data;
	unsigned long busy;
	unsigned int data_avail;
	struct rpmb_dev *rdev;
};

struct virtio_rpmb_ioc {
	unsigned int ioc_cmd;
	int result;
	u8 target;
	u8 reserved[3];
};

static void virtio_rpmb_recv_done(struct virtqueue *vq)
{
	struct virtio_rpmb_info *vi;
	struct virtio_device *vdev = vq->vdev;

	vi = vq->vdev->priv;
	if (!vi) {
		dev_err(&vdev->dev, "Error: no found vi data.\n");
		return;
	}

	if (!virtqueue_get_buf(vi->vq, &vi->data_avail))
		dev_err(&vdev->dev, "Error: Buffer not found in vq.\n");

	complete(&vi->have_data);
}

static int rpmb_virtio_cmd_seq(struct device *dev, u8 target,
			       struct rpmb_cmd *cmds, u32 ncmds)
{
	struct virtio_device *vdev = dev_to_virtio(dev);
	struct virtio_rpmb_info *vi = vdev->priv;
	unsigned int i;
	struct virtio_rpmb_ioc vio_cmd;
	struct rpmb_ioc_seq_cmd seq;
	struct scatterlist vio_ioc, vio_seq, frame[3];
	struct scatterlist *sgs[5];
	unsigned int num_out = 0, num_in = 0;
	size_t sz;
	int ret;

	if (test_and_set_bit(0, &vi->busy)) {
		dev_dbg(dev, "busy , do nothing\n");
		return -EBUSY;
	}

	vio_cmd.ioc_cmd = RPMB_IOC_SEQ_CMD;
	vio_cmd.result = 0;
	vio_cmd.target = target;
	sg_init_one(&vio_ioc, &vio_cmd, sizeof(vio_cmd));
	seq.num_of_cmds = ncmds;
	sgs[num_out + num_in++] = &vio_ioc;

	for (i = 0; i < ncmds; i++) {
		seq.cmds[i].flags   = cmds[i].flags;
		seq.cmds[i].nframes = cmds[i].nframes;
		seq.cmds[i].frames_ptr = i;
	}
	sg_init_one(&vio_seq, &seq, sizeof(seq));
	sgs[num_out + num_in++] = &vio_seq;

	for (i = 0; i < ncmds; i++) {
		sz = sizeof(struct rpmb_frame_jdec) * (cmds[i].nframes ?: 1);
		sg_init_one(&frame[i], cmds[i].frames, sz);
		sgs[num_out + num_in++] = &frame[i];
	}

	virtqueue_add_sgs(vi->vq, sgs, num_out, num_in, vi, GFP_ATOMIC);

	virtqueue_kick(vi->vq);

	/* wait for completion from vq.*/
	ret = wait_for_completion_killable(&vi->have_data);
	if (ret < 0) {
		dev_err(dev, "Error: completion done ret = %d.\n", ret);
		goto out;
	}

	if (vio_cmd.result != 0) {
		dev_err(dev, "Error: command error = %d.\n", vio_cmd.result);
		ret = -EIO;
	}

out:
	clear_bit(0, &vi->busy);
	return ret;
}

static int rpmb_virtio_get_capacity(struct device *dev, u8 target)
{
	return 0;
}

static struct rpmb_ops rpmb_virtio_ops = {
	.cmd_seq = rpmb_virtio_cmd_seq,
	.get_capacity = rpmb_virtio_get_capacity,
	.type = RPMB_TYPE_EMMC,
};

static int rpmb_virtio_dev_init(struct virtio_rpmb_info *vi)
{
	int ret = 0;
	struct device *dev = &vi->vq->vdev->dev;

	rpmb_virtio_ops.dev_id_len = strlen(id);
	rpmb_virtio_ops.dev_id = id;
	rpmb_virtio_ops.wr_cnt_max = 1;
	rpmb_virtio_ops.rd_cnt_max = 1;
	rpmb_virtio_ops.block_size = 1;

	vi->rdev = rpmb_dev_register(dev, 0, &rpmb_virtio_ops);
	if (IS_ERR(vi->rdev)) {
		ret = PTR_ERR(vi->rdev);
		goto err;
	}

	dev_set_drvdata(dev, vi);
err:
	return ret;
}

static int virtio_rpmb_init(struct virtio_device *vdev)
{
	int ret;
	struct virtio_rpmb_info *vi;

	vi = kzalloc(sizeof(*vi), GFP_KERNEL);
	if (!vi)
		return -ENOMEM;

	init_completion(&vi->have_data);

	clear_bit(0, &vi->busy);
	vdev->priv = vi;

	/* We expect a single virtqueue. */
	vi->vq = virtio_find_single_vq(vdev, virtio_rpmb_recv_done, "request");
	if (IS_ERR(vi->vq)) {
		dev_err(&vdev->dev, "get single vq failed!\n");
		ret = PTR_ERR(vi->vq);
		goto err;
	}

	/* create vrpmb device. */
	ret = rpmb_virtio_dev_init(vi);
	if (ret) {
		dev_err(&vdev->dev, "create vrpmb device failed.\n");
		goto err;
	}

	dev_info(&vdev->dev, "init done!\n");

	return 0;

err:
	kfree(vi);
	return ret;
}

static void virtio_rpmb_remove(struct virtio_device *vdev)
{
	struct virtio_rpmb_info *vi;

	vi = vdev->priv;
	if (!vi)
		return;

	rpmb_dev_unregister(vi->rdev);
	complete(&vi->have_data);
	vi->data_avail = 0;
	clear_bit(0, &vi->busy);

	if (vdev->config->reset)
		vdev->config->reset(vdev);

	if (vdev->config->del_vqs)
		vdev->config->del_vqs(vdev);

	kfree(vi);
}

static int virtio_rpmb_probe(struct virtio_device *vdev)
{
	return virtio_rpmb_init(vdev);
}

#ifdef CONFIG_PM_SLEEP
static int virtio_rpmb_freeze(struct virtio_device *vdev)
{
	virtio_rpmb_remove(vdev);
	return 0;
}

static int virtio_rpmb_restore(struct virtio_device *vdev)
{
	return virtio_rpmb_init(vdev);
}
#endif

static struct virtio_device_id id_table[] = {
	{ VIRTIO_ID_RPMB, VIRTIO_DEV_ANY_ID },
	{ 0 },
};

static struct virtio_driver virtio_rpmb_driver = {
	.driver.name =	KBUILD_MODNAME,
	.driver.owner =	THIS_MODULE,
	.id_table =	id_table,
	.probe =	virtio_rpmb_probe,
	.remove =	virtio_rpmb_remove,
#ifdef CONFIG_PM_SLEEP
	.freeze =	virtio_rpmb_freeze,
	.restore =	virtio_rpmb_restore,
#endif
};

module_virtio_driver(virtio_rpmb_driver);
MODULE_DEVICE_TABLE(virtio, id_table);

MODULE_DESCRIPTION("Virtio rpmb frontend driver");
MODULE_AUTHOR("Intel Corporation");
MODULE_LICENSE("Dual BSD/GPL");
