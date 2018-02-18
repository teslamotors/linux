/*
 * VI NOTIFY driver for Tegra186
 *
 * Copyright (c) 2015-2016 NVIDIA Corporation.  All rights reserved.
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

#include <linux/completion.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/tegra-ivc.h>
#include <linux/tegra-ivc-bus.h>

#include "drivers/video/tegra/host/vi/vi_notify.h"
#include "drivers/video/tegra/host/nvhost_acm.h"

#define BUG_200219206

struct vi_notify_req {
	union {
		struct {
			u8 type;
			u8 channel;
			u8 stream;
			u8 vc;
			union {
				u32 syncpt_ids[3];
				u32 mask;
			};
		};
		char size[128];
	};
};

enum {
	TEGRA_IVC_VI_CLASSIFY,
	TEGRA_IVC_VI_SET_SYNCPTS,
	TEGRA_IVC_VI_RESET_CHANNEL,
	TEGRA_IVC_VI_ENABLE_REPORTS,
};

/* Extended VI notify message */
struct vi_notify_msg_ex {
	u32 type;	/* message type (LSB=0) */
	u32 dest;	/* destination channels (bitmask) */
	u32 size;	/* data size */
	u8 data[];	/* payload data */
};

/* Extended message types */
#define VI_NOTIFY_MSG_INVALID	0x00000000
#define VI_NOTIFY_MSG_ACK	0x00000002
#define VI_NOTIFY_MSG_STATUS	0x00000004

struct tegra_ivc_vi_notify {
	struct vi_notify_dev *vi_notify;
	struct platform_device *vi;
	u32 tags;
	u16 channels;
	wait_queue_head_t write_q;
	struct completion ack;
};

static void tegra_ivc_vi_notify_process(struct tegra_ivc_channel *chan,
					const struct vi_notify_msg_ex *msg)
{
	struct tegra_ivc_vi_notify *ivn = tegra_ivc_channel_get_drvdata(chan);
	struct vi_notify_dev *vnd = ivn->vi_notify;
	u32 mask;
	u8 ch;

	switch (msg->type) {
	case VI_NOTIFY_MSG_ACK:
		complete(&ivn->ack);
		break;
	case VI_NOTIFY_MSG_STATUS:
		if (msg->size != sizeof(struct vi_capture_status)) {
			dev_warn(&chan->dev, "Invalid status message.\n");
			break;
		}
		mask = msg->dest & ivn->channels;
		for (ch = 0; mask; mask >>= 1, ch++) {
			if (!(mask & 1u))
				continue;
			vi_notify_dev_report(vnd, ch,
					(struct vi_capture_status *)msg->data);
		}
		break;
	default:
		dev_warn(&chan->dev, "Unknown message type: %u\n", msg->type);
		break;
	}
}

static void tegra_ivc_vi_notify_recv(struct tegra_ivc_channel *chan,
					const void *data, size_t len)
{
	struct tegra_ivc_vi_notify *ivn = tegra_ivc_channel_get_drvdata(chan);
	const struct vi_notify_msg *entries = data;

	/* Receive VI Notify events */
	while (len >= sizeof(*entries)) {
		if (!entries->tag)
			break;
		if (VI_NOTIFY_TAG_VALID(entries->tag))
			vi_notify_dev_recv(ivn->vi_notify, entries);
		else
			tegra_ivc_vi_notify_process(chan, data);

		entries++;
		len -= sizeof(*entries);
	}
}

static void tegra_ivc_channel_vi_notify_process(struct tegra_ivc_channel *chan)
{
	struct tegra_ivc_vi_notify *ivn = tegra_ivc_channel_get_drvdata(chan);

	wake_up(&ivn->write_q);

	while (tegra_ivc_can_read(&chan->ivc)) {
		const void *data = tegra_ivc_read_get_next_frame(&chan->ivc);
		size_t length = chan->ivc.frame_size;

		tegra_ivc_vi_notify_recv(chan, data, length);
		tegra_ivc_read_advance(&chan->ivc);
	}
}

/* VI Notify */
static int tegra_ivc_vi_notify_prepare(struct tegra_ivc_channel *chan)
{
	struct tegra_ivc_vi_notify *ivn = tegra_ivc_channel_get_drvdata(chan);

	if (ivn->tags == 0 && ivn->channels == 0) {
		int err = pm_runtime_get_sync(&chan->dev);
		if (err)
			return err;

		return nvhost_module_busy(ivn->vi);
	}

	return 0;
}

