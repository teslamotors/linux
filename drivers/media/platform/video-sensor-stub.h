/*
 * Copyright (c) 2013--2015 Intel Corporation.
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
 *
 */

#ifndef SENSOR_STUB_H
#define SENSOR_STUB_H

#define SENSOR_STUB_NAME	"ipu4-sensor-stub"

#include <media/media-entity.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-core.h>


#define PIXEL_ORDER_GRBG				0
#define PIXEL_ORDER_RGGB				1
#define PIXEL_ORDER_BGGR				2
#define PIXEL_ORDER_GBRG				3

struct sensor_csi_data_format {
	u32 code;
	u8 width;
	u8 compressed;
	u8 pixel_order;
};

struct stub_sensor {
	struct v4l2_subdev sd;
	struct media_pad pad;
	bool streaming;
	struct mutex mutex;
	const struct sensor_csi_data_format *csi_format;
	int height;
	int width;
	u32 mbus_frame_fmts;
	u32 default_mbus_frame_fmts;
	uint32_t mipi_data_type;
	int default_pixel_order;
	struct v4l2_ctrl *hblank;
	struct v4l2_ctrl *vblank;
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *link_freq;
	int dev_init_done;

};

#define to_stub_sensor(_sd)	\
	container_of(_sd, struct stub_sensor, sd)
#endif
