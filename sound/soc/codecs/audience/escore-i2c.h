/*
 * es705-i2c.h  --  Audience eS705 I2C interface
 *
 * Copyright 2011 Audience, Inc.
 *
 * Author: Greg Clemson <gclemson@audience.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ESCORE_I2C_H
#define _ESCORE_I2C_H

#include "escore.h"
#define ES_I2C_BOOT_CMD			0x0001
#define ES_I2C_BOOT_ACK			0x0101

#ifdef CONFIG_ARCH_MSM8974
#define ESCORE_I2C_VTG_MIN_UV	1800000
#define ESCORE_I2C_VTG_MAX_UV	1800000
#define ESCORE_I2C_LOAD_UA		30000
#endif

#define ES_MAX_I2C_XFER_LEN		0x8000

extern struct i2c_driver escore_i2c_driver;
extern int escore_i2c_init(void);
extern struct es_stream_device es_i2c_streamdev;

#endif
