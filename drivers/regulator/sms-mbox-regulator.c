/*
 * Copyright (C) 2018 Tesla, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/mfd/sms-mbox.h>

/* voltage */
struct sms_mbox_regulator_pdata {
	struct mutex lock;
};

struct sms_mbox_regulator_info {
	struct regulator_desc desc;
	struct regulator_dev *regdev;
	struct trav_mbox *mbox;
	int id;
};

static struct of_regulator_match sms_mbox_regulator_match[] = {
	{
		.name = SMS_MBOX_VOLTAGE_TURBO_INT_NAME,
		.driver_data = (void *) SMS_MBOX_VOLTAGE_TURBO_INT_COMP_ID,
	},
	{
		.name = SMS_MBOX_VOLTAGE_TURBO_CPU_NAME,
		.driver_data = (void *) SMS_MBOX_VOLTAGE_TURBO_CPU_COMP_ID,
	},
	{
		.name = SMS_MBOX_VOLTAGE_TURBO_GPU_NAME,
		.driver_data = (void *) SMS_MBOX_VOLTAGE_TURBO_GPU_COMP_ID,
	},
	{
		.name = SMS_MBOX_VOLTAGE_TURBO_TRIP0_NAME,
		.driver_data = (void *) SMS_MBOX_VOLTAGE_TURBO_TRIP0_COMP_ID,
	},
	/* trip1 share volt with trip0 */
};

static int sms_mbox_regulator_get_voltage(struct regulator_dev *regdev)
{
	struct sms_mbox_regulator_info *info = rdev_get_drvdata(regdev);

	return sms_mbox_get_voltage(info->mbox, info->id);
}

static int sms_mbox_regulator_set_voltage(struct regulator_dev *regdev,
                int min_uV, int max_uV, unsigned *selector)
{
	struct sms_mbox_regulator_info *info = rdev_get_drvdata(regdev);

	return sms_mbox_set_voltage(info->mbox, info->id, min_uV);
}

static struct regulator_ops sms_mbox_regulator_ops = {
	.get_voltage = sms_mbox_regulator_get_voltage,
	.set_voltage = sms_mbox_regulator_set_voltage,
};

static struct sms_mbox_regulator_info sms_mbox_regulator_info[] = {
	{
		.desc = { .name = SMS_MBOX_VOLTAGE_TURBO_INT_NAME, },
	},
	{
		.desc = { .name = SMS_MBOX_VOLTAGE_TURBO_CPU_NAME, },
	},
	{
		.desc = { .name = SMS_MBOX_VOLTAGE_TURBO_GPU_NAME, },
	},
	{
		.desc = { .name = SMS_MBOX_VOLTAGE_TURBO_TRIP0_NAME, },
	},
	/* trip1 share volt with trip0 */
};

static int sms_mbox_regulator_register(struct platform_device *pdev,
					struct trav_mbox *mbox, int comp_id,
					struct regulator_init_data *init_data,
					int id, struct device_node *np)
{
	struct sms_mbox_regulator_info *info = NULL;
	struct regulator_config config = { };

	info = &sms_mbox_regulator_info[id];

	info->mbox = mbox;
	info->id = comp_id;

	dev_info(&pdev->dev, "%s comp_id: %d\n", info->desc.name, comp_id);

	info->desc.type = REGULATOR_VOLTAGE;
	info->desc.owner = THIS_MODULE;
	info->desc.continuous_voltage_range = true;

	info->desc.ops = &sms_mbox_regulator_ops;

	config.dev = &pdev->dev;
	config.init_data = init_data;
	config.driver_data = info;
	config.of_node = np;

	/* register regulator with framework */
	info->regdev = devm_regulator_register(&pdev->dev, &info->desc, &config);
	if (IS_ERR(info->regdev)) {
		dev_err(&pdev->dev, "failed to register regulator %s\n",
			info->desc.name);
		return PTR_ERR(info->regdev);
	}

	return 0;
}

static int sms_mbox_regulator_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np;
	struct trav_mbox *mbox;
	struct sms_mbox_regulator_pdata *pdata;
	int i, ret = 0;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;
	mutex_init(&pdata->lock);
	platform_set_drvdata(pdev, pdata);

	np = of_get_child_by_name(dev->parent->of_node, "regulators");
	if (!np)
		return -ENODEV;

	ret = of_regulator_match(dev, np,
				 sms_mbox_regulator_match,
				 ARRAY_SIZE(sms_mbox_regulator_match));
	of_node_put(np);
	if (ret < 0) {
		dev_err(dev, "Error parsing regulator init data: %d\n", ret);
		return ret;
	}

	mbox = dev_get_drvdata(dev->parent);

	for (i = 0; i < ARRAY_SIZE(sms_mbox_regulator_match); i++) {
		ret = sms_mbox_regulator_register(pdev, mbox,
				(int) (unsigned long) sms_mbox_regulator_match[i].driver_data,
				sms_mbox_regulator_match[i].init_data, i,
				sms_mbox_regulator_match[i].of_node);
		if (ret)
			return ret;
	}

	return 0;
}

static const struct platform_device_id sms_mbox_regulator_table[] = {
	{ .name = SMS_MBOX_REGULATOR_NAME },
	{ /* Sentinel */ }
};

MODULE_DEVICE_TABLE(platform, sms_mbox_regulator_table);

static struct platform_driver sms_mbox_regulator_driver = {
	.id_table = sms_mbox_regulator_table,
	.driver = {
		.name = SMS_MBOX_REGULATOR_NAME,
	},
	.probe  = sms_mbox_regulator_probe,
};

module_platform_driver(sms_mbox_regulator_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("TRAV SMS mailbox regulator driver module");
