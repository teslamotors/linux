/*
 * ina3221.c - driver for TI INA3221
 *
 * Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * Based on hwmon driver:
 * 		drivers/hwmon/ina3221.c
 * and contributed by:
 *		Deepak Nibade <dnibade@nvidia.com>
 *		Timo Alho <talho@nvidia.com>
 *		Anshul Jain <anshulj@nvidia.com>
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

#define INA3221_CONFIG			0x00
#define INA3221_SHUNT_VOL_CHAN1		0x01
#define INA3221_BUS_VOL_CHAN1		0x02
#define INA3221_SHUNT_VOL_CHAN2		0x03
#define INA3221_BUS_VOL_CHAN2		0x04
#define INA3221_SHUNT_VOL_CHAN3		0x05
#define INA3221_BUS_VOL_CHAN3		0x06
#define INA3221_CRIT_CHAN1		0x07
#define INA3221_WARN_CHAN1		0x08
#define INA3221_CRIT_CHAN2		0x09
#define INA3221_WARN_CHAN2		0x0A
#define INA3221_CRIT_CHAN3		0x0B
#define INA3221_WARN_CHAN3		0x0C
#define INA3221_MASK_ENABLE		0x0F

#define INA3221_SHUNT_VOL(i)		(INA3221_SHUNT_VOL_CHAN1 + (i) * 2)
#define INA3221_BUS_VOL(i)		(INA3221_BUS_VOL_CHAN1 + (i) * 2)
#define INA3221_CRIT(i)			(INA3221_CRIT_CHAN1 + (i) * 2)
#define INA3221_WARN(i)			(INA3221_WARN_CHAN1 + (i) * 2)

#define INA3221_RESET			0x8000
#define INA3221_POWER_DOWN		0
#define INA3221_ENABLE_CHAN		(7 << 12) /* enable all 3 channels */
#define INA3221_AVG			(3 << 9) /* 64 averages */
#define INA3221_VBUS_CT			(4 << 6) /* Vbus 1.1 mS conv time */
#define INA3221_VSHUNT_CT		(4 << 3) /* Vshunt 1.1 mS conv time */
#define INA3221_CONT_MODE		7 /* continuous bus n shunt V measure */
#define INA3221_TRIG_MODE		3 /* triggered bus n shunt V measure */

#define INA3221_CONT_CONFIG_DATA	(INA3221_ENABLE_CHAN | INA3221_AVG | \
					INA3221_VBUS_CT | INA3221_VSHUNT_CT | \
					INA3221_CONT_MODE) /* 0x7727 */

#define INA3221_TRIG_CONFIG_DATA	(INA3221_ENABLE_CHAN | \
					INA3221_TRIG_MODE) /* 0x7723 */
#define INA3221_NUMBER_OF_RAILS		3

#define INA3221_CVRF                    0x01

#define CPU_THRESHOLD			2
#define CPU_FREQ_THRESHOLD		102000

#define INA3221_MAX_CONVERSION_TRIALS 10

#define PACK_MODE_CHAN(mode, chan)	((mode) | ((chan) << 8))
#define UNPACK_MODE(address)		((address) & 0xFF)
#define UNPACK_CHAN(address)		(((address) >> 8) & 0xFF)

#define U32_MINUS_1	((u32) -1)
enum {
	CHANNEL_NAME = 0,
	CRIT_CURRENT_LIMIT,
	WARN_CURRENT_LIMIT,
	RUNNING_MODE,
	VBUS_VOLTAGE_CURRENT,
};

enum mode {
	TRIGGERED = 0,
	FORCED_TRIGGERED = 1,
	CONTINUOUS = 2,
	FORCED_CONTINUOUS = 3,
};

#define IS_TRIGGERED(x) (!((x) & 2))
#define IS_CONTINUOUS(x) ((x) & 2)

struct ina3221_chan_pdata {
	const char *rail_name;
	u32 warn_conf_limits;
	u32 crit_conf_limits;
	u32 shunt_resistor;
};

struct ina3221_platform_data {
	u16 cont_conf_data;
	u16 trig_conf_data;
	bool enable_forced_continuous;
	struct ina3221_chan_pdata cpdata[INA3221_NUMBER_OF_RAILS];
};

struct ina3221_chip {
	struct device *dev;
	struct i2c_client *client;
	struct ina3221_platform_data *pdata;
	struct mutex mutex;
	int shutdown_complete;
	int is_suspended;
	int mode;
	int alert_enabled;
	struct notifier_block nb_hot;
	struct notifier_block nb_cpufreq;
};

static int __locked_ina3221_switch_mode(struct ina3221_chip *chip,
		int cpus, int cpufreq);

static inline struct ina3221_chip *to_ina3221_chip(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	return iio_priv(indio_dev);
}

static inline int shuntv_register_to_uv(u16 reg)
{
	int ret = (s16)reg;

	return (ret >> 3) * 40;
}

static inline u16 uv_to_shuntv_register(s32 uv)
{
	return (u16)(uv/5);
}

