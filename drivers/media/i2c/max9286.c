/* SPDX-LIcense_Identifier: GPL-2.0 */
/* Copyright (C) 2018 Intel Corporation */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

#include <media/media-device.h>
#include <media/media-entity.h>
#include <media/max9286.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-core.h>

#include "max9286-reg-settings.h"

struct max9286 {
	struct v4l2_subdev v4l2_sd;
	struct max9286_pdata *pdata;
	struct media_pad pad[NR_OF_MAX_PADS];
	unsigned char sensor_present;
	unsigned int total_sensor_num;
	unsigned int nsources;
	unsigned int nsinks;
	unsigned int npads;
	unsigned int nstreams;
	const char *name;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_subdev *sub_devs[NR_OF_MAX_SINK_PADS];
	struct v4l2_mbus_framefmt *ffmts[NR_OF_MAX_PADS];
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

	struct regmap *regmap8;
	struct mutex max_mutex;
	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *test_pattern;
};

#define to_max_9286(_sd) container_of(_sd, struct max9286, v4l2_sd)

/*
 * Order matters.
 *
 * 1. Bits-per-pixel, descending.
 * 2. Bits-per-pixel compressed, descending.
 * 3. Pixel order, same as in pixel_order_str. Formats for all four pixel
 *    orders must be defined.
 */
