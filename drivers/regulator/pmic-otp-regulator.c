/*
 * PMIC-OTP Regulator driver
 *
 * Copyright (C) 2015 NVIDIA CORPORATION. All rights reserved.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/err.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>

struct pmic_otp_regulator_info {
	struct device *dev;
	int current_vsel;
	int current_state;
	int current_mode;
	struct regulator_desc rdesc;
	struct regulator_init_data *ridata;
	struct regulator_dev *rdev;
};

static int pmic_otp_regulator_enable(struct regulator_dev *rdev)
{
	struct pmic_otp_regulator_info *rinfo = rdev_get_drvdata(rdev);

	rinfo->current_state  = 1;
	return 0;
}

static int pmic_otp_regulator_disable(struct regulator_dev *rdev)
{
	struct pmic_otp_regulator_info *rinfo = rdev_get_drvdata(rdev);

	rinfo->current_state  = 0;
	return 0;
}

static int pmic_otp_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct pmic_otp_regulator_info *rinfo = rdev_get_drvdata(rdev);

	return rinfo->current_state;
}

static int pmic_otp_regulator_set_voltage_sel(struct regulator_dev *rdev,
	unsigned vsel)
{
	struct pmic_otp_regulator_info *rinfo = rdev_get_drvdata(rdev);

	rinfo->current_vsel = vsel;
	return 0;
}

static int pmic_otp_regulator_get_voltage_sel(struct regulator_dev *rdev)
{
	struct pmic_otp_regulator_info *rinfo = rdev_get_drvdata(rdev);

	return rinfo->current_vsel;
}

static int pmic_otp_regulator_set_mode(struct regulator_dev *rdev,
				       unsigned int mode)
{
	struct pmic_otp_regulator_info *rinfo = rdev_get_drvdata(rdev);

	rinfo->current_mode = mode;
	return 0;
}

static unsigned int pmic_otp_regulator_get_mode(struct regulator_dev *rdev)
{
	struct pmic_otp_regulator_info *rinfo = rdev_get_drvdata(rdev);

	return rinfo->current_mode;
}

static struct regulator_ops pmic_otp_regulator_ops = {
	.is_enabled = pmic_otp_regulator_is_enabled,
	.enable = pmic_otp_regulator_enable,
	.disable = pmic_otp_regulator_disable,
	.list_voltage = regulator_list_voltage_linear,
	.map_voltage = regulator_map_voltage_linear,
	.get_voltage_sel = pmic_otp_regulator_get_voltage_sel,
	.set_voltage_sel = pmic_otp_regulator_set_voltage_sel,
	.set_mode = pmic_otp_regulator_set_mode,
	.get_mode = pmic_otp_regulator_get_mode,
};

static int pmic_otp_regulator_probe(struct platform_device *pdev)
{
	struct pmic_otp_regulator_info *pmic_otp;
	struct device_node *np = pdev->dev.of_node;
	struct regulator_desc *rdesc;
	struct regulator_config config = { };
	int ret = 0;

	if (!np) {
		dev_err(&pdev->dev, "Driver only support DT registration\n");
		return -ENODEV;
	}

	pmic_otp = devm_kzalloc(&pdev->dev, sizeof(*pmic_otp), GFP_KERNEL);
	if (!pmic_otp) {
		dev_err(&pdev->dev, "memory alloc failed\n");
		return -ENOMEM;
	}

	pmic_otp->dev = &pdev->dev;
	platform_set_drvdata(pdev, pmic_otp);

	pmic_otp->ridata = of_get_regulator_init_data(&pdev->dev, np);
	pmic_otp->ridata->constraints.always_on = 1;
	pmic_otp->ridata->constraints.valid_ops_mask &= ~REGULATOR_CHANGE_STATUS;

	rdesc = &pmic_otp->rdesc;
	rdesc->type = REGULATOR_VOLTAGE;
	rdesc->owner = THIS_MODULE;
	rdesc->ops = &pmic_otp_regulator_ops;
	rdesc->name = "pmic-otp-regulator";
	rdesc->id = 0;
	rdesc->min_uV = 0;
	rdesc->uV_step = 10000;
	rdesc->n_voltages = DIV_ROUND_UP(pmic_otp->ridata->constraints.max_uV,
					rdesc->uV_step) + 1;

	config.dev = &pdev->dev;
	config.init_data = pmic_otp->ridata;
	config.driver_data = pmic_otp;
	config.of_node = np;

	pmic_otp->rdev = devm_regulator_register(&pdev->dev, rdesc, &config);
	if (IS_ERR(pmic_otp->rdev)) {
		ret = PTR_ERR(pmic_otp->rdev);
		dev_err(&pdev->dev, "regulator %s register failed: %d\n",
				rdesc->name, ret);
		return ret;
	}

	return 0;
}

static const struct of_device_id pmic_otp_regulator_id[] = {
	{ .compatible = "regulator-pmic-otp", },
	{},
};
MODULE_DEVICE_TABLE(of, pmic_otp_regulator_id);

static struct platform_driver pmic_otp_regulator_driver = {
	.probe = pmic_otp_regulator_probe,
	.driver = {
		.name = "regulator-pmic-otp",
		.owner = THIS_MODULE,
		.of_match_table = pmic_otp_regulator_id,
	},
};

static int __init pmic_otp_regulator_init(void)
{
	return platform_driver_register(&pmic_otp_regulator_driver);
}
subsys_initcall(pmic_otp_regulator_init);

static void __exit pmic_otp_reg_exit(void)
{
	platform_driver_unregister(&pmic_otp_regulator_driver);
}
module_exit(pmic_otp_reg_exit);

MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_DESCRIPTION("Regulator PMIC OTP driver");
MODULE_ALIAS("platform:regulator-pmic-otp");
MODULE_LICENSE("GPL v2");
