/*
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
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/platform_device.h>

#include <media/intel-ipu4-isys.h>
#include <media/media-device.h>
#include <media/media-entity.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-core.h>

#include "video-aggre-stub.h"

/*
 * Order matters.
 *
 * 1. Bits-per-pixel, descending.
 * 2. Bits-per-pixel compressed, descending.
 * 3. Pixel order, same as in pixel_order_str. Formats for all four pixel
 *    orders must be defined.
 */
static const struct video_aggre_csi_data_format va_csi_data_formats[] = {
	{ MEDIA_BUS_FMT_RGB888_1X24, 24, 24, PIXEL_ORDER_GBRG, 0x24 },
	{ MEDIA_BUS_FMT_RGB565_1X16, 16, 16, PIXEL_ORDER_GBRG, 0x22 },
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
	{ MEDIA_BUS_FMT_SGRBG10_DPCM8_1X8, 10, 8, PIXEL_ORDER_GRBG, 0x2b },
	{ MEDIA_BUS_FMT_SRGGB10_DPCM8_1X8, 10, 8, PIXEL_ORDER_RGGB, 0x2b },
	{ MEDIA_BUS_FMT_SBGGR10_DPCM8_1X8, 10, 8, PIXEL_ORDER_BGGR, 0x2b },
	{ MEDIA_BUS_FMT_SGBRG10_DPCM8_1X8, 10, 8, PIXEL_ORDER_GBRG, 0x2b },
	{ MEDIA_BUS_FMT_SGRBG8_1X8, 8, 8, PIXEL_ORDER_GRBG, 0x2a },
	{ MEDIA_BUS_FMT_SRGGB8_1X8, 8, 8, PIXEL_ORDER_RGGB, 0x2a },
	{ MEDIA_BUS_FMT_SBGGR8_1X8, 8, 8, PIXEL_ORDER_BGGR, 0x2a },
	{ MEDIA_BUS_FMT_SGBRG8_1X8, 8, 8, PIXEL_ORDER_GBRG, 0x2a },
};

const char *pixel_order_str[] = { "GRBG", "RGGB", "BGGR", "GBRG" };

#define to_csi_format_idx(fmt) (((unsigned long)(fmt)			\
				- (unsigned long)va_csi_data_formats) \
				/ sizeof(*va_csi_data_formats))

