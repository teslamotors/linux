/*
 * ti940.c -- driver for ti940
 *
 * Copyright (c) 2017 Tesla Motors, Inc.
 *
 * Based on ti964.c.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Original copyright messages:
 *
 * Copyright (c) 2016 Intel Corporation.
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
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/pm.h>

#include <linux/regulator/consumer.h>

#include <media/media-device.h>
#include <media/media-entity.h>
#include <media/ti940.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-core.h>

#include "ti940-reg.h"

struct ti940 {
	struct v4l2_subdev sd;
	struct media_pad pad[NR_OF_VA_PADS];
	struct v4l2_ctrl_handler ctrl_handler;
	struct ti940_pdata *pdata;
	struct v4l2_subdev *sub_devs[NR_OF_VA_SINK_PADS];
	const char *name;

	struct mutex mutex;

	struct regmap *regmap8;

	struct v4l2_mbus_framefmt *ffmts[NR_OF_VA_PADS];
	struct rect *crop;
	struct rect *compose;

	struct {
		unsigned int *stream_id;
	} *stream; /* stream enable/disable status, indexed by pad */
	struct {
		unsigned int sink;
		unsigned int source;
		int flags;
	} *route; /* pad level info, indexed by stream */

	/* TODO: Remove this, so the ti940 driver doesn't need to be aware of
	 *       the tesla-specific regulator.
	 */
	struct regulator *vcc;
	int regulator_enabled;

	unsigned int nsinks;
	unsigned int nsources;
	unsigned int nstreams;
	unsigned int npads;

	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *test_pattern;
};

#define to_ti940(_sd) container_of(_sd, struct ti940, sd)

/*
 * Order matters.
 *
 * 1. Bits-per-pixel, descending.
 * 2. Bits-per-pixel compressed, descending.
 * 3. Pixel order, same as in pixel_order_str. Formats for all four pixel
 *    orders must be defined.
 */
static const struct ti940_csi_data_format va_csi_data_formats[] = {
	{ MEDIA_BUS_FMT_RGB888_1X24, 24, 24, PIXEL_ORDER_GRBG, 0x1e },
	{ MEDIA_BUS_FMT_YUYV8_1X16, 16, 16, PIXEL_ORDER_GBRG, 0x1e },
	{ MEDIA_BUS_FMT_UYVY8_1X16, 16, 16, PIXEL_ORDER_GBRG, 0x1e },
	{ MEDIA_BUS_FMT_SGRBG12_1X12, 12, 12, PIXEL_ORDER_GRBG, 0x2c },
	{ MEDIA_BUS_FMT_SRGGB12_1X12, 12, 12, PIXEL_ORDER_RGGB, 0x2c },
	{ MEDIA_BUS_FMT_SBGGR12_1X12, 12, 12, PIXEL_ORDER_BGGR, 0x2c },
	{ MEDIA_BUS_FMT_SGBRG12_1X12, 12, 12, PIXEL_ORDER_GBRG, 0x2c },
	{ MEDIA_BUS_FMT_SGRBG10_1X10, 10, 10, PIXEL_ORDER_GRBG, 0x2b },
	{ MEDIA_BUS_FMT_SRGGB10_1X10, 10, 10, PIXEL_ORDER_RGGB, 0x2b },
	{ MEDIA_BUS_FMT_SBGGR10_1X10, 10, 10, PIXEL_ORDER_BGGR, 0x2b },
	{ MEDIA_BUS_FMT_SGBRG10_1X10, 10, 10, PIXEL_ORDER_GBRG, 0x2b },
	{ MEDIA_BUS_FMT_SGRBG8_1X8, 8, 8, PIXEL_ORDER_GRBG, 0x2a },
	{ MEDIA_BUS_FMT_SRGGB8_1X8, 8, 8, PIXEL_ORDER_RGGB, 0x2a },
	{ MEDIA_BUS_FMT_SBGGR8_1X8, 8, 8, PIXEL_ORDER_BGGR, 0x2a },
	{ MEDIA_BUS_FMT_SGBRG8_1X8, 8, 8, PIXEL_ORDER_GBRG, 0x2a },
};

