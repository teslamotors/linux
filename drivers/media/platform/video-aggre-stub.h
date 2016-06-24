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
#ifndef VIDEO_AGGRE_H
#define VIDEO_AGGRE_H

#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>

#define VIDEO_AGGRE_NAME "video-aggre"

#define PIXEL_ORDER_GRBG				0
#define PIXEL_ORDER_RGGB				1
#define PIXEL_ORDER_BGGR				2
#define PIXEL_ORDER_GBRG				3

#define NR_OF_VA_STREAMS	4
#define NR_OF_VA_SOURCE_PADS	1
#define NR_OF_VA_SINK_PADS	0
#define NR_OF_VA_PADS		(NR_OF_VA_SOURCE_PADS + NR_OF_VA_SINK_PADS)

#define VA_PAD_SOURCE	0

#define VIDEO_AGGRE_MIN_WIDTH	640
#define VIDEO_AGGRE_MIN_HEIGHT	480
#define VIDEO_AGGRE_MAX_WIDTH	1920
#define VIDEO_AGGRE_MAX_HEIGHT	1080

struct video_aggre_csi_data_format {
	u32 code;
	u8 width;
	u8 compressed;
	u8 pixel_order;
	u8 mipi_dt_code;
};

struct video_aggregator {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_ctrl_handler ctrl_handler;
	const char *name;

	bool streaming;
	struct mutex mutex;

	struct v4l2_mbus_framefmt *ffmts[NR_OF_VA_STREAMS];
	struct rect *crop;
	struct rect *compose;

	unsigned int *flags;

	unsigned int nsinks;
	unsigned int nsources;
	unsigned int nstreams;
	unsigned int npads;

	struct v4l2_ctrl *link_freq;
};

#define to_video_aggre(_sd)	\
	container_of(_sd, struct video_aggregator, sd)
#endif
