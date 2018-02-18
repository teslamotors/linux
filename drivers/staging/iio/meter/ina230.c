/*
 * ina230.c - driver for TI INA230
 *
 * Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * Based on hwmon driver:
 * 		drivers/hwmon/ina230.c
 * and contributed by:
 *		Peter Boonstoppel <pboonstoppel@nvidia.com>
 *		Deepak Nibade <dnibade@nvidia.com>
 *		Timo Alho <talho@nvidia.com>
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


/* ina230 (/ ina226)register offsets */
#define INA230_CONFIG	0
#define INA230_SHUNT	1
#define INA230_VOLTAGE	2
#define INA230_POWER	3
#define INA230_CURRENT	4
#define INA230_CAL	5
#define INA230_MASK	6
#define INA230_ALERT	7

/*
Mask register for ina230 (/ina 226):
D15|D14|D13|D12|D11 D10 D09 D08 D07 D06 D05 D04 D03 D02 D01 D00
SOL|SUL|BOL|BUL|POL|CVR|-   -   -   -   -  |AFF|CVF|OVF|APO|LEN
*/
#define INA230_MASK_SOL		(1 << 15)
#define INA230_MASK_SUL		(1 << 14)
#define INA230_MASK_CVF		(1 << 3)
#define INA230_MAX_CONVERSION_TRIALS	50

/*
Config register for ina230 (/ ina226):
Some of these values may be needed to calculate platform_data values
D15|D14 D13 D12|D11 D10 D09|D08 D07 D06|D05 D04 D03|D02 D01 D00
rst|-   -   -  |AVG        |Vbus_CT    |Vsh_CT     |MODE
*/
#define INA230_RESET		(1 << 15)
#define INA230_VBUS_CT		(0 << 6) /* Vbus 140us conversion time */
#define INA230_VSH_CT		(0 << 3) /* Vshunt 140us conversion time */

#define INA230_CONT_MODE	7	/* Continuous Bus and shunt measure */
#define INA230_TRIG_MODE	3	/* Triggered Bus and shunt measure */
#define INA230_POWER_DOWN	0

enum {
	CHANNEL_NAME = 0,
	CURRENT_THRESHOLD,
	ALERT_FLAG,
	VBUS_VOLTAGE_CURRENT,
};

struct ina230_platform_data {
	const char *rail_name;
	int current_threshold;
	int resistor;
	int min_cores_online;
	unsigned int calibration_data;
	unsigned int power_lsb;
	u32 trig_conf_data;
	u32 cont_conf_data;
	u32 divisor;
	unsigned int shunt_resistor;
	unsigned int precision_multiplier;
	bool shunt_polarity_inverted; /* 0: not invert, 1: inverted */
	bool alert_latch_enable;
};

struct ina230_chip {
	struct device *dev;
	struct i2c_client *client;
	struct ina230_platform_data *pdata;
	struct mutex mutex;
	bool running;
	struct notifier_block nb;
};


/* bus voltage resolution: 1.25mv */
#define busv_register_to_mv(x) (((x) * 5) >> 2)

/* shunt voltage resolution: 2.5uv */
#define shuntv_register_to_uv(x) (((x) * 5) >> 1)
#define uv_to_alert_register(x) (((x) << 1) / 5)

static inline struct ina230_chip *to_ina230_chip(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	return iio_priv(indio_dev);
}

static int ina230_ensure_enabled_start(struct ina230_chip *chip)
{
	int ret;

	if (chip->running)
		return 0;

	ret = i2c_smbus_write_word_data(chip->client, INA230_CONFIG,
			   __constant_cpu_to_be16(chip->pdata->trig_conf_data));
	if (ret < 0)
		dev_err(chip->dev, "CONFIG write failed: %d\n", ret);

	return ret;
}

static void ina230_ensure_enabled_end(struct ina230_chip *chip)
{
	int ret;

	if (chip->running)
		return;

	ret = i2c_smbus_write_word_data(chip->client, INA230_CONFIG,
				     __constant_cpu_to_be16(INA230_POWER_DOWN));
	if (ret < 0)
		dev_err(chip->dev, "CONFIG write failed: %d\n", ret);
}

