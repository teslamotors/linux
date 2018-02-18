/*
 * Copyright (c) 2016 NVIDIA CORPORATION. All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>

#include <linux/tegra-camera-rtcpu.h>
#include <linux/tegra-rtcpu-monitor.h>

#include "drivers/video/tegra/host/vi/vi_notify.h"
#include "vi-notify.h"

struct tegra_camrtc_mon {
	struct device *rce_dev;
	struct vi_notify_msg_ex *msg;
	u32 msg_size;
};

int tegra_camrtc_mon_restore_rtcpu(struct tegra_camrtc_mon *cam_rtcpu_mon)
{
	int err;
	struct vi_capture_status ev;
	struct vi_notify_msg_ex *msg;

	if (!cam_rtcpu_mon || !cam_rtcpu_mon->msg) {
		pr_err("Invalid cam_rtcpu_mon params\n");
		return -EINVAL;
	}

	msg = cam_rtcpu_mon->msg;

	memset(msg, 0, cam_rtcpu_mon->msg_size);

	/* Complete event info */
	ev.status = VI_CAPTURE_STATUS_NOTIFIER_BACKEND_DOWN;

	/*
	 * RTCPU_DOWN is a special per-channel error message,
	 * which does not have any of the below information
	 * available and not required also. So set all of them to 0.
	 */
	ev.st = ev.vc = ev.frame = ev.sof_ts =
		ev.eof_ts = ev.data = ev.capture_id = 0;

	/* Fill in notify msg structure. */
	msg->type = VI_NOTIFY_MSG_STATUS;

	msg->dest = 0xFFF; /* Broadcast the error to all 12 Vi Channels */

	msg->size = sizeof(struct vi_capture_status);
	memcpy(msg->data, &ev, msg->size);

	/* Stop the rtcpu */
	tegra_camrtc_stop(cam_rtcpu_mon->rce_dev);

	/* Broadcast rtcpu-down message to all vi channels */
	err = tegra_ivc_vi_notify_report(msg);
	if (err) {
		dev_err(cam_rtcpu_mon->rce_dev,
			"tegra_ivc_vi_notify_report failed %d\n", err);
		return err;
	}

	/* (Re)start the rtcpu */
	tegra_camrtc_start(cam_rtcpu_mon->rce_dev);

	return 0;
}
EXPORT_SYMBOL(tegra_camrtc_mon_restore_rtcpu);

/* TODO: ISR setup for SCE WDT RemoteIRQ */

struct tegra_camrtc_mon *tegra_camrtc_mon_create(struct device *dev)
{
	struct tegra_camrtc_mon *cam_rtcpu_mon;

	cam_rtcpu_mon = kzalloc(sizeof(*cam_rtcpu_mon), GFP_KERNEL);
	if (unlikely(cam_rtcpu_mon == NULL))
		return ERR_PTR(-ENOMEM);

	cam_rtcpu_mon->rce_dev = dev;

	cam_rtcpu_mon->msg_size = sizeof(*cam_rtcpu_mon->msg) +
					sizeof(struct vi_capture_status);

	cam_rtcpu_mon->msg = kzalloc(cam_rtcpu_mon->msg_size, GFP_KERNEL);
	if (unlikely(cam_rtcpu_mon->msg == NULL)) {
		dev_err(dev, "failed to allocate vi_notify_msg_ex struct\n");
		return ERR_PTR(-ENOMEM);
	}

	dev_info(dev, "tegra_camrtc_mon_create is successful\n");

	return cam_rtcpu_mon;
}
EXPORT_SYMBOL(tegra_camrtc_mon_create);

int tegra_cam_rtcpu_mon_destroy(struct tegra_camrtc_mon *cam_rtcpu_mon)
{
	if (IS_ERR_OR_NULL(cam_rtcpu_mon))
		return -EINVAL;

	if (cam_rtcpu_mon->msg)
		kfree(cam_rtcpu_mon->msg);
	kfree(cam_rtcpu_mon);

	return 0;
}
EXPORT_SYMBOL(tegra_cam_rtcpu_mon_destroy);

MODULE_DESCRIPTION("CAMERA RTCPU monitor driver");
MODULE_AUTHOR("Sudhir Vyas <svyas@nvidia.com>");
MODULE_LICENSE("GPL v2");
