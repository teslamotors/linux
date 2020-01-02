/*
 * Intel Host Storage Interface Linux driver
 * Copyright (c) 2015 - 2016, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include "cmd.h"
#include "spd.h"

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

int mei_spd_rpmb_cmd_req(struct mei_spd *spd, u16 req_type, void *buf)
{
	struct rpmb_data d;
	struct rpmb_frame *frame = buf;
	int ret;

	if (!spd->rdev) {
		spd_err(spd, "RPMB not ready\n");
		return -ENODEV;
	}

	d.req_type = req_type;
	d.icmd.nframes = 1;
	d.icmd.frames  = frame;
	d.ocmd.nframes = 1;
	d.ocmd.frames  = frame;
	ret = rpmb_cmd_req(spd->rdev, &d);
	if (ret)
		spd_err(spd, "RPMB req failed ret = %d\n", ret);
	return ret;
}

bool mei_spd_rpmb_is_open(struct mei_spd *spd)
{
	return !!spd->rdev;
}
