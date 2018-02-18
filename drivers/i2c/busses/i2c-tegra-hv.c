/*
 * drivers/i2c/busses/i2c-tegra-hv.c
 *
 * Copyright (C) 2015 NVIDIA Corporation.  All rights reserved.
 * Author: Arnab Basu <abasu@nvidia.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/i2c-tegra-hv.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/completion.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/ioport.h>

#include "i2c-tegra-hv-common.h"

#include <asm/unaligned.h>

#define TEGRA_I2C_TIMEOUT			(msecs_to_jiffies(1000))
#define TEGRA_I2C_RETRIES			3

#define I2C_NO_ERROR 0

/**
 * struct tegra_hv_i2c_dev - per device i2c context
 * @dev: device reference
 * @adapter: core i2c layer adapter information
 * @base: i2c controller base
 * @comm_chan: context for the logical channel over which this
 * device communicates with the i2c server
 * @msg_complete: transfer completion notifier
 * @max_payload_size: maximum packet size supported
 * by this i2c device
 * @completion_timeout: time to wait for reply from server
 */
struct tegra_hv_i2c_dev {
	struct device *dev;
	struct i2c_adapter adapter;
	phys_addr_t base;
	void *comm_chan;
	struct completion msg_complete;
	u32 max_payload_size;
	u32 completion_timeout;
};

static void tegra_hv_i2c_isr(void *dev_id)
{
	struct tegra_hv_i2c_dev *i2c_dev = dev_id;
	complete(&i2c_dev->msg_complete);
}

static int tegra_hv_i2c_xfer_msg(struct tegra_hv_i2c_dev *i2c_dev,
		struct i2c_msg *msg, int sno, bool more_msgs)
{
	int ret;
	int msg_err;
	int msg_read;
	int rv;
	int j = 0;
	uint32_t flags = 0;

	if (msg->len == 0)
		return -EINVAL;

	if (msg->len > i2c_dev->max_payload_size)
		return -E2BIG;

	msg_err = I2C_NO_ERROR;
	msg_read = (msg->flags & I2C_M_RD);
	reinit_completion(&i2c_dev->msg_complete);

	if (more_msgs)
		flags |= HV_I2C_FLAGS_REPEAT_START;

	if (msg->flags & I2C_M_TEN)
		flags |= HV_I2C_FLAGS_10BIT_ADDR;

	ret = hv_i2c_transfer(i2c_dev->comm_chan, i2c_dev->base, msg->addr,
			msg_read, msg->buf, msg->len, &msg_err, sno, flags);
	if (ret < 0) {
		dev_err(i2c_dev->dev, "unable to send message (%d)\n", ret);
		return ret;
	}

	ret = wait_for_completion_timeout(&i2c_dev->msg_complete,
					i2c_dev->completion_timeout);

	if (ret == 0) {
		dev_err(i2c_dev->dev,
			"i2c transfer timed out, addr 0x%04x, data 0x%02x\n",
			msg->addr, msg->buf[0]);
		rv = -EBUSY;
		goto error;
	}

	dev_dbg(i2c_dev->dev, "transfer complete: %d %d %d\n",
		ret, completion_done(&i2c_dev->msg_complete), msg_err);

	if (likely(msg_err == I2C_NO_ERROR))
		return 0;

	dev_dbg(i2c_dev->dev, "received error code %d\n", msg_err);
	rv = -EIO;
error:
	reinit_completion(&i2c_dev->msg_complete);
	ret = hv_i2c_comm_chan_cleanup(i2c_dev->comm_chan, i2c_dev->base);

	if (ret < 0) {
		dev_err(i2c_dev->dev, "Failed to send cleanup message\n");
	}

	while ((ret = wait_for_completion_timeout(&i2c_dev->msg_complete,
				i2c_dev->completion_timeout * 2)) == 0) {
		dev_err(i2c_dev->dev, "Cleanup failed after timeout (%d tries)\n",
				j++);
		if (j >= 5)
			break;
		/* Skipping INIT_COMPLETION on purpose, if completion gets
		 * signalled in the time between 2 calls to wait_for_completion
		 * we don't want to overwrite that
		 */
	}
	tegra_hv_i2c_poll_cleanup(i2c_dev->comm_chan);