static const struct max9286_csi_data_format max_csi_data_formats[] = {
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

static const uint32_t max9286_supported_codes_pad[] = {
	MEDIA_BUS_FMT_YUYV8_1X16,
	MEDIA_BUS_FMT_UYVY8_1X16,
	MEDIA_BUS_FMT_SGRBG12_1X12,
	MEDIA_BUS_FMT_SRGGB12_1X12,
	MEDIA_BUS_FMT_SBGGR12_1X12,
	MEDIA_BUS_FMT_SGBRG12_1X12,
	MEDIA_BUS_FMT_SGRBG10_1X10,
	MEDIA_BUS_FMT_SRGGB10_1X10,
	MEDIA_BUS_FMT_SBGGR10_1X10,
	MEDIA_BUS_FMT_SGBRG10_1X10,
	MEDIA_BUS_FMT_SGRBG8_1X8,
	MEDIA_BUS_FMT_SRGGB8_1X8,
	MEDIA_BUS_FMT_SBGGR8_1X8,
	MEDIA_BUS_FMT_SGBRG8_1X8,
	0,
};

static const uint32_t *max9286_supported_codes[] = {
	max9286_supported_codes_pad,
};

static struct regmap_config max9286_reg_config8 = {
	.reg_bits = 8,
	.val_bits = 8,
};

/* Serializer register write */
static int max96705_write_register(struct max9286 *max,
	unsigned int offset, u8 reg, u8 val)
{
	int ret;
	int retry, timeout = 10;
	struct i2c_client *client = v4l2_get_subdevdata(&max->v4l2_sd);

	client->addr = S_ADDR_MAX96705 + offset;
	for (retry = 0; retry < timeout; retry++) {
		ret = i2c_smbus_write_byte_data(client, reg, val);
		if (val < 0)
			usleep_range(5000, 6000);
		else
			break;
	}

	client->addr = DS_ADDR_MAX9286;
	if (retry >= timeout) {
		dev_err(max->v4l2_sd.dev,
			"%s:write reg failed: reg=%2x\n", __func__, reg);
		return -EREMOTEIO;
	}

	return 0;
}

/* Serializer register read */
static int
max96705_read_register(struct max9286 *max, unsigned int i, u8 reg)
{
	int val;
	int retry, timeout = 10;
	struct i2c_client *client = v4l2_get_subdevdata(&max->v4l2_sd);

	client->addr = S_ADDR_MAX96705 + i;
	for (retry = 0; retry < timeout; retry++) {
		val = i2c_smbus_read_byte_data(client, reg);
		if (val >= 0)
			break;
		usleep_range(5000, 6000);
	}

	client->addr = DS_ADDR_MAX9286;
	if (retry >= timeout) {
		dev_err(max->v4l2_sd.dev,
			"%s:read reg failed: reg=%2x\n", __func__, reg);
		return -EREMOTEIO;
	}

	return val;
}

/* Initialize image sensors and set stream on registers */
static int max9286_set_stream(struct v4l2_subdev *subdev, int enable)
{
	struct max9286 *max = to_max_9286(subdev);
	struct media_pad *remote_pad;
	struct v4l2_subdev *sd;
	int i, rval;
	unsigned int val;
	u8 slval = 0;

	dev_dbg(max->v4l2_sd.dev, "MAX9286 set stream. enable = %d\n", enable);

	/* Disable I2C ACK */
	rval = regmap_write(max->regmap8, DS_I2CLOCACK, 0xB6);
	if (rval) {
		dev_err(max->v4l2_sd.dev, "Failed to disable I2C ACK!\n");
		return rval;
	}

	for (i = 0; i < NR_OF_MAX_SINK_PADS; i++) {
		if (((0x01 << (i)) &  max->sensor_present) == 0)
			continue;

		/* Find the pad at the remote end of the link */
		remote_pad = media_entity_remote_pad(&max->pad[i]);

		if (!remote_pad)
			continue;

		slval = 0xEF;
		rval = regmap_write(max->regmap8, DS_LINK_ENABLE, slval);
		if (rval) {
			dev_err(max->v4l2_sd.dev,
				"Failed to enable GMSL links!\n");
			return rval;
		}

		rval = regmap_write(max->regmap8, DS_ATUO_MASK_LINK, 0x30);
		if (rval) {
			dev_err(max->v4l2_sd.dev, "Failed to write 0x69\n");
			return rval;
		}
		/* Calls sensor set stream */
		sd = media_entity_to_v4l2_subdev(remote_pad->entity);
		rval = v4l2_subdev_call(sd, video, s_stream, enable);
		if (rval) {
			dev_err(max->v4l2_sd.dev,
				"Failed to set stream for %s. enable = %d\n",
				sd->name, enable);
			return rval;
		}
		break;
	}

	/* Enable I2C ACK */
	rval = regmap_write(max->regmap8, DS_I2CLOCACK, 0x36);
	if (rval) {
		dev_err(max->v4l2_sd.dev, "Failed to enable I2C ACK!\n");
		return rval;
	}

	/* Check if valid PCLK is available for the links */
	for (i = 1; i <= NR_OF_MAX_SINK_PADS; i++) {
		if (((0x01 << (i - 1)) &  max->sensor_present) == 0)
			continue;

		val = max96705_read_register(max, i, S_INPUT_STATUS);
		if ((val != -EREMOTEIO) && (val & 0x01))
			dev_info(max->v4l2_sd.dev,
				"Valid PCLK detected for link %d\n", i);
		else if (val != -EREMOTEIO)
			dev_info(max->v4l2_sd.dev,
				"Failed to read PCLK reg for link %d\n", i);
	}

	/* Set preemphasis settings for all serializers (set to 3.3dB)*/
	max96705_write_register(max, S_ADDR_MAX96705_BROADCAST -
		S_ADDR_MAX96705, S_CMLLVL_PREEMP, 0xAA);
	usleep_range(5000, 6000);

	/* Enable link equalizers */
	rval = regmap_write(max->regmap8, DS_ENEQ, 0x0F);
	if (rval) {
		dev_err(max->v4l2_sd.dev,
			"Failed to automatically detect serial data rate!\n");
		return rval;
	}
	usleep_range(5000, 6000);
	rval = regmap_write(max->regmap8, 0x0C, 0x91);

	/* Enable serial links and desable configuration */
	max96705_write_register(max, S_ADDR_MAX96705_BROADCAST -
		S_ADDR_MAX96705, S_MAIN_CTL, 0x83);
	/* Wait for more than 2 Frames time from each sensor */
	usleep_range(100000, 101000);

	/*
	 * Poll frame synchronization bit of deserializer
	 * All the cameras should work in SYNC mode
	 * MAX9286 sends a pulse to each camera, then each camera sends out
	 * one frame. The VSYNC for each camera should appear in almost same
	 * time for the deserializer to lock FSYNC
	 */
	rval = regmap_read(max->regmap8, DS_FSYNC_LOCKED, &val);
	if (rval) {
		dev_info(max->v4l2_sd.dev, "Frame SYNC not locked!\n");
		return rval;
	} else if (val & (0x01 << 6))
		dev_info(max->v4l2_sd.dev, "Deserializer Frame SYNC locked\n");

	/*
	 * Enable/set bit[7] of DS_CSI_VC_CTL register for VC operation
	 * Set VC according to the link number
	 * Enable CSI-2 output
	 */
	if (!enable) {
		rval = regmap_write(max->regmap8, DS_CSI_VC_CTL, 0x93);
		if (rval) {
			dev_err(max->v4l2_sd.dev,
				"Failed to disable CSI output!\n");
			return rval;
		}
	} else {
		rval = regmap_write(max->regmap8, DS_CSI_VC_CTL, 0x9B);
		if (rval) {
			dev_err(max->v4l2_sd.dev,
				"Failed to enable CSI output!\n");
			return rval;
		}
	}

	return 0;
}

/* Get the media bus format */
static struct v4l2_mbus_framefmt *
__max9286_get_ffmt(struct v4l2_subdev *subdev,
	struct v4l2_subdev_pad_config *cfg,
	unsigned int pad, unsigned int which,
	unsigned int stream)
{
	struct max9286 *max = to_max_9286(subdev);

	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_format(subdev, cfg, pad);
	else
		return &max->ffmts[pad][stream];
}

/* callback for VIDIOC_SUBDEV_G_FMT ioctl handler code */
static int max9286_get_format(struct v4l2_subdev *subdev,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *fmt)
{
	struct max9286 *max = to_max_9286(subdev);

	if (fmt->stream > max->nstreams)
		return -EINVAL;

	mutex_lock(&max->max_mutex);
	fmt->format = *__max9286_get_ffmt(subdev, cfg, fmt->pad, fmt->which,
		fmt->stream);
	mutex_unlock(&max->max_mutex);

	dev_dbg(subdev->dev, "subdev_format: which: %s, pad: %d, stream: %d.\n",
		fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE ?
		"V4L2_SUBDEV_FORMAT_ACTIVE" : "V4L2_SUBDEV_FORMAT_TRY",
		fmt->pad, fmt->stream);

	dev_dbg(subdev->dev, "framefmt: width: %d, height: %d, code: 0x%x.\n",
		fmt->format.width, fmt->format.height, fmt->format.code);

	return 0;
}

/* Validate csi_data_format */
static const struct max9286_csi_data_format *
max9286_validate_csi_data_format(u32 code)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(max_csi_data_formats); i++) {
		if (max_csi_data_formats[i].code == code)
			return &max_csi_data_formats[i];
	}

	return &max_csi_data_formats[0];
}

