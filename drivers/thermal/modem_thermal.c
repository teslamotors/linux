/*
 * Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/platform_data/modem_thermal.h>
#include <linux/thermal.h>
#include <linux/slab.h>

struct modem_thermal_data {
	unsigned int num_zones;
	unsigned int sensors_active;
	struct modem_zone_info *zi;
	struct device *dev;
};

struct modem_zone_info {
	long temp;
	struct thermal_zone_device *tzd;
};

static ssize_t temp_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	const char *buff;
	int i;
	unsigned int num_tokens = 1;
	long new_temp;
	struct modem_thermal_data *mtd = dev_get_drvdata(dev);

	if (!buf)
		goto err;

	buff = buf;
	while ((buff = strpbrk(buff + 1, ",")))
		num_tokens++;

	if (num_tokens != mtd->num_zones)
		return -EINVAL;

	buff = buf;
	for (i = 0; i < num_tokens; i++, buff++) {
		if (sscanf(buff, "%ld", &new_temp) != 1)
			goto err;

		if (new_temp != mtd->zi[i].temp) {
			mtd->zi[i].temp = new_temp;
			thermal_zone_device_update(mtd->zi[i].tzd);
		}
		buff = strpbrk(buff, ",");
		if (!buff)
			break;
	}

	return count;
err:
	return -EINVAL;
}
static DEVICE_ATTR(temp, S_IWUSR, NULL, temp_store);

static ssize_t sensors_active_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct modem_thermal_data *mtd = dev_get_drvdata(dev);

	return sprintf(buf, "%lu\n", mtd->sensors_active);
}

static ssize_t sensors_active_store(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct modem_thermal_data *mtd = dev_get_drvdata(dev);
	int enabled;

	if (sscanf(buf, "%d", &enabled) != 1 || enabled < 0 || enabled > 1)
		return -EINVAL;

	mtd->sensors_active = enabled;

	return count;
}
static DEVICE_ATTR(sensors_active, S_IRUGO | S_IWUSR, sensors_active_show,
		sensors_active_store);

static int modem_thermal_get_temp(struct thermal_zone_device *tzd, long *t)
{
	struct modem_thermal_data *mtd =
				(struct modem_thermal_data *)tzd->devdata;
	struct modem_zone_info *zi = NULL;
	int i;

	for (i = 0; i < mtd->num_zones; i++)
		if (mtd->zi[i].tzd == tzd) {
			zi = &mtd->zi[i];
			break;
		}

	if (!zi)
		return -EINVAL;

	if (!zi->temp)
		return -EINVAL;

	if (!mtd->sensors_active)
		return -EBUSY;

	*t = zi->temp;

	return 0;
}
static struct thermal_zone_device_ops modem_thermal_ops = {
	.get_temp = modem_thermal_get_temp,
};

static int modem_thermal_probe(struct platform_device *pdev)
{
	struct modem_thermal_platform_data *pdata = pdev->dev.platform_data;
	struct modem_thermal_data *mtd;
	char tz_name[THERMAL_NAME_LENGTH];
	int i;
	int ret;

	mtd = kzalloc(sizeof(struct modem_thermal_data), GFP_KERNEL);
	if (!mtd) {
		dev_err(&pdev->dev, "failed to allocated memory\n");
		ret = -ENOMEM;
		return ret;
	}

	mtd->num_zones = pdata->num_zones;

	mtd->zi = kzalloc(mtd->num_zones * sizeof(struct modem_zone_info),
			GFP_KERNEL);
	if (!mtd->zi) {
		dev_err(&pdev->dev, "failed to allocate zones\n");
		ret = -ENOMEM;
		goto err_mtd_free;
	}

	for (i = 0; i < mtd->num_zones; i++) {
		snprintf(&tz_name, THERMAL_NAME_LENGTH, "modem%d", i);
		mtd->zi[i].tzd = thermal_zone_device_register(&tz_name, 0, 0,
				mtd, &modem_thermal_ops, NULL, 0, 0);
		if (IS_ERR(mtd->zi[i].tzd)) {
			dev_err(&pdev->dev, "error in therm reg\n");
			ret = -EINVAL;
			goto err_tzd_unregister;
		}
	}

	if (device_create_file(&pdev->dev, &dev_attr_temp)) {
		dev_err(&pdev->dev, "can't create temp sysfs file\n");
		ret = -EINVAL;
		goto err_remove_temp_file;
	}

	if (device_create_file(&pdev->dev, &dev_attr_sensors_active)) {
		dev_err(&pdev->dev, "can't create sensors sysfs file\n");
		ret = -EINVAL;
		goto err_remove_sensor_file;
	}

	mtd->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, mtd);

	return 0;

err_remove_sensor_file:
	device_remove_file(&pdev->dev, &dev_attr_sensors_active);

err_remove_temp_file:
	device_remove_file(&pdev->dev, &dev_attr_temp);

err_tzd_unregister:
	for (i = 0; i < mtd->num_zones; i++)
		if (!IS_ERR(mtd->zi[i].tzd))
			thermal_zone_device_unregister(mtd->zi[i].tzd);
	kfree(mtd->zi);
err_mtd_free:
	kfree(mtd);
	return ret;
}

static int modem_thermal_remove(struct platform_device *pdev)
{
	struct modem_thermal_data *mtd = platform_get_drvdata(pdev);
	int i;

	device_remove_file(&pdev->dev, &dev_attr_temp);
	device_remove_file(&pdev->dev, &dev_attr_sensors_active);

	for (i = 0; i < mtd->num_zones; i++)
		if (!IS_ERR(mtd->zi[i].tzd))
			thermal_zone_device_unregister(mtd->zi[i].tzd);

	kfree(mtd->zi);
	kfree(mtd);

	return 0;
}

static struct platform_driver modem_thermal_driver = {
	.probe = modem_thermal_probe,
	.remove = modem_thermal_remove,
	.driver = {
		.name = "modem-thermal",
		.owner = THIS_MODULE,
	},
};
module_platform_driver(modem_thermal_driver);

MODULE_DESCRIPTION("Modem thermal driver");
MODULE_AUTHOR("Neil Patel<neilp@nvidia.com>");
MODULE_ALIAS("platform:modem-thermal");
MODULE_LICENSE("GPL v2");