static int __locked_ina230_power_down(struct ina230_chip *chip)
{
	int ret;

	if (!chip->running)
		return 0;

	ret = i2c_smbus_write_word_data(chip->client, INA230_MASK, 0);
	if (ret < 0)
		dev_err(chip->dev, "Mask write failed: %d\n", ret);

	ret = i2c_smbus_write_word_data(chip->client, INA230_CONFIG,
				     __constant_cpu_to_be16(INA230_POWER_DOWN));
	if (ret < 0)
		dev_err(chip->dev, "CONFIG write failed: %d\n", ret);

	chip->running = false;
	return ret;
}

static int ina230_power_down(struct ina230_chip *chip)
{
	int ret;

	mutex_lock(&chip->mutex);
	ret = __locked_ina230_power_down(chip);
	mutex_unlock(&chip->mutex);
	return ret;
}

static int __locked_ina230_start_current_mon(struct ina230_chip *chip)
{
	int ret;
	s32 shunt_uv;
	s16 shunt_limit;
	s16 alert_mask;
	int mask_len;

	if (!chip->pdata->current_threshold) {
		dev_err(chip->dev, "no current threshold specified\n");
		return -EINVAL;
	}

	ret = i2c_smbus_write_word_data(chip->client, INA230_CONFIG,
		    __constant_cpu_to_be16(chip->pdata->cont_conf_data));
	if (ret < 0) {
		dev_err(chip->dev, "CONFIG write failed: %d\n", ret);
		return ret;
	}

	if (chip->pdata->resistor) {
		shunt_uv = chip->pdata->resistor;
		shunt_uv *= chip->pdata->current_threshold;
	} else {
		s32 v;
		/* no resistor value defined, compute shunt_uv the hard way */
		v = chip->pdata->precision_multiplier * 5120 * 25;
		v /= chip->pdata->calibration_data;
		v *= chip->pdata->current_threshold;
		v /= chip->pdata->power_lsb;
		shunt_uv = (s16)(v & 0xffff);
	}
	if (chip->pdata->shunt_polarity_inverted)
		shunt_uv *= -1;

	shunt_limit = (s16) uv_to_alert_register(shunt_uv);

	ret = i2c_smbus_write_word_data(chip->client, INA230_ALERT,
					   cpu_to_be16(shunt_limit));
	if (ret < 0) {
		dev_err(chip->dev, "ALERT write failed: %d\n", ret);
		return ret;
	}

	mask_len = chip->pdata->alert_latch_enable ? 0x1 : 0x0;
	alert_mask = shunt_limit >= 0 ? INA230_MASK_SOL + mask_len :
		INA230_MASK_SUL + mask_len;
	ret = i2c_smbus_write_word_data(chip->client, INA230_MASK,
					   cpu_to_be16(alert_mask));
	if (ret < 0) {
		dev_err(chip->dev, "MASK write failed: %d\n", ret);
		return ret;
	}
	chip->running = true;
	return 0;
}

static void __locked_ina230_evaluate_state(struct ina230_chip *chip)
{
	int cpus = num_online_cpus();

	if (chip->running) {
		if (cpus < chip->pdata->min_cores_online ||
		    !chip->pdata->current_threshold)
			__locked_ina230_power_down(chip);
	} else {
		if (cpus >= chip->pdata->min_cores_online &&
		    chip->pdata->current_threshold)
			__locked_ina230_start_current_mon(chip);
	}
}

static int  __locked_wait_for_conversion(struct ina230_chip *chip)
{
	int ret, conversion, trials = 0;

	/* wait till conversion ready bit is set */
	do {
		ret = be16_to_cpu(i2c_smbus_read_word_data(chip->client,
							INA230_MASK));
		if (ret < 0) {
			dev_err(chip->dev, "MASK read failed: %d\n", ret);
			return ret;
		}
		conversion = ret & INA230_MASK_CVF;
	} while ((!conversion) && (++trials < INA230_MAX_CONVERSION_TRIALS));

	if (trials == INA230_MAX_CONVERSION_TRIALS) {
		dev_err(chip->dev, "maximum retries exceeded\n");
		return -EAGAIN;
	}

	return 0;
}

static void ina230_evaluate_state(struct ina230_chip *chip)
{
	mutex_lock(&chip->mutex);
	__locked_ina230_evaluate_state(chip);
	mutex_unlock(&chip->mutex);
}