/* callback for VIDIOC_SUBDEV_S_FMT ioctl handler code */
static int max9286_set_format(struct v4l2_subdev *subdev,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *fmt)
{
	struct max9286 *max = to_max_9286(subdev);
	const struct max9286_csi_data_format *csi_format;
	struct v4l2_mbus_framefmt *ffmt;

	if (fmt->stream > max->nstreams)
		return -EINVAL;

	csi_format = max9286_validate_csi_data_format(fmt->format.code);

	mutex_lock(&max->max_mutex);
	ffmt = __max9286_get_ffmt(subdev, cfg, fmt->pad, fmt->which,
		fmt->stream);
	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE) {
		ffmt->width = fmt->format.width;
		ffmt->height = fmt->format.height;
		ffmt->code = csi_format->code;
	}

	fmt->format = *ffmt;
	mutex_unlock(&max->max_mutex);

	dev_dbg(subdev->dev, "framefmt: width: %d, height: %d, code: 0x%x.\n",
		ffmt->width, ffmt->height, ffmt->code);

	return 0;
}

/* get the current low level media bus frame parameters */
static int max9286_get_frame_desc(struct v4l2_subdev *sd,
	unsigned int pad, struct v4l2_mbus_frame_desc *desc)
{
	struct max9286 *max = to_max_9286(sd);
	struct v4l2_mbus_frame_desc_entry *entry = desc->entry;
	u8 vc = 0;
	int i;

	desc->type = V4L2_MBUS_FRAME_DESC_TYPE_CSI2;

	for (i = 0; i < min_t(int, max->nstreams, desc->num_entries); i++) {
		struct v4l2_mbus_framefmt *ffmt =
			&max->ffmts[i][MAX_PAD_SOURCE];
		const struct max9286_csi_data_format *csi_format =
			max9286_validate_csi_data_format(ffmt->code);

		entry->size.two_dim.width = ffmt->width;
		entry->size.two_dim.height = ffmt->height;
		entry->pixelcode = ffmt->code;
		entry->bus.csi2.channel = vc++;
		entry->bpp = csi_format->compressed;
		entry++;
	}

	return 0;
}

/* Enumerate media bus formats available at a given sub-device pad */
static int max9286_enum_mbus_code(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_mbus_code_enum *code)
{
	struct max9286 *max = to_max_9286(sd);
	const uint32_t *supported_code = max9286_supported_codes[code->pad];
	bool next_stream = false;
	int i;

	if (code->stream & V4L2_SUBDEV_FLAG_NEXT_STREAM) {
		next_stream = true;
		code->stream &= ~V4L2_SUBDEV_FLAG_NEXT_STREAM;
	}

	if (code->stream > max->nstreams)
		return -EINVAL;

	if (next_stream) {
		if (!(max->pad[code->pad].flags & MEDIA_PAD_FL_MULTIPLEX))
			return -EINVAL;

		if (code->stream < max->nstreams - 1) {
			code->stream++;
			return 0;
		} else
			return -EINVAL;
	}

	for (i = 0; supported_code[i]; i++) {
		if (i == code->index) {
			code->code = supported_code[i];
			return 0;
		}
	}

	return -EINVAL;
}

/* Configure Media Controller routing */
static int max9286_set_routing(struct v4l2_subdev *sd,
	struct v4l2_subdev_routing *route)
{
	struct max9286 *max = to_max_9286(sd);
	int i, j, ret = 0;

	for (i = 0; i < min(route->num_routes, max->nstreams); ++i) {
		struct v4l2_subdev_route *t = &route->routes[i];
		unsigned int sink = t->sink_pad;
		unsigned int source = t->source_pad;

		if (t->sink_stream > max->nstreams - 1 ||
			t->source_stream > max->nstreams - 1)
			continue;

		if (t->source_pad != MAX_PAD_SOURCE)
			continue;

		for (j = 0; j < max->nstreams; j++) {
			if (sink == max->route[j].sink &&
				source == max->route[j].source)
				break;
		}

		if (j == max->nstreams)
			continue;

		max->stream[sink].stream_id[0] = t->sink_stream;
		max->stream[source].stream_id[sink] = t->source_stream;

		if (t->flags & V4L2_SUBDEV_ROUTE_FL_ACTIVE)
			max->route[j].flags |= V4L2_SUBDEV_ROUTE_FL_ACTIVE;
		else if (!(t->flags & V4L2_SUBDEV_ROUTE_FL_ACTIVE))
			max->route[j].flags &= (~V4L2_SUBDEV_ROUTE_FL_ACTIVE);
	}