static void tegra_ivc_vi_notify_complete(struct tegra_ivc_channel *chan)
{
	struct tegra_ivc_vi_notify *ivn = tegra_ivc_channel_get_drvdata(chan);

	if (ivn->tags == 0 && ivn->channels == 0) {
		nvhost_module_idle(ivn->vi);
		pm_runtime_put(&chan->dev);
	}
}

static int tegra_ivc_vi_notify_send(struct tegra_ivc_channel *chan,
					const struct vi_notify_req *req)
{
	struct tegra_ivc_vi_notify *ivn = tegra_ivc_channel_get_drvdata(chan);
	int ret;

	ret = tegra_ivc_vi_notify_prepare(chan);

	while (ret == 0) {
		DEFINE_WAIT(wait);

		prepare_to_wait(&ivn->write_q, &wait, TASK_INTERRUPTIBLE);

		ret = tegra_ivc_write(&chan->ivc, req, sizeof(*req));
		if (ret >= 0)
			;
		else if (ret != -ENOMEM)
			dev_err(&chan->dev, "cannot send request: %d\n", ret);
		else if (signal_pending(current))
			ret = -ERESTARTSYS;
		else {
			ret = 0;
			schedule();
		}

		finish_wait(&ivn->write_q, &wait);
	}

	if (ret < 0)
		return ret;

	/* Wait for RTCPU to acknowledge the request. This fixes races such as:
	 * - RTCPU attempting to use a powered-off VI,
	 * - VI emitting an event before the request is processed. */
	ret = wait_for_completion_killable_timeout(&ivn->ack, HZ);
	if (ret <= 0) {
		dev_err(&chan->dev, "no reply from camera processor\n");
#ifndef BUG_200219206
		WARN_ON(1);
#endif
		return -ETIMEDOUT;
	}

	return 0;
}

static int tegra_ivc_vi_notify_probe(struct device *dev,
					struct vi_notify_dev *vnd)
{
	struct tegra_ivc_vi_notify *ivn = dev_get_drvdata(dev);

	ivn->vi_notify = vnd;
	return 0;
}

static int tegra_ivc_vi_notify_classify(struct device *dev, u32 mask)
{
	struct tegra_ivc_channel *chan = to_tegra_ivc_channel(dev);
	struct tegra_ivc_vi_notify *ivn = tegra_ivc_channel_get_drvdata(chan);
	struct vi_notify_req req = {
		.type = TEGRA_IVC_VI_CLASSIFY,
		.mask = mask,
	};
	int err;

	if (ivn->tags == mask)
		return 0; /* nothing to do */

	err = tegra_ivc_vi_notify_send(chan, &req);
	if (likely(err == 0))
		ivn->tags = mask;
	tegra_ivc_vi_notify_complete(chan);

	return err;
}

static int tegra_ivc_vi_notify_set_syncpts(struct device *dev, u8 ch,
					const u32 ids[3])
{
	struct tegra_ivc_channel *chan = to_tegra_ivc_channel(dev);
	struct tegra_ivc_vi_notify *ivn = tegra_ivc_channel_get_drvdata(chan);
	struct vi_notify_req msg = {
		.type = TEGRA_IVC_VI_SET_SYNCPTS,
		.channel = ch,
	};
	int err;

	memcpy(msg.syncpt_ids, ids, sizeof(msg.syncpt_ids));

	err = tegra_ivc_vi_notify_send(chan, &msg);
	if (likely(err == 0))
		ivn->channels |= 1u << ch;
	tegra_ivc_vi_notify_complete(chan);

	return err;
}

static int tegra_ivc_vi_notify_enable_reports(struct device *dev, u8 ch,
					u8 st, u8 vc, const u32 ids[3])
{
	struct tegra_ivc_channel *chan = to_tegra_ivc_channel(dev);
	struct tegra_ivc_vi_notify *ivn = tegra_ivc_channel_get_drvdata(chan);
	struct vi_notify_req msg = {
		.type = TEGRA_IVC_VI_ENABLE_REPORTS,
		.channel = ch,
		.stream = st,
		.vc = vc,
	};
	int err;

	memcpy(msg.syncpt_ids, ids, sizeof(msg.syncpt_ids));

	err = tegra_ivc_vi_notify_send(chan, &msg);
	if (likely(err == 0))
		ivn->channels |= 1u << ch;
	tegra_ivc_vi_notify_complete(chan);

	return err;
}