static int ina230_get_bus_voltage(struct ina230_chip *chip, int *volt_mv)
{
	int ret;
	int voltage_mv;
	mutex_lock(&chip->mutex);
	ret = ina230_ensure_enabled_start(chip);
	if (ret < 0) {
		mutex_unlock(&chip->mutex);
		return ret;
	}

	ret = __locked_wait_for_conversion(chip);
	if (ret < 0) {
		mutex_unlock(&chip->mutex);
		return ret;
	}
	/* getting voltage readings in milli volts*/
	voltage_mv = (s16)be16_to_cpu(i2c_smbus_read_word_data(chip->client,
						  INA230_VOLTAGE));

	ina230_ensure_enabled_end(chip);
	mutex_unlock(&chip->mutex);

	if (voltage_mv < 0) {
		dev_err(chip->dev, "%s: failed: %d\n", __func__, voltage_mv);
		return -EINVAL;
	}
	*volt_mv = busv_register_to_mv(voltage_mv);
	return 0;
}

static int ina230_get_shunt_voltage(struct ina230_chip *chip, int *volt_uv)
{
	int voltage_uv;
	int ret;
	mutex_lock(&chip->mutex);
	ret = ina230_ensure_enabled_start(chip);
	if (ret < 0) {
		mutex_unlock(&chip->mutex);
		return ret;
	}

	ret = __locked_wait_for_conversion(chip);
	if (ret < 0) {
		mutex_unlock(&chip->mutex);
		return ret;
	}
	voltage_uv = (s16)be16_to_cpu(i2c_smbus_read_word_data(chip->client,
						  INA230_SHUNT));

	ina230_ensure_enabled_end(chip);
	mutex_unlock(&chip->mutex);

	*volt_uv = shuntv_register_to_uv(voltage_uv);
	return 0;
}

static int ina230_get_bus_current(struct ina230_chip *chip, int *curr_ma)
{
	int current_ma;
	int ret;

	mutex_lock(&chip->mutex);
	ret = ina230_ensure_enabled_start(chip);
	if (ret < 0)
		goto out;

	/* fill calib data */
	ret = i2c_smbus_write_word_data(chip->client, INA230_CAL,
		__constant_cpu_to_be16(chip->pdata->calibration_data));
	if (ret < 0) {
		dev_err(chip->dev, "CAL read failed: %d\n", ret);
		goto out;
	}

	ret = __locked_wait_for_conversion(chip);
	if (ret)
		goto out;

	/* getting current readings in milli amps*/
	ret = i2c_smbus_read_word_data(chip->client, INA230_CURRENT);
	if (ret < 0)
		goto out;

	current_ma = (s16) be16_to_cpu(ret);

	ina230_ensure_enabled_end(chip);
	mutex_unlock(&chip->mutex);

	if (chip->pdata->shunt_polarity_inverted)
		current_ma *= -1;

	current_ma *= (s16) chip->pdata->power_lsb;
	if (chip->pdata->divisor)
		current_ma /= (s16) chip->pdata->divisor;
	if (chip->pdata->precision_multiplier)
		current_ma /= (s16) chip->pdata->precision_multiplier;

	*curr_ma = current_ma;
	return 0;

out:
	mutex_unlock(&chip->mutex);
	return ret;
}

static int ina230_get_shunt_current(struct ina230_chip *chip, int *curr_ma)
{
	int voltage_uv;
	int inverse_shunt_resistor, current_ma;
	int ret;

	mutex_lock(&chip->mutex);
	ret = ina230_ensure_enabled_start(chip);
	if (ret < 0) {
		mutex_unlock(&chip->mutex);
		return ret;
	}

	voltage_uv = (s16)be16_to_cpu(i2c_smbus_read_word_data(chip->client,
						  INA230_SHUNT));

	ina230_ensure_enabled_end(chip);
	mutex_unlock(&chip->mutex);

	voltage_uv = shuntv_register_to_uv(voltage_uv);
	voltage_uv = abs(voltage_uv);

	inverse_shunt_resistor = 1000 / chip->pdata->resistor;
	current_ma = voltage_uv * inverse_shunt_resistor / 1000;

	*curr_ma = current_ma;
	return 0;
}

