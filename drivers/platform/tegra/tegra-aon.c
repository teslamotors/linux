/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/tegra-hsp.h>
#include <linux/mailbox_controller.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/tegra-aon.h>
#include <linux/tegra-ivc.h>
#include <linux/tegra-ivc-instance.h>

#include <asm/cache.h>

/* This has to be a multiple of the cache line size */
static inline int ivc_min_frame_size(void)
{
	return cache_line_size();
}

#define SMBOX1_OFFSET			0x8000

#define SHRD_SEM_OFFSET	0x10000
#define SHRD_SEM_SET	0x4

#define TEGRA_AON_HSP_DATA_ARRAY_SIZE	3

#define IPCBUF_SIZE 2097152

enum smbox_msgs {
	SMBOX_IVC_READY_MSG = 0xAAAA5555,
	SMBOX_IVC_DBG_ENABLE = 0xAAAA6666,
};

enum ivc_tasks_dbg_enable {
	IVC_TASKS_GLOBAL_DBG_ENABLE = 0,
	IVC_ECHO_TASK_DBG_ENABLE = 1,
	IVC_DBG_TASK_DBG_ENABLE = 2,
	IVC_TASK_ENABLE_MAX = 3,
	IVC_TASKS_MAX = 31,
	IVC_TASKS_GLOBAL_DBG_DISABLE = 32,
	IVC_ECHO_TASK_DBG_DISABLE = 33,
	IVC_DBG_TASK_DBG_DISABLE = 34,
	IVC_TASK_DISABLE_MAX = 35,
};

struct tegra_aon {
	struct mbox_controller mbox;
	int hsp_master;
	int hsp_db;
	struct work_struct ch_rx_work;
	void __iomem *smbox_base;
	void __iomem *shrdsem_base;
	void *ipcbuf;
	dma_addr_t ipcbuf_dma;
	size_t ipcbuf_size;
};

struct tegra_aon_ivc_chan {
	struct ivc ivc;
	char *name;
	struct tegra_aon *aon;
	bool last_tx_done;
};

static void tegra_aon_notify_remote(struct ivc *ivc)
{
	struct tegra_aon_ivc_chan *ivc_chan;
	int ret;

	ivc_chan = container_of(ivc, struct tegra_aon_ivc_chan, ivc);
	ret = tegra_hsp_db_ring(ivc_chan->aon->hsp_db);
	if (ret)
		pr_err("tegra_hsp_db_ring failed: %d\n", ret);
}

static void tegra_aon_rx_worker(struct work_struct *work)
{
	struct mbox_chan *mbox_chan;
	struct ivc *ivc;
	struct tegra_aon_mbox_msg msg;
	int i;
	struct tegra_aon *aon = container_of(work,
					struct tegra_aon, ch_rx_work);

	for (i = 0; i < aon->mbox.num_chans; i++) {
		mbox_chan = &aon->mbox.chans[i];
		ivc = (struct ivc *)mbox_chan->con_priv;
		while (tegra_ivc_can_read(ivc)) {
			msg.data = tegra_ivc_read_get_next_frame(ivc);
			msg.length = ivc->frame_size;
			mbox_chan_received_data(mbox_chan, &msg);
			tegra_ivc_read_advance(ivc);
		}
	}
}

static void hsp_irq_handler(void *data)
{
	struct tegra_aon *aon = data;

	schedule_work(&aon->ch_rx_work);
}

#define NV(p) "nvidia," p

static int tegra_aon_parse_channel(struct device *dev,
				struct mbox_chan *mbox_chan,
				struct device_node *ch_node)
{
	struct tegra_aon *aon;
	struct tegra_aon_ivc_chan *ivc_chan;
	struct {
		u32 rx, tx;
	} start, end;
	u32 nframes, frame_size;
	int ret = 0;

	aon = platform_get_drvdata(to_platform_device(dev));

	/* Sanity check */
	if (!mbox_chan || !ch_node || !dev || !aon)
		return -EINVAL;

	ret = of_property_read_u32_array(ch_node, "reg", &start.rx, 2);
	if (ret) {
		dev_err(dev, "missing <%s> property\n", "reg");
		return ret;
	}
	ret = of_property_read_u32(ch_node, NV("frame-count"), &nframes);
	if (ret) {
		dev_err(dev, "missing <%s> property\n", NV("frame-count"));
		return ret;
	}
	ret = of_property_read_u32(ch_node, NV("frame-size"), &frame_size);
	if (ret) {
		dev_err(dev, "missing <%s> property\n", NV("frame-size"));
		return ret;
	}

	if (!nframes) {
		dev_err(dev, "Invalid <nframes> property\n");
		return -EINVAL;
	}

