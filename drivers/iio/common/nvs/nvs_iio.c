/* Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* The NVS = NVidia Sensor framework */
/* The NVS implementation of scan_elements enable/disable works as follows
 * (See NVS HAL for further explaination):
 * To enable, the NVS HAL will:
 * 1. Disable buffer
 * 2. Enable channels
 * 3. Calculate buffer alignments based on enabled channels
 * 4. Enable buffer
 * It is expected that the NVS kernel driver will detect the channels enabled
 * and enable the device using the IIO iio_buffer_setup_ops.
 * To disable, the NVS HAL will:
 * 1. Disable buffer
 * 2. Disable channels
 * 3. Calculate buffer alignments based on enabled channels
 * 4. If (one or more channels are enabled)
 *        4a. Enable buffer
 *    else
 *        4b. Disable master enable
 * It is expected that the master enable will be enabled as part of the
 * iio_buffer_setup_ops.
 * The NVS sysfs attribute for the master enable is "enable" without any
 * channel name.
 * The master enable is an enhancement of the standard IIO enable/disable
 * procedure.  Consider a device that contains multiple sensors.  To enable all
 * the sensors with the standard IIO enable mechanism, the device would be
 * powered off and on for each sensor that was enabled.  By having a master
 * enable, the device does not have to be powerered down for each buffer
 * disable but only when the master enable is disabled, thereby improving
 * efficiency.
 */
/* This NVS kernel driver has a test mechanism for sending specific data up the
 * SW stack by writing the requested data value to the IIO raw sysfs attribute.
 * That data will be sent anytime there would normally be new data from the
 * device.
 * The feature is disabled whenever the device is disabled.  It remains
 * disabled until the IIO raw sysfs attribute is written to again.
 */
/* The NVS HAL will use the IIO scale and offset sysfs attributes to modify the
 * data using the following formula: (data * scale) + offset
 * A scale value of 0 disables scale.
 * A scale value of 1 puts the NVS HAL into calibration mode where the scale
 * and offset are read everytime the data is read to allow realtime calibration
 * of the scale and offset values to be used in the device tree parameters.
 * Keep in mind the data is buffered but the NVS HAL will display the data and
 * scale/offset parameters in the log.  See calibration steps below.
 */


#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/trigger.h>
#include <linux/nvs.h>

#define NVS_IIO_DRIVER_VERSION		(213)

enum NVS_ATTR {
	NVS_ATTR_ENABLE,
	NVS_ATTR_PART,
	NVS_ATTR_VENDOR,
	NVS_ATTR_VERSION,
	NVS_ATTR_MILLIAMP,
	NVS_ATTR_FIFO_RSRV_EVNT_CNT,
	NVS_ATTR_FIFO_MAX_EVNT_CNT,
	NVS_ATTR_FLAGS,
	NVS_ATTR_MATRIX,
	NVS_ATTR_SELF_TEST,
};

enum NVS_DBG {
	NVS_INFO_DATA = 0,
	NVS_INFO_VER,
	NVS_INFO_ERRS,
	NVS_INFO_RESET,
	NVS_INFO_REGS,
	NVS_INFO_CFG,
	NVS_INFO_DBG,
	NVS_INFO_DBG_DATA,
	NVS_INFO_DBG_BUF,
	NVS_INFO_DBG_IRQ,
	NVS_INFO_LIMIT_MAX,
	NVS_INFO_DBG_KEY = 0x131071D,
};

static ssize_t nvs_info_show(struct device *dev,
			     struct device_attribute *attr, char *buf);
static ssize_t nvs_info_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count);
static ssize_t nvs_attr_show(struct device *dev,
			     struct device_attribute *attr, char *buf);
static ssize_t nvs_attr_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count);

static DEVICE_ATTR(nvs, S_IRUGO | S_IWUSR | S_IWGRP,
		   nvs_info_show, nvs_info_store);
static IIO_DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
		       nvs_attr_show, nvs_attr_store, NVS_ATTR_ENABLE);
static IIO_DEVICE_ATTR(part, S_IRUGO,
		       nvs_attr_show, NULL, NVS_ATTR_PART);
static IIO_DEVICE_ATTR(vendor, S_IRUGO,
		       nvs_attr_show, NULL, NVS_ATTR_VENDOR);
static IIO_DEVICE_ATTR(version, S_IRUGO,
		       nvs_attr_show, NULL, NVS_ATTR_VERSION);
static IIO_DEVICE_ATTR(milliamp, S_IRUGO,
		       nvs_attr_show, NULL, NVS_ATTR_MILLIAMP);
static IIO_DEVICE_ATTR(fifo_reserved_event_count, S_IRUGO,
		       nvs_attr_show, NULL, NVS_ATTR_FIFO_RSRV_EVNT_CNT);
static IIO_DEVICE_ATTR(fifo_max_event_count, S_IRUGO,
		       nvs_attr_show, NULL, NVS_ATTR_FIFO_MAX_EVNT_CNT);
static IIO_DEVICE_ATTR(flags, S_IRUGO | S_IWUSR | S_IWGRP,
		       nvs_attr_show, nvs_attr_store, NVS_ATTR_FLAGS);
/* matrix permissions are read only - writes are for debug */
static IIO_DEVICE_ATTR(matrix, S_IRUGO,
		       nvs_attr_show, nvs_attr_store, NVS_ATTR_MATRIX);
static IIO_DEVICE_ATTR(self_test, S_IRUGO,
		       nvs_attr_show, NULL, NVS_ATTR_SELF_TEST);

static struct attribute *nvs_attrs[] = {
	&dev_attr_nvs.attr,
	&iio_dev_attr_enable.dev_attr.attr,
	&iio_dev_attr_part.dev_attr.attr,
	&iio_dev_attr_vendor.dev_attr.attr,
	&iio_dev_attr_version.dev_attr.attr,
	&iio_dev_attr_milliamp.dev_attr.attr,
	&iio_dev_attr_fifo_reserved_event_count.dev_attr.attr,
	&iio_dev_attr_fifo_max_event_count.dev_attr.attr,
	&iio_dev_attr_flags.dev_attr.attr,
	&iio_dev_attr_matrix.dev_attr.attr,
	&iio_dev_attr_self_test.dev_attr.attr,
	NULL
};

struct nvs_state {
	void *client;
	struct device *dev;
	struct nvs_fn_dev *fn_dev;
	struct sensor_cfg *cfg;
	struct iio_trigger *trig;
	struct iio_chan_spec *ch;
	struct attribute *attrs[ARRAY_SIZE(nvs_attrs)];
	struct attribute_group attr_group;
	struct iio_info info;
	bool shutdown;
	bool suspend;
	bool flush;
	int enabled;
	int batch_flags;
	unsigned int batch_period_us;
	unsigned int batch_timeout_us;
	unsigned int dbg;
	unsigned int fn_dev_sts;
	unsigned int fn_dev_errs;
	long dbg_data_lock;
	s64 ts_diff;
	s64 ts;
	u8 *buf;
};

struct nvs_iio_ch {
	const char *snsr_name;
	enum iio_chan_type typ;
	enum iio_modifier *mod;
};

static enum iio_modifier nvs_iio_modifier_none[] = {
	IIO_NO_MOD,
};

static enum iio_modifier nvs_iio_modifier_vec[] = {
	IIO_MOD_X,
	IIO_MOD_Y,
	IIO_MOD_Z,
	IIO_MOD_STATUS,
	IIO_NO_MOD,
};

static enum iio_modifier nvs_iio_modifier_uncal[] = {
	IIO_MOD_X_UNCALIB,
	IIO_MOD_Y_UNCALIB,
	IIO_MOD_Z_UNCALIB,
	IIO_MOD_X_BIAS,
	IIO_MOD_Y_BIAS,
	IIO_MOD_Z_BIAS,
	IIO_MOD_STATUS,
	IIO_NO_MOD,
};

