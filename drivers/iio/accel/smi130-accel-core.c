/*
 * 3-axis accelerometer driver supporting following Bosch-Sensortec chips:
 *  - SMI130
 *
 * Copyright (c) 2016, Tesla Motors.
 *
 * Based on bmc150 driver:
 * Copyright (c) 2014, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iio/events.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/regmap.h>

#include "smi130-accel.h"

#define SMI130_ACCEL_DRV_NAME			"smi130_accel"
#define SMI130_ACCEL_IRQ_NAME			"smi130_accel_event"

#define SMI130_ACCEL_REG_CHIP_ID		0x00

#define SMI130_ACCEL_REG_XOUT_L			0x02

#define SMI130_ACCEL_REG_TEMP			0x08
#define SMI130_ACCEL_TEMP_CENTER_VAL			24

#define SMI130_ACCEL_REG_PMU_RANGE		0x0F
#define SMI130_ACCEL_DEF_RANGE_2G			0x03
#define SMI130_ACCEL_DEF_RANGE_4G			0x05
#define SMI130_ACCEL_DEF_RANGE_8G			0x08
#define SMI130_ACCEL_DEF_RANGE_16G			0x0C

/* Default BW: 125Hz */
#define SMI130_ACCEL_REG_PMU_BW			0x10
#define SMI130_ACCEL_DEF_BW				125

#define SMI130_ACCEL_REG_RESET			0x14
#define SMI130_ACCEL_RESET_VAL				0xB6

#define SMI130_ACCEL_REG_INT_EN_1		0x17
#define SMI130_ACCEL_INT_EN_BIT_DATA_EN			BIT(4)
#define SMI130_ACCEL_INT_EN_BIT_FFULL_EN		BIT(5)
#define SMI130_ACCEL_INT_EN_BIT_FWM_EN			BIT(6)

#define SMI130_ACCEL_REG_INT_MAP_1		0x1A
#define SMI130_ACCEL_INT_MAP_1_BIT_DATA			BIT(0)

#define SMI130_ACCEL_AXIS_TO_REG(axis)		(SMI130_ACCEL_REG_XOUT_L + (axis * 2))

enum smi130_accel_axis {
	AXIS_X,
	AXIS_Y,
	AXIS_Z,
	AXIS_MAX,
};

struct smi130_scale_info {
	int scale;
	u8 reg_range;
};

struct smi130_accel_chip_info {
	const char *name;
	u8 chip_id;
	const struct iio_chan_spec *channels;
	int num_channels;
	const struct smi130_scale_info scale_table[4];
};

struct smi130_accel_trigger {
	struct smi130_accel_data *data;
	struct iio_trigger *indio_trig;
	int (*setup)(struct smi130_accel_trigger *t, bool state);
	int intr;
	bool enabled;
};

enum smi130_accel_trigger_id {
	SMI130_ACCEL_TRIGGER_DATA_READY,
	SMI130_ACCEL_TRIGGERS,
};

struct smi130_accel_data {
	struct regmap *regmap;
	atomic_t active_intr;
	struct smi130_accel_trigger triggers[SMI130_ACCEL_TRIGGERS];
	struct mutex mutex;
	s16 buffer[8];
	u8 bw_bits;
	u32 range;
	int ev_enable_state;
	const struct smi130_accel_chip_info *chip_info;
};

static const struct {
	int val;
	int val2;
	u8 bw_bits;
} smi130_accel_samp_freq_table[] = { {7, 810000, 0x08},
				     {15, 630000, 0x09},
				     {31, 250000, 0x0A},
				     {62, 500000, 0x0B},
				     {125, 0, 0x0C},
				     {250, 0, 0x0D},
				     {500, 0, 0x0E},
				     {1000, 0, 0x0F} };

const struct regmap_config smi130_regmap_conf = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x3f,
};
EXPORT_SYMBOL_GPL(smi130_regmap_conf);