	return rv;
}

static int tegra_hv_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[],
	int num)
{
	struct tegra_hv_i2c_dev *i2c_dev = i2c_get_adapdata(adap);
	int i;
	int ret = 0;

	for (i = 0; i < num; i++) {
		ret = tegra_hv_i2c_xfer_msg(i2c_dev, &msgs[i], i,
					    (i < (num - 1)));
		if (ret)
			break;
	}

	return ret ? ret : i;
}

static u32 tegra_hv_i2c_func(struct i2c_adapter *adap)
{
	u32 ret = I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL | I2C_FUNC_10BIT_ADDR;

	return ret;
}

static const struct i2c_algorithm tegra_hv_i2c_algo = {
	.master_xfer	= tegra_hv_i2c_xfer,
	.functionality	= tegra_hv_i2c_func,
};

static struct tegra_hv_i2c_platform_data *parse_i2c_tegra_dt(
	struct platform_device *pdev)
{
	struct tegra_hv_i2c_platform_data *pdata;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	return pdata;
}

/* Match table for of_platform binding */
static const struct of_device_id tegra_hv_i2c_of_match[] = {
	{ .compatible = "nvidia,tegra124-i2c-hv", .data = NULL},
	{ .compatible = "nvidia,tegra210-i2c-hv", .data = NULL},
	{ .compatible = "nvidia,tegra186-i2c-hv", .data = NULL},
	{},
};
MODULE_DEVICE_TABLE(of, tegra_hv_i2c_of_match);

static int tegra_hv_i2c_probe(struct platform_device *pdev)
{
	struct tegra_hv_i2c_dev *i2c_dev;
	struct tegra_hv_i2c_platform_data *pdata = pdev->dev.platform_data;
	int ret = 0;
	const struct of_device_id *match;
	int bus_num = -1;
	void *chan;
	int err;
	struct resource *res;

	if (pdev->dev.of_node) {
		match = of_match_device(of_match_ptr(tegra_hv_i2c_of_match),
						     &pdev->dev);
		if (!match) {
			dev_err(&pdev->dev, "Device Not matching\n");
			return -ENODEV;
		}
		if (!pdata)
			pdata = parse_i2c_tegra_dt(pdev);
	} else {
		WARN(1, "Only device tree based init is supported\n");
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no mem resource\n");
		return -EINVAL;
	}

	i2c_dev = devm_kzalloc(&pdev->dev, sizeof(*i2c_dev), GFP_KERNEL);
	if (!i2c_dev) {
		dev_err(&pdev->dev, "Could not allocate struct tegra_hv_i2c_dev");
		return -ENOMEM;
	}

	chan = hv_i2c_comm_init(&pdev->dev, tegra_hv_i2c_isr, i2c_dev);
	if (IS_ERR(chan))
		return PTR_ERR(chan);

	i2c_dev->dev = &pdev->dev;
	i2c_dev->comm_chan = chan;

	platform_set_drvdata(pdev, i2c_dev);

	i2c_set_adapdata(&i2c_dev->adapter, i2c_dev);
	i2c_dev->adapter.owner = THIS_MODULE;
	i2c_dev->adapter.class = I2C_CLASS_HWMON;
	strlcpy(i2c_dev->adapter.name, "Tegra I2C HV adapter",
		sizeof(i2c_dev->adapter.name));
	i2c_dev->adapter.algo = &tegra_hv_i2c_algo;
	i2c_dev->adapter.dev.parent = &pdev->dev;
	i2c_dev->adapter.nr = bus_num;
	i2c_dev->adapter.dev.of_node = pdev->dev.of_node;

	if (pdata->retries)
		i2c_dev->adapter.retries = pdata->retries;
	else
		i2c_dev->adapter.retries = TEGRA_I2C_RETRIES;

	if (pdata->timeout)
		i2c_dev->adapter.timeout = pdata->timeout;

	if (i2c_dev->adapter.timeout)
		i2c_dev->completion_timeout = i2c_dev->adapter.timeout;
	else
		i2c_dev->completion_timeout = TEGRA_I2C_TIMEOUT;

	i2c_dev->base = res->start;
	init_completion(&i2c_dev->msg_complete);

	/* Send a cleanup message in case this is a reboot and we had a
	 * transaction in progress
	 */
	ret = hv_i2c_comm_chan_cleanup(i2c_dev->comm_chan, i2c_dev->base);
	if (ret < 0) {
		dev_warn(&pdev->dev, "Cleanup after (re)boot failed\n");
	} else {
		ret = wait_for_completion_timeout(&i2c_dev->msg_complete,
				i2c_dev->completion_timeout);
		if (ret == 0)
			dev_warn(&pdev->dev, "Timed out sending cleanup after (re)boot\n");
	}

	reinit_completion(&i2c_dev->msg_complete);

	ret = hv_i2c_get_max_payload(i2c_dev->comm_chan, i2c_dev->base,
				     &(i2c_dev->max_payload_size), &err);
	if (ret < 0) {
		dev_warn(&pdev->dev, "Could not get max payload, defaulting to 4096\n");
		i2c_dev->max_payload_size = 4096;
	} else {
		ret = wait_for_completion_timeout(&i2c_dev->msg_complete,
				i2c_dev->completion_timeout);
		if (ret == 0) {
			dev_warn(&pdev->dev, "Timed out getting max payload, defaulting to 4096\n");
			i2c_dev->max_payload_size = 4096;
		} else if (err != I2C_NO_ERROR) {
			dev_warn(&pdev->dev, "Error getting max payload, defaulting to 4096\n");
			i2c_dev->max_payload_size = 4096;
		}
	}

	ret = i2c_add_numbered_adapter(&i2c_dev->adapter);
	if (ret) {
		dev_err(&pdev->dev, "Failed to add I2C adapter\n");
		return ret;
	}

	return 0;
}