static inline int busv_register_to_mv(u16 reg)
{
	int ret = (s16)reg;

	return (ret >> 3) * 8;
}

/* convert shunt voltage register value to current (in mA) */
static int shuntv_register_to_ma(u16 reg, int resistance)
{
	int uv, ma;

	uv = (s16)reg;
	uv = ((uv >> 3) * 40); /* LSB (4th bit) is 40uV */
	/*
	 * calculate uv/resistance with rounding knowing that C99 truncates
	 * towards zero
	 */
	if (uv > 0)
		ma = ((uv * 2 / resistance) + 1) / 2;
	else
		ma = ((uv * 2 / resistance) - 1) / 2;
	return ma;
}

static int __locked_power_down_ina3221(struct ina3221_chip *chip)
{
	int ret;

	ret = i2c_smbus_write_word_data(chip->client, INA3221_CONFIG,
				INA3221_POWER_DOWN);
	if (ret < 0)
		dev_err(chip->dev, "Power down failed: %d", ret);
	return ret;
}

static int __locked_power_up_ina3221(struct ina3221_chip *chip, int config)
{
	int ret;

	ret = i2c_smbus_write_word_data(chip->client, INA3221_CONFIG,
					cpu_to_be16(config));
	if (ret < 0)
		dev_err(chip->dev, "Power up failed: %d\n", ret);
	return ret;
}

static int __locked_start_conversion(struct ina3221_chip *chip)
{
	int ret, cvrf, trials = 0;

	if (IS_TRIGGERED(chip->mode)) {
		ret = __locked_power_up_ina3221(chip,
						chip->pdata->trig_conf_data);

		if (ret < 0)
			return ret;

		/* wait till conversion ready bit is set */
		do {
			ret = i2c_smbus_read_word_data(chip->client,
						       INA3221_MASK_ENABLE);
			if (ret < 0) {
				dev_err(chip->dev, "MASK read failed: %d\n",
					ret);
				return ret;
			}
			cvrf = be16_to_cpu(ret) & INA3221_CVRF;
		} while ((!cvrf) && (++trials < INA3221_MAX_CONVERSION_TRIALS));
		if (trials == INA3221_MAX_CONVERSION_TRIALS) {
			dev_err(chip->dev, "maximum retries exceeded\n");
			return -EAGAIN;
		}
	}

	return 0;
}

static int __locked_end_conversion(struct ina3221_chip *chip)
{
	int ret = 0;

	if (IS_TRIGGERED(chip->mode))
		ret = __locked_power_down_ina3221(chip);

	return ret;
}

static int __locked_do_conversion(struct ina3221_chip *chip, u16 *vsh,
		  u16 *vbus, int ch)
{
	struct i2c_client *client = chip->client;
	int ret;

	ret = __locked_start_conversion(chip);
	if (ret < 0)
		return ret;

	if (vsh) {
		ret = i2c_smbus_read_word_data(client, INA3221_SHUNT_VOL(ch));
		if (ret < 0)
			return ret;
		*vsh = be16_to_cpu(ret);
	}

	if (vbus) {
		ret = i2c_smbus_read_word_data(client, INA3221_BUS_VOL(ch));
		if (ret < 0)
			return ret;
		*vbus = be16_to_cpu(ret);
	}

	return __locked_end_conversion(chip);
}

static int ina3221_get_mode(struct ina3221_chip *chip, char *buf)
{
	int v;

	mutex_lock(&chip->mutex);
	if (chip->shutdown_complete) {
		mutex_unlock(&chip->mutex);
		return -EIO;
	}

	v = (IS_TRIGGERED(chip->mode)) ? 0 : 1;
	mutex_unlock(&chip->mutex);
	return snprintf(buf, PAGE_SIZE, "%d\n", v);
}

static int ina3221_set_mode_val(struct ina3221_chip *chip, long val)
{
	int cpufreq;
	int cpus;
	int ret = 0;

	mutex_lock(&chip->mutex);

	if (chip->shutdown_complete) {
		mutex_unlock(&chip->mutex);
		return -EIO;
	}

	if (val > 0) {
		ret = __locked_power_up_ina3221(chip,
				chip->pdata->cont_conf_data);
		if (!ret)
			chip->mode = FORCED_CONTINUOUS;
	} else if (val == 0) {
		chip->mode = FORCED_TRIGGERED;
		ret = __locked_power_down_ina3221(chip);
	} else {
		if (chip->alert_enabled) {
			if (IS_TRIGGERED(chip->mode))
				chip->mode = TRIGGERED;
			else
				chip->mode = CONTINUOUS;
			/* evaluate the state */
			cpufreq = cpufreq_quick_get(0);
			cpus = num_online_cpus();
			ret = __locked_ina3221_switch_mode(chip, cpus, cpufreq);
		} else {
			chip->mode = TRIGGERED;
			ret = __locked_power_down_ina3221(chip);
		}
	}
	mutex_unlock(&chip->mutex);
	return ret;
}

