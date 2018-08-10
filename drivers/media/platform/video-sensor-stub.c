/*
 * Copyright (c) 2013--2018 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


#include <linux/device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/wait.h>

#include <media/ipu-isys.h>
#include <media/media-device.h>
#include <media/media-entity.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-core.h>

#include "video-sensor-stub.h"


/*
 * Order matters.
 *
 * 1. Bits-per-pixel, descending.
 * 2. Bits-per-pixel compressed, descending.
 * 3. Pixel order, same as in pixel_order_str. Formats for all four pixel
 *    orders must be defined.
 */
static const struct sensor_csi_data_format sensor_csi_data_formats[] = {
	{ MEDIA_BUS_FMT_UYVY8_1X16, 16, 16, PIXEL_ORDER_GBRG, },
	{ MEDIA_BUS_FMT_SGRBG12_1X12, 12, 12, PIXEL_ORDER_GRBG, },
	{ MEDIA_BUS_FMT_SRGGB12_1X12, 12, 12, PIXEL_ORDER_RGGB, },
	{ MEDIA_BUS_FMT_SBGGR12_1X12, 12, 12, PIXEL_ORDER_BGGR, },
	{ MEDIA_BUS_FMT_SGBRG12_1X12, 12, 12, PIXEL_ORDER_GBRG, },
	{ MEDIA_BUS_FMT_SGRBG10_1X10, 10, 10, PIXEL_ORDER_GRBG, },
	{ MEDIA_BUS_FMT_SRGGB10_1X10, 10, 10, PIXEL_ORDER_RGGB, },
	{ MEDIA_BUS_FMT_SBGGR10_1X10, 10, 10, PIXEL_ORDER_BGGR, },
	{ MEDIA_BUS_FMT_SGBRG10_1X10, 10, 10, PIXEL_ORDER_GBRG, },
	{ MEDIA_BUS_FMT_SGRBG10_DPCM8_1X8, 10, 8, PIXEL_ORDER_GRBG, },
	{ MEDIA_BUS_FMT_SRGGB10_DPCM8_1X8, 10, 8, PIXEL_ORDER_RGGB, },
	{ MEDIA_BUS_FMT_SBGGR10_DPCM8_1X8, 10, 8, PIXEL_ORDER_BGGR, },
	{ MEDIA_BUS_FMT_SGBRG10_DPCM8_1X8, 10, 8, PIXEL_ORDER_GBRG, },
	{ MEDIA_BUS_FMT_SGRBG8_1X8, 8, 8, PIXEL_ORDER_GRBG, },
	{ MEDIA_BUS_FMT_SRGGB8_1X8, 8, 8, PIXEL_ORDER_RGGB, },
	{ MEDIA_BUS_FMT_SBGGR8_1X8, 8, 8, PIXEL_ORDER_BGGR, },
	{ MEDIA_BUS_FMT_SGBRG8_1X8, 8, 8, PIXEL_ORDER_GBRG, },
};

static const char * const pixel_order_str[] = { "GRBG", "RGGB", "BGGR", "GBRG" };

#define to_csi_format_idx(fmt) (((unsigned long)(fmt)			\
				 - (unsigned long)sensor_csi_data_formats) \
				/ sizeof(*sensor_csi_data_formats))

static const uint32_t sensor_supported_codes_pad[] = {
	MEDIA_BUS_FMT_UYVY8_1X16,
	MEDIA_BUS_FMT_SBGGR12_1X12,
	MEDIA_BUS_FMT_SGBRG12_1X12,
	MEDIA_BUS_FMT_SGRBG12_1X12,
	MEDIA_BUS_FMT_SRGGB12_1X12,
	MEDIA_BUS_FMT_SBGGR10_1X10,
	MEDIA_BUS_FMT_SGBRG10_1X10,
	MEDIA_BUS_FMT_SGRBG10_1X10,
	MEDIA_BUS_FMT_SRGGB10_1X10,
	MEDIA_BUS_FMT_SBGGR8_1X8,
	MEDIA_BUS_FMT_SGBRG8_1X8,
	MEDIA_BUS_FMT_SGRBG8_1X8,
	MEDIA_BUS_FMT_SRGGB8_1X8,
	0,
};


