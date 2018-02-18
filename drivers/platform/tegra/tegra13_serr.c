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
#include <linux/platform/tegra/denver_mca.h>

/* Instantiate all accessors for a bank */
DEFINE_DENVER_MCA_OPS(0,  4,  0,  4,  1,  4,  2,  4,  3,  4,  4)
DEFINE_DENVER_MCA_OPS(1,  4,  6,  4,  7,  5,  0,  5,  1,  5,  2)
DEFINE_DENVER_MCA_OPS(2,  5,  4,  5,  5,  5,  6,  5,  7,  6,  0)
DEFINE_DENVER_MCA_OPS(3,  6,  2,  6,  3,  6,  4,  6,  5,  6,  6)
DEFINE_DENVER_MCA_OPS(4,  7,  0,  7,  1,  7,  2,  7,  3,  7,  4)
DEFINE_DENVER_MCA_OPS(5,  7,  6,  7,  7,  8,  0,  8,  1,  8,  2)
DEFINE_DENVER_MCA_OPS(6,  8,  4,  8,  5,  8,  6,  8,  7,  9,  0)
DEFINE_DENVER_MCA_OPS(7,  9,  2,  9,  3,  9,  4,  9,  5,  9,  6)
DEFINE_DENVER_MCA_OPS(8,  9,  0, 10,  1, 10,  2, 10,  3, 10,  4)
DEFINE_DENVER_MCA_OPS(9,  9,  6, 10,  7, 11,  0, 11,  1, 11,  2)

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

static struct denver_mca_bank mca_banks[] = {
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
	{SIMPLE_DENVER_MCA_OP_ENTRY("ROC:DPMU",	7)},
	{SIMPLE_DENVER_MCA_OP_ENTRY("ROC:IOB",	8)},
	{SIMPLE_DENVER_MCA_OP_ENTRY("IFU",	9)},
	{}
};

static void tegra13_register_denver_mca_banks(void)
{
	int i;

	for (i = 0; mca_banks[i].name; i++)
		register_denver_mca_bank(&mca_banks[i]);
}

static void tegra13_unregister_denver_mca_banks(void)
{
	int i;

	for (i = 0; mca_banks[i].name; i++)
		unregister_denver_mca_bank(&mca_banks[i]);
}

static int __init tegra13_serr_init(void)
{
	tegra13_register_denver_mca_banks();
	return 0;
}

module_init(tegra13_serr_init);

static void __exit tegra13_serr_exit(void)
{
	tegra13_unregister_denver_mca_banks();
}
module_exit(tegra13_serr_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Tegra T132 SError handler");