static int smi130_accel_set_bw(struct smi130_accel_data *data, int val,
			       int val2)
{
	int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(smi130_accel_samp_freq_table); ++i) {
		if (smi130_accel_samp_freq_table[i].val == val &&
		    smi130_accel_samp_freq_table[i].val2 == val2) {
			ret = regmap_write(data->regmap,
				SMI130_ACCEL_REG_PMU_BW,
				smi130_accel_samp_freq_table[i].bw_bits);
			if (ret < 0)
				return ret;

			data->bw_bits =
				smi130_accel_samp_freq_table[i].bw_bits;
			return 0;
		}
	}

	return -EINVAL;
}

static int smi130_accel_get_bw(struct smi130_accel_data *data, int *val,
			       int *val2)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(smi130_accel_samp_freq_table); ++i) {
		if (smi130_accel_samp_freq_table[i].bw_bits == data->bw_bits) {
			*val = smi130_accel_samp_freq_table[i].val;
			*val2 = smi130_accel_samp_freq_table[i].val2;
			return IIO_VAL_INT_PLUS_MICRO;
		}
	}

	return -EINVAL;
}

static int smi130_accel_set_scale(struct smi130_accel_data *data, int val)
{
	struct device *dev = regmap_get_device(data->regmap);
	int ret, i;

	for (i = 0; i < ARRAY_SIZE(data->chip_info->scale_table); ++i) {
		if (data->chip_info->scale_table[i].scale == val) {
			ret = regmap_write(data->regmap,
				     SMI130_ACCEL_REG_PMU_RANGE,
				     data->chip_info->scale_table[i].reg_range);
			if (ret < 0) {
				dev_err(dev, "Error writing pmu_range\n");
				return ret;
			}

			data->range = data->chip_info->scale_table[i].reg_range;
			return 0;
		}
	}

	return -EINVAL;
}

static int smi130_accel_get_temp(struct smi130_accel_data *data, int *val)
{
	struct device *dev = regmap_get_device(data->regmap);
	int ret;
	unsigned int value;

	mutex_lock(&data->mutex);

	ret = regmap_read(data->regmap, SMI130_ACCEL_REG_TEMP, &value);
	if (ret < 0) {
		dev_err(dev, "Error reading reg_temp\n");
		mutex_unlock(&data->mutex);
		return ret;
	}
	*val = sign_extend32(value, 7);

	mutex_unlock(&data->mutex);

	return IIO_VAL_INT;
}

static int smi130_accel_get_axis(struct smi130_accel_data *data,
				 struct iio_chan_spec const *chan,
				 int *val)
{
	struct device *dev = regmap_get_device(data->regmap);
	int ret;
	int axis = chan->scan_index;
	__le16 raw_val;

	mutex_lock(&data->mutex);

	ret = regmap_bulk_read(data->regmap, SMI130_ACCEL_AXIS_TO_REG(axis),
			       &raw_val, sizeof(raw_val));
	if (ret < 0) {
		dev_err(dev, "Error reading axis %d\n", axis);
		mutex_unlock(&data->mutex);
		return ret;
	}
	*val = sign_extend32(le16_to_cpu(raw_val) >> chan->scan_type.shift,
			     chan->scan_type.realbits - 1);
	mutex_unlock(&data->mutex);
	if (ret < 0)
		return ret;

	return IIO_VAL_INT;
}

static int smi130_accel_read_raw(struct iio_dev *indio_dev,
				 struct iio_chan_spec const *chan,
				 int *val, int *val2, long mask)
{
	struct smi130_accel_data *data = iio_priv(indio_dev);
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		switch (chan->type) {
		case IIO_TEMP:
			return smi130_accel_get_temp(data, val);
		case IIO_ACCEL:
			if (iio_buffer_enabled(indio_dev))
				return -EBUSY;
			else
				return smi130_accel_get_axis(data, chan, val);
		default:
			return -EINVAL;
		}
	case IIO_CHAN_INFO_OFFSET:
		if (chan->type == IIO_TEMP) {
			*val = SMI130_ACCEL_TEMP_CENTER_VAL;
			return IIO_VAL_INT;
		} else {
			return -EINVAL;
		}
	case IIO_CHAN_INFO_SCALE:
		*val = 0;
		switch (chan->type) {
		case IIO_TEMP:
			*val2 = 500000;
			return IIO_VAL_INT_PLUS_MICRO;
		case IIO_ACCEL:
		{
			int i;
			const struct smi130_scale_info *si;
			int st_size = ARRAY_SIZE(data->chip_info->scale_table);

			for (i = 0; i < st_size; ++i) {
				si = &data->chip_info->scale_table[i];
				if (si->reg_range == data->range) {
					*val2 = si->scale;
					return IIO_VAL_INT_PLUS_MICRO;
				}
			}
			return -EINVAL;
		}
		default:
			return -EINVAL;
		}
	case IIO_CHAN_INFO_SAMP_FREQ:
		mutex_lock(&data->mutex);
		ret = smi130_accel_get_bw(data, val, val2);
		mutex_unlock(&data->mutex);
		return ret;
	default:
		return -EINVAL;
	}
}

