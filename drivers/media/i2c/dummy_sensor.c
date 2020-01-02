/*
 * Driver for Samsung TRAV 1/4" 5Mp CMOS Image Sensor SoC
 * with an Embedded Image Signal Processor.
 *
 * Copyright (C) 2012, Linaro, Sangwook Lee <sangwook.lee@linaro.org>
 * Copyright (C) 2012, Insignal Co,. Ltd, Homin Lee <suapapa@insignal.co.kr>
 *
 * Based on s5k6aa and noon010pc30 driver
 * Copyright (C) 2011, Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/clk.h>
#include <linux/crc32.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/of_graph.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <asm/unaligned.h>

#include <media/media-entity.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-fwnode.h>

static int debug;
module_param(debug, int, 0644);

#define TRAV_DRIVER_NAME		"trav-sensor"
#define TRAV_FIRMWARE		"trav.bin"

struct trav_gpio {
	int gpio;
	int level;
};

struct trav_platform_data {
	struct trav_gpio gpio_reset;
	struct trav_gpio gpio_stby;
};

struct trav_frmsize {
	struct v4l2_frmsize_discrete size;
	/* Fixed sensor matrix crop rectangle */
	struct v4l2_rect input_window;
};

struct regval_list {
	u32 addr;
	u16 val;
};

/*
 * TODO: currently only preview is supported and snapshot (capture)
 * is not implemented yet
 */
static const struct trav_frmsize trav_prev_sizes[] = {
	{
		.size = { 640, 480 },
		.input_window = { 0x00, 0x00, 0x27F, 0x1DF },
	}, {
		.size = { 720, 480 },
		.input_window = { 0x00, 0x00, 0x2CF, 0x1DF },
	}, {
		.size = { 1160, 720 },
		.input_window = { 0x00, 0x00, 0x487, 0x2CF},
	}, {
		.size = { 1280, 960 },
		.input_window = { 0x00, 0x00, 0x4FF, 0x3C3},
	}, {
		.size = { 1280, 964 },
		.input_window = { 0x00, 0x00, 0x4FF, 0x3C3},
	}, {
		.size = { 1920, 720 },
		.input_window = { 0x00, 0x00, 0x77F, 0x2CF },
	}, {
		.size = { 1920, 1080 },
		.input_window = { 0x00, 0x00, 0x77F, 0x437 },
	}
};

#define TRAV_NUM_PREV ARRAY_SIZE(trav_prev_sizes)
#define TRAV_DEFAULT_FRAME_SIZE		3

struct trav_pixfmt {
	u32 code;
	u32 colorspace;
};

/* By default value, output from sensor will be YUV422 0-255 */
static const struct trav_pixfmt trav_formats[] = {
	{ MEDIA_BUS_FMT_SBGGR8_1X8, V4L2_COLORSPACE_RAW},
	{ MEDIA_BUS_FMT_SGBRG8_1X8, V4L2_COLORSPACE_RAW},
	{ MEDIA_BUS_FMT_SGRBG8_1X8, V4L2_COLORSPACE_RAW},
	{ MEDIA_BUS_FMT_SRGGB8_1X8, V4L2_COLORSPACE_RAW},
	{ MEDIA_BUS_FMT_SBGGR10_1X10, V4L2_COLORSPACE_RAW},
	{ MEDIA_BUS_FMT_SGBRG10_1X10, V4L2_COLORSPACE_RAW},
	{ MEDIA_BUS_FMT_SGRBG10_1X10, V4L2_COLORSPACE_RAW},
	{ MEDIA_BUS_FMT_SRGGB10_1X10, V4L2_COLORSPACE_RAW},
	{ MEDIA_BUS_FMT_SBGGR12_1X12, V4L2_COLORSPACE_RAW},
	{ MEDIA_BUS_FMT_SGBRG12_1X12, V4L2_COLORSPACE_RAW},
	{ MEDIA_BUS_FMT_SGRBG12_1X12, V4L2_COLORSPACE_RAW},
	{ MEDIA_BUS_FMT_SRGGB12_1X12, V4L2_COLORSPACE_RAW},
	{ MEDIA_BUS_FMT_SBGGR14_1X14, V4L2_COLORSPACE_RAW},
	{ MEDIA_BUS_FMT_SGBRG14_1X14, V4L2_COLORSPACE_RAW},
	{ MEDIA_BUS_FMT_SGRBG14_1X14, V4L2_COLORSPACE_RAW},
	{ MEDIA_BUS_FMT_SRGGB14_1X14, V4L2_COLORSPACE_RAW},
	{ MEDIA_BUS_FMT_SBGGR16_1X16, V4L2_COLORSPACE_RAW},
	{ MEDIA_BUS_FMT_SGBRG16_1X16, V4L2_COLORSPACE_RAW},
	{ MEDIA_BUS_FMT_SGRBG16_1X16, V4L2_COLORSPACE_RAW},
	{ MEDIA_BUS_FMT_SRGGB16_1X16, V4L2_COLORSPACE_RAW},
	{ MEDIA_BUS_FMT_RGB888_1X24, V4L2_COLORSPACE_SRGB},
	{ MEDIA_BUS_FMT_RGB888_1X32_PADHI, V4L2_COLORSPACE_SRGB},
	{ MEDIA_BUS_FMT_UYVY8_2X8, V4L2_COLORSPACE_RAW},
	{ MEDIA_BUS_FMT_VYUY8_2X8, V4L2_COLORSPACE_RAW},
};

