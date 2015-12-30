/*
 * Copyright (c) 2013--2015 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 */



#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/wait.h>


#include "video-sensor-stub.h"
#define STUB_ADAPTER_NAME "i2c-adapter-stub"

static struct platform_device * stub_platform_device;
static const struct i2c_algorithm stub_is_i2c_algorithm;

struct stub_i2c {
	struct i2c_adapter adapter;
	struct clk *clock;
};

static int intel_ipu4_platform_probe(struct platform_device *pdev)
{

	struct device_node *node = pdev->dev.of_node;
	struct stub_i2c *isp_i2c;
	struct i2c_adapter *i2c_adap;
	int rval = -E2BIG;

	dev_info(&pdev->dev,"Dummy adapter Probe\n");

	isp_i2c = devm_kzalloc(&pdev->dev, sizeof(*isp_i2c), GFP_KERNEL);
	if (!isp_i2c) {
		dev_err(&pdev->dev, "Failed to alloc i2c adapter structure\n");
		rval = -ENOMEM;
		goto out;
	}

	i2c_adap = &isp_i2c->adapter;
	i2c_adap->dev.of_node = node;
	i2c_adap->dev.parent = &pdev->dev;
	strlcpy(i2c_adap->name, SENSOR_STUB_NAME, sizeof(i2c_adap->name));
	i2c_adap->owner = THIS_MODULE;
	i2c_adap->algo = &stub_is_i2c_algorithm;
	i2c_adap->class = I2C_CLASS_SPD;

	rval = i2c_add_adapter(i2c_adap);

	dev_info(&pdev->dev,"I2C adapter nr = %d \n ", i2c_adap->nr);

	if (rval < 0)
		goto out;

	dev_info(&pdev->dev,"Dummy adapter probe ok\n");
	return 0;

out:
	dev_err(&pdev->dev,"Dummy adapter probe exit failed %d\n", rval);
	return rval;

}

static int intel_ipu4_platform_remove(struct platform_device *pdev)
{
	struct stub_i2c *isp_i2c = platform_get_drvdata(pdev);
	i2c_del_adapter(&isp_i2c->adapter);
	return 0;
}

static const struct of_device_id adapter_stub_of_match[] = {
	{ .compatible = SENSOR_STUB_NAME  },
	{ },
};

static struct platform_device_id sensor_id_table[] = {
	{ STUB_ADAPTER_NAME, 0 },
	{ },
};

static struct platform_driver stub_platform_driver = {
	.driver = {
		.name = STUB_ADAPTER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = adapter_stub_of_match,
	},
	.probe = intel_ipu4_platform_probe,
	.remove = intel_ipu4_platform_remove,
	.id_table = sensor_id_table,
};

static int __init stub_init(void)
{
	int rval;
	stub_platform_device = platform_device_register_simple(STUB_ADAPTER_NAME, -1, NULL, 0);

	rval = platform_driver_register(&stub_platform_driver);
	if (rval) {
		pr_err("can't register driver (%d)\n", rval);
		platform_device_unregister(stub_platform_device);
	}
	return rval;
}

static void __exit stub_exit(void)
{
	platform_driver_unregister(&stub_platform_driver);
	platform_device_unregister(stub_platform_device);
	stub_platform_device = NULL;
}

module_init(stub_init);
module_exit(stub_exit);

MODULE_AUTHOR("Jouni Ukkonen <jouni.ukkonen@intel.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Intel dummy i2c adapter");
