// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 Intel Corporation

#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/version.h>

#include <media/media-device.h>
#include <media/media-entity.h>
#include <media/ti960.h>
#include <media/crlmodule.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-core.h>

#include "ti960-reg.h"
#include "ti953.h"

struct ti960_subdev {
	struct v4l2_subdev *sd;
	unsigned short rx_port;
	unsigned short fsin_gpio;
	unsigned short phy_i2c_addr;
	unsigned short alias_i2c_addr;
	unsigned short ser_i2c_addr;
	char sd_name[16];
};

struct ti960 {
	struct v4l2_subdev sd;
	struct media_pad pad[NR_OF_TI960_PADS];
	struct v4l2_ctrl_handler ctrl_handler;
	struct ti960_pdata *pdata;
	struct ti960_subdev sub_devs[NR_OF_TI960_SINK_PADS];
	struct crlmodule_platform_data subdev_pdata[NR_OF_TI960_SINK_PADS];
	const char *name;

	struct mutex mutex;

	struct regmap *regmap8;
	struct regmap *regmap16;

	struct v4l2_mbus_framefmt *ffmts[NR_OF_TI960_PADS];
	struct rect *crop;
	struct rect *compose;

	struct v4l2_subdev_route *ti960_route;

	unsigned int nsinks;
	unsigned int nsources;
	unsigned int nstreams;
	unsigned int npads;

	struct gpio_chip gc;

	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *test_pattern;
};

#define to_ti960(_sd) container_of(_sd, struct ti960, sd)

static const s64 ti960_op_sys_clock[] =  {400000000, 800000000};
static const u8 ti960_op_sys_clock_reg_val[] = {
	TI960_MIPI_800MBPS,
	TI960_MIPI_1600MBPS
};

/*
 * Order matters.
 *
 * 1. Bits-per-pixel, descending.
 * 2. Bits-per-pixel compressed, descending.
 * 3. Pixel order, same as in pixel_order_str. Formats for all four pixel
 *    orders must be defined.
 */
static const struct ti960_csi_data_format va_csi_data_formats[] = {
	{ MEDIA_BUS_FMT_YUYV8_1X16, 16, 16, PIXEL_ORDER_GBRG, 0x1e },
	{ MEDIA_BUS_FMT_UYVY8_1X16, 16, 16, PIXEL_ORDER_GBRG, 0x1e },
	{ MEDIA_BUS_FMT_SGRBG16_1X16, 16, 16, PIXEL_ORDER_GRBG, 0x2e },
	{ MEDIA_BUS_FMT_SRGGB16_1X16, 16, 16, PIXEL_ORDER_RGGB, 0x2e },
	{ MEDIA_BUS_FMT_SBGGR16_1X16, 16, 16, PIXEL_ORDER_BGGR, 0x2e },
	{ MEDIA_BUS_FMT_SGBRG16_1X16, 16, 16, PIXEL_ORDER_GBRG, 0x2e },
	{ MEDIA_BUS_FMT_SGRBG12_1X12, 12, 12, PIXEL_ORDER_GRBG, 0x2c },
	{ MEDIA_BUS_FMT_SRGGB12_1X12, 12, 12, PIXEL_ORDER_RGGB, 0x2c },
	{ MEDIA_BUS_FMT_SBGGR12_1X12, 12, 12, PIXEL_ORDER_BGGR, 0x2c },
	{ MEDIA_BUS_FMT_SGBRG12_1X12, 12, 12, PIXEL_ORDER_GBRG, 0x2c },
	{ MEDIA_BUS_FMT_SGRBG10_1X10, 10, 10, PIXEL_ORDER_GRBG, 0x2b },
	{ MEDIA_BUS_FMT_SRGGB10_1X10, 10, 10, PIXEL_ORDER_RGGB, 0x2b },
	{ MEDIA_BUS_FMT_SBGGR10_1X10, 10, 10, PIXEL_ORDER_BGGR, 0x2b },
	{ MEDIA_BUS_FMT_SGBRG10_1X10, 10, 10, PIXEL_ORDER_GBRG, 0x2b },
};

static const uint32_t ti960_supported_codes_pad[] = {
	MEDIA_BUS_FMT_YUYV8_1X16,
	MEDIA_BUS_FMT_UYVY8_1X16,
	MEDIA_BUS_FMT_SBGGR16_1X16,
	MEDIA_BUS_FMT_SGBRG16_1X16,
	MEDIA_BUS_FMT_SGRBG16_1X16,
	MEDIA_BUS_FMT_SRGGB16_1X16,
	MEDIA_BUS_FMT_SBGGR12_1X12,
	MEDIA_BUS_FMT_SGBRG12_1X12,
	MEDIA_BUS_FMT_SGRBG12_1X12,
	MEDIA_BUS_FMT_SRGGB12_1X12,
	MEDIA_BUS_FMT_SBGGR10_1X10,
	MEDIA_BUS_FMT_SGBRG10_1X10,
	MEDIA_BUS_FMT_SGRBG10_1X10,
	MEDIA_BUS_FMT_SRGGB10_1X10,
	0,
};

static const uint32_t *ti960_supported_codes[] = {
	ti960_supported_codes_pad,
};

static struct regmap_config ti960_reg_config8 = {
	.reg_bits = 8,
	.val_bits = 8,
};

static struct regmap_config ti960_reg_config16 = {
	.reg_bits = 16,
	.val_bits = 8,
	.reg_format_endian = REGMAP_ENDIAN_BIG,
};

static int ti960_reg_read(struct ti960 *va, unsigned char reg, unsigned int *val)
{
	int ret, retry, timeout = 10;

	for (retry = 0; retry < timeout; retry++) {
		ret = regmap_read(va->regmap8, reg, val);
		if (ret < 0) {
			dev_err(va->sd.dev, "960 reg read ret=%x", ret);
			usleep_range(5000, 6000);
		} else {
			break;
		}
	}

	if (retry >= timeout) {
		dev_err(va->sd.dev,
			"%s:devid read failed: reg=%2x, ret=%d\n",
			__func__, reg, ret);
		return -EREMOTEIO;
	}

	return 0;
}