static int smi130_accel_write_raw(struct iio_dev *indio_dev,
				  struct iio_chan_spec const *chan,
				  int val, int val2, long mask)
{
	struct smi130_accel_data *data = iio_priv(indio_dev);
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_SAMP_FREQ:
		mutex_lock(&data->mutex);
		ret = smi130_accel_set_bw(data, val, val2);
		mutex_unlock(&data->mutex);
		break;
	case IIO_CHAN_INFO_SCALE:
		if (val)
			return -EINVAL;

		mutex_lock(&data->mutex);
		ret = smi130_accel_set_scale(data, val2);
		mutex_unlock(&data->mutex);
		return ret;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static IIO_CONST_ATTR_SAMP_FREQ_AVAIL(
		"7.810000 15.630000 31.250000 62.50000 125 250 500 1000");

static IIO_CONST_ATTR(scale_available, "0.000976 0.001953 0.003906 0.007812");


static struct attribute *smi130_accel_attributes[] = {
	&iio_const_attr_sampling_frequency_available.dev_attr.attr,
	&iio_const_attr_scale_available.dev_attr.attr,
	NULL,
};

static const struct attribute_group smi130_accel_attrs_group = {
	.attrs = smi130_accel_attributes,
};

#define SMI130_ACCEL_CHANNEL(_axis, bits) {				\
	.type = IIO_ACCEL,						\
	.modified = 1,							\
	.channel2 = IIO_MOD_##_axis,					\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),			\
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE) |		\
				BIT(IIO_CHAN_INFO_SAMP_FREQ),		\
	.scan_index = AXIS_##_axis,					\
	.scan_type = {							\
		.sign = 's',						\
		.realbits = (bits),					\
		.storagebits = 16,					\
		.shift = 16 - (bits),					\
		.endianness = IIO_LE,					\
	},								\
	.num_event_specs = 0						\
}

#define SMI130_ACCEL_CHANNELS(bits) {					\
	{								\
		.type = IIO_TEMP,					\
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |		\
				      BIT(IIO_CHAN_INFO_SCALE) |	\
				      BIT(IIO_CHAN_INFO_OFFSET),	\
		.scan_index = -1,					\
	},								\
	SMI130_ACCEL_CHANNEL(X, bits),					\
	SMI130_ACCEL_CHANNEL(Y, bits),					\
	SMI130_ACCEL_CHANNEL(Z, bits),					\
	IIO_CHAN_SOFT_TIMESTAMP(3),					\
}

static const struct iio_chan_spec smi130_accel_channels[] =
	SMI130_ACCEL_CHANNELS(12);

static const struct smi130_accel_chip_info smi130_accel_chip_info_tbl[] = {
	[smi130] = {
		.name = "SMI130",
		.chip_id = 0xFA,
		.channels = smi130_accel_channels,
		.num_channels = ARRAY_SIZE(smi130_accel_channels),
		.scale_table = { {(1000000/1024), SMI130_ACCEL_DEF_RANGE_2G},
				 {(1000000/512), SMI130_ACCEL_DEF_RANGE_4G},
				 {(1000000/256), SMI130_ACCEL_DEF_RANGE_8G},
				 {(1000000/128), SMI130_ACCEL_DEF_RANGE_16G} },
	},
};

static const struct iio_info smi130_accel_info = {
	.attrs			= &smi130_accel_attrs_group,
	.read_raw		= smi130_accel_read_raw,
	.write_raw		= smi130_accel_write_raw,
	.driver_module		= THIS_MODULE,
};

