/*
 * ina219.c - driver for TI INA219
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * Based on hwmon driver:
 *		drivers/hwmon/ina219.c
 * and contributed by:
 *		venu byravarasu <vbyravarasu@nvidia.com>
 *		Anshul Jain <anshulj@nvidia.com>
 *		Deepak Nibade <dnibade@nvidia.com>
 *
 * This program is free software. you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/sysfs.h>
#include <linux/slab.h>

#define INA219_CONFIG	0
#define INA219_SHUNT	1
#define INA219_VOLTAGE	2
#define INA219_POWER	3
#define INA219_CURRENT	4
#define INA219_CAL	5

#define INA219_RESET		(1 << 15)

struct ina219_platform_data {
	const char *rail_name;
	unsigned int calibration_data;
	unsigned int power_lsb;
	u32 trig_conf_data;
	u32 cont_conf_data;
	u32 divisor;
	unsigned int shunt_resistor;
	unsigned int precision_multiplier;
};

struct ina219_chip {
	struct device *dev;
	struct i2c_client *client;
	struct ina219_platform_data *pdata;
	struct mutex mutex;
	bool state;
	struct notifier_block nb;
};

enum {
	CHANNEL_NAME,
	CHANNEL_STATE,
};

enum {
	STOPPED,
	RUNNING,
};

#define busv_register_to_mv(x) (((x) >> 3) * 4)
#define shuntv_register_to_uv(x) ((x) * 10)

static inline struct ina219_chip *to_ina219_chip(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	return iio_priv(indio_dev);
}

static int ina219_power_down(struct ina219_chip *chip)
{
	int ret;

	ret = i2c_smbus_write_word_data(chip->client, INA219_CONFIG, 0);
	if (ret < 0)
		dev_err(chip->dev, "INA power down failed: %d\n", ret);
	return ret;
}

static int ina219_power_up(struct ina219_chip *chip, u16 config_data)
{
	int ret;

	ret = i2c_smbus_write_word_data(chip->client, INA219_CONFIG,
				__constant_cpu_to_be16(config_data));
	if (ret < 0)
		goto exit;

	ret = i2c_smbus_write_word_data(chip->client, INA219_CAL,
			__constant_cpu_to_be16(chip->pdata->calibration_data));
	if (ret < 0)
		goto exit;

	return 0;

exit:
	dev_err(chip->dev, "INA power up failed: %d\n", ret);
	return ret;

}

static int ina219_get_bus_voltage(struct ina219_chip *chip, int *volt_mv)
{
	int voltage_mv;
	int cur_state;
	int ret;

	mutex_lock(&chip->mutex);
	cur_state = chip->state;

	if (chip->state == STOPPED) {
		ret = ina219_power_up(chip, chip->pdata->trig_conf_data);
		if (ret < 0)
			goto exit;
	}

	/* getting voltage readings in milli volts*/
	voltage_mv = (s16)be16_to_cpu(i2c_smbus_read_word_data(chip->client,
					INA219_VOLTAGE));

	if (voltage_mv < 0)
		goto exit;

	*volt_mv = busv_register_to_mv(voltage_mv);

	if (cur_state == STOPPED) {
		ret = ina219_power_down(chip);
		if (ret < 0)
			goto exit;
	}

	mutex_unlock(&chip->mutex);
	return 0;

exit:
	mutex_unlock(&chip->mutex);
	dev_err(chip->dev, "%s: failed\n", __func__);
	return -EAGAIN;
}

static int ina219_get_shunt_voltage(struct ina219_chip *chip, int *volt_uv)
{
	int voltage_uv;
	int cur_state;
	int ret;

	mutex_lock(&chip->mutex);
	cur_state = chip->state;
	if (chip->state == STOPPED) {
		ret = ina219_power_up(chip, chip->pdata->trig_conf_data);
		if (ret < 0)
			goto exit;
	}

	voltage_uv = be16_to_cpu(i2c_smbus_read_word_data(chip->client,
				INA219_SHUNT));

	if (voltage_uv < 0)
		goto exit;

	*volt_uv = shuntv_register_to_uv(voltage_uv);

	if (cur_state == STOPPED) {
		ret = ina219_power_down(chip);
		if (ret < 0)
			goto exit;
	}

	mutex_unlock(&chip->mutex);
	return 0;
exit:
	mutex_unlock(&chip->mutex);
	dev_err(chip->dev, "%s: failed\n", __func__);
	return -EAGAIN;
}