static const uint32_t ti940_supported_codes_pad[] = {
	MEDIA_BUS_FMT_RGB888_1X24,
	MEDIA_BUS_FMT_YUYV8_1X16,
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

static const uint32_t *ti940_supported_codes[] = {
	ti940_supported_codes_pad,
};

static struct regmap_config ti940_reg_config8 = {
	.reg_bits = 8,
	.val_bits = 8,
};

static inline int ti940_regulator_enable(struct ti940 *va)
{
	int ret = regulator_enable(va->vcc);
	if (ret >= 0)
		va->regulator_enabled = 1;
	return ret;
}

static inline int ti940_regulator_disable(struct ti940 *va)
{
	int ret = regulator_disable(va->vcc);
	if (ret >= 0)
		va->regulator_enabled = 0;
	return ret;
}

static int ti940_enum_mbus_code(struct v4l2_subdev *sd,
				      struct v4l2_subdev_pad_config *cfg,
				      struct v4l2_subdev_mbus_code_enum *code)
{
	struct ti940 *va = to_ti940(sd);
	const uint32_t *supported_code =
		ti940_supported_codes[code->pad];
	bool next_stream = false;
	int i;

	if (code->stream & V4L2_SUBDEV_FLAG_NEXT_STREAM) {
		next_stream = true;
		code->stream &= ~V4L2_SUBDEV_FLAG_NEXT_STREAM;
	}

	if (code->stream > va->nstreams)
		return -EINVAL;

	if (next_stream) {
		if (!(va->pad[code->pad].flags & MEDIA_PAD_FL_MULTIPLEX))
			return -EINVAL;
		if (code->stream < va->nstreams - 1) {
			code->stream++;
			return 0;
		} else {
			return -EINVAL;
		}
	}

	for (i = 0; supported_code[i]; i++) {
		if (i == code->index) {
			code->code = supported_code[i];
			return 0;
		}
	}

	return -EINVAL;
}

static const struct ti940_csi_data_format
		*ti940_validate_csi_data_format(u32 code)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(va_csi_data_formats); i++) {
		if (va_csi_data_formats[i].code == code)
			return &va_csi_data_formats[i];
	}

	return &va_csi_data_formats[0];
}

static int ti940_get_frame_desc(struct v4l2_subdev *sd,
	unsigned int pad, struct v4l2_mbus_frame_desc *desc)
{
	struct ti940 *va = to_ti940(sd);
	struct v4l2_mbus_frame_desc_entry *entry = desc->entry;
	u8 vc = 0;
	int i;

	desc->type = V4L2_MBUS_FRAME_DESC_TYPE_CSI2;

	for (i = 0; i < min(va->nstreams, (unsigned)desc->num_entries); i++) {
		struct v4l2_mbus_framefmt *ffmt =
			&va->ffmts[i][VA_PAD_SOURCE];
		const struct ti940_csi_data_format *csi_format =
			ti940_validate_csi_data_format(ffmt->code);

		entry->size.two_dim.width = ffmt->width;
		entry->size.two_dim.height = ffmt->height;
		entry->pixelcode = ffmt->code;
		entry->bus.csi2.channel = vc++;
		entry->bpp = csi_format->compressed;
		entry++;
	}

	return 0;
}

static struct v4l2_mbus_framefmt *
__ti940_get_ffmt(struct v4l2_subdev *subdev,
			 struct v4l2_subdev_pad_config *cfg,
			 unsigned int pad, unsigned int which,
			 unsigned int stream)
{
	struct ti940 *va = to_ti940(subdev);

	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_format(subdev, cfg, pad);
	else
		return &va->ffmts[pad][stream];
}