static int tegra_hv_i2c_remove(struct platform_device *pdev)
{
	struct tegra_hv_i2c_dev *i2c_dev = platform_get_drvdata(pdev);

	hv_i2c_comm_chan_free(i2c_dev->comm_chan);
	i2c_dev->comm_chan = NULL;
	i2c_del_adapter(&i2c_dev->adapter);
	return 0;
}

static void tegra_hv_i2c_shutdown(struct platform_device *pdev)
{
	struct tegra_hv_i2c_dev *i2c_dev = platform_get_drvdata(pdev);

	dev_info(i2c_dev->dev, "Bus is shutdown down..\n");
	i2c_shutdown_adapter(&i2c_dev->adapter);
}

static struct platform_device_id tegra_hv_i2c_devtype[] = {
	{
		.name = "tegra12-hv-i2c",
		.driver_data = 0,
	},

};

static struct platform_driver tegra_hv_i2c_driver = {
	.probe   = tegra_hv_i2c_probe,
	.remove  = tegra_hv_i2c_remove,
	.late_shutdown = tegra_hv_i2c_shutdown,
	.id_table = tegra_hv_i2c_devtype,
	.driver  = {
		.name  = "tegra-hv-i2c",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tegra_hv_i2c_of_match),
		.pm    = NULL,
	},
};

static int __init tegra_hv_i2c_init_driver(void)
{
	return platform_driver_register(&tegra_hv_i2c_driver);
}

static void __exit tegra_hv_i2c_exit_driver(void)
{
	platform_driver_unregister(&tegra_hv_i2c_driver);
}

subsys_initcall(tegra_hv_i2c_init_driver);
module_exit(tegra_hv_i2c_exit_driver);

MODULE_DESCRIPTION("nVidia Tegra Hypervisor I2C Bus Controller driver");
MODULE_AUTHOR("Arnab Basu");
MODULE_LICENSE("GPL v2");
