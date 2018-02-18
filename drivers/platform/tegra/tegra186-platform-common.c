/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/pstore_ram.h>

static __init int display_tegra_dt_info(void)
{
	int ret_d;
	int ret_t;
	unsigned long dt_root;
	const char *dts_fname;
	const char *dtb_bdate;
	const char *dtb_btime;

	dt_root = of_get_flat_dt_root();

	dts_fname = of_get_flat_dt_prop(dt_root, "nvidia,dtsfilename", NULL);
	if (dts_fname)
		pr_info("DTS File Name: %s\n", dts_fname);
	else
		pr_info("DTS File Name: <unknown>\n");

	ret_d = of_property_read_string_index(of_find_node_by_path("/"),
			"nvidia,dtbbuildtime", 0, &dtb_bdate);
	ret_t = of_property_read_string_index(of_find_node_by_path("/"),
			"nvidia,dtbbuildtime", 1, &dtb_btime);
	if (!ret_d && !ret_t)
		pr_info("DTB Build time: %s %s\n", dtb_bdate, dtb_btime);
	else
		pr_info("DTB Build time: <unknown>\n");

	return 0;
}
arch_initcall(display_tegra_dt_info);

#ifdef CONFIG_PSTORE_RAM
#define RECORD_MEM_SIZE SZ_64K
#define CONSOLE_MEM_SIZE SZ_512K
#define FTRACE_MEM_SIZE SZ_512K
#define RTRACE_MEM_SIZE SZ_512K

static struct ramoops_platform_data ramoops_data;

static struct platform_device ramoops_dev  = {
	.name = "ramoops",
	.dev = {
		.platform_data = &ramoops_data,
	},
};

static int __init ramoops_init(struct reserved_mem *rmem)
{
	ramoops_data.mem_address = rmem->base;
	ramoops_data.mem_size = rmem->size;
	ramoops_data.record_size = RECORD_MEM_SIZE;
#ifdef CONFIG_PSTORE_CONSOLE
	ramoops_data.console_size = CONSOLE_MEM_SIZE;
#endif
#ifdef CONFIG_PSTORE_FTRACE
	ramoops_data.ftrace_size = FTRACE_MEM_SIZE;
#endif
#ifdef CONFIG_PSTORE_RTRACE
	ramoops_data.rtrace_size = RTRACE_MEM_SIZE;
#endif
#ifdef CONFIG_PANIC_ON_OOPS
	ramoops_data.dump_oops = 0;
#else
	ramoops_data.dump_oops = 1;
#endif
	return 0;
}
RESERVEDMEM_OF_DECLARE(tegra_ramoops, "nvidia,ramoops", ramoops_init);

static int __init tegra_register_ramoops_device(void)
{
	int ret = platform_device_register(&ramoops_dev);
	if (ret)
		pr_err("Unable to register ramoops platform device\n");
	return ret;
}
core_initcall(tegra_register_ramoops_device);
#endif /* CONFIG_PSTORE_RAM */