static int ina3221_set_mode(struct ina3221_chip *chip,
		const char *buf, size_t count)
{
	long val;
	int ret = 0;

	if (kstrtol(buf, 10, &val) < 0)
		return -EINVAL;

	ret = ina3221_set_mode_val(chip, val);
	return ret ? ret : count;
}

static int ina3221_get_channel_voltage(struct ina3221_chip *chip,
		int channel, int *voltage_mv)
{
	u16 vbus;
	int ret;

	mutex_lock(&chip->mutex);

	if (chip->shutdown_complete) {
		mutex_unlock(&chip->mutex);
		return -EIO;
	}

	ret = __locked_do_conversion(chip, NULL, &vbus, channel);
	if (ret < 0) {
		dev_err(chip->dev, "Voltage read on channel %d failed: %d\n",
			channel, ret);
		goto exit;
	}
	*voltage_mv = busv_register_to_mv(vbus);
exit:
	mutex_unlock(&chip->mutex);
	return ret;
}

static int ina3221_get_channel_current(struct ina3221_chip *chip,
		int channel, int trigger, int *current_ma)
{
	u16 vsh;
	int ret = 0;

	mutex_lock(&chip->mutex);

	if (chip->shutdown_complete) {
		mutex_unlock(&chip->mutex);
		return -EIO;
	}

	/* return 0 if INA is off */
	if (trigger && (IS_TRIGGERED(chip->mode))) {
		*current_ma = 0;
		goto exit;
	}

	ret = __locked_do_conversion(chip, &vsh, NULL, channel);
	if (ret < 0) {
		dev_err(chip->dev, "Current read on channel %d failed: %d\n",
			channel, ret);
		goto exit;
	}
	*current_ma = shuntv_register_to_ma(vsh,
			 chip->pdata->cpdata[channel].shunt_resistor);
exit:
	mutex_unlock(&chip->mutex);
	return ret;
}

static int ina3221_get_channel_power(struct ina3221_chip *chip,
		int channel, int trigger, int *power_mw)
{
	u16 vsh, vbus;
	int current_ma, voltage_mv;
	int ret = 0;

	mutex_lock(&chip->mutex);

	if (chip->shutdown_complete) {
		mutex_unlock(&chip->mutex);
		return -EIO;
	}

	if (trigger && (IS_TRIGGERED(chip->mode))) {
		*power_mw = 0;
		goto exit;
	}

	ret = __locked_do_conversion(chip, &vsh, &vbus, channel);
	if (ret < 0) {
		dev_err(chip->dev, "Read on channel %d failed: %d\n",
			channel, ret);
		goto exit;
	}

	current_ma = shuntv_register_to_ma(vsh,
			chip->pdata->cpdata[channel].shunt_resistor);
	voltage_mv = busv_register_to_mv(vbus);
	*power_mw = (voltage_mv * current_ma) / 1000;
exit:
	mutex_unlock(&chip->mutex);
	return ret;
}

static int ina3221_get_channel_vbus_voltage_current(struct ina3221_chip *chip,
		int channel, int *current_ma, int *voltage_mv)
{
	u16 vsh, vbus;
	int ret = 0;

	mutex_lock(&chip->mutex);

	if (chip->shutdown_complete) {
		mutex_unlock(&chip->mutex);
		return -EIO;
	}

	ret = __locked_do_conversion(chip, &vsh, &vbus, channel);
	if (ret < 0) {
		dev_err(chip->dev, "Read on channel %d failed: %d\n",
			channel, ret);
		goto exit;
	}

	*current_ma = shuntv_register_to_ma(vsh,
			chip->pdata->cpdata[channel].shunt_resistor);
	*voltage_mv = busv_register_to_mv(vbus);
exit:
	mutex_unlock(&chip->mutex);
	return ret;
}

static int __locked_set_crit_alert_register(struct ina3221_chip *chip,
					    u32 channel)
{
	struct ina3221_chan_pdata *cpdata = &chip->pdata->cpdata[channel];
	int shunt_volt_limit;

	chip->alert_enabled = 1;
	shunt_volt_limit = cpdata->crit_conf_limits * cpdata->shunt_resistor;
	shunt_volt_limit = uv_to_shuntv_register(shunt_volt_limit);

	return i2c_smbus_write_word_data(chip->client, INA3221_CRIT(channel),
					 cpu_to_be16(shunt_volt_limit));
}

static int __locked_set_warn_alert_register(struct ina3221_chip *chip,
					    u32 channel)
{
	struct ina3221_chan_pdata *cpdata = &chip->pdata->cpdata[channel];
	int shunt_volt_limit;

	chip->alert_enabled = 1;
	shunt_volt_limit = cpdata->warn_conf_limits * cpdata->shunt_resistor;
	shunt_volt_limit = uv_to_shuntv_register(shunt_volt_limit);
	return i2c_smbus_write_word_data(chip->client, INA3221_WARN(channel),
					 cpu_to_be16(shunt_volt_limit));
}

