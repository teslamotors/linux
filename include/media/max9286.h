/* SPDX-LIcense_Identifier: GPL-2.0 */
/* Copyright (C) 2018 Intel Corporation */

#ifndef MAX9286_H
#define MAX9286_H

#include <linux/i2c.h>
#include <linux/regmap.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>

#define MAX9286_NAME		"max9286"

#define PIXEL_ORDER_GRBG	0
#define PIXEL_ORDER_RGGB	1
#define PIXEL_ORDER_BGGR	2
#define PIXEL_ORDER_GBRG	3

#define NR_OF_MAX_STREAMS	4
#define NR_OF_MAX_SOURCE_PADS	1
#define NR_OF_MAX_SINK_PADS	4
#define NR_OF_MAX_PADS		(NR_OF_MAX_SOURCE_PADS + NR_OF_MAX_SINK_PADS)

#define MAX_PAD_SOURCE		4

#define MAX9286_MIN_WIDTH	640
#define MAX9286_MIN_HEIGHT	480
#define MAX9286_MAX_WIDTH	1280
#define MAX9286_MAX_HEIGHT	800

struct max9286_csi_data_format {
	u32 code;
	u8 width;
	u8 compressed;
	u8 pixel_order;
	u8 mipi_dt_code;
};

struct max9286_subdev_i2c_info {
	struct i2c_board_info board_info;
	int i2c_adapter_id;
	const char suffix; /* suffix for subdevs */
};

struct max9286_pdata {
	unsigned int subdev_num;
	struct max9286_subdev_i2c_info *subdev_info;
	unsigned int reset_gpio;
	const char suffix; /* suffix for multi aggregators, abcd... */
};

#endif
