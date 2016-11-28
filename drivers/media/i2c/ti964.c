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
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/platform_device.h>

#include <media/media-device.h>
#include <media/media-entity.h>
#include <media/ti964.h>
#include <media/crlmodule.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-core.h>

#include "ti964-reg.h"

struct ti964_subdev {
	struct v4l2_subdev *sd;
	unsigned short rx_port;
	unsigned short phy_i2c_addr;
	unsigned short alias_i2c_addr;
};

struct ti964 {
	struct v4l2_subdev sd;
	struct media_pad pad[NR_OF_VA_PADS];
	struct v4l2_ctrl_handler ctrl_handler;
	struct ti964_pdata *pdata;
	struct ti964_subdev sub_devs[NR_OF_VA_SINK_PADS];
	struct crlmodule_platform_data subdev_pdata[NR_OF_VA_SINK_PADS];
	const char *name;

	struct mutex mutex;

	struct regmap *regmap8;
	struct regmap *regmap16;

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

	unsigned int nsinks;
	unsigned int nsources;
	unsigned int nstreams;
	unsigned int npads;

	struct gpio_chip gc;

	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *test_pattern;
};

#define to_ti964(_sd) container_of(_sd, struct ti964, sd)

/*
 * Order matters.
 *
 * 1. Bits-per-pixel, descending.
 * 2. Bits-per-pixel compressed, descending.
 * 3. Pixel order, same as in pixel_order_str. Formats for all four pixel
 *    orders must be defined.
 */