static enum iio_modifier nvs_iio_modifier_heart[] = {
	IIO_MOD_BPM,
	IIO_MOD_STATUS,
	IIO_NO_MOD,
};

static const struct nvs_iio_ch nvs_iio_ch_tbl[] = {
	/* unknown sensor type */
	{
		.snsr_name		= "generic_sensor",
		.typ			= IIO_GENERIC,
		.mod			= nvs_iio_modifier_none,
	},
	/* SENSOR_TYPE_ACCELEROMETER */
	{
		.snsr_name		= "accelerometer",
		.typ			= IIO_ACCEL,
		.mod			= nvs_iio_modifier_vec,
	},
	/* SENSOR_TYPE_MAGNETIC_FIELD */
	{
		.snsr_name		= "magnetic_field",
		.typ			= IIO_MAGN,
		.mod			= nvs_iio_modifier_vec,
	},
	/* SENSOR_TYPE_ORIENTATION */
	{
		.snsr_name		= "orientation",
		.typ			= IIO_ORIENTATION,
		.mod			= nvs_iio_modifier_vec,
	},
	/* SENSOR_TYPE_GYROSCOPE */
	{
		.snsr_name		= "gyroscope",
		.typ			= IIO_ANGL_VEL,
		.mod			= nvs_iio_modifier_vec,
	},
	/* SENSOR_TYPE_LIGHT */
	{
		.snsr_name		= "light",
		.typ			= IIO_LIGHT,
		.mod			= nvs_iio_modifier_none,
	},
	/* SENSOR_TYPE_PRESSURE */
	{
		.snsr_name		= "pressure",
		.typ			= IIO_PRESSURE,
		.mod			= nvs_iio_modifier_none,
	},
	/* SENSOR_TYPE_TEMPERATURE */
	{
		.snsr_name		= "temperature",
		.typ			= IIO_TEMP,
		.mod			= nvs_iio_modifier_none,
	},
	/* SENSOR_TYPE_PROXIMITY */
	{
		.snsr_name		= "proximity",
		.typ			= IIO_PROXIMITY,
		.mod			= nvs_iio_modifier_none,
	},
	/* SENSOR_TYPE_GRAVITY */
	{
		.snsr_name		= "gravity",
		.typ			= IIO_GRAVITY,
		.mod			= nvs_iio_modifier_vec,
	},
	/* SENSOR_TYPE_LINEAR_ACCELERATION */
	{
		.snsr_name		= "linear_acceleration",
		.typ			= IIO_LINEAR_ACCEL,
		.mod			= nvs_iio_modifier_vec,
	},
	/* SENSOR_TYPE_ROTATION_VECTOR */
	{
		.snsr_name		= "rotation_vector",
		.typ			= IIO_ROT,
		.mod			= nvs_iio_modifier_vec,
	},
	/* SENSOR_TYPE_RELATIVE_HUMIDITY */
	{
		.snsr_name		= "relative_humidity",
		.typ			= IIO_HUMIDITY,
		.mod			= nvs_iio_modifier_vec,
	},
	/* SENSOR_TYPE_AMBIENT_TEMPERATURE */
	{
		.snsr_name		= "ambient_temperature",
		.typ			= IIO_TEMP,
		.mod			= nvs_iio_modifier_none,
	},
	/* SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED */
	{
		.snsr_name		= "magnetic_field_uncalibrated",
		.typ			= IIO_MAGN_UNCAL,
		.mod			= nvs_iio_modifier_uncal,
	},
	/* SENSOR_TYPE_GAME_ROTATION_VECTOR */
	{
		.snsr_name		= "game_rotation_vector",
		.typ			= IIO_GAME_ROT,
		.mod			= nvs_iio_modifier_vec,
	},
	/* SENSOR_TYPE_GYROSCOPE_UNCALIBRATED */
	{
		.snsr_name		= "gyroscope_uncalibrated",
		.typ			= IIO_ANGLVEL_UNCAL,
		.mod			= nvs_iio_modifier_uncal,
	},
	/* SENSOR_TYPE_SIGNIFICANT_MOTION */
	{
		.snsr_name		= "significant_motion",
		.typ			= IIO_MOTION,
		.mod			= NULL,
	},
	/* SENSOR_TYPE_STEP_DETECTOR */
	{
		.snsr_name		= "step_detector",
		.typ			= IIO_STEP,
		.mod			= NULL,
	},
	/* SENSOR_TYPE_STEP_COUNTER */
	{
		.snsr_name		= "step_counter",
		.typ			= IIO_STEP_COUNT,
		.mod			= nvs_iio_modifier_none,
	},
	/* SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR */
	{
		.snsr_name		= "geomagnetic_rotation_vector",
		.typ			= IIO_GEOMAGN_ROT,
		.mod			= nvs_iio_modifier_vec,
	},
	/* SENSOR_TYPE_HEART_RATE */
	{
		.snsr_name		= "heart_rate",
		.typ			= IIO_HEART_RATE,
		.mod			= nvs_iio_modifier_heart,
	},
	/* SENSOR_TYPE_TILT_DETECTOR */
	{
		.snsr_name		= "tilt_detector",
		.typ			= IIO_INCLI,
		.mod			= NULL,
	},
	/* SENSOR_TYPE_WAKE_GESTURE */
	{
		.snsr_name		= "wake_gesture",
		.typ			= IIO_GESTURE_WAKE,
		.mod			= NULL,
	},
	/* SENSOR_TYPE_GLANCE_GESTURE */
	{
		.snsr_name		= "glance_gesture",
		.typ			= IIO_GESTURE_GLANCE,
		.mod			= NULL,
	},
	/* SENSOR_TYPE_PICK_UP_GESTURE */
	{
		.snsr_name		= "pick_up_gesture",
		.typ			= IIO_GESTURE_PICKUP,
		.mod			= NULL,
	},
};


static void nvs_mutex_lock(void *handle)
{
	struct iio_dev *indio_dev = (struct iio_dev *)handle;

	if (indio_dev)
		mutex_lock(&indio_dev->mlock);
}

static void nvs_mutex_unlock(void *handle)
{
	struct iio_dev *indio_dev = (struct iio_dev *)handle;

	if (indio_dev)
		mutex_unlock(&indio_dev->mlock);
}

static ssize_t nvs_dbg_cfg(struct iio_dev *indio_dev, char *buf)
{
	struct nvs_state *st = iio_priv(indio_dev);
	unsigned int i;
	ssize_t t;

	t = sprintf(buf, "name=%s\n", st->cfg->name);
	t += sprintf(buf + t, "snsr_id=%d\n", st->cfg->snsr_id);
	t += sprintf(buf + t, "timestamp_sz=%d\n", st->cfg->timestamp_sz);
	t += sprintf(buf + t, "snsr_data_n=%d\n", st->cfg->snsr_data_n);
	t += sprintf(buf + t, "kbuf_sz=%d\n", st->cfg->kbuf_sz);
	t += sprintf(buf + t, "ch_n=%u\n", st->cfg->ch_n);
	t += sprintf(buf + t, "ch_sz=%d\n", st->cfg->ch_sz);
	t += sprintf(buf + t, "ch_inf=%p\n", st->cfg->ch_inf);
	t += sprintf(buf + t, "delay_us_min=%u\n", st->cfg->delay_us_min);
	t += sprintf(buf + t, "delay_us_max=%u\n", st->cfg->delay_us_max);
	t += sprintf(buf + t, "matrix: ");
	for (i = 0; i < 9; i++)
		t += sprintf(buf + t, "%hhd ", st->cfg->matrix[i]);
	t += sprintf(buf + t, "\nuncal_lo=%d\n", st->cfg->uncal_lo);
	t += sprintf(buf + t, "uncal_hi=%d\n", st->cfg->uncal_hi);
	t += sprintf(buf + t, "cal_lo=%d\n", st->cfg->cal_lo);
	t += sprintf(buf + t, "cal_hi=%d\n", st->cfg->cal_hi);
	t += sprintf(buf + t, "thresh_lo=%d\n", st->cfg->thresh_lo);
	t += sprintf(buf + t, "thresh_hi=%d\n", st->cfg->thresh_hi);
	t += sprintf(buf + t, "report_n=%d\n", st->cfg->report_n);
	t += sprintf(buf + t, "float_significance=%s\n",
		     nvs_float_significances[st->cfg->float_significance]);
	return t;
}