static const uint32_t * const sensor_supported_codes[] = {
	sensor_supported_codes_pad,
};

static u32 sensor_pixel_order(struct stub_sensor *sensor)
{
	return sensor->default_pixel_order;
}


static int sensor_get_frame_desc(struct v4l2_subdev *sd, unsigned int pad,
				struct v4l2_mbus_frame_desc *desc)
{
	struct stub_sensor *sensor = to_stub_sensor(sd);
	struct v4l2_mbus_frame_desc_entry *entry = desc->entry;

	desc->type = V4L2_MBUS_FRAME_DESC_TYPE_CSI2;
	entry->bpp = sensor->csi_format->compressed;
	entry->pixelcode = sensor->csi_format->code;
	entry->size.two_dim.width = sensor->width;
	entry->size.two_dim.height = sensor->height;
	entry->bus.csi2.data_type = sensor->mipi_data_type;
	desc->num_entries = 1;

	return 0;
}


static u32 __stub_get_mbus_code(struct v4l2_subdev *subdev, unsigned int pad)
{
	struct stub_sensor *sensor = to_stub_sensor(subdev);

	return sensor->csi_format->code;
}

static int __stub_get_format(struct v4l2_subdev *subdev,
			     struct v4l2_subdev_pad_config *cfg,
			struct v4l2_subdev_format *fmt)
{
	struct stub_sensor *sensor = to_stub_sensor(subdev);

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		fmt->format = *v4l2_subdev_get_try_format(subdev,
							  cfg, fmt->pad);
	} else {
		fmt->format.code = __stub_get_mbus_code(subdev, fmt->pad);
		fmt->format.width = sensor->width;
		fmt->format.height = sensor->height;
		fmt->format.field = V4L2_FIELD_NONE;
	}

	return 0;
}


static int sensor_stub_get_format(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_format *fmt)
{
	struct stub_sensor *sensor = to_stub_sensor(subdev);
	int rval;

	mutex_lock(&sensor->mutex);
	rval = __stub_get_format(subdev, cfg, fmt);
	mutex_unlock(&sensor->mutex);
	return rval;
}
static int stub_get_mbus_formats(struct stub_sensor *sensor)
{
	const struct sensor_csi_data_format *f = &sensor_csi_data_formats[12];

	sensor->default_pixel_order = PIXEL_ORDER_GRBG;
	sensor->mbus_frame_fmts = ~0;
	sensor->csi_format = f;
	return 0;
}


static int stub_open(struct v4l2_subdev *subdev, struct v4l2_subdev_fh *fh)
{
	struct stub_sensor *sensor = to_stub_sensor(subdev);
	u32 mbus_code =
		sensor_csi_data_formats[sensor_pixel_order(sensor)].code;
	struct v4l2_mbus_framefmt *try_fmt =
		v4l2_subdev_get_try_format(subdev, fh->pad, 0);

	try_fmt->width = 4096;
	try_fmt->height = 3072;
	try_fmt->code = mbus_code;

	return 0;
}

static const struct sensor_csi_data_format
*stub_validate_csi_data_format(struct stub_sensor *sensor, u32 code)
{
	const struct sensor_csi_data_format *csi_format = sensor->csi_format;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(sensor_csi_data_formats); i++) {
		if (sensor->mbus_frame_fmts & (1 << i)
		    && sensor_csi_data_formats[i].code == code)
			return &sensor_csi_data_formats[i];
	}

	return csi_format;
}


