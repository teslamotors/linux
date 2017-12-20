/* Tegra legacy fuse driver
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
#include <linux/moduleparam.h>

#include <soc/tegra/fuse.h>

static unsigned int tegra_chip_id;
static unsigned int tegra_chip_rev;

static int param_get_chip_id(char *val, const struct kernel_param *kp)
{
	tegra_chip_id = (tegra_read_chipid() >> 8) & 0xff;
	return param_get_uint(val, kp);
}

static int param_get_chip_rev(char *val, const struct kernel_param *kp)
{
	tegra_chip_rev = tegra_sku_info.revision;
	return param_get_uint(val, kp);
}

static struct kernel_param_ops chip_id_ops = { .get = param_get_chip_id, };
static struct kernel_param_ops chip_rev_ops = { .get = param_get_chip_rev, };

module_param_cb(tegra_chip_id, &chip_id_ops, &tegra_chip_id, 0444);
module_param_cb(tegra_chip_rev, &chip_rev_ops, &tegra_chip_rev, 0444);