#define	TRAV_DEFAULT_FMT	8

static const char * const trav_supply_names[] = {
	/*
	 * Usually 2.8V is used for analog power (vdda)
	 * and digital IO (vddio, vdddcore)
	 */
	"vdda",
	"vddio",
	"vddcore",
	"vddreg", /* The internal trav regulator's supply (1.8V) */
};

#define TRAV_NUM_SUPPLIES ARRAY_SIZE(trav_supply_names)

enum trav_gpio_id {
	STBY,
	RST,
	GPIO_NUM,
};

struct trav {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_ctrl_handler handler;
	struct i2c_client *i2c_client;

	struct trav_platform_data *pdata;
	const struct trav_pixfmt *curr_pixfmt;
	const struct trav_frmsize *curr_frmsize;
	struct mutex lock;
	u8 streaming;
	u8 set_params;

	struct regulator_bulk_data supplies[TRAV_NUM_SUPPLIES];
	struct trav_gpio gpio[GPIO_NUM];
};

static inline struct trav *to_trav(struct v4l2_subdev *sd)
{
	return container_of(sd, struct trav, sd);
}

static int trav_read_fw_ver(struct v4l2_subdev *sd)
{
	return 0;
}

static int trav_load_firmware(struct v4l2_subdev *sd)
{
	return 0;
}

static int trav_init_sensor(struct v4l2_subdev *sd)
{
	int ret;

	ret = trav_load_firmware(sd);
	if (ret)
		v4l2_err(sd, "Failed to write initial settings\n");

	return ret;
}

static int __trav_power_on(struct trav *priv)
{
#ifdef TRAV_CONTROL_CAMERA_SENSOR
	int ret;

	ret = regulator_bulk_enable(TRAV_NUM_SUPPLIES, priv->supplies);
	if (ret)
		return ret;
#endif
	return 0;
}

static int __trav_power_off(struct trav *priv)
{
	priv->streaming = 0;

#ifdef TRAV_CONTROL_CAMERA_SENSOR
	return regulator_bulk_disable(TRAV_NUM_SUPPLIES, priv->supplies);
#else
	return 0;
#endif
}

/* Find nearest matching image pixel size. */
static int trav_try_frame_size(struct v4l2_mbus_framefmt *mf,
				  const struct trav_frmsize **size)
{
	unsigned int min_err = ~0;
	int i = ARRAY_SIZE(trav_prev_sizes);
	const struct trav_frmsize *fsize = &trav_prev_sizes[0],
		*match = NULL;

	while (i--) {
		int err = abs(fsize->size.width - mf->width)
				+ abs(fsize->size.height - mf->height);
		if (err < min_err) {
			min_err = err;
			match = fsize;
		}
		fsize++;
	}
	if (match) {
		mf->width  = match->size.width;
		mf->height = match->size.height;
		if (size)
			*size = match;
		return 0;
	}

	return -EINVAL;
}

static int trav_enum_mbus_code(struct v4l2_subdev *sd,
				   struct v4l2_subdev_pad_config *cfg,
				   struct v4l2_subdev_mbus_code_enum *mbus_code)
{
	if (mbus_code->index >= ARRAY_SIZE(trav_formats))
		return -EINVAL;

	mbus_code->code = trav_formats[mbus_code->index].code;

	return 0;
}