static int sensor_stub_set_format(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_format *fmt)
{
	struct stub_sensor *sensor = to_stub_sensor(subdev);
	u32 code = fmt->format.code;
	int rval;

	sensor->height = fmt->format.height;
	sensor->width = fmt->format.width;
	rval = __stub_get_format(subdev, cfg, fmt);
	mutex_lock(&sensor->mutex);
	if (!rval && subdev == &sensor->sd) {
		const struct sensor_csi_data_format *csi_format =
				stub_validate_csi_data_format(sensor, code);
		if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE) {
			sensor->csi_format = csi_format;
		}
		fmt->format.code = csi_format->code;
	}

	mutex_unlock(&sensor->mutex);
	return rval;
}

static int stub_set_stream(struct v4l2_subdev *subdev, int enable)
{

	struct stub_sensor *sensor = to_stub_sensor(subdev);
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->sd);

	if (enable) {
		dev_dbg(&client->dev, "Streaming is ON\n");
		sensor->streaming = 1;
	} else {
		dev_dbg(&client->dev, "Streaming is OFF\n");
		sensor->streaming = 0;
	}
	return 0;
}

static struct v4l2_subdev_internal_ops sensor_sd_internal_ops = {
	.open = stub_open,
};

static const struct v4l2_subdev_video_ops sensor_sd_video_ops = {
	.s_stream = stub_set_stream,
};

static int stub_sensor_s_ctrl(struct v4l2_ctrl *ctrl)
{
	return 0;
}

static const struct v4l2_ctrl_ops ipu_isys_sensor_ctrl_ops = {
	.s_ctrl = stub_sensor_s_ctrl,
};

static const struct v4l2_subdev_pad_ops sensor_sd_pad_ops = {
	.get_fmt = sensor_stub_get_format,
	.set_fmt = sensor_stub_set_format,
	.get_frame_desc = sensor_get_frame_desc,
};

static struct v4l2_subdev_ops sensor_ops = {
	.video = &sensor_sd_video_ops,
	.pad = &sensor_sd_pad_ops,
};

static const struct i2c_device_id sensor_stub_id_table[] = {
	{ SENSOR_STUB_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, sensor_stub_id_table);

static int sensor_stub_probe(struct i2c_client *client,
			const struct i2c_device_id *devid)
{
	struct stub_sensor *sensor;
	int rval;

	sensor = devm_kzalloc(&client->dev, sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		dev_err(&client->dev, "Failed to alloc Sensor structure\n");
		return -ENOMEM;
	}
	mutex_init(&sensor->mutex);
	v4l2_i2c_subdev_init(&sensor->sd, client, &sensor_ops);
	sensor->sd.internal_ops = &sensor_sd_internal_ops;
	sensor->csi_format = &sensor_csi_data_formats[12];
	sensor->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	sensor->pad.flags = MEDIA_PAD_FL_SOURCE;
	sensor->streaming = false;
	snprintf(sensor->sd.name, sizeof(sensor->sd.name),
		"pixter %d-%4.4x", i2c_adapter_id(client->adapter),
		client->addr);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
	rval = media_entity_init(&sensor->sd.entity, 1, &sensor->pad, 0);
#else
	rval = media_entity_pads_init(&sensor->sd.entity, 1, &sensor->pad);
#endif
	if (rval)
		return rval;

	return stub_get_mbus_formats(sensor);
}

static int sensor_stub_remove(struct i2c_client *client)
{
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct stub_sensor *sensor = to_stub_sensor(subdev);

	v4l2_device_unregister_subdev(&sensor->sd);
	media_entity_cleanup(&sensor->sd.entity);

	return 0;

}

static struct i2c_driver sensor_stub_i2c_driver = {
	.driver = {
		.name = SENSOR_STUB_NAME,
	},
	.probe	= sensor_stub_probe,
	.remove	= sensor_stub_remove,
	.id_table = sensor_stub_id_table,
};

module_i2c_driver(sensor_stub_i2c_driver);


MODULE_AUTHOR("Jouni Ukkonen <jouni.ukkonen@intel.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Intel Dummy Sensor driver");