/* dummy enable function to support client's NULL function pointer */
static int nvs_fn_dev_enable(void *client, int snsr_id, int enable)
{
	return 0;
}

static unsigned int nvs_buf_index(unsigned int size, unsigned int *bytes)
{
	unsigned int index;

	if (!(*bytes % size))
		index = *bytes;
	else
		index = *bytes - *bytes % size + size;
	*bytes = index + size;
	return index;
}

static ssize_t nvs_dbg_data(struct iio_dev *indio_dev, char *buf)
{
	struct nvs_state *st = iio_priv(indio_dev);
	struct iio_chan_spec const *ch;
	ssize_t t;
	unsigned int shift;
	unsigned int buf_i;
	unsigned int ch_n;
	unsigned int n;
	unsigned int i;
	u64 data;

	t = sprintf(buf, "%s: ", st->cfg->name);
	n = 0;
	for (i = 0; i < indio_dev->num_channels - 1; i++) {
		ch = &indio_dev->channels[i];
		if (iio_scan_mask_query(indio_dev, indio_dev->buffer, i)) {
			ch_n = ch->scan_type.storagebits / 8;
			buf_i = nvs_buf_index(ch_n, &n);
		} else {
			t += sprintf(buf + t, "disabled ");
			continue;
		}

		if (ch_n <= sizeof(data)) {
			data = 0LL;
			memcpy(&data, &st->buf[buf_i], ch_n);
			if (ch->scan_type.sign == 's') {
				shift = 64 - ch->scan_type.realbits;
				t += sprintf(buf + t, "%lld ",
					     (s64)(data << shift) >> shift);
			} else {
				t += sprintf(buf + t, "%llu ", data);
			}
		} else {
			t += sprintf(buf + t, "ERR ");
		}
	}
	t += sprintf(buf + t, "ts=%lld  ts_diff=%lld\n", st->ts, st->ts_diff);
	return t;
}

static int nvs_buf_i(struct iio_dev *indio_dev, unsigned int ch)
{
	unsigned int buf_i = 0;
	unsigned int n;
	unsigned int i;

	if (!iio_scan_mask_query(indio_dev, indio_dev->buffer, ch))
		return -EINVAL;

	n = 0;
	for (i = 0; i <= ch; i++) {
		if (iio_scan_mask_query(indio_dev, indio_dev->buffer, i))
			buf_i = nvs_buf_index(indio_dev->channels[i].scan_type.
					      storagebits / 8, &n);
	}
	return buf_i;
}

static int nvs_buf_push(struct iio_dev *indio_dev, unsigned char *data, s64 ts)
{
	struct nvs_state *st = iio_priv(indio_dev);
	char char_buf[128];
	unsigned int n;
	unsigned int i;
	unsigned int src_i = 0;
	unsigned int dst_i;
	unsigned int bytes = 0;
	unsigned int data_chan_n;
	int ret = 0;

	data_chan_n = indio_dev->num_channels - 1;
	/* It's possible to have events without data (data_chan_n == 0).
	 * In this case, just the timestamp is sent.
	 */
	if (data_chan_n) {
		for (i = 0; i < data_chan_n; i++) {
			if (iio_scan_mask_query(indio_dev,
						indio_dev->buffer, i)) {
				n = indio_dev->channels[i].
						     scan_type.storagebits / 8;
				dst_i = nvs_buf_index(n, &bytes);
				if (data && !(st->dbg_data_lock & (1 << i)))
					memcpy(&st->buf[dst_i],
					       &data[src_i], n);
				src_i += n;
			}
		}
	}
	if (ts) {
		st->ts_diff = ts - st->ts;
		st->ts = ts;
		if (st->ts_diff < 0)
			dev_err(st->dev, "%s %s ts_diff=%lld\n",
				__func__, st->cfg->name, st->ts_diff);
	} else {
		st->flush = false;
		if (*st->fn_dev->sts & (NVS_STS_SPEW_MSG | NVS_STS_SPEW_DATA))
			dev_info(st->dev, "%s %s FLUSH\n",
				 __func__, st->cfg->name);
	}
	if (indio_dev->buffer->scan_timestamp) {
		n = sizeof(ts);
		dst_i = nvs_buf_index(n, &bytes);
		memcpy(&st->buf[dst_i], &ts, n);
	}
	if (iio_buffer_enabled(indio_dev)) {
		ret = iio_push_to_buffers(indio_dev, st->buf);
		i = st->cfg->flags & SENSOR_FLAG_READONLY_MASK;
		if (i == SENSOR_FLAG_ONE_SHOT_MODE && ts && !ret) {
			/* one-shot sensor sent sensor data so disable */
			ret = st->fn_dev->enable(st->client,
						 st->cfg->snsr_id, 0);
			if (!ret)
				st->enabled = 0;
		}
		if (*st->fn_dev->sts & NVS_STS_SPEW_BUF) {
			for (i = 0; i < bytes; i++)
				dev_info(st->dev, "%s buf[%u]=%x\n",
					 st->cfg->name, i, st->buf[i]);
			dev_info(st->dev, "%s ts=%lld  diff=%lld\n",
				 st->cfg->name, ts, st->ts_diff);
		}
	}
	if ((*st->fn_dev->sts & NVS_STS_SPEW_DATA) && ts) {
		nvs_dbg_data(indio_dev, char_buf);
		dev_info(st->dev, "%s", char_buf);
	}
	if (!ret)
		/* return pushed byte count from data if no error.
		 * external entity can use as offset to next data set.
		 */
		ret = src_i;
	return ret;
}

static int nvs_handler(void *handle, void *buffer, s64 ts)
{
	struct iio_dev *indio_dev = (struct iio_dev *)handle;
	unsigned char *buf = buffer;
	int ret = 0;

	if (indio_dev)
		ret = nvs_buf_push(indio_dev, buf, ts);
	return ret;
}

static int nvs_enable(struct iio_dev *indio_dev, bool en)
{
	struct nvs_state *st = iio_priv(indio_dev);
	unsigned int i;
	int enable = 0;
	int ret;

	if (en) {
		if (indio_dev->num_channels > 1) {
			for (i = 0; i < indio_dev->num_channels - 1; i++) {
				if (iio_scan_mask_query(indio_dev,
							indio_dev->buffer, i))
					enable |= 1 << i;
			}
		} else {
			enable = 1;
		}
	} else {
		st->dbg_data_lock = 0;
	}

	ret = st->fn_dev->enable(st->client, st->cfg->snsr_id, enable);
	if (!ret)
		st->enabled = enable;
	if (*st->fn_dev->sts & NVS_STS_SPEW_MSG)
		dev_info(st->dev, "%s %s enable=%x ret=%d",
			 __func__, st->cfg->name, enable, ret);
	return ret;
}

