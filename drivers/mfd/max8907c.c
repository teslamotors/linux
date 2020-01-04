/*
 * max8907c.c - mfd driver for MAX8907c
 *
 * Copyright (C) 2010 Gyungoh Yoo <jack.yoo@maxim-ic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mfd/core.h>
#include <linux/mfd/max8907c.h>

static struct mfd_cell cells[] = {
	{.name = "max8907-regulator",},
};

int max8907c_reg_read(struct device *dev, u8 reg)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct max8907c *max8907c = dev_get_drvdata(dev);
	u8 val;
	int ret;

	mutex_lock(&max8907c->io_lock);

	ret = max8907c->read_dev(i2c, reg, 1, &val);

	mutex_unlock(&max8907c->io_lock);
	pr_debug("max8907c: reg read  reg=%x, val=%x\n",
		 (unsigned int)reg, (unsigned int)val);

	if (ret != 0)
		pr_err("Failed to read max8907c I2C driver: %d\n", ret);
	return val;
}
EXPORT_SYMBOL_GPL(max8907c_reg_read);

int max8907c_reg_write(struct device *dev, u8 reg, u8 val)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct max8907c *max8907c = dev_get_drvdata(dev);
	int ret;

	pr_debug("max8907c: reg write  reg=%x, val=%x\n",
		 (unsigned int)reg, (unsigned int)val);
	mutex_lock(&max8907c->io_lock);

	ret = max8907c->write_dev(i2c, reg, 1, &val);

	mutex_unlock(&max8907c->io_lock);

	if (ret != 0)
		pr_err("Failed to write max8907c I2C driver: %d\n", ret);
	return ret;
}
EXPORT_SYMBOL_GPL(max8907c_reg_write);

int max8907c_set_bits(struct device *dev, u8 reg, u8 mask, u8 val)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct max8907c *max8907c = dev_get_drvdata(dev);
	u8 tmp;
	int ret;

	pr_debug("max8907c: reg write  reg=%02X, val=%02X, mask=%02X\n",
		 (unsigned int)reg, (unsigned int)val, (unsigned int)mask);
	mutex_lock(&max8907c->io_lock);

	ret = max8907c->read_dev(i2c, reg, 1, &tmp);
	if (ret == 0) {
		val = (tmp & ~mask) | (val & mask);
		ret = max8907c->write_dev(i2c, reg, 1, &val);
	}

	mutex_unlock(&max8907c->io_lock);

	if (ret != 0)
		pr_err("Failed to write max8907c I2C driver: %d\n", ret);
	return ret;
}
EXPORT_SYMBOL_GPL(max8907c_set_bits);

static int max8907c_i2c_read(void *io_data, u8 reg, u8 count, u8 * dest)
{
	struct i2c_client *i2c = (struct i2c_client *)io_data;
	struct i2c_msg xfer[2];
	int ret = 0;

	xfer[0].addr = i2c->addr;
	xfer[0].flags = I2C_M_NOSTART;
	xfer[0].len = 1;
	xfer[0].buf = &reg;

	xfer[1].addr = i2c->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = count;
	xfer[1].buf = dest;

	ret = i2c_transfer(i2c->adapter, xfer, 2);
	if (ret < 0)
		return ret;
	if (ret != 2)
		return -EIO;

	return 0;
}

static int max8907c_i2c_write(void *io_data, u8 reg, u8 count, const u8 * src)
{
	struct i2c_client *i2c = (struct i2c_client *)io_data;
	u8 msg[0x100 + 1];
	int ret = 0;

	msg[0] = reg;
	memcpy(&msg[1], src, count);

	ret = i2c_master_send(i2c, msg, count + 1);
	if (ret < 0)
		return ret;
	if (ret != count + 1)
		return -EIO;

	return 0;
}

static int max8907c_remove_subdev(struct device *dev, void *unused)
{
	platform_device_unregister(to_platform_device(dev));
	return 0;
}

static int max8907c_remove_subdevs(struct max8907c *max8907c)
{
	return device_for_each_child(max8907c->dev, NULL,
				     max8907c_remove_subdev);
}

static int max8097c_add_subdevs(struct max8907c *max8907c,
				struct max8907c_platform_data *pdata)
{
	struct platform_device *pdev;
	int ret;
	int i;

	for (i = 0; i < pdata->num_subdevs; i++) {
		pdev = platform_device_alloc(pdata->subdevs[i]->name,
					     pdata->subdevs[i]->id);

		pdev->dev.parent = max8907c->dev;
		pdev->dev.platform_data = pdata->subdevs[i]->dev.platform_data;

		ret = platform_device_add(pdev);
		if (ret)
			goto error;
	}
	return 0;

error:
	max8907c_remove_subdevs(max8907c);
	return ret;
}

static int max8907c_i2c_probe(struct i2c_client *i2c,
			      const struct i2c_device_id *id)
{
	struct max8907c *max8907c;
	struct max8907c_platform_data *pdata = i2c->dev.platform_data;
	int ret;
	int i;

	max8907c = kzalloc(sizeof(struct max8907c), GFP_KERNEL);
	if (max8907c == NULL)
		return -ENOMEM;

	max8907c->read_dev = max8907c_i2c_read;
	max8907c->write_dev = max8907c_i2c_write;
	max8907c->dev = &i2c->dev;
	i2c_set_clientdata(i2c, max8907c);

	mutex_init(&max8907c->io_lock);

	for (i = 0; i < ARRAY_SIZE(cells); i++)
		cells[i].driver_data = max8907c;
	ret = mfd_add_devices(max8907c->dev, -1, cells, ARRAY_SIZE(cells),
			      NULL, 0);
	if (ret != 0) {
		kfree(max8907c);
		pr_debug("max8907c: failed to add MFD devices   %X\n", ret);
		return ret;
	}

	ret = max8097c_add_subdevs(max8907c, pdata);

	return ret;
}

static int max8907c_i2c_remove(struct i2c_client *i2c)
{
	struct max8907c *max8907c = i2c_get_clientdata(i2c);

	mfd_remove_devices(max8907c->dev);
	kfree(max8907c);

	return 0;
}

static const struct i2c_device_id max8907c_i2c_id[] = {
	{"max8907c", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, max8907c_i2c_id);

static struct i2c_driver max8907c_i2c_driver = {
	.driver = {
		   .name = "max8907c",
		   .owner = THIS_MODULE,
		   },
	.probe = max8907c_i2c_probe,
	.remove = max8907c_i2c_remove,
	.id_table = max8907c_i2c_id,
};

static int __init max8907c_i2c_init(void)
{
	int ret = -ENODEV;

	ret = i2c_add_driver(&max8907c_i2c_driver);
	if (ret != 0)
		pr_err("Failed to register I2C driver: %d\n", ret);

	return ret;
}

subsys_initcall(max8907c_i2c_init);

static void __exit max8907c_i2c_exit(void)
{
	i2c_del_driver(&max8907c_i2c_driver);
}

module_exit(max8907c_i2c_exit);

MODULE_DESCRIPTION("MAX8907C multi-function core driver");
MODULE_AUTHOR("Gyungoh Yoo <jack.yoo@maxim-ic.com>");
MODULE_LICENSE("GPL");