static int ti940_get_format(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_format *fmt)
{
	struct ti940 *va = to_ti940(subdev);

	if (fmt->stream > va->nstreams)
		return -EINVAL;

	mutex_lock(&va->mutex);
	fmt->format = *__ti940_get_ffmt(subdev, cfg, fmt->pad,
						    fmt->which, fmt->stream);
	mutex_unlock(&va->mutex);

	dev_dbg(subdev->dev, "subdev_format: which: %s, pad: %d, stream: %d.\n",
		 fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE ?
		 "V4L2_SUBDEV_FORMAT_ACTIVE" : "V4L2_SUBDEV_FORMAT_TRY",
		 fmt->pad, fmt->stream);

	dev_dbg(subdev->dev, "framefmt: width: %d, height: %d, code: 0x%x.\n",
	       fmt->format.width, fmt->format.height, fmt->format.code);

	return 0;
}

static int ti940_set_format(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_format *fmt)
{
	struct ti940 *va = to_ti940(subdev);
	const struct ti940_csi_data_format *csi_format;
	struct v4l2_mbus_framefmt *ffmt;

	if (fmt->stream > va->nstreams)
		return -EINVAL;

	csi_format = ti940_validate_csi_data_format(
		fmt->format.code);

	mutex_lock(&va->mutex);
	ffmt = __ti940_get_ffmt(subdev, cfg, fmt->pad, fmt->which,
				      fmt->stream);

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE) {
		ffmt->width = fmt->format.width;
		ffmt->height = fmt->format.height;
		ffmt->code = csi_format->code;
	}
	fmt->format = *ffmt;
	mutex_unlock(&va->mutex);

	dev_dbg(subdev->dev, "framefmt: width: %d, height: %d, code: 0x%x.\n",
	       ffmt->width, ffmt->height, ffmt->code);

	return 0;
}

static int ti940_open(struct v4l2_subdev *subdev,
				struct v4l2_subdev_fh *fh)
{
	struct v4l2_mbus_framefmt *try_fmt =
		v4l2_subdev_get_try_format(subdev, fh->pad, 0);

	struct v4l2_subdev_format fmt = {
		.which = V4L2_SUBDEV_FORMAT_TRY,
		.pad = VA_PAD_SOURCE,
		.format = {
			.width = TI940_MAX_WIDTH,
			.height = TI940_MAX_HEIGHT,
			.code = MEDIA_BUS_FMT_YUYV8_1X16,
		},
		.stream = 0,
	};

	*try_fmt = fmt.format;

	return 0;
}

static int ti940_registered(struct v4l2_subdev *subdev)
{
	struct ti940 *va = to_ti940(subdev);
	int i, j, k, l, rval;

	for (i = 0, k = 0; i < va->pdata->subdev_num; i++) {
		struct ti940_subdev_i2c_info *info =
			&va->pdata->subdev_info[i];
		struct i2c_adapter *adapter;

		if (k >= va->nsinks)
			break;

		adapter = i2c_get_adapter(info->i2c_adapter_id);
		va->sub_devs[k] = v4l2_i2c_new_subdev_board(
			va->sd.v4l2_dev, adapter,
			&info->board_info, 0);
		i2c_put_adapter(adapter);
		if (!va->sub_devs[k]) {
			dev_err(va->sd.dev,
				"can't create new i2c subdev %d-%04x\n",
				info->i2c_adapter_id,
				info->board_info.addr);
			continue;
		}

		for (j = 0; j < va->sub_devs[k]->entity.num_pads; j++) {
			if (va->sub_devs[k]->entity.pads[j].flags &
				MEDIA_PAD_FL_SOURCE)
				break;
		}

		if (j == va->sub_devs[k]->entity.num_pads) {
			dev_warn(va->sd.dev,
				"no source pad in subdev %d-%04x\n",
				info->i2c_adapter_id,
				info->board_info.addr);
			return -ENOENT;
		}

		for (l = 0; l < va->nsinks; l++) {
			rval = media_create_pad_link(
				&va->sub_devs[k]->entity, j,
				&va->sd.entity, l, 0);
			if (rval) {
				dev_err(va->sd.dev,
					"can't create link to %d-%04x\n",
					info->i2c_adapter_id,
					info->board_info.addr);
				return -EINVAL;
			}
		}
		k++;
	}

	return 0;
}