static ssize_t nvs_attr_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct nvs_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	s8 matrix[9];
	char *str;
	const char *msg;
	unsigned int new;
	unsigned int old = 0;
	int ret;

	if (this_attr->address != NVS_ATTR_MATRIX) {
		ret = kstrtouint(buf, 0, &new);
		if (ret)
			return -EINVAL;
	}

	mutex_lock(&indio_dev->mlock);
	if (st->shutdown || st->suspend || (*st->fn_dev->sts &
				       (NVS_STS_SHUTDOWN | NVS_STS_SUSPEND))) {
		mutex_unlock(&indio_dev->mlock);
		return -EPERM;
	}

	switch (this_attr->address) {
	case NVS_ATTR_ENABLE:
		msg = "ATTR_ENABLE";
		old = st->fn_dev->enable(st->client, st->cfg->snsr_id, -1);
		ret = nvs_enable(indio_dev, (bool)new);
		if (ret > 0)
			ret = 0;
		break;

	case NVS_ATTR_FLAGS:
		msg = "ATTR_FLAGS";
		old = st->cfg->flags;
		st->cfg->flags &= SENSOR_FLAG_READONLY_MASK;
		new &= ~SENSOR_FLAG_READONLY_MASK;
		st->cfg->flags |= new;
		new = st->cfg->flags;
		break;

	case NVS_ATTR_MATRIX:
		msg = "ATTR_MATRIX";
		for (new = 0; new < sizeof(matrix); new++) {
			str = strsep((char **)&buf, " \0");
			if (str != NULL) {
				ret = kstrtos8(str, 10, &matrix[new]);
				if (ret)
					break;

				if (matrix[new] < -1 || matrix[new] > 1) {
					ret = -EINVAL;
					break;
				}
			} else {
				ret = -EINVAL;
				break;
			}
		}
		if (new == sizeof(matrix)) {
			memcpy(st->cfg->matrix, matrix,
			       sizeof(st->cfg->matrix));
		}
		break;

	default:
		msg = "ATTR_UNKNOWN";
		ret = -EINVAL;
	}

	mutex_unlock(&indio_dev->mlock);
	if (*st->fn_dev->sts & NVS_STS_SPEW_MSG) {
		if (ret)
			dev_err(st->dev, "%s %s %s %d->%d ERR=%d\n",
				__func__, st->cfg->name, msg, old, new, ret);
		else
			dev_info(st->dev, "%s %s %s %d->%d\n",
				 __func__, st->cfg->name, msg, old, new);
	}
	if (ret)
		return ret;

	return count;
}

static ssize_t nvs_attr_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct nvs_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int ret = -EINVAL;

	switch (this_attr->address) {
	case NVS_ATTR_ENABLE:
		if (st->fn_dev->enable != nvs_fn_dev_enable) {
			mutex_lock(&indio_dev->mlock);
			ret = sprintf(buf, "%x\n",
				      st->fn_dev->enable(st->client,
							st->cfg->snsr_id, -1));
			mutex_unlock(&indio_dev->mlock);
		}
		return ret;

	case NVS_ATTR_PART:
		return sprintf(buf, "%s %s\n", st->cfg->part, st->cfg->name);

	case NVS_ATTR_VENDOR:
		return sprintf(buf, "%s\n", st->cfg->vendor);

	case NVS_ATTR_VERSION:
		return sprintf(buf, "%d\n", st->cfg->version);

	case NVS_ATTR_MILLIAMP:
		return sprintf(buf, "%d.%06u\n",
			       st->cfg->milliamp.ival,
			       st->cfg->milliamp.fval);

	case NVS_ATTR_FIFO_RSRV_EVNT_CNT:
		return sprintf(buf, "%u\n", st->cfg->fifo_rsrv_evnt_cnt);

	case NVS_ATTR_FIFO_MAX_EVNT_CNT:
		return sprintf(buf, "%u\n", st->cfg->fifo_max_evnt_cnt);

	case NVS_ATTR_FLAGS:
		return sprintf(buf, "%u\n", st->cfg->flags);

	case NVS_ATTR_MATRIX:
		return sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			       st->cfg->matrix[0],
			       st->cfg->matrix[1],
			       st->cfg->matrix[2],
			       st->cfg->matrix[3],
			       st->cfg->matrix[4],
			       st->cfg->matrix[5],
			       st->cfg->matrix[6],
			       st->cfg->matrix[7],
			       st->cfg->matrix[8]);

	case NVS_ATTR_SELF_TEST:
		if (st->fn_dev->self_test) {
			mutex_lock(&indio_dev->mlock);
			ret = st->fn_dev->self_test(st->client,
						    st->cfg->snsr_id, buf);
			mutex_unlock(&indio_dev->mlock);
			return ret;
		}
		break;

	default:
		return -EINVAL;
	}

	return -EINVAL;
}

static const char NVS_DBG_KEY[] = {
	0x54, 0x65, 0x61, 0x67, 0x61, 0x6E, 0x26, 0x52, 0x69, 0x69, 0x73, 0x00
};

static ssize_t nvs_info_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct nvs_state *st = iio_priv(indio_dev);
	unsigned int dbg;
	int ret;

	ret = kstrtouint(buf, 0, &dbg);
	if (ret)
		return -EINVAL;

	st->dbg = dbg;
	switch (dbg) {
	case NVS_INFO_DATA:
		*st->fn_dev->sts &= ~NVS_STS_SPEW_MSK;
		break;

	case NVS_INFO_DBG:
		*st->fn_dev->sts ^= NVS_STS_SPEW_MSG;
		break;

	case NVS_INFO_DBG_DATA:
		*st->fn_dev->sts ^= NVS_STS_SPEW_DATA;
		break;

	case NVS_INFO_DBG_BUF:
		*st->fn_dev->sts ^= NVS_STS_SPEW_BUF;
		break;

	case NVS_INFO_DBG_IRQ:
		*st->fn_dev->sts ^= NVS_STS_SPEW_IRQ;
		break;

	case NVS_INFO_DBG_KEY:
		break;

	default:
		if (dbg < NVS_INFO_LIMIT_MAX)
			break;

		if (st->fn_dev->nvs_write) {
			st->dbg = NVS_INFO_LIMIT_MAX;
			dbg -= NVS_INFO_LIMIT_MAX;
			ret = st->fn_dev->nvs_write(st->client,
						    st->cfg->snsr_id, dbg);
			if (ret < 0)
				return ret;
		} else if (!st->fn_dev->nvs_read) {
			st->dbg = NVS_INFO_DATA;
			return -EINVAL;
		}

		break;
	}

	return count;
}

static ssize_t nvs_info_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct nvs_state *st = iio_priv(indio_dev);
	enum NVS_DBG dbg;
	unsigned int i;
	int ret = -EINVAL;

	dbg = st->dbg;
	st->dbg = NVS_INFO_DATA;
	switch (dbg) {
	case NVS_INFO_DATA:
		return nvs_dbg_data(indio_dev, buf);

	case NVS_INFO_VER:
		return sprintf(buf, "version=%u\n", NVS_IIO_DRIVER_VERSION);

	case NVS_INFO_ERRS:
		i = *st->fn_dev->errs;
		*st->fn_dev->errs = 0;
		return sprintf(buf, "error count=%u\n", i);

	case NVS_INFO_RESET:
		if (st->fn_dev->reset) {
			mutex_lock(&indio_dev->mlock);
			ret = st->fn_dev->reset(st->client, st->cfg->snsr_id);
			mutex_unlock(&indio_dev->mlock);
		}
		if (ret)
			return sprintf(buf, "reset ERR\n");
		else
			return sprintf(buf, "reset done\n");

	case NVS_INFO_REGS:
		if (st->fn_dev->regs)
			return st->fn_dev->regs(st->client,
						st->cfg->snsr_id, buf);
		break;

	case NVS_INFO_CFG:
		return nvs_dbg_cfg(indio_dev, buf);

	case NVS_INFO_DBG_KEY:
		return sprintf(buf, "%s\n", NVS_DBG_KEY);

	case NVS_INFO_DBG:
		return sprintf(buf, "DBG spew=%x\n",
			       !!(*st->fn_dev->sts & NVS_STS_SPEW_MSG));

	case NVS_INFO_DBG_DATA:
		return sprintf(buf, "DATA spew=%x\n",
			       !!(*st->fn_dev->sts & NVS_STS_SPEW_DATA));

	case NVS_INFO_DBG_BUF:
		return sprintf(buf, "BUF spew=%x\n",
			       !!(*st->fn_dev->sts & NVS_STS_SPEW_BUF));

	case NVS_INFO_DBG_IRQ:
		return sprintf(buf, "IRQ spew=%x\n",
			       !!(*st->fn_dev->sts & NVS_STS_SPEW_IRQ));

	default:
		if (dbg < NVS_INFO_LIMIT_MAX)
			break;

		if (st->fn_dev->nvs_read)
			ret = st->fn_dev->nvs_read(st->client,
						   st->cfg->snsr_id, buf);
		break;
	}

	return ret;
}