static int __locked_set_crit_warn_limits(struct ina3221_chip *chip)
{
	struct ina3221_chan_pdata *cpdata;
	int i;
	int ret = 0;

	for (i = 0; i < INA3221_NUMBER_OF_RAILS; i++) {
		cpdata = &chip->pdata->cpdata[i];

		if (cpdata->crit_conf_limits != U32_MINUS_1) {
			ret = __locked_set_crit_alert_register(chip, i);
			if (ret < 0)
				break;
		}

		if (cpdata->warn_conf_limits != U32_MINUS_1) {
			ret = __locked_set_warn_alert_register(chip, i);
			if (ret < 0)
				break;
		}
	}
	return ret;
}

static int ina3221_set_channel_critical(struct ina3221_chip *chip,
	int channel, int curr_limit)
{
	struct ina3221_chan_pdata *cpdata = &chip->pdata->cpdata[channel];
	int ret;

	mutex_lock(&chip->mutex);

	if (chip->shutdown_complete) {
		mutex_unlock(&chip->mutex);
		return -EIO;
	}

	cpdata->crit_conf_limits = curr_limit;
	ret = __locked_set_crit_alert_register(chip, channel);
	mutex_unlock(&chip->mutex);
	return ret;
}

static int ina3221_get_channel_critical(struct ina3221_chip *chip,
	int channel, int *curr_limit)
{
	struct ina3221_chan_pdata *cpdata = &chip->pdata->cpdata[channel];
	u32 crit_reg_addr = INA3221_CRIT(channel);
	int ret;

	mutex_lock(&chip->mutex);

	if (chip->shutdown_complete) {
		mutex_unlock(&chip->mutex);
		return -EIO;
	}

	/* getting voltage readings in micro volts*/
	ret = i2c_smbus_read_word_data(chip->client, crit_reg_addr);
	if (ret < 0) {
		dev_err(chip->dev, "Channel %d crit register read failed: %d\n",
			channel, ret);
		goto exit;
	}

	*curr_limit = shuntv_register_to_ma(be16_to_cpu(ret),
			cpdata->shunt_resistor);
	ret = 0;
exit:
	mutex_unlock(&chip->mutex);
	return ret;
}

static int ina3221_set_channel_warning(struct ina3221_chip *chip,
	int channel, int curr_limit)
{
	struct ina3221_chan_pdata *cpdata = &chip->pdata->cpdata[channel];
	int ret;

	mutex_lock(&chip->mutex);

	if (chip->shutdown_complete) {
		mutex_unlock(&chip->mutex);
		return -EIO;
	}

	cpdata->warn_conf_limits = curr_limit;
	ret = __locked_set_warn_alert_register(chip, channel);
	mutex_unlock(&chip->mutex);
	return ret;
}

static int ina3221_get_channel_warning(struct ina3221_chip *chip,
	int channel, int *curr_limit)
{
	struct ina3221_chan_pdata *cpdata = &chip->pdata->cpdata[channel];
	u32 warn_reg_addr = INA3221_WARN(channel);
	int ret;

	mutex_lock(&chip->mutex);

	if (chip->shutdown_complete) {
		mutex_unlock(&chip->mutex);
		return -EIO;
	}

	/* get warning shunt voltage threshold in micro volts. */
	ret = i2c_smbus_read_word_data(chip->client, warn_reg_addr);
	if (ret < 0) {
		dev_err(chip->dev, "Channel %d warn register read failed: %d\n",
			channel, ret);
		goto exit;
	}

	/* convert shunt voltage to current in mA */
	*curr_limit = shuntv_register_to_ma(be16_to_cpu(ret),
			cpdata->shunt_resistor);
	ret = 0;
exit:
	mutex_unlock(&chip->mutex);
	return ret;
}

static int __locked_ina3221_switch_mode(struct ina3221_chip *chip,
		   int cpus, int cpufreq)
{
	int ret = 0;

	if (!chip->alert_enabled)
		return 0;

	switch (chip->mode) {
	case TRIGGERED:
		if ((cpus >= CPU_THRESHOLD) ||
				(cpufreq >= CPU_FREQ_THRESHOLD)) {
			/**
			 * Turn INA on when cpu frequency crosses threshold or
			 * number of cpus crosses threshold
			 */
			dev_vdbg(chip->dev, "Turn-on cpus:%d, cpufreq:%d\n",
				cpus, cpufreq);

			ret = __locked_power_up_ina3221(chip,
					chip->pdata->cont_conf_data);
			if (ret < 0) {
				dev_err(chip->dev, "INA power up failed: %d\n",
					ret);
				return ret;
			}
			chip->mode = CONTINUOUS;
		}
		break;
	case CONTINUOUS:
		 if ((cpus < CPU_THRESHOLD) && (cpufreq < CPU_FREQ_THRESHOLD)) {
			/*
			 * Turn off ina when number of cpu cores on are below
			 * threshold and cpu frequency are below threshold
			 */
			dev_vdbg(chip->dev, "Turn-off, cpus:%d, cpufreq:%d\n",
				cpus, cpufreq);

			ret = __locked_power_down_ina3221(chip);
			if (ret < 0) {
				dev_err(chip->dev,
					"INA power down failed:%d\n", ret);
				return ret;
			}
			chip->mode = TRIGGERED;
		}
		break;
	case FORCED_CONTINUOUS:
	case FORCED_TRIGGERED:
	default:
		break;
	}
	return 0;
}

