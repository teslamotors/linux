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

#ifndef __LC898122_HEADER__
#define __LC898122_HEADER__

#define LC898122_VCM_ADDR	0x24
#define LC898122_NAME		"lc898122"

struct lc898122_platform_data {
	struct device *sensor_device;
};

#endif /* __LC898122_HEADER__ */