static int nvs_attr_rm(struct nvs_state *st, struct attribute *rm_attr)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(st->attrs) - 1; i++) {
		if (st->attrs[i] == rm_attr) {
			for (; i < ARRAY_SIZE(st->attrs) - 1; i++)
				st->attrs[i] = st->attrs[i + 1];
			return 0;
		}
	}

	return -EINVAL;
}

static int nvs_attr(struct iio_dev *indio_dev)
{
	struct nvs_state *st = iio_priv(indio_dev);
	unsigned int i;

	memcpy(st->attrs, nvs_attrs, sizeof(st->attrs));
	/* test if matrix data */
	for (i = 0; i < ARRAY_SIZE(st->cfg->matrix); i++) {
		if (st->cfg->matrix[i])
			break;
	}
	if (i >= ARRAY_SIZE(st->cfg->matrix))
		/* remove matrix attribute */
		nvs_attr_rm(st, &iio_dev_attr_matrix.dev_attr.attr);

	if (st->fn_dev->self_test == NULL)
		nvs_attr_rm(st, &iio_dev_attr_self_test.dev_attr.attr);

	st->attr_group.attrs = st->attrs;
	return 0;
}

static int nvs_read_raw(struct iio_dev *indio_dev,
			struct iio_chan_spec const *chan,
			int *val, int *val2, long mask)
{
	struct nvs_state *st = iio_priv(indio_dev);
	unsigned int n;
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = nvs_buf_i(indio_dev, chan->scan_index);
		if (ret < 0)
			return ret;

		*val = 0;
		n = chan->scan_type.storagebits / 8;
		if (n > sizeof(*val))
			n = sizeof(*val);
		memcpy(val, &st->buf[ret], n);
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_BATCH_FLUSH:
		*val = (int)st->flush;
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_BATCH_PERIOD:
		if (st->fn_dev->enable == nvs_fn_dev_enable) {
			ret = st->enabled;
		} else {
			ret = st->fn_dev->enable(st->client,
						 st->cfg->snsr_id, -1);
			if (ret < 0)
				return ret;
		}

		if (ret)
			*val = st->batch_period_us;
		else
			*val = st->cfg->delay_us_min;
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_BATCH_TIMEOUT:
		if (st->fn_dev->enable == nvs_fn_dev_enable) {
			ret = st->enabled;
		} else {
			ret = st->fn_dev->enable(st->client,
						 st->cfg->snsr_id, -1);
			if (ret < 0)
				return ret;
		}

		if (ret)
			*val = st->batch_timeout_us;
		else
			*val = st->cfg->delay_us_max;
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_BATCH_FLAGS:
		*val = st->batch_flags;
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_SCALE:
		if (st->cfg->ch_n_max) {
			if (chan->scan_index >= st->cfg->ch_n_max)
				return -EINVAL;

			*val = st->cfg->scales[chan->scan_index].ival;
			*val2 = st->cfg->scales[chan->scan_index].fval;
		} else {
			*val = st->cfg->scale.ival;
			*val2 = st->cfg->scale.fval;
		}
		if (st->cfg->float_significance)
			return IIO_VAL_INT_PLUS_NANO;

		return IIO_VAL_INT_PLUS_MICRO;

	case IIO_CHAN_INFO_OFFSET:
		if (st->cfg->ch_n_max) {
			if (chan->scan_index >= st->cfg->ch_n_max)
				return -EINVAL;

			*val = st->cfg->offsets[chan->scan_index].ival;
			*val2 = st->cfg->offsets[chan->scan_index].fval;
		} else {
			*val = st->cfg->offset.ival;
			*val2 = st->cfg->offset.fval;
		}
		if (st->cfg->float_significance)
			return IIO_VAL_INT_PLUS_NANO;

		return IIO_VAL_INT_PLUS_MICRO;

	case IIO_CHAN_INFO_THRESHOLD_LOW:
		*val = st->cfg->thresh_lo;
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_THRESHOLD_HIGH:
		*val = st->cfg->thresh_hi;
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_PEAK:
		*val = st->cfg->max_range.ival;
		*val2 = st->cfg->max_range.fval;
		if (st->cfg->float_significance)
			return IIO_VAL_INT_PLUS_NANO;

		return IIO_VAL_INT_PLUS_MICRO;

	case IIO_CHAN_INFO_PEAK_SCALE:
		*val = st->cfg->resolution.ival;
		*val2 = st->cfg->resolution.fval;
		if (st->cfg->float_significance)
			return IIO_VAL_INT_PLUS_NANO;

		return IIO_VAL_INT_PLUS_MICRO;

	default:
		ret = -EINVAL;
	}

	return ret;
}