static int ti940_set_power(struct v4l2_subdev *subdev, int on)
{
    return true;
}

static int ti940_set_stream(struct v4l2_subdev *subdev, int enable)
{
	struct ti940 *va = to_ti940(subdev);
	int i, rval;

	dev_dbg(va->sd.dev, "TI940 set stream. enable=%d\n", enable);

	for (i = 0; i < NR_OF_VA_SINK_PADS; i++) {
		struct media_pad *remote_pad =
			media_entity_remote_pad(&va->pad[i]);
		struct v4l2_subdev *sd;
		u8 bpp;

		if (!remote_pad)
			continue;
		
		/* Stream on sensor. */
		sd = media_entity_to_v4l2_subdev(remote_pad->entity);
		rval = v4l2_subdev_call(sd, video, s_stream, enable);
		if (rval) {
			dev_err(va->sd.dev,
				"Failed to set stream for %s. enable = %d\n",
				sd->name, enable);
			return rval;
		}

		/* Set RX port mode. */
		bpp = ti940_validate_csi_data_format(
			va->ffmts[i][0].code)->width;
	}

	return 0;
}

static struct v4l2_subdev_internal_ops ti940_sd_internal_ops = {
	.open = ti940_open,
	.registered = ti940_registered,
};

static const struct v4l2_subdev_video_ops ti940_sd_video_ops = {
	.s_stream = ti940_set_stream,
};

static const struct v4l2_subdev_core_ops ti940_core_subdev_ops = {
	.s_power = ti940_set_power,
};

static int ti940_s_ctrl(struct v4l2_ctrl *ctrl)
{
	return 0;
}

static const struct v4l2_ctrl_ops ti940_ctrl_ops = {
	.s_ctrl = ti940_s_ctrl,
};

static const s64 ti940_op_sys_clock[] =  {400000000, };
static const struct v4l2_ctrl_config ti940_controls[] = {
	{
		.ops = &ti940_ctrl_ops,
		.id = V4L2_CID_LINK_FREQ,
		.name = "V4L2_CID_LINK_FREQ",
		.type = V4L2_CTRL_TYPE_INTEGER_MENU,
		.max = ARRAY_SIZE(ti940_op_sys_clock) - 1,
		.min =  0,
		.step  = 0,
		.def = 0,
		.qmenu_int = ti940_op_sys_clock,
	},
	{
		.ops = &ti940_ctrl_ops,
		.id = V4L2_CID_TEST_PATTERN,
		.name = "V4L2_CID_TEST_PATTERN",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.max = 1,
		.min =	0,
		.step  = 1,
		.def = 0,
	},
};

static const struct v4l2_subdev_pad_ops ti940_sd_pad_ops = {
	.get_fmt = ti940_get_format,
	.set_fmt = ti940_set_format,
	.get_frame_desc = ti940_get_frame_desc,
	.enum_mbus_code = ti940_enum_mbus_code,
};

static struct v4l2_subdev_ops ti940_sd_ops = {
	.core = &ti940_core_subdev_ops,
	.video = &ti940_sd_video_ops,
	.pad = &ti940_sd_pad_ops,
};