static int ti960_reg_set_bit(struct ti960 *va, unsigned char reg,
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

static int ti960_map_phy_i2c_addr(struct ti960 *va, unsigned short rx_port,
			      unsigned short addr)
{
	int rval;

	rval = regmap_write(va->regmap8, TI960_RX_PORT_SEL,
		(rx_port << 4) + (1 << rx_port));
	if (rval)
		return rval;

	return regmap_write(va->regmap8, TI960_SLAVE_ID0, addr);
}

static int ti960_map_alias_i2c_addr(struct ti960 *va, unsigned short rx_port,
			      unsigned short addr)
{
	int rval;

	rval = regmap_write(va->regmap8, TI960_RX_PORT_SEL,
		(rx_port << 4) + (1 << rx_port));
	if (rval)
		return rval;

	return regmap_write(va->regmap8, TI960_SLAVE_ALIAS_ID0, addr);
}

static int ti960_map_ser_alias_addr(struct ti960 *va, unsigned short rx_port,
			      unsigned short ser_alias)
{
	int rval;

	dev_dbg(va->sd.dev, "%s port %d, ser_alias %x\n", __func__, rx_port, ser_alias);
	rval = regmap_write(va->regmap8, TI960_RX_PORT_SEL,
		(rx_port << 4) + (1 << rx_port));
	if (rval)
		return rval;

	return regmap_write(va->regmap8, TI960_SER_ALIAS_ID, ser_alias);
}

static int ti960_fsin_gpio_init(struct ti960 *va, unsigned short rx_port,
		unsigned short ser_alias, unsigned short fsin_gpio)
{
	unsigned char gpio_data;
	int rval;
	int reg_val;

	dev_dbg(va->sd.dev, "%s\n", __func__);
	rval = regmap_read(va->regmap8, TI960_FS_CTL, &reg_val);
	if (rval) {
		dev_dbg(va->sd.dev, "Failed to read gpio status.\n");
		return rval;
	}

	if (!reg_val & TI960_FSIN_ENABLE) {
		dev_dbg(va->sd.dev, "FSIN not enabled, skip config FSIN GPIO.\n");
		return 0;
	}

	rval = regmap_write(va->regmap8, TI960_RX_PORT_SEL,
		(rx_port << 4) + (1 << rx_port));
	if (rval)
		return rval;

	switch (fsin_gpio) {
	case 0:
	case 1:
		rval = regmap_read(va->regmap8, TI960_BC_GPIO_CTL0, &reg_val);
		if (rval) {
			dev_dbg(va->sd.dev, "Failed to read gpio status.\n");
			return rval;
		}

		if (fsin_gpio == 0) {
			reg_val &= ~TI960_GPIO0_MASK;
			reg_val |= TI960_GPIO0_FSIN;
		} else {
			reg_val &= ~TI960_GPIO1_MASK;
			reg_val |= TI960_GPIO1_FSIN;
		}

		rval = regmap_write(va->regmap8, TI960_BC_GPIO_CTL0, reg_val);
		if (rval)
			dev_dbg(va->sd.dev, "Failed to set gpio.\n");
		break;
	case 2:
	case 3:
		rval = regmap_read(va->regmap8, TI960_BC_GPIO_CTL1, &reg_val);
		if (rval) {
			dev_dbg(va->sd.dev, "Failed to read gpio status.\n");
			return rval;
		}

		if (fsin_gpio == 2) {
			reg_val &= ~TI960_GPIO2_MASK;
			reg_val |= TI960_GPIO2_FSIN;
		} else {
			reg_val &= ~TI960_GPIO3_MASK;
			reg_val |= TI960_GPIO3_FSIN;
		}

		rval = regmap_write(va->regmap8, TI960_BC_GPIO_CTL1, reg_val);
		if (rval)
			dev_dbg(va->sd.dev, "Failed to set gpio.\n");
		break;
	}

	/* enable output and remote control */
	ti953_reg_write(&va->sd, rx_port, ser_alias, TI953_GPIO_INPUT_CTRL, TI953_GPIO_OUT_EN);
	rval = ti953_reg_read(&va->sd, rx_port, ser_alias, TI953_LOCAL_GPIO_DATA,
			&gpio_data);
	if (rval)
		return rval;
	ti953_reg_write(&va->sd, rx_port, ser_alias, TI953_LOCAL_GPIO_DATA,
			gpio_data | TI953_GPIO0_RMTEN << fsin_gpio);

	return rval;
}

static int ti960_get_routing(struct v4l2_subdev *sd,
				   struct v4l2_subdev_routing *route)
{
	struct ti960 *va = to_ti960(sd);
	int i, j;

	/* active routing first */
	j = 0;
	for (i = 0; i < va->nstreams; ++i) {
		if (j >= route->num_routes)
			break;
		if (!(va->ti960_route[i].flags & V4L2_SUBDEV_ROUTE_FL_ACTIVE))
			continue;
		route->routes[j].sink_pad = va->ti960_route[i].sink_pad;
		route->routes[j].sink_stream = va->ti960_route[i].sink_stream;
		route->routes[j].source_pad = va->ti960_route[i].source_pad;
		route->routes[j].source_stream = va->ti960_route[i].source_stream;
		route->routes[j].flags = va->ti960_route[i].flags;
		j++;
	}

	for (i = 0; i < va->nstreams; ++i) {
		if (j >= route->num_routes)
			break;
		if (va->ti960_route[i].flags & V4L2_SUBDEV_ROUTE_FL_ACTIVE)
			continue;
		route->routes[j].sink_pad = va->ti960_route[i].sink_pad;
		route->routes[j].sink_stream = va->ti960_route[i].sink_stream;
		route->routes[j].source_pad = va->ti960_route[i].source_pad;
		route->routes[j].source_stream = va->ti960_route[i].source_stream;
		route->routes[j].flags = va->ti960_route[i].flags;
		j++;
	}

	route->num_routes = i;

	return 0;
}

static int ti960_set_routing(struct v4l2_subdev *sd,
				   struct v4l2_subdev_routing *route)
{
	struct ti960 *va = to_ti960(sd);
	int i, j, ret = 0;

	for (i = 0; i < min(route->num_routes, va->nstreams); ++i) {
		struct v4l2_subdev_route *t = &route->routes[i];

		if (t->sink_stream > va->nstreams - 1 ||
		    t->source_stream > va->nstreams - 1)
			continue;

		if (t->source_pad != TI960_PAD_SOURCE)
			continue;

		for (j = 0; j < va->nstreams; j++) {
			if (t->sink_pad == va->ti960_route[j].sink_pad &&
				t->source_pad == va->ti960_route[j].source_pad &&
				t->sink_stream == va->ti960_route[j].sink_stream &&
				t->source_stream == va->ti960_route[j].source_stream)
				break;
		}

		if (j == va->nstreams)
			continue;

		if (t->flags & V4L2_SUBDEV_ROUTE_FL_ACTIVE)
			va->ti960_route[j].flags |=
				V4L2_SUBDEV_ROUTE_FL_ACTIVE;
		else if (!(t->flags & V4L2_SUBDEV_ROUTE_FL_ACTIVE))
			va->ti960_route[j].flags &=
				(~V4L2_SUBDEV_ROUTE_FL_ACTIVE);
	}

	return ret;
}

static int ti960_enum_mbus_code(struct v4l2_subdev *sd,
				      struct v4l2_subdev_pad_config *cfg,
				      struct v4l2_subdev_mbus_code_enum *code)
{
	struct ti960 *va = to_ti960(sd);
	const uint32_t *supported_code =
		ti960_supported_codes[code->pad];
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

static const struct ti960_csi_data_format
		*ti960_validate_csi_data_format(u32 code)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(va_csi_data_formats); i++) {
		if (va_csi_data_formats[i].code == code)
			return &va_csi_data_formats[i];
	}

	return &va_csi_data_formats[0];
}