static int nvs_write_raw(struct iio_dev *indio_dev,
			 struct iio_chan_spec const *chan,
			 int val, int val2, long mask)
{
	struct nvs_state *st = iio_priv(indio_dev);
	char *msg;
	int ch = chan->scan_index;
	int old = 0;
	int old2 = 0;
	int ret = 0;

	mutex_lock(&indio_dev->mlock);
	if (st->shutdown || st->suspend || (*st->fn_dev->sts &
				       (NVS_STS_SHUTDOWN | NVS_STS_SUSPEND))) {
		mutex_unlock(&indio_dev->mlock);
		return -EPERM;
	}

	switch (mask) {
	case IIO_CHAN_INFO_BATCH_FLUSH:
		msg = "IIO_CHAN_INFO_BATCH_FLUSH";
		old = (int)st->flush;
		st->flush = true;
		if (st->fn_dev->flush) {
			ret = st->fn_dev->flush(st->client, st->cfg->snsr_id);
			if (ret) {
				nvs_buf_push(indio_dev, st->buf, 0);
				ret = 0;
			}
		} else {
			nvs_buf_push(indio_dev, st->buf, 0);
		}
		break;

	case IIO_CHAN_INFO_BATCH_PERIOD:
		msg = "IIO_CHAN_INFO_BATCH_PERIOD";
		old = st->batch_period_us;
		if (val < st->cfg->delay_us_min)
			val = st->cfg->delay_us_min;
		if (st->cfg->delay_us_max && val > st->cfg->delay_us_max)
			val = st->cfg->delay_us_max;
		if (st->fn_dev->batch) {
			ret = st->fn_dev->batch(st->client, st->cfg->snsr_id,
						st->batch_flags,
						val,
						st->batch_timeout_us);
			if (!ret)
				st->batch_period_us = val;
		} else {
			if (st->batch_timeout_us)
				ret = -EINVAL;
		}
		break;

	case IIO_CHAN_INFO_BATCH_TIMEOUT:
		msg = "IIO_CHAN_INFO_BATCH_TIMEOUT";
		old = st->batch_timeout_us;
		st->batch_timeout_us = val;
		break;

	case IIO_CHAN_INFO_BATCH_FLAGS:
		msg = "IIO_CHAN_INFO_BATCH_FLAGS";
		old = st->batch_flags;
		st->batch_flags = val;
		break;

	case IIO_CHAN_INFO_SCALE:
		msg = "IIO_CHAN_INFO_SCALE";
		if (st->cfg->ch_n_max) {
			if (ch >= st->cfg->ch_n_max) {
				ret = -EINVAL;
				break;
			}

			old = st->cfg->scales[ch].ival;
			old2 = st->cfg->scales[ch].fval;
			if (st->fn_dev->scale) {
				ret = st->fn_dev->scale(st->client,
						    st->cfg->snsr_id, ch, val);
				if (ret > 0) {
					st->cfg->scales[ch].ival = val;
					st->cfg->scales[ch].fval = val2;
					ret = 0;
				}
			} else {
				st->cfg->scales[ch].ival = val;
				st->cfg->scales[ch].fval = val2;
			}
		} else {
			old = st->cfg->scale.ival;
			old2 = st->cfg->scale.fval;
			if (st->fn_dev->scale) {
				ret = st->fn_dev->scale(st->client,
							st->cfg->snsr_id,
							-1, val);
				if (ret > 0) {
					st->cfg->scale.ival = val;
					st->cfg->scale.fval = val2;
					ret = 0;
				}
			} else {
				st->cfg->scale.ival = val;
				st->cfg->scale.fval = val2;
			}
		}
		break;

	case IIO_CHAN_INFO_OFFSET:
		msg = "IIO_CHAN_INFO_OFFSET";
		if (st->cfg->ch_n_max) {
			if (ch >= st->cfg->ch_n_max) {
				ret = -EINVAL;
				break;
			}

			old = st->cfg->offsets[ch].ival;
			old2 = st->cfg->offsets[ch].fval;
			if (st->fn_dev->offset) {
				ret = st->fn_dev->offset(st->client,
						    st->cfg->snsr_id, ch, val);
				if (ret > 0) {
					st->cfg->offsets[ch].ival = val;
					st->cfg->offsets[ch].fval = val2;
					ret = 0;
				}
			} else {
				st->cfg->offsets[ch].ival = val;
				st->cfg->offsets[ch].fval = val2;
			}
		} else {
			old = st->cfg->offset.ival;
			old2 = st->cfg->offset.fval;
			if (st->fn_dev->offset) {
				ret = st->fn_dev->offset(st->client,
							 st->cfg->snsr_id,
							 -1 , val);
				if (ret > 0) {
					st->cfg->offset.ival = val;
					st->cfg->offset.fval = val2;
					ret = 0;
				}
			} else {
				st->cfg->offset.ival = val;
				st->cfg->offset.fval = val2;
			}
		}
		break;

	case IIO_CHAN_INFO_THRESHOLD_LOW:
		msg = "IIO_CHAN_INFO_THRESHOLD_LOW";
		old = st->cfg->thresh_lo;
		if (st->fn_dev->thresh_lo) {
			ret = st->fn_dev->thresh_lo(st->client,
						    st->cfg->snsr_id, val);
			if (ret > 0) {
				st->cfg->thresh_lo = val;
				ret = 0;
			}
		} else {
			st->cfg->thresh_lo = val;
		}
		break;

	case IIO_CHAN_INFO_THRESHOLD_HIGH:
		msg = "IIO_CHAN_INFO_THRESHOLD_HIGH";
		old = st->cfg->thresh_hi;
		if (st->fn_dev->thresh_hi) {
			ret = st->fn_dev->thresh_hi(st->client,
						    st->cfg->snsr_id, val);
			if (ret > 0) {
				st->cfg->thresh_hi = val;
				ret = 0;
			}
		} else {
			st->cfg->thresh_hi = val;
		}
		break;

	case IIO_CHAN_INFO_PEAK:
		msg = "IIO_CHAN_INFO_PEAK";
		old = st->cfg->max_range.ival;
		old2 = st->cfg->max_range.fval;
		if (st->fn_dev->max_range) {
			ret = st->fn_dev->max_range(st->client,
						    st->cfg->snsr_id, val);
			if (ret > 0) {
				st->cfg->max_range.ival = val;
				st->cfg->max_range.fval = val2;
				ret = 0;
			}
		} else {
			st->cfg->max_range.ival = val;
			st->cfg->max_range.fval = val2;
		}
		break;

	case IIO_CHAN_INFO_PEAK_SCALE:
		msg = "IIO_CHAN_INFO_PEAK_SCALE";
		old = st->cfg->resolution.ival;
		old2 = st->cfg->resolution.fval;
		if (st->fn_dev->resolution) {
			ret = st->fn_dev->resolution(st->client,
						     st->cfg->snsr_id, val);
			if (ret > 0) {
				st->cfg->resolution.ival = val;
				st->cfg->resolution.fval = val2;
				ret = 0;
			}
		} else {
			st->cfg->resolution.ival = val;
			st->cfg->resolution.fval = val2;
		}
		break;

	case IIO_CHAN_INFO_RAW:
		/* writing to raw is a debug feature allowing a sticky data
		 * value to be pushed up and the same value pushed until the
		 * device is turned off.
		 */
		msg = "IIO_CHAN_INFO_RAW";
		ret = nvs_buf_i(indio_dev, ch);
		if (ret >= 0) {
			memcpy(&st->buf[ret], &val,
			       chan->scan_type.storagebits / 8);
			st->dbg_data_lock |= (1 << ch);
			st->ts++;
			ret = nvs_buf_push(indio_dev, st->buf, st->ts);
			if (ret > 0)
				ret = 0;
		}
		break;

	default:
		msg = "IIO_CHAN_INFO_UNKNOWN";
		ret = -EINVAL;
	}

	mutex_unlock(&indio_dev->mlock);
	if (*st->fn_dev->sts & NVS_STS_SPEW_MSG) {
		if (ret)
			dev_err(st->dev, "%s %s %s c=%d %d:%d->%d:%d ERR=%d\n",
				__func__, st->cfg->name, msg,
				ch, old, old2, val, val2, ret);
		else
			dev_info(st->dev, "%s %s %s chan=%d %d:%d->%d:%d\n",
				 __func__, st->cfg->name, msg,
				 ch, old, old2, val, val2);
	}
	return ret;
}

static int nvs_buffer_preenable(struct iio_dev *indio_dev)
{
	struct nvs_state *st = iio_priv(indio_dev);

	if (st->shutdown || st->suspend || (*st->fn_dev->sts &
					 (NVS_STS_SHUTDOWN | NVS_STS_SUSPEND)))
		return -EINVAL;

	return 0;
}

static int nvs_buffer_postenable(struct iio_dev *indio_dev)
{
	int ret;

	ret = nvs_enable(indio_dev, true);
	/* never return > 0 to IIO buffer engine */
	if (ret > 0)
		ret = 0;
	return ret;
}

static const struct iio_buffer_setup_ops nvs_buffer_setup_ops = {
	/* iio_sw_buffer_preenable:
	 * Generic function for equal sized ring elements + 64 bit timestamp
	 * Assumes that any combination of channels can be enabled.
	 * Typically replaced to implement restrictions on what combinations
	 * can be captured (hardware scan modes).
	 */
	.preenable = &nvs_buffer_preenable,
	/* iio_triggered_buffer_postenable:
	 * Generic function that simply attaches the pollfunc to the trigger.
	 * Replace this to mess with hardware state before we attach the
	 * trigger.
	 */
	.postenable = &nvs_buffer_postenable,
	/* this driver relies on the NVS HAL to power off this device with the
	 * master enable.
	 *.predisable = N/A
	 *.postdisable = N/A
	 */
};

static const long nvs_info_mask_dflt =	BIT(IIO_CHAN_INFO_RAW) |
					BIT(IIO_CHAN_INFO_BATCH_FLAGS) |
					BIT(IIO_CHAN_INFO_BATCH_PERIOD) |
					BIT(IIO_CHAN_INFO_BATCH_TIMEOUT) |
					BIT(IIO_CHAN_INFO_BATCH_FLUSH) |
					BIT(IIO_CHAN_INFO_PEAK) |
					BIT(IIO_CHAN_INFO_PEAK_SCALE) |
					BIT(IIO_CHAN_INFO_SCALE) |
					BIT(IIO_CHAN_INFO_OFFSET);