static int ina3221_cpufreq_notify(struct notifier_block *nb,
		unsigned long event, void *hcpu)
{
	struct ina3221_chip *chip = container_of(nb,
					struct ina3221_chip, nb_cpufreq);
	int cpufreq;
	int cpus;
	int ret = 0;

	if (event != CPUFREQ_POSTCHANGE)
		return 0;

	mutex_lock(&chip->mutex);
	if (chip->is_suspended || chip->shutdown_complete)
		goto exit;

	cpufreq = ((struct cpufreq_freqs *)hcpu)->new;
	cpus = num_online_cpus();
	dev_vdbg(chip->dev, "CPUfreq notified freq:%d cpus:%d\n",
			cpufreq, cpus);
	ret = __locked_ina3221_switch_mode(chip, cpus, cpufreq);
	if (ret < 0) {
		dev_err(chip->dev, "INA change mode failed %d\n", ret);
		goto exit;
	}
exit:
	mutex_unlock(&chip->mutex);
	return ret;
}

static int ina3221_hotplug_notify(struct notifier_block *nb,
		unsigned long event, void *hcpu)
{
	struct ina3221_chip *chip = container_of(nb,
					struct ina3221_chip, nb_hot);
	int cpus;
	int cpufreq = 0;
	int ret = 0;

	if (event == CPU_ONLINE || event == CPU_DEAD) {
		mutex_lock(&chip->mutex);
		if (chip->is_suspended) {
			mutex_unlock(&chip->mutex);
			return 0;
		}
		if (chip->shutdown_complete) {
			mutex_unlock(&chip->mutex);
			return -EIO;
		}
		cpufreq = cpufreq_quick_get(0);
		cpus = num_online_cpus();
		dev_vdbg(chip->dev, "hotplug notified cpufreq:%d cpus:%d\n",
				cpufreq, cpus);
		ret = __locked_ina3221_switch_mode(chip, cpus, cpufreq);

		if (ret < 0)
			dev_err(chip->dev, "INA switch mode failed: %d\n", ret);
		mutex_unlock(&chip->mutex);
	}
	return ret;
}