	return ret;
}

/* Configure Media Controller routing */
static int max9286_get_routing(struct v4l2_subdev *sd,
	struct v4l2_subdev_routing *route)
{
	struct max9286 *max = to_max_9286(sd);
	int i;

	for (i = 0; i < min(max->nstreams, route->num_routes); ++i) {
		unsigned int sink = max->route[i].sink;
		unsigned int source = max->route[i].source;

		route->routes[i].sink_pad = sink;
		route->routes[i].sink_stream =
			max->stream[sink].stream_id[0];
		route->routes[i].source_pad = source;
		route->routes[i].source_stream =
			max->stream[source].stream_id[sink];
		route->routes[i].flags = max->route[i].flags;
	}

	route->num_routes = i;

	return 0;
}

/* called when the subdev device node is opened by an application */
static int max9286_open(struct v4l2_subdev *subdev,
	struct v4l2_subdev_fh *fh)
{
	struct v4l2_mbus_framefmt *try_fmt =
		v4l2_subdev_get_try_format(subdev, fh->pad, 0);

	struct v4l2_subdev_format fmt = {
		.which = V4L2_SUBDEV_FORMAT_TRY,
		.pad = MAX_PAD_SOURCE,
		.format = {
			.width = MAX9286_MAX_WIDTH,
			.height = MAX9286_MAX_HEIGHT,

			.code = MEDIA_BUS_FMT_SGRBG12_1X12,
		},
		.stream = 0,
	};

	*try_fmt = fmt.format;

	return 0;
}

/*
 * called when this subdev is registered. When called the v4l2_dev field is
 * set to the correct v4l2_device.
 */
static int max9286_registered(struct v4l2_subdev *subdev)
{
	struct max9286 *max = to_max_9286(subdev);
	int i, j, k, l, rval, num, nsinks;

	num = max->pdata->subdev_num;
	nsinks = max->nsinks;
	for (i = 0, k = 0; (i < num) && (k < nsinks); i++, k++) {
		struct max9286_subdev_i2c_info *info =
			&max->pdata->subdev_info[i];
		struct i2c_adapter *adapter;

		adapter = i2c_get_adapter(info->i2c_adapter_id);
		max->sub_devs[k] = v4l2_i2c_new_subdev_board(
			max->v4l2_sd.v4l2_dev, adapter,
			&info->board_info, 0);
		i2c_put_adapter(adapter);
		if (!max->sub_devs[k]) {
			dev_err(max->v4l2_sd.dev,
				"can't create new i2c subdev %d-%04x\n",
				info->i2c_adapter_id,
				info->board_info.addr);
			continue;
		}

		for (j = 0; j < max->sub_devs[k]->entity.num_pads; j++) {
			if (max->sub_devs[k]->entity.pads[j].flags &
				MEDIA_PAD_FL_SOURCE)
				break;
		}

		if (j == max->sub_devs[k]->entity.num_pads) {
			dev_warn(max->v4l2_sd.dev,
				"no source pad in subdev %d-%04x\n",
				info->i2c_adapter_id,
				info->board_info.addr);
			return -ENOENT;
		}

		for (l = 0; l < max->nsinks; l++) {
			rval = media_create_pad_link(
				&max->sub_devs[k]->entity, j,
				&max->v4l2_sd.entity, l, 0);
			if (rval) {
				dev_err(max->v4l2_sd.dev,
					"can't create link to %d-%04x\n",
					info->i2c_adapter_id,
					info->board_info.addr);
				return -EINVAL;
			}
		}
	}

	return 0;
}

static int max9286_set_power(struct v4l2_subdev *subdev, int on)
{
	return 0;
}

static const struct v4l2_subdev_core_ops max9286_core_subdev_ops = {
	.s_power = max9286_set_power,
};

static bool max9286_sd_has_route(struct media_entity *entity,
	unsigned int pad0, unsigned int pad1, int *stream)
{
	struct max9286 *va = to_max_9286(media_entity_to_v4l2_subdev(entity));

	if (stream == NULL || *stream >= va->nstreams)
		return false;

	if ((va->route[*stream].flags & V4L2_SUBDEV_ROUTE_FL_ACTIVE) &&
		((va->route[*stream].source == pad0 &&
		va->route[*stream].sink == pad1) ||
		(va->route[*stream].source == pad1 &&
		va->route[*stream].sink == pad0)))
		return true;

	return false;
}

static const struct v4l2_subdev_video_ops max9286_sd_video_ops = {
	.s_stream = max9286_set_stream,
};