	if (frame_size < ivc_min_frame_size()) {
		dev_err(dev, "Invalid <frame-size> property\n");
		return -EINVAL;
	}

	end.rx = start.rx + tegra_ivc_total_queue_size(nframes * frame_size);
	end.tx = start.tx + tegra_ivc_total_queue_size(nframes * frame_size);

	if (end.rx > aon->ipcbuf_size) {
		dev_err(dev, "%s buffer exceeds ivc size\n", "rx");
		return -EINVAL;
	}
	if (end.tx > aon->ipcbuf_size) {
		dev_err(dev, "%s buffer exceeds ivc size\n", "tx");
		return -EINVAL;
	}

	if (start.tx < start.rx ? end.tx > start.rx : end.rx > start.tx) {
		dev_err(dev, "rx and tx buffers overlap on channel %s\n",
			ch_node->name);
		return -EINVAL;
	}

	ivc_chan = devm_kzalloc(dev, sizeof(*ivc_chan), GFP_KERNEL);
	if (!ivc_chan) {
		dev_err(dev, "Failed to allocate AON IVC channel\n");
		return -ENOMEM;
	}

	ivc_chan->name = devm_kstrdup(dev, ch_node->name, GFP_KERNEL);
	if (!ivc_chan->name)
		return -ENOMEM;

	/* Allocate the IVC links */
	ret = tegra_ivc_init_with_dma_handle(&ivc_chan->ivc,
			     (unsigned long)aon->ipcbuf + start.rx,
			     (u64)aon->ipcbuf_dma + start.rx,
			     (unsigned long)aon->ipcbuf + start.tx,
			     (u64)aon->ipcbuf_dma + start.tx,
			     nframes, frame_size, dev,
			     tegra_aon_notify_remote);
	if (ret) {
		dev_err(dev, "failed to instantiate IVC.\n");
		return ret;
	}

	ivc_chan->aon = aon;
	mbox_chan->con_priv = ivc_chan;

	dev_dbg(dev, "%s: RX: 0x%x-0x%x TX: 0x%x-0x%x\n",
		ivc_chan->name, start.rx, end.rx, start.tx, end.tx);

	return ret;
}

static int tegra_aon_check_channels_overlap(struct device *dev,
			struct tegra_aon_ivc_chan *ch0,
			struct tegra_aon_ivc_chan *ch1)
{
	unsigned s0, s1;
	unsigned long tx0, rx0, tx1, rx1;

	if (ch0 == NULL || ch1 == NULL)
		return -EINVAL;

	tx0 = (unsigned long)ch0->ivc.tx_channel;
	rx0 = (unsigned long)ch0->ivc.rx_channel;
	s0 = ch0->ivc.nframes * ch0->ivc.frame_size;
	s0 = tegra_ivc_total_queue_size(s0);

	tx1 = (unsigned long)ch1->ivc.tx_channel;
	rx1 = (unsigned long)ch1->ivc.rx_channel;
	s1 = ch1->ivc.nframes * ch1->ivc.frame_size;
	s1 = tegra_ivc_total_queue_size(s1);

	if ((tx0 < tx1 ? tx0 + s0 > tx1 : tx1 + s1 > tx0) ||
		(rx0 < tx1 ? rx0 + s0 > tx1 : tx1 + s1 > rx0) ||
		(rx0 < rx1 ? rx0 + s0 > rx1 : rx1 + s1 > rx0) ||
		(tx0 < rx1 ? tx0 + s0 > rx1 : rx1 + s1 > tx0)) {
		dev_err(dev, "ivc buffers overlap on channels %s and %s\n",
			ch0->name, ch1->name);
		return -EINVAL;
	}

	return 0;
}