static const uint32_t video_aggre_supported_codes_pad[] = {
	MEDIA_BUS_FMT_RGB888_1X24,
	MEDIA_BUS_FMT_RGB565_1X16,
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

static const uint32_t *video_aggre_supported_codes[] = {
	video_aggre_supported_codes_pad,
};

static void __dump_v4l2_mbus_framefmt(struct v4l2_subdev *sd,
				      struct v4l2_mbus_framefmt *ffmt)
{
	dev_dbg(sd->dev, "framefmt: width: %d, height: %d, code: 0x%x.\n",
	       ffmt->width, ffmt->height, ffmt->code);
}
static void __dump_v4l2_subdev_format(struct v4l2_subdev *sd,
				      struct v4l2_subdev_format *fmt)
{
	dev_dbg(sd->dev, "subdev_format: which: %s, pad: %d, stream: %d.\n",
		 fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE ?
		 "V4L2_SUBDEV_FORMAT_ACTIVE" : "V4L2_SUBDEV_FORMAT_TRY",
		 fmt->pad, fmt->stream);

	__dump_v4l2_mbus_framefmt(sd, &fmt->format);
}

static int video_aggre_get_routing(struct v4l2_subdev *sd,
				   struct v4l2_subdev_routing *route)
{
	struct video_aggregator *va = to_video_aggre(sd);
	int i, j;

	for (i = 0, j = 0; i < min(va->nstreams, route->num_routes); ++i) {
		route->routes[j].sink_pad = 0;
		route->routes[j].sink_stream = 0;
		route->routes[j].source_pad = VA_PAD_SOURCE;
		route->routes[j].source_stream = i;
		route->routes[j++].flags = va->flags[i];
	}

	route->num_routes = j;

	return 0;
}

static int video_aggre_set_routing(struct v4l2_subdev *sd,
				   struct v4l2_subdev_routing *route)
{
	struct video_aggregator *va = to_video_aggre(sd);
	int i, ret = 0;

	for (i = 0; i < min(route->num_routes, va->nstreams); ++i) {
		struct v4l2_subdev_route *t = &route->routes[i];

		if (t->sink_stream > va->nstreams - 1 ||
			t->source_stream > va->nstreams - 1) {
			continue;
		}

		if (t->source_pad != VA_PAD_SOURCE)
			continue;

		if (t->flags & V4L2_SUBDEV_ROUTE_FL_ACTIVE)
			va->flags[t->source_stream] |=
				V4L2_SUBDEV_ROUTE_FL_ACTIVE;
		else if (!(t->flags & V4L2_SUBDEV_ROUTE_FL_ACTIVE))
			va->flags[t->source_stream] &=
				(~V4L2_SUBDEV_ROUTE_FL_ACTIVE);
	}

	return ret;
}

static int video_aggre_enum_mbus_code(struct v4l2_subdev *sd,
				      struct v4l2_subdev_pad_config *cfg,
				      struct v4l2_subdev_mbus_code_enum *code)
{
	struct video_aggregator *va = to_video_aggre(sd);
	const uint32_t *supported_code = video_aggre_supported_codes[code->pad];
	bool next_stream = false;
	int i;

	if (code->stream & V4L2_SUBDEV_FLAG_NEXT_STREAM) {
		next_stream = true;
		code->stream &= ~V4L2_SUBDEV_FLAG_NEXT_STREAM;
	}

	if (code->stream > va->nstreams)
		return -EINVAL;

	if (next_stream) {
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

static const struct video_aggre_csi_data_format
		*video_aggre_validate_csi_data_format(u32 code)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(va_csi_data_formats); i++) {
		if (va_csi_data_formats[i].code == code)
			return &va_csi_data_formats[i];
	}

	return &va_csi_data_formats[0];
}

static int video_aggre_get_frame_desc(struct v4l2_subdev *sd, unsigned int pad,
				struct v4l2_mbus_frame_desc *desc)
{
	struct video_aggregator *va = to_video_aggre(sd);
	struct v4l2_mbus_frame_desc_entry *entry = desc->entry;
	u8 vc = 0;
	int i;

	desc->type = V4L2_MBUS_FRAME_DESC_TYPE_CSI2;

	for (i = 0; i < min(va->nstreams, desc->num_entries); i++) {
		struct v4l2_mbus_framefmt *ffmt =
			&va->ffmts[i][VA_PAD_SOURCE];
		const struct video_aggre_csi_data_format *csi_format =
			video_aggre_validate_csi_data_format(ffmt->code);

		entry->size.two_dim.width = ffmt->width;
		entry->size.two_dim.height = ffmt->height;
		entry->pixelcode = ffmt->code;
		entry->bus.csi2.channel = vc++;
		entry->bpp = csi_format->compressed;
		entry++;
		desc->num_entries++;
	}

	return 0;
}

static struct v4l2_mbus_framefmt *
__video_aggre_get_ffmt(struct v4l2_subdev *subdev,
			 struct v4l2_subdev_pad_config *cfg,
			 unsigned int pad, unsigned int which,
			 unsigned int stream)
{
	struct video_aggregator *va = to_video_aggre(subdev);

	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_format(subdev, cfg, pad);
	else
		return &va->ffmts[stream][pad];
}

static int video_aggre_get_format(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_format *fmt)
{
	struct video_aggregator *va = to_video_aggre(subdev);

	if (fmt->stream > va->nstreams)
		return -EINVAL;

	mutex_lock(&va->mutex);
	fmt->format = *__video_aggre_get_ffmt(subdev, cfg, fmt->pad, fmt->which,
					      fmt->stream);
	mutex_unlock(&va->mutex);

	__dump_v4l2_subdev_format(subdev, fmt);

	return 0;
}

static int video_aggre_set_format(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_format *fmt)
{
	struct video_aggregator *va = to_video_aggre(subdev);
	const struct video_aggre_csi_data_format *csi_format;
	struct v4l2_mbus_framefmt *ffmt;

	if (fmt->stream > va->nstreams)
		return -EINVAL;

	csi_format = video_aggre_validate_csi_data_format(fmt->format.code);

	mutex_lock(&va->mutex);
	ffmt = __video_aggre_get_ffmt(subdev, cfg, fmt->pad, fmt->which,
				      fmt->stream);

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE) {
		ffmt->width = fmt->format.width;
		ffmt->height = fmt->format.height;
		ffmt->code = csi_format->code;
	}
	fmt->format = *ffmt;
	mutex_unlock(&va->mutex);

	__dump_v4l2_mbus_framefmt(subdev, ffmt);

	return 0;
}

