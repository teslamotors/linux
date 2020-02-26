/*
 * tc3587.c -- driver for the tc3587 that looks like the ti940 driver
 *
 * Copytight (c) 2018 Codethink Ltd.
 * Copyright (c) 2017,2018 Tesla Motors, Inc.
 *
 * Based on ti964.c and ti940.c
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
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>

#include <linux/regulator/consumer.h>

#include <media/media-device.h>
#include <media/media-entity.h>
#include <media/ti940.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-core.h>
#include <media/crlmodule.h>

#define GMSL_LOCK_GPIO_DEFAULT 290
#define TC3587_NAME "tc3587"
#define TC3587_SYSCTL 0x0002
#define TC3587_SYSCTL_MASK 0x03


struct tc3587_stream {
	unsigned int *stream_id;
};

struct tc3587_route {
	unsigned int sink;
	unsigned int source;
	int flags;
};

struct tc3587 {
	struct v4l2_subdev sd;
	struct media_pad pad[NR_OF_VA_PADS];
	struct v4l2_ctrl_handler ctrl_handler;
	struct ti940_pdata *pdata;
	struct v4l2_subdev *sub_devs[NR_OF_VA_SINK_PADS];
	struct i2c_client *client;
	const char *name;

	struct mutex mutex;

	struct v4l2_mbus_framefmt *ffmts[NR_OF_VA_PADS];
	struct v4l2_rect crop[NR_OF_VA_PADS];
	struct v4l2_rect compose[NR_OF_VA_PADS];
	/* stream enable/disable status, indexed by pad */
	struct tc3587_stream stream[NR_OF_VA_STREAMS+1];
	struct tc3587_route route[NR_OF_VA_STREAMS+1];

	struct regulator *vcc_cam;
	struct regulator *vcc_csi;
	struct regulator *vcc;
  
	unsigned short nsinks;
	unsigned short nsources;
	unsigned short nstreams;
	unsigned short npads;

	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *test_pattern;

	unsigned int gpio_lock;
	int gpio_lock_irq;
};

#define to_tc3587(_sd) container_of(_sd, struct tc3587, sd)

/*
 * Order matters.
 *
 * 1. Bits-per-pixel, descending.
 * 2. Bits-per-pixel compressed, descending.
 * 3. Pixel order, same as in pixel_order_str. Formats for all four pixel
 *    orders must be defined.
 */