static const struct media_entity_operations max9286_sd_entity_ops = {
		.has_route = max9286_sd_has_route,
};

static const struct v4l2_subdev_pad_ops max9286_sd_pad_ops = {
	.get_fmt = max9286_get_format,
	.set_fmt = max9286_set_format,
	.get_frame_desc = max9286_get_frame_desc,
	.enum_mbus_code = max9286_enum_mbus_code,
	.set_routing = max9286_set_routing,
	.get_routing = max9286_get_routing,
};

static struct v4l2_subdev_ops max9286_sd_ops = {
	.core = &max9286_core_subdev_ops,
	.video = &max9286_sd_video_ops,
	.pad = &max9286_sd_pad_ops,
};

static struct v4l2_subdev_internal_ops max9286_sd_internal_ops = {
	.open = max9286_open,
	.registered = max9286_registered,
};

static int max9286_s_ctrl(struct v4l2_ctrl *ctrl)
{
	return 0;
}

static const struct v4l2_ctrl_ops max9286_ctrl_ops = {
	.s_ctrl = max9286_s_ctrl,
};

static const s64 max9286_op_sys_clock[] = { 87750000, };
static const struct v4l2_ctrl_config max9286_controls[] = {
	{
		.ops = &max9286_ctrl_ops,
		.id = V4L2_CID_LINK_FREQ,
		.name = "V4L2_CID_LINK_FREQ",
		.type = V4L2_CTRL_TYPE_INTEGER_MENU,
		.max = ARRAY_SIZE(max9286_op_sys_clock) - 1,
		.min = 0,
		.step = 0,
		.def = 0,
		.qmenu_int = max9286_op_sys_clock,
	},
	{
		.ops = &max9286_ctrl_ops,
		.id = V4L2_CID_TEST_PATTERN,
		.name = "V4L2_CID_TEST_PATTERN",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.max = 1,
		.min = 0,
		.step = 1,
		.def = 0,
	},
};

/* Registers MAX9286 sub-devices (Image sensors) */
static int max9286_register_subdev(struct max9286 *max)
{
	int i, rval;
	struct i2c_client *client = v4l2_get_subdevdata(&max->v4l2_sd);

	/* subdevice driver initializes v4l2 subdev */
	v4l2_subdev_init(&max->v4l2_sd, &max9286_sd_ops);
	snprintf(max->v4l2_sd.name, sizeof(max->v4l2_sd.name),
		"MAX9286 %d-%4.4x", i2c_adapter_id(client->adapter),
		client->addr);
	max->v4l2_sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		V4L2_SUBDEV_FL_HAS_SUBSTREAMS;

	max->v4l2_sd.internal_ops = &max9286_sd_internal_ops;
	max->v4l2_sd.entity.ops = &max9286_sd_entity_ops;
	v4l2_set_subdevdata(&max->v4l2_sd, client);

	v4l2_ctrl_handler_init(&max->ctrl_handler,
		ARRAY_SIZE(max9286_controls));

	if (max->ctrl_handler.error) {
		dev_err(max->v4l2_sd.dev,
			"Failed to init max9286 controls. ERR: %d!\n",
			max->ctrl_handler.error);
		return max->ctrl_handler.error;
	}

	max->v4l2_sd.ctrl_handler = &max->ctrl_handler;

	for (i = 0; i < ARRAY_SIZE(max9286_controls); i++) {
		const struct v4l2_ctrl_config *cfg =
			&max9286_controls[i];
		struct v4l2_ctrl *ctrl;

		ctrl = v4l2_ctrl_new_custom(&max->ctrl_handler, cfg, NULL);
		if (!ctrl) {
			dev_err(max->v4l2_sd.dev,
				"Failed to create ctrl %s!\n", cfg->name);
			rval = max->ctrl_handler.error;
			goto failed_out;
		}
	}

	max->link_freq = v4l2_ctrl_find(&max->ctrl_handler, V4L2_CID_LINK_FREQ);
	max->test_pattern = v4l2_ctrl_find(&max->ctrl_handler,
		V4L2_CID_TEST_PATTERN);

	for (i = 0; i < max->nsinks; i++)
		max->pad[i].flags = MEDIA_PAD_FL_SINK;
	max->pad[MAX_PAD_SOURCE].flags =
		MEDIA_PAD_FL_SOURCE | MEDIA_PAD_FL_MULTIPLEX;
	rval = media_entity_pads_init(&max->v4l2_sd.entity,
		NR_OF_MAX_PADS, max->pad);
	if (rval) {
		dev_err(max->v4l2_sd.dev,
			"Failed to init media entity for max9286!\n");
		goto failed_out;
	}

	return 0;
failed_out:
	media_entity_cleanup(&max->v4l2_sd.entity);
	v4l2_ctrl_handler_free(&max->ctrl_handler);
	return rval;
}