static const unsigned long smi130_accel_scan_masks[] = {
					BIT(AXIS_X) | BIT(AXIS_Y) | BIT(AXIS_Z),
					0};

static irqreturn_t smi130_accel_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct smi130_accel_data *data = iio_priv(indio_dev);
	int ret;

	mutex_lock(&data->mutex);
	ret = regmap_bulk_read(data->regmap, SMI130_ACCEL_REG_XOUT_L,
			       data->buffer, AXIS_MAX * 2);
	mutex_unlock(&data->mutex);
	if (ret < 0)
		goto err_read;

	iio_push_to_buffers_with_timestamp(indio_dev, data->buffer,
					   pf->timestamp);
err_read:
	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;
}

static int smi130_accel_chip_init(struct smi130_accel_data *data)
{
	struct device *dev = regmap_get_device(data->regmap);
	int ret, i;
	unsigned int val;

	regmap_write(data->regmap, SMI130_ACCEL_REG_RESET, SMI130_ACCEL_RESET_VAL);
	usleep_range(1800, 20000);

	ret = regmap_read(data->regmap, SMI130_ACCEL_REG_CHIP_ID, &val);
	if (ret < 0) {
		dev_err(dev, "Error: Reading chip id\n");
		return ret;
	}

	dev_dbg(dev, "Chip Id %x\n", val);
	for (i = 0; i < ARRAY_SIZE(smi130_accel_chip_info_tbl); i++) {
		if (smi130_accel_chip_info_tbl[i].chip_id == val) {
			data->chip_info = &smi130_accel_chip_info_tbl[i];
			break;
		}
	}

	if (!data->chip_info) {
		dev_err(dev, "Invalid chip %x\n", val);
		return -ENODEV;
	}

	/* Set Bandwidth */
	ret = smi130_accel_set_bw(data, SMI130_ACCEL_DEF_BW, 0);
	if (ret < 0)
		return ret;

	/* Set Default Range */
	ret = regmap_write(data->regmap, SMI130_ACCEL_REG_PMU_RANGE,
			   SMI130_ACCEL_DEF_RANGE_4G);
	if (ret < 0) {
		dev_err(dev, "Error writing reg_pmu_range\n");
		return ret;
	}

	/* XXX Where is this actually set in HW? */
	data->range = SMI130_ACCEL_DEF_RANGE_4G;

	return 0;
}

int smi130_accel_core_probe(struct device *dev, struct regmap *regmap, const char *name)
{
	struct smi130_accel_data *data;
	struct iio_dev *indio_dev;
	int ret;

	indio_dev = devm_iio_device_alloc(dev, sizeof(*data));
	if (!indio_dev)
		return -ENOMEM;

	data = iio_priv(indio_dev);
	dev_set_drvdata(dev, indio_dev);

	data->regmap = regmap;

	ret = smi130_accel_chip_init(data);
	if (ret < 0)
		return ret;

	mutex_init(&data->mutex);

	indio_dev->dev.parent = dev;
	indio_dev->channels = data->chip_info->channels;
	indio_dev->num_channels = data->chip_info->num_channels;
	indio_dev->name = name ? name : data->chip_info->name;
	indio_dev->available_scan_masks = smi130_accel_scan_masks;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &smi130_accel_info;

	ret = iio_triggered_buffer_setup(indio_dev,
					 &iio_pollfunc_store_time,
					 smi130_accel_trigger_handler,
					 NULL);
	if (ret < 0) {
		dev_err(dev, "Failed: iio triggered buffer setup\n");
		return ret;
	}

	ret = iio_device_register(indio_dev);
	if (ret < 0) {
		dev_err(dev, "Unable to register iio device\n");
		goto err_buffer_cleanup;
	}

	return 0;

err_buffer_cleanup:
	iio_triggered_buffer_cleanup(indio_dev);

	return ret;
}
EXPORT_SYMBOL_GPL(smi130_accel_core_probe);

int smi130_accel_core_remove(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);

	iio_device_unregister(indio_dev);

	iio_triggered_buffer_cleanup(indio_dev);

	return 0;
}
EXPORT_SYMBOL_GPL(smi130_accel_core_remove);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("SMI130 accelerometer driver");