static int video_aggre_open(struct v4l2_subdev *subdev,
				struct v4l2_subdev_fh *fh)
{
	struct v4l2_mbus_framefmt *try_fmt =
		v4l2_subdev_get_try_format(subdev, fh->pad, 0);

	struct v4l2_subdev_format fmt = {
		.which = V4L2_SUBDEV_FORMAT_TRY,
		.pad = VA_PAD_SOURCE,
		.format = {
			.width = 1920,
			.height = 1080,
			.code = MEDIA_BUS_FMT_RGB888_1X24,
		},
		.stream = 0,
	};

	*try_fmt = fmt.format;

	return 0;
}

static int stub_set_stream(struct v4l2_subdev *subdev, int enable)
{
	struct video_aggregator *va = to_video_aggre(subdev);

	if (enable) {
		dev_dbg(va->sd.dev, "Streaming is ON\n");
		va->streaming = 1;
	} else {
		dev_dbg(va->sd.dev, "Streaming is OFF\n");
		va->streaming = 0;
	}

	return 0;
}

static struct v4l2_subdev_internal_ops video_aggre_sd_internal_ops = {
	.open = video_aggre_open,
};

static const struct v4l2_subdev_video_ops video_aggre_sd_video_ops = {
	.s_stream = stub_set_stream,
};

static const struct v4l2_subdev_core_ops video_aggre_core_subdev_ops = {
	.g_ctrl = v4l2_subdev_g_ctrl,
	.s_ctrl = v4l2_subdev_s_ctrl,
	.g_ext_ctrls = v4l2_subdev_g_ext_ctrls,
	.s_ext_ctrls = v4l2_subdev_s_ext_ctrls,
	.try_ext_ctrls = v4l2_subdev_try_ext_ctrls,
	.queryctrl = v4l2_subdev_queryctrl,
};

static int video_aggre_s_ctrl(struct v4l2_ctrl *ctrl)
{
	return 0;
}

static const struct v4l2_ctrl_ops video_aggre_ctrl_ops = {
	.s_ctrl = video_aggre_s_ctrl,
};

static const s64 video_aggre_op_sys_clock[] =  {400000000, };
static const struct v4l2_ctrl_config video_aggre_controls[] = {
	{
		.ops = &video_aggre_ctrl_ops,
		.id = V4L2_CID_LINK_FREQ,
		.name = "V4L2_CID_LINK_FREQ",
		.type = V4L2_CTRL_TYPE_INTEGER_MENU,
		.max = ARRAY_SIZE(video_aggre_op_sys_clock) - 1,
		.min =  0,
		.step  = 1,
		.def = 0,
		.qmenu_int = video_aggre_op_sys_clock,
	}
};

static const struct v4l2_subdev_pad_ops video_aggre_sd_pad_ops = {
	.get_fmt = video_aggre_get_format,
	.set_fmt = video_aggre_set_format,
	.get_frame_desc = video_aggre_get_frame_desc,
	.enum_mbus_code = video_aggre_enum_mbus_code,
	.set_routing = video_aggre_set_routing,
	.get_routing = video_aggre_get_routing,
};

static struct v4l2_subdev_ops video_aggre_sd_ops = {
	.video = &video_aggre_sd_video_ops,
	.pad = &video_aggre_sd_pad_ops,
};