static int ina219_get_shunt_power(struct ina219_chip *chip, int *shunt_power_mw)
{
	int power_mw;
	int voltage_shunt_uv;
	int voltage_bus_mv;
	int inverse_shunt_resistor;
	int cur_state;
	int ret;

	mutex_lock(&chip->mutex);
	cur_state = chip->state;
	if (chip->state == STOPPED) {
		ret = ina219_power_up(chip, chip->pdata->trig_conf_data);
		if (ret < 0)
			goto exit;
	}

	voltage_shunt_uv = be16_to_cpu(i2c_smbus_read_word_data(chip->client,
					INA219_SHUNT));
	if (voltage_shunt_uv < 0)
		goto exit;

	voltage_shunt_uv = shuntv_register_to_uv(voltage_shunt_uv);

	voltage_bus_mv = be16_to_cpu(i2c_smbus_read_word_data(chip->client,
				INA219_VOLTAGE));
	if (voltage_bus_mv < 0)
		goto exit;

	voltage_bus_mv = busv_register_to_mv(voltage_bus_mv);

	/*avoid overflow*/
	inverse_shunt_resistor = 1000/(chip->pdata->shunt_resistor);
	power_mw = voltage_shunt_uv * inverse_shunt_resistor; /*current uAmps*/
	power_mw = power_mw / 1000; /*current mAmps*/
	power_mw = power_mw * (voltage_bus_mv); /*Power uW*/
	power_mw = power_mw / 1000; /*Power mW*/

	if (cur_state == STOPPED) {
		ret = ina219_power_down(chip);
		if (ret < 0)
			goto exit;
	}

	mutex_unlock(&chip->mutex);
	*shunt_power_mw = power_mw;
	return 0;
exit:
	mutex_unlock(&chip->mutex);
	dev_err(chip->dev, "%s: failed\n", __func__);
	return -EAGAIN;
}

static int ina219_get_bus_power(struct ina219_chip *chip, int *bus_power_mw)
{
	int power_mw;
	int voltage_mv;
	int overflow, conversion;
	int cur_state;
	int ret;

	mutex_lock(&chip->mutex);
	cur_state = chip->state;
	if (chip->state == STOPPED) {
		ret = ina219_power_up(chip, chip->pdata->trig_conf_data);
		if (ret < 0) {
			ret = -EAGAIN;
			goto exit;
		}
	} else {
		mutex_unlock(&chip->mutex);
		return ina219_get_shunt_power(chip, bus_power_mw);
	}

	/* check if the readings are valid */
	do {
		/* read power register to clear conversion bit */
		ret = be16_to_cpu(i2c_smbus_read_word_data(chip->client,
				INA219_POWER));
		if (ret < 0) {
			dev_err(chip->dev, "POWER read failed: %d\n", ret);
			goto exit;
		}

		voltage_mv = be16_to_cpu(i2c_smbus_read_word_data(chip->client,
					INA219_VOLTAGE));
		overflow = voltage_mv & 1;
		if (overflow) {
			dev_err(chip->dev, "overflow error\n");
			goto exit;
		}
		conversion = (voltage_mv >> 1) & 1;
	} while (!conversion);

	/* getting power readings in milli watts*/
	power_mw = be16_to_cpu(i2c_smbus_read_word_data(chip->client,
			INA219_POWER));
	power_mw *= chip->pdata->power_lsb;
	if (chip->pdata->precision_multiplier)
		power_mw /= chip->pdata->precision_multiplier;
	if (power_mw < 0)
		goto exit;

	/* set ina219 to power down mode */
	ret = ina219_power_down(chip);
	if (ret < 0)
		goto exit;

	mutex_unlock(&chip->mutex);
	*bus_power_mw = power_mw;
	return 0;

exit:
	mutex_unlock(&chip->mutex);
	dev_err(chip->dev, "%s: failed\n", __func__);
	return ret;
}