static int ina230_get_bus_power(struct ina230_chip *chip, int *pow_mw)
{
	int power_mw;
	int ret;

	mutex_lock(&chip->mutex);
	ret = ina230_ensure_enabled_start(chip);
	if (ret < 0)
		goto out;

	/* fill calib data */
	ret = i2c_smbus_write_word_data(chip->client, INA230_CAL,
		__constant_cpu_to_be16(chip->pdata->calibration_data));
	if (ret < 0) {
		dev_err(chip->dev, "CAL read failed: %d\n", ret);
		goto out;
	}

	ret = __locked_wait_for_conversion(chip);
	if (ret)
		goto out;

	/* getting power readings in milli watts*/
	power_mw = be16_to_cpu(i2c_smbus_read_word_data(chip->client,
					INA230_POWER));
	if (power_mw < 0) {
		ret = -EINVAL;
		goto out;
	}

	ina230_ensure_enabled_end(chip);
	mutex_unlock(&chip->mutex);

	power_mw = power_mw * chip->pdata->power_lsb;
	if (chip->pdata->precision_multiplier)
		power_mw /= chip->pdata->precision_multiplier;

	*pow_mw = power_mw;
	return 0;

out:
	mutex_unlock(&chip->mutex);
	return ret;
}

static int ina230_get_shunt_power(struct ina230_chip *chip, int *power_mw)
{
	int voltage_uv, voltage_mv;
	int inverse_shunt_resistor, current_ma;
	int ret;

	mutex_lock(&chip->mutex);
	ret = ina230_ensure_enabled_start(chip);
	if (ret < 0) {
		mutex_unlock(&chip->mutex);
		return ret;
	}

	voltage_mv = (s16)be16_to_cpu(i2c_smbus_read_word_data(chip->client,
						  INA230_VOLTAGE));

	voltage_uv = (s16)be16_to_cpu(i2c_smbus_read_word_data(chip->client,
							  INA230_SHUNT));

	ina230_ensure_enabled_end(chip);
	mutex_unlock(&chip->mutex);

	voltage_mv = busv_register_to_mv(voltage_mv);
	voltage_uv = shuntv_register_to_uv(voltage_uv);
	voltage_uv = abs(voltage_uv);

	inverse_shunt_resistor = 1000 / chip->pdata->resistor;
	current_ma = voltage_uv * inverse_shunt_resistor / 1000;
	*power_mw = (voltage_mv * current_ma) / 1000;
	return 0;
}

static int ina230_get_vbus_voltage_current(struct ina230_chip *chip,
					   int *current_ma, int *voltage_mv)
{
	int ret = 0, val;
	int ma;

	mutex_lock(&chip->mutex);
	/* ensure that triggered mode will be used */
	chip->running = 0;
	ret = ina230_ensure_enabled_start(chip);
	if (ret < 0)
		goto out;

	ret = __locked_wait_for_conversion(chip);
	if (ret)
		goto out;

	val = i2c_smbus_read_word_data(chip->client, INA230_VOLTAGE);
	if (val < 0) {
		ret = val;
		goto out;
	}
	*voltage_mv = busv_register_to_mv(be16_to_cpu(val));

	if (chip->pdata->resistor) {
		val = i2c_smbus_read_word_data(chip->client, INA230_SHUNT);
		if (val < 0) {
			ret = val;
			goto out;
		}
		ma = shuntv_register_to_uv((s16)be16_to_cpu(val));
		ma = DIV_ROUND_CLOSEST(ma, chip->pdata->resistor);
		if (chip->pdata->shunt_polarity_inverted)
			ma *= -1;
		*current_ma = ma;
	} else {
		*current_ma = 0;
	}
out:
	/* restart continuous current monitoring, if enabled */
	if (chip->pdata->current_threshold)
		__locked_ina230_evaluate_state(chip);
	mutex_unlock(&chip->mutex);
	return ret;
}


static int ina230_set_current_threshold(struct ina230_chip *chip,
		int current_ma)
{
	int ret = 0;

	mutex_lock(&chip->mutex);

	chip->pdata->current_threshold = current_ma;
	if (current_ma) {
		if (chip->running)
			/* force restart */
			ret = __locked_ina230_start_current_mon(chip);
		else
			__locked_ina230_evaluate_state(chip);
	} else {
		ret = __locked_ina230_power_down(chip);
	}

	mutex_unlock(&chip->mutex);
	return ret;
}

static int ina230_show_alert_flag(struct ina230_chip *chip, char *buf)
{
	int alert_flag;
	int ret;

	mutex_lock(&chip->mutex);
	ret = ina230_ensure_enabled_start(chip);
	if (ret < 0) {
		mutex_unlock(&chip->mutex);
		return ret;
	}

	alert_flag = be16_to_cpu(i2c_smbus_read_word_data(chip->client,
							INA230_MASK));

	ina230_ensure_enabled_end(chip);
	mutex_unlock(&chip->mutex);

	alert_flag = (alert_flag >> 4) & 0x1;
	return snprintf(buf, PAGE_SIZE, "%d\n", alert_flag);
}