static const struct ti940_csi_data_format va_csi_data_formats[] = {
	{ MEDIA_BUS_FMT_YUYV8_1X16, 16, 16, PIXEL_ORDER_GBRG, 0x1e },
	{ MEDIA_BUS_FMT_RGB888_1X24, 24, 24, PIXEL_ORDER_GRBG, 0x1e },
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

static const uint32_t tc3587_supported_codes_pad[] = {
	MEDIA_BUS_FMT_YUYV8_1X16,
	MEDIA_BUS_FMT_RGB888_1X24,
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

#define TC_REG_32BIT	(1 << 30)

static int tc3587_wr_reg(struct tc3587 *tc, unsigned reg, uint32_t val)
{
	uint8_t data[6];
	int len = (reg & TC_REG_32BIT) ? 6 : 4;
	int rc;

	/* first part is register address */
	data[0] = reg >> 8;
	data[1] = reg & 0xff;

	data[2] = val >> 8;
	data[3] = val & 0xff;
	if (reg & TC_REG_32BIT) {
		data[4] = val >> 24;
		data[5] = val >> 16;
	} else
		WARN_ON(val > 0xffff);

	dev_info(tc->sd.dev, "%s: wr reg %04x (%d bit) with %08x\n",
		 __func__, reg & ~(0xff000000),
		 (reg & TC_REG_32BIT) ? 32: 16, val);

	rc = i2c_master_send(tc->client, data, len);
	if (unlikely(rc < 0 || rc != len)) {
		dev_err(tc->sd.dev,
			"%s: wr reg %04x (%dbit) with %08x (rc=%d)\n",
			__func__, reg & ~(0xff000000),
			(reg & TC_REG_32BIT) ? 32: 16, val, rc);
		return -1;
	}
	return 0;
}

static int tc3587_rd_reg(struct tc3587 *tc, unsigned reg, uint32_t *val)
{
	struct i2c_msg msgs[2];
	uint32_t ret = 0;
	uint8_t txb[2];
	uint8_t rxb[4];
	int rc;

	msgs[0].addr = tc->client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = txb;
	msgs[1].addr = tc->client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = (reg & TC_REG_32BIT) ? 4 : 2;
	msgs[1].buf = rxb;

	txb[0] = reg >> 8;
	txb[1] = reg & 0xff;

	rc = i2c_transfer(tc->client->adapter, msgs, ARRAY_SIZE(msgs));
	if (rc < 0)
		return rc;
	if (rc != 2)
		return -EIO;

	ret = ((u32)rxb[0]) << 8;
	ret |= ((u32)rxb[1]);

	if (reg & TC_REG_32BIT) {
		ret |= ((u32)rxb[2]) << 24;
		ret |= ((u32)rxb[3]) << 16;
	}

	*val = ret;
	return 0;
}

static const uint32_t *tc3587_supported_codes[] = {
	tc3587_supported_codes_pad,
};

/* removed tc3587_reg_set_bit as not used */

static int tc3587_enum_mbus_code(struct v4l2_subdev *sd,
				      struct v4l2_subdev_pad_config *cfg,
				      struct v4l2_subdev_mbus_code_enum *code)
{
	struct tc3587 *va = to_tc3587(sd);
	const uint32_t *supported_code =
		tc3587_supported_codes[code->pad];
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
		*tc3587_validate_csi_data_format(u32 code)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(va_csi_data_formats); i++) {
		if (va_csi_data_formats[i].code == code)
			return &va_csi_data_formats[i];
	}

	return &va_csi_data_formats[0];
}

static int tc3587_get_frame_desc(struct v4l2_subdev *sd,
	unsigned int pad, struct v4l2_mbus_frame_desc *desc)
{
	struct tc3587 *va = to_tc3587(sd);
	struct v4l2_mbus_frame_desc_entry *entry = desc->entry;
	u8 vc = 0;
	int i;

	desc->type = V4L2_MBUS_FRAME_DESC_TYPE_CSI2;

	for (i = 0; i < min(va->nstreams, (unsigned)desc->num_entries); i++) {
		struct v4l2_mbus_framefmt *ffmt =
			&va->ffmts[i][VA_PAD_SOURCE];
		const struct ti940_csi_data_format *csi_format =
			tc3587_validate_csi_data_format(ffmt->code);

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
__tc3587_get_ffmt(struct v4l2_subdev *subdev,
			 struct v4l2_subdev_pad_config *cfg,
			 unsigned int pad, unsigned int which,
			 unsigned int stream)
{
	struct tc3587 *va = to_tc3587(subdev);

	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_format(subdev, cfg, pad);
	else
		return &va->ffmts[pad][stream];
}

static int tc3587_get_format(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_format *fmt)
{
	struct tc3587 *va = to_tc3587(subdev);

	if (fmt->stream > va->nstreams)
		return -EINVAL;

	mutex_lock(&va->mutex);
	fmt->format = *__tc3587_get_ffmt(subdev, cfg, fmt->pad,
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

static int tc3587_set_format(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_format *fmt)
{
	struct tc3587 *va = to_tc3587(subdev);
	const struct ti940_csi_data_format *csi_format;
	struct v4l2_mbus_framefmt *ffmt;

	if (fmt->stream > va->nstreams)
		return -EINVAL;

	csi_format = tc3587_validate_csi_data_format(
		fmt->format.code);

	mutex_lock(&va->mutex);
	ffmt = __tc3587_get_ffmt(subdev, cfg, fmt->pad, fmt->which,
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

static int tc3587_open(struct v4l2_subdev *subdev,
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

static int tc3587_registered(struct v4l2_subdev *subdev)
{
	struct tc3587 *va = to_tc3587(subdev);
	int i, j, k, l, rval;

	dev_info(va->sd.dev, "pdata %p - subdev_num=%d\n",
		 va->pdata, va->pdata->subdev_num);

	for (i = 0, k = 0; i < va->pdata->subdev_num; i++) {
		struct ti940_subdev_i2c_info *info =
			&va->pdata->subdev_info[i];
		struct i2c_adapter *adapter;

		dev_info(va->sd.dev, "%s: i=%d, k=%d\n", __func__, i, k);

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

static int tc3587_set_power(struct v4l2_subdev *subdev, int on)
{
    return true;
}

static int tc3587_set_stream(struct v4l2_subdev *subdev, int enable)
{
	struct tc3587 *va = to_tc3587(subdev);
	int i, rval;

	dev_dbg(va->sd.dev, "TC3587 set stream. enable=%d\n", enable);

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
		bpp = tc3587_validate_csi_data_format(
			va->ffmts[i][0].code)->width;
	}

	return 0;
}

static struct v4l2_subdev_internal_ops tc3587_sd_internal_ops = {
	.open = tc3587_open,
	.registered = tc3587_registered,
};

static const struct v4l2_subdev_video_ops tc3587_sd_video_ops = {
	.s_stream = tc3587_set_stream,
};

static const struct v4l2_subdev_core_ops tc3587_core_subdev_ops = {
	.s_power = tc3587_set_power,
};

static int tc3587_s_ctrl(struct v4l2_ctrl *ctrl)
{
	return 0;
}

static const struct v4l2_ctrl_ops tc3587_ctrl_ops = {
	.s_ctrl = tc3587_s_ctrl,
};

/* this value should match the csi link output frequency (from the pll) */
static const s64 tc3587_op_sys_clock[] =  {373000000, };

static const struct v4l2_ctrl_config tc3587_controls[] = {
	{
		.ops = &tc3587_ctrl_ops,
		.id = V4L2_CID_LINK_FREQ,
		.name = "V4L2_CID_LINK_FREQ",
		.type = V4L2_CTRL_TYPE_INTEGER_MENU,
		.max = ARRAY_SIZE(tc3587_op_sys_clock) - 1,
		.min =  0,
		.step  = 0,
		.def = 0,
		.qmenu_int = tc3587_op_sys_clock,
	},
	{
		.ops = &tc3587_ctrl_ops,
		.id = V4L2_CID_TEST_PATTERN,
		.name = "V4L2_CID_TEST_PATTERN",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.max = 1,
		.min =	0,
		.step  = 1,
		.def = 0,
	},
};

static const struct v4l2_subdev_pad_ops tc3587_sd_pad_ops = {
	.get_fmt = tc3587_get_format,
	.set_fmt = tc3587_set_format,
	.get_frame_desc = tc3587_get_frame_desc,
	.enum_mbus_code = tc3587_enum_mbus_code,
};

static struct v4l2_subdev_ops tc3587_sd_ops = {
	.core = &tc3587_core_subdev_ops,
	.video = &tc3587_sd_video_ops,
	.pad = &tc3587_sd_pad_ops,
};

static int tc3587_register_subdev(struct tc3587 *va)
{
	int i, rval;
	struct i2c_client *client = v4l2_get_subdevdata(&va->sd);

	v4l2_subdev_init(&va->sd, &tc3587_sd_ops);
	snprintf(va->sd.name, sizeof(va->sd.name), "TC3587 0x%x",
		 client->addr);

	va->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	va->sd.internal_ops = &tc3587_sd_internal_ops;

	v4l2_set_subdevdata(&va->sd, client);

	v4l2_ctrl_handler_init(&va->ctrl_handler,
				ARRAY_SIZE(tc3587_controls));

	if (va->ctrl_handler.error) {
		dev_err(va->sd.dev,
			"Failed to init tc3587 controls. ERR: %d!\n",
			va->ctrl_handler.error);
		return va->ctrl_handler.error;
	}

	va->sd.ctrl_handler = &va->ctrl_handler;

	for (i = 0; i < ARRAY_SIZE(tc3587_controls); i++) {
		const struct v4l2_ctrl_config *cfg =
			&tc3587_controls[i];
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
			"Failed to init media entity for tc3587!\n");
		goto failed_out;
	}

	return 0;

failed_out:
	v4l2_ctrl_handler_free(&va->ctrl_handler);
	return rval;
}

#define TC_REG16(__reg) (__reg)
#define TC_REG32(__reg) ((__reg) | TC_REG_32BIT)

struct tc_init_reg {
	uint32_t	reg;
	uint32_t	val;
};

/* initialisation sequence for 2 lane */
static struct tc_init_reg tc_init_data2[] = {
	{ .reg = TC_REG16(0x0004), .val = 0x0004, },
	{ .reg = TC_REG16(0x0002), .val = 0x0001, },
	{ .reg = TC_REG16(0x0002), .val = 0x0000, },
	{ .reg = TC_REG16(0x0016), .val = 0x205F, },
	{ .reg = TC_REG16(0x0018), .val = 0x0213, },
	{ .reg = TC_REG16(0x0006), .val = 0x0120, },
	{ .reg = TC_REG16(0x0008), .val = 0x0060, },
	{ .reg = TC_REG16(0x0022), .val = 0x0910, },
	{ .reg = TC_REG32(0x0140), .val = 0x00000000, },
	{ .reg = TC_REG32(0x0144), .val = 0x00000000, },
	{ .reg = TC_REG32(0x0148), .val = 0x00000000, },
	{ .reg = TC_REG32(0x014C), .val = 0x00000001, },
	{ .reg = TC_REG32(0x0150), .val = 0x00000001, },
	{ .reg = TC_REG32(0x0210), .val = 0x00001D00, },
	{ .reg = TC_REG32(0x0214), .val = 0x00000003, },
	{ .reg = TC_REG32(0x0218), .val = 0x00002004, },
	{ .reg = TC_REG32(0x021C), .val = 0x00000002, },
	{ .reg = TC_REG32(0x0220), .val = 0x00000103, },
	{ .reg = TC_REG32(0x0224), .val = 0x00004988, },
	{ .reg = TC_REG32(0x0228), .val = 0x00000009, },
	{ .reg = TC_REG32(0x022C), .val = 0x00000002, },
	{ .reg = TC_REG32(0x0234), .val = 0x00000007, }, 
#if 1
	/* turn off the continuous clock mode */
	{ .reg = TC_REG32(0x0238), .val = 0x00000000, },
#else
	{ .reg = TC_REG32(0x0238), .val = 0x00000001, },
#endif
	{ .reg = TC_REG32(0x0204), .val = 0x00000001, },
	{ .reg = TC_REG32(0x0518), .val = 0x00000001, },
	{ .reg = TC_REG32(0x0500), .val = 0xA3008083, },
	{ .reg = TC_REG16(0x0004), .val = 0x0065, },
};

/* Set 373MHz, 4 lane configuration */
static struct tc_init_reg tc_init_data4[] = {
	{ .reg = TC_REG16(0x0004), .val = 0x0004, },	/* disable bridge */
	{ .reg = TC_REG16(0x0002), .val = 0x0001, },	/* reset device */
	{ .reg = TC_REG16(0x0002), .val = 0x0000, },	/* clear reset */
	{ .reg = TC_REG16(0x0016), .val = 0x208B, },	/* PLL config 1 */
	{ .reg = TC_REG16(0x0018), .val = 0x0213, },	/* PLL config 2 */
	{ .reg = TC_REG16(0x0006), .val = 0x01F4, },	/* FIFO config */
	{ .reg = TC_REG16(0x0008), .val = 0x0060, },	/* something? */
	{ .reg = TC_REG16(0x0022), .val = 0x0910, },	/* Set width */
	{ .reg = TC_REG16(0x0050), .val = 0x001E, },	/* output fmt ID */
	/* set the lane bypass for all the csi lanes */
	{ .reg = TC_REG32(0x0140), .val = 0x0000, },	/* unbypass clk */
	{ .reg = TC_REG32(0x0144), .val = 0x0000, },	/* unbypass d0 */
	{ .reg = TC_REG32(0x0148), .val = 0x0000, },	/* unbypass d1 */
	{ .reg = TC_REG32(0x014C), .val = 0x0000, },	/* unbypass d2 */
	{ .reg = TC_REG32(0x0150), .val = 0x0000, },	/* unbypass d3 */
	{ .reg = TC_REG32(0x0210), .val = 0x2600, },	/* line init wait */
	{ .reg = TC_REG32(0x0214), .val = 0x0004, },	/* syslptx */
	{ .reg = TC_REG32(0x0218), .val = 0x1A04, },	/* tclk counters */
	{ .reg = TC_REG32(0x021C), .val = 0x0002, },	/* tclk counters */
	{ .reg = TC_REG32(0x0220), .val = 0x0505, },	/* tclk counters */
	{ .reg = TC_REG32(0x0224), .val = 0x4988, },	/* ths counter  */
	{ .reg = TC_REG32(0x0228), .val = 0x0009, },	/* tclk post */
	{ .reg = TC_REG32(0x022C), .val = 0x0003, },	/* ths trail */
	{ .reg = TC_REG32(0x0234), .val = 0x001f, },	/* all lanes vreg on */
#if 1
	{ .reg = TC_REG32(0x0238), .val = 0x0000, },	/* cont. clock off */
#else
	{ .reg = TC_REG32(0x0238), .val = 0x0001, },	/* cont. clock */
#endif
	{ .reg = TC_REG32(0x0204), .val = 0x0001, },	/* start tx */
	{ .reg = TC_REG32(0x0518), .val = 0x0001, },	/* start CSI */
	{ .reg = TC_REG32(0x0500), .val = 0xA3008087, }, /* set to 4 lanes */
	{ .reg = TC_REG16(0x0004), .val = 0x67, },	/* input enable */
};

static irqreturn_t gmsllock_irq_handler(int irq, void *data)
{
	struct tc3587 *va = (struct tc3587 *) data;
	dev_info(va->sd.dev, "GMSL-LOCK status changed: %d\n",
		gpio_get_value(va->gpio_lock));
	return IRQ_HANDLED;
}

static int cam_irq_set(struct device *dev, struct tc3587 *va)
{
	int rval = 0;

	rval = devm_gpio_request(dev, va->gpio_lock, "gmsl-lock");
	if (rval) {
		dev_err(va->sd.dev, "gpio_request for GPIO-%d err %d\n",
			va->gpio_lock, rval);
		return rval;
	}

	rval = gpio_direction_input(va->gpio_lock);
	if (rval) {
		dev_err(va->sd.dev, "gpio_direction_input for GPIO-%d err %d\n",
			va->gpio_lock, rval);
		return rval;
	}

	dev_info(va->sd.dev, "GPIO-%d (GMSL-LOCK) read back = %d\n",
		va->gpio_lock, gpio_get_value(va->gpio_lock));

	va->gpio_lock_irq = gpio_to_irq(va->gpio_lock);
	if (va->gpio_lock_irq < 0) {
		dev_err(va->sd.dev, "gpio_to_irq for GPIO-%d err %d\n",
			va->gpio_lock, va->gpio_lock_irq);
		return -EINVAL;
	}

	dev_info(va->sd.dev, "GPIO-%d (GMSL-LOCK) is mapped to IRQ-%d\n",
		va->gpio_lock, va->gpio_lock_irq);

	rval = devm_request_threaded_irq(dev, va->gpio_lock_irq,
					NULL,
					gmsllock_irq_handler,
					IRQF_TRIGGER_RISING  |
					IRQF_TRIGGER_FALLING |
					IRQF_ONESHOT,
					"tc3587-gmsl-lock",
					va);
	if (rval < 0)
		dev_err(va->sd.dev, "request_irq for IRQ-%d err %d\n",
			va->gpio_lock_irq, rval);

	return rval;
}


static int tc3587_init(struct tc3587 *va)
{
	struct crlmodule_platform_data *crl = va->client->dev.platform_data;
	struct tc_init_reg *reg;
	int i, rval;
	int initsize = 0;
	uint32_t val = 0;
	const int retries = 100;

	if (!crl) {
		dev_err(va->sd.dev, "no platform data\n");
		return -EINVAL;
	}

	for (i = 0; i <= retries; ++i) {
		rval = tc3587_rd_reg(va, 0x0000, &val);
		if (!rval)
			break;
		dev_err(va->sd.dev, "Failed attempt %d of %d to read device ID of TC3587!\n",
			i, retries);
		if (i == retries)
			return rval;
		msleep(250);
	}

	dev_info(va->sd.dev, "TC3587 device ID: 0x%X (%d lane)\n", val, crl->lanes);

	switch (crl->lanes) {
	case 4:
		initsize = ARRAY_SIZE(tc_init_data4);
		reg = tc_init_data4;
		break;
	case 2:
		initsize = ARRAY_SIZE(tc_init_data2);
		reg = tc_init_data2;
		break;
	default:
		dev_err(va->sd.dev, "No configuration for %d lanes\n", crl->lanes);
		return -EINVAL;
	}

	for (i = 0; i < initsize; i++, reg++) {
		rval = tc3587_wr_reg(va, reg->reg, reg->val);
		if (rval) {
			dev_err(va->sd.dev, "Failed to write reg %08x (%d)\n",
				reg->reg, rval);
			return rval < 0 ? rval : -1;
		}
		if (reg->reg == 0x2)
			msleep(50);
	}

	return 0;
}

static void cam_gpio_set(struct device *dev, unsigned gpio, unsigned val)
{
	int claim_rc, rc;

	dev_info(dev, "setting gpio %d to %d\n", gpio, val);
	claim_rc = gpio_request(gpio, "camera");
	if (claim_rc)
		dev_err(dev, "gpio_request(%d,%d) err %d\n", gpio, val, rc);
	rc = gpio_direction_output(gpio, val);
	if (rc)
		dev_err(dev, "gpio_direction_output(%d,%d) err %d\n", gpio, val, rc);
	dev_info(dev, "read back %d = %d\n", gpio, gpio_get_value(gpio));
	if (!claim_rc)
		gpio_free(gpio);
}

static void init_camera_power(struct device *dev, struct tc3587 *ti)
{
	int ret;

	ret = regulator_enable(ti->vcc_cam);
	WARN_ON(ret);
	ret = regulator_enable(ti->vcc_csi);
	WARN_ON(ret);
	ret = regulator_enable(ti->vcc);
	WARN_ON(ret);
	msleep(500);

	cam_gpio_set(dev, 365, 0);	/* GMSL DeSer off (also reset active)*/
	msleep(500);			/* basic power stabilisation */
	cam_gpio_set(dev, 365, 1);	/* GMSL DeSer on */
	cam_gpio_set(dev, 460, 0);	/* enable GMSL DeSer */
	msleep(500);			/* wait for GMSL ok */
}

static void tc3587_remove_cam_power(struct device *dev, struct tc3587 *ti)
{
	cam_gpio_set(dev, 365, 0);	/* GMSL DeSer off (also reset active)*/

	regulator_disable(ti->vcc_cam);
	regulator_disable(ti->vcc_csi);
	regulator_disable(ti->vcc);
}

static ssize_t tc3587_hack_store_reinit(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct tc3587 *ti = dev_get_drvdata(dev);
	int rc;

	dev_info(dev, "issuing init sequence\n");
	rc = tc3587_init(ti);
	return (rc < 0) ? rc : count;
}

static DEVICE_ATTR(reinit, 0200, NULL, tc3587_hack_store_reinit);

static ssize_t tc3587_show_fifostate(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct tc3587 *ti = dev_get_drvdata(dev);
	unsigned int val;
	int rval;

	rval = tc3587_rd_reg(ti, 0x00F8, &val);
	if (rval < 0)
		return rval;
	return scnprintf(buf, PAGE_SIZE, "0x%04x\n", val);
}

static DEVICE_ATTR(fifostate, 0400, tc3587_show_fifostate, NULL);

static ssize_t tc3587_show_gmsllockstate(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct tc3587 *tc = dev_get_drvdata(dev);

	if (tc->gpio_lock_irq < 0)
		return -1;

	return scnprintf(buf, PAGE_SIZE, "%d\n", gpio_get_value(tc->gpio_lock));
}

static DEVICE_ATTR(gmslstate, 0400, tc3587_show_gmsllockstate, NULL);

static ssize_t tc3587_show_syscntl(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct tc3587 *tc = dev_get_drvdata(dev);
	unsigned int val;
	int rval;

	rval = tc3587_rd_reg(tc, TC3587_SYSCTL, &val);
	if (rval < 0)
		return rval;

	val &= TC3587_SYSCTL_MASK;

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static DEVICE_ATTR(syscntl, 0400, tc3587_show_syscntl, NULL);

static ssize_t tc3587_show_readystate(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct tc3587 *tc = dev_get_drvdata(dev);
	unsigned int val, ready = 0;
	int rval;

	rval = tc3587_rd_reg(tc, TC3587_SYSCTL, &val);
	if (rval < 0)
		return rval;

	val &= TC3587_SYSCTL_MASK;

	if (val == 0 && gpio_get_value(tc->gpio_lock) == 1)
		ready = 1;

	return scnprintf(buf, PAGE_SIZE, "%d\n", ready);
}

static DEVICE_ATTR(readystate, 0400, tc3587_show_readystate, NULL);


#define power true

static int tc3587_probe(struct i2c_client *client,
			const struct i2c_device_id *devid)
{
	struct tc3587 *va;
	int i, rval = 0;

	if (client->dev.platform_data == NULL)
		return -ENODEV;

	va = devm_kzalloc(&client->dev, sizeof(*va), GFP_KERNEL);
	if (!va)
		return -ENOMEM;

	dev_info(&client->dev, "probing device\n");

	va->vcc = devm_regulator_get(&client->dev, "vcc");
	if (IS_ERR(va->vcc)) {
		dev_err(&client->dev, "did not find vcc\n");
		return PTR_ERR(va->vcc);
	}

	va->vcc_cam = devm_regulator_get(&client->dev, "vcc_camera");
	if (IS_ERR(va->vcc)) {
		dev_err(&client->dev, "did not find vcc_camera\n");
		return PTR_ERR(va->vcc_cam);
	}

	va->vcc_csi = devm_regulator_get(&client->dev, "vcc_csi");
	if (IS_ERR(va->vcc)) {
		dev_err(&client->dev, "did not find vcc_cam\n");
		return PTR_ERR(va->vcc_cam);
	}

	if (power)
		init_camera_power(&client->dev, va);

	va->pdata = client->dev.platform_data;

	va->nsources = NR_OF_VA_SOURCE_PADS;
	va->nsinks = NR_OF_VA_SINK_PADS;
	va->npads = NR_OF_VA_PADS;
	va->nstreams = NR_OF_VA_STREAMS;

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

	va->client = client;

	mutex_init(&va->mutex);
	v4l2_i2c_subdev_init(&va->sd, client, &tc3587_sd_ops);
	rval = tc3587_register_subdev(va);
	if (rval) {
		dev_err(&client->dev, "Failed to register va subdevice!\n");
		return rval;
	}

	va->gpio_lock = GMSL_LOCK_GPIO_DEFAULT;
	rval = cam_irq_set(&client->dev, va);
	/* The IRQ is not a requirement for TC3587 operation. So if it fails,
	*  the probe() should continue anyways and ignore irq setup failure.
	*/
	if (rval < 0) {
		dev_err(&client->dev, "Failed to register interrupt for GPIO-%d\n",
			va->gpio_lock);
	}

	if (power)
		rval = tc3587_init(va);
	else
		rval = 0;
	if (rval) {
		dev_err(&client->dev, "Failed to init TC3587!\n");
		gpio_free(va->gpio_lock);
		return rval;
	}

	device_create_file(&client->dev, &dev_attr_reinit);
	device_create_file(&client->dev, &dev_attr_fifostate);
	device_create_file(&client->dev, &dev_attr_gmslstate);
	device_create_file(&client->dev, &dev_attr_syscntl);
	device_create_file(&client->dev, &dev_attr_readystate);

	dev_info(&client->dev, "probed fake-ish TC3587\n");
	return 0;
}

static int tc3587_remove(struct i2c_client *client)
{
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct tc3587 *va = to_tc3587(subdev);
	int i;

	device_remove_file(&client->dev, &dev_attr_reinit);
	device_remove_file(&client->dev, &dev_attr_fifostate);
	device_remove_file(&client->dev, &dev_attr_gmslstate);
	device_remove_file(&client->dev, &dev_attr_syscntl);
	device_remove_file(&client->dev, &dev_attr_readystate);

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

	if (power)
		tc3587_remove_cam_power(&client->dev, va);

	if (va->gpio_lock_irq >= 0) {
		disable_irq(va->gpio_lock_irq);
	}
	gpio_free(va->gpio_lock);

	return 0;
}

static const struct i2c_device_id tc3587_id_table[] = {
	{ TC3587_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, tc3587_id_table);

static struct i2c_driver tc3587_i2c_driver = {
	.driver = {
		.name = TC3587_NAME,
	},
	.probe	= tc3587_probe,
	.remove	= tc3587_remove,
	.id_table = tc3587_id_table,
};
module_i2c_driver(tc3587_i2c_driver);


MODULE_AUTHOR("Ben Dooks <ben.dooks@codethink.co.uk>");
MODULE_AUTHOR("Tianshu Qiu <tian.shu.qiu@intel.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TC3587 CSI2-Aggregator driver");
