/*
 * Generic driver for common register list based camera sensor modules
 *
 * Copyright (c) 2014--2015 Intel Corporation.
 *
 * Author: Vinod Govindapillai <vinod.govindapillai@intel.com>
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

#ifndef __CRLMODULE_H
#define __CRLMODULE_H

#include <media/v4l2-subdev.h>

#define CRLMODULE_NAME		"crlmodule"

struct crlmodule_platform_data {
	unsigned short i2c_addr;
	unsigned short i2c_adapter;

	unsigned int ext_clk;		/* sensor external clk */

	unsigned int lanes;		/* Number of CSI-2 lanes */
	const s64 *op_sys_clock;

	int xshutdown;			/* gpio */
	char module_name[16]; /* module name from ACPI */
};

#endif /* __CRLMODULE_H  */