/*
 * Get the output link order
 * By default:
 * bits[7:6] 11: Link 3 is 4th in the CSI-2 output order
 * bits[5:4] 10: Link 2 is 3rd in the CSI-2 output order
 * bits[3:2] 01: Link 1 is 2nd in the CSI-2 output order
 * bits[1:0] 00: Link 0 is 1st in the CSI-2 output order
 */
u8 get_output_link_order(struct max9286 *max)
{
	u8 val = 0xE4, i;
	u8 order_config[14][3] = {
		{1, 8, 0x27},
		{1, 4, 0xC6},
		{1, 2, 0xE1},
		{1, 1, 0xE4},
		{2, 0xC, 0x4E},
		{2, 0xA, 0x72},
		{2, 0x9, 0x78},
		{2, 0x6, 0xD2},
		{2, 0x5, 0xD8},
		{2, 0x3, 0xE4},
		{3, 0xE, 0x93},
		{3, 0xD, 0x9C},
		{3, 0xB, 0xB4},
		{3, 0x7, 0xE4},
	};

	if (max->total_sensor_num < 4) {
		for (i = 0; i < 14; i++) {
			if ((max->total_sensor_num == order_config[i][0])
				&& (max->sensor_present == order_config[i][1]))
				return  order_config[i][2];
		}
	}

	/* sensor_num = 4 will return 0xE4 */
	return val;
}

