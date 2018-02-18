/*
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

#include <linux/errno.h>
#include <linux/bug.h>
#include <linux/tegra-soc.h>
#include "guest_sysinfo.h"
#include "syscalls.h"

static uint64_t sysinfo_ipa;
static struct guest_sysinfo *sysinfo;

static int sysinfo_init(void)
{
	if (sysinfo_ipa && sysinfo)
		return 0;

	sysinfo_ipa = hyp_sysinfo_ipa();
	if (!sysinfo_ipa) {
		pr_err("hv_sysinfo: Invalid Sysinfo ipa!");
		return -EINVAL;
	}

	sysinfo = (struct guest_sysinfo *)ioremap(sysinfo_ipa,
			sizeof(struct guest_sysinfo));
	if (!sysinfo) {
		pr_err("hv_sysinfo: Unmapped Sysinfo ipa!");
		return -EFAULT;
	}

	return 0;
}

static void sysinfo_deinit(void)
{
	if (sysinfo)
		iounmap(sysinfo);
	sysinfo_ipa = 0;
}

#ifdef CONFIG_TEGRA_VIRT_SUPPORT_BOOTDATA
#define BOOT_DATA_PROP_STRING	"nvidia,bootdata"
static struct property bootdata_prop = {
	.name = BOOT_DATA_PROP_STRING,
};

static int __init hv_expose_bootdata(void)
{
	int err;
	uint64_t bootdata_ipa;
	uint8_t *boot_data = NULL;

	if (!is_tegra_hypervisor_mode())
		return -EPERM;

	err = sysinfo_init();
	if (err)
		return err;

	if (sysinfo->bootdata_size && sysinfo_ipa) {
		bootdata_ipa = ((uint64_t)sysinfo_ipa +
				(uint64_t)sysinfo->bootdata_offset);
		boot_data = (uint8_t *)ioremap(bootdata_ipa,
					sysinfo->bootdata_size);
		if (!boot_data) {
			pr_err("hv_sysinfo: Failed to map bootdata!\n");
			err = -EFAULT;
			goto unmap_sysinfo;
		}

		bootdata_prop.length = sysinfo->bootdata_size;
		bootdata_prop.value = boot_data;
		err = of_add_property(of_chosen, &bootdata_prop);
		if (err) {
			pr_err("hv_sysinfo: Failed to add /chosen/%s prop\n",
				BOOT_DATA_PROP_STRING);
			goto unmap_bootdata;
		}
		pr_info("hv_sysinfo: Successfully added /chosen/%s property!\n",
			BOOT_DATA_PROP_STRING);

	} else {
		pr_info("hv_sysinfo: Bootdata not available via sysinfo!\n");
		err = -ENOENT;
		goto unmap_sysinfo;
	}

	return 0;

unmap_bootdata:
	if (boot_data)
		iounmap(boot_data);
unmap_sysinfo:
	sysinfo_deinit();
	return err;
}

late_initcall(hv_expose_bootdata);
#endif