static const long nvs_info_mask_shared_by_type_dflt =
					BIT(IIO_CHAN_INFO_BATCH_FLAGS) |
					BIT(IIO_CHAN_INFO_BATCH_PERIOD) |
					BIT(IIO_CHAN_INFO_BATCH_TIMEOUT) |
					BIT(IIO_CHAN_INFO_BATCH_FLUSH) |
					BIT(IIO_CHAN_INFO_PEAK) |
					BIT(IIO_CHAN_INFO_PEAK_SCALE);

static const struct iio_chan_spec nvs_ch_ts[] = {
	IIO_CHAN_SOFT_TIMESTAMP(0)
};

static void nvs_chan_scan_type(struct iio_chan_spec *ch, int ch_sz)
{
	if (ch_sz < 0) {
		/* signed data */
		ch->scan_type.sign = 's';
		ch->scan_type.storagebits = -8 * ch_sz;
	} else {
		/* unsigned data */
		ch->scan_type.sign = 'u';
		ch->scan_type.storagebits = 8 * ch_sz;
	}
	ch->scan_type.realbits = ch->scan_type.storagebits;
}

static int nvs_chan(struct iio_dev *indio_dev)
{
	struct nvs_state *st = iio_priv(indio_dev);
	long info_mask_msk = 0;
	long info_mask = nvs_info_mask_dflt;
	long info_mask_shared_by_type = nvs_info_mask_shared_by_type_dflt;
	int ch_status_sz = 0;
	unsigned int buf_sz = 0;
	unsigned int ch_type_i;
	unsigned int i;

	if (st->cfg->ch_inf) {
		/* if channels already defined */
		if (!st->cfg->ch_n)
			return -ENODEV;

		indio_dev->channels = (struct iio_chan_spec *)st->cfg->ch_inf;
		indio_dev->num_channels = st->cfg->ch_n;
		for (i = 0; i < st->cfg->ch_n; i++)
			nvs_buf_index(indio_dev->channels[i].
				      scan_type.storagebits / 8, &buf_sz);
		st->buf = devm_kzalloc(st->dev, buf_sz, GFP_KERNEL);
		if (st->buf == NULL)
			return -ENOMEM;

		return 0;
	}

	/* create IIO channels */
	ch_type_i = st->cfg->snsr_id; /* st->cfg->snsr_id will be >= 0 */
	/* Here we have two ways to identify the sensor:
	 * 1. By name which we try to match to
	 * 2. By sensor ID (st->cfg->snsr_id).  This method is typically used
	 *    by the sensor hub so that name strings don't have to be passed
	 *    and because there are multiple sensors the sensor hub driver has
	 *    to track via the sensor ID.
	 */
	if (st->cfg->name) {
		/* if st->cfg->name exists then we use that */
		for (i = 0; i < ARRAY_SIZE(nvs_iio_ch_tbl); i++) {
			if (!strcmp(st->cfg->name,
				    nvs_iio_ch_tbl[i].snsr_name))
				break;
		}
		if (i < ARRAY_SIZE(nvs_iio_ch_tbl))
			ch_type_i = i; /* found matching name */
		else if (ch_type_i >= ARRAY_SIZE(nvs_iio_ch_tbl))
			ch_type_i = 0; /* use generic sensor parameters */
	} else {
		/* no st->cfg->name - use st->cfg->snsr_id to specify device */
		if (ch_type_i >= ARRAY_SIZE(nvs_iio_ch_tbl))
			return -ENODEV;

		st->cfg->name = nvs_iio_ch_tbl[ch_type_i].snsr_name;
	}

	i = st->cfg->ch_n;
	i++; /* timestamp */
	if (st->cfg->snsr_data_n && st->cfg->ch_n) {
		/* if more data than channel data then status channel */
		ch_status_sz = abs(st->cfg->ch_sz) * st->cfg->ch_n;
		ch_status_sz = st->cfg->snsr_data_n - ch_status_sz;
		if (ch_status_sz <= 0)
			ch_status_sz = 0;
		else
			i++;
	}
	st->ch = devm_kzalloc(st->dev,  i * sizeof(struct iio_chan_spec),
			      GFP_KERNEL);
	if (st->ch == NULL)
		return -ENOMEM;

	if (SENSOR_FLAG_ONE_SHOT_MODE == (st->cfg->flags &
					  SENSOR_FLAG_SPECIAL_REPORTING_MODE))
		info_mask_msk = BIT(IIO_CHAN_INFO_BATCH_FLAGS) |
				BIT(IIO_CHAN_INFO_BATCH_PERIOD) |
				BIT(IIO_CHAN_INFO_BATCH_TIMEOUT) |
				BIT(IIO_CHAN_INFO_BATCH_FLUSH);

	info_mask &= ~info_mask_msk;
	info_mask_shared_by_type &= ~info_mask_msk;
	info_mask_msk = 0;
	if (st->cfg->thresh_lo || st->cfg->thresh_hi)
		info_mask_msk = BIT(IIO_CHAN_INFO_THRESHOLD_LOW) |
				BIT(IIO_CHAN_INFO_THRESHOLD_HIGH);
	info_mask |= info_mask_msk;
	info_mask_shared_by_type |= info_mask_msk;
	if (!st->cfg->ch_n_max)
		info_mask_shared_by_type |= BIT(IIO_CHAN_INFO_SCALE) |
					    BIT(IIO_CHAN_INFO_OFFSET);
	i = 0;
	if (st->cfg->ch_n) { /* It's possible to not have any data channels */
		for (; i < st->cfg->ch_n; i++) {
			st->ch[i].type = nvs_iio_ch_tbl[ch_type_i].typ;
			nvs_chan_scan_type(&st->ch[i], st->cfg->ch_sz);
			st->ch[i].info_mask_shared_by_all = info_mask;
			if (st->cfg->ch_n - 1) {
				/* multiple channels */
				st->ch[i].channel2 = i + 1;
				st->ch[i].info_mask_shared_by_type =
						      info_mask_shared_by_type;
				st->ch[i].info_mask_separate =
					  ~st->ch[i].info_mask_shared_by_type &
							   st->ch[i].info_mask_shared_by_all;
				st->ch[i].modified = 1;
			} else {
				/* single channel */
				st->ch[i].info_mask_separate =
							   st->ch[i].info_mask_shared_by_all;
			}
			st->ch[i].scan_index = i;
			nvs_buf_index(st->ch[i].scan_type.storagebits / 8,
				      &buf_sz);
		}
	}
	if (ch_status_sz) {
		/* create a status channel */
		st->ch[i].type = nvs_iio_ch_tbl[ch_type_i].typ;
		st->ch[i].channel2 = IIO_MOD_STATUS;
		nvs_chan_scan_type(&st->ch[i], ch_status_sz);
		st->ch[i].info_mask_shared_by_all = BIT(IIO_CHAN_INFO_RAW);
		st->ch[i].info_mask_separate = BIT(IIO_CHAN_INFO_RAW);
		st->ch[i].modified = 1;
		st->ch[i].scan_index = i;
		nvs_buf_index(st->ch[i].scan_type.storagebits / 8, &buf_sz);
		i++;
	}
	/* timestamp channel */
	memcpy(&st->ch[i], &nvs_ch_ts, sizeof(nvs_ch_ts));
	st->ch[i].scan_index = i;
	nvs_buf_index(st->ch[i].scan_type.storagebits / 8, &buf_sz);
	i++;
	st->buf = devm_kzalloc(st->dev, (size_t)buf_sz, GFP_KERNEL);
	if (st->buf == NULL)
		return -ENOMEM;

	indio_dev->channels = st->ch;
	indio_dev->num_channels = i;
	return 0;
}

static const struct iio_trigger_ops nvs_trigger_ops = {
	.owner = THIS_MODULE,
};