/* MAX9286 initial setup and Reverse channel setup */
static int max9286_init(struct max9286 *max, struct i2c_client *client)
{
	int i, rval;
	unsigned int val, lval;
	u8 mval, slval, tmval;

	usleep_range(10000, 11000);

	rval = regmap_read(max->regmap8, DS_MAX9286_DEVID, &val);
	if (rval) {
		dev_err(max->v4l2_sd.dev,
			"Failed to read device ID of MAX9286!\n");
		return rval;
	}
	dev_info(max->v4l2_sd.dev, "MAX9286 device ID: 0x%X\n", val);

	rval = regmap_write(max->regmap8, DS_CSI_VC_CTL, 0x93);
	if (rval) {
		dev_err(max->v4l2_sd.dev, "Failed to disable CSI output!\n");
		return rval;
	}
	/* All the links are working in Legacy reverse control-channel mode */
	/* Enable Custom Reverse Channel and First Pulse Length */
	rval = regmap_write(max->regmap8, DS_ENCRC_FPL, 0x4F);
	if (rval) {
		dev_err(max->v4l2_sd.dev, "Failed to disable PRBS test!\n");
		return rval;
	}
	/*
	 * 2ms of delay is required after any analog change to reverse control
	 * channel for bus timeout and I2C state machine to settle from any
	 * glitches
	 */
	usleep_range(2000, 3000);
	/* First pulse length rise time changed from 300ns to 200ns */
	rval = regmap_write(max->regmap8, DS_FPL_RT, 0x1E);
	if (rval) {
		dev_err(max->v4l2_sd.dev, "Failed to disable PRBS test!\n");
		return rval;
	}
	usleep_range(2000, 3000);

	/* Enable configuration links */
	max96705_write_register(max, 0, S_MAIN_CTL, 0x43);
	usleep_range(5000, 6000);

	/*
	 * Enable high threshold for reverse channel input buffer
	 * This increases immunity to power supply noise when the
	 * coaxial link is used for power as well as signal
	 */
	max96705_write_register(max, 0, S_RSVD_8, 0x01);
	/* Enable change of reverse control parameters */

	max96705_write_register(max, 0, S_RSVD_97, 0x5F);

	/* Wait 2ms after any change to reverse control channel */
	usleep_range(2000, 3000);

	/* Increase reverse amplitude from 100mV to 170mV to compensate for
	 * higher threshold
	 */
	rval = regmap_write(max->regmap8, DS_FPL_RT, 0x19);
	if (rval) {
		dev_err(max->v4l2_sd.dev, "Failed to disable PRBS test!\n");
		return rval;
	}
	usleep_range(2000, 3000);

	/*
	 * Enable CSI-2 lanes D0, D1, D2, D3
	 * Enable CSI-2 DBL (Double Input Mode)
	 * Enable GMSL DBL for RAWx2
	 * Enable YUV422 8-bit data type
	 */
	rval = regmap_write(max->regmap8, DS_CSI_DBL_DT, 0xF7);
	if (rval) {
		dev_err(max->v4l2_sd.dev, "Failed to disable PRBS test!\n");
		return rval;
	}
	usleep_range(2000, 3000);
	/* Enable Frame sync Auto-mode for row/column reset on frame sync
	 * sensors
	 */
	rval = regmap_write(max->regmap8, DS_FSYNCMODE, 0x00);
	if (rval) {
		dev_err(max->v4l2_sd.dev, "Failed to disable PRBS test!\n");
		return rval;
	}
	usleep_range(2000, 3000);
	rval = regmap_write(max->regmap8, DS_OVERLAP_WIN_LOW, 0x00);
	rval = regmap_write(max->regmap8, DS_OVERLAP_WIN_HIGH, 0x00);

	rval = regmap_write(max->regmap8, DS_FSYNC_PERIOD_LOW, 0x55);
	rval = regmap_write(max->regmap8, DS_FSYNC_PERIOD_MIDDLE, 0xc2);
	rval = regmap_write(max->regmap8, DS_FSYNC_PERIOD_HIGH, 0x2C);

	for (i = 0; i < ARRAY_SIZE(max9286_byte_order_settings); i++) {
		rval = max96705_write_register(max, 0,
				max9286_byte_order_settings[i].reg,
				max9286_byte_order_settings[i].val);
		if (rval) {
			dev_err(max->v4l2_sd.dev,
				"Failed to set max9286 byte order\n");
			return rval;
		}
	}

	/* Detect video links */
	rval = regmap_read(max->regmap8, DS_CONFIGL_VIDEOL_DET, &lval);
	if (rval) {
		dev_err(max->v4l2_sd.dev, "Failed to read register 0x49!\n");
		return rval;
	}

	/*
	 * Check on which links the sensors are connected
	 * And also check total number of sensors connected to the deserializer
	 */
	max->sensor_present = ((lval >> 4) & 0xF) | (lval & 0xF);

	for (i = 0; i < NR_OF_MAX_STREAMS; i++) {
		if (max->sensor_present & (0x1 << i)) {
			dev_info(max->v4l2_sd.dev,
				"Sensor present on deserializer link %d\n", i);
			max->total_sensor_num += 1;
		}
	}

	dev_info(max->v4l2_sd.dev,
		"total sensor present = %d", max->total_sensor_num);
	dev_info(max->v4l2_sd.dev,
		"sensor present on links = %d", max->sensor_present);

	if (!max->total_sensor_num) {
		dev_err(max->v4l2_sd.dev, "No sensors connected!\n");
	} else {
		dev_info(max->v4l2_sd.dev,
			"Total number of sensors connected = %d\n",
			max->total_sensor_num);
	}

	slval = get_output_link_order(max);

	/* Set link output order */
	rval = regmap_write(max->regmap8, DS_LINK_OUTORD, slval);
	if (rval) {
		dev_err(max->v4l2_sd.dev,
			"Failed to set Link output order!\n");
		return rval;
	}

	slval = 0xE0 | max->sensor_present;

	/*
	 * Enable DBL
	 * Edge select: Rising Edge
	 * Enable HS/VS encoding
	 */
	max96705_write_register(max, 0, S_CONFIG, 0x94);
	usleep_range(2000, 3000);

	mval = 0;
	tmval = 0;
	/*
	 * Setup each serializer individually and their respective I2C slave
	 * address changed to a unique value by enabling one reverse channel
	 * at a time via deserializer's DS_FWDCCEN_REVCCEN control register.
	 * Also create broadcast slave address for MAX96705 serializer.
	 * After this stage, i2cdetect on I2C-ADAPTER should display the
	 * below devices
	 * 10: Sensor address
	 * 11, 12, 13, 14: Sensors alias addresses
	 * 41, 42, 43, 44: Serializers alias addresses
	 * 45: Serializer's broadcast address
	 * 48: Deserializer's address
	 */

	for (i = 1; i <= NR_OF_MAX_SINK_PADS; i++) {
		/* Setup the link when the sensor is connected to the link */
		if (((0x1 << (i - 1)) &  max->sensor_present) == 0)
			continue;

		/* Enable only one reverse channel at a time */
		mval = (0x11 << (i - 1));
		tmval |= (0x11 << (i - 1));
		rval = regmap_write(max->regmap8, DS_FWDCCEN_REVCCEN, mval);
		if (rval) {
			dev_err(max->v4l2_sd.dev,
				"Failed to enable channel for %d!\n", i);
			return rval;
		}
		/* Wait 2ms after enabling reverse channel */
		usleep_range(2000, 3000);

		/* Change Serializer slave address */
		max96705_write_register(max, 0, S_SERADDR,
			(S_ADDR_MAX96705 + i) << 1);
		/* Unique link 'i' image sensor slave address */
		max96705_write_register(max, i, S_I2C_SOURCE_IS,
			(ADDR_AR0231AT_SENSOR + i) << 1);
		/* Link 'i' image sensor slave address */
		max96705_write_register(max, i, S_I2C_DST_IS,
			ADDR_AR0231AT_SENSOR << 1);
		/* Serializer broadcast address */
		max96705_write_register(max, i, S_I2C_SOURCE_SER,
			S_ADDR_MAX96705_BROADCAST << 1);
		/* Link 'i' serializer address */
		max96705_write_register(max, i, S_I2C_DST_SER,
			(S_ADDR_MAX96705 + i) << 1);
	}

	/* Enable I2c reverse channels */
	rval = regmap_write(max->regmap8, DS_FWDCCEN_REVCCEN, tmval);
	if (rval) {
		dev_err(max->v4l2_sd.dev,
			"Failed to enable channel for %d!\n", i);
		return rval;
	}
	usleep_range(2000, 3000);

	return 0;
}

