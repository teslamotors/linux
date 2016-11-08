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
 * Based on ATOMISP dw9714 implementation by
 * Huang Shenbo <shenbo.huang@intel.com.
 */

#ifndef __DW9714_H__
#define __DW9714_H__

#include <linux/types.h>

#define DW9714_NAME		"dw9714"

#define DW9714_MAX_FOCUS_POS	1023
#define DW9714_CTRL_STEPS	16 /* Keep this value power of 2 */
#define DW9714_CTRL_DELAY_US	1000

#define VCM_DEFAULT_S 0x0
#define VCM_VAL(data, s) (u16)((data) << 4 | (s))

struct dw9714_platform_data {
	struct device *sensor_dev;
	int gpio_xsd; /* Should be < 0 if not used */
};

#endif