static int trav_enum_frame_size(struct v4l2_subdev *sd,
				   struct v4l2_subdev_pad_config *cfg,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int i = ARRAY_SIZE(trav_formats);

	dev_dbg(&client->dev, "%s:\n", __func__);

	if (fse->index >= ARRAY_SIZE(trav_prev_sizes))
		return -EINVAL;

	while (--i)
		if (fse->code == trav_formats[i].code)
			break;

	fse->code = trav_formats[i].code;

	fse->min_width  = trav_prev_sizes[fse->index].size.width;
	fse->max_width  = fse->min_width;
	fse->max_height = trav_prev_sizes[fse->index].size.height;
	fse->min_height = fse->max_height;

	return 0;
}

static int trav_get_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_subdev_format *fmt)
{
	struct trav *priv = to_trav(sd);
	struct v4l2_mbus_framefmt *mf;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		if (cfg) {
			mf = v4l2_subdev_get_try_format(sd, cfg, 0);
			fmt->format = *mf;
		}
		return 0;
	}

	mf = &fmt->format;

	mutex_lock(&priv->lock);
	mf->width = priv->curr_frmsize->size.width;
	mf->height = priv->curr_frmsize->size.height;
	mf->code = priv->curr_pixfmt->code;
	mf->colorspace = priv->curr_pixfmt->colorspace;
	mf->field = V4L2_FIELD_NONE;
	mutex_unlock(&priv->lock);

	return 0;
}

static const struct trav_pixfmt *trav_try_fmt(struct v4l2_subdev *sd,
					    struct v4l2_mbus_framefmt *mf)
{
	int i = ARRAY_SIZE(trav_formats);

	while (--i)
		if (mf->code == trav_formats[i].code)
			break;
	mf->code = trav_formats[i].code;

	return &trav_formats[i];
}

static int trav_set_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_pad_config *cfg,
			    struct v4l2_subdev_format *fmt)
{
	struct trav *priv = to_trav(sd);
	const struct trav_frmsize *fsize = NULL;
	const struct trav_pixfmt *pf;
	struct v4l2_mbus_framefmt *mf;
	int ret = 0;

	pf = trav_try_fmt(sd, &fmt->format);
	trav_try_frame_size(&fmt->format, &fsize);
	fmt->format.colorspace = V4L2_COLORSPACE_RAW;
	fmt->format.field = V4L2_FIELD_NONE;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		if (cfg) {
			mf = v4l2_subdev_get_try_format(sd, cfg, 0);
			*mf = fmt->format;
		}
		return 0;
	}

	mutex_lock(&priv->lock);
	if (!priv->streaming) {
		priv->curr_frmsize = fsize;
		priv->curr_pixfmt = pf;
		priv->set_params = 1;
	} else {
		ret = -EBUSY;
	}
	mutex_unlock(&priv->lock);

	return ret;
}

static const struct v4l2_subdev_pad_ops trav_pad_ops = {
	.enum_mbus_code		= trav_enum_mbus_code,
	.enum_frame_size	= trav_enum_frame_size,
	.get_fmt		= trav_get_fmt,
	.set_fmt		= trav_set_fmt,
};

/*
 * V4L2 subdev controls
 */
static int trav_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd = &container_of(ctrl->handler, struct trav,
						handler)->sd;
	struct trav *priv = to_trav(sd);
	int err = 0;

	v4l2_info(sd, "ctrl: 0x%x, value: %d\n", ctrl->id, ctrl->val);

	mutex_lock(&priv->lock);
	switch (ctrl->id) {
	case V4L2_CID_CONTRAST:
		break;

	case V4L2_CID_SATURATION:
		break;

	case V4L2_CID_SHARPNESS:
		break;

	case V4L2_CID_BRIGHTNESS:
		break;
	}
	mutex_unlock(&priv->lock);
	if (err < 0)
		v4l2_err(sd, "Failed to write s_ctrl err %d\n", err);

	return err;
}

static const struct v4l2_ctrl_ops trav_ctrl_ops = {
	.s_ctrl = trav_s_ctrl,
};

/*
 * Reading trav version information
 */
static int trav_registered(struct v4l2_subdev *sd)
{
	int ret = 0;
	struct trav *priv = to_trav(sd);

	mutex_lock(&priv->lock);
	ret = __trav_power_on(priv);
	if (!ret) {
		ret = trav_read_fw_ver(sd);
		__trav_power_off(priv);
	}
	mutex_unlock(&priv->lock);

	return ret;
}

/*
 * V4L2 subdev internal operations
 */