static int tegra_aon_validate_channels(struct device *dev)
{
	struct tegra_aon *aon;
	struct tegra_aon_ivc_chan *i_chan, *j_chan;
	int i, j;
	int ret;

	aon = dev_get_drvdata(dev);

	for (i = 0; i < aon->mbox.num_chans; i++) {
		i_chan = aon->mbox.chans[i].con_priv;

		for (j = i + 1; j < aon->mbox.num_chans; j++) {
			j_chan = aon->mbox.chans[j].con_priv;

			ret = tegra_aon_check_channels_overlap(dev, i_chan,
								j_chan);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static int tegra_aon_parse_channels(struct device *dev)
{
	struct tegra_aon *aon;
	struct device_node *reg_node, *ch_node;
	int ret, i;

	aon = dev_get_drvdata(dev);
	i = 0;

	for_each_child_of_node(dev->of_node, reg_node) {
		if (strcmp(reg_node->name, "ivc-channels"))
			continue;

		for_each_child_of_node(reg_node, ch_node) {
			ret = tegra_aon_parse_channel(dev,
						&aon->mbox.chans[i++],
						ch_node);
			if (ret) {
				dev_err(dev, "failed to parse a channel\n");
				return ret;
			}
		}
		break;
	}

	return tegra_aon_validate_channels(dev);
}

static int tegra_aon_mbox_send_data(struct mbox_chan *mbox_chan, void *data)
{
	struct tegra_aon_ivc_chan *ivc_chan;
	struct tegra_aon_mbox_msg *msg;
	u32 bytes;
	int ret;

	msg = (struct tegra_aon_mbox_msg *)data;
	ivc_chan = (struct tegra_aon_ivc_chan *)mbox_chan->con_priv;
	bytes = tegra_ivc_write(&ivc_chan->ivc, msg->data, msg->length);
	ret = (bytes != msg->length) ? -EBUSY : 0;
	if (bytes < 0) {
		pr_err("%s mbox send failed with error %d\n", __func__, ret);
		ret = bytes;
	}
	ivc_chan->last_tx_done = (ret == 0);

	return ret;
}

static int tegra_aon_mbox_startup(struct mbox_chan *mbox_chan)
{
	return 0;
}

static void tegra_aon_mbox_shutdown(struct mbox_chan *mbox_chan)
{
}

static bool tegra_aon_mbox_last_tx_done(struct mbox_chan *mbox_chan)
{
	struct tegra_aon_ivc_chan *ivc_chan;

	ivc_chan = (struct tegra_aon_ivc_chan *)mbox_chan->con_priv;

	return ivc_chan->last_tx_done;
}

static struct mbox_chan_ops tegra_aon_mbox_chan_ops = {
	.send_data = tegra_aon_mbox_send_data,
	.startup = tegra_aon_mbox_startup,
	.shutdown = tegra_aon_mbox_shutdown,
	.last_tx_done = tegra_aon_mbox_last_tx_done,
};

static int tegra_aon_count_ivc_channels(struct device_node *dev_node)
{
	int num = 0;
	struct device_node *child_node;

	for_each_child_of_node(dev_node, child_node) {
		if (strcmp(child_node->name, "ivc-channels"))
			continue;
		num = of_get_child_count(child_node);
		break;
	}

	return num;
}

static ssize_t store_ivc_dbg(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct tegra_aon *aon = dev_get_drvdata(dev);
	u32 shrdsem_msg;
	u32 channel;
	u32 enable;
	int ret;

	if (count > ivc_min_frame_size())
		return -EINVAL;

	ret = kstrtouint(buf, 0, &channel);
	if (ret)
		return -EINVAL;

	if ((channel >= IVC_TASKS_GLOBAL_DBG_ENABLE &&
		channel < IVC_TASK_ENABLE_MAX) ||
		(channel >= IVC_TASKS_GLOBAL_DBG_DISABLE &&
		channel < IVC_TASK_DISABLE_MAX)) {
		enable = (channel > IVC_TASKS_MAX) ? 0 : BIT(IVC_TASKS_MAX);
		channel = channel % (IVC_TASKS_MAX + 1);
	} else {
		return -EINVAL;
	}

	shrdsem_msg = BIT(channel) | enable;
	writel(shrdsem_msg, aon->shrdsem_base + SHRD_SEM_SET);
	writel(SMBOX_IVC_DBG_ENABLE, aon->smbox_base + SMBOX1_OFFSET);

	return count;
}


static DEVICE_ATTR(ivc_dbg, S_IWUSR, NULL, store_ivc_dbg);

static int tegra_aon_probe(struct platform_device *pdev)
{
	struct tegra_aon *aon;
	struct device *dev = &pdev->dev;
	struct device_node *dn = dev->of_node;
	u32 hsp_data[TEGRA_AON_HSP_DATA_ARRAY_SIZE];
	int num_chans;
	int ret = 0;

	dev_dbg(&pdev->dev, "tegra aon driver probe Start\n");

	aon = devm_kzalloc(&pdev->dev, sizeof(*aon), GFP_KERNEL);
	if (!aon)
		return -ENOMEM;
	platform_set_drvdata(pdev, aon);
	aon->ipcbuf_size = IPCBUF_SIZE;

	aon->ipcbuf = dmam_alloc_coherent(dev, aon->ipcbuf_size,
			&aon->ipcbuf_dma, GFP_KERNEL | __GFP_ZERO);
	if (!aon->ipcbuf) {
		dev_err(dev, "failed to allocate IPC memory\n");
		return -ENOMEM;
	}

	aon->smbox_base = of_iomap(dn, 0);
	if (!aon->smbox_base) {
		dev_err(dev, "failed to map smbox IO space.\n");
		return -EINVAL;
	}

	aon->shrdsem_base = of_iomap(dn, 1);
	if (!aon->shrdsem_base) {
		dev_err(dev, "failed to map shrdsem IO space.\n");
		iounmap(aon->smbox_base);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(dn, NV("hsp-notifications"), hsp_data,
					ARRAY_SIZE(hsp_data));
	if (ret) {
		dev_err(dev, "missing <%s> property\n",
				NV("hsp-notifications"));
		goto exit;
	}

	/* hsp_data[0] is HSP phandle */
	aon->hsp_master = hsp_data[1];
	aon->hsp_db = hsp_data[2];

	num_chans = tegra_aon_count_ivc_channels(dn);
	if (num_chans <= 0) {
		dev_err(dev, "no ivc channels\n");
		ret = -EINVAL;
		goto exit;
	}

	aon->mbox.dev = &pdev->dev;
	aon->mbox.chans = devm_kzalloc(&pdev->dev,
					num_chans * sizeof(*aon->mbox.chans),
					GFP_KERNEL);
	if (!aon->mbox.chans) {
		ret = -ENOMEM;
		goto exit;
	}

	aon->mbox.num_chans = num_chans;
	aon->mbox.ops = &tegra_aon_mbox_chan_ops;
	aon->mbox.txdone_poll = true;
	aon->mbox.txpoll_period = 1;

	/* Parse out all channels from DT */
	ret = tegra_aon_parse_channels(dev);
	if (ret) {
		dev_err(dev, "ivc-channels set up failed: %d\n", ret);
		goto exit;
	}

	/* Init IVC channels ch_rx_work */
	INIT_WORK(&aon->ch_rx_work, tegra_aon_rx_worker);

	/* Listen to the remote's notification */
	ret = tegra_hsp_db_add_handler(aon->hsp_master, hsp_irq_handler, aon);
	if (ret) {
		dev_err(dev, "failed to add hsp db handler: %d\n", ret);
		goto exit;
	}
	/* Allow remote to ring CCPLEX's doorbell */
	ret = tegra_hsp_db_enable_master(aon->hsp_master);
	if (ret) {
		dev_err(dev, "failed to enable hsp db master: %d\n", ret);
		tegra_hsp_db_del_handler(aon->hsp_master);
		goto exit;
	}

	ret = mbox_controller_register(&aon->mbox);
	if (ret) {
		dev_err(&pdev->dev, "failed to register mailbox: %d\n", ret);
		tegra_hsp_db_del_handler(aon->hsp_master);
		goto exit;
	}
	writel((u32)aon->ipcbuf_dma, aon->shrdsem_base + SHRD_SEM_SET);
	writel((u32)aon->ipcbuf_size, aon->shrdsem_base + SHRD_SEM_OFFSET
					+ SHRD_SEM_SET);
	writel(SMBOX_IVC_READY_MSG, aon->smbox_base + SMBOX1_OFFSET);
	ret = device_create_file(dev, &dev_attr_ivc_dbg);
	if (ret) {
		dev_err(dev, "failed to create device file: %d\n", ret);
		goto exit;
	}

	dev_dbg(&pdev->dev, "tegra aon driver probe OK\n");
	return ret;

exit:
	iounmap(aon->shrdsem_base);
	iounmap(aon->smbox_base);
	return ret;
}

static int tegra_aon_remove(struct platform_device *pdev)
{
	struct tegra_aon *aon = platform_get_drvdata(pdev);

	iounmap(aon->shrdsem_base);
	iounmap(aon->smbox_base);
	mbox_controller_unregister(&aon->mbox);
	tegra_hsp_db_del_handler(aon->hsp_master);

	return 0;
}

static int tegra_aon_resume(struct device *dev)
{
	struct tegra_aon *aon = platform_get_drvdata(to_platform_device(dev));

	return tegra_hsp_db_enable_master(aon->hsp_master);
}

static const struct dev_pm_ops tegra_aon_pm_ops = {
	.resume = tegra_aon_resume,
};

static const struct of_device_id tegra_aon_of_match[] = {
	{ .compatible = NV("tegra186-aon"), },
	{},
};
MODULE_DEVICE_TABLE(of, tegra_aon_of_match);

static struct platform_driver tegra_aon_driver = {
	.probe	= tegra_aon_probe,
	.remove = tegra_aon_remove,
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "tegra_aon",
		.of_match_table = of_match_ptr(tegra_aon_of_match),
		.pm = &tegra_aon_pm_ops,
	},
};
module_platform_driver(tegra_aon_driver);