static int ina3221_read_raw(struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan, int *val, int *val2, long mask)
{
	struct ina3221_chip *chip = iio_priv(indio_dev);
	struct device *dev = chip->dev;
	int type = chan->type;
	int channel = chan->channel;
	int address = chan->address;
	int ret = 0;

	if (channel >= 3) {
		dev_err(dev, "Invalid channel Id %d\n", channel);
		return -EINVAL;
	}
	if (mask != IIO_CHAN_INFO_PROCESSED) {
		dev_err(dev, "Invalid mask 0x%08lx\n", mask);
		return -EINVAL;
	}

	switch (type) {
	case IIO_VOLTAGE:
		ret = ina3221_get_channel_voltage(chip, channel, val);
		break;

	case IIO_CURRENT:
		ret = ina3221_get_channel_current(chip, channel, address, val);
		break;

	case IIO_POWER:
		ret = ina3221_get_channel_power(chip, channel, address, val);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if (!ret)
		ret = IIO_VAL_INT;
	return ret;
}

static ssize_t ina3221_show_channel(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct ina3221_chip *chip = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int mode = UNPACK_MODE(this_attr->address);
	int channel = UNPACK_CHAN(this_attr->address);
	int ret;
	int current_ma;
	int voltage_mv;

	if (channel >= 3) {
		dev_err(dev, "Invalid channel Id %d\n", channel);
		return -EINVAL;
	}

	switch (mode) {
	case CHANNEL_NAME:
		return snprintf(buf, PAGE_SIZE, "%s\n",
				chip->pdata->cpdata[channel].rail_name);

	case CRIT_CURRENT_LIMIT:
		ret = ina3221_get_channel_critical(chip, channel, &current_ma);
		if (!ret)
			return snprintf(buf, PAGE_SIZE, "%d ma\n", current_ma);
		return ret;

	case WARN_CURRENT_LIMIT:
		ret = ina3221_get_channel_warning(chip, channel, &current_ma);
		if (!ret)
			return snprintf(buf, PAGE_SIZE, "%d ma\n", current_ma);

		return ret;

	case RUNNING_MODE:
		return ina3221_get_mode(chip, buf);

	case VBUS_VOLTAGE_CURRENT:
		ret = ina3221_get_channel_vbus_voltage_current(chip,
					channel, &current_ma, &voltage_mv);
		if (!ret)
			return snprintf(buf, PAGE_SIZE, "%d %d\n",
					voltage_mv, current_ma);
		return ret;

	default:
		break;
	}
	return -EINVAL;
}

static ssize_t ina3221_set_channel(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct ina3221_chip *chip = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int mode = UNPACK_MODE(this_attr->address);
	int channel = UNPACK_CHAN(this_attr->address);
	long val;
	int current_ma;
	int ret;

	if (channel >= 3) {
		dev_err(dev, "Invalid channel Id %d\n", channel);
		return -EINVAL;
	}

	switch (mode) {
	case CRIT_CURRENT_LIMIT:
		if (kstrtol(buf, 10, &val) < 0)
			return -EINVAL;

		current_ma = (int) val;
		ret = ina3221_set_channel_critical(chip, channel, current_ma);
		return ret < 0 ? ret : len;

	case WARN_CURRENT_LIMIT:
		if (kstrtol(buf, 10, &val) < 0)
			return -EINVAL;

		current_ma = (int) val;
		ret = ina3221_set_channel_warning(chip, channel, current_ma);
		return ret < 0 ? ret : len;

	case RUNNING_MODE:
		return ina3221_set_mode(chip, buf, len);
	}
	return -EINVAL;
}

static IIO_DEVICE_ATTR(rail_name_0, S_IRUGO | S_IWUSR,
		ina3221_show_channel, ina3221_set_channel,
		PACK_MODE_CHAN(CHANNEL_NAME, 0));
static IIO_DEVICE_ATTR(rail_name_1, S_IRUGO | S_IWUSR,
		ina3221_show_channel, ina3221_set_channel,
		PACK_MODE_CHAN(CHANNEL_NAME, 1));
static IIO_DEVICE_ATTR(rail_name_2, S_IRUGO | S_IWUSR,
		ina3221_show_channel, ina3221_set_channel,
		PACK_MODE_CHAN(CHANNEL_NAME, 2));

static IIO_DEVICE_ATTR(crit_current_limit_0, S_IRUGO | S_IWUSR,
		ina3221_show_channel, ina3221_set_channel,
		PACK_MODE_CHAN(CRIT_CURRENT_LIMIT, 0));
static IIO_DEVICE_ATTR(crit_current_limit_1, S_IRUGO | S_IWUSR,
		ina3221_show_channel, ina3221_set_channel,
		PACK_MODE_CHAN(CRIT_CURRENT_LIMIT, 1));
static IIO_DEVICE_ATTR(crit_current_limit_2, S_IRUGO | S_IWUSR,
		ina3221_show_channel, ina3221_set_channel,
		PACK_MODE_CHAN(CRIT_CURRENT_LIMIT, 2));

static IIO_DEVICE_ATTR(warn_current_limit_0, S_IRUGO | S_IWUSR,
		ina3221_show_channel, ina3221_set_channel,
		PACK_MODE_CHAN(WARN_CURRENT_LIMIT, 0));
static IIO_DEVICE_ATTR(warn_current_limit_1, S_IRUGO | S_IWUSR,
		ina3221_show_channel, ina3221_set_channel,
		PACK_MODE_CHAN(WARN_CURRENT_LIMIT, 1));
static IIO_DEVICE_ATTR(warn_current_limit_2, S_IRUGO | S_IWUSR,
		ina3221_show_channel, ina3221_set_channel,
		PACK_MODE_CHAN(WARN_CURRENT_LIMIT, 2));

static IIO_DEVICE_ATTR(ui_input_0, S_IRUGO | S_IWUSR,
		ina3221_show_channel, ina3221_set_channel,
		PACK_MODE_CHAN(VBUS_VOLTAGE_CURRENT, 0));
static IIO_DEVICE_ATTR(ui_input_1, S_IRUGO | S_IWUSR,
		ina3221_show_channel, ina3221_set_channel,
		PACK_MODE_CHAN(VBUS_VOLTAGE_CURRENT, 1));
static IIO_DEVICE_ATTR(ui_input_2, S_IRUGO | S_IWUSR,
		ina3221_show_channel, ina3221_set_channel,
		PACK_MODE_CHAN(VBUS_VOLTAGE_CURRENT, 2));

static IIO_DEVICE_ATTR(running_mode, S_IRUGO | S_IWUSR,
		ina3221_show_channel, ina3221_set_channel,
		PACK_MODE_CHAN(RUNNING_MODE, 0));

static struct attribute *ina3221_attributes[] = {
	&iio_dev_attr_rail_name_0.dev_attr.attr,
	&iio_dev_attr_rail_name_1.dev_attr.attr,
	&iio_dev_attr_rail_name_2.dev_attr.attr,
	&iio_dev_attr_crit_current_limit_0.dev_attr.attr,
	&iio_dev_attr_crit_current_limit_1.dev_attr.attr,
	&iio_dev_attr_crit_current_limit_2.dev_attr.attr,
	&iio_dev_attr_warn_current_limit_0.dev_attr.attr,
	&iio_dev_attr_warn_current_limit_1.dev_attr.attr,
	&iio_dev_attr_warn_current_limit_2.dev_attr.attr,
	&iio_dev_attr_ui_input_0.dev_attr.attr,
	&iio_dev_attr_ui_input_1.dev_attr.attr,
	&iio_dev_attr_ui_input_2.dev_attr.attr,
	&iio_dev_attr_running_mode.dev_attr.attr,
	NULL,
};

static const struct attribute_group ina3221_groups = {
	.attrs = ina3221_attributes,
};

#define channel_type(_type, _add, _channel, _name)			\
	{								\
		.type = _type,						\
		.indexed = 1,						\
		.address = _add,					\
		.channel = _channel,					\
		.extend_name = _name,					\
		.info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),	\
	}