static const struct ti964_csi_data_format va_csi_data_formats[] = {
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

static const uint32_t ti964_supported_codes_pad[] = {
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

static const uint32_t *ti964_supported_codes[] = {
	ti964_supported_codes_pad,
};

static struct regmap_config ti964_reg_config8 = {
	.reg_bits = 8,
	.val_bits = 8,
};

static struct regmap_config ti964_reg_config16 = {
	.reg_bits = 16,
	.val_bits = 8,
	.reg_format_endian = REGMAP_ENDIAN_BIG,
};

static int ti964_reg_set_bit(struct ti964 *va, unsigned char reg,
	unsigned char bit, unsigned char val)
{
	int ret;
	unsigned int reg_val;

	ret = regmap_read(va->regmap8, reg, &reg_val);
	if (ret)
		return ret;
	if (val)
		reg_val |= 1 << bit;
	else
		reg_val &= ~(1 << bit);

	return regmap_write(va->regmap8, reg, reg_val);
}

static int ti964_map_phy_i2c_addr(struct ti964 *va, unsigned short rx_port,
			      unsigned short addr)
{
	int rval;

	rval = regmap_write(va->regmap8, TI964_RX_PORT_SEL,
		(rx_port << 4) + (1 << rx_port));
	if (rval)
		return rval;

	return regmap_write(va->regmap8, TI964_SLAVE_ID0, addr);
}

static int ti964_map_alias_i2c_addr(struct ti964 *va, unsigned short rx_port,
			      unsigned short addr)
{
	int rval;

	rval = regmap_write(va->regmap8, TI964_RX_PORT_SEL,
		(rx_port << 4) + (1 << rx_port));
	if (rval)
		return rval;

	return regmap_write(va->regmap8, TI964_SLAVE_ALIAS_ID0, addr);
}

static int ti964_get_routing(struct v4l2_subdev *sd,
				   struct v4l2_subdev_routing *route)
{
	struct ti964 *va = to_ti964(sd);
	int i;

	for (i = 0; i < min(va->nstreams, route->num_routes); ++i) {
		unsigned int sink = va->route[i].sink;
		unsigned int source = va->route[i].source;

		route->routes[i].sink_pad = sink;
		route->routes[i].sink_stream =
			va->stream[sink].stream_id[0];
		route->routes[i].source_pad = source;
		route->routes[i].source_stream =
			va->stream[source].stream_id[sink];
		route->routes[i].flags = va->route[i].flags;
	}

	route->num_routes = i;

	return 0;
}

static int ti964_set_routing(struct v4l2_subdev *sd,
				   struct v4l2_subdev_routing *route)
{
	struct ti964 *va = to_ti964(sd);
	int i, j, ret = 0;

	for (i = 0; i < min(route->num_routes, va->nstreams); ++i) {
		struct v4l2_subdev_route *t = &route->routes[i];
		unsigned int sink = t->sink_pad;
		unsigned int source = t->source_pad;

		if (t->sink_stream > va->nstreams - 1 ||
		    t->source_stream > va->nstreams - 1)
			continue;

		if (t->source_pad != VA_PAD_SOURCE)
			continue;

		for (j = 0; j < va->nstreams; j++) {
			if (sink == va->route[j].sink &&
				source == va->route[j].source)
				break;
		}

		if (j == va->nstreams)
			continue;

		va->stream[sink].stream_id[0] = t->sink_stream;
		va->stream[source].stream_id[sink] = t->source_stream;

		if (t->flags & V4L2_SUBDEV_ROUTE_FL_ACTIVE)
			va->route[j].flags |=
				V4L2_SUBDEV_ROUTE_FL_ACTIVE;
		else if (!(t->flags & V4L2_SUBDEV_ROUTE_FL_ACTIVE))
			va->route[j].flags &=
				(~V4L2_SUBDEV_ROUTE_FL_ACTIVE);
	}

	return ret;
}

static int ti964_enum_mbus_code(struct v4l2_subdev *sd,
				      struct v4l2_subdev_pad_config *cfg,
				      struct v4l2_subdev_mbus_code_enum *code)
{
	struct ti964 *va = to_ti964(sd);
	const uint32_t *supported_code =
		ti964_supported_codes[code->pad];
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

static const struct ti964_csi_data_format
		*ti964_validate_csi_data_format(u32 code)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(va_csi_data_formats); i++) {
		if (va_csi_data_formats[i].code == code)
			return &va_csi_data_formats[i];
	}

	return &va_csi_data_formats[0];
}

static int ti964_get_frame_desc(struct v4l2_subdev *sd,
	unsigned int pad, struct v4l2_mbus_frame_desc *desc)
{
	struct ti964 *va = to_ti964(sd);
	struct v4l2_mbus_frame_desc_entry *entry = desc->entry;
	u8 vc = 0;
	int i;

	desc->type = V4L2_MBUS_FRAME_DESC_TYPE_CSI2;

	for (i = 0; i < min_t(int, va->nstreams, desc->num_entries); i++) {
		struct v4l2_mbus_framefmt *ffmt =
			&va->ffmts[i][VA_PAD_SOURCE];
		const struct ti964_csi_data_format *csi_format =
			ti964_validate_csi_data_format(ffmt->code);

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
__ti964_get_ffmt(struct v4l2_subdev *subdev,
			 struct v4l2_subdev_pad_config *cfg,
			 unsigned int pad, unsigned int which,
			 unsigned int stream)
{
	struct ti964 *va = to_ti964(subdev);

	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_format(subdev, cfg, pad);
	else
		return &va->ffmts[pad][stream];
}

static int ti964_get_format(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_format *fmt)
{
	struct ti964 *va = to_ti964(subdev);

	if (fmt->stream > va->nstreams)
		return -EINVAL;

	mutex_lock(&va->mutex);
	fmt->format = *__ti964_get_ffmt(subdev, cfg, fmt->pad,
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

static int ti964_set_format(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_format *fmt)
{
	struct ti964 *va = to_ti964(subdev);
	const struct ti964_csi_data_format *csi_format;
	struct v4l2_mbus_framefmt *ffmt;

	if (fmt->stream > va->nstreams)
		return -EINVAL;

	csi_format = ti964_validate_csi_data_format(
		fmt->format.code);

	mutex_lock(&va->mutex);
	ffmt = __ti964_get_ffmt(subdev, cfg, fmt->pad, fmt->which,
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

static int ti964_open(struct v4l2_subdev *subdev,
				struct v4l2_subdev_fh *fh)
{
	struct v4l2_mbus_framefmt *try_fmt =
		v4l2_subdev_get_try_format(subdev, fh->pad, 0);

	struct v4l2_subdev_format fmt = {
		.which = V4L2_SUBDEV_FORMAT_TRY,
		.pad = VA_PAD_SOURCE,
		.format = {
			.width = TI964_MAX_WIDTH,
			.height = TI964_MAX_HEIGHT,
			.code = MEDIA_BUS_FMT_YUYV8_1X16,
		},
		.stream = 0,
	};

	*try_fmt = fmt.format;

	return 0;
}

static int ti964_registered(struct v4l2_subdev *subdev)
{
	struct ti964 *va = to_ti964(subdev);
	int i, j, k, l, rval;

	for (i = 0, k = 0; i < va->pdata->subdev_num; i++) {
		struct ti964_subdev_info *info =
			&va->pdata->subdev_info[i];
		struct crlmodule_platform_data *pdata =
			(struct crlmodule_platform_data *)
			info->board_info.platform_data;
		struct i2c_adapter *adapter;

		if (k >= va->nsinks)
			break;

		/*
		 * The sensors should not share the same pdata structure.
		 * Clone the pdata for each sensor.
		 */
		memcpy(&va->subdev_pdata[k], pdata, sizeof(*pdata));
		va->subdev_pdata[k].xshutdown = va->gc.base + info->rx_port;
		info->board_info.platform_data = &va->subdev_pdata[k];

		if (!info->phy_i2c_addr || !info->board_info.addr) {
			dev_err(va->sd.dev, "can't find the physical and alias addr.\n");
			return -EINVAL;
		}

		/* Map PHY I2C address. */
		rval = ti964_map_phy_i2c_addr(va, info->rx_port,
					info->phy_i2c_addr);
		if (rval)
			return rval;

		/* Map 7bit ALIAS I2C address. */
		rval = ti964_map_alias_i2c_addr(va, info->rx_port,
				info->board_info.addr << 1);
		if (rval)
			return rval;

		adapter = i2c_get_adapter(info->i2c_adapter_id);
		va->sub_devs[k].sd = v4l2_i2c_new_subdev_board(
			va->sd.v4l2_dev, adapter,
			&info->board_info, 0);
		i2c_put_adapter(adapter);
		if (!va->sub_devs[k].sd) {
			dev_err(va->sd.dev,
				"can't create new i2c subdev %d-%04x\n",
				info->i2c_adapter_id,
				info->board_info.addr);
			continue;
		}
		va->sub_devs[k].rx_port = info->rx_port;
		va->sub_devs[k].phy_i2c_addr = info->phy_i2c_addr;
		va->sub_devs[k].alias_i2c_addr = info->board_info.addr;

		for (j = 0; j < va->sub_devs[k].sd->entity.num_pads; j++) {
			if (va->sub_devs[k].sd->entity.pads[j].flags &
				MEDIA_PAD_FL_SOURCE)
				break;
		}

		if (j == va->sub_devs[k].sd->entity.num_pads) {
			dev_warn(va->sd.dev,
				"no source pad in subdev %d-%04x\n",
				info->i2c_adapter_id,
				info->board_info.addr);
			return -ENOENT;
		}

		for (l = 0; l < va->nsinks; l++) {
			rval = media_entity_create_link(
				&va->sub_devs[k].sd->entity, j,
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

static int ti964_set_power(struct v4l2_subdev *subdev, int on)
{
	struct ti964 *va = to_ti964(subdev);

	return regmap_write(va->regmap8, TI964_RESET,
			   (on) ? TI964_POWER_ON : TI964_POWER_OFF);
}

static int ti964_map_subdevs_addr(struct ti964 *va)
{
	unsigned short rx_port, phy_i2c_addr, alias_i2c_addr;
	int i, rval;

	for (i = 0; i < NR_OF_VA_SINK_PADS; i++) {
		rx_port = va->sub_devs[i].rx_port;
		phy_i2c_addr = va->sub_devs[i].phy_i2c_addr;
		alias_i2c_addr = va->sub_devs[i].alias_i2c_addr;

		if (!phy_i2c_addr || !alias_i2c_addr)
			continue;

		rval = ti964_map_phy_i2c_addr(va, rx_port, phy_i2c_addr);
		if (rval)
			return rval;

		/* set 7bit alias i2c addr */
		rval = ti964_map_alias_i2c_addr(va, rx_port,
						alias_i2c_addr << 1);
		if (rval)
			return rval;
	}

	return 0;
}

static int ti964_set_stream(struct v4l2_subdev *subdev, int enable)
{
	struct ti964 *va = to_ti964(subdev);
	int i, j, rval;

	dev_dbg(va->sd.dev, "TI964 set stream. enable=%d\n", enable);
	if (v4l2_ctrl_g_ctrl(va->test_pattern)) {
		dev_dbg(va->sd.dev, "TI964 starts to stream test pattern.\n");
		for (i = 0; i < ARRAY_SIZE(ti964_tp_settings); i++) {
			rval = regmap_write(va->regmap8,
				ti964_tp_settings[i].reg,
				ti964_tp_settings[i].val);
			if (rval) {
				dev_err(va->sd.dev, "Register write error.\n");
				return rval;
			}
		}
		rval = regmap_write(va->regmap8, TI964_IND_ACC_DATA, enable);
		if (rval) {
			dev_err(va->sd.dev, "Register write error.\n");
			return rval;
		}
		return 0;
	}

	for (i = 0; i < NR_OF_VA_SINK_PADS; i++) {
		struct media_pad *remote_pad =
			media_entity_remote_pad(&va->pad[i]);
		unsigned int rx_id;
		struct v4l2_subdev *sd;
		u8 bpp;

		if (!remote_pad)
			continue;
		/* Find TI964 subdev. */
		sd = media_entity_to_v4l2_subdev(remote_pad->entity);
		for (j = 0; j < NR_OF_VA_SINK_PADS; j++) {
			if (va->sub_devs[j].sd == sd)
				break;
		}
		if (j == NR_OF_VA_SINK_PADS) {
			dev_err(va->sd.dev, "Cannot find TI964 subdev.\n");
			continue;
		}
		/* Select RX port. */
		rx_id = va->sub_devs[j].rx_port;
		rval = regmap_write(va->regmap8, TI964_RX_PORT_SEL,
				(rx_id << 4) + (1 << rx_id));
		if (rval) {
			dev_err(va->sd.dev, "Failed to select RX port.\n");
			return rval;
		}
		/* Set RX port mode. */
		bpp = ti964_validate_csi_data_format(
			va->ffmts[i][0].code)->width;
		rval = regmap_write(va->regmap8, TI964_PORT_CONFIG,
			(bpp == 12) ?
			TI964_FPD3_RAW12_75MHz : TI964_FPD3_RAW10_100MHz);
		if (rval) {
			dev_err(va->sd.dev, "Failed to set port config.\n");
			return rval;
		}
		/* RAW8 and YUV422 need to enable RAW10 bit mode. */
		rval = regmap_write(va->regmap8, TI964_PORT_CONFIG2,
			(bpp == 8 || bpp == 16) ?
			TI964_RAW10_8BIT : TI964_RAW10_NORMAL);
		if (rval) {
			dev_err(va->sd.dev, "Failed to set port config2.\n");
			return rval;
		}
		/* Stream on sensor. */
		rval = v4l2_subdev_call(sd, video, s_stream, enable);
		if (rval) {
			dev_err(va->sd.dev,
				"Failed to set stream for %s. enable = %d\n",
				sd->name, enable);
			return rval;
		}
		/* Enable RX port fordwarding. */
		rval = ti964_reg_set_bit(va, TI964_FWD_CTL1, rx_id + 4, !enable);
		if (rval) {
			dev_err(va->sd.dev,
				"Failed to forward RX port%d. enable = %d\n",
				i, enable);
			return rval;
		}
	}
	rval = regmap_write(va->regmap8, TI964_CSI_CTL, enable);
	if (rval) {
		dev_err(va->sd.dev, "Register write error.\n");
		return rval;
	}

	return 0;
}

static struct v4l2_subdev_internal_ops ti964_sd_internal_ops = {
	.open = ti964_open,
	.registered = ti964_registered,
};

static const struct v4l2_subdev_video_ops ti964_sd_video_ops = {
	.s_stream = ti964_set_stream,
};

static const struct v4l2_subdev_core_ops ti964_core_subdev_ops = {
	.s_power = ti964_set_power,
	.g_ctrl = v4l2_subdev_g_ctrl,
	.s_ctrl = v4l2_subdev_s_ctrl,
	.g_ext_ctrls = v4l2_subdev_g_ext_ctrls,
	.s_ext_ctrls = v4l2_subdev_s_ext_ctrls,
	.try_ext_ctrls = v4l2_subdev_try_ext_ctrls,
	.queryctrl = v4l2_subdev_queryctrl,
};

static int ti964_s_ctrl(struct v4l2_ctrl *ctrl)
{
	return 0;
}

static const struct v4l2_ctrl_ops ti964_ctrl_ops = {
	.s_ctrl = ti964_s_ctrl,
};

static const s64 ti964_op_sys_clock[] =  {400000000, };
static const struct v4l2_ctrl_config ti964_controls[] = {
	{
		.ops = &ti964_ctrl_ops,
		.id = V4L2_CID_LINK_FREQ,
		.name = "V4L2_CID_LINK_FREQ",
		.type = V4L2_CTRL_TYPE_INTEGER_MENU,
		.max = ARRAY_SIZE(ti964_op_sys_clock) - 1,
		.min =  0,
		.step  = 0,
		.def = 0,
		.qmenu_int = ti964_op_sys_clock,
	},
	{
		.ops = &ti964_ctrl_ops,
		.id = V4L2_CID_TEST_PATTERN,
		.name = "V4L2_CID_TEST_PATTERN",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.max = 1,
		.min =	0,
		.step  = 1,
		.def = 0,
	},
};

static const struct v4l2_subdev_pad_ops ti964_sd_pad_ops = {
	.get_fmt = ti964_get_format,
	.set_fmt = ti964_set_format,
	.get_frame_desc = ti964_get_frame_desc,
	.enum_mbus_code = ti964_enum_mbus_code,
	.set_routing = ti964_set_routing,
	.get_routing = ti964_get_routing,
};

static struct v4l2_subdev_ops ti964_sd_ops = {
	.core = &ti964_core_subdev_ops,
	.video = &ti964_sd_video_ops,
	.pad = &ti964_sd_pad_ops,
};

static int ti964_register_subdev(struct ti964 *va)
{
	int i, rval;
	struct i2c_client *client = v4l2_get_subdevdata(&va->sd);

	v4l2_subdev_init(&va->sd, &ti964_sd_ops);
	snprintf(va->sd.name, sizeof(va->sd.name), "TI964 %d-%4.4x",
		i2c_adapter_id(client->adapter), client->addr);

	va->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
			V4L2_SUBDEV_FL_HAS_SUBSTREAMS;

	va->sd.internal_ops = &ti964_sd_internal_ops;

	v4l2_set_subdevdata(&va->sd, client);

	v4l2_ctrl_handler_init(&va->ctrl_handler,
				ARRAY_SIZE(ti964_controls));

	if (va->ctrl_handler.error) {
		dev_err(va->sd.dev,
			"Failed to init ti964 controls. ERR: %d!\n",
			va->ctrl_handler.error);
		return va->ctrl_handler.error;
	}

	va->sd.ctrl_handler = &va->ctrl_handler;

	for (i = 0; i < ARRAY_SIZE(ti964_controls); i++) {
		const struct v4l2_ctrl_config *cfg =
			&ti964_controls[i];
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
	va->pad[VA_PAD_SOURCE].flags =
		MEDIA_PAD_FL_SOURCE | MEDIA_PAD_FL_MULTIPLEX;
	rval = media_entity_init(&va->sd.entity, NR_OF_VA_PADS, va->pad, 0);
	if (rval) {
		dev_err(va->sd.dev,
			"Failed to init media entity for ti964!\n");
		goto failed_out;
	}

	return 0;

failed_out:
	v4l2_ctrl_handler_free(&va->ctrl_handler);
	return rval;
}

static int ti964_init(struct ti964 *va)
{
	unsigned int reset_gpio = va->pdata->reset_gpio;
	int i, rval;
	unsigned int val;

	gpio_set_value(reset_gpio, 1);
	dev_dbg(va->sd.dev, "Setting reset gpio %d to 1.\n", reset_gpio);

	rval = regmap_read(va->regmap8, TI964_DEVID, &val);
	if (rval) {
		dev_err(va->sd.dev, "Failed to read device ID of TI964!\n");
		return rval;
	}
	dev_info(va->sd.dev, "TI964 device ID: 0x%X\n", val);

	for (i = 0; i < ARRAY_SIZE(ti964_init_settings); i++) {
		rval = regmap_write(va->regmap8,
			ti964_init_settings[i].reg,
			ti964_init_settings[i].val);
		if (rval)
			return rval;
	}

	rval = ti964_map_subdevs_addr(va);
	if (rval)
		return rval;

	return 0;
}

static void ti964_gpio_set(struct gpio_chip *chip, unsigned gpio, int value)
{
	struct i2c_client *client = to_i2c_client(chip->dev);
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct ti964 *va = to_ti964(subdev);
	unsigned int reg_val;
	int ret;

	if (gpio >= NR_OF_VA_SINK_PADS)
		return;

	ret = regmap_write(va->regmap8, TI964_RX_PORT_SEL,
			  (gpio << 4) + (1 << gpio));
	if (ret) {
		dev_dbg(chip->dev, "Failed to select RX port.\n");
		return;
	}
	ret = regmap_read(va->regmap8, TI964_BC_GPIO_CTL0, &reg_val);
	if (ret) {
		dev_dbg(chip->dev, "Failed to read gpio status.\n");
		return;
	}
	reg_val &= ~TI964_GPIO1_MASK;
	reg_val |= value ? TI964_GPIO_HIGH : TI964_GPIO_LOW;
	ret = regmap_write(va->regmap8, TI964_BC_GPIO_CTL0, reg_val);
	if (ret)
		dev_dbg(chip->dev, "Failed to set gpio.\n");
}

static int ti964_gpio_direction_output(struct gpio_chip *chip,
				       unsigned gpio, int level)
{
	return 0;
}

static int ti964_probe(struct i2c_client *client,
			const struct i2c_device_id *devid)
{
	struct ti964 *va;
	int i, rval = 0;

	if (client->dev.platform_data == NULL)
		return -ENODEV;

	va = devm_kzalloc(&client->dev, sizeof(*va), GFP_KERNEL);
	if (!va)
		return -ENOMEM;

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
					   &ti964_reg_config8);
	if (IS_ERR(va->regmap8)) {
		dev_err(&client->dev, "Failed to init regmap8!\n");
		return -EIO;
	}

	va->regmap16 = devm_regmap_init_i2c(client,
					    &ti964_reg_config16);
	if (IS_ERR(va->regmap16)) {
		dev_err(&client->dev, "Failed to init regmap16!\n");
		return -EIO;
	}

	mutex_init(&va->mutex);
	v4l2_i2c_subdev_init(&va->sd, client, &ti964_sd_ops);
	rval = ti964_register_subdev(va);
	if (rval) {
		dev_err(&client->dev, "Failed to register va subdevice!\n");
		return rval;
	}

	if (devm_gpio_request_one(va->sd.dev, va->pdata->reset_gpio, 0,
				  "ti964 reset") != 0) {
		dev_err(va->sd.dev, "Unable to acquire gpio %d\n",
			va->pdata->reset_gpio);
		return -ENODEV;
	}

	rval = ti964_init(va);
	if (rval) {
		dev_err(&client->dev, "Failed to init TI964!\n");
		return rval;
	}

	/*
	 * TI964 has several back channel GPIOs.
	 * We export GPIO1 with the GPIO chip to control
	 * sensor power and reset pin.
	 */
	va->gc.dev = &client->dev;
	va->gc.owner = THIS_MODULE;
	va->gc.label = "TI964 GPIO";
	va->gc.ngpio = NR_OF_VA_SINK_PADS;
	va->gc.base = -1;
	va->gc.set = ti964_gpio_set;
	va->gc.direction_output = ti964_gpio_direction_output;
	rval = gpiochip_add(&va->gc);
	if (rval) {
		dev_err(&client->dev, "Failed to add gpio chip!\n");
		return -EIO;
	}

	return 0;
}

static int ti964_remove(struct i2c_client *client)
{
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct ti964 *va = to_ti964(subdev);
	int i;

	if (!va)
		return 0;

	mutex_destroy(&va->mutex);
	v4l2_ctrl_handler_free(&va->ctrl_handler);
	v4l2_device_unregister_subdev(&va->sd);
	media_entity_cleanup(&va->sd.entity);

	for (i = 0; i < NR_OF_VA_SINK_PADS; i++) {
		if (va->sub_devs[i].sd) {
			struct i2c_client *sub_client =
				v4l2_get_subdevdata(va->sub_devs[i].sd);

			i2c_unregister_device(sub_client);
		}
		va->sub_devs[i].sd = NULL;
	}

	gpiochip_remove(&va->gc);

	return 0;
}

#ifdef CONFIG_PM
static int ti964_suspend(struct device *dev)
{
	return 0;
}

static int ti964_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct ti964 *va = to_ti964(subdev);

	return ti964_init(va);
}
#else
#define ti964_suspend	NULL
#define ti964_resume	NULL
#endif /* CONFIG_PM */

static const struct i2c_device_id ti964_id_table[] = {
	{ TI964_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, ti964_id_table);

static const struct dev_pm_ops ti964_pm_ops = {
	.suspend = ti964_suspend,
	.resume = ti964_resume,
};

static struct i2c_driver ti964_i2c_driver = {
	.driver = {
		.name = TI964_NAME,
		.pm = &ti964_pm_ops,
	},
	.probe	= ti964_probe,
	.remove	= ti964_remove,
	.id_table = ti964_id_table,
};
module_i2c_driver(ti964_i2c_driver);

MODULE_AUTHOR("Tianshu Qiu <tian.shu.qiu@intel.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TI964 CSI2-Aggregator driver");
