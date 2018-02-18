/*
 * palmas-ldousb-in.c : Dynamically select ldousb input based on inputs voltage.
 *
 * Copyright (c) 2014, NVIDIA Corporation.
 *
 * Author: Mallikarjun Kasoju <mkasoju@nvidia.com>
 *         Laxman Dewangan <ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mfd/palmas.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/mfd/palmas.h>
#include <linux/power_supply.h>

#define LDOUSB_INPUT1					0
#define LDOUSB_INPUT2					1
#define PALMAS_LDOSUB_IN_SEL_CHECK_DELAY		100
#define PALMAS_LDOSUB_IN_VOLTAGE_THRES_TOLERANCE	100

struct palmas_ldousb_in_info {
	struct palmas *palmas;
	struct device *dev;
	struct delayed_work	work;
	struct power_supply *pwr_supply;
	struct regulator *input_reg[2];
	int current_input;
	u32 ldousb_in_threshold_voltage;
	u32 threshold_voltage_tolerance;
	bool enable_in1_above_threshold;
	u32 current_switching_voltage;
};

static void palmas_ldousb_in_sel_work(struct work_struct *work)
{
	struct palmas_ldousb_in_info *ldousb_info = container_of(work,
				struct palmas_ldousb_in_info, work.work);
	struct power_supply *psy = ldousb_info->pwr_supply;
	union power_supply_propval val;
	int current_sys_voltage;
	bool sys_above_thresh;
	int sel, input_next;
	int ret;

	ret = psy->get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	if (ret < 0) {
		dev_err(ldousb_info->dev, "Battery voltage get failed: %d\n",
			ret);
		goto reschedule;
	}

	current_sys_voltage = val.intval;
	if (current_sys_voltage < 0) {
		dev_err(ldousb_info->dev, "System voltage is not proper\n");
		goto reschedule;
	}

	if (current_sys_voltage >= ldousb_info->current_switching_voltage) {
		sys_above_thresh = true;
		ldousb_info->current_switching_voltage =
				ldousb_info->ldousb_in_threshold_voltage;
	} else {
		sys_above_thresh = false;
		ldousb_info->current_switching_voltage =
			ldousb_info->ldousb_in_threshold_voltage +
				ldousb_info->threshold_voltage_tolerance;
	}

	if (ldousb_info->enable_in1_above_threshold)
		input_next = (sys_above_thresh) ? LDOUSB_INPUT1 : LDOUSB_INPUT2;
	else
		input_next = (sys_above_thresh) ? LDOUSB_INPUT2 : LDOUSB_INPUT1;

	if (ldousb_info->current_input == input_next)
		goto reschedule;

	sel = (input_next == LDOUSB_INPUT2) ?
			PALMAS_LDO_CTRL_LDOUSB_ON_VBUS_VSYS_IN2 :
				PALMAS_LDO_CTRL_LDOUSB_ON_VBUS_VSYS_IN1;
	ret = regulator_enable(ldousb_info->input_reg[input_next]);
	if (ret < 0) {
		dev_err(ldousb_info->dev, "regulator enable for input %d failed: %d\n",
				input_next, ret);
		goto reschedule;
	}

	ret = palmas_update_bits(ldousb_info->palmas, PALMAS_LDO_BASE,
				PALMAS_LDO_CTRL,
				PALMAS_LDO_CTRL_LDOUSB_ON_VBUS_VSYS_MASK, sel);
	if (ret < 0) {
		dev_err(ldousb_info->dev, "LDO_CTRL update failed: %d\n", ret);
		goto reschedule;
	}
	dev_info(ldousb_info->dev, "LDOUSB-IN%d selected\n", input_next + 1);
	ret = regulator_disable(ldousb_info->input_reg[
					ldousb_info->current_input]);
	if (ret < 0)
		dev_err(ldousb_info->dev, "regulator disable for input %d failed: %d\n",
				ldousb_info->current_input, ret);
	ldousb_info->current_input = input_next;

reschedule:
	schedule_delayed_work(&ldousb_info->work,
			PALMAS_LDOSUB_IN_SEL_CHECK_DELAY);
}

static int palmas_ldousb_in_probe(struct platform_device *pdev)
{
	struct palmas *palmas = dev_get_drvdata(pdev->dev.parent);
	struct palmas_ldousb_in_info *ldousb_info;
	struct palmas_platform_data *palmas_pdata;
	struct palmas_ldousb_in_platform_data *ldousb_pdata = NULL;
	struct device_node *node = pdev->dev.of_node;
	u32 pval;
	unsigned int val;
	int ret;

	ldousb_info = devm_kzalloc(&pdev->dev,
					sizeof(*ldousb_info), GFP_KERNEL);
	if (!ldousb_info) {
		dev_err(&pdev->dev, "Memory allocation failed.\n");
		return -ENOMEM;
	}

	ldousb_info->palmas = palmas;
	ldousb_info->dev = &pdev->dev;
	platform_set_drvdata(pdev, ldousb_info);

	palmas_pdata = dev_get_platdata(pdev->dev.parent);
	if (palmas_pdata && palmas_pdata->ldousb_in_pdata) {
		ldousb_pdata = palmas_pdata->ldousb_in_pdata;
		ldousb_info->ldousb_in_threshold_voltage =
				ldousb_pdata->ldousb_in_threshold_voltage;
		ldousb_info->threshold_voltage_tolerance =
				ldousb_pdata->threshold_voltage_tolerance;
		ldousb_info->enable_in1_above_threshold =
				ldousb_pdata->enable_in1_above_threshold;
	}

	if (!ldousb_pdata && pdev->dev.of_node) {
		ret = of_property_read_u32(node,
				"ti,ldousb-in-threshold-voltage", &pval);
		if (!ret)
			ldousb_info->ldousb_in_threshold_voltage = pval;

		ret = of_property_read_u32(node,
				"ti,threshold-voltage-tolerance", &pval);
		if (!ret)
			ldousb_info->threshold_voltage_tolerance = pval;

		ldousb_info->enable_in1_above_threshold =
				of_property_read_bool(node,
					"ti,enable-in1-above-threshold");
	}

	if (!ldousb_info->ldousb_in_threshold_voltage) {
		dev_err(&pdev->dev,
			"Invalid threshold voltage, can not be 0\n");
		return -EINVAL;
	}

	if (!ldousb_info->threshold_voltage_tolerance)
		ldousb_info->threshold_voltage_tolerance =
				PALMAS_LDOSUB_IN_VOLTAGE_THRES_TOLERANCE;

	ldousb_info->pwr_supply = power_supply_get_by_name("battery");
	if (!ldousb_info->pwr_supply) {
		dev_err(&pdev->dev, "Battery power supply is not available\n");
		return -ENODEV;
	}

	ldousb_info->input_reg[LDOUSB_INPUT1] = devm_regulator_get(&pdev->dev,
							"ldousb-in1");
	if (IS_ERR(ldousb_info->input_reg[LDOUSB_INPUT1])) {
		ret = PTR_ERR(ldousb_info->input_reg[LDOUSB_INPUT1]);
		dev_err(&pdev->dev, "ldousb-in1 reg get failed: %d\n", ret);
		return ret;
	}

	ldousb_info->input_reg[LDOUSB_INPUT2] = devm_regulator_get(&pdev->dev,
							"ldousb-in2");
	if (IS_ERR(ldousb_info->input_reg[LDOUSB_INPUT2])) {
		ret = PTR_ERR(ldousb_info->input_reg[LDOUSB_INPUT2]);
		dev_err(&pdev->dev, "ldousb-in2 reg get failed: %d\n", ret);
		return ret;
	}

	ret = palmas_read(palmas, PALMAS_LDO_BASE, PALMAS_LDO_CTRL, &val);
	if (ret < 0) {
		dev_err(&pdev->dev, "LDO_CTRL register read failed: %d\n", ret);
		return ret;
	}
	ldousb_info->current_input =
			(val & PALMAS_LDO_CTRL_LDOUSB_ON_VBUS_VSYS_MASK) ?
					LDOUSB_INPUT1 : LDOUSB_INPUT2;
	ldousb_info->current_switching_voltage =
				ldousb_info->ldousb_in_threshold_voltage;

	INIT_DEFERRABLE_WORK(&ldousb_info->work, palmas_ldousb_in_sel_work);
	schedule_delayed_work(&ldousb_info->work,
			PALMAS_LDOSUB_IN_SEL_CHECK_DELAY);
	return 0;
}

static int palmas_ldousb_in_remove(struct platform_device *pdev)
{
	struct palmas_ldousb_in_info *ldousb_info = platform_get_drvdata(pdev);

	cancel_delayed_work(&ldousb_info->work);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int palmas_ldousb_in_suspend(struct device *dev)
{
	struct palmas_ldousb_in_info *ldousb_info = dev_get_drvdata(dev);

	cancel_delayed_work(&ldousb_info->work);
	return 0;
}

static int palmas_ldousb_in_resume(struct device *dev)
{
	struct palmas_ldousb_in_info *ldousb_info = dev_get_drvdata(dev);

	schedule_delayed_work(&ldousb_info->work,
			PALMAS_LDOSUB_IN_SEL_CHECK_DELAY);
	return 0;
};
#endif

static const struct dev_pm_ops palmas_ldousb_in_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(palmas_ldousb_in_suspend,
				palmas_ldousb_in_resume)
};

static struct of_device_id of_palmas_ldousb_in_match_tbl[] = {
	{ .compatible = "ti,palmas-ldousb-in", },
	{ /* end */ }
};
MODULE_DEVICE_TABLE(of, of_palmas_ldousb_in_match_tbl);

static struct platform_driver palmas_ldousb_in_driver = {
	.probe		= palmas_ldousb_in_probe,
	.remove		= palmas_ldousb_in_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "palmas-ldousb-in",
		.pm = &palmas_ldousb_in_ops,
		.of_match_table = of_palmas_ldousb_in_match_tbl,
	},
};
module_platform_driver(palmas_ldousb_in_driver);

MODULE_ALIAS("platform:palmas-ldousb-in");
MODULE_AUTHOR("Mallikarjun Kasoju <mkasoju@nvidia.com>");
MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_LICENSE("GPL V2");
