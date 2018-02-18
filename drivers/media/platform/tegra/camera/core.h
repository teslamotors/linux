/*
 * NVIDIA Tegra Video Input Device Driver Core Helpers
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Bryan Wu <pengw@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __TEGRA_CORE_H__
#define __TEGRA_CORE_H__

#include <media/v4l2-subdev.h>

/* Minimum and maximum width and height common to Tegra video input device. */
#define TEGRA_MIN_WIDTH		32U
#define TEGRA_MAX_WIDTH		32768U
#define TEGRA_MIN_HEIGHT	32U
#define TEGRA_MAX_HEIGHT	32768U

/* 1080p resolution as default resolution for test pattern generator */
#define TEGRA_DEF_WIDTH		1920
#define TEGRA_DEF_HEIGHT	1080

#define TEGRA_VF_DEF		MEDIA_BUS_FMT_SRGGB10_1X10

/*
 * These go into the TEGRA_VI_CSI_n_IMAGE_DEF registers bits 23:16
 * Output pixel memory format for the VI channel.
 */
enum tegra_image_format {
	TEGRA_IMAGE_FORMAT_T_L8 = 16,

	TEGRA_IMAGE_FORMAT_T_R16_I = 32,
	TEGRA_IMAGE_FORMAT_T_B5G6R5,
	TEGRA_IMAGE_FORMAT_T_R5G6B5,
	TEGRA_IMAGE_FORMAT_T_A1B5G5R5,
	TEGRA_IMAGE_FORMAT_T_A1R5G5B5,
	TEGRA_IMAGE_FORMAT_T_B5G5R5A1,
	TEGRA_IMAGE_FORMAT_T_R5G5B5A1,
	TEGRA_IMAGE_FORMAT_T_A4B4G4R4,
	TEGRA_IMAGE_FORMAT_T_A4R4G4B4,
	TEGRA_IMAGE_FORMAT_T_B4G4R4A4,
	TEGRA_IMAGE_FORMAT_T_R4G4B4A4,

	TEGRA_IMAGE_FORMAT_T_A8B8G8R8 = 64,
	TEGRA_IMAGE_FORMAT_T_A8R8G8B8,
	TEGRA_IMAGE_FORMAT_T_B8G8R8A8,
	TEGRA_IMAGE_FORMAT_T_R8G8B8A8,
	TEGRA_IMAGE_FORMAT_T_A2B10G10R10,
	TEGRA_IMAGE_FORMAT_T_A2R10G10B10,
	TEGRA_IMAGE_FORMAT_T_B10G10R10A2,
	TEGRA_IMAGE_FORMAT_T_R10G10B10A2,

	TEGRA_IMAGE_FORMAT_T_A8Y8U8V8 = 193,
	TEGRA_IMAGE_FORMAT_T_V8U8Y8A8,

	TEGRA_IMAGE_FORMAT_T_A2Y10U10V10 = 197,
	TEGRA_IMAGE_FORMAT_T_V10U10Y10A2,
	TEGRA_IMAGE_FORMAT_T_Y8_U8__Y8_V8,
	TEGRA_IMAGE_FORMAT_T_Y8_V8__Y8_U8,
	TEGRA_IMAGE_FORMAT_T_U8_Y8__V8_Y8,
	TEGRA_IMAGE_FORMAT_T_T_V8_Y8__U8_Y8,

	TEGRA_IMAGE_FORMAT_T_T_Y8__U8__V8_N444 = 224,
	TEGRA_IMAGE_FORMAT_T_Y8__U8V8_N444,
	TEGRA_IMAGE_FORMAT_T_Y8__V8U8_N444,
	TEGRA_IMAGE_FORMAT_T_Y8__U8__V8_N422,
	TEGRA_IMAGE_FORMAT_T_Y8__U8V8_N422,
	TEGRA_IMAGE_FORMAT_T_Y8__V8U8_N422,
	TEGRA_IMAGE_FORMAT_T_Y8__U8__V8_N420,
	TEGRA_IMAGE_FORMAT_T_Y8__U8V8_N420,
	TEGRA_IMAGE_FORMAT_T_Y8__V8U8_N420,
	TEGRA_IMAGE_FORMAT_T_X2Lc10Lb10La10,
	TEGRA_IMAGE_FORMAT_T_A2R6R6R6R6R6,
};

/*
 * These go into the TEGRA_VI_CSI_n_CSI_IMAGE_DT registers bits 7:0
 * Input data type of VI channel.
 */
enum tegra_image_dt {
	TEGRA_IMAGE_DT_YUV420_8 = 24,
	TEGRA_IMAGE_DT_YUV420_10,

	TEGRA_IMAGE_DT_YUV420CSPS_8 = 28,
	TEGRA_IMAGE_DT_YUV420CSPS_10,
	TEGRA_IMAGE_DT_YUV422_8,
	TEGRA_IMAGE_DT_YUV422_10,
	TEGRA_IMAGE_DT_RGB444,
	TEGRA_IMAGE_DT_RGB555,
	TEGRA_IMAGE_DT_RGB565,
	TEGRA_IMAGE_DT_RGB666,
	TEGRA_IMAGE_DT_RGB888,

	TEGRA_IMAGE_DT_RAW6 = 40,
	TEGRA_IMAGE_DT_RAW7,
	TEGRA_IMAGE_DT_RAW8,
	TEGRA_IMAGE_DT_RAW10,
	TEGRA_IMAGE_DT_RAW12,
	TEGRA_IMAGE_DT_RAW14,
};

/* Supported CSI to VI Data Formats */
enum tegra_vf_code {
	TEGRA_VF_RAW6 = 0,
	TEGRA_VF_RAW7,
	TEGRA_VF_RAW8,
	TEGRA_VF_RAW10,
	TEGRA_VF_RAW12,
	TEGRA_VF_RAW14,
	TEGRA_VF_EMBEDDED8,
	TEGRA_VF_RGB565,
	TEGRA_VF_RGB555,
	TEGRA_VF_RGB888,
	TEGRA_VF_RGB444,
	TEGRA_VF_RGB666,
	TEGRA_VF_YUV422,
	TEGRA_VF_YUV420,
	TEGRA_VF_YUV420_CSPS,
};

/**
 * struct tegra_video_format - Tegra video format description
 * @vf_code: video format code
 * @width: format width in bits per component
 * @code: media bus format code
 * @bpp: bytes per pixel (when stored in memory)
 * @img_fmt: image format
 * @img_dt: image data type
 * @fourcc: V4L2 pixel format FCC identifier
 * @description: format description, suitable for userspace
 */
struct tegra_video_format {
	enum tegra_vf_code vf_code;
	unsigned int width;
	unsigned int code;
	unsigned int bpp;
	enum tegra_image_format img_fmt;
	enum tegra_image_dt img_dt;
	u32 fourcc;
};

u32 tegra_core_get_fourcc_by_idx(unsigned int index);
u32 tegra_core_get_word_count(unsigned int frame_width,
			      const struct tegra_video_format *fmt);
int tegra_core_get_idx_by_code(unsigned int code);
const struct tegra_video_format *tegra_core_get_format_by_code(unsigned int
							       code);
const struct tegra_video_format *tegra_core_get_format_by_fourcc(u32 fourcc);
u32 tegra_core_bytes_per_line(unsigned int width, unsigned int align,
			      const struct tegra_video_format *fmt);
#endif