static void tegra_ivc_vi_notify_reset_channel(struct device *dev, u8 ch)
{
	struct tegra_ivc_channel *chan = to_tegra_ivc_channel(dev);
	struct tegra_ivc_vi_notify *ivn = tegra_ivc_channel_get_drvdata(chan);
	struct vi_notify_req msg = {
		.type = TEGRA_IVC_VI_RESET_CHANNEL,
		.channel = ch,
	};
	int err;

	err = tegra_ivc_vi_notify_send(chan, &msg);
	if (likely(err == 0))
		ivn->channels &= ~(1u << ch);
	tegra_ivc_vi_notify_complete(chan);
}

static struct vi_notify_driver tegra_ivc_vi_notify_driver = {
	.owner		= THIS_MODULE,
	.probe		= tegra_ivc_vi_notify_probe,
	.classify	= tegra_ivc_vi_notify_classify,
	.set_syncpts	= tegra_ivc_vi_notify_set_syncpts,
	.enable_reports = tegra_ivc_vi_notify_enable_reports,
	.reset_channel	= tegra_ivc_vi_notify_reset_channel,
};

/* Platform device */
static struct platform_device *tegra_vi_get(struct device *dev)
{
	struct device_node *vi_node;
	struct platform_device *vi_pdev;

	vi_node = of_parse_phandle(dev->of_node, "device", 0);
	if (vi_node == NULL) {
		dev_err(dev, "cannot get VI device");
		return ERR_PTR(-ENODEV);
	}

	vi_pdev = of_find_device_by_node(vi_node);
	of_node_put(vi_node);

	if (vi_pdev == NULL)
		return ERR_PTR(-EPROBE_DEFER);

	if (&vi_pdev->dev.driver == NULL) {
		platform_device_put(vi_pdev);
		return ERR_PTR(-EPROBE_DEFER);
	}
	return vi_pdev;
}

static int tegra_ivc_channel_vi_notify_probe(struct tegra_ivc_channel *chan)
{
	struct tegra_ivc_vi_notify *ivn = tegra_ivc_channel_get_drvdata(chan);
	int err;

	ivn = devm_kmalloc(&chan->dev, sizeof(*ivn), GFP_KERNEL);
	if (unlikely(ivn == NULL))
		return -ENOMEM;

	ivn->vi = tegra_vi_get(&chan->dev);
	if (IS_ERR(ivn->vi))
		return PTR_ERR(ivn->vi);

	ivn->tags = 0;
	ivn->channels = 0;
	init_waitqueue_head(&ivn->write_q);
	init_completion(&ivn->ack);

	tegra_ivc_channel_set_drvdata(chan, ivn);

	err = vi_notify_register(&tegra_ivc_vi_notify_driver, &chan->dev, 12);
	if (err)
		platform_device_put(ivn->vi);
	return err;
}

static void tegra_ivc_channel_vi_notify_remove(struct tegra_ivc_channel *chan)
{
	struct tegra_ivc_vi_notify *ivn = tegra_ivc_channel_get_drvdata(chan);

	vi_notify_unregister(&tegra_ivc_vi_notify_driver, &chan->dev);
	platform_device_put(ivn->vi);
}

static struct of_device_id tegra_ivc_channel_vi_notify_of_match[] = {
	{ .compatible = "nvidia,tegra186-camera-ivc-protocol-vinotify" },
	{ },
};

static const struct tegra_ivc_channel_ops tegra_ivc_channel_vi_notify_ops = {
	.probe	= tegra_ivc_channel_vi_notify_probe,
	.remove	= tegra_ivc_channel_vi_notify_remove,
	.notify	= tegra_ivc_channel_vi_notify_process,
};

static struct tegra_ivc_driver tegra_ivc_channel_vi_notify_driver = {
	.driver = {
		.name	= "tegra-ivc-vi-notify",
		.bus	= &tegra_ivc_bus_type,
		.owner	= THIS_MODULE,
		.of_match_table = tegra_ivc_channel_vi_notify_of_match,
	},
	.dev_type	= &tegra_ivc_channel_type,
	.ops.channel	= &tegra_ivc_channel_vi_notify_ops,
};
tegra_ivc_module_driver(tegra_ivc_channel_vi_notify_driver);
MODULE_AUTHOR("Remi Denis-Courmont <remid@nvidia.com>");
MODULE_DESCRIPTION("NVIDIA Tegra IVC VI Notify driver");
MODULE_LICENSE("GPL");