static int ti940_register_subdev(struct ti940 *va)
{
	int i, rval;
	struct i2c_client *client = v4l2_get_subdevdata(&va->sd);

	v4l2_subdev_init(&va->sd, &ti940_sd_ops);
	snprintf(va->sd.name, sizeof(va->sd.name), "TI940 0x%x",
		 client->addr);

	va->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	va->sd.internal_ops = &ti940_sd_internal_ops;

	v4l2_set_subdevdata(&va->sd, client);

	v4l2_ctrl_handler_init(&va->ctrl_handler,
				ARRAY_SIZE(ti940_controls));

	if (va->ctrl_handler.error) {
		dev_err(va->sd.dev,
			"Failed to init ti940 controls. ERR: %d!\n",
			va->ctrl_handler.error);
		return va->ctrl_handler.error;
	}

	va->sd.ctrl_handler = &va->ctrl_handler;

	for (i = 0; i < ARRAY_SIZE(ti940_controls); i++) {
		const struct v4l2_ctrl_config *cfg =
			&ti940_controls[i];
		struct v4l2_ctrl *ctrl;

		ctrl = v4l2_ctrl_new_custom(&va->ctrl_handler, cfg, NULL);
		if (!ctrl) {
			dev_err(va->sd.dev,
				"Failed to create ctrl %s!\n", cfg->name);
			rval = va->ctrl_handler.error;
			goto failed_out;
		}
	}

	va->link_freq = v4l2_ctrl_find(&va->ctrl_handler, V4L2_CID_LINK_FREQ);
	va->test_pattern = v4l2_ctrl_find(&va->ctrl_handler,
					  V4L2_CID_TEST_PATTERN);

	for (i = 0; i < va->nsinks; i++)
		va->pad[i].flags = MEDIA_PAD_FL_SINK;
	va->pad[VA_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	rval = media_entity_pads_init(&va->sd.entity, NR_OF_VA_PADS, va->pad);
	if (rval) {
		dev_err(va->sd.dev,
			"Failed to init media entity for ti940!\n");
		goto failed_out;
	}

	return 0;

failed_out:
	v4l2_ctrl_handler_free(&va->ctrl_handler);
	return rval;
}

static int ti940_init(struct ti940 *va)
{
	int i, rval;
	unsigned int val;

	rval = regmap_read(va->regmap8, TI940_DEVID, &val);
	if (rval) {
		dev_err(va->sd.dev, "Failed to read device ID of TI940!\n");
		return rval;
	}
	dev_info(va->sd.dev, "TI940 device ID: 0x%X\n", val);

	for (i = 0; i < ARRAY_SIZE(ti940_init_settings); i++) {
		rval = regmap_write(va->regmap8,
			ti940_init_settings[i].reg,
			ti940_init_settings[i].val);
		if (rval)
			return rval;
	}

	return 0;
}

static int ti940_probe(struct i2c_client *client,
			const struct i2c_device_id *devid)
{
	struct ti940 *va;
	int i, rval = 0;

	if (client->dev.platform_data == NULL)
		return -ENODEV;

	va = devm_kzalloc(&client->dev, sizeof(*va), GFP_KERNEL);
	if (!va)
		return -ENOMEM;

	va->vcc = devm_regulator_get(&client->dev, "vcc");
	if (IS_ERR(va->vcc)) {
		dev_err(&client->dev, "Could not find 3V3-VID regulator\n");
		return PTR_ERR(va->vcc);
	}

	WARN_ON(ti940_regulator_enable(va));
	dev_info(&client->dev, "Enabled vcc regulator.\n");
	msleep(500);

	va->pdata = client->dev.platform_data;

	va->nsources = NR_OF_VA_SOURCE_PADS;
	va->nsinks = NR_OF_VA_SINK_PADS;
	va->npads = NR_OF_VA_PADS;
	va->nstreams = NR_OF_VA_STREAMS;

	va->crop = devm_kcalloc(&client->dev, va->npads,
				sizeof(struct v4l2_rect), GFP_KERNEL);

	va->compose = devm_kcalloc(&client->dev, va->npads,
				   sizeof(struct v4l2_rect), GFP_KERNEL);

	va->route = devm_kcalloc(&client->dev, va->nstreams,
				       sizeof(*va->route), GFP_KERNEL);

	va->stream = devm_kcalloc(&client->dev, va->npads,
				       sizeof(*va->stream), GFP_KERNEL);

	if (!va->crop || !va->compose || !va->route || !va->stream)
		return -ENOMEM;

	for (i = 0; i < va->npads; i++) {
		va->ffmts[i] = devm_kcalloc(&client->dev, va->nstreams,
					    sizeof(struct v4l2_mbus_framefmt),
					    GFP_KERNEL);
		if (!va->ffmts[i])
			return -ENOMEM;

		va->stream[i].stream_id =
			devm_kcalloc(&client->dev, va->nsinks,
			sizeof(*va->stream[i].stream_id), GFP_KERNEL);
		if (!va->stream[i].stream_id)
			return -ENOMEM;
	}

	for (i = 0; i < va->nstreams; i++) {
		va->route[i].sink = i;
		va->route[i].source = VA_PAD_SOURCE;
		va->route[i].flags = 0;
	}

	for (i = 0; i < va->nsinks; i++) {
		va->stream[i].stream_id[0] = i;
		va->stream[VA_PAD_SOURCE].stream_id[i] = i;
	}

	va->regmap8 = devm_regmap_init_i2c(client,
					   &ti940_reg_config8);
	if (IS_ERR(va->regmap8)) {
		dev_err(&client->dev, "Failed to init regmap8!\n");
		return -EIO;
	}

	mutex_init(&va->mutex);
	v4l2_i2c_subdev_init(&va->sd, client, &ti940_sd_ops);
	rval = ti940_register_subdev(va);
	if (rval) {
		dev_err(&client->dev, "Failed to register va subdevice!\n");
		return rval;
	}

	rval = ti940_init(va);
	if (rval) {
		dev_err(&client->dev, "Failed to init TI940!\n");
		return rval;
	}

	return 0;
}

static int ti940_remove(struct i2c_client *client)
{
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct ti940 *va = to_ti940(subdev);
	int i;

	mutex_destroy(&va->mutex);
	v4l2_ctrl_handler_free(&va->ctrl_handler);
	v4l2_device_unregister_subdev(&va->sd);
	media_entity_cleanup(&va->sd.entity);

	for (i = 0; i < NR_OF_VA_SINK_PADS; i++) {
		if (va->sub_devs[i]) {
			struct i2c_client *sub_client =
				v4l2_get_subdevdata(va->sub_devs[i]);

			i2c_unregister_device(sub_client);
		}
		va->sub_devs[i] = NULL;
	}

	ti940_regulator_disable(va);

	return 0;
}

static int ti940_suspend(struct device *dev)
{
	int ret = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct ti940 *va = to_ti940(subdev);

	if (va->regulator_enabled)
		/* don't change va->regulator_enabled */
		ret = regulator_disable(va->vcc);

	return ret;
}

static int ti940_resume(struct device *dev)
{
	int ret = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct ti940 *va = to_ti940(subdev);

	/* only enable if we were enabled before suspend */
	if (va->regulator_enabled)
		ret = regulator_enable(va->vcc);

	return ret;
}

static const struct i2c_device_id ti940_id_table[] = {
	{ TI940_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, ti940_id_table);

static struct dev_pm_ops ti940_pm_ops = {
	.resume = ti940_resume,
	.suspend = ti940_suspend,
};

static struct i2c_driver ti940_i2c_driver = {
	.driver = {
		.name = TI940_NAME,
		.owner = THIS_MODULE,
		.pm = &ti940_pm_ops,
	},
	.probe	= ti940_probe,
	.remove	= ti940_remove,
	.id_table = ti940_id_table,
};
module_i2c_driver(ti940_i2c_driver);

MODULE_AUTHOR("Tianshu Qiu <tian.shu.qiu@intel.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TI940 CSI2-Aggregator driver");