static int ina219_get_shunt_current(struct ina219_chip *chip, int *shunt_cur_ma)
{
	int current_ma;
	int voltage_uv;
	int inverse_shunt_resistor;
	int cur_state;
	int ret;

	mutex_lock(&chip->mutex);
	cur_state = chip->state;
	if (chip->state == STOPPED) {
		ret = ina219_power_up(chip, chip->pdata->trig_conf_data);
		if (ret < 0)
			goto exit;
	}

	voltage_uv = (s16)be16_to_cpu(i2c_smbus_read_word_data(chip->client,
							INA219_SHUNT));
	if (voltage_uv < 0)
		goto exit;

	inverse_shunt_resistor = 1000/(chip->pdata->shunt_resistor);
	voltage_uv = shuntv_register_to_uv(voltage_uv);
	current_ma = voltage_uv * inverse_shunt_resistor;
	current_ma = current_ma / 1000;

	if (cur_state == STOPPED) {
		ret = ina219_power_down(chip);
		if (ret < 0)
			goto exit;
	}

	mutex_unlock(&chip->mutex);
	*shunt_cur_ma = current_ma;
	return 0;
exit:
	dev_err(chip->dev, "%s: failed\n", __func__);
	mutex_unlock(&chip->mutex);
	return -EAGAIN;
}

static int ina219_get_bus_current(struct ina219_chip *chip, int *raw_cur_ma)
{
	int current_ma;
	int voltage_mv;
	int overflow, conversion;
	int cur_state;
	int ret;

	mutex_lock(&chip->mutex);

	cur_state = chip->state;
	if (chip->state == STOPPED) {
		ret = ina219_power_up(chip, chip->pdata->trig_conf_data);
		if (ret < 0) {
			ret = -EAGAIN;
			goto exit;
		}
	} else {
		mutex_unlock(&chip->mutex);
		return ina219_get_shunt_current(chip, raw_cur_ma);
	}

	/* check if the readings are valid */
	do {
		/* read power register to clear conversion bit */
		ret = be16_to_cpu(i2c_smbus_read_word_data(chip->client,
					INA219_POWER));
		if (ret < 0) {
			dev_err(chip->dev, "POWER read failed: %d\n", ret);
			goto exit;
		}

		voltage_mv = be16_to_cpu(i2c_smbus_read_word_data(chip->client,
					INA219_VOLTAGE));
		overflow = voltage_mv & 1;
		if (overflow) {
			dev_err(chip->dev, "overflow error\n");
			goto exit;
		}
		conversion = (voltage_mv >> 1) & 1;
	} while (!conversion);

	/* getting current readings in milli amps*/
	current_ma = be16_to_cpu(i2c_smbus_read_word_data(chip->client,
					INA219_CURRENT));
	if (current_ma < 0)
		goto exit;

	current_ma = (current_ma * chip->pdata->power_lsb) /
				chip->pdata->divisor;
	if (chip->pdata->precision_multiplier)
		current_ma /= chip->pdata->precision_multiplier;

	ret = ina219_power_down(chip);
	if (ret < 0)
		goto exit;

	mutex_unlock(&chip->mutex);
	*raw_cur_ma = current_ma;
	return 0;

exit:
	mutex_unlock(&chip->mutex);
	dev_err(chip->dev, "%s: failed\n", __func__);
	return ret;
}

static int ina219_set_state(struct ina219_chip *chip, long new_state)
{
	int ret = -1;

	mutex_lock(&chip->mutex);

	if ((new_state > 0) && (chip->state == STOPPED))
		ret = ina219_power_up(chip, chip->pdata->cont_conf_data);
	else if ((new_state == 0) && (chip->state == RUNNING))
		ret = ina219_power_down(chip);

	if (ret < 0) {
		dev_err(chip->dev, "Switching INA on/off failed: %d", ret);
		mutex_unlock(&chip->mutex);
		return -EAGAIN;
	}

	if (new_state)
		chip->state = RUNNING;
	else
		chip->state = STOPPED;

	mutex_unlock(&chip->mutex);
	return 1;
}