static int ina230_hotplug_notify(struct notifier_block *nb,
		unsigned long event, void *hcpu)
{
	struct ina230_chip *chip = container_of(nb, struct ina230_chip, nb);

	if (event == CPU_ONLINE || event == CPU_DEAD)
		ina230_evaluate_state(chip);
	return 0;
}

static int ina230_read_raw(struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan, int *val, int *val2, long mask)
{
	struct ina230_chip *chip = iio_priv(indio_dev);
	struct device *dev = chip->dev;
	int type = chan->type;
	int address = chan->address;
	int ret = 0;

	if (mask != IIO_CHAN_INFO_PROCESSED) {
		dev_err(dev, "Invalid mask 0x%08lx\n", mask);
		return -EINVAL;
	}

	switch (address) {
	case 0:
		switch (type) {
		case IIO_VOLTAGE:
			ret = ina230_get_bus_voltage(chip, val);
			break;

		case IIO_CURRENT:
			ret = ina230_get_bus_current(chip, val);
			break;

		case IIO_POWER:
			ret = ina230_get_bus_power(chip, val);
			break;
		default:
			ret = -EINVAL;
			break;
		}
		break;

	case 1:
		switch (type) {
		case IIO_VOLTAGE:
			ret = ina230_get_shunt_voltage(chip, val);
			break;

		case IIO_CURRENT:
			ret = ina230_get_shunt_current(chip, val);
			break;

		case IIO_POWER:
			ret = ina230_get_shunt_power(chip, val);
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

static ssize_t ina230_show_channel(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct ina230_chip *chip = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int current_ma = 0;
	int voltage_mv = 0;
	int ret;

	switch (this_attr->address) {
	case CHANNEL_NAME:
		return snprintf(buf, PAGE_SIZE, "%s\n",
				chip->pdata->rail_name);

	case CURRENT_THRESHOLD:
		return snprintf(buf, PAGE_SIZE, "%d mA\n",
				chip->pdata->current_threshold);

	case ALERT_FLAG:
		return ina230_show_alert_flag(chip, buf);

	case VBUS_VOLTAGE_CURRENT:
		ret = ina230_get_vbus_voltage_current(chip, &current_ma,
						      &voltage_mv);
		if (!ret)
			return snprintf(buf, PAGE_SIZE, "%d %d\n",
					voltage_mv, current_ma);
		return ret;

	default:
		break;
	}
	return -EINVAL;
}

static ssize_t ina230_set_channel(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct ina230_chip *chip = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int mode = this_attr->address;
	long val;
	int current_ma;
	int ret;

	switch (mode) {
	case CURRENT_THRESHOLD:
		if (kstrtol(buf, 10, &val) < 0)
			return -EINVAL;

		current_ma = (int) val;
		ret = ina230_set_current_threshold(chip, current_ma);
		if (ret)
			return ret;
		return len;
	default:
		break;
	}
	return -EINVAL;
}

static IIO_DEVICE_ATTR(rail_name, S_IRUGO,
		ina230_show_channel, NULL, CHANNEL_NAME);

static IIO_DEVICE_ATTR(current_threshold, S_IRUGO | S_IWUSR,
		ina230_show_channel, ina230_set_channel, CURRENT_THRESHOLD);

static IIO_DEVICE_ATTR(alert_flag, S_IRUGO,
		ina230_show_channel, NULL, ALERT_FLAG);

static IIO_DEVICE_ATTR(ui_input, S_IRUSR|S_IRGRP,
		       ina230_show_channel, NULL,
		       VBUS_VOLTAGE_CURRENT);


static struct attribute *ina230_attributes[] = {
	&iio_dev_attr_rail_name.dev_attr.attr,
	&iio_dev_attr_current_threshold.dev_attr.attr,
	&iio_dev_attr_alert_flag.dev_attr.attr,
	&iio_dev_attr_ui_input.dev_attr.attr,
	NULL,
};

static const struct attribute_group ina230_groups = {
	.attrs = ina230_attributes,
};

static const struct iio_chan_spec ina230_channels_spec[] = {
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

static const struct iio_info ina230_info = {
	.attrs = &ina230_groups,
	.driver_module = THIS_MODULE,
	.read_raw = &ina230_read_raw,
};

static struct ina230_platform_data *ina230_get_platform_data_dt(
	struct i2c_client *client)
{
	struct ina230_platform_data *pdata;
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

	ret = of_property_read_u32(np, "ti,current-threshold", &pval);
	if (!ret)
		pdata->current_threshold = (int)pval;

	ret = of_property_read_u32(np, "ti,resistor", &pval);
	if (!ret)
		pdata->resistor = pval;

	ret = of_property_read_u32(np, "ti,minimum-core-online", &pval);
	if (!ret)
		pdata->min_cores_online = pval;

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

	pdata->shunt_polarity_inverted = of_property_read_bool(np,
					"ti,shunt-polartiy-inverted");

	pdata->alert_latch_enable = of_property_read_bool(np,
					"ti,enable-alert-latch");
	return pdata;
}

static int ina230_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct ina230_chip *chip;
	struct iio_dev *indio_dev;
	struct ina230_platform_data *pdata;
	int ret;

	pdata = ina230_get_platform_data_dt(client);
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
	chip->running = false;
	chip->nb.notifier_call = ina230_hotplug_notify;
	mutex_init(&chip->mutex);

	indio_dev->info = &ina230_info;
	indio_dev->channels = ina230_channels_spec;
	indio_dev->num_channels = ARRAY_SIZE(ina230_channels_spec);
	indio_dev->name = id->name;
	indio_dev->dev.parent = &client->dev;
	indio_dev->modes = INDIO_DIRECT_MODE;
	ret = devm_iio_device_register(chip->dev, indio_dev);
	if (ret < 0) {
		dev_err(chip->dev, "iio registration fails with error %d\n",
			ret);
		return ret;
	}

	ret = i2c_smbus_write_word_data(client, INA230_CONFIG,
			__constant_cpu_to_be16(INA230_RESET));
	if (ret < 0) {
		dev_err(&client->dev, "ina230 reset failed: %d\n", ret);
		return ret;
	}

	register_hotcpu_notifier(&(chip->nb));

	ret = i2c_smbus_write_word_data(client, INA230_MASK, 0);
	if (ret < 0) {
		dev_err(&client->dev, "MASK write failed: %d\n", ret);
		goto exit;
	}

	/* Power it on once current_threshold defined, or power it down */
	if (pdata->current_threshold) {
		ina230_evaluate_state(chip);
	} else {
		/* set ina230 to power down mode */
		ret = i2c_smbus_write_word_data(client, INA230_CONFIG,
				__constant_cpu_to_be16(INA230_POWER_DOWN));
		if (ret < 0) {
			dev_err(&client->dev, "INA power down failed: %d\n",
				ret);
			goto exit;
		}
	}

	return 0;

exit:
	unregister_hotcpu_notifier(&chip->nb);
	return ret;
}

static int ina230_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct ina230_chip *chip = iio_priv(indio_dev);

	unregister_hotcpu_notifier(&chip->nb);
	ina230_power_down(chip);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int ina230_suspend(struct device *dev)
{
	struct ina230_chip *chip = to_ina230_chip(dev);

	return ina230_power_down(chip);
}

static int ina230_resume(struct device *dev)
{
	struct ina230_chip *chip = to_ina230_chip(dev);

	ina230_evaluate_state(chip);
	return 0;
}
#endif

static const struct dev_pm_ops ina230_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ina230_suspend, ina230_resume)
};

static const struct i2c_device_id ina230_id[] = {
	{"ina226x", 0 },
	{"ina230x", 0 },
	{"hpa01112x", 0 },
	{"hpa02149x", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, ina230_id);

static struct i2c_driver ina230_driver = {
	.driver = {
		.name	= "ina230x",
		.pm	= &ina230_pm_ops,
	},
	.probe		= ina230_probe,
	.remove		= ina230_remove,
	.id_table	= ina230_id,
};

module_i2c_driver(ina230_driver);

MODULE_DESCRIPTION("TI INA230 bidirectional current/power Monitor");
MODULE_AUTHOR("Peter Boonstoppel <pboonstoppel@nvidia.com>");
MODULE_AUTHOR("Deepak Nibade <dnibade@nvidia.com>");
MODULE_AUTHOR("Timo Alho <talho@nvidia.com>");
MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_LICENSE("GPL v2");