static int trav_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct v4l2_mbus_framefmt *mf = v4l2_subdev_get_try_format(sd, fh->pad, 0);

	mf->width = trav_prev_sizes[TRAV_DEFAULT_FRAME_SIZE].size.width;
	mf->height = trav_prev_sizes[TRAV_DEFAULT_FRAME_SIZE].size.height;
	mf->code = trav_formats[TRAV_DEFAULT_FMT].code;
	mf->colorspace = V4L2_COLORSPACE_RAW;
	mf->field = V4L2_FIELD_NONE;

	return 0;
}

static const struct v4l2_subdev_internal_ops trav_subdev_internal_ops = {
	.registered = trav_registered,
	.open = trav_open,
};

static int trav_s_power(struct v4l2_subdev *sd, int on)
{
	struct trav *priv = to_trav(sd);
	int ret;

	v4l2_info(sd, "Switching %s\n", on ? "on" : "off");

	if (on) {
		ret = __trav_power_on(priv);
		if (ret < 0)
			return ret;
		ret = trav_init_sensor(sd);
		if (ret < 0)
			__trav_power_off(priv);
		else
			priv->set_params = 1;
	} else {
		ret = __trav_power_off(priv);
	}

	return ret;
}

static int trav_log_status(struct v4l2_subdev *sd)
{
	v4l2_ctrl_handler_log_status(sd->ctrl_handler, sd->name);

	return 0;
}

static const struct v4l2_subdev_core_ops trav_core_ops = {
	.s_power	= trav_s_power,
	.log_status	= trav_log_status,
};

static int __trav_s_params(struct trav *priv)
{
	return 0;
}

static int __trav_s_stream(struct trav *priv, int on)
{
	int ret;

	if (on && priv->set_params) {
		ret = __trav_s_params(priv);
		if (ret < 0)
			return ret;
		priv->set_params = 0;
	}

	return 0;
}

static int trav_s_stream(struct v4l2_subdev *sd, int on)
{
	struct trav *priv = to_trav(sd);
	int ret = 0;

	v4l2_info(sd, "Turn streaming %s\n", on ? "on" : "off");

	mutex_lock(&priv->lock);

	if (priv->streaming == !on) {
		ret = __trav_s_stream(priv, on);
		if (!ret)
			priv->streaming = on & 1;
	}

	mutex_unlock(&priv->lock);
	return ret;
}

static const struct v4l2_subdev_video_ops trav_video_ops = {
	.s_stream = trav_s_stream,
};

static const struct v4l2_subdev_ops trav_ops = {
	.core = &trav_core_ops,
	.pad = &trav_pad_ops,
	.video = &trav_video_ops,
};

#ifdef TRAV_CONTROL_CAMERA_SENSOR
static void trav_free_gpios(struct trav *priv)
{
}

static int trav_config_gpios(struct trav *priv)
{
	return 0;
}
#endif

static int trav_init_v4l2_ctrls(struct trav *priv)
{
	const struct v4l2_ctrl_ops *ops = &trav_ctrl_ops;
	struct v4l2_ctrl_handler *hdl = &priv->handler;
	int ret;

	ret = v4l2_ctrl_handler_init(hdl, 4);
	if (ret)
		return ret;

	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_BRIGHTNESS, -208, 127, 1, 0);
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_CONTRAST, -127, 127, 1, 0);
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_SATURATION, -127, 127, 1, 0);

	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_SHARPNESS, -127, 127, 1, 0);
	if (hdl->error) {
		ret = hdl->error;
		v4l2_ctrl_handler_free(hdl);
		return ret;
	}
	priv->sd.ctrl_handler = hdl;

	return 0;
};

static int trav_parse_gpios(struct trav *priv)
{
	return 0;
}

static int trav_get_platform_data(struct trav *priv)
{
	struct device *dev = &priv->i2c_client->dev;
	const struct trav_platform_data *pdata = dev->platform_data;
	struct device_node *node = dev->of_node;
	struct device_node *node_ep;
	struct v4l2_fwnode_endpoint ep;
	int ret;

	if (!node) {
		if (!pdata) {
			dev_err(dev, "Platform data not specified\n");
			return -EINVAL;
		}
		priv->gpio[STBY] = pdata->gpio_stby;
		priv->gpio[RST] = pdata->gpio_reset;
		return 0;
	}

	ret = trav_parse_gpios(priv);
	if (ret < 0)
		return -EINVAL;

	node_ep = of_graph_get_next_endpoint(node, NULL);
	if (!node_ep) {
		dev_warn(dev, "no endpoint defined for node: %s\n",
						node->full_name);
		return 0;
	}

	ret = v4l2_fwnode_endpoint_parse(of_fwnode_handle(node_ep), &ep);
	of_node_put(node_ep);
	if (ret)
		return ret;

	if (ep.bus_type != V4L2_MBUS_CSI2) {
		dev_err(dev, "unsupported bus type\n");
		return -EINVAL;
	}
	/*
	 * Number of MIPI CSI-2 data lanes is currently not configurable,
	 * always a default value of 4 lanes is used.
	 */
	if (ep.bus.mipi_csi2.num_data_lanes != 4)
		dev_info(dev, "falling back to 4 MIPI CSI-2 data lanes\n");

	return 0;
}

