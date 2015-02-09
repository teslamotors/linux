// SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
/*
 * Intel Host Storage Interface Linux driver
 * Copyright (c) 2015 - 2018, Intel Corporation.
 */

#include "cmd.h"
#include "spd.h"
#include <linux/slab.h>

static int mei_spd_rpmb_start(struct mei_spd *spd, struct rpmb_dev *rdev)
{
	if (spd->rdev == rdev)
		return 0;

	if (spd->rdev) {
		spd_warn(spd, "rpmb device already registered\n");
		return -EEXIST;
	}

	spd->rdev = rpmb_dev_get(rdev);
	spd_dbg(spd, "rpmb partition created\n");
	return 0;
}

static int mei_spd_rpmb_stop(struct mei_spd *spd, struct rpmb_dev *rdev)
{
	if (!spd->rdev) {
		spd_dbg(spd, "Already stopped\n");
		return -EPROTO;
	}

	if (rdev && spd->rdev != rdev) {
		spd_dbg(spd, "Wrong RPMB on stop\n");
		return -EINVAL;
	}

	rpmb_dev_put(spd->rdev);
	spd->rdev = NULL;

	spd_dbg(spd, "rpmb partition removed\n");
	return 0;
}

static int mei_spd_rpmb_match(struct mei_spd *spd, struct rpmb_dev *rdev)
{
	if (spd->dev_id_sz && rdev->ops->dev_id) {
		if (rdev->ops->dev_id_len != spd->dev_id_sz ||
		    memcmp(rdev->ops->dev_id, spd->dev_id,
			   rdev->ops->dev_id_len)) {
			spd_dbg(spd, "ignore request for another rpmb\n");
			/* return 0; FW sends garbage now, ignore it */
		}
	}

	switch (rdev->ops->type) {
	case RPMB_TYPE_EMMC:
		if (spd->dev_type != SPD_TYPE_EMMC)
			return 0;
		break;
	case RPMB_TYPE_UFS:
		if (spd->dev_type != SPD_TYPE_UFS)
			return 0;
		break;
	default:
		return 0;
	}

	return 1;
}

static int rpmb_add_device(struct device *dev, struct class_interface *intf)
{
	struct mei_spd *spd =
		container_of(intf, struct mei_spd, rpmb_interface);
	struct rpmb_dev *rdev = to_rpmb_dev(dev);

	if (!mei_spd_rpmb_match(spd, rdev))
		return 0;

	mutex_lock(&spd->lock);
	if (mei_spd_rpmb_start(spd, rdev)) {
		mutex_unlock(&spd->lock);
		return 0;
	}

	schedule_work(&spd->status_send_w);
	mutex_unlock(&spd->lock);

	return 0;
}

static void rpmb_remove_device(struct device *dev, struct class_interface *intf)
{
	struct mei_spd *spd =
		container_of(intf, struct mei_spd, rpmb_interface);
	struct rpmb_dev *rdev = to_rpmb_dev(dev);

	if (!mei_spd_rpmb_match(spd, rdev))
		return;

	mutex_lock(&spd->lock);
	if (mei_spd_rpmb_stop(spd, rdev)) {
		mutex_unlock(&spd->lock);
		return;
	}

	if (spd->state != MEI_SPD_STATE_STOPPING)
		schedule_work(&spd->status_send_w);
	mutex_unlock(&spd->lock);
}

void mei_spd_rpmb_prepare(struct mei_spd *spd)
{
	spd->rpmb_interface.add_dev    = rpmb_add_device;
	spd->rpmb_interface.remove_dev = rpmb_remove_device;
	spd->rpmb_interface.class      = &rpmb_class;
}

/**
 * mei_spd_rpmb_init - init RPMB connection
 *
 * @spd: device
 *
 * Locking: spd->lock should not be held
 * Returns: 0 if initialized successfully, <0 otherwise
 */
int mei_spd_rpmb_init(struct mei_spd *spd)
{
	int ret;

	ret = class_interface_register(&spd->rpmb_interface);
	if (ret)
		spd_err(spd, "Can't register interface\n");
	return ret;
}

/**
 * mei_spd_rpmb_exit - clean RPMB connection
 *
 * @spd: device
 *
 * Locking: spd->lock should not be held
 */
void mei_spd_rpmb_exit(struct mei_spd *spd)
{
	class_interface_unregister(&spd->rpmb_interface);
}

int mei_spd_rpmb_cmd_req(struct mei_spd *spd, u16 req, void *buf)
{
	struct rpmb_cmd cmd[3];
	struct rpmb_frame_jdec *frame_res = NULL;
	u32 flags;
	unsigned int i;
	int ret;

	if (!spd->rdev) {
		spd_err(spd, "RPMB not ready\n");
		return -ENODEV;
	}

	i = 0;
	flags = RPMB_F_WRITE;
	if (req == RPMB_WRITE_DATA || req == RPMB_PROGRAM_KEY)
		flags |= RPMB_F_REL_WRITE;
	cmd[i].flags = flags;
	cmd[i].nframes = 1;
	cmd[i].frames = buf;
	i++;

	if (req == RPMB_WRITE_DATA || req == RPMB_PROGRAM_KEY) {
		frame_res = kzalloc(sizeof(*frame_res), GFP_KERNEL);
		if (!frame_res)
			return -ENOMEM;
		frame_res->req_resp =  cpu_to_be16(RPMB_RESULT_READ);
		cmd[i].flags = RPMB_F_WRITE;
		cmd[i].nframes = 1;
		cmd[i].frames = frame_res;
		i++;
	}

	cmd[i].flags = 0;
	cmd[i].nframes = 1;
	cmd[i].frames = buf;
	i++;

	ret = rpmb_cmd_seq(spd->rdev, cmd, i);
	if (ret)
		spd_err(spd, "RPMB req failed ret = %d\n", ret);

	kfree(frame_res);
	return ret;
}

bool mei_spd_rpmb_is_open(struct mei_spd *spd)
{
	return !!spd->rdev;
}
