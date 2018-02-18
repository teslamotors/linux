/*
 * Driver for Maxim 77620 thermal
 *
 * Copyright (c) 2014 - 2015 NVIDIA Corporation. All rights reserved.
 *
 * Author: Mallikarjun Kasoju <mkasoju@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/thermal.h>
#include <linux/of.h>
#include <linux/mfd/max77620.h>

#define MAX77620_NORMAL_OPERATING_TEMP 100000

struct max77620_therm_zone {
	struct device			*dev;
	struct max77620_chip			*max77620;
	struct thermal_zone_device	*tz_device;
	int				irq_tjalarm1;
	int				irq_tjalarm2;
	long				hd_threshold_temp;
};

static int max77620_thermal_read_temp(void *data, long *temp)
{
	struct max77620_therm_zone *ptherm_zone = data;
	u8 val;
	int ret;

	ret = max77620_reg_read(ptherm_zone->max77620->dev, MAX77620_PWR_SLAVE,
				MAX77620_REG_STATLBT, &val);
	if (ret < 0) {
		dev_err(ptherm_zone->dev, "Failed to read STATLBT, %d\n", ret);
		return -EINVAL;
	}

	if (val & MAX77620_IRQ_TJALRM2_MASK)
		*temp = 145000;
	else if (val & MAX77620_IRQ_TJALRM1_MASK)
		*temp = 125000;
	else
		*temp = MAX77620_NORMAL_OPERATING_TEMP;
	return 0;
}

static irqreturn_t max77620_thermal_irq(int irq, void *data)
{
	struct max77620_therm_zone *ptherm_zone = data;

	if (ptherm_zone->irq_tjalarm1 == irq)
		dev_info(ptherm_zone->dev,
			"Junction Temp Alarm1(120C) occured\n");
	else if (ptherm_zone->irq_tjalarm2 == irq)
		dev_info(ptherm_zone->dev,
			"Junction Temp Alarm2(140C) occured\n");
	else
		dev_info(ptherm_zone->dev, "Unknown interrupt %d\n", irq);

	thermal_zone_device_update(ptherm_zone->tz_device);
	return IRQ_HANDLED;
}

static int max77620_thermal_probe(struct platform_device *pdev)
{
	struct max77620_chip *max77620 = dev_get_drvdata(pdev->dev.parent);
	struct device_node *np = pdev->dev.parent->of_node;
	struct max77620_therm_zone *ptherm_zone;
	u32 hd_threshold_temp = 0;
	u32 pval;
	int ret, irq_tjalarm1, irq_tjalarm2;

	irq_tjalarm1 = platform_get_irq(pdev, 0);
	irq_tjalarm2 = platform_get_irq(pdev, 1);

	if (!pdev->dev.of_node)
		pdev->dev.of_node = np;

	if (np) {
		ret = of_property_read_u32(np,
			"maxim,hot-die-threshold-temp", &pval);
		if (!ret)
			hd_threshold_temp = pval;
	}

	if (!hd_threshold_temp) {
		dev_err(&pdev->dev, "Hot die temp is not provided\n");
		return -EINVAL;
	}

	ptherm_zone = devm_kzalloc(&pdev->dev, sizeof(*ptherm_zone),
			GFP_KERNEL);
	if (!ptherm_zone) {
		dev_err(&pdev->dev, "No available free memory\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, ptherm_zone);
	ptherm_zone->dev = &pdev->dev;
	ptherm_zone->max77620 = max77620;
	ptherm_zone->hd_threshold_temp = hd_threshold_temp;

	ptherm_zone->tz_device = thermal_zone_of_sensor_register(&pdev->dev, 0,
					ptherm_zone, max77620_thermal_read_temp,
					NULL);
	if (IS_ERR(ptherm_zone->tz_device)) {
		ret = PTR_ERR(ptherm_zone->tz_device);
		dev_err(ptherm_zone->dev,
			"Device can not register as thermal sensor: %d\n", ret);
		return ret;
	}

	ptherm_zone->irq_tjalarm1 = irq_tjalarm1;
	ptherm_zone->irq_tjalarm2 = irq_tjalarm2;
	ret = request_threaded_irq(ptherm_zone->irq_tjalarm1, NULL,
			max77620_thermal_irq,
			IRQF_ONESHOT, dev_name(&pdev->dev),
			ptherm_zone);
	if (ret < 0) {
		dev_err(&pdev->dev, "request irq1 %d failed: %dn",
			ptherm_zone->irq_tjalarm1, ret);
		goto int_req_failed1;
	}

	ret = request_threaded_irq(ptherm_zone->irq_tjalarm2, NULL,
			max77620_thermal_irq,
			IRQF_ONESHOT, dev_name(&pdev->dev),
			ptherm_zone);
	if (ret < 0) {
		dev_err(&pdev->dev, "request irq2 %d failed: %dn",
			ptherm_zone->irq_tjalarm2, ret);
		goto int_req_failed2;
	}
	return 0;

int_req_failed2:
	free_irq(ptherm_zone->irq_tjalarm1, ptherm_zone);
int_req_failed1:
	thermal_zone_of_sensor_unregister(&pdev->dev, ptherm_zone->tz_device);
	return ret;
}

static int max77620_thermal_remove(struct platform_device *pdev)
{
	struct max77620_therm_zone *ptherm_zone = platform_get_drvdata(pdev);

	free_irq(ptherm_zone->irq_tjalarm1, ptherm_zone);
	free_irq(ptherm_zone->irq_tjalarm2, ptherm_zone);
	thermal_zone_of_sensor_unregister(&pdev->dev, ptherm_zone->tz_device);
	return 0;
}

static struct platform_device_id max77620_thermal_devtype[] = {
	{
		.name = "max77620-thermal",
	},
	{
		.name = "max20024-thermal",
	},
};

static struct platform_driver max77620_thermal_driver = {
	.probe = max77620_thermal_probe,
	.remove = max77620_thermal_remove,
	.id_table = max77620_thermal_devtype,
	.driver = {
		.name = "max77620-thermal",
		.owner = THIS_MODULE,
	},
};

module_platform_driver(max77620_thermal_driver);

MODULE_DESCRIPTION("Max77620 Thermal driver");
MODULE_AUTHOR("Mallikarjun Kasoju<mkasoju@nvidia.com>");
MODULE_ALIAS("platform:max77620-thermal");
MODULE_LICENSE("GPL v2");
