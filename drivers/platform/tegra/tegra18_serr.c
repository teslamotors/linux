/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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

#include <asm/traps.h>
#include <linux/debugfs.h>
#include <linux/debugfs.h>
#include <linux/cpu_pm.h>
#include <linux/cpu.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/platform/tegra/denver_mca.h>
#include <linux/tegra-mce.h>
#include <linux/mce_ari.h>

/* Denver MCA */

/* Instantiate all accessors for a bank */
DEFINE_DENVER_MCA_OPS(0,  4,  0,  4,  1,  4,  2,  4,  3,  4,  4)
DEFINE_DENVER_MCA_OPS(1,  4,  6,  4,  7,  5,  0,  5,  1,  5,  2)
DEFINE_DENVER_MCA_OPS(2,  5,  4,  5,  5,  5,  6,  5,  7,  6,  0)
DEFINE_DENVER_MCA_OPS(3,  6,  2,  6,  3,  6,  4,  6,  5,  6,  6)
DEFINE_DENVER_MCA_OPS(4,  7,  0,  7,  1,  7,  2,  7,  3,  7,  4)
DEFINE_DENVER_MCA_OPS(5,  7,  6,  7,  7,  8,  0,  8,  1,  8,  2)
DEFINE_DENVER_MCA_OPS(6,  8,  4,  8,  5,  8,  6,  8,  7,  9,  0)
DEFINE_DENVER_MCA_OPS(7,  9,  2,  9,  3,  9,  4,  9,  5,  9,  6)

static struct denver_mca_error jsr_ret_errors[] = {
	{.name = "Internal timer error",
	 .error_code = 0},
	{}
};

static struct denver_mca_error jsr_mts_errors[] = {
	{.name = "Internal unclasified error",
	 .error_code = 4},
	{}
};

static struct denver_mca_bank denver_mca_banks[] = {
	{.name = "JSR:RET",
	 DENVER_MCA_OP_ENTRY(0),
	 .errors = jsr_ret_errors},
	{.name = "JSR:MTS",
	 DENVER_MCA_OP_ENTRY(1),
	 .errors = jsr_mts_errors},
	{SIMPLE_DENVER_MCA_OP_ENTRY("DCC:1",	2)},
	{SIMPLE_DENVER_MCA_OP_ENTRY("DCC:2",	3)},
	{SIMPLE_DENVER_MCA_OP_ENTRY("LVB:3",	4)},
	{SIMPLE_DENVER_MCA_OP_ENTRY("MM",	5)},
	{SIMPLE_DENVER_MCA_OP_ENTRY("L2",	6)},
	{SIMPLE_DENVER_MCA_OP_ENTRY("IFU",	7)},
	{}
};

static void tegra18_register_denver_mca_banks(void)
{
	int i;

	for (i = 0; denver_mca_banks[i].name; i++)
		register_denver_mca_bank(&denver_mca_banks[i]);
}

static void tegra18_unregister_denver_mca_banks(void)
{
	int i;

	for (i = 0; denver_mca_banks[i].name; i++)
		unregister_denver_mca_bank(&denver_mca_banks[i]);
}

/*
 * Without this enable set, Denver cores will halt on serrors
 * instead of continuing (if possible) to the kernel serror
 * handler
 */
static void tegra18_denver_serr_enable(void)
{
	mca_cmd_t cmd;
	u32 error;
	cmd.data = 0;
	cmd.cmd = MCE_ARI_MCA_WRITE_SERR;
	cmd.idx = MCE_ARI_MCA_RD_WR_GLOBAL_CONFIG_REGISTER;
	tegra_mce_write_uncore_mca(cmd, 1, &error);
}


static int __init tegra18_serr_init(void)
{

	tegra18_denver_serr_enable();
	tegra18_register_denver_mca_banks();
	return 0;
}
module_init(tegra18_serr_init);

static void __exit tegra18_serr_exit(void)
{
	tegra18_unregister_denver_mca_banks();
}
module_exit(tegra18_serr_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Tegra T18x SError handler");
