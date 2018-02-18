/*
 * GP10B specific sysfs files
 *
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/platform_device.h>

#include "gk20a/gk20a.h"
#include "gp10b_sysfs.h"

#define ROOTRW (S_IRWXU|S_IRGRP|S_IROTH)

static ssize_t ecc_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct gk20a *g = get_gk20a(dev);
	u32 ecc_mask;
	u32 err = 0;

	err = sscanf(buf, "%d", &ecc_mask);
	if (err == 1) {
		err = g->ops.pmu.send_lrf_tex_ltc_dram_overide_en_dis_cmd
			(g, ecc_mask);
		if (err)
			dev_err(dev, "ECC override did not happen\n");
	} else
		return -EINVAL;
	return count;
}

static ssize_t ecc_enable_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct gk20a *g = get_gk20a(dev);

	return sprintf(buf, "ecc override =0x%x\n",
			g->ops.gr.get_lrf_tex_ltc_dram_override(g));
}

static DEVICE_ATTR(ecc_enable, ROOTRW, ecc_enable_read, ecc_enable_store);

void gp10b_create_sysfs(struct device *dev)
{
	int error = 0;

	error |= device_create_file(dev, &dev_attr_ecc_enable);
	if (error)
		dev_err(dev, "Failed to create sysfs attributes!\n");
}

void gp10b_remove_sysfs(struct device *dev)
{
	device_remove_file(dev, &dev_attr_ecc_enable);
}