#define channel_spec(chan)						\
	channel_type(IIO_VOLTAGE, 0, chan, NULL),			\
	channel_type(IIO_CURRENT, 0, chan, NULL),			\
	channel_type(IIO_CURRENT, 1, chan, "trigger"),			\
	channel_type(IIO_POWER, 0, chan, NULL),				\
	channel_type(IIO_POWER, 1, chan, "trigger")

static const struct iio_chan_spec ina3221_channels_spec[] = {
	channel_spec(0),
	channel_spec(1),
	channel_spec(2),
};

static const struct iio_info ina3221_info = {
	.attrs = &ina3221_groups,
	.driver_module = THIS_MODULE,
	.read_raw = &ina3221_read_raw,
};

static struct ina3221_platform_data *ina3221_get_platform_data_dt(
	struct i2c_client *client)
{
	struct ina3221_platform_data *pdata;
	struct device *dev = &client->dev;
	struct device_node *np = dev->of_node;
	struct device_node *child;
	u32 reg;
	int ret;
	u32 pval;
	int valid_channel = 0;

	if (!np) {
		dev_err(&client->dev, "Only DT supported\n");
		return ERR_PTR(-ENODEV);
	}

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&client->dev, "pdata allocation failed\n");
		return ERR_PTR(-ENOMEM);
	}

	ret = of_property_read_u32(np, "ti,continuous-config", &pval);
	if (!ret)
		pdata->cont_conf_data = (u16)pval;

	ret = of_property_read_u32(np, "ti,trigger-config", &pval);
	if (!ret)
		pdata->trig_conf_data = (u16)pval;

	pdata->enable_forced_continuous = of_property_read_bool(np,
				"ti,enable-forced-continuous");

	for_each_child_of_node(np, child) {
		ret = of_property_read_u32(child, "reg", &reg);
		if (ret || reg >= 3) {
			dev_err(dev, "reg property invalid on node %s\n",
				child->name);
			continue;
		}

		pdata->cpdata[reg].rail_name =  of_get_property(child,
						"ti,rail-name", NULL);
		if (!pdata->cpdata[reg].rail_name) {
			dev_err(dev, "Rail name is not provided on node %s\n",
				child->full_name);
			continue;
		}

		ret = of_property_read_u32(child, "ti,current-warning-limit-ma",
				&pval);
		if (!ret)
			pdata->cpdata[reg].warn_conf_limits = pval;
		else
			pdata->cpdata[reg].warn_conf_limits = U32_MINUS_1;

		ret = of_property_read_u32(child,
				"ti,current-critical-limit-ma", &pval);
		if (!ret)
			pdata->cpdata[reg].crit_conf_limits = pval;
		else
			pdata->cpdata[reg].crit_conf_limits = U32_MINUS_1;

		ret = of_property_read_u32(child, "ti,shunt-resistor-mohm",
				&pval);
		if (!ret)
			pdata->cpdata[reg].shunt_resistor = pval;

		valid_channel++;
	}

	if (!valid_channel)
		return ERR_PTR(-EINVAL);

	return pdata;
}

