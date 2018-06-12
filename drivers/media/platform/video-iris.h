/*
 * Copyright (c) 2016--2018 Intel Corporation.
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

#define CAMERA_PIRIS_MAX_STEP		21

enum IRIS_MODE {
	DC_IRIS_MODE = 0,
	P_IRIS_MODE,
};

struct camera_pwm_info {
	struct pwm_device *pwm;
	unsigned int duty_period;	/* duty */
};

struct camera_iris_platform_data {
	unsigned int gpio_iris_set;
	unsigned int gpio_piris_en;
	unsigned int gpio_pwm1;
	unsigned int gpio_pwm2;
};

struct camera_iris {
	struct platform_device *pdev;
	struct camera_iris_platform_data *pdata;
	struct v4l2_device v4l2_dev;
	struct v4l2_subdev sd;
	struct v4l2_ctrl_handler ctrl_handler;
	struct camera_pwm_info camera_pwm;
	int iris_mode;
	int piris_step; /* 21 steps for piris in total */
	const char *name;
};

#endif

