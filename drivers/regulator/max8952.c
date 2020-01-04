/*
 * max8952.c -- support regulators in max8952
 *
 * Copyright (C) 2010 Gyungoh Yoo <jack.yoo@maxim-ic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/regulator/driver.h>
#include <linux/slab.h>
#include <linux/regulator/max8952.h>
#include <linux/platform_device.h>

#define VOLTAGE_TO_VALUE(v) (((v) - 750000) / 10000)
#define VALUE_TO_VOLTAGE(val) ((val) * 10000 + 750000)

struct max8952_info {
	int voltages_count;
	const int *voltages_list;
	u8 reg_base;
	struct regulator_desc desc;
};

#define REG(ids, base, list) \
	{ \
		.voltages_list = (list), \
		.reg_base = (base), \
		.desc = { \
			.name = #ids, \
			.id = MAX8952_##ids, \
			.n_voltages = ARRAY_SIZE((list)), \
			.ops = &max8952_ops, \
			.type = REGULATOR_VOLTAGE, \
			.owner = THIS_MODULE, \
		}, \
	}

static const int max8952_mode0_voltages[] = {
	750000, 760000, 1260000, 1270000, 1280000, 1370000, 1380000
};

static const int max8952_mode1_voltages[] = {
	750000, 760000, 1040000, 1050000, 1060000, 1370000, 1380000
};

static const int max8952_mode2_voltages[] = {
	750000, 760000, 1210000, 1220000, 1230000, 1370000, 1380000
};

static const int max8952_mode3_voltages[] = {
	750000, 760000, 1040000, 1050000, 1060000, 1370000, 1380000
};

static int max8952_list_voltage(struct regulator_dev *dev, unsigned index);
static int max8952_set_voltage(struct regulator_dev *dev, int min_uV,
			       int max_uV);
static int max8952_get_voltage(struct regulator_dev *dev);

static struct regulator_ops max8952_ops = {
	.list_voltage = max8952_list_voltage,
	.set_voltage = max8952_set_voltage,
	.get_voltage = max8952_get_voltage,
};

static struct max8952_info max8952_regulators[] = {
	REG(MODE0, MAX8952_REG_MODE0, max8952_mode0_voltages),
	REG(MODE1, MAX8952_REG_MODE1, max8952_mode1_voltages),
	REG(MODE2, MAX8952_REG_MODE2, max8952_mode2_voltages),
	REG(MODE3, MAX8952_REG_MODE3, max8952_mode3_voltages),
};

#define MAX8952_REGULATOR_CNT (ARRAY_SIZE(max8952_regulators))

struct max8952 {
	struct i2c_client *i2c;
	struct mutex io_lock;
	struct regulator_dev *rdev[MAX8952_REGULATOR_CNT];
};

static int max8952_i2c_read(struct i2c_client *i2c, u8 reg, u8 count, u8 * dest)
{
	struct i2c_msg xfer[2];
	int ret;

	xfer[0].addr = i2c->addr;
	xfer[0].flags = I2C_M_NOSTART;
	xfer[0].len = 1;
	xfer[0].buf = &reg;

	xfer[1].addr = i2c->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = count;
	xfer[1].buf = dest;

	ret = i2c_transfer(i2c->adapter, xfer, 2);
	if (ret == 2)
		ret = 0;
	else if (ret >= 0)
		ret = -EIO;

	return ret;
}

static int max8952_i2c_write(struct i2c_client *i2c, u8 reg, u8 count,
			     const u8 *src)
{
	u8 msg[0x100 + 1];
	int ret;

	msg[0] = reg;
	memcpy(&msg[1], src, count);

	ret = i2c_master_send(i2c, msg, count + 1);
	if (ret < 0)
		return ret;
	if (ret != count + 1)
		return -EIO;
	return 0;
}

static u8 max8952_read(struct max8952 *max8952, u8 reg)
{
	u8 val;
	int ret;

	mutex_lock(&max8952->io_lock);

	ret = max8952_i2c_read(max8952->i2c, reg, 1, &val);

	mutex_unlock(&max8952->io_lock);
	pr_debug("max8952: reg read  reg=%x, val=%x\n", (unsigned int)reg,
		 (unsigned int)val);

	if (ret < 0)
		pr_err("Failed to read max8952 I2C driver: %d\n", ret);
	return val;
}

static int max8952_write(struct max8952 *max8952, u8 reg, u8 val)
{
	int ret;

	pr_debug("max8952: reg write  reg=%x, val=%x\n", (unsigned int)reg,
		 (unsigned int)val);
	mutex_lock(&max8952->io_lock);

	ret = max8952_i2c_write(max8952->i2c, reg, 1, &val);

	mutex_unlock(&max8952->io_lock);

	if (ret < 0)
		pr_err("Failed to write max8952 I2C driver: %d\n", ret);
	return ret;
}

int max8952_set_bits(struct max8952 *max8952, u8 reg, u8 mask, u8 val)
{
	u8 tmp;
	int ret;

	pr_debug("max8952: reg write  reg=%02X, val=%02X, mask=%02X\n",
		 (unsigned int)reg, (unsigned int)val, (unsigned int)mask);
	mutex_lock(&max8952->io_lock);

	ret = max8952_i2c_read(max8952->i2c, reg, 1, &tmp);
	if (ret == 0) {
		val = (tmp & ~mask) | (val & mask);
		ret = max8952_i2c_write(max8952->i2c, reg, 1, &val);
	}

	mutex_unlock(&max8952->io_lock);

	if (ret != 0)
		pr_err("Failed to write max8952 I2C driver: %d\n", ret);
	return ret;
}

static int max8952_list_voltage(struct regulator_dev *rdev, unsigned index)
{
	const struct max8952_info *reg = &max8952_regulators[rdev_get_id(rdev)];

	return reg->voltages_list[index];
}

static int max8952_set_voltage(struct regulator_dev *rdev, int min_uV,
			       int max_uV)
{
	struct max8952 *max8952 = rdev_get_drvdata(rdev);
	const struct max8952_info *reg = &max8952_regulators[rdev_get_id(rdev)];
	int val = -1;
	int voltage;
	int i;

	for (i = 0; i < reg->desc.n_voltages; i++) {
		voltage = reg->voltages_list[i];
		if (min_uV <= voltage && voltage <= max_uV) {
			val = VOLTAGE_TO_VALUE(voltage);
			break;
		}
	}
	if (val == -1)
		return -EDOM;

	return max8952_set_bits(max8952, reg->reg_base, MAX8952_MASK_OUTMODE,
				val);
}

static int max8952_get_voltage(struct regulator_dev *rdev)
{
	struct max8952 *max8952 = rdev_get_drvdata(rdev);
	const struct max8952_info *reg = &max8952_regulators[rdev_get_id(rdev)];
	int val;

	val = max8952_read(max8952, reg->reg_base);
	val &= MAX8952_MASK_OUTMODE;

	return VALUE_TO_VOLTAGE(val);
}

static int __devinit max8952_probe(struct i2c_client *i2c,
				   const struct i2c_device_id *i2c_id)
{
	struct max8952 *max8952;
	struct max8952_platform_data *pdata = i2c->dev.platform_data;
	struct regulator_dev *rdev;
	int id;
	int i;

	max8952 = kzalloc(sizeof(struct max8952), GFP_KERNEL);
	if (max8952 == NULL)
		return -ENOMEM;

	max8952->i2c = i2c;
	mutex_init(&max8952->io_lock);

	for (i = 0; i < pdata->num_subdevs; i++) {
		id = pdata->subdevs[i]->id;
		rdev =
		    regulator_register(&max8952_regulators[id].desc, &i2c->dev,
				       (struct regulator_init_data *)pdata->
				       subdevs[i]->dev.platform_data, max8952);
		if (IS_ERR(rdev)) {
			dev_err(&i2c->dev,
				"Cannot register regulator \"%s\", %ld\n",
				max8952_regulators[id].desc.name,
				PTR_ERR(rdev));
			goto error;
		}
		max8952->rdev[id] = rdev;
	}

	i2c_set_clientdata(i2c, max8952);
	return 0;

error:
	kfree(max8952);
	return PTR_ERR(rdev);
}

static int __devexit max8952_remove(struct i2c_client *i2c)
{
	struct max8952 *max8952 = i2c_get_clientdata(i2c);
	int i;

	for (i = 0; i < MAX8952_REGULATOR_CNT; i++) {
		if (max8952->rdev[i])
			regulator_unregister(max8952->rdev[i]);
	}
	kfree(max8952);

	return 0;
}

static const struct i2c_device_id max8952_id[] = {
	{"max8952", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, max8952_id);

static struct i2c_driver max8952_driver = {
	.probe = max8952_probe,
	.remove = __devexit_p(max8952_remove),
	.driver = {
		   .name = "max8952",
		   .owner = THIS_MODULE,
		   },
	.id_table = max8952_id,
};

static int __init max8952_init(void)
{
	return i2c_add_driver(&max8952_driver);
}

subsys_initcall(max8952_init);

static void __exit max8952_exit(void)
{
	i2c_del_driver(&max8952_driver);
}

module_exit(max8952_exit);

MODULE_DESCRIPTION("MAX8952 regulator driver");
MODULE_AUTHOR("Gyungoh Yoo <jack.yoo@maxim-ic.com>");
MODULE_LICENSE("GPL");