static int ina3221_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct ina3221_chip *chip;
	struct iio_dev *indio_dev;
	struct ina3221_platform_data *pdata;
	int ret;

	pdata = ina3221_get_platform_data_dt(client);
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
	chip->dev = &client->dev;
	chip->client = client;
	i2c_set_clientdata(client, indio_dev);
	chip->pdata = pdata;
	mutex_init(&chip->mutex);

	if (pdata->enable_forced_continuous)
		chip->mode = FORCED_CONTINUOUS;
	else
		chip->mode = TRIGGERED;
	chip->shutdown_complete = 0;
	chip->is_suspended = 0;

	indio_dev->info = &ina3221_info;
	indio_dev->channels = ina3221_channels_spec;
	indio_dev->num_channels = ARRAY_SIZE(ina3221_channels_spec);
	indio_dev->name = id->name;
	indio_dev->dev.parent = &client->dev;
	indio_dev->modes = INDIO_DIRECT_MODE;
	ret = devm_iio_device_register(chip->dev, indio_dev);
	if (ret < 0) {
		dev_err(chip->dev, "iio registration fails with error %d\n",
			ret);
		return ret;
	}

	/* reset ina3221 */
	ret = i2c_smbus_write_word_data(client, INA3221_CONFIG,
		__constant_cpu_to_be16((INA3221_RESET)));
	if (ret < 0) {
		dev_err(&client->dev, "ina3221 reset failure status: 0x%x\n",
			ret);
		return ret;
	}

	chip->nb_hot.notifier_call = ina3221_hotplug_notify;
	chip->nb_cpufreq.notifier_call = ina3221_cpufreq_notify;
	register_hotcpu_notifier(&(chip->nb_hot));
	cpufreq_register_notifier(&(chip->nb_cpufreq),
			CPUFREQ_TRANSITION_NOTIFIER);

	ret = __locked_set_crit_warn_limits(chip);
	if (ret < 0) {
		dev_info(&client->dev, "Not able to set warn and crit limits!\n");
		/*Not an error condition, could let the probe continue*/
	}

	if (chip->mode == FORCED_CONTINUOUS) {
		ret = ina3221_set_mode_val(chip, FORCED_CONTINUOUS);
		if (ret < 0) {
			dev_err(&client->dev,
				"INA forced continuous failed: %d\n", ret);
			goto exit_pd;
		}
	} else {
		ret = __locked_power_down_ina3221(chip);
		if (ret < 0) {
			dev_err(&client->dev,
				"INA power down failed: %d\n", ret);
			goto exit_pd;
		}
	}
	return 0;

exit_pd:
	unregister_hotcpu_notifier(&(chip->nb_hot));
	cpufreq_unregister_notifier(&(chip->nb_cpufreq),
			CPUFREQ_TRANSITION_NOTIFIER);
	return ret;
}

static int ina3221_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct ina3221_chip *chip = iio_priv(indio_dev);

	mutex_lock(&chip->mutex);
	__locked_power_down_ina3221(chip);
	mutex_unlock(&chip->mutex);
	unregister_hotcpu_notifier(&(chip->nb_hot));
	cpufreq_unregister_notifier(&(chip->nb_cpufreq),
			CPUFREQ_TRANSITION_NOTIFIER);
	return 0;
}

static void ina3221_shutdown(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct ina3221_chip *chip = iio_priv(indio_dev);

	mutex_lock(&chip->mutex);
	__locked_power_down_ina3221(chip);
	chip->shutdown_complete = 1;
	mutex_unlock(&chip->mutex);
}

#ifdef CONFIG_PM_SLEEP
static int ina3221_suspend(struct device *dev)
{
	struct ina3221_chip *chip = to_ina3221_chip(dev);
	int ret = 0;

	mutex_lock(&chip->mutex);
	if (chip->mode != FORCED_CONTINUOUS) {
		ret = __locked_power_down_ina3221(chip);
		if (ret < 0) {
			dev_err(dev, "INA can't be turned off: 0x%x\n", ret);
			goto error;
		}
	}
	if (chip->mode == CONTINUOUS)
		chip->mode = TRIGGERED;
	chip->is_suspended = 1;
error:
	mutex_unlock(&chip->mutex);
	return ret;
}

static int ina3221_resume(struct device *dev)
{
	struct ina3221_chip *chip = to_ina3221_chip(dev);
	int cpufreq, cpus;
	int ret = 0;

	mutex_lock(&chip->mutex);
	if (chip->mode == FORCED_CONTINUOUS) {
		ret = __locked_power_up_ina3221(chip,
						chip->pdata->cont_conf_data);
	} else {
		cpufreq = cpufreq_quick_get(0);
		cpus = num_online_cpus();
		ret = __locked_ina3221_switch_mode(chip, cpus, cpufreq);
	}
	if (ret < 0)
		dev_err(dev, "INA can't be turned off/on: 0x%x\n", ret);
	chip->is_suspended = 0;
	mutex_unlock(&chip->mutex);
	return ret;
}
#endif

static const struct dev_pm_ops ina3221_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ina3221_suspend,
				ina3221_resume)
};

static const struct i2c_device_id ina3221_id[] = {
	{.name = "ina3221x",},
	{},
};
MODULE_DEVICE_TABLE(i2c, ina3221_id);

static struct i2c_driver ina3221_driver = {
	.driver = {
		.name	= "ina3221x",
		.owner = THIS_MODULE,
		.pm = &ina3221_pm_ops,
	},
	.probe		= ina3221_probe,
	.remove		= ina3221_remove,
	.shutdown	= ina3221_shutdown,
	.id_table	= ina3221_id,
};

module_i2c_driver(ina3221_driver);

MODULE_DESCRIPTION("TI INA3221 3-Channel Shunt and Bus Voltage Monitor");
MODULE_AUTHOR("Deepak Nibade <dnibade@nvidia.com>");
MODULE_AUTHOR("Timo Alho <talho@nvidia.com>");
MODULE_AUTHOR("Anshul Jain <anshulj@nvidia.com>");
MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_LICENSE("GPL v2");