static int ina219_read_raw(struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan, int *val, int *val2, long mask)
{
	struct ina219_chip *chip = iio_priv(indio_dev);
	int type = chan->type;
	int address = chan->address;
	int ret = 0;

	if (mask != IIO_CHAN_INFO_PROCESSED) {
		dev_err(chip->dev, "Invalid mask 0x%08lx\n", mask);
		return -EINVAL;
	}

	switch (address) {
	case 0:
		switch (type) {
		case IIO_VOLTAGE:
			ret = ina219_get_bus_voltage(chip, val);
			break;

		case IIO_CURRENT:
			ret = ina219_get_bus_current(chip, val);
			break;

		case IIO_POWER:
			ret = ina219_get_bus_power(chip, val);
			break;
		default:
			ret = -EINVAL;
			break;
		}
		break;

	case 1:
		switch (type) {
		case IIO_VOLTAGE:
			ret = ina219_get_shunt_voltage(chip, val);
			break;

		case IIO_CURRENT:
			ret = ina219_get_shunt_current(chip, val);
			break;

		case IIO_POWER:
			ret = ina219_get_shunt_power(chip, val);
			break;
		default:
			ret = -EINVAL;
			break;
		}
		break;

	default:
		ret = -EINVAL;
		break;
	}

	if (!ret)
		ret = IIO_VAL_INT;
	return ret;
}

static ssize_t ina219_show_channel(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct ina219_chip *chip = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int count;

	switch (this_attr->address) {
	case CHANNEL_NAME:
		return snprintf(buf, PAGE_SIZE, "%s\n",
				chip->pdata->rail_name);

	case CHANNEL_STATE:
		mutex_lock(&chip->mutex);
		count = snprintf(buf, PAGE_SIZE, "%d\n", chip->state);
		mutex_unlock(&chip->mutex);
		return count;

	default:
		break;
	}
	return -EINVAL;
}

static ssize_t ina219_set_channel(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct ina219_chip *chip = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int mode = this_attr->address;
	long val;

	switch (mode) {
	case CHANNEL_STATE:
		if (kstrtol(buf, 10, &val) < 0)
			return -EINVAL;
		return ina219_set_state(chip, val);

	default:
		break;
	}
	return -EINVAL;
}

static IIO_DEVICE_ATTR(rail_name, S_IRUGO,
		ina219_show_channel, NULL, CHANNEL_NAME);

static IIO_DEVICE_ATTR(state, S_IRUGO | S_IWUSR,
		ina219_show_channel, ina219_set_channel, CHANNEL_STATE);

static struct attribute *ina219_attributes[] = {
	&iio_dev_attr_rail_name.dev_attr.attr,
	&iio_dev_attr_state.dev_attr.attr,
	NULL,
};

static const struct attribute_group ina219_groups = {
	.attrs = ina219_attributes,
};

static const struct iio_chan_spec ina219_channels_spec[] = {
	{
		.type = IIO_VOLTAGE,
		.address = 0,
		.info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),
	}, {
		.type = IIO_VOLTAGE,
		.address = 1,
		.extend_name = "shunt",
		.info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),
	}, {
		.type = IIO_CURRENT,
		.address = 0,
		.info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),
	}, {
		.type = IIO_CURRENT,
		.address = 1,
		.extend_name = "shunt",
		.info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),
	}, {
		.type = IIO_POWER,
		.address = 0,
		.info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),
	}, {
		.type = IIO_POWER,
		.address = 1,
		.extend_name = "shunt",
		.info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),
	},
};

static const struct iio_info ina219_info = {
	.attrs = &ina219_groups,
	.driver_module = THIS_MODULE,
	.read_raw = &ina219_read_raw,
};

static struct ina219_platform_data *ina219_get_platform_data_dt(
	struct i2c_client *client)
{
	struct ina219_platform_data *pdata;
	struct device *dev = &client->dev;
	struct device_node *np = dev->of_node;
	u32 pval;
	int ret;

