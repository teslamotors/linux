/*
 * Copyright (c) 2014--2017 Intel Corporation.
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

#ifndef MEDIA_INTEL_IPU4_H
#define MEDIA_INTEL_IPU4_H

#include <linux/i2c.h>
#include <linux/clkdev.h>

#define INTEL_IPU4_ISYS_MAX_CSI2_LANES		4

struct intel_ipu4_isys_csi2_config {
	unsigned int nlanes;
	unsigned int port;
};

struct intel_ipu4_isys_subdev_i2c_info {
	struct i2c_board_info board_info;
	int i2c_adapter_id;
};

struct intel_ipu4_isys_subdev_info {
	struct intel_ipu4_isys_csi2_config *csi2;
	struct intel_ipu4_isys_subdev_i2c_info i2c;
	char *acpiname;
};

struct intel_ipu4_isys_clk_mapping {
	struct clk_lookup clkdev_data;
	char *platform_clock_name;
};

struct intel_ipu4_isys_subdev_pdata {
	struct intel_ipu4_isys_subdev_info **subdevs;
	struct intel_ipu4_isys_clk_mapping *clk_map;
};

#endif /* MEDIA_INTEL_IPU4_H */