static int trav_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct trav *priv;
	int ret;

	priv = devm_kzalloc(&client->dev, sizeof(struct trav), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->i2c_client = client;
	ret = trav_get_platform_data(priv);
	if (ret < 0)
		return ret;

	mutex_init(&priv->lock);
	priv->streaming = 0;

	sd = &priv->sd;
	/* Registering subdev */
	v4l2_i2c_subdev_init(sd, client, &trav_ops);
	strlcpy(sd->name, client->name, sizeof(sd->name));

	sd->internal_ops = &trav_subdev_internal_ops;
	/* Support v4l2 sub-device user space API */
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	priv->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &priv->pad);
	if (ret)
		return ret;

	/* At present camera sensors are controlled via I2C scripts */
	/* Enable below flag to control them via sensor driver functions */
#ifdef TRAV_CONTROL_CAMERA_SENSOR
	ret = trav_config_gpios(priv);
	if (ret) {
		dev_err(&client->dev, "Failed to set gpios\n");
		goto out_err1;
	}
	for (i = 0; i < TRAV_NUM_SUPPLIES; i++)
		priv->supplies[i].supply = trav_supply_names[i];

	ret = devm_regulator_bulk_get(&client->dev, TRAV_NUM_SUPPLIES,
				 priv->supplies);
	if (ret) {
		dev_err(&client->dev, "Failed to get regulators\n");
		goto out_err2;
	}
#endif

	ret = trav_init_v4l2_ctrls(priv);
	if (ret)
		goto out_err2;

	priv->curr_pixfmt = &trav_formats[TRAV_DEFAULT_FMT];
	priv->curr_frmsize = &trav_prev_sizes[TRAV_DEFAULT_FRAME_SIZE];

	ret = v4l2_async_register_subdev(sd);
	if (ret < 0)
		goto out_err1;

	return 0;

out_err2:
#ifdef TRAV_CONTROL_CAMERA_SENSOR
	trav_free_gpios(priv);
#endif
out_err1:
	media_entity_cleanup(&priv->sd.entity);

	return ret;
}

static int trav_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct trav *priv = to_trav(sd);

	v4l2_async_unregister_subdev(sd);

	mutex_destroy(&priv->lock);
#ifdef TRAV_CONTROL_CAMERA_SENSOR
	trav_free_gpios(priv);
#endif
	v4l2_device_unregister_subdev(sd);
	v4l2_ctrl_handler_free(&priv->handler);
	media_entity_cleanup(&sd->entity);

	return 0;
}

static const struct i2c_device_id trav_id[] = {
	{ TRAV_DRIVER_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, trav_id);

#ifdef CONFIG_OF
static const struct of_device_id trav_sensor_of_match[] = {
	{ .compatible = "trav-sensor0" },
	{ .compatible = "trav-sensor1" },
	{ .compatible = "trav-sensor2" },
	{ .compatible = "trav-sensor3" },
	{ .compatible = "trav-sensor4" },
	{ .compatible = "trav-sensor5" },
	{ .compatible = "trav-sensor6" },
	{ .compatible = "trav-sensor7" },
	{ .compatible = "trav-sensor8" },
	{ .compatible = "trav-sensor9" },
	{ .compatible = "trav-sensor10" },
	{ .compatible = "trav-sensor11" },
	{ }
};
MODULE_DEVICE_TABLE(of, trav_sensor_of_match);
#endif

static struct i2c_driver v4l2_i2c_driver = {
	.driver = {
		.of_match_table = of_match_ptr(trav_sensor_of_match),
		.name = TRAV_DRIVER_NAME,
	},
	.probe = trav_probe,
	.remove = trav_remove,
	.id_table = trav_id,
};

module_i2c_driver(v4l2_i2c_driver);

MODULE_DESCRIPTION("TRAV 5MP SOC camera");
MODULE_AUTHOR("Ajay Kumar <ajaykumar.rs@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_FIRMWARE(TRAV_FIRMWARE);