	if (!np) {
		dev_err(dev, "Only DT supported\n");
		return ERR_PTR(-ENODEV);
	}

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "pdata allocation failed\n");
		return ERR_PTR(-ENOMEM);
	}

	pdata->rail_name =  of_get_property(np, "ti,rail-name", NULL);
	if (!pdata->rail_name)
		dev_err(dev, "Rail name is not provided on node %s\n",
				np->full_name);

	ret = of_property_read_u32(np, "ti,continuous-config", &pval);
	if (!ret)
		pdata->cont_conf_data = (u16)pval;

	ret = of_property_read_u32(np, "ti,trigger-config", &pval);
	if (!ret)
		pdata->trig_conf_data = (u16)pval;

	ret = of_property_read_u32(np, "ti,calibration-data", &pval);
	if (!ret)
		pdata->calibration_data = pval;

	ret = of_property_read_u32(np, "ti,power-lsb", &pval);
	if (!ret)
		pdata->power_lsb = pval;

	ret = of_property_read_u32(np, "ti,divisor", &pval);
	if (!ret)
		pdata->divisor = pval;

	ret = of_property_read_u32(np, "ti,shunt-resistor-mohm", &pval);
	if (!ret)
		pdata->shunt_resistor = pval;

	ret = of_property_read_u32(np, "ti,precision-multiplier", &pval);
	if (!ret)
		pdata->precision_multiplier = pval;

	return pdata;
}

static int ina219_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct ina219_chip *chip;
	struct iio_dev *indio_dev;
	struct ina219_platform_data *pdata;
	int ret;

	pdata = ina219_get_platform_data_dt(client);
	if (IS_ERR(pdata)) {
		ret = PTR_ERR(pdata);
		dev_err(&client->dev, "platform data processing failed %d\n",
			ret);
		return ret;
	}

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*chip));
	if (!indio_dev) {
		dev_err(&client->dev, "iio allocation fails\n");
		return -ENOMEM;
	}

	chip = iio_priv(indio_dev);

	i2c_set_clientdata(client, indio_dev);
	chip->client = client;
	chip->dev = &client->dev;
	chip->pdata = pdata;
	chip->state = STOPPED;
	mutex_init(&chip->mutex);

	indio_dev->info = &ina219_info;
	indio_dev->channels = ina219_channels_spec;
	indio_dev->num_channels = ARRAY_SIZE(ina219_channels_spec);
	indio_dev->name = id->name;
	indio_dev->dev.parent = &client->dev;
	indio_dev->modes = INDIO_DIRECT_MODE;
	ret = devm_iio_device_register(chip->dev, indio_dev);
	if (ret < 0) {
		dev_err(chip->dev, "iio registration fails with error %d\n",
			ret);
		return ret;
	}

	ret = i2c_smbus_write_word_data(client, INA219_CONFIG,
			__constant_cpu_to_be16(INA219_RESET));
	if (ret < 0) {
		dev_err(&client->dev, "ina219 reset failed: %d\n", ret);
		return ret;
	}

	ret = ina219_power_down(chip);
	if (ret < 0) {
		dev_err(&client->dev, "INA power down failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static int ina219_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct ina219_chip *chip = iio_priv(indio_dev);
	int ret;

	ret = ina219_power_down(chip);
	if (ret < 0)
		dev_err(&client->dev, "INA power down failed: %d\n", ret);

	chip->state = STOPPED;
	return 0;
}

static const struct i2c_device_id ina219_id[] = {
	{"ina219x", 0 },
	{"hpa00900x", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, ina219_id);

static struct i2c_driver ina219_driver = {
	.driver = {
		.name	= "ina219x",
	},
	.probe		= ina219_probe,
	.remove		= ina219_remove,
	.id_table	= ina219_id,
};

module_i2c_driver(ina219_driver);

MODULE_DESCRIPTION("TI INA219 bidirectional current/power Monitor");
MODULE_AUTHOR("venu byravarasu <vbyravarasu@nvidia.com>");
MODULE_AUTHOR("Anshul Jain <anshulj@nvidia.com>");
MODULE_AUTHOR("Deepak Nibade <dnibade@nvidia.com>");
MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_LICENSE("GPL v2");
