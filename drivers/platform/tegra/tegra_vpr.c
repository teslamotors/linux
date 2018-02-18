/*
 * Copyright (c) 2016 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/gk20a.h>
#include <linux/smc-calls.h>
#include <linux/ote_protocol.h>
#include <linux/delay.h>

phys_addr_t tegra_vpr_start;
phys_addr_t tegra_vpr_size;
bool tegra_vpr_resize;
static DEFINE_MUTEX(vpr_lock);

static int tegra_vpr_arg(char *options)
{
	char *p = options;

	tegra_vpr_size = memparse(p, &p);
	if (*p == '@')
		tegra_vpr_start = memparse(p+1, &p);
	pr_info("Found vpr, start=0x%llx size=%llx",
		(u64)tegra_vpr_start, (u64)tegra_vpr_size);
	return 0;
}
early_param("vpr", tegra_vpr_arg);

static int tegra_vpr_resize_arg(char *options)
{
	tegra_vpr_resize = true;
	return 0;
}
early_param("vpr_resize", tegra_vpr_resize_arg);

static int tegra_update_resize_cfg(phys_addr_t base , size_t size)
{
	int err = -EPERM;
#define MAX_RETRIES 6
	int retries = MAX_RETRIES;
retry:
	err = gk20a_do_idle();
	if (!err) {
		/* Config VPR_BOM/_SIZE in MC */
		err = tegra_set_vpr_params((void *)(uintptr_t)base, size);
		gk20a_do_unidle();
	} else {
		if (retries--) {
			pr_err("%s:%d: fail retry=%d",
				__func__, __LINE__, MAX_RETRIES - retries);
			msleep(1);
			goto retry;
		}
	}
	return err;
}

struct dma_resize_notifier_ops vpr_dev_ops = {
	.resize = tegra_update_resize_cfg
};
EXPORT_SYMBOL(vpr_dev_ops);

int tegra_set_vpr_params(void *vpr_base, size_t vpr_size)
{
	int retval = -EINVAL;

	mutex_lock(&vpr_lock);
	retval = invoke_smc(TE_SMC_PROGRAM_VPR,
				(uintptr_t)vpr_base, vpr_size);
	mutex_unlock(&vpr_lock);

	if (retval != 0) {
		pr_err("%s: smc failed, base 0x%p size %zx, err (0x%x)\n",
			__func__, vpr_base, vpr_size, retval);
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL(tegra_set_vpr_params);
