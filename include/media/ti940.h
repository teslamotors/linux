/*
 * ti940.h -- driver for ti940
 *
 * Copyright (c) 2017 Tesla Motors, Inc.
 *
 * Based on ti964.h.
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
#ifndef TI940_H
#define TI940_H

#include <linux/i2c.h>
#include <linux/regmap.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>

#define TI940_NAME "ti940"

#define PIXEL_ORDER_GRBG	0
#define PIXEL_ORDER_RGGB	1
#define PIXEL_ORDER_BGGR	2
#define PIXEL_ORDER_GBRG	3

#define NR_OF_VA_STREAMS	1
#define NR_OF_VA_SOURCE_PADS	1
#define NR_OF_VA_SINK_PADS	1
#define NR_OF_VA_PADS		(NR_OF_VA_SOURCE_PADS + NR_OF_VA_SINK_PADS)

#define VA_PAD_SOURCE	1

#define TI940_MIN_WIDTH		640
#define TI940_MIN_HEIGHT	480
#define TI940_MAX_WIDTH		1920
#define TI940_MAX_HEIGHT	1080

struct ti940_csi_data_format {
	u32 code;
	u8 width;
	u8 compressed;
	u8 pixel_order;
	u8 mipi_dt_code;
};

struct ti940_subdev_i2c_info {
	struct i2c_board_info board_info;
	int i2c_adapter_id;
};

struct ti940_pdata {
	unsigned int subdev_num;
	struct ti940_subdev_i2c_info *subdev_info;
	unsigned int reset_gpio;
};

#endif