/* Unbind the MAX9286 device driver from the I2C client */
static int max9286_remove(struct i2c_client *client)
{
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct max9286 *max = to_max_9286(subdev);
	int i;

	mutex_destroy(&max->max_mutex);
	v4l2_ctrl_handler_free(&max->ctrl_handler);
	v4l2_device_unregister_subdev(&max->v4l2_sd);
	media_entity_cleanup(&max->v4l2_sd.entity);

	for (i = 0; i < NR_OF_MAX_SINK_PADS; i++) {
		if (max->sub_devs[i]) {
			struct i2c_client *sub_client =
				v4l2_get_subdevdata(max->sub_devs[i]);

			i2c_unregister_device(sub_client);
		}
		max->sub_devs[i] = NULL;
	}

	return 0;
}

/* Called by I2C probe */
static int max9286_probe(struct i2c_client *client,
	const struct i2c_device_id *devid)
{
	struct max9286 *max;
	int i = 0;
	int rval = 0;

	if (client->dev.platform_data == NULL)
		return -ENODEV;

	max = devm_kzalloc(&client->dev, sizeof(*max), GFP_KERNEL);
	if (!max)
		return -ENOMEM;

	max->pdata = client->dev.platform_data;

	max->nsources = NR_OF_MAX_SOURCE_PADS;
	max->nsinks = NR_OF_MAX_SINK_PADS;
	max->npads = NR_OF_MAX_PADS;
	max->nstreams = NR_OF_MAX_STREAMS;

	max->crop = devm_kcalloc(&client->dev, max->npads,
		sizeof(struct v4l2_rect), GFP_KERNEL);
	max->compose = devm_kcalloc(&client->dev, max->npads,
		sizeof(struct v4l2_rect), GFP_KERNEL);
	max->route = devm_kcalloc(&client->dev, max->nstreams,
		sizeof(*max->route), GFP_KERNEL);
	max->stream = devm_kcalloc(&client->dev, max->npads,
		sizeof(*max->stream), GFP_KERNEL);

	if (!max->crop || !max->compose || !max->route || !max->stream)
		return -ENOMEM;

	for (i = 0; i < max->npads; i++) {
		max->ffmts[i] =
			devm_kcalloc(&client->dev, max->nstreams,
				sizeof(struct v4l2_mbus_framefmt), GFP_KERNEL);
		if (!max->ffmts[i])
			return -ENOMEM;

		max->stream[i].stream_id =
			devm_kcalloc(&client->dev, max->nsinks,
				sizeof(int), GFP_KERNEL);
		if (!max->stream[i].stream_id)
			return -ENOMEM;
	}

	for (i = 0; i < max->nstreams; i++) {
		max->route[i].sink = i;
		max->route[i].source = MAX_PAD_SOURCE;
		max->route[i].flags = 0;
	}

	for (i = 0; i < max->nsinks; i++) {
		max->stream[i].stream_id[0] = i;
		max->stream[MAX_PAD_SOURCE].stream_id[i] = i;
	}

	max->regmap8 = devm_regmap_init_i2c(client, &max9286_reg_config8);
	if (IS_ERR(max->regmap8)) {
		dev_err(&client->dev, "Failed to init regmap8!\n");
		return -EIO;
	}

	mutex_init(&max->max_mutex);

	v4l2_i2c_subdev_init(&max->v4l2_sd, client, &max9286_sd_ops);

	rval = max9286_register_subdev(max);
	if (rval) {
		dev_err(&client->dev,
			"Failed to register MAX9286 subdevice!\n");
		goto error_mutex_destroy;
	}

	rval = max9286_init(max, client);
	if (rval) {
		dev_err(&client->dev, "Failed to initialise MAX9286!\n");
		goto error_media_entity;
	}

	return 0;

error_media_entity:
	media_entity_cleanup(&max->v4l2_sd.entity);
	v4l2_ctrl_handler_free(&max->ctrl_handler);
error_mutex_destroy:
	mutex_destroy(&max->max_mutex);

	return rval;
}

#ifdef CONFIG_PM
static int max9286_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct max9286 *max = to_max_9286(subdev);

	return max9286_init(max, client);
}
#else
#define max9286_resume	NULL
#endif /* CONFIG_PM */

static const struct i2c_device_id max9286_id_table[] = {
	{ MAX9286_NAME, 0 },
	{},
};

static const struct dev_pm_ops max9286_pm_ops = {
	.resume = max9286_resume,
};

static struct i2c_driver max9286_i2c_driver = {
	.driver = {
		.name = MAX9286_NAME,
		.pm = &max9286_pm_ops,
	},
	.probe = max9286_probe,
	.remove = max9286_remove,
	.id_table = max9286_id_table,
};

module_i2c_driver(max9286_i2c_driver);

MODULE_AUTHOR("Kiran Kumar <kiran2.kumar@intel.com>");
MODULE_AUTHOR("Kun Jiang <kun.jiang@intel.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Maxim96705 serializer and Maxim9286 deserializer driver");
