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
#ifndef CAMERA_IRIS_H
#define CAMERA_IRIS_H

#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <linux/pwm.h>

#define CAMERA_IRIS_NAME "camera-iris"

#define CAMERA_PWM_PERIOD		200 /* nseconds, 5MHz */
#define CAMERA_MAX_DUTY			1000

struct camera_pwm_info {
	struct pwm_device *pwm;
	unsigned int duty_period;	/* duty */
};

struct camera_iris {
	struct platform_device *pdev;
	struct v4l2_device v4l2_dev;
	struct v4l2_subdev sd;
	struct v4l2_ctrl_handler ctrl_handler;
	struct camera_pwm_info camera_pwm;
	const char *name;
};

#endif