static int video_aggre_register_subdev(struct video_aggregator *va)
{
	int i, rval;
	struct v4l2_ctrl_config *ctrl = video_aggre_controls;
	struct i2c_client *client = v4l2_get_subdevdata(&va->sd);

	v4l2_subdev_init(&va->sd, &video_aggre_sd_ops);
	snprintf(va->sd.name, sizeof(va->sd.name), "Video Aggregator 0x%x",
		 client->addr);

	va->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
			V4L2_SUBDEV_FL_HAS_SUBSTREAMS;

	va->sd.internal_ops = &video_aggre_sd_internal_ops;

	v4l2_set_subdevdata(&va->sd, va);

	v4l2_ctrl_handler_init(&va->ctrl_handler,
				ARRAY_SIZE(video_aggre_controls));

	if (va->ctrl_handler.error) {
		dev_err(va->sd.dev,
			"Failed to init video aggre controls. ERR: %d!\n",
			va->ctrl_handler.error);
		return va->ctrl_handler.error;
	}

	va->sd.ctrl_handler = &va->ctrl_handler;
	v4l2_ctrl_handler_setup(&va->ctrl_handler);

	va->link_freq = v4l2_ctrl_new_int_menu(&va->ctrl_handler,
					       &video_aggre_ctrl_ops,
					       ctrl->id, ctrl->max,
					       ctrl->def, ctrl->qmenu_int);

	va->pad.flags = MEDIA_PAD_FL_SOURCE | MEDIA_PAD_FL_MULTIPLEX;
	rval = media_entity_init(&va->sd.entity, 1, &va->pad, 0);
	if (rval) {
		dev_err(va->sd.dev,
			"Failed to init media entity video aggre!\n");
		return rval;
	}

	return 0;
}

static int video_aggre_probe(struct i2c_client *client,
			const struct i2c_device_id *devid)
{
	struct video_aggregator *va;
	int i, rval = 0;

	va = devm_kzalloc(&client->dev, sizeof(*va), GFP_KERNEL);
	if (!va) {
		dev_err(&client->dev, "Failed to alloc va structure\n");
		return -ENOMEM;
	}

	va->nsources = NR_OF_VA_SOURCE_PADS;
	va->nsinks = NR_OF_VA_SINK_PADS;
	va->npads = NR_OF_VA_PADS;
	va->nstreams = NR_OF_VA_STREAMS;

	for (i = 0; i < va->nstreams; i++) {
		va->ffmts[i] = devm_kcalloc(&client->dev, va->npads,
					    sizeof(struct v4l2_mbus_framefmt),
					    GFP_KERNEL);
		if (!va->ffmts[i])
			return -ENOMEM;
	}

	va->crop = devm_kcalloc(&client->dev, va->npads,
				sizeof(struct v4l2_rect), GFP_KERNEL);

	va->compose = devm_kcalloc(&client->dev, va->npads,
				   sizeof(struct v4l2_rect), GFP_KERNEL);

	va->flags = devm_kcalloc(&client->dev, va->nstreams,
				       sizeof(*va->flags), GFP_KERNEL);

	if (!va->crop || !va->compose || !va->flags)
		return -ENOMEM;

	for (i = 0; i < va->nstreams; i++)
		va->flags[i] = V4L2_SUBDEV_ROUTE_FL_SOURCE;

	mutex_init(&va->mutex);
	v4l2_i2c_subdev_init(&va->sd, client, &video_aggre_sd_ops);
	rval = video_aggre_register_subdev(va);
	if (rval) {
		dev_err(&client->dev, "Failed to register va subdevice!\n");
		return rval;
	}

	return 0;
}

static int video_aggre_remove(struct i2c_client *client)
{
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct video_aggregator *va = to_video_aggre(subdev);

	mutex_destroy(&va->mutex);
	v4l2_device_unregister_subdev(&va->sd);

	return 0;
}

static const struct i2c_device_id video_aggre_id_table[] = {
	{ VIDEO_AGGRE_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, video_aggre_id_table);

static struct i2c_driver video_aggre_i2c_driver = {
	.driver = {
		.name = VIDEO_AGGRE_NAME,
	},
	.probe	= video_aggre_probe,
	.remove	= video_aggre_remove,
	.id_table = video_aggre_id_table,
};
module_i2c_driver(video_aggre_i2c_driver);

MODULE_AUTHOR("Jianxu Zheng <jian.xu.zheng@intel.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Intel Dummy CSI2-Aggregator driver");
