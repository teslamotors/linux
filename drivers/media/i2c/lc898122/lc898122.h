
/*
 * Copyright (c) 2015--2016 Intel Corporation.
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

#ifndef __LC898122_H__
#define __LC898122_H__

#include <linux/types.h>
#include "lc898122-oisinit.h"

struct lc898122_device {
	struct i2c_client *client;
	struct v4l2_ctrl_handler ctrls_vcm;
	struct v4l2_subdev subdev_vcm;
	struct lc898122_ois state;
	struct device *sensor_dev;
	atomic_t open;
	u8 *buf;
};

#define LC898122_EEPROM_SIZE 128
#define MAX_WRITE_BUF_SIZE 32

#define LC898122_INVALID_CONFIG	0xffffffff
#define LC898122_MAX_FOCUS_POS	2047

#define HALLOFFSET_X_LOW		0x11
#define HALLOFFSET_X_HIGH		0x12
#define HALLOFFSET_Y_LOW		0x13
#define HALLOFFSET_Y_HIGH		0x14
#define HALL_BIAS_X_LOW			0x15
#define HALL_BIAS_X_HIGH		0x16
#define HALL_BIAS_Y_LOW			0x17
#define HALL_BIAS_Y_HIGH		0x18
#define HALL_AD_OFFSET_X_LOW		0x19
#define HALL_AD_OFFSET_X_HIGH		0x1A
#define HALL_AD_OFFSET_Y_LOW		0x1B
#define HALL_AD_OFFSET_Y_HIGH		0x1C
#define LOOP_GAIN_X_LOW			0x1D
#define LOOP_GAIN_X_HIGH		0x1E
#define LOOP_GAIN_Y_LOW			0x1F
#define LOOP_GAIN_Y_HIGH		0x20
#define GYRO_OFFSET_X_LOW		0x25
#define GYRO_OFFSET_X_HIGH		0x26
#define GYRO_OFFSET_Y_LOW		0x27
#define GYRO_OFFSET_Y_HIGH		0x28
#define OSC_VAL				0x29
#define GYRO_GAIN_X0			0x2A
#define GYRO_GAIN_X1			0x2B
#define GYRO_GAIN_X2			0x2C
#define GYRO_GAIN_X3			0x2D
#define GYRO_GAIN_Y0			0x2E
#define GYRO_GAIN_Y1			0x2F
#define GYRO_GAIN_Y2			0x30
#define GYRO_GAIN_Y3			0x31
#define VCM_DAC_OFFSET			0x38
#define AF_5M_L			0x0C
#define AF_5M_H			0x0D
#define AF_10CM_L		0x0E
#define AF_10CM_H		0x0F


#endif
