/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2016 - 2018 Intel Corporation */

#ifndef __LC898122_HEADER__
#define __LC898122_HEADER__

#define LC898122_VCM_ADDR	0x24
#define LC898122_NAME		"lc898122"

struct lc898122_platform_data {
	struct device *sensor_device;
};

#endif /* __LC898122_HEADER__ */