static int ti960_get_routing_remote_pad(struct v4l2_subdev *sd,
	unsigned int pad)
{
	struct ti960 *va = to_ti960(sd);
	int i;

	for (i = 0; i < va->nstreams; ++i) {
		if (va->ti960_route[i].sink_pad == pad)
			return va->ti960_route[i].source_pad;
		if (va->ti960_route[i].source_pad == pad)
			return va->ti960_route[i].sink_pad;
	}
	return -1;
}

static int ti960_get_frame_desc(struct v4l2_subdev *sd,
	unsigned int pad, struct v4l2_mbus_frame_desc *desc)
{
	struct ti960 *va = to_ti960(sd);
	int sink_pad = pad;

	if (va->pad[pad].flags & MEDIA_PAD_FL_SOURCE)
		sink_pad = ti960_get_routing_remote_pad(sd, pad);
	if (sink_pad >= 0) {
		struct media_pad *remote_pad =
			media_entity_remote_pad(&sd->entity.pads[sink_pad]);
		if (remote_pad) {
			struct v4l2_subdev *rsd = media_entity_to_v4l2_subdev(remote_pad->entity);

			dev_dbg(sd->dev, "%s remote sd: %s\n", __func__, rsd->name);
			v4l2_subdev_call(rsd, pad, get_frame_desc, 0, desc);
		}
	} else
		dev_err(sd->dev, "can't find the frame desc\n");
	return 0;
}

static struct v4l2_mbus_framefmt *
__ti960_get_ffmt(struct v4l2_subdev *subdev,
			 struct v4l2_subdev_pad_config *cfg,
			 unsigned int pad, unsigned int which,
			 unsigned int stream)
{
	struct ti960 *va = to_ti960(subdev);

	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_format(subdev, cfg, pad);
	else
		return &va->ffmts[pad][stream];
}

