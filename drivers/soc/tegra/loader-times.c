/* loader-times.c
 *
 * Support for reading the timestamps from the loader.
 *
 * Copyright 2016 Codethink Ltd.
 *	Ben Dooks <ben.dooks@codethink.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>

static u32 start, done;

static int loader_time_show(void)
{
	if (of_property_read_u32(of_root, "nvidia,loader-start", &start) < 0 ||
	    of_property_read_u32(of_root, "nvidia,loader-done", &done) < 0) {
		pr_info("%s: no timing info\n", __func__);
		return 0;
	}

	pr_info("%s: start %u, done %u, time spent %u us\n",
		__func__, start, done, done-start);
	return 0;
}

late_initcall(loader_time_show);