static int nvs_suspend(void *handle)
{
	struct iio_dev *indio_dev = (struct iio_dev *)handle;
	struct nvs_state *st;
	int ret = 0;

	if (indio_dev == NULL)
		return 0;

	st = iio_priv(indio_dev);
	mutex_lock(&indio_dev->mlock);
	st->suspend = true;
	if (!(st->cfg->flags & SENSOR_FLAG_WAKE_UP)) {
		ret = st->fn_dev->enable(st->client, st->cfg->snsr_id, -1);
		if (ret >= 0)
			st->enabled = ret;
		if (ret > 0)
			ret = st->fn_dev->enable(st->client,
						 st->cfg->snsr_id, 0);
		else
			ret = 0;
	}
	mutex_unlock(&indio_dev->mlock);
	return ret;
}

static int nvs_resume(void *handle)
{
	struct iio_dev *indio_dev = (struct iio_dev *)handle;
	struct nvs_state *st;
	int ret = 0;

	if (indio_dev == NULL)
		return 0;

	st = iio_priv(indio_dev);
	mutex_lock(&indio_dev->mlock);
	if (!(st->cfg->flags & SENSOR_FLAG_WAKE_UP)) {
		if (st->enabled)
			ret = st->fn_dev->enable(st->client,
						st->cfg->snsr_id, st->enabled);
	}
	st->suspend = false;
	mutex_unlock(&indio_dev->mlock);
	return ret;
}

static void nvs_shutdown(void *handle)
{
	struct iio_dev *indio_dev = (struct iio_dev *)handle;
	struct nvs_state *st;
	int ret;

	if (indio_dev == NULL)
		return;

	st = iio_priv(indio_dev);
	mutex_lock(&indio_dev->mlock);
	st->shutdown = true;
	ret = st->fn_dev->enable(st->client, st->cfg->snsr_id, -1);
	if (ret > 0)
		st->fn_dev->enable(st->client, st->cfg->snsr_id, 0);
	mutex_unlock(&indio_dev->mlock);
}

static int nvs_remove(void *handle)
{
	struct iio_dev *indio_dev = (struct iio_dev *)handle;
	struct nvs_state *st;

	if (indio_dev == NULL)
		return 0;

	st = iio_priv(indio_dev);
	if (indio_dev->dev.devt)
		iio_device_unregister(indio_dev);
	if (st->trig != NULL) {
		iio_trigger_unregister(st->trig);
		iio_trigger_free(st->trig);
	}
	if (indio_dev->buffer != NULL) {
		iio_buffer_unregister(indio_dev);
		iio_kfifo_free(indio_dev->buffer);
	}
	if (st->ch)
		devm_kfree(st->dev, st->ch);
	if (st->buf)
		devm_kfree(st->dev, st->buf);
	if (st->cfg->name)
		dev_info(st->dev, "%s %s snsr_id=%d\n",
			 __func__, st->cfg->name, st->cfg->snsr_id);
	else
		dev_info(st->dev, "%s snsr_id=%d\n",
			 __func__, st->cfg->snsr_id);
	iio_device_free(indio_dev);
	return 0;
}

static int nvs_init(struct iio_dev *indio_dev, struct nvs_state *st)
{
	int ret;

	ret = nvs_attr(indio_dev);
	if (ret) {
		dev_err(st->dev, "%s nvs_attr ERR=%d\n", __func__, ret);
		return ret;
	}

	ret = nvs_chan(indio_dev);
	if (ret) {
		dev_err(st->dev, "%s nvs_chan ERR=%d\n", __func__, ret);
		return ret;
	}

	indio_dev->buffer = iio_kfifo_allocate(indio_dev);
	if (!indio_dev->buffer) {
		dev_err(st->dev, "%s iio_kfifo_allocate ERR\n", __func__);
		return -ENOMEM;
	}

	indio_dev->buffer->scan_timestamp = true;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->currentmode = INDIO_DIRECT_MODE;
	indio_dev->dev.parent = st->dev;
	indio_dev->name = st->cfg->name;
	st->info.driver_module = THIS_MODULE;
	st->info.attrs = &st->attr_group;
	st->info.read_raw = &nvs_read_raw;
	st->info.write_raw = &nvs_write_raw;
	indio_dev->info = &st->info;
	indio_dev->setup_ops = &nvs_buffer_setup_ops;
	ret = iio_buffer_register(indio_dev, indio_dev->channels,
				  indio_dev->num_channels);
	if (ret) {
		dev_err(st->dev, "%s iio_buffer_register ERR\n", __func__);
		return ret;
	}

	if (st->cfg->kbuf_sz) {
		indio_dev->buffer->access->set_length(indio_dev->buffer,
						      st->cfg->kbuf_sz);
		indio_dev->buffer->access->request_update(indio_dev->buffer);
	}
	st->trig = iio_trigger_alloc("%s-dev%d",
				     indio_dev->name, indio_dev->id);
	if (st->trig == NULL) {
		dev_err(st->dev, "%s iio_allocate_trigger ERR\n", __func__);
		return -ENOMEM;
	}

	st->trig->dev.parent = st->dev;
	st->trig->ops = &nvs_trigger_ops;
	ret = iio_trigger_register(st->trig);
	if (ret) {
		dev_err(st->dev, "%s iio_trigger_register ERR\n", __func__);
		return -ENOMEM;
	}

	indio_dev->trig = st->trig;
	indio_dev->modes |= INDIO_BUFFER_TRIGGERED;
	indio_dev->multi_link = true;
	ret = iio_device_register(indio_dev);
	if (ret) {
		dev_err(st->dev, "%s iio_device_register ERR\n", __func__);
		return ret;
	}

	return 0;
}

static int nvs_probe(void **handle, void *dev_client, struct device *dev,
		     struct nvs_fn_dev *fn_dev, struct sensor_cfg *snsr_cfg)
{
	struct iio_dev *indio_dev;
	struct nvs_state *st;
	int ret;

	dev_info(dev, "%s\n", __func__);
	if (snsr_cfg->snsr_id < 0) {
		/* device has been disabled */
		if (snsr_cfg->name)
			dev_info(dev, "%s %s disabled\n",
				 __func__, snsr_cfg->name);
		else
			dev_info(dev, "%s device disabled\n", __func__);
		return -ENODEV;
	}

	indio_dev = iio_device_alloc(sizeof(*st));
	if (indio_dev == NULL) {
		dev_err(dev, "%s iio_device_alloc ERR\n", __func__);
		return -ENOMEM;
	}

	st = iio_priv(indio_dev);
	st->client = dev_client;
	st->dev = dev;
	st->fn_dev = fn_dev;
	/* protect ourselves from NULL pointers */
	if (st->fn_dev->enable == NULL)
		/* hook a dummy benign function */
		st->fn_dev->enable = nvs_fn_dev_enable;
	if (st->fn_dev->sts == NULL)
		st->fn_dev->sts = &st->fn_dev_sts;
	if (st->fn_dev->errs == NULL)
		st->fn_dev->errs = &st->fn_dev_errs;
	/* all other pointers are tested for NULL in this code */
	st->cfg = snsr_cfg;
	ret = nvs_init(indio_dev, st);
	if (ret) {
		if (st->cfg->name)
			dev_err(st->dev, "%s %s snsr_id=%d EXIT ERR=%d\n",
				__func__, st->cfg->name,
				st->cfg->snsr_id, ret);
		else
			dev_err(st->dev, "%s snsr_id=%d EXIT ERR=%d\n",
				__func__, st->cfg->snsr_id, ret);
		nvs_remove(indio_dev);
	} else {
		*handle = indio_dev;
	}
	dev_info(st->dev, "%s %s done\n", __func__, st->cfg->name);
	return ret;
}

static struct nvs_fn_if nvs_fn_if_iio = {
	.probe				= nvs_probe,
	.remove				= nvs_remove,
	.shutdown			= nvs_shutdown,
	.nvs_mutex_lock			= nvs_mutex_lock,
	.nvs_mutex_unlock		= nvs_mutex_unlock,
	.suspend			= nvs_suspend,
	.resume				= nvs_resume,
	.handler			= nvs_handler,
};

struct nvs_fn_if *nvs_iio(void)
{
	return &nvs_fn_if_iio;
}