static int ti960_get_format(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_format *fmt)
{
	struct ti960 *va = to_ti960(subdev);

	if (fmt->stream > va->nstreams)
		return -EINVAL;

	mutex_lock(&va->mutex);
	fmt->format = *__ti960_get_ffmt(subdev, cfg, fmt->pad,
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

static int ti960_set_format(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_format *fmt)
{
	struct ti960 *va = to_ti960(subdev);
	const struct ti960_csi_data_format *csi_format;
	struct v4l2_mbus_framefmt *ffmt;

	if (fmt->stream > va->nstreams)
		return -EINVAL;

	csi_format = ti960_validate_csi_data_format(
		fmt->format.code);

	mutex_lock(&va->mutex);
	ffmt = __ti960_get_ffmt(subdev, cfg, fmt->pad, fmt->which,
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

static int ti960_open(struct v4l2_subdev *subdev,
				struct v4l2_subdev_fh *fh)
{
	struct v4l2_mbus_framefmt *try_fmt =
		v4l2_subdev_get_try_format(subdev, fh->pad, 0);

	struct v4l2_subdev_format fmt = {
		.which = V4L2_SUBDEV_FORMAT_TRY,
		.pad = TI960_PAD_SOURCE,
		.format = {
			.width = TI960_MAX_WIDTH,
			.height = TI960_MAX_HEIGHT,
			.code = MEDIA_BUS_FMT_SBGGR12_1X12,
		},
		.stream = 0,
	};

	*try_fmt = fmt.format;

	return 0;
}

static int ti960_map_subdevs_addr(struct ti960 *va)
{
	unsigned short rx_port, phy_i2c_addr, alias_i2c_addr;
	int i, rval;

	for (i = 0; i < NR_OF_TI960_SINK_PADS; i++) {
		rx_port = va->sub_devs[i].rx_port;
		phy_i2c_addr = va->sub_devs[i].phy_i2c_addr;
		alias_i2c_addr = va->sub_devs[i].alias_i2c_addr;

		if (!phy_i2c_addr || !alias_i2c_addr)
			continue;

		rval = ti960_map_phy_i2c_addr(va, rx_port, phy_i2c_addr);
		if (rval)
			return rval;

		/* set 7bit alias i2c addr */
		rval = ti960_map_alias_i2c_addr(va, rx_port,
						alias_i2c_addr << 1);
		if (rval)
			return rval;
	}

	return 0;
}

/*
 * FIXME: workaround, reset to avoid block.
 */
static int reset_sensor(struct ti960 *va, unsigned short rx_port,
		unsigned short ser_alias, int reset)
{
	int rval;
	unsigned char gpio_data;

	rval = ti953_reg_read(&va->sd, rx_port, ser_alias,
			TI953_LOCAL_GPIO_DATA,
			&gpio_data);
	if (rval)
		return rval;

	ti953_reg_write(&va->sd, rx_port, ser_alias, TI953_GPIO_INPUT_CTRL,
			TI953_GPIO_OUT_EN);
	gpio_data &= ~(TI953_GPIO0_RMTEN << reset);
	gpio_data &= ~(TI953_GPIO0_OUT << reset);
	ti953_reg_write(&va->sd, rx_port, ser_alias, TI953_LOCAL_GPIO_DATA,
			gpio_data);
	msleep(50);
	gpio_data |= TI953_GPIO0_OUT << reset;
	ti953_reg_write(&va->sd, rx_port, ser_alias, TI953_LOCAL_GPIO_DATA,
			gpio_data);

	return 0;
}

static int ti960_registered(struct v4l2_subdev *subdev)
{
	struct ti960 *va = to_ti960(subdev);
	struct i2c_client *client = v4l2_get_subdevdata(subdev);
	int i, j, k, l, m, rval;
	bool port_registered[NR_OF_TI960_SINK_PADS];


	for (i = 0 ; i < NR_OF_TI960_SINK_PADS; i++)
		port_registered[i] = false;

	for (i = 0, k = 0; i < va->pdata->subdev_num; i++) {
		struct ti960_subdev_info *info =
			&va->pdata->subdev_info[i];
		struct crlmodule_platform_data *pdata =
			(struct crlmodule_platform_data *)
			info->board_info.platform_data;

		if (k >= va->nsinks)
			break;

		if (port_registered[info->rx_port]) {
			dev_err(va->sd.dev,
				"rx port %d registed already\n",
				info->rx_port);
			continue;
		}

		rval = ti960_map_ser_alias_addr(va, info->rx_port,
				info->ser_alias << 1);
		if (rval)
			return rval;


		if (!ti953_detect(&va->sd, info->rx_port, info->ser_alias))
			continue;

		/*
		 * The sensors should not share the same pdata structure.
		 * Clone the pdata for each sensor.
		 */
		memcpy(&va->subdev_pdata[k], pdata, sizeof(*pdata));

		va->sub_devs[k].fsin_gpio = va->subdev_pdata[k].fsin;

		/* Spin sensor subdev suffix name */
		va->subdev_pdata[k].suffix = info->suffix;

		/*
		 * Change the gpio value to have xshutdown
		 * and rx port included, so in gpio_set those
		 * can be caculated from it.
		 */
		va->subdev_pdata[k].xshutdown += va->gc.base +
					info->rx_port * NR_OF_GPIOS_PER_PORT;
		info->board_info.platform_data = &va->subdev_pdata[k];

		if (!info->phy_i2c_addr || !info->board_info.addr) {
			dev_err(va->sd.dev, "can't find the physical and alias addr.\n");
			return -EINVAL;
		}

		ti953_reg_write(&va->sd, info->rx_port, info->ser_alias,
				TI953_RESET_CTL, TI953_DIGITAL_RESET_1);
		msleep(50);

		if (va->subdev_pdata[k].module_flags & CRL_MODULE_FL_INIT_SER) {
			rval = ti953_init(&va->sd, info->rx_port, info->ser_alias);
			if (rval)
				return rval;
		}

		if (va->subdev_pdata[k].module_flags & CRL_MODULE_FL_POWERUP) {
			ti953_reg_write(&va->sd, info->rx_port, info->ser_alias,
					TI953_GPIO_INPUT_CTRL, TI953_GPIO_OUT_EN);

			/* boot sequence */
			for (m = 0; m < CRL_MAX_GPIO_POWERUP_SEQ; m++) {
				if (va->subdev_pdata[k].gpio_powerup_seq[m] < 0)
					break;
				msleep(50);
				ti953_reg_write(&va->sd, info->rx_port, info->ser_alias,
						TI953_LOCAL_GPIO_DATA,
						va->subdev_pdata[k].gpio_powerup_seq[m]);
			}
		}

		/* Map PHY I2C address. */
		rval = ti960_map_phy_i2c_addr(va, info->rx_port,
					info->phy_i2c_addr);
		if (rval)
			return rval;

		/* Map 7bit ALIAS I2C address. */
		rval = ti960_map_alias_i2c_addr(va, info->rx_port,
				info->board_info.addr << 1);
		if (rval)
			return rval;

		va->sub_devs[k].sd = v4l2_i2c_new_subdev_board(
			va->sd.v4l2_dev, client->adapter,
			&info->board_info, 0);
		if (!va->sub_devs[k].sd) {
			dev_err(va->sd.dev,
				"can't create new i2c subdev %c\n",
				info->suffix);
			continue;
		}
		va->sub_devs[k].rx_port = info->rx_port;
		va->sub_devs[k].phy_i2c_addr = info->phy_i2c_addr;
		va->sub_devs[k].alias_i2c_addr = info->board_info.addr;
		va->sub_devs[k].ser_i2c_addr = info->ser_alias;
		memcpy(va->sub_devs[k].sd_name,
				va->subdev_pdata[k].module_name,
				min(sizeof(va->sub_devs[k].sd_name) - 1,
				sizeof(va->subdev_pdata[k].module_name) - 1));

		for (j = 0; j < va->sub_devs[k].sd->entity.num_pads; j++) {
			if (va->sub_devs[k].sd->entity.pads[j].flags &
				MEDIA_PAD_FL_SOURCE)
				break;
		}

		if (j == va->sub_devs[k].sd->entity.num_pads) {
			dev_warn(va->sd.dev,
				"no source pad in subdev %c\n",
				info->suffix);
			return -ENOENT;
		}

		for (l = 0; l < va->nsinks; l++) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
			rval = media_entity_create_link(
#else
			rval = media_create_pad_link(
#endif
				&va->sub_devs[k].sd->entity, j,
				&va->sd.entity, l, 0);
			if (rval) {
				dev_err(va->sd.dev,
					"can't create link to %c\n",
					info->suffix);
				return -EINVAL;
			}
		}
		port_registered[va->sub_devs[k].rx_port] = true;
		k++;
	}
	rval = ti960_map_subdevs_addr(va);
	if (rval)
		return rval;

	return 0;
}

static int ti960_set_power(struct v4l2_subdev *subdev, int on)
{
	struct ti960 *va = to_ti960(subdev);
	int ret;
	u8 val;

	ret = regmap_write(va->regmap8, TI960_RESET,
			   (on) ? TI960_POWER_ON : TI960_POWER_OFF);
	if (ret || !on)
		return ret;

	/* Configure MIPI clock bsaed on control value. */
	ret = regmap_write(va->regmap8, TI960_CSI_PLL_CTL,
			    ti960_op_sys_clock_reg_val[
			    v4l2_ctrl_g_ctrl(va->link_freq)]);
	if (ret)
		return ret;
	val = TI960_CSI_ENABLE;
	val |= TI960_CSI_CONTS_CLOCK;
	/* Enable skew calculation when 1.6Gbps output is enabled. */
	if (v4l2_ctrl_g_ctrl(va->link_freq))
		val |= TI960_CSI_SKEWCAL;
	return regmap_write(va->regmap8, TI960_CSI_CTL, val);
}

static bool ti960_broadcast_mode(struct v4l2_subdev *subdev)
{
	struct ti960 *va = to_ti960(subdev);
	struct v4l2_subdev_format fmt = { 0 };
	struct v4l2_subdev *sd;
	char *sd_name = NULL;
	bool first = true;
	unsigned int h = 0, w = 0, code = 0;
	bool single_stream = true;
	int i, rval;

	for (i = 0; i < NR_OF_TI960_SINK_PADS; i++) {
		struct media_pad *remote_pad =
			media_entity_remote_pad(&va->pad[i]);

		if (!remote_pad)
			continue;

		sd = media_entity_to_v4l2_subdev(remote_pad->entity);
		fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
		fmt.pad = remote_pad->index;
		fmt.stream = 0;

		rval = v4l2_subdev_call(sd, pad, get_fmt, NULL, &fmt);
		if (rval)
			return false;

		if (first) {
			sd_name = va->sub_devs[i].sd_name;
			h = fmt.format.height;
			w = fmt.format.width;
			code = fmt.format.code;
			first = false;
		} else {
			if (strncmp(sd_name, va->sub_devs[i].sd_name, 16))
				return false;

			if (h != fmt.format.height || w != fmt.format.width
				|| code != fmt.format.code)
				return false;

			single_stream = false;
		}
	}

	if (single_stream)
		return false;

	return true;
}

static int ti960_rx_port_config(struct ti960 *va, int sink, int rx_port)
{
	int rval;
	int i;
	unsigned int csi_vc_map;

	/* Select RX port. */
	rval = regmap_write(va->regmap8, TI960_RX_PORT_SEL,
			(rx_port << 4) + (1 << rx_port));
	if (rval) {
		dev_err(va->sd.dev, "Failed to select RX port.\n");
		return rval;
	}

	rval = regmap_write(va->regmap8, TI960_PORT_CONFIG,
		TI960_FPD3_CSI);
	if (rval) {
		dev_err(va->sd.dev, "Failed to set port config.\n");
		return rval;
	}

	/*
	 * CSI VC MAPPING.
	 */
	rval = regmap_read(va->regmap8, TI960_CSI_VC_MAP, &csi_vc_map);
	if (rval < 0) {
		dev_err(va->sd.dev, "960 reg read ret=%x", rval);
		return rval;
	}
	for (i = 0; i < va->nstreams; ++i) {
		if (!(va->ti960_route[i].flags & V4L2_SUBDEV_ROUTE_FL_ACTIVE))
			continue;
		if (rx_port != va->ti960_route[i].sink_pad)
			continue;
		csi_vc_map &= ~(0x3 << (va->ti960_route[i].sink_stream & 0x3) * 2);
		csi_vc_map |= (va->ti960_route[i].source_stream & 0x3)
			<< (va->ti960_route[i].sink_stream & 0x3) * 2;
	}
	dev_dbg(va->sd.dev, "%s port %d, csi_vc_map %x",
		__func__, rx_port, csi_vc_map);
	rval = regmap_write(va->regmap8, TI960_CSI_VC_MAP,
		csi_vc_map);
	if (rval) {
		dev_err(va->sd.dev, "Failed to set port config.\n");
		return rval;
	}
	return 0;
}

static int ti960_find_subdev_index(struct ti960 *va, struct v4l2_subdev *sd)
{
	int i;

	for (i = 0; i < NR_OF_TI960_SINK_PADS; i++) {
		if (va->sub_devs[i].sd == sd)
			return i;
	}

	WARN_ON(1);

	return -EINVAL;
}

static int ti960_set_frame_sync(struct ti960 *va, int enable)
{
	int i, rval;
	int index = !!enable;

	for (i = 0; i < ARRAY_SIZE(ti960_frame_sync_settings[index]); i++) {
		rval = regmap_write(va->regmap8,
				ti960_frame_sync_settings[index][i].reg,
				ti960_frame_sync_settings[index][i].val);
		if (rval) {
			dev_err(va->sd.dev, "Failed to %s frame sync\n",
				enable ? "enable" : "disable");
			return rval;
		}
	}

	return 0;
}

static int ti960_set_stream(struct v4l2_subdev *subdev, int enable)
{
	struct ti960 *va = to_ti960(subdev);
	struct v4l2_subdev *sd;
	int i, j, rval;
	bool broadcast;
	unsigned short rx_port;
	unsigned short ser_alias;
	int sd_idx = -1;
	DECLARE_BITMAP(rx_port_enabled, 32);

	dev_dbg(va->sd.dev, "TI960 set stream, enable %d\n", enable);

	broadcast = ti960_broadcast_mode(subdev);
	if (enable)
		dev_info(va->sd.dev, "TI960 in %s mode",
			broadcast ? "broadcast" : "non broadcast");

	bitmap_zero(rx_port_enabled, 32);
	for (i = 0; i < NR_OF_TI960_SINK_PADS; i++) {
		struct media_pad *remote_pad =
			media_entity_remote_pad(&va->pad[i]);

		if (!remote_pad)
			continue;

		/* Find ti960 subdev */
		sd = media_entity_to_v4l2_subdev(remote_pad->entity);
		j = ti960_find_subdev_index(va, sd);
		if (j < 0)
			return -EINVAL;
		rx_port = va->sub_devs[j].rx_port;
		ser_alias = va->sub_devs[j].ser_i2c_addr;
		rval = ti960_rx_port_config(va, i, rx_port);
		if (rval < 0)
			return rval;

		bitmap_set(rx_port_enabled, rx_port, 1);

		if (broadcast && sd_idx == -1) {
			sd_idx = j;
		} else if (broadcast) {
			rval = ti960_map_alias_i2c_addr(va, rx_port,
				va->sub_devs[sd_idx].alias_i2c_addr << 1);
			if (rval < 0)
				return rval;
		} else {
			/* Stream on/off sensor */
			dev_err(va->sd.dev,
					"set stream for %s, enable  %d\n",
					sd->name, enable);
			rval = v4l2_subdev_call(sd, video, s_stream, enable);
			if (rval) {
				dev_err(va->sd.dev,
					"Failed to set stream for %s, enable  %d\n",
					sd->name, enable);
				return rval;
			}

			/* RX port fordward */
			rval = ti960_reg_set_bit(va, TI960_FWD_CTL1,
						rx_port + 4, !enable);
			if (rval) {
				dev_err(va->sd.dev,
					"Failed to forward RX port%d. enable %d\n",
					i, enable);
				return rval;
			}
			if (va->subdev_pdata[j].module_flags & CRL_MODULE_FL_RESET) {
				rval = reset_sensor(va, rx_port, ser_alias,
						va->subdev_pdata[j].reset);
				if (rval)
					return rval;
			}
		}
	}

	if (broadcast) {
		if (sd_idx < 0) {
			dev_err(va->sd.dev, "No sensor connected!\n");
			return -ENODEV;
		}
		sd = va->sub_devs[sd_idx].sd;
		rval = v4l2_subdev_call(sd, video, s_stream, enable);
		if (rval) {
			dev_err(va->sd.dev,
				"Failed to set stream for %s. enable  %d\n",
				sd->name, enable);
			return rval;
		}

		rval = ti960_set_frame_sync(va, enable);
		if (rval) {
			dev_err(va->sd.dev,
				"Failed to set frame sync.\n");
			return rval;
		}

		for (i = 0; i < NR_OF_TI960_SINK_PADS; i++) {
			if (enable && test_bit(i, rx_port_enabled)) {
				rval = ti960_fsin_gpio_init(va,
						va->sub_devs[i].rx_port,
						va->sub_devs[i].ser_i2c_addr,
						va->sub_devs[i].fsin_gpio);
				if (rval) {
					dev_err(va->sd.dev,
						"Failed to enable frame sync gpio init.\n");
					return rval;
				}

				if (va->subdev_pdata[i].module_flags & CRL_MODULE_FL_RESET) {
					rx_port = va->sub_devs[i].rx_port;
					ser_alias = va->sub_devs[i].ser_i2c_addr;
					rval = reset_sensor(va, rx_port, ser_alias,
							va->subdev_pdata[i].reset);
					if (rval)
						return rval;
				}
			}
		}

		for (i = 0; i < NR_OF_TI960_SINK_PADS; i++) {
			if (!test_bit(i, rx_port_enabled))
				continue;

			/* RX port fordward */
			rval = ti960_reg_set_bit(va, TI960_FWD_CTL1,
						i + 4, !enable);
			if (rval) {
				dev_err(va->sd.dev,
					"Failed to forward RX port%d. enable %d\n",
					i, enable);
				return rval;
			}
		}

		/*
		 * Restore each subdev i2c address as we may
		 * touch it later.
		 */
		rval = ti960_map_subdevs_addr(va);
		if (rval)
			return rval;
	}

	return 0;
}

static struct v4l2_subdev_internal_ops ti960_sd_internal_ops = {
	.open = ti960_open,
	.registered = ti960_registered,
};

static bool ti960_sd_has_route(struct media_entity *entity,
		unsigned int pad0, unsigned int pad1, int *stream)
{
	struct ti960 *va = to_ti960(media_entity_to_v4l2_subdev(entity));
	int i;

	if (va == NULL || stream == NULL ||
		*stream >= va->nstreams || *stream < 0)
		return false;

	for (i = 0; i < va->nstreams; ++i) {
		if ((va->ti960_route[*stream].flags & V4L2_SUBDEV_ROUTE_FL_ACTIVE) &&
		((va->ti960_route[*stream].source_pad == pad0 &&
		 va->ti960_route[*stream].sink_pad == pad1) ||
		(va->ti960_route[*stream].source_pad == pad1 &&
		 va->ti960_route[*stream].sink_pad == pad0)))
			return true;
	}

	return false;
}

static const struct media_entity_operations ti960_sd_entity_ops = {
	.has_route = ti960_sd_has_route,
};

static const struct v4l2_subdev_video_ops ti960_sd_video_ops = {
	.s_stream = ti960_set_stream,
};

static const struct v4l2_subdev_core_ops ti960_core_subdev_ops = {
	.s_power = ti960_set_power,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
	.g_ctrl = v4l2_subdev_g_ctrl,
	.s_ctrl = v4l2_subdev_s_ctrl,
	.g_ext_ctrls = v4l2_subdev_g_ext_ctrls,
	.s_ext_ctrls = v4l2_subdev_s_ext_ctrls,
	.try_ext_ctrls = v4l2_subdev_try_ext_ctrls,
	.queryctrl = v4l2_subdev_queryctrl,
#endif
};

static int ti960_s_ctrl(struct v4l2_ctrl *ctrl)
{
	return 0;
}

static const struct v4l2_ctrl_ops ti960_ctrl_ops = {
	.s_ctrl = ti960_s_ctrl,
};

static const struct v4l2_ctrl_config ti960_controls[] = {
	{
		.ops = &ti960_ctrl_ops,
		.id = V4L2_CID_LINK_FREQ,
		.name = "V4L2_CID_LINK_FREQ",
		.type = V4L2_CTRL_TYPE_INTEGER_MENU,
		.max = ARRAY_SIZE(ti960_op_sys_clock) - 1,
		.min =  0,
		.step  = 0,
		.def = 1,
		.qmenu_int = ti960_op_sys_clock,
	},
	{
		.ops = &ti960_ctrl_ops,
		.id = V4L2_CID_TEST_PATTERN,
		.name = "V4L2_CID_TEST_PATTERN",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.max = 1,
		.min =	0,
		.step  = 1,
		.def = 0,
	},
};

static const struct v4l2_subdev_pad_ops ti960_sd_pad_ops = {
	.get_fmt = ti960_get_format,
	.set_fmt = ti960_set_format,
	.get_frame_desc = ti960_get_frame_desc,
	.enum_mbus_code = ti960_enum_mbus_code,
	.set_routing = ti960_set_routing,
	.get_routing = ti960_get_routing,
};

static struct v4l2_subdev_ops ti960_sd_ops = {
	.core = &ti960_core_subdev_ops,
	.video = &ti960_sd_video_ops,
	.pad = &ti960_sd_pad_ops,
};

static int ti960_register_subdev(struct ti960 *va)
{
	int i, rval;
	struct i2c_client *client = v4l2_get_subdevdata(&va->sd);

	v4l2_subdev_init(&va->sd, &ti960_sd_ops);
	snprintf(va->sd.name, sizeof(va->sd.name), "TI960 %c",
		va->pdata->suffix);

	va->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
			V4L2_SUBDEV_FL_HAS_SUBSTREAMS;

	va->sd.internal_ops = &ti960_sd_internal_ops;
	va->sd.entity.ops = &ti960_sd_entity_ops;

	v4l2_set_subdevdata(&va->sd, client);

	v4l2_ctrl_handler_init(&va->ctrl_handler,
				ARRAY_SIZE(ti960_controls));

	if (va->ctrl_handler.error) {
		dev_err(va->sd.dev,
			"Failed to init ti960 controls. ERR: %d!\n",
			va->ctrl_handler.error);
		return va->ctrl_handler.error;
	}

	va->sd.ctrl_handler = &va->ctrl_handler;

	for (i = 0; i < ARRAY_SIZE(ti960_controls); i++) {
		const struct v4l2_ctrl_config *cfg =
			&ti960_controls[i];
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
	va->pad[TI960_PAD_SOURCE].flags =
		MEDIA_PAD_FL_SOURCE | MEDIA_PAD_FL_MULTIPLEX;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
	rval = media_entity_init(&va->sd.entity, NR_OF_TI960_PADS, va->pad, 0);
#else
	rval = media_entity_pads_init(&va->sd.entity,
				      NR_OF_TI960_PADS, va->pad);
#endif
	if (rval) {
		dev_err(va->sd.dev,
			"Failed to init media entity for ti960!\n");
		goto failed_out;
	}

	return 0;

failed_out:
	v4l2_ctrl_handler_free(&va->ctrl_handler);
	return rval;
}

static int ti960_init(struct ti960 *va)
{
	unsigned int reset_gpio = va->pdata->reset_gpio;
	int i, rval;
	unsigned int val;

	gpio_set_value(reset_gpio, 1);
	usleep_range(2000, 3000);
	dev_err(va->sd.dev, "Setting reset gpio %d to 1.\n", reset_gpio);

	rval = ti960_reg_read(va, TI960_DEVID, &val);
	if (rval) {
		dev_err(va->sd.dev, "Failed to read device ID of TI960!\n");
		return rval;
	}
	dev_info(va->sd.dev, "TI960 device ID: 0x%X\n", val);

	for (i = 0; i < ARRAY_SIZE(ti960_gpio_settings); i++) {
		rval = regmap_write(va->regmap8,
			ti960_gpio_settings[i].reg,
			ti960_gpio_settings[i].val);
		if (rval) {
			dev_err(va->sd.dev,
				"Failed to write TI960 gpio setting, reg %2x, val %2x\n",
				ti960_gpio_settings[i].reg, ti960_gpio_settings[i].val);
			return rval;
		}
	}
	usleep_range(10000, 11000);

	for (i = 0; i < ARRAY_SIZE(ti960_init_settings); i++) {
		rval = regmap_write(va->regmap8,
			ti960_init_settings[i].reg,
			ti960_init_settings[i].val);
		if (rval) {
			dev_err(va->sd.dev,
				"Failed to write TI960 init setting, reg %2x, val %2x\n",
				ti960_init_settings[i].reg, ti960_init_settings[i].val);
			return rval;
		}
	}

	/* wait for ti953 ready */
	msleep(200);

	rval = ti960_map_subdevs_addr(va);
	if (rval)
		return rval;

	return 0;
}

static void ti960_gpio_set(struct gpio_chip *chip, unsigned int gpio, int value)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
	struct i2c_client *client = to_i2c_client(chip->dev);
#else
	struct i2c_client *client = to_i2c_client(chip->parent);
#endif
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct ti960 *va = to_ti960(subdev);
	unsigned int reg_val;
	int rx_port, gpio_port;
	int ret;

	if (gpio >= NR_OF_TI960_GPIOS)
		return;

	rx_port = gpio / NR_OF_GPIOS_PER_PORT;
	gpio_port = gpio % NR_OF_GPIOS_PER_PORT;

	ret = regmap_write(va->regmap8, TI960_RX_PORT_SEL,
			  (rx_port << 4) + (1 << rx_port));
	if (ret) {
		dev_dbg(&client->dev, "Failed to select RX port.\n");
		return;
	}
	ret = regmap_read(va->regmap8, TI960_BC_GPIO_CTL0, &reg_val);
	if (ret) {
		dev_dbg(&client->dev, "Failed to read gpio status.\n");
		return;
	}

	if (gpio_port == 0) {
		reg_val &= ~TI960_GPIO0_MASK;
		reg_val |= value ? TI960_GPIO0_HIGH : TI960_GPIO0_LOW;
	} else {
		reg_val &= ~TI960_GPIO1_MASK;
		reg_val |= value ? TI960_GPIO1_HIGH : TI960_GPIO1_LOW;
	}

	ret = regmap_write(va->regmap8, TI960_BC_GPIO_CTL0, reg_val);
	if (ret)
		dev_dbg(&client->dev, "Failed to set gpio.\n");
}

static int ti960_gpio_direction_output(struct gpio_chip *chip,
				       unsigned int gpio, int level)
{
	return 0;
}

static int ti960_probe(struct i2c_client *client,
			const struct i2c_device_id *devid)
{
	struct ti960 *va;
	int i, j, k, l, rval = 0;

	if (client->dev.platform_data == NULL)
		return -ENODEV;

	va = devm_kzalloc(&client->dev, sizeof(*va), GFP_KERNEL);
	if (!va)
		return -ENOMEM;

	va->pdata = client->dev.platform_data;

	va->nsources = NR_OF_TI960_SOURCE_PADS;
	va->nsinks = NR_OF_TI960_SINK_PADS;
	va->npads = NR_OF_TI960_PADS;
	va->nstreams = NR_OF_TI960_STREAMS;

	va->crop = devm_kcalloc(&client->dev, va->npads,
				sizeof(struct v4l2_rect), GFP_KERNEL);

	va->compose = devm_kcalloc(&client->dev, va->npads,
				   sizeof(struct v4l2_rect), GFP_KERNEL);

	if (!va->crop || !va->compose)
		return -ENOMEM;

	for (i = 0; i < va->npads; i++) {
		va->ffmts[i] = devm_kcalloc(&client->dev, va->nstreams,
					    sizeof(struct v4l2_mbus_framefmt),
					    GFP_KERNEL);
		if (!va->ffmts[i])
			return -ENOMEM;
	}

	va->ti960_route = devm_kcalloc(&client->dev, NR_OF_TI960_STREAMS,
		sizeof(struct v4l2_subdev_routing), GFP_KERNEL);

	if (!va->ti960_route)
		return -ENOMEM;

	/* routing for virtual channel supports */
	l = 0;
	for (i = 0; i < NR_OF_TI960_SINK_PADS; i++)
		for (j = 0; j < NR_OF_TI960_VCS_PER_SINK_PAD; j++)
			for (k = 0; k < NR_OF_TI960_VCS_SOURCE_PAD; k++) {
				va->ti960_route[l].sink_pad = i;
				va->ti960_route[l].sink_stream = j;
				va->ti960_route[l].source_pad = TI960_PAD_SOURCE;
				va->ti960_route[l].source_stream = k;
				va->ti960_route[l].flags = MEDIA_PAD_FL_MULTIPLEX;
				l++;
	}

	va->regmap8 = devm_regmap_init_i2c(client,
					   &ti960_reg_config8);
	if (IS_ERR(va->regmap8)) {
		dev_err(&client->dev, "Failed to init regmap8!\n");
		return -EIO;
	}

	va->regmap16 = devm_regmap_init_i2c(client,
					    &ti960_reg_config16);
	if (IS_ERR(va->regmap16)) {
		dev_err(&client->dev, "Failed to init regmap16!\n");
		return -EIO;
	}

	mutex_init(&va->mutex);
	v4l2_i2c_subdev_init(&va->sd, client, &ti960_sd_ops);
	rval = ti960_register_subdev(va);
	if (rval) {
		dev_err(&client->dev, "Failed to register va subdevice!\n");
		return rval;
	}

	if (devm_gpio_request_one(va->sd.dev, va->pdata->reset_gpio, 0,
				  "ti960 reset") != 0) {
		dev_err(va->sd.dev, "Unable to acquire gpio %d\n",
			va->pdata->reset_gpio);
		return -ENODEV;
	}

	rval = ti960_init(va);
	if (rval) {
		dev_err(&client->dev, "Failed to init TI960!\n");
		return rval;
	}

	/*
	 * TI960 has several back channel GPIOs.
	 * We export GPIO0 and GPIO1 to control reset or fsin.
	 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
	va->gc.dev = &client->dev;
#else
	va->gc.parent = &client->dev;
#endif
	va->gc.owner = THIS_MODULE;
	va->gc.label = "TI960 GPIO";
	va->gc.ngpio = NR_OF_TI960_GPIOS;
	va->gc.base = -1;
	va->gc.set = ti960_gpio_set;
	va->gc.direction_output = ti960_gpio_direction_output;
	rval = gpiochip_add(&va->gc);
	if (rval) {
		dev_err(&client->dev, "Failed to add gpio chip! %d\n", rval);
		return -EIO;
	}

	return 0;
}

static int ti960_remove(struct i2c_client *client)
{
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct ti960 *va = to_ti960(subdev);
	int i;

	if (!va)
		return 0;

	mutex_destroy(&va->mutex);
	v4l2_ctrl_handler_free(&va->ctrl_handler);
	v4l2_device_unregister_subdev(&va->sd);
	media_entity_cleanup(&va->sd.entity);

	for (i = 0; i < NR_OF_TI960_SINK_PADS; i++) {
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
static int ti960_suspend(struct device *dev)
{
	return 0;
}

static int ti960_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct ti960 *va = to_ti960(subdev);

	return ti960_init(va);
}
#else
#define ti960_suspend	NULL
#define ti960_resume	NULL
#endif /* CONFIG_PM */

static const struct i2c_device_id ti960_id_table[] = {
	{ TI960_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, ti960_id_table);

static const struct dev_pm_ops ti960_pm_ops = {
	.suspend = ti960_suspend,
	.resume = ti960_resume,
};

static struct i2c_driver ti960_i2c_driver = {
	.driver = {
		.name = TI960_NAME,
		.pm = &ti960_pm_ops,
	},
	.probe	= ti960_probe,
	.remove	= ti960_remove,
	.id_table = ti960_id_table,
};
module_i2c_driver(ti960_i2c_driver);

MODULE_AUTHOR("Chen Meng J <meng.j.chen@intel.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TI960 CSI2-Aggregator driver");
