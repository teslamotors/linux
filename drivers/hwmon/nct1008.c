/*
 * drivers/hwmon/nct1008.c
 *
 * Temperature Sensor driver for NCT1008 Temperature Monitor chip
 * manufactured by ON Semiconductors (www.onsemi.com).
 *
 * Copyright (c) 2010, NVIDIA Corporation.
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

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/i2c/nct1008.h>
#include <linux/hwmon.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/delay.h>

#define NCT1008_LOCAL_TEMP_RD			0x00
#define NCT1008_CONFIG_RD			0x03
#define NCT1008_MFR_ID_RD			0xFE

#define NCT1008_CONFIG_WR			0x09
#define NCT1008_CONV_RATE_WR			0x0A
#define NCT1008_OFFSET_WR			0x11
#define NCT1008_LOCAL_THERM_LIMIT_WR		0x20

#define DRIVER_NAME "nct1008"

struct nct1008_data {
	struct device *hwmon_dev;
	struct i2c_client *client;
	struct nct1008_platform_data plat_data;
};

static ssize_t nct1008_show_temp(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	signed int temp_value = 0;
	u8 data = 0;

	if (!dev || !buf || !attr)
		return -EINVAL;

	data = i2c_smbus_read_byte_data(client, NCT1008_LOCAL_TEMP_RD);
	if (data < 0) {
		dev_err(&client->dev, "%s: failed to read "
			"temperature\n", __func__);
		return -EINVAL;
	}

	temp_value = (signed int)data;
	return sprintf(buf, "%d\n", temp_value);
}

static DEVICE_ATTR(temperature, S_IRUGO, nct1008_show_temp, NULL);

static struct attribute *nct1008_attributes[] = {
	&dev_attr_temperature.attr,
	NULL
};

static const struct attribute_group nct1008_attr_group = {
	.attrs = nct1008_attributes,
};

static int __devinit nct1008_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct nct1008_data *pdata;
	int err;
	u8 data = 0;

	if (client->dev.platform_data == NULL) {
		dev_err(&client->dev, "platform data is NULL, exiting\n");
		return -ENODEV;
	}

	pdata = kzalloc(sizeof(struct nct1008_data), GFP_KERNEL);
	if (!pdata) {
		dev_err(&client->dev, "%s: failed to allocate "
			"device\n", __func__);
		return -ENOMEM;
	}

	pdata->client = client;
	i2c_set_clientdata(client, pdata);

	memcpy(&pdata->plat_data, client->dev.platform_data,
		sizeof(struct nct1008_platform_data));

	data = i2c_smbus_read_byte_data(client, NCT1008_MFR_ID_RD);
	if (data < 0) {
		dev_err(&client->dev, "%s: failed to read manufacturer "
			"id\n", __func__);
		err = data;
		goto fail_alloc;
	}
	dev_info(&client->dev, "%s:0x%x chip found\n", client->name, data);

	/* set conversion rate (conv/sec) */
	data = pdata->plat_data.conv_rate;
	err = i2c_smbus_write_byte_data(client, NCT1008_CONV_RATE_WR, data);
	if (err < 0) {
		dev_err(&client->dev, "%s: failed to set conversion "
			"rate\n", __func__);
		goto fail_alloc;
	}

	/* set config params */
	data = pdata->plat_data.config;
	err = i2c_smbus_write_byte_data(client, NCT1008_CONFIG_WR, data);
	if (err < 0) {
		dev_err(&client->dev, "%s: failed to set config\n", __func__);
		goto fail_alloc;
	}

	/* set offset value */
	data = pdata->plat_data.offset;
	err = i2c_smbus_write_byte_data(client, NCT1008_OFFSET_WR, data);
	if (err < 0) {
		dev_err(&client->dev,
				"%s: failed to set offset\n", __func__);
		goto fail_alloc;
	}

	/* set cpu shutdown threshold */
	data = pdata->plat_data.thermal_threshold;
	err = i2c_smbus_write_byte_data(client, NCT1008_LOCAL_THERM_LIMIT_WR, data);
	if (err < 0) {
		dev_err(&client->dev,
				"%s: failed to set THERM# limit\n", __func__);
		goto fail_alloc;
	}

	pdata->hwmon_dev = hwmon_device_register(&client->dev);
	if (IS_ERR(pdata->hwmon_dev)) {
		err = PTR_ERR(pdata->hwmon_dev);
		dev_err(&client->dev, "%s: hwmon_device_register "
			"failed\n", __func__);
		goto fail_alloc;
	}

	/* register sysfs hooks */
	err = sysfs_create_group(&client->dev.kobj, &nct1008_attr_group);
	if (err < 0)
		goto fail_sys;

	dev_info(&client->dev, "%s: initialized\n", __func__);
	return 0;

fail_sys:
	hwmon_device_unregister(pdata->hwmon_dev);
fail_alloc:
	kfree(pdata);
	return err;
}

static int __devexit nct1008_remove(struct i2c_client *client)
{
	struct nct1008_data *pdata = i2c_get_clientdata(client);

	if (!pdata)
		return -EINVAL;

	hwmon_device_unregister(pdata->hwmon_dev);
	kfree(pdata);
	return 0;
}

#ifdef CONFIG_PM
static int nct1008_suspend(struct i2c_client *client, pm_message_t state)
{
	u8 config;
	int err;

	config = i2c_smbus_read_byte_data(client, NCT1008_CONFIG_RD);
	if (config < 0) {
		dev_err(&client->dev, "%s: failed to read config\n", __func__);
		return -EIO;
	}

	/* take device to standby state */
	config |= NCT1008_CONFIG_RUN_STANDBY;

	err = i2c_smbus_write_byte_data(client, NCT1008_CONFIG_WR, config);
	if (err < 0) {
		dev_err(&client->dev, "%s: failed to set config\n", __func__);
		return -EIO;
	}

	return 0;
}

static int nct1008_resume(struct i2c_client *client)
{
	u8 config = 0;
	int err;

	config = i2c_smbus_read_byte_data(client, NCT1008_CONFIG_RD);
	if (config < 0) {
		dev_err(&client->dev, "%s: failed to read config\n", __func__);
		return -EIO;
	}

	/* take device out of standby state */
	config &= ~NCT1008_CONFIG_RUN_STANDBY;

	err = i2c_smbus_write_byte_data(client, NCT1008_CONFIG_WR, config);
	if (err < 0) {
		dev_err(&client->dev, "%s: failed to set config\n", __func__);
		return -EIO;
	}
	return 0;
}
#endif

static const struct i2c_device_id nct1008_id[] = {
	{ DRIVER_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, nct1008_id);

static struct i2c_driver nct1008_driver = {
	.class		= I2C_CLASS_HWMON,
	.driver = {
		.name	= DRIVER_NAME,
	},
	.probe		= nct1008_probe,
	.remove		= __devexit_p(nct1008_remove),
	.id_table	= nct1008_id,
#ifdef CONFIG_PM
	.suspend = nct1008_suspend,
	.resume = nct1008_resume,
#endif
};

static int __init nct1008_init(void)
{
	return i2c_add_driver(&nct1008_driver);
}

static void __exit nct1008_exit(void)
{
	i2c_del_driver(&nct1008_driver);
}

module_init (nct1008_init);
module_exit (nct1008_exit);

#define DRIVER_DESC "NCT1008 temperature sensor driver"
#define DRIVER_LICENSE "GPL"

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE(DRIVER_LICENSE);
