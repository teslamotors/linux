/*
 * Tegra sysfs mbox driver for IVC channels
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

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/tegra-ivc.h>
#include <linux/tegra-ivc-bus.h>

#define NV(p) "nvidia," #p

#ifndef dev_err_once
#define dev_err_once(dev, fmt, ...)					\
do {									\
	static bool __print_once __read_mostly;				\
									\
	if (!__print_once) {						\
		__print_once = true;					\
		dev_err(dev, fmt, ##__VA_ARGS__);			\
	}								\
} while (0)
#endif

static ssize_t sysfs_mbox_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tegra_ivc_channel *chan = dev_get_drvdata(dev);
	size_t length = chan->ivc.frame_size;
	ssize_t count = 0;

	dev_err_once(dev, "mbox is deprecated\n");

	while ((count + length < PAGE_SIZE) &&
		tegra_ivc_can_read(&chan->ivc)) {
		const void *data = tegra_ivc_read_get_next_frame(&chan->ivc);

		dev_dbg(&chan->dev, "rx msg\n");
		print_hex_dump(KERN_DEBUG, "", DUMP_PREFIX_OFFSET,
				16, 1, data, length, true);

		memcpy(buf, data, length);
		tegra_ivc_read_advance(&chan->ivc);
		buf += length;
		count += length;
	}

	return count;
}

static ssize_t sysfs_mbox_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tegra_ivc_channel *chan = dev_get_drvdata(dev);

	if (count > chan->ivc.frame_size) {
		dev_err(dev, "data size %zu > mbox frame size %u\n",
			count, chan->ivc.frame_size);
		return -EMSGSIZE;
	}

	dev_err_once(dev, "mbox is deprecated\n");
	dev_dbg(&chan->dev, "tx msg\n");
	print_hex_dump(KERN_DEBUG, "", DUMP_PREFIX_OFFSET,
			16, 1, buf, count, true);

	return tegra_ivc_write(&chan->ivc, buf, count);
}

static const DEVICE_ATTR(mbox, 0600, sysfs_mbox_show, sysfs_mbox_store);

/* This is a quick-and-dirty hack driver. Do not extend. */
static const struct of_device_id sysfs_mbox_of_match[] = {
	{ .compatible = "nvidia,tegra186-camera-ivc-protocol-mods",
		.data = "sce-mods" },
	{ },
};

/* Tegra IVC channel device */
static int sysfs_mbox_probe(struct tegra_ivc_channel *chan)
{
	struct device *legacy;
	const struct of_device_id *id = of_match_device(sysfs_mbox_of_match,
							&chan->dev);
	int err;

	if (id == NULL)
		return -EINVAL;

	BUG_ON(id->data == NULL);

	legacy = root_device_register(id->data);

	if (IS_ERR(legacy))
		return PTR_ERR(legacy);

	dev_set_drvdata(legacy, chan);
	tegra_ivc_channel_set_drvdata(chan, legacy);

	err = sysfs_create_link(&legacy->kobj, &chan->dev.kobj, "ivc-channel");
	if (err)
		goto err1;

	err = sysfs_create_link(&chan->dev.kobj, &legacy->kobj, "legacy");
	if (err)
		goto err2;

	err = device_create_file(legacy, &dev_attr_mbox);
	if (err) {
		sysfs_delete_link(&chan->dev.kobj, &legacy->kobj, "legacy");
err2:
		sysfs_delete_link(&legacy->kobj, &chan->dev.kobj,
					"ivc-channel");
err1:
		root_device_unregister(legacy);
	}
	return err;
}

static void sysfs_mbox_remove(struct tegra_ivc_channel *chan)
{
	struct device *legacy = tegra_ivc_channel_get_drvdata(chan);

	device_remove_file(legacy, &dev_attr_mbox);
	sysfs_delete_link(&chan->dev.kobj, &legacy->kobj, "legacy");
	sysfs_delete_link(&legacy->kobj, &chan->dev.kobj, "ivc-channel");
	root_device_unregister(legacy);
}

static const struct tegra_ivc_channel_ops tegra_ivc_channel_sysfs_ops = {
	.probe	= sysfs_mbox_probe,
	.remove	= sysfs_mbox_remove,
};

static struct tegra_ivc_driver sysfs_mbox_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.bus	= &tegra_ivc_bus_type,
		.name	= "tegra-ivc-channel-sysfs",
		.of_match_table = sysfs_mbox_of_match,
	},
	.dev_type	= &tegra_ivc_channel_type,
	.ops.channel	= &tegra_ivc_channel_sysfs_ops,
};
tegra_ivc_module_driver(sysfs_mbox_driver);
MODULE_AUTHOR("Pekka Pessi <ppessi@nvidia.com>");
MODULE_DESCRIPTION("Legacy Tegra IVC sysfs driver");
MODULE_LICENSE("GPL");
